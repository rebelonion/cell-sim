#include <random>
#include <unordered_map>
#include <tuple>

#include "raylib.h"
#include "raymath.h"
#include "MeshGenerator.h"
#include "TruncatedOctahedraManager.h"
#include "BoundaryManager.h"
#define RLIGHTS_IMPLEMENTATION
#include "rlights.h"

#if defined(PLATFORM_DESKTOP)
#define GLSL_VERSION            330
#else   // PLATFORM_ANDROID, PLATFORM_WEB
#define GLSL_VERSION            100
#endif


int main() {
    constexpr int screenWidth = 800 * 2;
    constexpr int screenHeight = 450 * 2;
    InitWindow(screenWidth, screenHeight, "3D Outline Shader");

    // Load and configure the instancing shader
    const Shader shader = LoadShader(TextFormat("../data/shaders/lighting_instancing.vs", GLSL_VERSION),
                                     TextFormat("../data/shaders/lighting.fs", GLSL_VERSION));

    shader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(shader, "mvp");
    shader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(shader, "viewPos");
    shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocationAttrib(shader, "instanceTransform");

    const int ambientLoc = GetShaderLocation(shader, "ambient");
    SetShaderValue(shader, ambientLoc, (float[4]){0.2f, 0.2f, 0.2f, 1.0f}, SHADER_UNIFORM_VEC4);

    Light lights[MAX_LIGHTS] = {0};
    constexpr float lightRadius = 400.0f;
    constexpr float lightHeight = 3.0f;
    float rotationAngle = 0.0f;

    lights[0] =
            CreateLight(LIGHT_POINT, (Vector3){-lightRadius, lightHeight, -lightRadius}, Vector3Zero(), WHITE, shader);
    lights[1] =
            CreateLight(LIGHT_POINT, (Vector3){lightRadius, lightHeight, lightRadius}, Vector3Zero(), WHITE, shader);
    lights[2] =
            CreateLight(LIGHT_POINT, (Vector3){-lightRadius, lightHeight, lightRadius}, Vector3Zero(), WHITE, shader);
    lights[3] =
            CreateLight(LIGHT_POINT, (Vector3){lightRadius, lightHeight, -lightRadius}, Vector3Zero(), WHITE, shader);

    Camera3D camera = {};
    camera.position = Vector3{200.0f, 200.0f, 200.0f};
    camera.target = Vector3{0.0f, 0.0f, 0.0f};
    camera.up = Vector3{0.0f, 1.0f, 0.0f};
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    
    DisableCursor();    // Limit cursor to relative movement inside the window

    Material material = LoadMaterialDefault();
    material.shader = shader;
    material.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
    material.maps[MATERIAL_MAP_SPECULAR].color = WHITE;
    constexpr float shininess = 0.0f;
    material.maps[MATERIAL_MAP_SPECULAR].value = 0.5f;
    material.maps[MATERIAL_MAP_DIFFUSE].value = 1.0f;
    const int shininessLoc = GetShaderLocation(shader, "shininess");
    SetShaderValue(shader, shininessLoc, &shininess, SHADER_UNIFORM_FLOAT);

    const Model model = LoadModelFromMesh(MeshGenerator::genTruncatedOctahedron());
    model.materials[0] = material;

    TruncatedOctahedraManager octaManager(model, material);
    // The TruncatedOctahedraManager now creates its own BoundaryManager internally

    const float LIGHT_ROTATION_SPEED = 0.5f;

    SetTargetFPS(60);
    bool generationActive = false;
    while (!WindowShouldClose()) {
        const float deltaTime = GetFrameTime();

        rotationAngle += LIGHT_ROTATION_SPEED * deltaTime;

        /*lights[0].position = (Vector3){
            -lightRadius * cosf(rotationAngle),
            lightHeight,
            -lightRadius * sinf(rotationAngle)
        };
        lights[1].position = (Vector3){
            lightRadius * cosf(rotationAngle + PI),
            lightHeight,
            lightRadius * sinf(rotationAngle + PI)
        };
        lights[2].position = (Vector3){
            -lightRadius * cosf(rotationAngle + PI / 2),
            lightHeight,
            lightRadius * sinf(rotationAngle + PI / 2)
        };
        lights[3].position = (Vector3){
            lightRadius * cosf(rotationAngle - PI / 2),
            lightHeight,
            -lightRadius * sinf(rotationAngle - PI / 2)
        };*/

        // Toggle cell generation thread
        if (IsKeyPressed(KEY_SPACE)) {
            if (octaManager.isGenerationActive()) {
                octaManager.stopGenerationThread();
            } else {
                octaManager.startGenerationThread();
            }
        }

        if (octaManager.isGenerationActive()) {
            octaManager.applyPendingChanges();
        }

        if (IsKeyPressed(KEY_V)) {
            octaManager.updateVisibility();
        }

        if (IsKeyPressed(KEY_B)) {
            octaManager.toggleBoundaryVisibility();
        }

        if (IsKeyPressed(KEY_N)) {
            octaManager.generateRandomBoundary();
        }

        if (IsKeyPressed(KEY_E)) {
            octaManager.toggleBoundaryEnabled();
        }

        // Update camera with free mode
        UpdateCamera(&camera, CAMERA_FREE);
        
        // WASD movement for camera position
        const float moveSpeed = 100.0f * deltaTime;
        if (IsKeyDown(KEY_W)) {
            Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
            camera.position = Vector3Add(camera.position, Vector3Scale(forward, moveSpeed));
            camera.target = Vector3Add(camera.target, Vector3Scale(forward, moveSpeed));
        }
        if (IsKeyDown(KEY_S)) {
            Vector3 backward = Vector3Normalize(Vector3Subtract(camera.position, camera.target));
            camera.position = Vector3Add(camera.position, Vector3Scale(backward, moveSpeed));
            camera.target = Vector3Add(camera.target, Vector3Scale(backward, moveSpeed));
        }
        if (IsKeyDown(KEY_A)) {
            Vector3 right = Vector3CrossProduct(Vector3Subtract(camera.target, camera.position), camera.up);
            right = Vector3Normalize(right);
            camera.position = Vector3Subtract(camera.position, Vector3Scale(right, moveSpeed));
            camera.target = Vector3Subtract(camera.target, Vector3Scale(right, moveSpeed));
        }
        if (IsKeyDown(KEY_D)) {
            Vector3 right = Vector3CrossProduct(Vector3Subtract(camera.target, camera.position), camera.up);
            right = Vector3Normalize(right);
            camera.position = Vector3Add(camera.position, Vector3Scale(right, moveSpeed));
            camera.target = Vector3Add(camera.target, Vector3Scale(right, moveSpeed));
        }

        const float cameraPos[3] = {camera.position.x, camera.position.y, camera.position.z};
        SetShaderValue(shader, shader.locs[SHADER_LOC_VECTOR_VIEW], cameraPos, SHADER_UNIFORM_VEC3);

        //for (const auto &light: lights) UpdateLightValues(shader, light);

        BeginDrawing(); {
            ClearBackground(GRAY);
            BeginMode3D(camera); {
                octaManager.draw();
            }
            EndMode3D();


            // Show minimal information when debug is off
            DrawFPS(10, 10);
            DrawText(TextFormat("Octahedra: %zu", octaManager.getCount()), 10, 30, 20, BLACK);

            if (octaManager.isGenerationActive()) {
                DrawText("Generating new octahedra (background)...", 10, 70, 20, GREEN);
            } else {
                DrawText("Press SPACE to toggle generation", 10, 70, 20, DARKGRAY);
            }

            // Show boundary constraint status
            const char *boundaryStatus = octaManager.isBoundaryEnabled() ? "ENABLED" : "DISABLED";
            DrawText(TextFormat("Boundary constraint: %s", boundaryStatus),
                     10, 90, 20, octaManager.isBoundaryEnabled() ? GREEN : GRAY);
            DrawText("Press R to reset entire simulation", 10, GetScreenHeight() - 120, 18, DARKGRAY);
            DrawText("Press D to toggle debug information", 10, GetScreenHeight() - 100, 18, DARKGRAY);
            DrawText("Press V to update visibility calculations", 10, GetScreenHeight() - 80, 18, DARKGRAY);
            DrawText("Press B to toggle boundary visibility", 10, GetScreenHeight() - 60, 18, DARKGRAY);
            DrawText("Press N to generate a new random boundary", 10, GetScreenHeight() - 40, 18, DARKGRAY);
            DrawText("Press E to toggle boundary constraint", 10, GetScreenHeight() - 20, 18, DARKGRAY);
        }
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
