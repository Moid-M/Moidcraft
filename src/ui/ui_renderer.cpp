#include "ui_renderer.hpp"
#include "core/vk_check.hpp"
#include <cstring>
#include <algorithm>
#include <glm/glm.hpp>

#include "stb_image.h"

UiRenderer::~UiRenderer() { cleanup(); }

void UiRenderer::init(VulkanContext* ctx, const std::string& texturesDir) {
    m_ctx = ctx;

    loadFontTexture(texturesDir + "/ascii.png");
    loadButtonTextures(texturesDir + "/button.png", texturesDir + "/button_highlighted.png");
    createTextPipeline();
    createButtonPipeline();
    createDescriptorSets();

    allocBatch(m_bgBuf, m_bgMem, m_bgCap, 4096);
    allocBatch(m_btnBuf, m_btnMem, m_btnCap, 4096);
    allocBatch(m_fgBuf, m_fgMem, m_fgCap, 4096);
}

void UiRenderer::cleanup() {
    if (!m_ctx || !m_ctx->device()) return;
    auto dev = m_ctx->device();

    m_textPipeline.reset();
    m_buttonPipeline.reset();

    if (m_descriptorPool) vkDestroyDescriptorPool(dev, m_descriptorPool, nullptr);
    if (m_descriptorLayout) vkDestroyDescriptorSetLayout(dev, m_descriptorLayout, nullptr);

    auto destroy = [&](VkBuffer& b, VkDeviceMemory& m) {
        if (b) vkDestroyBuffer(dev, b, nullptr);
        if (m) vkFreeMemory(dev, m, nullptr);
        b = VK_NULL_HANDLE; m = VK_NULL_HANDLE;
    };
    destroy(m_bgBuf, m_bgMem);
    destroy(m_btnBuf, m_btnMem);
    destroy(m_fgBuf, m_fgMem);

    if (m_fontView) vkDestroyImageView(dev, m_fontView, nullptr);
    if (m_fontSampler) vkDestroySampler(dev, m_fontSampler, nullptr);
    if (m_fontImage) vkDestroyImage(dev, m_fontImage, nullptr);
    if (m_fontMemory) vkFreeMemory(dev, m_fontMemory, nullptr);

    if (m_buttonView) vkDestroyImageView(dev, m_buttonView, nullptr);
    if (m_buttonImage) vkDestroyImage(dev, m_buttonImage, nullptr);
    if (m_buttonMemory) vkFreeMemory(dev, m_buttonMemory, nullptr);
}

void UiRenderer::allocBatch(VkBuffer& buf, VkDeviceMemory& mem, size_t& cap, size_t maxVerts) {
    if (buf) return;
    auto dev = m_ctx->device();
    cap = maxVerts * sizeof(UiVertex);
    VkBufferCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size = cap;
    info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(vkCreateBuffer(dev, &info, nullptr, &buf));
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(dev, buf, &req);
    VkMemoryAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = m_ctx->findMemoryType(req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VK_CHECK(vkAllocateMemory(dev, &alloc, nullptr, &mem));
    vkBindBufferMemory(dev, buf, mem, 0);
}

void UiRenderer::beginFrame(int fbWidth, int fbHeight) {
    m_fbWidth = fbWidth;
    m_fbHeight = fbHeight;
    m_bgVerts.clear();
    m_btnVerts.clear();
    m_fgVerts.clear();
}

void UiRenderer::addQuad(float x, float y, float w, float h,
                          float u1, float v1, float u2, float v2,
                          uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                          std::vector<UiVertex>& verts) {
    verts.push_back({x,   y,   u1, v1, r, g, b, a});
    verts.push_back({x,   y+h, u1, v2, r, g, b, a});
    verts.push_back({x+w, y+h, u2, v2, r, g, b, a});
    verts.push_back({x,   y,   u1, v1, r, g, b, a});
    verts.push_back({x+w, y+h, u2, v2, r, g, b, a});
    verts.push_back({x+w, y,   u2, v1, r, g, b, a});
}

void UiRenderer::drawRect(float x, float y, float w, float h, glm::vec4 color) {
    uint8_t r = uint8_t(color.r * 255);
    uint8_t g = uint8_t(color.g * 255);
    uint8_t b = uint8_t(color.b * 255);
    uint8_t a = uint8_t(color.a * 255);
    float u = 9.5f / 128.0f;
    float v = 32.5f / 128.0f;
    addQuad(x, y, w, h, u, v, u, v, r, g, b, a, m_bgVerts);
}

void UiRenderer::drawText(const char* text, float x, float y, float size, glm::vec4 color) {
    drawText(text, x, y, size, color, m_bgVerts);
}

void UiRenderer::drawTextFg(const char* text, float x, float y, float size, glm::vec4 color) {
    drawText(text, x, y, size, color, m_fgVerts);
}

void UiRenderer::drawText(const char* text, float x, float y, float size, glm::vec4 color,
                           std::vector<UiVertex>& verts) {
    uint8_t r = uint8_t(color.r * 255);
    uint8_t g = uint8_t(color.g * 255);
    uint8_t b = uint8_t(color.b * 255);
    uint8_t a = uint8_t(color.a * 255);

    float texW = float(FONT_COLS * FONT_CELL);
    float texH = float(FONT_COLS * FONT_CELL);

    float cx = x;
    while (*text) {
        char c = *text++;
        if (c == ' ') { cx += size * 0.5f; continue; }

        int idx = (unsigned char)c;
        int gridX = idx % FONT_COLS;
        int gridY = idx / FONT_COLS;

        float u1 = (gridX * FONT_CELL) / texW;
        float v1 = (gridY * FONT_CELL) / texH;
        float u2 = (gridX * FONT_CELL + FONT_CELL) / texW;
        float v2 = (gridY * FONT_CELL + FONT_CELL) / texH;

        addQuad(cx, y, size, size, u1, v1, u2, v2, r, g, b, a, verts);
        cx += size * 0.80f;
    }
}

void UiRenderer::drawButton(float x, float y, float w, float h, bool hovered) {
    if (!m_hasButtons) {
        glm::vec4 c = hovered ? glm::vec4(0.3f, 0.3f, 0.45f, 1.0f) : glm::vec4(0.2f, 0.2f, 0.30f, 1.0f);
        uint8_t r = uint8_t(c.r * 255), g = uint8_t(c.g * 255), b = uint8_t(c.b * 255), a = uint8_t(c.a * 255);
        float u = 9.5f / 128.0f, v = 32.5f / 128.0f;
        addQuad(x, y, w, h, u, v, u, v, r, g, b, a, m_btnVerts);
        return;
    }

    uint8_t r = 255, g = 255, b = 255, a = 255;
    int border = BORDER;
    float texW = float(BUTTON_W);
    float halfH = float(BUTTON_H);
    float texH = float(BUTTON_TEX_H);

    float yOff = hovered ? 0.5f : 0.0f;
    float su[4] = {0.0f, float(border)/texW, float(texW-border)/texW, 1.0f};
    float sv[4] = {yOff, yOff + float(border)/texH, yOff + float(halfH-border)/texH, yOff + 0.5f};

    float dx[4] = {x, x + border, x + w - border, x + w};
    float dy[4] = {y, y + border, y + h - border, y + h};

    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < 3; col++) {
            float qx = dx[col];
            float qy = dy[row];
            float qw = dx[col+1] - dx[col];
            float qh = dy[row+1] - dy[row];
            if (qw <= 0 || qh <= 0) continue;
            addQuad(qx, qy, qw, qh, su[col], sv[row], su[col+1], sv[row+1], r, g, b, a, m_btnVerts);
        }
    }
}

int UiRenderer::textWidth(const char* text, float size) const {
    float w = 0;
    while (*text) {
        char c = *text++;
        if (c == ' ') { w += size * 0.5f; continue; }
        w += size * 0.80f;
    }
    return int(w);
}

void UiRenderer::uploadAndDraw(VkCommandBuffer cmd, std::vector<UiVertex>& verts,
                                VkBuffer& buf, VkDeviceMemory& mem, size_t cap,
                                VkPipeline pipeline, VkPipelineLayout layout,
                                VkDescriptorSet descSet) {
    size_t count = verts.size();
    if (count == 0) return;

    if (count * sizeof(UiVertex) > cap) {
        std::cerr << "CRITICAL: UI buffer overflow! count=" << count << " cap=" << cap << std::endl;
        return;
    }

    size_t byteSize = count * sizeof(UiVertex);
    void* data;
    vkMapMemory(m_ctx->device(), mem, 0, byteSize, 0, &data);
    memcpy(data, verts.data(), byteSize);
    vkUnmapMemory(m_ctx->device(), mem);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    glm::vec2 screenSize(m_fbWidth, m_fbHeight);
    vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_VERTEX_BIT,
                       0, sizeof(glm::vec2), &screenSize);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            layout, 0, 1, &descSet, 0, nullptr);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &buf, &offset);
    vkCmdDraw(cmd, count, 1, 0, 0);
}

void UiRenderer::flushAll(VkCommandBuffer cmd) {
    // Batch 0: background + title (text pipeline) - draws first, behind everything
    uploadAndDraw(cmd, m_bgVerts, m_bgBuf, m_bgMem, m_bgCap,
                  m_textPipeline->pipeline(), m_textPipeline->layout(), m_textDescriptorSet);

    // Batch 1: buttons (button pipeline) - draws second, in front of bg
    if (m_hasButtons) {
        uploadAndDraw(cmd, m_btnVerts, m_btnBuf, m_btnMem, m_btnCap,
                      m_buttonPipeline->pipeline(), m_buttonPipeline->layout(), m_buttonDescriptorSet);
    } else {
        uploadAndDraw(cmd, m_btnVerts, m_btnBuf, m_btnMem, m_btnCap,
                      m_textPipeline->pipeline(), m_textPipeline->layout(), m_textDescriptorSet);
    }

    // Batch 2: foreground labels (text pipeline) - draws last, in front of everything
    uploadAndDraw(cmd, m_fgVerts, m_fgBuf, m_fgMem, m_fgCap,
                  m_textPipeline->pipeline(), m_textPipeline->layout(), m_textDescriptorSet);
}

// ---- Pipeline creation ----

void UiRenderer::createTextPipeline() {
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;
    VK_CHECK(vkCreateDescriptorSetLayout(m_ctx->device(), &layoutInfo, nullptr, &m_descriptorLayout));

    VkPushConstantRange pcRange{};
    pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pcRange.offset = 0;
    pcRange.size = sizeof(glm::vec2);

    VkVertexInputBindingDescription bindingDesc{};
    bindingDesc.binding = 0;
    bindingDesc.stride = sizeof(UiVertex);
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::vector<VkVertexInputAttributeDescription> attribs(3);
    attribs[0].location = 0;
    attribs[0].binding = 0;
    attribs[0].format = VK_FORMAT_R32G32_SFLOAT;
    attribs[0].offset = offsetof(UiVertex, x);
    attribs[1].location = 1;
    attribs[1].binding = 0;
    attribs[1].format = VK_FORMAT_R32G32_SFLOAT;
    attribs[1].offset = offsetof(UiVertex, u);
    attribs[2].location = 2;
    attribs[2].binding = 0;
    attribs[2].format = VK_FORMAT_R8G8B8A8_UNORM;
    attribs[2].offset = offsetof(UiVertex, r);

    m_textPipeline = std::make_unique<VulkanPipeline>();
    m_textPipeline->init(m_ctx,
        "shaders/ui.vert.spv", "shaders/ui.frag.spv",
        bindingDesc, attribs,
        m_descriptorLayout, pcRange, false, true);
}

void UiRenderer::createButtonPipeline() {
    if (!m_hasButtons) return;

    VkPushConstantRange pcRange{};
    pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pcRange.offset = 0;
    pcRange.size = sizeof(glm::vec2);

    VkVertexInputBindingDescription bindingDesc{};
    bindingDesc.binding = 0;
    bindingDesc.stride = sizeof(UiVertex);
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::vector<VkVertexInputAttributeDescription> attribs(3);
    attribs[0].location = 0;
    attribs[0].binding = 0;
    attribs[0].format = VK_FORMAT_R32G32_SFLOAT;
    attribs[0].offset = offsetof(UiVertex, x);
    attribs[1].location = 1;
    attribs[1].binding = 0;
    attribs[1].format = VK_FORMAT_R32G32_SFLOAT;
    attribs[1].offset = offsetof(UiVertex, u);
    attribs[2].location = 2;
    attribs[2].binding = 0;
    attribs[2].format = VK_FORMAT_R8G8B8A8_UNORM;
    attribs[2].offset = offsetof(UiVertex, r);

    m_buttonPipeline = std::make_unique<VulkanPipeline>();
    m_buttonPipeline->init(m_ctx,
        "shaders/ui.vert.spv", "shaders/ui_button.frag.spv",
        bindingDesc, attribs,
        m_descriptorLayout, pcRange, false, true);
}

void UiRenderer::createDescriptorSets() {
    auto dev = m_ctx->device();

    int maxSets = 2 + (m_hasButtons ? 1 : 0);
    int poolCount = 2 + (m_hasButtons ? 1 : 0);

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = poolCount;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = maxSets;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    VK_CHECK(vkCreateDescriptorPool(dev, &poolInfo, nullptr, &m_descriptorPool));

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_descriptorLayout;
    VK_CHECK(vkAllocateDescriptorSets(dev, &allocInfo, &m_textDescriptorSet));

    VkDescriptorImageInfo fontInfo{};
    fontInfo.sampler = m_fontSampler;
    fontInfo.imageView = m_fontView;
    fontInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_textDescriptorSet;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &fontInfo;
    vkUpdateDescriptorSets(dev, 1, &write, 0, nullptr);

    if (m_hasButtons) {
        VK_CHECK(vkAllocateDescriptorSets(dev, &allocInfo, &m_buttonDescriptorSet));

        VkDescriptorImageInfo btnInfo{};
        btnInfo.sampler = m_fontSampler;
        btnInfo.imageView = m_buttonView;
        btnInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        write.dstSet = m_buttonDescriptorSet;
        write.pImageInfo = &btnInfo;
        vkUpdateDescriptorSets(dev, 1, &write, 0, nullptr);
    }
}

// ---- Texture loading ----

void UiRenderer::loadFontTexture(const std::string& path) {
    int w, h, ch;
    unsigned char* rgba = stbi_load(path.c_str(), &w, &h, &ch, 4);
    if (!rgba) throw std::runtime_error("Failed to load font: " + path);

    std::vector<uint8_t> fontPixels(w * h * 4);
    for (int i = 0; i < w * h; i++) {
        uint8_t val = (rgba[i*4+3] > 128) ? rgba[i*4] : 0;
        fontPixels[i*4+0] = val; // R
        fontPixels[i*4+1] = val; // G
        fontPixels[i*4+2] = val; // B
        fontPixels[i*4+3] = val; // A
    }
    stbi_image_free(rgba);

    auto dev = m_ctx->device();
    int texW = w;
    int texH = h;
    VkDeviceSize imageSize = texW * texH * 4;

    VkBuffer staging;
    VkDeviceMemory stagingMem;
    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = imageSize;
    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(vkCreateBuffer(dev, &bufInfo, nullptr, &staging));

    VkMemoryRequirements bufReq;
    vkGetBufferMemoryRequirements(dev, staging, &bufReq);
    VkMemoryAllocateInfo bufAlloc{};
    bufAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    bufAlloc.allocationSize = bufReq.size;
    bufAlloc.memoryTypeIndex = m_ctx->findMemoryType(bufReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VK_CHECK(vkAllocateMemory(dev, &bufAlloc, nullptr, &stagingMem));
    vkBindBufferMemory(dev, staging, stagingMem, 0);

    void* data;
    vkMapMemory(dev, stagingMem, 0, imageSize, 0, &data);
    memcpy(data, fontPixels.data(), imageSize);
    vkUnmapMemory(dev, stagingMem);

    VkImageCreateInfo imgInfo{};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imgInfo.extent = {u32(texW), u32(texH), 1};
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VK_CHECK(vkCreateImage(dev, &imgInfo, nullptr, &m_fontImage));

    VkMemoryRequirements imgReq;
    vkGetImageMemoryRequirements(dev, m_fontImage, &imgReq);
    VkMemoryAllocateInfo imgAlloc{};
    imgAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    imgAlloc.allocationSize = imgReq.size;
    imgAlloc.memoryTypeIndex = m_ctx->findMemoryType(imgReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK(vkAllocateMemory(dev, &imgAlloc, nullptr, &m_fontMemory));
    vkBindImageMemory(dev, m_fontImage, m_fontMemory, 0);

    VkCommandBuffer cmd = m_ctx->beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = m_fontImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {u32(texW), u32(texH), 1};
    vkCmdCopyBufferToImage(cmd, staging, m_fontImage,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    m_ctx->endSingleTimeCommands(cmd);

    vkDestroyBuffer(dev, staging, nullptr);
    vkFreeMemory(dev, stagingMem, nullptr);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_fontImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    VK_CHECK(vkCreateImageView(dev, &viewInfo, nullptr, &m_fontView));

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    VK_CHECK(vkCreateSampler(dev, &samplerInfo, nullptr, &m_fontSampler));
}

void UiRenderer::loadButtonTextures(const std::string& normalPath, const std::string& highlightedPath) {
    int w, h, ch;
    unsigned char* pixels = stbi_load(normalPath.c_str(), &w, &h, &ch, 1);
    if (!pixels || w != BUTTON_W || h != BUTTON_H) { stbi_image_free(pixels); return; }

    int wHi, hHi, chHi;
    unsigned char* pixelsHi = stbi_load(highlightedPath.c_str(), &wHi, &hHi, &chHi, 1);
    if (!pixelsHi || wHi != BUTTON_W || hHi != BUTTON_H) { stbi_image_free(pixels); stbi_image_free(pixelsHi); return; }

    auto dev = m_ctx->device();

    int texW = BUTTON_W;
    int texH = BUTTON_TEX_H;
    VkDeviceSize imageSize = texW * texH;

    std::vector<uint8_t> combined(imageSize);
    memcpy(combined.data(), pixels, texW * BUTTON_H);
    memcpy(combined.data() + texW * BUTTON_H, pixelsHi, texW * BUTTON_H);
    stbi_image_free(pixels);
    stbi_image_free(pixelsHi);

    VkBuffer staging;
    VkDeviceMemory stagingMem;
    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = imageSize;
    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(vkCreateBuffer(dev, &bufInfo, nullptr, &staging));

    VkMemoryRequirements bufReq;
    vkGetBufferMemoryRequirements(dev, staging, &bufReq);
    VkMemoryAllocateInfo bufAlloc{};
    bufAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    bufAlloc.allocationSize = bufReq.size;
    bufAlloc.memoryTypeIndex = m_ctx->findMemoryType(bufReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VK_CHECK(vkAllocateMemory(dev, &bufAlloc, nullptr, &stagingMem));
    vkBindBufferMemory(dev, staging, stagingMem, 0);

    void* data;
    vkMapMemory(dev, stagingMem, 0, imageSize, 0, &data);
    memcpy(data, combined.data(), imageSize);
    vkUnmapMemory(dev, stagingMem);

    VkImageCreateInfo imgInfo{};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = VK_FORMAT_R8_UNORM;
    imgInfo.extent = {u32(texW), u32(texH), 1};
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VK_CHECK(vkCreateImage(dev, &imgInfo, nullptr, &m_buttonImage));

    VkMemoryRequirements imgReq;
    vkGetImageMemoryRequirements(dev, m_buttonImage, &imgReq);
    VkMemoryAllocateInfo imgAlloc{};
    imgAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    imgAlloc.allocationSize = imgReq.size;
    imgAlloc.memoryTypeIndex = m_ctx->findMemoryType(imgReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK(vkAllocateMemory(dev, &imgAlloc, nullptr, &m_buttonMemory));
    vkBindImageMemory(dev, m_buttonImage, m_buttonMemory, 0);

    VkCommandBuffer cmd = m_ctx->beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = m_buttonImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {u32(texW), u32(texH), 1};
    vkCmdCopyBufferToImage(cmd, staging, m_buttonImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    m_ctx->endSingleTimeCommands(cmd);

    vkDestroyBuffer(dev, staging, nullptr);
    vkFreeMemory(dev, stagingMem, nullptr);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_buttonImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    VK_CHECK(vkCreateImageView(dev, &viewInfo, nullptr, &m_buttonView));

    m_hasButtons = true;
}
