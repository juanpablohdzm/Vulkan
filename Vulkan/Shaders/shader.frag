#version 450
//In layouts
layout(location = 0) in vec3 fragColor; //Interpolated color from vertex (location must match)

//Out layouts
layout(location = 0) out vec4 outColor; //Final output color


void main()
{
    outColor = vec4(fragColor, 1.0);
}