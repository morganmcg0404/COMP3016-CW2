# COMP3016 CW2 - Voxel Terrain Engine

A 3D voxel-based terrain generation engine built with OpenGL, featuring procedurally generated Minecraft-style terrain with chunk-based rendering, camera controls, and Perlin noise terrain generation. Built with GLFW, GLAD, Assimp, and GLM for Visual Studio 2022.

## Features

- **Voxel-Based Terrain**: Minecraft-style block-based world generation
- **Chunk System**: Efficient chunk-based rendering with configurable render distance
- **Procedural Generation**: Perlin noise-based terrain generation with multiple biomes
- **Camera System**: First-person camera with keyboard (WASD) and mouse controls
- **Lighting**: Phong lighting model with ambient, diffuse, and specular components
- **Performance Optimized**: Chunk management system for efficient memory usage
- **Modern OpenGL**: Uses OpenGL 4.3 Core Profile with custom shaders

## Project Structure

```
COMP3016-CW2/
├── dependencies/          # External libraries
│   ├── GLFW/             # Window and input management
│   ├── GLAD/             # OpenGL function loader
│   ├── Assimp/           # 3D model loading (for future use)
│   └── GLM/              # Mathematics library (vectors, matrices)
├── include/              # Header files
│   ├── Game.h           # Main game class
│   ├── Camera.h         # First-person camera implementation
│   ├── Shader.h         # Shader program wrapper
│   ├── Block.h          # Block type definitions
│   ├── Chunk.h          # Chunk data structure (16x256x16 blocks)
│   ├── ChunkManager.h   # Chunk loading/unloading system
│   └── TerrainGenerator.h  # Perlin noise terrain generation
├── src/                  # Source files
│   ├── main.cpp         # Entry point, window setup, main loop
│   └── Game.cpp         # Game logic, rendering, input handling
├── resources/            # Game resources
│   └── shaders/         # GLSL shaders
│       ├── basic.vert   # Vertex shader (transforms, lighting prep)
│       └── basic.frag   # Fragment shader (Phong lighting)
├── lib/                  # Additional library files
└── bin/                  # Build output (generated)
```

## Dependencies Setup

### 1. GLFW (Window and Input Management)
- **Download:** https://www.glfw.org/download.html
- **Version:** 3.3.8 or later (64-bit Windows binaries)
- **Status:** ✅ Already installed
- **Setup:**
  1. Download the 64-bit Windows pre-compiled binaries
  2. Extract to `dependencies/GLFW/`
  3. Ensure the following structure:
     ```
     dependencies/GLFW/
     ├── include/GLFW/
     └── lib-vc2022/glfw3.lib
     ```

### 2. GLAD (OpenGL Loader)
- **Download:** https://glad.dav1d.de/
- **Configuration:**
  - Language: C/C++
  - Specification: OpenGL
  - API gl: Version 4.3 (Core Profile)
  - Generate a loader: ✓
- **Status:** ✅ Already installed
- **Setup:**
  1. Download the generated files
  2. Extract to `dependencies/GLAD/`
  3. Ensure the following structure:
     ```
     dependencies/GLAD/
     ├── include/
     │   ├── glad/glad.h
     │   └── KHR/khrplatform.h
     └── src/glad.c
     ```

### 3. Assimp (Model Loading)
- **Download:** https://github.com/assimp/assimp/releases
- **Version:** 5.3.1 or later
- **Status:** ✅ Already installed
- **Note:** Currently included for future model loading features
- **Setup:**
  1. Download the pre-built libraries for Visual Studio 2022 (64-bit)
  2. Extract to `dependencies/Assimp/`
  3. Ensure the following structure:
     ```
     dependencies/Assimp/
     ├── include/assimp/
     ├── lib/x64/
     │   └── assimp-vc143-mt.lib
     └── bin/x64/
         └── assimp-vc143-mt.dll
     ```

### 4. GLM (Mathematics Library - Header Only)
- **Download:** https://github.com/g-truc/glm/releases
- **Version:** 0.9.9.8 or later
- **Status:** ✅ Already installed
- **Setup:**
  1. Download the source code
  2. Extract to `dependencies/GLM/`
  3. Ensure the following structure:
     ```
     dependencies/GLM/
     └── glm/
         ├── glm.hpp
         ├── gtc/
         ├── gtx/
         └── (other headers)
     ```

## Building the Project

### Prerequisites
- **Visual Studio 2022** with C++ Desktop Development workload
- **Windows 10/11** (64-bit)
- All dependencies installed (see above)

### Build Steps

1. **Open the solution:**
   - Open `COMP3016-CW2.sln` in Visual Studio 2022

2. **Select configuration:**
   - Choose `Debug` or `Release` configuration
   - Ensure platform is set to `x64` (64-bit)

3. **Build:**
   - Press `F7` or go to `Build > Build Solution`
   - The project should compile without errors

4. **Run:**
   - Press `F5` to run with debugging
   - Or press `Ctrl+F5` to run without debugging

### Expected Output
- A 1280x720 window should open displaying the voxel terrain
- You should see procedurally generated terrain with lighting
- Camera should respond to mouse and keyboard input

## Controls

- **W/A/S/D**: Move camera forward/left/backward/right
- **Mouse Movement**: Look around (first-person view)
- **ESC**: Exit the application

## Technical Details

### Chunk System
- Each chunk is **16x256x16** blocks
- Chunks generate dynamically based on camera position
- Configurable render distance (default: 20 chunks)
- Efficient memory management with chunk loading/unloading

### Terrain Generation
- Uses **Perlin noise** algorithm for natural-looking terrain
- Multiple noise octaves for varied terrain features
- Height-based block type selection (grass, dirt, stone)
- Seamless chunk boundaries

### Rendering
- **OpenGL 4.3 Core Profile**
- Custom vertex and fragment shaders
- **Phong lighting model** with:
  - Ambient lighting (30% strength)
  - Diffuse lighting (directional)
  - Specular highlights (20% strength, shininess: 16)
- Block face culling for performance

### Camera System
- First-person perspective
- Smooth mouse-based rotation
- WASD movement with adjustable speed (default: 15 units/sec)
- Proper view and projection matrices

## Code Architecture

### Core Classes

**Game** (`Game.h`, `Game.cpp`)
- Main game loop and state management
- Input processing (keyboard and mouse)
- Rendering coordination
- Camera and shader management

**Camera** (`Camera.h`)
- First-person camera implementation
- View matrix calculation
- Mouse and keyboard input processing

**Shader** (`Shader.h`)
- GLSL shader program wrapper
- Compile and link vertex/fragment shaders
- Uniform variable management

**Block** (`Block.h`)
- Block type enumeration (Air, Grass, Dirt, Stone)
- Block properties and definitions

**Chunk** (`Chunk.h`)
- 16x256x16 block data structure
- Mesh generation from block data
- OpenGL VAO/VBO management

**ChunkManager** (`ChunkManager.h`)
- Manages multiple chunks
- Chunk generation around camera
- Chunk loading/unloading based on distance

**TerrainGenerator** (`TerrainGenerator.h`)
- Perlin noise implementation
- Height map generation
- Block type assignment based on height

## Important Notes

### Library Versions
- Make sure all libraries are **64-bit (x64)** versions
- Ensure libraries are built with compatible Visual Studio versions (VS2022/v143 toolset)
- OpenGL 4.3+ support required (most modern GPUs support this)

### Project Configuration
The project is already configured with:
- Include directories pointing to all dependency headers
- Library directories for GLFW and Assimp
- Proper linker settings for required `.lib` files
- OpenGL libraries (opengl32.lib)

### Common Issues

**Link errors:** 
- Verify all `.lib` files are in the correct directories
- Check that library names in the project match the actual file names
- Ensure you're building for x64 platform

**DLL not found errors:**
- Check if `assimp-vc143-mt.dll` is in the output directory
- The post-build event should copy DLLs automatically
- Manually copy from `dependencies/Assimp/bin/x64/` to `bin/Debug/` if needed

**Include errors:**
- Verify all include directories are correctly set in project properties
- Check that header files exist in `dependencies/*/include/` paths
- Rebuild the solution after modifying include paths

**OpenGL errors:**
- Ensure your graphics drivers are up to date
- Verify OpenGL 4.3+ support on your GPU
- Check that GLAD is properly initialized before OpenGL calls

**Performance issues:**
- Reduce render distance in `ChunkManager` initialization (Game.cpp)
- Switch to Release configuration for better performance
- Check GPU usage and driver updates

### Additional Configuration

If you need to modify include/library paths:
1. Right-click the project in Solution Explorer
2. Select `Properties`
3. Modify paths under:
   - `C/C++ > General > Additional Include Directories`
   - `Linker > General > Additional Library Directories`
   - `Linker > Input > Additional Dependencies`

Current configuration includes:
- **Include Directories:**
  - `dependencies/GLFW/include`
  - `dependencies/GLAD/include`
  - `dependencies/Assimp/include`
  - `dependencies/GLM`
  
- **Library Directories:**
  - `dependencies/GLFW/lib-vc2022`
  - `dependencies/Assimp/lib/x64`
  
- **Linked Libraries:**
  - `glfw3.lib`
  - `opengl32.lib`
  - `assimp-vc143-mt.lib`

## Future Enhancement Possibilities

- **Physics**: Add collision detection and physics simulation
- **Textures**: Implement texture atlas for block textures
- **Biomes**: Expand terrain generation with multiple biomes
- **Water**: Add transparent water blocks
- **Cave Generation**: Implement cave systems using 3D noise
- **Block Placement/Destruction**: Add player interaction with blocks
- **Lighting**: Implement dynamic lighting and shadows
- **Multiplayer**: Network functionality for multiplayer support
- **Audio**: Add ambient sounds and music (irrKlang integration)
- **Optimization**: Implement frustum culling and LOD system

## Development Workflow

1. **Add new source files:**
   - Add `.cpp` files to the `src/` directory
   - Add `.h` files to the `include/` directory
   - Right-click project in Solution Explorer > Add > Existing Item

2. **Modify shaders:**
   - Edit `resources/shaders/basic.vert` for vertex processing
   - Edit `resources/shaders/basic.frag` for fragment/pixel processing
   - Changes take effect on next program run (shaders are loaded at runtime)

3. **Adjust terrain generation:**
   - Modify `TerrainGenerator.h` for different terrain algorithms
   - Adjust noise parameters for different terrain styles
   - Change block type assignments for varied landscapes

4. **Modify chunk settings:**
   - Adjust chunk render distance in `Game.cpp` (ChunkManager initialization)
   - Modify chunk dimensions in `Chunk.h` (currently 16x256x16)
   - Update mesh generation logic for different block arrangements

## Learning Resources

### OpenGL & Graphics Programming
- **LearnOpenGL**: https://learnopengl.com/ - Comprehensive OpenGL tutorial
- **OpenGL Reference**: https://docs.gl/ - OpenGL function documentation
- **GLFW Documentation**: https://www.glfw.org/documentation.html
- **GLM Documentation**: https://github.com/g-truc/glm/blob/master/manual.md

### Voxel Engine Development
- **Minecraft Rendering**: Understanding chunk-based systems
- **Perlin Noise**: https://en.wikipedia.org/wiki/Perlin_noise
- **Voxel Engines**: Research papers on efficient voxel rendering

### Library References
- **Assimp**: https://assimp-docs.readthedocs.io/ - 3D model loading
- **OpenGL Shading Language (GLSL)**: Shader programming reference

### Game Development
- **Game Programming Patterns**: http://gameprogrammingpatterns.com/
- **Real-Time Rendering**: Advanced rendering techniques

## Project Timeline & Milestones

### Completed Features ✅
- [x] Project setup with all dependencies
- [x] Window creation and OpenGL context
- [x] Camera system with first-person controls
- [x] Shader system (vertex and fragment shaders)
- [x] Phong lighting implementation
- [x] Block data structure
- [x] Chunk system (16x256x16 blocks)
- [x] Chunk manager with render distance
- [x] Perlin noise terrain generation
- [x] Mesh generation from block data
- [x] Dynamic chunk loading/unloading
- [x] Input handling (keyboard and mouse)

### In Development 🚧
- [ ] Texture mapping for blocks
- [ ] Block interaction (place/destroy)
- [ ] Improved terrain features

### Future Considerations 💭
- [ ] Physics integration
- [ ] Audio system

## Performance Metrics

### Current Performance Characteristics
- **Render Distance**: 20 chunks (configurable)
- **Blocks per Chunk**: 16 × 256 × 16 = 65,536 blocks
- **Vertex Count**: Varies based on visible faces (face culling implemented)
- **Target Frame Rate**: 60+ FPS on modern hardware

### Optimization Techniques Used
- **Face Culling**: Only renders visible block faces
- **Chunk Batching**: Each chunk is a single draw call
- **Frustum Culling**: Only renders chunks in view (chunk-level)
- **Efficient Data Structures**: Optimized block storage

## Troubleshooting Guide

### Application won't start
1. Verify all DLLs are in the output directory (`bin/Debug/` or `bin/Release/`)
2. Check that you're running the correct platform configuration (x64)
3. Ensure graphics drivers are up to date
4. Verify OpenGL 4.3+ support on your system

### Black screen or no terrain visible
1. Check console output for shader compilation errors
2. Verify shader files exist in `resources/shaders/`
3. Ensure camera position is correct (default: 0, 20, 0)
4. Check that chunks are being generated (debug output)

### Low FPS or performance issues
1. Reduce render distance in `Game.cpp` (ChunkManager initialization)
2. Build in Release configuration instead of Debug
3. Update graphics drivers
4. Check GPU utilization in Task Manager
5. Reduce chunk generation frequency

### Compilation errors
1. Verify all include paths are correct
2. Check that all dependencies are properly installed
3. Clean solution and rebuild (`Build > Clean Solution`, then `Build > Rebuild Solution`)
4. Ensure C++17 standard is selected in project properties

## Author

Morgan McGovern - COMP3016 Coursework 2

## Acknowledgments

- **LearnOpenGL.com** - For excellent OpenGL tutorials
- **GLFW, GLAD, GLM, Assimp** - Open source libraries that made this possible
- **Perlin Noise Algorithm** - Ken Perlin's procedural generation technique
- **University of Plymouth** - COMP3016 Course Materials

## License

This is a coursework project for COMP3016 at the University of Plymouth.

---

**Last Updated**: November 2025  
**Project Status**: Active Development  
**Build Status**: ✅ Compiling Successfully
