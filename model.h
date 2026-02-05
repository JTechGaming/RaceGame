#pragma once

#include "shader.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <assimp/include/assimp/Importer.hpp>
#include <assimp/include/assimp/scene.h>
#include <assimp/include/assimp/postprocess.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <string>
#include <vector>

#define MAX_BONE_INFLUENCE 4

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
    glm::vec3 Tangent;
    glm::vec3 Bitangent;
	int m_BoneIDs[MAX_BONE_INFLUENCE];
	float m_Weights[MAX_BONE_INFLUENCE];
};

struct Texture {
    unsigned int id;
    std::string type;
    std::string path;
}; 

class Mesh {
    public:
        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
        std::vector<Texture> textures;

        Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices, std::vector<Texture> textures);
        void draw(Shader &shader);
    private:
        unsigned int VAO, VBO, EBO;

        void setupMesh();
};  

class Model 
{
    public:
        Model(char *path) {
            loadModel(path);
        }
        void draw(Shader &shader);	
    private:
        // model data
        std::vector<Texture> texturesLoaded;
        std::vector<Mesh> meshes;
        std::string directory;

        void loadModel(std::string path);
        void processNode(aiNode *node, const aiScene *scene);
        Mesh processMesh(aiMesh *mesh, const aiScene *scene);
        std::vector<Texture> loadMaterialTextures(aiMaterial *mat, aiTextureType type, 
                                             std::string typeName);
};