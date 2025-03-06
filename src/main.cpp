#include <random>
#include <unordered_map>
#include <tuple>

#include "raylib.h"
#include "raymath.h"
#include "MeshGenerator.h"
#include "TruncatedOctahedraManager.h"
#define RLIGHTS_IMPLEMENTATION
#include "rlights.h"

#if defined(PLATFORM_DESKTOP)
    #define GLSL_VERSION            330
#else   // PLATFORM_ANDROID, PLATFORM_WEB
#define GLSL_VERSION            100
#endif

// Structure to hold debug statistics
struct DebugStats {
    int frameCounter = 0;
    std::unordered_map<int, int> neighborStats;
    std::pair<size_t, size_t> placementStats;
    std::pair<size_t, size_t> availabilityStats;
    std::tuple<size_t, size_t, size_t, double> hashStats;
};

// Function to draw debug information
void DrawDebugInfo(const TruncatedOctahedraManager& octaManager, bool genNewOctahedra, DebugStats& stats) {
    // Update statistics every 60 frames to avoid performance impact
    if (stats.frameCounter++ % 60 == 0) {
        stats.neighborStats = octaManager.getNeighborStats();
        stats.placementStats = octaManager.getPlacementStats();
        stats.availabilityStats = octaManager.getAvailabilityStats();
        stats.hashStats = octaManager.getHashStats();
    }

    // Basic information always displayed
    DrawFPS(10, 10);
    DrawText(TextFormat("Octahedra: %zu", octaManager.getCount()), 10, 30, 20, BLACK);
    if (genNewOctahedra) {
        DrawText("Generating new octahedra...", 10, 50, 20, GREEN);
    } else {
        DrawText("Press SPACE to generate new octahedra", 10, 50, 20, DARKGRAY);
    }
    // Display hash table statistics to monitor performance
    size_t nonEmptyBuckets = std::get<0>(stats.hashStats);
    size_t maxBucketSize = std::get<1>(stats.hashStats);
    size_t totalItems = std::get<2>(stats.hashStats);
    double avgBucketSize = std::get<3>(stats.hashStats);
    
    Color hashColor = maxBucketSize > 20 ? RED : DARKBLUE; // Warn if buckets getting too large
    
    DrawText("Hash Table Stats:", 300, 80, 20, BLACK);
    DrawText(TextFormat("Cells: %zu | Buckets: %zu/%zu", 
                totalItems, nonEmptyBuckets, OctahedronGrid::HASH_TABLE_SIZE),
             300, 100, 18, DARKBLUE);
    DrawText(TextFormat("Avg bucket: %.2f | Max bucket: %zu", 
                avgBucketSize, maxBucketSize),
             300, 120, 18, hashColor);

    // Display placement statistics
    size_t totalPlacements = stats.placementStats.first + stats.placementStats.second;
    float squarePercent = totalPlacements > 0 ?
                         100.0f * static_cast<float>(stats.placementStats.first) / static_cast<float>(totalPlacements) : 0.0f;
    float hexagonPercent = totalPlacements > 0 ?
                          100.0f * static_cast<float>(stats.placementStats.second) / static_cast<float>(totalPlacements) : 0.0f;

    DrawText("Placement Statistics:", 10, 140, 20, BLACK);
    DrawText(TextFormat("Square faces: %zu (%.1f%%)", stats.placementStats.first, squarePercent),
             10, 160, 18, DARKBLUE);
    DrawText(TextFormat("Hexagon faces: %zu (%.1f%%)", stats.placementStats.second, hexagonPercent),
             10, 180, 18, DARKBLUE);

    // Display availability statistics
    size_t totalAvailable = stats.availabilityStats.first + stats.availabilityStats.second;
    float squareAvailPercent = totalAvailable > 0 ?
                              100.0f * static_cast<float>(stats.availabilityStats.first) / static_cast<float>(totalAvailable) : 0.0f;
    float hexagonAvailPercent = totalAvailable > 0 ?
                               100.0f * static_cast<float>(stats.availabilityStats.second) / static_cast<float>(totalAvailable) : 0.0f;

    DrawText("Availability Statistics:", 10, 200, 20, BLACK);
    DrawText(TextFormat("Square faces available: %zu (%.1f%%)", stats.availabilityStats.first, squareAvailPercent),
             10, 220, 18, MAROON);
    DrawText(TextFormat("Hexagon faces available: %zu (%.1f%%)", stats.availabilityStats.second, hexagonAvailPercent),
             10, 240, 18, MAROON);

    // Display neighbor statistics
    DrawText("Neighbor Statistics:", 10, 270, 20, BLACK);
    int yPos = 290;
    for (int i = 0; i <= 14; i++) {
        if (stats.neighborStats.find(i) != stats.neighborStats.end()) {
            DrawText(TextFormat("%d neighbors: %d cells", i, stats.neighborStats[i]),
                     10, yPos, 18, DARKBLUE);
            yPos += 20;
        }
    }

    // UI instructions
    DrawText("Press 1-5 to resize grid (1=64³, 2=128³, 3=256³, 4=384³, 5=512³)",
             10, GetScreenHeight() - 70, 18, DARKGRAY);
    DrawText("Press R to reset placement statistics",
             10, GetScreenHeight() - 50, 18, DARKGRAY);
    DrawText("Press D to toggle debug information",
             10, GetScreenHeight() - 30, 18, DARKGRAY);
}

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
            CreateLight(LIGHT_POINT, (Vector3){-lightRadius, lightHeight, -lightRadius}, Vector3Zero(), DARKPURPLE, shader);
    lights[1] =
            CreateLight(LIGHT_POINT, (Vector3){lightRadius, lightHeight, lightRadius}, Vector3Zero(), DARKBLUE, shader);
    lights[2] =
            CreateLight(LIGHT_POINT, (Vector3){-lightRadius, lightHeight, lightRadius}, Vector3Zero(), DARKGREEN, shader);
    lights[3] =
            CreateLight(LIGHT_POINT, (Vector3){lightRadius, lightHeight, -lightRadius}, Vector3Zero(), DARKBLUE, shader);

    Camera3D camera = {};
    camera.position = Vector3{400.0f, 400.0f, 400.0f};
    camera.target = Vector3{0.0f, 0.0f, 0.0f};
    camera.up = Vector3{0.0f, 1.0f, 0.0f};
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // Create instancing-compatible material
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

    const float LIGHT_ROTATION_SPEED = 0.5f;

    bool genNewOctahedra = false;
    bool showDebugInfo = true;  // Toggle for debug information display
    DebugStats debugStats;      // Stats for debug display

    SetTargetFPS(60);
    while (!WindowShouldClose()) {
        const float deltaTime = GetFrameTime();

        rotationAngle += LIGHT_ROTATION_SPEED * deltaTime;

        lights[0].position = (Vector3){
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
        };

        // Toggle cell spawning
        if (IsKeyPressed(KEY_SPACE)) genNewOctahedra = !genNewOctahedra;
        if (genNewOctahedra) {
            octaManager.trySpawningNewOctahedra(deltaTime);
        }
        
        // Toggle debug info display
        if (IsKeyPressed(KEY_D)) showDebugInfo = !showDebugInfo;

        UpdateCamera(&camera, CAMERA_ORBITAL);

        const float cameraPos[3] = {camera.position.x, camera.position.y, camera.position.z};
        SetShaderValue(shader, shader.locs[SHADER_LOC_VECTOR_VIEW], cameraPos, SHADER_UNIFORM_VEC3);

        for (const auto &light: lights) UpdateLightValues(shader, light);

        BeginDrawing(); {
            ClearBackground(GRAY);
            BeginMode3D(camera); {
                octaManager.draw();
            }
            EndMode3D();

            // Draw basic info or full debug info based on toggle
            if (showDebugInfo) {
                DrawDebugInfo(octaManager, genNewOctahedra, debugStats);
            } else {
                // Show minimal information when debug is off
                DrawFPS(10, 10);
                DrawText(TextFormat("Octahedra: %zu", octaManager.getCount()), 10, 30, 20, BLACK);
                if (genNewOctahedra) {
                    DrawText("Generating new octahedra...", 10, 50, 20, GREEN);
                } else {
                    DrawText("Press SPACE to generate new octahedra", 10, 50, 20, DARKGRAY);
                }
                DrawText("Press D to toggle debug information", 10, GetScreenHeight() - 30, 18, DARKGRAY);
            }
        }
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
