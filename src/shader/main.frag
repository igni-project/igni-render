#version 450

layout(binding = 0) uniform GlobalUniformBufferObject
{
	mat4 view;
	mat4 proj;
} globalUbo;

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragColour;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 fragPosition;

layout(location = 0) out vec4 outColour;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outPosition;

void main()
{
	outColour = texture(texSampler, fragTexCoord);
	outNormal = vec4(normalize(fragNormal * 0.5 + 0.5), 1.0);
	outPosition = vec4(fragPosition, 1.0);
}


