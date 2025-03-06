#pragma once

#include <vector>
#include <array>
#include <optional>
#include <cmath>
#include <algorithm>
#include <tuple>
#include "raylib.h"
#include "raymath.h"

// Class to efficiently store and access truncated octahedra in a 3D space
// Optimized for handling 2+ million cells with spatial hashing
class OctahedronGrid {
public:
    // Cell data stored at each occupied grid position
    struct CellData {
        size_t cellIndex;  // Index in the main cell array
        Vector3 position;  // Actual world position
    };
    
    // Spacing parameters for the grid (based on truncated octahedron geometry)
    static constexpr float SQUARE_DISTANCE = 2.0f * 2.828f;
    static constexpr float HEXAGON_DISTANCE = SQUARE_DISTANCE * 0.866025404f; // sqrt(3)/2

    // Struct to hold statistics about available positions
    struct NeighborAvailability {
        std::vector<Vector3> positions;
        int squareFaces = 0;
        int hexagonFaces = 0;
    };

    static constexpr size_t HASH_TABLE_SIZE = 262144; // 2^18 for better distribution with large datasets
    
    // Constructor 
    OctahedronGrid() {
        // Initialize the spatial hash table - size for ~2M cells with good load factor
        spatialHash.resize(HASH_TABLE_SIZE);
    }
    
    // Insert a cell at the given world position
    void insert(const Vector3& worldPos, size_t cellIndex) {
        // Create cell data and store at the hash index
        CellData data{cellIndex, worldPos};
        auto hashIndex = positionToHashIndex(worldPos);
        
        // Reserve space for the expected bucket size to avoid frequent reallocations
        if (spatialHash[hashIndex].empty()) {
            spatialHash[hashIndex].reserve(8); // Most buckets have few entries
        }
        
        spatialHash[hashIndex].push_back(data);
    }
    
    // Remove a cell at the given world position
    void remove(const Vector3& worldPos) {
        auto hashIndex = positionToHashIndex(worldPos);
        auto& bucket = spatialHash[hashIndex];
        
        // Find and remove the exact position - using erase-remove idiom
        bucket.erase(
            std::remove_if(bucket.begin(), bucket.end(),
                [&](const CellData& cell) {
                    return Vector3Distance(cell.position, worldPos) < POSITION_EPSILON;
                }),
            bucket.end()
        );
    }
    
    // Check if a world position is already occupied
    [[nodiscard]] bool isOccupied(const Vector3& worldPos) const {
        auto hashIndex = positionToHashIndex(worldPos);
        const auto& bucket = spatialHash[hashIndex];
        
        if (bucket.empty()) return false; // Fast return for empty buckets
        
        // Check if any cell in this bucket is close to our position
        for (const auto& cell : bucket) {
            if (Vector3Distance(cell.position, worldPos) < POSITION_EPSILON) {
                return true;
            }
        }
        return false;
    }
    
    // Get all 14 neighboring positions (relative to given position)
    [[nodiscard]] std::array<Vector3, 14> getNeighborPositions(const Vector3& pos) const {
        std::array<Vector3, 14> neighbors;
        
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
            neighbors[i+6] = Vector3Add(pos, Vector3Scale(hexDirs[i], HEXAGON_DISTANCE));
        }
        
        return neighbors;
    }
    
    // Highly optimized version - check multiple positions at once
    // Uses loop unrolling and early returns for speed
    [[nodiscard]] NeighborAvailability getAvailableNeighborsWithStats(const Vector3& pos) const {
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
        
        // Check square neighbors (manually unrolled for performance)
        for (int i = 0; i < 6; i++) {
            if (!isOccupied(squareNeighbors[i])) {
                result.positions.push_back(squareNeighbors[i]);
                result.squareFaces++;
            }
        }
        
        // Prepare hexagonal face directions
        constexpr float s = 0.577350269f;
        const Vector3 hexDirs[8] = {
            {s, s, s}, {-s, s, s}, {s, -s, s}, {s, s, -s},
            {-s, -s, s}, {-s, s, -s}, {s, -s, -s}, {-s, -s, -s}
        };
        
        // Check hexagon neighbors 
        for (int i = 0; i < 8; i++) {
            Vector3 hexPos = Vector3Add(pos, Vector3Scale(hexDirs[i], HEXAGON_DISTANCE));
            if (!isOccupied(hexPos)) {
                result.positions.push_back(hexPos);
                result.hexagonFaces++;
            }
        }
        
        return result;
    }
    
    // Get available (unoccupied) neighboring positions
    [[nodiscard]] std::vector<Vector3> getAvailableNeighborPositions(const Vector3& pos) const {
        auto result = getAvailableNeighborsWithStats(pos);
        return result.positions;
    }
    
    // Get neighbor cell data if occupied
    [[nodiscard]] std::vector<CellData> getOccupiedNeighbors(const Vector3& pos) const {
        std::vector<CellData> neighbors;
        neighbors.reserve(14);
        
        auto neighborPositions = getNeighborPositions(pos);
        for (const auto& neighborPos : neighborPositions) {
            auto hashIndex = positionToHashIndex(neighborPos);
            const auto& bucket = spatialHash[hashIndex];
            
            // Skip empty buckets
            if (bucket.empty()) continue;
            
            for (const auto& cell : bucket) {
                if (Vector3Distance(cell.position, neighborPos) < POSITION_EPSILON) {
                    neighbors.push_back(cell);
                    break; // Found the matching cell, no need to check more
                }
            }
        }
        
        return neighbors;
    }
    
    // Get hash table statistics for performance monitoring
    [[nodiscard]] std::tuple<size_t, size_t, size_t, double> getHashStats() const {
        size_t nonEmptyBuckets = 0;
        size_t maxBucketSize = 0;
        size_t totalItems = 0;
        
        for (const auto& bucket : spatialHash) {
            size_t bucketSize = bucket.size();
            if (bucketSize > 0) {
                nonEmptyBuckets++;
                maxBucketSize = std::max(maxBucketSize, bucketSize);
                totalItems += bucketSize;
            }
        }
        
        double avgBucketSize = nonEmptyBuckets > 0 ? 
            static_cast<double>(totalItems) / nonEmptyBuckets : 0.0;
            
        return {nonEmptyBuckets, maxBucketSize, totalItems, avgBucketSize};
    }

private:
    // Small epsilon for position comparison to handle floating point precision
    static constexpr float POSITION_EPSILON = 0.001f;
    
    // Spatial hash table (buckets of cells)
    std::vector<std::vector<CellData>> spatialHash;
    // High-performance spatial hash function optimized for truncated octahedra
    // Based on research for large-scale spatial hashing
    [[nodiscard]] size_t positionToHashIndex(const Vector3& pos) const {
        // Scale by cell size first for better distribution
        float cellSize = SQUARE_DISTANCE * 0.5f;
        
        // Quantize to integer cells with enough precision
        int64_t x = static_cast<int64_t>(pos.x / cellSize);
        int64_t y = static_cast<int64_t>(pos.y / cellSize);
        int64_t z = static_cast<int64_t>(pos.z / cellSize);
        
        // Use prime multipliers for better distribution
        // Based on research for spatial hashing
        const int64_t p1 = 73856093;
        const int64_t p2 = 19349663;
        const int64_t p3 = 83492791;
        
        // Hash the coordinates
        int64_t hash = ((x * p1) ^ (y * p2) ^ (z * p3)) & 0x7FFFFFFF;
        
        // Take modulo of table size
        return static_cast<size_t>(hash % HASH_TABLE_SIZE);
    }
};