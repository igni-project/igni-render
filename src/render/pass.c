#include "pass.h"

#include "misc.h"
#include "physdev.h"
#include <stdio.h>
#include <stdlib.h>

int createPipelineInfos(PipelineCreateInfos* info)
{
	info->vertexInputState.sType =
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	info->vertexInputState.vertexBindingDescriptionCount = 0;
	info->vertexInputState.vertexAttributeDescriptionCount = 0;

	info->inputAssemblyState.sType =
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	info->inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	info->inputAssemblyState.primitiveRestartEnable = VK_FALSE;

	info->viewportState.sType = 
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;

	info->rasterState.sType =
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	info->rasterState.depthClampEnable = VK_FALSE;
	info->rasterState.rasterizerDiscardEnable = VK_FALSE;
	info->rasterState.polygonMode = VK_POLYGON_MODE_FILL;
	info->rasterState.lineWidth = 1.0f;
	info->rasterState.cullMode = VK_CULL_MODE_NONE;
	info->rasterState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	info->rasterState.depthBiasEnable = VK_FALSE;

	info->multisampleState.sType = 
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	info->multisampleState.sampleShadingEnable = VK_FALSE;
	info->multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	info->multisampleState.minSampleShading = 1.0f;

	info->colourBlendState.sType =
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	info->colourBlendState.logicOpEnable = VK_FALSE;

	info->depthStencilState.sType =
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	info->depthStencilState.depthTestEnable = VK_TRUE;
	info->depthStencilState.depthWriteEnable = VK_TRUE;
	info->depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS;
	info->depthStencilState.depthBoundsTestEnable = VK_FALSE;
	info->depthStencilState.stencilTestEnable = VK_FALSE;

	info->dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;

	info->pipeline.pVertexInputState = &info->vertexInputState;
	info->pipeline.pInputAssemblyState = &info->inputAssemblyState;
	info->pipeline.pViewportState = &info->viewportState;
	info->pipeline.pRasterizationState = &info->rasterState;
	info->pipeline.pMultisampleState = &info->multisampleState;
	info->pipeline.pColorBlendState = &info->colourBlendState;
	info->pipeline.pDepthStencilState = &info->depthStencilState;
	info->pipeline.pDynamicState = &info->dynamicState;

	return 0;
}

int pipelineAllocColourBlendAttachments(PipelineCreateInfos* info, int count)
{
	info->colourBlendState.attachmentCount = count;
	info->colourBlendState.pAttachments = 
		malloc(sizeof(VkPipelineColorBlendAttachmentState) * count);

	if (!info->colourBlendState.pAttachments) {
		perror("Failed to allocate colour blend attachments");
		return -1;
	}

	return 0;
}

int pipelineAddViewport(PipelineCreateInfos* info, VkViewport viewport)
{
	return 0;
}

int pipelineAddScissor(PipelineCreateInfos* info, VkRect2D scissor)
{
	return 0;
}

int createDescriptorSets(
	VkDevice device,
	VkDescriptorSetLayout layout,
	VkDescriptorPool pool,
	VkDescriptorSet* descSets,
	unsigned int setCount
)
{
	VkDescriptorSetLayout layouts[setCount];
	for (int i = 0; i < setCount; i++) {
		layouts[i] = layout;
	}

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = pool;
	allocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
	allocInfo.pSetLayouts = layouts;

	if (vkAllocateDescriptorSets(device, &allocInfo, descSets) != VK_SUCCESS) {
		printf("Failed to allocate descriptor sets.\n");
		return -1;
	}

	return 0;
}

int createFramebufferAttachment(
	FramebufferAttachment* buf,
	VkFormat fmt,
	VkDevice device,
	VkPhysicalDevice physDev,
	VkExtent2D extent,
	VkImageAspectFlagBits aspect,
	VkImageUsageFlagBits usage
)
{

	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = extent.width;
	imageInfo.extent.height = extent.height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = fmt;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = usage;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

	if (vkCreateImage(device, &imageInfo, 0, &buf->image) != VK_SUCCESS) {
		printf("Failed to create image\n");
		return -1;
	}
	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(device, buf->image, &memRequirements);

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(
		physDev,
		memRequirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	if (vkAllocateMemory(device, &allocInfo, 0, &buf->mem) != VK_SUCCESS) {
		printf("Failed to allocate image memory\n");
		return -1;

	}

	vkBindImageMemory(device, buf->image, buf->mem, 0);

	if (createImageView(device, buf->image, fmt, aspect, 1, &buf->view)) {
		printf("Failed to create image view\n");
		return -1;
	}

	if (createSampler(device, physDev, &buf->sampler, 1)) {
		return -1;
	}

	return 0;
}

const VkRenderPassCreateInfo defRenderPassCreateInfo = {
	.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO
};

const VkCommandBufferAllocateInfo defCommandBufferAllocateInfo = {
	.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
	.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
	.commandBufferCount = MAX_FRAMES_IN_FLIGHT

};

const VkAttachmentDescription defAttachmentDescription = {
	.format = VK_FORMAT_UNDEFINED,
	.samples = VK_SAMPLE_COUNT_1_BIT,
	.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
	.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
	.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
	.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	.finalLayout = VK_IMAGE_LAYOUT_UNDEFINED
};

const VkImageViewCreateInfo defImageViewCreateInfo = {
	.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
	.viewType = VK_IMAGE_VIEW_TYPE_2D,

	.components.r = VK_COMPONENT_SWIZZLE_IDENTITY,
	.components.g = VK_COMPONENT_SWIZZLE_IDENTITY,
	.components.b = VK_COMPONENT_SWIZZLE_IDENTITY,
	.components.a = VK_COMPONENT_SWIZZLE_IDENTITY,

	.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
	.subresourceRange.baseMipLevel = 0,
	.subresourceRange.levelCount = 1,
	.subresourceRange.baseArrayLayer = 0,
	.subresourceRange.layerCount = 1
};

const VkRenderPassBeginInfo defRenderPassBeginInfo = {
	.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO
};


const VkCommandPoolCreateInfo defCommandPoolCreateInfo = {
	.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
	.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
};

void destroyRenderPass(VkDevice device, RenderPass pass)
{
	vkDestroyRenderPass(device, pass.pass, 0);

	vkDestroyDescriptorSetLayout(device, pass.descSetLayout, 0);
	vkDestroyCommandPool(device, pass.commandPool, 0);

	vkDestroyPipelineLayout(device, pass.pipelineLayout, 0);
	vkDestroyPipeline(device, pass.pipeline, 0);
}

void destroyFramebufferAttachment(VkDevice device, FramebufferAttachment att)
{
	vkDestroyImage(device, att.image, 0);
	vkFreeMemory(device, att.mem, 0);
	vkDestroyImageView(device, att.view, 0);
	vkDestroySampler(device, att.sampler, 0);
}

