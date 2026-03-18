#pragma once

#include "shader.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <unordered_map>

#include "tiny_obj_loader.h"
#include "stb_image.h"

#include <vector>
#include <iostream>

#define MAX_BONE_INFLUENCE 4

extern unsigned int defaultTexture;

// Forward declarations
class AssetManager;

struct Texture {
    unsigned int id;
    std::string type;
    std::string path;
};

class Resource {
public:
    std::string filePath;
    virtual void load(const std::string& path) = 0;
    virtual ~Resource() {}
};

class TextureResource : public Resource {
public:
    void load(const std::string& path) override {
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
    
    Mesh() = default;
    
    Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices, std::vector<Texture> textures) 
        : vertices(std::move(vertices)), indices(std::move(indices)), textures(std::move(textures)) {
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
        if (textures.empty()) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, defaultTexture);
            shader.setInt("material.texture_diffuse1", 0);
        }
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

// Forward declaration of ModelResource
class ModelResource;

// AssetManager definition with nested Pool template
class AssetManager {
public:
    static std::string buildModelPath(const std::string &modelPath) {
        namespace fs = std::filesystem;
        fs::path p(modelPath);
        std::string stem = p.filename().string();
        if (stem.empty()) {
            // fallback if path ends with slash or is empty
            return modelPath;
        }
        // construct candidate: original path + '/' + stem + ".obj"
        fs::path candidate = p;
        candidate /= stem + ".obj";
        return candidate.string();
    }

    template<typename T>
    class Pool {
    public:
        Pool(AssetManager* manager) : assetManager(manager) {}
        
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
        AssetManager* assetManager;
    };

    Pool<TextureResource> texturePool;
    Pool<ModelResource> modelPool;

    AssetManager() : texturePool(this), modelPool(this) {}

    ModelResource* loadModel(const std::string& path) {
        ModelResource* model = modelPool.getOrLoad(path);
        return model;
    }
};

// Now define ModelResource after AssetManager is declared
class ModelResource : public Resource {
private:
    static AssetManager* s_assetManager;
    std::vector<Mesh> meshes;
    std::string directory;

public:
    static void setAssetManager(AssetManager* manager) {
        s_assetManager = manager;
    }
    
    void draw(Shader &shader) {
        for (unsigned int i=0; i<meshes.size(); i++) {
            meshes[i].draw(shader);
        }
    }

    void load(const std::string& path) override {
        std::cerr << "ModelResource::load called with path: " << path << '\n';
        namespace fs = std::filesystem;
        if (fs::exists(path) && fs::is_directory(path)) {
            // try to resolve an obj within the directory
            fs::path dir(path);
            fs::path candidate = dir / (dir.filename().string() + ".obj");
            if (fs::exists(candidate)) {
                std::cerr << "Resolved directory to file: " << candidate.string() << '\n';
                load(candidate.string()); // recursion with proper path
                return;
            }
            std::cerr << "Error: directory passed to ModelResource::load and no obj found: " << path << '\n';
            return;
        }
        // store directory for texture loading
        size_t pos = path.find_last_of('/');
        if (pos == std::string::npos) {
            directory = "";
        } else {
            directory = path.substr(0, pos + 1);
        }
        tinyobj::ObjReaderConfig reader_config;
        reader_config.mtl_search_path = directory;
        reader_config.triangulate = true;

        std::cerr << "Texture directory: " << directory << '\n';

        tinyobj::ObjReader reader;

        if (!reader.ParseFromFile(path, reader_config)) {
            if (!reader.Error().empty()) {
                std::cerr << "TinyObjReader: " << reader.Error();
            }
            std::cout << "parse failed for path: " << path << '\n';
            return;
        }
        std::cerr << "parse succeeded for " << path << '\n';
        if (!reader.Warning().empty()) {
            std::cout << "TinyObjReader warning: " << reader.Warning();
        }

        auto& attrib = reader.GetAttrib();
        auto& shapes = reader.GetShapes();
        auto& materials = reader.GetMaterials();

        std::cerr << "attrib sizes: verts=" << attrib.vertices.size()
                  << " normals=" << attrib.normals.size()
                  << " texcoords=" << attrib.texcoords.size() << '\n';
        std::cerr << "shapes count=" << shapes.size() << " materials=" << materials.size() << '\n';

        // Loop over shapes and create Mesh objects grouped by material
        for (size_t s = 0; s < shapes.size(); s++) {
            std::cerr << "processing shape " << s << " with faces="
                      << shapes[s].mesh.num_face_vertices.size() << '\n';
            // group by material id -> per-material vertex/index arrays
            struct MeshData { std::vector<Vertex> vertices; std::vector<unsigned int> indices; };
            std::map<int, MeshData> groups;

            size_t index_offset = 0;
            for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
                if (f % 500 == 0) {
                    std::cerr << "  shape " << s << " face " << f << "\n";
                }
                size_t fv = size_t(shapes[s].mesh.num_face_vertices[f]);

                int mat_id = -1;
                if (f < shapes[s].mesh.material_ids.size()) {
                    mat_id = shapes[s].mesh.material_ids[f];
                }

                for (size_t v = 0; v < fv; v++) {
                    if (v == 0 && f % 1000 == 0) {
                        std::cerr << "    face " << f << " starts, fv=" << fv << "\n";
                    }
                    tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
                    Vertex vertex;
                    vertex.Tangent = glm::vec3(0.0f);
                    vertex.Bitangent = glm::vec3(0.0f);
                    for (int i = 0; i < MAX_BONE_INFLUENCE; i++) {
                        vertex.m_BoneIDs[i] = 0;
                        vertex.m_Weights[i] = 0.0f;
                    }

                    if (idx.vertex_index >= 0) {
                        size_t vi = size_t(idx.vertex_index);
                        if (vi >= attrib.vertices.size() / 3) {
                            std::cerr << "ERROR: vertex_index out of range: " << vi << " max=" << attrib.vertices.size() / 3 << '\n';
                            vertex.position = glm::vec3(0.0f);
                        } else {
                            tinyobj::real_t vx = attrib.vertices[3 * vi + 0];
                            tinyobj::real_t vy = attrib.vertices[3 * vi + 1];
                            tinyobj::real_t vz = attrib.vertices[3 * vi + 2];
                            vertex.position = glm::vec3(vx, vy, vz);
                        }
                    }

                    if (idx.normal_index >= 0 && !attrib.normals.empty()) {
                        size_t ni = size_t(idx.normal_index);
                        if (ni >= attrib.normals.size() / 3) {
                            std::cerr << "ERROR: normal_index out of range: " << ni << " max=" << attrib.normals.size() / 3 << '\n';
                            vertex.normal = glm::vec3(0.0f);
                        } else {
                            tinyobj::real_t nx = attrib.normals[3 * ni + 0];
                            tinyobj::real_t ny = attrib.normals[3 * ni + 1];
                            tinyobj::real_t nz = attrib.normals[3 * ni + 2];
                            vertex.normal = glm::vec3(nx, ny, nz);
                        }
                    } else {
                        vertex.normal = glm::vec3(0.0f);
                    }

                    if (idx.texcoord_index >= 0 && !attrib.texcoords.empty()) {
                        size_t ti = size_t(idx.texcoord_index);
                        if (ti >= attrib.texcoords.size() / 2) {
                            std::cerr << "ERROR: texcoord_index out of range: " << ti << " max=" << attrib.texcoords.size() / 2 << '\n';
                            vertex.texCoord = glm::vec2(0.0f);
                        } else {
                            tinyobj::real_t tx = attrib.texcoords[2 * ti + 0];
                            tinyobj::real_t ty = attrib.texcoords[2 * ti + 1];
                            vertex.texCoord = glm::clamp(glm::vec2(tx, 1.0f - ty), 0.0f, 1.0f);
                        }
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
                        
                        TextureResource* res = s_assetManager->texturePool.getOrLoad(fullPath);
                        
                        Texture tex;
                        tex.id = res->getID();
                        tex.type = "texture_diffuse";
                        tex.path = fullPath;
                        meshTextures.push_back(tex);
                    }
                    
                    if (!mat.specular_texname.empty()) {
                        TextureResource* res = s_assetManager->texturePool.getOrLoad(directory + mat.specular_texname);
                        meshTextures.push_back({res->getID(), "texture_specular", mat.specular_texname});
                    }
                }
                meshes.emplace_back(Mesh(entry.second.vertices, entry.second.indices, meshTextures));
            }
        }
    }
};
