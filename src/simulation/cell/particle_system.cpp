#include "cell_manager.h"
#include "../spatial/spatial_grid_system.h"
#include "../../rendering/camera/camera.h"
#include "../../utils/timer.h"
#include <iostream>

// Particle structure matching shader
struct Particle {
    glm::vec3 position;      // World position
    float lifetime;          // Remaining lifetime (0 = dead)
    glm::vec3 velocity;      // Velocity for movement
    float maxLifetime;       // Maximum lifetime for fade calculation
    glm::vec4 color;         // RGBA color
};

// Particle instance for rendering (properly aligned)
struct ParticleInstance {
    glm::vec3 position;
    float size;
    glm::vec4 color;
    float lifetime;      // Current lifetime
    float maxLifetime;   // Maximum lifetime
    float fadeFactor;    // Distance-based fade factor
    float _padding[1];   // Padding for 16-byte alignment
};

void CellManager::initializeParticleSystem()
{
    std::cout << "Initializing particle system with unified spatial grid...\n";
    
    // Calculate total particles based on grid
    int totalVoxels = config::TOTAL_GRID_CELLS;
    totalMaxParticles = totalVoxels * maxParticlesPerVoxel;
    
    std::cout << "Total voxels: " << totalVoxels << "\n";
    std::cout << "Max particles per voxel: " << maxParticlesPerVoxel << "\n";
    std::cout << "Total max particles: " << totalMaxParticles << "\n";
    
    // Create particle buffer
    glCreateBuffers(1, &particleBuffer);
    glNamedBufferStorage(particleBuffer, 
                        totalMaxParticles * sizeof(Particle), 
                        nullptr, 
                        GL_DYNAMIC_STORAGE_BIT);
    
    // Initialize particles to zero
    std::vector<Particle> initialParticles(totalMaxParticles);
    for (auto& p : initialParticles) {
        p.lifetime = 0.0f;
        p.maxLifetime = 0.0f;
    }
    glNamedBufferSubData(particleBuffer, 0, totalMaxParticles * sizeof(Particle), initialParticles.data());
    
    // Create particle count buffer for grid insertion
    glCreateBuffers(1, &particleCountBuffer);
    uint32_t particleCountData[4] = { static_cast<uint32_t>(totalMaxParticles), 0, 0, 0 };
    glNamedBufferStorage(particleCountBuffer, 
                        sizeof(uint32_t) * 4, 
                        particleCountData, 
                        GL_DYNAMIC_STORAGE_BIT);
    
    // Create instance buffer for rendering
    glCreateBuffers(1, &particleInstanceBuffer);
    glNamedBufferStorage(particleInstanceBuffer, 
                        totalMaxParticles * sizeof(ParticleInstance), 
                        nullptr, 
                        GL_DYNAMIC_STORAGE_BIT);
    
    // Create VAO and VBO for particle quad
    glCreateVertexArrays(1, &particleVAO);
    glCreateBuffers(1, &particleVBO);
    
    // Quad vertices (position + texcoord)
    float quadVertices[] = {
        // Positions        // TexCoords
        -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,
         0.5f, -0.5f, 0.0f,  1.0f, 0.0f,
         0.5f,  0.5f, 0.0f,  1.0f, 1.0f,
        -0.5f,  0.5f, 0.0f,  0.0f, 1.0f
    };
    
    glNamedBufferStorage(particleVBO, sizeof(quadVertices), quadVertices, 0);
    
    // Setup VAO
    glVertexArrayVertexBuffer(particleVAO, 0, particleVBO, 0, 5 * sizeof(float));
    
    // Position attribute
    glEnableVertexArrayAttrib(particleVAO, 0);
    glVertexArrayAttribFormat(particleVAO, 0, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(particleVAO, 0, 0);
    
    // TexCoord attribute
    glEnableVertexArrayAttrib(particleVAO, 1);
    glVertexArrayAttribFormat(particleVAO, 1, 2, GL_FLOAT, GL_FALSE, 3 * sizeof(float));
    glVertexArrayAttribBinding(particleVAO, 1, 0);
    
    // Setup instance buffer binding
    glVertexArrayVertexBuffer(particleVAO, 1, particleInstanceBuffer, 0, sizeof(ParticleInstance));
    
    // Instance position attribute
    glEnableVertexArrayAttrib(particleVAO, 2);
    glVertexArrayAttribFormat(particleVAO, 2, 3, GL_FLOAT, GL_FALSE, offsetof(ParticleInstance, position));
    glVertexArrayAttribBinding(particleVAO, 2, 1);
    glVertexArrayBindingDivisor(particleVAO, 1, 1);
    
    // Instance size attribute
    glEnableVertexArrayAttrib(particleVAO, 3);
    glVertexArrayAttribFormat(particleVAO, 3, 1, GL_FLOAT, GL_FALSE, offsetof(ParticleInstance, size));
    glVertexArrayAttribBinding(particleVAO, 3, 1);
    
    // Instance color attribute
    glEnableVertexArrayAttrib(particleVAO, 4);
    glVertexArrayAttribFormat(particleVAO, 4, 4, GL_FLOAT, GL_FALSE, offsetof(ParticleInstance, color));
    glVertexArrayAttribBinding(particleVAO, 4, 1);
    
    // Instance lifetime attribute
    glEnableVertexArrayAttrib(particleVAO, 5);
    glVertexArrayAttribFormat(particleVAO, 5, 1, GL_FLOAT, GL_FALSE, offsetof(ParticleInstance, lifetime));
    glVertexArrayAttribBinding(particleVAO, 5, 1);
    
    // Instance max lifetime attribute
    glEnableVertexArrayAttrib(particleVAO, 6);
    glVertexArrayAttribFormat(particleVAO, 6, 1, GL_FLOAT, GL_FALSE, offsetof(ParticleInstance, maxLifetime));
    glVertexArrayAttribBinding(particleVAO, 6, 1);
    
    // Instance fade factor attribute
    glEnableVertexArrayAttrib(particleVAO, 7);
    glVertexArrayAttribFormat(particleVAO, 7, 1, GL_FLOAT, GL_FALSE, offsetof(ParticleInstance, fadeFactor));
    glVertexArrayAttribBinding(particleVAO, 7, 1);
    
    // Initialize shaders - now using unified grid system
    particleUpdateShader = new Shader("shaders/particles/particle_update.comp");
    particleGridInsertShader = new Shader("shaders/particles/particle_grid_insert.comp");
    particleExtractShader = new Shader("shaders/particles/particle_extract.comp");
    particleRenderShader = new Shader("shaders/particles/particle.vert", "shaders/particles/particle.frag");
    
    std::cout << "Particle system initialized successfully with unified spatial grid\n";
}

void CellManager::updateParticles(float deltaTime)
{
    if (!enableParticles) return;
    
    // Step 1: Update particle behavior (voxel-based shader with original behavior)
    particleUpdateShader->use();
    
    // Set uniforms
    particleUpdateShader->setFloat("u_deltaTime", deltaTime);
    particleUpdateShader->setFloat("u_time", static_cast<float>(glfwGetTime()));
    particleUpdateShader->setInt("u_gridResolution", config::GRID_RESOLUTION);
    particleUpdateShader->setFloat("u_gridCellSize", config::GRID_CELL_SIZE);
    particleUpdateShader->setFloat("u_worldSize", config::WORLD_SIZE);
    particleUpdateShader->setInt("u_maxParticlesPerVoxel", maxParticlesPerVoxel);
    particleUpdateShader->setInt("u_maxParticlesTotal", totalMaxParticles);
    particleUpdateShader->setInt("u_maxCellsPerGrid", config::MAX_CELLS_PER_GRID);
    particleUpdateShader->setFloat("u_spawnRate", particleSpawnRate);
    particleUpdateShader->setFloat("u_particleLifetime", particleLifetime);
    
    // Sphere culling uniforms
    particleUpdateShader->setFloat("u_sphereRadius", config::SPHERE_RADIUS);
    particleUpdateShader->setVec3("u_sphereCenter", config::SPHERE_CENTER);
    particleUpdateShader->setInt("u_enableSphereCulling", config::ENABLE_SPHERE_CULLING ? 1 : 0);
    
    // Noise parameters for distinct cloud regions
    particleUpdateShader->setFloat("u_noiseScale", 0.08f);     // Slightly smaller scale for bigger regions
    particleUpdateShader->setFloat("u_noiseThreshold", 0.4f);  // Higher threshold for less sparse areas
    particleUpdateShader->setFloat("u_timeScale", 0.15f);      // Slower evolution for more stable regions
    particleUpdateShader->setVec3("u_cloudOffset", glm::vec3(0.0f, 20.0f, 0.0f)); // Offset clouds upward
    
    // Bind particle buffer (particles update themselves but don't need grid buffers for this step)
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particleBuffer);
    
    // Dispatch compute shader (one thread per voxel)
    int totalVoxels = config::TOTAL_GRID_CELLS;
    GLuint numGroups = (totalVoxels + 255) / 256;
    particleUpdateShader->dispatch(numGroups, 1, 1);
    
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    
    // Step 2: Insert active particles into unified spatial grid
    particleGridInsertShader->use();
    
    // Set uniforms for grid insertion
    particleGridInsertShader->setInt("u_gridResolution", config::GRID_RESOLUTION);
    particleGridInsertShader->setFloat("u_gridCellSize", config::GRID_CELL_SIZE);
    particleGridInsertShader->setFloat("u_worldSize", config::WORLD_SIZE);
    particleGridInsertShader->setInt("u_maxCellsPerGrid", config::MAX_CELLS_PER_GRID);
    particleGridInsertShader->setInt("u_particleIndexOffset", config::PARTICLE_SPATIAL_GRID_INDEX_OFFSET); // Offset to distinguish from cells
    
    // Bind buffers for unified grid insertion
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particleBuffer);     // Particle data
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, this->spatialGridSystem->getCellGridBuffer());         // Unified grid buffer
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, this->spatialGridSystem->getCellOffsetBuffer());    // Grid offsets
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, this->spatialGridSystem->getCellCountBuffer());     // Grid counts
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, particleCountBuffer); // Particle count buffer
    
    // Dispatch compute shader (one thread per particle)
    GLuint numParticleGroups = (totalMaxParticles + 255) / 256;
    particleGridInsertShader->dispatch(numParticleGroups, 1, 1);
    
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void CellManager::renderParticles(const Camera& camera, const glm::vec2& viewportSize)
{
    if (!enableParticles) return;
    
    // TimerGPU timer("Particle Rendering"); // Temporarily disabled due to query errors
    
    // Extract instances for rendering
    {
        // TimerGPU extractTimer("Particle Extract"); // Temporarily disabled due to query errors
        
        // Clear counter
        uint32_t zero = 0;
        glNamedBufferSubData(particleCountBuffer, 0, sizeof(uint32_t), &zero);
        
        particleExtractShader->use();
        
        // Set uniforms
        particleExtractShader->setInt("u_maxParticles", totalMaxParticles);
        particleExtractShader->setFloat("u_particleSize", particleSize);
        
        // Distance culling uniforms (same as cells)
        particleExtractShader->setVec3("u_cameraPos", camera.getPosition());
        particleExtractShader->setFloat("u_maxRenderDistance", 170.0f);    // Same as cells
        particleExtractShader->setFloat("u_fadeStartDistance", 30.0f);     // Same as cells
        particleExtractShader->setFloat("u_fadeEndDistance", 160.0f);      // Same as cells
        particleExtractShader->setInt("u_useDistanceCulling", 1);          // Enable distance culling
        particleExtractShader->setInt("u_useFade", 1);                     // Enable distance fading
        
        // Sphere culling uniforms
        particleExtractShader->setFloat("u_sphereRadius", config::SPHERE_RADIUS);
        particleExtractShader->setVec3("u_sphereCenter", config::SPHERE_CENTER);
        particleExtractShader->setInt("u_enableSphereCulling", config::ENABLE_SPHERE_CULLING ? 1 : 0);
        
        // Bind buffers
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particleBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, particleInstanceBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, particleCountBuffer);
        
        // Dispatch compute shader
        GLuint numGroups = (totalMaxParticles + 255) / 256;
        particleExtractShader->dispatch(numGroups, 1, 1);
        
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);
    }
    
    // Read back instance count
    uint32_t instanceCount = 0;
    glGetNamedBufferSubData(particleCountBuffer, 0, sizeof(uint32_t), &instanceCount);
    
    if (instanceCount == 0) return;
    
    // Render particles
    {
        // TimerGPU renderTimer("Particle Draw"); // Temporarily disabled due to query errors
        
        particleRenderShader->use();
        
        // Set uniforms
        float aspectRatio = viewportSize.x / viewportSize.y;
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), 
                                               aspectRatio,
                                               0.1f, 1000.0f);
        glm::mat4 view = camera.getViewMatrix();
        
        particleRenderShader->setMat4("uProjection", projection);
        particleRenderShader->setMat4("uView", view);
        particleRenderShader->setVec3("uCameraPos", camera.getPosition());
        
        // Enable blending for transparency
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
        // Disable depth writing but keep depth testing
        glDepthMask(GL_FALSE);
        
        // Bind VAO and render
        glBindVertexArray(particleVAO);
        glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, 4, instanceCount);
        glBindVertexArray(0);
        
        // Restore state
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
}

void CellManager::cleanupParticleSystem()
{
    if (particleBuffer != 0) {
        glDeleteBuffers(1, &particleBuffer);
        particleBuffer = 0;
    }
    
    if (particleInstanceBuffer != 0) {
        glDeleteBuffers(1, &particleInstanceBuffer);
        particleInstanceBuffer = 0;
    }
    
    if (particleCountBuffer != 0) {
        glDeleteBuffers(1, &particleCountBuffer);
        particleCountBuffer = 0;
    }
    
    if (particleVAO != 0) {
        glDeleteVertexArrays(1, &particleVAO);
        particleVAO = 0;
    }
    
    if (particleVBO != 0) {
        glDeleteBuffers(1, &particleVBO);
        particleVBO = 0;
    }
    
    if (particleUpdateShader) {
        particleUpdateShader->destroy();
        delete particleUpdateShader;
        particleUpdateShader = nullptr;
    }
    
    if (particleGridInsertShader) {
        particleGridInsertShader->destroy();
        delete particleGridInsertShader;
        particleGridInsertShader = nullptr;
    }
    
    if (particleExtractShader) {
        particleExtractShader->destroy();
        delete particleExtractShader;
        particleExtractShader = nullptr;
    }
    
    if (particleRenderShader) {
        particleRenderShader->destroy();
        delete particleRenderShader;
        particleRenderShader = nullptr;
    }
}
