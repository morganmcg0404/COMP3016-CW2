#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include "Game.h"

// Window dimensions
const unsigned int SCREEN_WIDTH = 1280;
const unsigned int SCREEN_HEIGHT = 720;

// Global game pointer for callbacks
Game* g_game = nullptr;

// Mouse state
bool firstMouse = true;
float lastX = SCREEN_WIDTH / 2.0f;
float lastY = SCREEN_HEIGHT / 2.0f;

// Callback function for window resize
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

// Mouse callback
void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    // Don't process camera movement if GUI is open
    if (g_game && g_game->IsGUIOpen())
    {
        lastX = (float)xpos;
        lastY = (float)ypos;
        return;
    }
    
    if (firstMouse)
    {
        lastX = (float)xpos;
        lastY = (float)ypos;
        firstMouse = false;
    }

    float xoffset = (float)xpos - lastX;
    float yoffset = lastY - (float)ypos; // Reversed since y-coordinates go from bottom to top

    lastX = (float)xpos;
    lastY = (float)ypos;

    if (g_game)
        g_game->ProcessMouseMovement(xoffset, yoffset);
}

// Mouse button callback
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    if (g_game)
        g_game->ProcessMouseButton(button, action);
}

// Mouse scroll callback
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    if (g_game)
        g_game->ProcessMouseScroll(yoffset);
}

// Process input
void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

int main()
{
    // Initialize GLFW
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // Configure GLFW
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Create window
    GLFWwindow* window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "COMP3016 CW2 - 3D Game", NULL, NULL);
    if (window == NULL)
    {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // Capture mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // Initialize GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // Configure OpenGL
    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    glEnable(GL_DEPTH_TEST);

    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;

    // Initialize Game
    Game game;
    g_game = &game;
    if (!game.Initialize())
    {
        std::cerr << "Failed to initialize game" << std::endl;
        glfwTerminate();
        return -1;
    }

    // Game loop
    float deltaTime = 0.0f;
    float lastFrame = 0.0f;
    float titleUpdateTimer = 0.0f;
    int frameCount = 0;
    float fpsTimer = 0.0f;
    int fps = 0;

    while (!glfwWindowShouldClose(window))
    {
        // Calculate delta time
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        
        // Calculate FPS
        frameCount++;
        fpsTimer += deltaTime;
        if (fpsTimer >= 1.0f)
        {
            fps = frameCount;
            frameCount = 0;
            fpsTimer = 0.0f;
        }
        
        // Update window title with coordinates and FPS every 0.1 seconds
        titleUpdateTimer += deltaTime;
        if (titleUpdateTimer >= 0.1f)
        {
            titleUpdateTimer = 0.0f;
            glm::vec3 pos = game.GetCameraPosition();
            char title[128];
            snprintf(title, sizeof(title), "Voxel Engine | FPS: %d | X: %.1f Y: %.0f Z: %.1f", fps, pos.x, ceil(pos.y), pos.z);
            glfwSetWindowTitle(window, title);
        }

        // Process input
        processInput(window);
        game.ProcessInput(window, deltaTime);

        // Update game state
        game.Update(deltaTime);

        // Render
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        game.Render(window);

        // Swap buffers and poll events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    game.Shutdown();
    g_game = nullptr;
    glfwTerminate();

    return 0;
}
