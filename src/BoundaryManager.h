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

// A custom polygonal prism boundary shape (extruded 2D polygon)
class PolygonalPrismBoundary : public BoundaryShape {
public:
    PolygonalPrismBoundary(const std::vector<Vector2>& basePoints, float minY, float maxY)
        : basePoints(basePoints), minY(minY), maxY(maxY) {}
    
    [[nodiscard]] bool contains(const Vector3& point) const override {
        // Check height bounds first (fast rejection)
        if (point.y < minY || point.y > maxY) {
            return false;
        }
        
        // Check if point is inside the polygon (using ray casting algorithm)
        return isPointInPolygon({point.x, point.z}, basePoints);
    }
    
    void drawWireframe(const Color& color) const override {
        // Draw base and top polygons
        const size_t numPoints = basePoints.size();
        for (size_t i = 0; i < numPoints; i++) {
            size_t nextIdx = (i + 1) % numPoints;
            
            // Draw bottom edges
            DrawLine3D(
                {basePoints[i].x, minY, basePoints[i].y},
                {basePoints[nextIdx].x, minY, basePoints[nextIdx].y},
                color
            );
            
            // Draw top edges
            DrawLine3D(
                {basePoints[i].x, maxY, basePoints[i].y},
                {basePoints[nextIdx].x, maxY, basePoints[nextIdx].y},
                color
            );
            
            // Draw vertical edges
            DrawLine3D(
                {basePoints[i].x, minY, basePoints[i].y},
                {basePoints[i].x, maxY, basePoints[i].y},
                color
            );
        }
    }
    
private:
    std::vector<Vector2> basePoints;
    float minY;
    float maxY;
    
    // Ray casting algorithm to determine if a point is inside a polygon
    [[nodiscard]] static bool isPointInPolygon(const Vector2& point, const std::vector<Vector2>& polygon) {
        bool inside = false;
        const size_t numPoints = polygon.size();
        
        for (size_t i = 0, j = numPoints - 1; i < numPoints; j = i++) {
            if (((polygon[i].y > point.y) != (polygon[j].y > point.y)) &&
                (point.x < (polygon[j].x - polygon[i].x) * (point.y - polygon[i].y) / 
                (polygon[j].y - polygon[i].y) + polygon[i].x)) {
                inside = !inside;
            }
        }
        
        return inside;
    }
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
    BoundaryManager() : showBoundary(true), boundaryColor(RED), boundaryEnabled(true) {
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
        std::uniform_int_distribution<> shapeDist(0, 3);
        int shapeType = shapeDist(gen);
        
        if (shapeType == 0) {
            // Random star-like shape
            std::vector<Vector2> randomShape;
            std::uniform_int_distribution<> pointsDist(6, 15); // Random number of points
            int numPoints = pointsDist(gen);
            
            std::uniform_real_distribution<float> radiusDist(80.0f, 200.0f);
            std::uniform_real_distribution<float> variationDist(0.3f, 0.8f);
            
            for (int i = 0; i < numPoints; i++) {
                float angle = (2 * PI * i) / numPoints;
                float radius = radiusDist(gen);
                
                // Add some variation to make it more interesting
                if (i % 2 == 0) {
                    radius *= variationDist(gen);
                }
                
                float x = cosf(angle) * radius;
                float y = sinf(angle) * radius;
                
                randomShape.push_back({x, y});
            }
            
            boundary = std::make_shared<PolygonalPrismBoundary>(randomShape, -100.0f, 100.0f);
        }
        else if (shapeType == 1) {
            // Random cylindrical boundary
            std::uniform_real_distribution<float> radiusDist(80.0f, 180.0f);
            std::uniform_real_distribution<float> heightDist(150.0f, 300.0f);
            
            float radius = radiusDist(gen);
            float height = heightDist(gen);
            
            boundary = std::make_shared<CylindricalBoundary>(Vector3{0, 0, 0}, radius, height);
        }
        else if (shapeType == 2) {
            // Large horizontal rectangle
            std::uniform_real_distribution<float> widthDist(300.0f, 400.0f);
            std::uniform_real_distribution<float> depthDist(300.0f, 400.0f);
            std::uniform_real_distribution<float> heightDist(30.0f, 70.0f);

            float width = widthDist(gen);
            float depth = depthDist(gen);

            boundary = std::make_shared<LargeRectangleBoundary>(Vector3{0, 0, 0}, width, depth, heightDist(gen));
        }
        else {
            // Random complex shape (with holes or concavities)
            std::vector<Vector2> complexShape;
            
            // Base on a combination of sine waves
            for (int i = 0; i < 36; i++) {
                float angle = (2 * PI * i) / 36.0f;
                
                std::uniform_real_distribution<float> freqDist(1.0f, 5.0f);
                float frequency = freqDist(gen);
                
                std::uniform_real_distribution<float> minRadDist(100.0f, 130.0f);
                float minRadius = minRadDist(gen);
                
                std::uniform_real_distribution<float> ampDist(30.0f, 80.0f);
                float amplitude = ampDist(gen);
                
                float radius = minRadius + amplitude * sinf(frequency * angle);
                
                float x = cosf(angle) * radius;
                float y = sinf(angle) * radius;
                
                complexShape.push_back({x, y});
            }
            
            boundary = std::make_shared<PolygonalPrismBoundary>(complexShape, -100.0f, 100.0f);
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