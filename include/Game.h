#pragma once

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <memory>

// Forward declarations
class Camera;
class Shader;
class ChunkManager;

class Game
{
public:
    Game();
    ~Game();

    // Initialize game systems (Physics, Audio, Graphics)
    bool Initialize();

    // Process user input
    void ProcessInput(GLFWwindow* window, float deltaTime);

    // Process mouse movement
    void ProcessMouseMovement(float xoffset, float yoffset);
    
    // Process mouse button
    void ProcessMouseButton(int button, int action);

    // Update game logic and physics
    void Update(float deltaTime);

    // Render the scene
    void Render();

    // Cleanup resources
    void Shutdown();

private:
    bool m_initialized;
    bool m_firstMouse;
    float m_lastX;
    float m_lastY;
    bool m_sprintKeyPressed;
    bool m_isPlayerMoving;
    bool m_isCrouching;
    float m_crouchOffset;

    // Game systems
    std::unique_ptr<Camera> m_camera;
    std::unique_ptr<Shader> m_shader;
    std::unique_ptr<Shader> m_shadowShader;
    std::unique_ptr<ChunkManager> m_chunkManager;

    // Shadow mapping
    unsigned int m_shadowMapFBO;
    unsigned int m_shadowMap;
    const unsigned int SHADOW_WIDTH = 8192;
    const unsigned int SHADOW_HEIGHT = 8192;
    void InitializeShadowMap();
    void RenderShadowMap();

    // Player rendering
    unsigned int m_playerVAO, m_playerVBO;
    void InitializePlayerModel();
    void RenderPlayer();
    
    // Hand rendering
    unsigned int m_handVAO, m_handVBO;
    float m_handBobTimer;
    float m_handSwingTimer;
    bool m_isSwinging;
    void InitializeHand();
    void RenderHand();
    void UpdateHandAnimation(float deltaTime, bool isMoving);

    // Collision helpers
    bool WouldCollide(const glm::vec3& newPosition);

    // Day/Night cycle
    float m_timeOfDay;  // 0.0 to 1.0 (0 = midnight, 0.5 = noon)
    float m_lightUpdateTimer;
    glm::vec3 m_lightDirection;
    glm::vec3 m_lightColor;
    glm::vec3 m_skyColor;
    void UpdateDayNightCycle(float deltaTime);
    glm::vec3 GetSunPosition();
    glm::vec3 GetMoonPosition();
    glm::vec3 GetSkyColor() const { return m_skyColor; }

    // Sky rendering
    unsigned int m_skyVAO, m_skyVBO;
    void InitializeSkybox();
    void RenderSky();
    
    // Star rendering
    unsigned int m_starsVAO, m_starsVBO;
    int m_starCount;
    void InitializeStars();
    void RenderStars();
};
