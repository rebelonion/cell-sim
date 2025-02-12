#pragma once

#include <vector>
#include <random>
#include <execution>

#include "raylib.h"
#include "raymath.h"
#include "PoolAllocator.h"
#include "SpatialGrid.h"

struct TransformData {
    std::vector<float> positions_x;
    std::vector<float> positions_y;
    std::vector<float> positions_z;

    void reserve(const size_t n) {
        positions_x.reserve(n);
        positions_y.reserve(n);
        positions_z.reserve(n);
    }

    void add(const Vector3 &pos) {
        positions_x.push_back(pos.x);
        positions_y.push_back(pos.y);
        positions_z.push_back(pos.z);
    }

    [[nodiscard]] size_t size() const { return positions_x.size(); }


    [[nodiscard]] Matrix getTransform(const size_t index) const {
        return MatrixTranslate(
            positions_x[index],
            positions_y[index],
            positions_z[index]
        );
    }
};

class TruncatedOctahedraManager {
public:
    TruncatedOctahedraManager(const Model &model, const Material &mat)
        : baseModel(model), material(mat), gen(std::random_device()()) {
        transforms.reserve(2000000);
        constexpr Vector3 center = {0.0f, 2.0f, 0.0f};
        addOctahedron(center);
    }

    void addOctahedron(const Vector3 &pos) {
        if (!grid.isPositionOccupied(pos)) {
            const size_t index = transforms.size();
            transforms.add(pos);
            grid.insert(pos, index);

            //if (transforms.size() % 1000 == 0) {
            //std::cout << "Total octahedra: " << transforms.size() << std::endl; debug
            //}
        }
    }

    [[nodiscard]] std::vector<Vector3> getAvailableNeighborPositions(const Vector3 &pos) const {
        std::vector<Vector3> available;
        available.reserve(14); // 6 square + 8 hexagon positions

        // Square faces
        Vector3 squareDirections[] = {
            {1, 0, 0}, {-1, 0, 0}, {0, 1, 0},
            {0, -1, 0}, {0, 0, 1}, {0, 0, -1}
        };

        for (const auto &dir: squareDirections) {
            if (Vector3 newPos = Vector3Add(pos, Vector3Scale(dir, SQUARE_DISTANCE)); !grid.
                isPositionOccupied(newPos)) {
                available.push_back(newPos);
            }
        }

        // Hexagonal faces
        constexpr float s = 0.577350269f;
        Vector3 hexDirections[] = {
            {s, s, s}, {-s, s, s}, {s, -s, s}, {s, s, -s},
            {-s, -s, s}, {-s, s, -s}, {s, -s, -s}, {-s, -s, -s}
        };

        for (const auto &dir: hexDirections) {
            if (Vector3 newPos = Vector3Add(pos, Vector3Scale(dir, HEXAGON_DISTANCE)); !grid.
                isPositionOccupied(newPos)) {
                available.push_back(newPos);
            }
        }

        return available;
    }

    void draw() const {
        if (transforms.size() == 0) return;

        // Create temporary array of matrices for rendering
        std::vector<Matrix> renderMatrices;
        renderMatrices.reserve(transforms.size());

        for (size_t i = 0; i < transforms.size(); i++) {
            renderMatrices.push_back(transforms.getTransform(i));
        }

        DrawMeshInstanced(baseModel.meshes[0], material, renderMatrices.data(), renderMatrices.size());
    }

    [[nodiscard]] size_t getCount() const {
        return transforms.size();
    }

    void trySpawningNewOctahedra(const float deltaTime) {
        const size_t totalSize = transforms.size();
        std::vector<bool> shouldSpawn(totalSize);
        std::vector<size_t> indices(totalSize);
        std::iota(indices.begin(), indices.end(), 0);

        // Generate spawn decisions in parallel using proper iterators
        std::uniform_real_distribution dis(0.0f, 1.0f);
        std::transform(
            std::execution::par_unseq,
            indices.begin(), indices.end(),
            shouldSpawn.begin(),
            [&](size_t idx) {
                thread_local std::mt19937 localGen(std::random_device{}());
                return dis(localGen) < SPAWN_CHANCE * deltaTime;
            }
        );

        // Collect indices that will spawn
        std::vector<size_t> spawnIndices;
        spawnIndices.reserve(totalSize / 10);
        for (size_t i = 0; i < totalSize; i++) {
            if (shouldSpawn[i]) spawnIndices.push_back(i);
        }

        // Pre-allocate result vector
        std::vector<Vector3> newPositions;
        newPositions.reserve(spawnIndices.size());

        // Process spawn positions in parallel
        std::mutex positionsMutex;
        std::for_each(
            std::execution::par_unseq,
            spawnIndices.begin(), spawnIndices.end(),
            [&](size_t idx) {
                thread_local std::mt19937 localGen(std::random_device{}());

                Vector3 currentPos = {
                    transforms.positions_x[idx],
                    transforms.positions_y[idx],
                    transforms.positions_z[idx]
                };

                if (const auto available = getAvailableNeighborPositions(currentPos); !available.empty()) {
                    std::uniform_int_distribution<> posDis(0, available.size() - 1);
                    const Vector3 newPos = available[posDis(localGen)];

                    std::lock_guard lock(positionsMutex);
                    newPositions.push_back(newPos);
                }
            }
        );

        // Batch add all new octahedra
        for (const auto &newPos: newPositions) {
            addOctahedron(newPos);
        }
    }

private:
    TransformData transforms;
    SpatialGrid grid;
    Model baseModel;
    Material material;
    std::mt19937 gen;
    PoolAllocator<Matrix> matrixPool{};

    const float SPAWN_CHANCE = 0.8f;
    const float SQUARE_DISTANCE = 2.0f * 2.828f;
    const float HEXAGON_DISTANCE = 1.732f * 2.828f;
};
