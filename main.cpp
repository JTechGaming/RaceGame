#include <glad/glad.h>
#include <GLFW/glfw3.h>

// define the single-header macro once in a translation unit to provide
// RapidYAML implementations.  When building multiple .cpp files this should be
// defined in exactly one of them (or pass -DRYML_SINGLE_HDR_DEFINE_NOW to
// the compiler).
#define RYML_SINGLE_HDR_DEFINE_NOW
#include "sceneManager.hpp"
#include "assetManager.hpp"
#include "soundEngine.hpp"

#include "shader.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// std
#include <iostream>
#include <filesystem>

#include "game.hpp"
#include "camera.hpp"

void processInput(GLFWwindow *window);
void framebufferSizeCallback(GLFWwindow* window, int width, int height);
void mouseCallback(GLFWwindow* window, double xpos, double ypos);
void scrollCallback(GLFWwindow* window, double xpos, double ypos);
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

float deltaTime = 0.0f;
float lastFrame = 0.0f;

float nearPlane = 0.001f;
float farPlane = 1000.0f;

bool WIREFRAME_MODE = false;

float fov = 90.0f;

Camera* currentCamera = nullptr;

unsigned int texture;
unsigned int defaultTexture;

std::string stringPath;
std::filesystem::file_time_type lastSceneTime;

AssetManager assetManager{};
SceneManager sceneManager{};

RaceGame game;
IGame& gameRef = game;

int main() {
    SoundEngine::init();
    ModelResource::setAssetManager(&assetManager);
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);
    #ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #endif
  
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    glfwWindowHint(GLFW_RED_BITS, mode->redBits);
    glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
    glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
    glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);

    GLFWwindow* window = glfwCreateWindow(mode->width, mode->height, "Race Game", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    char* path;
    int dirnameLength;
    int length = wai_getExecutablePath(NULL, 0, NULL);
    path = (char*)malloc(length + 1);
    wai_getExecutablePath(path, length, &dirnameLength);
    path[length] = '\0';

    // Convert to string and get scene path
    std::copy(path, path + length, std::back_inserter(stringPath));
    size_t foundIndex = stringPath.find_last_of("/\\");
    if (foundIndex == std::string::npos) {
        std::cout << "Failed to find executable path\n";
        return 1;
    }
    stringPath = stringPath.substr(0, foundIndex);

    sceneManager.findScenes(stringPath);

    lastSceneTime = std::filesystem::last_write_time(sceneManager.getScene().scenePath);

    glEnable(GL_DEPTH_TEST);
    
    // Create default white texture for meshes without textures
    glGenTextures(1, &defaultTexture);
    glBindTexture(GL_TEXTURE_2D, defaultTexture);
    unsigned char white[] = {255, 255, 255, 255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    int fbWidth, fbHeight;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    glViewport(0, 0, fbWidth, fbHeight);

    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSetCursorPosCallback(window, mouseCallback); 
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetKeyCallback(window, keyCallback);

    Shader shader("shaders/vertexShader.glsl", "shaders/fragmentShader.glsl");
    Shader uvShader("shaders/vertexShader.glsl", "shaders/uv_debug.glsl");
    bool showUV = false;
    bool prevUState = false;
    bool showChecker = false;
    bool prevCState = false;

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    
    game.OnInit(&assetManager, &sceneManager, stringPath);
    game.setWindow(window);

    game.setDebug(true);

    currentCamera = &game.getCamera();

    while (!glfwWindowShouldClose(window)) {
        // Check for scene file changes
        auto currentSceneTime = std::filesystem::last_write_time(sceneManager.getScene().scenePath);
        if (currentSceneTime != lastSceneTime) {
            std::cout << "Scene file changed, reloading..." << std::endl;
            sceneManager.reloadScene(stringPath);
            lastSceneTime = currentSceneTime;
        }

        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        game.OnTick(deltaTime);

        game.getCamera().tick();

        processInput(window); // input

        // rendering code
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (WIREFRAME_MODE) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        } else {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }

        // toggle UV debug visualization with U key (edge detect)
        bool uPressed = glfwGetKey(window, GLFW_KEY_U) == GLFW_PRESS;
        if (uPressed && !prevUState) showUV = !showUV;
        prevUState = uPressed;

        // toggle checker visualization with C key
        bool cPressed = glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS;
        if (cPressed && !prevCState) showChecker = !showChecker;
        prevCState = cPressed;

        if (showUV) uvShader.use(); else shader.use();

        glm::mat4 view;
        view = glm::lookAt(game.getCamera().getCameraPos(), game.getCamera().getCameraPos() + game.getCamera().getCameraFront(), game.getCamera().getCameraUp());

        glm::mat4 projection = glm::mat4(1.0f);
        projection = glm::perspective(glm::radians(fov), (float)mode->width / (float)mode->height, nearPlane, farPlane);

        Shader &active = showUV ? uvShader : shader;
        active.setMat4("projection", projection);
        active.setMat4("view", view);
        active.setBool("debug", false);
        shader.setVec3("material.baseColor", glm::vec3(1.0f));
        
        game.OnPreRender(active, deltaTime);

        ParsedScene currentScene = sceneManager.getScene();
        std::string basePath = stringPath; // keep original base so we don't mutate it repeatedly
        for (auto& sceneObject : currentScene.sceneObjects) {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, sceneObject.transform.pos);
            model = glm::scale(model, sceneObject.transform.scale);
            active.setMat4("model", model);
            std::string fullModelPath = basePath + "/" + AssetManager::buildModelPath(sceneObject.modelPath);
            assetManager.modelPool.getOrLoad(fullModelPath)->draw(active);
        }

        game.OnPostRender(active, deltaTime);

        // end of rendering code

        // poll and swap
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    game.OnShutdown();

    glfwTerminate();
    return 0;
}

void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void processInput(GLFWwindow *window) {
    const float cameraSpeed = 2.5f * deltaTime;

    if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

void mouseCallback(GLFWwindow* window, double xpos, double ypos) {
    if (currentCamera) {
        currentCamera->processMouse(xpos, ypos);
    }
}

void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    if (currentCamera) {
        currentCamera->updateOrbitRadius((float)yoffset);
    }
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_R && action == GLFW_PRESS) {
        std::cout << "Hotswapping current scene" << '\n';
        sceneManager.reloadScene(stringPath);   
    }
    if (key == GLFW_KEY_P && action == GLFW_PRESS) {
        gameRef.setDebug(!gameRef.getDebug());
    }
    if (key == GLFW_KEY_F && action == GLFW_PRESS) {
        WIREFRAME_MODE = !WIREFRAME_MODE;
        gameRef.setWireframe(WIREFRAME_MODE);
    }
}