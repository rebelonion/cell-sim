#pragma once

#include <vector>
#include <array>
#include <cmath>
#include <tuple>
#include <unordered_map>
#include "raylib.h"

class OctahedronGrid {
public:
    struct CellData {
        size_t cellIndex;
        Vector3 position;
    };

    static constexpr float SQUARE_DISTANCE = 2.0f * 2.82842712475f;
    static constexpr float HEXAGON_DISTANCE = SQUARE_DISTANCE * 0.866025404f;

    struct NeighborAvailability {
        std::vector<Vector3> positions;
        int squareFaces = 0;
        int hexagonFaces = 0;
    };

    explicit OctahedronGrid(const size_t length = 50, const size_t width = 50, const size_t height = 50)
        : gridLength(length), gridWidth(width), gridHeight(height) {
        resizeGrid(length, width, height);
    }

    void resizeGrid(const size_t length, const size_t width, const size_t height) {
        gridLength = length;
        gridWidth = width;
        gridHeight = height;
        
        grid.resize(gridLength * gridWidth * gridHeight, {SIZE_MAX, {0, 0, 0}});
        indexToPositionMap.reserve(length * width * height);
    }

    ~OctahedronGrid() {
        indexToPositionMap.clear();
        grid.clear();
    }

    void insert(const Vector3 &worldPos, const size_t cellIndex) {
        const Vector3 snappedPos = snapToGridPosition(worldPos);
        const size_t index = positionToIndex(snappedPos);
        if (index >= grid.size()) return;

        grid[index] = {cellIndex, snappedPos};
        indexToPositionMap[cellIndex] = snappedPos;
    }

    [[nodiscard]] bool isOccupied(const Vector3 &worldPos) const {
        const Vector3 snappedPos = snapToGridPosition(worldPos);
        const size_t index = positionToIndex(snappedPos);
        if (index >= grid.size()) return false;

        return grid[index].cellIndex != SIZE_MAX;
    }

    [[nodiscard]] std::vector<Vector3>
    getNeighborPositions(const Vector3 &pos, const bool filterOccupied = false) const {
        std::vector<Vector3> neighbors{};
        neighbors.reserve(14); // 6 square + 8 hex neighbors

        const Vector3 snappedPos = snapToGridPosition(pos);
        auto [x, y, z] = positionToCoordinates(snappedPos);

        // 6 square face neighbors
        std::array<std::tuple<int, int, int>, 6> squareDirs = {
            std::make_tuple(x - 1, y, z),
            std::make_tuple(x + 1, y, z),
            std::make_tuple(x, y, z - 1),
            std::make_tuple(x, y, z + 1),
            std::make_tuple(x, y - 1, z),
            std::make_tuple(x, y + 1, z)
        };

        for (const auto &[nx, ny, nz]: squareDirs) {
            if (isValidCoordinate(nx, ny, nz)) {
                if (Vector3 neighborPos = coordinatesToPosition(nx, ny, nz);
                    !filterOccupied || !isOccupied(neighborPos)) {
                    neighbors.push_back(neighborPos);
                }
            }
        }

        // 8 hexagonal face neighbors with offset for top and bottom layers
        std::array<std::tuple<int, int, int>, 8> hexDirs = {
            std::make_tuple(x - 1, y + 1, z - 1),
            std::make_tuple(x, y + 1, z - 1),
            std::make_tuple(x, y + 1, z),
            std::make_tuple(x - 1, y + 1, z),
            std::make_tuple(x - 1, y - 1, z - 1),
            std::make_tuple(x, y - 1, z - 1),
            std::make_tuple(x, y - 1, z),
            std::make_tuple(x - 1, y - 1, z)
        };

        for (const auto &[nx, ny, nz]: hexDirs) {
            if (isValidCoordinate(nx, ny, nz)) {
                if (Vector3 neighborPos = coordinatesToPosition(nx, ny, nz);
                    !filterOccupied || !isOccupied(neighborPos)) {
                    neighbors.push_back(neighborPos);
                }
            }
        }

        return neighbors;
    }

    [[nodiscard]] std::vector<CellData> getOccupiedNeighbors(const Vector3 &pos) const {
        std::vector<CellData> neighbors;
        neighbors.reserve(14);

        for (auto neighborPositions = getNeighborPositions(pos, false); const auto &neighborPos: neighborPositions) {
            if (const size_t index = positionToIndex(neighborPos);
                index < grid.size() && grid[index].cellIndex != SIZE_MAX) {
                neighbors.push_back(grid[index]);
            }
        }

        return neighbors;
    }

    [[nodiscard]] std::vector<Vector3> getAvailableNeighbors(const Vector3 &pos) const {
        return getNeighborPositions(pos, true);
    }

    [[nodiscard]] size_t findCellIndex(const Vector3 &worldPos) const {
        const Vector3 snappedPos = snapToGridPosition(worldPos);
        const size_t index = positionToIndex(snappedPos);

        if (index >= grid.size()) return SIZE_MAX;
        return grid[index].cellIndex;
    }

    [[nodiscard]] Vector3 getPositionForIndex(const size_t cellIndex) const {
        // O(1) lookup using the map
        if (const auto it = indexToPositionMap.find(cellIndex); it != indexToPositionMap.end()) {
            return it->second;
        }
        return {0.0f, 0.0f, 0.0f};
    }

    [[nodiscard]] static Vector3 snapToGridPosition(const Vector3 &position) {
        constexpr float halfSquareDist = SQUARE_DISTANCE * 0.5f;
        const float snappedY = roundf(position.y / halfSquareDist) * halfSquareDist;

        const float snappedX = roundf(position.x / halfSquareDist) * halfSquareDist;
        const float snappedZ = roundf(position.z / halfSquareDist) * halfSquareDist;

        /*const float distanceFromSquare = std::abs(snappedY - SQUARE_DISTANCE);
        const float distanceFromHalfSquare = std::abs(snappedY - halfSquareDist);
        const bool closerToWholeSquare = distanceFromSquare < distanceFromHalfSquare;
        float snappedX, snappedZ;
        if (!closerToWholeSquare) {
            snappedX = roundf(position.x / halfSquareDist) * halfSquareDist;
            snappedZ = roundf(position.z / halfSquareDist) * halfSquareDist;
        } else {
            snappedX = roundf(position.x / SQUARE_DISTANCE) * SQUARE_DISTANCE;
            snappedZ = roundf(position.z / SQUARE_DISTANCE) * SQUARE_DISTANCE;
        }*/

        return {snappedX, snappedY, snappedZ};
    }

private:
    static constexpr float POSITION_EPSILON = HEXAGON_DISTANCE * 0.5f;

    size_t gridLength;
    size_t gridWidth;
    size_t gridHeight;
    std::vector<CellData> grid;
    std::unordered_map<size_t, Vector3> indexToPositionMap;

    [[nodiscard]] size_t positionToIndex(const Vector3 &pos) const {
        auto [x, y, z] = positionToCoordinates(pos);
        if (!isValidCoordinate(x, y, z)) return SIZE_MAX;

        // Using the formula: (Y_n*X*Z) + (Z_n*X) + (X_n) = n
        return (y * gridLength * gridWidth) + (z * gridLength) + x;
    }

    [[nodiscard]] static std::tuple<int, int, int> positionToCoordinates(const Vector3 &pos) {
        constexpr float halfSquareDist = SQUARE_DISTANCE;
        constexpr float yScaleFactor = 2.0f; // Inverse of the 0.5 scale factor

        int y = static_cast<int>(roundf(pos.y / halfSquareDist * yScaleFactor));
        float adjustedX = pos.x;
        float adjustedZ = pos.z;

        if (y % 2 != 0) {
            adjustedX -= halfSquareDist * 0.5f;
            adjustedZ -= halfSquareDist * 0.5f;
        }

        int x = static_cast<int>(roundf(adjustedX / halfSquareDist));
        int z = static_cast<int>(roundf(adjustedZ / halfSquareDist));

        return {x, y, z};
    }

    [[nodiscard]] static Vector3 coordinatesToPosition(const int x, const int y, const int z) {
        constexpr float halfSquareDist = SQUARE_DISTANCE;
        constexpr float yScaleFactor = 0.5f;

        float worldX = static_cast<float>(x) * halfSquareDist;
        float worldZ = static_cast<float>(z) * halfSquareDist;
        const float worldY = static_cast<float>(y) * halfSquareDist * yScaleFactor;

        // For octahedral geometry, adjust for the half-unit offset between layers
        if (y % 2 != 0) {
            worldX += halfSquareDist * 0.5f;
            worldZ += halfSquareDist * 0.5f;
        }

        return {worldX, worldY, worldZ};
    }

    [[nodiscard]] bool isValidCoordinate(const int x, const int y, const int z) const {
        return x >= 0 && x < static_cast<int>(gridLength) &&
               y >= 0 && y < static_cast<int>(gridHeight) &&
               z >= 0 && z < static_cast<int>(gridWidth);
    }
};
