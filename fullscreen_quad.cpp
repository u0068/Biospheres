#include<glad/glad.h>
#include "fullscreen_quad.h"

// Reference containers for the Vertex Array Object and the Vertex Buffer Object
static GLuint VAO{ 0 }, VBO{ 0 };

void initFullscreenQuad() {
	// Vertices coordinates
	GLfloat vertices[] = {
		-1.0f, -1.0f, // Bottom Left
		 1.0f, -1.0f, // Bottom Right
		-1.0f,  1.0f, // Top Left
		 1.0f,  1.0f, // Top Right
	};

	// I might abstract a lot of the opengl stuff later, but for now this is fine

	// Generate the VAO and VBO with only 1 object each
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);

	// Make the VAO the current Vertex Array Object by binding it
	glBindVertexArray(VAO);

	// Bind the VBO specifying it's a GL_ARRAY_BUFFER
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	// Introduce the vertices into the VBO
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	// Configure the Vertex Attribute so that OpenGL knows how to read the VBO
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
	// Enable the Vertex Attribute so that OpenGL knows to use it
	glEnableVertexAttribArray(0);

	// Bind both the VBO and VAO to 0 so that we don't accidentally modify the VAO and VBO we created
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

void renderFullscreenQuad() {
	// Bind the VAO so OpenGL knows to use it
	glBindVertexArray(VAO);
	// Draw the quad using a triangle strip
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void destroyFullscreenQuad() {
    glDeleteBuffers(1, &VBO);
    glDeleteVertexArrays(1, &VAO);
}
