#pragma once

#include <GLFW/glfw3.h>
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

    // Game systems
    std::unique_ptr<Camera> m_camera;
    std::unique_ptr<Shader> m_shader;
    std::unique_ptr<ChunkManager> m_chunkManager;
};
