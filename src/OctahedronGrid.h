#pragma once

#include <vector>
#include <array>
#include <cmath>
#include <algorithm>
#include <cfloat>
#include <tuple>
#include "raylib.h"
#include "raymath.h"

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

    OctahedronGrid(size_t length = 500, size_t width = 500, size_t height = 500)
        : gridLength(length), gridWidth(width), gridHeight(height) {
        // Initialize the 3D matrix with empty cells
        grid.resize(gridLength * gridWidth * gridHeight, {SIZE_MAX, {0, 0, 0}});
    }

    void insert(const Vector3 &worldPos, const size_t cellIndex) {
        const Vector3 snappedPos = snapToGridPosition(worldPos);

        // Convert position to matrix index
        size_t index = positionToIndex(snappedPos);
        if (index >= grid.size()) return;

        grid[index] = {cellIndex, snappedPos};
    }

    [[nodiscard]] bool isOccupied(const Vector3 &worldPos) const {
        const Vector3 snappedPos = snapToGridPosition(worldPos);
        size_t index = positionToIndex(snappedPos);

        if (index >= grid.size()) return false;
        return grid[index].cellIndex != SIZE_MAX;
    }

    [[nodiscard]] std::vector<Vector3>
    getNeighborPositions(const Vector3 &pos, const bool filterOccupied = false) const {
        std::vector<Vector3> neighbors{};
        neighbors.reserve(14); // 6 square + 8 hex neighbors

        Vector3 snappedPos = snapToGridPosition(pos);

        // Convert to grid coordinates
        auto [x, y, z] = positionToCoordinates(snappedPos);

        // 6 square face neighbors
        std::array<std::tuple<int, int, int>, 6> squareDirs = {
            std::make_tuple(x-1, y, z),
            std::make_tuple(x+1, y, z),
            std::make_tuple(x, y-1, z),
            std::make_tuple(x, y+1, z),
            std::make_tuple(x, y, z-1),
            std::make_tuple(x, y, z+1)
        };

        for (const auto& [nx, ny, nz] : squareDirs) {
            if (isValidCoordinate(nx, ny, nz)) {
                Vector3 neighborPos = coordinatesToPosition(nx, ny, nz);
                if (!filterOccupied || !isOccupied(neighborPos)) {
                    neighbors.push_back(neighborPos);
                }
            }
        }

        // 8 hexagonal face neighbors with offset for top and bottom layers
        std::array<std::tuple<int, int, int>, 8> hexDirs = {
            std::make_tuple(x-1, y-1, z+1),
            std::make_tuple(x, y-1, z+1),
            std::make_tuple(x, y, z+1),
            std::make_tuple(x-1, y, z+1),
            std::make_tuple(x-1, y-1, z-1),
            std::make_tuple(x, y-1, z-1),
            std::make_tuple(x, y, z-1),
            std::make_tuple(x-1, y, z-1)
        };

        for (const auto& [nx, ny, nz] : hexDirs) {
            if (isValidCoordinate(nx, ny, nz)) {
                Vector3 neighborPos = coordinatesToPosition(nx, ny, nz);
                if (!filterOccupied || !isOccupied(neighborPos)) {
                    neighbors.push_back(neighborPos);
                }
            }
        }

        return neighbors;
    }

    [[nodiscard]] std::vector<CellData> getOccupiedNeighbors(const Vector3 &pos) const {
        std::vector<CellData> neighbors;
        neighbors.reserve(14);

        auto neighborPositions = getNeighborPositions(pos, false);
        for (const auto &neighborPos: neighborPositions) {
            size_t index = positionToIndex(neighborPos);
            if (index < grid.size() && grid[index].cellIndex != SIZE_MAX) {
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
        size_t index = positionToIndex(snappedPos);

        if (index >= grid.size()) return SIZE_MAX;
        return grid[index].cellIndex;
    }

    [[nodiscard]] static Vector3 snapToGridPosition(const Vector3 &position) {
        // Snap X and Y to multiples of SQUARE_DISTANCE
        const float halfSquareDist = SQUARE_DISTANCE * 0.5f;
        float snappedX = roundf(position.x / halfSquareDist) * halfSquareDist;
        float snappedY = roundf(position.y/ halfSquareDist) * halfSquareDist;

        // Snap Z to multiples of SQUARE_DISTANCE/2
        float snappedZ = roundf(position.z / halfSquareDist) * halfSquareDist;

        return {snappedX, snappedY, snappedZ};
    }

private:
    static constexpr float POSITION_EPSILON = HEXAGON_DISTANCE * 0.5f;

    size_t gridLength;
    size_t gridWidth;
    size_t gridHeight;
    std::vector<CellData> grid;

    // Convert world position to grid index
    [[nodiscard]] size_t positionToIndex(const Vector3 &pos) const {
        auto [x, y, z] = positionToCoordinates(pos);

        if (!isValidCoordinate(x, y, z)) return SIZE_MAX;

        // Using the formula: (Z_n*X*Y) + (Y_n*X) + (X_n) = n
        return (z * gridLength * gridWidth) + (y * gridLength) + x;
    }

    // Convert world position to grid coordinates
    [[nodiscard]] std::tuple<int, int, int> positionToCoordinates(const Vector3 &pos) const {
        const float halfSquareDist = SQUARE_DISTANCE;

        // First, get approximate z coordinate
        int z = static_cast<int>(roundf(pos.z / halfSquareDist));

        // Adjust x and y based on z layer (odd/even)
        float adjustedX = pos.x;
        float adjustedY = pos.y;

        // Remove the offset for odd z layers
        if (z % 2 != 0) {
            adjustedX -= halfSquareDist * 0.5f;
            adjustedY -= halfSquareDist * 0.5f;
        }

        // Now convert to grid coordinates
        int x = static_cast<int>(roundf(adjustedX / halfSquareDist));
        int y = static_cast<int>(roundf(adjustedY / halfSquareDist));

        return {x, y, z};
    }

    // Convert grid coordinates to world position
    [[nodiscard]] Vector3 coordinatesToPosition(int x, int y, int z) const {
        const float halfSquareDist = SQUARE_DISTANCE;

        float worldX = static_cast<float>(x) * halfSquareDist;
        float worldY = static_cast<float>(y) * halfSquareDist;
        float worldZ = static_cast<float>(z) * halfSquareDist;

        // Apply offset for layers based on z (odd/even)
        // For octahedral geometry, adjust for the half-unit offset between layers
        if (z % 2 != 0) {
            worldX += halfSquareDist * 0.5f;
            worldY += halfSquareDist * 0.5f;
        }

        return {worldX, worldY, worldZ};
    }

    // Check if coordinates are within grid bounds
    [[nodiscard]] bool isValidCoordinate(int x, int y, int z) const {
        return (x >= 0 && x < static_cast<int>(gridLength) &&
                y >= 0 && y < static_cast<int>(gridWidth) &&
                z >= 0 && z < static_cast<int>(gridHeight));
    }
};