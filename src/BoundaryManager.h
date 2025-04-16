#pragma once

#include <memory>
#include "raylib.h"
#include "raymath.h"

// Simplified rectangular boundary class
class RectangleBoundary {
public:
    RectangleBoundary(const Vector3 &center, float width, float depth, float height)
        : center(center), width(width), depth(depth), height(height), isResizable(true) {
    }

    [[nodiscard]] bool contains(const Vector3 &point) const {
        return point.x >= center.x - width / 2 && point.x <= center.x + width / 2 &&
               point.y >= center.y - height / 2 && point.y <= center.y + height / 2 &&
               point.z >= center.z - depth / 2 && point.z <= center.z + depth / 2;
    }

    void drawWireframe(const Color &color) const {
        const Vector3 corners[8] = {
            {center.x - width / 2, center.y - height / 2, center.z - depth / 2},
            {center.x + width / 2, center.y - height / 2, center.z - depth / 2},
            {center.x + width / 2, center.y + height / 2, center.z - depth / 2},
            {center.x - width / 2, center.y + height / 2, center.z - depth / 2},
            {center.x - width / 2, center.y - height / 2, center.z + depth / 2},
            {center.x + width / 2, center.y - height / 2, center.z + depth / 2},
            {center.x + width / 2, center.y + height / 2, center.z + depth / 2},
            {center.x - width / 2, center.y + height / 2, center.z + depth / 2}
        };

        for (int i = 0; i < 4; i++) {
            DrawLine3D(corners[i], corners[(i + 1) % 4], color);
            DrawLine3D(corners[i + 4], corners[((i + 1) % 4) + 4], color);
            DrawLine3D(corners[i], corners[i + 4], color);
        }
    }

    // Resize boundary using arrow keys
    void resize(bool right, bool left, bool up, bool down) {
        if (!isResizable) return;

        constexpr float resizeSpeed = 10.0f;

        if (right) width += resizeSpeed;
        if (left) width = std::max(50.0f, width - resizeSpeed);
        if (up) depth += resizeSpeed;
        if (down) depth = std::max(50.0f, depth - resizeSpeed);

        center = {
            width / 2 + 10,
            height / 2 + 10,
            depth / 2 + 10
        };
    }

    // Lock boundary size (called once simulation starts)
    void lockSize() {
        isResizable = false;
    }

    // Get boundary dimensions for grid sizing
    [[nodiscard]] float getWidth() const { return width; }
    [[nodiscard]] float getDepth() const { return depth; }
    [[nodiscard]] float getHeight() const { return height; }
    [[nodiscard]] Vector3 getCenter() const { return center; }
    [[nodiscard]] bool canResize() const { return isResizable; }

private:
    Vector3 center;
    float width;
    float depth;
    float height;
    bool isResizable;
};

// Simplified boundary manager (rectangle only)
class BoundaryManager {
public:
    BoundaryManager()
        : showBoundary(true), boundaryEnabled(true), boundaryColor(RED) {
        // Create default rectangular boundary centered on corner
        float width = 300.0f;
        float depth = 300.0f;
        float height = 50.0f;

        // Center point adjusted to place corner at origin
        Vector3 cornerCenter = {
            width / 2 + 10,
            height / 2 + 10,
            depth / 2 + 10
        };

        boundary = std::make_shared<RectangleBoundary>(cornerCenter, width, depth, height);
    }

    // Check if a point is within the current boundary
    [[nodiscard]] bool isPointWithinBoundary(const Vector3 &point) const {
        // If boundary is disabled or no boundary exists, allow placement anywhere
        if (!boundaryEnabled || !boundary) {
            return true;
        }

        return boundary->contains(point);
    }

    // Draw the current boundary
    void draw() const {
        if (boundary && showBoundary && boundaryEnabled) {
            boundary->drawWireframe(boundaryColor);
        }
    }

    // Toggle boundary visibility
    void toggleVisibility() {
        showBoundary = !showBoundary;
    }

    // Enable or disable the boundary constraint
    void setBoundaryEnabled(bool enabled) {
        boundaryEnabled = enabled;
    }

    // Toggle boundary constraint on/off
    void toggleBoundaryEnabled() {
        boundaryEnabled = !boundaryEnabled;
    }

    // Check if boundary is currently enabled
    [[nodiscard]] bool isBoundaryEnabled() const {
        return boundaryEnabled;
    }

    // Set boundary color
    void setColor(const Color &color) {
        boundaryColor = color;
    }

    // Handle arrow key resizing
    void handleResizing() {
        if (!boundary || !boundary->canResize()) return;

        bool right = IsKeyDown(KEY_RIGHT);
        bool left = IsKeyDown(KEY_LEFT);
        bool up = IsKeyDown(KEY_UP);
        bool down = IsKeyDown(KEY_DOWN);

        if (right || left || up || down) {
            boundary->resize(right, left, up, down);
        }
    }

    // Lock boundary size (called once simulation starts)
    void lockBoundarySize() {
        if (boundary) {
            boundary->lockSize();
        }
    }

    // Get boundary dimensions for grid sizing
    [[nodiscard]] float getBoundaryWidth() const {
        return boundary ? boundary->getWidth() : 300.0f;
    }

    [[nodiscard]] float getBoundaryDepth() const {
        return boundary ? boundary->getDepth() : 300.0f;
    }

    [[nodiscard]] float getBoundaryHeight() const {
        return boundary ? boundary->getHeight() : 50.0f;
    }

    [[nodiscard]] Vector3 getBoundaryCenter() const {
        return boundary ? boundary->getCenter() : Vector3{0, 0, 0};
    }

private:
    std::shared_ptr<RectangleBoundary> boundary;
    bool showBoundary; // Controls visual rendering of boundary
    bool boundaryEnabled; // Controls whether boundary constraint is enforced
    Color boundaryColor;
};
