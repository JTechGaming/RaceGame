#version 330 core
out vec4 fragColor;

in vec2 texCoord;
in vec3 Normal;

struct Material {
    sampler2D texture_diffuse1;
    sampler2D texture_specular1;
    vec3 baseColor;
};

uniform Material material;
uniform bool debug;

void main() {
    vec4 tex = texture(material.texture_diffuse1, texCoord);
    // If no texture is provided, defaultTexture is white and baseColor holds Kd fallback.
    if (debug) {
        fragColor = vec4(material.baseColor, 1.0);
    } else {
        fragColor = vec4(material.baseColor * tex.rgb, tex.a);
    }
}