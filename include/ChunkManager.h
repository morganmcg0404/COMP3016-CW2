#pragma once

#include <map>
#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include "Chunk.h"
#include "TerrainGenerator.h"
#include "Camera.h"

class ChunkManager
{
public:
    ChunkManager(int renderDistance = 20) : m_renderDistance(renderDistance) {}

    ~ChunkManager()
    {
        // Clean up all chunks
        for (auto& pair : m_chunks)
        {
            delete pair.second;
        }
        m_chunks.clear();
    }

    // Update chunks based on camera position
    void Update(const Camera& camera)
    {
        // Get camera chunk position
        int camChunkX = (int)floor(camera.Position.x / CHUNK_SIZE);
        int camChunkZ = (int)floor(camera.Position.z / CHUNK_SIZE);

        // Calculate how many chunks we need in each direction
        int chunksPerSide = (m_renderDistance / CHUNK_SIZE) + 1;

        std::vector<long long> chunksToKeep;

        // Load chunks within render distance
        for (int x = camChunkX - chunksPerSide; x <= camChunkX + chunksPerSide; x++)
        {
            for (int z = camChunkZ - chunksPerSide; z <= camChunkZ + chunksPerSide; z++)
            {
                // Check if chunk is within circular render distance
                float dx = (x * CHUNK_SIZE + CHUNK_SIZE / 2) - camera.Position.x;
                float dz = (z * CHUNK_SIZE + CHUNK_SIZE / 2) - camera.Position.z;
                float distance = sqrt(dx * dx + dz * dz);

                if (distance <= m_renderDistance)
                {
                    long long key = TerrainGenerator::GetChunkKey(x, z);
                    chunksToKeep.push_back(key);

                    // Create chunk if it doesn't exist
                    if (m_chunks.find(key) == m_chunks.end())
                    {
                        glm::vec3 chunkPos(x * CHUNK_SIZE, 0, z * CHUNK_SIZE);
                        Chunk* newChunk = new Chunk(chunkPos);
                        TerrainGenerator::GenerateChunk(newChunk);
                        m_chunks[key] = newChunk;
                    }
                }
            }
        }

        // Unload chunks that are too far away
        std::vector<long long> chunksToRemove;
        for (auto& pair : m_chunks)
        {
            bool shouldKeep = false;
            for (long long key : chunksToKeep)
            {
                if (pair.first == key)
                {
                    shouldKeep = true;
                    break;
                }
            }

            if (!shouldKeep)
            {
                chunksToRemove.push_back(pair.first);
            }
        }

        // Remove chunks
        for (long long key : chunksToRemove)
        {
            delete m_chunks[key];
            m_chunks.erase(key);
        }
    }

    // Render all loaded chunks
    void Render()
    {
        for (auto& pair : m_chunks)
        {
            pair.second->Render();
        }
    }

    // Get number of loaded chunks
    int GetLoadedChunkCount() const
    {
        return static_cast<int>(m_chunks.size());
    }

    // Get block at world position
    BlockType GetBlockAtPosition(float worldX, float worldY, float worldZ) const
    {
        // Convert world coordinates to chunk coordinates
        int chunkX = (int)floor(worldX / CHUNK_SIZE);
        int chunkZ = (int)floor(worldZ / CHUNK_SIZE);
        
        // Get local coordinates within chunk
        int localX = ((int)floor(worldX) % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        int localY = (int)floor(worldY);
        int localZ = ((int)floor(worldZ) % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
        
        // Check bounds
        if (localY < 0 || localY >= CHUNK_HEIGHT)
            return BlockType::AIR;
        
        // Find chunk
        long long key = TerrainGenerator::GetChunkKey(chunkX, chunkZ);
        auto it = m_chunks.find(key);
        if (it != m_chunks.end())
        {
            return it->second->blocks[localX][localY][localZ].type;
        }
        
        return BlockType::AIR;
    }

    // Check if position is solid (for collision)
    bool IsSolid(float worldX, float worldY, float worldZ) const
    {
        BlockType blockType = GetBlockAtPosition(worldX, worldY, worldZ);
        return blockType != BlockType::AIR;
    }

private:
    std::map<long long, Chunk*> m_chunks;
    int m_renderDistance;
};
