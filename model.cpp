#define TINYOBJLOADER_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "model.hpp"
#include <iostream>
#include <map>
#include <cstring>

Mesh::Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices, std::vector<Texture> textures) {
    this->vertices = vertices;
    this->indices = indices;
    this->textures = textures;

    setupMesh();
}

void Mesh::setupMesh() {
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

void Mesh::draw(Shader &shader) {
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

void Model::draw(Shader &shader) {
    for (unsigned int i=0; i<meshes.size(); i++) {
        meshes[i].draw(shader);
    }
}

void Model::loadModel(std::string path) {
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

    // cache for loaded textures to avoid duplicates
    std::map<std::string, Texture> loadedTextures;

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

        // For each material group, create a Mesh and load its textures
        for (auto &entry : groups) {
            int mat_id = entry.first;
            MeshData &md = entry.second;

            std::vector<Texture> textures;
            if (mat_id >= 0 && mat_id < (int)materials.size()) {
                const tinyobj::material_t &mat = materials[mat_id];

                if (!mat.diffuse_texname.empty()) {
                    std::string texPath = directory + mat.diffuse_texname;
                    auto it = loadedTextures.find(texPath);
                    if (it == loadedTextures.end()) {
                        unsigned int texID = TextureFromFile(texPath.c_str());
                        Texture tex;
                        tex.id = texID;
                        tex.type = "texture_diffuse";
                        tex.path = texPath;
                        textures.push_back(tex);
                        loadedTextures[texPath] = tex;
                        texturesLoaded.push_back(tex);
                    } else {
                        textures.push_back(it->second);
                    }
                }

                if (!mat.specular_texname.empty()) {
                    std::string texPath = directory + mat.specular_texname;
                    auto it = loadedTextures.find(texPath);
                    if (it == loadedTextures.end()) {
                        unsigned int texID = TextureFromFile(texPath.c_str());
                        Texture tex;
                        tex.id = texID;
                        tex.type = "texture_specular";
                        tex.path = texPath;
                        textures.push_back(tex);
                        loadedTextures[texPath] = tex;
                        texturesLoaded.push_back(tex);
                    } else {
                        textures.push_back(it->second);
                    }
                }
            }

            meshes.emplace_back(md.vertices, md.indices, textures);
        }
    }
}

unsigned int Model::TextureFromFile(const char *path) {
    int width, height, nrComponents;
    // Don't flip images here; we invert OBJ V coordinates instead
    stbi_set_flip_vertically_on_load(false);
    unsigned char *data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (!data) {
        std::cerr << "Texture failed to load at path: " << path << std::endl;
        return 0;
    }

    GLenum format = GL_RGB;
    if (nrComponents == 1)
        format = GL_RED;
    else if (nrComponents == 3)
        format = GL_RGB;
    else if (nrComponents == 4)
        format = GL_RGBA;

    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
    return textureID;
}