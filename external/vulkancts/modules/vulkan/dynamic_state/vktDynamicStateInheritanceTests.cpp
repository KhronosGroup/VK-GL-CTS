/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 NVIDIA Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief VK_NV_inherited_viewport_scissor Tests
 *
 * Simple test cases for secondary command buffers inheriting dynamic
 * viewport and scissor state from the calling primary command buffer
 * or an earlier secondary command buffer. Tests draw a bunch of color
 * rectangles using a trivial geometry pipeline (no vertex
 * transformation except for fixed-function viewport transform,
 * geometry shader selects viewport/scissor index). The depth test is
 * enabled to check for incorrect depth transformation.
 *//*--------------------------------------------------------------------*/
#include "vktDynamicStateInheritanceTests.hpp"

#include <math.h>
#include <sstream>
#include <vector>

#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"

using namespace vk;
using tcu::Vec2;
using tcu::Vec3;

namespace vkt
{
namespace DynamicState
{
namespace
{
// Size of test framebuffer, power of 2 to avoid rounding errors.
static const deInt32 kWidth = 256, kHeight = 128;

// Maximum viewport/scissors, and maximum rectangles, for any test case.
static const deUint32 kMaxViewports = 16, kMaxRectangles = 1024;

// Color format of framebuffer image, this seems universally supported.
static const VkFormat kFormat = VK_FORMAT_B8G8R8A8_UNORM;

// Texel data matching kFormat, and functions for converting to/from
// packed 32-bit color. alpha is unused.
struct Texel
{
	deUint8 blue, green, red, alpha;
};

inline Texel texelFrom_r8g8b8(deInt32 r8g8b8)
{
	return {deUint8(r8g8b8 & 255),
			deUint8((r8g8b8 >> 8) & 255),
			deUint8((r8g8b8 >> 16) & 255), 0u};
}

// Parameters of axis-aligned rectangle to rasterize.  No mvp matrix
// or anything, only testing fixed-function viewport transformation.
struct Rectangle
{
	Vec3    xyz;         // Before viewport transformation
	deInt32 r8g8b8;      // (8-bit) red << 16 | green << 8 | blue
	Vec2    widthHeight; // positive; before viewport transformation
	deInt32 viewportIndex;
};

// Determines where the secondary command buffer's inherited viewport/scissor state comes from (if inherited at all).
enum InheritanceMode
{
	kInheritanceDisabled,   // Disable extension, use non-dynamic viewport/scissor count
	kInheritFromPrimary,    // Inherit from calling primary cmd buffer
	kInheritFromSecondary,  // Inherit from earlier secondary cmd buffer
	kSplitInheritance,      // Split viewport/scissor array in two, inherit
							// some from primary and rest from secondary

	// Inherit state-with-count-EXT from calling primary cmd buffer
	kInheritFromPrimaryWithCount,
	// Inherit state-with-count-EXT from earlier secondary cmd buffer
	kInheritFromSecondaryWithCount,
};

// Input test geometry.
struct TestGeometry
{
	// Color and depth to clear the framebuffer to.
	Vec3    clearColor;
	float   clearDepth;

	// List of rectangles to rasterize, in order.
	std::vector<Rectangle>  rectangles;

	// List of viewports and scissors to use, both vectors must have
	// same length and have length at least 1.
	std::vector<VkViewport> viewports;
	std::vector<VkRect2D>   scissors;
	InheritanceMode inheritanceMode;
};


// Whether the test was a success, and both the device-rasterized image
// and the CPU-computed expected image.
struct TestResults
{
	bool passed;

	// Index with [y][x]
	Texel   deviceResult[kHeight][kWidth];
	Texel expectedResult[kHeight][kWidth];
};


// TODO probably tcu has a clamp already.
template <typename T>
inline T clamp(T x, T minVal, T maxVal)
{
	return std::min(std::max(x, minVal), maxVal);
}


class InheritanceTestInstance : public TestInstance
{
	const vk::InstanceInterface& m_in;
	const vk::DeviceInterface&   m_vk;
	InheritanceMode              m_inheritanceMode;

	// Vertex buffer storing rectangle list, and its mapping and
	// backing memory. kMaxRectangles is its capacity (in Rectangles).
	BufferWithMemory m_rectangleBuffer;

	// Buffer for downloading rendered image from device
	BufferWithMemory m_downloadBuffer;

	// Image attachments and views.
	// Create info for depth buffer set at runtime due to depth format search.
	VkImageCreateInfo     m_depthImageInfo;
	ImageWithMemory       m_colorImage,    m_depthImage;
	VkImageViewCreateInfo m_colorViewInfo, m_depthViewInfo;
	Unique<VkImageView>   m_colorView,     m_depthView;

	// Simple render pass and framebuffer.
	Move<VkRenderPass>  m_renderPass;
	Move<VkFramebuffer> m_framebuffer;

	// Shader modules for graphics pipelines.
	Move<VkShaderModule> m_vertModule, m_geomModule, m_fragModule;

	// Vertex, geometry, fragment stages for creating the pipeline.
	VkPipelineShaderStageCreateInfo m_stages[3];

	// Geometry shader pipeline, converts points into rasterized
	// struct Rectangles using geometry shader, which also selects the
	// viewport to use. Pipeline array maps viewport/scissor count to
	// the pipeline to use (special value 0 indicates that
	// viewport/scissor count is dynamic state).
	Move<VkPipelineLayout> m_rectanglePipelineLayout;
	Move<VkPipeline>         m_rectanglePipelines[kMaxViewports + 1];

	// Command pool
	Move<VkCommandPool> m_cmdPool;

	// Primary command buffer, re-used for every test
	VkCommandBuffer m_primaryCmdBuffer;

	// Secondary command buffers, first for specifying
	// viewport/scissor state, second for subpass contents.
	// Both re-used to check for stale state.
	VkCommandBuffer m_setStateCmdBuffer, m_subpassCmdBuffer;

	// "depth buffer" used for CPU rasterization of expected image.
	float m_cpuDepthBuffer[kHeight][kWidth];

public:
	InheritanceTestInstance(Context& context, InheritanceMode inheritanceMode);
	tcu::TestStatus iterate(void);

private:
	void startRenderCmds(const TestGeometry& geometry);
	void rasterizeExpectedResults(const TestGeometry& geometry, Texel (&output)[kHeight][kWidth]);
};



// Most state for graphics pipeline
namespace pipelinestate {

// Vertex shader, just pass through Rectangle data.
const char vert_glsl[] =
"#version 460\n"
"\n"
"layout(location=0) in vec3 xyz;\n"
"layout(location=1) in int r8g8b8;\n"
"layout(location=2) in vec2 widthHeight;\n"
"layout(location=3) in int viewportIndex;\n"
"\n"
"layout(location=0) flat out int o_r8g8b8;\n"
"layout(location=1) flat out vec2 o_widthHeight;\n"
"layout(location=2) flat out int o_viewportIndex;\n"
"\n"
"void main()\n"
"{\n"
"	gl_Position     = vec4(xyz, 1.0);\n"
"	o_r8g8b8        = r8g8b8;\n"
"	o_widthHeight   = widthHeight;\n"
"	o_viewportIndex = viewportIndex;\n"
"}\n";

// Geometry shader, convert points to rectangles and select correct viewport.
const char geom_glsl[] =
"#version 460\n"
"\n"
"layout(points) in;\n"
"layout(triangle_strip, max_vertices=4) out;\n"
"\n"
"layout(location=0) flat in int r8g8b8[];\n"
"layout(location=1) flat in vec2 widthHeight[];\n"
"layout(location=2) flat in int viewportIndex[];\n"
"\n"
"layout(location=0) flat out vec4 o_color;\n"
"\n"
"void main()\n"
"{\n"
"	int redBits   = (r8g8b8[0] >> 16) & 255;\n"
"	int greenBits = (r8g8b8[0] >> 8)  & 255;\n"
"	int blueBits  =  r8g8b8[0]        & 255;\n"
"	float n       = 1.0 / 255.0;\n"
"	vec4 color    = vec4(redBits * n, greenBits * n, blueBits * n, 1.0);\n"
"\n"
"	gl_ViewportIndex = viewportIndex[0];\n"
"	gl_Position = gl_in[0].gl_Position;\n"
"	o_color     = color;\n"
"	EmitVertex();\n"
"\n"
"	gl_ViewportIndex = viewportIndex[0];\n"
"	gl_Position = gl_in[0].gl_Position + vec4(0.0, widthHeight[0].y, 0.0, 0.0);\n"
"	o_color     = color;\n"
"	EmitVertex();\n"
"\n"
"	gl_ViewportIndex = viewportIndex[0];\n"
"	gl_Position = gl_in[0].gl_Position + vec4(widthHeight[0].x, 0.0, 0.0, 0.0);\n"
"	o_color     = color;\n"
"	EmitVertex();\n"
"\n"
"	gl_ViewportIndex = viewportIndex[0];\n"
"	gl_Position = gl_in[0].gl_Position + vec4(widthHeight[0].xy, 0.0, 0.0);\n"
"	o_color     = color;\n"
"	EmitVertex();\n"
"\n"
"	EndPrimitive();\n"
"}\n";

// Pass through fragment shader
const char frag_glsl[] =
"#version 460\n"
"layout(location=0) flat in vec4 color;\n"
"layout(location=0) out     vec4 o_color;\n"
"\n"
"void main()\n"
"{\n"
"	o_color = color;\n"
"}\n";

static const VkVertexInputBindingDescription binding = {0, sizeof(Rectangle), VK_VERTEX_INPUT_RATE_VERTEX};

static const VkVertexInputAttributeDescription attributes[4] = {
	{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Rectangle, xyz)},
	{1, 0, VK_FORMAT_R32_SINT, offsetof(Rectangle, r8g8b8)},
	{2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Rectangle, widthHeight)},
	{3, 0, VK_FORMAT_R32_SINT, offsetof(Rectangle, viewportIndex)} };

static const VkPipelineVertexInputStateCreateInfo vertexInput = {
	VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, NULL,
	0, 1, &binding, 4, attributes };

static const VkPipelineInputAssemblyStateCreateInfo assembly {
	VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, NULL,
	0, VK_PRIMITIVE_TOPOLOGY_POINT_LIST, VK_FALSE };

static const VkPipelineViewportStateCreateInfo viewportTemplate = {
	VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, NULL,
	0, 0, NULL, 0, NULL };

static const VkPipelineRasterizationStateCreateInfo rasterization = {
	VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, NULL,
	0,
	VK_FALSE,
	VK_FALSE,
	VK_POLYGON_MODE_FILL,
	VK_CULL_MODE_BACK_BIT,
	VK_FRONT_FACE_COUNTER_CLOCKWISE,
	VK_FALSE, 0.0f, 0.0f, 0.0f, 1.0f };

static const VkPipelineMultisampleStateCreateInfo multisample = {
	VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, NULL,
	0, VK_SAMPLE_COUNT_1_BIT, VK_FALSE, 0.0f, NULL, VK_FALSE, VK_FALSE };

static const VkPipelineDepthStencilStateCreateInfo depthStencil = {
	VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, NULL,
	0, VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS,
	0, 0, {}, {}, 0, 0 };

static const VkPipelineColorBlendAttachmentState blendAttachment {
	VK_FALSE,
	VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
	VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
	VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT };

static const VkPipelineColorBlendStateCreateInfo blend = {
	VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, NULL,
	0, VK_FALSE, VK_LOGIC_OP_CLEAR, 1, &blendAttachment, {} };

static const VkDynamicState dynamicStateData[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
static const VkDynamicState dynamicStateWithCountData[2] = {
	VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT_EXT,
	VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT_EXT };

static const VkPipelineDynamicStateCreateInfo dynamicState = {
	VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, NULL,
	0, 2, dynamicStateData };

static const VkPipelineDynamicStateCreateInfo dynamicStateWithCount = {
	VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, NULL,
	0, 2, dynamicStateWithCountData };

// Fill in the given graphics pipeline state. The caller needs to
// provide space to store the generated
// VkPipelineViewportStateCreateInfo, and provide the render pass,
// pipeline layout, shader modules, and the viewport/scissor count (0
// to use VK_DYNAMIC_STATE_VIEWPORT/SCISSOR_WITH_COUNT_EXT)
static void fill(VkRenderPass                       renderPass,
				 VkPipelineLayout                   layout,
				 deUint32                           staticViewportScissorCount,
				 deUint32                           stageCount,
				 const VkPipelineShaderStageCreateInfo* pStages,
				 VkGraphicsPipelineCreateInfo*      outCreateInfo,
				 VkPipelineViewportStateCreateInfo* outViewportState)
{
	outCreateInfo->sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	outCreateInfo->pNext = NULL;
	outCreateInfo->flags = 0;
	outCreateInfo->stageCount = stageCount;
	outCreateInfo->pStages = pStages;
	outCreateInfo->pVertexInputState = &vertexInput;
	outCreateInfo->pInputAssemblyState = &assembly;
	outCreateInfo->pTessellationState = NULL;
	outCreateInfo->pViewportState = outViewportState;
	outCreateInfo->pRasterizationState = &rasterization;
	outCreateInfo->pMultisampleState = &multisample;
	outCreateInfo->pDepthStencilState = &depthStencil;
	outCreateInfo->pColorBlendState = &blend;
	outCreateInfo->pDynamicState = staticViewportScissorCount == 0 ? &dynamicStateWithCount : &dynamicState;
	outCreateInfo->layout = layout;
	outCreateInfo->renderPass = renderPass;
	outCreateInfo->subpass = 0;
	outCreateInfo->basePipelineHandle = 0;
	outCreateInfo->basePipelineIndex = 0;

	VkPipelineViewportStateCreateInfo viewportState = viewportTemplate;
	viewportState.viewportCount = staticViewportScissorCount;
	viewportState.scissorCount  = staticViewportScissorCount;
	*outViewportState = viewportState;
}

} // end namespace pipelinestate


const VkBufferCreateInfo rectangleBufferInfo = {
	VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0,
	kMaxRectangles * sizeof(Rectangle),
	VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, NULL };

const VkBufferCreateInfo downloadBufferInfo = {
	VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0,
	kWidth * kHeight * sizeof(Texel),
	VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, NULL };

const VkImageCreateInfo colorImageInfo = {
	VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, NULL, 0,
	VK_IMAGE_TYPE_2D,
	kFormat,
	{ kWidth, kHeight, 1 },
	1, 1, VK_SAMPLE_COUNT_1_BIT,
	VK_IMAGE_TILING_OPTIMAL,
	VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
	VK_SHARING_MODE_EXCLUSIVE, 0, NULL,
	VK_IMAGE_LAYOUT_UNDEFINED };

VkImageCreateInfo makeDepthImageInfo(Context& context)
{
	VkImageCreateInfo info = {
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, NULL, 0,
		VK_IMAGE_TYPE_2D,
		VK_FORMAT_UNDEFINED, // To be filled in.
		{ kWidth, kHeight, 1 },
		1, 1, VK_SAMPLE_COUNT_1_BIT,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_SHARING_MODE_EXCLUSIVE, 0, NULL,
		VK_IMAGE_LAYOUT_UNDEFINED
	};

	VkFormat depthFormats[4] = {
		VK_FORMAT_X8_D24_UNORM_PACK32, VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT};
	for (int i = 0; i < 4; ++i)
	{
		VkFormatProperties properties;
		context.getInstanceInterface().getPhysicalDeviceFormatProperties(
			context.getPhysicalDevice(), depthFormats[i], &properties);
		if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
		{
			info.format = depthFormats[i];
			return info;
		}
	}
	throw std::runtime_error("Did not find suitable depth attachment format.");
}



// Initialize the Vulkan state for the tests.
InheritanceTestInstance::InheritanceTestInstance(Context& context, InheritanceMode inheritanceMode)
	: TestInstance(context)
	, m_in(context.getInstanceInterface())
	, m_vk(context.getDeviceInterface())
	, m_inheritanceMode(inheritanceMode)
	, m_rectangleBuffer(m_vk, m_context.getDevice(), m_context.getDefaultAllocator(), rectangleBufferInfo,
						MemoryRequirement::HostVisible | MemoryRequirement::Coherent)
	, m_downloadBuffer(m_vk, m_context.getDevice(), m_context.getDefaultAllocator(), downloadBufferInfo,
						MemoryRequirement::HostVisible | MemoryRequirement::Coherent)
	, m_depthImageInfo(makeDepthImageInfo(context))
	, m_colorImage(m_vk, m_context.getDevice(), m_context.getDefaultAllocator(), colorImageInfo, MemoryRequirement::Local)
	, m_depthImage(m_vk, m_context.getDevice(), m_context.getDefaultAllocator(), m_depthImageInfo, MemoryRequirement::Local)
	, m_colorViewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL, 0, m_colorImage.get(), VK_IMAGE_VIEW_TYPE_2D,
					   kFormat, {}, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } }
	, m_depthViewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL, 0, m_depthImage.get(), VK_IMAGE_VIEW_TYPE_2D,
					   m_depthImageInfo.format, {}, { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 } }
	, m_colorView(createImageView(m_vk, m_context.getDevice(), &m_colorViewInfo, NULL))
	, m_depthView(createImageView(m_vk, m_context.getDevice(), &m_depthViewInfo, NULL))
{
	VkDevice dev = m_context.getDevice();

	// Render pass, adapted from Alexander Overvoorde's
	// vulkan-tutorial.com (CC0 1.0 Universal)
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = kFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription depthAttachment{};
	depthAttachment.format = m_depthImageInfo.format;
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef{};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;
	subpass.pDepthStencilAttachment = &depthAttachmentRef;

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkAttachmentDescription attachments[2] = {colorAttachment, depthAttachment};
	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 2;
	renderPassInfo.pAttachments = attachments;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	m_renderPass = createRenderPass(m_vk, dev, &renderPassInfo, NULL);

	// Set up framebuffer
	VkImageView attachmentViews[2] = { m_colorView.get(), m_depthView.get() };
	VkFramebufferCreateInfo framebufferInfo {
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		NULL,
		0,
		m_renderPass.get(),
		2, attachmentViews,
		kWidth, kHeight, 1 };
	m_framebuffer = createFramebuffer(m_vk, dev, &framebufferInfo, NULL);

	// Compile graphics pipeline stages.
	m_vertModule = vk::createShaderModule(m_vk, dev, m_context.getBinaryCollection().get("vert"), 0u);
	m_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	m_stages[0].pNext = NULL;
	m_stages[0].flags = 0;
	m_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	m_stages[0].module = m_vertModule.get();
	m_stages[0].pName = "main";
	m_stages[0].pSpecializationInfo = NULL;

	m_geomModule = vk::createShaderModule(m_vk, dev, m_context.getBinaryCollection().get("geom"), 0u);
	m_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	m_stages[1].pNext = NULL;
	m_stages[1].flags = 0;
	m_stages[1].stage = VK_SHADER_STAGE_GEOMETRY_BIT;
	m_stages[1].module = m_geomModule.get();
	m_stages[1].pName = "main";
	m_stages[1].pSpecializationInfo = NULL;

	m_fragModule = vk::createShaderModule(m_vk, dev, m_context.getBinaryCollection().get("frag"), 0u);
	m_stages[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	m_stages[2].pNext = NULL;
	m_stages[2].flags = 0;
	m_stages[2].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	m_stages[2].module = m_fragModule.get();
	m_stages[2].pName = "main";
	m_stages[2].pSpecializationInfo = NULL;

	// Set up pipeline layout (empty)
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{ };
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	m_rectanglePipelineLayout = createPipelineLayout(m_vk, dev, &pipelineLayoutInfo, NULL);
	// Graphics pipelines are created on-the-fly later.

	// Command pool and command buffers.
	VkCommandPoolCreateInfo poolInfo {
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		NULL,
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		m_context.getUniversalQueueFamilyIndex() };
	m_cmdPool = createCommandPool(m_vk, dev, &poolInfo, NULL);

	VkCommandBufferAllocateInfo cmdBufferInfo {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, NULL,
		m_cmdPool.get(),
		VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 };
	VK_CHECK(m_vk.allocateCommandBuffers(dev, &cmdBufferInfo, &m_primaryCmdBuffer));
	cmdBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
	VK_CHECK(m_vk.allocateCommandBuffers(dev, &cmdBufferInfo, &m_setStateCmdBuffer));
	VK_CHECK(m_vk.allocateCommandBuffers(dev, &cmdBufferInfo, &m_subpassCmdBuffer));
}


static deUint8 u8_from_unorm(float x)
{
	return deUint8(roundf(clamp(x, 0.0f, 1.0f) * 255.0f));
}


// Start work (on the univeral queue) for filling m_downloadBuffer with the image
// resulting from rendering the test case. Must vkQueueWaitIdle before
// accessing the data, or calling this function again.
void InheritanceTestInstance::startRenderCmds(const TestGeometry& geometry)
{
	DE_ASSERT(geometry.viewports.size() > 0);
	DE_ASSERT(geometry.viewports.size() <= kMaxViewports);
	DE_ASSERT(geometry.viewports.size() == geometry.scissors.size());

	// Fill vertex buffer
	DE_ASSERT(kMaxRectangles >= geometry.rectangles.size());
	Rectangle* pRectangles = static_cast<Rectangle*>(m_rectangleBuffer.getAllocation().getHostPtr());
	for (size_t i = 0; i < geometry.rectangles.size(); ++i)
	{
		pRectangles[i] = geometry.rectangles[i];
	}

	VkCommandBufferInheritanceInfo inheritanceInfo {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
		NULL,
		m_renderPass.get(),
		0,
		m_framebuffer.get(),
		0, 0, 0 };

	VkCommandBufferBeginInfo cmdBeginInfo {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		NULL,
		VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
		&inheritanceInfo };

	// ************************************************************************
	// Record state-setting secondary command buffer.
	// ************************************************************************
	VK_CHECK(m_vk.beginCommandBuffer(m_setStateCmdBuffer, &cmdBeginInfo));
	switch (m_inheritanceMode)
	{
	case kInheritanceDisabled:
	case kInheritFromPrimary:
	case kInheritFromPrimaryWithCount:
		break;
	case kInheritFromSecondary:
		// Set all viewport/scissor state.
		m_vk.cmdSetViewport(m_setStateCmdBuffer, 0, deUint32(geometry.viewports.size()), &geometry.viewports[0]);
		m_vk.cmdSetScissor(m_setStateCmdBuffer, 0, deUint32(geometry.scissors.size()), &geometry.scissors[0]);
		break;
	case kSplitInheritance:
		// Set just the first viewport / scissor, rest are set in
		// primary command buffer. Checks that extension properly
		// muxes state from different sources.
		m_vk.cmdSetViewport(m_setStateCmdBuffer, 0, 1, &geometry.viewports[0]);
		m_vk.cmdSetScissor(m_setStateCmdBuffer, 0, 1, &geometry.scissors[0]);
		break;
	case kInheritFromSecondaryWithCount:
		m_vk.cmdSetViewportWithCount(m_setStateCmdBuffer,
									 deUint32(geometry.viewports.size()),
									 &geometry.viewports[0]);
		m_vk.cmdSetScissorWithCount(m_setStateCmdBuffer,
									deUint32(geometry.scissors.size()),
									&geometry.scissors[0]);
		break;
	}
	VK_CHECK(m_vk.endCommandBuffer(m_setStateCmdBuffer));

	// ************************************************************************
	// Record subpass command buffer, bind vertex buffer and pipeline,
	// then draw rectangles.
	// ************************************************************************
	if (m_inheritanceMode != kInheritanceDisabled)
	{
		// Enable viewport/scissor inheritance struct.
		VkCommandBufferInheritanceViewportScissorInfoNV inheritViewportInfo {
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_VIEWPORT_SCISSOR_INFO_NV,
			NULL,
			VK_TRUE,
			deUint32(geometry.viewports.size()), &geometry.viewports[0] };
		inheritanceInfo.pNext = &inheritViewportInfo;
		VK_CHECK(m_vk.beginCommandBuffer(m_subpassCmdBuffer, &cmdBeginInfo));
		inheritanceInfo.pNext = NULL;
	}
	else
	{
		VK_CHECK(m_vk.beginCommandBuffer(m_subpassCmdBuffer, &cmdBeginInfo));
	}
	// Set viewport/scissor state only when not inherited.
	if (m_inheritanceMode == kInheritanceDisabled)
	{
		m_vk.cmdSetViewport(m_subpassCmdBuffer, 0, deUint32(geometry.viewports.size()), &geometry.viewports[0]);
		m_vk.cmdSetScissor(m_subpassCmdBuffer, 0, deUint32(geometry.scissors.size()), &geometry.scissors[0]);
	}
	// Get the graphics pipeline, creating it if needed (encountered
	// new static viewport/scissor count). 0 = dynamic count.
	deUint32 staticViewportCount = 0;
	switch (m_inheritanceMode)
	{
	case kInheritanceDisabled:
	case kInheritFromPrimary:
	case kInheritFromSecondary:
	case kSplitInheritance:
		staticViewportCount = deUint32(geometry.viewports.size());
		break;
	case kInheritFromPrimaryWithCount:
	case kInheritFromSecondaryWithCount:
		staticViewportCount = 0;
		break;
	}
	VkPipeline graphicsPipeline = m_rectanglePipelines[staticViewportCount].get();
	if (!graphicsPipeline)
	{
		VkGraphicsPipelineCreateInfo pipelineInfo;
		VkPipelineViewportStateCreateInfo viewportInfo;
		pipelinestate::fill(
			m_renderPass.get(), m_rectanglePipelineLayout.get(), staticViewportCount,
			3, m_stages,
			&pipelineInfo, &viewportInfo);
		m_rectanglePipelines[staticViewportCount] = createGraphicsPipeline(m_vk, m_context.getDevice(), 0, &pipelineInfo, NULL);
		graphicsPipeline = m_rectanglePipelines[staticViewportCount].get();
	}
	m_vk.cmdBindPipeline(m_subpassCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

	// Bind vertex buffer and draw.
	VkDeviceSize offset = 0;
	VkBuffer     vertexBuffer = m_rectangleBuffer.get();
	m_vk.cmdBindVertexBuffers(m_subpassCmdBuffer, 0, 1, &vertexBuffer, &offset);
	m_vk.cmdDraw(m_subpassCmdBuffer, deUint32(geometry.rectangles.size()), 1, 0, 0);
	VK_CHECK(m_vk.endCommandBuffer(m_subpassCmdBuffer));

	// ************************************************************************
	// Primary command buffer commands, start render pass and execute
	// the secondary command buffers, then copy rendered image to
	// download buffer.
	// ************************************************************************
	VkCommandBufferBeginInfo beginInfo {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		NULL,
		0, NULL };
	VK_CHECK(m_vk.beginCommandBuffer(m_primaryCmdBuffer, &beginInfo));

	VkClearValue clearValues[2];
	clearValues[0].color.float32[0] = geometry.clearColor.x();
	clearValues[0].color.float32[1] = geometry.clearColor.y();
	clearValues[0].color.float32[2] = geometry.clearColor.z();
	clearValues[0].color.float32[3] = 1.0f;
	clearValues[1].depthStencil = { geometry.clearDepth, 0 };

	VkRenderPassBeginInfo renderPassBeginInfo {
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		NULL,
		m_renderPass.get(),
		m_framebuffer.get(),
		{ { 0, 0 }, { kWidth, kHeight } },
		2, clearValues };

	switch (m_inheritanceMode)
	{
	case kInheritFromPrimary:
		// Specify all viewport/scissor state only when we expect to.
		// inherit ALL viewport/scissor state from primary command buffer.
		m_vk.cmdSetViewport(m_primaryCmdBuffer, 0, deUint32(geometry.viewports.size()), &geometry.viewports[0]);
		m_vk.cmdSetScissor(m_primaryCmdBuffer, 0, deUint32(geometry.scissors.size()), &geometry.scissors[0]);
		break;
	case kInheritFromPrimaryWithCount:
		// Same but with count inherited.
		m_vk.cmdSetViewportWithCount(m_primaryCmdBuffer,
									 deUint32(geometry.viewports.size()),
									 &geometry.viewports[0]);
		m_vk.cmdSetScissorWithCount(m_primaryCmdBuffer,
									deUint32(geometry.scissors.size()),
									&geometry.scissors[0]);
		break;
	case kSplitInheritance:
		// Specify the remaining viewport, scissors not set by the
		// setStateCmdBuffer in this test mode.
		if (geometry.viewports.size() > 1)
		{
			m_vk.cmdSetViewport(m_primaryCmdBuffer, 1, deUint32(geometry.viewports.size() - 1), &geometry.viewports[1]);
			m_vk.cmdSetScissor(m_primaryCmdBuffer, 1, deUint32(geometry.scissors.size() - 1), &geometry.scissors[1]);
		}
		/* FALLTHROUGH */
	case kInheritanceDisabled:
	case kInheritFromSecondary:
	case kInheritFromSecondaryWithCount:
		// Specify some bogus state, ensure correctly overwritten later.
		VkViewport bogusViewport { 0.f, 0.f, 8.f, 8.f, 0.f, 0.1f };
		VkRect2D   bogusScissors { { 2, 0 }, { 100, 100 }};
		m_vk.cmdSetViewport(m_primaryCmdBuffer, 0, 1, &bogusViewport);
		m_vk.cmdSetScissor(m_primaryCmdBuffer, 0, 1, &bogusScissors);
		break;
	}

	m_vk.cmdBeginRenderPass(m_primaryCmdBuffer, &renderPassBeginInfo,
		VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
	VkCommandBuffer secondaryCmdBuffers[2] = {m_setStateCmdBuffer,
											  m_subpassCmdBuffer};
	m_vk.cmdExecuteCommands(m_primaryCmdBuffer, 2, secondaryCmdBuffers);
	m_vk.cmdEndRenderPass(m_primaryCmdBuffer);

	// Barrier, then copy rendered image to download buffer.
	VkImageMemoryBarrier imageBarrier {
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		NULL,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_ACCESS_TRANSFER_READ_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		0, 0,
		m_colorImage.get(),
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }};
	m_vk.cmdPipelineBarrier(
		m_primaryCmdBuffer,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0, 0, NULL, 0, NULL, 1, &imageBarrier );
	VkBufferImageCopy bufferImageCopy {
		0, 0, 0,
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
		{ 0, 0, 0 },
		{ kWidth, kHeight, 1 } };
	m_vk.cmdCopyImageToBuffer(
		m_primaryCmdBuffer,
		m_colorImage.get(),
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		m_downloadBuffer.get(),
		1, &bufferImageCopy);

	// Barrier, make buffer visible to host.
	VkBufferMemoryBarrier bufferBarrier {
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
		NULL,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_ACCESS_HOST_READ_BIT,
		0, 0,
		m_downloadBuffer.get(),
		0, VK_WHOLE_SIZE };
	m_vk.cmdPipelineBarrier(
		m_primaryCmdBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_HOST_BIT,
		0, 0, NULL, 1, &bufferBarrier, 0, NULL);

	// End and submit primary command buffer.
	VK_CHECK(m_vk.endCommandBuffer(m_primaryCmdBuffer));
	VkSubmitInfo submitInfo {
		VK_STRUCTURE_TYPE_SUBMIT_INFO,
		NULL,
		0, NULL, NULL,
		1, &m_primaryCmdBuffer,
		0, NULL };
	m_vk.queueSubmit(m_context.getUniversalQueue(), 1, &submitInfo, 0);
}


void InheritanceTestInstance::rasterizeExpectedResults(const TestGeometry& geometry, Texel (&output)[kHeight][kWidth])
{
	// Clear color and depth buffers.
	Texel clearColorTexel{u8_from_unorm(geometry.clearColor.z()),
						  u8_from_unorm(geometry.clearColor.y()),
						  u8_from_unorm(geometry.clearColor.x()), 0u};
	for (size_t y = 0; y < kHeight; ++y)
	{
		for (size_t x = 0; x < kWidth; ++x)
		{
			m_cpuDepthBuffer[y][x] = geometry.clearDepth;
			output[y][x]           = clearColorTexel;
		}
	}

	// Rasterize each rectangle. Pixels have half-integer centers.
	for (size_t i = 0; i < geometry.rectangles.size(); ++i) {
		Rectangle r = geometry.rectangles[i];

		// Select correct viewport and scissor.
		VkViewport viewport = geometry.viewports.at(r.viewportIndex);
		VkRect2D   scissor  = geometry.scissors.at(r.viewportIndex);

		// Transform xyz and width/height with selected viewport.
		float ox = viewport.x + viewport.width * 0.5f;
		float oy = viewport.y + viewport.height * 0.5f;
		float oz = viewport.minDepth;

		float px = viewport.width;
		float py = viewport.height;
		float pz = viewport.maxDepth - viewport.minDepth;

		float xLow  = clamp(r.xyz.x(), -1.0f, 1.0f);
		float xHigh = clamp(r.xyz.x() + r.widthHeight.x(), -1.0f, 1.0f);
		float yLow  = clamp(r.xyz.y(), -1.0f, 1.0f);
		float yHigh = clamp(r.xyz.y() + r.widthHeight.y(), -1.0f, 1.0f);

		float xf[2];
		xf[0]    = px * 0.5f * xLow  + ox;
		xf[1]    = px * 0.5f * xHigh + ox;
		float yf[2];
		yf[0]    = py * 0.5f * yLow  + oy;
		yf[1]    = py * 0.5f * yHigh + oy;
		float zf = pz * r.xyz.z() + oz;

		deInt32 xBegin = deInt32(floorf(xf[0] + 0.5f));
		deInt32 xEnd   = deInt32(floorf(xf[1] + 0.5f));
		deInt32 yBegin = deInt32(floorf(yf[0] + 0.5f));
		deInt32 yEnd   = deInt32(floorf(yf[1] + 0.5f));

		// Scissor test, only correct when drawn rectangle has
		// positive width/height.
		deInt32 xsLow  = scissor.offset.x;
		deInt32 xsHigh = xsLow + deInt32(scissor.extent.width);
		xBegin         = clamp(xBegin, xsLow, xsHigh);
		xEnd           = clamp(xEnd,   xsLow, xsHigh);
		deInt32 ysLow  = scissor.offset.y;
		deInt32 ysHigh = ysLow + deInt32(scissor.extent.height);
		yBegin         = clamp(yBegin, ysLow, ysHigh);
		yEnd           = clamp(yEnd,   ysLow, ysHigh);

		// Clamp to framebuffer size
		xBegin = clamp(xBegin, 0, kWidth);
		xEnd   = clamp(xEnd,   0, kWidth);
		yBegin = clamp(yBegin, 0, kHeight);
		yEnd   = clamp(yEnd,   0, kHeight);

		// Rasterize.
		Texel rectTexel = texelFrom_r8g8b8(r.r8g8b8);
		for (deInt32 x = xBegin; x < xEnd; ++x)
		{
			for (deInt32 y = yBegin; y < yEnd; ++y)
			{
				// Depth test
				float oldDepth = m_cpuDepthBuffer[y][x];
				if (!(zf < oldDepth)) continue;

				output[y][x]           = rectTexel;
				m_cpuDepthBuffer[y][x] = zf;
			}
		}
	}
}


std::vector<TestGeometry> makeGeometry()
{
	std::vector<TestGeometry> cases;

	TestGeometry geometry;
	geometry.clearColor = Vec3(1.0f, 1.0f, 1.0f);
	geometry.clearDepth = 1.0f;

	// Simple test case, three squares, the last one should go in
	// between the first two in depth due to viewport 1 halving the
	// actual depth value.
	geometry.rectangles.push_back(Rectangle{
		Vec3(-0.5f, -1.0f, 0.2f),
		0xFF0000,
		Vec2(0.5f, 1.0f),
		0 });
	geometry.rectangles.push_back(Rectangle{
		Vec3(0.0f, 0.0f, 0.6f),
		0x0000FF,
		Vec2(0.5f, 1.0f),
		0 });
	geometry.rectangles.push_back(Rectangle{
		Vec3(-0.25f, -0.5f, 0.8f), // becomes 0.4f depth
		0x008000,
		Vec2(0.5f, 1.0f),
		1 });
	geometry.viewports.push_back({0, 0, kWidth, kHeight, 0.0f, 1.0f});
	geometry.viewports.push_back({0, 0, kWidth, kHeight, 0.0f, 0.5f});
	geometry.scissors.push_back({{0, 0}, {kWidth, kHeight}});
	geometry.scissors.push_back({{0, 0}, {kWidth, kHeight}});

	cases.push_back(geometry);

	// Apply scissor rectangle to red and blue squares.
	geometry.scissors[0].extent.width = kWidth / 2 + 1;
	cases.push_back(geometry);

	// Squash down and offset green rectangle's viewport.
	geometry.viewports[1].y      = kHeight * 0.25f;
	geometry.viewports[1].height = kHeight * 0.75f;
	cases.push_back(geometry);

	// Add another viewport and scissor.
	geometry.viewports.push_back(
		{kWidth / 2 - 4, 0, kWidth / 2, kHeight - 8, 0.5f, 1.0f});
	geometry.scissors.push_back(
		{{kWidth / 2 - 2, 10}, {kWidth / 2, kHeight}});
	geometry.rectangles.push_back(Rectangle{
		Vec3(-1.0f, -1.0f, 0.5f), // Becomes 0.75f depth
		0x000000,
		Vec2(1.75f, 1.75f),
		2 });
	cases.push_back(geometry);

	// Add a few more rectangles.
	geometry.rectangles.push_back(Rectangle{
		Vec3(-0.25f, -0.25f, 0.1f),
		0xFF00FF,
		Vec2(0.375f, 0.375f),
		0 });
	geometry.rectangles.push_back(Rectangle{
		Vec3(-1.0f, -1.0f, 0.8f), // Becomes 0.9f depth
		0x00FFFF,
		Vec2(2.0f, 2.0f),
		2 });
	geometry.rectangles.push_back(Rectangle{
		Vec3(-1.0f, -1.0f, 0.7f),
		0x808000,
		Vec2(2.0f, 2.0f),
		0 });
	cases.push_back(geometry);

	// Change clear depth and color.
	geometry.clearDepth = 0.85f;
	geometry.clearColor = Vec3(1.0f, 1.0f, 0.0f);
	cases.push_back(geometry);

	// Alter viewport/scissor 2.
	geometry.viewports[2] = VkViewport{ 0, 0, kWidth, kHeight, 0.51f, 0.53f };
	geometry.scissors[2]  = VkRect2D{ { 20, 0 }, { kWidth, kHeight } };
	cases.push_back(geometry);

	// Change clear depth and color again.
	geometry.clearDepth = 0.5f;
	geometry.clearColor = Vec3(0.0f, 1.0f, 0.0f);
	cases.push_back(geometry);

	return cases;
}


tcu::TestStatus InheritanceTestInstance::iterate(void)
{
	std::vector<TestGeometry> testGeometries = makeGeometry();
	deUint32 failBits = 0;
	DE_ASSERT(testGeometries.size() < 32);

	for (size_t i = 0; i != testGeometries.size(); ++i)
	{
		const TestGeometry& geometry = testGeometries[i];
		TestResults results;

		// Start drawing commands.
		startRenderCmds(geometry);

		// Work on CPU-side expected results while waiting for device.
		rasterizeExpectedResults(geometry, results.expectedResult);

		// Wait for commands to finish and copy back results.
		m_vk.queueWaitIdle(m_context.getUniversalQueue());
		memcpy(results.deviceResult,
			   m_downloadBuffer.getAllocation().getHostPtr(),
			   kWidth * kHeight * sizeof(Texel));

		// Compare results. The test cases should be simple enough not to
		// require fuzzy matching (power of 2 framebuffer, no nearby depth
		// values, etc.)
		bool passed = true;
		for (size_t y = 0; y < kHeight; ++y)
		{
			for (size_t x = 0; x < kWidth; ++x)
			{
				passed &= results.expectedResult[y][x].red   == results.deviceResult[y][x].red;
				passed &= results.expectedResult[y][x].green == results.deviceResult[y][x].green;
				passed &= results.expectedResult[y][x].blue  == results.deviceResult[y][x].blue;
			}
		}
		results.passed = passed; // Log results?

		failBits |= deUint32(!passed) << i;
	}

	if (failBits != 0)
	{
		std::stringstream stream;
		stream << "Failed for test geometry";
		for (int i = 0; i < 32; ++i)
		{
			if (1 & (failBits >> i))
			{
				stream << ' ' << i;
			}
		}
		return tcu::TestStatus::fail(stream.str());
	}
	else
	{
		return tcu::TestStatus::pass("pass");
	}
}


class InheritanceTestCase : public TestCase
{
public:
	InheritanceTestCase (tcu::TestContext& testCtx, InheritanceMode inheritanceMode,
						 const char* name, const char* description)
		: TestCase(testCtx, name, description), m_inheritanceMode(inheritanceMode)
	{

	}

	TestInstance* createInstance (Context& context) const
	{
		return new InheritanceTestInstance(context, m_inheritanceMode);
	}

	virtual void checkSupport (Context& context) const
	{
		context.requireDeviceFunctionality("VK_NV_inherited_viewport_scissor");
		if (m_inheritanceMode == kInheritFromPrimaryWithCount || m_inheritanceMode == kInheritFromSecondaryWithCount)
		{
			context.requireDeviceFunctionality("VK_EXT_extended_dynamic_state");
		}
	}

	virtual void initPrograms (vk::SourceCollections& programCollection) const
	{
		programCollection.glslSources.add("vert") << glu::VertexSource  (pipelinestate::vert_glsl);
		programCollection.glslSources.add("geom") << glu::GeometrySource(pipelinestate::geom_glsl);
		programCollection.glslSources.add("frag") << glu::FragmentSource(pipelinestate::frag_glsl);
	}
private:
	InheritanceMode        m_inheritanceMode;
};

} // anonymous namespace


DynamicStateInheritanceTests::DynamicStateInheritanceTests (tcu::TestContext& testCtx)
	: TestCaseGroup(testCtx, "inheritance", "Tests for inherited viewport/scissor state")
{

}

void DynamicStateInheritanceTests::init (void)
{
	addChild(new InheritanceTestCase(m_testCtx, kInheritanceDisabled, "baseline",
			 "Baseline, no viewport/scissor inheritance"));
	addChild(new InheritanceTestCase(m_testCtx, kInheritFromPrimary, "primary",
			 "Inherit viewport/scissor from calling primary command buffer"));
	addChild(new InheritanceTestCase(m_testCtx, kInheritFromSecondary, "secondary",
			 "Inherit viewport/scissor from another secondary command buffer"));
	addChild(new InheritanceTestCase(m_testCtx, kSplitInheritance, "split",
			 "Inherit some viewports/scissors from primary, some from secondary"));
	addChild(new InheritanceTestCase(m_testCtx, kInheritFromPrimaryWithCount, "primary_with_count",
			 "Inherit viewport/scissor with count from calling primary command buffer"));
	addChild(new InheritanceTestCase(m_testCtx, kInheritFromSecondaryWithCount, "secondary_with_count",
			 "Inherit viewport/scissor with count from another secondary command buffer"));
}

} // DynamicState
} // vkt
