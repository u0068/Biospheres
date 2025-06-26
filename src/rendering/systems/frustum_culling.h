#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <vector>
#include <cstdint>

// Forward declarations
class Camera;

// Frustum plane representation
struct Plane {
    glm::vec3 normal;
    float distance;
    
    Plane() : normal(0.0f), distance(0.0f) {}
    Plane(const glm::vec3& n, float d) : normal(n), distance(d) {}
    
    // Calculate signed distance from point to plane
    float distanceToPoint(const glm::vec3& point) const {
        return glm::dot(normal, point) + distance;
    }
    
    // Check if point is in front of plane (positive side)
    bool isPointInFront(const glm::vec3& point) const {
        return distanceToPoint(point) >= 0.0f;
    }
};

// Bounding sphere representation
struct BoundingSphere {
    glm::vec3 center;
    float radius;
    
    BoundingSphere() : center(0.0f), radius(0.0f) {}
    BoundingSphere(const glm::vec3& c, float r) : center(c), radius(r) {}
};

// Frustum plane indices
enum FrustumPlaneIndex {
    FRUSTUM_LEFT = 0,
    FRUSTUM_RIGHT = 1,
    FRUSTUM_BOTTOM = 2,
    FRUSTUM_TOP = 3,
    FRUSTUM_NEAR = 4,
    FRUSTUM_FAR = 5
};

// Frustum culling class
class Frustum {
private:
    std::array<Plane, 6> planes;
    
public:
    // Extract frustum planes from view-projection matrix
    void extractFromMatrix(const glm::mat4& viewProjectionMatrix) {
        // Extract planes from the view-projection matrix
        // Each plane is represented as ax + by + cz + d = 0
        
        // Left plane
        planes[FRUSTUM_LEFT].normal.x = viewProjectionMatrix[0][3] + viewProjectionMatrix[0][0];
        planes[FRUSTUM_LEFT].normal.y = viewProjectionMatrix[1][3] + viewProjectionMatrix[1][0];
        planes[FRUSTUM_LEFT].normal.z = viewProjectionMatrix[2][3] + viewProjectionMatrix[2][0];
        planes[FRUSTUM_LEFT].distance = viewProjectionMatrix[3][3] + viewProjectionMatrix[3][0];
        
        // Right plane
        planes[FRUSTUM_RIGHT].normal.x = viewProjectionMatrix[0][3] - viewProjectionMatrix[0][0];
        planes[FRUSTUM_RIGHT].normal.y = viewProjectionMatrix[1][3] - viewProjectionMatrix[1][0];
        planes[FRUSTUM_RIGHT].normal.z = viewProjectionMatrix[2][3] - viewProjectionMatrix[2][0];
        planes[FRUSTUM_RIGHT].distance = viewProjectionMatrix[3][3] - viewProjectionMatrix[3][0];
        
        // Bottom plane
        planes[FRUSTUM_BOTTOM].normal.x = viewProjectionMatrix[0][3] + viewProjectionMatrix[0][1];
        planes[FRUSTUM_BOTTOM].normal.y = viewProjectionMatrix[1][3] + viewProjectionMatrix[1][1];
        planes[FRUSTUM_BOTTOM].normal.z = viewProjectionMatrix[2][3] + viewProjectionMatrix[2][1];
        planes[FRUSTUM_BOTTOM].distance = viewProjectionMatrix[3][3] + viewProjectionMatrix[3][1];
        
        // Top plane
        planes[FRUSTUM_TOP].normal.x = viewProjectionMatrix[0][3] - viewProjectionMatrix[0][1];
        planes[FRUSTUM_TOP].normal.y = viewProjectionMatrix[1][3] - viewProjectionMatrix[1][1];
        planes[FRUSTUM_TOP].normal.z = viewProjectionMatrix[2][3] - viewProjectionMatrix[2][1];
        planes[FRUSTUM_TOP].distance = viewProjectionMatrix[3][3] - viewProjectionMatrix[3][1];
        
        // Near plane
        planes[FRUSTUM_NEAR].normal.x = viewProjectionMatrix[0][3] + viewProjectionMatrix[0][2];
        planes[FRUSTUM_NEAR].normal.y = viewProjectionMatrix[1][3] + viewProjectionMatrix[1][2];
        planes[FRUSTUM_NEAR].normal.z = viewProjectionMatrix[2][3] + viewProjectionMatrix[2][2];
        planes[FRUSTUM_NEAR].distance = viewProjectionMatrix[3][3] + viewProjectionMatrix[3][2];
        
        // Far plane
        planes[FRUSTUM_FAR].normal.x = viewProjectionMatrix[0][3] - viewProjectionMatrix[0][2];
        planes[FRUSTUM_FAR].normal.y = viewProjectionMatrix[1][3] - viewProjectionMatrix[1][2];
        planes[FRUSTUM_FAR].normal.z = viewProjectionMatrix[2][3] - viewProjectionMatrix[2][2];
        planes[FRUSTUM_FAR].distance = viewProjectionMatrix[3][3] - viewProjectionMatrix[3][2];
        
        // Normalize all planes
        for (auto& plane : planes) {
            float length = glm::length(plane.normal);
            if (length > 0.0f) {
                plane.normal /= length;
                plane.distance /= length;
            }
        }
    }
    
    // Test if a sphere is inside the frustum
    bool isSphereInFrustum(const BoundingSphere& sphere) const {
        for (const auto& plane : planes) {
            float distance = plane.distanceToPoint(sphere.center);
            if (distance < -sphere.radius) {
                return false; // Sphere is completely outside this plane
            }
        }
        return true; // Sphere is inside or intersecting the frustum
    }
    
    // Test if a point is inside the frustum
    bool isPointInFrustum(const glm::vec3& point) const {
        for (const auto& plane : planes) {
            if (!plane.isPointInFront(point)) {
                return false;
            }
        }
        return true;
    }
    
    // Get a specific plane
    const Plane& getPlane(FrustumPlaneIndex index) const {
        return planes[index];
    }
    
    // Get all planes
    const std::array<Plane, 6>& getPlanes() const {
        return planes;
    }
};

// Utility functions for frustum culling
namespace FrustumCulling {
    
    // Create frustum from camera and projection parameters
    // Note: This function is defined in the implementation file to avoid circular dependencies
    Frustum createFrustum(const Camera& camera, float fov, float aspectRatio, float nearPlane, float farPlane);
    
    // Create frustum from view-projection matrix
    inline Frustum createFrustum(const glm::mat4& viewProjectionMatrix) {
        Frustum frustum;
        frustum.extractFromMatrix(viewProjectionMatrix);
        return frustum;
    }
    
    // Test multiple spheres against frustum (returns indices of visible spheres)
    inline std::vector<uint32_t> cullSpheres(const Frustum& frustum, 
                                           const std::vector<BoundingSphere>& spheres) {
        std::vector<uint32_t> visibleIndices;
        visibleIndices.reserve(spheres.size());
        
        for (uint32_t i = 0; i < spheres.size(); ++i) {
            if (frustum.isSphereInFrustum(spheres[i])) {
                visibleIndices.push_back(i);
            }
        }
        
        return visibleIndices;
    }
    
    // Count visible spheres without storing indices (more memory efficient)
    inline uint32_t countVisibleSpheres(const Frustum& frustum, 
                                       const std::vector<BoundingSphere>& spheres) {
        uint32_t count = 0;
        for (const auto& sphere : spheres) {
            if (frustum.isSphereInFrustum(sphere)) {
                count++;
            }
        }
        return count;
    }
    
    // Test array of positions and radii (more efficient for cell data)
    inline std::vector<uint32_t> cullSpheresFromArrays(const Frustum& frustum,
                                                      const glm::vec3* positions,
                                                      const float* radii,
                                                      uint32_t count) {
        std::vector<uint32_t> visibleIndices;
        visibleIndices.reserve(count);
        
        for (uint32_t i = 0; i < count; ++i) {
            BoundingSphere sphere(positions[i], radii[i]);
            if (frustum.isSphereInFrustum(sphere)) {
                visibleIndices.push_back(i);
            }
        }
        
        return visibleIndices;
    }
} 