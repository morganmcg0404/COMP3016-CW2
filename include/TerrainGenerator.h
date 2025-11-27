#pragma once

#include <map>
#include <memory>
#include <glm/glm.hpp>
#include "Chunk.h"
#include "Block.h"
#include <cmath>
#include <ctime>
#include <iostream>

class TerrainGenerator
{
public:
    TerrainGenerator() {}

    // World seed for unique generation
    static int worldSeed;

    // Initialize world seed (call once at game start)
    static void InitializeSeed(int seed = 0)
    {
        if (seed == 0)
        {
            // Generate random seed from current time
            worldSeed = static_cast<int>(time(nullptr));
        }
        else
        {
            worldSeed = seed;
        }
        std::cout << "World seed: " << worldSeed << std::endl;
    }

    // Simple hash function for chunk coordinates
    static long long GetChunkKey(int x, int z)
    {
        return ((long long)x << 32) | (unsigned int)z;
    }

    // Improved noise function with world seed
    static float Noise2D(float x, float z)
    {
        int n = (int)(x * 374761393.0f + z * 668265263.0f) + worldSeed;
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

    // Perlin noise (octaves) - smoother terrain with base elevation
    static float PerlinNoise(float x, float z)
    {
        float total = 0.0f;
        float persistence = 0.4f;  // Reduced from 0.5 for smoother terrain
        int octaves = 3;  // Reduced from 4 for less detail/roughness

        for (int i = 0; i < octaves; i++)
        {
            float frequency = powf(2.0f, (float)i);
            float amplitude = powf(persistence, (float)i);

            // Reduced frequency multiplier for gentler slopes
            total += InterpolatedNoise(x * frequency * 0.008f, z * frequency * 0.008f) * amplitude;
        }

        return total;
    }

    // Small hills and valleys noise - higher frequency for local variation
    static float DetailNoise(float x, float z)
    {
        // High frequency noise for small hills and valleys
        float detail = InterpolatedNoise(x * 0.05f, z * 0.05f);
        
        // Add another layer for even smaller details
        detail += InterpolatedNoise(x * 0.1f, z * 0.1f) * 0.5f;
        
        return detail * 0.3f; // Scale down the effect
    }

    // Get biome region based on chunk coordinates (each biome is minimum 8 chunks)
    static BiomeType GetBiomeForRegion(int chunkX, int chunkZ)
    {
        // Divide world into 8x8 chunk regions
        int regionX = chunkX / 8;
        int regionZ = chunkZ / 8;
        
        // Use hash-based random determination for this region
        int hash = regionX * 374761393 + regionZ * 668265263;
        hash = (hash ^ 61) ^ (hash >> 16);
        hash = hash + (hash << 3);
        hash = hash ^ (hash >> 4);
        hash = hash * 0x27d4eb2d;
        hash = hash ^ (hash >> 15);
        
        // Convert to 0-1 range
        float value = (float)(hash & 0x7FFFFFFF) / (float)0x7FFFFFFF;
        
        // Threshold: favors grassland over desert (70% grassland, 30% desert)
        if (value > 0.3f)
            return BiomeType::GRASSLAND;
        else
            return BiomeType::DESERT;
    }

    // Determine biome based on world position with boundary detection
    static BiomeType GetBiome(float worldX, float worldZ)
    {
        // Convert world position to chunk coordinates
        int chunkX = (int)floor(worldX / CHUNK_SIZE);
        int chunkZ = (int)floor(worldZ / CHUNK_SIZE);
        
        return GetBiomeForRegion(chunkX, chunkZ);
    }
    
    // Get distance to nearest biome boundary and the neighboring biome type
    static float GetDistanceToBiomeBoundary(float worldX, float worldZ, BiomeType& neighborBiome)
    {
        int chunkX = (int)floor(worldX / CHUNK_SIZE);
        int chunkZ = (int)floor(worldZ / CHUNK_SIZE);
        
        BiomeType currentBiome = GetBiomeForRegion(chunkX, chunkZ);
        neighborBiome = currentBiome; // Default to same biome
        
        // Get position within chunk (0-15)
        float localX = worldX - (chunkX * CHUNK_SIZE);
        float localZ = worldZ - (chunkZ * CHUNK_SIZE);
        
        // Check neighboring chunks for different biomes
        float minDist = 999.0f;
        
        // Check all 4 adjacent chunks
        BiomeType leftBiome = GetBiomeForRegion(chunkX - 1, chunkZ);
        if (leftBiome != currentBiome && localX < minDist)
        {
            minDist = localX;
            neighborBiome = leftBiome;
        }
        
        BiomeType rightBiome = GetBiomeForRegion(chunkX + 1, chunkZ);
        if (rightBiome != currentBiome && (CHUNK_SIZE - localX) < minDist)
        {
            minDist = CHUNK_SIZE - localX;
            neighborBiome = rightBiome;
        }
        
        BiomeType frontBiome = GetBiomeForRegion(chunkX, chunkZ - 1);
        if (frontBiome != currentBiome && localZ < minDist)
        {
            minDist = localZ;
            neighborBiome = frontBiome;
        }
        
        BiomeType backBiome = GetBiomeForRegion(chunkX, chunkZ + 1);
        if (backBiome != currentBiome && (CHUNK_SIZE - localZ) < minDist)
        {
            minDist = CHUNK_SIZE - localZ;
            neighborBiome = backBiome;
        }
        
        return minDist;
    }
    
    // Get blended block type - only blends at biome boundaries with smooth gradient
    static BlockType GetBlendedSurfaceBlock(float worldX, float worldZ)
    {
        BiomeType currentBiome = GetBiome(worldX, worldZ);
        BiomeType neighborBiome;
        float distToBoundary = GetDistanceToBiomeBoundary(worldX, worldZ, neighborBiome);
        
        // Define blend distance (8 blocks on each side of boundary = 16 block transition zone)
        const float BLEND_DISTANCE = 8.0f;
        
        // Pure biome if more than BLEND_DISTANCE blocks from boundary
        if (distToBoundary > BLEND_DISTANCE)
        {
            return (currentBiome == BiomeType::GRASSLAND) ? BlockType::GRASS : BlockType::DESERT_SAND;
        }
        
        // Calculate blend factor: 0 at boundary, 1 at BLEND_DISTANCE
        float blendFactor = distToBoundary / BLEND_DISTANCE;
        
        // Use multiple octaves of noise for natural variation
        float noise1 = InterpolatedNoise(worldX * 0.1f, worldZ * 0.1f);  // Large patterns
        float noise2 = InterpolatedNoise(worldX * 0.3f, worldZ * 0.3f);  // Medium patterns
        float noise3 = InterpolatedNoise(worldX * 0.8f, worldZ * 0.8f);  // Small details
        
        // Combine noise layers
        float combinedNoise = noise1 * 0.5f + noise2 * 0.3f + noise3 * 0.2f;
        
        // Adjust threshold based on distance to boundary
        // Near boundary (blendFactor = 0): roughly 50/50 mix
        // Far from boundary (blendFactor = 1): pure current biome
        float threshold = combinedNoise * (1.0f - blendFactor);
        
        // Determine which biome's block to use
        if (currentBiome == BiomeType::GRASSLAND)
        {
            // In grassland, gradually introduce more grass as we move away from desert
            if (threshold < -blendFactor)
                return BlockType::DESERT_SAND;
            else
                return BlockType::GRASS;
        }
        else
        {
            // In desert, gradually introduce more sand as we move away from grassland
            if (threshold > blendFactor)
                return BlockType::GRASS;
            else
                return BlockType::DESERT_SAND;
        }
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
        int maxSearchRadius = 500; // Search up to 500 blocks away
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

    // Get tree placement hash
    static int GetTreeHash(int x, int z)
    {
        // Make coordinates positive for better distribution
        x += 10000;
        z += 10000;
        
        int hash = x * 374761393 + z * 668265263;
        hash = (hash ^ 61) ^ (hash >> 16);
        hash = hash + (hash << 3);
        hash = hash ^ (hash >> 4);
        hash = hash * 0x27d4eb2d;
        hash = hash ^ (hash >> 15);
        return hash & 0x7FFFFFFF; // Make positive
    }

    // Check if a tree should spawn at this position
    static bool ShouldSpawnTree(float worldX, float worldZ)
    {
        int ix = (int)floor(worldX);
        int iz = (int)floor(worldZ);
        
        // Only check positions on a grid (every 8 blocks)
        if (ix % 8 != 0 || iz % 8 != 0)
            return false;
        
        // Use hash for random chance (10% on valid grid positions)
        int hash = GetTreeHash(ix / 8, iz / 8);
        return (hash % 10) < 1;
    }

    // Check if position is valid for tree (not used anymore - grid handles spacing)
    static bool IsValidTreePosition(float worldX, float worldZ)
    {
        return true;
    }

    // Place a tree at the given position
    static void PlaceTree(Chunk* chunk, int localX, int groundY, int localZ)
    {
        // Tree trunk - 7 blocks tall
        for (int y = 0; y < 7; y++)
        {
            int treeY = groundY + 1 + y;
            if (treeY < CHUNK_HEIGHT)
            {
                chunk->SetBlock(localX, treeY, localZ, BlockType::WOOD);
            }
        }

        // Leaves - start on 4th block from ground (index 3)
        int leafStartY = groundY + 4;
        
        // Leaf shape: 5x5x4 full cube (2 blocks in every direction)
        for (int dy = 0; dy < 4; dy++)
        {
            int leafY = leafStartY + dy;
            if (leafY >= CHUNK_HEIGHT) break;
            
            for (int dx = -2; dx <= 2; dx++)
            {
                for (int dz = -2; dz <= 2; dz++)
                {
                    int leafX = localX + dx;
                    int leafZ = localZ + dz;
                    
                    // Skip if out of chunk bounds
                    if (leafX < 0 || leafX >= CHUNK_SIZE || leafZ < 0 || leafZ >= CHUNK_SIZE)
                        continue;
                    
                    // Don't replace trunk with leaves
                    if (dx == 0 && dz == 0)
                        continue;
                    
                    chunk->SetBlock(leafX, leafY, leafZ, BlockType::LEAVES);
                }
            }
        }

        // Top leaves - 2 blocks above the tree trunk (extending beyond trunk)
        int topTrunkY = groundY + 7; // Top of 7-block trunk
        for (int dy = 1; dy <= 2; dy++) // 1 and 2 blocks above trunk
        {
            int leafY = topTrunkY + dy;
            if (leafY >= CHUNK_HEIGHT) break;
            
            for (int dx = -2; dx <= 2; dx++)
            {
                for (int dz = -2; dz <= 2; dz++)
                {
                    int leafX = localX + dx;
                    int leafZ = localZ + dz;
                    
                    // Skip if out of chunk bounds
                    if (leafX < 0 || leafX >= CHUNK_SIZE || leafZ < 0 || leafZ >= CHUNK_SIZE)
                        continue;
                    
                    chunk->SetBlock(leafX, leafY, leafZ, BlockType::LEAVES);
                }
            }
        }
    }

    // Generate terrain for a chunk
    static void GenerateChunk(Chunk* chunk)
    {
        // First pass: Generate terrain
        for (int x = 0; x < CHUNK_SIZE; x++)
        {
            for (int z = 0; z < CHUNK_SIZE; z++)
            {
                float worldX = chunk->position.x + x;
                float worldZ = chunk->position.z + z;

                // Get base height using Perlin noise
                float height = PerlinNoise(worldX, worldZ);
                height = (height + 1.0f) * 0.5f;  // Normalize to 0-1
                
                // Add small hills and valleys detail
                float detail = DetailNoise(worldX, worldZ);
                
                // Combine base terrain with detail
                // Base terrain: 10-22 blocks (12 block range)
                // Detail adds: -3 to +3 blocks variation for hills/valleys
                int baseHeight = (int)(height * 12.0f) + 10;
                int detailHeight = (int)(detail * 10.0f) - 3;
                int terrainHeight = baseHeight + detailHeight;
                
                // Clamp to reasonable range
                if (terrainHeight < 8) terrainHeight = 8;
                if (terrainHeight > 28) terrainHeight = 28;

                // Get blended surface block type
                BlockType surfaceBlockType = GetBlendedSurfaceBlock(worldX, worldZ);
                
                // Determine biome for subsurface blocks
                BiomeType biome = GetBiome(worldX, worldZ);

                // Fill blocks with layering
                for (int y = 0; y < terrainHeight && y < CHUNK_HEIGHT; y++)
                {
                    BlockType blockType;
                    
                    if (y == terrainHeight - 1)
                    {
                        // Top layer - blended grass/sand based on biome transition
                        blockType = surfaceBlockType;
                    }
                    else if (y >= terrainHeight - 5)
                    {
                        // 4 blocks under surface - use dirt for grass, sand for desert
                        // In transition zones, match the surface block
                        if (surfaceBlockType == BlockType::GRASS)
                            blockType = BlockType::DIRT;
                        else
                            blockType = BlockType::DESERT_SAND;
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

        // Second pass: Generate trees
        int treesPlaced = 0;
        for (int x = 0; x < CHUNK_SIZE; x++)
        {
            for (int z = 0; z < CHUNK_SIZE; z++)
            {
                float worldX = chunk->position.x + x;
                float worldZ = chunk->position.z + z;

                // Check if this is a grassland biome
                BiomeType biome = GetBiome(worldX, worldZ);
                if (biome != BiomeType::GRASSLAND)
                    continue;

                // Check if a tree should spawn here
                if (!ShouldSpawnTree(worldX, worldZ))
                    continue;

                // Check if position is valid (8 blocks from other trees)
                if (!IsValidTreePosition(worldX, worldZ))
                    continue;

                // Only place trees that won't have leaves cut off by chunk boundaries
                // Trees need 2 blocks of space in all directions for leaves
                if (x < 2 || x >= CHUNK_SIZE - 2 || z < 2 || z >= CHUNK_SIZE - 2)
                    continue;

                // Find ground level
                int groundY = -1;
                for (int y = CHUNK_HEIGHT - 1; y >= 0; y--)
                {
                    if (chunk->blocks[x][y][z].type == BlockType::GRASS)
                    {
                        groundY = y;
                        break;
                    }
                }

                // Place tree if ground was found and there's room (9 blocks for trunk + top leaves)
                if (groundY >= 0 && groundY + 9 < CHUNK_HEIGHT)
                {
                    PlaceTree(chunk, x, groundY, z);
                    treesPlaced++;
                    std::cout << "Placed tree at chunk (" << chunk->position.x << ", " << chunk->position.z 
                              << ") local (" << x << ", " << z << ") world (" << worldX << ", " << worldZ << ")" << std::endl;
                }
            }
        }
        
        if (treesPlaced > 0)
        {
            std::cout << "Total trees placed in chunk: " << treesPlaced << std::endl;
        }

        chunk->GenerateMesh();
    }
};
