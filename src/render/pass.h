#ifndef RENDER_PASS_H
#define RENDER_PASS_H 1


#include <vulkan/vulkan.h>
#include "sync.h"

typedef struct
{
	VkImage image;
	VkDeviceMemory mem;
	VkImageView view;
	VkSampler sampler;
	VkFramebuffer fb;
} FramebufferAttachment;

typedef struct 
{
	VkRenderPass pass;

	VkCommandPool commandPool;
	VkCommandBuffer* commandBuffers;

	VkDescriptorSetLayout descSetLayout;

	VkPipelineLayout pipelineLayout; 
	VkPipeline pipeline;
} RenderPass;

typedef struct
{
	VkRenderPassCreateInfo renderPass;
	VkCommandPoolCreateInfo cmdPool;
	VkCommandBufferAllocateInfo cmdBuf;
	VkDescriptorSetLayoutCreateInfo descSetLayout;
} RenderPassCreateInfos;

typedef struct
{
	VkPipelineVertexInputStateCreateInfo vertexInputState;
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState;
	VkPipelineViewportStateCreateInfo viewportState;
	VkPipelineRasterizationStateCreateInfo rasterState;
	VkPipelineMultisampleStateCreateInfo multisampleState;
	VkPipelineColorBlendStateCreateInfo colourBlendState;
	VkPipelineDepthStencilStateCreateInfo depthStencilState;
	VkPipelineDynamicStateCreateInfo dynamicState;
	VkGraphicsPipelineCreateInfo pipeline;
} PipelineCreateInfos;

int createRenderPassInfos(RenderPassCreateInfos* info);
int createRenderPass(VkDevice dev, RenderPassCreateInfos info, RenderPass* p);

int createPipelineInfos(PipelineCreateInfos* info);
int pipelineAllocColourBlendAttachments(PipelineCreateInfos* info, int count);
int pipelineAddViewport(PipelineCreateInfos* info, VkViewport viewport);
int pipelineAddScissor(PipelineCreateInfos* info, VkRect2D scissor);

int createDescriptorSets(
	VkDevice device,
	VkDescriptorSetLayout layout,
	VkDescriptorPool pool,
	VkDescriptorSet* descSets,
	unsigned int setCount
);

int createFramebufferAttachment(
	FramebufferAttachment* buf,
	VkFormat fmt,
	VkDevice device,
	VkPhysicalDevice physDev,
	VkExtent2D extent,
	VkImageAspectFlagBits aspect,
	VkImageUsageFlagBits usage
);

extern const VkRenderPassCreateInfo defRenderPassCreateInfo;
extern const VkCommandBufferAllocateInfo defCommandBufferAllocateInfo;
extern const VkAttachmentDescription defAttachmentDescription;
extern const VkImageViewCreateInfo defImageViewCreateInfo;
extern const VkRenderPassBeginInfo defRenderPassBeginInfo;
extern const VkCommandPoolCreateInfo defCommandPoolCreateInfo;
extern const VkCommandBufferAllocateInfo defCommandBufferCreateInfo;

void destroyRenderPass(VkDevice device, RenderPass pass);
void destroyFramebufferAttachment(VkDevice device, FramebufferAttachment att);

#endif

