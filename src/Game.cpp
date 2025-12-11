#include "../include/Game.h"
#include "../include/Camera.h"
#include "../include/Shader.h"
#include "../include/ChunkManager.h"
#include "../include/TerrainGenerator.h"
#include "../include/Block.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <algorithm>

// Define static member
int TerrainGenerator::worldSeed = 0;

// For future integration:
// #include <irrKlang.h>
// #include <PxPhysicsAPI.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// Windows GDI+ for texture loading
#define NOMINMAX  // Prevent Windows.h from defining min/max macros
#include <windows.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

Game::Game()
    : m_initialized(false), m_firstMouse(true), m_lastX(640.0f), m_lastY(360.0f), m_sprintKeyPressed(false),
      m_isPlayerMoving(false), m_isCrouching(false), m_crouchOffset(0.0f),
      m_timeOfDay(0.25f), m_lightUpdateTimer(0.0f),  // Start at dawn (0.25 = 6am)
      m_skyColor(0.53f, 0.81f, 0.92f),  // Start with bright blue sky
      m_handBobTimer(0.0f), m_handSwingTimer(0.0f), m_isSwinging(false), m_debugUpdateTimer(0.0f),
      m_selectedSlot(0),  // Start with empty hand
      m_petPosition(0.0f, 50.0f, 0.0f), m_petVelocity(0.0f), m_petSitting(false), m_petSitPosition(0.0f), m_petScale(1.0f),
      m_showGUI(false), m_renderDistance(20.0f), m_mouseSensitivity(0.1f), m_fov(45.0f), m_timeSpeed(1.0f), m_window(nullptr)
{
    // Initialize hotbar with different blocks (slot 0 is empty hand)
    m_hotbarItems[0] = BlockType::AIR;      // Empty hand
    m_hotbarItems[1] = BlockType::GRASS;
    m_hotbarItems[2] = BlockType::DIRT;
    m_hotbarItems[3] = BlockType::STONE;
    m_hotbarItems[4] = BlockType::WOOD;
    m_hotbarItems[5] = BlockType::LEAVES;
    m_hotbarItems[6] = BlockType::BIRCH_WOOD;
    m_hotbarItems[7] = BlockType::BIRCH_GRASS;
    m_hotbarItems[8] = BlockType::DESERT_SAND;  // Extra slot
}

Game::~Game()
{
}

bool Game::Initialize()
{
    std::cout << "Initializing game systems..." << std::endl;

    // Initialize world seed for unique terrain generation
    TerrainGenerator::InitializeSeed();

    // Initialize camera at a temporary position
    m_camera = std::make_unique<Camera>(glm::vec3(0.0f, 50.0f, 0.0f));
    m_camera->MovementSpeed = 15.0f;

    // Load shader
    try
    {
        m_shader = std::make_unique<Shader>("resources/shaders/basic.vert", "resources/shaders/basic.frag");
        
        // Initialize texture uniforms
        m_shader->use();
        m_shader->setInt("shadowMap", 0);
        m_shader->setInt("diffuseTexture", 1);
        m_shader->setBool("useTexture", false);
    }
    catch (...)
    {
        std::cerr << "Failed to load shaders" << std::endl;
        return false;
    }

    // Initialize chunk manager with 8 chunk render distance (128 blocks)
    m_chunkManager = std::make_unique<ChunkManager>(128);

    // Generate initial chunks around the camera
    m_chunkManager->Update(*m_camera);

    // Find the ground height at spawn position and place player on top
    float spawnX = 0.0f;
    float spawnZ = 0.0f;
    float groundHeight = 0.0f;
    
    // Search downward from a high position to find the first solid block
    for (int y = CHUNK_HEIGHT - 1; y >= 0; y--)
    {
        if (m_chunkManager->IsSolid(spawnX, (float)y, spawnZ))
        {
            groundHeight = (float)y + 1.0f; // Place player on top of block
            break;
        }
    }
    
    // Set camera position (eye level is at groundHeight + PLAYER_EYE_HEIGHT)
    m_camera->Position = glm::vec3(spawnX, groundHeight + PLAYER_EYE_HEIGHT, spawnZ);
    m_camera->IsGrounded = true;
    m_camera->Velocity = glm::vec3(0.0f);

    std::cout << "Player spawned at height: " << groundHeight << std::endl;

    // Initialize player model
    InitializePlayerModel();

    // Initialize skybox
    InitializeSkybox();
    
    // Initialize stars
    InitializeStars();
    
    // Initialize hand
    InitializeHand();

    // Load shadow shader
    m_shadowShader = std::make_unique<Shader>("resources/shaders/shadow.vert", "resources/shaders/shadow.frag");
    std::cout << "Shadow shader loaded successfully" << std::endl;

    // Initialize shadow mapping
    InitializeShadowMap();
    
    // Initialize crosshair
    InitializeCrosshair();
    std::cout << "Crosshair initialized" << std::endl;
    
    // Initialize hotbar
    InitializeHotbar();
    std::cout << "Hotbar initialized" << std::endl;
    
    // Initialize block outline
    InitializeBlockOutline();
    std::cout << "Block outline initialized" << std::endl;
    
    // Initialize pet (spawn near player)
    m_petPosition = m_camera->Position + glm::vec3(2.0f, 0.0f, 2.0f);
    InitializePet();
    std::cout << "Pet initialized" << std::endl;
    
    // Initialize mobs
    InitializeMobs();
    
    // Initialize GUI
    InitializeGUI();
    std::cout << "GUI initialized" << std::endl;

    m_initialized = true;
    std::cout << "Game systems initialized successfully" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  WASD - Move | Mouse - Look around | Space - Jump" << std::endl;
    std::cout << "  Shift - Walk slower | Ctrl - Toggle Sprint | G - Toggle GUI" << std::endl;
    
    return true;
}

void Game::ProcessInput(GLFWwindow* window, float deltaTime)
{
    // Toggle GUI with G key
    static bool gKeyWasPressed = false;
    bool gKeyPressed = (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS);
    if (gKeyPressed && !gKeyWasPressed)
    {
        ToggleGUI(window);
    }
    gKeyWasPressed = gKeyPressed;
    
    // Find nearest Birch biome with B key
    static bool bKeyWasPressed = false;
    bool bKeyPressed = (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS);
    if (bKeyPressed && !bKeyWasPressed)
    {
        float playerX = m_camera->Position.x;
        float playerZ = m_camera->Position.z;
        float foundX, foundZ, distance;
        TerrainGenerator::FindClosestBiomeOfType(playerX, playerZ, BiomeType::BIRCH, foundX, foundZ, distance);
        std::cout << "\n=== Nearest BIRCH Biome ===" << std::endl;
        std::cout << "Coordinates: (" << (int)foundX << ", " << (int)foundZ << ")" << std::endl;
        std::cout << "Distance: " << (int)distance << " blocks" << std::endl;
        std::cout << "========================\n" << std::endl;
    }
    bKeyWasPressed = bKeyPressed;
    
    // Find nearest Desert biome with N key
    static bool nKeyWasPressed = false;
    bool nKeyPressed = (glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS);
    if (nKeyPressed && !nKeyWasPressed)
    {
        float playerX = m_camera->Position.x;
        float playerZ = m_camera->Position.z;
        float foundX, foundZ, distance;
        TerrainGenerator::FindClosestBiomeOfType(playerX, playerZ, BiomeType::DESERT, foundX, foundZ, distance);
        std::cout << "\n=== Nearest DESERT Biome ===" << std::endl;
        std::cout << "Coordinates: (" << (int)foundX << ", " << (int)foundZ << ")" << std::endl;
        std::cout << "Distance: " << (int)distance << " blocks" << std::endl;
        std::cout << "========================\n" << std::endl;
    }
    nKeyWasPressed = nKeyPressed;
    
    // Spawn Ben mob near player with M key
    static bool mKeyWasPressed = false;
    bool mKeyPressed = (glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS);
    if (mKeyPressed && !mKeyWasPressed)
    {
        // Find a valid spawn position near the player
        glm::vec3 spawnPos = m_camera->Position + m_camera->Front * 5.0f;
        
        // Find ground height
        float groundY = spawnPos.y;
        for (int y = (int)spawnPos.y + 10; y > (int)spawnPos.y - 10; y--)
        {
            if (m_chunkManager->IsSolid(spawnPos.x, (float)y, spawnPos.z))
            {
                groundY = (float)y + 1.0f;
                break;
            }
        }
        
        spawnPos.y = groundY;
        
        // Check if we have an existing Ben mesh template
        if (!m_mobs.empty())
        {
            Mob newMob;
            newMob.position = spawnPos;
            newMob.spawnLocation = spawnPos;
            newMob.velocity = glm::vec3(0.0f);
            newMob.rotationY = (rand() % 360) * 3.14159f / 180.0f;
            newMob.wanderTimer = 0.0f;
            newMob.wanderTarget = spawnPos;
            newMob.isActive = true;
            newMob.meshes = m_mobs[0].meshes; // Copy meshes from first mob
            m_mobs.push_back(newMob);
            
            std::cout << "Spawned Ben mob at (" << (int)spawnPos.x << ", " << (int)spawnPos.y << ", " << (int)spawnPos.z << ")" << std::endl;
        }
        else
        {
            std::cout << "No Ben template available - mobs not initialized yet" << std::endl;
        }
    }
    mKeyWasPressed = mKeyPressed;
    
    // Handle GUI interactions when GUI is open
    if (m_showGUI)
    {
        int windowWidth, windowHeight;
        glfwGetWindowSize(window, &windowWidth, &windowHeight);
        HandleGUIInteraction(window, windowWidth, windowHeight);
    }
    
    // Hotbar selection with number keys (1-9)
    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) m_selectedSlot = 0;
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) m_selectedSlot = 1;
    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) m_selectedSlot = 2;
    if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS) m_selectedSlot = 3;
    if (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS) m_selectedSlot = 4;
    if (glfwGetKey(window, GLFW_KEY_6) == GLFW_PRESS) m_selectedSlot = 5;
    if (glfwGetKey(window, GLFW_KEY_7) == GLFW_PRESS) m_selectedSlot = 6;
    if (glfwGetKey(window, GLFW_KEY_8) == GLFW_PRESS) m_selectedSlot = 7;
    if (glfwGetKey(window, GLFW_KEY_9) == GLFW_PRESS) m_selectedSlot = 8;
    
    // Check if crouching (shift slows movement)
    m_isCrouching = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS);

    // Toggle sprint on Control key press (not hold)
    bool sprintKeyCurrentlyPressed = (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS);
    if (sprintKeyCurrentlyPressed && !m_sprintKeyPressed)
    {
        m_camera->ToggleSprint();
    }
    m_sprintKeyPressed = sprintKeyCurrentlyPressed;

    // Jump on space
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
    {
        m_camera->Jump();
    }

    // Check if player is moving for hand animation
    m_isPlayerMoving = (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS ||
                        glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS ||
                        glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS ||
                        glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS);
    
    // Check if player is moving forward only (for sprint)
    bool movingForward = (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS);
    bool movingBackward = (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS);
    
    // Disable sprint if player stops moving forward or moves backwards
    if (m_camera->IsSprinting && (!movingForward || movingBackward))
    {
        m_camera->IsSprinting = false;
    }
    
    // Horizontal movement (WASD) - check collision before applying
    glm::vec3 totalMovement(0.0f);
    
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        totalMovement += m_camera->GetMovementVector(FORWARD, deltaTime, m_isCrouching);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        totalMovement += m_camera->GetMovementVector(BACKWARD, deltaTime, m_isCrouching);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        totalMovement += m_camera->GetMovementVector(LEFT, deltaTime, m_isCrouching);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        totalMovement += m_camera->GetMovementVector(RIGHT, deltaTime, m_isCrouching);
    
    // Apply movement if it doesn't cause collision
    if (totalMovement != glm::vec3(0.0f))
    {
        glm::vec3 newPosition = m_camera->Position + totalMovement;
        if (!WouldCollide(newPosition))
        {
            m_camera->ApplyMovement(totalMovement);
        }
        else
        {
            // Try sliding along walls - separate X and Z movement
            glm::vec3 xMovement(totalMovement.x, 0.0f, 0.0f);
            glm::vec3 zMovement(0.0f, 0.0f, totalMovement.z);
            
            if (xMovement != glm::vec3(0.0f) && !WouldCollide(m_camera->Position + xMovement))
            {
                m_camera->ApplyMovement(xMovement);
            }
            
            if (zMovement != glm::vec3(0.0f) && !WouldCollide(m_camera->Position + zMovement))
            {
                m_camera->ApplyMovement(zMovement);
            }
        }
    }
}

void Game::ProcessMouseMovement(float xoffset, float yoffset)
{
    if (m_firstMouse)
    {
        m_lastX = 0;
        m_lastY = 0;
        m_firstMouse = false;
    }

    // Apply mouse sensitivity from GUI
    m_camera->ProcessMouseMovement(xoffset * m_mouseSensitivity / 0.1f, yoffset * m_mouseSensitivity / 0.1f);
}

void Game::Update(float deltaTime)
{
    // Update day/night cycle
    UpdateDayNightCycle(deltaTime);

    // Apply physics
    m_camera->ApplyPhysics(deltaTime);
    
    // Update hand animation based on movement
    UpdateHandAnimation(deltaTime, m_isPlayerMoving);

    // Collision detection - use camera position directly
    float feetY = m_camera->Position.y - PLAYER_EYE_HEIGHT;
    
    // Check if player is on ground (check block directly below feet)
    m_camera->IsGrounded = m_chunkManager->IsSolid(m_camera->Position.x, feetY - 0.1f, m_camera->Position.z);
    
    // If grounded and falling, stop downward velocity and snap to top of block
    if (m_camera->IsGrounded && m_camera->Velocity.y <= 0.0f)
    {
        // Find the exact block Y position below the player
        int blockY = (int)floor(feetY - 0.1f);
        
        // Snap camera to correct height above that block
        float targetY = (float)blockY + 1.0f + PLAYER_EYE_HEIGHT;
        
        // Only snap if we're close (prevent teleporting from far away)
        if (abs(m_camera->Position.y - targetY) < 1.0f)
        {
            m_camera->Position.y = targetY;
            m_camera->Velocity.y = 0.0f;
        }
    }
    
    // Horizontal collision is now prevented in ProcessInput, but check for penetration from gravity/falling
    if (WouldCollide(m_camera->Position))
    {
        // If somehow inside a block (from falling), push out
        float halfWidth = PLAYER_WIDTH / 2.0f;
        glm::vec3 feetCheck = m_camera->GetFeetPosition();
        
        // Check corners and push out minimally
        for (float heightOffset = 0.0f; heightOffset < PLAYER_HEIGHT; heightOffset += 0.5f)
        {
            glm::vec3 centerCheck = feetCheck + glm::vec3(0.0f, heightOffset, 0.0f);
            
            glm::vec3 corners[] = {
                centerCheck + glm::vec3(halfWidth, 0.0f, halfWidth),
                centerCheck + glm::vec3(-halfWidth, 0.0f, halfWidth),
                centerCheck + glm::vec3(halfWidth, 0.0f, -halfWidth),
                centerCheck + glm::vec3(-halfWidth, 0.0f, -halfWidth)
            };
            
            for (const auto& corner : corners)
            {
                if (m_chunkManager->IsSolid(corner.x, corner.y, corner.z))
                {
                    int blockX = (int)floor(corner.x);
                    int blockZ = (int)floor(corner.z);
                    
                    float blockCenterX = (float)blockX + 0.5f;
                    float blockCenterZ = (float)blockZ + 0.5f;
                    
                    glm::vec3 diff = m_camera->Position - glm::vec3(blockCenterX, m_camera->Position.y, blockCenterZ);
                    
                    if (abs(diff.x) > abs(diff.z))
                    {
                        m_camera->Position.x += (diff.x > 0) ? 0.05f : -0.05f;
                    }
                    else
                    {
                        m_camera->Position.z += (diff.z > 0) ? 0.05f : -0.05f;
                    }
                }
            }
        }
    }
    
    // Check head collision (prevent jumping through blocks)
    glm::vec3 headPos = m_camera->Position + glm::vec3(0.0f, 0.2f, 0.0f);
    if (m_chunkManager->IsSolid(headPos.x, headPos.y, headPos.z) && m_camera->Velocity.y > 0.0f)
    {
        m_camera->Velocity.y = 0.0f;
    }

    // Update chunk loading/unloading based on camera position
    m_chunkManager->Update(*m_camera);
    
    // Update physics for gravity-affected blocks
    m_chunkManager->UpdatePhysics(deltaTime);
    
    // Update pet
    UpdatePet(deltaTime);
    
    // Update mobs
    UpdateMobs(deltaTime);
}

void Game::Render(GLFWwindow* window)
{
    if (!m_initialized)
        return;

    // 1. Render shadow map from light's perspective
    RenderShadowMap();

    // 2. Render scene normally with shadows
    // Get actual window framebuffer dimensions
    int windowWidth, windowHeight;
    glfwGetFramebufferSize(window, &windowWidth, &windowHeight);
    glViewport(0, 0, windowWidth, windowHeight);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    // Set sky color based on time of day
    glClearColor(m_skyColor.r, m_skyColor.g, m_skyColor.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Use main shader
    m_shader->use();

    // Set up matrices
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 view = m_camera->GetViewMatrix();
    float aspectRatio = (float)windowWidth / (float)windowHeight;
    glm::mat4 projection = glm::perspective(glm::radians(m_fov), aspectRatio, 0.1f, 1000.0f);

    m_shader->setMat4("model", glm::value_ptr(model));
    m_shader->setMat4("view", glm::value_ptr(view));
    m_shader->setMat4("projection", glm::value_ptr(projection));

    // Set lighting uniforms (dynamic based on time of day)
    glm::vec3 sunPos = GetSunPosition();
    m_shader->setVec3("lightPos", sunPos.x, sunPos.y, sunPos.z);
    m_shader->setVec3("viewPos", m_camera->Position.x, m_camera->Position.y, m_camera->Position.z);
    m_shader->setVec3("lightColor", m_lightColor.x, m_lightColor.y, m_lightColor.z);

    // Calculate light space matrix with same adjustments as shadow pass
    float sunAngle = m_timeOfDay * 2.0f * 3.14159f;
    float sunHeight = sin(sunAngle);
    
    glm::vec3 adjustedSunPos = sunPos;
    if (abs(sunHeight) < 0.5f) {
        // When sun is low, keep it at a minimum angle to prevent long shadows
        adjustedSunPos.y = m_camera->Position.y + (sunHeight >= 0 ? 250.0f : -250.0f);
        // Maintain horizontal distance for realistic direction
        float horizontalDist = 500.0f * cos(sunAngle);
        adjustedSunPos.z = m_camera->Position.z + horizontalDist;
    }
    
    glm::mat4 lightProjection = glm::ortho(-300.0f, 300.0f, -300.0f, 300.0f, 1.0f, 1000.0f);
    glm::mat4 lightView = glm::lookAt(adjustedSunPos, m_camera->Position, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 lightSpaceMatrix = lightProjection * lightView;
    m_shader->setMat4("lightSpaceMatrix", glm::value_ptr(lightSpaceMatrix));

    // Bind shadow map
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_shadowMap);
    m_shader->setInt("shadowMap", 1);

    // Render sky first
    RenderSky();
    
    // Render stars at night
    RenderStars();

    // Reset model matrix and lighting for terrain rendering
    model = glm::mat4(1.0f);
    m_shader->setMat4("model", glm::value_ptr(model));
    m_shader->setMat4("view", glm::value_ptr(view));
    m_shader->setMat4("projection", glm::value_ptr(projection));
    m_shader->setVec3("lightColor", m_lightColor.x, m_lightColor.y, m_lightColor.z);
    m_shader->setBool("unlit", false);  // Enable lighting for terrain

    // Render all chunks
    m_chunkManager->Render();

    // Render player model
    RenderPlayer();
    
    // Render pet
    RenderPet();
    
    // Render mobs
    RenderMobs();
    
    // Render hand (last so it's always on top)
    RenderHand();
    
    // Render block outline
    RenderBlockOutline();
    
    // Render hotbar
    RenderHotbar(windowWidth, windowHeight);
    
    // Render GUI if enabled
    if (m_showGUI)
    {
        RenderGUI(windowWidth, windowHeight);
    }
    
    // Render crosshair (absolute last)
    RenderCrosshair();
}

void Game::InitializePlayerModel()
{
    // Player cube dimensions: 2/3 block wide (0.667), 2 blocks tall
    float halfWidth = PLAYER_WIDTH / 2.0f;  // 0.333
    float height = PLAYER_HEIGHT;           // 2.0

    // Define vertices for a cube (position + normal + color)
    float vertices[] = {
        // Positions (x, y, z)        Normals (nx, ny, nz)      Color (r, g, b)
        // Front face
        -halfWidth, 0.0f, halfWidth,   0.0f, 0.0f, 1.0f,        0.3f, 0.5f, 0.8f,
         halfWidth, 0.0f, halfWidth,   0.0f, 0.0f, 1.0f,        0.3f, 0.5f, 0.8f,
         halfWidth, height, halfWidth,  0.0f, 0.0f, 1.0f,        0.3f, 0.5f, 0.8f,
         halfWidth, height, halfWidth,  0.0f, 0.0f, 1.0f,        0.3f, 0.5f, 0.8f,
        -halfWidth, height, halfWidth,  0.0f, 0.0f, 1.0f,        0.3f, 0.5f, 0.8f,
        -halfWidth, 0.0f, halfWidth,   0.0f, 0.0f, 1.0f,        0.3f, 0.5f, 0.8f,

        // Back face
        -halfWidth, 0.0f, -halfWidth,  0.0f, 0.0f, -1.0f,       0.2f, 0.4f, 0.7f,
         halfWidth, 0.0f, -halfWidth,  0.0f, 0.0f, -1.0f,       0.2f, 0.4f, 0.7f,
         halfWidth, height, -halfWidth, 0.0f, 0.0f, -1.0f,       0.2f, 0.4f, 0.7f,
         halfWidth, height, -halfWidth, 0.0f, 0.0f, -1.0f,       0.2f, 0.4f, 0.7f,
        -halfWidth, height, -halfWidth, 0.0f, 0.0f, -1.0f,       0.2f, 0.4f, 0.7f,
        -halfWidth, 0.0f, -halfWidth,  0.0f, 0.0f, -1.0f,       0.2f, 0.4f, 0.7f,

        // Left face
        -halfWidth, 0.0f, -halfWidth,  -1.0f, 0.0f, 0.0f,       0.25f, 0.45f, 0.75f,
        -halfWidth, 0.0f, halfWidth,   -1.0f, 0.0f, 0.0f,       0.25f, 0.45f, 0.75f,
        -halfWidth, height, halfWidth,  -1.0f, 0.0f, 0.0f,       0.25f, 0.45f, 0.75f,
        -halfWidth, height, halfWidth,  -1.0f, 0.0f, 0.0f,       0.25f, 0.45f, 0.75f,
        -halfWidth, height, -halfWidth, -1.0f, 0.0f, 0.0f,       0.25f, 0.45f, 0.75f,
        -halfWidth, 0.0f, -halfWidth,  -1.0f, 0.0f, 0.0f,       0.25f, 0.45f, 0.75f,

        // Right face
         halfWidth, 0.0f, -halfWidth,  1.0f, 0.0f, 0.0f,        0.35f, 0.55f, 0.85f,
         halfWidth, 0.0f, halfWidth,   1.0f, 0.0f, 0.0f,        0.35f, 0.55f, 0.85f,
         halfWidth, height, halfWidth,  1.0f, 0.0f, 0.0f,        0.35f, 0.55f, 0.85f,
         halfWidth, height, halfWidth,  1.0f, 0.0f, 0.0f,        0.35f, 0.55f, 0.85f,
         halfWidth, height, -halfWidth, 1.0f, 0.0f, 0.0f,        0.35f, 0.55f, 0.85f,
         halfWidth, 0.0f, -halfWidth,  1.0f, 0.0f, 0.0f,        0.35f, 0.55f, 0.85f,

        // Top face
        -halfWidth, height, -halfWidth, 0.0f, 1.0f, 0.0f,        0.4f, 0.6f, 0.9f,
         halfWidth, height, -halfWidth, 0.0f, 1.0f, 0.0f,        0.4f, 0.6f, 0.9f,
         halfWidth, height, halfWidth,  0.0f, 1.0f, 0.0f,        0.4f, 0.6f, 0.9f,
         halfWidth, height, halfWidth,  0.0f, 1.0f, 0.0f,        0.4f, 0.6f, 0.9f,
        -halfWidth, height, halfWidth,  0.0f, 1.0f, 0.0f,        0.4f, 0.6f, 0.9f,
        -halfWidth, height, -halfWidth, 0.0f, 1.0f, 0.0f,        0.4f, 0.6f, 0.9f,

        // Bottom face
        -halfWidth, 0.0f, -halfWidth,  0.0f, -1.0f, 0.0f,       0.15f, 0.35f, 0.65f,
         halfWidth, 0.0f, -halfWidth,  0.0f, -1.0f, 0.0f,       0.15f, 0.35f, 0.65f,
         halfWidth, 0.0f, halfWidth,   0.0f, -1.0f, 0.0f,       0.15f, 0.35f, 0.65f,
         halfWidth, 0.0f, halfWidth,   0.0f, -1.0f, 0.0f,       0.15f, 0.35f, 0.65f,
        -halfWidth, 0.0f, halfWidth,   0.0f, -1.0f, 0.0f,       0.15f, 0.35f, 0.65f,
        -halfWidth, 0.0f, -halfWidth,  0.0f, -1.0f, 0.0f,       0.15f, 0.35f, 0.65f,
    };

    glGenVertexArrays(1, &m_playerVAO);
    glGenBuffers(1, &m_playerVBO);

    glBindVertexArray(m_playerVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_playerVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Color attribute
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

void Game::RenderPlayer()
{
    // Get player feet position (camera position - eye height)
    glm::vec3 playerPos = m_camera->GetFeetPosition();

    // Create model matrix for player
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, playerPos);
    
    // Rotate player to match camera yaw (horizontal rotation only)
    model = glm::rotate(model, glm::radians(m_camera->Yaw + 90.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    // Set model matrix
    m_shader->setMat4("model", glm::value_ptr(model));

    // Don't render the player model - player doesn't need to see it
    // (commented out to hide player model from first-person view)
    /*
    // Render the player cube
    glBindVertexArray(m_playerVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    */
}

bool Game::WouldCollide(const glm::vec3& newPosition)
{
    // Check if the player would collide at the new position
    float halfWidth = PLAYER_WIDTH / 2.0f;
    
    // Calculate feet position at new location
    glm::vec3 newFeetPos = newPosition - glm::vec3(0.0f, PLAYER_EYE_HEIGHT, 0.0f);
    
    // Check multiple heights of the player's bounding box
    for (float heightOffset = 0.0f; heightOffset < PLAYER_HEIGHT; heightOffset += 0.5f)
    {
        glm::vec3 checkCenter = newFeetPos + glm::vec3(0.0f, heightOffset, 0.0f);
        
        // Check all four corners of the player's bounding box
        glm::vec3 corners[] = {
            checkCenter + glm::vec3(halfWidth, 0.0f, halfWidth),
            checkCenter + glm::vec3(-halfWidth, 0.0f, halfWidth),
            checkCenter + glm::vec3(halfWidth, 0.0f, -halfWidth),
            checkCenter + glm::vec3(-halfWidth, 0.0f, -halfWidth),
            checkCenter  // Also check center
        };
        
        for (const auto& corner : corners)
        {
            if (m_chunkManager->IsSolid(corner.x, corner.y, corner.z))
            {
                return true;  // Would collide
            }
        }
    }
    
    return false;  // No collision
}

bool Game::RaycastBlock(glm::vec3& hitPos, float maxDistance)
{
    // Get ray origin and direction from camera
    glm::vec3 rayOrigin = m_camera->Position;
    glm::vec3 rayDir = m_camera->Front;
    
    // DDA algorithm for voxel raycast
    float stepSize = 0.05f;  // Small steps for accuracy
    glm::vec3 currentPos = rayOrigin;
    
    for (float distance = 0.0f; distance < maxDistance; distance += stepSize)
    {
        currentPos = rayOrigin + rayDir * distance;
        
        // Check if current position has a solid block
        if (m_chunkManager->IsSolid(currentPos.x, currentPos.y, currentPos.z))
        {
            // Found a block - store position and return true
            hitPos = glm::vec3(floor(currentPos.x), floor(currentPos.y), floor(currentPos.z));
            return true;
        }
    }
    
    return false;  // No block hit within range
}

bool Game::RaycastBlockWithNormal(glm::vec3& hitPos, glm::vec3& normal, float maxDistance)
{
    // Get ray origin and direction from camera
    glm::vec3 rayOrigin = m_camera->Position;
    glm::vec3 rayDir = m_camera->Front;
    
    // DDA algorithm for voxel raycast
    float stepSize = 0.05f;  // Small steps for accuracy
    glm::vec3 currentPos = rayOrigin;
    glm::vec3 previousPos = rayOrigin;
    
    for (float distance = 0.0f; distance < maxDistance; distance += stepSize)
    {
        currentPos = rayOrigin + rayDir * distance;
        
        // Check if current position has a solid block
        if (m_chunkManager->IsSolid(currentPos.x, currentPos.y, currentPos.z))
        {
            // Found a block - store position
            hitPos = glm::vec3(floor(currentPos.x), floor(currentPos.y), floor(currentPos.z));
            
            // Calculate normal by determining which face we entered from
            // Compare where we came from (previous) to where we hit (current)
            glm::vec3 prevFloor = glm::vec3(floor(previousPos.x), floor(previousPos.y), floor(previousPos.z));
            glm::vec3 diff = prevFloor - hitPos;
            
            // The face we hit is the one pointing towards where we came from
            if (abs(diff.x) > abs(diff.y) && abs(diff.x) > abs(diff.z))
                normal = glm::vec3(diff.x > 0 ? 1.0f : -1.0f, 0.0f, 0.0f);
            else if (abs(diff.y) > abs(diff.z))
                normal = glm::vec3(0.0f, diff.y > 0 ? 1.0f : -1.0f, 0.0f);
            else
                normal = glm::vec3(0.0f, 0.0f, diff.z > 0 ? 1.0f : -1.0f);
            
            return true;
        }
        
        previousPos = currentPos;
    }
    
    return false;  // No block hit within range
}

void Game::Shutdown()
{
    if (!m_initialized)
        return;

    std::cout << "Shutting down game systems..." << std::endl;

    // Cleanup player model
    glDeleteVertexArrays(1, &m_playerVAO);
    glDeleteBuffers(1, &m_playerVBO);

    // Cleanup skybox
    glDeleteVertexArrays(1, &m_skyVAO);
    glDeleteBuffers(1, &m_skyVBO);
    
    // Cleanup stars
    glDeleteVertexArrays(1, &m_starsVAO);
    glDeleteBuffers(1, &m_starsVBO);
    
    // Cleanup hand
    glDeleteVertexArrays(1, &m_handVAO);
    glDeleteBuffers(1, &m_handVBO);
    
    // Cleanup crosshair
    glDeleteVertexArrays(1, &m_crosshairVAO);
    glDeleteBuffers(1, &m_crosshairVBO);
    
    // Cleanup hotbar
    glDeleteVertexArrays(1, &m_hotbarVAO);
    glDeleteBuffers(1, &m_hotbarVBO);
    
    // Cleanup block outline
    glDeleteVertexArrays(1, &m_outlineVAO);
    glDeleteBuffers(1, &m_outlineVBO);
    
    // Cleanup pet
    glDeleteVertexArrays(1, &m_petVAO);
    glDeleteBuffers(1, &m_petVBO);
    
    // Cleanup GUI
    glDeleteVertexArrays(1, &m_guiVAO);
    glDeleteBuffers(1, &m_guiVBO);

    // Cleanup shadow map
    glDeleteFramebuffers(1, &m_shadowMapFBO);
    glDeleteTextures(1, &m_shadowMap);

    // Cleanup will happen automatically with unique_ptr destructors
    m_chunkManager.reset();
    m_shader.reset();
    m_camera.reset();

    m_initialized = false;
}

void Game::InitializeShadowMap()
{
    // Create framebuffer for shadow map
    glGenFramebuffers(1, &m_shadowMapFBO);

    // Create depth texture
    glGenTextures(1, &m_shadowMap);
    glBindTexture(GL_TEXTURE_2D, m_shadowMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    // Attach depth texture to framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, m_shadowMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_shadowMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    // Check framebuffer status
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cerr << "Shadow framebuffer is not complete!" << std::endl;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Game::RenderShadowMap()
{
    // Calculate light space matrix
    glm::vec3 sunPos = GetSunPosition();
    
    // Clamp sun angle to prevent extremely long shadows
    float sunAngle = m_timeOfDay * 2.0f * 3.14159f;
    float sunHeight = sin(sunAngle);
    
    // Adjust sun position to maintain reasonable shadow angles
    glm::vec3 adjustedSunPos = sunPos;
    if (abs(sunHeight) < 0.5f) {
        // When sun is low, keep it at a minimum angle to prevent long shadows
        adjustedSunPos.y = m_camera->Position.y + (sunHeight >= 0 ? 250.0f : -250.0f);
        // Maintain horizontal distance for realistic direction
        float horizontalDist = 500.0f * cos(sunAngle);
        adjustedSunPos.z = m_camera->Position.z + horizontalDist;
    }
    
    glm::mat4 lightProjection = glm::ortho(-300.0f, 300.0f, -300.0f, 300.0f, 1.0f, 1000.0f);
    glm::mat4 lightView = glm::lookAt(adjustedSunPos, m_camera->Position, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 lightSpaceMatrix = lightProjection * lightView;

    // Render scene from light's perspective to shadow map
    m_shadowShader->use();
    glm::mat4 model = glm::mat4(1.0f);
    m_shadowShader->setMat4("model", glm::value_ptr(model));
    m_shadowShader->setMat4("lightSpaceMatrix", glm::value_ptr(lightSpaceMatrix));

    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
    glBindFramebuffer(GL_FRAMEBUFFER, m_shadowMapFBO);
    glClear(GL_DEPTH_BUFFER_BIT);
    glCullFace(GL_FRONT); // Peter panning prevention

    // Render chunks to shadow map
    m_chunkManager->Render();

    glCullFace(GL_BACK);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Game::UpdateDayNightCycle(float deltaTime)
{
    // Update timer
    m_lightUpdateTimer += deltaTime * m_timeSpeed;

    // Update lighting every 2 seconds
    if (m_lightUpdateTimer >= 2.0f)
    {
        m_lightUpdateTimer = 0.0f;

        // Advance time of day (full cycle = 360 seconds = 6 minutes)
        m_timeOfDay += 2.0f / 360.0f;  // 2 seconds out of 360 second cycle
        if (m_timeOfDay > 1.0f)
            m_timeOfDay -= 1.0f;

        // Calculate sun angle (0 = midnight, 0.25 = sunrise, 0.5 = noon, 0.75 = sunset, 1.0 = midnight)
        float sunAngle = m_timeOfDay * 2.0f * 3.14159f;  // Convert to radians

        // Calculate light intensity based on sun height
        float sunHeight = sin(sunAngle);
        float lightIntensity;

        if (sunHeight > -0.1f)
        {
            // Day time - keep full brightness until sun approaches horizon
            if (sunHeight > 0.2f)
            {
                // Full daylight when sun is well above horizon
                lightIntensity = 1.0f;
            }
            else
            {
                // Quick dimming only as sun approaches/crosses horizon
                float transitionProgress = (sunHeight + 0.1f) / 0.3f;  // 0 at -0.1, 1.0 at 0.2
                lightIntensity = 0.3f + (transitionProgress * 0.7f);  // 0.3 to 1.0
            }
            m_lightColor = glm::vec3(1.0f, 0.95f, 0.8f) * lightIntensity;  // Warm daylight
            
            // Bright blue sky during day - fast transition near horizon
            float dayProgress = std::max(0.0f, (sunHeight + 0.1f) / 0.3f);  // 0 at -0.1, 1.0 at 0.2
            dayProgress = std::min(1.0f, dayProgress);
            glm::vec3 duskColor = glm::vec3(0.4f, 0.45f, 0.6f);  // Twilight blue
            glm::vec3 dayColor = glm::vec3(0.53f, 0.81f, 0.92f);  // Bright sky blue
            m_skyColor = glm::mix(duskColor, dayColor, dayProgress);
        }
        else
        {
            // Night time - dim moonlight (gets dark quickly)
            float nightProgress = std::min(1.0f, (abs(sunHeight) - 0.1f) / 0.15f);  // 0 at -0.1, 1.0 at -0.25
            
            lightIntensity = 0.15f + ((1.0f - nightProgress) * 0.15f);  // 0.3 to 0.15
            m_lightColor = glm::vec3(0.6f, 0.7f, 1.0f) * lightIntensity;  // Cool moonlight
            
            // Quick transition to very dark at night
            glm::vec3 duskColor = glm::vec3(0.4f, 0.45f, 0.6f);  // Twilight blue
            glm::vec3 nightColor = glm::vec3(0.02f, 0.02f, 0.08f);  // Almost black
            m_skyColor = glm::mix(duskColor, nightColor, nightProgress);
        }

        // Time of day updates silently
    }
}

glm::vec3 Game::GetSunPosition()
{
    // Sun moves in a circular arc across the sky
    float sunAngle = m_timeOfDay * 2.0f * 3.14159f;
    float distance = 500.0f;

    // Position sun in the sky (circular path)
    glm::vec3 sunPos;
    sunPos.x = m_camera->Position.x;
    sunPos.y = m_camera->Position.y + sin(sunAngle) * distance;
    sunPos.z = m_camera->Position.z + cos(sunAngle) * distance;

    return sunPos;
}

glm::vec3 Game::GetMoonPosition()
{
    // Moon is opposite the sun
    float moonAngle = (m_timeOfDay + 0.5f) * 2.0f * 3.14159f;
    float distance = 500.0f;

    glm::vec3 moonPos;
    moonPos.x = m_camera->Position.x;
    moonPos.y = m_camera->Position.y + sin(moonAngle) * distance;
    moonPos.z = m_camera->Position.z + cos(moonAngle) * distance;

    return moonPos;
}

void Game::InitializeSkybox()
{
    // Create a large quad for sun/moon rendering with color attribute
    float size = 20.0f;
    float vertices[] = {
        // Position (x, y, z)    // Normal (nx, ny, nz)  // Color (r, g, b)
        -size, -size, 0.0f,      0.0f, 0.0f, 1.0f,       1.0f, 1.0f, 1.0f,
         size, -size, 0.0f,      0.0f, 0.0f, 1.0f,       1.0f, 1.0f, 1.0f,
         size,  size, 0.0f,      0.0f, 0.0f, 1.0f,       1.0f, 1.0f, 1.0f,
         size,  size, 0.0f,      0.0f, 0.0f, 1.0f,       1.0f, 1.0f, 1.0f,
        -size,  size, 0.0f,      0.0f, 0.0f, 1.0f,       1.0f, 1.0f, 1.0f,
        -size, -size, 0.0f,      0.0f, 0.0f, 1.0f,       1.0f, 1.0f, 1.0f
    };

    glGenVertexArrays(1, &m_skyVAO);
    glGenBuffers(1, &m_skyVBO);

    glBindVertexArray(m_skyVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_skyVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Color attribute
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

void Game::RenderSky()
{
    // Disable depth test and enable blending for sky objects
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Render sun if it's above horizon
    float sunAngle = m_timeOfDay * 2.0f * 3.14159f;
    float sunHeight = sin(sunAngle);

    if (sunHeight > -0.1f)  // Render sun when it's visible or slightly below horizon
    {
        glm::vec3 sunPos = GetSunPosition();

        // Create billboard matrix (quad always faces camera)
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, sunPos);

        // Make quad face the camera
        glm::vec3 cameraToSun = glm::normalize(sunPos - m_camera->Position);
        glm::vec3 right = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), cameraToSun));
        glm::vec3 up = glm::cross(cameraToSun, right);

        model[0] = glm::vec4(right, 0.0f);
        model[1] = glm::vec4(up, 0.0f);
        model[2] = glm::vec4(cameraToSun, 0.0f);

        m_shader->setMat4("model", glm::value_ptr(model));

        // Update vertex colors to yellow for the sun
        float size = 20.0f;
        float sunVertices[] = {
            // Position (x, y, z)    // Normal (nx, ny, nz)  // Color (r, g, b) - BRIGHT YELLOW
            -size, -size, 0.0f,      0.0f, 0.0f, 1.0f,       3.0f, 2.7f, 0.3f,
             size, -size, 0.0f,      0.0f, 0.0f, 1.0f,       3.0f, 2.7f, 0.3f,
             size,  size, 0.0f,      0.0f, 0.0f, 1.0f,       3.0f, 2.7f, 0.3f,
             size,  size, 0.0f,      0.0f, 0.0f, 1.0f,       3.0f, 2.7f, 0.3f,
            -size,  size, 0.0f,      0.0f, 0.0f, 1.0f,       3.0f, 2.7f, 0.3f,
            -size, -size, 0.0f,      0.0f, 0.0f, 1.0f,       3.0f, 2.7f, 0.3f
        };
        
        glBindBuffer(GL_ARRAY_BUFFER, m_skyVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(sunVertices), sunVertices);

        // Override lighting for sun - make it very bright with yellow tint
        glm::vec3 brightPos = sunPos + glm::vec3(0.0f, 1000.0f, 0.0f);
        m_shader->setVec3("lightPos", brightPos.x, brightPos.y, brightPos.z);
        m_shader->setVec3("lightColor", 5.0f, 4.5f, 1.0f);  // Yellow-tinted bright light

        glBindVertexArray(m_skyVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }

    // Render moon if it's above horizon
    float moonAngle = (m_timeOfDay + 0.5f) * 2.0f * 3.14159f;
    float moonHeight = sin(moonAngle);

    if (moonHeight > -0.1f)
    {
        glm::vec3 moonPos = GetMoonPosition();

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, moonPos);

        // Make quad face the camera
        glm::vec3 cameraToMoon = glm::normalize(moonPos - m_camera->Position);
        glm::vec3 right = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), cameraToMoon));
        glm::vec3 up = glm::cross(cameraToMoon, right);

        model[0] = glm::vec4(right, 0.0f);
        model[1] = glm::vec4(up, 0.0f);
        model[2] = glm::vec4(cameraToMoon, 0.0f);

        m_shader->setMat4("model", glm::value_ptr(model));

        // Update vertex colors to white for the moon
        float size = 20.0f;
        float moonVertices[] = {
            // Position (x, y, z)    // Normal (nx, ny, nz)  // Color (r, g, b) - WHITE
            -size, -size, 0.0f,      0.0f, 0.0f, 1.0f,       1.0f, 1.0f, 1.0f,
             size, -size, 0.0f,      0.0f, 0.0f, 1.0f,       1.0f, 1.0f, 1.0f,
             size,  size, 0.0f,      0.0f, 0.0f, 1.0f,       1.0f, 1.0f, 1.0f,
             size,  size, 0.0f,      0.0f, 0.0f, 1.0f,       1.0f, 1.0f, 1.0f,
            -size,  size, 0.0f,      0.0f, 0.0f, 1.0f,       1.0f, 1.0f, 1.0f,
            -size, -size, 0.0f,      0.0f, 0.0f, 1.0f,       1.0f, 1.0f, 1.0f
        };
        
        glBindBuffer(GL_ARRAY_BUFFER, m_skyVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(moonVertices), moonVertices);

        // Override lighting for moon - make it bright white
        glm::vec3 brightPos = moonPos + glm::vec3(0.0f, 1000.0f, 0.0f);
        m_shader->setVec3("lightPos", brightPos.x, brightPos.y, brightPos.z);
        m_shader->setVec3("lightColor", 20.0f, 20.0f, 20.0f);  // Very bright white

        glBindVertexArray(m_skyVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }
    
    // Disable blending for normal rendering
    glDisable(GL_BLEND);

    // Re-enable depth test
    glEnable(GL_DEPTH_TEST);
}

void Game::InitializeStars()
{
    m_starCount = 500;  // Number of stars
    std::vector<float> starVertices;
    
    // Generate random stars distributed across the sky sphere
    srand(12345);  // Fixed seed for consistent star positions
    for (int i = 0; i < m_starCount; i++)
    {
        // Generate random position on a sphere
        float theta = ((float)rand() / RAND_MAX) * 2.0f * 3.14159f;  // 0 to 2PI
        float phi = ((float)rand() / RAND_MAX) * 3.14159f;  // 0 to PI
        float distance = 800.0f;  // Far away from camera
        
        // Convert spherical to cartesian coordinates
        float x = distance * sin(phi) * cos(theta);
        float y = distance * cos(phi);
        float z = distance * sin(phi) * sin(theta);
        
        // Random brightness (0.5 to 1.0)
        float brightness = 0.5f + ((float)rand() / RAND_MAX) * 0.5f;
        
        // Position, Normal (not used), Color (white with varying brightness)
        starVertices.push_back(x);
        starVertices.push_back(y);
        starVertices.push_back(z);
        starVertices.push_back(0.0f);
        starVertices.push_back(0.0f);
        starVertices.push_back(1.0f);
        starVertices.push_back(brightness);
        starVertices.push_back(brightness);
        starVertices.push_back(brightness);
    }
    
    glGenVertexArrays(1, &m_starsVAO);
    glGenBuffers(1, &m_starsVBO);
    
    glBindVertexArray(m_starsVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_starsVBO);
    glBufferData(GL_ARRAY_BUFFER, starVertices.size() * sizeof(float), starVertices.data(), GL_STATIC_DRAW);
    
    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    // Color attribute
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    glBindVertexArray(0);
}

void Game::RenderStars()
{
    // Only render stars at night
    float sunAngle = m_timeOfDay * 2.0f * 3.14159f;
    float sunHeight = sin(sunAngle);
    
    // Calculate star visibility (fade in/out based on sun position)
    float starVisibility = 0.0f;
    if (sunHeight < -0.1f)
    {
        // Fade in stars as sun goes below horizon
        starVisibility = std::min(1.0f, (-sunHeight - 0.1f) / 0.2f);  // Full brightness when sun is well below horizon
    }
    
    if (starVisibility <= 0.0f)
        return;  // Don't render during day
    
    // Disable depth test so stars render behind everything
    glDepthMask(GL_FALSE);
    
    // Set larger point size for stars
    glPointSize(2.0f);
    
    // Use current shader and set bright lighting for stars
    glm::vec3 starLightPos = m_camera->Position + glm::vec3(0.0f, 1000.0f, 0.0f);
    m_shader->setVec3("lightPos", starLightPos.x, starLightPos.y, starLightPos.z);
    m_shader->setVec3("lightColor", 5.0f * starVisibility, 5.0f * starVisibility, 5.0f * starVisibility);
    
    // Translate stars to follow camera
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, m_camera->Position);
    m_shader->setMat4("model", glm::value_ptr(model));
    
    // Render stars as points
    glBindVertexArray(m_starsVAO);
    glDrawArrays(GL_POINTS, 0, m_starCount);
    glBindVertexArray(0);
    
    // Re-enable depth writing
    glDepthMask(GL_TRUE);
}

void Game::InitializeHand()
{
    // Create a simple hand model (a rectangular block)
    float width = 0.18f;   // Width
    float height = 0.18f;  // Height (same as width)
    float depth = 0.5f;    // Depth (length extending forward)
    
    glm::vec3 skinColor(0.8f, 0.6f, 0.5f);  // Skin tone
    
    std::vector<float> vertices;
    
    // Front face
    vertices.insert(vertices.end(), {0.0f, 0.0f, depth, 0.0f, 0.0f, 1.0f, skinColor.r, skinColor.g, skinColor.b});
    vertices.insert(vertices.end(), {width, 0.0f, depth, 0.0f, 0.0f, 1.0f, skinColor.r, skinColor.g, skinColor.b});
    vertices.insert(vertices.end(), {width, height, depth, 0.0f, 0.0f, 1.0f, skinColor.r, skinColor.g, skinColor.b});
    vertices.insert(vertices.end(), {width, height, depth, 0.0f, 0.0f, 1.0f, skinColor.r, skinColor.g, skinColor.b});
    vertices.insert(vertices.end(), {0.0f, height, depth, 0.0f, 0.0f, 1.0f, skinColor.r, skinColor.g, skinColor.b});
    vertices.insert(vertices.end(), {0.0f, 0.0f, depth, 0.0f, 0.0f, 1.0f, skinColor.r, skinColor.g, skinColor.b});
    
    // Back face
    vertices.insert(vertices.end(), {width, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, skinColor.r, skinColor.g, skinColor.b});
    vertices.insert(vertices.end(), {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, skinColor.r, skinColor.g, skinColor.b});
    vertices.insert(vertices.end(), {0.0f, height, 0.0f, 0.0f, 0.0f, -1.0f, skinColor.r, skinColor.g, skinColor.b});
    vertices.insert(vertices.end(), {0.0f, height, 0.0f, 0.0f, 0.0f, -1.0f, skinColor.r, skinColor.g, skinColor.b});
    vertices.insert(vertices.end(), {width, height, 0.0f, 0.0f, 0.0f, -1.0f, skinColor.r, skinColor.g, skinColor.b});
    vertices.insert(vertices.end(), {width, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, skinColor.r, skinColor.g, skinColor.b});
    
    // Top face (reversed winding order for proper culling when rotated)
    vertices.insert(vertices.end(), {0.0f, height, depth, 0.0f, 1.0f, 0.0f, skinColor.r, skinColor.g, skinColor.b});
    vertices.insert(vertices.end(), {0.0f, height, 0.0f, 0.0f, 1.0f, 0.0f, skinColor.r, skinColor.g, skinColor.b});
    vertices.insert(vertices.end(), {width, height, 0.0f, 0.0f, 1.0f, 0.0f, skinColor.r, skinColor.g, skinColor.b});
    vertices.insert(vertices.end(), {width, height, 0.0f, 0.0f, 1.0f, 0.0f, skinColor.r, skinColor.g, skinColor.b});
    vertices.insert(vertices.end(), {width, height, depth, 0.0f, 1.0f, 0.0f, skinColor.r, skinColor.g, skinColor.b});
    vertices.insert(vertices.end(), {0.0f, height, depth, 0.0f, 1.0f, 0.0f, skinColor.r, skinColor.g, skinColor.b});
    
    // Bottom face
    vertices.insert(vertices.end(), {0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, skinColor.r, skinColor.g, skinColor.b});
    vertices.insert(vertices.end(), {width, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, skinColor.r, skinColor.g, skinColor.b});
    vertices.insert(vertices.end(), {width, 0.0f, depth, 0.0f, -1.0f, 0.0f, skinColor.r, skinColor.g, skinColor.b});
    vertices.insert(vertices.end(), {width, 0.0f, depth, 0.0f, -1.0f, 0.0f, skinColor.r, skinColor.g, skinColor.b});
    vertices.insert(vertices.end(), {0.0f, 0.0f, depth, 0.0f, -1.0f, 0.0f, skinColor.r, skinColor.g, skinColor.b});
    vertices.insert(vertices.end(), {0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, skinColor.r, skinColor.g, skinColor.b});
    
    // Right face
    vertices.insert(vertices.end(), {width, 0.0f, depth, 1.0f, 0.0f, 0.0f, skinColor.r, skinColor.g, skinColor.b});
    vertices.insert(vertices.end(), {width, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, skinColor.r, skinColor.g, skinColor.b});
    vertices.insert(vertices.end(), {width, height, 0.0f, 1.0f, 0.0f, 0.0f, skinColor.r, skinColor.g, skinColor.b});
    vertices.insert(vertices.end(), {width, height, 0.0f, 1.0f, 0.0f, 0.0f, skinColor.r, skinColor.g, skinColor.b});
    vertices.insert(vertices.end(), {width, height, depth, 1.0f, 0.0f, 0.0f, skinColor.r, skinColor.g, skinColor.b});
    vertices.insert(vertices.end(), {width, 0.0f, depth, 1.0f, 0.0f, 0.0f, skinColor.r, skinColor.g, skinColor.b});
    
    // Left face
    vertices.insert(vertices.end(), {0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, skinColor.r, skinColor.g, skinColor.b});
    vertices.insert(vertices.end(), {0.0f, 0.0f, depth, -1.0f, 0.0f, 0.0f, skinColor.r, skinColor.g, skinColor.b});
    vertices.insert(vertices.end(), {0.0f, height, depth, -1.0f, 0.0f, 0.0f, skinColor.r, skinColor.g, skinColor.b});
    vertices.insert(vertices.end(), {0.0f, height, depth, -1.0f, 0.0f, 0.0f, skinColor.r, skinColor.g, skinColor.b});
    vertices.insert(vertices.end(), {0.0f, height, 0.0f, -1.0f, 0.0f, 0.0f, skinColor.r, skinColor.g, skinColor.b});
    vertices.insert(vertices.end(), {0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, skinColor.r, skinColor.g, skinColor.b});
    
    glGenVertexArrays(1, &m_handVAO);
    glGenBuffers(1, &m_handVBO);
    
    glBindVertexArray(m_handVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_handVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    
    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    // Color attribute
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    glBindVertexArray(0);
}

void Game::UpdateHandAnimation(float deltaTime, bool isMoving)
{
    // Update bob timer when moving
    if (isMoving)
    {
        float bobSpeed = 5.0f;  // Base bobbing speed
        
        // Adjust speed based on movement state
        if (m_camera->IsSprinting)
            bobSpeed = 8.0f;  // Faster when sprinting
        else if (m_camera->BaseSpeed < 3.0f)  // Crouching
            bobSpeed = 3.0f;   // Slower when crouching
        
        m_handBobTimer += deltaTime * bobSpeed;
    }
    else
    {
        // Finish the current bob cycle before stopping
        if (m_handBobTimer > 0.0f)
        {
            // Continue the bob at reduced speed until we reach a neutral position
            m_handBobTimer += deltaTime * 3.0f;
            
            // Snap to zero when we've completed a cycle (when sine is near zero)
            float bobAmount = sin(m_handBobTimer);
            if (fabs(bobAmount) < 0.1f)  // Near neutral position
            {
                m_handBobTimer = 0.0f;
            }
        }
    }
    
    // Update swing animation
    if (m_isSwinging)
    {
        m_handSwingTimer += deltaTime * 10.0f;  // Fast swing animation
        
        if (m_handSwingTimer >= 1.0f)
        {
            m_handSwingTimer = 0.0f;
            m_isSwinging = false;
        }
    }
}

void Game::RenderHand()
{
    // Save current OpenGL state
    GLboolean depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    
    // Disable depth test so hand renders on top
    glDisable(GL_DEPTH_TEST);
    
    // Always force face culling off for hand rendering
    glDisable(GL_CULL_FACE);
    
    // Calculate hand position and rotation using a separate view/projection for UI space
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    float handAspectRatio = (float)viewport[2] / (float)viewport[3];
    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), handAspectRatio, 0.01f, 10.0f);
    
    // Position hand in screen space (center-right, lower)
    glm::vec3 handPos(0.45f, -0.6f, -2.0f);  // Left, down, forward from view center
    
    // Apply bobbing when moving
    float bobAmount = sin(m_handBobTimer) * 0.05f;
    handPos.y += bobAmount;
    
    // Apply swing animation
    float swingRotation = 0.0f;
    if (m_isSwinging)
    {
        // Swing down and back up
        float swingProgress = m_handSwingTimer;
        swingRotation = sin(swingProgress * 3.14159f) * -45.0f;  // -45 to 0 degrees
        handPos.y -= sin(swingProgress * 3.14159f) * 0.3f;
    }
    
    // Create hand transformation matrix
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, handPos);
    model = glm::rotate(model, glm::radians(45.0f), glm::vec3(1.0f, 0.0f, 0.0f));  // Rotate 45 degrees upward
    model = glm::scale(model, glm::vec3(1.2f, 1.2f, 1.2f));
    
    // Set shader uniforms
    m_shader->use();
    m_shader->setMat4("model", glm::value_ptr(model));
    m_shader->setMat4("view", glm::value_ptr(view));
    m_shader->setMat4("projection", glm::value_ptr(projection));
    
    // Disable lighting for hand - render with pure vertex color
    m_shader->setBool("unlit", true);
    
    // Draw hand
    glBindVertexArray(m_handVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    
    // Restore OpenGL state (but keep culling disabled since it wasn't originally enabled)
    if (depthTestEnabled)
        glEnable(GL_DEPTH_TEST);
    // Don't restore cull face - it should stay disabled
}

glm::vec3 Game::GetCameraPosition() const
{
    if (m_camera)
        return m_camera->Position;
    return glm::vec3(0.0f);
}

void Game::ProcessMouseButton(int button, int action)
{
    // Don't process block interactions if GUI is open
    if (m_showGUI)
        return;
    
    // Left mouse button (button 0) - Break block
    if (button == 0 && action == 1)  // GLFW_MOUSE_BUTTON_LEFT and GLFW_PRESS
    {
        m_isSwinging = true;
        m_handSwingTimer = 0.0f;
        
        // Raycast to find block player is looking at
        glm::vec3 hitPos;
        if (RaycastBlock(hitPos, 8.0f))  // 8 block range
        {
            // Destroy the block
            m_chunkManager->DestroyBlock(hitPos.x, hitPos.y, hitPos.z);
        }
    }
    
    // Right mouse button (button 1) - Place block or interact with pet
    if (button == 1 && action == 1)  // GLFW_MOUSE_BUTTON_RIGHT and GLFW_PRESS
    {
        // First check if clicking on pet
        glm::vec3 rayDir = m_camera->Front;
        glm::vec3 rayOrigin = m_camera->Position;
        float petDistance = glm::length(m_petPosition - rayOrigin);
        
        // Simple sphere collision for pet interaction (within 8 blocks)
        if (petDistance <= 8.0f)
        {
            // Check if ray passes near pet
            float t = glm::dot(m_petPosition - rayOrigin, rayDir);
            if (t > 0)
            {
                glm::vec3 closest = rayOrigin + rayDir * t;
                float distToPet = glm::length(closest - m_petPosition);
                
                if (distToPet < 0.5f)  // Hit pet
                {
                    TogglePetSit();
                    return;  // Don't place block
                }
            }
        }
        
        // Only place if not holding empty hand
        if (m_selectedSlot > 0)
        {
            glm::vec3 hitPos, normal;
            if (RaycastBlockWithNormal(hitPos, normal, 8.0f))  // 8 block range
            {
                // Place block adjacent to hit surface
                glm::vec3 placePos = hitPos + normal;
                
                // Don't place block where player is standing
                glm::vec3 playerPos = m_camera->Position;
                
                // Check if placement would collide with player's body (not just feet)
                // Use full 3D distance check
                float dist = glm::distance(placePos, playerPos);
                
                if (dist > 0.5f)  // Reduced radius so player can place blocks closer
                {
                    m_chunkManager->PlaceBlock(placePos.x, placePos.y, placePos.z, m_hotbarItems[m_selectedSlot]);
                }
            }
        }
    }
}

void Game::ProcessMouseScroll(double yoffset)
{
    // Scroll up = previous slot, scroll down = next slot
    m_selectedSlot -= (int)yoffset;
    
    // Wrap around
    if (m_selectedSlot < 0) m_selectedSlot = 8;
    if (m_selectedSlot > 8) m_selectedSlot = 0;
}

void Game::InitializeCrosshair()
{
    // Crosshair size in normalized device coordinates (-1 to 1)
    float size = 0.02f;  // Small crosshair
    float thickness = 0.004f;  // Thin lines
    
    // Crosshair vertices (two rectangles forming a +)
    float vertices[] = {
        // Horizontal line
        -size, -thickness, 0.0f,  1.0f, 1.0f, 1.0f,  // Left
         size, -thickness, 0.0f,  1.0f, 1.0f, 1.0f,  // Right
         size,  thickness, 0.0f,  1.0f, 1.0f, 1.0f,  // Right top
        -size,  thickness, 0.0f,  1.0f, 1.0f, 1.0f,  // Left top
        
        // Vertical line
        -thickness, -size, 0.0f,  1.0f, 1.0f, 1.0f,  // Bottom
         thickness, -size, 0.0f,  1.0f, 1.0f, 1.0f,  // Bottom right
         thickness,  size, 0.0f,  1.0f, 1.0f, 1.0f,  // Top right
        -thickness,  size, 0.0f,  1.0f, 1.0f, 1.0f   // Top
    };
    
    glGenVertexArrays(1, &m_crosshairVAO);
    glGenBuffers(1, &m_crosshairVBO);
    
    glBindVertexArray(m_crosshairVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_crosshairVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Color attribute
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    glBindVertexArray(0);
}

void Game::RenderCrosshair()
{
    // Save OpenGL state
    GLboolean depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    
    // Disable depth test so crosshair always appears on top
    glDisable(GL_DEPTH_TEST);
    
    m_shader->use();
    m_shader->setBool("unlit", true);  // No lighting for crosshair
    
    // Get current viewport to calculate aspect ratio
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    float aspectRatio = (float)viewport[2] / (float)viewport[3];  // width / height
    
    // Create orthographic projection that accounts for aspect ratio
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 projection = glm::ortho(-aspectRatio, aspectRatio, -1.0f, 1.0f, -1.0f, 1.0f);
    
    m_shader->setMat4("model", glm::value_ptr(model));
    m_shader->setMat4("view", glm::value_ptr(view));
    m_shader->setMat4("projection", glm::value_ptr(projection));
    
    // Draw horizontal line (4 vertices = 2 triangles)
    glBindVertexArray(m_crosshairVAO);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glDrawArrays(GL_TRIANGLE_FAN, 4, 4);
    glBindVertexArray(0);
    
    // Restore OpenGL state
    if (depthTestEnabled)
        glEnable(GL_DEPTH_TEST);
}

void Game::InitializeHotbar()
{
    // Simple initialization - hotbar will be drawn procedurally in RenderHotbar
    glGenVertexArrays(1, &m_hotbarVAO);
    glGenBuffers(1, &m_hotbarVBO);
}

void Game::RenderHotbar(int windowWidth, int windowHeight)
{
    // Save OpenGL state
    GLboolean depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    glDisable(GL_DEPTH_TEST);
    
    m_shader->use();
    m_shader->setBool("unlit", true);
    
    // Calculate aspect ratio
    float aspectRatio = (float)windowWidth / (float)windowHeight;
    
    // Orthographic projection
    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 projection = glm::ortho(-aspectRatio, aspectRatio, -1.0f, 1.0f, -1.0f, 1.0f);
    
    // Hotbar dimensions
    float slotSize = 0.08f;
    float slotSpacing = 0.01f;
    float totalWidth = 9 * slotSize + 8 * slotSpacing;
    float startX = -totalWidth / 2.0f;
    float bottomY = -0.85f;  // Near bottom of screen
    
    // Draw each hotbar slot
    for (int i = 0; i < 9; i++)
    {
        float x = startX + i * (slotSize + slotSpacing);
        
        // Create slot rectangle with position, normal, and color
        float vertices[] = {
            // pos                                normal                    color
            x, bottomY, 0.0f,                     0.0f, 0.0f, 1.0f,        0.5f, 0.5f, 0.5f,
            x + slotSize, bottomY, 0.0f,          0.0f, 0.0f, 1.0f,        0.5f, 0.5f, 0.5f,
            x + slotSize, bottomY + slotSize, 0.0f, 0.0f, 0.0f, 1.0f,     0.5f, 0.5f, 0.5f,
            x, bottomY + slotSize, 0.0f,          0.0f, 0.0f, 1.0f,        0.5f, 0.5f, 0.5f
        };
        
        // Highlight selected slot
        if (i == m_selectedSlot)
        {
            vertices[6] = vertices[15] = vertices[24] = vertices[33] = 1.0f;  // R
            vertices[7] = vertices[16] = vertices[25] = vertices[34] = 1.0f;  // G
            vertices[8] = vertices[17] = vertices[26] = vertices[35] = 1.0f;  // B
        }
        
        glBindVertexArray(m_hotbarVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_hotbarVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
        
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);
        
        glm::mat4 model = glm::mat4(1.0f);
        m_shader->setMat4("model", glm::value_ptr(model));
        m_shader->setMat4("view", glm::value_ptr(view));
        m_shader->setMat4("projection", glm::value_ptr(projection));
        
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        
        // Draw block icon inside slot if not empty hand
        if (m_hotbarItems[i] != BlockType::AIR)
        {
            Block tempBlock(m_hotbarItems[i]);
            glm::vec3 blockColor = tempBlock.GetColor();
            float iconSize = slotSize * 0.6f;
            float iconOffset = (slotSize - iconSize) / 2.0f;
            float iconX = x + iconOffset;
            float iconY = bottomY + iconOffset;
            
            float iconVertices[] = {
                // pos                                      normal                    color
                iconX, iconY, 0.0f,                        0.0f, 0.0f, 1.0f,        blockColor.r, blockColor.g, blockColor.b,
                iconX + iconSize, iconY, 0.0f,             0.0f, 0.0f, 1.0f,        blockColor.r, blockColor.g, blockColor.b,
                iconX + iconSize, iconY + iconSize, 0.0f,  0.0f, 0.0f, 1.0f,        blockColor.r, blockColor.g, blockColor.b,
                iconX, iconY + iconSize, 0.0f,             0.0f, 0.0f, 1.0f,        blockColor.r, blockColor.g, blockColor.b
            };
            
            glBufferData(GL_ARRAY_BUFFER, sizeof(iconVertices), iconVertices, GL_DYNAMIC_DRAW);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        }
    }
    
    glBindVertexArray(0);
    
    // Restore OpenGL state
    if (depthTestEnabled)
        glEnable(GL_DEPTH_TEST);
}

void Game::InitializeBlockOutline()
{
    // Create VAO and VBO for block outline
    glGenVertexArrays(1, &m_outlineVAO);
    glGenBuffers(1, &m_outlineVBO);
}

void Game::RenderBlockOutline()
{
    // Raycast to find block player is looking at
    glm::vec3 hitPos;
    if (!RaycastBlock(hitPos, 8.0f))
        return;  // No block in range
    
    // Save OpenGL state
    GLboolean depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    
    m_shader->use();
    m_shader->setBool("unlit", true);
    
    // Setup matrices
    glm::mat4 view = m_camera->GetViewMatrix();
    
    // Get window dimensions for aspect ratio
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    float aspectRatio = (float)viewport[2] / (float)viewport[3];
    glm::mat4 projection = glm::perspective(glm::radians(m_fov), aspectRatio, 0.1f, 1000.0f);
    
    // Create slightly larger cube around the block (1.01 scale for outline offset)
    float offset = 0.01f;
    float x = hitPos.x - offset;
    float y = hitPos.y - offset;
    float z = hitPos.z - offset;
    float size = 1.0f + 2.0f * offset;
    
    // Define outline edges (12 edges of a cube)
    float outlineVertices[] = {
        // Bottom square
        x, y, z,                0.0f, 0.0f, 0.0f,  0.0f, 0.0f, 0.0f,
        x + size, y, z,         0.0f, 0.0f, 0.0f,  0.0f, 0.0f, 0.0f,
        
        x + size, y, z,         0.0f, 0.0f, 0.0f,  0.0f, 0.0f, 0.0f,
        x + size, y, z + size,  0.0f, 0.0f, 0.0f,  0.0f, 0.0f, 0.0f,
        
        x + size, y, z + size,  0.0f, 0.0f, 0.0f,  0.0f, 0.0f, 0.0f,
        x, y, z + size,         0.0f, 0.0f, 0.0f,  0.0f, 0.0f, 0.0f,
        
        x, y, z + size,         0.0f, 0.0f, 0.0f,  0.0f, 0.0f, 0.0f,
        x, y, z,                0.0f, 0.0f, 0.0f,  0.0f, 0.0f, 0.0f,
        
        // Top square
        x, y + size, z,                0.0f, 0.0f, 0.0f,  0.0f, 0.0f, 0.0f,
        x + size, y + size, z,         0.0f, 0.0f, 0.0f,  0.0f, 0.0f, 0.0f,
        
        x + size, y + size, z,         0.0f, 0.0f, 0.0f,  0.0f, 0.0f, 0.0f,
        x + size, y + size, z + size,  0.0f, 0.0f, 0.0f,  0.0f, 0.0f, 0.0f,
        
        x + size, y + size, z + size,  0.0f, 0.0f, 0.0f,  0.0f, 0.0f, 0.0f,
        x, y + size, z + size,         0.0f, 0.0f, 0.0f,  0.0f, 0.0f, 0.0f,
        
        x, y + size, z + size,         0.0f, 0.0f, 0.0f,  0.0f, 0.0f, 0.0f,
        x, y + size, z,                0.0f, 0.0f, 0.0f,  0.0f, 0.0f, 0.0f,
        
        // Vertical edges
        x, y, z,                0.0f, 0.0f, 0.0f,  0.0f, 0.0f, 0.0f,
        x, y + size, z,         0.0f, 0.0f, 0.0f,  0.0f, 0.0f, 0.0f,
        
        x + size, y, z,         0.0f, 0.0f, 0.0f,  0.0f, 0.0f, 0.0f,
        x + size, y + size, z,  0.0f, 0.0f, 0.0f,  0.0f, 0.0f, 0.0f,
        
        x + size, y, z + size,         0.0f, 0.0f, 0.0f,  0.0f, 0.0f, 0.0f,
        x + size, y + size, z + size,  0.0f, 0.0f, 0.0f,  0.0f, 0.0f, 0.0f,
        
        x, y, z + size,         0.0f, 0.0f, 0.0f,  0.0f, 0.0f, 0.0f,
        x, y + size, z + size,  0.0f, 0.0f, 0.0f,  0.0f, 0.0f, 0.0f
    };
    
    glBindVertexArray(m_outlineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_outlineVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(outlineVertices), outlineVertices, GL_DYNAMIC_DRAW);
    
    // Setup vertex attributes (position, normal, color - all black for outline)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    // Set matrices
    glm::mat4 model = glm::mat4(1.0f);
    m_shader->setMat4("model", glm::value_ptr(model));
    m_shader->setMat4("view", glm::value_ptr(view));
    m_shader->setMat4("projection", glm::value_ptr(projection));
    
    // Draw lines with thicker width
    glLineWidth(2.0f);
    glDrawArrays(GL_LINES, 0, 24);  // 24 vertices = 12 lines
    glLineWidth(1.0f);
    
    glBindVertexArray(0);
    
    // Restore OpenGL state
    if (depthTestEnabled)
        glEnable(GL_DEPTH_TEST);
}

void Game::InitializePet()
{
    // Load Tom model
    LoadModel("resources/models/Tom/TomAdult (merge).fbx");
    
    std::cout << "Pet model loaded with " << m_petMeshes.size() << " meshes" << std::endl;
}

void Game::UpdatePet(float deltaTime)
{
    if (m_petSitting)
    {
        // If sitting, stay at sit position
        m_petPosition = m_petSitPosition;
        m_petVelocity = glm::vec3(0.0f);
        return;
    }

    // Follow the player
    glm::vec3 targetPosition = m_camera->Position - glm::vec3(0.0f, PLAYER_EYE_HEIGHT, 0.0f);
    glm::vec3 toPlayer = targetPosition - m_petPosition;
    float distance = glm::length(toPlayer);

    // Keep a distance of 4-6 blocks from the player
    const float MIN_DISTANCE = 4.0f;
    const float MAX_DISTANCE = 20.0f;
    const float MOVE_SPEED = 5.0f;

    // If too far, teleport to player
    if (distance > MAX_DISTANCE)
    {
        m_petPosition = targetPosition + glm::vec3(2.0f, 0.0f, 2.0f);
        m_petVelocity = glm::vec3(0.0f);
        return;
    }

    // Move towards player if beyond minimum distance
    if (distance > MIN_DISTANCE)
    {
        glm::vec3 direction = glm::normalize(toPlayer);
        glm::vec3 targetVelocity = direction * MOVE_SPEED;
        
        // Smooth velocity changes
        m_petVelocity = glm::mix(m_petVelocity, targetVelocity, deltaTime * 5.0f);
        
        // Apply horizontal movement
        glm::vec3 horizontalVelocity = glm::vec3(m_petVelocity.x, 0.0f, m_petVelocity.z);
        m_petPosition += horizontalVelocity * deltaTime;
    }
    else
    {
        // Close enough, slow down
        m_petVelocity = glm::mix(m_petVelocity, glm::vec3(0.0f), deltaTime * 10.0f);
    }

    // Ground collision - check for ground BEFORE applying gravity
    float petHalfHeight = 0.25f * m_petScale;
    float targetGroundHeight = -1000.0f;
    bool onGround = false;
    
    // Check if there's a solid block below
    for (float checkDist = 0.0f; checkDist <= petHalfHeight + 0.5f; checkDist += 0.1f)
    {
        glm::vec3 checkPos = m_petPosition - glm::vec3(0.0f, checkDist, 0.0f);
        if (m_chunkManager->IsSolid(checkPos.x, checkPos.y, checkPos.z))
        {
            int blockY = (int)floor(checkPos.y);
            targetGroundHeight = (float)blockY + 1.0f + petHalfHeight;
            onGround = true;
            break;
        }
    }
    
    // Apply gravity only if not on ground
    if (onGround)
    {
        // Smoothly move to ground height if close
        if (abs(m_petPosition.y - targetGroundHeight) < 0.01f)
        {
            m_petPosition.y = targetGroundHeight;
            m_petVelocity.y = 0.0f;
        }
        else
        {
            // Interpolate to ground height
            m_petPosition.y = glm::mix(m_petPosition.y, targetGroundHeight, deltaTime * 10.0f);
            m_petVelocity.y = 0.0f;
        }
    }
    else
    {
        // In air - apply gravity
        const float GRAVITY = -20.0f;
        m_petVelocity.y += GRAVITY * deltaTime;
        m_petPosition.y += m_petVelocity.y * deltaTime;
    }
}

void Game::RenderPet()
{
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, m_petPosition);
    
    // Rotate pet to face the player
    if (!m_petSitting)
    {
        glm::vec3 toPlayer = m_camera->Position - m_petPosition;
        float angle = atan2(toPlayer.x, toPlayer.z);
        model = glm::rotate(model, angle, glm::vec3(0.0f, 1.0f, 0.0f));
    }
    
    // Apply scale from GUI (Tom model is large, so scale down)
    model = glm::scale(model, glm::vec3(m_petScale * 0.01f));

    m_shader->setMat4("model", glm::value_ptr(model));

    // Render all meshes
    static bool debugPrinted = false;
    for (size_t i = 0; i < m_petMeshes.size(); i++)
    {
        // Bind texture if available
        if (m_petMeshes[i].textureID != 0)
        {
            if (!debugPrinted)
            {
                std::cout << "Rendering mesh " << i << " with texture ID: " << m_petMeshes[i].textureID << std::endl;
            }
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, m_petMeshes[i].textureID);
            m_shader->setInt("diffuseTexture", 1);
            m_shader->setBool("useTexture", true);
        }
        else
        {
            if (!debugPrinted)
            {
                std::cout << "Rendering mesh " << i << " without texture (using vertex colors)" << std::endl;
            }
            m_shader->setBool("useTexture", false);
        }
        
        glBindVertexArray(m_petMeshes[i].VAO);
        glDrawElements(GL_TRIANGLES, m_petMeshes[i].indices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
        
        m_shader->setBool("useTexture", false);
    }
    debugPrinted = true;
}

void Game::TogglePetSit()
{
    m_petSitting = !m_petSitting;
    if (m_petSitting)
    {
        // Remember position where pet sits
        m_petSitPosition = m_petPosition;
        std::cout << "Pet is now sitting" << std::endl;
    }
    else
    {
        std::cout << "Pet is now following" << std::endl;
    }
}

void Game::InitializeGUI()
{
    // Simple VAO/VBO for GUI rendering
    glGenVertexArrays(1, &m_guiVAO);
    glGenBuffers(1, &m_guiVBO);
}

void Game::ToggleGUI(GLFWwindow* window)
{
    m_showGUI = !m_showGUI;
    m_window = window;
    
    // Toggle cursor mode
    if (m_showGUI)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        std::cout << "GUI enabled - Cursor unlocked" << std::endl;
    }
    else
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        std::cout << "GUI disabled - Cursor locked" << std::endl;
    }
}

bool Game::IsMouseOverGUI(double mouseX, double mouseY, int windowWidth, int windowHeight)
{
    if (!m_showGUI) return false;
    
    // GUI panel is in top-right corner: 20% width, 40% height
    float panelLeft = windowWidth * 0.78f;
    float panelTop = windowHeight * 0.02f;
    float panelRight = windowWidth * 0.98f;
    float panelBottom = windowHeight * 0.42f;
    
    return mouseX >= panelLeft && mouseX <= panelRight && 
           mouseY >= panelTop && mouseY <= panelBottom;
}

void Game::HandleGUIInteraction(GLFWwindow* window, int windowWidth, int windowHeight)
{
    // Get mouse position
    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);
    
    // Check if left mouse button is pressed
    bool mousePressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    
    if (!mousePressed) return;
    
    // GUI panel dimensions
    float panelWidth = windowWidth * 0.2f;
    float panelX = windowWidth - panelWidth - windowWidth * 0.02f;
    float panelY = windowHeight * 0.02f;
    
    float sliderY = panelY + 60.0f;  // Account for title space
    float sliderSpacing = 70.0f;
    float sliderWidth = panelWidth * 0.8f;
    float sliderHeight = 8.0f;
    float sliderX = panelX + panelWidth * 0.1f;
    
    // Helper to check and update slider
    auto updateSlider = [&](float& value, float minVal, float maxVal, float yPos) {
        if (mouseY >= yPos - 10 && mouseY <= yPos + sliderHeight + 10 &&
            mouseX >= sliderX && mouseX <= sliderX + sliderWidth)
        {
            float percent = (mouseX - sliderX) / sliderWidth;
            percent = glm::clamp(percent, 0.0f, 1.0f);
            value = minVal + percent * (maxVal - minVal);
            return true;
        }
        return false;
    };
    
    // Update sliders
    updateSlider(m_fov, 30.0f, 90.0f, sliderY);
    updateSlider(m_mouseSensitivity, 0.05f, 0.3f, sliderY + sliderSpacing);
    updateSlider(m_petScale, 0.5f, 3.0f, sliderY + sliderSpacing * 2);
    updateSlider(m_timeSpeed, 0.1f, 5.0f, sliderY + sliderSpacing * 3);
}

void Game::RenderGUI(int windowWidth, int windowHeight)
{
    // Save current OpenGL state
    GLboolean depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    glDisable(GL_DEPTH_TEST);
    
    // Setup orthographic projection for GUI
    glm::mat4 projection = glm::ortho(0.0f, (float)windowWidth, (float)windowHeight, 0.0f, -1.0f, 1.0f);
    glm::mat4 view = glm::mat4(1.0f);
    
    m_shader->use();
    m_shader->setMat4("projection", glm::value_ptr(projection));
    m_shader->setMat4("view", glm::value_ptr(view));
    m_shader->setBool("unlit", true);
    
    // GUI panel dimensions (top-right corner)
    float panelWidth = windowWidth * 0.2f;
    float panelHeight = windowHeight * 0.4f;
    float panelX = windowWidth - panelWidth - windowWidth * 0.02f;
    float panelY = windowHeight * 0.02f;
    
    // Draw panel background (semi-transparent dark gray)
    float panelVertices[] = {
        // Position (x, y, z)         // Normal              // Color (RGBA-like, semi-transparent dark)
        panelX, panelY, 0.0f,         0.0f, 0.0f, 1.0f,     0.1f, 0.1f, 0.1f,
        panelX + panelWidth, panelY, 0.0f, 0.0f, 0.0f, 1.0f, 0.1f, 0.1f, 0.1f,
        panelX + panelWidth, panelY + panelHeight, 0.0f, 0.0f, 0.0f, 1.0f, 0.1f, 0.1f, 0.1f,
        panelX + panelWidth, panelY + panelHeight, 0.0f, 0.0f, 0.0f, 1.0f, 0.1f, 0.1f, 0.1f,
        panelX, panelY + panelHeight, 0.0f, 0.0f, 0.0f, 1.0f, 0.1f, 0.1f, 0.1f,
        panelX, panelY, 0.0f,         0.0f, 0.0f, 1.0f,     0.1f, 0.1f, 0.1f
    };
    
    glBindVertexArray(m_guiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_guiVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(panelVertices), panelVertices, GL_DYNAMIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    glm::mat4 model = glm::mat4(1.0f);
    m_shader->setMat4("model", glm::value_ptr(model));
    
    // Enable blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glDrawArrays(GL_TRIANGLES, 0, 6);
    
    // Draw title bar (top of panel)
    float titleHeight = 30.0f;
    float titleVertices[] = {
        panelX, panelY, 0.0f, 0.0f, 0.0f, 1.0f, 0.2f, 0.3f, 0.4f,
        panelX + panelWidth, panelY, 0.0f, 0.0f, 0.0f, 1.0f, 0.2f, 0.3f, 0.4f,
        panelX + panelWidth, panelY + titleHeight, 0.0f, 0.0f, 0.0f, 1.0f, 0.2f, 0.3f, 0.4f,
        panelX + panelWidth, panelY + titleHeight, 0.0f, 0.0f, 0.0f, 1.0f, 0.2f, 0.3f, 0.4f,
        panelX, panelY + titleHeight, 0.0f, 0.0f, 0.0f, 1.0f, 0.2f, 0.3f, 0.4f,
        panelX, panelY, 0.0f, 0.0f, 0.0f, 1.0f, 0.2f, 0.3f, 0.4f
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(titleVertices), titleVertices, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    
    // Draw sliders for each setting
    float sliderY = panelY + 60.0f;  // Start below title
    float sliderSpacing = 70.0f;
    float sliderWidth = panelWidth * 0.8f;
    float sliderHeight = 8.0f;
    float sliderX = panelX + panelWidth * 0.1f;
    
    // Helper lambda to draw a slider
    auto drawSlider = [&](float value, float minVal, float maxVal, float yPos, glm::vec3 color) {
        // Slider track (gray)
        float trackVertices[] = {
            sliderX, yPos, 0.0f, 0.0f, 0.0f, 1.0f, 0.3f, 0.3f, 0.3f,
            sliderX + sliderWidth, yPos, 0.0f, 0.0f, 0.0f, 1.0f, 0.3f, 0.3f, 0.3f,
            sliderX + sliderWidth, yPos + sliderHeight, 0.0f, 0.0f, 0.0f, 1.0f, 0.3f, 0.3f, 0.3f,
            sliderX + sliderWidth, yPos + sliderHeight, 0.0f, 0.0f, 0.0f, 1.0f, 0.3f, 0.3f, 0.3f,
            sliderX, yPos + sliderHeight, 0.0f, 0.0f, 0.0f, 1.0f, 0.3f, 0.3f, 0.3f,
            sliderX, yPos, 0.0f, 0.0f, 0.0f, 1.0f, 0.3f, 0.3f, 0.3f
        };
        
        glBufferData(GL_ARRAY_BUFFER, sizeof(trackVertices), trackVertices, GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        
        // Slider fill (colored, based on value)
        float fillPercent = (value - minVal) / (maxVal - minVal);
        float fillWidth = sliderWidth * fillPercent;
        float fillVertices[] = {
            sliderX, yPos, 0.0f, 0.0f, 0.0f, 1.0f, color.r, color.g, color.b,
            sliderX + fillWidth, yPos, 0.0f, 0.0f, 0.0f, 1.0f, color.r, color.g, color.b,
            sliderX + fillWidth, yPos + sliderHeight, 0.0f, 0.0f, 0.0f, 1.0f, color.r, color.g, color.b,
            sliderX + fillWidth, yPos + sliderHeight, 0.0f, 0.0f, 0.0f, 1.0f, color.r, color.g, color.b,
            sliderX, yPos + sliderHeight, 0.0f, 0.0f, 0.0f, 1.0f, color.r, color.g, color.b,
            sliderX, yPos, 0.0f, 0.0f, 0.0f, 1.0f, color.r, color.g, color.b
        };
        
        glBufferData(GL_ARRAY_BUFFER, sizeof(fillVertices), fillVertices, GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        
        // Slider handle (small square)
        float handleSize = 16.0f;
        float handleX = sliderX + fillWidth - handleSize/2;
        float handleY = yPos + sliderHeight/2 - handleSize/2;
        float handleVertices[] = {
            handleX, handleY, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f,
            handleX + handleSize, handleY, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f,
            handleX + handleSize, handleY + handleSize, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f,
            handleX + handleSize, handleY + handleSize, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f,
            handleX, handleY + handleSize, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f,
            handleX, handleY, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f
        };
        
        glBufferData(GL_ARRAY_BUFFER, sizeof(handleVertices), handleVertices, GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    };
    
    // Helper to draw simple text label above slider
    auto drawTextLabel = [&](const char* text, float yPos, glm::vec3 color) {
        float textX = sliderX;
        float textY = yPos - 25.0f;
        float charWidth = 3.0f;
        float charHeight = 10.0f;
        float charSpacing = 5.0f;
        
        // Draw simple text using small rectangles
        for (int i = 0; text[i] != '\0'; i++)
        {
            float x = textX + i * charSpacing;
            float charVertices[] = {
                x, textY, 0.0f, 0.0f, 0.0f, 1.0f, color.r, color.g, color.b,
                x + charWidth, textY, 0.0f, 0.0f, 0.0f, 1.0f, color.r, color.g, color.b,
                x + charWidth, textY + charHeight, 0.0f, 0.0f, 0.0f, 1.0f, color.r, color.g, color.b,
                x + charWidth, textY + charHeight, 0.0f, 0.0f, 0.0f, 1.0f, color.r, color.g, color.b,
                x, textY + charHeight, 0.0f, 0.0f, 0.0f, 1.0f, color.r, color.g, color.b,
                x, textY, 0.0f, 0.0f, 0.0f, 1.0f, color.r, color.g, color.b
            };
            glBufferData(GL_ARRAY_BUFFER, sizeof(charVertices), charVertices, GL_DYNAMIC_DRAW);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }
    };
    
    // Draw sliders and text labels for each setting
    // FOV (blue) - Camera field of view
    drawTextLabel("FOV", sliderY, glm::vec3(0.8f, 0.8f, 0.8f));
    drawSlider(m_fov, 30.0f, 90.0f, sliderY, glm::vec3(0.2f, 0.6f, 1.0f));
    
    // Mouse Sensitivity (orange) - Camera rotation speed
    drawTextLabel("MOUSE SENS", sliderY + sliderSpacing, glm::vec3(0.8f, 0.8f, 0.8f));
    drawSlider(m_mouseSensitivity, 0.05f, 0.3f, sliderY + sliderSpacing, glm::vec3(1.0f, 0.6f, 0.2f));
    
    // Pet Scale (pink) - Pet size multiplier
    drawTextLabel("PET SCALE", sliderY + sliderSpacing * 2, glm::vec3(0.8f, 0.8f, 0.8f));
    drawSlider(m_petScale, 0.5f, 3.0f, sliderY + sliderSpacing * 2, glm::vec3(0.8f, 0.4f, 0.6f));
    
    // Time Speed (green) - Day/night cycle speed
    drawTextLabel("TIME SPEED", sliderY + sliderSpacing * 3, glm::vec3(0.8f, 0.8f, 0.8f));
    drawSlider(m_timeSpeed, 0.1f, 5.0f, sliderY + sliderSpacing * 3, glm::vec3(0.6f, 1.0f, 0.4f));
    
    glDisable(GL_BLEND);
    glBindVertexArray(0);
    
    m_shader->setBool("unlit", false);
    
    // Restore OpenGL state
    if (depthTestEnabled)
        glEnable(GL_DEPTH_TEST);
}

void Game::LoadModel(const std::string& path)
{
    // Extract directory from path
    size_t lastSlash = path.find_last_of("/\\");
    if (lastSlash != std::string::npos)
    {
        m_modelDirectory = path.substr(0, lastSlash + 1);
    }
    
    std::cout << "Loading model from: " << path << std::endl;
    std::cout << "Texture directory: " << m_modelDirectory << std::endl;
    
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path, 
        aiProcess_Triangulate | 
        aiProcess_FlipUVs |
        aiProcess_GenNormals |
        aiProcess_JoinIdenticalVertices);
    
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
        std::cout << "ERROR: Assimp - " << importer.GetErrorString() << std::endl;
        return;
    }
    
    std::cout << "Model has " << scene->mNumMaterials << " materials" << std::endl;
    
    ProcessNode(scene->mRootNode, scene);
}

void Game::ProcessNode(void* node, const void* scene)
{
    aiNode* ainode = static_cast<aiNode*>(node);
    const aiScene* aiscene = static_cast<const aiScene*>(scene);
    
    // Process all meshes in this node
    for (unsigned int i = 0; i < ainode->mNumMeshes; i++)
    {
        aiMesh* mesh = aiscene->mMeshes[ainode->mMeshes[i]];
        m_petMeshes.push_back(ProcessMesh(mesh, aiscene));
    }
    
    // Recursively process child nodes
    for (unsigned int i = 0; i < ainode->mNumChildren; i++)
    {
        ProcessNode(ainode->mChildren[i], aiscene);
    }
}

Mesh Game::ProcessMesh(void* mesh, const void* scene)
{
    aiMesh* aimesh = static_cast<aiMesh*>(mesh);
    const aiScene* aiscene = static_cast<const aiScene*>(scene);
    
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    
    // Get material color if available
    glm::vec3 meshColor = glm::vec3(0.8f, 0.6f, 0.4f); // Default tan/brown color
    if (aimesh->mMaterialIndex >= 0)
    {
        aiMaterial* material = aiscene->mMaterials[aimesh->mMaterialIndex];
        aiColor3D color(1.0f, 1.0f, 1.0f);
        if (material->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS)
        {
            meshColor = glm::vec3(color.r, color.g, color.b);
        }
    }
    
    // Process vertices
    for (unsigned int i = 0; i < aimesh->mNumVertices; i++)
    {
        Vertex vertex;
        
        // Position
        vertex.Position.x = aimesh->mVertices[i].x;
        vertex.Position.y = aimesh->mVertices[i].y;
        vertex.Position.z = aimesh->mVertices[i].z;
        
        // Normals
        if (aimesh->HasNormals())
        {
            vertex.Normal.x = aimesh->mNormals[i].x;
            vertex.Normal.y = aimesh->mNormals[i].y;
            vertex.Normal.z = aimesh->mNormals[i].z;
        }
        else
        {
            vertex.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
        }
        
        // Use vertex colors if available, otherwise use material color
        if (aimesh->HasVertexColors(0))
        {
            vertex.Color.r = aimesh->mColors[0][i].r;
            vertex.Color.g = aimesh->mColors[0][i].g;
            vertex.Color.b = aimesh->mColors[0][i].b;
        }
        else
        {
            vertex.Color = meshColor;
        }
        
        // Texture coordinates
        if (aimesh->mTextureCoords[0])
        {
            vertex.TexCoords.x = aimesh->mTextureCoords[0][i].x;
            vertex.TexCoords.y = aimesh->mTextureCoords[0][i].y;
        }
        else
        {
            vertex.TexCoords = glm::vec2(0.0f, 0.0f);
        }
        
        vertices.push_back(vertex);
    }
    
    // Process indices
    for (unsigned int i = 0; i < aimesh->mNumFaces; i++)
    {
        aiFace face = aimesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++)
        {
            indices.push_back(face.mIndices[j]);
        }
    }
    
    // Load texture if material has one
    unsigned int textureID = 0;
    if (aimesh->mMaterialIndex >= 0)
    {
        aiMaterial* material = aiscene->mMaterials[aimesh->mMaterialIndex];
        
        // Get material name
        aiString matName;
        material->Get(AI_MATKEY_NAME, matName);
        std::string materialName = std::string(matName.C_Str());
        
        // Get mesh name
        std::string meshName = std::string(aimesh->mName.C_Str());
        
        std::cout << "Mesh: '" << meshName << "' | Material: '" << materialName << "'" << std::endl;
        std::cout << "Material has " << material->GetTextureCount(aiTextureType_DIFFUSE) << " diffuse textures" << std::endl;
        
        if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0)
        {
            aiString texPath;
            material->GetTexture(aiTextureType_DIFFUSE, 0, &texPath);
            std::string texPathStr = std::string(texPath.C_Str());
            std::cout << "Texture path from material: " << texPathStr << std::endl;
            
            // Handle different path formats
            std::string fullPath;
            if (texPathStr.find(":") != std::string::npos || texPathStr[0] == '/' || texPathStr[0] == '\\')
            {
                // Absolute path - try to extract just filename
                size_t lastSlash = texPathStr.find_last_of("/\\");
                if (lastSlash != std::string::npos)
                {
                    texPathStr = texPathStr.substr(lastSlash + 1);
                }
            }
            
            fullPath = m_modelDirectory + texPathStr;
            std::cout << "Loading texture from: " << fullPath << std::endl;
            textureID = LoadTexture(fullPath);
        }
        else
        {
            std::cout << "No diffuse texture in material, assigning by name..." << std::endl;
            
            // Check if loading Ben model
            bool isBenModel = (m_modelDirectory.find("Ben") != std::string::npos);
            
            if (isBenModel)
            {
                // For Ben model, assign textures based on mesh/material name
                std::string searchName = materialName + " " + meshName;
                std::transform(searchName.begin(), searchName.end(), searchName.begin(), ::tolower);
                
                std::string textureName;
                // Cylinder0041 = eyes, Cylinder0071 = front/face side, Cylinder0121 = back side
                if (searchName.find("0041") != std::string::npos || 
                    searchName.find("eye") != std::string::npos)
                {
                    textureName = "Cylinder0041_diff.png";
                    std::cout << "Detected eyes mesh, using Cylinder0041 texture" << std::endl;
                }
                else if (searchName.find("0121") != std::string::npos || 
                    searchName.find("back") != std::string::npos)
                {
                    textureName = "Cylinder0121_diff.png";
                    std::cout << "Detected back mesh, using Cylinder0121 texture" << std::endl;
                }
                else if (searchName.find("0071") != std::string::npos || 
                         searchName.find("front") != std::string::npos ||
                         searchName.find("face") != std::string::npos)
                {
                    textureName = "Cylinder0071_diff.png";
                    std::cout << "Detected front mesh, using Cylinder0071 texture" << std::endl;
                }
                else
                {
                    // Default to front texture
                    textureName = "Cylinder0071_diff.png";
                    std::cout << "Unknown Ben mesh ('" << meshName << "', '" << materialName << "'), defaulting to Cylinder0071" << std::endl;
                }
                
                std::string fullPath = m_modelDirectory + textureName;
                textureID = LoadTexture(fullPath);
            }
            else
            {
                // Assign textures based on material or mesh name for Tom
                std::string searchName = materialName + " " + meshName;
                // Convert to lowercase for comparison
                std::transform(searchName.begin(), searchName.end(), searchName.begin(), ::tolower);
                
                std::string textureName;
                if (searchName.find("eye") != std::string::npos)
                {
                    textureName = "Tom_Eyes_Default.png";
                    std::cout << "Detected eyes mesh, using eye texture" << std::endl;
                }
                else if (searchName.find("body") != std::string::npos || 
                         searchName.find("head") != std::string::npos ||
                         searchName.find("tom") != std::string::npos)
                {
                    textureName = "Tom_Body_Default.png";
                    std::cout << "Detected body mesh, using body texture" << std::endl;
                }
                else
                {
                    // Default to body texture
                    textureName = "Tom_Body_Default.png";
                    std::cout << "Unknown mesh type, defaulting to body texture" << std::endl;
                }
                
                std::string fullPath = m_modelDirectory + textureName;
                textureID = LoadTexture(fullPath);
            }
        }
    }
    
    Mesh resultMesh;
    resultMesh.vertices = vertices;
    resultMesh.indices = indices;
    resultMesh.textureID = textureID;
    resultMesh.SetupMesh();
    
    return resultMesh;
}

unsigned int Game::LoadTexture(const std::string& path)
{
    unsigned int textureID = 0;
    
    // Initialize GDI+
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
    
    // Convert path to wide string
    std::wstring wPath(path.begin(), path.end());
    
    // Load image using GDI+
    Gdiplus::Bitmap* bitmap = new Gdiplus::Bitmap(wPath.c_str());
    
    Gdiplus::Status status = bitmap->GetLastStatus();
    if (status != Gdiplus::Ok)
    {
        std::cout << "Failed to load texture: " << path << " (GDI+ Status: " << status << ")" << std::endl;
        delete bitmap;
        Gdiplus::GdiplusShutdown(gdiplusToken);
        return 0;
    }
    
    // Get image dimensions
    UINT width = bitmap->GetWidth();
    UINT height = bitmap->GetHeight();
    
    // Lock bitmap data
    Gdiplus::BitmapData bitmapData;
    Gdiplus::Rect rect(0, 0, width, height);
    bitmap->LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bitmapData);
    
    // Convert BGRA to RGBA
    unsigned char* pixels = new unsigned char[width * height * 4];
    unsigned char* src = (unsigned char*)bitmapData.Scan0;
    
    for (UINT i = 0; i < width * height; i++)
    {
        pixels[i * 4 + 0] = src[i * 4 + 2]; // R
        pixels[i * 4 + 1] = src[i * 4 + 1]; // G
        pixels[i * 4 + 2] = src[i * 4 + 0]; // B
        pixels[i * 4 + 3] = src[i * 4 + 3]; // A
    }
    
    // Create OpenGL texture
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // Cleanup
    delete[] pixels;
    bitmap->UnlockBits(&bitmapData);
    delete bitmap;
    Gdiplus::GdiplusShutdown(gdiplusToken);
    
    std::cout << "Loaded texture: " << path << " (" << width << "x" << height << ")" << std::endl;
    
    return textureID;
}

void Game::GenerateMobSpawnLocations()
{
    // Generate spawn locations in a grid with 100 block spacing
    const int SPAWN_SPACING = 100;
    const int SPAWN_RADIUS = 500; // Generate spawns within 500 blocks of origin
    
    for (int x = -SPAWN_RADIUS; x <= SPAWN_RADIUS; x += SPAWN_SPACING)
    {
        for (int z = -SPAWN_RADIUS; z <= SPAWN_RADIUS; z += SPAWN_SPACING)
        {
            // Add some random offset so they're not perfectly aligned
            float offsetX = ((rand() % 20) - 10) * 1.0f;
            float offsetZ = ((rand() % 20) - 10) * 1.0f;
            
            glm::vec3 spawnPos(x + offsetX, 50.0f, z + offsetZ); // Y will be adjusted to ground
            m_mobSpawnLocations.push_back(spawnPos);
        }
    }
    
    std::cout << "Generated " << m_mobSpawnLocations.size() << " mob spawn locations" << std::endl;
}

bool Game::IsMobSpawnValid(const glm::vec3& position)
{
    // Check if too close to player
    float distToPlayer = glm::length(position - m_camera->Position);
    if (distToPlayer < 20.0f)
        return false;
    
    // Check if position is on solid ground
    if (!m_chunkManager->IsSolid(position.x, position.y - 1.0f, position.z))
        return false;
    
    // Check if there's space above (2 blocks high)
    if (m_chunkManager->IsSolid(position.x, position.y + 1.0f, position.z) ||
        m_chunkManager->IsSolid(position.x, position.y + 2.0f, position.z))
        return false;
    
    return true;
}

void Game::InitializeMobs()
{
    std::cout << "Initializing mob system..." << std::endl;
    
    // Generate spawn locations
    GenerateMobSpawnLocations();
    
    // Create mob template by loading Ben model
    std::vector<Mesh> benMeshes;
    std::string oldModelDir = m_modelDirectory;
    
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile("resources/models/Ben/talking ben.obj", 
        aiProcess_Triangulate | 
        aiProcess_FlipUVs |
        aiProcess_GenNormals |
        aiProcess_JoinIdenticalVertices);
    
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
        std::cout << "ERROR: Failed to load Ben model - " << importer.GetErrorString() << std::endl;
        return;
    }
    
    m_modelDirectory = "resources/models/Ben/";
    
    // Process all meshes
    std::function<void(aiNode*, const aiScene*)> processNode = [&](aiNode* node, const aiScene* scene) {
        for (unsigned int i = 0; i < node->mNumMeshes; i++)
        {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            benMeshes.push_back(ProcessMesh(mesh, scene));
        }
        for (unsigned int i = 0; i < node->mNumChildren; i++)
        {
            processNode(node->mChildren[i], scene);
        }
    };
    
    processNode(scene->mRootNode, scene);
    m_modelDirectory = oldModelDir;
    
    std::cout << "Ben model loaded with " << benMeshes.size() << " meshes" << std::endl;
    
    // Spawn mobs at valid locations within render distance
    for (const auto& spawnLoc : m_mobSpawnLocations)
    {
        // Check if within reasonable distance of spawn
        float distToOrigin = glm::length(spawnLoc);
        if (distToOrigin > 300.0f)
            continue;
        
        // Find ground height
        float groundY = 50.0f;
        for (int y = 100; y > 0; y--)
        {
            if (m_chunkManager->IsSolid(spawnLoc.x, (float)y, spawnLoc.z))
            {
                groundY = (float)y + 1.0f;
                break;
            }
        }
        
        glm::vec3 mobPos(spawnLoc.x, groundY, spawnLoc.z);
        
        // Validate spawn location
        if (IsMobSpawnValid(mobPos))
        {
            Mob newMob;
            newMob.position = mobPos;
            newMob.spawnLocation = mobPos;
            newMob.velocity = glm::vec3(0.0f);
            newMob.rotationY = (rand() % 360) * 3.14159f / 180.0f;
            newMob.wanderTimer = 0.0f;
            newMob.wanderTarget = mobPos;
            newMob.isActive = true;
            newMob.meshes = benMeshes;
            m_mobs.push_back(newMob);
        }
    }
    
    std::cout << "Spawned " << m_mobs.size() << " Ben mobs" << std::endl;
}

void Game::UpdateMobs(float deltaTime)
{
    for (auto& mob : m_mobs)
    {
        if (!mob.isActive)
            continue;
        
        // Apply gravity
        const float GRAVITY = -20.0f;
        mob.velocity.y += GRAVITY * deltaTime;
        
        // Calculate next position
        float nextY = mob.position.y + mob.velocity.y * deltaTime;
        
        // Check if would collide with ground at next position (checking at foot level)
        if (m_chunkManager->IsSolid(mob.position.x, nextY, mob.position.z))
        {
            // Find ground level (highest solid block below mob)
            for (int checkY = (int)mob.position.y; checkY > (int)mob.position.y - 5; checkY--)
            {
                if (m_chunkManager->IsSolid(mob.position.x, (float)checkY, mob.position.z))
                {
                    // Position mob standing on top of this block
                    mob.position.y = (float)checkY + 1.0f;
                    mob.velocity.y = 0.0f;
                    break;
                }
            }
        }
        else
        {
            // No collision, apply the movement
            mob.position.y = nextY;
        }
        
        // Wandering behavior
        mob.wanderTimer -= deltaTime;
        
        if (mob.wanderTimer <= 0.0f)
        {
            // Pick a new random target within 10 blocks of spawn
            float offsetX = ((rand() % 200) - 100) * 0.1f; // -10 to +10
            float offsetZ = ((rand() % 200) - 100) * 0.1f;
            mob.wanderTarget = mob.spawnLocation + glm::vec3(offsetX, 0.0f, offsetZ);
            mob.wanderTimer = 3.0f + (rand() % 50) * 0.1f; // 3-8 seconds
        }
        
        // Move towards wander target
        glm::vec3 toTarget = mob.wanderTarget - mob.position;
        toTarget.y = 0.0f; // Only move horizontally
        float distToTarget = glm::length(toTarget);
        
        if (distToTarget > 0.5f)
        {
            glm::vec3 moveDir = glm::normalize(toTarget);
            float moveSpeed = 1.5f; // blocks per second
            
            // Calculate next horizontal position
            glm::vec3 nextPos = mob.position + moveDir * moveSpeed * deltaTime;
            
            // Check if movement is blocked by solid blocks at current height
            bool blockedAtCurrentHeight = m_chunkManager->IsSolid(nextPos.x, mob.position.y, nextPos.z) ||
                                         m_chunkManager->IsSolid(nextPos.x, mob.position.y + 1.0f, nextPos.z);
            
            if (!blockedAtCurrentHeight)
            {
                // Path is clear, move normally
                mob.position.x = nextPos.x;
                mob.position.z = nextPos.z;
                
                // Face movement direction (flip rotation 180 degrees)
                mob.rotationY = atan2(moveDir.x, moveDir.z) + 3.14159f;
            }
            else
            {
                // Check if we can step up one block
                bool canStepUp = !m_chunkManager->IsSolid(nextPos.x, mob.position.y + 1.0f, nextPos.z) &&
                                !m_chunkManager->IsSolid(nextPos.x, mob.position.y + 2.0f, nextPos.z) &&
                                m_chunkManager->IsSolid(nextPos.x, mob.position.y, nextPos.z);
                
                if (canStepUp)
                {
                    // Step up one block
                    mob.position.x = nextPos.x;
                    mob.position.z = nextPos.z;
                    mob.position.y += 1.0f;
                    
                    // Face movement direction
                    mob.rotationY = atan2(moveDir.x, moveDir.z) + 3.14159f;
                }
                else
                {
                    // Can't move forward or step up, pick new target
                    mob.wanderTimer = 0.0f;
                }
            }
        }
        
        // Despawn if too far from player
        float distToPlayer = glm::length(mob.position - m_camera->Position);
        if (distToPlayer > 600.0f)
        {
            mob.isActive = false;
        }
    }
    
    // Spawn new mobs near player if needed
    static float spawnTimer = 0.0f;
    spawnTimer += deltaTime;
    
    if (spawnTimer > 5.0f) // Check every 5 seconds
    {
        spawnTimer = 0.0f;
        
        // Count active mobs near player
        int nearbyMobs = 0;
        for (const auto& mob : m_mobs)
        {
            if (mob.isActive)
            {
                float dist = glm::length(mob.position - m_camera->Position);
                if (dist < 200.0f)
                    nearbyMobs++;
            }
        }
        
        // Try to spawn more if there are few nearby
        if (nearbyMobs < 10)
        {
            for (const auto& spawnLoc : m_mobSpawnLocations)
            {
                float distToPlayer = glm::length(spawnLoc - m_camera->Position);
                if (distToPlayer > 50.0f && distToPlayer < 200.0f)
                {
                    // Find ground height
                    float groundY = 50.0f;
                    for (int y = 100; y > 0; y--)
                    {
                        if (m_chunkManager->IsSolid(spawnLoc.x, (float)y, spawnLoc.z))
                        {
                            groundY = (float)y + 1.0f;
                            break;
                        }
                    }
                    
                    glm::vec3 mobPos(spawnLoc.x, groundY, spawnLoc.z);
                    
                    if (IsMobSpawnValid(mobPos))
                    {
                        // Check if a mob already exists at this location
                        bool locationOccupied = false;
                        for (const auto& existingMob : m_mobs)
                        {
                            if (existingMob.isActive && glm::length(existingMob.position - mobPos) < 5.0f)
                            {
                                locationOccupied = true;
                                break;
                            }
                        }
                        
                        if (!locationOccupied)
                        {
                            // Find an inactive mob slot or create new
                            bool spawned = false;
                            for (auto& mob : m_mobs)
                            {
                                if (!mob.isActive)
                                {
                                    mob.position = mobPos;
                                    mob.spawnLocation = mobPos;
                                    mob.velocity = glm::vec3(0.0f);
                                    mob.rotationY = (rand() % 360) * 3.14159f / 180.0f;
                                    mob.wanderTimer = 0.0f;
                                    mob.wanderTarget = mobPos;
                                    mob.isActive = true;
                                    spawned = true;
                                    break;
                                }
                            }
                            
                            if (spawned)
                                break; // Only spawn one per check
                        }
                    }
                }
            }
        }
    }
}

void Game::RenderMobs()
{
    for (const auto& mob : m_mobs)
    {
        if (!mob.isActive)
            continue;
        
        // Don't render if too far from player
        float distToPlayer = glm::length(mob.position - m_camera->Position);
        if (distToPlayer > 200.0f)
            continue;
        
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, mob.position + glm::vec3(0.0f, 0.4f, 0.0f)); // Raise model slightly
        model = glm::rotate(model, mob.rotationY, glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::scale(model, glm::vec3(0.35f)); // Ben model scale
        
        m_shader->setMat4("model", glm::value_ptr(model));
        
        // Render all meshes
        for (size_t i = 0; i < mob.meshes.size(); i++)
        {
            // Bind texture if available
            if (mob.meshes[i].textureID != 0)
            {
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, mob.meshes[i].textureID);
                m_shader->setInt("diffuseTexture", 1);
                m_shader->setBool("useTexture", true);
            }
            else
            {
                m_shader->setBool("useTexture", false);
            }
            
            glBindVertexArray(mob.meshes[i].VAO);
            glDrawElements(GL_TRIANGLES, mob.meshes[i].indices.size(), GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);
            
            m_shader->setBool("useTexture", false);
        }
    }
}
