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
 * \brief Shader Object Misc Tests
 *//*--------------------------------------------------------------------*/

#include "vktShaderObjectMiscTests.hpp"
#include "deUniquePtr.hpp"
#include "tcuTestCase.hpp"
#include "vktTestCase.hpp"
#include "vkShaderObjectUtil.hpp"
#include "vktShaderObjectCreateUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkTypeUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "deMath.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "tcuCommandLine.hpp"
#include "tcuTextureUtil.hpp"

namespace vkt
{
namespace ShaderObject
{

namespace
{

struct TestParams {
	bool blendEnabled[2];
	bool vertexInputBefore;
	bool vertexBuffersNullStride;
	deUint32 stride;
	bool destroyDescriptorSetLayout;
};

vk::VkFormat findDSFormat (const vk::InstanceInterface& vki, const vk::VkPhysicalDevice physicalDevice)
{
	const vk::VkFormat dsFormats[] = {
		vk::VK_FORMAT_D24_UNORM_S8_UINT,
		vk::VK_FORMAT_D32_SFLOAT_S8_UINT,
		vk::VK_FORMAT_D16_UNORM_S8_UINT,
	};

	for (deUint32 i = 0; i < 3; ++i) {
		const vk::VkFormatProperties	formatProperties = getPhysicalDeviceFormatProperties(vki, physicalDevice, dsFormats[i]);
		if ((formatProperties.optimalTilingFeatures & vk::VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)
			return dsFormats[i];
	}
	return vk::VK_FORMAT_UNDEFINED;
}

class ShaderObjectMiscInstance : public vkt::TestInstance
{
public:
							ShaderObjectMiscInstance	(Context& context, const TestParams& params)
														: vkt::TestInstance	(context)
														, m_params			(params)
														{}
	virtual					~ShaderObjectMiscInstance	(void) {}

	tcu::TestStatus			iterate						(void) override;
private:
	void					setVertexInput				(const vk::DeviceInterface& vk, vk::VkCommandBuffer cmdBuffer, vk::VkDeviceSize stride) const;
	void					bindVertexBuffers			(const vk::DeviceInterface& vk, vk::VkCommandBuffer cmdBuffer, vk::VkDeviceSize* stride, vk::VkBuffer buffer, vk::VkDeviceSize bufferSize) const;
	TestParams m_params;
};

void ShaderObjectMiscInstance::setVertexInput (const vk::DeviceInterface& vk, vk::VkCommandBuffer cmdBuffer, vk::VkDeviceSize stride) const
{
	vk::VkVertexInputBindingDescription2EXT bindingDescription = vk::initVulkanStructure();
	bindingDescription.binding = 0u;
	bindingDescription.stride = (deUint32)stride;
	bindingDescription.inputRate = vk::VK_VERTEX_INPUT_RATE_VERTEX;
	bindingDescription.divisor = 1u;
	vk::VkVertexInputAttributeDescription2EXT attributeDescription = vk::initVulkanStructure();
	attributeDescription.location = 0u;
	attributeDescription.binding = 0u;
	attributeDescription.format = vk::VK_FORMAT_R32G32B32A32_SFLOAT;
	attributeDescription.offset = 0u;
	vk.cmdSetVertexInputEXT(cmdBuffer, 1u, &bindingDescription, 1u, &attributeDescription);
}

void ShaderObjectMiscInstance::bindVertexBuffers (const vk::DeviceInterface& vk, vk::VkCommandBuffer cmdBuffer, vk::VkDeviceSize* stride, vk::VkBuffer buffer, vk::VkDeviceSize bufferSize) const
{
	vk::VkDeviceSize offset = 0u;
	vk.cmdBindVertexBuffers2(cmdBuffer, 0u, 1u, &buffer, &offset, &bufferSize, stride);
}

tcu::TestStatus ShaderObjectMiscInstance::iterate (void)
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
	const auto							subresourceRange			= vk::makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
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

	const deUint32 colorAttachmentCount = 2;

	std::vector<de::MovePtr<vk::ImageWithMemory>>	images		(colorAttachmentCount);
	std::vector<vk::Move<vk::VkImageView>>			imageViews	(colorAttachmentCount);
	for (deUint32 i = 0; i < colorAttachmentCount; ++i)
	{
		images[i] = de::MovePtr<vk::ImageWithMemory>(new vk::ImageWithMemory(vk, device, alloc, createInfo, vk::MemoryRequirement::Any));
		imageViews[i] = vk::makeImageView(vk, device, **images[i], vk::VK_IMAGE_VIEW_TYPE_2D, colorAttachmentFormat, subresourceRange);
	}

	const vk::VkDeviceSize				colorOutputBufferSize	= renderArea.extent.width * renderArea.extent.height * tcu::getPixelSize(vk::mapVkFormat(colorAttachmentFormat));
	std::vector<de::MovePtr<vk::BufferWithMemory>>	colorOutputBuffers	(colorAttachmentCount);
	for (deUint32 i = 0; i < colorAttachmentCount; ++i)
	{
		colorOutputBuffers[i] = de::MovePtr<vk::BufferWithMemory>(new vk::BufferWithMemory(
								vk, device, alloc, makeBufferCreateInfo(colorOutputBufferSize, vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT), vk::MemoryRequirement::HostVisible));
	}

	const vk::Move<vk::VkCommandPool>	cmdPool					(vk::createCommandPool(vk, device, 0u, queueFamilyIndex));
	const vk::Move<vk::VkCommandBuffer>	cmdBuffer				(vk::allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	vk::Move<vk::VkDescriptorSetLayout> descriptorSetLayout(
		vk::DescriptorSetLayoutBuilder()
		.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_FRAGMENT_BIT)
		.build(vk, device));

	const vk::Unique<vk::VkDescriptorPool>	descriptorPool(
		vk::DescriptorPoolBuilder()
		.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.build(vk, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	const vk::VkDeviceSize					bufferSizeBytes = sizeof(tcu::Vec4);
	const vk::Unique<vk::VkDescriptorSet>	descriptorSet	(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));
	const vk::BufferWithMemory				inputBuffer		(vk, device, alloc, vk::makeBufferCreateInfo(bufferSizeBytes, vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), vk::MemoryRequirement::HostVisible);

	const vk::VkDescriptorBufferInfo		descriptorInfo = vk::makeDescriptorBufferInfo(*inputBuffer, 0ull, bufferSizeBytes);
	vk::DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorInfo)
		.update(vk, device);
	const auto								pipelineLayout = makePipelineLayout(vk, device, *descriptorSetLayout);

	float* inputDataPtr = reinterpret_cast<float*>(inputBuffer.getAllocation().getHostPtr());
	memset(inputDataPtr, 0, bufferSizeBytes);
	for (deUint32 i = 0; i < 4; ++i)
		inputDataPtr[i] = 0.5f;
	flushAlloc(vk, device, inputBuffer.getAllocation());

	const auto&							binaries				= m_context.getBinaryCollection();
	const auto							vertShader				= vk::createShader(vk, device, vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_VERTEX_BIT, binaries.get("inputVert"), tessellationSupported, geometrySupported, &*descriptorSetLayout));
	const auto							fragShader				= vk::createShader(vk, device, vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_FRAGMENT_BIT, binaries.get("multiFrag"), tessellationSupported, geometrySupported, &*descriptorSetLayout));

	const vk::VkClearValue				clearValue = vk::makeClearValueColor({ 0.0f, 0.0f, 0.0f, 0.0f });
	vk::beginCommandBuffer(vk, *cmdBuffer);

	for (deUint32 i = 0; i < colorAttachmentCount; ++i)
	{
		vk::VkImageMemoryBarrier preImageBarrier = vk::makeImageMemoryBarrier(vk::VK_ACCESS_NONE, vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, vk::VK_IMAGE_LAYOUT_UNDEFINED, vk::VK_IMAGE_LAYOUT_GENERAL, **images[i], subresourceRange);
		vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, (vk::VkDependencyFlags)0u, 0u, (const vk::VkMemoryBarrier*)DE_NULL, 0u, (const vk::VkBufferMemoryBarrier*)DE_NULL, 1u, &preImageBarrier);
	}

	std::vector<vk::VkRenderingAttachmentInfoKHR> colorAttachments(colorAttachmentCount);
	vk::VkRenderingAttachmentInfoKHR colorAttachment
	{
		vk::VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,	// VkStructureType						sType;
		DE_NULL,												// const void*							pNext;
		VK_NULL_HANDLE,											// VkImageView							imageView;
		vk::VK_IMAGE_LAYOUT_GENERAL,							// VkImageLayout						imageLayout;
		vk::VK_RESOLVE_MODE_NONE,								// VkResolveModeFlagBits				resolveMode;
		DE_NULL,												// VkImageView							resolveImageView;
		vk::VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout						resolveImageLayout;
		vk::VK_ATTACHMENT_LOAD_OP_CLEAR,						// VkAttachmentLoadOp					loadOp;
		vk::VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp					storeOp;
		clearValue												// VkClearValue							clearValue;
	};

	for (deUint32 i = 0; i < colorAttachmentCount; ++i)
	{
		colorAttachment.imageView = *imageViews[i];
		colorAttachments[i] = colorAttachment;
	}

	vk::VkRenderingInfoKHR renderingInfo
	{
		vk::VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
		DE_NULL,
		(vk::VkRenderingFlags)0u,								// VkRenderingFlagsKHR					flags;
		renderArea,												// VkRect2D								renderArea;
		1u,														// deUint32								layerCount;
		0x0,													// deUint32								viewMask;
		(deUint32)colorAttachments.size(),						// deUint32								colorAttachmentCount;
		colorAttachments.data(),								// const VkRenderingAttachmentInfoKHR*	pColorAttachments;
		DE_NULL,												// const VkRenderingAttachmentInfoKHR*	pDepthAttachment;
		DE_NULL,												// const VkRenderingAttachmentInfoKHR*	pStencilAttachment;
	};

	const vk::VkDeviceSize				bufferSize	= 1024;
	de::MovePtr<vk::BufferWithMemory>	buffer		= de::MovePtr<vk::BufferWithMemory>(new vk::BufferWithMemory(
		vk, device, alloc, vk::makeBufferCreateInfo(bufferSize, vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), vk::MemoryRequirement::HostVisible));
	float* dataPtr = reinterpret_cast<float*>(buffer->getAllocation().getHostPtr());
	memset(dataPtr, 0, bufferSize);
	for (deUint32 i = 0; i < 4; ++i)
	{
		dataPtr[i * (m_params.stride / sizeof(float)) + 0] = float(i & 1);
		dataPtr[i * (m_params.stride / sizeof(float)) + 1] = float((i >> 1) & 1);
		dataPtr[i * (m_params.stride / sizeof(float)) + 2] = 0.0f;
		dataPtr[i * (m_params.stride / sizeof(float)) + 3] = 1.0f;
	}
	flushAlloc(vk, device, buffer->getAllocation());

	vk::Move<vk::VkDescriptorSetLayout> null;
	if (m_params.destroyDescriptorSetLayout)
		descriptorSetLayout = null;

	vk.cmdBeginRendering(*cmdBuffer, &renderingInfo);
	vk::setDefaultShaderObjectDynamicStates(vk, *cmdBuffer, deviceExtensions, vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, false);

	vk::VkColorBlendEquationEXT		colorBlendEquation = {
		vk::VK_BLEND_FACTOR_ONE,				// VkBlendFactor	srcColorBlendFactor;
		vk::VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,// VkBlendFactor	dstColorBlendFactor;
		vk::VK_BLEND_OP_ADD,					// VkBlendOp		colorBlendOp;
		vk::VK_BLEND_FACTOR_ONE,				// VkBlendFactor	srcAlphaBlendFactor;
		vk::VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,// VkBlendFactor	dstAlphaBlendFactor;
		vk::VK_BLEND_OP_ADD,					// VkBlendOp		alphaBlendOp;
	};
	vk::VkColorComponentFlags		colorWriteMask = vk::VK_COLOR_COMPONENT_R_BIT | vk::VK_COLOR_COMPONENT_G_BIT | vk::VK_COLOR_COMPONENT_B_BIT | vk::VK_COLOR_COMPONENT_A_BIT;
	for (deUint32 i = 0; i < colorAttachmentCount; ++i)
	{
		vk::VkBool32 colorBlendEnable = m_params.blendEnabled[i] ? VK_TRUE : VK_FALSE;
		vk.cmdSetColorBlendEnableEXT(*cmdBuffer, i, 1u, &colorBlendEnable);
		if (m_params.blendEnabled[i])
		{
			vk.cmdSetColorBlendEquationEXT(*cmdBuffer, i, 1u, &colorBlendEquation);
		}
		vk.cmdSetColorWriteMaskEXT(*cmdBuffer, i, 1u, &colorWriteMask);
	}
	const vk::VkPhysicalDeviceProperties properties = vk::getPhysicalDeviceProperties(instanceDriver, m_context.getPhysicalDevice());
	std::vector<vk::VkBool32> colorWriteEnables(properties.limits.maxColorAttachments);
	for (deUint32 i = 0; i < properties.limits.maxColorAttachments; ++i)
	{
		colorWriteEnables[i] = i < colorAttachmentCount ? VK_TRUE : VK_FALSE;
	}
	vk.cmdSetColorWriteEnableEXT(*cmdBuffer, properties.limits.maxColorAttachments, colorWriteEnables.data());

	if (m_params.vertexInputBefore)
		setVertexInput(vk, *cmdBuffer, m_params.vertexBuffersNullStride ? m_params.stride : 100);

	vk::VkDeviceSize stride = m_params.stride;
	vk::VkDeviceSize* pStride = m_params.vertexBuffersNullStride ? DE_NULL : &stride;
	bindVertexBuffers(vk, *cmdBuffer, pStride, **buffer, bufferSize);

	if (!m_params.vertexInputBefore)
		setVertexInput(vk, *cmdBuffer, m_params.stride);

	vk.cmdBindDescriptorSets(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0, 1, &descriptorSet.get(), 0, DE_NULL);

	vk::bindGraphicsShaders(vk, *cmdBuffer, *vertShader, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, *fragShader, taskSupported, meshSupported);
	vk.cmdDraw(*cmdBuffer, 4, 1, 0, 0);
	vk.cmdDraw(*cmdBuffer, 4, 1, 0, 0);

	vk::endRendering(vk, *cmdBuffer);

	for (deUint32 i = 0; i < colorAttachmentCount; ++i)
	{
		vk::VkImageMemoryBarrier postImageBarrier = vk::makeImageMemoryBarrier(vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, vk::VK_ACCESS_TRANSFER_READ_BIT, vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_IMAGE_LAYOUT_GENERAL, **images[i], subresourceRange);
		vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, (vk::VkDependencyFlags)0u, 0u, (const vk::VkMemoryBarrier*)DE_NULL, 0u, (const vk::VkBufferMemoryBarrier*)DE_NULL, 1u, &postImageBarrier);
	}

	const vk::VkBufferImageCopy	copyRegion = vk::makeBufferImageCopy(extent, subresourceLayers);
	for (deUint32 i = 0; i < colorAttachmentCount; ++i)
		vk.cmdCopyImageToBuffer(*cmdBuffer, **images[i], vk::VK_IMAGE_LAYOUT_GENERAL, **colorOutputBuffers[i], 1u, &copyRegion);

	vk::endCommandBuffer(vk, *cmdBuffer);

	vk::submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	const deInt32			width			= renderArea.extent.width;
	const deInt32			height			= renderArea.extent.height;
	const float				threshold		= 1.0f / 256.0f;
	const deInt32			xOffset			= width / 8;
	const deInt32			yOffset			= height / 8;
	const tcu::Vec4			refColor1		= tcu::Vec4(0.75f, 0.75f, 0.75f, 0.75f);
	const tcu::Vec4			refColor2		= tcu::Vec4(0.5f, 0.5f, 0.5f, 0.5f);
	const tcu::Vec4			blackColor		= tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f);

	for (deUint32 k = 0; k < colorAttachmentCount; ++k)
	{
		tcu::ConstPixelBufferAccess resultBuffer = tcu::ConstPixelBufferAccess(vk::mapVkFormat(colorAttachmentFormat), renderArea.extent.width, renderArea.extent.height, 1, (const void*)colorOutputBuffers[k]->getAllocation().getHostPtr());
		for (deInt32 j = 0; j < height; ++j)
		{
			for (deInt32 i = 0; i < width; ++i)
			{
				const tcu::Vec4 color = resultBuffer.getPixel(i, j).asFloat();

				tcu::Vec4 expectedColor = blackColor;
				if (i >= xOffset && i < width - xOffset && j >= yOffset && j < height - yOffset)
				{
					if (m_params.blendEnabled[k])
						expectedColor = refColor1;
					else
						expectedColor = refColor2;
				}

				if (deFloatAbs(color.x() - expectedColor.x()) > threshold || deFloatAbs(color.y() - expectedColor.y()) > threshold || deFloatAbs(color.z() - expectedColor.z()) > threshold || deFloatAbs(color.w() - expectedColor.w()) > threshold)
				{
					log << tcu::TestLog::Message << "Color at (" << i << ", " << j << ") was " << color << ", but expected color was " << expectedColor << tcu::TestLog::EndMessage;
					return tcu::TestStatus::fail("Fail");
				}
			}
		}
	}

	return tcu::TestStatus::pass("Pass");
}

class ShaderObjectMiscCase : public vkt::TestCase
{
public:
							ShaderObjectMiscCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& params)
													: vkt::TestCase		(testCtx, name, description)
													, m_params			(params)
													{}
	virtual					~ShaderObjectMiscCase	(void) {}

	void					checkSupport			(vkt::Context& context) const override;
	virtual void			initPrograms			(vk::SourceCollections& programCollection) const override;
	TestInstance*			createInstance			(Context& context) const override { return new ShaderObjectMiscInstance(context, m_params); }
private:
	TestParams m_params;
};

void ShaderObjectMiscCase::checkSupport (Context& context) const
{
	context.requireDeviceFunctionality("VK_EXT_shader_object");
}

void ShaderObjectMiscCase::initPrograms (vk::SourceCollections& programCollection) const
{
	std::stringstream inputVert;
	std::stringstream multiFrag;

	inputVert
		<< "#version 450\n"
		<< "layout(location = 0) in vec4 inPos;\n"
		<< "void main() {\n"
		<< "    gl_Position = vec4((inPos.xy - 0.5f) * 1.5f, inPos.zw);\n"
		<< "}\n";

	multiFrag
		<< "#version 450\n"
		<< "layout(set=0, binding=0) readonly buffer inputBuf {\n"
		<< "    vec4 color;\n"
		<< "};\n"
		<< "layout (location=0) out vec4 outColor0;\n"
		<< "layout (location=1) out vec4 outColor1;\n"
		<< "void main() {\n"
		<< "    outColor0 = color;\n"
		<< "    outColor1 = color;\n"
		<< "}\n";

	programCollection.glslSources.add("inputVert") << glu::VertexSource(inputVert.str());
	programCollection.glslSources.add("multiFrag") << glu::FragmentSource(multiFrag.str());
}

de::MovePtr<tcu::TextureLevel> readDepthAttachment (const vk::DeviceInterface&	vk,
													vk::VkDevice				device,
													vk::VkQueue					queue,
													deUint32					queueFamilyIndex,
													vk::Allocator&				allocator,
													vk::VkImage					image,
													vk::VkFormat				format,
													const tcu::UVec2&			renderSize,
													vk::VkImageLayout			currentLayout)
{
	vk::Move<vk::VkBuffer>					buffer;
	de::MovePtr<vk::Allocation>				bufferAlloc;
	vk::Move<vk::VkCommandPool>				cmdPool;
	vk::Move<vk::VkCommandBuffer>			cmdBuffer;

	tcu::TextureFormat				retFormat		(tcu::TextureFormat::D, tcu::TextureFormat::CHANNELTYPE_LAST);
	tcu::TextureFormat				bufferFormat	(tcu::TextureFormat::D, tcu::TextureFormat::CHANNELTYPE_LAST);
	const vk::VkImageAspectFlags	barrierAspect	= vk::VK_IMAGE_ASPECT_DEPTH_BIT | (mapVkFormat(format).order == tcu::TextureFormat::DS ? vk::VK_IMAGE_ASPECT_STENCIL_BIT : (vk::VkImageAspectFlagBits)0);

	switch (format)
	{
	case vk::VK_FORMAT_D16_UNORM:
	case vk::VK_FORMAT_D16_UNORM_S8_UINT:
		bufferFormat.type = retFormat.type = tcu::TextureFormat::UNORM_INT16;
		break;
	case vk::VK_FORMAT_D24_UNORM_S8_UINT:
	case vk::VK_FORMAT_X8_D24_UNORM_PACK32:
		retFormat.type = tcu::TextureFormat::UNORM_INT24;
		// vkCmdCopyBufferToImage copies D24 data to 32-bit pixels.
		bufferFormat.type = tcu::TextureFormat::UNSIGNED_INT_24_8_REV;
		break;
	case vk::VK_FORMAT_D32_SFLOAT:
	case vk::VK_FORMAT_D32_SFLOAT_S8_UINT:
		bufferFormat.type = retFormat.type = tcu::TextureFormat::FLOAT;
		break;
	default:
		TCU_FAIL("unrecognized format");
	}

	const vk::VkDeviceSize			pixelDataSize	= renderSize.x() * renderSize.y() * bufferFormat.getPixelSize();
	de::MovePtr<tcu::TextureLevel>	resultLevel		(new tcu::TextureLevel(retFormat, renderSize.x(), renderSize.y()));

	// Create destination buffer
	{
		const vk::VkBufferCreateInfo bufferParams =
		{
			vk::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			0u,											// VkBufferCreateFlags	flags;
			pixelDataSize,								// VkDeviceSize			size;
			vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT,			// VkBufferUsageFlags	usage;
			vk::VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
			0u,											// deUint32				queueFamilyIndexCount;
			DE_NULL										// const deUint32*		pQueueFamilyIndices;
		};

		buffer		= createBuffer(vk, device, &bufferParams);
		bufferAlloc = allocator.allocate(getBufferMemoryRequirements(vk, device, *buffer), vk::MemoryRequirement::HostVisible);
		VK_CHECK(vk.bindBufferMemory(device, *buffer, bufferAlloc->getMemory(), bufferAlloc->getOffset()));
	}

	// Create command pool and buffer
	cmdPool		= createCommandPool(vk, device, vk::VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
	cmdBuffer	= allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	beginCommandBuffer(vk, *cmdBuffer);
	copyImageToBuffer(vk, *cmdBuffer, image, *buffer, tcu::IVec2(renderSize.x(), renderSize.y()), vk::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, currentLayout, 1u, barrierAspect, vk::VK_IMAGE_ASPECT_DEPTH_BIT);
	endCommandBuffer(vk, *cmdBuffer);

	submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

	// Read buffer data
	invalidateAlloc(vk, device, *bufferAlloc);
	tcu::copy(*resultLevel, tcu::ConstPixelBufferAccess(bufferFormat, resultLevel->getSize(), bufferAlloc->getHostPtr()));

	return resultLevel;
}

de::MovePtr<tcu::TextureLevel> readStencilAttachment (const vk::DeviceInterface&	vk,
													  vk::VkDevice					device,
													  vk::VkQueue					queue,
													  deUint32						queueFamilyIndex,
													  vk::Allocator&				allocator,
													  vk::VkImage					image,
													  vk::VkFormat					format,
													  const tcu::UVec2&				renderSize,
													  vk::VkImageLayout				currentLayout)
{
	vk::Move<vk::VkBuffer>			buffer;
	de::MovePtr<vk::Allocation>		bufferAlloc;
	vk::Move<vk::VkCommandPool>		cmdPool;
	vk::Move<vk::VkCommandBuffer>	cmdBuffer;

	tcu::TextureFormat				retFormat		(tcu::TextureFormat::S, tcu::TextureFormat::UNSIGNED_INT8);
	tcu::TextureFormat				bufferFormat	(tcu::TextureFormat::S, tcu::TextureFormat::UNSIGNED_INT8);

	const vk::VkImageAspectFlags	barrierAspect	= vk::VK_IMAGE_ASPECT_STENCIL_BIT | (mapVkFormat(format).order == tcu::TextureFormat::DS ? vk::VK_IMAGE_ASPECT_DEPTH_BIT : (vk::VkImageAspectFlagBits)0);
	const vk::VkDeviceSize			pixelDataSize	= renderSize.x() * renderSize.y() * bufferFormat.getPixelSize();
	de::MovePtr<tcu::TextureLevel>	resultLevel		(new tcu::TextureLevel(retFormat, renderSize.x(), renderSize.y()));

	// Create destination buffer
	{
		const vk::VkBufferCreateInfo bufferParams =
		{
			vk::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			0u,											// VkBufferCreateFlags	flags;
			pixelDataSize,								// VkDeviceSize			size;
			vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT,		// VkBufferUsageFlags	usage;
			vk::VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode;
			0u,											// deUint32				queueFamilyIndexCount;
			DE_NULL										// const deUint32*		pQueueFamilyIndices;
		};

		buffer		= createBuffer(vk, device, &bufferParams);
		bufferAlloc = allocator.allocate(getBufferMemoryRequirements(vk, device, *buffer), vk::MemoryRequirement::HostVisible);
		VK_CHECK(vk.bindBufferMemory(device, *buffer, bufferAlloc->getMemory(), bufferAlloc->getOffset()));
	}

	// Create command pool and buffer
	cmdPool		= createCommandPool(vk, device, vk::VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
	cmdBuffer	= allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	beginCommandBuffer(vk, *cmdBuffer);
	copyImageToBuffer(vk, *cmdBuffer, image, *buffer, tcu::IVec2(renderSize.x(), renderSize.y()), vk::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, currentLayout, 1u, barrierAspect, vk::VK_IMAGE_ASPECT_STENCIL_BIT);
	endCommandBuffer(vk, *cmdBuffer);

	submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

	// Read buffer data
	invalidateAlloc(vk, device, *bufferAlloc);
	tcu::copy(*resultLevel, tcu::ConstPixelBufferAccess(bufferFormat, resultLevel->getSize(), bufferAlloc->getHostPtr()));

	return resultLevel;
}

struct StateTestParams {
	bool pipeline;
	bool meshShader;
	bool vertShader;
	bool tessShader;
	bool geomShader;
	bool fragShader;
	bool logicOp;
	bool alphaToOne;
	bool depthBounds;
	bool depthClamp;
	bool depthClip;
	bool depthClipControl;
	bool colorWrite;
	bool geometryStreams;
	bool discardRectangles;
	bool conservativeRasterization;
	bool rasterizerDiscardEnable;
	bool lines;
	bool sampleLocations;
	bool provokingVertex;
	bool lineRasterization;
	bool cull;
	bool stencilTestEnable;
	bool depthTestEnable;
	bool depthBiasEnable;
	bool depthBoundsTestEnable;
	bool logicOpEnable;
	bool colorBlendEnable;
	bool discardRectanglesEnable;
	bool sampleLocationsEnable;
	bool conservativeRasterizationOverestimate;
	bool stippledLineEnable;
	bool colorWriteEnable;

	void reset()
	{
		logicOp = false;
		alphaToOne = false;
		depthBounds = false;
		depthClamp = false;
		depthClip = false;
		depthClipControl = false;
		colorWrite = true;
		geometryStreams = false;
		discardRectangles = false;
		conservativeRasterization = false;
		rasterizerDiscardEnable = false;
		lines = false;
		sampleLocations = false;
		provokingVertex = false;
		lineRasterization = false;
		cull = false;
		stencilTestEnable = false;
		depthTestEnable = false;
		depthBiasEnable = false;
		depthBoundsTestEnable = false;
		logicOpEnable = false;
		colorBlendEnable = false;
		discardRectanglesEnable = false;
		sampleLocationsEnable = false;
		conservativeRasterizationOverestimate = false;
		stippledLineEnable = false;
		colorWriteEnable = true;
	}
};

class ShaderObjectStateInstance : public vkt::TestInstance
{
public:
							ShaderObjectStateInstance	(Context& context, const StateTestParams& testParams)
														: vkt::TestInstance	(context)
														, m_params			(testParams)
														{}
	virtual					~ShaderObjectStateInstance	(void) {}

	tcu::TestStatus			iterate						(void) override;

private:
	void createDevice									(void);
	std::vector<vk::VkDynamicState> getDynamicStates	(void) const;
	bool hasDynamicState						(const std::vector<vk::VkDynamicState> dynamicStates, const vk::VkDynamicState dynamicState);
	void setDynamicStates								(const vk::DeviceInterface& vk, vk::VkCommandBuffer cmdBuffer);
	bool isInsidePrimitive								(deUint32 i, deUint32 j, deUint32 width, deUint32 height);

	vk::Move<vk::VkDevice>								m_customDevice;
	de::MovePtr<vk::DeviceDriver>						m_logicalDeviceInterface;
	vk::VkQueue											m_logicalDeviceQueue;
	const StateTestParams								m_params;
};

void ShaderObjectStateInstance::createDevice (void)
{
	vk::VkPhysicalDeviceMeshShaderFeaturesEXT			meshShaderFeatuers			= vk::initVulkanStructure();
	vk::VkPhysicalDeviceColorWriteEnableFeaturesEXT		colorWriteEnableFeatures	= vk::initVulkanStructure();
	vk::VkPhysicalDeviceDepthClipControlFeaturesEXT		depthClipControlFeatures	= vk::initVulkanStructure();
	vk::VkPhysicalDeviceDepthClipEnableFeaturesEXT		depthClipEnableFeatures		= vk::initVulkanStructure();
	vk::VkPhysicalDeviceTransformFeedbackFeaturesEXT	transformFeedbackFeatures	= vk::initVulkanStructure();
	vk::VkPhysicalDeviceLineRasterizationFeaturesEXT	lineRasterizationFeatures	= vk::initVulkanStructure();

	vk::VkPhysicalDeviceDynamicRenderingFeatures		dynamicRenderingFeatures	= m_context.getDynamicRenderingFeatures();
	vk::VkPhysicalDeviceShaderObjectFeaturesEXT			shaderObjectFeatures		= m_context.getShaderObjectFeaturesEXT();

	vk::VkPhysicalDeviceExtendedDynamicStateFeaturesEXT		edsFeatures		= m_context.getExtendedDynamicStateFeaturesEXT();
	vk::VkPhysicalDeviceExtendedDynamicState2FeaturesEXT	eds2Features	= m_context.getExtendedDynamicState2FeaturesEXT();
	vk::VkPhysicalDeviceExtendedDynamicState3FeaturesEXT	eds3Features	= m_context.getExtendedDynamicState3FeaturesEXT();
	vk::VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT	viFeatures		= m_context.getVertexInputDynamicStateFeaturesEXT();

	dynamicRenderingFeatures.pNext = DE_NULL;
	shaderObjectFeatures.pNext = DE_NULL;
	edsFeatures.pNext = DE_NULL;
	eds2Features.pNext = DE_NULL;
	eds3Features.pNext = DE_NULL;
	viFeatures.pNext = DE_NULL;

	vk::VkPhysicalDeviceFeatures2						features2					= vk::initVulkanStructure();
	void* pNext = &dynamicRenderingFeatures;

	const float										queuePriority					= 1.0f;
	std::vector<const char*>						deviceExtensions				= { "VK_KHR_dynamic_rendering" };
	if (m_params.pipeline)
	{
		const auto& deviceExts = m_context.getDeviceExtensions();
		if (std::find(deviceExts.begin(), deviceExts.end(), "VK_EXT_extended_dynamic_state") != deviceExts.end())
		{
			deviceExtensions.push_back("VK_EXT_extended_dynamic_state");
			edsFeatures.pNext = pNext;
			pNext = &edsFeatures;
		}
		if (std::find(deviceExts.begin(), deviceExts.end(), "VK_EXT_extended_dynamic_state2") != deviceExts.end())
		{
			deviceExtensions.push_back("VK_EXT_extended_dynamic_state2");
			eds2Features.pNext = pNext;
			pNext = &eds2Features;
		}
		if (std::find(deviceExts.begin(), deviceExts.end(), "VK_EXT_extended_dynamic_state3") != deviceExts.end())
		{
			deviceExtensions.push_back("VK_EXT_extended_dynamic_state3");
			eds3Features.pNext = pNext;
			pNext = &eds3Features;
		}
		if (std::find(deviceExts.begin(), deviceExts.end(), "VK_EXT_vertex_input_dynamic_state") != deviceExts.end())
		{
			deviceExtensions.push_back("VK_EXT_vertex_input_dynamic_state");
			viFeatures.pNext = pNext;
			pNext = &viFeatures;
		}
	}
	else
	{
		deviceExtensions.push_back("VK_EXT_shader_object");
		dynamicRenderingFeatures.pNext = &shaderObjectFeatures;
	}

	if (m_params.tessShader)
		features2.features.tessellationShader = VK_TRUE;
	if (m_params.geomShader)
		features2.features.geometryShader = VK_TRUE;

	if (m_params.logicOp)
		features2.features.logicOp = VK_TRUE;
	if (m_params.alphaToOne)
		features2.features.alphaToOne = VK_TRUE;
	if (m_params.depthBounds)
		features2.features.depthBounds = VK_TRUE;
	if (m_params.depthClamp)
		features2.features.depthClamp = VK_TRUE;
	if (m_params.depthBiasEnable)
		features2.features.depthBiasClamp = VK_TRUE;
	if (m_params.depthClip)
	{
		depthClipEnableFeatures.pNext = pNext;
		pNext = &depthClipEnableFeatures;
		depthClipEnableFeatures.depthClipEnable = VK_TRUE;
		deviceExtensions.push_back("VK_EXT_depth_clip_enable");
	}
	if (m_params.depthClipControl)
	{
		depthClipControlFeatures.pNext = pNext;
		pNext = &depthClipControlFeatures;
		depthClipControlFeatures.depthClipControl = VK_TRUE;
		deviceExtensions.push_back("VK_EXT_depth_clip_control");
	}
	if (m_params.colorWrite)
	{
		colorWriteEnableFeatures.pNext = pNext;
		pNext = &colorWriteEnableFeatures;
		colorWriteEnableFeatures.colorWriteEnable = VK_TRUE;
		deviceExtensions.push_back("VK_EXT_color_write_enable");
	}
	if (m_params.geometryStreams)
	{
		transformFeedbackFeatures.pNext = pNext;
		pNext = &transformFeedbackFeatures;
		transformFeedbackFeatures.transformFeedback = VK_TRUE;
		transformFeedbackFeatures.geometryStreams = VK_TRUE;
		deviceExtensions.push_back("VK_EXT_transform_feedback");
	}
	if (m_params.sampleLocations)
		deviceExtensions.push_back("VK_EXT_sample_locations");
	if (m_params.discardRectangles)
		deviceExtensions.push_back("VK_EXT_discard_rectangles");
	if (m_params.conservativeRasterization)
		deviceExtensions.push_back("VK_EXT_conservative_rasterization");
	if (m_params.sampleLocations)
		deviceExtensions.push_back("VK_EXT_sample_locations");
	if (m_params.provokingVertex)
		deviceExtensions.push_back("VK_EXT_provoking_vertex");
	if (m_params.lineRasterization)
	{
		lineRasterizationFeatures.pNext = pNext;
		pNext = &lineRasterizationFeatures;
		lineRasterizationFeatures.rectangularLines = VK_TRUE;
		deviceExtensions.push_back("VK_EXT_line_rasterization");
	}
	if (m_params.meshShader)
	{
		meshShaderFeatuers.pNext = pNext;
		pNext = &meshShaderFeatuers;
		meshShaderFeatuers.meshShader = VK_TRUE;
		deviceExtensions.push_back("VK_EXT_mesh_shader");
	}

	features2.pNext = pNext;

	vk::VkDeviceQueueCreateInfo						queueInfo =
	{
		vk::VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,	// VkStructureType					sType;
		DE_NULL,										// const void*						pNext;
		0u,												// VkDeviceQueueCreateFlags			flags;
		0u,												// deUint32							queueFamilyIndex;
		1u,												// deUint32							queueCount;
		&queuePriority									// const float*						pQueuePriorities;
	};

	const vk::VkDeviceCreateInfo					deviceInfo =
	{
		vk::VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,		// VkStructureType					sType;
		&features2,										// const void*						pNext;
		(vk::VkDeviceCreateFlags)0,						// VkDeviceCreateFlags				flags;
		1u,												// uint32_t							queueCreateInfoCount;
		&queueInfo,										// const VkDeviceQueueCreateInfo*	pQueueCreateInfos;
		0u,												// uint32_t							enabledLayerCount;
		DE_NULL,										// const char* const*				ppEnabledLayerNames;
		deUint32(deviceExtensions.size()),				// uint32_t							enabledExtensionCount;
		deviceExtensions.data(),						// const char* const*				ppEnabledExtensionNames;
		DE_NULL											// const VkPhysicalDeviceFeatures*	pEnabledFeatures;
	};

	m_customDevice = createCustomDevice(m_context.getTestContext().getCommandLine().isValidationEnabled(), m_context.getPlatformInterface(), m_context.getInstance(), m_context.getInstanceInterface(), m_context.getPhysicalDevice(), &deviceInfo);
	m_logicalDeviceInterface = de::MovePtr<vk::DeviceDriver>(new vk::DeviceDriver(m_context.getPlatformInterface(), m_context.getInstance(), *m_customDevice, m_context.getUsedApiVersion()));
	m_logicalDeviceInterface->getDeviceQueue(*m_customDevice, m_context.getUniversalQueueFamilyIndex(), 0, &m_logicalDeviceQueue);
}

std::vector<vk::VkDynamicState> ShaderObjectStateInstance::getDynamicStates (void) const
{
	const auto& edsFeatures		= m_context.getExtendedDynamicStateFeaturesEXT();
	const auto& eds2Features	= m_context.getExtendedDynamicState2FeaturesEXT();
	const auto& eds3Features	= m_context.getExtendedDynamicState3FeaturesEXT();
	const auto& viFeatures		= m_context.getVertexInputDynamicStateFeaturesEXT();

	std::vector<vk::VkDynamicState> dynamicStates;

	if (edsFeatures.extendedDynamicState)
	{
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT);
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT);
	}

	dynamicStates.push_back(vk::VK_DYNAMIC_STATE_LINE_WIDTH);
	dynamicStates.push_back(vk::VK_DYNAMIC_STATE_DEPTH_BIAS);
	dynamicStates.push_back(vk::VK_DYNAMIC_STATE_BLEND_CONSTANTS);
	dynamicStates.push_back(vk::VK_DYNAMIC_STATE_DEPTH_BOUNDS);
	dynamicStates.push_back(vk::VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK);
	dynamicStates.push_back(vk::VK_DYNAMIC_STATE_STENCIL_WRITE_MASK);
	dynamicStates.push_back(vk::VK_DYNAMIC_STATE_STENCIL_REFERENCE);
	if (edsFeatures.extendedDynamicState && !m_params.meshShader && !m_params.pipeline)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE);
	if (edsFeatures.extendedDynamicState)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_CULL_MODE);
	if (edsFeatures.extendedDynamicState)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE);
	if (edsFeatures.extendedDynamicState)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_DEPTH_COMPARE_OP);
	if (edsFeatures.extendedDynamicState)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE);
	if (edsFeatures.extendedDynamicState)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE);
	if (edsFeatures.extendedDynamicState)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_FRONT_FACE);
	if (edsFeatures.extendedDynamicState && !m_params.meshShader)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY);
	if (edsFeatures.extendedDynamicState)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_STENCIL_OP);
	if (edsFeatures.extendedDynamicState)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE);
	if (eds2Features.extendedDynamicState2)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE);
	if (eds2Features.extendedDynamicState2 && !m_params.meshShader)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE);
	if (eds2Features.extendedDynamicState2)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE_EXT);
	if (viFeatures.vertexInputDynamicState && !m_params.meshShader && !m_params.pipeline)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_VERTEX_INPUT_EXT);
	if (eds2Features.extendedDynamicState2LogicOp)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_LOGIC_OP_EXT);
	if (eds2Features.extendedDynamicState2PatchControlPoints && !m_params.meshShader)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_PATCH_CONTROL_POINTS_EXT);
	if (eds3Features.extendedDynamicState3TessellationDomainOrigin)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_TESSELLATION_DOMAIN_ORIGIN_EXT);
	if (eds3Features.extendedDynamicState3DepthClampEnable)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_DEPTH_CLAMP_ENABLE_EXT);
	if (eds3Features.extendedDynamicState3PolygonMode)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_POLYGON_MODE_EXT);
	if (eds3Features.extendedDynamicState3RasterizationSamples)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT);
	if (eds3Features.extendedDynamicState3SampleMask)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_SAMPLE_MASK_EXT);
	if (eds3Features.extendedDynamicState3AlphaToCoverageEnable)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_ALPHA_TO_COVERAGE_ENABLE_EXT);
	if (eds3Features.extendedDynamicState3AlphaToOneEnable)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_ALPHA_TO_ONE_ENABLE_EXT);
	if (eds3Features.extendedDynamicState3LogicOpEnable)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_LOGIC_OP_ENABLE_EXT);
	if (eds3Features.extendedDynamicState3ColorBlendEnable)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT);
	if (eds3Features.extendedDynamicState3ColorBlendEquation)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT);
	if (eds3Features.extendedDynamicState3ColorWriteMask)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT);
	if (eds3Features.extendedDynamicState3RasterizationStream && m_params.geometryStreams)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_RASTERIZATION_STREAM_EXT);
	if (m_params.discardRectangles)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_DISCARD_RECTANGLE_ENABLE_EXT);
	if (m_params.discardRectangles)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_DISCARD_RECTANGLE_MODE_EXT);
	if (m_params.discardRectangles)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_DISCARD_RECTANGLE_EXT);
	if (eds3Features.extendedDynamicState3ConservativeRasterizationMode && m_params.conservativeRasterization)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_CONSERVATIVE_RASTERIZATION_MODE_EXT);
	if (eds3Features.extendedDynamicState3ExtraPrimitiveOverestimationSize && m_params.conservativeRasterization)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_EXTRA_PRIMITIVE_OVERESTIMATION_SIZE_EXT);
	if (eds3Features.extendedDynamicState3DepthClipEnable && m_params.depthClip)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_DEPTH_CLIP_ENABLE_EXT);
	if (eds3Features.extendedDynamicState3SampleLocationsEnable && m_params.sampleLocations)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_ENABLE_EXT);
	if (m_params.sampleLocations)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_EXT);
	if (eds3Features.extendedDynamicState3ProvokingVertexMode && m_params.provokingVertex)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_PROVOKING_VERTEX_MODE_EXT);
	if (eds3Features.extendedDynamicState3LineRasterizationMode && m_params.lineRasterization)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_LINE_RASTERIZATION_MODE_EXT);
	if (eds3Features.extendedDynamicState3LineStippleEnable && m_params.lineRasterization)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_LINE_STIPPLE_ENABLE_EXT);
	if (m_params.lineRasterization)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_LINE_STIPPLE_EXT);
	if (eds3Features.extendedDynamicState3DepthClipNegativeOneToOne && m_params.depthClipControl)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_DEPTH_CLIP_NEGATIVE_ONE_TO_ONE_EXT);
	if (m_params.colorWrite)
		dynamicStates.push_back(vk::VK_DYNAMIC_STATE_COLOR_WRITE_ENABLE_EXT);
	return dynamicStates;
}

bool ShaderObjectStateInstance::hasDynamicState(const std::vector<vk::VkDynamicState> dynamicStates, const vk::VkDynamicState dynamicState)
{
	if (!m_params.pipeline)
		return false;
	return std::find(dynamicStates.begin(), dynamicStates.end(), dynamicState) != dynamicStates.end();
}

bool extensionEnabled (const std::vector<std::string>& deviceExtensions, const std::string& ext)
{
	return std::find(deviceExtensions.begin(), deviceExtensions.end(), ext) != deviceExtensions.end();
}

void ShaderObjectStateInstance::setDynamicStates (const vk::DeviceInterface& vk, vk::VkCommandBuffer cmdBuffer)
{
	const auto dynamicStates		= getDynamicStates();
	const auto deviceExtensions		= vk::removeUnsupportedShaderObjectExtensions(m_context.getInstanceInterface(), m_context.getPhysicalDevice(), m_context.getDeviceExtensions());

	vk::VkViewport viewport = { 0, 0, 32, 32, 0.0f, 1.0f, };
	if (m_params.depthClamp)
		viewport.maxDepth = 0.5f;
	if (!m_params.pipeline || hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT))
		vk.cmdSetViewportWithCount(cmdBuffer, 1u, &viewport);
	vk::VkRect2D scissor = { { 0, 0, }, { 32, 32, }, };
	if (!m_params.pipeline || hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT))
		vk.cmdSetScissorWithCount(cmdBuffer, 1u, &scissor);
	if ((!m_params.pipeline && !m_params.rasterizerDiscardEnable && m_params.lines) || hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_LINE_WIDTH))
		vk.cmdSetLineWidth(cmdBuffer, 1.0f);
	if ((!m_params.pipeline && !m_params.rasterizerDiscardEnable && m_params.depthBiasEnable))
		vk.cmdSetDepthBias(cmdBuffer, 4.0f, 1.0f, 4.0f);
	else if (hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_DEPTH_BIAS))
		vk.cmdSetDepthBias(cmdBuffer, 1.0f, 0.0f, 1.0f);
	float blendConstants[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	if ((!m_params.pipeline && m_params.fragShader && !m_params.rasterizerDiscardEnable && m_params.colorBlendEnable) || hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_BLEND_CONSTANTS))
		vk.cmdSetBlendConstants(cmdBuffer, blendConstants);
	if ((!m_params.pipeline && !m_params.rasterizerDiscardEnable && m_params.depthBoundsTestEnable) || hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_DEPTH_BOUNDS))
		vk.cmdSetDepthBounds(cmdBuffer, 0.2f, 0.3f);
	vk.cmdSetStencilCompareMask(cmdBuffer, vk::VK_STENCIL_FACE_FRONT_AND_BACK, 0xFFFFFFFF);
	vk.cmdSetStencilWriteMask(cmdBuffer, vk::VK_STENCIL_FACE_FRONT_AND_BACK, 0xFFFFFFFF);
	vk.cmdSetStencilReference(cmdBuffer, vk::VK_STENCIL_FACE_FRONT_AND_BACK, 0xFFFFFFFF);
	if (!m_params.pipeline || hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE))
		vk.cmdBindVertexBuffers2(cmdBuffer, 0, 0, DE_NULL, DE_NULL, DE_NULL, DE_NULL);
	if ((!m_params.pipeline && !m_params.rasterizerDiscardEnable) || hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_CULL_MODE))
		vk.cmdSetCullMode(cmdBuffer, m_params.cull ? vk::VK_CULL_MODE_FRONT_AND_BACK : vk::VK_CULL_MODE_NONE);
	if ((!m_params.pipeline && !m_params.rasterizerDiscardEnable && m_params.depthBounds) || hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_DEPTH_BOUNDS))
		vk.cmdSetDepthBoundsTestEnable(cmdBuffer, m_params.depthBoundsTestEnable ? VK_TRUE : VK_FALSE);
	if ((!m_params.pipeline && !m_params.rasterizerDiscardEnable) || hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_DEPTH_COMPARE_OP))
		vk.cmdSetDepthCompareOp(cmdBuffer, vk::VK_COMPARE_OP_LESS);
	if ((!m_params.pipeline && !m_params.rasterizerDiscardEnable) || hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE))
		vk.cmdSetDepthTestEnable(cmdBuffer, VK_TRUE);
	if ((!m_params.pipeline && !m_params.rasterizerDiscardEnable) || hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE))
		vk.cmdSetDepthWriteEnable(cmdBuffer, VK_TRUE);
	if ((!m_params.pipeline && !m_params.rasterizerDiscardEnable && (m_params.cull || m_params.stencilTestEnable)) || hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_FRONT_FACE))
		vk.cmdSetFrontFace(cmdBuffer, vk::VK_FRONT_FACE_CLOCKWISE);
	if ((!m_params.pipeline && m_params.vertShader) || hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY))
	{
		if (m_params.tessShader)
			vk.cmdSetPrimitiveTopology(cmdBuffer, vk::VK_PRIMITIVE_TOPOLOGY_PATCH_LIST);
		else if (m_params.lines)
			vk.cmdSetPrimitiveTopology(cmdBuffer, vk::VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
		else
			vk.cmdSetPrimitiveTopology(cmdBuffer, vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
	}
	if ((!m_params.pipeline && !m_params.rasterizerDiscardEnable && m_params.stencilTestEnable) || hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_STENCIL_OP))
		vk.cmdSetStencilOp(cmdBuffer, vk::VK_STENCIL_FACE_FRONT_AND_BACK, vk::VK_STENCIL_OP_REPLACE, vk::VK_STENCIL_OP_REPLACE, vk::VK_STENCIL_OP_REPLACE, vk::VK_COMPARE_OP_GREATER);
	if ((!m_params.pipeline && !m_params.rasterizerDiscardEnable) || hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE))
		vk.cmdSetStencilTestEnable(cmdBuffer, m_params.stencilTestEnable ? VK_TRUE : VK_FALSE);
	if ((!m_params.pipeline && !m_params.rasterizerDiscardEnable) || hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE))
		vk.cmdSetDepthBiasEnable(cmdBuffer, m_params.depthBiasEnable ? VK_TRUE : VK_FALSE);
	if ((!m_params.pipeline && m_params.vertShader) || hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE))
		vk.cmdSetPrimitiveRestartEnable(cmdBuffer, VK_FALSE);
	if ((!m_params.pipeline && m_params.fragShader) || hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE))
		vk.cmdSetRasterizerDiscardEnable(cmdBuffer, m_params.rasterizerDiscardEnable ? VK_TRUE : VK_FALSE);
	if ((!m_params.pipeline && m_params.vertShader) || hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE))
		if (extensionEnabled(deviceExtensions, "VK_EXT_shader_object") || extensionEnabled(deviceExtensions, "VK_EXT_vertex_input_dynamic_state"))
			vk.cmdSetVertexInputEXT(cmdBuffer, 0u, DE_NULL, 0u, DE_NULL);
	if ((!m_params.pipeline && m_params.fragShader && !m_params.rasterizerDiscardEnable && m_params.logicOpEnable) || hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_LOGIC_OP_EXT))
		vk.cmdSetLogicOpEXT(cmdBuffer, vk::VK_LOGIC_OP_COPY);
	if (!m_params.pipeline || hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_PATCH_CONTROL_POINTS_EXT))
		vk.cmdSetPatchControlPointsEXT(cmdBuffer, 4u);
	if ((!m_params.pipeline && m_params.tessShader) || hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_TESSELLATION_DOMAIN_ORIGIN_EXT))
		vk.cmdSetTessellationDomainOriginEXT(cmdBuffer, vk::VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT);
	if (!m_params.pipeline && !m_params.rasterizerDiscardEnable && m_params.depthClamp)
		vk.cmdSetDepthClampEnableEXT(cmdBuffer, VK_TRUE);
	else if (hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_DEPTH_CLAMP_ENABLE_EXT))
		vk.cmdSetDepthClampEnableEXT(cmdBuffer, m_params.depthClamp ? VK_TRUE : VK_FALSE);
	if ((!m_params.pipeline && !m_params.rasterizerDiscardEnable) || hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_POLYGON_MODE_EXT))
		vk.cmdSetPolygonModeEXT(cmdBuffer, vk::VK_POLYGON_MODE_FILL);
	if ((!m_params.pipeline && !m_params.rasterizerDiscardEnable) || hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT))
		vk.cmdSetRasterizationSamplesEXT(cmdBuffer, vk::VK_SAMPLE_COUNT_1_BIT);
	vk::VkSampleMask sampleMask = 0xFFFFFFFF;
	if ((!m_params.pipeline && !m_params.rasterizerDiscardEnable) || hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_SAMPLE_MASK_EXT))
		vk.cmdSetSampleMaskEXT(cmdBuffer, vk::VK_SAMPLE_COUNT_1_BIT, &sampleMask);
	if ((!m_params.pipeline && !m_params.rasterizerDiscardEnable) || hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_ALPHA_TO_COVERAGE_ENABLE_EXT))
		vk.cmdSetAlphaToCoverageEnableEXT(cmdBuffer, VK_FALSE);
	if (!m_params.pipeline && !m_params.rasterizerDiscardEnable && m_params.alphaToOne)
		vk.cmdSetAlphaToOneEnableEXT(cmdBuffer, VK_TRUE);
	else if (hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_ALPHA_TO_ONE_ENABLE_EXT))
		vk.cmdSetAlphaToOneEnableEXT(cmdBuffer, m_params.alphaToOne ? VK_TRUE : VK_FALSE);
	if (!m_params.pipeline && m_params.fragShader && !m_params.rasterizerDiscardEnable && m_params.logicOp)
		vk.cmdSetLogicOpEnableEXT(cmdBuffer, m_params.logicOpEnable ? VK_TRUE : VK_FALSE);
	else if (hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_LOGIC_OP_ENABLE_EXT))
		vk.cmdSetLogicOpEnableEXT(cmdBuffer, m_params.logicOpEnable ? VK_TRUE : VK_FALSE);
	vk::VkBool32 colorBlendEnable = m_params.colorBlendEnable ? VK_TRUE : VK_FALSE;
	if ((!m_params.pipeline && m_params.fragShader && !m_params.rasterizerDiscardEnable) || hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT))
		vk.cmdSetColorBlendEnableEXT(cmdBuffer, 0u, 1u, &colorBlendEnable);
	vk::VkColorBlendEquationEXT		colorBlendEquation = {
		vk::VK_BLEND_FACTOR_ONE,		// VkBlendFactor	srcColorBlendFactor;
		vk::VK_BLEND_FACTOR_ONE,		// VkBlendFactor	dstColorBlendFactor;
		vk::VK_BLEND_OP_ADD,			// VkBlendOp		colorBlendOp;
		vk::VK_BLEND_FACTOR_ONE,		// VkBlendFactor	srcAlphaBlendFactor;
		vk::VK_BLEND_FACTOR_ONE,		// VkBlendFactor	dstAlphaBlendFactor;
		vk::VK_BLEND_OP_ADD,			// VkBlendOp		alphaBlendOp;
	};
	if ((!m_params.pipeline && m_params.fragShader && !m_params.rasterizerDiscardEnable) || hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT))
		vk.cmdSetColorBlendEquationEXT(cmdBuffer, 0u, 1u, &colorBlendEquation);
	vk::VkColorComponentFlags		colorWriteMask = vk::VK_COLOR_COMPONENT_R_BIT | vk::VK_COLOR_COMPONENT_G_BIT | vk::VK_COLOR_COMPONENT_B_BIT | vk::VK_COLOR_COMPONENT_A_BIT;
	if ((!m_params.pipeline && m_params.fragShader && !m_params.rasterizerDiscardEnable) || hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT))
		vk.cmdSetColorWriteMaskEXT(cmdBuffer, 0u, 1u, &colorWriteMask);
	if ((!m_params.pipeline && m_params.geomShader && m_params.geometryStreams) || hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_RASTERIZATION_STREAM_EXT))
		vk.cmdSetRasterizationStreamEXT(cmdBuffer, 0u);
	if (m_params.discardRectangles)
		vk.cmdSetDiscardRectangleEnableEXT(cmdBuffer, m_params.discardRectanglesEnable ? VK_TRUE : VK_FALSE);
	if ((!m_params.pipeline && m_params.discardRectanglesEnable) || hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_DISCARD_RECTANGLE_MODE_EXT))
		vk.cmdSetDiscardRectangleModeEXT(cmdBuffer, vk::VK_DISCARD_RECTANGLE_MODE_EXCLUSIVE_EXT);
	if ((!m_params.pipeline && m_params.discardRectanglesEnable) || hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_DISCARD_RECTANGLE_EXT))
		vk.cmdSetDiscardRectangleEXT(cmdBuffer, 0u, 1u, &scissor);
	if ((!m_params.pipeline && m_params.fragShader && !m_params.rasterizerDiscardEnable && m_params.conservativeRasterization) || hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_CONSERVATIVE_RASTERIZATION_MODE_EXT))
		vk.cmdSetConservativeRasterizationModeEXT(cmdBuffer, m_params.conservativeRasterizationOverestimate ? vk::VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT : vk::VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT);
	if ((!m_params.pipeline && m_params.fragShader && !m_params.rasterizerDiscardEnable && m_params.conservativeRasterization && m_params.conservativeRasterizationOverestimate) || hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_EXTRA_PRIMITIVE_OVERESTIMATION_SIZE_EXT))
		vk.cmdSetExtraPrimitiveOverestimationSizeEXT(cmdBuffer, de::min(1.0f, m_context.getConservativeRasterizationPropertiesEXT().maxExtraPrimitiveOverestimationSize));
	if ((!m_params.pipeline && m_params.depthClip) || hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_DEPTH_CLIP_ENABLE_EXT))
		vk.cmdSetDepthClipEnableEXT(cmdBuffer, VK_TRUE);
	if ((!m_params.pipeline && m_params.sampleLocations) || hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_ENABLE_EXT))
		vk.cmdSetSampleLocationsEnableEXT(cmdBuffer, m_params.sampleLocationsEnable ? VK_TRUE : VK_FALSE);
	vk::VkSampleLocationEXT sampleLocation = { 0.5f, 0.5f };
	const vk::VkSampleLocationsInfoEXT sampleLocationsInfo =
	{
		vk::VK_STRUCTURE_TYPE_SAMPLE_LOCATIONS_INFO_EXT,	// VkStructureType               sType;
		DE_NULL,											// const void*                   pNext;
		vk::VK_SAMPLE_COUNT_1_BIT,							// VkSampleCountFlagBits         sampleLocationsPerPixel;
		{ 1u, 1u },											// VkExtent2D                    sampleLocationGridSize;
		1,													// uint32_t                      sampleLocationsCount;
		&sampleLocation,									// const VkSampleLocationEXT*    pSampleLocations;
	};
	if ((!m_params.pipeline && m_params.sampleLocations && m_params.sampleLocationsEnable) || hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_EXT))
		vk.cmdSetSampleLocationsEXT(cmdBuffer, &sampleLocationsInfo);
	if ((!m_params.pipeline && m_params.provokingVertex) || hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_PROVOKING_VERTEX_MODE_EXT))
		vk.cmdSetProvokingVertexModeEXT(cmdBuffer, vk::VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT);
	if (m_params.pipeline || (m_params.fragShader && !m_params.rasterizerDiscardEnable && m_params.lineRasterization && m_params.lines))
	{
		if (!m_params.pipeline || hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_LINE_RASTERIZATION_MODE_EXT))
			vk.cmdSetLineRasterizationModeEXT(cmdBuffer, vk::VK_LINE_RASTERIZATION_MODE_RECTANGULAR_EXT);
		if (!m_params.pipeline || hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_LINE_STIPPLE_ENABLE_EXT))
			vk.cmdSetLineStippleEnableEXT(cmdBuffer, m_params.stippledLineEnable ? VK_TRUE : VK_FALSE);
		if ((!m_params.pipeline && m_params.stippledLineEnable) || hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_LINE_STIPPLE_EXT))
			vk.cmdSetLineStippleEXT(cmdBuffer, 1u, 0x1);
	}
	if ((!m_params.pipeline && m_params.depthClipControl) || hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_DEPTH_CLIP_NEGATIVE_ONE_TO_ONE_EXT))
		vk.cmdSetDepthClipNegativeOneToOneEXT(cmdBuffer, VK_TRUE);
	vk::VkBool32 colorWriteEnable = m_params.colorWriteEnable ? VK_TRUE : VK_FALSE;
	if ((!m_params.pipeline && m_params.colorWrite) || hasDynamicState(dynamicStates, vk::VK_DYNAMIC_STATE_COLOR_WRITE_ENABLE_EXT))
		vk.cmdSetColorWriteEnableEXT(cmdBuffer, 1u, &colorWriteEnable);
}

bool ShaderObjectStateInstance::isInsidePrimitive (deUint32 i, deUint32 j, deUint32 width, deUint32 height)
{
	deUint32	xOffset		= width / 4;
	deUint32	yOffset		= height / 4;
	if (m_params.tessShader)
		xOffset /= 2;
	if (m_params.geomShader)
		yOffset /= 2;

	bool inside = false;
	if (m_params.lines)
	{
		if (m_params.stippledLineEnable)
		{
			if (m_params.tessShader && m_params.geomShader)
				inside = (j == 4 && i == 3) || (j == 20 && i == 3);
			else if (m_params.tessShader)
				inside = (j == 8 && i == 3);
			else if (m_params.geomShader)
				inside = (j == 3 && i == 8) || (j == 27 && i == 8);
			else
				inside = (j == 7 && i == 8) || (j == 23 && i == 8);
		}
		else
		{
			if (m_params.tessShader && m_params.geomShader)
				inside = m_params.lines && (i == 3 && (j >= 4 && j < 28));
			else if (m_params.tessShader)
				inside = m_params.lines && (i == 3 && (j >= 8 && j < 24));
			else if (m_params.geomShader)
				inside = m_params.lines && ((j == 3 || j == 27) && (i >= 8 && i < 24));
			else
				inside = m_params.lines && (i >= 8 && i < 24 && (j == 7 || j == 23));
		}
	}
	else
	{
		inside = !m_params.lines && (i >= xOffset && i < width - xOffset && j >= yOffset && j < height - yOffset);
	}
	return inside;
}

tcu::TestStatus ShaderObjectStateInstance::iterate (void)
{
	const vk::VkInstance				instance						= m_context.getInstance();
	const vk::InstanceDriver			instanceDriver					(m_context.getPlatformInterface(), instance);
	createDevice();
	const vk::DeviceInterface&			vk								= *m_logicalDeviceInterface;
	const vk::VkDevice					device							= *m_customDevice;
	const deUint32						queueFamilyIndex				= m_context.getUniversalQueueFamilyIndex();
	const vk::VkQueue					queue							= m_logicalDeviceQueue;
	auto								alloctor						= de::MovePtr<vk::Allocator>(new vk::SimpleAllocator(vk, device, getPhysicalDeviceMemoryProperties(instanceDriver, m_context.getPhysicalDevice())));
	auto&								alloc							= *alloctor;
	const auto							deviceExtensions				= vk::removeUnsupportedShaderObjectExtensions(m_context.getInstanceInterface(), m_context.getPhysicalDevice(), m_context.getDeviceExtensions());
	const bool							tessellationSupported			= m_context.getDeviceFeatures().tessellationShader;
	const bool							geometrySupported				= m_context.getDeviceFeatures().geometryShader;
	tcu::TestLog&						log								= m_context.getTestContext().getLog();

	vk::VkFormat						colorAttachmentFormat			= vk::VK_FORMAT_R8G8B8A8_UNORM;
	vk::VkFormat						depthStencilAttachmentFormat	= findDSFormat(m_context.getInstanceInterface(), m_context.getPhysicalDevice());
	const auto							subresourceRange				= makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const auto							subresourceLayers				= vk::makeImageSubresourceLayers(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
	auto								depthSubresourceRange			= vk::makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_DEPTH_BIT | vk::VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 1u, 0u, 1u);
	const vk::VkRect2D					renderArea						= vk::makeRect2D(0, 0, 32, 32);
	vk::VkExtent3D						extent							= { renderArea.extent.width, renderArea.extent.height, 1};

	const bool							taskSupported					= m_context.getMeshShaderFeatures().taskShader;
	const bool							meshSupported					= m_context.getMeshShaderFeatures().meshShader;

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

	const vk::VkImageCreateInfo	depthCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType
		DE_NULL,									// const void*				pNext
		0u,											// VkImageCreateFlags		flags
		vk::VK_IMAGE_TYPE_2D,						// VkImageType				imageType
		depthStencilAttachmentFormat,				// VkFormat					format
		{ 32, 32, 1 },								// VkExtent3D				extent
		1u,											// uint32_t					mipLevels
		1u,											// uint32_t					arrayLayers
		vk::VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits	samples
		vk::VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling			tiling
		vk::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT,	// VkImageUsageFlags		usage
		vk::VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode
		0,											// uint32_t					queueFamilyIndexCount
		DE_NULL,									// const uint32_t*			pQueueFamilyIndices
		vk::VK_IMAGE_LAYOUT_UNDEFINED				// VkImageLayout			initialLayout
	};

	de::MovePtr<vk::ImageWithMemory>	image						= de::MovePtr<vk::ImageWithMemory>(new vk::ImageWithMemory(vk, device, alloc, createInfo, vk::MemoryRequirement::Any));
	const auto							imageView					= vk::makeImageView(vk, device, **image, vk::VK_IMAGE_VIEW_TYPE_2D, colorAttachmentFormat, subresourceRange);

	de::MovePtr<vk::ImageWithMemory>	depthImage					= de::MovePtr<vk::ImageWithMemory>(new vk::ImageWithMemory(vk, device, alloc, depthCreateInfo, vk::MemoryRequirement::Any));
	const auto							depthImageView				= vk::makeImageView(vk, device, **depthImage, vk::VK_IMAGE_VIEW_TYPE_2D, depthStencilAttachmentFormat, depthSubresourceRange);

	const vk::VkDeviceSize				colorOutputBufferSize		= renderArea.extent.width * renderArea.extent.height * tcu::getPixelSize(vk::mapVkFormat(colorAttachmentFormat));
	de::MovePtr<vk::BufferWithMemory>	colorOutputBuffer			= de::MovePtr<vk::BufferWithMemory>(new vk::BufferWithMemory(
		vk, device, alloc, makeBufferCreateInfo(colorOutputBufferSize, vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT), vk::MemoryRequirement::HostVisible));

	const vk::Move<vk::VkCommandPool>	cmdPool						(vk::createCommandPool(vk, device, 0u, queueFamilyIndex));
	const vk::Move<vk::VkCommandBuffer>	cmdBuffer					(vk::allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const vk::Unique<vk::VkDescriptorSetLayout> descriptorSetLayout(
		vk::DescriptorSetLayoutBuilder()
		.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_ALL_GRAPHICS | vk::VK_SHADER_STAGE_MESH_BIT_EXT)
		.build(vk, device));

	const vk::Unique<vk::VkDescriptorPool>	descriptorPool(
		vk::DescriptorPoolBuilder()
		.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.build(vk, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	const vk::VkDeviceSize					bufferSizeBytes	= sizeof(deUint32) * 8;
	const vk::Unique<vk::VkDescriptorSet>	descriptorSet	(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));
	const vk::BufferWithMemory				outputBuffer	(vk, device, alloc, vk::makeBufferCreateInfo(bufferSizeBytes, vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), vk::MemoryRequirement::HostVisible);

	const vk::VkDescriptorBufferInfo		descriptorInfo	= vk::makeDescriptorBufferInfo(*outputBuffer, 0ull, bufferSizeBytes);
	vk::DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorInfo)
		.update(vk, device);

	const auto								pipelineLayout	= makePipelineLayout(vk, device, *descriptorSetLayout);

	const auto&								binaries		= m_context.getBinaryCollection();
	vk::Move<vk::VkPipeline>				pipeline;
	vk::Move<vk::VkShaderEXT>				meshShader;
	vk::Move<vk::VkShaderEXT>				vertShader;
	vk::Move<vk::VkShaderEXT>				tescShader;
	vk::Move<vk::VkShaderEXT>				teseShader;
	vk::Move<vk::VkShaderEXT>				geomShader;
	vk::Move<vk::VkShaderEXT>				fragShader;

	if (m_params.pipeline)
	{
		vk::Move<vk::VkShaderModule> meshShaderModule;
		vk::Move<vk::VkShaderModule> vertShaderModule;
		vk::Move<vk::VkShaderModule> tescShaderModule;
		vk::Move<vk::VkShaderModule> teseShaderModule;
		vk::Move<vk::VkShaderModule> geomShaderModule;
		vk::Move<vk::VkShaderModule> fragShaderModule;
		if (m_params.meshShader)
			meshShaderModule = vk::createShaderModule(vk, device, binaries.get("mesh"));
		if (m_params.vertShader)
			vertShaderModule = vk::createShaderModule(vk, device, binaries.get("vert"));
		if (m_params.tessShader)
			tescShaderModule = vk::createShaderModule(vk, device, binaries.get("tesc"));
		if (m_params.tessShader)
			teseShaderModule = vk::createShaderModule(vk, device, binaries.get("tese"));
		if (m_params.geomShader)
			geomShaderModule = vk::createShaderModule(vk, device, binaries.get("geom"));
		if (m_params.fragShader)
			fragShaderModule = vk::createShaderModule(vk, device, binaries.get("frag"));

		const vk::VkPipelineVertexInputStateCreateInfo		vertexInputState	=
		{
			vk::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			(vk::VkPipelineVertexInputStateCreateFlags)0,					// VkPipelineVertexInputStateCreateFlags	flags;
			0u,																// deUint32									vertexBindingDescriptionCount;
			DE_NULL,														// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
			0u,																// deUint32									vertexAttributeDescriptionCount;
			DE_NULL															// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
		};

		vk::VkPrimitiveTopology topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		if (m_params.tessShader)
			topology = vk::VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
		else if (m_params.lines)
			topology = vk::VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

		const vk::VkPipelineInputAssemblyStateCreateInfo	inputAssemblyState =
		{
			vk::VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType							sType;
			DE_NULL,															// const void*								pNext;
			(vk::VkPipelineInputAssemblyStateCreateFlags)0,						// VkPipelineInputAssemblyStateCreateFlags	flags;
			topology,															// VkPrimitiveTopology						topology;
			VK_FALSE															// VkBool32									primitiveRestartEnable;
		};

		const vk::VkPipelineTessellationStateCreateInfo		tessellationState =
		{
			vk::VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,	// VkStructureType									sType;
			DE_NULL,														// const void*										pNext;
			(vk::VkPipelineTessellationStateCreateFlags)0u,					// VkPipelineTessellationStateCreateFlags			flags;
			4u																// deUint32											patchControlPoints;
		};

		const vk::VkPipelineRasterizationDepthClipStateCreateInfoEXT	depthClipState =
		{
			vk::VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT,	// VkStructureType										sType;
			DE_NULL,																		// const void*											pNext;
			(vk::VkPipelineRasterizationDepthClipStateCreateFlagsEXT)0u,					// VkPipelineRasterizationDepthClipStateCreateFlagsEXT	flags;
			m_params.depthClip																// VkBool32												depthClipEnable;
		};

		const vk::VkPipelineRasterizationStateCreateInfo	rasterizationState =
		{
			vk::VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,				// VkStructureType							sType;
			m_params.depthClip ? &depthClipState : DE_NULL,								// const void*								pNext;
			(vk::VkPipelineRasterizationStateCreateFlags)0,								// VkPipelineRasterizationStateCreateFlags	flags;
			m_params.depthClamp,														// VkBool32									depthClampEnable;
			m_params.rasterizerDiscardEnable,											// VkBool32									rasterizerDiscardEnable;
			vk::VK_POLYGON_MODE_FILL,													// VkPolygonMode							polygonMode;
			m_params.cull ? (vk::VkCullModeFlags)vk::VK_CULL_MODE_FRONT_AND_BACK : (vk::VkCullModeFlags)vk::VK_CULL_MODE_NONE,	// VkCullModeFlags							cullMode;
			vk::VK_FRONT_FACE_CLOCKWISE,												// VkFrontFace								frontFace;
			m_params.depthBiasEnable ? VK_TRUE : VK_FALSE,								// VkBool32									depthBiasEnable;
			0.0f,																		// float									depthBiasConstantFactor;
			0.0f,																		// float									depthBiasClamp;
			0.0f,																		// float									depthBiasSlopeFactor;
			1.0f																		// float									lineWidth;
		};

		const vk::VkPipelineMultisampleStateCreateInfo		multisampleState =
		{
			vk::VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType							sType
			DE_NULL,														// const void*								pNext
			(vk::VkPipelineMultisampleStateCreateFlags)0u,					// VkPipelineMultisampleStateCreateFlags	flags
			vk::VK_SAMPLE_COUNT_1_BIT,										// VkSampleCountFlagBits					rasterizationSamples
			VK_FALSE,														// VkBool32									sampleShadingEnable
			1.0f,															// float									minSampleShading
			DE_NULL,														// const VkSampleMask*						pSampleMask
			VK_FALSE,														// VkBool32									alphaToCoverageEnable
			m_params.alphaToOne ? VK_TRUE : VK_FALSE						// VkBool32									alphaToOneEnable
		};


		const vk::VkStencilOpState stencilOpState = vk::makeStencilOpState(
			vk::VK_STENCIL_OP_KEEP,				// stencil fail
			vk::VK_STENCIL_OP_KEEP,				// depth & stencil pass
			vk::VK_STENCIL_OP_KEEP,				// depth only fail
			vk::VK_COMPARE_OP_ALWAYS,			// compare op
			0u,									// compare mask
			0u,									// write mask
			0u);								// reference

		const vk::VkPipelineDepthStencilStateCreateInfo			depthStencilState
		{
			vk::VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,	// VkStructureType                          sType
			DE_NULL,														// const void*                              pNext
			(vk::VkPipelineDepthStencilStateCreateFlags)0u,					// VkPipelineDepthStencilStateCreateFlags   flags
			m_params.depthTestEnable ? VK_TRUE : VK_FALSE,					// VkBool32                                 depthTestEnable
			VK_TRUE,														// VkBool32                                 depthWriteEnable
			vk::VK_COMPARE_OP_LESS,											// VkCompareOp                              depthCompareOp
			m_params.depthBoundsTestEnable ? VK_TRUE : VK_FALSE,			// VkBool32                                 depthBoundsTestEnable
			m_params.stencilTestEnable ? VK_TRUE : VK_FALSE,				// VkBool32                                 stencilTestEnable
			stencilOpState,													// VkStencilOpState                         front
			stencilOpState,													// VkStencilOpState                         back
			0.0f,															// float                                    minDepthBounds
			1.0f,															// float                                    maxDepthBounds
		};

		const vk::VkPipelineColorBlendAttachmentState		colorBlendAttState =
		{
			m_params.colorBlendEnable ? VK_TRUE : VK_FALSE,					// VkBool32											blendEnable;
			vk::VK_BLEND_FACTOR_ONE,										// VkBlendFactor									srcColorBlendFactor;
			vk::VK_BLEND_FACTOR_ZERO,										// VkBlendFactor									dstColorBlendFactor;
			vk::VK_BLEND_OP_ADD,											// VkBlendOp										colorBlendOp;
			vk::VK_BLEND_FACTOR_ONE,										// VkBlendFactor									srcAlphaBlendFactor;
			vk::VK_BLEND_FACTOR_ZERO,										// VkBlendFactor									dstAlphaBlendFactor;
			vk::VK_BLEND_OP_ADD,											// VkBlendOp										alphaBlendOp;
			vk::VK_COLOR_COMPONENT_R_BIT | vk::VK_COLOR_COMPONENT_G_BIT |
			vk::VK_COLOR_COMPONENT_B_BIT | vk::VK_COLOR_COMPONENT_A_BIT,	// VkColorComponentFlags							colorWriteMask;
		};

		const deUint32							colorAttachmentCount	= 2;
		const vk::VkPhysicalDeviceProperties	properties				= vk::getPhysicalDeviceProperties(instanceDriver, m_context.getPhysicalDevice());
		std::vector<vk::VkBool32>				colorWriteEnables		(properties.limits.maxColorAttachments);
		for (deUint32 i = 0; i < properties.limits.maxColorAttachments; ++i)
		{
			colorWriteEnables[i] = i < colorAttachmentCount ? VK_TRUE : VK_FALSE;
		}
		const vk::VkPipelineColorWriteCreateInfoEXT			colorWriteState =
		{
			vk::VK_STRUCTURE_TYPE_PIPELINE_COLOR_WRITE_CREATE_INFO_EXT,	// VkStructureType	sType;
			DE_NULL,													// const void*		pNext;
			(deUint32)colorWriteEnables.size(),							// uint32_t			attachmentCount;
			colorWriteEnables.data()									// const VkBool32*	pColorWriteEnables;
		};

		const vk::VkPipelineColorBlendStateCreateInfo		colorBlendState =
		{
			vk::VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType									sType;
			m_params.colorWrite ? &colorWriteState : DE_NULL,				// const void*										pNext;
			(vk::VkPipelineColorBlendStateCreateFlags)0,					// VkPipelineColorBlendStateCreateFlags				flags;
			m_params.logicOpEnable ? VK_TRUE : VK_FALSE,					// VkBool32											logicOpEnable;
			vk::VK_LOGIC_OP_COPY,											// VkLogicOp										logicOp;
			1u,																// uint32_t											attachmentCount;
			&colorBlendAttState,											// const VkPipelineColorBlendAttachmentState*		pAttachments;
			{ 0.0f, 0.0f, 0.0f, 0.0f },										// float											blendConstants[4];
		};

		vk::VkViewport	viewport	= { 0, 0, 32, 32, 0.0f, 1.0f, };
		vk::VkRect2D	scissor		= { { 0, 0, }, { 32, 32, }, };

		const auto&		edsFeatures				= m_context.getExtendedDynamicStateFeaturesEXT();
		deUint32		viewportAndScissorCount = edsFeatures.extendedDynamicState ? 0u : 1u;

		const vk::VkPipelineViewportDepthClipControlCreateInfoEXT		depthClipControlState =
		{
			vk::VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_DEPTH_CLIP_CONTROL_CREATE_INFO_EXT,	// VkStructureType	sType;
			DE_NULL,																	// const void*		pNext;
			VK_TRUE,																	// VkBool32			negativeOneToOne;
		};

		const vk::VkPipelineViewportStateCreateInfo						viewportState =
		{
			vk::VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,		// VkStructureType									sType
			m_params.depthClipControl ? &depthClipControlState : DE_NULL,	// const void*										pNext
			(vk::VkPipelineViewportStateCreateFlags)0,						// VkPipelineViewportStateCreateFlags				flags
			viewportAndScissorCount,										// deUint32											viewportCount
			&viewport,														// const VkViewport*								pViewports
			viewportAndScissorCount,										// deUint32											scissorCount
			&scissor														// const VkRect2D*									pScissors
		};

		const auto											dynamicStates	= getDynamicStates();

		const vk::VkPipelineDynamicStateCreateInfo			dynamicState	=
		{
			vk::VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,		// VkStructureType								sType
			DE_NULL,														// const void*									pNext
			0u,																// VkPipelineDynamicStateCreateFlags			flags
			(deUint32)dynamicStates.size(),									// deUint32										dynamicStateCount
			dynamicStates.data(),											// const VkDynamicState*						pDynamicStates
		};

		const vk::VkPipelineRenderingCreateInfo					pipelineRenderingCreateInfo =
		{
			vk::VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,	// VkStructureType	sType
			DE_NULL,												// const void*		pNext
			0u,														// uint32_t			viewMask
			1u,														// uint32_t			colorAttachmentCount
			&colorAttachmentFormat,									// const VkFormat*	pColorAttachmentFormats
			depthStencilAttachmentFormat,							// VkFormat			depthAttachmentFormat
			depthStencilAttachmentFormat,							// VkFormat			stencilAttachmentFormat
		};

		if (m_params.meshShader)
			pipeline = vk::makeGraphicsPipeline(vk, device, *pipelineLayout, VK_NULL_HANDLE, *meshShaderModule, *fragShaderModule, VK_NULL_HANDLE, {}, {}, 0u, &rasterizationState, &multisampleState, &depthStencilState, &colorBlendState, &dynamicState, 0u, &pipelineRenderingCreateInfo);
		else
			pipeline = vk::makeGraphicsPipeline(vk, device, *pipelineLayout, *vertShaderModule, *tescShaderModule, *teseShaderModule, *geomShaderModule, *fragShaderModule, VK_NULL_HANDLE, 0u, &vertexInputState, &inputAssemblyState, &tessellationState, &viewportState, &rasterizationState, &multisampleState, &depthStencilState, &colorBlendState, &dynamicState, &pipelineRenderingCreateInfo);
	}
	else
	{
		if (m_params.meshShader)
		{
			auto meshShaderCreateInfo = vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_MESH_BIT_EXT, binaries.get("mesh"), tessellationSupported, geometrySupported, &*descriptorSetLayout);
			meshShaderCreateInfo.flags = vk::VK_SHADER_CREATE_NO_TASK_SHADER_BIT_EXT;
			meshShader = vk::createShader(vk, device, meshShaderCreateInfo);
		}
		if (m_params.vertShader)
			vertShader = vk::createShader(vk, device, vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_VERTEX_BIT, binaries.get("vert"), tessellationSupported, geometrySupported, &*descriptorSetLayout));
		if (m_params.tessShader)
			tescShader = vk::createShader(vk, device, vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, binaries.get("tesc"), tessellationSupported, geometrySupported, &*descriptorSetLayout));
		if (m_params.tessShader)
			teseShader = vk::createShader(vk, device, vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, binaries.get("tese"), tessellationSupported, geometrySupported, &*descriptorSetLayout));
		if (m_params.geomShader)
			geomShader = vk::createShader(vk, device, vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_GEOMETRY_BIT, binaries.get("geom"), tessellationSupported, geometrySupported, &*descriptorSetLayout));
		if (m_params.fragShader)
			fragShader = vk::createShader(vk, device, vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_FRAGMENT_BIT, binaries.get("frag"), tessellationSupported, geometrySupported, &*descriptorSetLayout));
	}

	const vk::VkDeviceSize				tfBufSize		= 4 * sizeof(tcu::Vec4);
	const vk::VkBufferCreateInfo		tfBufCreateInfo = vk::makeBufferCreateInfo(tfBufSize, vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT | vk::VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT);
	const vk::Move<vk::VkBuffer>		tfBuf			= createBuffer(vk, device, &tfBufCreateInfo);
	const de::MovePtr<vk::Allocation>	tfBufAllocation	= alloc.allocate(getBufferMemoryRequirements(vk, device, *tfBuf), vk::MemoryRequirement::HostVisible);
	const vk::VkMemoryBarrier			tfMemoryBarrier	= vk::makeMemoryBarrier(vk::VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT, vk::VK_ACCESS_HOST_READ_BIT);
	vk.bindBufferMemory(device, *tfBuf, tfBufAllocation->getMemory(), tfBufAllocation->getOffset());

	const vk::VkClearValue				clearValue		= vk::makeClearValueColor({ 0.0f, 0.0f, 0.0f, 0.0f });
	const vk::VkClearValue				clearDepthValue = vk::makeClearValueDepthStencil(1.0f, 0u);
	vk::beginCommandBuffer(vk, *cmdBuffer);

	vk.cmdBindDescriptorSets(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0, 1, &descriptorSet.get(), 0, DE_NULL);

	vk::VkImageMemoryBarrier preImageBarrier = vk::makeImageMemoryBarrier(vk::VK_ACCESS_NONE, vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, vk::VK_IMAGE_LAYOUT_UNDEFINED, vk::VK_IMAGE_LAYOUT_GENERAL, **image, subresourceRange);
	vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, (vk::VkDependencyFlags)0u, 0u, (const vk::VkMemoryBarrier*)DE_NULL, 0u, (const vk::VkBufferMemoryBarrier*)DE_NULL, 1u, &preImageBarrier);

	vk::VkImageMemoryBarrier preDepthImageBarrier = vk::makeImageMemoryBarrier(vk::VK_ACCESS_NONE, vk::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, vk::VK_IMAGE_LAYOUT_UNDEFINED, vk::VK_IMAGE_LAYOUT_GENERAL, **depthImage, depthSubresourceRange);
	vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, vk::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, (vk::VkDependencyFlags)0u, 0u, (const vk::VkMemoryBarrier*)DE_NULL, 0u, (const vk::VkBufferMemoryBarrier*)DE_NULL, 1u, &preDepthImageBarrier);

	vk::beginRendering(vk, *cmdBuffer, *imageView, *depthImageView, true, renderArea, clearValue, clearDepthValue, vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_ATTACHMENT_LOAD_OP_CLEAR);

	if (m_params.pipeline)
	{
		vk.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
	}
	else
	{
		if (m_params.meshShader)
		{
			vk::VkShaderStageFlagBits stages[] = {
				vk::VK_SHADER_STAGE_MESH_BIT_EXT,
				vk::VK_SHADER_STAGE_FRAGMENT_BIT,
			};
			vk::VkShaderEXT shaders[] = {
				*meshShader,
				*fragShader,
			};
			vk::bindNullRasterizationShaders(vk, *cmdBuffer, m_context.getDeviceFeatures());
			vk.cmdBindShadersEXT(*cmdBuffer, 2, stages, shaders);
		}
		else
		{
			vk::bindGraphicsShaders(vk, *cmdBuffer, *vertShader, *tescShader, *teseShader, *geomShader, *fragShader, taskSupported, meshSupported);
		}
	}
	setDynamicStates(vk, *cmdBuffer);

	if (m_params.geometryStreams)
	{
		vk::VkDeviceSize offset = 0u;
		vk.cmdBindTransformFeedbackBuffersEXT(*cmdBuffer, 0, 1, &*tfBuf, &offset, &tfBufSize);
		vk.cmdBeginTransformFeedbackEXT(*cmdBuffer, 0, 0u, DE_NULL, DE_NULL);
	}

	bool secondDraw = !m_params.depthClamp && !m_params.depthClip;
	if (m_params.meshShader)
	{
		if (secondDraw)
			vk.cmdDrawMeshTasksEXT(*cmdBuffer, 2, 1, 1);
		else
			vk.cmdDrawMeshTasksEXT(*cmdBuffer, 1, 1, 1);
	}
	else
	{
		vk.cmdDraw(*cmdBuffer, 4, 1, 0, 0);
		if (secondDraw)
			vk.cmdDraw(*cmdBuffer, 4, 1, 0, 1);
	}
	if (m_params.geometryStreams)
		vk.cmdEndTransformFeedbackEXT(*cmdBuffer, 0, 0u, DE_NULL, DE_NULL);
	vk::endRendering(vk, *cmdBuffer);

	vk::VkImageMemoryBarrier postImageBarrier = vk::makeImageMemoryBarrier(vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, vk::VK_ACCESS_TRANSFER_READ_BIT, vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_IMAGE_LAYOUT_GENERAL, **image, subresourceRange);
	vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, (vk::VkDependencyFlags)0u, 0u, (const vk::VkMemoryBarrier*)DE_NULL, 0u, (const vk::VkBufferMemoryBarrier*)DE_NULL, 1u, &postImageBarrier);

	vk::VkImageMemoryBarrier postDepthImageBarrier = vk::makeImageMemoryBarrier(vk::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, vk::VK_ACCESS_TRANSFER_READ_BIT, vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_IMAGE_LAYOUT_GENERAL, **depthImage, depthSubresourceRange);
	vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, (vk::VkDependencyFlags)0u, 0u, (const vk::VkMemoryBarrier*)DE_NULL, 0u, (const vk::VkBufferMemoryBarrier*)DE_NULL, 1u, &postDepthImageBarrier);

	vk::VkBufferMemoryBarrier bufferBarrier = vk::makeBufferMemoryBarrier(vk::VK_ACCESS_SHADER_WRITE_BIT, vk::VK_ACCESS_HOST_READ_BIT, *outputBuffer, 0u, bufferSizeBytes);
	vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, (vk::VkDependencyFlags)0u, 0u, (const vk::VkMemoryBarrier*)DE_NULL, 1u, &bufferBarrier, 0u, (const vk::VkImageMemoryBarrier*)DE_NULL);

	if (m_params.geometryStreams)
		vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT, vk::VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &tfMemoryBarrier, 0u, DE_NULL, 0u, DE_NULL);


	const vk::VkBufferImageCopy	copyRegion = vk::makeBufferImageCopy(extent, subresourceLayers);
	vk.cmdCopyImageToBuffer(*cmdBuffer, **image, vk::VK_IMAGE_LAYOUT_GENERAL, **colorOutputBuffer, 1u, &copyRegion);

	vk::endCommandBuffer(vk, *cmdBuffer);
	vk::submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	tcu::ConstPixelBufferAccess resultBuffer = tcu::ConstPixelBufferAccess(vk::mapVkFormat(colorAttachmentFormat), renderArea.extent.width, renderArea.extent.height, 1, (const void*)colorOutputBuffer->getAllocation().getHostPtr());

	const deInt32			width		= resultBuffer.getWidth();
	const deInt32			height		= resultBuffer.getHeight();
	const float				threshold	= 1.0f / 256.0f;
	tcu::Vec4				whiteColor	= tcu::Vec4(0.75f);
	tcu::Vec4				blackColor	= tcu::Vec4(0.0f);

	const vk::Allocation& outputBufferAllocation = outputBuffer.getAllocation();
	invalidateAlloc(vk, device, outputBufferAllocation);

	const deUint32* bufferPtr = static_cast<deUint32*>(outputBufferAllocation.getHostPtr());

	if (m_params.geometryStreams)
	{
		invalidateAlloc(vk, device, *tfBufAllocation);
		const float* tfData = static_cast<float*>(tfBufAllocation->getHostPtr());
		deUint32 count = m_params.lines ? 2 : 3;
		for (deUint32 i = 0; i < count; ++i)
		{
			for (deUint32 j = 0; j < 4; ++j)
			{
				if (tfData[i * 4 + j] != float(i + 1))
				{
					return tcu::TestStatus::fail("Fail");
				}
			}
		}
		return tcu::TestStatus::pass("Pass");
	}

	if (m_params.vertShader)
	{
		if (bufferPtr[0] != 1u)
		{
			log << tcu::TestLog::Message << "Buffer value at index 0 was expected to be 1, but was[" << bufferPtr[0] << tcu::TestLog::EndMessage;
			return tcu::TestStatus::fail("Fail");
		}
	}

	if (m_params.tessShader)
	{
		if (bufferPtr[1] != 2u)
		{
			log << tcu::TestLog::Message << "Buffer value at index 1 was expected to be 2, but was[" << bufferPtr[1] << tcu::TestLog::EndMessage;
			return tcu::TestStatus::fail("Fail");
		}
		if (bufferPtr[2] != 3u)
		{
			log << tcu::TestLog::Message << "Buffer value at index 2 was expected to be 3, but was[" << bufferPtr[2] << tcu::TestLog::EndMessage;
			return tcu::TestStatus::fail("Fail");
		}
	}

	if (m_params.geomShader)
	{
		if (bufferPtr[3] != 4u)
		{
			log << tcu::TestLog::Message << "Buffer value at index 3 was expected to be 4, but was[" << bufferPtr[3] << tcu::TestLog::EndMessage;
			return tcu::TestStatus::fail("Fail");
		}
	}

	if (m_params.fragShader && !m_params.rasterizerDiscardEnable)
	{
		for (deInt32 j = 0; j < height; ++j)
		{
			for (deInt32 i = 0; i < width; ++i)
			{
				const tcu::Vec4 color = resultBuffer.getPixel(i, j).asFloat();

				tcu::Vec4 expectedColor = blackColor;
				bool inside = isInsidePrimitive(i, j, width, height);
				if (m_params.conservativeRasterization && m_params.conservativeRasterizationOverestimate && !inside)
					continue;
				if (inside && (!m_params.cull || m_params.lines) && (!m_params.colorWrite || m_params.colorWriteEnable))
				{
					if (!m_params.depthBoundsTestEnable && (!m_params.depthClip || i < 16) && !m_params.discardRectanglesEnable)
					{
						expectedColor = whiteColor;
						if (m_params.alphaToOne)
							expectedColor.w() = 1.0f;
						if (m_params.colorBlendEnable && secondDraw && !m_params.logicOpEnable && !m_params.stencilTestEnable)
							expectedColor = tcu::Vec4(1.0f);
					}
				}

				if (deFloatAbs(color.x() - expectedColor.x()) > threshold || deFloatAbs(color.y() - expectedColor.y()) > threshold || deFloatAbs(color.z() - expectedColor.z()) > threshold || deFloatAbs(color.w() - expectedColor.w()) > threshold)
				{
					log << tcu::TestLog::Message << "Color at (" << i << ", " << j << ") is expected to be (" << expectedColor << "), but was (" << color << ")" << tcu::TestLog::EndMessage;
					return tcu::TestStatus::fail("Fail");
				}
			}
		}
	}

	if (m_params.fragShader && !m_params.rasterizerDiscardEnable)
	{
		const auto	depthBuffer = readDepthAttachment(vk, device, queue, queueFamilyIndex, alloc, **depthImage, depthStencilAttachmentFormat, tcu::UVec2(width, height), vk::VK_IMAGE_LAYOUT_GENERAL);
		const auto	depthAccess = depthBuffer->getAccess();
		const auto	stencilBuffer = readStencilAttachment(vk, device, queue, queueFamilyIndex, alloc, **depthImage, depthStencilAttachmentFormat, tcu::UVec2(width, height), vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		const auto	stencilAccess = stencilBuffer->getAccess();
		const float depthEpsilon = 0.02f;

		for (deInt32 j = 0; j < height; ++j)
		{
			for (deInt32 i = 0; i < width; ++i)
			{
				const float depth = depthAccess.getPixDepth(i, j);
				const int stencil = stencilAccess.getPixStencil(i, j);
				bool inside = isInsidePrimitive(i, j, width, height);
				if (m_params.conservativeRasterization && m_params.conservativeRasterizationOverestimate && !inside)
					continue;
				if (inside && !m_params.depthBoundsTestEnable && !m_params.discardRectanglesEnable && (!m_params.cull || m_params.lines))
				{
					float depthMin = 0.4f - depthEpsilon;
					float depthMax = 0.6f + depthEpsilon;
					if (m_params.stencilTestEnable)
					{
						depthMin = 0.7f - depthEpsilon;
						depthMax = 0.9f + depthEpsilon;
					}
					if (m_params.depthClamp)
					{
						depthMin = 0.35f - depthEpsilon;
						depthMax = 0.45f + depthEpsilon;
					}
					if (m_params.depthClip)
					{
						depthMin = 0.9f - depthEpsilon;
						depthMax = 1.0f + depthEpsilon;
					}
					if (m_params.depthClipControl)
					{
						depthMin = 0.7f - depthEpsilon;
						depthMax = 1.0f + depthEpsilon;
					}
					if (m_params.depthBiasEnable)
					{
						if (m_params.lines)
						{
							depthMin += 0.004f;
							depthMax += 0.004f;
						}
						else
						{
							depthMin += 0.03f;
							depthMax += 0.03f;
						}
					}

					if (depth < depthMin || depth > depthMax)
					{
						log << tcu::TestLog::Message << "Depth at (" << i << ", " << j << ") is expected to be between 0.4f and 0.6f, but was (" << depth << ")" << tcu::TestLog::EndMessage;
						return tcu::TestStatus::fail("Fail");
					}
					if (m_params.stencilTestEnable && (!m_params.depthClip || i < 16))
					{
						if (stencil != 255)
						{
							log << tcu::TestLog::Message << "Stencil at (" << i << ", " << j << ") is expected to be 0, but was (" << stencil << ")" << tcu::TestLog::EndMessage;
							return tcu::TestStatus::fail("Fail");
						}
					}
				}
				else
				{
					if (deFloatAbs(depth - 1.0f) > depthEpsilon)
					{
						log << tcu::TestLog::Message << "Depth at (" << i << ", " << j << ") is expected to be 1.0f, but was (" << depth << ")" << tcu::TestLog::EndMessage;
						return tcu::TestStatus::fail("Fail");
					}
					if (m_params.stencilTestEnable)
					{
						if (stencil != 0)
						{
							log << tcu::TestLog::Message << "Stencil at (" << i << ", " << j << ") is expected to be 1, but was (" << stencil << ")" << tcu::TestLog::EndMessage;
							return tcu::TestStatus::fail("Fail");
						}
					}
				}
			}
		}
	}

	if (m_params.meshShader)
	{
		if (bufferPtr[4] != 5u)
		{
			log << tcu::TestLog::Message << "Buffer value at index 5 was expected to be 6, but was[" << bufferPtr[5] << tcu::TestLog::EndMessage;
			return tcu::TestStatus::fail("Fail");
		}
	}

	return tcu::TestStatus::pass("Pass");
}

class ShaderObjectStateCase : public vkt::TestCase
{
public:
							ShaderObjectStateCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const StateTestParams& testParams)
													: vkt::TestCase		(testCtx, name, description)
													, m_params			(testParams)
													{}
	virtual					~ShaderObjectStateCase	(void) {}

	void					checkSupport			(vkt::Context& context) const override;
	virtual void			initPrograms			(vk::SourceCollections& programCollection) const override;
	TestInstance*			createInstance			(Context& context) const override { return new ShaderObjectStateInstance(context, m_params); }

	const StateTestParams	m_params;
};

void ShaderObjectStateCase::checkSupport (Context& context) const
{
	const auto& edsFeatures = context.getExtendedDynamicStateFeaturesEXT();
	const auto& eds2Features = context.getExtendedDynamicState2FeaturesEXT();
	const auto& eds3Features = context.getExtendedDynamicState3FeaturesEXT();

	const auto&						vki					= context.getInstanceInterface();
	const auto						physicalDevice		= context.getPhysicalDevice();
	if (findDSFormat(vki, physicalDevice) == vk::VK_FORMAT_UNDEFINED)
		TCU_THROW(NotSupportedError, "Required depth/stencil format not supported");

	if (!m_params.pipeline)
		context.requireDeviceFunctionality("VK_EXT_shader_object");
	else
		context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");

	if (m_params.logicOp)
	{
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_LOGIC_OP);
		if (m_params.pipeline && !eds2Features.extendedDynamicState2LogicOp)
			TCU_THROW(NotSupportedError, "extendedDynamicState2LogicOp not supported");
	}
	if (m_params.alphaToOne)
	{
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_ALPHA_TO_ONE);
		if (m_params.pipeline && !eds3Features.extendedDynamicState3AlphaToOneEnable)
			TCU_THROW(NotSupportedError, "extendedDynamicState3AlphaToOneEnable not supported");
	}
	if (m_params.depthBounds)
	{
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_DEPTH_BOUNDS);
		if (m_params.pipeline && !edsFeatures.extendedDynamicState)
			TCU_THROW(NotSupportedError, "extendedDynamicState not supported");
	}
	if (m_params.depthClamp)
	{
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_DEPTH_CLAMP);
		if (m_params.pipeline && !eds3Features.extendedDynamicState3DepthClampEnable)
			TCU_THROW(NotSupportedError, "extendedDynamicState3DepthClampEnable not supported");
	}
	if (m_params.depthClip)
	{
		context.requireDeviceFunctionality("VK_EXT_depth_clip_enable");
		if (!context.getDepthClipEnableFeaturesEXT().depthClipEnable)
			TCU_THROW(NotSupportedError, "depthClipEnable not supported");
		if (m_params.pipeline && !eds3Features.extendedDynamicState3DepthClipEnable)
			TCU_THROW(NotSupportedError, "extendedDynamicState3DepthClipEnable not supported");
	}
	if (m_params.depthClipControl)
	{
		context.requireDeviceFunctionality("VK_EXT_depth_clip_control");
		if (!context.getDepthClipControlFeaturesEXT().depthClipControl)
			TCU_THROW(NotSupportedError, "depthClipControl not supported");
		if (m_params.pipeline && !eds3Features.extendedDynamicState3DepthClipNegativeOneToOne)
			TCU_THROW(NotSupportedError, "extendedDynamicState3DepthClipNegativeOneToOne not supported");
	}
	if (m_params.colorWrite)
	{
		context.requireDeviceFunctionality("VK_EXT_color_write_enable");
		if (!context.getColorWriteEnableFeaturesEXT().colorWriteEnable)
			TCU_THROW(NotSupportedError, "colorWriteEnable not supported");
	}
	if (m_params.geometryStreams)
	{
		context.requireDeviceFunctionality("VK_EXT_transform_feedback");
		if (!context.getTransformFeedbackFeaturesEXT().geometryStreams)
			TCU_THROW(NotSupportedError, "geometryStreams not supported");
		if (m_params.pipeline && !eds3Features.extendedDynamicState3RasterizationStream)
			TCU_THROW(NotSupportedError, "extendedDynamicState3RasterizationStream not supported");
	}
	if (m_params.discardRectangles)
	{
		context.requireDeviceFunctionality("VK_EXT_discard_rectangles");

		deUint32									propertyCount = 0u;
		std::vector<vk::VkExtensionProperties>		extensionsProperties;
		context.getInstanceInterface().enumerateDeviceExtensionProperties(context.getPhysicalDevice(), DE_NULL, &propertyCount, DE_NULL);
		extensionsProperties.resize(propertyCount);
		context.getInstanceInterface().enumerateDeviceExtensionProperties(context.getPhysicalDevice(), DE_NULL, &propertyCount, extensionsProperties.data());

		for (const auto& extProp : extensionsProperties)
		{
			if (strcmp(extProp.extensionName, "VK_EXT_discard_rectangles") == 0)
			{
				if (extProp.specVersion < 2)
					TCU_THROW(NotSupportedError, "VK_EXT_discard_rectangles is version 1. Needs version 2 or higher");
			}
		}
	}
	if (m_params.conservativeRasterization)
	{
		context.requireDeviceFunctionality("VK_EXT_conservative_rasterization");
		if (m_params.pipeline && !eds3Features.extendedDynamicState3ConservativeRasterizationMode)
			TCU_THROW(NotSupportedError, "extendedDynamicState3ConservativeRasterizationMode not supported");
	}
	if (m_params.sampleLocations)
	{
		context.requireDeviceFunctionality("VK_EXT_sample_locations");
		if (m_params.sampleLocationsEnable && (context.getSampleLocationsPropertiesEXT().sampleLocationSampleCounts & vk::VK_SAMPLE_COUNT_1_BIT) == 0)
			TCU_THROW(NotSupportedError, "VK_SAMPLE_COUNT_1_BIT not supported in sampleLocationSampleCounts");
	}
	if (m_params.provokingVertex)
	{
		context.requireDeviceFunctionality("VK_EXT_provoking_vertex");
		if (m_params.pipeline && !eds3Features.extendedDynamicState3ProvokingVertexMode)
			TCU_THROW(NotSupportedError, "extendedDynamicState3ProvokingVertexMode not supported");
	}
	if (m_params.lineRasterization)
	{
		context.requireDeviceFunctionality("VK_EXT_line_rasterization");
		if (!context.getLineRasterizationFeaturesEXT().rectangularLines)
			TCU_THROW(NotSupportedError, "rectangularLines not supported");
		if (m_params.pipeline && !eds3Features.extendedDynamicState3LineRasterizationMode)
			TCU_THROW(NotSupportedError, "extendedDynamicState3LineRasterizationMode not supported");
		if (m_params.pipeline && !eds3Features.extendedDynamicState3LineStippleEnable)
			TCU_THROW(NotSupportedError, "extendedDynamicState3LineStippleEnable not supported");
		if (m_params.stippledLineEnable && !context.getLineRasterizationFeaturesEXT().stippledRectangularLines)
			TCU_THROW(NotSupportedError, "stippledRectangularLines not supported");
	}
	if (m_params.geomShader)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);
	if (m_params.tessShader)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_TESSELLATION_SHADER);
	if (m_params.meshShader)
	{
		context.requireDeviceFunctionality("VK_EXT_mesh_shader");
		if (!context.getMeshShaderFeaturesEXT().meshShader)
			TCU_THROW(NotSupportedError, "Mesh shaders not supported");
	}
	if (m_params.lines)
	{
		if (m_params.pipeline && !edsFeatures.extendedDynamicState)
			TCU_THROW(NotSupportedError, "extendedDynamicState not supported");
	}
	if (m_params.colorBlendEnable && m_params.pipeline)
	{
		context.requireDeviceFunctionality("VK_EXT_extended_dynamic_state3");
		if (!eds3Features.extendedDynamicState3ColorBlendEnable)
			TCU_THROW(NotSupportedError, "extendedDynamicState3ColorBlendEnable not supported");
	}
}

void ShaderObjectStateCase::initPrograms (vk::SourceCollections& programCollection) const
{
	std::stringstream vert;
	std::stringstream geom;
	std::stringstream tesc;
	std::stringstream tese;
	std::stringstream frag;

	vert
		<< "#version 450\n"
		<< "layout(binding = 0) buffer Output {\n"
		<< "    uint values[8];\n"
		<< "} buffer_out;\n\n"
		<< "void main() {\n"
		<< "    vec2 pos = vec2(float(gl_VertexIndex & 1), float((gl_VertexIndex >> 1) & 1)) - vec2(0.0001f);\n";
	if (m_params.depthClip)
		vert << "    float z = 0.9f;\n";
	else
		vert << "    float z = 0.7f;\n";
	vert << "    if ((gl_VertexIndex & 1) > 0)\n"
		<< "        z += 0.2f;\n"
		<< "    if ((gl_InstanceIndex & 1) > 0)\n"
		<< "        z -= 0.3f;\n"
		<< "    gl_Position = vec4(pos - 0.5f, z, 1.0f);\n"
		<< "	if (gl_VertexIndex == 0)\n"
		<< "        buffer_out.values[0] = 1u;\n"
		<< "}\n";

	tesc
		<< "#version 450\n"
		<< "layout(vertices = 4) out;\n"
		<< "layout(binding = 0) buffer Output {\n"
		<< "    uint values[8];\n"
		<< "} buffer_out;\n\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "    if (gl_InvocationID == 0) {\n"
		<< "		gl_TessLevelInner[0] = 1.0;\n"
		<< "		gl_TessLevelInner[1] = 1.0;\n"
		<< "		gl_TessLevelOuter[0] = 1.0;\n"
		<< "		gl_TessLevelOuter[1] = 1.0;\n"
		<< "		gl_TessLevelOuter[2] = 1.0;\n"
		<< "		gl_TessLevelOuter[3] = 1.0;\n"
		<< "        buffer_out.values[1] = 2u;\n"
		<< "	}\n"
		<< "    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
		<< "}\n";

	tese
		<< "#version 450\n";
	if (m_params.lines)
		tese << "layout(isolines, equal_spacing) in;\n";
	else
		tese << "layout(quads, equal_spacing) in;\n";
	tese << "layout(binding = 0) buffer Output {\n"
		<< "    uint values[8];\n"
		<< "} buffer_out;\n\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	float u = gl_TessCoord.x;\n"
		<< "	float v = gl_TessCoord.y;\n"
		<< "	float omu = 1.0f - u;\n"
		<< "	float omv = 1.0f - v;\n"
		<< "	gl_Position = omu * omv * gl_in[0].gl_Position + u * omv * gl_in[2].gl_Position + u * v * gl_in[3].gl_Position + omu * v * gl_in[1].gl_Position;\n"
		<< "	gl_Position.x *= 1.5f;\n"
		<< "    if (gl_PrimitiveID == 0u)\n"
		<< "		buffer_out.values[2] = 3u;\n"
		<< "}\n";

	geom
		<< "#version 450\n";
	if (m_params.lines)
		geom << "layout(lines) in;\n";
	else
		geom << "layout(triangles) in;\n";
	if (m_params.lines)
		geom << "layout(line_strip, max_vertices = 4) out;\n";
	else
		geom << "layout(triangle_strip, max_vertices = 4) out;\n";
	if (m_params.geometryStreams)
		geom << "layout(stream = 0, xfb_buffer = 0, xfb_offset = 0, xfb_stride = 16, location = 0) out vec4 out0;\n";
	geom << "layout(binding = 0) buffer Output {\n"
		<< "    uint values[8];\n"
		<< "} buffer_out;\n\n"
		<< "void main(void)\n"
		<< "{\n"
		<< "    gl_Position = gl_in[0].gl_Position;\n"
		<< "    gl_Position.y *= 1.5f;\n";
	if (m_params.geometryStreams)
		geom << "    out0 = vec4(1.0f);\n"
			 << "    EmitStreamVertex(0);\n";
	else
		geom << "    EmitVertex();\n";
	geom << "    gl_Position = gl_in[1].gl_Position;\n"
		<< "    gl_Position.y *= 1.5f;\n";
	if (m_params.geometryStreams)
		geom << "    out0 = vec4(2.0f);\n"
			 << "    EmitStreamVertex(0);\n";
	else
		geom << "    EmitVertex();\n";
	if (!m_params.lines)
	{
		geom << "    gl_Position = gl_in[2].gl_Position;\n"
			<< "    gl_Position.y *= 1.5f;\n";
		if (m_params.geometryStreams)
			geom << "    out0 = vec4(3.0f);\n"
				 << "    EmitStreamVertex(0);\n";
		else
			geom << "    EmitVertex();\n";
	}
	if (m_params.geometryStreams)
		geom << "    EndStreamPrimitive(0);\n";
	else
		geom << "    EndPrimitive();\n";
	geom << "    buffer_out.values[3] = 4u;\n";
	geom << "}\n";

	frag
		<< "#version 450\n"
		<< "layout (location=0) out vec4 outColor;\n"
		<< "void main() {\n"
		<< "    outColor = vec4(0.75f);\n"
		<< "}\n";

	programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());
	programCollection.glslSources.add("tesc") << glu::TessellationControlSource(tesc.str());
	programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(tese.str());
	programCollection.glslSources.add("geom") << glu::GeometrySource(geom.str());
	programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());

	if (m_params.meshShader)
	{
		std::stringstream mesh;

		mesh
			<< "#version 460\n"
			<< "#extension GL_EXT_mesh_shader : require\n"
			<< "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
			<< "layout(max_vertices = 4) out;\n"
			<< "layout(max_primitives = 2) out;\n";
		if (m_params.lines)
			mesh << "layout(lines) out;\n";
		else
			mesh << "layout(triangles) out;\n";
		mesh
			<< "layout(binding = 0) buffer Output {\n"
			<< "    uint values[8];\n"
			<< "} buffer_out;\n\n"
			<< "void main() {\n"
			<< "    SetMeshOutputsEXT(4u, 2u);\n";
		if (m_params.depthClip)
			mesh << "    float z = 0.9f;\n";
		else
			mesh << "    float z = 0.7f;\n";
		mesh
			<< "    if (gl_GlobalInvocationID.x == 1) z -= 0.3f;\n"
			<< "    gl_MeshVerticesEXT[0].gl_Position = vec4(-0.5f, -0.5f, z, 1.0f);\n"
			<< "    gl_MeshVerticesEXT[1].gl_Position = vec4(-0.5f, 0.5f, z, 1.0f);\n"
			<< "    gl_MeshVerticesEXT[2].gl_Position = vec4(0.5f, -0.5f, z + 0.2f, 1.0f);\n"
			<< "    gl_MeshVerticesEXT[3].gl_Position = vec4(0.5f, 0.5f, z + 0.2f, 1.0f);\n";
		if (m_params.lines)
			mesh << "    gl_PrimitiveLineIndicesEXT[0] = uvec2(0u, 2u);\n"
				 << "    gl_PrimitiveLineIndicesEXT[1] = uvec2(1u, 3u);\n";
		else
			mesh << "    gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0u, 1u, 2u);\n"
				 << "    gl_PrimitiveTriangleIndicesEXT[1] = uvec3(1u, 3u, 2u);\n";
		mesh
			<< "    buffer_out.values[4] = 5u;\n"
			<< "}\n";

		programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
	}
}

struct UnusedBuiltinParams
{
	bool linked;
	vk::VkShaderStageFlagBits stage;
	bool builtin;
};

enum TessellationSpacing
{
	EQUAL,
	EVEN,
	ODD,
};

struct TessellationModesParams
{
	deUint32 subdivision;
	TessellationSpacing spacing;
};

class ShaderObjectUnusedBuiltinInstance : public vkt::TestInstance
{
public:
							ShaderObjectUnusedBuiltinInstance	(Context& context, const UnusedBuiltinParams& params)
																: vkt::TestInstance	(context)
																, m_params			(params)
																{}
	virtual					~ShaderObjectUnusedBuiltinInstance	(void) {}

	tcu::TestStatus			iterate								(void) override;
private:
	UnusedBuiltinParams m_params;
};

tcu::TestStatus ShaderObjectUnusedBuiltinInstance::iterate (void)
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

	const auto&					binaries			= m_context.getBinaryCollection();
	vk::VkShaderEXT				shaders[5];

	vk::VkShaderCreateInfoEXT	shaderCreateInfos[5]
	{
		vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_VERTEX_BIT, binaries.get("vert"), tessellationSupported, geometrySupported),
		vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, binaries.get("tesc"), tessellationSupported, geometrySupported),
		vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, binaries.get("tese"), tessellationSupported, geometrySupported),
		vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_GEOMETRY_BIT, binaries.get("geom"), tessellationSupported, geometrySupported),
		vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_FRAGMENT_BIT, binaries.get("frag"), tessellationSupported, geometrySupported),
	};

	vk.createShadersEXT(device, 5u, shaderCreateInfos, DE_NULL, shaders);

	if (m_params.linked)
		for (auto& ci : shaderCreateInfos)
			ci.flags |= vk::VK_SHADER_CREATE_LINK_STAGE_BIT_EXT;

	const vk::Move<vk::VkCommandPool>	cmdPool		(vk::createCommandPool(vk, device, 0u, queueFamilyIndex));
	const vk::Move<vk::VkCommandBuffer>	cmdBuffer	(allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	vk::beginCommandBuffer(vk, *cmdBuffer);

	vk::VkImageMemoryBarrier preImageBarrier = vk::makeImageMemoryBarrier(vk::VK_ACCESS_NONE, vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, vk::VK_IMAGE_LAYOUT_UNDEFINED, vk::VK_IMAGE_LAYOUT_GENERAL, **image, subresourceRange);
	vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, (vk::VkDependencyFlags)0u, 0u, (const vk::VkMemoryBarrier*)DE_NULL, 0u, (const vk::VkBufferMemoryBarrier*)DE_NULL, 1u, &preImageBarrier);

	const vk::VkClearValue				clearValue = vk::makeClearValueColor({ 0.0f, 0.0f, 0.0f, 0.0f });
	vk::beginRendering(vk, *cmdBuffer, *imageView, renderArea, clearValue, vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_ATTACHMENT_LOAD_OP_CLEAR);

	vk::bindGraphicsShaders(vk, *cmdBuffer, shaders[0], shaders[1], shaders[2], shaders[3], shaders[4], taskSupported, meshSupported);
	vk::setDefaultShaderObjectDynamicStates(vk, *cmdBuffer, deviceExtensions, vk::VK_PRIMITIVE_TOPOLOGY_PATCH_LIST);

	vk.cmdDraw(*cmdBuffer, 4, 1, 0, 0);

	vk::endRendering(vk, *cmdBuffer);

	vk::VkImageMemoryBarrier postImageBarrier = vk::makeImageMemoryBarrier(vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, vk::VK_ACCESS_TRANSFER_READ_BIT, vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_IMAGE_LAYOUT_GENERAL, **image, subresourceRange);
	vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, (vk::VkDependencyFlags)0u, 0u, (const vk::VkMemoryBarrier*)DE_NULL, 0u, (const vk::VkBufferMemoryBarrier*)DE_NULL, 1u, &postImageBarrier);

	const vk::VkBufferImageCopy	copyRegion = vk::makeBufferImageCopy(extent, subresourceLayers);
	vk.cmdCopyImageToBuffer(*cmdBuffer, **image, vk::VK_IMAGE_LAYOUT_GENERAL, **colorOutputBuffer, 1u, &copyRegion);

	vk::endCommandBuffer(vk, *cmdBuffer);
	vk::submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	for (deUint32 i = 0u; i < 5u; ++i)
		vk.destroyShaderEXT(device, shaders[i], DE_NULL);

	tcu::ConstPixelBufferAccess resultBuffer = tcu::ConstPixelBufferAccess(vk::mapVkFormat(colorAttachmentFormat), renderArea.extent.width, renderArea.extent.height, 1, (const void*)colorOutputBuffer->getAllocation().getHostPtr());

	const tcu::Vec4			black		= tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f);
	const tcu::Vec4			white		= tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f);
	const deUint32			width		= resultBuffer.getWidth();
	const deUint32			height		= resultBuffer.getHeight();
	const deUint32			xOffset		= 4u;
	const deUint32			yOffset		= 4u;

	for (deUint32 j = 0; j < height; ++j)
	{
		for (deUint32 i = 0; i < width; ++i)
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

class ShaderObjectUnusedBuiltinCase : public vkt::TestCase
{
public:
							ShaderObjectUnusedBuiltinCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const UnusedBuiltinParams& testParams)
															: vkt::TestCase		(testCtx, name, description)
															, m_params			(testParams)
															{}
	virtual					~ShaderObjectUnusedBuiltinCase	(void) {}

	void					checkSupport					(vkt::Context& context) const override;
	virtual void			initPrograms					(vk::SourceCollections& programCollection) const override;
	TestInstance*			createInstance					(Context& context) const override { return new ShaderObjectUnusedBuiltinInstance(context, m_params); }
private:
	const UnusedBuiltinParams m_params;
};

void ShaderObjectUnusedBuiltinCase::checkSupport (vkt::Context& context) const
{
	context.requireDeviceFunctionality("VK_EXT_shader_object");
	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);
	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_TESSELLATION_SHADER);
}

void ShaderObjectUnusedBuiltinCase::initPrograms (vk::SourceCollections& programCollection) const
{
	std::stringstream vert;
	std::stringstream geom;
	std::stringstream tesc;
	std::stringstream tese;
	std::stringstream frag;

	vert
		<< "#version 450\n";
	if (m_params.stage == vk::VK_SHADER_STAGE_VERTEX_BIT && !m_params.builtin)
		vert << "layout(location = 0) out vec4 unused;\n";
	vert
		<< "void main() {\n"
		<< "    vec2 pos = vec2(float(gl_VertexIndex & 1), float((gl_VertexIndex >> 1) & 1));\n"
		<< "    gl_Position = vec4(pos - 0.5f, 0.0f, 1.0f);\n";
	if (m_params.stage == vk::VK_SHADER_STAGE_VERTEX_BIT)
	{
		if (m_params.builtin)
		{
			vert << "    gl_PointSize = 16.0f;\n";
			vert << "    gl_ClipDistance[0] = 2.0f;\n";
		}
		else
		{
			vert << "    unused = vec4(1.0f);\n";
		}
	}
	vert << "}\n";

	tesc
		<< "#version 450\n"
		<< "\n"
		<< "layout(vertices = 4) out;\n";
	if (m_params.stage == vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT && !m_params.builtin)
		tesc << "layout(location = 0) out vec4 unused[];\n";
	tesc << "\n"
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
		<< "    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n";
	if (m_params.stage == vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
	{
		if (m_params.builtin)
		{
			tesc << "    gl_out[gl_InvocationID].gl_PointSize = 16.0f;\n";
			tesc << "    gl_out[gl_InvocationID].gl_ClipDistance[0] = 2.0f;\n";
		}
		else
		{
			tesc << "    unused[gl_InvocationID] = vec4(1.0f);\n";
		}
	}
	tesc << "}\n";

	tese
		<< "#version 450\n"
		<< "\n"
		<< "layout(quads, equal_spacing) in;\n";
	if (m_params.stage == vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT && !m_params.builtin)
		tese << "layout(location = 0) out vec4 unused;\n";
	tese << "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	float u = gl_TessCoord.x;\n"
		<< "	float v = gl_TessCoord.y;\n"
		<< "	float omu = 1.0f - u;\n"
		<< "	float omv = 1.0f - v;\n"
		<< "	gl_Position = omu * omv * gl_in[0].gl_Position + u * omv * gl_in[2].gl_Position + u * v * gl_in[3].gl_Position + omu * v * gl_in[1].gl_Position;\n"
		<< "	gl_Position.x *= 1.5f;\n";
	if (m_params.stage == vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
	{
		if (m_params.builtin)
		{
			tese << "    gl_PointSize = 16.0f;\n";
			tese << "    gl_ClipDistance[0] = 2.0f;\n";
		}
		else
		{
			tese << "    unused = vec4(1.0f);\n";
		}
	}
	tese << "}\n";

	geom
		<< "#version 450\n"
		<< "layout(triangles) in;\n"
		<< "layout(triangle_strip, max_vertices = 4) out;\n";
	if (m_params.stage == vk::VK_SHADER_STAGE_GEOMETRY_BIT && !m_params.builtin)
		geom << "layout(location = 0) out vec4 unused;\n";
	geom << "\n"
		<< "void main(void)\n"
		<< "{\n"
		<< "    gl_Position = gl_in[0].gl_Position;\n"
		<< "	gl_Position.y *= 1.5f;\n"
		<< "    gl_Position.z = 0.5f;\n"
		<< "    EmitVertex();\n"
		<< "    gl_Position = gl_in[1].gl_Position;\n"
		<< "	gl_Position.y *= 1.5f;\n"
		<< "    gl_Position.z = 0.5f;\n"
		<< "    EmitVertex();\n"
		<< "    gl_Position = gl_in[2].gl_Position;\n"
		<< "	gl_Position.y *= 1.5f;\n"
		<< "    gl_Position.z = 0.5f;\n"
		<< "    EmitVertex();\n"
		<< "    EndPrimitive();\n";
	if (m_params.stage == vk::VK_SHADER_STAGE_GEOMETRY_BIT)
	{
		if (m_params.builtin)
			geom << "    gl_PointSize = 16.0f;\n";
		else
			geom << "    unused = vec4(1.0f);\n";
	}
	geom << "}\n";

	frag
		<< "#version 450\n"
		<< "layout (location=0) out vec4 outColor;\n"
		<< "void main() {\n"
		<< "    outColor = vec4(1.0f);\n"
		<< "}\n";

	programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());
	programCollection.glslSources.add("tesc") << glu::TessellationControlSource(tesc.str());
	programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(tese.str());
	programCollection.glslSources.add("geom") << glu::GeometrySource(geom.str());
	programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

class ShaderObjectTessellationModesInstance : public vkt::TestInstance
{
public:
							ShaderObjectTessellationModesInstance	(Context& context, const TessellationModesParams& params)
																	: vkt::TestInstance	(context)
																	, m_params			(params)
																	{}
	virtual					~ShaderObjectTessellationModesInstance	(void) {}

	tcu::TestStatus			iterate									(void) override;
private:
	TessellationModesParams m_params;
};

tcu::TestStatus ShaderObjectTessellationModesInstance::iterate (void)
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

	const auto&					binaries			= m_context.getBinaryCollection();
	const auto					vertShader			= vk::createShader(vk, device, vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_VERTEX_BIT, binaries.get("vert"), tessellationSupported, geometrySupported));
	const auto					tescShader			= vk::createShader(vk, device, vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, binaries.get("tesc"), tessellationSupported, geometrySupported));
	const auto					teseShader			= vk::createShader(vk, device, vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, binaries.get("tese"), tessellationSupported, geometrySupported));
	const auto					fragShader			= vk::createShader(vk, device, vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_FRAGMENT_BIT, binaries.get("frag"), tessellationSupported, geometrySupported));

	const vk::Move<vk::VkCommandPool>	cmdPool		(vk::createCommandPool(vk, device, 0u, queueFamilyIndex));
	const vk::Move<vk::VkCommandBuffer>	cmdBuffer	(allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	vk::beginCommandBuffer(vk, *cmdBuffer);

	vk::VkImageMemoryBarrier preImageBarrier = vk::makeImageMemoryBarrier(vk::VK_ACCESS_NONE, vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, vk::VK_IMAGE_LAYOUT_UNDEFINED, vk::VK_IMAGE_LAYOUT_GENERAL, **image, subresourceRange);
	vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, (vk::VkDependencyFlags)0u, 0u, (const vk::VkMemoryBarrier*)DE_NULL, 0u, (const vk::VkBufferMemoryBarrier*)DE_NULL, 1u, &preImageBarrier);

	const vk::VkClearValue				clearValue = vk::makeClearValueColor({ 0.0f, 0.0f, 0.0f, 0.0f });
	vk::beginRendering(vk, *cmdBuffer, *imageView, renderArea, clearValue, vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_ATTACHMENT_LOAD_OP_CLEAR);

	vk::bindGraphicsShaders(vk, *cmdBuffer, *vertShader, *tescShader, *teseShader, VK_NULL_HANDLE, *fragShader, taskSupported, meshSupported);
	vk::setDefaultShaderObjectDynamicStates(vk, *cmdBuffer, deviceExtensions, vk::VK_PRIMITIVE_TOPOLOGY_PATCH_LIST);

	vk.cmdSetPolygonModeEXT(*cmdBuffer, vk::VK_POLYGON_MODE_LINE);

	vk.cmdDraw(*cmdBuffer, 4, 1, 0, 0);

	vk::endRendering(vk, *cmdBuffer);

	vk::VkImageMemoryBarrier postImageBarrier = vk::makeImageMemoryBarrier(vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, vk::VK_ACCESS_TRANSFER_READ_BIT, vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_IMAGE_LAYOUT_GENERAL, **image, subresourceRange);
	vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, (vk::VkDependencyFlags)0u, 0u, (const vk::VkMemoryBarrier*)DE_NULL, 0u, (const vk::VkBufferMemoryBarrier*)DE_NULL, 1u, &postImageBarrier);

	const vk::VkBufferImageCopy	copyRegion = vk::makeBufferImageCopy(extent, subresourceLayers);
	vk.cmdCopyImageToBuffer(*cmdBuffer, **image, vk::VK_IMAGE_LAYOUT_GENERAL, **colorOutputBuffer, 1u, &copyRegion);

	vk::endCommandBuffer(vk, *cmdBuffer);
	vk::submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	tcu::ConstPixelBufferAccess resultBuffer = tcu::ConstPixelBufferAccess(vk::mapVkFormat(colorAttachmentFormat), renderArea.extent.width, renderArea.extent.height, 1, (const void*)colorOutputBuffer->getAllocation().getHostPtr());

	const tcu::Vec4			black		= tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f);
	const tcu::Vec4			white		= tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f);
	const deUint32			width		= resultBuffer.getWidth();
	const deUint32			height		= resultBuffer.getHeight();

	const bool equal1[17][17] =
	{
		{ 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, },
		{ 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, },
		{ 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, },
		{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, },
		{ 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, },
		{ 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, },
		{ 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, },
		{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, },
	};

	const bool even1[17][17] =
	{
		{ 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, },
		{ 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, },
		{ 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 1, },
		{ 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, },
		{ 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, },
		{ 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, },
		{ 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, },
		{ 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 1, },
		{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, },
		{ 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, },
		{ 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 1, },
		{ 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, },
		{ 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, },
		{ 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, },
		{ 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, },
		{ 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 1, },
		{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, },
	};

	const bool odd2[17][17] =
	{
		{ 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, },
		{ 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 1, },
		{ 1, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, },
		{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, },
		{ 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, },
		{ 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, },
		{ 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 1, },
		{ 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, },
		{ 1, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1, },
		{ 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1, 1, },
		{ 1, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1, },
		{ 1, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, },
		{ 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, },
		{ 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, },
		{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, },
		{ 1, 0, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 1, },
		{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, },
	};

	for (deUint32 j = 0; j < height; ++j)
	{
		for (deUint32 i = 0; i < width; ++i)
		{
			const tcu::Vec4 color = resultBuffer.getPixel(i, j).asFloat();

			bool inside = false;
			if (i >= 7 && i < 24 && j >= 7 && j < 24)
			{
				if ((m_params.subdivision == 1 && m_params.spacing == EQUAL) || (m_params.subdivision == 1 && m_params.spacing == ODD))
					inside |= equal1[j - 7][i - 7];
				else if ((m_params.subdivision == 1 && m_params.spacing == EVEN) || (m_params.subdivision == 2 && m_params.spacing == EQUAL) || (m_params.subdivision == 2 && m_params.spacing == EVEN))
					inside |= even1[j - 7][i - 7];
				else if (m_params.subdivision == 2 && m_params.spacing == ODD)
					inside |= odd2[j - 7][i - 7];
			}

			if (inside)
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

class ShaderObjectTessellationModesCase : public vkt::TestCase
{
public:
							ShaderObjectTessellationModesCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TessellationModesParams& testParams)
																: vkt::TestCase		(testCtx, name, description)
																, m_params			(testParams)
																{}
	virtual					~ShaderObjectTessellationModesCase	(void) {}

	void					checkSupport						(vkt::Context& context) const override;
	virtual void			initPrograms						(vk::SourceCollections& programCollection) const override;
	TestInstance*			createInstance						(Context& context) const override { return new ShaderObjectTessellationModesInstance(context, m_params); }
private:
	const TessellationModesParams m_params;
};

void ShaderObjectTessellationModesCase::checkSupport (vkt::Context& context) const
{
	context.requireDeviceFunctionality("VK_EXT_shader_object");
	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_TESSELLATION_SHADER);
}

void ShaderObjectTessellationModesCase::initPrograms (vk::SourceCollections& programCollection) const
{
	std::stringstream vert;
	std::stringstream tesc;
	std::stringstream tese;
	std::stringstream frag;

	vert
		<< "#version 450\n"
		<< "void main() {\n"
		<< "    vec2 pos = vec2(float(gl_VertexIndex & 1), float((gl_VertexIndex >> 1) & 1));\n"
		<< "    gl_Position = vec4(pos - 0.5f, 0.0f, 1.0f);\n"
		<< "}\n";

	tesc
		<< "#version 450\n"
		<< "\n"
		<< "layout(vertices = 4) out;\n"
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "    if (gl_InvocationID == 0) {\n";
	if (m_params.subdivision == 1)
		tesc << "    float subdivision = 1.0f;\n";
	else
		tesc << "    float subdivision = 2.0f;\n";
	tesc
		<< "		gl_TessLevelInner[0] = subdivision;\n"
		<< "		gl_TessLevelInner[1] = subdivision;\n"
		<< "		gl_TessLevelOuter[0] = subdivision;\n"
		<< "		gl_TessLevelOuter[1] = subdivision;\n"
		<< "		gl_TessLevelOuter[2] = subdivision;\n"
		<< "		gl_TessLevelOuter[3] = subdivision;\n"
		<< "	}\n"
		<< "    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
		<< "}\n";

	tese
		<< "#version 450\n"
		<< "\n";
	if (m_params.spacing == EQUAL)
		tese << "layout(quads, equal_spacing) in;\n";
	else if (m_params.spacing == EVEN)
		tese << "layout(quads, fractional_even_spacing) in;\n";
	else
		tese << "layout(quads, fractional_odd_spacing) in;\n";
	tese
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	float u = gl_TessCoord.x;\n"
		<< "	float v = gl_TessCoord.y;\n"
		<< "	float omu = 1.0f - u;\n"
		<< "	float omv = 1.0f - v;\n"
		<< "	gl_Position = omu * omv * gl_in[0].gl_Position + u * omv * gl_in[2].gl_Position + u * v * gl_in[3].gl_Position + omu * v * gl_in[1].gl_Position;\n"
		<< "}\n";

	frag
		<< "#version 450\n"
		<< "layout (location=0) out vec4 outColor;\n"
		<< "void main() {\n"
		<< "    outColor = vec4(1.0f);\n"
		<< "}\n";
	programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());
	programCollection.glslSources.add("tesc") << glu::TessellationControlSource(tesc.str());
	programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(tese.str());
	programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

}

tcu::TestCaseGroup* createShaderObjectMiscTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> miscGroup(new tcu::TestCaseGroup(testCtx, "misc", ""));

	const struct
	{
		deUint32	stride;
		const char* name;
	} strideTests[] =
	{
		{ 16,	"16",	},
		{ 32,	"32",	},
		{ 48,	"48",	},
		{ 40,	"40",	},
	};

	for (deUint32 i = 0; i < 2; ++i)
	{
		bool blend1 = i == 0;
		de::MovePtr<tcu::TestCaseGroup> blend1Group(new tcu::TestCaseGroup(testCtx, blend1 ? "on" : "off", ""));
		for (deUint32 j = 0; j < 2; ++j)
		{
			bool blend2 = j == 0;
			de::MovePtr<tcu::TestCaseGroup> blend2Group(new tcu::TestCaseGroup(testCtx, blend2 ? "on" : "off", ""));
			for (deUint32 k = 0; k < 2; ++k)
			{
				bool vertexInputBefore = k == 0;
				de::MovePtr<tcu::TestCaseGroup> vertexInputBeforeGroup(new tcu::TestCaseGroup(testCtx, vertexInputBefore ? "before" : "after", ""));
				for (deUint32 l = 0; l < 2; ++l)
				{
					bool vertexBuffersNullStride = l == 0;
					de::MovePtr<tcu::TestCaseGroup> vertexBuffersNullStrideGroup(new tcu::TestCaseGroup(testCtx, vertexBuffersNullStride ? "null" : "non_null", ""));
					for (const auto& strideTest : strideTests)
					{
						de::MovePtr<tcu::TestCaseGroup> strideGroup(new tcu::TestCaseGroup(testCtx, strideTest.name, ""));
						for (deUint32 m = 0; m < 2; ++m)
						{
							bool destroyDescriptorSetLayout = m == 1;
							std::string destroyName = destroyDescriptorSetLayout ? "set" : "destroyed";

							TestParams params;
							params.blendEnabled[0] = blend1;
							params.blendEnabled[1] = blend2;
							params.vertexInputBefore = vertexInputBefore;
							params.vertexBuffersNullStride = vertexBuffersNullStride;
							params.stride = strideTest.stride;
							params.destroyDescriptorSetLayout = destroyDescriptorSetLayout;
							strideGroup->addChild(new ShaderObjectMiscCase(testCtx, destroyName, "", params));
						}
						vertexBuffersNullStrideGroup->addChild(strideGroup.release());
					}
					vertexInputBeforeGroup->addChild(vertexBuffersNullStrideGroup.release());
				}
				blend2Group->addChild(vertexInputBeforeGroup.release());
			}
			blend1Group->addChild(blend2Group.release());
		}
		miscGroup->addChild(blend1Group.release());
	}

	const struct
	{
		bool pipeline;
		const char* name;
	} pipelineTests[] =
	{
		{ false,	"shaders"	},
		{ true,		"pipeline"	},
	};

	const struct
	{
		bool meshShader;
		bool vertShader;
		bool tessShader;
		bool geomShader;
		bool fragShader;
		const char* name;
	} shadersTests[] =
	{
		{ false, true, false, false, false,		"vert",					},
		{ false, true, false, false, true,		"vert_frag",			},
		{ false, true, true, false, true,		"vert_tess_frag",		},
		{ false, true, false, true, true,		"vert_geom_frag",		},
		{ false, true, true, true, true,		"vert_tess_geom_frag",	},
		{ true, false, false, false, true,		"mesh_frag",			},
	};

	const struct
	{
		bool alphaToOne;
		const char* name;
	} alphaToOneTests[] =
	{
		{ false,	"disabled"	},
		{ true,		"enabled"	},
	};

	const struct
	{
		bool depthTestEnable;
		bool depthBounds;
		bool depthBoundsTestEnable;
		bool depthClamp;
		bool depthClip;
		bool depthClipControl;
		bool depthBiasEnable;
		const char* name;
	} depthTests[]
	{
		{ false, false, false, false, false, false, false,	"none"				},
		{ true, true, false, false, false, false, false,	"bounds_disabled"	},
		{ true, true, true, false, false, false, false,		"bounds_enabled"	},
		{ true, false, false, true, false, false, false,	"clamp"				},
		{ true, false, false, false, true, false, false,	"clip"				},
		{ true, false, false, false, false, true, false,	"clip_control"		},
		{ true, false, false, false, false, false, true,	"bias"				},
	};

	const struct
	{
		bool discardRectangles;
		bool discardRectanglesEnabled;
		const char* name;
	} discardRectanglesTests[] =
	{
		{ false, false, "disabled"	},
		{ true, false,	"enabled"	},
		{ true, true,	"discard"	},
	};

	const struct
	{
		bool rasterizationDiscardEnable;
		const char* name;
	} rasterizationDiscardEnableTests[] =
	{
		{ false,	"disabled"	},
		{ true,		"enabled"	},
	};

	const struct
	{
		bool colorBlendEnable;
		const char* name;
	} colorBlendTests[] =
	{
		{ false,	"disabled"	},
		{ true,		"enabled"	},
	};

	const struct
	{
		bool lines;
		const char* name;
	} primitiveTests[] =
	{
		{ false,	"triangles" },
		{ true,		"lines"		},
	};

	const struct
	{
		bool stencilEnable;
		const char* name;
	} stencilTests[] =
	{
		{ false,	"disabled"	},
		{ true,		"enabled"	},
	};

	const struct
	{
		bool logicOp;
		bool logicOpEnable;
		const char* name;
	} logicOpTests[] =
	{
		{ false, false, "disabled"	},
		{ true, false,	"enabled"	},
		{ true, true,	"copy"		},
	};

	const struct
	{
		bool geometryStreams;
		const char* name;
	} geometryStreamsTests[] =
	{
		{ false,	"disabled"	},
		{ true,		"enabled"	},
	};

	const struct
	{
		bool provokingVertex;
		const char* name;
	} provokingVertexTests[] =
	{
		{ false,	"disabled"	},
		{ true,		"enabled"	},
	};

	const struct
	{
		bool sampleLocations;
		bool sampleLocationsEnable;
		const char* name;
	} sampleLocationsTests[] =
	{
		{ false, false, "disabled"	},
		{ true, false,	"enabled"	},
		{ true, true,	"used"		},
	};

	const struct
	{
		bool lineRasterization;
		bool stippledLineEnable;
		const char* name;
	} linesTests[] =
	{
		{ false, false, "default"				},
		{ true, false,	"rectangular"			},
		{ true, true,	"rectangular_stippled"	},
	};

	const struct
	{
		bool cull;
		const char* name;
	} cullTests[] =
	{
		{ false,	"none"				},
		{ true,		"front_and_back"	},
	};

	const struct
	{
		bool conservativeRasterization;
		bool conservativeRasterizationOverestimate;
		const char* name;
	} conservativeRasterizationTests[] =
	{
		{ false, false, "disabled"		},
		{ true, false,	"enabled"		},
		{ true, true,	"overestimate"	},
	};

	const struct
	{
		bool colorWrite;
		bool colorWriteEnable;
		const char* name;
	} colorWriteEnableTests[] =
	{
		{ false, false, "disabled"	},
		{ true, false,	"false"		},
		{ true, true,	"true"		},
	};

	de::MovePtr<tcu::TestCaseGroup> stateGroup(new tcu::TestCaseGroup(testCtx, "state", ""));
	for (const auto& pipelineTest : pipelineTests)
	{
		de::MovePtr<tcu::TestCaseGroup> pipelineGroup(new tcu::TestCaseGroup(testCtx, pipelineTest.name, ""));
		for (const auto shadersTest : shadersTests)
		{
			de::MovePtr<tcu::TestCaseGroup> shadersGroup(new tcu::TestCaseGroup(testCtx, shadersTest.name, ""));

			StateTestParams params;
			params.pipeline = pipelineTest.pipeline;
			params.meshShader = shadersTest.meshShader;
			params.vertShader = shadersTest.vertShader;
			params.tessShader = shadersTest.tessShader;
			params.geomShader = shadersTest.geomShader;
			params.fragShader = shadersTest.fragShader;
			params.reset();

			de::MovePtr<tcu::TestCaseGroup> alphaToOneGroup(new tcu::TestCaseGroup(testCtx, "alphaToOne", ""));
			for (const auto& alphaToOneTest : alphaToOneTests)
			{
				params.alphaToOne = alphaToOneTest.alphaToOne;
				alphaToOneGroup->addChild(new ShaderObjectStateCase(testCtx, alphaToOneTest.name, "", params));
			}
			shadersGroup->addChild(alphaToOneGroup.release());
			params.reset();

			de::MovePtr<tcu::TestCaseGroup> depthGroup(new tcu::TestCaseGroup(testCtx, "depth", ""));
			for (const auto& depthTest : depthTests)
			{
				params.depthTestEnable = depthTest.depthTestEnable;
				params.depthBounds = depthTest.depthBounds;
				params.depthBoundsTestEnable = depthTest.depthBoundsTestEnable;
				params.depthClamp = depthTest.depthClamp;
				params.depthClip = depthTest.depthClip;
				params.depthClipControl = depthTest.depthClipControl;
				params.depthBiasEnable = depthTest.depthBiasEnable;
				depthGroup->addChild(new ShaderObjectStateCase(testCtx, depthTest.name, "", params));
			}
			shadersGroup->addChild(depthGroup.release());
			params.reset();

			de::MovePtr<tcu::TestCaseGroup> discardRectanglesGroup(new tcu::TestCaseGroup(testCtx, "discard_rectangles", ""));
			for (const auto& discardRectangles : discardRectanglesTests)
			{
				params.discardRectangles = discardRectangles.discardRectangles;
				params.discardRectanglesEnable = discardRectangles.discardRectanglesEnabled;
				discardRectanglesGroup->addChild(new ShaderObjectStateCase(testCtx, discardRectangles.name, "", params));
			}
			shadersGroup->addChild(discardRectanglesGroup.release());
			params.reset();

			de::MovePtr<tcu::TestCaseGroup> rasterizationDiscardEnableGroup(new tcu::TestCaseGroup(testCtx, "rasterization_discard", ""));
			for (const auto& rasterizationDiscardTest : rasterizationDiscardEnableTests)
			{
				params.rasterizerDiscardEnable = rasterizationDiscardTest.rasterizationDiscardEnable;
				rasterizationDiscardEnableGroup->addChild(new ShaderObjectStateCase(testCtx, rasterizationDiscardTest.name, "", params));
			}
			shadersGroup->addChild(rasterizationDiscardEnableGroup.release());
			params.reset();

			de::MovePtr<tcu::TestCaseGroup> colorBlendGroup(new tcu::TestCaseGroup(testCtx, "color_blend", ""));
			for (const auto& colorBlendTest : colorBlendTests)
			{
				params.colorBlendEnable = colorBlendTest.colorBlendEnable;
				colorBlendGroup->addChild(new ShaderObjectStateCase(testCtx, colorBlendTest.name, "", params));
			}
			shadersGroup->addChild(colorBlendGroup.release());
			params.reset();

			de::MovePtr<tcu::TestCaseGroup> primitivesGroup(new tcu::TestCaseGroup(testCtx, "primitives", ""));
			for (const auto& primitivesTest : primitiveTests)
			{
				params.lines = primitivesTest.lines;
				primitivesGroup->addChild(new ShaderObjectStateCase(testCtx, primitivesTest.name, "", params));
			}
			shadersGroup->addChild(primitivesGroup.release());
			params.reset();

			de::MovePtr<tcu::TestCaseGroup> stencilGroup(new tcu::TestCaseGroup(testCtx, "stencil", ""));
			for (const auto& stencilTest : stencilTests)
			{
				params.stencilTestEnable = stencilTest.stencilEnable;
				stencilGroup->addChild(new ShaderObjectStateCase(testCtx, stencilTest.name, "", params));
			}
			shadersGroup->addChild(stencilGroup.release());
			params.reset();

			de::MovePtr<tcu::TestCaseGroup> logicOpGroup(new tcu::TestCaseGroup(testCtx, "logic_op", ""));
			for (const auto& logicOpTest : logicOpTests)
			{
				params.logicOp = logicOpTest.logicOp;
				params.logicOpEnable = logicOpTest.logicOpEnable;
				logicOpGroup->addChild(new ShaderObjectStateCase(testCtx, logicOpTest.name, "", params));
			}
			shadersGroup->addChild(logicOpGroup.release());
			params.reset();

			if (shadersTest.geomShader)
			{
				de::MovePtr<tcu::TestCaseGroup> geometryStreamsGroup(new tcu::TestCaseGroup(testCtx, "geometry_streams", ""));
				for (const auto& geometryStreamsTest : geometryStreamsTests)
				{
					params.geometryStreams = geometryStreamsTest.geometryStreams;
					geometryStreamsGroup->addChild(new ShaderObjectStateCase(testCtx, geometryStreamsTest.name, "", params));
				}
				shadersGroup->addChild(geometryStreamsGroup.release());
				params.reset();
			}

			de::MovePtr<tcu::TestCaseGroup> provokingVertexGroup(new tcu::TestCaseGroup(testCtx, "provoking_vertex", ""));
			for (const auto& provokingVertexTest : provokingVertexTests)
			{
				params.provokingVertex = provokingVertexTest.provokingVertex;
				provokingVertexGroup->addChild(new ShaderObjectStateCase(testCtx, provokingVertexTest.name, "", params));
			}
			shadersGroup->addChild(provokingVertexGroup.release());
			params.reset();

			de::MovePtr<tcu::TestCaseGroup> sampleLocationsGroup(new tcu::TestCaseGroup(testCtx, "sample_locations", ""));
			for (const auto& sampleLocationsTest : sampleLocationsTests)
			{
				params.sampleLocations = sampleLocationsTest.sampleLocations;
				params.sampleLocationsEnable = sampleLocationsTest.sampleLocationsEnable;
				sampleLocationsGroup->addChild(new ShaderObjectStateCase(testCtx, sampleLocationsTest.name, "", params));
			}
			shadersGroup->addChild(sampleLocationsGroup.release());
			params.reset();

			de::MovePtr<tcu::TestCaseGroup> linesGroup(new tcu::TestCaseGroup(testCtx, "lines", ""));
			for (const auto& linesTest : linesTests)
			{
				params.lines = true;
				params.stippledLineEnable = linesTest.stippledLineEnable;
				params.lineRasterization = linesTest.lineRasterization;
				linesGroup->addChild(new ShaderObjectStateCase(testCtx, linesTest.name, "", params));
			}
			shadersGroup->addChild(linesGroup.release());
			params.reset();

			de::MovePtr<tcu::TestCaseGroup> cullGroup(new tcu::TestCaseGroup(testCtx, "cull", ""));
			for (const auto& cullTest : cullTests)
			{
				params.cull = cullTest.cull;
				cullGroup->addChild(new ShaderObjectStateCase(testCtx, cullTest.name, "", params));
			}
			shadersGroup->addChild(cullGroup.release());
			params.reset();

			de::MovePtr<tcu::TestCaseGroup> conservativeRasterizationGroup(new tcu::TestCaseGroup(testCtx, "conservative_rasterization", ""));
			for (const auto& conservativeRasterizationTest : conservativeRasterizationTests)
			{
				params.conservativeRasterization = conservativeRasterizationTest.conservativeRasterization;
				params.conservativeRasterizationOverestimate = conservativeRasterizationTest.conservativeRasterizationOverestimate;
				conservativeRasterizationGroup->addChild(new ShaderObjectStateCase(testCtx, conservativeRasterizationTest.name, "", params));
			}
			shadersGroup->addChild(conservativeRasterizationGroup.release());
			params.reset();

			de::MovePtr<tcu::TestCaseGroup> colorWriteGroup(new tcu::TestCaseGroup(testCtx, "color_write", ""));
			for (const auto& colorWriteEnableTest : colorWriteEnableTests)
			{
				params.colorWrite = colorWriteEnableTest.colorWrite;
				params.colorWriteEnable = colorWriteEnableTest.colorWriteEnable;
				colorWriteGroup->addChild(new ShaderObjectStateCase(testCtx, colorWriteEnableTest.name, "", params));
			}
			shadersGroup->addChild(colorWriteGroup.release());
			params.reset();

			pipelineGroup->addChild(shadersGroup.release());
		}
		stateGroup->addChild(pipelineGroup.release());
	}
	miscGroup->addChild(stateGroup.release());

	const struct
	{
		bool linked;
		const char* name;
	} linkedTests[] =
	{
		{ false,	"unlinked"	},
		{ true,		"linked"	},
	};

	const struct
	{
		vk::VkShaderStageFlagBits stage;
		const char* name;
	} shaderStageTests[] =
	{
		{ vk::VK_SHADER_STAGE_VERTEX_BIT,					"vert"	},
		{ vk::VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,		"tesc"	},
		{ vk::VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,	"tese"	},
		{ vk::VK_SHADER_STAGE_GEOMETRY_BIT,					"geom"	},
	};

	const struct
	{
		bool builtin;
		const char* name;
	} typeTests[] =
	{
		{ false,	"output"	},
		{ true,		"builtin"	},
	};

	de::MovePtr<tcu::TestCaseGroup> unusedVariableGroup(new tcu::TestCaseGroup(testCtx, "unused_variable", ""));
	for (const auto& linkedTest : linkedTests)
	{
		de::MovePtr<tcu::TestCaseGroup> linkedGroup(new tcu::TestCaseGroup(testCtx, linkedTest.name, ""));
		for (const auto& typeTest : typeTests)
		{
			de::MovePtr<tcu::TestCaseGroup> typeGroup(new tcu::TestCaseGroup(testCtx, typeTest.name, ""));
			for (const auto& shaderStageTest : shaderStageTests)
			{
				UnusedBuiltinParams params;
				params.linked = linkedTest.linked;
				params.stage = shaderStageTest.stage;
				params.builtin = typeTest.builtin;
				typeGroup->addChild(new ShaderObjectUnusedBuiltinCase(testCtx, shaderStageTest.name, "", params));
			}
			linkedGroup->addChild(typeGroup.release());
		}
		unusedVariableGroup->addChild(linkedGroup.release());
	}
	miscGroup->addChild(unusedVariableGroup.release());

	const struct
	{
		deUint32 subdivision;
		const char* name;
	} subdivisionTests[] =
	{
		{ 1,	"one"	},
		{ 2,	"two"	},
	};

	const struct
	{
		TessellationSpacing spacing;
		const char* name;
	} spacingTests[] =
	{
		{ EQUAL,	"equal"	},
		{ EVEN,		"even"	},
		{ ODD,		"odd"	},
	};

	de::MovePtr<tcu::TestCaseGroup> tessellationModesGroup(new tcu::TestCaseGroup(testCtx, "tessellation_modes", ""));
	for (const auto& subdivisionTest : subdivisionTests)
	{
		de::MovePtr<tcu::TestCaseGroup> subdivisionGroup(new tcu::TestCaseGroup(testCtx, subdivisionTest.name, ""));

		for (const auto& spacingTest : spacingTests)
		{
			TessellationModesParams params;
			params.subdivision = subdivisionTest.subdivision;
			params.spacing = spacingTest.spacing;
			subdivisionGroup->addChild(new ShaderObjectTessellationModesCase(testCtx, spacingTest.name, "", params));
		}
		tessellationModesGroup->addChild(subdivisionGroup.release());
	}
	miscGroup->addChild(tessellationModesGroup.release());

	return miscGroup.release();
}

} // ShaderObject
} // vkt
