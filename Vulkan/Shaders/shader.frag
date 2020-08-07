#version 450

layout(location = 0 ) in vec3 fragCol;
layout(location = 1) in vec2 fragTex;

layout(set=1, binding= 0) uniform sampler2D textureSampler;

//Out layouts
layout(location = 0) out vec4 outColor; //Final output color


void main()
{
    outColor = texture(textureSampler,fragTex);
}