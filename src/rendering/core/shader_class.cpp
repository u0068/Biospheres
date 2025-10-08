#include "shader_class.h"
#include <fstream>
#include <cerrno>
#include <glm/vec2.hpp>
#include <glm/gtc/type_ptr.hpp>

// Reads a text file and outputs a string with everything in the text file
std::string get_file_contents(const char* filename)
{
    // Try a set of common base path prefixes to be resilient to working directory
    // differences (e.g., running from x64/Release vs project root)
    const char* prefixes[] = { "", "../", "../../", "../../../" };

    for (const char* prefix : prefixes)
    {
        std::string candidatePath = std::string(prefix) + filename;
        std::ifstream file(candidatePath.c_str(), std::ios::binary);
        if (file)
        {
            std::string contents;
            file.seekg(0, std::ios::end);
            contents.resize(static_cast<size_t>(file.tellg()));
            file.seekg(0, std::ios::beg);
            file.read(&contents[0], contents.size());
            file.close();
            return contents;
        }
    }

    // If this point is reached, then the file could not be opened
    std::cout << "ERROR::SHADER::FILE_NOT_FOUND: " << filename << "\n";
    std::cout << "Make sure that the file path is correct!\n";
    throw(errno);
}

// Constructor that build the Shader Program from a vertex and fragment shader
Shader::Shader(const char* vertexFile, const char* fragmentFile)
{
	int success;
	char infoLog[512];

	// Read vertexFile and fragmentFile and store the strings
	std::string vertexCode = get_file_contents(vertexFile);
	std::string fragmentCode = get_file_contents(fragmentFile);

	// Convert the shader source strings into character arrays
	const char* vertexSource = vertexCode.c_str();
	const char* fragmentSource = fragmentCode.c_str();

	// Create Vertex Shader Object and get its reference
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	// Attach Vertex Shader source to the Vertex Shader Object
	glShaderSource(vertexShader, 1, &vertexSource, NULL);
	// Compile the Vertex Shader into machine code
	glCompileShader(vertexShader);
	// Print compile errors if any
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
		//Make sure there are no typos in the name!

	}

	// Create Fragment Shader Object and get its reference
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	// Attach Fragment Shader source to the Fragment Shader Object
	glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
	// Compile the Fragment Shader into machine code
	glCompileShader(fragmentShader);
	// Print compile errors if any
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << "\n";
	}

	// Create Shader Program Object and get its reference
	ID = glCreateProgram();
	// Attach the Vertex and Fragment Shaders to the Shader Program
	glAttachShader(ID, vertexShader);
	glAttachShader(ID, fragmentShader);
	// Wrap-up/Link all the shaders together into the Shader Program
	glLinkProgram(ID);
	// Print linking errors if any
	glGetProgramiv(ID, GL_LINK_STATUS, &success);
	if (!success)
	{
		glGetProgramInfoLog(ID, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << "\n";
	}

	// destroy the now useless Vertex and Fragment Shader objects
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

}

// Constructor that builds the Shader Program from vertex, fragment, and geometry shaders
Shader::Shader(const char* vertexFile, const char* fragmentFile, const char* geometryFile)
{
	int success;
	char infoLog[512];

	// Read shader files and store the strings
	std::string vertexCode = get_file_contents(vertexFile);
	std::string fragmentCode = get_file_contents(fragmentFile);
	std::string geometryCode = get_file_contents(geometryFile);

	// Convert the shader source strings into character arrays
	const char* vertexSource = vertexCode.c_str();
	const char* fragmentSource = fragmentCode.c_str();
	const char* geometrySource = geometryCode.c_str();

	// Create Vertex Shader Object and get its reference
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexSource, NULL);
	glCompileShader(vertexShader);
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << "\n";
	}

	// Create Fragment Shader Object and get its reference
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
	glCompileShader(fragmentShader);
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << "\n";
	}

	// Create Geometry Shader Object and get its reference
	GLuint geometryShader = glCreateShader(GL_GEOMETRY_SHADER);
	glShaderSource(geometryShader, 1, &geometrySource, NULL);
	glCompileShader(geometryShader);
	glGetShaderiv(geometryShader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(geometryShader, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::GEOMETRY::COMPILATION_FAILED\n" << infoLog << "\n";
	}

	// Create Shader Program Object and get its reference
	ID = glCreateProgram();
	// Attach all shaders to the Shader Program
	glAttachShader(ID, vertexShader);
	glAttachShader(ID, fragmentShader);
	glAttachShader(ID, geometryShader);
	// Link all the shaders together into the Shader Program
	glLinkProgram(ID);
	// Print linking errors if any
	glGetProgramiv(ID, GL_LINK_STATUS, &success);
	if (!success)
	{
		glGetProgramInfoLog(ID, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << "\n";
	}

	// Destroy the now useless shader objects
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
	glDeleteShader(geometryShader);
}

// Constructor for compute shader
Shader::Shader(const char* computeFile)
{
	int success;
	char infoLog[512];

	// Read computeFile and store the string
	std::string computeCode = get_file_contents(computeFile);
	const char* computeSource = computeCode.c_str();

	// Create Compute Shader Object and get its reference
	GLuint computeShader = glCreateShader(GL_COMPUTE_SHADER);
	// Attach Compute Shader source to the Compute Shader Object
	glShaderSource(computeShader, 1, &computeSource, NULL);
	// Compile the Compute Shader into machine code
	glCompileShader(computeShader);
	// Print compile errors if any
	glGetShaderiv(computeShader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(computeShader, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::COMPUTE::COMPILATION_FAILED\n" << infoLog << "\n";
	}

	// Create Shader Program Object and get its reference
	ID = glCreateProgram();
	// Attach the Compute Shader to the Shader Program
	glAttachShader(ID, computeShader);
	// Link the shader program
	glLinkProgram(ID);
	// Print linking errors if any
	glGetProgramiv(ID, GL_LINK_STATUS, &success);
	if (!success)
	{
		glGetProgramInfoLog(ID, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::COMPUTE_PROGRAM::LINKING_FAILED\n" << infoLog << "\n";
	}

	// destroy the now useless Compute Shader object
	glDeleteShader(computeShader);
}

// Activates the Shader Program
void Shader::use()
{
	glUseProgram(ID);
}

// Deletes the Shader Program
void Shader::destroy()
{
	if (ID) glDeleteProgram(ID);
}

// Dispatch compute shader
void Shader::dispatch(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z)
{
	glDispatchCompute(num_groups_x, num_groups_y, num_groups_z);
}

// utility uniform functions

void Shader::setInt(const std::string& name, int value) const
{
	int location = glGetUniformLocation(ID, name.c_str());
	glUniform1i(location, value);
}

void Shader::setFloat(const std::string& name, float value) const
{
	int location = glGetUniformLocation(ID, name.c_str());
	glUniform1f(location, value);
}

void Shader::setVec2(const std::string& name, float x, float y) const
{
	int location = glGetUniformLocation(ID, name.c_str());
	glUniform2f(location, x, y);
}

void Shader::setVec2(const std::string& name, glm::vec2 vector) const
{
	int location = glGetUniformLocation(ID, name.c_str());
	glUniform2f(location, vector.x, vector.y);
}

void Shader::setVec3(const std::string& name, float x, float y, float z) const
{
	int location = glGetUniformLocation(ID, name.c_str());
	glUniform3f(location, x, y ,z);
}

void Shader::setVec3(const std::string& name, glm::vec3 vector) const
{
	int location = glGetUniformLocation(ID, name.c_str());
	glUniform3f(location, vector.x, vector.y, vector.z);
}

void Shader::setVec4(const std::string& name, float x, float y, float z, float w) const
{
	int location = glGetUniformLocation(ID, name.c_str());
	glUniform4f(location, x, y, z, w);
}

void Shader::setMat4(const std::string& name, const glm::mat4& matrix) const
{
	int location = glGetUniformLocation(ID, name.c_str());
	glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
}