#pragma once

#include <map>
#include <memory>
#include <glm/glm.hpp>
#include "Chunk.h"
#include "Block.h"
#include <cmath>

class TerrainGenerator
{
public:
    TerrainGenerator() {}

    // Simple hash function for chunk coordinates
    static long long GetChunkKey(int x, int z)
    {
        return ((long long)x << 32) | (unsigned int)z;
    }

    // Improved noise function
    static float Noise2D(float x, float z)
    {
        int n = (int)(x * 374761393.0f + z * 668265263.0f);
        n = (n << 13) ^ n;
        return (1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
    }

    // Smooth noise using interpolation
    static float SmoothNoise(float x, float z)
    {
        float corners = (Noise2D(x - 1, z - 1) + Noise2D(x + 1, z - 1) + Noise2D(x - 1, z + 1) + Noise2D(x + 1, z + 1)) / 16.0f;
        float sides = (Noise2D(x - 1, z) + Noise2D(x + 1, z) + Noise2D(x, z - 1) + Noise2D(x, z + 1)) / 8.0f;
        float center = Noise2D(x, z) / 4.0f;
        return corners + sides + center;
    }

    // Interpolated noise
    static float InterpolatedNoise(float x, float z)
    {
        int intX = (int)x;
        float fracX = x - intX;
        int intZ = (int)z;
        float fracZ = z - intZ;

        float v1 = SmoothNoise((float)intX, (float)intZ);
        float v2 = SmoothNoise((float)intX + 1, (float)intZ);
        float v3 = SmoothNoise((float)intX, (float)intZ + 1);
        float v4 = SmoothNoise((float)intX + 1, (float)intZ + 1);

        float i1 = Interpolate(v1, v2, fracX);
        float i2 = Interpolate(v3, v4, fracX);

        return Interpolate(i1, i2, fracZ);
    }

    // Cosine interpolation
    static float Interpolate(float a, float b, float x)
    {
        float ft = x * 3.1415927f;
        float f = (1.0f - cosf(ft)) * 0.5f;
        return a * (1.0f - f) + b * f;
    }

    // Perlin noise (octaves)
    static float PerlinNoise(float x, float z)
    {
        float total = 0.0f;
        float persistence = 0.5f;
        int octaves = 4;

        for (int i = 0; i < octaves; i++)
        {
            float frequency = powf(2.0f, (float)i);
            float amplitude = powf(persistence, (float)i);

            total += InterpolatedNoise(x * frequency * 0.01f, z * frequency * 0.01f) * amplitude;
        }

        return total;
    }

    // Determine biome based on position
    static BiomeType GetBiome(float worldX, float worldZ)
    {
        // Use simple hash-based noise for biome distribution
        // Scale down for large biomes (4+ chunks = 64+ blocks)
        int gridX = (int)(worldX / 80.0f); // 80 blocks per biome region
        int gridZ = (int)(worldZ / 80.0f);
        
        // Simple hash function
        int hash = gridX * 374761393 + gridZ * 668265263;
        hash = (hash ^ 61) ^ (hash >> 16);
        hash = hash + (hash << 3);
        hash = hash ^ (hash >> 4);
        hash = hash * 0x27d4eb2d;
        hash = hash ^ (hash >> 15);
        
        // Use the hash to determine biome (50/50 split)
        if ((hash & 1) == 0)
            return BiomeType::GRASSLAND;  // Green
        else
            return BiomeType::DESERT;     // Brown
    }

    // Get the noise value at a position (for debugging)
    static float GetBiomeNoise(float worldX, float worldZ)
    {
        int gridX = (int)(worldX / 80.0f);
        int gridZ = (int)(worldZ / 80.0f);
        
        int hash = gridX * 374761393 + gridZ * 668265263;
        hash = (hash ^ 61) ^ (hash >> 16);
        hash = hash + (hash << 3);
        hash = hash ^ (hash >> 4);
        hash = hash * 0x27d4eb2d;
        hash = hash ^ (hash >> 15);
        
        // Return normalized value for display (-1 to 1)
        return ((hash & 1) == 0) ? 0.5f : -0.5f;
    }

    // Find the closest different biome
    static void FindClosestOtherBiome(float worldX, float worldZ, float& outDistance, float& outAngle)
    {
        BiomeType currentBiome = GetBiome(worldX, worldZ);
        
        float closestDistance = 99999.0f;
        float closestAngle = 0.0f;
        
        // Search in a spiral pattern outward
        int maxSearchRadius = 200; // Search up to 200 blocks away
        int step = 5; // Check every 5 blocks
        
        for (int radius = step; radius < maxSearchRadius; radius += step)
        {
            // Check 8 directions around the current position
            for (int angle = 0; angle < 360; angle += 15)
            {
                float radians = angle * 3.14159f / 180.0f;
                float checkX = worldX + cos(radians) * radius;
                float checkZ = worldZ + sin(radians) * radius;
                
                BiomeType checkBiome = GetBiome(checkX, checkZ);
                
                if (checkBiome != currentBiome)
                {
                    float distance = sqrt((checkX - worldX) * (checkX - worldX) + (checkZ - worldZ) * (checkZ - worldZ));
                    if (distance < closestDistance)
                    {
                        closestDistance = distance;
                        closestAngle = angle;
                    }
                }
            }
            
            // If we found a different biome, stop searching
            if (closestDistance < 99999.0f)
                break;
        }
        
        outDistance = closestDistance;
        outAngle = closestAngle;
    }

    // Generate terrain for a chunk
    static void GenerateChunk(Chunk* chunk)
    {
        for (int x = 0; x < CHUNK_SIZE; x++)
        {
            for (int z = 0; z < CHUNK_SIZE; z++)
            {
                float worldX = chunk->position.x + x;
                float worldZ = chunk->position.z + z;

                // Get height using Perlin noise
                float height = PerlinNoise(worldX, worldZ);
                height = (height + 1.0f) * 0.5f;  // Normalize to 0-1
                int terrainHeight = (int)(height * 20.0f) + 5;  // Height between 5 and 25

                // Determine biome
                BiomeType biome = GetBiome(worldX, worldZ);
                BlockType surfaceBlockType = (biome == BiomeType::GRASSLAND) ? BlockType::GRASS : BlockType::DESERT_SAND;

                // Fill blocks with layering
                for (int y = 0; y < terrainHeight && y < CHUNK_HEIGHT; y++)
                {
                    BlockType blockType;
                    
                    if (y == terrainHeight - 1)
                    {
                        // Top layer - grass or sand
                        blockType = surfaceBlockType;
                    }
                    else if (y >= terrainHeight - 5)
                    {
                        // 4 blocks under surface - dirt (only for grass biome) or sand (for desert)
                        blockType = (biome == BiomeType::GRASSLAND) ? BlockType::DIRT : BlockType::DESERT_SAND;
                    }
                    else
                    {
                        // Everything deeper - stone
                        blockType = BlockType::STONE;
                    }
                    
                    chunk->SetBlock(x, y, z, blockType);
                }
            }
        }

        chunk->GenerateMesh();
    }
};
