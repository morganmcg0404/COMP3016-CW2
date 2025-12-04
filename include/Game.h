#pragma once

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <memory>
#include "Block.h"

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
    
    // Process mouse scroll
    void ProcessMouseScroll(double yoffset);

    // Update game logic and physics
    void Update(float deltaTime);

    // Render the scene
    void Render(GLFWwindow* window);

    // Cleanup resources
    void Shutdown();
    
    // Get camera position for debug display
    glm::vec3 GetCameraPosition() const;
    
    // Get GUI state
    bool IsGUIOpen() const { return m_showGUI; }

private:
    bool m_initialized;
    bool m_firstMouse;
    float m_lastX;
    float m_lastY;
    bool m_sprintKeyPressed;
    bool m_isPlayerMoving;
    bool m_isCrouching;
    float m_crouchOffset;
    
    // Debug display
    float m_debugUpdateTimer;

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

    // Crosshair rendering
    unsigned int m_crosshairVAO, m_crosshairVBO;
    void InitializeCrosshair();
    void RenderCrosshair();
    
    // Hotbar rendering
    unsigned int m_hotbarVAO, m_hotbarVBO;
    int m_selectedSlot;  // 0-8 (0 is empty hand)
    BlockType m_hotbarItems[9];  // Blocks in hotbar slots
    void InitializeHotbar();
    void RenderHotbar(int windowWidth, int windowHeight);
    
    // Block outline rendering
    unsigned int m_outlineVAO, m_outlineVBO;
    void InitializeBlockOutline();
    void RenderBlockOutline();
    
    // Pet system
    unsigned int m_petVAO, m_petVBO;
    glm::vec3 m_petPosition;
    glm::vec3 m_petVelocity;
    bool m_petSitting;
    glm::vec3 m_petSitPosition;
    float m_petScale;
    void InitializePet();
    void UpdatePet(float deltaTime);
    void RenderPet();
    void TogglePetSit();
    
    // GUI System
    bool m_showGUI;
    unsigned int m_guiVAO, m_guiVBO;
    float m_renderDistance;
    float m_mouseSensitivity;
    float m_fov;
    float m_timeSpeed;
    GLFWwindow* m_window;  // Store window pointer for cursor management
    void InitializeGUI();
    void RenderGUI(int windowWidth, int windowHeight);
    void ToggleGUI(GLFWwindow* window);
    bool IsMouseOverGUI(double mouseX, double mouseY, int windowWidth, int windowHeight);
    void HandleGUIInteraction(GLFWwindow* window, int windowWidth, int windowHeight);

    // Collision helpers
    bool WouldCollide(const glm::vec3& newPosition);
    
    // Raycasting for block interaction
    bool RaycastBlock(glm::vec3& hitPos, float maxDistance);
    bool RaycastBlockWithNormal(glm::vec3& hitPos, glm::vec3& normal, float maxDistance);

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
