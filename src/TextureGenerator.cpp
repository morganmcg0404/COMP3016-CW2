#include "../include/TextureGenerator.h"
#include <glm/glm.hpp>
#include <cmath>

// Simple hash-based pseudorandom number generator for 2D
float TextureGenerator::Hash(float x, float y)
{
    float n = glm::sin(glm::dot(glm::vec2(x, y), glm::vec2(12.9898f, 78.233f))) * 43758.5453f;
    return n - glm::floor(n);
}

// Linear interpolation
float TextureGenerator::Lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

glm::vec3 TextureGenerator::GetGridCellColor(glm::vec3 baseColor, int gridX, int gridY)
{
    // Generate a unique hash for each grid cell
    float noise = Hash(static_cast<float>(gridX) * 73.0f, static_cast<float>(gridY) * 131.0f);
    
    // Apply noise as a brightness variation with more aggressive graininess
    float variation = 0.6f + (noise * 0.8f); // 60% to 140% of original brightness
    
    glm::vec3 result = baseColor * variation;
    
    // Clamp values to 0-1 range
    result.r = glm::clamp(result.r, 0.0f, 1.0f);
    result.g = glm::clamp(result.g, 0.0f, 1.0f);
    result.b = glm::clamp(result.b, 0.0f, 1.0f);
    
    return result;
}

glm::vec3 TextureGenerator::GetVertexColor(glm::vec3 baseColor, glm::vec3 vertexLocalPos)
{
    // Map vertex position (0-1 on each axis) to a much finer grain grid (e.g., 16x16x16)
    // This creates a grainy texture by directly hashing the grid position without interpolation
    int gridX = static_cast<int>(glm::floor(vertexLocalPos.x * 32.0f)); // Higher resolution for graininess
    int gridY = static_cast<int>(glm::floor(vertexLocalPos.y * 32.0f));
    int gridZ = static_cast<int>(glm::floor(vertexLocalPos.z * 32.0f));
    
    // Use all three coordinates for variation in all directions
    return GetGridCellColor(baseColor, gridX * 73 + gridY * 131 + gridZ * 17, gridX ^ gridY ^ gridZ);
}
