#version 450

layout(constant_id = 0) const int grid_size = 1000;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    uvec2 pos;
    float zoom;
} ubo;

layout(location = 0) in uvec2 inVertex;
layout(location = 1) in uvec2 inCellPos;
layout(location = 2) in uint inCellAlive;

layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position = vec4(
        ((inCellPos.x + inVertex.x) * ubo.zoom) / grid_size,
        ((inCellPos.y + inVertex.y) * ubo.zoom) / grid_size,
        0.,
        1.
    );
    if (inCellAlive == 1) {
        fragColor = vec3(0.2, 1., 0.);
    } else {
        fragColor = vec3(0.2, .2, .2);
    }
}