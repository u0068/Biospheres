#pragma once

// Third-party includes
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

// Standard includes
#include <string>

/**
 * Simple shader class for the baker tool
 * Handles shader compilation, linking, and uniform setting
 */
class Shader {
public:
    // Constructor reads and builds the shader
    Shader(const std::string& vertexPath, const std::string& fragmentPath);
    ~Shader();
    
    // Use/activate the shader
    void use() const;
    
    // Utility uniform functions
    void setBool(const std::string& name, bool value) const;
    void setInt(const std::string& name, int value) const;
    void setFloat(const std::string& name, float value) const;
    void setVec2(const std::string& name, const glm::vec2& value) const;
    void setVec3(const std::string& name, const glm::vec3& value) const;
    void setVec4(const std::string& name, const glm::vec4& value) const;
    void setMat4(const std::string& name, const glm::mat4& value) const;
    
    // Get shader program ID
    GLuint getID() const { return ID; }
    
private:
    GLuint ID;
    
    // Utility functions
    std::string readFile(const std::string& filePath);
    GLuint compileShader(const std::string& source, GLenum shaderType, const std::string& typeName);
    void checkCompileErrors(GLuint shader, const std::string& type);
};