#version 330 core
out vec4 fragColor;

in vec2 texCoord;

void main() {
    // visualize UVs: red = U, green = V
    fragColor = vec4(texCoord, 0.0, 1.0);
}
