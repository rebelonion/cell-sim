#pragma once

#include <vector>
#include <random>
#include <execution>
#include <mutex>
#include <numeric>
#include <unordered_set>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <array>
#include <algorithm>
#include <iostream>

#include "raylib.h"
#include "OctahedronGrid.h"

#include "BoundaryManager.h"
#include "TransformData.h"

constexpr std::array<Color, 15> NEIGHBOR_COLORS = {
    {
        BLUE, // 0 neighbors
        SKYBLUE, // 1 neighbor
        DARKBLUE, // 2 neighbors
        PURPLE, // 3 neighbors
        VIOLET, // 4 neighbors
        PINK, // 5 neighbors
        MAGENTA, // 6 neighbors
        MAROON, // 7 neighbors
        RED, // 8 neighbors
        ORANGE, // 9 neighbors
        GOLD, // 10 neighbors
        YELLOW, // 11 neighbors
        BEIGE, // 12 neighbors
        LIME, // 13 neighbors
        GREEN // 14 neighbors (fully surrounded)
    }
};


class TruncatedOctahedraManager {
public:
    TruncatedOctahedraManager(const Model &model, const Material &mat)
        : baseModel(model), material(mat),
          gen(std::random_device()()),
          generationActive(false),
          shouldStopThread(false),
          boundaryManager(std::make_shared<BoundaryManager>()),
          gridInitialized(false),
          octahedraSpacing(20.0f),
          octahedraLayers(1) {
        setupColoredModels();
        transforms.reserve(1000);
        startingPositions.reserve(1000);
        generateStartingPositions();
    }

    void handleBoundaryResizing() {
        const float oldWidth = boundaryManager->getBoundaryWidth();
        const float oldDepth = boundaryManager->getBoundaryDepth();
        const float oldHeight = boundaryManager->getBoundaryHeight();
        boundaryManager->handleResizing();
        const float newWidth = boundaryManager->getBoundaryWidth();
        const float newDepth = boundaryManager->getBoundaryDepth();
        const float newHeight = boundaryManager->getBoundaryHeight();

        if (oldWidth != newWidth || oldDepth != newDepth || oldHeight != newHeight) {
            generateStartingPositions();
        }
    }

    void generateStartingPositions() {
        startingPositions.clear();
        const float width = boundaryManager->getBoundaryWidth();
        const float depth = boundaryManager->getBoundaryDepth();
        const float height = boundaryManager->getBoundaryHeight();
        const Vector3 center = boundaryManager->getBoundaryCenter();

        const float horizontalSpacing = octahedraSpacing;
        const float verticalSpacing = octahedraSpacing * 0.866025404f; // sqrt(3)/2

        const float minX = center.x - width / 2 + horizontalSpacing / 2;
        const float maxX = center.x + width / 2 - horizontalSpacing / 2;
        const float minZ = center.z - depth / 2 + verticalSpacing / 2;
        const float maxZ = center.z + depth / 2 - verticalSpacing / 2;

        // Calculate Y position based on boundary center
        const float minY = center.y - height / 2 + octahedraSpacing / 2;

        // Calculate Y spacing for multiple layers if needed
        float layerSpacing = octahedraLayers > 1 ? (height - octahedraSpacing) / (octahedraLayers - 1) : 0;
        layerSpacing = std::max(layerSpacing, octahedraSpacing); // Ensure minimum spacing between layers

        for (int layer = 0; layer < octahedraLayers; layer++) {
            const float layerY = minY + layer * layerSpacing;
            const float layerXOffset = (layer % 2) * horizontalSpacing * 0.25f;
            const float layerZOffset = (layer % 3) * verticalSpacing * 0.25f;

            for (float z = minZ + layerZOffset; z <= maxZ; z += verticalSpacing) {
                const bool isOffsetRow = static_cast<int>((z - minZ) / verticalSpacing) % 2 == 1;
                const float rowOffset = isOffsetRow ? horizontalSpacing * 0.5f : 0.0f;
                for (float x = minX + rowOffset + layerXOffset; x <= maxX; x += horizontalSpacing) {
                    const Vector3 position = {x, layerY, z};
                    const Vector3 snappedPos = OctahedronGrid::snapToGridPosition(position);
                    if (isWithinBoundary(snappedPos)) {
                        startingPositions.push_back(snappedPos);
                    }
                }
            }
        }

        // If no positions were generated, add center position as a fallback
        if (startingPositions.empty()) {
            const Vector3 centerPos = {center.x, center.y, center.z};
            startingPositions.push_back(OctahedronGrid::snapToGridPosition(centerPos));
        }
    }

    // Create octahedra based on the precomputed starting positions
    void createInitialOctahedra() {
        if (transforms.size() > 0) {
            grid = OctahedronGrid();
            transforms = TransformData();
            transforms.reserve(5000000);
        }

        for (const auto &position: startingPositions) {
            addOctahedron(position);
        }

        updateVisibility();
    }

    void resetOctahedra() {
        createInitialOctahedra();
    }

    // Create independent colored models for each neighbor count
    void setupColoredModels() {
        for (int i = 0; i < 15; i++) {
            // Create a new mesh for each model
            const Mesh mesh = MeshGenerator::genTruncatedOctahedron();
            coloredModels[i] = LoadModelFromMesh(mesh);
            Material newMaterial = LoadMaterialDefault();
            newMaterial.maps[MATERIAL_MAP_DIFFUSE].color = NEIGHBOR_COLORS[i];
            newMaterial.maps[MATERIAL_MAP_SPECULAR].color = WHITE;
            newMaterial.maps[MATERIAL_MAP_DIFFUSE].value = 1.0f;
            newMaterial.maps[MATERIAL_MAP_SPECULAR].value = 0.5f;
            newMaterial.shader = material.shader; // Use the same shader as the base material
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
    }

    void addOctahedron(const Vector3 &pos) {
        if (const Vector3 snappedPos = OctahedronGrid::snapToGridPosition(pos);
            !grid.isOccupied(snappedPos) && isWithinBoundary(snappedPos)) {
            const size_t index = transforms.size();
            transforms.add();
            grid.insert(snappedPos, index);
        }
    }

    [[nodiscard]] bool isWithinBoundary(const Vector3 &pos) const {
        return boundaryManager->isPointWithinBoundary(pos);
    }

    [[nodiscard]] std::vector<Vector3> getAvailableNeighborPositions(const Vector3 &pos) const {
        auto positions = grid.getAvailableNeighbors(pos);

        std::vector<Vector3> filteredPositions;
        filteredPositions.reserve(positions.size());

        for (const auto &neighborPos: positions) {
            if (boundaryManager->isPointWithinBoundary(neighborPos)) {
                filteredPositions.push_back(neighborPos);
            }
        }

        return filteredPositions;
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
        const Vector3 pos = grid.getPositionForIndex(idx);
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

                for (auto neighborPositions = grid.getNeighborPositions(pos, false); const auto &neighborPos:
                     neighborPositions) {
                    if (size_t neighborIdx = grid.findCellIndex(neighborPos); neighborIdx != SIZE_MAX) {
                        processedNeighbors.insert(neighborIdx);
                    }
                }
            }

            std::vector neighborIndices(processedNeighbors.begin(), processedNeighbors.end());
            if (!neighborIndices.empty()) {
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
        // If we're in the pre-simulation state (no octahedra created yet), show a preview
        if (transforms.size() == 0) {
            if (!startingPositions.empty()) {
                drawStartingPositionsPreview();
            }
            boundaryManager->draw();
            return;
        }

        // Group transforms by neighbor count (0-14)
        std::array<std::vector<Matrix>, 15> neighborCountMatrices;
        for (auto &matrices: neighborCountMatrices) {
            matrices.reserve(1000);
        }

        // Organize visible cells by neighbor count
        for (size_t i = 0; i < transforms.size(); i++) {
            if (transforms.isVisible(i)) {
                Vector3 position = grid.getPositionForIndex(i);
                int neighborCount = transforms.getNeighborCount(i);

                neighborCount = std::clamp(neighborCount, 0, 14);
                neighborCountMatrices[neighborCount].push_back(transforms.getTransform(i, position));
            }
        }
        // Render each group with its corresponding colored material
        for (int count = 0; count < 15; count++) {
            const auto &matrices = neighborCountMatrices[count];
            if (matrices.empty()) continue;

            if (constexpr size_t MAX_BATCH_SIZE = 100000; matrices.size() <= MAX_BATCH_SIZE) {
                DrawMeshInstanced(coloredModels[count].meshes[0], coloredModels[count].materials[0], matrices.data(),
                                  matrices.size());
            } else {
                for (size_t offset = 0; offset < matrices.size(); offset += MAX_BATCH_SIZE) {
                    const size_t batchSize = std::min(MAX_BATCH_SIZE, matrices.size() - offset);
                    DrawMeshInstanced(coloredModels[count].meshes[0], coloredModels[count].materials[0],
                                      matrices.data() + offset, batchSize);
                }
            }
        }

        boundaryManager->draw();
    }

    void drawStartingPositionsPreview() const {
        std::vector<Matrix> previewMatrices;
        previewMatrices.reserve(startingPositions.size());

        for (const auto &position: startingPositions) {
            previewMatrices.push_back(TransformData::getTransform(0, position));
        }

        if (!previewMatrices.empty()) {
            if (constexpr size_t MAX_BATCH_SIZE = 100000; previewMatrices.size() <= MAX_BATCH_SIZE) {
                DrawMeshInstanced(coloredModels[0].meshes[0], coloredModels[0].materials[0], previewMatrices.data(),
                                  previewMatrices.size());
            } else {
                for (size_t offset = 0; offset < previewMatrices.size(); offset += MAX_BATCH_SIZE) {
                    const size_t batchSize = std::min(MAX_BATCH_SIZE, previewMatrices.size() - offset);
                    DrawMeshInstanced(coloredModels[0].meshes[0], coloredModels[0].materials[0],
                                      previewMatrices.data() + offset, batchSize);
                }
            }
        }
    }

    void toggleBoundaryVisibility() const {
        boundaryManager->toggleVisibility();
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

    [[nodiscard]] size_t getStartingPositionCount() const {
        return startingPositions.size();
    }

    void setOctahedraSpacing(const float spacing) {
        if (spacing > 0.0f) {
            octahedraSpacing = spacing;
            if (!isGenerationActive()) {
                generateStartingPositions();
            }
        }
    }

    [[nodiscard]] float getOctahedraSpacing() const {
        return octahedraSpacing;
    }

    void setOctahedraLayers(const int layers) {
        if (layers > 0) {
            octahedraLayers = layers;
            if (!isGenerationActive()) {
                generateStartingPositions();
            }
        }
    }

    [[nodiscard]] int getOctahedraLayers() const {
        return octahedraLayers;
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
                return dis(localGen) < SPAWN_CHANCE + deltaTime;
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
                const Vector3 currentPos = grid.getPositionForIndex(idx);

                if (const auto available = getAvailableNeighborPositions(currentPos); !available.empty()) {
                    std::uniform_int_distribution<> posDis(0, available.size() - 1);
                    const Vector3 newPos = available[posDis(localGen)];
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

        if (!gridInitialized) {
            // Lock boundary size so it can't be resized during simulation
            boundaryManager->lockBoundarySize();

            const float boundaryWidth = boundaryManager->getBoundaryWidth();
            const float boundaryDepth = boundaryManager->getBoundaryDepth();
            const float boundaryHeight = boundaryManager->getBoundaryHeight();

            constexpr float gridMargin = 1.2f; // 20% margin
            const size_t gridLength = static_cast<size_t>(boundaryWidth * gridMargin / OctahedronGrid::SQUARE_DISTANCE)
                                      + 10;
            const size_t gridHeight = static_cast<size_t>(
                                          boundaryDepth * gridMargin / (OctahedronGrid::SQUARE_DISTANCE / 2)) + 10;
            const size_t gridWidth = static_cast<size_t>(boundaryHeight * gridMargin / OctahedronGrid::SQUARE_DISTANCE)
                                     + 10;

            grid.resizeGrid(gridLength, gridWidth, gridHeight);
            transforms.reserve(gridLength * gridHeight * gridWidth);
            gridInitialized = true;

            createInitialOctahedra();
        }

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
                std::cerr << e.what() << std::endl;
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
            std::lock_guard lock(pendingChangesMutex);
            if (pendingNewPositions.empty()) {
                return;
            }
            newPositionsToApply = std::move(pendingNewPositions);
            pendingNewPositions.clear();
        }

        const size_t newCellCount = newPositionsToApply.size();

        if (transforms.size() + newCellCount > transforms.is_visible.capacity()) {
            const size_t newCapacity = (transforms.size() + newCellCount) * 1.5;
            transforms.reserve(newCapacity);
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
            trySpawningNewOctahedra(0.56f);

            if (shouldStopThread) break;

            std::this_thread::yield();
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
    std::shared_ptr<BoundaryManager> boundaryManager;

    bool gridInitialized;
    const float SPAWN_CHANCE = 0.1f;
    float octahedraSpacing;
    int octahedraLayers;
    std::vector<Vector3> startingPositions;
};
