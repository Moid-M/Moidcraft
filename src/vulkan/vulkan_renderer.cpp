#include "vulkan_renderer.hpp"
#include "core/vk_check.hpp"
#include <array>
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>

VulkanRenderer::~VulkanRenderer() { cleanup(); }

void VulkanRenderer::init(VulkanContext* ctx, const Camera* camera) {
    m_ctx = ctx;
    m_camera = camera;

    createDescriptorLayout();
    createDescriptorPool();
    createUniformBuffers();
    createDescriptorSets();
    createCommandBuffers();
    createSyncObjects();
    createPipeline();
}

void VulkanRenderer::cleanup() {
    if (!m_ctx || !m_ctx->device()) return;
    auto dev = m_ctx->device();

    vkDeviceWaitIdle(dev);

    m_opaquePipeline.reset();
    m_transparentPipeline.reset();
    m_debugPipeline.reset();

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (m_commandBuffers[i]) vkFreeCommandBuffers(dev, m_ctx->commandPool(), 1, &m_commandBuffers[i]);
        if (m_imageAvailable[i]) vkDestroySemaphore(dev, m_imageAvailable[i], nullptr);
        if (m_renderFinished[i]) vkDestroySemaphore(dev, m_renderFinished[i], nullptr);
        if (m_inFlightFences[i]) vkDestroyFence(dev, m_inFlightFences[i], nullptr);
        if (m_uniformBuffers[i]) vkDestroyBuffer(dev, m_uniformBuffers[i], nullptr);
        if (m_uniformMemories[i]) vkFreeMemory(dev, m_uniformMemories[i], nullptr);
    }

    if (m_descriptorPool) vkDestroyDescriptorPool(dev, m_descriptorPool, nullptr);
    if (m_descriptorLayout) vkDestroyDescriptorSetLayout(dev, m_descriptorLayout, nullptr);
}

void VulkanRenderer::beginFrame() {
    VkFence& fence = m_inFlightFences[m_currentFrame];
    vkWaitForFences(m_ctx->device(), 1, &fence, VK_TRUE, UINT64_MAX);
    vkResetFences(m_ctx->device(), 1, &fence);

    VkResult result = vkAcquireNextImageKHR(
        m_ctx->device(), m_ctx->swapchain(), UINT64_MAX,
        m_imageAvailable[m_currentFrame], VK_NULL_HANDLE, &m_imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Swapchain out of date");
    }
    VK_CHECK(result);

    VkCommandBuffer cmd = m_commandBuffers[m_currentFrame];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin));

    VkClearValue clearValues[2];
    clearValues[0].color = {{0.5f, 0.7f, 1.0f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rpBegin{};
    rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBegin.renderPass = m_ctx->renderPass();
    rpBegin.framebuffer = m_ctx->framebuffers()[m_imageIndex];
    rpBegin.renderArea.offset = {0, 0};
    rpBegin.renderArea.extent = m_ctx->swapchainExtent();
    rpBegin.clearValueCount = 2;
    rpBegin.pClearValues = clearValues;

    vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = static_cast<float>(m_ctx->swapchainExtent().width);
    viewport.height = static_cast<float>(m_ctx->swapchainExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = m_ctx->swapchainExtent();
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    updateUniforms();
}

void VulkanRenderer::endFrame() {
    VkCommandBuffer cmd = m_commandBuffers[m_currentFrame];
    vkCmdEndRenderPass(cmd);
    VK_CHECK(vkEndCommandBuffer(cmd));

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {m_imageAvailable[m_currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = waitSemaphores;
    submit.pWaitDstStageMask = waitStages;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;

    VkSemaphore signalSemaphores[] = {m_renderFinished[m_currentFrame]};
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = signalSemaphores;

    VK_CHECK(vkQueueSubmit(m_ctx->graphicsQueue(), 1, &submit, m_inFlightFences[m_currentFrame]));

    VkPresentInfoKHR present{};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = signalSemaphores;
    present.swapchainCount = 1;
    present.pSwapchains = &m_ctx->swapchain();
    present.pImageIndices = &m_imageIndex;

    VkResult result = vkQueuePresentKHR(m_ctx->presentQueue(), &present);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Swapchain out of date");
    }
    VK_CHECK(result);

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanRenderer::drawMesh(const VulkanMesh& mesh, const glm::mat4& model) {
    if (!mesh.valid()) return;
    VkCommandBuffer cmd = m_commandBuffers[m_currentFrame];
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_opaquePipeline->pipeline());
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_opaquePipeline->layout(), 0, 1,
                            &m_descriptorSets[m_currentFrame], 0, nullptr);
    vkCmdPushConstants(cmd, m_opaquePipeline->layout(), VK_SHADER_STAGE_VERTEX_BIT,
                       0, sizeof(glm::mat4), &model);
    mesh.bind(cmd);
    mesh.draw(cmd);
}

void VulkanRenderer::drawMeshTransparent(const VulkanMesh& mesh, const glm::mat4& model) {
    if (!mesh.valid()) return;
    VkCommandBuffer cmd = m_commandBuffers[m_currentFrame];
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_transparentPipeline->pipeline());
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_transparentPipeline->layout(), 0, 1,
                            &m_descriptorSets[m_currentFrame], 0, nullptr);
    vkCmdPushConstants(cmd, m_transparentPipeline->layout(), VK_SHADER_STAGE_VERTEX_BIT,
                       0, sizeof(glm::mat4), &model);
    mesh.bind(cmd);
    mesh.draw(cmd);
}

void VulkanRenderer::drawMeshDebug(const VulkanMesh& mesh, const glm::mat4& model) {
    if (!mesh.valid()) return;
    VkCommandBuffer cmd = m_commandBuffers[m_currentFrame];
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_debugPipeline->pipeline());
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_debugPipeline->layout(), 0, 1,
                            &m_descriptorSets[m_currentFrame], 0, nullptr);
    vkCmdPushConstants(cmd, m_debugPipeline->layout(), VK_SHADER_STAGE_VERTEX_BIT,
                       0, sizeof(glm::mat4), &model);
    mesh.bind(cmd);
    mesh.draw(cmd);
}

void VulkanRenderer::updateUniforms() {
    UniformBufferObject ubo{};
    ubo.view = m_camera->viewMatrix();
    float aspect = (float)m_ctx->swapchainExtent().width / (float)m_ctx->swapchainExtent().height;
    ubo.proj = m_camera->projectionMatrix(aspect, m_camera->fov());
    ubo.proj[1][1] *= -1;
    ubo.fogColor = m_underwater
        ? glm::vec4(0.05f, 0.15f, 0.35f, 0.8f)
        : glm::vec4(0.5f, 0.7f, 1.0f, 1.0f);
    ubo.cameraPos = glm::vec4(m_camera->position(), 1.0f);
    ubo.time = 0.0f;
    ubo.gamma = m_gamma;
    ubo.alphaDiscard = 0.05f;

    memcpy(m_uniformMapped[m_currentFrame], &ubo, sizeof(ubo));
}

void VulkanRenderer::waitIdle() {
    vkDeviceWaitIdle(m_ctx->device());
}

void VulkanRenderer::createDescriptorLayout() {
    std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
    // Binding 0: Uniform buffer
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    // Binding 1: Texture sampler
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = static_cast<u32>(bindings.size());
    info.pBindings = bindings.data();
    VK_CHECK(vkCreateDescriptorSetLayout(m_ctx->device(), &info, nullptr, &m_descriptorLayout));
}

void VulkanRenderer::createDescriptorPool() {
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = MAX_FRAMES_IN_FLIGHT;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = MAX_FRAMES_IN_FLIGHT;

    VkDescriptorPoolCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info.maxSets = MAX_FRAMES_IN_FLIGHT;
    info.poolSizeCount = static_cast<u32>(poolSizes.size());
    info.pPoolSizes = poolSizes.data();
    VK_CHECK(vkCreateDescriptorPool(m_ctx->device(), &info, nullptr, &m_descriptorPool));
}

void VulkanRenderer::createDescriptorSets() {
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_descriptorLayout);

    VkDescriptorSetAllocateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.descriptorPool = m_descriptorPool;
    info.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
    info.pSetLayouts = layouts.data();
    VK_CHECK(vkAllocateDescriptorSets(m_ctx->device(), &info, m_descriptorSets));

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo bufInfo{};
        bufInfo.buffer = m_uniformBuffers[i];
        bufInfo.offset = 0;
        bufInfo.range = sizeof(UniformBufferObject);

        VkWriteDescriptorSet writes[2]{};
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = m_descriptorSets[i];
        writes[0].dstBinding = 0;
        writes[0].descriptorCount = 1;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[0].pBufferInfo = &bufInfo;

        if (m_texMgr && m_texMgr->sampler()) {
            VkDescriptorImageInfo imgInfo = m_texMgr->descriptorInfo();
            writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[1].dstSet = m_descriptorSets[i];
            writes[1].dstBinding = 1;
            writes[1].descriptorCount = 1;
            writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[1].pImageInfo = &imgInfo;

            vkUpdateDescriptorSets(m_ctx->device(), 2, writes, 0, nullptr);
        } else {
            vkUpdateDescriptorSets(m_ctx->device(), 1, writes, 0, nullptr);
        }
    }
}

void VulkanRenderer::createUniformBuffers() {
    VkDeviceSize size = sizeof(UniformBufferObject);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkBufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        info.size = size;
        info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VK_CHECK(vkCreateBuffer(m_ctx->device(), &info, nullptr, &m_uniformBuffers[i]));

        VkMemoryRequirements req;
        vkGetBufferMemoryRequirements(m_ctx->device(), m_uniformBuffers[i], &req);

        VkMemoryAllocateInfo alloc{};
        alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc.allocationSize = req.size;
        alloc.memoryTypeIndex = m_ctx->findMemoryType(
            req.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        VK_CHECK(vkAllocateMemory(m_ctx->device(), &alloc, nullptr, &m_uniformMemories[i]));
        vkBindBufferMemory(m_ctx->device(), m_uniformBuffers[i], m_uniformMemories[i], 0);
        vkMapMemory(m_ctx->device(), m_uniformMemories[i], 0, size, 0, &m_uniformMapped[i]);
    }
}

void VulkanRenderer::createCommandBuffers() {
    VkCommandBufferAllocateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.commandPool = m_ctx->commandPool();
    info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    info.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
    VK_CHECK(vkAllocateCommandBuffers(m_ctx->device(), &info, m_commandBuffers));
}

void VulkanRenderer::createSyncObjects() {
    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VK_CHECK(vkCreateSemaphore(m_ctx->device(), &semInfo, nullptr, &m_imageAvailable[i]));
        VK_CHECK(vkCreateSemaphore(m_ctx->device(), &semInfo, nullptr, &m_renderFinished[i]));
        VK_CHECK(vkCreateFence(m_ctx->device(), &fenceInfo, nullptr, &m_inFlightFences[i]));
    }
}

void VulkanRenderer::createPipeline() {
    VkPushConstantRange pcRange{};
    pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pcRange.offset = 0;
    pcRange.size = sizeof(glm::mat4);

    auto attribs = Vertex::attribDesc();
    std::vector<VkVertexInputAttributeDescription> attribVec(
        attribs.begin(), attribs.end());

    m_opaquePipeline = std::make_unique<VulkanPipeline>();
    m_opaquePipeline->init(m_ctx,
        "shaders/block.vert.spv", "shaders/block.frag.spv",
        Vertex::bindingDesc(), attribVec,
        m_descriptorLayout, pcRange, true, false);

    m_transparentPipeline = std::make_unique<VulkanPipeline>();
    m_transparentPipeline->init(m_ctx,
        "shaders/block.vert.spv", "shaders/block.frag.spv",
        Vertex::bindingDesc(), attribVec,
        m_descriptorLayout, pcRange, true, true);

    m_debugPipeline = std::make_unique<VulkanPipeline>();
    m_debugPipeline->init(m_ctx,
        "shaders/block.vert.spv", "shaders/block.frag.spv",
        Vertex::bindingDesc(), attribVec,
        m_descriptorLayout, pcRange,
        false, true, VK_CULL_MODE_NONE,
        VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
}
