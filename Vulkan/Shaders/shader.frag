#version 450

layout(location = 0 ) in vec3 fragCol;

//Out layouts
layout(location = 0) out vec4 outColor; //Final output color


void main()
{
    outColor = vec4(fragCol, 1.0);
}