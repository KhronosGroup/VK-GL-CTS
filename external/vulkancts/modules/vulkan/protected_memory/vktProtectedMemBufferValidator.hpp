#ifndef _VKTPROTECTEDMEMBUFFERVALIDATOR_HPP
#define _VKTPROTECTEDMEMBUFFERVALIDATOR_HPP
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
 * \brief Protected content buffer validator helper
 *//*--------------------------------------------------------------------*/

#include "tcuVector.hpp"
#include "vkDefs.hpp"
#include "vktTestCase.hpp"
#include "tcuVector.hpp"
#include "tcuTestLog.hpp"

#include "vkBuilderUtil.hpp"
#include "vkPrograms.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestGroupUtil.hpp"
#include "tcuStringTemplate.hpp"

#include "vktProtectedMemUtils.hpp"
#include "vktProtectedMemContext.hpp"

namespace vkt
{
namespace ProtectedMem
{

class ProtectedContext;

template<typename T>
struct ValidationData {
	const tcu::IVec4	positions[4];
	const T				values[4];
};

template<typename T>
struct ValidationDataStorage {
	T					values;
};

typedef ValidationData<tcu::UVec4>	ValidationDataUVec4;
typedef ValidationData<tcu::IVec4>	ValidationDataIVec4;
typedef ValidationData<tcu::Vec4>	ValidationDataVec4;

enum TestType {
	TYPE_UINT,
	TYPE_INT,
	TYPE_FLOAT,
};

enum BufferType {
	SAMPLER_BUFFER,
	STORAGE_BUFFER,
};

void					initBufferValidatorPrograms		(vk::SourceCollections&	programCollection, TestType testType, BufferType bufferType);
vk::VkDescriptorType	getDescriptorType				(BufferType bufferType);

template<typename T>
class BufferValidator
{
public:
									BufferValidator			(const ValidationData<T> data)
										: m_refData			(data)
										, m_refDataStorage	(*reinterpret_cast<ValidationDataStorage<T>*>( &std::vector<char>(sizeof(ValidationDataStorage<T>), '\0').front()))
										, m_bufferType		(SAMPLER_BUFFER)
									{
									}

									BufferValidator			(const ValidationDataStorage<T> data)
										: m_refData			(*reinterpret_cast<ValidationData<T>*>( &std::vector<char>(sizeof(ValidationData<T>), '\0').front()))
										, m_refDataStorage	(data)
										, m_bufferType		(STORAGE_BUFFER)
									{
									}

									~BufferValidator		() {}
	void							initPrograms			(vk::SourceCollections&	programCollection) const;

	bool							validateBuffer			(ProtectedContext&	ctx,
																 const vk::VkBuffer	buffer) const;
private:
	deUint32						getReferenceDataSize	() const;
	const void *					getReferenceDataSrc		() const;
	void							printReferenceInfo		(ProtectedContext&		ctx) const;

	const ValidationData<T>			m_refData;
	const ValidationDataStorage<T>	m_refDataStorage;

	BufferType						m_bufferType;
};

template<>
inline void BufferValidator<tcu::UVec4>::initPrograms (vk::SourceCollections& programCollection) const
{
	initBufferValidatorPrograms(programCollection, TYPE_UINT, m_bufferType);
}

template<>
inline void BufferValidator<tcu::IVec4>::initPrograms (vk::SourceCollections& programCollection) const
{
	initBufferValidatorPrograms(programCollection, TYPE_INT, m_bufferType);
}

template<>
inline void BufferValidator<tcu::Vec4>::initPrograms (vk::SourceCollections& programCollection) const
{
	initBufferValidatorPrograms(programCollection, TYPE_FLOAT, m_bufferType);
}

template<typename T>
deUint32 BufferValidator<T>::getReferenceDataSize () const
{
	return m_bufferType == SAMPLER_BUFFER ? (deUint32)sizeof(m_refData) : (deUint32)sizeof(m_refDataStorage);
}

template<typename T>
const void * BufferValidator<T>::getReferenceDataSrc () const
{
	return m_bufferType == SAMPLER_BUFFER ? (void*)&m_refData : (void*)&m_refDataStorage;
}

template<typename T>
void BufferValidator<T>::printReferenceInfo (ProtectedContext& ctx) const
{
	if (m_bufferType == SAMPLER_BUFFER)
	{
		ctx.getTestContext().getLog()
				<< tcu::TestLog::Message << "Reference positions: \n"
				<< "1: " << m_refData.positions[0] << "\n"
				<< "2: " << m_refData.positions[1] << "\n"
				<< "3: " << m_refData.positions[2] << "\n"
				<< "4: " << m_refData.positions[3] << "\n"
				<< tcu::TestLog::EndMessage
				<< tcu::TestLog::Message << "Reference fill values: \n"
				<< "1: " << m_refData.values[0] << "\n"
				<< "2: " << m_refData.values[1] << "\n"
				<< "3: " << m_refData.values[2] << "\n"
				<< "4: " << m_refData.values[3] << "\n"
				<< tcu::TestLog::EndMessage;
	} else if (m_bufferType == STORAGE_BUFFER)
	{
		ctx.getTestContext().getLog()
				<< tcu::TestLog::Message << "Reference values: \n"
				<< "1: " << m_refDataStorage.values << "\n"
				<< tcu::TestLog::EndMessage;
	}
}

template<typename T>
bool BufferValidator<T>::validateBuffer (ProtectedContext&		ctx,
										 const vk::VkBuffer		buffer) const
{
	// Log out a few reference info
	printReferenceInfo(ctx);

	const deUint64							oneSec				= 1000 * 1000 * 1000;

	const vk::DeviceInterface&				vk					= ctx.getDeviceInterface();
	const vk::VkDevice						device				= ctx.getDevice();
	const vk::VkQueue						queue				= ctx.getQueue();
	const deUint32							queueFamilyIndex	= ctx.getQueueFamilyIndex();

	vk::Move<vk::VkBufferView>				bufferView;

	const deUint32							refDataSize			= getReferenceDataSize();
	de::UniquePtr<vk::BufferWithMemory>		refUniform			(makeBuffer(ctx,
																 PROTECTION_DISABLED,
																 queueFamilyIndex,
																 refDataSize,
																 vk::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
																 vk::MemoryRequirement::HostVisible));

	// Set the reference uniform data
	{
		deMemcpy(refUniform->getAllocation().getHostPtr(), getReferenceDataSrc(), refDataSize);
		flushAlloc(vk, device, refUniform->getAllocation());
	}

	const deUint32							helperBufferSize	= (deUint32)(2 * sizeof(deUint32));
	de::MovePtr<vk::BufferWithMemory>		helperBuffer		(makeBuffer(ctx,
																 PROTECTION_ENABLED,
																 queueFamilyIndex,
																 helperBufferSize,
																 vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
																 vk::MemoryRequirement::Protected));
	vk::Unique<vk::VkShaderModule>			resetSSBOShader		(vk::createShaderModule(vk, device, ctx.getBinaryCollection().get("ResetSSBO"), 0));
	vk::Unique<vk::VkShaderModule>			validatorShader		(vk::createShaderModule(vk, device, ctx.getBinaryCollection().get("BufferValidator"), 0));

	// Create descriptors
	vk::Unique<vk::VkDescriptorSetLayout>	descriptorSetLayout	(vk::DescriptorSetLayoutBuilder()
																	.addSingleBinding(getDescriptorType(m_bufferType), vk::VK_SHADER_STAGE_COMPUTE_BIT)
																	.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT)
																	.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT)
																	.build(vk, device));
	vk::Unique<vk::VkDescriptorPool>		descriptorPool		(vk::DescriptorPoolBuilder()
																	.addType(getDescriptorType(m_bufferType), 1u)
																	.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u)
																	.addType(vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u)
																	.build(vk, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));
	vk::Unique<vk::VkDescriptorSet>			descriptorSet		(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));


	// Update descriptor set information
	{
		vk::VkDescriptorBufferInfo	descRefUniform	= makeDescriptorBufferInfo(**refUniform, 0, refDataSize);
		vk::VkDescriptorBufferInfo	descBuffer		= makeDescriptorBufferInfo(**helperBuffer, 0, helperBufferSize);

		vk::DescriptorSetUpdateBuilder descriptorSetUpdateBuilder;
		switch (m_bufferType)
		{
			case SAMPLER_BUFFER:
			{
				const vk::VkBufferViewCreateInfo		viewParams			=
					{
						vk::VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,	// VkStructureType			sType
						DE_NULL,										// const void*				pNext
						0u,												// VkBufferViewCreateFlags	flags
						buffer,											// VkBuffer					buffer
						vk::VK_FORMAT_R32G32B32A32_UINT,				// VkFormat					format
						0u,												// VkDeviceSize				offset
						VK_WHOLE_SIZE									// VkDeviceSize				range
					};
				bufferView = vk::Move<vk::VkBufferView> (vk::createBufferView(vk, device, &viewParams));
				descriptorSetUpdateBuilder
					.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, &bufferView.get());
				break;
			}
			case STORAGE_BUFFER:
			{
				const deUint32					testBufferSize	= (deUint32)(sizeof(ValidationDataStorage<T>));
				vk::VkDescriptorBufferInfo		descTestBuffer	= makeDescriptorBufferInfo(buffer, 0, testBufferSize);
				descriptorSetUpdateBuilder
					.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descTestBuffer);
				break;
			}
		}
		descriptorSetUpdateBuilder
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

		queueSubmitResult = queueSubmit(ctx, PROTECTION_ENABLED, queue, *cmdBuffer, *fence, oneSec);
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

#endif // _VKTPROTECTEDMEMBUFFERVALIDATOR_HPP
