/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2023 LunarG, Inc.
 * Copyright (c) 2023 Nintendo
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
 * \brief Shader Object Binding Tests
 *//*--------------------------------------------------------------------*/

#include "vktShaderObjectBindingTests.hpp"
#include "deUniquePtr.hpp"
#include "tcuTestCase.hpp"
#include "vktTestCase.hpp"
#include "vkShaderObjectUtil.hpp"
#include "vktShaderObjectCreateUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkObjUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "deMath.h"
#include "vktCustomInstancesDevices.hpp"
#include "tcuCommandLine.hpp"
#include "vkBuilderUtil.hpp"

namespace vkt
{
namespace ShaderObject
{

namespace
{

enum TestType {
	PASSTHROUGH_GEOM,
	SWAP,
	DISABLED,
	UNBIND,
	DRAW_DISPATCH_DRAW,
	DISPATCH_DRAW_DISPATCH,
};

struct BindingDrawParams {
	TestType testType;
	vk::VkShaderStageFlagBits stage;
	vk::VkShaderStageFlagBits unusedOutputs;
	vk::VkShaderStageFlagBits binaryStage;
	bool bindUnsupported;
	bool setStateAfter;
	bool unbindWithNullpShaders;
};

struct MeshBindingDrawParams {
	vk::VkShaderStageFlagBits stage;
};

struct BindingParams {
	bool useMeshShaders;
};

class ShaderObjectBindingDrawInstance : public vkt::TestInstance
{
public:
								ShaderObjectBindingDrawInstance		(Context& context, const BindingDrawParams& params)
																	: vkt::TestInstance		(context)
																	, m_params				(params)
																	{}
	virtual						~ShaderObjectBindingDrawInstance	(void) {}

	tcu::TestStatus				iterate								(void) override;
private:
	vk::Move<vk::VkShaderEXT>	createShader						(const vk::DeviceInterface& vk, const vk::VkDevice device, vk::VkShaderStageFlagBits stage, const std::string& name, const vk::VkDescriptorSetLayout* descriptorSetLayout = DE_NULL);
	void						createDevice						(void);
	vk::VkDevice				getDevice							(void) { return (m_params.testType == DISABLED) ? m_customDevice.get() : m_context.getDevice(); }
	void						setDynamicStates					(vk::VkCommandBuffer cmdBuffer, bool tessShader);

	BindingDrawParams				m_params;
	vk::Move<vk::VkDevice>			m_customDevice;
};

void ShaderObjectBindingDrawInstance::createDevice (void)
{
	if (m_params.testType != DISABLED)
		return;

	const float										queuePriority		= 1.0f;
	const auto&										deviceExtensions	= m_context.getDeviceCreationExtensions();
	auto											features2			= m_context.getDeviceFeatures2();

	if (m_params.stage == vk::VK_SHADER_STAGE_GEOMETRY_BIT)
		features2.features.geometryShader = VK_FALSE;
	else if (m_params.stage == vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
		features2.features.tessellationShader = VK_FALSE;

	vk::VkDeviceQueueCreateInfo						queueInfo =
	{
		vk::VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,					// VkStructureType					sType;
		DE_NULL,													// const void*						pNext;
		0u,															// VkDeviceQueueCreateFlags			flags;
		0u,															// deUint32							queueFamilyIndex;
		1u,															// deUint32							queueCount;
		&queuePriority												// const float*						pQueuePriorities;
	};

	const vk::VkDeviceCreateInfo					deviceInfo =
	{
		vk::VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,						// VkStructureType					sType;
		&features2,														// const void*						pNext;
		(vk::VkDeviceCreateFlags)0,										// VkDeviceCreateFlags				flags;
		1u,																// uint32_t							queueCreateInfoCount;
		&queueInfo,														// const VkDeviceQueueCreateInfo*	pQueueCreateInfos;
		0u,																// uint32_t							enabledLayerCount;
		DE_NULL,														// const char* const*				ppEnabledLayerNames;
		deUint32(deviceExtensions.size()),								// uint32_t							enabledExtensionCount;
		(deviceExtensions.empty()) ? DE_NULL : deviceExtensions.data(),	// const char* const*				ppEnabledExtensionNames;
		DE_NULL															// const VkPhysicalDeviceFeatures*	pEnabledFeatures;
	};

	m_customDevice = createCustomDevice(m_context.getTestContext().getCommandLine().isValidationEnabled(), m_context.getPlatformInterface(), m_context.getInstance(), m_context.getInstanceInterface(), m_context.getPhysicalDevice(), &deviceInfo);
}

vk::Move<vk::VkShaderEXT> ShaderObjectBindingDrawInstance::createShader (const vk::DeviceInterface& vk, const vk::VkDevice device, vk::VkShaderStageFlagBits stage, const std::string& name, const vk::VkDescriptorSetLayout* descriptorSetLayout)
{
	const auto&					binaries					= m_context.getBinaryCollection();
	const bool					tessellationSupported		= m_context.getDeviceFeatures().tessellationShader;
	const bool					geometrySupported			= m_context.getDeviceFeatures().geometryShader;

	if (m_params.binaryStage == stage)
	{
		auto shaderCreateInfo = vk::makeShaderCreateInfo(stage, binaries.get(name), tessellationSupported, geometrySupported, descriptorSetLayout);
		const auto shader = vk::createShader(vk, device, shaderCreateInfo);

		size_t dataSize;
		vk.getShaderBinaryDataEXT(device, *shader, &dataSize, DE_NULL);
		std::vector<deUint8> data(dataSize);
		vk.getShaderBinaryDataEXT(device, *shader, &dataSize, data.data());

		shaderCreateInfo.codeType = vk::VK_SHADER_CODE_TYPE_BINARY_EXT;
		shaderCreateInfo.codeSize = dataSize;
		shaderCreateInfo.pCode = data.data();

		return vk::createShader(vk, device, shaderCreateInfo);
	}

	return vk::createShader(vk, device, vk::makeShaderCreateInfo(stage, binaries.get(name), tessellationSupported, geometrySupported, descriptorSetLayout));
}

void ShaderObjectBindingDrawInstance::setDynamicStates (vk::VkCommandBuffer cmdBuffer, bool tessShader)
{
	const vk::DeviceInterface&			vk							= m_context.getDeviceInterface();
	const auto							deviceExtensions			= vk::removeUnsupportedShaderObjectExtensions(m_context.getInstanceInterface(), m_context.getPhysicalDevice(), m_context.getDeviceExtensions());

	const vk::VkPrimitiveTopology		topology					= tessShader ? vk::VK_PRIMITIVE_TOPOLOGY_PATCH_LIST : vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

	vk::setDefaultShaderObjectDynamicStates(vk, cmdBuffer, deviceExtensions, topology, false);

	vk::VkBool32 colorBlendEnable = VK_TRUE;
	vk.cmdSetColorBlendEnableEXT(cmdBuffer, 0u, 1u, &colorBlendEnable);
	vk::VkColorBlendEquationEXT		colorBlendEquation = {
		vk::VK_BLEND_FACTOR_ONE,				// VkBlendFactor	srcColorBlendFactor;
		vk::VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,// VkBlendFactor	dstColorBlendFactor;
		vk::VK_BLEND_OP_ADD,					// VkBlendOp		colorBlendOp;
		vk::VK_BLEND_FACTOR_ONE,				// VkBlendFactor	srcAlphaBlendFactor;
		vk::VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,// VkBlendFactor	dstAlphaBlendFactor;
		vk::VK_BLEND_OP_ADD,					// VkBlendOp		alphaBlendOp;
	};
	vk.cmdSetColorBlendEquationEXT(cmdBuffer, 0u, 1u, &colorBlendEquation);
}

tcu::TestStatus ShaderObjectBindingDrawInstance::iterate (void)
{
	const vk::VkInstance				instance					= m_context.getInstance();
	const vk::InstanceDriver			instanceDriver				(m_context.getPlatformInterface(), instance);
	createDevice();
	const vk::DeviceInterface&			vk							= m_context.getDeviceInterface();
	const vk::VkDevice					device						= getDevice();
	const deUint32						queueFamilyIndex			= m_context.getUniversalQueueFamilyIndex();
	const vk::VkQueue					queue						= getDeviceQueue(m_context.getDeviceInterface(), device, queueFamilyIndex, 0u);
	auto								alloctor					= de::MovePtr<vk::Allocator>(new vk::SimpleAllocator(vk, device, getPhysicalDeviceMemoryProperties(instanceDriver, m_context.getPhysicalDevice())));
	auto&								alloc						= *alloctor;
	tcu::TestLog&						log							= m_context.getTestContext().getLog();

	const bool							tessellationSupported		= m_context.getDeviceFeatures().tessellationShader;
	const bool							geometrySupported			= m_context.getDeviceFeatures().geometryShader;
	const bool							taskSupported				= m_context.getMeshShaderFeatures().taskShader;
	const bool							meshSupported				= m_context.getMeshShaderFeatures().meshShader;

	vk::VkFormat						colorAttachmentFormat		= vk::VK_FORMAT_R8G8B8A8_UNORM;
	const auto							subresourceRange			= makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const auto							subresourceLayers			= vk::makeImageSubresourceLayers(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
	const vk::VkRect2D					renderArea					= vk::makeRect2D(0, 0, 32, 32);
	vk::VkExtent3D						extent						= { renderArea.extent.width, renderArea.extent.height, 1};

	const vk::VkImageCreateInfo	createInfo =
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType
		DE_NULL,									// const void*				pNext
		0u,											// VkImageCreateFlags		flags
		vk::VK_IMAGE_TYPE_2D,						// VkImageType				imageType
		colorAttachmentFormat,						// VkFormat					format
		{ 32, 32, 1 },								// VkExtent3D				extent
		1u,											// uint32_t					mipLevels
		1u,											// uint32_t					arrayLayers
		vk::VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits	samples
		vk::VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling			tiling
		vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT,	// VkImageUsageFlags		usage
		vk::VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode
		0,											// uint32_t					queueFamilyIndexCount
		DE_NULL,									// const uint32_t*			pQueueFamilyIndices
		vk::VK_IMAGE_LAYOUT_UNDEFINED				// VkImageLayout			initialLayout
	};

	de::MovePtr<vk::ImageWithMemory>	image					= de::MovePtr<vk::ImageWithMemory>(new vk::ImageWithMemory(vk, device, alloc, createInfo, vk::MemoryRequirement::Any));
	const auto							imageView				= vk::makeImageView(vk, device, **image, vk::VK_IMAGE_VIEW_TYPE_2D, colorAttachmentFormat, subresourceRange);

	const vk::VkDeviceSize				colorOutputBufferSize	= renderArea.extent.width * renderArea.extent.height * tcu::getPixelSize(vk::mapVkFormat(colorAttachmentFormat));
	de::MovePtr<vk::BufferWithMemory>	colorOutputBuffer		= de::MovePtr<vk::BufferWithMemory>(new vk::BufferWithMemory(
		vk, device, alloc, makeBufferCreateInfo(colorOutputBufferSize, vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT), vk::MemoryRequirement::HostVisible));

	const vk::VkDeviceSize				bufferSizeBytes				= sizeof(deUint32) * 16;
	const vk::Unique<vk::VkDescriptorSetLayout> descriptorSetLayout(
		vk::DescriptorSetLayoutBuilder()
		.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT)
		.build(vk, device));

	const vk::Unique<vk::VkDescriptorPool>	descriptorPool(
		vk::DescriptorPoolBuilder()
		.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2u)
		.build(vk, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 2u));

	const vk::Move<vk::VkCommandPool>	cmdPool		(vk::createCommandPool(vk, device, 0u, queueFamilyIndex));
	const vk::Move<vk::VkCommandBuffer>	cmdBuffer	(vk::allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const vk::Unique<vk::VkDescriptorSet>	descriptorSet1	(vk::makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));
	const vk::Unique<vk::VkDescriptorSet>	descriptorSet2	(vk::makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));
	const vk::BufferWithMemory				outputBuffer1	(vk, device, alloc, vk::makeBufferCreateInfo(bufferSizeBytes, vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), vk::MemoryRequirement::HostVisible);
	const vk::BufferWithMemory				outputBuffer2	(vk, device, alloc, vk::makeBufferCreateInfo(bufferSizeBytes, vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), vk::MemoryRequirement::HostVisible);

	const auto								computePipelineLayout	= makePipelineLayout(vk, device, descriptorSetLayout.get());

	const vk::VkDescriptorBufferInfo		descriptorInfo1	= vk::makeDescriptorBufferInfo(*outputBuffer1, 0ull, bufferSizeBytes);
	const vk::VkDescriptorBufferInfo		descriptorInfo2	= vk::makeDescriptorBufferInfo(*outputBuffer2, 0ull, bufferSizeBytes);
	vk::DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet1, vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorInfo1)
		.update(vk, device);
	vk::DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet2, vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorInfo2)
		.update(vk, device);

	vk::Move<vk::VkShaderEXT>			vertShader;
	vk::Move<vk::VkShaderEXT>			tescShader;
	vk::Move<vk::VkShaderEXT>			teseShader;
	vk::Move<vk::VkShaderEXT>			geomShader;
	vk::Move<vk::VkShaderEXT>			fragShader;
	vk::Move<vk::VkShaderEXT>			compShader;
	vk::Move<vk::VkShaderEXT>			passThroughGeomShader;
	vk::Move<vk::VkShaderEXT>			vertAltShader;
	vk::Move<vk::VkShaderEXT>			tescAltShader;
	vk::Move<vk::VkShaderEXT>			teseAltShader;
	vk::Move<vk::VkShaderEXT>			geomAltShader;
	vk::Move<vk::VkShaderEXT>			fragAltShader;

	if (tessellationSupported && geometrySupported)
	{
		vertShader = createShader(vk, device, vk::VK_SHADER_STAGE_VERTEX_BIT, "vert");
		vertAltShader = createShader(vk, device, vk::VK_SHADER_STAGE_VERTEX_BIT, "vertAlt");
	}
	else if (tessellationSupported)
	{
		vertShader = createShader(vk, device, vk::VK_SHADER_STAGE_VERTEX_BIT, "vertNoGeom");
		vertAltShader = createShader(vk, device, vk::VK_SHADER_STAGE_VERTEX_BIT, "vertAltNoGeom");
	}
	else if (geometrySupported)
	{
		vertShader = createShader(vk, device, vk::VK_SHADER_STAGE_VERTEX_BIT, "vertNoTess");
		vertAltShader = createShader(vk, device, vk::VK_SHADER_STAGE_VERTEX_BIT, "vertAltNoTess");
	}
	else
	{
		vertShader = createShader(vk, device, vk::VK_SHADER_STAGE_VERTEX_BIT, "vertNoTessGeom");
		vertAltShader = createShader(vk, device, vk::VK_SHADER_STAGE_VERTEX_BIT, "vertAltNoTessGeom");
	}
	if (tessellationSupported && (m_params.stage != vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT || m_params.testType != DISABLED))
	{
		tescShader = createShader(vk, device, vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, "tesc");
		teseShader = createShader(vk, device, vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, "tese");
		tescAltShader = createShader(vk, device, vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, "tescAlt");
		teseAltShader = createShader(vk, device, vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, "teseAlt");
	}
	if (geometrySupported && (m_params.stage != vk::VK_SHADER_STAGE_GEOMETRY_BIT || m_params.testType != DISABLED))
	{
		geomShader = createShader(vk, device, vk::VK_SHADER_STAGE_GEOMETRY_BIT, "geom");
		geomAltShader = createShader(vk, device, vk::VK_SHADER_STAGE_GEOMETRY_BIT, "geomAlt");
		passThroughGeomShader = createShader(vk, device, vk::VK_SHADER_STAGE_GEOMETRY_BIT, "passThroughGeom");
	}
	fragShader					= createShader(vk, device, vk::VK_SHADER_STAGE_FRAGMENT_BIT, "blendFrag");
	compShader					= createShader(vk, device, vk::VK_SHADER_STAGE_COMPUTE_BIT, "comp", &*descriptorSetLayout);
	fragAltShader				= createShader(vk, device, vk::VK_SHADER_STAGE_FRAGMENT_BIT, "fragAlt");

	const vk::VkClearValue				clearValue = vk::makeClearValueColor({ 0.0f, 0.0f, 0.0f, 0.0f });
	vk::beginCommandBuffer(vk, *cmdBuffer);

	vk::VkImageMemoryBarrier preImageBarrier = vk::makeImageMemoryBarrier(vk::VK_ACCESS_NONE, vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, vk::VK_IMAGE_LAYOUT_UNDEFINED, vk::VK_IMAGE_LAYOUT_GENERAL, **image, subresourceRange);
	vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, (vk::VkDependencyFlags)0u, 0u, (const vk::VkMemoryBarrier*)DE_NULL, 0u, (const vk::VkBufferMemoryBarrier*)DE_NULL, 1u, &preImageBarrier);

	if (!m_params.setStateAfter)
		setDynamicStates(*cmdBuffer, tessellationSupported);

	vk::VkBool32 colorBlendEnable = VK_TRUE;
	vk.cmdSetColorBlendEnableEXT(*cmdBuffer, 0u, 1u, &colorBlendEnable);
	vk::VkColorBlendEquationEXT		colorBlendEquation = {
		vk::VK_BLEND_FACTOR_ONE,				// VkBlendFactor	srcColorBlendFactor;
		vk::VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,// VkBlendFactor	dstColorBlendFactor;
		vk::VK_BLEND_OP_ADD,					// VkBlendOp		colorBlendOp;
		vk::VK_BLEND_FACTOR_ONE,				// VkBlendFactor	srcAlphaBlendFactor;
		vk::VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,// VkBlendFactor	dstAlphaBlendFactor;
		vk::VK_BLEND_OP_ADD,					// VkBlendOp		alphaBlendOp;
	};
	vk.cmdSetColorBlendEquationEXT(*cmdBuffer, 0u, 1u, &colorBlendEquation);

	if (m_params.testType != DISPATCH_DRAW_DISPATCH)
		vk::beginRendering(vk, *cmdBuffer, *imageView, renderArea, clearValue, vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_ATTACHMENT_LOAD_OP_CLEAR);

	vk::VkShaderEXT nullShader = VK_NULL_HANDLE;

	if (m_params.testType == PASSTHROUGH_GEOM)
	{
		vk::bindGraphicsShaders(vk, *cmdBuffer, *vertShader, *tescShader, *teseShader, *passThroughGeomShader, *fragShader, taskSupported, meshSupported);
		if (m_params.setStateAfter)
			setDynamicStates(*cmdBuffer, tessellationSupported);
		vk.cmdDraw(*cmdBuffer, 4, 1, 0, 0);
		vk::VkShaderStageFlagBits geomStage = vk::VK_SHADER_STAGE_GEOMETRY_BIT;
		vk.cmdBindShadersEXT(*cmdBuffer, 1u, &geomStage, &nullShader);
		vk.cmdDraw(*cmdBuffer, 4, 1, 0, 0);
	}
	else if (m_params.testType == SWAP)
	{
		vk::bindGraphicsShaders(vk, *cmdBuffer, *vertShader, *tescShader, *teseShader, *geomShader, *fragShader, taskSupported, meshSupported);
		if (m_params.setStateAfter)
			setDynamicStates(*cmdBuffer, tessellationSupported);
		vk.cmdDraw(*cmdBuffer, 4, 1, 0, 0);
		vk::VkShaderEXT shader = VK_NULL_HANDLE;
		if (m_params.stage == vk::VK_SHADER_STAGE_VERTEX_BIT)
			shader = *vertAltShader;
		else if (m_params.stage == vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
			shader = *tescAltShader;
		else if (m_params.stage == vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
			shader = *teseAltShader;
		else if (m_params.stage == vk::VK_SHADER_STAGE_GEOMETRY_BIT)
			shader = *geomAltShader;
		else if (m_params.stage == vk::VK_SHADER_STAGE_FRAGMENT_BIT)
			shader = *fragAltShader;
		vk.cmdBindShadersEXT(*cmdBuffer, 1u, &m_params.stage, &shader);
		vk.cmdDraw(*cmdBuffer, 4, 1, 0, 0);
	}
	else if (m_params.testType == DISABLED)
	{
		if (taskSupported) {
			vk::VkShaderStageFlagBits stage = vk::VK_SHADER_STAGE_TASK_BIT_EXT;
			vk::VkShaderEXT shader = VK_NULL_HANDLE;
			vk.cmdBindShadersEXT(*cmdBuffer, 1u, &stage, &shader);
		}
		if (meshSupported) {
			vk::VkShaderStageFlagBits stage = vk::VK_SHADER_STAGE_MESH_BIT_EXT;
			vk::VkShaderEXT shader = VK_NULL_HANDLE;
			vk.cmdBindShadersEXT(*cmdBuffer, 1u, &stage, &shader);
		}
		if (m_params.stage == vk::VK_SHADER_STAGE_GEOMETRY_BIT)
		{
			vk::VkShaderStageFlagBits stages[] = { vk::VK_SHADER_STAGE_VERTEX_BIT, vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, vk::VK_SHADER_STAGE_FRAGMENT_BIT };
			vk::VkShaderEXT shaders[] = { *vertShader, *tescShader, *teseShader, *fragShader };
			vk.cmdBindShadersEXT(*cmdBuffer, 4u, stages, shaders);
			if (m_params.bindUnsupported)
				vk.cmdBindShadersEXT(*cmdBuffer, 1u, &m_params.stage, &nullShader);
			if (m_params.setStateAfter)
				setDynamicStates(*cmdBuffer, tessellationSupported);
			vk.cmdDraw(*cmdBuffer, 4, 1, 0, 0);
		}
		else
		{
			vk.cmdSetPrimitiveTopology(*cmdBuffer, vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
			vk::VkShaderStageFlagBits stages[] = { vk::VK_SHADER_STAGE_VERTEX_BIT, vk::VK_SHADER_STAGE_GEOMETRY_BIT, vk::VK_SHADER_STAGE_FRAGMENT_BIT };
			vk::VkShaderEXT shaders[] = { *vertShader, *geomShader, *fragShader };
			vk.cmdBindShadersEXT(*cmdBuffer, 3u, stages, shaders);
			if (m_params.bindUnsupported)
				vk.cmdBindShadersEXT(*cmdBuffer, 1u, &m_params.stage, &nullShader);
			if (m_params.setStateAfter)
				setDynamicStates(*cmdBuffer, tessellationSupported);
			vk.cmdDraw(*cmdBuffer, 4, 1, 0, 0);
		}
	}
	else if (m_params.testType == UNBIND)
	{
		vk::bindGraphicsShaders(vk, *cmdBuffer, *vertShader, *tescShader, *teseShader, *geomShader, *fragShader, taskSupported, meshSupported);
		vk.cmdDraw(*cmdBuffer, 4, 1, 0, 0);
		if (m_params.stage == vk::VK_SHADER_STAGE_GEOMETRY_BIT)
		{
			if (m_params.unbindWithNullpShaders)
				vk.cmdBindShadersEXT(*cmdBuffer, 1u, &m_params.stage, DE_NULL);
			else
				vk.cmdBindShadersEXT(*cmdBuffer, 1u, &m_params.stage, &nullShader);
		}
		else
		{
			vk::VkShaderStageFlagBits stages[] = { vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT };
			vk::VkShaderEXT nullShaders[] = { VK_NULL_HANDLE, VK_NULL_HANDLE };
			vk.cmdSetPrimitiveTopology(*cmdBuffer, vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
			if (m_params.unbindWithNullpShaders)
				vk.cmdBindShadersEXT(*cmdBuffer, 2u, stages, DE_NULL);
			else
				vk.cmdBindShadersEXT(*cmdBuffer, 2u, stages, nullShaders);
		}
		vk.cmdDraw(*cmdBuffer, 4, 1, 0, 0);
	}
	else if (m_params.testType == DRAW_DISPATCH_DRAW)
	{
		vk::bindGraphicsShaders(vk, *cmdBuffer, *vertShader, *tescShader, *teseShader, *geomShader, *fragShader, taskSupported, meshSupported);
		if (m_params.setStateAfter)
			setDynamicStates(*cmdBuffer, tessellationSupported);
		vk.cmdDraw(*cmdBuffer, 4, 1, 0, 0);
		vk::VkShaderStageFlagBits computeStage = vk::VK_SHADER_STAGE_COMPUTE_BIT;
		vk.cmdBindShadersEXT(*cmdBuffer, 1u, &computeStage, &*compShader);
		vk.cmdDraw(*cmdBuffer, 4, 1, 0, 0);
	}
	else if (m_params.testType == DISPATCH_DRAW_DISPATCH)
	{
		vk.cmdBindDescriptorSets(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout.get(), 0, 1, &descriptorSet1.get(), 0, DE_NULL);
		vk::VkShaderStageFlagBits computeStage = vk::VK_SHADER_STAGE_COMPUTE_BIT;
		vk.cmdBindShadersEXT(*cmdBuffer, 1u, &computeStage, &*compShader);
		vk.cmdDispatch(*cmdBuffer, 1u, 1u, 1u);
		vk::bindGraphicsShaders(vk, *cmdBuffer, *vertShader, *tescShader, *teseShader, *geomShader, *fragShader, taskSupported, meshSupported);
		vk.cmdBindDescriptorSets(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout.get(), 0, 1, &descriptorSet2.get(), 0, DE_NULL);
		vk.cmdDispatch(*cmdBuffer, 1u, 1u, 1u);
	}

	if (m_params.testType != DISPATCH_DRAW_DISPATCH)
		vk::endRendering(vk, *cmdBuffer);

	vk::VkImageMemoryBarrier postImageBarrier = vk::makeImageMemoryBarrier(vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, vk::VK_ACCESS_TRANSFER_READ_BIT, vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_IMAGE_LAYOUT_GENERAL, **image, subresourceRange);
	vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, (vk::VkDependencyFlags)0u, 0u, (const vk::VkMemoryBarrier*)DE_NULL, 0u, (const vk::VkBufferMemoryBarrier*)DE_NULL, 1u, &postImageBarrier);

	const vk::VkBufferImageCopy	copyRegion = vk::makeBufferImageCopy(extent, subresourceLayers);
	vk.cmdCopyImageToBuffer(*cmdBuffer, **image, vk::VK_IMAGE_LAYOUT_GENERAL, **colorOutputBuffer, 1u, &copyRegion);

	vk::endCommandBuffer(vk, *cmdBuffer);

	vk::submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	tcu::ConstPixelBufferAccess resultBuffer = tcu::ConstPixelBufferAccess(vk::mapVkFormat(colorAttachmentFormat), renderArea.extent.width, renderArea.extent.height, 1, (const void*)colorOutputBuffer->getAllocation().getHostPtr());

	const deInt32			width			= resultBuffer.getWidth();
	const deInt32			height			= resultBuffer.getHeight();
	const float				threshold		= 1.0f / 256.0f;
	deInt32					xOffset1		= width / 8;
	deInt32					yOffset1		= height / 8;
	deInt32					xOffset2		= width / 8;
	deInt32					yOffset2		= height / 8;
	tcu::Vec4				expectedColor1	= tcu::Vec4(0.75f, 0.75f, 0.75f, 0.75f);
	tcu::Vec4				expectedColor2	= tcu::Vec4(0.5f, 0.5f, 0.5f, 0.5f);
	tcu::Vec4				blackColor		= tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f);

	if (m_params.testType == PASSTHROUGH_GEOM)
	{
		yOffset1 = height / 4;
		xOffset2 = xOffset1;
		yOffset2 = yOffset1;
	}
	else if (m_params.testType == SWAP)
	{
		if (m_params.stage == vk::VK_SHADER_STAGE_VERTEX_BIT)
		{
			xOffset2 = 0;
			yOffset2 = 0;
		}
		else if (m_params.stage == vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
		{
			xOffset2 = 10;
			yOffset2 = 10;
		}
		else if (m_params.stage == vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
		{
			xOffset2 = 12;
			yOffset2 = height / 8;
		}
		else if (m_params.stage == vk::VK_SHADER_STAGE_GEOMETRY_BIT)
		{
			xOffset2 = width / 8;
			yOffset2 = 12;
		}
		else if (m_params.stage == vk::VK_SHADER_STAGE_FRAGMENT_BIT)
		{
			expectedColor1 = tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f);
		}
	}
	else if (m_params.testType == DISABLED)
	{
		if (m_params.stage == vk::VK_SHADER_STAGE_GEOMETRY_BIT)
		{
			yOffset1 = height / 4;
			xOffset2 = 16;
			yOffset2 = 16;
		}
		else
		{
			xOffset1 = width / 4;
			xOffset2 = 16;
			yOffset2 = 16;
		}
	}
	else if (m_params.testType == UNBIND)
	{
		if (m_params.stage == vk::VK_SHADER_STAGE_GEOMETRY_BIT)
		{
			xOffset2 = xOffset1;
			yOffset2 = yOffset1 * 2;
		}
		else
		{
			xOffset2 = xOffset1 * 2;
			yOffset2 = yOffset1;
		}
	}
	else if (m_params.testType == DRAW_DISPATCH_DRAW)
	{
		xOffset2 = xOffset1;
		yOffset2 = yOffset1;
	}

	if (m_params.testType == DISPATCH_DRAW_DISPATCH)
	{
		for (deUint32 i = 0; i < 2; ++i)
		{
			const vk::Allocation& outputBufferAllocation = i == 0 ? outputBuffer1.getAllocation() : outputBuffer2.getAllocation();
			invalidateAlloc(vk, device, outputBufferAllocation);

			const deUint32* bufferPtr = static_cast<deUint32*>(outputBufferAllocation.getHostPtr());

			for (deUint32 j = 0; j < 16; ++j)
			{
				if (bufferPtr[j] != j)
					return tcu::TestStatus::fail("Fail");
			}
		}
		return tcu::TestStatus::pass("Pass");
	}

	for (deInt32 j = 0; j < height; ++j)
	{
		for (deInt32 i = 0; i < width; ++i)
		{
			const tcu::Vec4 color = resultBuffer.getPixel(i, j).asFloat();

			bool first = i >= xOffset1 && i < width - xOffset1 && j >= yOffset1 && j < height - yOffset1;
			bool second = i >= xOffset2 && i < width - xOffset2 && j >= yOffset2 && j < height - yOffset2;
			tcu::Vec4 expectedColor = blackColor;
			if (first && second)
				expectedColor = expectedColor1;
			else if (first || second)
				expectedColor = expectedColor2;

			if (deFloatAbs(color.x() - expectedColor.x()) > threshold || deFloatAbs(color.y() - expectedColor.y()) > threshold || deFloatAbs(color.z() - expectedColor.z()) > threshold || deFloatAbs(color.w() - expectedColor.w()) > threshold)
			{
				log << tcu::TestLog::Message << "Color at (" << i << ", " << j << ") is expected to be (" << expectedColor << "), but was (" << color << ")" << tcu::TestLog::EndMessage;
				return tcu::TestStatus::fail("Fail");
			}
		}
	}

	return tcu::TestStatus::pass("Pass");
}

class ShaderObjectBindingDrawCase : public vkt::TestCase
{
public:
					ShaderObjectBindingDrawCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const BindingDrawParams& params)
												: vkt::TestCase		(testCtx, name, description)
												, m_params			(params)
												{}
	virtual			~ShaderObjectBindingDrawCase	(void) {}

	void			checkSupport				(vkt::Context& context) const override;
	virtual void	initPrograms				(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance				(Context& context) const override { return new ShaderObjectBindingDrawInstance(context, m_params); }
private:
	BindingDrawParams m_params;
};

void ShaderObjectBindingDrawCase::checkSupport (Context& context) const
{
	context.requireDeviceFunctionality("VK_EXT_shader_object");

	if (m_params.stage == vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT || m_params.stage == vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT ||
		m_params.binaryStage == vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT || m_params.binaryStage == vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
		context.requireDeviceCoreFeature(vkt::DEVICE_CORE_FEATURE_TESSELLATION_SHADER);

	if (m_params.stage == vk::VK_SHADER_STAGE_GEOMETRY_BIT || m_params.binaryStage == vk::VK_SHADER_STAGE_GEOMETRY_BIT)
		context.requireDeviceCoreFeature(vkt::DEVICE_CORE_FEATURE_GEOMETRY_SHADER);
}

void ShaderObjectBindingDrawCase::initPrograms(vk::SourceCollections& programCollection) const
{
	vk::addBasicShaderObjectShaders(programCollection);

	std::stringstream passThroughGeom;
	std::stringstream blendFrag;
	std::stringstream vertAlt;
	std::stringstream geomAlt;
	std::stringstream tescAlt;
	std::stringstream teseAlt;
	std::stringstream fragAlt;
	std::stringstream vertNoTess;
	std::stringstream vertNoGeom;
	std::stringstream vertNoTessGeom;
	std::stringstream vertAltNoTess;
	std::stringstream vertAltNoGeom;
	std::stringstream vertAltNoTessGeom;

	vertNoTess
		<< "#version 450\n"
		<< "void main() {\n"
		<< "    vec2 pos = vec2(float(gl_VertexIndex & 1), float((gl_VertexIndex >> 1) & 1));\n"
		<< "    gl_Position = vec4((pos.x - 0.5f) * 1.5f, pos.y - 0.5f, 0.0f, 1.0f);\n"
		<< "}\n";

	vertNoGeom
		<< "#version 450\n"
		<< "void main() {\n"
		<< "    vec2 pos = vec2(float(gl_VertexIndex & 1), float((gl_VertexIndex >> 1) & 1));\n"
		<< "    gl_Position = vec4(pos.x - 0.5f, (pos.y - 0.5f) * 1.5f, 0.0f, 1.0f);\n"
		<< "}\n";

	vertNoTessGeom
		<< "#version 450\n"
		<< "void main() {\n"
		<< "    vec2 pos = vec2(float(gl_VertexIndex & 1), float((gl_VertexIndex >> 1) & 1));\n"
		<< "    gl_Position = vec4((pos - 0.5f) * 1.5f, 0.0f, 1.0f);\n"
		<< "}\n";

	passThroughGeom
		<< "#version 450\n"
		<< "layout(triangles) in;\n"
		<< "layout(triangle_strip, max_vertices = 4) out;\n"
		<< "\n"
		<< "void main(void)\n"
		<< "{\n"
		<< "    gl_Position = gl_in[0].gl_Position;\n"
		<< "    EmitVertex();\n"
		<< "    gl_Position = gl_in[1].gl_Position;\n"
		<< "    EmitVertex();\n"
		<< "    gl_Position = gl_in[2].gl_Position;\n"
		<< "    EmitVertex();\n"
		<< "    EndPrimitive();\n"
		<< "}\n";

	blendFrag
		<< "#version 450\n"
		<< "layout (location=0) out vec4 outColor;\n"
		<< "void main() {\n"
		<< "    outColor = vec4(0.5f, 0.5f, 0.5f, 0.5f);\n"
		<< "}\n";

	vertAlt
		<< "#version 450\n";
	if (m_params.unusedOutputs == vk::VK_SHADER_STAGE_VERTEX_BIT)
		vertAlt
			<< "layout (location = 0) out vec4 color;\n";
	vertAlt
		<< "void main() {\n"
		<< "    vec2 pos = vec2(float(gl_VertexIndex & 1), float((gl_VertexIndex >> 1) & 1));\n"
		<< "    gl_Position = vec4((pos - 0.5f) * 2, 0.0f, 1.0f);\n";
	if (m_params.unusedOutputs == vk::VK_SHADER_STAGE_VERTEX_BIT)
		vertAlt
			<< "color = vec4(1.0f);\n";
	vertAlt
		<< "}\n";

	vertAltNoTess
		<< "#version 450\n";
	if (m_params.unusedOutputs == vk::VK_SHADER_STAGE_VERTEX_BIT)
		vertAltNoTess
		<< "layout (location = 0) out vec4 color;\n";
	vertAltNoTess
		<< "void main() {\n"
		<< "    vec2 pos = vec2(float(gl_VertexIndex & 1), float((gl_VertexIndex >> 1) & 1));\n"
		<< "    gl_Position = vec4((pos.x - 0.5f) * 2.0f * 1.5f, (pos.y - 0.5f) * 2.0f, 0.0f, 1.0f);\n";
	if (m_params.unusedOutputs == vk::VK_SHADER_STAGE_VERTEX_BIT)
		vertAltNoTess
		<< "	color = vec4(1.0f);\n";
	vertAltNoTess
		<< "}\n";

	vertAltNoGeom
		<< "#version 450\n";
	if (m_params.unusedOutputs == vk::VK_SHADER_STAGE_VERTEX_BIT)
		vertAltNoGeom
		<< "layout (location = 0) out vec4 color;\n";
	vertAltNoGeom
		<< "void main() {\n"
		<< "    vec2 pos = vec2(float(gl_VertexIndex & 1), float((gl_VertexIndex >> 1) & 1));\n"
		<< "    gl_Position = vec4((pos.x - 0.5f) * 2.0f, (pos.y - 0.5f) * 2.0f * 1.5f, 0.0f, 1.0f);\n";
	if (m_params.unusedOutputs == vk::VK_SHADER_STAGE_VERTEX_BIT)
		vertAltNoGeom
		<< "	color = vec4(1.0f);\n";
	vertAltNoGeom
		<< "}\n";

	vertAltNoTessGeom
		<< "#version 450\n";
	if (m_params.unusedOutputs == vk::VK_SHADER_STAGE_VERTEX_BIT)
		vertAltNoTessGeom
		<< "layout (location = 0) out vec4 color;\n";
	vertAltNoTessGeom
		<< "void main() {\n"
		<< "    vec2 pos = vec2(float(gl_VertexIndex & 1), float((gl_VertexIndex >> 1) & 1));\n"
		<< "    gl_Position = vec4((pos - 0.5f) * 2 * 1.5f, 0.0f, 1.0f);\n";
	if (m_params.unusedOutputs == vk::VK_SHADER_STAGE_VERTEX_BIT)
		vertAltNoTessGeom
		<< "	color = vec4(1.0f);\n";
	vertAltNoTessGeom
		<< "}\n";

	tescAlt
		<< "#version 450\n"
		<< "\n"
		<< "layout(vertices = 4) out;\n";
	if (m_params.unusedOutputs == vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
		tescAlt
			<< "layout (location = 0) out vec4 color[];\n";
	tescAlt
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "    if (gl_InvocationID == 0) {\n"
		<< "		gl_TessLevelInner[0] = 1.0;\n"
		<< "		gl_TessLevelInner[1] = 1.0;\n"
		<< "		gl_TessLevelOuter[0] = 1.0;\n"
		<< "		gl_TessLevelOuter[1] = 1.0;\n"
		<< "		gl_TessLevelOuter[2] = 1.0;\n"
		<< "		gl_TessLevelOuter[3] = 1.0;\n"
		<< "	}\n"
		<< "	vec4 pos = gl_in[gl_InvocationID].gl_Position;\n"
		<< "	pos.xy *= 0.5f;\n"
		<< "    gl_out[gl_InvocationID].gl_Position = pos;\n";
	if (m_params.unusedOutputs == vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
		tescAlt
			<< "	color[gl_InvocationID] = vec4(1.0f);\n";
	tescAlt
		<< "}\n";

	teseAlt
		<< "#version 450\n"
		<< "\n"
		<< "layout(quads, equal_spacing) in;\n";
	if (m_params.unusedOutputs == vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
		teseAlt
		<< "layout (location = 0) out vec4 color;\n";
	teseAlt
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	float u = gl_TessCoord.x;\n"
		<< "	float v = gl_TessCoord.y;\n"
		<< "	float omu = 1.0f - u;\n"
		<< "	float omv = 1.0f - v;\n"
		<< "	gl_Position = omu * omv * gl_in[0].gl_Position + u * omv * gl_in[2].gl_Position + u * v * gl_in[3].gl_Position + omu * v * gl_in[1].gl_Position;\n"
		<< "	gl_Position.x *= 0.5f;\n";
	if (m_params.unusedOutputs == vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
		teseAlt
		<< "	color = vec4(1.0f);\n";
	teseAlt
		<< "}\n";

	geomAlt
		<< "#version 450\n"
		<< "layout(triangles) in;\n"
		<< "layout(triangle_strip, max_vertices = 4) out;\n";
	if (m_params.unusedOutputs == vk::VK_SHADER_STAGE_GEOMETRY_BIT)
		geomAlt
			<< "layout (location = 0) out vec4 color;\n";
	geomAlt
		<< "\n"
		<< "void main(void)\n"
		<< "{\n"
		<< "    gl_Position = gl_in[0].gl_Position;\n"
		<< "	gl_Position.y *= 0.5f;\n"
		<< "    EmitVertex();\n"
		<< "    gl_Position = gl_in[1].gl_Position;\n"
		<< "	gl_Position.y *= 0.5f;\n"
		<< "    EmitVertex();\n"
		<< "    gl_Position = gl_in[2].gl_Position;\n"
		<< "	gl_Position.y *= 0.5f;\n"
		<< "    EmitVertex();\n"
		<< "    EndPrimitive();\n";
	if (m_params.unusedOutputs == vk::VK_SHADER_STAGE_GEOMETRY_BIT)
		geomAlt
			<< "	color = vec4(1.0f);\n";
	geomAlt
		<< "}\n";

	fragAlt
		<< "#version 450\n"
		<< "layout (location=0) out vec4 outColor;\n";
	if (m_params.unusedOutputs == vk::VK_SHADER_STAGE_FRAGMENT_BIT)
		fragAlt
			<< "layout (location = 1) out vec4 color;\n";
	fragAlt
		<< "void main() {\n"
		<< "    outColor = vec4(1.0f, 0.0f, 0.0f, 1.0f);\n";
	if (m_params.unusedOutputs == vk::VK_SHADER_STAGE_FRAGMENT_BIT)
		fragAlt
			<< "color = vec4(1.0f);\n";
	fragAlt
		<< "}\n";

	programCollection.glslSources.add("passThroughGeom") << glu::GeometrySource(passThroughGeom.str());
	programCollection.glslSources.add("blendFrag") << glu::FragmentSource(blendFrag.str());
	programCollection.glslSources.add("vertAlt") << glu::VertexSource(vertAlt.str());
	programCollection.glslSources.add("tescAlt") << glu::TessellationControlSource(tescAlt.str());
	programCollection.glslSources.add("teseAlt") << glu::TessellationEvaluationSource(teseAlt.str());
	programCollection.glslSources.add("geomAlt") << glu::GeometrySource(geomAlt.str());
	programCollection.glslSources.add("fragAlt") << glu::FragmentSource(fragAlt.str());

	programCollection.glslSources.add("vertNoTess") << glu::VertexSource(vertNoTess.str());
	programCollection.glslSources.add("vertNoGeom") << glu::VertexSource(vertNoGeom.str());
	programCollection.glslSources.add("vertNoTessGeom") << glu::VertexSource(vertNoTessGeom.str());
	programCollection.glslSources.add("vertAltNoTess") << glu::VertexSource(vertAltNoTess.str());
	programCollection.glslSources.add("vertAltNoGeom") << glu::VertexSource(vertAltNoGeom.str());
	programCollection.glslSources.add("vertAltNoTessGeom") << glu::VertexSource(vertAltNoTessGeom.str());
}


class ShaderObjectBindingInstance : public vkt::TestInstance
{
public:
								ShaderObjectBindingInstance		(Context& context, const BindingParams& params)
																: vkt::TestInstance	(context)
																, m_params			(params)
																{}
	virtual						~ShaderObjectBindingInstance	(void) {}

	tcu::TestStatus				iterate							(void) override;
private:
	BindingParams m_params;
};

tcu::TestStatus ShaderObjectBindingInstance::iterate (void)
{
	const vk::VkInstance				instance					= m_context.getInstance();
	const vk::InstanceDriver			instanceDriver				(m_context.getPlatformInterface(), instance);
	const vk::DeviceInterface&			vk							= m_context.getDeviceInterface();
	const vk::VkDevice					device						= m_context.getDevice();
	const vk::VkQueue					queue						= m_context.getUniversalQueue();
	const deUint32						queueFamilyIndex			= m_context.getUniversalQueueFamilyIndex();

	const auto							meshShaderFeatures			= m_context.getMeshShaderFeatures();
	const bool							tessellationSupported		= m_context.getDeviceFeatures().tessellationShader;
	const bool							geometrySupported			= m_context.getDeviceFeatures().geometryShader;
	const auto&							binaries					= m_context.getBinaryCollection();

	const vk::Unique<vk::VkDescriptorSetLayout> descriptorSetLayout(
		vk::DescriptorSetLayoutBuilder()
		.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT)
		.build(vk, device));

	vk::Move<vk::VkShaderEXT>			vertShader					= vk::createShader(vk, device, vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_VERTEX_BIT, binaries.get("vert"), tessellationSupported, geometrySupported));
	vk::Move<vk::VkShaderEXT>			tescShader;
	vk::Move<vk::VkShaderEXT>			teseShader;
	vk::Move<vk::VkShaderEXT>			geomShader;
	vk::Move<vk::VkShaderEXT>			fragShader					= vk::createShader(vk, device, vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_FRAGMENT_BIT, binaries.get("frag"), tessellationSupported, geometrySupported));
	vk::Move<vk::VkShaderEXT>			compShader					= vk::createShader(vk, device, vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_COMPUTE_BIT, binaries.get("comp"), tessellationSupported, geometrySupported, &*descriptorSetLayout));
	vk::Move<vk::VkShaderEXT>			taskShader;
	vk::Move<vk::VkShaderEXT>			meshShader;
	if (m_context.getDeviceFeatures().tessellationShader)
	{
		tescShader = vk::createShader(vk, device, vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, binaries.get("tesc"), tessellationSupported, geometrySupported));
		teseShader = vk::createShader(vk, device, vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, binaries.get("tese"), tessellationSupported, geometrySupported));
	}
	if (m_context.getDeviceFeatures().geometryShader)
	{
		geomShader = vk::createShader(vk, device, vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_GEOMETRY_BIT, binaries.get("geom"), tessellationSupported, geometrySupported));
	}
	if (m_params.useMeshShaders)
	{
		if (meshShaderFeatures.taskShader)
			taskShader = vk::createShader(vk, device, vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_TASK_BIT_EXT, binaries.get("task"), tessellationSupported, geometrySupported));
		if (meshShaderFeatures.meshShader)
			meshShader = vk::createShader(vk, device, vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_MESH_BIT_EXT, binaries.get("mesh"), tessellationSupported, geometrySupported, &*descriptorSetLayout));
	}

	const vk::Move<vk::VkCommandPool>	cmdPool		(vk::createCommandPool(vk, device, vk::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const vk::Move<vk::VkCommandBuffer>	cmdBuffer	(vk::allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const bool bind[] = { true, false };
	for (const auto bindVert : bind)
	{
		for (const auto bindTesc : bind)
		{
			for (const auto bindTese : bind)
			{
				for (const auto bindGeom : bind)
				{
					for (const auto bindFrag : bind)
					{
						for (const auto bindComp : bind)
						{
							for (const auto bindTask : bind)
							{
								if (bindVert && bindTask)
									continue;
								for (const auto bindMesh : bind)
								{
									if (bindVert && bindMesh)
										continue;
									std::vector<vk::VkShaderStageFlagBits> stages = {
										vk::VK_SHADER_STAGE_VERTEX_BIT,
										vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
										vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
										vk::VK_SHADER_STAGE_GEOMETRY_BIT,
										vk::VK_SHADER_STAGE_FRAGMENT_BIT,
										vk::VK_SHADER_STAGE_COMPUTE_BIT,
										vk::VK_SHADER_STAGE_MESH_BIT_EXT,
										vk::VK_SHADER_STAGE_TASK_BIT_EXT,
									};
									std::vector<vk::VkShaderEXT> shaders =
									{
										bindVert ? *vertShader : VK_NULL_HANDLE,
										(bindTesc && m_context.getDeviceFeatures().tessellationShader) ? *tescShader : VK_NULL_HANDLE,
										(bindTese && m_context.getDeviceFeatures().tessellationShader) ? *teseShader : VK_NULL_HANDLE,
										(bindGeom && m_context.getDeviceFeatures().geometryShader) ? *geomShader : VK_NULL_HANDLE,
										bindFrag ? *fragShader : VK_NULL_HANDLE,
										bindComp ? *compShader : VK_NULL_HANDLE,
										bindMesh ? *meshShader : VK_NULL_HANDLE,
										bindTask ? *taskShader : VK_NULL_HANDLE,
									};
									deUint32 count = 6u;
									if (meshShaderFeatures.meshShader)
										++count;
									if (meshShaderFeatures.taskShader)
										++count;
									vk::beginCommandBuffer(vk, *cmdBuffer);
									vk.cmdBindShadersEXT(*cmdBuffer, count, stages.data(), shaders.data());
									vk::endCommandBuffer(vk, *cmdBuffer);
									vk::submitCommandsAndWait(vk, device, queue, *cmdBuffer);
								}
							}
						}
					}
				}
			}
		}
	}

	if (m_context.getDeviceFeatures().tessellationShader && m_context.getDeviceFeatures().geometryShader && meshShaderFeatures.taskShader && meshShaderFeatures.meshShader)
	{
		std::vector<vk::VkShaderStageFlagBits> stages = {
			vk::VK_SHADER_STAGE_VERTEX_BIT,
			vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
			vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
			vk::VK_SHADER_STAGE_GEOMETRY_BIT,
			vk::VK_SHADER_STAGE_FRAGMENT_BIT,
			vk::VK_SHADER_STAGE_COMPUTE_BIT,
			vk::VK_SHADER_STAGE_MESH_BIT_EXT,
			vk::VK_SHADER_STAGE_TASK_BIT_EXT,
		};
		vk::beginCommandBuffer(vk, *cmdBuffer);
		vk.cmdBindShadersEXT(*cmdBuffer, (deUint32)stages.size(), stages.data(), DE_NULL);
		vk::endCommandBuffer(vk, *cmdBuffer);
		vk::submitCommandsAndWait(vk, device, queue, *cmdBuffer);
	}

	return tcu::TestStatus::pass("pass");
}

class MeshShaderObjectBindingInstance : public vkt::TestInstance
{
public:
							MeshShaderObjectBindingInstance		(Context& context, const MeshBindingDrawParams& params)
																: vkt::TestInstance	(context)
																, m_params			(params)
																{}
	virtual					~MeshShaderObjectBindingInstance	(void) {}

	tcu::TestStatus			iterate								(void) override;
private:
	MeshBindingDrawParams m_params;
};

tcu::TestStatus MeshShaderObjectBindingInstance::iterate (void)
{
	const vk::VkInstance				instance					= m_context.getInstance();
	const vk::InstanceDriver			instanceDriver				(m_context.getPlatformInterface(), instance);
	const vk::DeviceInterface&			vk							= m_context.getDeviceInterface();
	const vk::VkDevice					device						= m_context.getDevice();
	const vk::VkQueue					queue						= m_context.getUniversalQueue();
	const deUint32						queueFamilyIndex			= m_context.getUniversalQueueFamilyIndex();
	auto&								alloc						= m_context.getDefaultAllocator();
	tcu::TestLog&						log							= m_context.getTestContext().getLog();
	const auto							deviceExtensions			= vk::removeUnsupportedShaderObjectExtensions(m_context.getInstanceInterface(), m_context.getPhysicalDevice(), m_context.getDeviceExtensions());

	vk::VkFormat						colorAttachmentFormat		= vk::VK_FORMAT_R8G8B8A8_UNORM;
	const auto							subresourceRange			= makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

	const vk::VkImageCreateInfo	createInfo =
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType
		DE_NULL,									// const void*				pNext
		0u,											// VkImageCreateFlags		flags
		vk::VK_IMAGE_TYPE_2D,						// VkImageType				imageType
		colorAttachmentFormat,						// VkFormat					format
		{ 32, 32, 1 },								// VkExtent3D				extent
		1u,											// uint32_t					mipLevels
		1u,											// uint32_t					arrayLayers
		vk::VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits	samples
		vk::VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling			tiling
		vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT,	// VkImageUsageFlags		usage
		vk::VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode
		0,											// uint32_t					queueFamilyIndexCount
		DE_NULL,									// const uint32_t*			pQueueFamilyIndices
		vk::VK_IMAGE_LAYOUT_UNDEFINED				// VkImageLayout			initialLayout
	};

	de::MovePtr<vk::ImageWithMemory>		image			= de::MovePtr<vk::ImageWithMemory>(new vk::ImageWithMemory(vk, device, alloc, createInfo, vk::MemoryRequirement::Any));
	const auto								imageView		= vk::makeImageView(vk, device, **image, vk::VK_IMAGE_VIEW_TYPE_2D, colorAttachmentFormat, subresourceRange);
	const vk::VkRect2D						renderArea		= vk::makeRect2D(0, 0, 32, 32);

	const vk::Unique<vk::VkDescriptorSetLayout> descriptorSetLayout(
		vk::DescriptorSetLayoutBuilder()
		.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_TASK_BIT_EXT | vk::VK_SHADER_STAGE_MESH_BIT_EXT)
		.build(vk, device));

	const vk::Unique<vk::VkDescriptorPool>	descriptorPool(
		vk::DescriptorPoolBuilder()
		.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.build(vk, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	const vk::VkDeviceSize					bufferSizeBytes = sizeof(deUint32) * 4;
	const vk::Unique<vk::VkDescriptorSet>	descriptorSet	(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));
	const vk::BufferWithMemory				outputBuffer	(vk, device, alloc, vk::makeBufferCreateInfo(bufferSizeBytes, vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), vk::MemoryRequirement::HostVisible);

	const vk::VkDescriptorBufferInfo		descriptorInfo	= vk::makeDescriptorBufferInfo(*outputBuffer, 0ull, bufferSizeBytes);
	vk::DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorInfo)
		.update(vk, device);

	const auto								pipelineLayout	= makePipelineLayout(vk, device, *descriptorSetLayout);

	const auto&								binaries		= m_context.getBinaryCollection();
	const auto								taskShader1		= vk::createShader(vk, device, vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_TASK_BIT_EXT, binaries.get("task1"), false, false, &*descriptorSetLayout));
	const auto								taskShader2		= vk::createShader(vk, device, vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_TASK_BIT_EXT, binaries.get("task2"), false, false, &*descriptorSetLayout));
	const auto								meshShader1		= vk::createShader(vk, device, vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_MESH_BIT_EXT, binaries.get("mesh1"), false, false, &*descriptorSetLayout));
	const auto								meshShader2		= vk::createShader(vk, device, vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_MESH_BIT_EXT, binaries.get("mesh2"), false, false, &*descriptorSetLayout));
	const auto								fragShader		= vk::createShader(vk, device, vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_FRAGMENT_BIT, binaries.get("frag"), false, false, &*descriptorSetLayout));

	const vk::Move<vk::VkCommandPool>		cmdPool			(vk::createCommandPool(vk, device, 0u, queueFamilyIndex));
	const vk::Move<vk::VkCommandBuffer>		cmdBuffer		(vk::allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const vk::VkClearValue					clearValue		= vk::makeClearValueColor({ 0.0f, 0.0f, 0.0f, 1.0f });

	vk::beginCommandBuffer(vk, *cmdBuffer);

	vk::VkImageMemoryBarrier preImageBarrier = vk::makeImageMemoryBarrier(vk::VK_ACCESS_NONE, vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, vk::VK_IMAGE_LAYOUT_UNDEFINED, vk::VK_IMAGE_LAYOUT_GENERAL, **image, subresourceRange);
	vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, (vk::VkDependencyFlags)0u, 0u, (const vk::VkMemoryBarrier*)DE_NULL, 0u, (const vk::VkBufferMemoryBarrier*)DE_NULL, 1u, &preImageBarrier);

	vk::beginRendering(vk, *cmdBuffer, *imageView, renderArea, clearValue, vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_ATTACHMENT_LOAD_OP_CLEAR);
	vk::setDefaultShaderObjectDynamicStates(vk, *cmdBuffer, deviceExtensions, vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, true);
	vk.cmdBindDescriptorSets(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0, 1, &descriptorSet.get(), 0, DE_NULL);

	std::vector<vk::VkShaderStageFlagBits> nullStages = {
		vk::VK_SHADER_STAGE_VERTEX_BIT,
	};
	if (m_context.getDeviceFeatures().tessellationShader) {
		nullStages.push_back(vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
		nullStages.push_back(vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
	}
	if (m_context.getDeviceFeatures().geometryShader) {
		nullStages.push_back(vk::VK_SHADER_STAGE_GEOMETRY_BIT);
	}
	for (const auto& stage : nullStages) {
		vk::VkShaderEXT shader = VK_NULL_HANDLE;
		vk.cmdBindShadersEXT(*cmdBuffer, 1u, &stage, &shader);
	}

	const vk::VkShaderStageFlagBits stages[] = {
			vk::VK_SHADER_STAGE_TASK_BIT_EXT,
			vk::VK_SHADER_STAGE_MESH_BIT_EXT,
			vk::VK_SHADER_STAGE_FRAGMENT_BIT,
	};
	vk::VkShaderEXT shaders[] = {
		*taskShader1,
		*meshShader1,
		*fragShader,
	};
	vk.cmdBindShadersEXT(*cmdBuffer, 3u, stages, shaders);
	vk.cmdDrawMeshTasksEXT(*cmdBuffer, 1, 1, 1);

	vk::VkBufferMemoryBarrier shaderBufferBarrier = vk::makeBufferMemoryBarrier(vk::VK_ACCESS_SHADER_WRITE_BIT, vk::VK_ACCESS_SHADER_WRITE_BIT, *outputBuffer, 0u, bufferSizeBytes);
	vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, vk::VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, (vk::VkDependencyFlags)0u, 0u, (const vk::VkMemoryBarrier*)DE_NULL, 1u, &shaderBufferBarrier, 0u, (const vk::VkImageMemoryBarrier*)DE_NULL);

	if (m_params.stage == vk::VK_SHADER_STAGE_TASK_BIT_EXT)
		vk.cmdBindShadersEXT(*cmdBuffer, 1u, &stages[0], &*taskShader2);
	else if (m_params.stage == vk::VK_SHADER_STAGE_MESH_BIT_EXT)
		vk.cmdBindShadersEXT(*cmdBuffer, 1u, &stages[1], &*meshShader2);
	vk.cmdDrawMeshTasksEXT(*cmdBuffer, 1, 1, 1);

	vk::endRendering(vk, *cmdBuffer);

	vk::VkImageMemoryBarrier postImageBarrier = vk::makeImageMemoryBarrier(vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, vk::VK_ACCESS_TRANSFER_READ_BIT, vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_IMAGE_LAYOUT_GENERAL, **image, subresourceRange);
	vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, (vk::VkDependencyFlags)0u, 0u, (const vk::VkMemoryBarrier*)DE_NULL, 0u, (const vk::VkBufferMemoryBarrier*)DE_NULL, 1u, &postImageBarrier);
	vk::VkBufferMemoryBarrier bufferBarrier = vk::makeBufferMemoryBarrier(vk::VK_ACCESS_SHADER_WRITE_BIT, vk::VK_ACCESS_HOST_READ_BIT, *outputBuffer, 0u, bufferSizeBytes);
	vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, (vk::VkDependencyFlags)0u, 0u, (const vk::VkMemoryBarrier*)DE_NULL, 1u, &bufferBarrier, 0u, (const vk::VkImageMemoryBarrier*)DE_NULL);
	vk::endCommandBuffer(vk, *cmdBuffer);

	submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

	const vk::Allocation& outputBufferAllocation = outputBuffer.getAllocation();
	invalidateAlloc(vk, device, outputBufferAllocation);

	const deUint32* bufferPtr = static_cast<deUint32*>(outputBufferAllocation.getHostPtr());

	if (m_params.stage == vk::VK_SHADER_STAGE_TASK_BIT_EXT)
	{
		if (bufferPtr[0] != 4u || bufferPtr[1] != 5u || bufferPtr[2] != 2u || bufferPtr[3] != 3u)
		{
			log << tcu::TestLog::Message << "Buffer values were expected to be [4, 5, 2, 3], but were[" << bufferPtr[0] << ", " << bufferPtr[1] << ", " << bufferPtr[2] << ", " << bufferPtr[3] << ", " << "]" << tcu::TestLog::EndMessage;
			return tcu::TestStatus::fail("Fail");
		}
	}
	else if (m_params.stage == vk::VK_SHADER_STAGE_MESH_BIT_EXT)
	{
		if (bufferPtr[0] != 0u || bufferPtr[1] != 1u || bufferPtr[2] != 6u || bufferPtr[3] != 7u)
		{
			log << tcu::TestLog::Message << "Buffer values were expected to be [0, 1, 6, 7], but were[" << bufferPtr[0] << ", " << bufferPtr[1] << ", " << bufferPtr[2] << ", " << bufferPtr[3] << ", " << "]" << tcu::TestLog::EndMessage;
			return tcu::TestStatus::fail("Fail");
		}
	}

	return tcu::TestStatus::pass("pass");
}

class MeshShaderObjectBindingCase : public vkt::TestCase
{
public:
					MeshShaderObjectBindingCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const MeshBindingDrawParams& params)
													: vkt::TestCase		(testCtx, name, description)
													, m_params			(params)
													{}
	virtual			~MeshShaderObjectBindingCase	(void) {}

	void			checkSupport					(vkt::Context& context) const override;
	virtual void	initPrograms					(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance					(Context& context) const override { return new MeshShaderObjectBindingInstance(context, m_params); }
private:
	MeshBindingDrawParams m_params;
};

void MeshShaderObjectBindingCase::checkSupport (vkt::Context& context) const
{
	context.requireDeviceFunctionality("VK_EXT_shader_object");
	context.requireDeviceFunctionality("VK_EXT_mesh_shader");
	const auto& features = context.getMeshShaderFeaturesEXT();
	if (!features.taskShader)
		TCU_THROW(NotSupportedError, "Task shaders not supported");
	if (!features.meshShader)
		TCU_THROW(NotSupportedError, "Mesh shaders not supported");
}

void MeshShaderObjectBindingCase::initPrograms (vk::SourceCollections& programCollection) const
{
	std::stringstream task1;
	std::stringstream task2;
	std::stringstream mesh1;
	std::stringstream mesh2;
	std::stringstream frag;

	task1
		<< "#version 450\n"
		<< "#extension GL_EXT_mesh_shader : enable\n"
		<< "layout (local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
		<< "layout(set = 0, binding = 0) buffer Output {\n"
		<< "    uint values[4];\n"
		<< "} buffer_out;\n\n"
		<< "void main ()\n"
		<< "{\n"
		<< "    buffer_out.values[0] = 0u;\n"
		<< "    buffer_out.values[1] = 1u;\n"
		<< "    EmitMeshTasksEXT(1u, 1u, 1u);\n"
		<< "}\n";

	task2
		<< "#version 450\n"
		<< "#extension GL_EXT_mesh_shader : enable\n"
		<< "layout (local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
		<< "layout(set = 0, binding = 0) buffer Output {\n"
		<< "    uint values[4];\n"
		<< "} buffer_out;\n\n"
		<< "void main ()\n"
		<< "{\n"
		<< "    buffer_out.values[0] = 4u;\n"
		<< "    buffer_out.values[1] = 5u;\n"
		<< "    EmitMeshTasksEXT(1u, 1u, 1u);\n"
		<< "}\n";

	mesh1
		<< "#version 460\n"
		<< "#extension GL_EXT_mesh_shader : require\n"
		<< "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
		<< "layout(max_vertices = 3) out;\n"
		<< "layout(max_primitives = 1) out;\n"
		<< "layout(triangles) out;\n"
		<< "layout(set = 0, binding = 0) buffer Output {\n"
		<< "    uint values[4];\n"
		<< "} buffer_out;\n\n"
		<< "void main() {\n"
		<< "      SetMeshOutputsEXT(3, 1);\n"
		<< "      gl_MeshVerticesEXT[0].gl_Position = vec4(-1.0, -1.0, 0.0f, 1.0f);\n"
		<< "      gl_MeshVerticesEXT[1].gl_Position = vec4( 3.0, -1.0, 0.0f, 1.0f);\n"
		<< "      gl_MeshVerticesEXT[2].gl_Position = vec4(-1.0,  3.0, 0.0f, 1.0f);\n"
		<< "      gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0, 1, 2);\n"
		<< "      buffer_out.values[2] = 2u;\n"
		<< "      buffer_out.values[3] = 3u;\n"
		<< "}\n";

	mesh2
		<< "#version 460\n"
		<< "#extension GL_EXT_mesh_shader : require\n"
		<< "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
		<< "layout(max_vertices = 3) out;\n"
		<< "layout(max_primitives = 1) out;\n"
		<< "layout(triangles) out;\n"
		<< "layout(set = 0, binding = 0) buffer Output {\n"
		<< "    uint values[4];\n"
		<< "} buffer_out;\n\n"
		<< "void main() {\n"
		<< "      SetMeshOutputsEXT(3, 1);\n"
		<< "      gl_MeshVerticesEXT[0].gl_Position = vec4(-1.0, -1.0, 0.0f, 1.0f);\n"
		<< "      gl_MeshVerticesEXT[1].gl_Position = vec4( 3.0, -1.0, 0.0f, 1.0f);\n"
		<< "      gl_MeshVerticesEXT[2].gl_Position = vec4(-1.0,  3.0, 0.0f, 1.0f);\n"
		<< "      gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0, 1, 2);\n"
		<< "      buffer_out.values[2] = 6u;\n"
		<< "      buffer_out.values[3] = 7u;\n"
		<< "}\n";

	frag
		<< "#version 450\n"
		<< "layout (location=0) out vec4 outColor;\n"
		<< "void main() {\n"
		<< "    outColor = vec4(1.0f);\n"
		<< "}\n";

	programCollection.glslSources.add("task1") << glu::TaskSource(task1.str()) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
	programCollection.glslSources.add("task2") << glu::TaskSource(task2.str()) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
	programCollection.glslSources.add("mesh1") << glu::MeshSource(mesh1.str()) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
	programCollection.glslSources.add("mesh2") << glu::MeshSource(mesh2.str()) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
	programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

class ShaderObjectBindingCase : public vkt::TestCase
{
public:
					ShaderObjectBindingCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const BindingParams& params)
												: vkt::TestCase		(testCtx, name, description)
												, m_params			(params)
												{}
	virtual			~ShaderObjectBindingCase	(void) {}

	void			checkSupport				(vkt::Context& context) const override;
	virtual void	initPrograms				(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance				(Context& context) const override { return new ShaderObjectBindingInstance(context, m_params); }
private:
	BindingParams m_params;
};

void ShaderObjectBindingCase::checkSupport (Context& context) const
{
	context.requireDeviceFunctionality("VK_EXT_shader_object");
	if (m_params.useMeshShaders)
		context.requireDeviceFunctionality("VK_EXT_mesh_shader");
}

void ShaderObjectBindingCase::initPrograms (vk::SourceCollections& programCollection) const
{
	vk::addBasicShaderObjectShaders(programCollection);

	if (m_params.useMeshShaders)
	{
		std::stringstream task;
		std::stringstream mesh;

		task
			<< "#version 450\n"
			<< "#extension GL_EXT_mesh_shader : enable\n"
			<< "layout (local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
			<< "void main ()\n"
			<< "{\n"
			<< "	EmitMeshTasksEXT(1u, 1u, 1u);\n"
			<< "}\n";

		mesh
			<< "#version 460\n"
			<< "#extension GL_EXT_mesh_shader : require\n"
			<< "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
			<< "layout(max_vertices = 3) out;\n"
			<< "layout(max_primitives = 1) out;\n"
			<< "layout(triangles) out;\n"
			<< "layout(set = 0, binding = 0) buffer Output {\n"
			<< "    uint values[4];\n"
			<< "} buffer_out;\n\n"
			<< "void main() {\n"
			<< "      SetMeshOutputsEXT(3, 1);\n"
			<< "      gl_MeshVerticesEXT[0].gl_Position = vec4(-1.0, -1.0, 0.0f, 1.0f);\n"
			<< "      gl_MeshVerticesEXT[1].gl_Position = vec4( 3.0, -1.0, 0.0f, 1.0f);\n"
			<< "      gl_MeshVerticesEXT[2].gl_Position = vec4(-1.0,  3.0, 0.0f, 1.0f);\n"
			<< "      gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0, 1, 2);\n"
			<< "      buffer_out.values[0] = 0u;\n"
			<< "      buffer_out.values[1] = 1u;\n"
			<< "      buffer_out.values[2] = 2u;\n"
			<< "      buffer_out.values[3] = 3u;\n"
			<< "}\n";

		programCollection.glslSources.add("task") << glu::TaskSource(task.str()) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
		programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
	}
}

}


tcu::TestCaseGroup* createShaderObjectBindingTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> bindingGroup(new tcu::TestCaseGroup(testCtx, "binding", ""));

	BindingDrawParams params;
	params.testType = PASSTHROUGH_GEOM;
	params.stage = vk::VK_SHADER_STAGE_GEOMETRY_BIT;
	params.unusedOutputs = vk::VK_SHADER_STAGE_TASK_BIT_EXT;
	params.binaryStage = vk::VK_SHADER_STAGE_TASK_BIT_EXT;
	params.bindUnsupported = false;
	params.setStateAfter = false;
	params.unbindWithNullpShaders = false;

	bindingGroup->addChild(new ShaderObjectBindingDrawCase(testCtx, "unbind_passthrough_geom", "", params));

	const struct
	{
		vk::VkShaderStageFlagBits stage;
		const char* name;
	} stageTest[] =
	{
		{ vk::VK_SHADER_STAGE_VERTEX_BIT,					"vert" },
		{ vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,		"tesc" },
		{ vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,	"tese" },
		{ vk::VK_SHADER_STAGE_GEOMETRY_BIT,					"geom" },
		{ vk::VK_SHADER_STAGE_FRAGMENT_BIT,					"frag" },
	};
	params.testType = SWAP;
	for (const auto& stage : stageTest)
	{
		params.stage = stage.stage;
		params.unusedOutputs = vk::VK_SHADER_STAGE_ALL; // Unused
		params.binaryStage = vk::VK_SHADER_STAGE_ALL; // Unused
		params.setStateAfter = false;
		std::string name = "swap_" + std::string(stage.name);
		bindingGroup->addChild(new ShaderObjectBindingDrawCase(testCtx, name.c_str(), "", params));
		for (const auto& unusedOutputs : stageTest)
		{
			for (const auto& binaryStage : stageTest)
			{
				for (deUint32 i = 0; i < 2; ++i)
				{
					params.stage = stage.stage;
					params.unusedOutputs = unusedOutputs.stage;
					params.binaryStage = binaryStage.stage;
					params.setStateAfter = (bool)i;
					std::string name2 = "swap_" + std::string(stage.name) + "_unused_output_" + std::string(unusedOutputs.name) + "_binary_" + std::string(binaryStage.name) + "_" + ((i == 0) ? "before" : "after");
					bindingGroup->addChild(new ShaderObjectBindingDrawCase(testCtx, name2.c_str(), "", params));
				}
			}
		}
	}

	params.unusedOutputs = vk::VK_SHADER_STAGE_ALL;
	params.binaryStage = vk::VK_SHADER_STAGE_ALL;

	const struct
	{
		vk::VkShaderStageFlagBits stage;
		const char* name;
	} unbindStageTest[] =
	{
		{ vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,		"tesc" },
		{ vk::VK_SHADER_STAGE_GEOMETRY_BIT,					"geom" },
	};
	params.testType = UNBIND;
	params.setStateAfter = false;
	for (const auto& stage : unbindStageTest)
	{
		for (deUint32 i = 0; i < 2; ++i)
		{
			params.stage = stage.stage;
			params.unbindWithNullpShaders = (bool)i;
			std::string name = "unbind_" + std::string(stage.name) + (params.unbindWithNullpShaders ? "_null_pshaders" : "_null_handle");
			bindingGroup->addChild(new ShaderObjectBindingDrawCase(testCtx, name.c_str(), "", params));
		}
	}

	const struct
	{
		vk::VkShaderStageFlagBits stage;
		const char* name;
	} meshStageTest[] =
	{
		{ vk::VK_SHADER_STAGE_TASK_BIT_EXT,		"task" },
		{ vk::VK_SHADER_STAGE_MESH_BIT_EXT,		"mesh" },
	};

	for (const auto& stage : meshStageTest)
	{
		MeshBindingDrawParams meshParams;
		meshParams.stage = stage.stage;
		std::string name = "mesh_swap_" + std::string(stage.name);
		bindingGroup->addChild(new MeshShaderObjectBindingCase(testCtx, name.c_str(), "", meshParams));
	}

	params.testType = DISABLED;
	params.stage = vk::VK_SHADER_STAGE_GEOMETRY_BIT;
	bindingGroup->addChild(new ShaderObjectBindingDrawCase(testCtx, "disabled_geom", "", params));
	params.stage = vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
	bindingGroup->addChild(new ShaderObjectBindingDrawCase(testCtx, "disabled_tess", "", params));
	params.stage = vk::VK_SHADER_STAGE_GEOMETRY_BIT;
	params.bindUnsupported = true;
	bindingGroup->addChild(new ShaderObjectBindingDrawCase(testCtx, "disabled_geom_bind", "", params));
	params.stage = vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
	bindingGroup->addChild(new ShaderObjectBindingDrawCase(testCtx, "disabled_tess_bind", "", params));
	params.testType = DRAW_DISPATCH_DRAW;
	bindingGroup->addChild(new ShaderObjectBindingDrawCase(testCtx, "draw_dispatch_draw", "", params));
	params.testType = DISPATCH_DRAW_DISPATCH;
	bindingGroup->addChild(new ShaderObjectBindingDrawCase(testCtx, "dispatch_draw_dispatch", "", params));

	BindingParams bindingParams;
	bindingParams.useMeshShaders = false;
	bindingGroup->addChild(new ShaderObjectBindingCase(testCtx, "bindings", "", bindingParams));
	bindingParams.useMeshShaders = true;
	bindingGroup->addChild(new ShaderObjectBindingCase(testCtx, "bindings_mesh_shaders", "", bindingParams));

	return bindingGroup.release();
}

} // ShaderObject
} // vkt
