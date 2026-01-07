#pragma once

#include <glm/glm.hpp>
#include <vector>

class TextureGenerator
{
public:
    // Generate noise-based color variation for a specific grid cell within a block (16x16 grid)
    static glm::vec3 GetGridCellColor(glm::vec3 baseColor, int gridX, int gridY);

    // Get the color for a vertex based on its position within a block's 16x16 grid
    static glm::vec3 GetVertexColor(glm::vec3 baseColor, glm::vec3 vertexLocalPos);

private:
    // Hash function for pseudorandom number generation
    static float Hash(float x, float y);
    
    // Linear interpolation
    static float Lerp(float a, float b, float t);
};
