#pragma once

#include <glm/glm.hpp>

// Biome types
enum class BiomeType
{
    GRASSLAND,  // Green biome
    DESERT,     // Brown biome
    BIRCH       // Birch forest biome
};

// Block types
enum class BlockType
{
    AIR,
    GRASS,
    DESERT_SAND,
    DIRT,
    STONE,
    WOOD,
    LEAVES,
    BEDROCK,
    BIRCH_WOOD,
    BIRCH_GRASS
};

// Block structure
struct Block
{
    BlockType type;
    bool isActive;

    Block() : type(BlockType::AIR), isActive(false) {}
    Block(BlockType t) : type(t), isActive(true) {}

    // Get block color based on type
    glm::vec3 GetColor() const
    {
        switch (type)
        {
        case BlockType::GRASS:
            return glm::vec3(0.15f, 0.6f, 0.15f);  // Darker green
        case BlockType::BIRCH_GRASS:
            return glm::vec3(0.5f, 0.85f, 0.5f);  // Lighter green
        case BlockType::DESERT_SAND:
            return glm::vec3(0.76f, 0.60f, 0.42f);  // Sandy brown
        case BlockType::DIRT:
            return glm::vec3(0.4f, 0.26f, 0.13f);  // Dark brown
        case BlockType::STONE:
            return glm::vec3(0.5f, 0.5f, 0.5f);  // Grey
        case BlockType::WOOD:
            return glm::vec3(0.4f, 0.2f, 0.1f);  // Dark brown
        case BlockType::BIRCH_WOOD:
            return glm::vec3(0.9f, 0.9f, 0.85f);  // White/cream (birch)
        case BlockType::LEAVES:
            return glm::vec3(0.1f, 0.8f, 0.1f);  // Bright green
        case BlockType::BEDROCK:
            return glm::vec3(0.1f, 0.1f, 0.1f);  // Very dark grey (almost black)
        case BlockType::AIR:
        default:
            return glm::vec3(1.0f, 1.0f, 1.0f);
        }
    }
};
