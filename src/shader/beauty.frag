#version 450

layout(binding = 1) uniform GlobalUniformBufferObject
{
	mat4 view;
	mat4 proj;
} globalUbo;

layout(binding = 0) uniform sampler2D colourSampler;
layout(binding = 1) uniform sampler2D normalSampler;
layout(binding = 2) uniform sampler2D positionSampler;

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColour;

void main()
{
	outColour = texture(colourSampler, inUV);
}

