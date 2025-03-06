#pragma once

#include <vector>
#include <random>
#include <execution>
#include <mutex>
#include <numeric>
#include <unordered_map>

#include "raylib.h"
#include "raymath.h"
#include "OctahedronGrid.h"

struct TransformData {
    std::vector<float> positions_x;
    std::vector<float> positions_y;
    std::vector<float> positions_z;
    std::vector<bool> is_visible;

    void reserve(const size_t n) {
        positions_x.reserve(n);
        positions_y.reserve(n);
        positions_z.reserve(n);
        is_visible.reserve(n);
    }

    void add(const Vector3 &pos) {
        positions_x.push_back(pos.x);
        positions_y.push_back(pos.y);
        positions_z.push_back(pos.z);
        is_visible.push_back(true);  // Initially visible
    }

    [[nodiscard]] size_t size() const { return positions_x.size(); }

    [[nodiscard]] Matrix getTransform(const size_t index) const {
        return MatrixTranslate(
            positions_x[index],
            positions_y[index],
            positions_z[index]
        );
    }
    
    [[nodiscard]] Vector3 getPosition(const size_t index) const {
        return {
            positions_x[index],
            positions_y[index],
            positions_z[index]
        };
    }
    
    void setVisibility(const size_t index, bool visible) {
        is_visible[index] = visible;
    }
    
    [[nodiscard]] bool isVisible(const size_t index) const {
        return is_visible[index];
    }
};

class TruncatedOctahedraManager {
public:
    TruncatedOctahedraManager(const Model &model, const Material &mat)
        : baseModel(model), material(mat),
          gen(std::random_device()()) {
        transforms.reserve(2000000);
        constexpr Vector3 center = {0.0f, 2.0f, 0.0f};
        addOctahedron(center);
    }

    void addOctahedron(const Vector3 &pos) {
        if (!grid.isOccupied(pos)) {
            const size_t index = transforms.size();
            transforms.add(pos);
            grid.insert(pos, index);

            //if (transforms.size() % 1000 == 0) {
            //std::cout << "Total octahedra: " << transforms.size() << std::endl; debug
            //}
        }
    }

    [[nodiscard]] std::vector<Vector3> getAvailableNeighborPositions(const Vector3 &pos) const {
        // Use the grid's optimized neighbor calculation with statistics
        auto result = grid.getAvailableNeighborsWithStats(pos);
        
        // Update availability statistics (logically const)
        const_cast<TruncatedOctahedraManager*>(this)->totalSquareFacesAvailable += result.squareFaces;
        const_cast<TruncatedOctahedraManager*>(this)->totalHexagonFacesAvailable += result.hexagonFaces;
        
        return result.positions;
    }
    
    // Get all occupied neighbor cells
    [[nodiscard]] std::vector<size_t> getNeighborCellIndices(const Vector3 &pos) const {
        auto neighbors = grid.getOccupiedNeighbors(pos);
        std::vector<size_t> indices;
        indices.reserve(neighbors.size());
        
        for (const auto& neighbor : neighbors) {
            indices.push_back(neighbor.cellIndex);
        }
        
        return indices;
    }

    void updateVisibility() {
        // Using parallelized execution for better performance
        std::vector<size_t> indices(transforms.size());
        std::iota(indices.begin(), indices.end(), 0);
        
        std::for_each(
            std::execution::par_unseq,
            indices.begin(), indices.end(),
            [&](size_t idx) {
                Vector3 pos = transforms.getPosition(idx);
                auto neighbors = grid.getOccupiedNeighbors(pos);
                
                // Cell is invisible if it has all 14 neighbors
                bool isVisible = neighbors.size() < 14;
                transforms.setVisibility(idx, isVisible);
            }
        );
    }
    
    void draw() const {
        if (transforms.size() == 0) return;

        // Create temporary array of matrices for rendering only visible cells
        std::vector<Matrix> renderMatrices;
        renderMatrices.reserve(transforms.size());

        for (size_t i = 0; i < transforms.size(); i++) {
            if (transforms.isVisible(i)) {
                renderMatrices.push_back(transforms.getTransform(i));
            }
        }

        DrawMeshInstanced(baseModel.meshes[0], material, renderMatrices.data(), renderMatrices.size());
    }

    [[nodiscard]] size_t getCount() const {
        return transforms.size();
    }

    // Get count of cells with specific number of neighbors (for UI display)
    [[nodiscard]] std::unordered_map<int, int> getNeighborStats() const {
        std::unordered_map<int, int> neighborCounts;

        for (size_t i = 0; i < transforms.size(); i++) {
            Vector3 pos = transforms.getPosition(i);
            int neighborCount = grid.getOccupiedNeighbors(pos).size();
            neighborCounts[neighborCount]++;
        }

        return neighborCounts;
    }
    
    // Get hash table statistics for performance monitoring
    [[nodiscard]] std::tuple<size_t, size_t, size_t, double> getHashStats() const {
        return grid.getHashStats();
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
                Vector3 currentPos = transforms.getPosition(idx);

                if (const auto available = getAvailableNeighborPositions(currentPos); !available.empty()) {
                    std::uniform_int_distribution<> posDis(0, available.size() - 1);
                    const Vector3 newPos = available[posDis(localGen)];
                    
                    // Determine if this is a square or hexagon face placement
                    bool isSquareFace = false;
                    
                    // Check if it's a square face (only one coordinate differs)
                    float dx = std::abs(newPos.x - currentPos.x);
                    float dy = std::abs(newPos.y - currentPos.y);
                    float dz = std::abs(newPos.z - currentPos.z);
                    
                    if ((dx > 0 && dy < 0.1f && dz < 0.1f) ||
                        (dx < 0.1f && dy > 0 && dz < 0.1f) ||
                        (dx < 0.1f && dy < 0.1f && dz > 0)) {
                        isSquareFace = true;
                    }
                    
                    // Update statistics
                    if (isSquareFace) {
                        std::lock_guard lock(positionsMutex);
                        squareFacePlacements++;
                    } else {
                        std::lock_guard lock(positionsMutex);
                        hexagonFacePlacements++;
                    }

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
    OctahedronGrid grid;
    Model baseModel;
    Material material;
    std::mt19937 gen;
    
    // Statistics for debugging
    size_t squareFacePlacements = 0;
    size_t hexagonFacePlacements = 0;
    
    // Availability statistics
    size_t totalSquareFacesAvailable = 0;
    size_t totalHexagonFacesAvailable = 0;

    const float SPAWN_CHANCE = 0.8f;
    
public:
    // For debugging - get statistics about which face types are used for placement
    [[nodiscard]] std::pair<size_t, size_t> getPlacementStats() const {
        return {squareFacePlacements, hexagonFacePlacements};
    }
    
    // Get availability statistics
    [[nodiscard]] std::pair<size_t, size_t> getAvailabilityStats() const {
        return {totalSquareFacesAvailable, totalHexagonFacesAvailable};
    }
    
    // Get visibility statistics
    [[nodiscard]] std::pair<size_t, size_t> getVisibilityStats() const {
        size_t visible = 0;
        size_t hidden = 0;
        
        for (size_t i = 0; i < transforms.size(); i++) {
            if (transforms.isVisible(i)) {
                visible++;
            } else {
                hidden++;
            }
        }
        
        return {visible, hidden};
    }
    
    // Reset all statistics
    void resetPlacementStats() {
        squareFacePlacements = 0;
        hexagonFacePlacements = 0;
        totalSquareFacesAvailable = 0;
        totalHexagonFacesAvailable = 0;
    }
private:
};
