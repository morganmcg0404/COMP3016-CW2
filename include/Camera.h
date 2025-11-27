#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

enum Camera_Movement {
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT,
    UP,
    DOWN
};

// Default camera values
const float YAW = -90.0f;
const float PITCH = 0.0f;
const float SPEED = 10.0f;
const float SENSITIVITY = 0.1f;
const float ZOOM = 45.0f;

// Physics constants
const float GRAVITY = -20.0f;
const float JUMP_VELOCITY = 8.0f;
const float WALK_SPEED = 5.0f;
const float SPRINT_SPEED = 10.0f;
const float CROUCH_SPEED = 2.5f;
const float PLAYER_HEIGHT = 2.0f;  // 2 blocks tall
const float PLAYER_WIDTH = 0.667f; // 2/3 of a block wide
const float PLAYER_EYE_HEIGHT = 1.7f; // Eye position near top of 2-block tall player

class Camera
{
public:
    // Camera Attributes
    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;
    glm::vec3 WorldUp;

    // Euler Angles
    float Yaw;
    float Pitch;

    // Camera options
    float MovementSpeed;
    float MouseSensitivity;
    float Zoom;

    // Physics properties
    glm::vec3 Velocity;
    bool IsGrounded;
    bool IsSprinting;
    float BaseSpeed;

    // Constructor with vectors
    Camera(glm::vec3 position = glm::vec3(0.0f, 10.0f, 0.0f), glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f), float yaw = YAW, float pitch = PITCH)
        : Front(glm::vec3(0.0f, 0.0f, -1.0f)), MovementSpeed(SPEED), MouseSensitivity(SENSITIVITY), Zoom(ZOOM),
          Velocity(glm::vec3(0.0f)), IsGrounded(false), IsSprinting(false), BaseSpeed(WALK_SPEED)
    {
        Position = position;
        WorldUp = up;
        Yaw = yaw;
        Pitch = pitch;
        updateCameraVectors();
    }

    // Returns the view matrix calculated using Euler Angles and the LookAt Matrix
    glm::mat4 GetViewMatrix()
    {
        return glm::lookAt(Position, Position + Front, Up);
    }

    // Calculate movement vector based on input (doesn't modify position)
    glm::vec3 GetMovementVector(Camera_Movement direction, float deltaTime, bool isCrouching)
    {
        // Determine speed based on state
        float speed = BaseSpeed;
        if (IsSprinting && !isCrouching)
            speed = SPRINT_SPEED;
        else if (isCrouching)
            speed = CROUCH_SPEED;
        else
            speed = WALK_SPEED;

        float velocity = speed * deltaTime;
        
        // Create horizontal movement direction (ignore vertical component of Front)
        glm::vec3 horizontalFront = glm::normalize(glm::vec3(Front.x, 0.0f, Front.z));
        
        glm::vec3 movement(0.0f);
        if (direction == FORWARD)
            movement = horizontalFront * velocity;
        if (direction == BACKWARD)
            movement = -horizontalFront * velocity;
        if (direction == LEFT)
            movement = -Right * velocity;
        if (direction == RIGHT)
            movement = Right * velocity;
        
        return movement;
    }

    // Apply movement to position
    void ApplyMovement(const glm::vec3& movement)
    {
        Position += movement;
    }

    // Jump if grounded
    void Jump()
    {
        if (IsGrounded)
        {
            Velocity.y = JUMP_VELOCITY;
            IsGrounded = false;
        }
    }

    // Toggle sprint
    void ToggleSprint()
    {
        IsSprinting = !IsSprinting;
    }

    // Apply physics (gravity and velocity)
    void ApplyPhysics(float deltaTime)
    {
        // Apply gravity
        if (!IsGrounded)
        {
            Velocity.y += GRAVITY * deltaTime;
        }
        
        // Apply velocity to position
        Position += Velocity * deltaTime;
    }

    // Get feet position (for collision detection)
    glm::vec3 GetFeetPosition() const
    {
        return Position - glm::vec3(0.0f, PLAYER_EYE_HEIGHT, 0.0f);
    }

    // Processes input received from a mouse input system
    void ProcessMouseMovement(float xoffset, float yoffset, GLboolean constrainPitch = true)
    {
        xoffset *= MouseSensitivity;
        yoffset *= MouseSensitivity;

        Yaw += xoffset;
        Pitch += yoffset;

        // Make sure that when pitch is out of bounds, screen doesn't get flipped
        if (constrainPitch)
        {
            if (Pitch > 89.0f)
                Pitch = 89.0f;
            if (Pitch < -89.0f)
                Pitch = -89.0f;
        }

        // Update Front, Right and Up Vectors using the updated Euler angles
        updateCameraVectors();
    }

    // Processes input received from a mouse scroll-wheel event
    void ProcessMouseScroll(float yoffset)
    {
        Zoom -= (float)yoffset;
        if (Zoom < 1.0f)
            Zoom = 1.0f;
        if (Zoom > 45.0f)
            Zoom = 45.0f;
    }

private:
    // Calculates the front vector from the Camera's (updated) Euler Angles
    void updateCameraVectors()
    {
        // Calculate the new Front vector
        glm::vec3 front;
        front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        front.y = sin(glm::radians(Pitch));
        front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        Front = glm::normalize(front);

        // Also re-calculate the Right and Up vector
        Right = glm::normalize(glm::cross(Front, WorldUp));
        Up = glm::normalize(glm::cross(Right, Front));
    }
};
