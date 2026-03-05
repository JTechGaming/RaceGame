#pragma once

#include "shader.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <vector>
#include <iostream>

#define MAX_BONE_INFLUENCE 4

class Resource {
public:
    std::string filePath;
    virtual void load(const std::string& path, AssetManager& assetManager) = 0;
    virtual ~Resource() {}
};

class TextureResource : Resource {
public:
    void load(const std::string& path, AssetManager& assetManager) override {
        // Don't flip images here; we invert OBJ V coordinates instead
        stbi_set_flip_vertically_on_load(false);
        unsigned char *data = stbi_load(path.c_str(), &width, &height, &nrComponents, 0);
        if (!data) {
            std::cerr << "Texture failed to load at path: " << path << std::endl;
            return;
        }

        GLenum format = GL_RGB;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }

    unsigned int getID() const {
        return textureID;
    }

    ~TextureResource() {
        if (textureID != 0) glDeleteTextures(1, &textureID);
    }

private:
    unsigned int textureID = 0;
    int width, height, nrComponents;
};

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
    glm::vec3 Tangent;
    glm::vec3 Bitangent;
	int m_BoneIDs[MAX_BONE_INFLUENCE];
	float m_Weights[MAX_BONE_INFLUENCE];
};

class Mesh {
public:
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    std::vector<Texture> textures;   
    
    Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices, std::vector<Texture> textures) {
        this->vertices = vertices;
        this->indices = indices;
        this->textures = textures;

        setupMesh();
    }
    void setupMesh() {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);
    
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);

        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);  

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), 
                    &indices[0], GL_STATIC_DRAW);

        // vertex positions
        glEnableVertexAttribArray(0);	
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
        // vertex normals
        glEnableVertexAttribArray(1);	
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
        // vertex texture coords
        glEnableVertexAttribArray(2);	
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoord));

        glBindVertexArray(0);
    }  
    void draw(Shader &shader) {
        unsigned int diffuseNr = 1;
        unsigned int specularNr = 1;
        for(unsigned int i = 0; i < textures.size(); i++)
        {
            glActiveTexture(GL_TEXTURE0 + i);
            // retrieve texture number (the N in diffuse_textureN)
            std::string number;
            std::string name = textures[i].type;
            if(name == "texture_diffuse")
                number = std::to_string(diffuseNr++);
            else if(name == "texture_specular")
                number = std::to_string(specularNr++);

            shader.setInt(("material." + name + number).c_str(), i);
            glBindTexture(GL_TEXTURE_2D, textures[i].id);
        }
        glActiveTexture(GL_TEXTURE0);

        // draw mesh
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
private:
    unsigned int VAO, VBO, EBO;
};

struct Texture {
    unsigned int id;;
    std::string type;
    std::string path;
};

class ModelResource : public Resource {
public:
    void draw(Shader &shader) {
        for (unsigned int i=0; i<meshes.size(); i++) {
            meshes[i].draw(shader);
        }
    }

    void load(const std::string& path, AssetManager& assetManager) override {
        // store directory for texture loading
        size_t pos = path.find_last_of('/');
        if (pos == std::string::npos) {
            directory = "";
        } else {
            directory = path.substr(0, pos + 1);
        }
        tinyobj::ObjReaderConfig reader_config;
        reader_config.mtl_search_path = directory;

        tinyobj::ObjReader reader;

        if (!reader.ParseFromFile(path, reader_config)) {
            if (!reader.Error().empty()) {
                std::cerr << "TinyObjReader: " << reader.Error();
            }
            return;
        }

        if (!reader.Warning().empty()) {
            std::cout << "TinyObjReader: " << reader.Warning();
        }

        auto& attrib = reader.GetAttrib();
        auto& shapes = reader.GetShapes();
        auto& materials = reader.GetMaterials();

        // Loop over shapes and create Mesh objects grouped by material
        for (size_t s = 0; s < shapes.size(); s++) {
            // group by material id -> per-material vertex/index arrays
            struct MeshData { std::vector<Vertex> vertices; std::vector<unsigned int> indices; };
            std::map<int, MeshData> groups;

            size_t index_offset = 0;
            for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
                size_t fv = size_t(shapes[s].mesh.num_face_vertices[f]);

                int mat_id = -1;
                if (f < shapes[s].mesh.material_ids.size()) {
                    mat_id = shapes[s].mesh.material_ids[f];
                }

                for (size_t v = 0; v < fv; v++) {
                    tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
                    Vertex vertex;

                    if (idx.vertex_index >= 0) {
                        tinyobj::real_t vx = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
                        tinyobj::real_t vy = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
                        tinyobj::real_t vz = attrib.vertices[3 * size_t(idx.vertex_index) + 2];
                        vertex.position = glm::vec3(vx, vy, vz);
                    }

                    if (idx.normal_index >= 0 && !attrib.normals.empty()) {
                        tinyobj::real_t nx = attrib.normals[3 * size_t(idx.normal_index) + 0];
                        tinyobj::real_t ny = attrib.normals[3 * size_t(idx.normal_index) + 1];
                        tinyobj::real_t nz = attrib.normals[3 * size_t(idx.normal_index) + 2];
                        vertex.normal = glm::vec3(nx, ny, nz);
                    } else {
                        vertex.normal = glm::vec3(0.0f);
                    }

                    if (idx.texcoord_index >= 0 && !attrib.texcoords.empty()) {
                        tinyobj::real_t tx = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
                        tinyobj::real_t ty = attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];
                        vertex.texCoord = glm::vec2(tx, ty);
                    } else {
                        vertex.texCoord = glm::vec2(0.0f);
                    }

                    MeshData &md = groups[mat_id];
                    md.vertices.push_back(vertex);
                    md.indices.push_back(static_cast<unsigned int>(md.vertices.size() - 1));
                }

                index_offset += fv;
            }

            meshes.reserve(groups.size());
            for (auto &entry : groups) {
                int mat_id = entry.first;
                std::vector<Texture> meshTextures;

                if (mat_id >= 0 && mat_id < (int)materials.size()) {
                    const auto& mat = materials[mat_id];

                    if (!mat.diffuse_texname.empty()) {
                        std::string fullPath = directory + mat.diffuse_texname;
                        
                        TextureResource* res = assetManager.texturePool.getOrLoad(fullPath);
                        
                        Texture tex;
                        tex.id = res->getID();
                        tex.type = "texture_diffuse";
                        tex.path = fullPath;
                        meshTextures.push_back(tex);
                    }
                    
                    if (!mat.specular_texname.empty()) {
                        TextureResource* res = assetManager.texturePool.getOrLoad(directory + mat.specular_texname);
                        meshTextures.push_back({res->getID(), "texture_specular", mat.specular_texname});
                    }
                }
                meshes.emplace_back(entry.second.vertices, entry.second.indices, meshTextures);
            }
        }
    }
private:
    std::vector<Mesh> meshes;
    std::string directory;
};

template<typename T>
class AssetPool {
public:
    T* getOrLoad(const std::string& path) {
        if (pool.find(path) != pool.end()) {
            return pool[path];
        }
        T* resource = new T();
        resource->load(path);
        pool[path] = resource;
        return resource;
    }

    void cleanup() {
        for (auto const& [path, resource] : pool) {
            delete resource;
        }
        pool.clear();
    }   

private:
    std::unordered_map<std::string, T*> pool;
};

class AssetManager {
public:
    AssetPool<TextureResource> texturePool;
    AssetPool<ModelResource> modelPool;

    ModelResource* loadModel(const std::string& path) {
        ModelResource* model = modelPool.getOrLoad(path);
        return model;
    }
};