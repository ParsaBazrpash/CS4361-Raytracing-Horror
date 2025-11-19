#include "raylib.h"
#include "../include/maze.h"
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

// ---------- Game Constants ----------
#define MAZE_WIDTH           15      // Number of cells horizontally
#define MAZE_HEIGHT          15      // Number of cells vertically
#define CELL_SIZE            3.0f    // Size of each cell in world units
#define WALL_THICK           0.2f    // Wall thickness for rendering
#define WALL_HEIGHT          4.0f    // Height of walls

#define PLAYER_RADIUS        0.30f   // Collision radius (XZ)
#define PLAYER_EYE_HEIGHT    1.80f   // Camera height above "feet"
#define GRAVITY             -18.0f
#define JUMP_SPEED           6.5f
#define MOVE_SPEED           5.0f
#define RUN_MULTIPLIER       1.8f
#define MOUSE_SENS           0.0020f // Radians per pixel

// Game state
typedef enum {
    GAME_STATE_PLAYING,
    GAME_STATE_WON
} GameState;

// Helper function: Clamp float value
static inline float clampf(float v, float lo, float hi) {
    return (v < lo) ? lo : (v > hi) ? hi : v;
}

// Circle (player) vs axis-aligned rectangle (wall) collision in XZ plane
static bool CircleRectIntersect(Vector2 c, float r, Rectangle rect) {
    float nx = clampf(c.x, rect.x, rect.x + rect.width);
    float nz = clampf(c.y, rect.y, rect.y + rect.height);
    float dx = c.x - nx;
    float dz = c.y - nz;
    return (dx*dx + dz*dz) <= r*r;
}

// Check collision with any wall rectangle
static bool CollidesAny(Vector2 c, float r, const WallRect* walls, int count) {
    for (int i = 0; i < count; ++i) {
        if (CircleRectIntersect(c, r, walls[i].rect)) return true;
    }
    return false;
}

// Initialize game (generate maze, reset player)
static void InitGame(Maze** maze, WallRect** walls, int* wallCount, Vector3* playerPos, 
                     float* yaw, float* pitch, GameState* gameState) {
    // Free old maze if exists
    if (*maze) {
        Maze_Destroy(*maze);
        *maze = NULL;
    }
    if (*walls) {
        free(*walls);
        *walls = NULL;
    }
    
    // Create and generate new maze
    *maze = Maze_Create(MAZE_WIDTH, MAZE_HEIGHT, CELL_SIZE);
    if (!*maze) {
        TraceLog(LOG_ERROR, "Failed to create maze!");
        return;
    }
    
    Maze_Generate(*maze);
    
    // Allocate wall rectangles
    int maxWalls = MAZE_WIDTH * MAZE_HEIGHT * 4; // Maximum possible walls
    *walls = (WallRect*)malloc(maxWalls * sizeof(WallRect));
    if (!*walls) {
        TraceLog(LOG_ERROR, "Failed to allocate wall rectangles!");
        return;
    }
    
    *wallCount = Maze_GetWallRects(*maze, *walls, maxWalls);
    
    // Reset player to start position
    Vector2 startWorld = Maze_CellToWorld(*maze, (int)(*maze)->startPos.x, (int)(*maze)->startPos.y);
    playerPos->x = startWorld.x;
    playerPos->y = 0.0f;
    playerPos->z = startWorld.y;
    
    *yaw = 0.0f;
    *pitch = 0.0f;
    *gameState = GAME_STATE_PLAYING;
}

// Render the maze in 3D
static void RenderMaze(const Maze* maze) {
    if (!maze) return;
    
    const float halfCell = maze->cellSize * 0.5f;
    const float wallHalfThick = WALL_THICK * 0.5f;
    const float wallHalfHeight = WALL_HEIGHT * 0.5f;
    
    Color wallColor = DARKGRAY;
    Color floorColor = (Color){200, 200, 200, 255};
    Color ceilingColor = (Color){170, 170, 170, 255};
    
    // Calculate maze bounds for floor/ceiling
    float mazeWidth = maze->width * maze->cellSize;
    float mazeHeight = maze->height * maze->cellSize;
    
    // Draw floor
    DrawPlane((Vector3){0, 0, 0}, (Vector2){mazeWidth, mazeHeight}, floorColor);
    
    // Draw ceiling
    DrawPlane((Vector3){0, WALL_HEIGHT, 0}, (Vector2){mazeWidth, mazeHeight}, ceilingColor);
    
    // Draw walls for each cell
    for (int y = 0; y < maze->height; y++) {
        for (int x = 0; x < maze->width; x++) {
            float worldX = (x - maze->width * 0.5f + 0.5f) * maze->cellSize;
            float worldZ = (y - maze->height * 0.5f + 0.5f) * maze->cellSize;
            
            // North wall
            if (Maze_HasWall(maze, x, y, MAZE_NORTH)) {
                DrawCube((Vector3){
                    worldX,
                    wallHalfHeight,
                    worldZ - halfCell
                }, maze->cellSize, WALL_HEIGHT, WALL_THICK, wallColor);
            }
            
            // South wall
            if (Maze_HasWall(maze, x, y, MAZE_SOUTH)) {
                DrawCube((Vector3){
                    worldX,
                    wallHalfHeight,
                    worldZ + halfCell
                }, maze->cellSize, WALL_HEIGHT, WALL_THICK, wallColor);
            }
            
            // West wall
            if (Maze_HasWall(maze, x, y, MAZE_WEST)) {
                DrawCube((Vector3){
                    worldX - halfCell,
                    wallHalfHeight,
                    worldZ
                }, WALL_THICK, WALL_HEIGHT, maze->cellSize, wallColor);
            }
            
            // East wall
            if (Maze_HasWall(maze, x, y, MAZE_EAST)) {
                DrawCube((Vector3){
                    worldX + halfCell,
                    wallHalfHeight,
                    worldZ
                }, WALL_THICK, WALL_HEIGHT, maze->cellSize, wallColor);
            }
        }
    }
    
    // Highlight exit cell (green floor)
    Vector2 exitWorld = Maze_CellToWorld(maze, (int)maze->exitPos.x, (int)maze->exitPos.y);
    DrawPlane((Vector3){exitWorld.x, 0.01f, exitWorld.y}, 
              (Vector2){maze->cellSize * 0.8f, maze->cellSize * 0.8f}, 
              (Color){0, 200, 0, 255});
}

int main(void) {
    // Initialize random seed
    srand((unsigned int)time(NULL));
    
    // Window setup
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(1280, 720, "3D Maze Game | WASD+mouse, Shift run, Space jump, F toggle mouse, R restart");
    SetTargetFPS(120);
    
    bool mouseCaptured = true;
    DisableCursor();
    
    // Game state
    Maze* maze = NULL;
    WallRect* walls = NULL;
    int wallCount = 0;
    GameState gameState = GAME_STATE_PLAYING;
    
    // Player state
    Vector3 playerPos = {0.0f, 0.0f, 0.0f};
    float playerVelY = 0.0f;
    bool onGround = true;
    
    float yaw = 0.0f;      // Horizontal rotation (0: looking +Z)
    float pitch = 0.0f;    // Vertical rotation
    const float PITCH_LIMIT = DEG2RAD * 89.0f;
    
    // Camera
    Camera3D cam = {0};
    cam.position = (Vector3){playerPos.x, playerPos.y + PLAYER_EYE_HEIGHT, playerPos.z};
    cam.target = (Vector3){0, 0, 1};
    cam.up = (Vector3){0, 1, 0};
    cam.fovy = 75.0f;
    cam.projection = CAMERA_PERSPECTIVE;
    
    // Initialize game
    InitGame(&maze, &walls, &wallCount, &playerPos, &yaw, &pitch, &gameState);
    
    // Main game loop
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        
        // Toggle mouse capture
        if (IsKeyPressed(KEY_F)) {
            mouseCaptured = !mouseCaptured;
            if (mouseCaptured) DisableCursor();
            else EnableCursor();
        }
        
        // Restart game
        if (IsKeyPressed(KEY_R)) {
            InitGame(&maze, &walls, &wallCount, &playerPos, &yaw, &pitch, &gameState);
        }
        
        // --- Mouse look (FPS) ---
        if (mouseCaptured && gameState == GAME_STATE_PLAYING) {
            Vector2 md = GetMouseDelta();
            yaw -= md.x * MOUSE_SENS;
            pitch -= md.y * MOUSE_SENS;
            if (pitch > PITCH_LIMIT) pitch = PITCH_LIMIT;
            if (pitch < -PITCH_LIMIT) pitch = -PITCH_LIMIT;
        }
        
        // Calculate forward and right vectors
        Vector3 forward = (Vector3){
            cosf(pitch) * sinf(yaw),
            sinf(pitch),
            cosf(pitch) * cosf(yaw)
        };
        Vector3 right = (Vector3){cosf(yaw), 0.0f, -sinf(yaw)};
        
        // --- Movement input (only when playing) ---
        if (gameState == GAME_STATE_PLAYING) {
            float speed = MOVE_SPEED * (IsKeyDown(KEY_LEFT_SHIFT) ? RUN_MULTIPLIER : 1.0f);
            Vector2 wish = (Vector2){0.0f, 0.0f}; // XZ move wish
            
            if (IsKeyDown(KEY_W)) {
                wish.x += forward.x;
                wish.y += forward.z;
            }
            if (IsKeyDown(KEY_S)) {
                wish.x -= forward.x;
                wish.y -= forward.z;
            }
            if (IsKeyDown(KEY_D)) {
                wish.x -= right.x;
                wish.y -= right.z;
            }
            if (IsKeyDown(KEY_A)) {
                wish.x += right.x;
                wish.y += right.z;
            }
            
            // Normalize wish direction
            float len = sqrtf(wish.x * wish.x + wish.y * wish.y);
            if (len > 0.0001f) {
                wish.x /= len;
                wish.y /= len;
            }
            
            // Try to move in X then Z with collision (slide along walls)
            Vector2 pXZ = (Vector2){playerPos.x, playerPos.z};
            Vector2 step = (Vector2){wish.x * speed * dt, wish.y * speed * dt};
            
            // X axis movement
            Vector2 testX = (Vector2){pXZ.x + step.x, pXZ.y};
            if (!CollidesAny(testX, PLAYER_RADIUS, walls, wallCount)) {
                pXZ.x = testX.x;
            }
            
            // Z axis movement
            Vector2 testZ = (Vector2){pXZ.x, pXZ.y + step.y};
            if (!CollidesAny(testZ, PLAYER_RADIUS, walls, wallCount)) {
                pXZ.y = testZ.y;
            }
            
            // Apply XZ back to 3D position
            playerPos.x = pXZ.x;
            playerPos.z = pXZ.y;
            
            // --- Jump + gravity ---
            onGround = (playerPos.y <= 0.0001f);
            if (onGround) {
                playerPos.y = 0.0f;
                playerVelY = 0.0f;
                if (IsKeyPressed(KEY_SPACE)) {
                    playerVelY = JUMP_SPEED;
                    onGround = false;
                }
            } else {
                playerVelY += GRAVITY * dt;
            }
            playerPos.y += playerVelY * dt;
            
            // Ceiling clamp
            float maxFeetY = WALL_HEIGHT - PLAYER_EYE_HEIGHT;
            if (playerPos.y > maxFeetY) {
                playerPos.y = maxFeetY;
                if (playerVelY > 0) playerVelY = 0;
            }
            
            // Check if player reached exit
            int cellX, cellY;
            Maze_WorldToCell(maze, playerPos.x, playerPos.z, &cellX, &cellY);
            if (Maze_IsExit(maze, cellX, cellY)) {
                gameState = GAME_STATE_WON;
            }
        }
        
        // Update camera from player
        cam.position = (Vector3){playerPos.x, playerPos.y + PLAYER_EYE_HEIGHT, playerPos.z};
        cam.target = (Vector3){
            cam.position.x + forward.x,
            cam.position.y + forward.y,
            cam.position.z + forward.z
        };
        
        // ----------- RENDER -----------
        BeginDrawing();
        ClearBackground((Color){24, 26, 29, 255});
        
        BeginMode3D(cam);
        
        // Render maze
        if (maze) {
            RenderMaze(maze);
        }
        
        EndMode3D();
        
        // Crosshair
        if (gameState == GAME_STATE_PLAYING) {
            int cx = GetScreenWidth() / 2, cy = GetScreenHeight() / 2;
            DrawLine(cx - 8, cy, cx + 8, cy, RAYWHITE);
            DrawLine(cx, cy - 8, cx, cy + 8, RAYWHITE);
        }
        
        // HUD
        if (gameState == GAME_STATE_PLAYING) {
            DrawText("WASD: move | Shift: run | Space: jump | F: toggle mouse | R: restart | Esc: quit",
                     20, 20, 18, RAYWHITE);
        } else if (gameState == GAME_STATE_WON) {
            // Win screen
            int screenWidth = GetScreenWidth();
            int screenHeight = GetScreenHeight();
            
            // Semi-transparent overlay
            DrawRectangle(0, 0, screenWidth, screenHeight, (Color){0, 0, 0, 200});
            
            // Win message
            const char* winText = "YOU WIN!";
            int fontSize = 60;
            int textWidth = MeasureText(winText, fontSize);
            DrawText(winText, (screenWidth - textWidth) / 2, screenHeight / 2 - 60, fontSize, GREEN);
            
            // Restart instruction
            const char* restartText = "Press R to restart or Esc to quit";
            fontSize = 24;
            textWidth = MeasureText(restartText, fontSize);
            DrawText(restartText, (screenWidth - textWidth) / 2, screenHeight / 2 + 20, fontSize, RAYWHITE);
        }
        
        EndDrawing();
    }
    
    // Cleanup
    if (maze) Maze_Destroy(maze);
    if (walls) free(walls);
    
    CloseWindow();
    return 0;
}
