#pragma once

#include <cmath>
#include <vector>

#include "raylib.h"

class MeshGenerator {
public:
    static Vector3 Vector3Cross(Vector3 v1, Vector3 v2) {
        return {
            v1.y * v2.z - v1.z * v2.y,
            v1.z * v2.x - v1.x * v2.z,
            v1.x * v2.y - v1.y * v2.x
        };
    }

    static Vector3 Vector3Normalize(const Vector3 v) {
        const float length = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
        if (length == 0) return v;
        return {v.x / length, v.y / length, v.z / length};
    }

    static Vector3 Vector3Subtract(const Vector3 v1, const Vector3 v2) {
        return {v1.x - v2.x, v1.y - v2.y, v1.z - v2.z};
    }

    static Mesh genTruncatedOctahedron() {
        const float sqrt2 = sqrtf(2.0f);

        std::vector<Vector3> vertices;

        // Square faces first
        // Top square (y = 2√2)
        vertices.push_back({0, 2 * sqrt2, sqrt2}); // 0: (0, 2√2, √2)
        vertices.push_back({0, 2 * sqrt2, -sqrt2}); // 1: (0, 2√2, -√2)
        vertices.push_back({sqrt2, 2 * sqrt2, 0}); // 2: (√2, 2√2, 0)
        vertices.push_back({-sqrt2, 2 * sqrt2, 0}); // 3: (-√2, 2√2, 0)

        // Bottom square (y = -2√2)
        vertices.push_back({0, -2 * sqrt2, -sqrt2}); // 4: (0, -2√2, -√2)
        vertices.push_back({0, -2 * sqrt2, sqrt2}); // 5: (0, -2√2, √2)
        vertices.push_back({sqrt2, -2 * sqrt2, 0}); // 6: (√2, -2√2, 0)
        vertices.push_back({-sqrt2, -2 * sqrt2, 0}); // 7: (-√2, -2√2, 0)

        // Front square (z = 2√2)
        vertices.push_back({0, -sqrt2, 2 * sqrt2}); // 8: (0, -√2, 2√2)
        vertices.push_back({0, sqrt2, 2 * sqrt2}); // 9: (0, √2, 2√2)
        vertices.push_back({sqrt2, 0, 2 * sqrt2}); // 10: (√2, 0, 2√2)
        vertices.push_back({-sqrt2, 0, 2 * sqrt2}); // 11: (-√2, 0, 2√2)

        // Back square (z = -2√2)
        vertices.push_back({0, sqrt2, -2 * sqrt2}); // 12: (0, √2, -2√2)
        vertices.push_back({0, -sqrt2, -2 * sqrt2}); // 13: (0, -√2, -2√2)
        vertices.push_back({sqrt2, 0, -2 * sqrt2}); // 14: (√2, 0, -2√2)
        vertices.push_back({-sqrt2, 0, -2 * sqrt2}); // 15: (-√2, 0, -2√2)

        // Right square (x = 2√2)
        vertices.push_back({2 * sqrt2, sqrt2, 0}); // 16: (2√2, √2, 0)
        vertices.push_back({2 * sqrt2, -sqrt2, 0}); // 17: (2√2, -√2, 0)
        vertices.push_back({2 * sqrt2, 0, sqrt2}); // 18: (2√2, 0, √2)
        vertices.push_back({2 * sqrt2, 0, -sqrt2}); // 19: (2√2, 0, -√2)

        // Left square (x = -2√2)
        vertices.push_back({-2 * sqrt2, -sqrt2, 0}); // 20: (-2√2, -√2, 0)
        vertices.push_back({-2 * sqrt2, sqrt2, 0}); // 21: (-2√2, √2, 0)
        vertices.push_back({-2 * sqrt2, 0, sqrt2}); // 22: (-2√2, 0, √2)
        vertices.push_back({-2 * sqrt2, 0, -sqrt2}); // 23: (-2√2, 0, -√2)

        // The hexagon should use these existing vertices in the correct order:
        // 0: (0, 2√2, √2)     - from top square
        // 2: (√2, 2√2, 0)     - from top square
        // 16: (2√2, √2, 0)    - from right square
        // 18: (2√2, 0, √2)    - from right square
        // 10: (√2, 0, 2√2)    - from front square
        // 9: (0, √2, 2√2)     - from front square

        // Calculate mesh data
        Mesh mesh = {};
        mesh.vertexCount = static_cast<int>(vertices.size());
        mesh.triangleCount = (24 / 4) * 2 + 32; // Square faces (6*2 triangles) + 8 hexagons (4 triangles each)

        // Allocate mesh data
        mesh.vertices = static_cast<float *>(MemAlloc(mesh.vertexCount * 3 * sizeof(float)));
        mesh.texcoords = static_cast<float *>(MemAlloc(mesh.vertexCount * 2 * sizeof(float)));
        mesh.normals = static_cast<float *>(MemAlloc(mesh.vertexCount * 3 * sizeof(float)));
        mesh.indices = static_cast<unsigned short *>(MemAlloc(mesh.triangleCount * 3 * sizeof(unsigned short)));
        mesh.colors = static_cast<unsigned char *>(MemAlloc(mesh.vertexCount * 4 * sizeof(unsigned char)));

        // Fill vertex data
        for (int i = 0; i < vertices.size(); i++) {
            mesh.vertices[i * 3] = vertices[i].x;
            mesh.vertices[i * 3 + 1] = vertices[i].y;
            mesh.vertices[i * 3 + 2] = vertices[i].z;
        }

        // Generate indices for squares
        int indexCount = 0;
        for (int face = 0; face < 6; face++) {
            int baseVertex = face * 4;

            mesh.indices[indexCount++] = baseVertex + 2; // First triangle
            mesh.indices[indexCount++] = baseVertex + 1;
            mesh.indices[indexCount++] = baseVertex + 3;

            mesh.indices[indexCount++] = baseVertex; // Second triangle
            mesh.indices[indexCount++] = baseVertex + 2;
            mesh.indices[indexCount++] = baseVertex + 3;
        }

        // First hexagon (original - top-front-right)
        // Using vertices: 0,2,16,18,10,9
        mesh.indices[indexCount++] = 0;
        mesh.indices[indexCount++] = 10;
        mesh.indices[indexCount++] = 2;

        mesh.indices[indexCount++] = 2;
        mesh.indices[indexCount++] = 10;
        mesh.indices[indexCount++] = 16;

        mesh.indices[indexCount++] = 16;
        mesh.indices[indexCount++] = 10;
        mesh.indices[indexCount++] = 18;

        mesh.indices[indexCount++] = 0;
        mesh.indices[indexCount++] = 9;
        mesh.indices[indexCount++] = 10;

        // Second hexagon (top-right-back)
        // Using vertices: 1,2,16,19,14,12
        mesh.indices[indexCount++] = 1;
        mesh.indices[indexCount++] = 2;
        mesh.indices[indexCount++] = 14;

        mesh.indices[indexCount++] = 2;
        mesh.indices[indexCount++] = 16;
        mesh.indices[indexCount++] = 14;

        mesh.indices[indexCount++] = 16;
        mesh.indices[indexCount++] = 19;
        mesh.indices[indexCount++] = 14;

        mesh.indices[indexCount++] = 1;
        mesh.indices[indexCount++] = 14;
        mesh.indices[indexCount++] = 12;

        // Third hexagon (front-right-bottom)
        // Using vertices: 10,18,17,6,5,8
        mesh.indices[indexCount++] = 10;
        mesh.indices[indexCount++] = 6;
        mesh.indices[indexCount++] = 18;

        mesh.indices[indexCount++] = 18;
        mesh.indices[indexCount++] = 6;
        mesh.indices[indexCount++] = 17;

        mesh.indices[indexCount++] = 10;
        mesh.indices[indexCount++] = 8;
        mesh.indices[indexCount++] = 6;

        mesh.indices[indexCount++] = 6;
        mesh.indices[indexCount++] = 8;
        mesh.indices[indexCount++] = 5;

        // Fourth hexagon (top-front-left) [FIXED]
        // Using vertices: 0,3,21,22,11,9
        mesh.indices[indexCount++] = 0;
        mesh.indices[indexCount++] = 3;
        mesh.indices[indexCount++] = 11;

        mesh.indices[indexCount++] = 3;
        mesh.indices[indexCount++] = 21;
        mesh.indices[indexCount++] = 11;

        mesh.indices[indexCount++] = 21;
        mesh.indices[indexCount++] = 22;
        mesh.indices[indexCount++] = 11;

        mesh.indices[indexCount++] = 0;
        mesh.indices[indexCount++] = 11;
        mesh.indices[indexCount++] = 9;

        // Fifth hexagon (top-back-left)
        mesh.indices[indexCount++] = 1;
        mesh.indices[indexCount++] = 15;
        mesh.indices[indexCount++] = 3;

        mesh.indices[indexCount++] = 3;
        mesh.indices[indexCount++] = 15;
        mesh.indices[indexCount++] = 21;

        mesh.indices[indexCount++] = 21;
        mesh.indices[indexCount++] = 15;
        mesh.indices[indexCount++] = 23;

        mesh.indices[indexCount++] = 1;
        mesh.indices[indexCount++] = 12;
        mesh.indices[indexCount++] = 15;

        // Sixth hexagon (back-right-bottom) [FIXED]
        // Using vertices: 13,14,19,17,6,4
        mesh.indices[indexCount++] = 13;
        mesh.indices[indexCount++] = 14;
        mesh.indices[indexCount++] = 6;

        mesh.indices[indexCount++] = 14;
        mesh.indices[indexCount++] = 19;
        mesh.indices[indexCount++] = 6;

        mesh.indices[indexCount++] = 19;
        mesh.indices[indexCount++] = 17;
        mesh.indices[indexCount++] = 6;

        mesh.indices[indexCount++] = 13;
        mesh.indices[indexCount++] = 6;
        mesh.indices[indexCount++] = 4;

        // Seventh hexagon (back-left-bottom)
        mesh.indices[indexCount++] = 13;
        mesh.indices[indexCount++] = 7;
        mesh.indices[indexCount++] = 15;

        mesh.indices[indexCount++] = 15;
        mesh.indices[indexCount++] = 7;
        mesh.indices[indexCount++] = 23;

        mesh.indices[indexCount++] = 23;
        mesh.indices[indexCount++] = 7;
        mesh.indices[indexCount++] = 20;

        mesh.indices[indexCount++] = 13;
        mesh.indices[indexCount++] = 4;
        mesh.indices[indexCount++] = 7;

        // Eighth hexagon (front-left-bottom) [FIXED]
        // Using vertices: 8,11,22,20,7,5
        mesh.indices[indexCount++] = 8;
        mesh.indices[indexCount++] = 11;
        mesh.indices[indexCount++] = 7;

        mesh.indices[indexCount++] = 11;
        mesh.indices[indexCount++] = 22;
        mesh.indices[indexCount++] = 7;

        mesh.indices[indexCount++] = 22;
        mesh.indices[indexCount++] = 20;
        mesh.indices[indexCount++] = 7;

        mesh.indices[indexCount++] = 8;
        mesh.indices[indexCount++] = 7;
        mesh.indices[indexCount] = 5;

        std::vector<Vector3> normalSum(vertices.size(), {0, 0, 0});
        std::vector<int> normalCount(vertices.size(), 0);
        // Calculate normals for each face and accumulate them for shared vertices
        for (int i = 0; i < mesh.triangleCount; i++) {
            unsigned short idx1 = mesh.indices[i * 3];
            unsigned short idx2 = mesh.indices[i * 3 + 1];
            unsigned short idx3 = mesh.indices[i * 3 + 2];

            Vector3 v1 = vertices[idx1];
            Vector3 v2 = vertices[idx2];
            Vector3 v3 = vertices[idx3];

            // Calculate face normal
            Vector3 edge1 = Vector3Subtract(v2, v1);
            Vector3 edge2 = Vector3Subtract(v3, v1);
            auto [x, y, z] = Vector3Normalize(Vector3Cross(edge1, edge2));

            // Add to the normal sum for each vertex
            normalSum[idx1] = {
                normalSum[idx1].x + x,
                normalSum[idx1].y + y,
                normalSum[idx1].z + z
            };
            normalSum[idx2] = {
                normalSum[idx2].x + x,
                normalSum[idx2].y + y,
                normalSum[idx2].z + z
            };
            normalSum[idx3] = {
                normalSum[idx3].x + x,
                normalSum[idx3].y + y,
                normalSum[idx3].z + z
            };

            normalCount[idx1]++;
            normalCount[idx2]++;
            normalCount[idx3]++;
        }

        // Average and normalize the normals for each vertex
        for (int i = 0; i < vertices.size(); i++) {
            Vector3 avgNormal = {
                normalSum[i].x / normalCount[i],
                normalSum[i].y / normalCount[i],
                normalSum[i].z / normalCount[i]
            };
            avgNormal = Vector3Normalize(avgNormal);

            mesh.normals[i * 3] = avgNormal.x;
            mesh.normals[i * 3 + 1] = avgNormal.y;
            mesh.normals[i * 3 + 2] = avgNormal.z;

            // Set default texture coordinates
            mesh.texcoords[i * 2] = 0.0f;
            mesh.texcoords[i * 2 + 1] = 0.0f;

            // Set default colors (white)
            mesh.colors[i * 4] = 255; // R
            mesh.colors[i * 4 + 1] = 255; // G
            mesh.colors[i * 4 + 2] = 255; // B
            mesh.colors[i * 4 + 3] = 255; // A
        }

        UploadMesh(&mesh, false);
        return mesh;
    }
};
