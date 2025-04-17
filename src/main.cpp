#include <random>
#include <unordered_map>
#include <tuple>
#include <cmath>
#include <algorithm>

#include "raylib.h"
#include "raymath.h"
#include "MeshGenerator.h"
#include "TruncatedOctahedraManager.h"
#include "BoundaryManager.h"
#define RLIGHTS_IMPLEMENTATION
#include "rlgl.h"
#include "rlights.h"

// Implementation of the Stem Cell GUI

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#define STEMCELL_GUI_IMPLEMENTATION
#include "StemCellGUI.h"

#if defined(PLATFORM_DESKTOP)
#define GLSL_VERSION            330
#else   // PLATFORM_ANDROID, PLATFORM_WEB
#define GLSL_VERSION            100
#endif

// Constants for unit conversion
// An octahedron is 0.0866mm wide, and in our 3D world it's 2.0f * 2.82842712475f
constexpr float OCTAHEDRON_REAL_SIZE_MM = 0.0866f;
constexpr float OCTAHEDRON_WORLD_SIZE = 2.0f * 2.82842712475f;
constexpr float MM_TO_WORLD_SCALE = OCTAHEDRON_WORLD_SIZE / OCTAHEDRON_REAL_SIZE_MM;


float calculateOptimalSpacing(
    const float targetSimTime,
    const float cellSplitTime,
    const float completionPercent,
    const float spawnChance
) {
    const float effectiveCellSplitTime = cellSplitTime / spawnChance;
    const float distance = (targetSimTime / effectiveCellSplitTime);
    return std::max(OCTAHEDRON_WORLD_SIZE, distance);
}

int main() {
    constexpr int screenWidth = 800 * 2;
    constexpr int screenHeight = 450 * 2;
    InitWindow(screenWidth, screenHeight, "Stem Cell Simulator");

    // Initialize the GUI
    StemCellGUIState guiState = InitStemCellGUI();

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
    camera.position = Vector3{-200.0f, 400.0f, -200.0f};
    camera.target = Vector3{200.0f, 120.0f, 200.0f};
    camera.up = Vector3{0.0f, 1.0f, 0.0f};
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    
    // Don't disable cursor for GUI interaction
    // DisableCursor();
    rlFrustum(100000.0f, 100000.0f, 100000.0f, 100000.0f, 100000.0f, 100000.0f);
    rlSetClipPlanes(0.1f, 10000.0f);

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
    auto boundaryManager = octaManager.getBoundaryManager();

    const float LIGHT_ROTATION_SPEED = 0.5f;

    // Time tracking variables
    bool simulationRunning = false;
    float simulationProgress = 0.0f;
    bool freeCameraMode = false;

    // Initialize GUI values based on initial boundary size
    float worldToMm = 1.0f / MM_TO_WORLD_SCALE;
    guiState.lengthValue = static_cast<int>(boundaryManager->getBoundaryWidth() * worldToMm);
    guiState.widthValue = static_cast<int>(boundaryManager->getBoundaryDepth() * worldToMm);
    guiState.layerSpinnerValue = octaManager.getOctahedraLayers();

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

        // Only allow changes when simulation is not running
        if (!simulationRunning) {
            static int lastLengthValue = guiState.lengthValue;
            static int lastWidthValue = guiState.widthValue;
            static int lastLayerValue = guiState.layerSpinnerValue;
            static int lastCellSplitValue = guiState.cellSplitSpinnerValue;
            static int lastSimTimeValue = guiState.simulationTimeSpinnerValue;
            static int lastCompletedAtValue = guiState.completedAtSpinnerValue;
            float newWidth = guiState.lengthValue * MM_TO_WORLD_SCALE;
            float newDepth = guiState.widthValue * MM_TO_WORLD_SCALE;
            float octahedronHeight = OCTAHEDRON_REAL_SIZE_MM * MM_TO_WORLD_SCALE;
            float newHeight = 2.0f * octahedronHeight * guiState.layerSpinnerValue;

            bool sizeChanged = false;
            if (newWidth != boundaryManager->getBoundaryWidth()) {
                boundaryManager->setBoundaryWidth(newWidth);
                sizeChanged = true;
            }
            if (newDepth != boundaryManager->getBoundaryDepth()) {
                boundaryManager->setBoundaryDepth(newDepth);
                sizeChanged = true;
            }
            if (newHeight != boundaryManager->getBoundaryHeight()) {
                boundaryManager->setBoundaryHeight(newHeight);
                sizeChanged = true;
            }

            if (guiState.layerSpinnerValue != octaManager.getOctahedraLayers()) {
                octaManager.setOctahedraLayers(guiState.layerSpinnerValue);
                sizeChanged = true;
            }

            if (sizeChanged) {
                octaManager.generateStartingPositions();
            }

            //leave always visible for now
            //boundaryManager->setBoundaryVisible(guiState.debugCheckBoxChecked);

            bool paramsChanged = 
                lastLengthValue != guiState.lengthValue ||
                lastWidthValue != guiState.widthValue ||
                lastLayerValue != guiState.layerSpinnerValue ||
                lastCellSplitValue != guiState.cellSplitSpinnerValue ||
                lastSimTimeValue != guiState.simulationTimeSpinnerValue ||
                lastCompletedAtValue != guiState.completedAtSpinnerValue;

            if (paramsChanged || sizeChanged) {
                float spawnChance = octaManager.getSpawnChance();
                float spacing = calculateOptimalSpacing(
                    static_cast<float>(guiState.simulationTimeSpinnerValue),
                    static_cast<float>(guiState.cellSplitSpinnerValue),
                    static_cast<float>(guiState.completedAtSpinnerValue) / 100.0f,
                    spawnChance
                );

                octaManager.setOctahedraSpacing(spacing);
                lastLengthValue = guiState.lengthValue;
                lastWidthValue = guiState.widthValue;
                lastLayerValue = guiState.layerSpinnerValue;
                lastCellSplitValue = guiState.cellSplitSpinnerValue;
                lastSimTimeValue = guiState.simulationTimeSpinnerValue;
                lastCompletedAtValue = guiState.completedAtSpinnerValue;
            }
        }
        
        // Start button pressed
        if (GuiButton(guiState.layoutRecs[3], "Start") && !simulationRunning) {
            simulationRunning = true;
            simulationProgress = 0.0f;
            float spawnChance = octaManager.getSpawnChance();
            float spacing = calculateOptimalSpacing(
                static_cast<float>(guiState.simulationTimeSpinnerValue),
                static_cast<float>(guiState.cellSplitSpinnerValue),
                static_cast<float>(guiState.completedAtSpinnerValue) / 100.0f,
                spawnChance
            );

            octaManager.setOctahedraSpacing(spacing);
            auto simulationTickCallback = [&]() {
                size_t cellCount = octaManager.getCount();
                float boundaryVolume = boundaryManager->getBoundaryWidth() * 
                                      boundaryManager->getBoundaryDepth() * 
                                      boundaryManager->getBoundaryHeight();
                
                float cellVolume = OCTAHEDRON_WORLD_SIZE * OCTAHEDRON_WORLD_SIZE * OCTAHEDRON_WORLD_SIZE;
                float maxPossibleCells = boundaryVolume / cellVolume;
                size_t targetCellCount = maxPossibleCells * (guiState.completedAtSpinnerValue / 100.0f);
                if (targetCellCount > 0) {
                    simulationProgress = (static_cast<float>(cellCount) / (static_cast<float>(targetCellCount) * 2))
                                         * (static_cast<float>(guiState.simulationTimeSpinnerValue) / 100.0f);
                    simulationProgress = std::min(simulationProgress, 1.0f);
                    guiState.progressBarValue = simulationProgress;
                    if (simulationProgress >= 1.0f && simulationRunning) {
                        simulationRunning = false;
                        octaManager.stopGenerationThread();
                        strcpy(guiState.progressLabelText, "Progress: Complete!");
                    }
                }
            };

            octaManager.startGenerationThread(simulationTickCallback);
        }
        
        // Reset button pressed
        if (GuiButton(guiState.layoutRecs[2], "Reset")) {
            simulationRunning = false;
            simulationProgress = 0.0f;
            guiState.progressBarValue = 0.0f;
            strcpy(guiState.progressLabelText, "Progress: Not Started");
            if (octaManager.isGenerationActive()) {
                octaManager.stopGenerationThread();
            }
            octaManager.resetOctahedra();
        }

        // Toggle free camera mode with Tab key
        if (IsKeyPressed(KEY_TAB)) {
            freeCameraMode = !freeCameraMode;
            if (freeCameraMode) {
                DisableCursor();
                SetMousePosition(0, 0);
            } else {
                EnableCursor();
            }
        }

        // Update camera only in free mode
        if (freeCameraMode) {
            UpdateCamera(&camera, CAMERA_FREE);
            const float moveSpeed = 100.0f * deltaTime;
            if (IsKeyDown(KEY_SPACE)) {
                camera.target = Vector3{200.0f, 120.0f, 200.0f};
            }
            if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) {
                Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
                camera.position = Vector3Add(camera.position, Vector3Scale(forward, moveSpeed));
                camera.target = Vector3Add(camera.target, Vector3Scale(forward, moveSpeed));
            }
            if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) {
                Vector3 backward = Vector3Normalize(Vector3Subtract(camera.position, camera.target));
                camera.position = Vector3Add(camera.position, Vector3Scale(backward, moveSpeed));
                camera.target = Vector3Add(camera.target, Vector3Scale(backward, moveSpeed));
            }
            if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) {
                Vector3 right = Vector3CrossProduct(Vector3Subtract(camera.target, camera.position), camera.up);
                right = Vector3Normalize(right);
                camera.position = Vector3Subtract(camera.position, Vector3Scale(right, moveSpeed));
                camera.target = Vector3Subtract(camera.target, Vector3Scale(right, moveSpeed));
            }
            if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) {
                Vector3 right = Vector3CrossProduct(Vector3Subtract(camera.target, camera.position), camera.up);
                right = Vector3Normalize(right);
                camera.position = Vector3Add(camera.position, Vector3Scale(right, moveSpeed));
                camera.target = Vector3Add(camera.target, Vector3Scale(right, moveSpeed));
            }
        }

        const float cameraPos[3] = {camera.position.x, camera.position.y, camera.position.z};
        SetShaderValue(shader, shader.locs[SHADER_LOC_VECTOR_VIEW], cameraPos, SHADER_UNIFORM_VEC3);
        for (const auto &light: lights) UpdateLightValues(shader, light);

        BeginDrawing(); {
            ClearBackground(DARKGRAY);

            BeginMode3D(camera); {
                octaManager.draw();
            }
            EndMode3D();

            DrawStemCellGUI(&guiState);

            if (freeCameraMode) {
                DrawText("Camera Mode: FREE (Press TAB to return to GUI Mode)", 
                         GetScreenWidth() - 480, 10, 16, RAYWHITE);
            } else {
                DrawText("Press TAB for free camera mode", 
                         GetScreenWidth() - 300, 10, 16, RAYWHITE);
            }

            DrawText(TextFormat("Cells: %zu", octaManager.getCount() * 15), //each octahedron has is 15 cells
                     GetScreenWidth() - 170, 40, 20, RAYWHITE);
            DrawText(TextFormat("Octahedra: %zu", octaManager.getCount()),
                     GetScreenWidth() - 170, 70, 20, RAYWHITE);
        }
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
