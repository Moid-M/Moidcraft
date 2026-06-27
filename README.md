# MoidCraft

A voxel-based sandbox game built from scratch with C++20 and Vulkan 1.3.

## Features

- **Procedural terrain** — infinite world with 6 biomes (Plains, Forest, Desert, Taiga, Mountains, Ocean)
- **Tree generation** — Oak and Spruce variants naturally placed
- **Vulkan 1.3 rendering** — MSAA, mipmapping, anisotropic filtering, distance fog, gamma correction
- **First-person controls** — WASD movement, jump, flying mode, smooth collision
- **Block interaction** — DDA raycasting for break/place (up to 8 blocks)
- **Inventory system** — 36-slot inventory with 9-slot hotbar and crafting table
- **Full UI** — Main menu, pause menu, settings screen with sliders & toggles
- **F3 debug overlay** — FPS, 1%/0.1% percentile lows, coordinates, render distance
- **Resource pack loading** — Loads vanilla `.zip` packs with procedural fallback
- **Settings persistence** — Render distance, brightness, sensitivity, FOV, vsync saved to `options.txt`

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
./moidcraft
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
| Escape | Open pause menu |
| F3 | Toggle debug overlay |

## Resource Packs

Drop a vanilla `.zip` resource pack in the project root or `resourcepacks/` directory. The game will automatically find and load it. If no pack is found, procedural fallback textures are generated.

## Project Structure

```
src/
  core/       - Engine, Input, Camera, Window, Settings, TextureManager
  vulkan/     - VulkanContext, VulkanRenderer, VulkanPipeline, VulkanMesh
  world/      - World, Chunk, ChunkMesh, TerrainGen, Block, TreeBlueprint
  player/     - Player movement, collision, interaction
  inventory/  - Inventory, CraftingTable
  ui/         - Menu system (MenuState, PauseState, SettingsState, UIRenderer)
extern/       - Third-party headers/libs (glm, stb, glfw, FastNoiseLite)
shaders/      - GLSL shaders compiled to SPIR-V
textures/     - UI textures (font, buttons, backgrounds)
```

## License

MIT License — see [LICENSE](LICENSE) for details.
