#ifndef RUN_HPP
#define RUN_HPP

#include "render/camera.h"
#include "render/shader.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GL/glew.h>

#include <Eigen/Sparse>
#include <Eigen/Dense>
#include <iostream>

#include "simulator.hpp"
#include "cloth.hpp"


using namespace std;
using namespace Eigen;

Simulator* simulator;
Camera camera(glm::vec3(-2.03286 ,-2.05865 ,1.60675), glm::vec3(0,0,1), 53.04, -27.92);

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 800;

float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;
float lastTime = 0.0f;

bool animation_play = false;

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	// make sure the viewport matches the new window dimensions; note that width and 
	// height will be significantly larger than specified on retina displays.
	glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xPos, double yPos)
{
	if (firstMouse)
	{
		lastX = xPos;
		lastY = yPos;
		firstMouse = false;
	}

	float xOffset = xPos - lastX;
	float yOffset = lastY - yPos; // reversed since y-coordinates go from bottom to top

	lastX = xPos;
	lastY = yPos;

	camera.rotate(xOffset, yOffset);
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	camera.zoom(yoffset);
}

void keyboard_callback(GLFWwindow* window)
{
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.translate(FORWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.translate(LEFT, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.translate(BACKWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.translate(RIGHT, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS) animation_play = !animation_play;
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);

}

static void error_callback(int error, const char *msg) {
    cerr << "GLWT error " << error << ": " << msg << endl;
}

void setVertex(GLuint *VAO, GLuint *VBO, int buffer_size, float *buffer, bool ifStatic) {
    glGenVertexArrays(1, VAO);
    glGenBuffers(1, VBO);
    glBindBuffer(GL_ARRAY_BUFFER, *VBO);
    glBufferData(GL_ARRAY_BUFFER, buffer_size, buffer, ifStatic ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW);
    glBindVertexArray(*VAO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

void setVertex2D(GLuint *VAO, GLuint *VBO, int buffer_size, float *buffer, bool ifStatic) {
    glGenVertexArrays(1, VAO);
    glGenBuffers(1, VBO);
    glBindBuffer(GL_ARRAY_BUFFER, *VBO);
    glBufferData(GL_ARRAY_BUFFER, buffer_size, buffer, ifStatic ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW);
    glBindVertexArray(*VAO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}
void run()
{
	simulator = new Simulator;

    glfwSetErrorCallback(error_callback);

	if (glfwInit() == GL_FALSE) {
	    cout << "Failed to init GLFW!" << endl;
	    return;
	};

    // set opengl version
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_SAMPLES, 4);
	GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "EOL_SIM", NULL, NULL);

	if (window == NULL)
	{
		cout << "Failed to create GLFW window! " << endl;
		glfwTerminate();
		return;
	}
    // init glfw
	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetScrollCallback(window, scroll_callback);

	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glewInit();

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_MULTISAMPLE);

	// init shader
	Shader worldShader("/Users/apple/CLionProjects/eol-cloth/src/shader/world.vs", "/Users/apple/CLionProjects/eol-cloth/src/shader/world.fs");
	Shader meshShader("/Users/apple/CLionProjects/eol-cloth/src/shader/mesh.vs", "/Users/apple/CLionProjects/eol-cloth/src/shader/mesh.fs");

	// init cube
    GLuint cubeVAO, cubeVBO;
    setVertex(&cubeVAO, &cubeVBO, simulator->obstacle->boxes[0]->buffer_size,simulator->obstacle->boxes[0]->buffer, true);

    GLuint cube1VAO, cube1VBO;
    setVertex(&cube1VAO, &cube1VBO, simulator->obstacle->boxes[1]->buffer_size,simulator->obstacle->boxes[1]->buffer, true);

    // niddle
	float niddle[] = {0.751,0.751,-0.005, 0.751,0.751,-1.005,
					  0.71,0.22,-0.005, 0.71,0.22,-1.005,
					  0.27,0.51,-0.005, 0.27,0.51,-1.005};
	GLuint pointVAO, pointVBO;
    setVertex2D(&pointVAO, &pointVBO, sizeof(float) * 18, niddle, true);


    // cloth
	GLuint clothVAO, clothVBO;
    setVertex(&clothVAO, &clothVBO, simulator->cloth->buffer_size, simulator->cloth->buffer, false);

    // mesh
	GLuint meshVAO, meshVBO;
    setVertex2D(&meshVAO, &meshVBO, simulator->cloth->mesh_buffer_size, simulator->cloth->mesh_buffer, false);


	while (!glfwWindowShouldClose(window))
	{
		float currentTime = glfwGetTime();
		deltaTime = currentTime - lastTime;
		lastTime = currentTime;

		keyboard_callback(window);

		if (animation_play) simulator->step();

		// remesh
		glBindBuffer(GL_ARRAY_BUFFER, meshVBO);
		glBufferData(GL_ARRAY_BUFFER, simulator->cloth->mesh_buffer_size, simulator->cloth->mesh_buffer, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, clothVBO);
		glBufferData(GL_ARRAY_BUFFER, simulator->cloth->buffer_size, simulator->cloth->buffer, GL_DYNAMIC_DRAW);


		//float* ptr = (float*)glMapBuffer(GL_ARRAY_BUFFER, GL_READ_WRITE);
		//memcpy(ptr, simulator->cloth->buffer, simulator->cloth->buffer_size);
		//glUnmapBuffer(GL_ARRAY_BUFFER);

		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		meshShader.use();
		glBindVertexArray(meshVAO);
		glDrawArrays(GL_LINES, 0, 6*simulator->cloth->mesh.faces.size());

		glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.05f, 100.0f);
		glm::mat4 view = camera.getViewMatrix();
		glm::mat4 model = glm::mat4(1.0f);

		// Set light
		worldShader.use();
		worldShader.setVec3("light.position", glm::vec3(1.2, 1.2, 1.8));
		worldShader.setVec3("viewPos", camera.Position);

		glm::vec3 lightColor = glm::vec3(1.0f);
		glm::vec3 diffuseColor = lightColor * glm::vec3(0.8f);
		glm::vec3 ambientColor = diffuseColor * glm::vec3(0.8f);
		worldShader.setVec3("light.ambient", ambientColor);
		worldShader.setVec3("light.diffuse", diffuseColor);
		worldShader.setVec3("light.specular", 1.0f, 1.0f, 1.0f);

		// Draw Cube
//		worldShader.setVec3("material.ambient", 1.0f, 0.6f, 0.4f);
		worldShader.setVec3("material.ambient", 0.0f, 0.0f, 0.5f);
		worldShader.setVec3("material.diffuse", 0.9f, 0.5f, 0.3f);
		worldShader.setVec3("material.specular", 0.1f, 0.1f, 0.1f);
        worldShader.setFloat("material.shininess", 2.0f);

		worldShader.setMat4("projection", projection);
		worldShader.setMat4("view", view);
		worldShader.setMat4("model", model);

		glBindVertexArray(cubeVAO);
		glDrawArrays(GL_TRIANGLES, 0, 36);
		//glBindVertexArray(pointVAO);
		//glDrawArrays(GL_LINES, 0, 6);

        //Draw the Cube1
        worldShader.setVec3("material.ambient", 0.0f, 0.0f, 0.5f);
        worldShader.setVec3("material.diffuse", 0.9f, 0.5f, 0.3f);
        worldShader.setVec3("material.specular", 0.1f, 0.1f, 0.1f);
        worldShader.setFloat("material.shininess", 2.0f);

        glBindVertexArray(cube1VAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);

		//Draw the Cloth
		worldShader.setVec3("material.ambient", 0.0f, 0.5f, 0.0f);
		worldShader.setVec3("material.diffuse", 0.5f, 0.0f, 0.0f);
		worldShader.setVec3("material.specular", 0.1f, 0.1f, 0.1f);
		worldShader.setFloat("material.shininess", 2.0f);
		glBindVertexArray(clothVAO);
		glDrawArrays(GL_TRIANGLES, 0, 3 * simulator->cloth->mesh.faces.size());


		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glfwTerminate();
	return;



}






#endif