#include "glad_helpers.h"

void initGLAD(GLFWwindow* window)
{
	//Load GLAD so it configures OpenGL
	int version = gladLoadGL(glfwGetProcAddress);
	if (!version) {
		std::cerr << "Failed to initialize GLAD\n";
		exit(EXIT_FAILURE);
	}
	printf("GL %d.%d\n", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));
	// Specify the viewport of OpenGL in the Window
	// In this case the viewport goes from x = 0, y = 0, to x = 800, y = 800
	int display_w, display_h;
	glfwGetFramebufferSize(window, &display_w, &display_h);
	glViewport(0, 0, display_w, display_h);
}