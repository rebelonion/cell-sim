#include <random>

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

        if (IsKeyPressed(KEY_SPACE)) genNewOctahedra = !genNewOctahedra;
        if (genNewOctahedra) {
            octaManager.trySpawningNewOctahedra(deltaTime);
        }

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

            DrawFPS(10, 10);
            DrawText(TextFormat("Octahedra: %zu", octaManager.getCount()), 10, 30, 20, BLACK);
            if (genNewOctahedra) {
                DrawText("Generating new octahedra...", 10, 50, 20, GREEN);
            } else {
                DrawText("Press SPACE to generate new octahedra", 10, 50, 20, DARKGRAY);
            }
        }
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
