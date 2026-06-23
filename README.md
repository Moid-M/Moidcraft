# MoidCraft

A Minecraft clone built with C++20 and Vulkan. This is an open-source project.

## Features

- Procedurally generated terrain with trees (FastNoiseLite)
- Vulkan 1.3 rendering with MSAA, mipmapping, fog, and transparency
- Per-axis AABB collision with block-boundary snapping
- DDA voxel raycasting for block break/place (up to 8 blocks)
- Hotbar with item selection (scroll wheel / 1-9 keys)
- Texture atlas from vanilla Minecraft `.zip` resource packs
- Fallback procedural textures when no resource pack is found
- Flying mode, jump holding, smooth movement

## Building

### Prerequisites

- C++20 compiler (GCC 11+, Clang 14+)
- CMake 3.20+
- Vulkan SDK (1.3+)
- X11 development libraries
- zlib development library

### Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
./minecraft
```

Enable Vulkan validation layers with `-DENABLE_VALIDATION_LAYERS=ON`.

## Controls

| Key | Action |
|---|---|
| WASD | Move |
| Space | Jump (hold for auto-jump) |
| Shift | Descend (creative flight) |
| F | Toggle flying |
| 1-9 | Select hotbar slot |
| Scroll | Cycle hotbar slot |
| Left click | Break block |
| Right click | Place block from hotbar |
| Escape | Toggle cursor capture |

## Resource Packs

Drop a vanilla Minecraft `.zip` resource pack in the project root or `resourcepacks/` directory. The game will automatically find and load it. If no pack is found, procedural fallback textures are generated.

## Project Structure

```
src/
  core/       - Engine, Input, Camera, TextureManager, Window
  vulkan/     - VulkanContext, VulkanRenderer, VulkanPipeline, VulkanMesh
  world/      - World, Chunk, ChunkMesh, TerrainGen, Block
  player/     - Player movement, collision, interaction
  inventory/  - Inventory, CraftingTable
  ui/         - Menu system (WIP)
extern/       - Third-party headers (glm, stb, glfw, FastNoiseLite)
shaders/      - GLSL shaders compiled to SPIR-V
```

## License

MIT License - see [LICENSE](LICENSE) for details.
