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
    ChunkManager(int renderDistance = 20) : m_renderDistance(renderDistance), m_physicsTimer(0.0f) {}

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

    // Destroy block at world position
    void DestroyBlock(float worldX, float worldY, float worldZ)
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
            return;
        
        // Find chunk
        long long key = TerrainGenerator::GetChunkKey(chunkX, chunkZ);
        auto it = m_chunks.find(key);
        if (it != m_chunks.end())
        {
            // Don't destroy bedrock
            BlockType blockType = it->second->blocks[localX][localY][localZ].type;
            if (blockType == BlockType::BEDROCK)
                return;
            
            // Destroy block
            it->second->SetBlock(localX, localY, localZ, BlockType::AIR);
            it->second->GenerateMesh();
        }
    }
    
    void PlaceBlock(float worldX, float worldY, float worldZ, BlockType blockType)
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
        {
            return;
        }
        
        // Find chunk
        long long key = TerrainGenerator::GetChunkKey(chunkX, chunkZ);
        auto it = m_chunks.find(key);
        if (it != m_chunks.end())
        {
            BlockType existingBlock = it->second->blocks[localX][localY][localZ].type;
            
            // Check if space is empty
            if (existingBlock == BlockType::AIR)
            {
                // Place block
                it->second->SetBlock(localX, localY, localZ, blockType);
                it->second->GenerateMesh();
                
                // Regenerate neighboring chunks if block is on edge
                if (localX == 0)
                {
                    long long leftKey = TerrainGenerator::GetChunkKey(chunkX - 1, chunkZ);
                    auto leftIt = m_chunks.find(leftKey);
                    if (leftIt != m_chunks.end())
                        leftIt->second->GenerateMesh();
                }
                if (localX == CHUNK_SIZE - 1)
                {
                    long long rightKey = TerrainGenerator::GetChunkKey(chunkX + 1, chunkZ);
                    auto rightIt = m_chunks.find(rightKey);
                    if (rightIt != m_chunks.end())
                        rightIt->second->GenerateMesh();
                }
                if (localZ == 0)
                {
                    long long frontKey = TerrainGenerator::GetChunkKey(chunkX, chunkZ - 1);
                    auto frontIt = m_chunks.find(frontKey);
                    if (frontIt != m_chunks.end())
                        frontIt->second->GenerateMesh();
                }
                if (localZ == CHUNK_SIZE - 1)
                {
                    long long backKey = TerrainGenerator::GetChunkKey(chunkX, chunkZ + 1);
                    auto backIt = m_chunks.find(backKey);
                    if (backIt != m_chunks.end())
                        backIt->second->GenerateMesh();
                }
            }
        }
    }
    
    // Update physics for gravity-affected blocks (like sand)
    void UpdatePhysics(float deltaTime)
    {
        // Update timer - only process physics every 0.05 seconds for smooth falling animation
        m_physicsTimer += deltaTime;
        if (m_physicsTimer < 0.05f)
            return;
        
        m_physicsTimer = 0.0f;
        
        // Collect all sand blocks that need to fall
        std::vector<std::tuple<int, int, int, BlockType>> blocksToMove;
        
        for (auto& chunkPair : m_chunks)
        {
            Chunk* chunk = chunkPair.second;
            int chunkWorldX = (int)chunk->position.x;
            int chunkWorldZ = (int)chunk->position.z;
            
            // Check each block in the chunk
            for (int x = 0; x < CHUNK_SIZE; x++)
            {
                for (int y = 1; y < CHUNK_HEIGHT; y++)  // Start at 1 since y=0 is bedrock
                {
                    for (int z = 0; z < CHUNK_SIZE; z++)
                    {
                        BlockType blockType = chunk->blocks[x][y][z].type;
                        
                        // Check if block is sand
                        if (blockType == BlockType::DESERT_SAND)
                        {
                            // Check if there's air below
                            BlockType belowType = chunk->blocks[x][y - 1][z].type;
                            
                            if (belowType == BlockType::AIR)
                            {
                                // Calculate world position
                                int worldX = chunkWorldX + x;
                                int worldY = y;
                                int worldZ = chunkWorldZ + z;
                                
                                blocksToMove.push_back(std::make_tuple(worldX, worldY, worldZ, blockType));
                            }
                        }
                    }
                }
            }
        }
        
        // Move all falling blocks one block down at a time (for visible falling)
        for (const auto& blockData : blocksToMove)
        {
            int worldX = std::get<0>(blockData);
            int worldY = std::get<1>(blockData);
            int worldZ = std::get<2>(blockData);
            BlockType blockType = std::get<3>(blockData);
            
            // Remove block from current position
            DestroyBlock((float)worldX, (float)worldY, (float)worldZ);
            
            // Place block one position down (if there's air)
            int newY = worldY - 1;
            if (newY > 0)
            {
                BlockType belowType = GetBlockAtPosition((float)worldX, (float)newY, (float)worldZ);
                if (belowType == BlockType::AIR)
                {
                    PlaceBlock((float)worldX, (float)newY, (float)worldZ, blockType);
                }
            }
        }
    }

private:
    std::map<long long, Chunk*> m_chunks;
    int m_renderDistance;
    float m_physicsTimer;  // Timer for physics updates
};
