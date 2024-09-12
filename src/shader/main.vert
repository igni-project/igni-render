#version 450

layout(binding = 0) uniform GlobalUniformBufferObject
{
	mat4 view;
	mat4 proj;
} globalUbo;

layout(binding = 2) uniform UniformBufferObject
{
	mat4 model;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColour;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec3 fragPosition;

void main()
{
	gl_Position = globalUbo.proj
		* globalUbo.view
		* ubo.model
		* vec4(inPosition, 1.0);

	fragTexCoord = inTexCoord;

	fragPosition = vec3(globalUbo.view * ubo.model * vec4(inPosition, 1.0));

	mat3 normalMat = transpose(inverse(mat3(globalUbo.view * ubo.model)));
	fragNormal = normalMat * inNormal;

}

