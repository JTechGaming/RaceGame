#version 330 core
out vec4 fragColor;

in vec2 texCoord;
in vec3 Normal;

struct Material {
    sampler2D texture_diffuse1;
    sampler2D texture_specular1;
};

uniform Material material;

void main()
{
    vec4 tex = texture(material.texture_diffuse1, texCoord);
    fragColor = tex;
}