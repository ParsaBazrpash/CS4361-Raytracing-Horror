#include "../include/assets.h"
#include "../include/maze.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>

// Generate procedural stone wall texture
Texture2D GenerateStoneWallTexture(int width, int height) {
    // Create image using raylib (it manages the memory)
    Image img = GenImageColor(width, height, (Color){80, 80, 85, 255});
    
    // Access pixel data directly
    Color* pixels = (Color*)img.data;
    
    // Add stone-like noise and variation
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Create mortar lines (grid pattern)
            int gridX = x % 32;
            int gridY = y % 32;
            bool isMortar = (gridX < 2 || gridY < 2 || gridX > 30 || gridY > 30);
            
            if (isMortar) {
                pixels[y * width + x] = (Color){50, 50, 55, 255};
            } else {
                // Add noise for stone texture
                float noise = ((float)(rand() % 100) / 100.0f) * 0.3f;
                int baseR = 80 + (int)(noise * 40);
                int baseG = 80 + (int)(noise * 30);
                int baseB = 85 + (int)(noise * 25);
                pixels[y * width + x] = (Color){baseR, baseG, baseB, 255};
            }
        }
    }
    
    Texture2D texture = LoadTextureFromImage(img);
    UnloadImage(img);  // raylib will free the memory it allocated
    return texture;
}

// Generate procedural wooden floor texture
Texture2D GenerateWoodFloorTexture(int width, int height) {
    // Create image using raylib (it manages the memory)
    Image img = GenImageColor(width, height, (Color){120, 90, 60, 255});
    
    // Access pixel data directly
    Color* pixels = (Color*)img.data;
    
    // Create wood grain pattern
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Wood planks (horizontal strips)
            int plankHeight = 64;
            int plankIdx = y / plankHeight;
            
            // Add grain lines
            float grain = sinf((float)x * 0.1f + (float)plankIdx * 0.5f) * 0.1f;
            float variation = ((float)(rand() % 100) / 100.0f) * 0.2f;
            
            int r = 120 + (int)((grain + variation) * 40);
            int g = 90 + (int)((grain + variation) * 30);
            int b = 60 + (int)((grain + variation) * 20);
            
            // Plank boundaries
            if ((y % plankHeight) < 2) {
                r = (int)(r * 0.7f);
                g = (int)(g * 0.7f);
                b = (int)(b * 0.7f);
            }
            
            pixels[y * width + x] = (Color){r, g, b, 255};
        }
    }
    
    Texture2D texture = LoadTextureFromImage(img);
    UnloadImage(img);  // raylib will free the memory it allocated
    return texture;
}

// Generate simple ceiling texture
Texture2D GenerateCeilingTexture(int width, int height) {
    // Create image using raylib (it manages the memory)
    Image img = GenImageColor(width, height, (Color){150, 150, 155, 255});
    
    // Access pixel data directly
    Color* pixels = (Color*)img.data;
    
    // Add subtle noise
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float noise = ((float)(rand() % 100) / 100.0f) * 0.15f;
            int r = 150 + (int)(noise * 20);
            int g = 150 + (int)(noise * 20);
            int b = 155 + (int)(noise * 20);
            pixels[y * width + x] = (Color){r, g, b, 255};
        }
    }
    
    Texture2D texture = LoadTextureFromImage(img);
    UnloadImage(img);  // raylib will free the memory it allocated
    return texture;
}

// Load all game assets
GameAssets* Assets_Load(void) {
    GameAssets* assets = (GameAssets*)malloc(sizeof(GameAssets));
    if (!assets) return NULL;
    
    assets->wallTexture = GenerateStoneWallTexture(256, 256);
    assets->floorTexture = GenerateWoodFloorTexture(256, 256);
    assets->ceilingTexture = GenerateCeilingTexture(256, 256);
    assets->loaded = true;
    
    return assets;
}

// Unload all game assets
void Assets_Unload(GameAssets* assets) {
    if (!assets || !assets->loaded) return;
    
    UnloadTexture(assets->wallTexture);
    UnloadTexture(assets->floorTexture);
    UnloadTexture(assets->ceilingTexture);
    assets->loaded = false;
    free(assets);
}

// Generate torches on walls at regular intervals
int Torches_Generate(const Maze* maze, Torch** outTorches, int maxTorches) {
    if (!maze || !outTorches || maxTorches <= 0) return 0;
    
    *outTorches = (Torch*)malloc(maxTorches * sizeof(Torch));
    if (!*outTorches) return 0;
    
    int count = 0;
    const float torchSpacing = 12.0f; // Place torches every 12 units (reduced frequency for performance)
    const float torchHeight = 2.0f;  // Height on wall
    const float wallOffset = 0.11f;  // Slight offset from wall surface
    
    // Iterate through cells and place torches on walls
    for (int y = 0; y < maze->height && count < maxTorches; y++) {
        for (int x = 0; x < maze->width && count < maxTorches; x++) {
            float worldX = (x - maze->width * 0.5f + 0.5f) * maze->cellSize;
            float worldZ = (y - maze->height * 0.5f + 0.5f) * maze->cellSize;
            float halfCell = maze->cellSize * 0.5f;
            
            // Check each wall direction
            // North wall
            if (Maze_HasWall(maze, x, y, MAZE_NORTH)) {
                // Place torch at intervals
                float cellStartX = worldX - halfCell;
                for (float offset = 0.5f; offset < maze->cellSize && count < maxTorches; offset += torchSpacing) {
                    (*outTorches)[count].position = (Vector3){
                        cellStartX + offset,
                        torchHeight,
                        worldZ - halfCell - wallOffset
                    };
                    (*outTorches)[count].normal = (Vector3){0, 0, 1}; // Facing south
                    (*outTorches)[count].flickerTime = (float)(rand() % 1000) / 1000.0f * 6.28f; // Random phase
                    (*outTorches)[count].baseIntensity = 0.8f + ((float)(rand() % 20) / 100.0f); // Slight variation
                    count++;
                }
            }
            
            // South wall
            if (Maze_HasWall(maze, x, y, MAZE_SOUTH)) {
                float cellStartX = worldX - halfCell;
                for (float offset = 0.5f; offset < maze->cellSize && count < maxTorches; offset += torchSpacing) {
                    (*outTorches)[count].position = (Vector3){
                        cellStartX + offset,
                        torchHeight,
                        worldZ + halfCell + wallOffset
                    };
                    (*outTorches)[count].normal = (Vector3){0, 0, -1}; // Facing north
                    (*outTorches)[count].flickerTime = (float)(rand() % 1000) / 1000.0f * 6.28f;
                    (*outTorches)[count].baseIntensity = 0.8f + ((float)(rand() % 20) / 100.0f);
                    count++;
                }
            }
            
            // West wall
            if (Maze_HasWall(maze, x, y, MAZE_WEST)) {
                float cellStartZ = worldZ - halfCell;
                for (float offset = 0.5f; offset < maze->cellSize && count < maxTorches; offset += torchSpacing) {
                    (*outTorches)[count].position = (Vector3){
                        worldX - halfCell - wallOffset,
                        torchHeight,
                        cellStartZ + offset
                    };
                    (*outTorches)[count].normal = (Vector3){1, 0, 0}; // Facing east
                    (*outTorches)[count].flickerTime = (float)(rand() % 1000) / 1000.0f * 6.28f;
                    (*outTorches)[count].baseIntensity = 0.8f + ((float)(rand() % 20) / 100.0f);
                    count++;
                }
            }
            
            // East wall
            if (Maze_HasWall(maze, x, y, MAZE_EAST)) {
                float cellStartZ = worldZ - halfCell;
                for (float offset = 0.5f; offset < maze->cellSize && count < maxTorches; offset += torchSpacing) {
                    (*outTorches)[count].position = (Vector3){
                        worldX + halfCell + wallOffset,
                        torchHeight,
                        cellStartZ + offset
                    };
                    (*outTorches)[count].normal = (Vector3){-1, 0, 0}; // Facing west
                    (*outTorches)[count].flickerTime = (float)(rand() % 1000) / 1000.0f * 6.28f;
                    (*outTorches)[count].baseIntensity = 0.8f + ((float)(rand() % 20) / 100.0f);
                    count++;
                }
            }
        }
    }
    
    return count;
}

// Update torch flickering
void Torches_Update(Torch* torches, int count, float dt) {
    for (int i = 0; i < count; i++) {
        torches[i].flickerTime += dt * 8.0f; // Flicker speed
        if (torches[i].flickerTime > 6.28f) {
            torches[i].flickerTime -= 6.28f;
        }
    }
}

// Render torches (simple cube representation)
void Torches_Render(const Torch* torches, int count) {
    // Draw simple cubes for torches
    for (int i = 0; i < count; i++) {
        // Draw torch base (small cube)
        DrawCube(torches[i].position, 0.1f, 0.3f, 0.1f, (Color){60, 40, 20, 255});
        // Draw torch bracket
        Vector3 bracketPos = torches[i].position;
        bracketPos.y += 0.15f;
        DrawCube(bracketPos, 0.15f, 0.05f, 0.05f, (Color){80, 80, 80, 255});
    }
}

// Create particle system
ParticleSystem* ParticleSystem_Create(int maxParticles) {
    ParticleSystem* ps = (ParticleSystem*)malloc(sizeof(ParticleSystem));
    if (!ps) return NULL;
    
    ps->particles = (Particle*)malloc(maxParticles * sizeof(Particle));
    if (!ps->particles) {
        free(ps);
        return NULL;
    }
    
    ps->maxParticles = maxParticles;
    ps->activeParticles = 0;
    ps->emitRate = 15.0f; // Reduced emit rate for performance (particles per second)
    ps->emitAccumulator = 0.0f;
    ps->emitterPos = (Vector3){0, 0, 0};
    
    return ps;
}

// Destroy particle system
void ParticleSystem_Destroy(ParticleSystem* ps) {
    if (!ps) return;
    if (ps->particles) free(ps->particles);
    free(ps);
}

// Update particle system
void ParticleSystem_Update(ParticleSystem* ps, Vector3 emitterPos, float dt) {
    if (!ps) return;
    
    ps->emitterPos = emitterPos;
    
    // Emit new particles
    ps->emitAccumulator += ps->emitRate * dt;
    int toEmit = (int)ps->emitAccumulator;
    ps->emitAccumulator -= (float)toEmit;
    
    for (int i = 0; i < toEmit && ps->activeParticles < ps->maxParticles; i++) {
        Particle* p = &ps->particles[ps->activeParticles];
        p->position = emitterPos;
        p->position.y += 0.25f; // Slight offset above torch
        p->velocity = (Vector3){
            ((float)(rand() % 200) - 100.0f) / 500.0f,
            ((float)(rand() % 300) + 100.0f) / 500.0f,
            ((float)(rand() % 200) - 100.0f) / 500.0f
        };
        p->life = 1.0f;
        p->maxLife = 0.5f + ((float)(rand() % 50) / 100.0f);
        p->size = 0.05f + ((float)(rand() % 30) / 1000.0f);
        p->color = (Color){
            255,
            150 + (rand() % 50),
            0,
            255
        };
        ps->activeParticles++;
    }
    
    // Update existing particles
    for (int i = 0; i < ps->activeParticles; i++) {
        Particle* p = &ps->particles[i];
        
        // Update physics
        p->velocity.y += -2.0f * dt; // Gravity
        p->position.x += p->velocity.x * dt;
        p->position.y += p->velocity.y * dt;
        p->position.z += p->velocity.z * dt;
        
        // Update life
        p->life -= dt;
        
        // Remove dead particles (swap with last)
        if (p->life <= 0.0f) {
            ps->particles[i] = ps->particles[ps->activeParticles - 1];
            ps->activeParticles--;
            i--;
        }
    }
}

// Render particle system (optimized - using simple cubes instead of spheres)
void ParticleSystem_Render(const ParticleSystem* ps) {
    if (!ps) return;
    
    // Use simple cubes instead of spheres for better performance
    for (int i = 0; i < ps->activeParticles; i++) {
        const Particle* p = &ps->particles[i];
        
        // Calculate alpha based on life
        float alpha = p->life / p->maxLife;
        Color renderColor = p->color;
        renderColor.a = (unsigned char)(alpha * 255.0f);
        
        // Draw as small cube (much faster than sphere)
        float size = p->size * 2.0f; // Scale up slightly for visibility
        DrawCube(p->position, size, size, size, renderColor);
    }
}

// Update lighting for torches (flickering point lights)
// Note: This stores light data for rendering - actual lighting is handled via shaders or visual representation
void Lighting_UpdateTorchLights(const Torch* torches, int count, float time) {
    // This function is called to update torch flicker states
    // Actual lighting rendering happens in the main render loop
    (void)torches;
    (void)count;
    (void)time;
}

