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
 * \brief Shader Object Link Tests
 *//*--------------------------------------------------------------------*/

#include "vktShaderObjectCreateTests.hpp"
#include "deUniquePtr.hpp"
#include "tcuTestCase.hpp"
#include "vktTestCase.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vktShaderObjectCreateUtil.hpp"
#include "vkObjUtil.hpp"
#include "deRandom.hpp"
#include "vkBuilderUtil.hpp"
#include "vkBarrierUtil.hpp"

namespace vkt
{
namespace ShaderObject
{

namespace
{

enum ShaderType {
	UNUSED,
	LINKED,
	UNLINKED,
};

struct Shaders {
	ShaderType	vertex;
	ShaderType	tesellation_control;
	ShaderType	tesellation_evaluation;
	ShaderType	geometry;
	ShaderType	fragment;
};

struct MeshShaders {
	ShaderType	task;
	ShaderType	mesh;
	ShaderType	fragment;
};

struct NextStages {
	vk::VkShaderStageFlags	vertNextStage;
	vk::VkShaderStageFlags	tescNextStage;
	vk::VkShaderStageFlags	teseNextStage;
	vk::VkShaderStageFlags	geomNextStage;
};

struct MeshNextStages {
	vk::VkShaderStageFlags	taskNextStage;
	vk::VkShaderStageFlags	meshNextStage;
};

enum BindType {
	SEPARATE,
	ONE_LINKED_UNLINKED,
	ALL,
};

struct TestParams {
	Shaders		shaders;
	bool		randomOrder;
	NextStages	nextStages;
	bool		separateLinked;
	BindType	separateBind;
};

struct MeshParams {
	MeshShaders		shaders;
	bool			randomOrder;
	MeshNextStages	nextStages;
};

class ShaderObjectLinkInstance : public vkt::TestInstance
{
public:
							ShaderObjectLinkInstance	(Context& context, const TestParams& params)
														: vkt::TestInstance	(context)
														, m_params			(params)
														{}
	virtual					~ShaderObjectLinkInstance	(void) {}

	tcu::TestStatus			iterate						(void) override;
private:
	vk::VkShaderStageFlags	getNextStage				(vk::VkShaderStageFlagBits currentStage);

	TestParams m_params;
};

vk::VkShaderStageFlags ShaderObjectLinkInstance::getNextStage (vk::VkShaderStageFlagBits currentStage)
{
	if (currentStage == vk::VK_SHADER_STAGE_VERTEX_BIT && m_params.shaders.vertex == LINKED)
	{
		if (m_params.shaders.tesellation_control != UNUSED)
		{
			if (m_params.shaders.tesellation_control == LINKED)
				return vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
		}
		else if (m_params.shaders.geometry != UNUSED)
		{
			if (m_params.shaders.geometry == LINKED)
				return vk::VK_SHADER_STAGE_GEOMETRY_BIT;
		}
		else if (m_params.shaders.fragment != UNUSED)
		{
			if (m_params.shaders.fragment == LINKED)
				return vk::VK_SHADER_STAGE_FRAGMENT_BIT;
		}
	}
	else if (currentStage == vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT && m_params.shaders.tesellation_control == LINKED && m_params.shaders.tesellation_evaluation == LINKED)
	{
		return vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
	}
	else if (currentStage == vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT && m_params.shaders.tesellation_evaluation == LINKED)
	{
		if (m_params.shaders.geometry != UNUSED)
		{
			if (m_params.shaders.geometry == LINKED)
				return vk::VK_SHADER_STAGE_GEOMETRY_BIT;
		}
		else if (m_params.shaders.fragment != UNUSED)
		{
			if (m_params.shaders.fragment == LINKED)
				return vk::VK_SHADER_STAGE_FRAGMENT_BIT;
		}
	}
	else if (currentStage == vk::VK_SHADER_STAGE_GEOMETRY_BIT && m_params.shaders.geometry == LINKED)
	{
		if (m_params.shaders.fragment != UNUSED)
		{
			if (m_params.shaders.fragment == LINKED)
				return vk::VK_SHADER_STAGE_FRAGMENT_BIT;
		}
	}

	if (currentStage == vk::VK_SHADER_STAGE_VERTEX_BIT)
		return m_params.nextStages.vertNextStage;

	if (currentStage == vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
		return m_params.nextStages.tescNextStage;

	if (currentStage == vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
		return m_params.nextStages.teseNextStage;

	if (currentStage == vk::VK_SHADER_STAGE_GEOMETRY_BIT)
		return m_params.nextStages.geomNextStage;

	return 0;
}

tcu::TestStatus ShaderObjectLinkInstance::iterate (void)
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
	const bool							tessellationSupported		= m_context.getDeviceFeatures().tessellationShader;
	const bool							geometrySupported			= m_context.getDeviceFeatures().geometryShader;
	const bool							taskSupported				= m_context.getMeshShaderFeatures().taskShader;
	const bool							meshSupported				= m_context.getMeshShaderFeatures().meshShader;

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

	de::MovePtr<vk::ImageWithMemory>	image					= de::MovePtr<vk::ImageWithMemory>(new vk::ImageWithMemory(vk, device, alloc, createInfo, vk::MemoryRequirement::Any));
	const auto							imageView				= vk::makeImageView(vk, device, **image, vk::VK_IMAGE_VIEW_TYPE_2D, colorAttachmentFormat, subresourceRange);
	const vk::VkRect2D					renderArea				= vk::makeRect2D(0, 0, 32, 32);

	const vk::VkDeviceSize				colorOutputBufferSize	= renderArea.extent.width * renderArea.extent.height * tcu::getPixelSize(vk::mapVkFormat(colorAttachmentFormat));
	de::MovePtr<vk::BufferWithMemory>	colorOutputBuffer		= de::MovePtr<vk::BufferWithMemory>(new vk::BufferWithMemory(
		vk, device, alloc, makeBufferCreateInfo(colorOutputBufferSize, vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT), vk::MemoryRequirement::HostVisible));

	const auto&					binaries			= m_context.getBinaryCollection();
	const auto&					vert				= binaries.get("vert");
	const auto&					tesc				= binaries.get("tesc");
	const auto&					tese				= binaries.get("tese");
	const auto&					geom				= binaries.get("geom");
	const auto&					frag				= binaries.get("frag");

	vk::VkShaderEXT				vertShader;
	vk::VkShaderEXT				tescShader;
	vk::VkShaderEXT				teseShader;
	vk::VkShaderEXT				geomShader;
	vk::VkShaderEXT				fragShader;

	std::vector<vk::VkShaderCreateInfoEXT> shaderCreateInfos;

	vk::VkShaderCreateInfoEXT	vertShaderCreateInfo = vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_VERTEX_BIT, vert, tessellationSupported, geometrySupported);
	vertShaderCreateInfo.nextStage = getNextStage(vk::VK_SHADER_STAGE_VERTEX_BIT);

	if (m_params.shaders.vertex == LINKED)
	{
		vertShaderCreateInfo.flags = vk::VK_SHADER_CREATE_LINK_STAGE_BIT_EXT;
		shaderCreateInfos.push_back(vertShaderCreateInfo);
	}
	else if (m_params.shaders.vertex == UNLINKED)
	{
		vk.createShadersEXT(device, 1, &vertShaderCreateInfo, DE_NULL, &vertShader);
	}

	vk::VkShaderCreateInfoEXT tescShaderCreateInfo = vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, tesc, tessellationSupported, geometrySupported);
	tescShaderCreateInfo.nextStage = getNextStage(vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);

	if (m_params.shaders.tesellation_control == LINKED)
	{
		tescShaderCreateInfo.flags = vk::VK_SHADER_CREATE_LINK_STAGE_BIT_EXT;
		shaderCreateInfos.push_back(tescShaderCreateInfo);
	}
	else if (m_params.shaders.tesellation_control == UNLINKED)
	{
		vk.createShadersEXT(device, 1, &tescShaderCreateInfo, DE_NULL, &tescShader);
	}

	vk::VkShaderCreateInfoEXT teseShaderCreateInfo = vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, tese, tessellationSupported, geometrySupported);
	teseShaderCreateInfo.nextStage = getNextStage(vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);

	if (m_params.shaders.tesellation_evaluation == LINKED)
	{
		teseShaderCreateInfo.flags = vk::VK_SHADER_CREATE_LINK_STAGE_BIT_EXT;
		shaderCreateInfos.push_back(teseShaderCreateInfo);
	}
	else if (m_params.shaders.tesellation_evaluation == UNLINKED)
	{
		vk.createShadersEXT(device, 1, &teseShaderCreateInfo, DE_NULL, &teseShader);
	}

	vk::VkShaderCreateInfoEXT geomShaderCreateInfo = vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_GEOMETRY_BIT, geom, tessellationSupported, geometrySupported);
	geomShaderCreateInfo.nextStage = getNextStage(vk::VK_SHADER_STAGE_GEOMETRY_BIT);

	if (m_params.shaders.geometry == LINKED)
	{
		geomShaderCreateInfo.flags = vk::VK_SHADER_CREATE_LINK_STAGE_BIT_EXT;
		shaderCreateInfos.push_back(geomShaderCreateInfo);
	}
	else if (m_params.shaders.geometry == UNLINKED)
	{
		vk.createShadersEXT(device, 1, &geomShaderCreateInfo, DE_NULL, &geomShader);
	}

	vk::VkShaderCreateInfoEXT fragShaderCreateInfo = vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_FRAGMENT_BIT, frag, tessellationSupported, geometrySupported);
	fragShaderCreateInfo.nextStage = getNextStage(vk::VK_SHADER_STAGE_FRAGMENT_BIT);

	if (m_params.shaders.fragment == LINKED)
	{
		fragShaderCreateInfo.flags = vk::VK_SHADER_CREATE_LINK_STAGE_BIT_EXT;
		shaderCreateInfos.push_back(fragShaderCreateInfo);
	}
	else if (m_params.shaders.fragment == UNLINKED)
	{
		vk.createShadersEXT(device, 1, &fragShaderCreateInfo, DE_NULL, &fragShader);
	}

	vk::VkPrimitiveTopology			primitiveTopology		= m_params.shaders.tesellation_control != UNUSED ?
																vk::VK_PRIMITIVE_TOPOLOGY_PATCH_LIST :
																vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

	if (!shaderCreateInfos.empty())
	{
		std::vector<vk::VkShaderEXT> shaders(shaderCreateInfos.size());
		deUint32 i = 0u;
		deUint32 j = 0u;
		if (m_params.randomOrder && shaderCreateInfos.size() > 1)
		{
			de::Random random(102030);
			i = random.getUint32() % (deUint32)shaders.size();
			do {
				j = random.getUint32() % (deUint32)shaders.size();
			} while (i == j);
			std::swap(shaderCreateInfos[i], shaderCreateInfos[j]);
		}
		if (m_params.separateLinked)
		{
			for (deUint32 k = 0; k < (deUint32)shaders.size(); ++k)
				vk.createShadersEXT(device, 1u, &shaderCreateInfos[k], DE_NULL, &shaders[k]);
		}
		else
		{
			vk.createShadersEXT(device, (deUint32)shaders.size(), &shaderCreateInfos[0], DE_NULL, &shaders[0]);
		}
		if (m_params.randomOrder && shaderCreateInfos.size() > 1)
		{
			std::swap(shaders[i], shaders[j]);
		}
		deUint32 n = 0;
		if (m_params.shaders.vertex == LINKED)
		{
			vertShader = shaders[n++];
		}
		if (m_params.shaders.tesellation_control == LINKED)
		{
			tescShader = shaders[n++];
		}
		if (m_params.shaders.tesellation_evaluation == LINKED)
		{
			teseShader = shaders[n++];
		}
		if (m_params.shaders.geometry == LINKED)
		{
			geomShader = shaders[n++];
		}
		if (m_params.shaders.fragment == LINKED)
		{
			fragShader = shaders[n++];
		}
	}

	const vk::VkCommandPoolCreateInfo	cmdPoolInfo =
	{
		vk::VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,			// sType
		DE_NULL,												// pNext
		vk::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,	// flags
		queueFamilyIndex,										// queuefamilyindex
	};

	const vk::Move<vk::VkCommandPool>	cmdPool		(createCommandPool(vk, device, &cmdPoolInfo));
	const vk::Move<vk::VkCommandBuffer>	cmdBuffer	(allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	// Draw
	vk::beginCommandBuffer(vk, *cmdBuffer, 0u);

	vk::VkImageMemoryBarrier preImageBarrier = vk::makeImageMemoryBarrier(vk::VK_ACCESS_NONE, vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, vk::VK_IMAGE_LAYOUT_UNDEFINED, vk::VK_IMAGE_LAYOUT_GENERAL, **image, subresourceRange);
	vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, (vk::VkDependencyFlags)0u, 0u, (const vk::VkMemoryBarrier*)DE_NULL, 0u, (const vk::VkBufferMemoryBarrier*)DE_NULL, 1u, &preImageBarrier);

	if (m_params.separateBind == SEPARATE)
	{
		vk::VkShaderStageFlagBits vertStage = vk::VK_SHADER_STAGE_VERTEX_BIT;
		vk::VkShaderStageFlagBits tescStage = vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
		vk::VkShaderStageFlagBits teseStage = vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
		vk::VkShaderStageFlagBits geomStage = vk::VK_SHADER_STAGE_GEOMETRY_BIT;
		vk::VkShaderStageFlagBits fragStage = vk::VK_SHADER_STAGE_FRAGMENT_BIT;
		vk::VkShaderEXT bindVertShader = (m_params.shaders.vertex != UNUSED) ? vertShader : VK_NULL_HANDLE;
		vk::VkShaderEXT bindTescShader = (m_params.shaders.tesellation_control != UNUSED) ? tescShader : VK_NULL_HANDLE;
		vk::VkShaderEXT bindTeseShader = (m_params.shaders.tesellation_evaluation!= UNUSED) ? teseShader : VK_NULL_HANDLE;
		vk::VkShaderEXT bindGeomShader = (m_params.shaders.geometry != UNUSED) ? geomShader : VK_NULL_HANDLE;
		vk::VkShaderEXT bindFragShader = (m_params.shaders.fragment != UNUSED) ? fragShader : VK_NULL_HANDLE;
		vk.cmdBindShadersEXT(*cmdBuffer, 1u, &vertStage, &bindVertShader);
		vk.cmdBindShadersEXT(*cmdBuffer, 1u, &tescStage, &bindTescShader);
		vk.cmdBindShadersEXT(*cmdBuffer, 1u, &teseStage, &bindTeseShader);
		vk.cmdBindShadersEXT(*cmdBuffer, 1u, &geomStage, &bindGeomShader);
		vk.cmdBindShadersEXT(*cmdBuffer, 1u, &fragStage, &bindFragShader);
	}
	else if (m_params.separateBind == ONE_LINKED_UNLINKED)
	{
		std::vector<vk::VkShaderStageFlagBits>	separateStages;
		std::vector<vk::VkShaderStageFlagBits>	togetherStages;
		std::vector<vk::VkShaderEXT>			separateShaders;
		std::vector<vk::VkShaderEXT>			togetherShaders;

		bool linkedAdded = false;
		if ((!linkedAdded && m_params.shaders.vertex == LINKED) || m_params.shaders.vertex == UNLINKED)
		{
			togetherStages.push_back(vk::VK_SHADER_STAGE_VERTEX_BIT);
			togetherShaders.push_back(vertShader);
			linkedAdded = true;
		}
		else if (m_params.shaders.vertex == LINKED)
		{
			separateStages.push_back(vk::VK_SHADER_STAGE_VERTEX_BIT);
			togetherShaders.push_back(vertShader);
		}

		if ((!linkedAdded && m_params.shaders.tesellation_control == LINKED) || m_params.shaders.tesellation_control == UNLINKED)
		{
			togetherStages.push_back(vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
			togetherShaders.push_back(tescShader);
			linkedAdded = true;
		}
		else if (m_params.shaders.tesellation_control == LINKED)
		{
			separateStages.push_back(vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
			separateShaders.push_back(tescShader);
		}

		if ((!linkedAdded && m_params.shaders.tesellation_evaluation == LINKED) || m_params.shaders.tesellation_evaluation == UNLINKED)
		{
			togetherStages.push_back(vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
			togetherShaders.push_back(teseShader);
			linkedAdded = true;
		}
		else if (m_params.shaders.tesellation_evaluation == LINKED)
		{
			separateStages.push_back(vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
			separateShaders.push_back(teseShader);
		}

		if ((!linkedAdded && m_params.shaders.geometry == LINKED) || m_params.shaders.geometry == UNLINKED)
		{
			togetherStages.push_back(vk::VK_SHADER_STAGE_GEOMETRY_BIT);
			togetherShaders.push_back(geomShader);
			linkedAdded = true;
		}
		else if (m_params.shaders.geometry == LINKED)
		{
			separateStages.push_back(vk::VK_SHADER_STAGE_GEOMETRY_BIT);
			separateShaders.push_back(geomShader);
		}

		if ((!linkedAdded && m_params.shaders.fragment == LINKED) || m_params.shaders.fragment == UNLINKED)
		{
			togetherStages.push_back(vk::VK_SHADER_STAGE_FRAGMENT_BIT);
			togetherShaders.push_back(fragShader);
			linkedAdded = true;
		}
		else if (m_params.shaders.fragment == LINKED)
		{
			separateStages.push_back(vk::VK_SHADER_STAGE_FRAGMENT_BIT);
			separateShaders.push_back(fragShader);
		}

		vk::bindGraphicsShaders(vk, *cmdBuffer, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, taskSupported, meshSupported);
		if (!togetherShaders.empty())
			vk.cmdBindShadersEXT(*cmdBuffer, (deUint32)togetherShaders.size(), togetherStages.data(), togetherShaders.data());
		if (!separateShaders.empty())
			vk.cmdBindShadersEXT(*cmdBuffer, (deUint32)separateShaders.size(), separateStages.data(), separateShaders.data());
	}
	else
	{
		vk::bindGraphicsShaders(vk, *cmdBuffer,
			m_params.shaders.vertex != UNUSED ? vertShader : VK_NULL_HANDLE,
			m_params.shaders.tesellation_control != UNUSED ? tescShader : VK_NULL_HANDLE,
			m_params.shaders.tesellation_evaluation != UNUSED ? teseShader : VK_NULL_HANDLE,
			m_params.shaders.geometry != UNUSED ? geomShader : VK_NULL_HANDLE,
			m_params.shaders.fragment != UNUSED ? fragShader : VK_NULL_HANDLE,
			taskSupported,
			meshSupported);
	}
	vk::setDefaultShaderObjectDynamicStates(vk, *cmdBuffer, deviceExtensions, primitiveTopology, false);

	bindNullTaskMeshShaders(vk, *cmdBuffer, m_context.getMeshShaderFeaturesEXT());

	const vk::VkClearValue				clearValue = vk::makeClearValueColor({ 0.0f, 0.0f, 0.0f, 1.0f });
	vk::beginRendering(vk, *cmdBuffer, *imageView, renderArea, clearValue, vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_ATTACHMENT_LOAD_OP_CLEAR);
	vk.cmdDraw(*cmdBuffer, 4, 1, 0, 0);
	vk::endRendering(vk, *cmdBuffer);

	vk::VkImageMemoryBarrier postImageBarrier = vk::makeImageMemoryBarrier(vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, vk::VK_ACCESS_TRANSFER_READ_BIT, vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_IMAGE_LAYOUT_GENERAL, **image, subresourceRange);
	vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, (vk::VkDependencyFlags)0u, 0u, (const vk::VkMemoryBarrier*)DE_NULL, 0u, (const vk::VkBufferMemoryBarrier*)DE_NULL, 1u, &postImageBarrier);

	const vk::VkBufferImageCopy	copyRegion =
	{
		0u,																		// VkDeviceSize				bufferOffset;
		0u,																		// deUint32					bufferRowLength;
		0u,																		// deUint32					bufferImageHeight;
		{
			vk::VK_IMAGE_ASPECT_COLOR_BIT,										// VkImageAspectFlags		aspect;
			0u,																	// deUint32					mipLevel;
			0u,																	// deUint32					baseArrayLayer;
			1u,																	// deUint32					layerCount;
		},																		// VkImageSubresourceLayers	imageSubresource;
		{ 0, 0, 0 },															// VkOffset3D				imageOffset;
		{renderArea.extent.width, renderArea.extent.height, 1}					// VkExtent3D				imageExtent;
	};
	vk.cmdCopyImageToBuffer(*cmdBuffer, **image, vk::VK_IMAGE_LAYOUT_GENERAL, **colorOutputBuffer, 1u, &copyRegion);

	vk::endCommandBuffer(vk, *cmdBuffer);

	submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

	// Cleanup
	if (m_params.shaders.vertex != UNUSED)
		vk.destroyShaderEXT(device, vertShader, DE_NULL);

	if (m_params.shaders.tesellation_control != UNUSED)
		vk.destroyShaderEXT(device, tescShader, DE_NULL);

	if (m_params.shaders.tesellation_evaluation != UNUSED)
		vk.destroyShaderEXT(device, teseShader, DE_NULL);

	if (m_params.shaders.geometry != UNUSED)
		vk.destroyShaderEXT(device, geomShader, DE_NULL);

	if (m_params.shaders.fragment != UNUSED)
		vk.destroyShaderEXT(device, fragShader, DE_NULL);

	tcu::ConstPixelBufferAccess resultBuffer = tcu::ConstPixelBufferAccess(vk::mapVkFormat(colorAttachmentFormat), renderArea.extent.width, renderArea.extent.height, 1, (const void*)colorOutputBuffer->getAllocation().getHostPtr());

	const tcu::Vec4			black		= tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
	const tcu::Vec4			white		= tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f);
	const deInt32			width		= resultBuffer.getWidth();
	const deInt32			height		= resultBuffer.getHeight();
	const deInt32			xOffset		= m_params.shaders.tesellation_control != UNUSED ? 4 : 8;
	const deInt32			yOffset		= m_params.shaders.geometry != UNUSED ? 4 : 8;

	for (deInt32 j = 0; j < height; ++j)
	{
		for (deInt32 i = 0; i < width; ++i)
		{
			const tcu::Vec4 color = resultBuffer.getPixel(i, j).asFloat();
			if (i >= xOffset && i < width - xOffset && j >= yOffset && j < height - yOffset)
			{
				if (color != white)
				{
					log << tcu::TestLog::Message << "Color at (" << i << ", " << j << ") is expected to be (1.0, 1.0, 1.0, 1.0), but was (" << color << ")" << tcu::TestLog::EndMessage;
					return tcu::TestStatus::fail("Fail");
				}
			}
			else
			{
				if (color != black)
				{
					log << tcu::TestLog::Message << "Color at (" << i << ", " << j << ") is expected to be (0.0, 0.0, 0.0, 0.0), but was (" << color << ")" << tcu::TestLog::EndMessage;
					return tcu::TestStatus::fail("Fail");
				}
			}
		}
	}

	return tcu::TestStatus::pass("Pass");
}

class ShaderObjectLinkCase : public vkt::TestCase
{
public:
							ShaderObjectLinkCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& params)
													: vkt::TestCase		(testCtx, name, description)
													, m_params			(params)
													{}
	virtual					~ShaderObjectLinkCase	(void) {}

	void					checkSupport			(vkt::Context& context) const override;
	virtual void			initPrograms			(vk::SourceCollections& programCollection) const override;
	TestInstance*			createInstance			(Context& context) const override { return new ShaderObjectLinkInstance(context, m_params); }
private:
	TestParams m_params;
};

void ShaderObjectLinkCase::checkSupport (Context& context) const
{
	context.requireDeviceFunctionality("VK_EXT_shader_object");

	if (m_params.shaders.tesellation_control != UNUSED || m_params.shaders.tesellation_evaluation != UNUSED
		|| (m_params.nextStages.vertNextStage | vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) > 0)
		context.requireDeviceCoreFeature(vkt::DEVICE_CORE_FEATURE_TESSELLATION_SHADER);

	if (m_params.shaders.geometry != UNUSED
		|| (m_params.nextStages.vertNextStage | vk::VK_SHADER_STAGE_GEOMETRY_BIT) > 0
		|| (m_params.nextStages.teseNextStage | vk::VK_SHADER_STAGE_GEOMETRY_BIT) > 0)
		context.requireDeviceCoreFeature(vkt::DEVICE_CORE_FEATURE_GEOMETRY_SHADER);
}

void ShaderObjectLinkCase::initPrograms (vk::SourceCollections& programCollection) const
{
	vk::addBasicShaderObjectShaders(programCollection);
}


class MeshShaderObjectLinkInstance : public vkt::TestInstance
{
public:
							MeshShaderObjectLinkInstance	(Context& context, const MeshParams& params)
															: vkt::TestInstance	(context)
															, m_params			(params)
															{}
	virtual					~MeshShaderObjectLinkInstance	(void) {}

	tcu::TestStatus			iterate							(void) override;
private:
	vk::VkShaderStageFlags	getNextStage					(vk::VkShaderStageFlagBits currentStage);

	MeshParams m_params;
};

vk::VkShaderStageFlags MeshShaderObjectLinkInstance::getNextStage (vk::VkShaderStageFlagBits currentStage)
{
	if (currentStage == vk::VK_SHADER_STAGE_TASK_BIT_EXT)
	{
		if (m_params.shaders.task == LINKED)
			return vk::VK_SHADER_STAGE_MESH_BIT_EXT;
		return m_params.nextStages.taskNextStage;
	}

	if (currentStage == vk::VK_SHADER_STAGE_MESH_BIT_EXT)
	{
		if (m_params.shaders.mesh == LINKED)
			return vk::VK_SHADER_STAGE_FRAGMENT_BIT;
		return m_params.nextStages.meshNextStage;
	}

	return 0;
}

tcu::TestStatus MeshShaderObjectLinkInstance::iterate (void)
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

	de::MovePtr<vk::ImageWithMemory>	image					= de::MovePtr<vk::ImageWithMemory>(new vk::ImageWithMemory(vk, device, alloc, createInfo, vk::MemoryRequirement::Any));
	const auto							imageView				= vk::makeImageView(vk, device, **image, vk::VK_IMAGE_VIEW_TYPE_2D, colorAttachmentFormat, subresourceRange);
	const vk::VkRect2D					renderArea				= vk::makeRect2D(0, 0, 32, 32);

	const vk::VkDeviceSize				colorOutputBufferSize	= renderArea.extent.width * renderArea.extent.height * tcu::getPixelSize(vk::mapVkFormat(colorAttachmentFormat));
	de::MovePtr<vk::BufferWithMemory>	colorOutputBuffer		= de::MovePtr<vk::BufferWithMemory>(new vk::BufferWithMemory(
		vk, device, alloc, makeBufferCreateInfo(colorOutputBufferSize, vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT), vk::MemoryRequirement::HostVisible));

	const vk::Unique<vk::VkDescriptorSetLayout> descriptorSetLayout(
		vk::DescriptorSetLayoutBuilder()
		.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_MESH_BIT_EXT)
		.build(vk, device));

	const vk::Unique<vk::VkDescriptorPool> descriptorPool(
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

	const auto&					binaries			= m_context.getBinaryCollection();
	const auto&					task				= binaries.get("task");
	const auto&					mesh				= binaries.get("mesh");
	const auto&					frag				= binaries.get("frag");

	vk::VkShaderEXT				taskShader;
	vk::VkShaderEXT				meshShader;
	vk::VkShaderEXT				fragShader;

	std::vector<vk::VkShaderCreateInfoEXT> shaderCreateInfos;

	vk::VkShaderCreateInfoEXT taskShaderCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,	// VkStructureType				sType;
		DE_NULL,										// const void*					pNext;
		0u,												// VkShaderCreateFlagsEXT		flags;
		vk::VK_SHADER_STAGE_TASK_BIT_EXT,				// VkShaderStageFlagBits		stage;
		getNextStage(vk::VK_SHADER_STAGE_TASK_BIT_EXT),	// VkShaderStageFlags			nextStage;
		vk::VK_SHADER_CODE_TYPE_SPIRV_EXT,				// VkShaderCodeTypeEXT			codeType;
		task.getSize(),									// size_t						codeSize;
		task.getBinary(),								// const void*					pCode;
		"main",											// const char*					pName;
		1u,												// uint32_t						setLayoutCount;
		&*descriptorSetLayout,							// VkDescriptorSetLayout*		pSetLayouts;
		0u,												// uint32_t						pushConstantRangeCount;
		DE_NULL,										// const VkPushConstantRange*	pPushConstantRanges;
		DE_NULL,										// const VkSpecializationInfo*	pSpecializationInfo;
	};

	if (m_params.shaders.task == LINKED)
	{
		taskShaderCreateInfo.flags = vk::VK_SHADER_CREATE_LINK_STAGE_BIT_EXT;
		shaderCreateInfos.push_back(taskShaderCreateInfo);
	}
	else if (m_params.shaders.task == UNLINKED)
	{
		vk.createShadersEXT(device, 1, &taskShaderCreateInfo, DE_NULL, &taskShader);
	}

	vk::VkShaderCreateInfoEXT meshShaderCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,				// VkStructureType				sType;
		DE_NULL,													// const void*					pNext;
		(m_params.shaders.task == UNUSED) ? (vk::VkShaderCreateFlagsEXT)vk::VK_SHADER_CREATE_NO_TASK_SHADER_BIT_EXT : (vk::VkShaderCreateFlagsEXT)0u,
																	// VkShaderCreateFlagsEXT		flags;
		vk::VK_SHADER_STAGE_MESH_BIT_EXT,							// VkShaderStageFlagBits		stage;
		getNextStage(vk::VK_SHADER_STAGE_MESH_BIT_EXT),				// VkShaderStageFlags			nextStage;
		vk::VK_SHADER_CODE_TYPE_SPIRV_EXT,							// VkShaderCodeTypeEXT			codeType;
		mesh.getSize(),												// size_t						codeSize;
		mesh.getBinary(),											// const void*					pCode;
		"main",														// const char*					pName;
		1u,															// uint32_t						setLayoutCount;
		&*descriptorSetLayout,										// VkDescriptorSetLayout*		pSetLayouts;
		0u,															// uint32_t						pushConstantRangeCount;
		DE_NULL,													// const VkPushConstantRange*	pPushConstantRanges;
		DE_NULL,													// const VkSpecializationInfo*	pSpecializationInfo;
	};

	if (m_params.shaders.mesh == LINKED)
	{
		meshShaderCreateInfo.flags = vk::VK_SHADER_CREATE_LINK_STAGE_BIT_EXT;
		shaderCreateInfos.push_back(meshShaderCreateInfo);
	}
	else if (m_params.shaders.mesh == UNLINKED)
	{
		vk.createShadersEXT(device, 1, &meshShaderCreateInfo, DE_NULL, &meshShader);
	}

	vk::VkShaderCreateInfoEXT fragShaderCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,	// VkStructureType				sType;
		DE_NULL,										// const void*					pNext;
		0u,												// VkShaderCreateFlagsEXT		flags;
		vk::VK_SHADER_STAGE_FRAGMENT_BIT,				// VkShaderStageFlagBits		stage;
		getNextStage(vk::VK_SHADER_STAGE_FRAGMENT_BIT),	// VkShaderStageFlags			nextStage;
		vk::VK_SHADER_CODE_TYPE_SPIRV_EXT,				// VkShaderCodeTypeEXT			codeType;
		frag.getSize(),									// size_t						codeSize;
		frag.getBinary(),								// const void*					pCode;
		"main",											// const char*					pName;
		1u,												// uint32_t						setLayoutCount;
		&*descriptorSetLayout,							// VkDescriptorSetLayout*		pSetLayouts;
		0u,												// uint32_t						pushConstantRangeCount;
		DE_NULL,										// const VkPushConstantRange*	pPushConstantRanges;
		DE_NULL,										// const VkSpecializationInfo*	pSpecializationInfo;
	};

	if (m_params.shaders.fragment == LINKED)
	{
		fragShaderCreateInfo.flags = vk::VK_SHADER_CREATE_LINK_STAGE_BIT_EXT;
		shaderCreateInfos.push_back(fragShaderCreateInfo);
	}
	else if (m_params.shaders.fragment == UNLINKED)
	{
		vk.createShadersEXT(device, 1, &fragShaderCreateInfo, DE_NULL, &fragShader);
	}

	if (!shaderCreateInfos.empty())
	{
		std::vector<vk::VkShaderEXT> shaders(shaderCreateInfos.size());
		deUint32 i = 0u;
		deUint32 j = 0u;
		if (m_params.randomOrder && shaderCreateInfos.size() > 1)
		{
			de::Random random(102030);
			i = random.getUint32() % (deUint32)shaders.size();
			do {
				j = random.getUint32() % (deUint32)shaders.size();
			} while (i == j);
			std::swap(shaderCreateInfos[i], shaderCreateInfos[j]);
		}
		vk.createShadersEXT(device, (deUint32)shaders.size(), &shaderCreateInfos[0], DE_NULL, &shaders[0]);
		if (m_params.randomOrder && shaderCreateInfos.size() > 1)
		{
			std::swap(shaders[i], shaders[j]);
		}
		deUint32 n = 0;
		if (m_params.shaders.task == LINKED)
		{
			taskShader = shaders[n++];
		}
		if (m_params.shaders.mesh == LINKED)
		{
			meshShader = shaders[n++];
		}
		if (m_params.shaders.fragment == LINKED)
		{
			fragShader = shaders[n++];
		}
	}

	const vk::VkCommandPoolCreateInfo	cmdPoolInfo =
	{
		vk::VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,			// sType
		DE_NULL,												// pNext
		vk::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,	// flags
		queueFamilyIndex,										// queuefamilyindex
	};

	const vk::Move<vk::VkCommandPool>	cmdPool		(createCommandPool(vk, device, &cmdPoolInfo));
	const vk::Move<vk::VkCommandBuffer>	cmdBuffer	(allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	// Draw
	vk::beginCommandBuffer(vk, *cmdBuffer);

	vk::VkImageMemoryBarrier preImageBarrier =
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType		sType
		DE_NULL,										// const void*			pNext
		vk::VK_ACCESS_NONE,								// VkAccessFlags		srcAccessMask
		vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,		// VkAccessFlags		dstAccessMask
		vk::VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout		oldLayout
		vk::VK_IMAGE_LAYOUT_GENERAL,					// VkImageLayout		newLayout
		VK_QUEUE_FAMILY_IGNORED,						// uint32_t				srcQueueFamilyIndex
		VK_QUEUE_FAMILY_IGNORED,						// uint32_t				dstQueueFamilyIndex
		**image,										// VkImage				image
		{
			vk::VK_IMAGE_ASPECT_COLOR_BIT,				// VkImageAspectFlags	aspectMask
			0u,											// uint32_t				baseMipLevel
			VK_REMAINING_MIP_LEVELS,					// uint32_t				mipLevels,
			0u,											// uint32_t				baseArray
			VK_REMAINING_ARRAY_LAYERS,					// uint32_t				arraySize
		}
	};

	vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, (vk::VkDependencyFlags)0u, 0u, (const vk::VkMemoryBarrier*)DE_NULL, 0u, (const vk::VkBufferMemoryBarrier*)DE_NULL, 1u, &preImageBarrier);

	const vk::VkClearValue				clearValue = vk::makeClearValueColor({ 0.0f, 0.0f, 0.0f, 1.0f });
	vk::beginRendering(vk, *cmdBuffer, *imageView, renderArea, clearValue, vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_ATTACHMENT_LOAD_OP_CLEAR);
	vk::setDefaultShaderObjectDynamicStates(vk, *cmdBuffer, deviceExtensions, vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, true);
	vk.cmdBindDescriptorSets(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0, 1, &descriptorSet.get(), 0, DE_NULL);

	bindNullRasterizationShaders(vk, *cmdBuffer, m_context.getDeviceFeatures());
	vk::VkShaderStageFlagBits stages[] = {
			vk::VK_SHADER_STAGE_TASK_BIT_EXT,
			vk::VK_SHADER_STAGE_MESH_BIT_EXT,
			vk::VK_SHADER_STAGE_FRAGMENT_BIT,
	};
	vk::VkShaderEXT shaders[] = {
		m_params.shaders.task != UNUSED ? taskShader : VK_NULL_HANDLE,
		m_params.shaders.mesh != UNUSED ? meshShader : VK_NULL_HANDLE,
		m_params.shaders.fragment != UNUSED ? fragShader : VK_NULL_HANDLE,
	};
	vk.cmdBindShadersEXT(*cmdBuffer, 3, stages, shaders);

	vk.cmdDrawMeshTasksEXT(*cmdBuffer, 1, 1, 1);

	vk::endRendering(vk, *cmdBuffer);

	vk::VkImageMemoryBarrier postImageBarrier =
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType		sType
		DE_NULL,										// const void*			pNext
		vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,		// VkAccessFlags		srcAccessMask
		vk::VK_ACCESS_TRANSFER_READ_BIT,				// VkAccessFlags		dstAccessMask
		vk::VK_IMAGE_LAYOUT_GENERAL,					// VkImageLayout		oldLayout
		vk::VK_IMAGE_LAYOUT_GENERAL,					// VkImageLayout		newLayout
		VK_QUEUE_FAMILY_IGNORED,						// uint32_t				srcQueueFamilyIndex
		VK_QUEUE_FAMILY_IGNORED,						// uint32_t				dstQueueFamilyIndex
		**image,										// VkImage				image
		{
			vk::VK_IMAGE_ASPECT_COLOR_BIT,				// VkImageAspectFlags	aspectMask
			0u,											// uint32_t				baseMipLevel
			VK_REMAINING_MIP_LEVELS,					// uint32_t				mipLevels,
			0u,											// uint32_t				baseArray
			VK_REMAINING_ARRAY_LAYERS,					// uint32_t				arraySize
		}
	};

	vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, (vk::VkDependencyFlags)0u, 0u, (const vk::VkMemoryBarrier*)DE_NULL, 0u, (const vk::VkBufferMemoryBarrier*)DE_NULL, 1u, &postImageBarrier);

	const vk::VkBufferImageCopy	copyRegion =
	{
		0u,																		// VkDeviceSize				bufferOffset;
		0u,																		// deUint32					bufferRowLength;
		0u,																		// deUint32					bufferImageHeight;
		{
			vk::VK_IMAGE_ASPECT_COLOR_BIT,										// VkImageAspectFlags		aspect;
			0u,																	// deUint32					mipLevel;
			0u,																	// deUint32					baseArrayLayer;
			1u,																	// deUint32					layerCount;
		},																		// VkImageSubresourceLayers	imageSubresource;
		{ 0, 0, 0 },															// VkOffset3D				imageOffset;
		{renderArea.extent.width, renderArea.extent.height, 1}					// VkExtent3D				imageExtent;
	};
	vk.cmdCopyImageToBuffer(*cmdBuffer, **image, vk::VK_IMAGE_LAYOUT_GENERAL, **colorOutputBuffer, 1u, &copyRegion);

	vk::endCommandBuffer(vk, *cmdBuffer);

	submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

	// Cleanup
	if (m_params.shaders.task != UNUSED)
		vk.destroyShaderEXT(device, taskShader, DE_NULL);

	if (m_params.shaders.mesh != UNUSED)
		vk.destroyShaderEXT(device, meshShader, DE_NULL);

	if (m_params.shaders.fragment != UNUSED)
		vk.destroyShaderEXT(device, fragShader, DE_NULL);

	if (m_params.shaders.fragment != UNUSED)
	{
		tcu::ConstPixelBufferAccess resultBuffer = tcu::ConstPixelBufferAccess(vk::mapVkFormat(colorAttachmentFormat), renderArea.extent.width, renderArea.extent.height, 1, (const void*)colorOutputBuffer->getAllocation().getHostPtr());

		const tcu::Vec4			white = tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f);
		const deInt32			width = resultBuffer.getWidth();
		const deInt32			height = resultBuffer.getHeight();

		for (deInt32 j = 0; j < height; ++j)
		{
			for (deInt32 i = 0; i < width; ++i)
			{
				const tcu::Vec4 color = resultBuffer.getPixel(i, j).asFloat();
				if (color != white)
				{
					log << tcu::TestLog::Message << "Color at (" << i << ", " << j << ") is expected to be (1.0, 1.0, 1.0, 1.0), but was (" << color << ")" << tcu::TestLog::EndMessage;
					return tcu::TestStatus::fail("Fail");
				}
			}
		}
	}
	if (m_params.shaders.mesh != UNUSED)
	{
		const vk::Allocation& outputBufferAllocation = outputBuffer.getAllocation();
		invalidateAlloc(vk, device, outputBufferAllocation);

		const deUint32* bufferPtr = static_cast<deUint32*>(outputBufferAllocation.getHostPtr());

		if (bufferPtr[0] != 0u || bufferPtr[1] != 1u || bufferPtr[2] != 2u || bufferPtr[3] != 3u)
		{
			log << tcu::TestLog::Message << "Buffer values were expected to be [0, 1, 2, 3], but were[" << bufferPtr[0] << ", " << bufferPtr[1] << ", " << bufferPtr[2] << ", " << bufferPtr[3] << ", " << "]" << tcu::TestLog::EndMessage;
			return tcu::TestStatus::fail("Fail");
		}
	}

	return tcu::TestStatus::pass("Pass");
}

class MeshShaderObjectLinkCase : public vkt::TestCase
{
public:
					MeshShaderObjectLinkCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const MeshParams& params)
												: vkt::TestCase		(testCtx, name, description)
												, m_params			(params)
												{}
	virtual			~MeshShaderObjectLinkCase	(void) {}

	void			checkSupport				(vkt::Context& context) const override;
	virtual void	initPrograms				(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance				(Context& context) const override { return new MeshShaderObjectLinkInstance(context, m_params); }
private:
	MeshParams m_params;
};

void MeshShaderObjectLinkCase::initPrograms (vk::SourceCollections& programCollection) const
{
	std::stringstream task;
	std::stringstream mesh;
	std::stringstream frag;

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

	frag
		<< "#version 450\n"
		<< "layout (location=0) out vec4 outColor;\n"
		<< "void main() {\n"
		<< "    outColor = vec4(1.0f);\n"
		<< "}\n";

	programCollection.glslSources.add("task") << glu::TaskSource(task.str()) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
	programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
	programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

void MeshShaderObjectLinkCase::checkSupport (Context& context) const
{
	context.requireDeviceFunctionality("VK_EXT_shader_object");

	context.requireDeviceFunctionality("VK_EXT_mesh_shader");
	const auto& features = context.getMeshShaderFeaturesEXT();
	if (!features.taskShader)
		TCU_THROW(NotSupportedError, "Task shaders not supported");
	if (!features.meshShader)
		TCU_THROW(NotSupportedError, "Mesh shaders not supported");
}

std::string typeToString(ShaderType type) {
	if (type == UNUSED)
		return "unused";

	if (type == LINKED)
		return "linked";

	if (type == UNLINKED)
		return "unlinked";

	return {};
}

}

tcu::TestCaseGroup* createShaderObjectLinkTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> linkGroup(new tcu::TestCaseGroup(testCtx, "link", ""));

	const Shaders shaderTests[] =
	{
		{ LINKED, LINKED, UNLINKED, UNUSED, UNLINKED,		},
		{ LINKED, LINKED, LINKED, UNUSED, UNLINKED,			},
		{ LINKED, LINKED, LINKED, LINKED, UNLINKED,			},
		{ LINKED, LINKED, LINKED, LINKED, LINKED,			},
		{ LINKED, UNUSED, UNUSED, LINKED, UNLINKED,			},
		{ LINKED, UNUSED, UNUSED, LINKED, LINKED,			},
		{ LINKED, UNUSED, UNUSED, UNUSED, LINKED,			},
		{ UNLINKED, UNLINKED, UNLINKED, UNUSED, UNLINKED	},
		{ UNLINKED, UNUSED, UNUSED, UNLINKED, UNLINKED		},
		{ UNLINKED, UNUSED, UNUSED, UNUSED, UNLINKED		},
		{ UNLINKED, LINKED, LINKED, UNUSED, UNLINKED		},
		{ UNLINKED, LINKED, LINKED, LINKED, UNLINKED		},
		{ UNLINKED, LINKED, LINKED, UNUSED, LINKED			},
		{ UNLINKED, LINKED, LINKED, LINKED, LINKED			},
		{ UNLINKED, UNUSED, UNUSED, LINKED, LINKED			},
	};

	const bool randomOrderTests[] =
	{
		false,
		true,
	};

	const struct
	{
		BindType bindType;
		const char* name;
	} bindTypeTests[] =
	{
		{ SEPARATE,				"separate"				},
		{ ONE_LINKED_UNLINKED,	"one_linked_unlinked"	},
		{ ALL,					"all"					},
	};

	for (const auto& shaders : shaderTests)
	{
		std::string shadersName = "";
		shadersName += typeToString(shaders.vertex) + "_";
		shadersName += typeToString(shaders.tesellation_control) + "_";
		shadersName += typeToString(shaders.tesellation_evaluation) + "_";
		shadersName += typeToString(shaders.geometry) + "_";
		shadersName += typeToString(shaders.fragment);
		de::MovePtr<tcu::TestCaseGroup> shadersGroup(new tcu::TestCaseGroup(testCtx, shadersName.c_str(), ""));

		for (const auto& bindType : bindTypeTests)
		{
			de::MovePtr<tcu::TestCaseGroup> bindGroup(new tcu::TestCaseGroup(testCtx, bindType.name, ""));
			for (const auto& randomOrder : randomOrderTests)
			{
				NextStages nextStages = {};
				if (shaders.tesellation_control != UNUSED) {
					nextStages.vertNextStage |= vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
					nextStages.tescNextStage |= vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
				}
				if (shaders.geometry != UNUSED) {
					nextStages.vertNextStage |= vk::VK_SHADER_STAGE_GEOMETRY_BIT;
					nextStages.teseNextStage |= vk::VK_SHADER_STAGE_GEOMETRY_BIT;
				}
				if (shaders.fragment != UNUSED) {
					nextStages.vertNextStage |= vk::VK_SHADER_STAGE_FRAGMENT_BIT;
					nextStages.teseNextStage |= vk::VK_SHADER_STAGE_FRAGMENT_BIT;
					nextStages.geomNextStage |= vk::VK_SHADER_STAGE_FRAGMENT_BIT;
				}

				TestParams params = {
					shaders,				// Shaders		shaders;
					randomOrder,			// bool			randomOrder;
					nextStages,				// NextStages	nextStages;
					false,					// bool			separateLinked;
					bindType.bindType,		// bool			separateBind;
				};

				std::string randomOrderName = randomOrder ? "random_order" : "default";

				bindGroup->addChild(new ShaderObjectLinkCase(testCtx, randomOrderName, "", params));
			}

			if (shaders.vertex == LINKED || shaders.tesellation_control == LINKED || shaders.tesellation_evaluation == LINKED || shaders.geometry == LINKED || shaders.fragment == LINKED)
			{
				TestParams params = {
					shaders,																												// Shaders		shaders;
					false,																													// bool			randomOrder;
					{
						vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | vk::VK_SHADER_STAGE_GEOMETRY_BIT | vk::VK_SHADER_STAGE_FRAGMENT_BIT,
						vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
						vk::VK_SHADER_STAGE_GEOMETRY_BIT | vk::VK_SHADER_STAGE_FRAGMENT_BIT,
						vk::VK_SHADER_STAGE_FRAGMENT_BIT, },																				// NextStages	nextStages;
					true,																													// bool         separateLinked;
					ALL,																													// BindType		separateBind
				};

				bindGroup->addChild(new ShaderObjectLinkCase(testCtx, "separate_link", "", params));
			}
			shadersGroup->addChild(bindGroup.release());
		}
		linkGroup->addChild(shadersGroup.release());
	}

	const struct
	{
		Shaders		shaders;
		NextStages	nextStages;
		const char*	name;
	} nextStageTests[] =
	{
		{
			{ UNLINKED, UNUSED, UNUSED, UNUSED, UNLINKED },
			{ vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | vk::VK_SHADER_STAGE_FRAGMENT_BIT, vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, vk::VK_SHADER_STAGE_FRAGMENT_BIT, 0u, },
			"vert_t"
		},
		{
			{ UNLINKED, UNUSED, UNUSED, UNLINKED, UNLINKED	},
			{ vk::VK_SHADER_STAGE_GEOMETRY_BIT, 0u, 0u, vk::VK_SHADER_STAGE_FRAGMENT_BIT, },
			"vert_g"
		},
		{
			{ UNLINKED, UNLINKED, UNLINKED, UNLINKED, UNLINKED	},
			{ vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | vk::VK_SHADER_STAGE_GEOMETRY_BIT, vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, vk::VK_SHADER_STAGE_GEOMETRY_BIT, vk::VK_SHADER_STAGE_FRAGMENT_BIT, },
			"vert_tg"
		},
		{
			{ UNLINKED, UNUSED, UNUSED, UNUSED, UNLINKED	},
			{ vk::VK_SHADER_STAGE_FRAGMENT_BIT, 0u, 0u, 0u, },
			"vert_f"
		},
		{
			{ UNLINKED, UNLINKED, UNLINKED, UNUSED, UNLINKED	},
			{ vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | vk::VK_SHADER_STAGE_FRAGMENT_BIT, vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, vk::VK_SHADER_STAGE_FRAGMENT_BIT, 0u, },
			"vert_tf"
		},
		{
			{ UNLINKED, UNUSED, UNUSED, UNLINKED, UNLINKED	},
			{ vk::VK_SHADER_STAGE_GEOMETRY_BIT | vk::VK_SHADER_STAGE_FRAGMENT_BIT, 0u, 0u, vk::VK_SHADER_STAGE_FRAGMENT_BIT, },
			"vert_gf"
		},
		{
			{ UNLINKED, UNLINKED, UNLINKED, UNLINKED, UNLINKED	},
			{ vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | vk::VK_SHADER_STAGE_GEOMETRY_BIT | vk::VK_SHADER_STAGE_FRAGMENT_BIT, vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, vk::VK_SHADER_STAGE_GEOMETRY_BIT, vk::VK_SHADER_STAGE_FRAGMENT_BIT, },
			"vert_tgf"
		},
		{
			{ UNLINKED, UNLINKED, UNLINKED, UNUSED, UNLINKED	},
			{ vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, vk::VK_SHADER_STAGE_FRAGMENT_BIT, 0u, },
			"tesc_t"
		},
		{
			{ UNLINKED, UNLINKED, UNLINKED, UNLINKED, UNLINKED	},
			{ vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, vk::VK_SHADER_STAGE_GEOMETRY_BIT, vk::VK_SHADER_STAGE_FRAGMENT_BIT, },
			"tese_g"
		},
		{
			{ UNLINKED, UNLINKED, UNLINKED, UNUSED, UNLINKED	},
			{ vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, vk::VK_SHADER_STAGE_FRAGMENT_BIT, 0u, },
			"tese_f"
		},
		{
			{ UNLINKED, UNLINKED, UNLINKED, UNLINKED, UNLINKED	},
			{ vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, vk::VK_SHADER_STAGE_GEOMETRY_BIT | vk::VK_SHADER_STAGE_FRAGMENT_BIT, vk::VK_SHADER_STAGE_FRAGMENT_BIT, },
			"tese_gf"
		},
		{
			{ UNLINKED, UNUSED, UNUSED, UNLINKED, UNLINKED	},
			{ vk::VK_SHADER_STAGE_GEOMETRY_BIT, 0u, 0u, vk::VK_SHADER_STAGE_FRAGMENT_BIT, },
			"geom_f"
		},
	};

	de::MovePtr<tcu::TestCaseGroup> nextStageGroup(new tcu::TestCaseGroup(testCtx, "next_stage", ""));
	for (const auto& nextStage : nextStageTests)
	{
		TestParams params = {
			nextStage.shaders,
			false,
			nextStage.nextStages,
			false,
			ALL,
		};
		nextStageGroup->addChild(new ShaderObjectLinkCase(testCtx, nextStage.name, "", params));
	}
	linkGroup->addChild(nextStageGroup.release());

	const MeshShaders meshShaderTests[] =
	{
		{ UNLINKED, UNLINKED, UNLINKED,		},
		{ UNLINKED, UNLINKED, UNUSED,		},
		{ LINKED, LINKED, UNLINKED,			},
		{ UNLINKED, LINKED, LINKED,			},
		{ LINKED, LINKED, LINKED,			},
	};

	for (const auto& meshShaders : meshShaderTests)
	{
		std::string name = "mesh_";
		name += typeToString(meshShaders.task) + "_";
		name += typeToString(meshShaders.mesh) + "_";
		name += typeToString(meshShaders.fragment);
		de::MovePtr<tcu::TestCaseGroup> meshGroup(new tcu::TestCaseGroup(testCtx, name.c_str(), ""));

		for (const auto& randomOrder : randomOrderTests)
		{
			MeshParams params = {
				meshShaders,
				randomOrder,
				{ 0u, 0u, },
			};

			std::string randomOrderName = (randomOrder) ? "random_order" : "default";

			meshGroup->addChild(new MeshShaderObjectLinkCase(testCtx, randomOrderName, "", params));
		}
		linkGroup->addChild(meshGroup.release());
	}

	const struct
	{
		MeshNextStages nextStages;
		const char* name;
	} meshNextStageTests[] =
	{
		{ { vk::VK_SHADER_STAGE_MESH_BIT_EXT, 0u, },	"mesh"	},
		{ { 0u, vk::VK_SHADER_STAGE_FRAGMENT_BIT, },	"frag"	},
	};

	de::MovePtr<tcu::TestCaseGroup> meshNextStageGroup(new tcu::TestCaseGroup(testCtx, "meshnext_stage", ""));
	for (const auto& meshNextStage : meshNextStageTests)
	{
		MeshParams params = {
			{ UNLINKED, UNLINKED, UNLINKED },
			false,
			meshNextStage.nextStages,
		};
		meshNextStageGroup->addChild(new MeshShaderObjectLinkCase(testCtx, meshNextStage.name, "", params));
	}
	linkGroup->addChild(meshNextStageGroup.release());

	return linkGroup.release();
}

} // ShaderObject
} // vkt
