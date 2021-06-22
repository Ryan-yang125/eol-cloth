//
// Created by Apple on 2021/6/21.
//

#include "shader.h"

Shader::Shader(const char *vsPath, const char *fsPath, const char *gsPath) {
    std::string vertexShaderCode;
    std::string fragmentShaderCode;
    std::string geometryShaderCode;

    std::ifstream vertexShaderFile;
    std::ifstream fragmentShaderFile;
    std::ifstream geometryShaderFile;

    vertexShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    fragmentShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    geometryShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    try
    {
        vertexShaderFile.open(vsPath);
        fragmentShaderFile.open(fsPath);
        std::stringstream vertexShaderStream, fragmentShaderStream;

        vertexShaderStream << vertexShaderFile.rdbuf();
        fragmentShaderStream << fragmentShaderFile.rdbuf();

        vertexShaderFile.close();
        fragmentShaderFile.close();

        vertexShaderCode = vertexShaderStream.str();
        fragmentShaderCode = fragmentShaderStream.str();

        if (gsPath != NULL)
        {
            geometryShaderFile.open(gsPath);
            std::stringstream geometryShaderStream;
            geometryShaderStream << geometryShaderFile.rdbuf();
            geometryShaderFile.close();
            geometryShaderCode = geometryShaderStream.str();
        }
    }
    catch (std::ifstream::failure error)
    {
        std::cout << "SHADER READING FAILED! " << std::endl;
    }

    const char* vsCode = vertexShaderCode.c_str();
    const char* fsCode = fragmentShaderCode.c_str();

    unsigned int vs, fs, gs;
    vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vsCode, NULL);
    glCompileShader(vs);
    checkCompileError(vs, "VERTEX");

    fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fsCode, NULL);
    glCompileShader(fs);
    checkCompileError(fs, "FRAGMENT");

    if (gsPath != NULL)
    {
        const char* gsCode = geometryShaderCode.c_str();
        gs = glCreateShader(GL_GEOMETRY_SHADER);
        glShaderSource(gs, 1, &gsCode, NULL);
        glCompileShader(gs);
        checkCompileError(gs, "GEOMETRY");
    }

    ID = glCreateProgram();
    glAttachShader(ID, vs);
    glAttachShader(ID, fs);
    if (gsPath != NULL) glAttachShader(ID, gs);
    glLinkProgram(ID);
    checkCompileError(ID, "PROGRAM");

    glDeleteShader(vs);
    glDeleteShader(fs);
    if (gsPath != NULL) glDeleteShader(gs);
}

void Shader::use() {
    glUseProgram(ID);
}

void Shader::checkCompileError(GLuint shader, std::string shaderType) {
    GLint success;
    GLchar infoLog[1024];

    if (shaderType != "PROGRAM")
    {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(shader, 1024, NULL, infoLog);
            std::cout << shaderType << " SHADER COMPILE FAILED: " << infoLog << std::endl;
        }
    }
    else
    {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success)
        {
            glGetProgramInfoLog(shader, 1024, NULL, infoLog);
            std::cout << shaderType << " LINK FAILED: " << infoLog << std::endl;
        }
    }
}