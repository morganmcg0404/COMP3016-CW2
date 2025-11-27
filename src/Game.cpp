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

// For future integration:
// #include <irrKlang.h>
// #include <PxPhysicsAPI.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

Game::Game()
    : m_initialized(false), m_firstMouse(true), m_lastX(640.0f), m_lastY(360.0f), m_sprintKeyPressed(false),
      m_timeOfDay(0.25f), m_lightUpdateTimer(0.0f)  // Start at dawn (0.25 = 6am)
{
}

Game::~Game()
{
}

bool Game::Initialize()
{
    std::cout << "Initializing game systems..." << std::endl;

    // Initialize camera at a temporary position
    m_camera = std::make_unique<Camera>(glm::vec3(0.0f, 50.0f, 0.0f));
    m_camera->MovementSpeed = 15.0f;

    // Load shader
    try
    {
        m_shader = std::make_unique<Shader>("resources/shaders/basic.vert", "resources/shaders/basic.frag");
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

    // Initialize skybox and stars
    InitializeSkybox();
    InitializeStars();
    m_skyColor = glm::vec3(0.53f, 0.81f, 0.92f); // Start with day sky

    // Load shadow shader
    m_shadowShader = std::make_unique<Shader>("resources/shaders/shadow.vert", "resources/shaders/shadow.frag");
    std::cout << "Shadow shader loaded successfully" << std::endl;

    // Initialize shadow mapping
    InitializeShadowMap();

    m_initialized = true;
    std::cout << "Game systems initialized successfully" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  WASD - Move | Mouse - Look around | Space - Jump" << std::endl;
    std::cout << "  Shift - Walk slower | Ctrl - Toggle Sprint" << std::endl;
    
    return true;
}

void Game::ProcessInput(GLFWwindow* window, float deltaTime)
{
    // Check if crouching (shift slows movement)
    bool isCrouching = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS);

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

    // Horizontal movement (WASD) - check collision before applying
    glm::vec3 totalMovement(0.0f);
    
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        totalMovement += m_camera->GetMovementVector(FORWARD, deltaTime, isCrouching);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        totalMovement += m_camera->GetMovementVector(BACKWARD, deltaTime, isCrouching);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        totalMovement += m_camera->GetMovementVector(LEFT, deltaTime, isCrouching);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        totalMovement += m_camera->GetMovementVector(RIGHT, deltaTime, isCrouching);
    
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

    m_camera->ProcessMouseMovement(xoffset, yoffset);
}

void Game::Update(float deltaTime)
{
    // Update day/night cycle
    UpdateDayNightCycle(deltaTime);

    // Apply physics
    m_camera->ApplyPhysics(deltaTime);

    // Collision detection - check feet position
    glm::vec3 feetPos = m_camera->GetFeetPosition();
    
    // Check if player is on ground (check block directly below feet)
    m_camera->IsGrounded = m_chunkManager->IsSolid(feetPos.x, feetPos.y - 0.1f, feetPos.z);
    
    // If grounded and falling, stop downward velocity and snap to top of block
    if (m_camera->IsGrounded && m_camera->Velocity.y <= 0.0f)
    {
        // Find the exact block Y position below the player
        int blockY = (int)floor(feetPos.y - 0.1f);
        
        // Snap player feet to top of that block
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

    // Debug: Print current biome and closest other biome
    static float biomeCheckTimer = 0.0f;
    biomeCheckTimer += deltaTime;
    if (biomeCheckTimer >= 2.0f)
    {
        biomeCheckTimer = 0.0f;
        
        BiomeType currentBiome = TerrainGenerator::GetBiome(m_camera->Position.x, m_camera->Position.z);
        float noiseValue = TerrainGenerator::GetBiomeNoise(m_camera->Position.x, m_camera->Position.z);
        std::string biomeName = (currentBiome == BiomeType::GRASSLAND) ? "GRASSLAND (Green)" : "DESERT (Brown)";
        
        // Find closest other biome
        float distance = 0.0f;
        float angle = 0.0f;
        TerrainGenerator::FindClosestOtherBiome(m_camera->Position.x, m_camera->Position.z, distance, angle);
        
        std::string otherBiomeName = (currentBiome == BiomeType::GRASSLAND) ? "DESERT" : "GRASSLAND";
        std::string direction = "";
        
        // Convert angle to compass direction
        if (angle >= 337.5f || angle < 22.5f) direction = "East";
        else if (angle >= 22.5f && angle < 67.5f) direction = "Northeast";
        else if (angle >= 67.5f && angle < 112.5f) direction = "North";
        else if (angle >= 112.5f && angle < 157.5f) direction = "Northwest";
        else if (angle >= 157.5f && angle < 202.5f) direction = "West";
        else if (angle >= 202.5f && angle < 247.5f) direction = "Southwest";
        else if (angle >= 247.5f && angle < 292.5f) direction = "South";
        else direction = "Southeast";
        
        std::cout << "=== Biome Info ===" << std::endl;
        std::cout << "Position: (" << (int)m_camera->Position.x << ", " << (int)m_camera->Position.z << ")" << std::endl;
        std::cout << "Current Biome: " << biomeName << " | Noise: " << noiseValue << std::endl;
        std::cout << "Closest " << otherBiomeName << ": " << (int)distance << " blocks " << direction 
                  << " (" << (int)angle << "°)" << std::endl;
        std::cout << "Grounded: " << (m_camera->IsGrounded ? "Yes" : "No") 
                  << " | Sprinting: " << (m_camera->IsSprinting ? "Yes" : "No") << std::endl;
        std::cout << std::endl;
    }
}

void Game::Render()
{
    if (!m_initialized)
        return;

    // 1. Render shadow map from light's perspective
    RenderShadowMap();

    // 2. Render scene normally with shadows
    glViewport(0, 0, 1280, 720);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Use main shader
    m_shader->use();

    // Set up matrices
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 view = m_camera->GetViewMatrix();
    glm::mat4 projection = glm::perspective(glm::radians(m_camera->Zoom), 1280.0f / 720.0f, 0.1f, 1000.0f);

    m_shader->setMat4("model", glm::value_ptr(model));
    m_shader->setMat4("view", glm::value_ptr(view));
    m_shader->setMat4("projection", glm::value_ptr(projection));

    // Set lighting uniforms (dynamic based on time of day)
    glm::vec3 sunPos = GetSunPosition();
    m_shader->setVec3("lightPos", sunPos.x, sunPos.y, sunPos.z);
    m_shader->setVec3("viewPos", m_camera->Position.x, m_camera->Position.y, m_camera->Position.z);
    m_shader->setVec3("lightColor", m_lightColor.x, m_lightColor.y, m_lightColor.z);

    // Calculate light space matrix with same adjustments as shadow pass
    // Calculate sun angle and height for shadow adjustments
    float renderSunAngle = m_timeOfDay * 2.0f * 3.14159f;
    float renderSunHeight = sin(renderSunAngle);
    
    glm::vec3 adjustedSunPos = sunPos;
    if (abs(renderSunHeight) < 0.5f) {
        // When sun is low, keep it at a minimum angle to prevent long shadows
        adjustedSunPos.y = m_camera->Position.y + (renderSunHeight >= 0 ? 250.0f : -250.0f);
        // Maintain horizontal distance for realistic direction
        float horizontalDist = 500.0f * cos(renderSunAngle);
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

    // Reset model matrix and lighting for terrain rendering
    model = glm::mat4(1.0f);
    m_shader->setMat4("model", glm::value_ptr(model));
    m_shader->setMat4("view", glm::value_ptr(view));
    m_shader->setMat4("projection", glm::value_ptr(projection));
    m_shader->setVec3("lightColor", m_lightColor.x, m_lightColor.y, m_lightColor.z);

    // Render all chunks
    m_chunkManager->Render();

    // Render player model
    RenderPlayer();
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
    m_lightUpdateTimer += deltaTime;

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

        if (sunHeight > 0.0f)
        {
            // Day time - full brightness
            lightIntensity = 0.3f + (sunHeight * 0.7f);  // 0.3 to 1.0
            m_lightColor = glm::vec3(1.0f, 0.95f, 0.8f) * lightIntensity;  // Warm daylight
            
            // Sky color transitions from orange at sunrise/sunset to blue at noon
            float dayProgress = sunHeight; // 0 at horizon, 1 at zenith
            glm::vec3 dayColor = glm::vec3(0.53f, 0.81f, 0.92f); // Bright blue
            glm::vec3 sunsetColor = glm::vec3(1.0f, 0.5f, 0.2f); // Orange
            m_skyColor = glm::mix(sunsetColor, dayColor, dayProgress);
        }
        else
        {
            // Night time - dim moonlight
            lightIntensity = 0.15f + (abs(sunHeight) * 0.1f);  // 0.15 to 0.25
            m_lightColor = glm::vec3(0.6f, 0.7f, 1.0f) * lightIntensity;  // Cool moonlight
            
            // Dark blue at night
            m_skyColor = glm::vec3(0.05f, 0.05f, 0.15f);
        }

        // Debug output
        static int lastHour = -1;
        int hour = (int)(m_timeOfDay * 24.0f);
        if (hour != lastHour)
        {
            lastHour = hour;
            std::string timeOfDayStr;
            if (hour >= 6 && hour < 12) timeOfDayStr = "Morning";
            else if (hour >= 12 && hour < 18) timeOfDayStr = "Afternoon";
            else if (hour >= 18 && hour < 22) timeOfDayStr = "Evening";
            else timeOfDayStr = "Night";

            std::cout << "Time: " << hour << ":00 (" << timeOfDayStr << ") | Light: " 
                      << (int)(lightIntensity * 100) << "%" << std::endl;
        }
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

    // Calculate sun angle once for the entire function
    float sunAngle = m_timeOfDay * 2.0f * 3.14159f;
    float sunHeight = sin(sunAngle);

    // Render sun if it's above horizon
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
    
    // Render stars at night - reuse sunHeight calculated at the top
    if (sunHeight < 0.0f)  // Only show stars at night
    {
        float starAlpha = abs(sunHeight);  // Fade in/out stars
        
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, m_camera->Position);  // Stars centered on camera
        
        m_shader->setMat4("model", glm::value_ptr(model));
        m_shader->setVec3("lightColor", 1.0f, 1.0f, 1.0f);  // White light for stars
        
        glBindVertexArray(m_starsVAO);
        glDrawArrays(GL_POINTS, 0, m_starCount);
        glBindVertexArray(0);
    }
    
    // Disable blending for normal rendering
    glDisable(GL_BLEND);

    // Re-enable depth test
    glEnable(GL_DEPTH_TEST);
}

void Game::InitializeStars()
{
    // Create random star positions in a sphere around the camera
    std::vector<float> starVertices;
    
    for (int i = 0; i < m_starCount; i++)
    {
        // Random spherical coordinates
        float theta = ((float)rand() / RAND_MAX) * 2.0f * 3.14159f;
        float phi = ((float)rand() / RAND_MAX) * 3.14159f;
        float radius = 400.0f;  // Stars far away
        
        // Convert to Cartesian
        float x = radius * sin(phi) * cos(theta);
        float y = radius * sin(phi) * sin(theta);
        float z = radius * cos(phi);
        
        // Only place stars in upper hemisphere and away from sun/moon path
        if (y > 50.0f)  // Only above horizon
        {
            // Position
            starVertices.push_back(x);
            starVertices.push_back(y);
            starVertices.push_back(z);
            
            // Normal (not used but required)
            starVertices.push_back(0.0f);
            starVertices.push_back(1.0f);
            starVertices.push_back(0.0f);
            
            // Color (white with slight variation)
            float brightness = 0.8f + ((float)rand() / RAND_MAX) * 0.2f;
            starVertices.push_back(brightness);
            starVertices.push_back(brightness);
            starVertices.push_back(brightness);
        }
    }
    
    // Create VAO and VBO
    glGenVertexArrays(1, &m_starsVAO);
    glGenBuffers(1, &m_starsVBO);
    
    glBindVertexArray(m_starsVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_starsVBO);
    glBufferData(GL_ARRAY_BUFFER, starVertices.size() * sizeof(float), &starVertices[0], GL_STATIC_DRAW);
    
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
    
    // Enable point size in shader
    glEnable(GL_PROGRAM_POINT_SIZE);
}

glm::vec3 Game::GetSkyColor() const
{
    return m_skyColor;
}
