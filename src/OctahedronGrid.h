#pragma once

#include <vector>
#include <array>
#include <cmath>
#include <algorithm>
#include <tuple>
#include "raylib.h"
#include "raymath.h"

// Class to efficiently store and access truncated octahedra in a 3D space
class OctahedronGrid {
public:
    struct CellData {
        size_t cellIndex;
        Vector3 position;
    };

    static constexpr float SQUARE_DISTANCE = 2.0f * 2.828f;
    static constexpr float HEXAGON_DISTANCE = SQUARE_DISTANCE * 0.866025404f;

    struct NeighborAvailability {
        std::vector<Vector3> positions;
        int squareFaces = 0;
        int hexagonFaces = 0;
    };

    static constexpr size_t HASH_TABLE_SIZE = 262144;

    OctahedronGrid() {
        spatialHash.resize(HASH_TABLE_SIZE);
    }

    void insert(const Vector3 &worldPos, const size_t cellIndex) {
        const CellData data{cellIndex, worldPos};
        const auto hashIndex = positionToHashIndex(worldPos);

        if (spatialHash[hashIndex].empty()) {
            spatialHash[hashIndex].reserve(8);
        }

        spatialHash[hashIndex].push_back(data);
    }

    void remove(const Vector3 &worldPos) {
        const auto hashIndex = positionToHashIndex(worldPos);
        auto &bucket = spatialHash[hashIndex];

        std::erase_if(bucket,
                      [&](const CellData &cell) {
                          return Vector3Distance(cell.position, worldPos) < POSITION_EPSILON;
                      });
    }

    [[nodiscard]] bool isOccupied(const Vector3 &worldPos) const {
        const auto hashIndex = positionToHashIndex(worldPos);
        const auto &bucket = spatialHash[hashIndex];

        if (bucket.empty()) return false;

        return std::ranges::any_of(bucket, [&](const CellData &cell) {
            return Vector3Distance(cell.position, worldPos) < POSITION_EPSILON;
        });
    }

    [[nodiscard]] static std::array<Vector3, 14> getNeighborPositions(const Vector3 &pos) {
        std::array<Vector3, 14> neighbors{};

        // 6 square face neighbors
        neighbors[0] = {pos.x + SQUARE_DISTANCE, pos.y, pos.z};
        neighbors[1] = {pos.x - SQUARE_DISTANCE, pos.y, pos.z};
        neighbors[2] = {pos.x, pos.y + SQUARE_DISTANCE, pos.z};
        neighbors[3] = {pos.x, pos.y - SQUARE_DISTANCE, pos.z};
        neighbors[4] = {pos.x, pos.y, pos.z + SQUARE_DISTANCE};
        neighbors[5] = {pos.x, pos.y, pos.z - SQUARE_DISTANCE};

        // 8 hexagonal face neighbors
        constexpr float s = 0.577350269f;
        Vector3 hexDirs[8] = {
            {s, s, s}, {-s, s, s}, {s, -s, s}, {s, s, -s},
            {-s, -s, s}, {-s, s, -s}, {s, -s, -s}, {-s, -s, -s}
        };

        for (int i = 0; i < 8; i++) {
            neighbors[i + 6] = Vector3Add(pos, Vector3Scale(hexDirs[i], HEXAGON_DISTANCE));
        }

        return neighbors;
    }

    [[nodiscard]] NeighborAvailability getAvailableNeighborsWithStats(const Vector3 &pos) const {
        NeighborAvailability result;
        result.positions.reserve(14);

        const Vector3 squareNeighbors[6] = {
            {pos.x + SQUARE_DISTANCE, pos.y, pos.z},
            {pos.x - SQUARE_DISTANCE, pos.y, pos.z},
            {pos.x, pos.y + SQUARE_DISTANCE, pos.z},
            {pos.x, pos.y - SQUARE_DISTANCE, pos.z},
            {pos.x, pos.y, pos.z + SQUARE_DISTANCE},
            {pos.x, pos.y, pos.z - SQUARE_DISTANCE}
        };

        for (auto squareNeighbor: squareNeighbors) {
            if (!isOccupied(squareNeighbor)) {
                result.positions.push_back(squareNeighbor);
                result.squareFaces++;
            }
        }

        constexpr float s = 0.577350269f;
        const Vector3 hexDirs[8] = {
            {s, s, s}, {-s, s, s}, {s, -s, s}, {s, s, -s},
            {-s, -s, s}, {-s, s, -s}, {s, -s, -s}, {-s, -s, -s}
        };

        for (const auto hexDir: hexDirs) {
            if (Vector3 hexPos = Vector3Add(pos, Vector3Scale(hexDir, HEXAGON_DISTANCE)); !isOccupied(hexPos)) {
                result.positions.push_back(hexPos);
                result.hexagonFaces++;
            }
        }

        return result;
    }

    [[nodiscard]] std::vector<Vector3> getAvailableNeighborPositions(const Vector3 &pos) const {
        auto result = getAvailableNeighborsWithStats(pos);
        return result.positions;
    }

    [[nodiscard]] std::vector<CellData> getOccupiedNeighbors(const Vector3 &pos) const {
        std::vector<CellData> neighbors;
        neighbors.reserve(14);

        auto neighborPositions = getNeighborPositions(pos);
        for (const auto &neighborPos: neighborPositions) {
            auto hashIndex = positionToHashIndex(neighborPos);
            const auto &bucket = spatialHash[hashIndex];

            if (bucket.empty()) continue;

            for (const auto &cell: bucket) {
                if (Vector3Distance(cell.position, neighborPos) < POSITION_EPSILON) {
                    neighbors.push_back(cell);
                    break;
                }
            }
        }

        return neighbors;
    }

    [[nodiscard]] size_t findCellIndex(const Vector3 &worldPos) const {
        const auto hashIndex = positionToHashIndex(worldPos);
        const auto &bucket = spatialHash[hashIndex];

        if (bucket.empty()) return SIZE_MAX;

        for (const auto &[cellIndex, position]: bucket) {
            if (Vector3Distance(position, worldPos) < POSITION_EPSILON) {
                return cellIndex;
            }
        }

        return SIZE_MAX;
    }

    [[nodiscard]] std::tuple<size_t, size_t, size_t, double> getHashStats() const {
        size_t nonEmptyBuckets = 0;
        size_t maxBucketSize = 0;
        size_t totalItems = 0;

        for (const auto &bucket: spatialHash) {
            if (size_t bucketSize = bucket.size(); bucketSize > 0) {
                nonEmptyBuckets++;
                maxBucketSize = std::max(maxBucketSize, bucketSize);
                totalItems += bucketSize;
            }
        }

        double avgBucketSize = nonEmptyBuckets > 0 ? static_cast<double>(totalItems) / nonEmptyBuckets : 0.0;

        return {nonEmptyBuckets, maxBucketSize, totalItems, avgBucketSize};
    }

private:
    static constexpr float POSITION_EPSILON = 0.001f;

    std::vector<std::vector<CellData> > spatialHash;

    [[nodiscard]] static size_t positionToHashIndex(const Vector3 &pos) {
        constexpr float cellSize = SQUARE_DISTANCE * 0.5f;

        const auto x = static_cast<int64_t>(pos.x / cellSize);
        const auto y = static_cast<int64_t>(pos.y / cellSize);
        const auto z = static_cast<int64_t>(pos.z / cellSize);

        constexpr int64_t p1 = 73856093;
        constexpr int64_t p2 = 19349663;
        constexpr int64_t p3 = 83492791;

        const int64_t hash = ((x * p1) ^ (y * p2) ^ (z * p3)) & 0x7FFFFFFF;

        return hash % HASH_TABLE_SIZE;
    }
};
