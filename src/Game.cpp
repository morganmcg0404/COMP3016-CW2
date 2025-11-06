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

// For future integration:
// #include <irrKlang.h>
// #include <PxPhysicsAPI.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

Game::Game()
    : m_initialized(false), m_firstMouse(true), m_lastX(640.0f), m_lastY(360.0f)
{
}

Game::~Game()
{
}

bool Game::Initialize()
{
    std::cout << "Initializing game systems..." << std::endl;

    // Initialize camera
    m_camera = std::make_unique<Camera>(glm::vec3(0.0f, 20.0f, 0.0f));
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

    // Initialize chunk manager with 20 block render distance
    m_chunkManager = std::make_unique<ChunkManager>(20);

    // Generate initial chunks around the camera
    m_chunkManager->Update(*m_camera);

    m_initialized = true;
    std::cout << "Game systems initialized successfully" << std::endl;
    std::cout << "Controls: WASD - Move, Space/Shift - Up/Down, Mouse - Look around" << std::endl;
    
    return true;
}

void Game::ProcessInput(GLFWwindow* window, float deltaTime)
{
    // Camera movement
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        m_camera->ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        m_camera->ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        m_camera->ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        m_camera->ProcessKeyboard(RIGHT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        m_camera->ProcessKeyboard(UP, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        m_camera->ProcessKeyboard(DOWN, deltaTime);
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
        std::cout << std::endl;
    }
}

void Game::Render()
{
    if (!m_initialized)
        return;

    // Use shader
    m_shader->use();

    // Set up matrices
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 view = m_camera->GetViewMatrix();
    glm::mat4 projection = glm::perspective(glm::radians(m_camera->Zoom), 1280.0f / 720.0f, 0.1f, 1000.0f);

    m_shader->setMat4("model", glm::value_ptr(model));
    m_shader->setMat4("view", glm::value_ptr(view));
    m_shader->setMat4("projection", glm::value_ptr(projection));

    // Set lighting uniforms
    glm::vec3 lightPos(100.0f, 100.0f, 100.0f);
    m_shader->setVec3("lightPos", lightPos.x, lightPos.y, lightPos.z);
    m_shader->setVec3("viewPos", m_camera->Position.x, m_camera->Position.y, m_camera->Position.z);
    m_shader->setVec3("lightColor", 1.0f, 1.0f, 1.0f);

    // Render all chunks
    m_chunkManager->Render();
}

void Game::Shutdown()
{
    if (!m_initialized)
        return;

    std::cout << "Shutting down game systems..." << std::endl;

    // Cleanup will happen automatically with unique_ptr destructors
    m_chunkManager.reset();
    m_shader.reset();
    m_camera.reset();

    m_initialized = false;
}
