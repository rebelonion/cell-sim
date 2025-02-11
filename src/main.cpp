#include "raylib.h"
#include "raymath.h"
#include <vector>
#include "MeshGenerator.h"
#define RLIGHTS_IMPLEMENTATION

#include <iostream>
#include <random>
#include <unordered_map>

#include "rlights.h"

#if defined(PLATFORM_DESKTOP)
    #define GLSL_VERSION            330
#else   // PLATFORM_ANDROID, PLATFORM_WEB
    #define GLSL_VERSION            100
#endif

struct OctahedronPosition {
    Vector3 position;
    bool active;
};

struct Vector3Hash {
    std::size_t operator()(const Vector3& v) const {
        // Convert to grid coordinates to handle floating point imprecision
        int x = static_cast<int>(v.x * 10.0f);
        int y = static_cast<int>(v.y * 10.0f);
        int z = static_cast<int>(v.z * 10.0f);
        return std::hash<int>()(x) ^ (std::hash<int>()(y) << 1) ^ (std::hash<int>()(z) << 2);
    }
};

// Equality operator for Vector3 using grid coordinates
struct Vector3Equal {
    bool operator()(const Vector3& a, const Vector3& b) const {
        const float epsilon = 0.1f;
        return fabs(a.x - b.x) < epsilon &&
               fabs(a.y - b.y) < epsilon &&
               fabs(a.z - b.z) < epsilon;
    }
};

class TruncatedOctahedraManager {
private:
    std::vector<Matrix> transformMatrices;  // For instanced rendering
    std::unordered_map<Vector3, bool, Vector3Hash, Vector3Equal> positionMap;
    const float SQUARE_DISTANCE = 2.0f * 2.828f;
    const float HEXAGON_DISTANCE = 1.732f * 2.828f;
    Model baseModel;
    std::mt19937 gen;
    const float SPAWN_CHANCE = 0.8f;

    // Frustum culling helpers
    BoundingBox modelBBox;
    float cullRadius;

public:
    TruncatedOctahedraManager(Model model) : baseModel(model), gen(std::random_device()()) {
        // Initialize center position
        Vector3 center = {0.0f, 2.0f, 0.0f};
        addOctahedron(center);

        // Calculate bounding sphere radius for culling
        modelBBox = GetModelBoundingBox(model);
        Vector3 size = Vector3Subtract(modelBBox.max, modelBBox.min);
        cullRadius = Vector3Length(Vector3Scale(size, 0.5f));
    }

    void addOctahedron(const Vector3& pos) {
        if (positionMap.find(pos) == positionMap.end()) {
            positionMap[pos] = true;
            Matrix transform = MatrixTranslate(pos.x, pos.y, pos.z);
            transformMatrices.push_back(transform);

            if (transformMatrices.size() % 1000 == 0) {
                std::cout << "Total octahedra: " << transformMatrices.size() << std::endl;
            }
        }
    }

    bool isPositionOccupied(const Vector3& pos) {
        return positionMap.find(pos) != positionMap.end();
    }

    std::vector<Vector3> getAvailableNeighborPositions(const Vector3& pos) {
        std::vector<Vector3> available;

        // Square neighbors
        Vector3 squareDirections[] = {
            {1, 0, 0}, {-1, 0, 0}, {0, 1, 0},
            {0, -1, 0}, {0, 0, 1}, {0, 0, -1}
        };

        for (const auto& dir : squareDirections) {
            Vector3 newPos = Vector3Add(pos, Vector3Scale(dir, SQUARE_DISTANCE));
            if (!isPositionOccupied(newPos)) {
                available.push_back(newPos);
            }
        }

        // Hexagon neighbors
        const float s = 0.577350269f;
        Vector3 hexDirections[] = {
            {s, s, s}, {-s, s, s}, {s, -s, s}, {s, s, -s},
            {-s, -s, s}, {-s, s, -s}, {s, -s, -s}, {-s, -s, -s}
        };

        for (const auto& dir : hexDirections) {
            Vector3 newPos = Vector3Add(pos, Vector3Scale(dir, HEXAGON_DISTANCE));
            if (!isPositionOccupied(newPos)) {
                available.push_back(newPos);
            }
        }

        return available;
    }

    void trySpawningNewOctahedra(float deltaTime) {
        std::uniform_real_distribution<float> dis(0.0f, 1.0f);

        // Create a copy of positions to iterate over while modifying the original
        std::vector<Vector3> currentPositions;
        currentPositions.reserve(positionMap.size());
        for (const auto& pair : positionMap) {
            currentPositions.push_back(pair.first);
        }

        for (const auto& pos : currentPositions) {
            auto available = getAvailableNeighborPositions(pos);

            if (!available.empty() && dis(gen) < SPAWN_CHANCE * deltaTime) {
                std::uniform_int_distribution<> posDis(0, available.size() - 1);
                Vector3 newPos = available[posDis(gen)];
                addOctahedron(newPos);
            }
        }
    }

    void draw(const Camera3D& camera) {
        if (transformMatrices.empty()) return;

        // Draw all instances using raylib's instancing function
        for (const Matrix& transform : transformMatrices) {
            DrawModel(baseModel, Vector3Zero(), 1.0f, WHITE);
            baseModel.transform = transform;
        }
    }

    size_t getCount() const {
        return transformMatrices.size();
    }
};


// Example usage:
int main() {
    const int screenWidth = 800;
    const int screenHeight = 450;
    InitWindow(screenWidth, screenHeight, "3D Outline Shader");

    Shader shader = LoadShader("../data/shaders/lighting.vs",
                               "../data/shaders/lighting.fs");
    shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(shader, "matModel");
    shader.locs[SHADER_LOC_MATRIX_VIEW] = GetShaderLocation(shader, "matView");
    shader.locs[SHADER_LOC_MATRIX_PROJECTION] = GetShaderLocation(shader, "matProjection");
    shader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(shader, "viewPos");

    int ambientLoc = GetShaderLocation(shader, "ambient");
    SetShaderValue(shader, ambientLoc, (float[4]){ 0.2f, 0.2f, 0.2f, 1.0f }, SHADER_UNIFORM_VEC4);

    Light lights[MAX_LIGHTS] = { 0 };
    float lightRadius = 40.0f;
    float lightHeight = 3.0f;
    float rotationAngle = 0.0f;

    lights[0] = CreateLight(LIGHT_POINT, (Vector3){ -lightRadius, lightHeight, -lightRadius }, Vector3Zero(), YELLOW, shader);
    lights[1] = CreateLight(LIGHT_POINT, (Vector3){ lightRadius, lightHeight, lightRadius }, Vector3Zero(), RED, shader);
    lights[2] = CreateLight(LIGHT_POINT, (Vector3){ -lightRadius, lightHeight, lightRadius }, Vector3Zero(), GREEN, shader);
    lights[3] = CreateLight(LIGHT_POINT, (Vector3){ lightRadius, lightHeight, -lightRadius }, Vector3Zero(), BLUE, shader);

    Camera3D camera = { 0 };
    camera.position = Vector3{ 100.0f, 100.0f, 100.0f };
    camera.target = Vector3{ 0.0f, 0.0f, 0.0f };
    camera.up = Vector3{ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    Material material = LoadMaterialDefault();
    material.shader = shader;
    material.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
    material.maps[MATERIAL_MAP_SPECULAR].color = WHITE;
    float shininess = 0.0f;
    material.maps[MATERIAL_MAP_SPECULAR].value = 0.5f;
    material.maps[MATERIAL_MAP_DIFFUSE].value = 1.0f;
    int shininessLoc = GetShaderLocation(shader, "shininess");
    SetShaderValue(shader, shininessLoc, &shininess, SHADER_UNIFORM_FLOAT);

    Model model = LoadModelFromMesh(MeshGenerator::genTruncatedOctahedron());
    model.materials[0] = material;

    TruncatedOctahedraManager octaManager(model);

    const float LIGHT_ROTATION_SPEED = 0.5f;

    SetTargetFPS(60);
    while (!WindowShouldClose()) {
        float deltaTime = GetFrameTime();

        // Update light rotation
        rotationAngle += LIGHT_ROTATION_SPEED * deltaTime;

        // Update light positions
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
            -lightRadius * cosf(rotationAngle + PI/2),
            lightHeight,
            lightRadius * sinf(rotationAngle + PI/2)
        };
        lights[3].position = (Vector3){
            lightRadius * cosf(rotationAngle - PI/2),
            lightHeight,
            -lightRadius * sinf(rotationAngle - PI/2)
        };

        // Try spawning new octahedra
        octaManager.trySpawningNewOctahedra(deltaTime);

        UpdateCamera(&camera, CAMERA_ORBITAL);

        float cameraPos[3] = { camera.position.x, camera.position.y, camera.position.z };
        SetShaderValue(shader, shader.locs[SHADER_LOC_VECTOR_VIEW], cameraPos, SHADER_UNIFORM_VEC3);

        for (int i = 0; i < MAX_LIGHTS; i++) UpdateLightValues(shader, lights[i]);

        BeginDrawing();
        ClearBackground(RAYWHITE);
        BeginMode3D(camera);

        //BeginShaderMode(shader);
        //DrawPlane(Vector3Zero(), (Vector2) { 20.0, 20.0 }, WHITE);
        //EndShaderMode();

        octaManager.draw(camera);

        EndMode3D();

        DrawFPS(10, 10);
        DrawText(TextFormat("Octahedra: %zu", octaManager.getCount()), 10, 30, 20, BLACK);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}