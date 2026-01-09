# COMP3016 CW2 - Voxel Terrain Engine
**Morgan McGovern | University of Plymouth**

A procedurally generated voxel terrain engine built with OpenGL 4.3, featuring dynamic lighting, shadow mapping, multi-biome terrain generation, and interactive gameplay elements.

---

## 📹 Video Demonstration

**YouTube Link:** https://www.youtube.com/watch?v=M-ZeBNPMJPQ

## 🎮 Gameplay Description

This is a first-person voxel exploration game with procedurally generated infinite terrain. Players spawn in a world featuring three distinct biomes (Grassland, Birch Forest, Desert) with dynamic day/night cycles and realistic shadows.

### Core Gameplay Features
- **Terrain Exploration**: Infinite procedurally generated world with seamless chunk streaming
- **Block Destruction**: Left-click to break blocks (5-block reach) with raycast detection
- **Movement System**: WASD controls with sprint (Ctrl), crouch (Shift), and jump (Space)
- **Day/Night Cycle**: 360-second cycle with moving sun/moon and dynamic lighting
- **Interactive GUI**: Press 'G' to adjust FOV, mouse sensitivity, time speed, and pet scale
- **AI Mobs**: Press 'M' to spawn wandering NPCs (Ben model) with pathfinding
- **Pet Companion**: Followable pet (Tom model) that can sit/stand with 'C' key
- **Hotbar System**: Number keys 1-9 to select different block types

### Player Controls
| Input | Action |
|-------|--------|
| **W/A/S/D** | Move forward/left/backward/right |
| **Mouse** | Look around (first-person view) |
| **Left Click** | Destroy block |
| **Space** | Jump |
| **Left Ctrl** | Toggle sprint |
| **Left Shift** | Crouch |
| **G** | Toggle GUI |
| **M** | Spawn mob |
| **C** | Toggle pet sit/stand |
| **1-9** | Select hotbar slot |
| **ESC** | Exit |

---

## 📦 Dependencies Used

### Core Libraries
1. **GLFW 3.3.8** - Window management and input handling
   - Cross-platform window creation
   - Keyboard/mouse callbacks
   - OpenGL context initialization

2. **GLAD (OpenGL 4.3 Core)** - OpenGL function loader
   - Dynamic loading of modern OpenGL functions
   - Core profile support for optimal performance

3. **GLM 0.9.9.8** - Mathematics library (header-only)
   - Vector/matrix operations (vec3, mat4)
   - Camera transformations (lookAt, perspective)
   - Used throughout for all 3D math

4. **Assimp 5.3.1** - 3D model loading
   - FBX model importing (TomAdult.fbx for pet, Ben for mobs)
   - Mesh processing and vertex data extraction

5. **Windows GDI+** - Image loading
   - PNG/JPG texture loading without external dependencies

### Build Environment
- **Visual Studio 2022** (Platform Toolset v143)
- **C++17 Standard**
- **Windows SDK** for platform-specific functionality

---

## 🤖 Use of AI

- **Code Generation**: Used AI to generate most of the code
- **Debugging**: Used AI to read through errors and fix any bugs found
- **Documentation**: Used to setup ReadMe structure. Used to write code comments

### Problems Encountered
- The AI would sometimes get stuck in a loop giving me 2 broken solutions on repeat until I told it to try something different.
- Sometimes would not understand what I wanted it to do for the procedural terrain generation so I had to give up.

### What Went Well
- In the first coursework the AI struggled because I was using a recently updated version of SDL, this time I did not encounter this problem as the AI had a lot of training on the used dependencies. This made the development very quick with most features only taking around an hour maximum to develop.

---

## 🏗️ Game Programming Patterns

### 1. **Component Pattern**
**Location**: Camera, Shader, ChunkManager classes

Separates concerns into independent, reusable components. Each class handles one specific responsibility.

```cpp
class Game {
    std::unique_ptr<Camera> m_camera;         // View transformations
    std::unique_ptr<Shader> m_shader;         // Shader management
    std::unique_ptr<ChunkManager> m_chunkManager; // World management
};
```

### 2. **Object Pool Pattern**
**Location**: `ChunkManager.h` (lines 26-84)

Reuses chunk objects instead of constant allocation/deallocation. Chunks are loaded within render distance and unloaded when far away, preventing memory fragmentation.

### 3. **Factory Pattern**
**Location**: `TerrainGenerator.h` (lines 495-623)

`GenerateChunk()` encapsulates complex chunk creation logic into a single entry point. Handles biome determination, height generation, and block placement.

### 4. **Observer Pattern**
**Location**: `main.cpp` (lines 23-59)

GLFW callbacks notify the game of input events. Mouse and keyboard events propagate to the appropriate handlers without tight coupling.

### 5. **Singleton Pattern**
**Location**: `TerrainGenerator.h` (lines 18-34)

Static world seed ensures consistent terrain generation across all chunk boundaries. Single source of truth for procedural generation.

### 6. **State Pattern**
**Location**: `Game.cpp` (lines 166-173, 253-257)

Different input handling based on game state. When GUI is open (`m_showGUI == true`), gameplay inputs are disabled and GUI interactions are enabled.

---

## ⚙️ Game Mechanics Implementation

### 1. Procedural Terrain Generation
**How it works**: Multi-octave Perlin noise creates natural-looking height variations. Biomes are determined by hashing chunk coordinates into regions. The way it is currently implemented does not work very well with issues with smooth chunk variation, currently there are lots of height variation issues.

**Code** (`TerrainGenerator.h`):
```cpp
float baseHeight = PerlinNoise(worldX, worldZ);      // Large features
float hills = HillsNoise(worldX, worldZ);            // Medium hills
float detail = DetailNoise(worldX, worldZ);          // Small details

float combined = (baseHeight + hills + detail + 1.5f) / 3.5f;
float smooth = smoothstep(smoothstep(combined));     // Double smoothstep
int terrainHeight = 12 + (int)(smooth * 16.0f);      // Map to blocks
```

### 2. Shadow Mapping with PCF
**How it works**: Two-pass rendering. First pass renders from light's perspective to depth texture. Second pass checks if fragment is in shadow using 3×3 PCF filtering.

**Code** (`basic.frag`):
```cpp
float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w * 0.5 + 0.5;
    float bias = max(0.001 * (1.0 - dot(normal, lightDir)), 0.0005);
    
    // PCF: Sample 3x3 grid
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            float depth = texture(shadowMap, projCoords.xy + vec2(x,y) * texelSize).r;
            shadow += (projCoords.z - bias > depth) ? 1.0 : 0.0;
        }
    }
    return shadow / 9.0;
}
```

### 3. Chunk Streaming System
**How it works**: Only loads chunks within configurable render distance. Circular distance calculation ensures smooth loading. Chunks unload when camera moves away.

**Code** (`ChunkManager.h`):
```cpp
void Update(const Camera& camera) {
    int camChunkX = (int)floor(camera.Position.x / CHUNK_SIZE);
    int camChunkZ = (int)floor(camera.Position.z / CHUNK_SIZE);
    
    for (int x = camChunkX - renderDistance; x <= camChunkX + renderDistance; x++) {
        for (int z = camChunkZ - renderDistance; z <= camChunkZ + renderDistance; z++) {
            float distance = sqrt(dx*dx + dz*dz);
            if (distance <= m_renderDistance && !chunkExists) {
                CreateChunk(x, z);
            }
        }
    }
    // Unload distant chunks...
}
```

### 4. Dynamic Day/Night Cycle
**How it works**: Time progresses continuously. Sun/moon positions calculated using trigonometry. Light color transitions between warm daylight and cool moonlight.

**Code** (`Game.cpp` lines 891-983):
```cpp
m_timeOfDay += deltaTime * m_timeSpeed / 360.0f;
float sunAngle = m_timeOfDay * 2.0f * PI;
float sunHeight = sin(sunAngle);
float intensity = clamp((sunHeight + 0.3f) / 1.3f, 0.1f, 1.0f);

m_lightColor = (sunHeight > 0) 
    ? glm::vec3(1.0f, 0.95f, 0.8f) * intensity   // Warm day
    : glm::vec3(0.6f, 0.7f, 1.0f) * intensity;   // Cool night
```

### 5. Block Raycasting
**How it works**: Ray steps along camera's forward vector checking for solid blocks every 0.1 units. Maximum reach of 5 blocks. Triggers hand animation on hit.

**Code** (`Game.cpp` lines 704-764):
```cpp
glm::vec3 rayPos = m_camera->Position;
glm::vec3 rayDir = m_camera->Front;

for (float dist = 0.0f; dist < 5.0f; dist += 0.1f) {
    glm::vec3 checkPos = rayPos + rayDir * dist;
    if (m_chunkManager->IsSolid(checkPos)) {
        m_chunkManager->DestroyBlock(checkPos);
        m_isSwinging = true;
        break;
    }
}
```

### 6. Physics & Collision Detection
**How it works**: Gravity applied every frame. Ground detection at feet position. Four-corner collision checks for horizontal movement prevent clipping. Currently issues when the player is trying to climb a staircase and there is a block above the player, they can get stuck inside of a block or be teleported back to where they were.

**Code** (`Game.cpp` lines 336-449):
```cpp
// Apply gravity
m_camera->Velocity.y += GRAVITY * deltaTime;
m_camera->Position += m_camera->Velocity * deltaTime;

// Ground collision
glm::vec3 feetPos = m_camera->Position - glm::vec3(0, PLAYER_EYE_HEIGHT, 0);
if (m_chunkManager->IsSolid(feetPos.x, feetPos.y - 0.1f, feetPos.z)) {
    m_camera->IsGrounded = true;
    m_camera->Velocity.y = 0.0f;
    m_camera->Position.y = floor(feetPos.y) + PLAYER_EYE_HEIGHT;
}
```

---

## 🛡️ Exception Handling & Test Cases

### Exception Handling

#### 1. Shader Compilation Errors
**Location**: `Game.cpp` lines 68-77
```cpp
try {
    m_shader = std::make_unique<Shader>("resources/shaders/basic.vert", 
                                        "resources/shaders/basic.frag");
} catch (const std::exception& e) {
    std::cerr << "Shader load failed: " << e.what() << std::endl;
    return false;
}
```

#### 2. Model Loading Safety
**Location**: `Game.cpp` lines 2204-2220
```cpp
const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate);
if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
    std::cerr << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
    return;
}
```

#### 3. OpenGL Resource Validation
**Location**: `Game.cpp` lines 820-879
```cpp
if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    std::cerr << "ERROR: Shadow framebuffer incomplete!" << std::endl;
}
```

#### 4. Bounds Checking
**Location**: `Chunk.h`
```cpp
void Chunk::SetBlock(int x, int y, int z, BlockType type) {
    if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_HEIGHT || 
        z < 0 || z >= CHUNK_SIZE) return;
    blocks[x][y][z].type = type;
}
```

#### 5. Memory Management
**Location**: `Game.h`, `Game.cpp` lines 805-815
- RAII with `std::unique_ptr` for automatic cleanup
- Explicit OpenGL resource deletion in destructor

### Test Cases

| # | Test | Expected Result | Status |
|---|------|----------------|--------|
| 1 | **Terrain Consistency** - Same seed generates identical chunks | ✅ Reproducible terrain | **PASSED** |
| 2 | **Chunk Boundaries** - No seams between adjacent chunks | ✅ Seamless transitions | **PASSED** |
| 3 | **Collision Detection** - Camera doesn't clip through blocks | ⚠️ Works on surface | **PARTIAL** - Underground glitches |
| 4 | **Chunk Streaming** - Load/unload based on distance | ✅ Only nearby chunks in memory | **PASSED** |
| 5 | **Raycast Accuracy** - Block destruction hits correct position | ✅ Accurate targeting | **PASSED** |
| 6 | **Shadow Direction** - Shadows opposite to sun/moon | ✅ Directionally correct | **PASSED** |
| 7 | **GUI Interaction** - Sliders update parameters in real-time | ✅ Immediate feedback | **PASSED** |
| 8 | **Biome Blending** - Smooth transitions at boundaries | ⚠️ Partial blending | **PARTIAL** - Needs refinement |
| 9 | **Memory Stability** - No leaks during extended play | ✅ Stable memory | **PASSED** |
| 10 | **Frame Rate** - Maintain 60+ FPS | ⚠️ 60+ on recommended hardware | **PARTIAL** - Lower-end systems near 60 |

**Summary**: 7/10 Passed, 3/10 Partial, 0/10 Failed

**Known Issues**:
- Collision detection has rare underground glitches (player can get stuck in blocks)
- Biome blending incomplete (transitions can be abrupt)
- Performance on systems below minimum specs near but not quite 60 FPS

---

## 📊 Technical Architecture

### Rendering Pipeline
1. **Shadow Pass**: Render scene from light's perspective to 8192×8192 depth texture
2. **Main Pass**: Render scene from camera with Phong lighting + shadow sampling
3. **GUI Pass**: Overlay 2D elements using orthographic projection

### Data Flow
```
main.cpp → Game → Camera/Shader/ChunkManager
         → ChunkManager → TerrainGenerator → Chunk → Blocks (16×256×16)
         → Input → Camera Movement → Physics → Collision
```

### Optimization Techniques
- **Face Culling**: Only visible block faces rendered (interior faces hidden)
- **Chunk Batching**: Single draw call per chunk (~65,536 blocks)
- **Frustum Culling**: Chunk-level visibility testing
- **Memory Pooling**: Chunk reuse prevents fragmentation
- **Smart Pointers**: RAII for automatic resource management

### Performance Metrics
- **Render Distance**: 20 chunks (configurable)
- **Blocks per Chunk**: 16 × 256 × 16 = 65,536
- **Shadow Resolution**: 8192×8192 with PCF
- **Target FPS**: 60+ on recommended hardware

---

## 🎓 Evaluation & Reflection

### What Was Achieved ✅
1. **Complete OpenGL rendering pipeline** with modern 4.3 Core Profile
2. **Advanced shadow mapping** with 8192×8192 resolution and PCF filtering
3. **Procedural terrain generation** with three distinct biomes and reproducible seeds
4. **Dynamic day/night cycle** with moving celestial bodies and realistic lighting
5. **Interactive GUI system** with real-time parameter adjustment
6. **AI-controlled entities** (mobs and pet companion)
7. **Robust architecture** using multiple design patterns
8. **Efficient chunk streaming** with stable 60+ FPS on recommended hardware

**Learning Outcomes**
- Better understanding of how to use AI to develop the features I give it. What to tell it and what is useless information
- Better at debugging with AI, If the AI gets stuck in a loop I know know how to get it out of the loop.

### What Could Be Improved 🔄
- I would make the PTG more complex from the start instead of making it basic and then trying to improve it later on, as this caused a lot of issues and was the main reason that it is currently not working perfectly
- I would improve the chunk manager so that it was better for performance. Currently it renders each chunk and stops culling faces if they are on a chunk border, if I fixed this issue it would improve performance for lower end systems.

### What Went Well 🌟
1. **Incremental Development**: I developed the game in stages seperating features into the different stages instead of trying to develop a lot of different features all at the same time.
2. **Performance Focus**: I thought about performance from the start instead of coming back to it at a later point.

### Final Thoughts 💭
This project was a lot of fun as it gave me a lot of freedom to do exactly what I wanted and made me think about features I usually wouldn't pay attention to, such as day/night system with live shadow casting. I was also able to use the AI to make features that I had no idea on how to develop but had ideas for, for example the texture of the blocks I had a random thought to use noise to add textures instead of each block just being one solid colour.

---

## 🚀 Quick Start

### Prerequisites
- Visual Studio 2022 with C++ Desktop Development
- Windows 10/11 (64-bit)
- GPU with OpenGL 4.3+ support

### Build Instructions
1. Open `COMP3016-CW2.sln` in Visual Studio 2022
2. Select `x64` platform and `Debug` or `Release` configuration
3. Press `F7` to build
4. Press `F5` to run

### Project Structure
```
COMP3016-CW2/
├── include/          # Header files (Game.h, Camera.h, Chunk.h, etc.)
├── src/              # Source files (main.cpp, Game.cpp)
├── resources/        # Shaders, models, textures
├── dependencies/     # GLFW, GLAD, GLM, Assimp
└── bin/              # Build output
```

---

## 📄 License

This is a coursework project for COMP3016 at the University of Plymouth.

**Author**: Morgan McGovern  
**Course**: COMP3016
**Build Status**: ✅ Compiling Successfully
