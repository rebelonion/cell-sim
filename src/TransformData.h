#pragma once

#include <vector>

#include "raylib.h"
#include "raymath.h"

struct TransformData {
    std::vector<bool> is_visible;
    std::vector<int> neighbor_counts;

    void reserve(const size_t n) {
        is_visible.reserve(n);
        neighbor_counts.reserve(n);
    }

    void add() {
        is_visible.push_back(true);
        neighbor_counts.push_back(0);
    }

    [[nodiscard]] size_t size() const { return is_visible.size(); }

    [[nodiscard]] static Matrix getTransform(const size_t index, const Vector3 &position) {
        return MatrixTranslate(
            position.x,
            position.y,
            position.z
        );
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
