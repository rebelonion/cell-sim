#pragma once

#include <vector>
#include <execution>

#include "raylib.h"
#include "raymath.h"

class SpatialGrid {
public:
    SpatialGrid() : cells(TOTAL_CELLS) {
    }

    void insert(const Vector3 &pos, const size_t transformIndex) {
        const size_t cellIndex = getIndex(pos);
        cells[cellIndex].indices.push_back(transformIndex);
        cells[cellIndex].positions.push_back(pos);
    }

    [[nodiscard]] bool isPositionOccupied(const Vector3 &pos) const {
        const size_t cellIndex = getIndex(pos);

        for (const auto &[indices, positions] = cells[cellIndex]; const auto &existingPos: positions) {
            if (constexpr float epsilon = 0.1f; Vector3Distance(pos, existingPos) < epsilon) {
                return true;
            }
        }
        return false;
    }

private:
    static constexpr float CELL_SIZE = 20.0f; // Matching CHUNK_SIZE
    static constexpr size_t GRID_SIZE = 512; // Adjust based on world size
    static constexpr size_t TOTAL_CELLS = GRID_SIZE * GRID_SIZE * GRID_SIZE;

    struct Cell {
        std::vector<size_t> indices; // Indices into the transform array
        std::vector<Vector3> positions; // Actual positions within this cell
    };

    std::vector<Cell> cells;

    [[nodiscard]] static size_t getIndex(const Vector3 &pos) {
        int x = static_cast<int>((pos.x + GRID_SIZE * CELL_SIZE * 0.5f) / CELL_SIZE);
        int y = static_cast<int>((pos.y + GRID_SIZE * CELL_SIZE * 0.5f) / CELL_SIZE);
        int z = static_cast<int>((pos.z + GRID_SIZE * CELL_SIZE * 0.5f) / CELL_SIZE);

        x = std::clamp(x, 0, static_cast<int>(GRID_SIZE) - 1);
        y = std::clamp(y, 0, static_cast<int>(GRID_SIZE) - 1);
        z = std::clamp(z, 0, static_cast<int>(GRID_SIZE) - 1);

        return x + y * GRID_SIZE + z * GRID_SIZE * GRID_SIZE;
    }
};
