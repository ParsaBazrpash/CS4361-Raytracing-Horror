#include "raylib.h"
#include <math.h>
#include <stdbool.h>

// ---------- Tunables ----------
#define ROOM_SIZE            20.0f   // interior size along X/Z
#define WALL_THICK           0.5f
#define WALL_HEIGHT          4.0f

#define PLAYER_RADIUS        0.30f   // collision radius (XZ)
#define PLAYER_EYE_HEIGHT    1.80f   // camera height above "feet"
#define GRAVITY             -18.0f
#define JUMP_SPEED           6.5f
#define MOVE_SPEED           5.0f
#define RUN_MULTIPLIER       1.8f
#define MOUSE_SENS           0.0020f // radians per pixel

static inline float clampf(float v, float lo, float hi) { return (v < lo) ? lo : (v > hi) ? hi : v; }

// Circle (player) vs axis-aligned rectangle (wall/obstacle) in XZ
static bool CircleRectIntersect(Vector2 c, float r, Rectangle rect)
{
    float nx = clampf(c.x, rect.x, rect.x + rect.width);
    float nz = clampf(c.y, rect.y, rect.y + rect.height);
    float dx = c.x - nx;
    float dz = c.y - nz;
    return (dx*dx + dz*dz) <= r*r;
}

static bool CollidesAny(Vector2 c, float r, const Rectangle* rects, int count)
{
    for (int i = 0; i < count; ++i)
        if (CircleRectIntersect(c, r, rects[i])) return true;
    return false;
}

int main(void)
{
    // Window + input
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(1280, 720, "FPS room | WASD+mouse, Shift run, Space jump, F toggle mouse");
    SetTargetFPS(120);

    bool mouseCaptured = true;
    DisableCursor(); // capture mouse to enable FPS look

    // Room geometry (collision rectangles in XZ)
    const float half = ROOM_SIZE * 0.5f;

    Rectangle colliders[5];
    // Left wall
    colliders[0] = (Rectangle){ -half - WALL_THICK, -half - WALL_THICK, WALL_THICK, ROOM_SIZE + 2*WALL_THICK };
    // Right wall
    colliders[1] = (Rectangle){  half,              -half - WALL_THICK, WALL_THICK, ROOM_SIZE + 2*WALL_THICK };
    // North wall
    colliders[2] = (Rectangle){ -half - WALL_THICK, -half - WALL_THICK, ROOM_SIZE + 2*WALL_THICK, WALL_THICK };
    // South wall
    colliders[3] = (Rectangle){ -half - WALL_THICK,  half,               ROOM_SIZE + 2*WALL_THICK, WALL_THICK };
    // Center obstacle (2x2 cube footprint)
    colliders[4] = (Rectangle){ -1.0f, -1.0f, 2.0f, 2.0f };
    const int colliderCount = 5;

    // Player state (track "feet" Y and separate yaw/pitch)
    Vector3 playerPos = (Vector3){ 0.0f, 0.0f, -5.0f }; // start near south wall
    float   playerVelY = 0.0f;
    bool    onGround   = true;

    float yaw   = 0.0f;     // 0: looking +Z
    float pitch = 0.0f;     // up/down
    const float PITCH_LIMIT = DEG2RAD*89.0f;

    // Camera (we'll update position/target every frame)
    Camera3D cam = { 0 };
    cam.position   = (Vector3){ playerPos.x, playerPos.y + PLAYER_EYE_HEIGHT, playerPos.z };
    cam.target     = (Vector3){ 0, 0, 1 };
    cam.up         = (Vector3){ 0, 1, 0 };
    cam.fovy       = 75.0f;
    cam.projection = CAMERA_PERSPECTIVE;

    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();

        // Toggle mouse capture
        if (IsKeyPressed(KEY_F)) {
            mouseCaptured = !mouseCaptured;
            if (mouseCaptured) DisableCursor(); else EnableCursor();
        }

        // --- Mouse look (FPS) ---
        if (mouseCaptured) {
            Vector2 md = GetMouseDelta();
            yaw   -= md.x * MOUSE_SENS;
            pitch -= md.y * MOUSE_SENS;
            if (pitch >  PITCH_LIMIT) pitch =  PITCH_LIMIT;
            if (pitch < -PITCH_LIMIT) pitch = -PITCH_LIMIT;
        }

        // Forward (unit) from yaw/pitch, right vector in XZ
        Vector3 forward = (Vector3){
            cosf(pitch) * sinf(yaw),
            sinf(pitch),
            cosf(pitch) * cosf(yaw)
        };
        Vector3 right = (Vector3){ cosf(yaw), 0.0f, -sinf(yaw) };

        // --- Movement input ---
        float speed = MOVE_SPEED * (IsKeyDown(KEY_LEFT_SHIFT) ? RUN_MULTIPLIER : 1.0f);
        Vector2 wish = (Vector2){ 0.0f, 0.0f }; // XZ move wish

        if (IsKeyDown(KEY_W)) { wish.x += forward.x; wish.y += forward.z; }
        if (IsKeyDown(KEY_S)) { wish.x -= forward.x; wish.y -= forward.z; }

        // FIX: D = right, A = left (swapped signs compared to previous)
        if (IsKeyDown(KEY_D)) { wish.x -= right.x; wish.y -= right.z; } // move right
        if (IsKeyDown(KEY_A)) { wish.x += right.x; wish.y += right.z; } // move left

        // Normalize wish (XZ)
        float len = sqrtf(wish.x*wish.x + wish.y*wish.y);
        if (len > 0.0001f) { wish.x /= len; wish.y /= len; }

        // Try to move in X then Z with collision (slide along walls)
        Vector2 pXZ = (Vector2){ playerPos.x, playerPos.z };
        Vector2 step = (Vector2){ wish.x * speed * dt, wish.y * speed * dt };

        // X axis
        Vector2 testX = (Vector2){ pXZ.x + step.x, pXZ.y };
        if (!CollidesAny(testX, PLAYER_RADIUS, colliders, colliderCount)) {
            pXZ.x = testX.x;
        }
        // Z axis
        Vector2 testZ = (Vector2){ pXZ.x, pXZ.y + step.y };
        if (!CollidesAny(testZ, PLAYER_RADIUS, colliders, colliderCount)) {
            pXZ.y = testZ.y;
        }

        // Apply XZ back to 3D pos
        playerPos.x = pXZ.x;
        playerPos.z = pXZ.y;

        // --- Jump + gravity ---
        onGround = (playerPos.y <= 0.0001f);
        if (onGround) {
            playerPos.y = 0.0f;
            playerVelY  = 0.0f;
            if (IsKeyPressed(KEY_SPACE)) {
                playerVelY = JUMP_SPEED;
                onGround = false;
            }
        } else {
            playerVelY += GRAVITY * dt;
        }
        playerPos.y += playerVelY * dt;

        // Ceiling clamp (so you can't clip through the ceiling)
        float maxFeetY = WALL_HEIGHT - PLAYER_EYE_HEIGHT; // keep eyes below ceiling
        if (playerPos.y > maxFeetY) {
            playerPos.y = maxFeetY;
            if (playerVelY > 0) playerVelY = 0;
        }

        // Update camera from player
        cam.position = (Vector3){ playerPos.x, playerPos.y + PLAYER_EYE_HEIGHT, playerPos.z };
        cam.target   = (Vector3){
            cam.position.x + forward.x,
            cam.position.y + forward.y,
            cam.position.z + forward.z
        };

        // ----------- RENDER -----------
        BeginDrawing();
        ClearBackground((Color){ 24, 26, 29, 255 });

        BeginMode3D(cam);
        // Floor + ceiling
        DrawPlane((Vector3){0, 0, 0},           (Vector2){ ROOM_SIZE, ROOM_SIZE }, (Color){ 200, 200, 200, 255 });
        DrawPlane((Vector3){0, WALL_HEIGHT, 0}, (Vector2){ ROOM_SIZE, ROOM_SIZE }, (Color){ 170, 170, 170, 255 });

        // Walls (as cubes)
        DrawCube((Vector3){ -half - WALL_THICK*0.5f, WALL_HEIGHT*0.5f, 0.0f }, WALL_THICK, WALL_HEIGHT, ROOM_SIZE + 2*WALL_THICK, DARKGRAY); // Left
        DrawCube((Vector3){  half + WALL_THICK*0.5f, WALL_HEIGHT*0.5f, 0.0f }, WALL_THICK, WALL_HEIGHT, ROOM_SIZE + 2*WALL_THICK, DARKGRAY); // Right
        DrawCube((Vector3){ 0.0f, WALL_HEIGHT*0.5f, -half - WALL_THICK*0.5f }, ROOM_SIZE + 2*WALL_THICK, WALL_HEIGHT, WALL_THICK, DARKGRAY); // North
        DrawCube((Vector3){ 0.0f, WALL_HEIGHT*0.5f,  half + WALL_THICK*0.5f }, ROOM_SIZE + 2*WALL_THICK, WALL_HEIGHT, WALL_THICK, DARKGRAY); // South

        // Center obstacle cube
        DrawCube((Vector3){ 0.0f, 1.0f, 0.0f }, 2.0f, 2.0f, 2.0f, BROWN);
        DrawCubeWires((Vector3){ 0.0f, 1.0f, 0.0f }, 2.0f, 2.0f, 2.0f, BLACK);

        EndMode3D();

        // Crosshair
        int cx = GetScreenWidth()/2, cy = GetScreenHeight()/2;
        DrawLine(cx - 8, cy, cx + 8, cy, RAYWHITE);
        DrawLine(cx, cy - 8, cx, cy + 8, RAYWHITE);

        // HUD
        DrawText("WASD: move | Shift: run | Space: jump | F: toggle mouse | Esc: quit",
                 20, 20, 18, RAYWHITE);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
