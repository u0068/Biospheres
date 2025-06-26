#include "frustum_culling.h"
#include "../camera/camera.h"

namespace FrustumCulling {
    
    Frustum createFrustum(const Camera& camera, float fov, float aspectRatio, float nearPlane, float farPlane) {
        glm::mat4 view = camera.getViewMatrix();
        glm::mat4 projection = glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
        glm::mat4 viewProjection = projection * view;
        
        Frustum frustum;
        frustum.extractFromMatrix(viewProjection);
        return frustum;
    }
    
} 