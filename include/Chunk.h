#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include "Block.h"
#include "TextureGenerator.h"

const int CHUNK_SIZE = 16;
const int CHUNK_HEIGHT = 64;

class Chunk
{
public:
    glm::vec3 position;  // World position of chunk
    Block blocks[CHUNK_SIZE][CHUNK_HEIGHT][CHUNK_SIZE];
    
    unsigned int VAO, VBO, EBO;
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    int indexCount;
    bool needsUpdate;

    Chunk(glm::vec3 pos) : position(pos), VAO(0), VBO(0), EBO(0), indexCount(0), needsUpdate(true)
    {
        // Initialize all blocks as air
        for (int x = 0; x < CHUNK_SIZE; x++)
        {
            for (int y = 0; y < CHUNK_HEIGHT; y++)
            {
                for (int z = 0; z < CHUNK_SIZE; z++)
                {
                    blocks[x][y][z] = Block();
                }
            }
        }
    }

    ~Chunk()
    {
        Cleanup();
    }

    // Set a block at local coordinates
    void SetBlock(int x, int y, int z, BlockType type)
    {
        if (x >= 0 && x < CHUNK_SIZE && y >= 0 && y < CHUNK_HEIGHT && z >= 0 && z < CHUNK_SIZE)
        {
            blocks[x][y][z] = Block(type);
            needsUpdate = true;
        }
    }

    // Get a block at local coordinates
    Block* GetBlock(int x, int y, int z)
    {
        if (x >= 0 && x < CHUNK_SIZE && y >= 0 && y < CHUNK_HEIGHT && z >= 0 && z < CHUNK_SIZE)
        {
            return &blocks[x][y][z];
        }
        return nullptr;
    }

    // Check if block is solid
    bool IsBlockSolid(int x, int y, int z)
    {
        Block* block = GetBlock(x, y, z);
        return block && block->isActive && block->type != BlockType::AIR;
    }

    // Generate mesh for this chunk
    void GenerateMesh()
    {
        vertices.clear();
        indices.clear();
        unsigned int vertexCount = 0;

        for (int x = 0; x < CHUNK_SIZE; x++)
        {
            for (int y = 0; y < CHUNK_HEIGHT; y++)
            {
                for (int z = 0; z < CHUNK_SIZE; z++)
                {
                    Block& block = blocks[x][y][z];
                    if (!block.isActive || block.type == BlockType::AIR)
                        continue;

                    glm::vec3 blockPos = position + glm::vec3(x, y, z);
                    glm::vec3 color = block.GetColor();

                    // Only render faces that are exposed (not adjacent to other solid blocks)
                    // Front face
                    if (!IsBlockSolid(x, y, z + 1))
                    {
                        AddFace(blockPos, color, 0, vertexCount);
                    }
                    // Back face
                    if (!IsBlockSolid(x, y, z - 1))
                    {
                        AddFace(blockPos, color, 1, vertexCount);
                    }
                    // Top face
                    if (!IsBlockSolid(x, y + 1, z))
                    {
                        AddFace(blockPos, color, 2, vertexCount);
                    }
                    // Bottom face
                    if (!IsBlockSolid(x, y - 1, z))
                    {
                        AddFace(blockPos, color, 3, vertexCount);
                    }
                    // Right face
                    if (!IsBlockSolid(x + 1, y, z))
                    {
                        AddFace(blockPos, color, 4, vertexCount);
                    }
                    // Left face
                    if (!IsBlockSolid(x - 1, y, z))
                    {
                        AddFace(blockPos, color, 5, vertexCount);
                    }
                }
            }
        }

        SetupMesh();
        needsUpdate = false;
    }

    // Render the chunk
    void Render()
    {
        if (indexCount == 0)
            return;

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    void Cleanup()
    {
        if (VAO != 0)
        {
            glDeleteVertexArrays(1, &VAO);
            glDeleteBuffers(1, &VBO);
            glDeleteBuffers(1, &EBO);
            VAO = VBO = EBO = 0;
        }
    }

private:
    void AddFace(glm::vec3 pos, glm::vec3 color, int faceIndex, unsigned int& vertexCount)
    {
        // Define cube faces
        static const float cubeVertices[6][4][6] = {
            // Front face (z+)
            {
                {0.0f, 0.0f, 1.0f,  0.0f, 0.0f, 1.0f},
                {1.0f, 0.0f, 1.0f,  0.0f, 0.0f, 1.0f},
                {1.0f, 1.0f, 1.0f,  0.0f, 0.0f, 1.0f},
                {0.0f, 1.0f, 1.0f,  0.0f, 0.0f, 1.0f}
            },
            // Back face (z-)
            {
                {1.0f, 0.0f, 0.0f,  0.0f, 0.0f, -1.0f},
                {0.0f, 0.0f, 0.0f,  0.0f, 0.0f, -1.0f},
                {0.0f, 1.0f, 0.0f,  0.0f, 0.0f, -1.0f},
                {1.0f, 1.0f, 0.0f,  0.0f, 0.0f, -1.0f}
            },
            // Top face (y+)
            {
                {0.0f, 1.0f, 0.0f,  0.0f, 1.0f, 0.0f},
                {1.0f, 1.0f, 0.0f,  0.0f, 1.0f, 0.0f},
                {1.0f, 1.0f, 1.0f,  0.0f, 1.0f, 0.0f},
                {0.0f, 1.0f, 1.0f,  0.0f, 1.0f, 0.0f}
            },
            // Bottom face (y-)
            {
                {0.0f, 0.0f, 1.0f,  0.0f, -1.0f, 0.0f},
                {1.0f, 0.0f, 1.0f,  0.0f, -1.0f, 0.0f},
                {1.0f, 0.0f, 0.0f,  0.0f, -1.0f, 0.0f},
                {0.0f, 0.0f, 0.0f,  0.0f, -1.0f, 0.0f}
            },
            // Right face (x+)
            {
                {1.0f, 0.0f, 1.0f,  1.0f, 0.0f, 0.0f},
                {1.0f, 0.0f, 0.0f,  1.0f, 0.0f, 0.0f},
                {1.0f, 1.0f, 0.0f,  1.0f, 0.0f, 0.0f},
                {1.0f, 1.0f, 1.0f,  1.0f, 0.0f, 0.0f}
            },
            // Left face (x-)
            {
                {0.0f, 0.0f, 0.0f,  -1.0f, 0.0f, 0.0f},
                {0.0f, 0.0f, 1.0f,  -1.0f, 0.0f, 0.0f},
                {0.0f, 1.0f, 1.0f,  -1.0f, 0.0f, 0.0f},
                {0.0f, 1.0f, 0.0f,  -1.0f, 0.0f, 0.0f}
            }
        };

        // Add vertices with per-vertex noise variation
        for (int i = 0; i < 4; i++)
        {
            glm::vec3 vertexLocalPos = glm::vec3(
                cubeVertices[faceIndex][i][0],
                cubeVertices[faceIndex][i][1],
                cubeVertices[faceIndex][i][2]
            );
            glm::vec3 vertexWorldPos = pos + vertexLocalPos;
            
            // Get noise-varied color for this vertex based on block-local position
            glm::vec3 variedColor = TextureGenerator::GetVertexColor(color, vertexLocalPos);
            
            // Position
            vertices.push_back(vertexWorldPos.x);
            vertices.push_back(vertexWorldPos.y);
            vertices.push_back(vertexWorldPos.z);
            // Normal
            vertices.push_back(cubeVertices[faceIndex][i][3]);
            vertices.push_back(cubeVertices[faceIndex][i][4]);
            vertices.push_back(cubeVertices[faceIndex][i][5]);
            // Color with noise variation
            vertices.push_back(variedColor.r);
            vertices.push_back(variedColor.g);
            vertices.push_back(variedColor.b);
        }

        // Add indices (two triangles per face)
        indices.push_back(vertexCount + 0);
        indices.push_back(vertexCount + 1);
        indices.push_back(vertexCount + 2);
        indices.push_back(vertexCount + 2);
        indices.push_back(vertexCount + 3);
        indices.push_back(vertexCount + 0);

        vertexCount += 4;
    }

    void SetupMesh()
    {
        // Cleanup old buffers if they exist
        if (VAO != 0)
        {
            glDeleteVertexArrays(1, &VAO);
            glDeleteBuffers(1, &VBO);
            glDeleteBuffers(1, &EBO);
        }

        // Create buffers
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), &vertices[0], GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

        // Position attribute
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        // Normal attribute
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        // Color attribute
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);

        glBindVertexArray(0);

        indexCount = static_cast<int>(indices.size());
    }
};
