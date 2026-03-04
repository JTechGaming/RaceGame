#include "rapidyaml-0.10.0.hpp"
#include "whereami.h"

#include <filesystem>
#include <iostream>
#include <fstream>
#include <string>
#include <sys/stat.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

struct LoadedModel {
    std::string modelName;
    std::string modelPath;
};

struct Transform {
    glm::vec3 pos;
    glm::vec3 rot;
    glm::vec3 scale;
};

struct SceneObject {
    std::string modelName;
    uint8_t pawnID = 0;
    Transform transform;
};

struct ParsedScene {
    std::string sceneName;
    std::vector<LoadedModel> loadedModels;
    std::vector<SceneObject> sceneObjects;
    uint8_t startingPawn;
};

// forward declarations
void parseScene(const std::string *path);
void loadScene(const ParsedScene* scene);
void unloadScene();

void findScenes() {
    // Get executable path
    char* path;
    int dirnameLength;
    int length = wai_getExecutablePath(NULL, 0, NULL);
    path = (char*)malloc(length + 1);
    wai_getExecutablePath(path, length, &dirnameLength);
    path[length] = '\0';

    // Convert to string and get scene path
    std::string stringPath;
    std::copy(path, path + length, std::back_inserter(stringPath));
    char regex = '/';
    size_t foundIndex;
    foundIndex = stringPath.find_last_of(regex);
    if (foundIndex == std::string::npos) {
        std::cout << "Failed to find executable path\n";
        return;
    }

    // Find all scenes
    std::string scenePath = stringPath.substr(0, foundIndex).append("/scenes");
    struct stat sb;
    for (const auto& entry : std::filesystem::directory_iterator(scenePath)) {
        std::filesystem::path outfilename = entry.path();
        std::string outfilename_str = outfilename.string();
        const char* filePath = outfilename_str.c_str();
        if (stat(filePath, &sb) == 0 && !(sb.st_mode & S_IFDIR)) {
            std ::cout << filePath << std::endl;
            std::string stringFilePath;
            std::copy(filePath, filePath + std::strlen(filePath), std::back_inserter(stringFilePath));
            if (stringFilePath.find(".yml") != std::string::npos) {
                parseScene(&stringFilePath);
            }
        }
    }
}

// trim whitespace from a string
static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, last - first + 1);
}

// safe float conversion with error handling
static float safe_stof(const c4::csubstr& cs, float default_val = 0.0f) {
    std::string s(cs.data(), cs.len);
    try {
        std::string trimmed = trim(s);
        if (trimmed.empty()) return default_val;
        return std::stof(trimmed);
    } catch (const std::exception& e) {
        std::cerr << "Warning: Could not convert '" << s << "' to float: " << e.what() << std::endl;
        return default_val;
    }
}

void parseScene(const std::string *path) {
    using namespace c4::yml;
    try {
        ParsedScene scene;

        // open the file and read its contents into a string
        std::ifstream ifs(*path);
        if (!ifs) {
            std::cerr << "Error: Failed to open scene file: " << *path << std::endl;
            return;
        }
        std::string content((std::istreambuf_iterator<char>(ifs)),
                            std::istreambuf_iterator<char>());

        // parse the YAML document; rapidyaml works with "csubstr" string views
        c4::csubstr yaml(content.c_str(), content.size());
        Tree tree = parse_in_arena(yaml);
        NodeRef root = tree.rootref();

        // example of accessing top‑level scalar
        if(root.has_child("name")) {
            c4::csubstr v = root["name"].val();
            std::string sceneName(v.data(), v.len);
            scene.sceneName = sceneName;
        }

        // iterate over the "models" sequence
        if(root.has_child("models")) {
            NodeRef models = root["models"];
            scene.loadedModels.reserve(models.num_children());
            for(auto it = models.begin(); it != models.end(); ++it) {
                c4::csubstr vn;
                c4::csubstr vid;
                std::string name, identifier;
                if((*it).has_child("name")) {
                    vn = (*it)["name"].val();
                    name.assign(vn.data(), vn.len);
                }
                if((*it).has_child("identifier")) {
                    vid = (*it)["identifier"].val();
                    identifier.assign(vid.data(), vid.len);
                }
                scene.loadedModels.emplace_back(std::move(name), std::move(identifier));
            }
        }

        // sequence of sceneObjects with nested mapping
        if(root.has_child("sceneObjects")) {
            NodeRef objs = root["sceneObjects"];
            scene.sceneObjects.reserve(objs.num_children());
            for(auto it = objs.begin(); it != objs.end(); ++it) {
                std::string model;
                uint8_t pawnID = 0;
                if((*it).has_child("model")) {
                    c4::csubstr v = (*it)["model"].val();
                    model.assign(v.data(), v.len);
                }
                if((*it).has_child("pawnID")) {
                    c4::csubstr v = (*it)["pawnID"].val();
                    pawnID = std::stoi(std::string(v.data(), v.len));
                }

                Transform transform;
                if((*it).has_child("transform")) {
                    NodeRef t = (*it)["transform"];
                    if(t.has_child("pos")) {
                        NodeRef seq = t["pos"];
                        float x = 0, y = 0, z = 0;
                        if(seq.num_children() >= 3) {
                            c4::csubstr vx = seq[0].val();
                            c4::csubstr vy = seq[1].val();
                            c4::csubstr vz = seq[2].val();
                            x = safe_stof(vx);
                            y = safe_stof(vy);
                            z = safe_stof(vz);
                        } else {
                            std::cerr << "Warning: pos in transform does not have 3 elements\n";
                        }
                        transform.pos = glm::vec3(x, y, z);
                    }
                    if(t.has_child("rot")) {
                        NodeRef seq = t["rot"];
                        float x = 0, y = 0, z = 0;
                        if(seq.num_children() >= 3) {
                            c4::csubstr vx = seq[0].val();
                            c4::csubstr vy = seq[1].val();
                            c4::csubstr vz = seq[2].val();
                            x = safe_stof(vx);
                            y = safe_stof(vy);
                            z = safe_stof(vz);
                        } else {
                            std::cerr << "Warning: rot in transform does not have 3 elements\n";
                        }
                        transform.rot = glm::vec3(x, y, z);
                    }
                    if(t.has_child("scale")) {
                        NodeRef seq = t["scale"];
                        float x = 0, y = 0, z = 0;
                        if(seq.num_children() >= 3) {
                            c4::csubstr vx = seq[0].val();
                            c4::csubstr vy = seq[1].val();
                            c4::csubstr vz = seq[2].val();
                            x = safe_stof(vx);
                            y = safe_stof(vy);
                            z = safe_stof(vz);
                        } else {
                            std::cerr << "Warning: scale in transform does not have 3 elements\n";
                        }
                        transform.scale = glm::vec3(x, y, z);
                    }
                }

                scene.sceneObjects.emplace_back(std::move(model), std::move(pawnID), transform);
            }
        }

        // Parse remaining scene settings
        if(root.has_child("startingPawn")) {
            c4::csubstr v = root["startingPawn"].val();
            uint8_t startingPawn = std::stoi(std::string(v.data(), v.len));
            scene.startingPawn = startingPawn;
        }

        //todo: collect error checking results
        loadScene(&scene);
    } catch (const std::exception& e) {
        std::cerr << "Error parsing scene: " << e.what() << std::endl;
        return;
    } catch (...) {
        std::cerr << "Unknown error while parsing scene\n";
        return;
    }
}

void loadScene(const ParsedScene* scene) {
    // todo: load
}

void unloadScene() {
    
}