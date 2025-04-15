#pragma once

#include <vector>
#include <memory>
#include <random>
#include "raylib.h"
#include "raymath.h"

// Represents a 3D boundary shape where octahedra will be contained
class BoundaryShape {
public:
    virtual ~BoundaryShape() = default;
    
    // Check if a 3D point is inside the boundary
    [[nodiscard]] virtual bool contains(const Vector3& point) const = 0;
    
    // Draw the wireframe representation of the boundary
    virtual void drawWireframe(const Color& color) const = 0;
};

// A cylindrical boundary shape
class CylindricalBoundary : public BoundaryShape {
public:
    CylindricalBoundary(const Vector3& center, float radius, float height)
        : center(center), radius(radius), height(height) {}
    
    [[nodiscard]] bool contains(const Vector3& point) const override {
        // Check if within cylinder
        float dx = point.x - center.x;
        float dz = point.z - center.z;
        float distanceSquared = dx * dx + dz * dz;
        
        return distanceSquared <= radius * radius && 
               point.y >= center.y - height/2 && 
               point.y <= center.y + height/2;
    }
    
    void drawWireframe(const Color& color) const override {
        // Draw top and bottom circles
        DrawCircle3D({center.x, center.y + height/2, center.z}, radius, {1, 0, 0}, 90.0f, color);
        DrawCircle3D({center.x, center.y - height/2, center.z}, radius, {1, 0, 0}, 90.0f, color);
        
        // Draw vertical lines
        constexpr int segments = 16;
        for (int i = 0; i < segments; i++) {
            float angle = (2 * PI * i) / segments;
            float x = center.x + radius * cosf(angle);
            float z = center.z + radius * sinf(angle);
            
            DrawLine3D(
                {x, center.y - height/2, z},
                {x, center.y + height/2, z},
                color
            );
        }
    }
    
private:
    Vector3 center;
    float radius;
    float height;
};

//a very large horizontal rectangle boundary generator
class LargeRectangleBoundary : public BoundaryShape {
public:
    LargeRectangleBoundary(const Vector3& center, float width, float depth, float height)
        : center(center), width(width), depth(depth), height(height) {}

    [[nodiscard]] bool contains(const Vector3& point) const override {
        return point.x >= center.x - width / 2 && point.x <= center.x + width / 2 &&
       point.y >= center.y - height / 2 && point.y <= center.y + height / 2 &&
       point.z >= center.z - depth / 2 && point.z <= center.z + depth / 2;
    }

    void drawWireframe(const Color& color) const override {
        Vector3 corners[8] = {
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

private:
    Vector3 center;
    float width;
    float depth;
    float height;
};

// Main class to manage boundary generation and rendering
class BoundaryManager {
public:
    BoundaryManager() : showBoundary(true), boundaryEnabled(true), boundaryColor(RED) {
        generateRandomBoundary();
    }
    
    // Check if a point is within the current boundary
    [[nodiscard]] bool isPointWithinBoundary(const Vector3& point) const {
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
    void setColor(const Color& color) {
        boundaryColor = color;
    }
    
    // Generate a new random boundary
    void generateRandomBoundary() {
        std::random_device rd;
        std::mt19937 gen(rd());
        
        // Randomly choose between different boundary shapes
        std::uniform_int_distribution<> shapeDist(0, 1);
        int shapeType = shapeDist(gen);
        
        if (shapeType == 0) {
            // Random cylindrical boundary
            std::uniform_real_distribution<float> radiusDist(80.0f, 180.0f);
            std::uniform_real_distribution<float> heightDist(150.0f, 300.0f);
            
            float radius = radiusDist(gen);
            float height = heightDist(gen);
            
            boundary = std::make_shared<CylindricalBoundary>(Vector3{0, 0, 0}, radius, height);
        }
        else {
            // Large horizontal rectangle
            std::uniform_real_distribution<float> widthDist(300.0f, 400.0f);
            std::uniform_real_distribution<float> depthDist(300.0f, 400.0f);
            std::uniform_real_distribution<float> heightDist(30.0f, 70.0f);

            float width = widthDist(gen);
            float depth = depthDist(gen);
            float height = heightDist(gen);

            boundary = std::make_shared<LargeRectangleBoundary>(Vector3{0, 0, 0}, width, depth, height);
        }
        
        // Generate a random color for the boundary
        std::uniform_int_distribution<> colorDist(0, 5);
        
        switch (colorDist(gen)) {
            case 0: boundaryColor = RED; break;
            case 1: boundaryColor = GREEN; break;
            case 2: boundaryColor = BLUE; break;
            case 3: boundaryColor = PURPLE; break;
            case 4: boundaryColor = ORANGE; break;
            default: boundaryColor = SKYBLUE; break;
        }
    }
    
private:
    std::shared_ptr<BoundaryShape> boundary;
    bool showBoundary;      // Controls visual rendering of boundary
    bool boundaryEnabled;   // Controls whether boundary constraint is enforced
    Color boundaryColor;
};