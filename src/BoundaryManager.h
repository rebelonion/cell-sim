#pragma once

#include <memory>
#include "raylib.h"

class RectangleBoundary {
public:
    RectangleBoundary(const Vector3 &center, const float width, const float depth, const float height)
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

    void resize(const bool right, const bool left, const bool up, const bool down) {
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

    void lockSize() {
        isResizable = false;
    }

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

class BoundaryManager {
public:
    BoundaryManager()
        : showBoundary(true), boundaryEnabled(true), boundaryColor(RED) {
        float width = 700.0f;
        float depth = 700.0f;
        float height = 50.0f;

        Vector3 cornerCenter = {
            width / 2 + 10,
            height / 2 + 10,
            depth / 2 + 10
        };

        boundary = std::make_shared<RectangleBoundary>(cornerCenter, width, depth, height);
    }

    [[nodiscard]] bool isPointWithinBoundary(const Vector3 &point) const {
        if (!boundaryEnabled || !boundary) {
            return true;
        }

        return boundary->contains(point);
    }

    void draw() const {
        if (boundary && showBoundary && boundaryEnabled) {
            boundary->drawWireframe(boundaryColor);
        }
    }

    void toggleVisibility() {
        showBoundary = !showBoundary;
    }

    void setBoundaryEnabled(const bool enabled) {
        boundaryEnabled = enabled;
    }

    void toggleBoundaryEnabled() {
        boundaryEnabled = !boundaryEnabled;
    }

    [[nodiscard]] bool isBoundaryEnabled() const {
        return boundaryEnabled;
    }

    void setColor(const Color &color) {
        boundaryColor = color;
    }
    
    void setBoundaryVisible(const bool visible) {
        showBoundary = visible;
    }
    
    void setBoundaryWidth(const float width) {
        if (boundary && boundary->canResize()) {
            boundary = std::make_shared<RectangleBoundary>(
                boundary->getCenter(),
                width,
                boundary->getDepth(),
                boundary->getHeight()
            );
        }
    }
    
    void setBoundaryDepth(const float depth) {
        if (boundary && boundary->canResize()) {
            boundary = std::make_shared<RectangleBoundary>(
                boundary->getCenter(),
                boundary->getWidth(),
                depth,
                boundary->getHeight()
            );
        }
    }
    
    void setBoundaryHeight(const float height) {
        if (boundary && boundary->canResize()) {
            boundary = std::make_shared<RectangleBoundary>(
                boundary->getCenter(),
                boundary->getWidth(),
                boundary->getDepth(),
                height
            );
        }
    }

    void handleResizing() const {
        if (!boundary || !boundary->canResize()) return;
        const bool right = IsKeyDown(KEY_RIGHT);
        const bool left = IsKeyDown(KEY_LEFT);
        const bool up = IsKeyDown(KEY_UP);
        const bool down = IsKeyDown(KEY_DOWN);

        if (right || left || up || down) {
            boundary->resize(right, left, up, down);
        }
    }

    void lockBoundarySize() const {
        if (boundary) {
            boundary->lockSize();
        }
    }

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
    bool showBoundary;
    bool boundaryEnabled;
    Color boundaryColor;
};
