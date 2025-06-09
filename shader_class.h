#pragma once

#include<glad/glad.h>
#include<string>
#include<iostream>
#include <vec2.hpp>
#include <vector>


std::string get_file_contents(const char* filename);

class Shader
{
public:
	// Reference ID of the Shader Program
	GLuint ID;
	// Constructor that build the Shader Program from 2 different shaders
	Shader(const char* vertexFile, const char* fragmentFile);
	
	// Activates the Shader Program
	void use();
	// Deletes the Shader Program
	void destroy();
	// utility uniform functions
	//void setBool(const std::string& name, bool value) const; // Apparently I can't do that?!?
	void setInt(const std::string& name, int value) const;
	void setFloat(const std::string& name, float value) const;
	void setVec2(const std::string& name, float x, float y) const;
	void setVec2(const std::string& name, glm::vec2 vector) const;
	//void setVec2Array(const std::string& name, const float size, std::vector<glm::vec2> vector) const;
	void setVec3(const std::string& name, float x, float y, float z) const;
	void setVec4(const std::string& name, float x, float y, float z, float w) const;

};