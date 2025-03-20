#pragma once

#include <vector>
#include <random>
#include <execution>
#include <mutex>
#include <numeric>
#include <unordered_map>
#include <unordered_set>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <array>
#include <algorithm>
#include <iostream>

#include "raylib.h"
#include "raymath.h"
#include "OctahedronGrid.h"

#include "BoundaryManager.h"

// Colors for different neighbor counts (0-14)
constexpr std::array<Color, 15> NEIGHBOR_COLORS = {{
    BLUE,           // 0 neighbors
    SKYBLUE,        // 1 neighbor
    DARKBLUE,       // 2 neighbors
    PURPLE,         // 3 neighbors
    VIOLET,         // 4 neighbors
    PINK,           // 5 neighbors
    MAGENTA,        // 6 neighbors
    MAROON,         // 7 neighbors
    RED,            // 8 neighbors
    ORANGE,         // 9 neighbors
    GOLD,           // 10 neighbors
    YELLOW,         // 11 neighbors
    BEIGE,          // 12 neighbors
    LIME,           // 13 neighbors
    GREEN           // 14 neighbors (fully surrounded)
}};

struct TransformData {
    std::vector<float> positions_x;
    std::vector<float> positions_y;
    std::vector<float> positions_z;
    std::vector<bool> is_visible;
    std::vector<int> neighbor_counts;

    void reserve(const size_t n) {
        positions_x.reserve(n);
        positions_y.reserve(n);
        positions_z.reserve(n);
        is_visible.reserve(n);
        neighbor_counts.reserve(n);
    }

    void add(const Vector3 &pos) {
        positions_x.push_back(pos.x);
        positions_y.push_back(pos.y);
        positions_z.push_back(pos.z);
        is_visible.push_back(true);
        neighbor_counts.push_back(0); // Initially no neighbors
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
    
    void setNeighborCount(const size_t index, int count) {
        neighbor_counts[index] = count;
    }
    
    [[nodiscard]] int getNeighborCount(const size_t index) const {
        return neighbor_counts[index];
    }
};

class TruncatedOctahedraManager {
public:
    TruncatedOctahedraManager(const Model &model, const Material &mat)
        : baseModel(model), material(mat),
          gen(std::random_device()()),
          generationActive(false),
          shouldStopThread(false),
          boundaryManager(std::make_shared<BoundaryManager>()) {
        
        // Create models for each neighbor count with different colors
        setupColoredModels();
        
        transforms.reserve(5000000);
        
        // Generate between 2 and 5 random octahedra
        std::uniform_int_distribution<int> countDist(2, 5);
        int numOctahedra = countDist(gen);
        
        // Create widely separated starting positions
        std::vector<Vector3> startingPositions;
        
        // Instead of fixed center, create octahedra in different quadrants/sectors
        // Use larger range to spread them out more
        std::uniform_real_distribution<float> spreadDist(30.0f, 70.0f);
        std::uniform_real_distribution<float> yDist(-10.0f, 30.0f);
        
        // Generate random positions that are well-separated
        for (int i = 0; i < numOctahedra; i++) {
            // Generate coordinates with distance from origin
            float dist = spreadDist(gen);
            float angle = (2.0f * PI * i) / numOctahedra; // Evenly distribute around a circle
            
            // Convert to cartesian coordinates
            Vector3 randomPos = {
                dist * cosf(angle),
                yDist(gen), // Random height
                dist * sinf(angle)
            };
            
            // Place octahedron only if within boundary
            if (isWithinBoundary(randomPos)) {
                startingPositions.push_back(randomPos);
                addOctahedron(randomPos);
            }
        }
        
        // If no octahedra were placed (all outside boundary), place one at center
        if (startingPositions.empty()) {
            constexpr Vector3 center = {0.0f, 2.0f, 0.0f};
            addOctahedron(center);
        }
        
        // Initialize neighbor counts for all initial cells
        for (size_t i = 0; i < transforms.size(); i++) {
            updateCellVisibility(i);
        }
    }
    
    // Create independent colored models for each neighbor count
    void setupColoredModels() {
        for (int i = 0; i < 15; i++) {
            // Create a new mesh for each model
            Mesh mesh = MeshGenerator::genTruncatedOctahedron();
            
            // Create the model from the mesh
            coloredModels[i] = LoadModelFromMesh(mesh);
            
            // Assign a new material with the desired color
            Material newMaterial = LoadMaterialDefault();
            newMaterial.maps[MATERIAL_MAP_DIFFUSE].color = NEIGHBOR_COLORS[i];
            newMaterial.maps[MATERIAL_MAP_SPECULAR].color = WHITE;
            newMaterial.maps[MATERIAL_MAP_DIFFUSE].value = 1.0f;
            newMaterial.maps[MATERIAL_MAP_SPECULAR].value = 0.5f;
            newMaterial.shader = material.shader;  // Use the same shader as the base material
            
            // Assign the material to the model
            coloredModels[i].materials[0] = newMaterial;
        }
    }

    [[nodiscard]] std::shared_ptr<BoundaryManager> getBoundaryManager() const {
        return boundaryManager;
    }

    ~TruncatedOctahedraManager() {
        shouldStopThread = true;

        if (generationThread.joinable()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            std::lock_guard lock(pendingChangesMutex);
            generationThread.join();
        }

        std::lock_guard lock(pendingChangesMutex);
        pendingNewPositions.clear();
        
        // Unload all the colored models
        for (int i = 0; i < 15; i++) {
            UnloadModel(coloredModels[i]);
        }
    }

    void addOctahedron(const Vector3 &pos) {
        // Check if position is inside the boundary (or if no boundary is set)
        if (!grid.isOccupied(pos) && isWithinBoundary(pos)) {
            const size_t index = transforms.size();
            transforms.add(pos);
            grid.insert(pos, index);
        }
    }

    // Check if a position is within the current boundary
    [[nodiscard]] bool isWithinBoundary(const Vector3 &pos) const {
        return boundaryManager->isPointWithinBoundary(pos);
    }

    [[nodiscard]] std::vector<Vector3> getAvailableNeighborPositions(const Vector3 &pos) const {
        auto [positions, squareFaces, hexagonFaces] = grid.getAvailableNeighborsWithStats(pos);
        const_cast<TruncatedOctahedraManager *>(this)->totalSquareFacesAvailable += squareFaces;
        const_cast<TruncatedOctahedraManager *>(this)->totalHexagonFacesAvailable += hexagonFaces;

        // Filter positions by boundary
        std::vector<Vector3> filteredPositions;
        filteredPositions.reserve(positions.size());

        for (const auto &neighborPos: positions) {
            if (boundaryManager->isPointWithinBoundary(neighborPos)) {
                filteredPositions.push_back(neighborPos);
            }
        }

        return filteredPositions;
    }

    [[nodiscard]] std::vector<size_t> getNeighborCellIndices(const Vector3 &pos) const {
        auto neighbors = grid.getOccupiedNeighbors(pos);
        std::vector<size_t> indices;
        indices.reserve(neighbors.size());

        for (const auto &[cellIndex, position]: neighbors) {
            indices.push_back(cellIndex);
        }

        return indices;
    }

    void updateVisibility() {
        std::vector<size_t> indices(transforms.size());
        std::iota(indices.begin(), indices.end(), 0);

        std::for_each(
            std::execution::par_unseq,
            indices.begin(), indices.end(),
            [&](const size_t idx) {
                updateCellVisibility(idx);
            }
        );
    }

    void updateCellVisibility(const size_t idx) {
        const Vector3 pos = transforms.getPosition(idx);
        const auto neighbors = grid.getOccupiedNeighbors(pos);

        const int neighborCount = static_cast<int>(neighbors.size());
        const bool isVisible = neighborCount < 14;
        transforms.setVisibility(idx, isVisible);
        transforms.setNeighborCount(idx, neighborCount);
    }

    void updateVisibilityForNewCells(const std::vector<Vector3> &newPositions) {
        constexpr size_t MAX_BATCH_SIZE = 1000;

        for (size_t startIdx = 0; startIdx < newPositions.size(); startIdx += MAX_BATCH_SIZE) {
            const size_t batchSize = std::min(MAX_BATCH_SIZE, newPositions.size() - startIdx);

            for (size_t i = 0; i < batchSize; i++) {
                const auto &pos = newPositions[startIdx + i];
                if (const size_t idx = grid.findCellIndex(pos); idx != SIZE_MAX) {
                    updateCellVisibility(idx);
                }
            }

            std::unordered_set<size_t> processedNeighbors;
            processedNeighbors.reserve(batchSize * 14);

            for (size_t i = 0; i < batchSize; i++) {
                const auto &pos = newPositions[startIdx + i];

                for (auto neighborPositions = OctahedronGrid::getNeighborPositions(pos); const auto &neighborPos:
                     neighborPositions) {
                    if (grid.isOccupied(neighborPos)) {
                        if (size_t neighborIdx = grid.findCellIndex(neighborPos); neighborIdx != SIZE_MAX) {
                            processedNeighbors.insert(neighborIdx);
                        }
                    }
                }
            }

            if (std::vector neighborIndices(processedNeighbors.begin(), processedNeighbors.end()); !neighborIndices.
                empty()) {
                std::for_each(
                    std::execution::par_unseq,
                    neighborIndices.begin(), neighborIndices.end(),
                    [&](const size_t idx) {
                        updateCellVisibility(idx);
                    }
                );
            }
        }
    }

    void draw() const {
        if (transforms.size() == 0) return;

        constexpr size_t MAX_BATCH_SIZE = 100000;

        // Group transforms by neighbor count (0-14)
        std::array<std::vector<Matrix>, 15> neighborCountMatrices;
        for (auto& matrices : neighborCountMatrices) {
            matrices.reserve(1000); // Initial reservation to avoid too many reallocations
        }

        // Organize visible cells by neighbor count
        for (size_t i = 0; i < transforms.size(); i++) {
            if (transforms.isVisible(i)) {
                int neighborCount = transforms.getNeighborCount(i);
                
                // Ensure neighbor count is in valid range
                neighborCount = std::clamp(neighborCount, 0, 14);
                
                neighborCountMatrices[neighborCount].push_back(transforms.getTransform(i));
            }
        }
        // Render each group with its corresponding colored material
        for (int count = 0; count < 15; count++) {
            const auto& matrices = neighborCountMatrices[count];
            if (matrices.empty()) continue;

            // Render in batches if needed
            if (matrices.size() <= MAX_BATCH_SIZE) {
                DrawMeshInstanced(coloredModels[count].meshes[0], coloredModels[count].materials[0], matrices.data(), matrices.size());
            } else {
                for (size_t offset = 0; offset < matrices.size(); offset += MAX_BATCH_SIZE) {
                    size_t batchSize = std::min(MAX_BATCH_SIZE, matrices.size() - offset);
                    DrawMeshInstanced(coloredModels[count].meshes[0], coloredModels[count].materials[0], matrices.data() + offset, batchSize);
                }
            }
        }

        // Draw boundary wireframe
        boundaryManager->draw();
    }

    void toggleBoundaryVisibility() const {
        boundaryManager->toggleVisibility();
    }

    void generateRandomBoundary() const {
        boundaryManager->generateRandomBoundary();
    }

    void toggleBoundaryEnabled() const {
        boundaryManager->toggleBoundaryEnabled();
    }

    [[nodiscard]] bool isBoundaryEnabled() const {
        return boundaryManager->isBoundaryEnabled();
    }

    [[nodiscard]] size_t getCount() const {
        return transforms.size();
    }

    [[nodiscard]] std::unordered_map<int, int> getNeighborStats() const {
        std::unordered_map<int, int> neighborCounts;

        for (size_t i = 0; i < transforms.size(); i++) {
            int neighborCount = transforms.getNeighborCount(i);
            neighborCounts[neighborCount]++;
        }

        return neighborCounts;
    }

    [[nodiscard]] std::tuple<size_t, size_t, size_t, double> getHashStats() const {
        return grid.getHashStats();
    }

    void trySpawningNewOctahedra(const float deltaTime) {
        const size_t totalSize = transforms.size();
        std::vector<bool> shouldSpawn(totalSize);
        std::vector<size_t> indices(totalSize);
        std::iota(indices.begin(), indices.end(), 0);

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

        std::vector<size_t> spawnIndices;
        spawnIndices.reserve(totalSize / 10);
        for (size_t i = 0; i < totalSize; i++) {
            if (shouldSpawn[i]) spawnIndices.push_back(i);
        }

        std::vector<Vector3> newPositions;
        newPositions.reserve(spawnIndices.size());

        std::mutex positionsMutex;
        std::for_each(
            std::execution::par_unseq,
            spawnIndices.begin(), spawnIndices.end(),
            [&](const size_t idx) {
                thread_local std::mt19937 localGen(std::random_device{}());
                Vector3 currentPos = transforms.getPosition(idx);

                if (const auto available = getAvailableNeighborPositions(currentPos); !available.empty()) {
                    std::uniform_int_distribution<> posDis(0, available.size() - 1);
                    const Vector3 newPos = available[posDis(localGen)];

                    bool isSquareFace = false;

                    float dx = std::abs(newPos.x - currentPos.x);
                    float dy = std::abs(newPos.y - currentPos.y);
                    float dz = std::abs(newPos.z - currentPos.z);

                    if ((dx > 0 && dy < 0.1f && dz < 0.1f) ||
                        (dx < 0.1f && dy > 0 && dz < 0.1f) ||
                        (dx < 0.1f && dy < 0.1f && dz > 0)) {
                        isSquareFace = true;
                    }

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

        for (const auto &newPos: newPositions) {
            addOctahedron(newPos);
        }

        if (!newPositions.empty()) {
            updateVisibilityForNewCells(newPositions);
        }
    }

    void startGenerationThread() {
        if (generationActive) return;

        shouldStopThread = false;
        generationActive = true;

        if (generationThread.joinable()) {
            generationThread.join();
        }

        generationThread = std::thread(&TruncatedOctahedraManager::generationThreadFunc, this);
    }

    void stopGenerationThread() {
        if (!generationActive) return;

        shouldStopThread = true;
        generationActive = false; {
            std::lock_guard<std::mutex> lock(pendingChangesMutex);
        }

        if (generationThread.joinable()) {
            try {
                generationThread.join();
            } catch (const std::exception &e) {
            }
        } {
            std::lock_guard lock(pendingChangesMutex);
            pendingNewPositions.clear();
        }
    }

    bool isGenerationActive() const {
        return generationActive;
    }

    void applyPendingChanges() {
        std::vector<Vector3> newPositionsToApply; {
            std::lock_guard<std::mutex> lock(pendingChangesMutex);
            if (pendingNewPositions.empty()) {
                return;
            }
            newPositionsToApply = std::move(pendingNewPositions);
            pendingNewPositions.clear();
        }

        const size_t newCellCount = newPositionsToApply.size();

        size_t expectedNewSize = transforms.size() + newCellCount;
        if (transforms.size() + newCellCount > transforms.positions_x.capacity()) {
            const size_t newCapacity = (transforms.size() + newCellCount) * 1.5;
            transforms.positions_x.reserve(newCapacity);
            transforms.positions_y.reserve(newCapacity);
            transforms.positions_z.reserve(newCapacity);
            transforms.is_visible.reserve(newCapacity);
            transforms.neighbor_counts.reserve(newCapacity);
        }

        constexpr size_t batchSize = 5000;
        const size_t numBatches = (newCellCount + batchSize - 1) / batchSize;

        for (size_t batch = 0; batch < numBatches; batch++) {
            const size_t startIdx = batch * batchSize;
            const size_t endIdx = std::min(startIdx + batchSize, newCellCount);

            for (size_t i = startIdx; i < endIdx; i++) {
                addOctahedron(newPositionsToApply[i]);
            }
        }

        updateVisibilityForNewCells(newPositionsToApply);
    }

private:
    void generationThreadFunc() {
        while (!shouldStopThread) {
            //generateCellBatch();
            trySpawningNewOctahedra(0.16f);

            if (shouldStopThread) break;

            std::this_thread::yield();
        }
    }

    void generateCellBatch() {
        size_t totalSize; {
            std::lock_guard<std::mutex> lock(pendingChangesMutex);
            totalSize = transforms.size();
        }

        if (totalSize == 0) return;

        size_t baseBatchSize = 5000;

        if (totalSize > 10000) {
            baseBatchSize = 5000 + static_cast<size_t>(5.0 * std::pow(totalSize / 10000.0, 2.0 / 3.0) * 1000);
        }

        const size_t batchSize = std::min(totalSize, std::min(baseBatchSize, static_cast<size_t>(100000)));

        std::vector<size_t> indices(batchSize); {
            std::uniform_int_distribution<size_t> indexDist(0, totalSize - 1);
            for (size_t i = 0; i < batchSize; i++) {
                indices[i] = indexDist(gen);
            }
        }

        std::vector<bool> shouldSpawn(batchSize);
        std::uniform_real_distribution spawnDist(0.0f, 1.0f);

        for (size_t i = 0; i < batchSize; i++) {
            shouldSpawn[i] = spawnDist(gen) < SPAWN_CHANCE;
        }

        std::vector<size_t> spawnIndices;
        spawnIndices.reserve(batchSize / 10);

        for (size_t i = 0; i < batchSize; i++) {
            if (shouldSpawn[i]) spawnIndices.push_back(indices[i]);
        }

        std::vector<Vector3> newLocalPositions;
        newLocalPositions.reserve(spawnIndices.size());

        for (const auto &idx: spawnIndices) {
            if (idx >= totalSize) continue;

            Vector3 currentPos; {
                std::lock_guard<std::mutex> lock(pendingChangesMutex);
                if (idx >= transforms.size()) continue;
                currentPos = transforms.getPosition(idx);
            }

            const auto available = getAvailableNeighborPositions(currentPos);
            if (!available.empty()) {
                std::uniform_int_distribution<size_t> posDis(0, available.size() - 1);
                const Vector3 newPos = available[posDis(gen)];

                float dx = std::abs(newPos.x - currentPos.x);
                float dy = std::abs(newPos.y - currentPos.y);
                float dz = std::abs(newPos.z - currentPos.z);

                bool isSquareFace = (dx > 0 && dy < 0.1f && dz < 0.1f) ||
                                    (dx < 0.1f && dy > 0 && dz < 0.1f) ||
                                    (dx < 0.1f && dy < 0.1f && dz > 0);

                if (isSquareFace) {
                    std::lock_guard<std::mutex> lock(pendingChangesMutex);
                    squareFacePlacements++;
                } else {
                    std::lock_guard<std::mutex> lock(pendingChangesMutex);
                    hexagonFacePlacements++;
                }

                newLocalPositions.push_back(newPos);
            }
        }

        if (!newLocalPositions.empty()) {
            std::lock_guard<std::mutex> lock(pendingChangesMutex);
            pendingNewPositions.insert(
                pendingNewPositions.end(),
                newLocalPositions.begin(),
                newLocalPositions.end()
            );
        }
    }

    TransformData transforms;
    OctahedronGrid grid;
    Model baseModel;
    Material material;
    std::array<Model, 15> coloredModels;
    std::mt19937 gen;

    std::thread generationThread;
    std::atomic<bool> generationActive;
    std::atomic<bool> shouldStopThread;
    std::mutex pendingChangesMutex;
    std::vector<Vector3> pendingNewPositions;

    // Boundary manager for controlling where octahedra can be placed
    std::shared_ptr<BoundaryManager> boundaryManager;

    size_t squareFacePlacements = 0;
    size_t hexagonFacePlacements = 0;

    size_t totalSquareFacesAvailable = 0;
    size_t totalHexagonFacesAvailable = 0;

    const float SPAWN_CHANCE = 0.1f;

public:
    [[nodiscard]] std::pair<size_t, size_t> getPlacementStats() const {
        return {squareFacePlacements, hexagonFacePlacements};
    }

    [[nodiscard]] std::pair<size_t, size_t> getAvailabilityStats() const {
        return {totalSquareFacesAvailable, totalHexagonFacesAvailable};
    }

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

    void resetPlacementStats() {
        squareFacePlacements = 0;
        hexagonFacePlacements = 0;
        totalSquareFacesAvailable = 0;
        totalHexagonFacesAvailable = 0;
    }

    // Reset the entire simulation to its initial state
    void resetSimulation() {
        // Stop the generation thread if it's running
        if (generationActive) {
            stopGenerationThread();
        }

        // Clear all octahedra
        {
            std::lock_guard<std::mutex> lock(pendingChangesMutex);
            transforms = TransformData();
            transforms.reserve(5000000);
            grid = OctahedronGrid();
            pendingNewPositions.clear();
        }

        // Reset all statistics
        resetPlacementStats();

        // Generate a new random boundary
        boundaryManager->generateRandomBoundary();
        
        // Generate between 2 and 5 random octahedra
        std::uniform_int_distribution<int> countDist(2, 5);
        int numOctahedra = countDist(gen);
        
        // Create widely separated starting positions
        std::vector<Vector3> startingPositions;
        
        // Instead of fixed center, create octahedra in different quadrants/sectors
        // Use larger range to spread them out more
        std::uniform_real_distribution<float> spreadDist(30.0f, 70.0f);
        std::uniform_real_distribution<float> yDist(-10.0f, 30.0f);
        
        // Generate random positions that are well-separated
        for (int i = 0; i < numOctahedra; i++) {
            // Generate coordinates with distance from origin
            float dist = spreadDist(gen);
            float angle = (2.0f * PI * i) / numOctahedra; // Evenly distribute around a circle
            
            // Convert to cartesian coordinates
            Vector3 randomPos = {
                dist * cosf(angle),
                yDist(gen), // Random height
                dist * sinf(angle)
            };
            
            // Place octahedron only if within boundary
            if (isWithinBoundary(randomPos)) {
                startingPositions.push_back(randomPos);
                addOctahedron(randomPos);
            }
        }
        
        // If no octahedra were placed (all outside boundary), place one at center
        if (startingPositions.empty()) {
            constexpr Vector3 center = {0.0f, 2.0f, 0.0f};
            addOctahedron(center);
        }
        
        // Initialize neighbor counts for all initial cells
        for (size_t i = 0; i < transforms.size(); i++) {
            updateCellVisibility(i);
        }
    }
};
