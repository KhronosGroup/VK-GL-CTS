/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 The Khronos Group Inc.
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
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
 * \brief Protected Memory image validator helper
 *//*--------------------------------------------------------------------*/

#include "vktProtectedMemImageValidator.hpp"

#include "tcuTestLog.hpp"
#include "tcuStringTemplate.hpp"

#include "vkBuilderUtil.hpp"
#include "vkPrograms.hpp"
#include "vkTypeUtil.hpp"
#include "vkImageUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestGroupUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include "vktProtectedMemUtils.hpp"
#include "vktProtectedMemContext.hpp"

namespace vkt
{
namespace ProtectedMem
{

void ImageValidator::initPrograms (vk::SourceCollections& programCollection) const
{
	// Layout:
	//  set = 0, location = 0 -> uniform *sampler2D u_protectedImage
	//  set = 0, location = 1 -> buffer ProtectedHelper (2 * uint)
	//  set = 0, location = 2 -> uniform Data (2 * vec2 + 4 * vec4)
	const char* validatorShader = "#version 450\n"
					  "layout(local_size_x = 1) in;\n"
					  "\n"
					  "layout(set=0, binding=0) uniform ${SAMPLER_TYPE} u_protectedImage;\n"
					  "\n"
					  "layout(set=0, binding=1) buffer ProtectedHelper\n"
					  "{\n"
					  "    highp uint zero; // set to 0\n"
					  "    highp uint dummyOut;\n"
					  "} helper;\n"
					  "\n"
					  "layout(set=0, binding=2) uniform Data\n"
					  "{\n"
					  "    highp vec2 protectedImageCoord[4];\n"
					  "    highp vec4 protectedImageRef[4];\n"
					  "};\n"
					  "\n"
					  "void error ()\n"
					  "{\n"
					  "    for (uint x = 0; x < 10; x += helper.zero)\n"
					  "        atomicAdd(helper.dummyOut, 1u);\n"
					  "}\n"
					  "\n"
					  "bool compare (vec4 a, vec4 b, float threshold)\n"
					  "{\n"
					  "    return all(lessThanEqual(abs(a - b), vec4(threshold)));\n"
					  "}\n"
					  "\n"
					  "void main (void)\n"
					  "{\n"
					  "    float threshold = 0.1;\n"
					  "    for (uint i = 0; i < 4; i++)\n"
					  "    {\n"
					  "        if (!compare(texture(u_protectedImage, protectedImageCoord[i]), protectedImageRef[i], threshold))\n"
					  "            error();\n"
					  "    }\n"
					  "}\n";

	const char* resetSSBOShader = "#version 450\n"
					  "layout(local_size_x = 1) in;\n"
					  "\n"
					  "layout(set=0, binding=1) buffer ProtectedHelper\n"
					  "{\n"
					  "    highp uint zero; // set to 0\n"
					  "    highp uint dummyOut;\n"
					  "} helper;\n"
					  "\n"
					  "void main (void)\n"
					  "{\n"
					  "    helper.zero = 0;\n"
					  "}\n";

	programCollection.glslSources.add("ResetSSBO") << glu::ComputeSource(resetSSBOShader);

	std::map<std::string, std::string> validationParams;
	validationParams["SAMPLER_TYPE"] = isIntFormat(m_imageFormat)	? "isampler2D" :
									   isUintFormat(m_imageFormat)	? "usampler2D" : "sampler2D";

	programCollection.glslSources.add("ImageValidator") << glu::ComputeSource(tcu::StringTemplate(validatorShader).specialize(validationParams));
}

bool ImageValidator::validateImage (ProtectedContext& ctx, const ValidationData& refData,
									const vk::VkImage image, const vk::VkFormat imageFormat, const vk::VkImageLayout imageLayout) const
{
	// Log out a few reference info
	{
		ctx.getTestContext().getLog()
			<< tcu::TestLog::Message << "Reference coordinates: \n"
				<< "1: " << refData.coords[0] << "\n"
				<< "2: " << refData.coords[1] << "\n"
				<< "3: " << refData.coords[2] << "\n"
				<< "4: " << refData.coords[3] << "\n"
				<< tcu::TestLog::EndMessage
			<< tcu::TestLog::Message << "Reference color values: \n"
				<< "1: " << refData.values[0] << "\n"
				<< "2: " << refData.values[1] << "\n"
				<< "3: " << refData.values[2] << "\n"
				<< "4: " << refData.values[3] << "\n"
			<< tcu::TestLog::EndMessage;
	}

	const deUint64							oneSec				= 1000 * 1000 * 1000;

	const vk::DeviceInterface&				vk					= ctx.getDeviceInterface();
	const vk::VkDevice						device				= ctx.getDevice();
	const vk::VkQueue						queue				= ctx.getQueue();
	const deUint32							queueFamilyIndex	= ctx.getQueueFamilyIndex();

	const deUint32							refUniformSize		= sizeof(refData);
	de::UniquePtr<vk::BufferWithMemory>		refUniform			(makeBuffer(ctx,
																		PROTECTION_DISABLED,
																		queueFamilyIndex,
																		refUniformSize,
																		vk::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
																		vk::MemoryRequirement::HostVisible));
	// Set the reference uniform data
	{
		deMemcpy(refUniform->getAllocation().getHostPtr(), &refData, refUniformSize);
		vk::flushMappedMemoryRange(vk, device, refUniform->getAllocation().getMemory(), refUniform->getAllocation().getOffset(), refUniformSize);
	}

	const deUint32							helperBufferSize	= (deUint32)(2 * sizeof(deUint32));
	de::MovePtr<vk::BufferWithMemory>		helperBuffer		(makeBuffer(ctx,
																			PROTECTION_ENABLED,
																			queueFamilyIndex,
																			helperBufferSize,
																			vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
																			vk::MemoryRequirement::Protected));
	vk::Unique<vk::VkShaderModule>			resetSSBOShader		(vk::createShaderModule(vk, device, ctx.getBinaryCollection().get("ResetSSBO"), 0));
	vk::Unique<vk::VkShaderModule>			validatorShader		(vk::createShaderModule(vk, device, ctx.getBinaryCollection().get("ImageValidator"), 0));

	vk::Unique<vk::VkSampler>				sampler				(makeSampler(vk, device));
	const vk::VkImageViewCreateInfo			viewParams			=
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// sType
		DE_NULL,										// pNext
		0u,												// flags
		image,											// image
		vk::VK_IMAGE_VIEW_TYPE_2D,						// viewType
		imageFormat,									// format
		vk::makeComponentMappingRGBA(),					// components
		{
			vk::VK_IMAGE_ASPECT_COLOR_BIT,	// aspectMask
			0u,								// baseMipLevel
			1u,								// mipLeves
			0u,								// baseArraySlice
			1u,								// arraySize
		}												// subresourceRange
	};
	vk::Unique<vk::VkImageView>				imageView			(vk::createImageView(vk, device, &viewParams));

	// Create descriptors
	vk::Unique<vk::VkDescriptorSetLayout>	descriptorSetLayout(vk::DescriptorSetLayoutBuilder()
		.addSingleSamplerBinding(vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, vk::VK_SHADER_STAGE_COMPUTE_BIT, DE_NULL)
		.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT)
		.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT)
		.build(vk, device));
	vk::Unique<vk::VkDescriptorPool>		descriptorPool(vk::DescriptorPoolBuilder()
		.addType(vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1u)
		.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u)
		.addType(vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u)
		.build(vk, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));
	vk::Unique<vk::VkDescriptorSet>			descriptorSet		(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

	// Update descriptor set infirmation
	{
		vk::VkDescriptorBufferInfo	descRefUniform	= makeDescriptorBufferInfo(**refUniform, 0, refUniformSize);
		vk::VkDescriptorBufferInfo	descBuffer		= makeDescriptorBufferInfo(**helperBuffer, 0, helperBufferSize);
		vk::VkDescriptorImageInfo	descSampledImg	= makeDescriptorImageInfo(*sampler, *imageView, imageLayout);

		vk::DescriptorSetUpdateBuilder()
			.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &descSampledImg)
			.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(1u), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descBuffer)
			.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(2u), vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &descRefUniform)
			.update(vk, device);
	}

	// Build pipeline
	vk::Unique<vk::VkPipelineLayout>		pipelineLayout		(makePipelineLayout(vk, device, *descriptorSetLayout));

	vk::Unique<vk::VkCommandPool>			cmdPool				(makeCommandPool(vk, device, PROTECTION_ENABLED, queueFamilyIndex));

	// Reset helper SSBO
	{
		const vk::Unique<vk::VkFence>		fence				(vk::createFence(vk, device));
		vk::Unique<vk::VkPipeline>			resetSSBOPipeline	(makeComputePipeline(vk, device, *pipelineLayout, *resetSSBOShader, DE_NULL));
		vk::Unique<vk::VkCommandBuffer>		resetCmdBuffer		(vk::allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));
		beginCommandBuffer(vk, *resetCmdBuffer);

		vk.cmdBindPipeline(*resetCmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *resetSSBOPipeline);
		vk.cmdBindDescriptorSets(*resetCmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &*descriptorSet, 0u, DE_NULL);
		vk.cmdDispatch(*resetCmdBuffer, 1u, 1u, 1u);

		endCommandBuffer(vk, *resetCmdBuffer);
		VK_CHECK(queueSubmit(ctx, PROTECTION_ENABLED, queue, *resetCmdBuffer, *fence, ~0ull));
	}

	// Create validation compute commands & submit
	vk::VkResult							queueSubmitResult;
	{
		const vk::Unique<vk::VkFence>		fence				(vk::createFence(vk, device));
		vk::Unique<vk::VkPipeline>			validationPipeline	(makeComputePipeline(vk, device, *pipelineLayout, *validatorShader, DE_NULL));
		vk::Unique<vk::VkCommandBuffer>		cmdBuffer			(vk::allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

		beginCommandBuffer(vk, *cmdBuffer);

		vk.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *validationPipeline);
		vk.cmdBindDescriptorSets(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &*descriptorSet, 0u, DE_NULL);
		vk.cmdDispatch(*cmdBuffer, 1u, 1u, 1u);

		endCommandBuffer(vk, *cmdBuffer);

		queueSubmitResult										= queueSubmit(ctx, PROTECTION_ENABLED, queue, *cmdBuffer, *fence, oneSec);
	}

	// \todo do we need to check the fence status?
	if (queueSubmitResult == vk::VK_TIMEOUT)
		return false;

	// at this point the submit result should be VK_TRUE
	VK_CHECK(queueSubmitResult);
	return true;
}

} // ProtectedMem
} // vkt
