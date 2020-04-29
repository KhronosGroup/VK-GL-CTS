/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 Google Inc.
 * Copyright (c) 2019 The Khronos Group Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Tests for descriptor copying
 *//*--------------------------------------------------------------------*/

#include "vktBindingDescriptorCopyTests.hpp"

#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkQueryUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkObjUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktTestCase.hpp"

#include "deDefs.h"
#include "deMath.h"
#include "deRandom.h"
#include "deSharedPtr.hpp"
#include "deString.h"

#include "tcuTestCase.hpp"
#include "tcuTestLog.hpp"

#include <string>
#include <sstream>

namespace vkt
{
namespace BindingModel
{
namespace
{
using namespace vk;
using namespace std;
using tcu::Vec4;
using tcu::Vec2;

enum PipelineType
{
	PIPELINE_TYPE_COMPUTE = 0,
	PIPELINE_TYPE_GRAPHICS = 1
};

struct DescriptorCopy
{
	deUint32	srcSet;
	deUint32	srcBinding;
	deUint32	srcArrayElement;
	deUint32	dstSet;
	deUint32	dstBinding;
	deUint32	dstArrayElement;
	deUint32	descriptorCount;
};

struct DescriptorData
{
	vector<deUint32>	data;		// The actual data. One element per dynamic offset.
	bool				written;	// Is the data written in descriptor update
	bool				copiedInto;	// Is the data being overwritten by a copy operation
};

typedef de::SharedPtr<ImageWithMemory>					ImageWithMemorySp;
typedef de::SharedPtr<Unique<VkImageView> >				VkImageViewSp;
typedef de::SharedPtr<Unique<VkBufferView> >			VkBufferViewSp;
typedef de::SharedPtr<Unique<VkSampler> >				VkSamplerSp;
typedef de::SharedPtr<Unique<VkDescriptorSetLayout> >	VkDescriptorSetLayoutSp;

const tcu::IVec2 renderSize(64, 64);

// Base class for descriptors
class Descriptor
{
public:
											Descriptor				(VkDescriptorType descriptorType, deUint32 arraySize = 1u, deUint32 writeStart = 0u, deUint32 elementsToWrite = 1u, deUint32 numDynamicAreas = 1u);
	virtual									~Descriptor				(void);
	VkDescriptorType						getType					(void) const { return m_descriptorType; }
	deUint32								getArraySize			(void) const { return m_arraySize; }
	virtual VkWriteDescriptorSet			getDescriptorWrite		(void) = 0;
	virtual string							getShaderDeclaration	(void) const = 0;
	virtual void							init					(Context& context, PipelineType pipelineType) = 0;
	virtual void							copyValue				(const Descriptor& src, deUint32 srcElement, deUint32 dstElement, deUint32 numElements);
	virtual void							invalidate				(Context& context) { DE_UNREF(context); }
	virtual vector<deUint32>				getData					(void) { DE_FATAL("Unexpected"); return vector<deUint32>(); }
	deUint32								getId					(void) const { return m_id; }
	virtual string							getShaderVerifyCode		(void) const = 0;
	string									getArrayString			(deUint32 index) const;
	deUint32								getFirstWrittenElement	(void) const;
	deUint32								getNumWrittenElements	(void) const;
	deUint32								getReferenceData		(deUint32 arrayIdx, deUint32 dynamicAreaIdx = 0) const { return m_data[arrayIdx].data[dynamicAreaIdx]; }
	virtual bool							isDynamic				(void) const { return false; }
	virtual void							setDynamicAreas			(vector<deUint32> dynamicAreas) { DE_UNREF(dynamicAreas); }
	virtual vector<VkImageViewSp>			getImageViews			(void) const { return vector<VkImageViewSp>(); }
	virtual vector<VkAttachmentReference>	getAttachmentReferences	(void) const { return vector<VkAttachmentReference>(); }

	static deUint32							s_nextId;
protected:
	VkDescriptorType						m_descriptorType;
	deUint32								m_arraySize;
	deUint32								m_id;
	vector<DescriptorData>					m_data;
	deUint32								m_numDynamicAreas;
};

typedef de::SharedPtr<Descriptor>	DescriptorSp;

// Base class for all buffer based descriptors
class BufferDescriptor : public Descriptor
{
public:
									BufferDescriptor		(VkDescriptorType type, deUint32 arraySize, deUint32 writeStart, deUint32 elementsToWrite, deUint32 numDynamicAreas = 1u);
	virtual							~BufferDescriptor		(void);
	void							init					(Context& context, PipelineType pipelineType);

	VkWriteDescriptorSet			getDescriptorWrite		(void);
	virtual string					getShaderDeclaration	(void) const = 0;
	void							invalidate				(Context& context);
	vector<deUint32>				getData					(void);
	virtual string					getShaderVerifyCode		(void) const = 0;
	virtual VkBufferUsageFlags		getBufferUsageFlags		(void) const = 0;
	virtual bool					usesBufferView			(void) { return false; }
private:
	vector<VkDescriptorBufferInfo>	m_descriptorBufferInfos;
	de::MovePtr<BufferWithMemory>	m_buffer;
	deUint32						m_bufferSize;
	vector<VkBufferViewSp>			m_bufferViews;
	vector<VkBufferView>			m_bufferViewHandles;
};

// Inline uniform block descriptor.
class InlineUniformBlockDescriptor : public Descriptor
{
public:
									InlineUniformBlockDescriptor		(deUint32 arraySize, deUint32 writeStart, deUint32 elementsToWrite, deUint32 numDynamicAreas = 1u);
	virtual							~InlineUniformBlockDescriptor		(void);
	void							init								(Context& context, PipelineType pipelineType);

	VkWriteDescriptorSet			getDescriptorWrite					(void);
	virtual string					getShaderDeclaration				(void) const;
	virtual string					getShaderVerifyCode					(void) const;
	virtual bool					usesBufferView						(void) { return false; }
	deUint32						getElementSizeInBytes				(void) const { return static_cast<deUint32>(sizeof(decltype(m_blockData)::value_type)); }
	deUint32						getSizeInBytes						(void) const { return m_blockElements * getElementSizeInBytes(); }

private:
	// Inline uniform blocks cannot form arrays, so we will reuse the array size to create a data array inside the uniform block as
	// an array of integers. However, with std140, each of those ints will be padded to 16 bytes in the shader. The struct below
	// allows memory to match between the host and the shader.
	struct PaddedUint
	{
					PaddedUint	() : value(0) { deMemset(padding, 0, sizeof(padding)); }
					PaddedUint	(deUint32 value_) : value(value_) { deMemset(padding, 0, sizeof(padding)); }
		PaddedUint&	operator=	(deUint32 value_) { value = value_; return *this; }

		deUint32	value;
		deUint32	padding[3];
	};

	vector<PaddedUint>							m_blockData;
	VkWriteDescriptorSetInlineUniformBlockEXT	m_inlineWrite;
	deUint32									m_blockElements;
	deUint32									m_writeStart;
	deUint32									m_elementsToWrite;
	deUint32									m_writeStartByteOffset;
	deUint32									m_bytesToWrite;
};

class UniformBufferDescriptor : public BufferDescriptor
{
public:
						UniformBufferDescriptor		(deUint32 arraySize = 1u, deUint32 writeStart = 0u, deUint32 elementsToWrite = 1u, deUint32 numDynamicAreas = 1u);
	virtual				~UniformBufferDescriptor	(void);

	string				getShaderDeclaration		(void) const;
	string				getShaderVerifyCode			(void) const;
	VkBufferUsageFlags	getBufferUsageFlags			(void) const { return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT; }
private:
};

class DynamicUniformBufferDescriptor : public BufferDescriptor
{
public:
						DynamicUniformBufferDescriptor	(deUint32 arraySize, deUint32 writeStart, deUint32 elementsToWrite, deUint32 numDynamicAreas);
	virtual				~DynamicUniformBufferDescriptor	(void);

	string				getShaderDeclaration			(void) const;
	string				getShaderVerifyCode				(void) const;
	VkBufferUsageFlags	getBufferUsageFlags				(void) const { return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT; }
	virtual void		setDynamicAreas					(vector<deUint32> dynamicAreas) { m_dynamicAreas = dynamicAreas; }
	virtual bool		isDynamic						(void) const { return true; }

private:
	vector<deUint32>	m_dynamicAreas;
};

class StorageBufferDescriptor : public BufferDescriptor
{
public:
						StorageBufferDescriptor		(deUint32 arraySize = 1u, deUint32 writeStart = 0u, deUint32 elementsToWrite = 1u, deUint32 numDynamicAreas = 1u);
	virtual				~StorageBufferDescriptor	(void);

	string				getShaderDeclaration		(void) const;
	string				getShaderVerifyCode			(void) const;
	VkBufferUsageFlags	getBufferUsageFlags			(void) const { return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT; }
private:
};

class DynamicStorageBufferDescriptor : public BufferDescriptor
{
public:
						DynamicStorageBufferDescriptor	(deUint32 arraySize, deUint32 writeStart, deUint32 elementsToWrite, deUint32 numDynamicAreas);
	virtual				~DynamicStorageBufferDescriptor	(void);

	string				getShaderDeclaration			(void) const;
	string				getShaderVerifyCode				(void) const;
	VkBufferUsageFlags	getBufferUsageFlags				(void) const { return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT; }
	virtual void		setDynamicAreas					(vector<deUint32> dynamicAreas) { m_dynamicAreas = dynamicAreas; }
	virtual bool		isDynamic						(void) const { return true; }

private:
	vector<deUint32>	m_dynamicAreas;
};

class UniformTexelBufferDescriptor : public BufferDescriptor
{
public:
						UniformTexelBufferDescriptor	(deUint32 arraySize = 1, deUint32 writeStart = 0, deUint32 elementsToWrite = 1, deUint32 numDynamicAreas = 1);
	virtual				~UniformTexelBufferDescriptor	(void);

	string				getShaderDeclaration			(void) const;
	string				getShaderVerifyCode				(void) const;
	VkBufferUsageFlags	getBufferUsageFlags				(void) const { return VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT; }
	bool				usesBufferView					(void) { return true; }
private:
};

class StorageTexelBufferDescriptor : public BufferDescriptor
{
public:
						StorageTexelBufferDescriptor	(deUint32 arraySize = 1u, deUint32 writeStart = 0u, deUint32 elementsToWrite = 1u, deUint32 numDynamicAreas = 1u);
	virtual				~StorageTexelBufferDescriptor	(void);

	string				getShaderDeclaration			(void) const;
	string				getShaderVerifyCode				(void) const;
	VkBufferUsageFlags	getBufferUsageFlags				(void) const { return VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT; }
	bool				usesBufferView					(void) { return true; }
private:
};

// Base class for all image based descriptors
class ImageDescriptor : public Descriptor
{
public:
									ImageDescriptor			(VkDescriptorType type, deUint32 arraySize, deUint32 writeStart, deUint32 elementsToWrite, deUint32 numDynamicAreas);
	virtual							~ImageDescriptor		(void);
	void							init					(Context& context, PipelineType pipelineType);

	VkWriteDescriptorSet			getDescriptorWrite		(void);
	virtual VkImageUsageFlags		getImageUsageFlags		(void) const = 0;
	virtual string					getShaderDeclaration	(void) const = 0;
	virtual string					getShaderVerifyCode		(void) const = 0;
	virtual VkAccessFlags			getAccessFlags			(void) const { return VK_ACCESS_SHADER_READ_BIT; }
	virtual VkImageLayout			getImageLayout			(void) const { return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; }

protected:
	vector<VkImageViewSp>			m_imageViews;

private:
	vector<ImageWithMemorySp>		m_images;
	vector<VkDescriptorImageInfo>	m_descriptorImageInfos;
	Move<VkSampler>					m_sampler;
};

class InputAttachmentDescriptor : public ImageDescriptor
{
public:
									InputAttachmentDescriptor	(deUint32 arraySize = 1u, deUint32 writeStart = 0u, deUint32 elementsToWrite = 1u, deUint32 numDynamicAreas = 1u);
	virtual							~InputAttachmentDescriptor	(void);

	VkImageUsageFlags				getImageUsageFlags			(void) const { return VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT; }
	string							getShaderDeclaration		(void) const;
	string							getShaderVerifyCode			(void) const;
	vector<VkImageViewSp>			getImageViews				(void) const { return m_imageViews; }
	void							copyValue					(const Descriptor& src, deUint32 srcElement, deUint32 dstElement, deUint32 numElements);
	VkAccessFlags					getAccessFlags				(void) const { return VK_ACCESS_INPUT_ATTACHMENT_READ_BIT; }
	vector<VkAttachmentReference>	getAttachmentReferences		(void) const;
	static deUint32					s_nextAttachmentIndex;
private:
	vector<deUint32>				m_attachmentIndices;
	deUint32						m_originalAttachmentIndex;
};

class CombinedImageSamplerDescriptor : public ImageDescriptor
{
public:
						CombinedImageSamplerDescriptor	(deUint32 arraySize = 1u, deUint32 writeStart = 0u, deUint32 elementsToWrite = 1u, deUint32 numDynamicAreas = 1u);
	virtual				~CombinedImageSamplerDescriptor	(void);

	VkImageUsageFlags	getImageUsageFlags				(void) const { return VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT; }
	string				getShaderDeclaration			(void) const;
	string				getShaderVerifyCode				(void) const;
private:
};

class SamplerDescriptor;

class SampledImageDescriptor : public ImageDescriptor
{
public:
								SampledImageDescriptor	(deUint32 arraySize = 1u, deUint32 writeStart = 0u, deUint32 elementsToWrite = 1u, deUint32 numDynamicAreas = 1u);
	virtual						~SampledImageDescriptor	(void);

	VkImageUsageFlags			getImageUsageFlags		(void) const { return VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT; }
	string						getShaderDeclaration	(void) const;
	string						getShaderVerifyCode		(void) const;
	void						addSampler				(SamplerDescriptor* sampler, deUint32 count = 1u) { for (deUint32 i = 0; i < count; i++) m_samplers.push_back(sampler); }
private:
	vector<SamplerDescriptor*>	m_samplers;
};

class SamplerDescriptor : public Descriptor
{
public:
									SamplerDescriptor		(deUint32 arraySize = 1u, deUint32 writeStart = 0u, deUint32 elementsToWrite = 1u, deUint32 numDynamicAreas = 1u);
	virtual							~SamplerDescriptor		(void);
	void							init					(Context& context, PipelineType pipelineType);

	void							addImage				(SampledImageDescriptor* image, deUint32 count = 1u) { for (deUint32 i = 0; i < count; i++ ) m_images.push_back(image); }
	VkWriteDescriptorSet			getDescriptorWrite		(void);
	string							getShaderDeclaration	(void) const;
	string							getShaderVerifyCode		(void) const;

private:
	vector<VkSamplerSp>				m_samplers;
	vector<VkDescriptorImageInfo>	m_descriptorImageInfos;
	vector<SampledImageDescriptor*>	m_images;
};

class StorageImageDescriptor : public ImageDescriptor
{
public:
						StorageImageDescriptor	(deUint32 arraySize = 1u, deUint32 writeStart = 0u, deUint32 elementsToWrite = 1u, deUint32 numDynamicAreas = 1u);
	virtual				~StorageImageDescriptor	(void);

	VkImageUsageFlags	getImageUsageFlags		(void) const { return VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT; }
	string				getShaderDeclaration	(void) const;
	string				getShaderVerifyCode		(void) const;
	VkImageLayout		getImageLayout			(void) const { return VK_IMAGE_LAYOUT_GENERAL; }
private:
};

class DescriptorSet
{
public:
								DescriptorSet	(void);
								~DescriptorSet	(void);
	void						addBinding		(DescriptorSp descriptor);
	const vector<DescriptorSp>	getBindings		(void) const { return m_bindings; }

private:
	vector<DescriptorSp>		m_bindings;
};

typedef de::SharedPtr<DescriptorSet> DescriptorSetSp;

// Class that handles descriptor sets and descriptors bound to those sets. Keeps track of copy operations.
class DescriptorCommands
{
public:
					DescriptorCommands			(PipelineType pipelineType);
					~DescriptorCommands			(void);
	void			addDescriptor				(DescriptorSp descriptor, deUint32 descriptorSet);
	void			copyDescriptor				(deUint32 srcSet, deUint32 srcBinding, deUint32	srcArrayElement, deUint32 dstSet, deUint32 dstBinding, deUint32 dstArrayElement, deUint32 descriptorCount);
	void			copyDescriptor				(deUint32 srcSet, deUint32 srcBinding, deUint32 dstSet, deUint32 dstBinding) { copyDescriptor(srcSet, srcBinding, 0u, dstSet, dstBinding, 0u, 1u); }
	string			getShaderDeclarations		(void) const;
	string			getDescriptorVerifications	(void) const;
	void			addResultBuffer				(void);
	deUint32		getResultBufferId			(void) const { return m_resultBuffer->getId(); }
	void			setDynamicAreas				(vector<deUint32> areas);
	bool			hasDynamicAreas				(void) const;
	PipelineType	getPipelineType				(void) const { return m_pipelineType; }

	tcu::TestStatus	run(Context& context);

private:
	PipelineType					m_pipelineType;
	vector<DescriptorSetSp>			m_descriptorSets;
	vector<DescriptorCopy>			m_descriptorCopies;
	vector<DescriptorSp>			m_descriptors;
	map<VkDescriptorType, deUint32>	m_descriptorCounts;
	DescriptorSp					m_resultBuffer;
	vector<deUint32>				m_dynamicAreas;
};

typedef de::SharedPtr<DescriptorCommands> DescriptorCommandsSp;

class DescriptorCopyTestInstance : public TestInstance
{
public:
							DescriptorCopyTestInstance	(Context& context, DescriptorCommandsSp commands);
							~DescriptorCopyTestInstance	(void);
	tcu::TestStatus			iterate						(void);
private:
	DescriptorCommandsSp	m_commands;
};

class DescriptorCopyTestCase : public TestCase
{
	public:
							DescriptorCopyTestCase	(tcu::TestContext& context, const char* name, const char* desc, DescriptorCommandsSp commands);
	virtual					~DescriptorCopyTestCase	(void);
	virtual	void			initPrograms			(SourceCollections& programCollection) const;
	virtual TestInstance*	createInstance			(Context& context) const;

private:
	DescriptorCommandsSp	m_commands;
};

deUint32 Descriptor::s_nextId = 0xabc; // Random starting point for ID counter
deUint32 InputAttachmentDescriptor::s_nextAttachmentIndex = 0;

Descriptor::Descriptor (VkDescriptorType	descriptorType,
						deUint32			arraySize,
						deUint32			writeStart,
						deUint32			elementsToWrite,
						deUint32			numDynamicAreas)
: m_descriptorType(descriptorType)
, m_arraySize(arraySize)
, m_id(s_nextId++)
, m_numDynamicAreas(numDynamicAreas)
{
	for (deUint32 arrayIdx = 0; arrayIdx < m_arraySize; arrayIdx++)
	{
		const bool				written			= arrayIdx >= writeStart && arrayIdx < writeStart + elementsToWrite;
		vector<deUint32>		data;

		for (deUint32 dynamicAreaIdx = 0; dynamicAreaIdx < m_numDynamicAreas; dynamicAreaIdx++)
			data.push_back(m_id + arrayIdx * m_numDynamicAreas + dynamicAreaIdx);

		const DescriptorData	descriptorData	=
		{
			data,		// vector<deUint32>	data
			written,	// bool				written
			false		// bool				copiedInto
		};

		m_data.push_back(descriptorData);
	}
}

Descriptor::~Descriptor (void)
{
}

// Copy refrence data from another descriptor
void Descriptor::copyValue (const Descriptor&	src,
							deUint32			srcElement,
							deUint32			dstElement,
							deUint32			numElements)
{
	for (deUint32 elementIdx = 0; elementIdx < numElements; elementIdx++)
	{
		DE_ASSERT(src.m_data[elementIdx + srcElement].written);

		for (deUint32 dynamicAreaIdx = 0; dynamicAreaIdx < de::min(m_numDynamicAreas, src.m_numDynamicAreas); dynamicAreaIdx++)
			m_data[elementIdx + dstElement].data[dynamicAreaIdx] = src.m_data[elementIdx + srcElement].data[dynamicAreaIdx];

		m_data[elementIdx + dstElement].copiedInto = true;
	}
}

string Descriptor::getArrayString (deUint32 index) const
{
	return m_arraySize > 1 ? (string("[") + de::toString(index) + "]") : "";
}

// Returns the first element to be written in descriptor update
deUint32 Descriptor::getFirstWrittenElement (void) const
{
	for (deUint32 i = 0; i < (deUint32)m_data.size(); i++)
		if (m_data[i].written)
			return i;

	return 0;
}

// Returns the number of array elements to be written for a descriptor array
deUint32 Descriptor::getNumWrittenElements (void) const
{
	deUint32	numElements = 0;

	for (deUint32 i = 0; i < (deUint32)m_data.size(); i++)
		if (m_data[i].written)
			numElements++;

	return numElements;
}

BufferDescriptor::BufferDescriptor (VkDescriptorType	type,
									deUint32			arraySize,
									deUint32			writeStart,
									deUint32			elementsToWrite,
									deUint32			numDynamicAreas)
: Descriptor(type, arraySize, writeStart, elementsToWrite, numDynamicAreas)
, m_bufferSize(256u * arraySize * numDynamicAreas)
{
}

BufferDescriptor::~BufferDescriptor (void)
{
}

void BufferDescriptor::init (Context&		context,
							 PipelineType	pipelineType)
{
	DE_UNREF(pipelineType);

	const DeviceInterface&		vk			= context.getDeviceInterface();
    const VkDevice				device		= context.getDevice();
    Allocator&					allocator	= context.getDefaultAllocator();

	// Create buffer
	{
		const VkBufferCreateInfo	bufferCreateInfo	=
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType		sType
			DE_NULL,								// const void*			pNext
			0u,										// VkBufferCreateFlags	flags
			m_bufferSize,							// VkDeviceSize			size
			getBufferUsageFlags(),					// VkBufferUsageFlags	usage
			VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode
			0u,										// uint32_t				queueFamilyIndexCount
			DE_NULL									// const uint32_t*		pQueueFamilyIndices
		};

		m_buffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(vk, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible));
	}

	// Create descriptor buffer infos
	{
		for (deUint32 arrayIdx = 0; arrayIdx < m_arraySize; arrayIdx++)
		{
			const VkDescriptorBufferInfo bufferInfo =
			{
				m_buffer->get(),						// VkBuffer		buffer
				256u * m_numDynamicAreas * arrayIdx,	// VkDeviceSize	offset
				isDynamic() ? 256u : 4u					// VkDeviceSize	range
			};

			m_descriptorBufferInfos.push_back(bufferInfo);
		}
	}

	// Create buffer views
	if (usesBufferView())
	{
		for (deUint32 viewIdx = 0; viewIdx < m_arraySize; viewIdx++)
		{
			const VkBufferViewCreateInfo bufferViewCreateInfo =
			{
				VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,	// VkStructureType			sType
				DE_NULL,									// const void*				pNext
				0u,											// VkBufferViewCreateFlags	flags
				m_buffer->get(),							// VkBuffer					buffer
				VK_FORMAT_R32_SFLOAT,						// VkFormat					format
				256u * viewIdx,								// VkDeviceSize				offset
				4u											// VkDeviceSize				range
			};

			m_bufferViews.push_back(VkBufferViewSp(new Unique<VkBufferView>(createBufferView(vk, device, &bufferViewCreateInfo))));
			m_bufferViewHandles.push_back(**m_bufferViews[viewIdx]);
		}
	}

	// Initialize buffer memory
	{
		deUint32* hostPtr = (deUint32*)m_buffer->getAllocation().getHostPtr();

		for (deUint32 arrayIdx = 0; arrayIdx < m_arraySize; arrayIdx++)
		{
			for (deUint32 dynamicAreaIdx = 0; dynamicAreaIdx < m_numDynamicAreas; dynamicAreaIdx++)
			{
				union BufferValue
				{
					deUint32	uintValue;
					float		floatValue;
				} bufferValue;

				bufferValue.uintValue = m_id + (arrayIdx * m_numDynamicAreas) + dynamicAreaIdx;

				if (usesBufferView())
					bufferValue.floatValue = (float)bufferValue.uintValue;

				hostPtr[(256 / 4) * (m_numDynamicAreas * arrayIdx + dynamicAreaIdx)] = bufferValue.uintValue;
			}
		}

		flushAlloc(vk, device, m_buffer->getAllocation());
	}
}

VkWriteDescriptorSet BufferDescriptor::getDescriptorWrite (void)
{
	const deUint32				firstElement	= getFirstWrittenElement();

	// Set and binding will be overwritten later
	const VkWriteDescriptorSet	descriptorWrite	=
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,									// VkStructureType					sType
		DE_NULL,																// const void*						pNext
		(VkDescriptorSet)0u,													// VkDescriptorSet					dstSet
		0u,																		// deUint32							dstBinding
		firstElement,															// deUint32							dstArrayElement
		getNumWrittenElements(),												// deUint32							descriptorCount
		getType(),																// VkDescriptorType					descriptorType
		DE_NULL,																// const VkDescriptorImageInfo		pImageInfo
		usesBufferView() ? DE_NULL : &m_descriptorBufferInfos[firstElement],	// const VkDescriptorBufferInfo*	pBufferInfo
		usesBufferView() ? &m_bufferViewHandles[firstElement] : DE_NULL			// const VkBufferView*				pTexelBufferView
	};

	return descriptorWrite;
}

void BufferDescriptor::invalidate (Context& context)
{
	const DeviceInterface&	vk		= context.getDeviceInterface();
	const VkDevice			device	= context.getDevice();

	invalidateAlloc(vk, device, m_buffer->getAllocation());
}

// Returns the buffer data as a vector
vector<deUint32> BufferDescriptor::getData (void)
{
	vector<deUint32>	data;
	deInt32*			hostPtr = (deInt32*)m_buffer->getAllocation().getHostPtr();

	for (deUint32 i = 0; i < m_arraySize; i++)
		data.push_back(hostPtr[i]);

	return data;
}

// Inline Uniform Block descriptor. These are similar to uniform buffers, but they can't form arrays for spec reasons.
// The array size is reused, instead, as the size of a data array inside the uniform block.
InlineUniformBlockDescriptor::InlineUniformBlockDescriptor (deUint32	arraySize,
															deUint32	writeStart,
															deUint32	elementsToWrite,
															deUint32	numDynamicAreas)
: Descriptor(VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT, arraySize, writeStart, elementsToWrite, 1u)
, m_blockElements(arraySize)
, m_writeStart(writeStart)
, m_elementsToWrite(elementsToWrite)
, m_writeStartByteOffset(m_writeStart * getElementSizeInBytes())
, m_bytesToWrite(m_elementsToWrite * getElementSizeInBytes())
{
	DE_UNREF(numDynamicAreas);
}

InlineUniformBlockDescriptor::~InlineUniformBlockDescriptor (void)
{
}

void InlineUniformBlockDescriptor::init (Context&		context,
										 PipelineType	pipelineType)
{
	DE_UNREF(context);
	DE_UNREF(pipelineType);

	// Initialize host memory.
	m_blockData.resize(m_blockElements);
	for (deUint32 i = 0; i < m_blockElements; ++i)
		m_blockData[i] = m_id + i;

	// Initialize descriptor write extension structure.
	m_inlineWrite.sType		= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_INLINE_UNIFORM_BLOCK_EXT;
	m_inlineWrite.pNext		= DE_NULL;
	m_inlineWrite.dataSize	= m_bytesToWrite;
	m_inlineWrite.pData		= &m_blockData[m_writeStart];
}

VkWriteDescriptorSet InlineUniformBlockDescriptor::getDescriptorWrite (void)
{
	// Set and binding will be overwritten later
	const VkWriteDescriptorSet	descriptorWrite	=
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,		// VkStructureType					sType
		&m_inlineWrite,								// const void*						pNext
		(VkDescriptorSet)0u,						// VkDescriptorSet					dstSet
		0u,											// deUint32							dstBinding
		m_writeStartByteOffset,						// deUint32							dstArrayElement
		m_bytesToWrite,								// deUint32							descriptorCount
		getType(),									// VkDescriptorType					descriptorType
		DE_NULL,									// const VkDescriptorImageInfo		pImageInfo
		DE_NULL,									// const VkDescriptorBufferInfo*	pBufferInfo
		DE_NULL										// const VkBufferView*				pTexelBufferView
	};

	return descriptorWrite;
}

string InlineUniformBlockDescriptor::getShaderDeclaration (void) const
{
	const string idStr = de::toString(m_id);
	return string(") uniform InlineUniformBlock" + idStr + "\n"
		"{\n"
		"	int data" + getArrayString(m_arraySize) + ";\n"
		"} inlineUniformBlock" + idStr + ";\n");
}

string InlineUniformBlockDescriptor::getShaderVerifyCode (void) const
{
	const string idStr = de::toString(m_id);
	string ret;

	for (deUint32 i = 0; i < m_arraySize; i++)
	{
		if (m_data[i].written || m_data[i].copiedInto)
		{
			ret += string("if (inlineUniformBlock") + idStr + ".data" + getArrayString(i) + " != " + de::toString(m_data[i].data[0]) + ") result = 0;\n";
		}
	}

	return ret;
}

UniformBufferDescriptor::UniformBufferDescriptor (deUint32	arraySize,
												  deUint32	writeStart,
												  deUint32	elementsToWrite,
												  deUint32	numDynamicAreas)
: BufferDescriptor(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, arraySize, writeStart, elementsToWrite, 1u)
{
	DE_UNREF(numDynamicAreas);
}

UniformBufferDescriptor::~UniformBufferDescriptor (void)
{
}

string UniformBufferDescriptor::getShaderDeclaration (void) const
{
	return string(
		") uniform UniformBuffer" + de::toString(m_id) + "\n"
		"{\n"
		"	int data;\n"
		"} uniformBuffer" + de::toString(m_id) + getArrayString(m_arraySize) +  ";\n");
}

string UniformBufferDescriptor::getShaderVerifyCode (void) const
{
	string ret;

	for (deUint32 i = 0; i < m_arraySize; i++)
	{
		if (m_data[i].written || m_data[i].copiedInto)
			ret += string("if (uniformBuffer") + de::toString(m_id) + getArrayString(i) + ".data != " + de::toString(m_data[i].data[0]) + ") result = 0;\n";
	}

	return ret;
}

DynamicUniformBufferDescriptor::DynamicUniformBufferDescriptor (deUint32	arraySize,
																deUint32	writeStart,
																deUint32	elementsToWrite,
																deUint32	numDynamicAreas)
: BufferDescriptor(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, arraySize, writeStart, elementsToWrite, numDynamicAreas)
{
}

DynamicUniformBufferDescriptor::~DynamicUniformBufferDescriptor (void)
{
}

string DynamicUniformBufferDescriptor::getShaderDeclaration (void) const
{
	return string(
		") uniform UniformBuffer" + de::toString(m_id) + "\n"
		"{\n"
		"	int data;\n"
		"} dynamicUniformBuffer" + de::toString(m_id) + getArrayString(m_arraySize) +  ";\n");
}

string DynamicUniformBufferDescriptor::getShaderVerifyCode (void) const
{
	string ret;

	for (deUint32 arrayIdx = 0; arrayIdx < m_arraySize; arrayIdx++)
	{
		if (m_data[arrayIdx].written || m_data[arrayIdx].copiedInto)
			ret += string("if (dynamicUniformBuffer") + de::toString(m_id) + getArrayString(arrayIdx) + ".data != " + de::toString(m_data[arrayIdx].data[m_dynamicAreas[arrayIdx]]) + ") result = 0;\n";
	}

	return ret;
}

StorageBufferDescriptor::StorageBufferDescriptor (deUint32	arraySize,
												  deUint32	writeStart,
												  deUint32	elementsToWrite,
												  deUint32	numDynamicAreas)
: BufferDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, arraySize, writeStart, elementsToWrite, 1u)
{
	DE_UNREF(numDynamicAreas);
}

StorageBufferDescriptor::~StorageBufferDescriptor (void)
{
}

string StorageBufferDescriptor::getShaderDeclaration (void) const
{
	return string(
		") buffer StorageBuffer" + de::toString(m_id) + "\n"
		"{\n"
		"	int data;\n"
		"} storageBuffer" + de::toString(m_id) + getArrayString(m_arraySize) + ";\n");
}

string StorageBufferDescriptor::getShaderVerifyCode (void) const
{
	string ret;

	for (deUint32 i = 0; i < m_arraySize; i++)
	{
		if (m_data[i].written || m_data[i].copiedInto)
			ret += string("if (storageBuffer") + de::toString(m_id) + getArrayString(i) + ".data != " + de::toString(m_data[i].data[0]) + ") result = 0;\n";
	}

	return ret;
}

DynamicStorageBufferDescriptor::DynamicStorageBufferDescriptor (deUint32	arraySize,
																deUint32	writeStart,
																deUint32	elementsToWrite,
																deUint32	numDynamicAreas)
: BufferDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, arraySize, writeStart, elementsToWrite, numDynamicAreas)
{
}

DynamicStorageBufferDescriptor::~DynamicStorageBufferDescriptor (void)
{
}

string DynamicStorageBufferDescriptor::getShaderDeclaration (void) const
{
	return string(
		") buffer StorageBuffer" + de::toString(m_id) + "\n"
		"{\n"
		"	int data;\n"
		"} dynamicStorageBuffer" + de::toString(m_id) + getArrayString(m_arraySize) +  ";\n");
}

string DynamicStorageBufferDescriptor::getShaderVerifyCode (void) const
{
	string ret;

	for (deUint32 arrayIdx = 0; arrayIdx < m_arraySize; arrayIdx++)
	{
		if (m_data[arrayIdx].written || m_data[arrayIdx].copiedInto)
			ret += string("if (dynamicStorageBuffer") + de::toString(m_id) + getArrayString(arrayIdx) + ".data != " + de::toString(m_data[arrayIdx].data[m_dynamicAreas[arrayIdx]]) + ") result = 0;\n";
	}

	return ret;
}

UniformTexelBufferDescriptor::UniformTexelBufferDescriptor (deUint32	arraySize,
															deUint32	writeStart,
															deUint32	elementsToWrite,
															deUint32	numDynamicAreas)
: BufferDescriptor(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, arraySize, writeStart, elementsToWrite, 1u)
{
	DE_UNREF(numDynamicAreas);
}

UniformTexelBufferDescriptor::~UniformTexelBufferDescriptor (void)
{
}

string UniformTexelBufferDescriptor::getShaderDeclaration (void) const
{
	return string(") uniform textureBuffer uniformTexelBuffer" + de::toString(m_id) + getArrayString(m_arraySize) + ";\n");
}

string UniformTexelBufferDescriptor::getShaderVerifyCode (void) const
{
	string ret;

	for (deUint32 i = 0; i < m_arraySize; i++)
	{
		if (m_data[i].written || m_data[i].copiedInto)
			ret += string("if (texelFetch(uniformTexelBuffer") + de::toString(m_id) + getArrayString(i) + ", 0).x != " + de::toString(m_data[i].data[0]) + ") result = 0;\n";
	}

	return ret;
}

StorageTexelBufferDescriptor::StorageTexelBufferDescriptor (deUint32	arraySize,
															deUint32	writeStart,
															deUint32	elementsToWrite,
															deUint32	numDynamicAreas)
: BufferDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, arraySize, writeStart, elementsToWrite, 1u)
{
	DE_UNREF(numDynamicAreas);
}

StorageTexelBufferDescriptor::~StorageTexelBufferDescriptor (void)
{
}

string StorageTexelBufferDescriptor::getShaderDeclaration (void) const
{
	return string(", r32f) uniform imageBuffer storageTexelBuffer" + de::toString(m_id) + getArrayString(m_arraySize) + ";\n");
}

string StorageTexelBufferDescriptor::getShaderVerifyCode (void) const
{
	string ret;

	for (deUint32 i = 0; i < m_arraySize; i++)
	{
		if (m_data[i].written || m_data[i].copiedInto)
			ret += string("if (imageLoad(storageTexelBuffer") + de::toString(m_id) + getArrayString(i) + ", 0).x != " + de::toString(m_data[i].data[0]) + ") result = 0;\n";
	}

	return ret;
}

ImageDescriptor::ImageDescriptor (VkDescriptorType	type,
								  deUint32			arraySize,
								  deUint32			writeStart,
								  deUint32			elementsToWrite,
								  deUint32			numDynamicAreas)
: Descriptor(type, arraySize, writeStart, elementsToWrite, 1u)
{
	DE_UNREF(numDynamicAreas);
}

ImageDescriptor::~ImageDescriptor (void)
{
}

void ImageDescriptor::init (Context&		context,
							PipelineType	pipelineType)
{
	const DeviceInterface&			vk					= context.getDeviceInterface();
    const VkDevice					device				= context.getDevice();
    Allocator&						allocator			= context.getDefaultAllocator();
	const VkQueue					queue				= context.getUniversalQueue();
	deUint32						queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
	const VkFormat					format				= VK_FORMAT_R32_SFLOAT;
	const VkComponentMapping		componentMapping	= makeComponentMappingRGBA();

	const VkImageSubresourceRange	subresourceRange	=
	{
		VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask
		0u,							// deUint32				baseMipLevel
		1u,							// deUint32				levelCount
		0u,							// deUint32				baseArrayLayer
		1u,							// deUint32				layerCount
	};

	// Create sampler
	{
		const tcu::Sampler			sampler			= tcu::Sampler(tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::NEAREST, tcu::Sampler::NEAREST);
		const tcu::TextureFormat	texFormat		= mapVkFormat(format);
		const VkSamplerCreateInfo	samplerParams	= mapSampler(sampler, texFormat);

		m_sampler = createSampler(vk, device, &samplerParams);
	}

	// Create images
	for (deUint32 imageIdx = 0; imageIdx < m_arraySize; imageIdx++)
	{
		const VkImageCreateInfo imageCreateInfo =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,						// VkStructureType			stype
			DE_NULL,													// const void*				pNext
			0u,															// VkImageCreateFlags		flags
			VK_IMAGE_TYPE_2D,											// VkImageType				imageType
			format,														// VkFormat					format
			{ (deUint32)renderSize.x(), (deUint32)renderSize.y(), 1 },	// VkExtent3D				extent
			1u,															// deUint32					mipLevels
			1u,															// deUint32					arrayLayers
			VK_SAMPLE_COUNT_1_BIT,										// VkSampleCountFlagBits	samples
			VK_IMAGE_TILING_OPTIMAL,									// VkImageTiling			tiling
			getImageUsageFlags(),										// VkImageUsageFlags		usage
			VK_SHARING_MODE_EXCLUSIVE,									// VkSharingMode			sharingMode
			1u,															// deUint32					queueFamilyIndexCount
			&queueFamilyIndex,											// const deUint32*			pQueueFamilyIndices
			VK_IMAGE_LAYOUT_UNDEFINED,									// VkImageLayout			initialLayout
		};

		m_images.push_back(ImageWithMemorySp(new ImageWithMemory(vk, device, allocator, imageCreateInfo, MemoryRequirement::Any)));
	}

	// Create image views
	for (deUint32 imageIdx = 0; imageIdx < m_arraySize; imageIdx++)
	{
		const VkImageViewCreateInfo imageViewCreateInfo =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// VkStructureType			sType
			DE_NULL,									// const void*				pNext
			0u,											// VkImageViewCreateFlags	flags
			**m_images[imageIdx],						// VkImage					image
			VK_IMAGE_VIEW_TYPE_2D,						// VkImageViewType			viewType
			format,										// VkFormat					format
			componentMapping,							// VkComponentMapping		components
			subresourceRange							// VkImageSubresourceRange	subresourceRange
		};

		m_imageViews.push_back(VkImageViewSp(new Unique<VkImageView>(createImageView(vk, device, &imageViewCreateInfo))));
	}

	// Create descriptor image infos
	{
		for (deUint32 i = 0; i < m_arraySize; i++)
		{
			const VkDescriptorImageInfo imageInfo =
			{
				*m_sampler,			// VkSampler		sampler
				**m_imageViews[i],	// VkImageView		imageView
				getImageLayout()	// VkImageLayout	imageLayout
			};

			m_descriptorImageInfos.push_back(imageInfo);
		}
	}

	// Clear images to reference value
	for (deUint32 imageIdx = 0; imageIdx < m_arraySize; imageIdx++)
	{
		const Unique<VkCommandPool>		cmdPool				(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex));
		const Unique<VkCommandBuffer>	cmdBuffer			(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

		const float						clearValue			= (float)(m_id + imageIdx);
		const VkClearValue				clearColor			= makeClearValueColorF32(clearValue, clearValue, clearValue, clearValue);

		const VkImageMemoryBarrier		preImageBarrier		=
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType
			DE_NULL,									// const void*				pNext
			0u,											// VkAccessFlags			srcAccessMask
			VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			dstAccessMask
			VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout			oldLayout
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			newLayout
			queueFamilyIndex,							// deUint32					srcQueueFamilyIndex
			queueFamilyIndex,							// deUint32					dstQueueFamilyIndex
			**m_images[imageIdx],						// VkImage					image
			subresourceRange							// VkImageSubresourceRange	subresourceRange
		};

		const VkImageMemoryBarrier		postImageBarrier	=
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType
			DE_NULL,									// const void*				pNext
			VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			srcAccessMask
			getAccessFlags(),							// VkAccessFlags			dstAccessMask
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			oldLayout
			getImageLayout(),							// VkImageLayout			newLayout
			queueFamilyIndex,							// deUint32					srcQueueFamilyIndex
			queueFamilyIndex,							// deUint32					dstQueueFamilyIndex
			**m_images[imageIdx],						// VkImage					image
			subresourceRange							// VkImageSubresourceRange	subresourceRange
		};

		beginCommandBuffer(vk, *cmdBuffer);
		vk.cmdPipelineBarrier(*cmdBuffer,
							  VK_PIPELINE_STAGE_HOST_BIT,
							  VK_PIPELINE_STAGE_TRANSFER_BIT,
							  (VkDependencyFlags)0u,
							  0u, (const VkMemoryBarrier*)DE_NULL,
							  0u, (const VkBufferMemoryBarrier*)DE_NULL,
							  1u, &preImageBarrier);
		vk.cmdClearColorImage(*cmdBuffer, **m_images[imageIdx], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor.color, 1, &subresourceRange);
		vk.cmdPipelineBarrier(*cmdBuffer,
							  VK_PIPELINE_STAGE_TRANSFER_BIT,
							  pipelineType == PIPELINE_TYPE_COMPUTE ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
							  (VkDependencyFlags)0u,
							  0u, (const VkMemoryBarrier*)DE_NULL,
							  0u, (const VkBufferMemoryBarrier*)DE_NULL,
							  1u, &postImageBarrier);
		endCommandBuffer(vk, *cmdBuffer);
		submitCommandsAndWait(vk, device, queue, *cmdBuffer);
	}
}

VkWriteDescriptorSet ImageDescriptor::getDescriptorWrite (void)
{
	const deUint32				firstElement	= getFirstWrittenElement();

	// Set and binding will be overwritten later
	const VkWriteDescriptorSet	descriptorWrite	=
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,	// VkStructureType					sType
		DE_NULL,								// const void*						pNext
		(VkDescriptorSet)0u,					// VkDescriptorSet					dstSet
		0u,										// deUint32							dstBinding
		firstElement,							// deUint32							dstArrayElement
		getNumWrittenElements(),				// deUint32							descriptorCount
		getType(),								// VkDescriptorType					descriptorType
		&m_descriptorImageInfos[firstElement],	// const VkDescriptorImageInfo		pImageInfo
		DE_NULL,								// const VkDescriptorBufferInfo*	pBufferInfo
		DE_NULL									// const VkBufferView*				pTexelBufferView
	};

	return descriptorWrite;
}

InputAttachmentDescriptor::InputAttachmentDescriptor (deUint32	arraySize,
													  deUint32	writeStart,
													  deUint32	elementsToWrite,
													  deUint32	numDynamicAreas)
: ImageDescriptor(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, arraySize, writeStart, elementsToWrite, 1u)
, m_originalAttachmentIndex(s_nextAttachmentIndex)
{
	DE_UNREF(numDynamicAreas);

	for (deUint32 i = 0; i < m_arraySize; i++)
		m_attachmentIndices.push_back(s_nextAttachmentIndex++);
}

InputAttachmentDescriptor::~InputAttachmentDescriptor (void)
{
}

string InputAttachmentDescriptor::getShaderDeclaration (void) const
{
	return string(", input_attachment_index=" + de::toString(m_originalAttachmentIndex) + ") uniform subpassInput inputAttachment" + de::toString(m_id) + getArrayString(m_arraySize) + ";\n");
}

string InputAttachmentDescriptor::getShaderVerifyCode (void) const
{
	string ret;

	for (deUint32 i = 0; i < m_arraySize; i++)
	{
		if (m_data[i].written || m_data[i].copiedInto)
			ret += string("if (subpassLoad(inputAttachment") + de::toString(m_id) + getArrayString(i) + ").x != " + de::toString(m_data[i].data[0]) + ") result = 0;\n";
	}

	return ret;
}

void InputAttachmentDescriptor::copyValue (const Descriptor&	src,
										   deUint32				srcElement,
										   deUint32				dstElement,
										   deUint32				numElements)
{
	Descriptor::copyValue(src, srcElement, dstElement, numElements);

	for (deUint32 elementIdx = 0; elementIdx < numElements; elementIdx++)
	{
		m_attachmentIndices[elementIdx + dstElement] = reinterpret_cast<const InputAttachmentDescriptor&>(src).m_attachmentIndices[elementIdx + srcElement];
	}
}

vector<VkAttachmentReference> InputAttachmentDescriptor::getAttachmentReferences (void) const
{
	vector<VkAttachmentReference> references;
	for (deUint32 i = 0; i < m_arraySize; i++)
	{
		const VkAttachmentReference attachmentReference =
		{
			// The first attachment is the color buffer, thus +1
			m_attachmentIndices[i] + 1,	// deUint32			attachment
			getImageLayout()			// VkImageLayout	layout
		};

		references.push_back(attachmentReference);
	}

	return references;
}

CombinedImageSamplerDescriptor::CombinedImageSamplerDescriptor (deUint32	arraySize,
																deUint32	writeStart,
																deUint32	elementsToWrite,
																deUint32	numDynamicAreas)
: ImageDescriptor(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, arraySize, writeStart, elementsToWrite, 1u)
{
	DE_UNREF(numDynamicAreas);
}

CombinedImageSamplerDescriptor::~CombinedImageSamplerDescriptor (void)
{
}

string CombinedImageSamplerDescriptor::getShaderDeclaration (void) const
{
	return string(") uniform sampler2D texSampler" + de::toString(m_id) + getArrayString(m_arraySize) + ";\n");
}

string CombinedImageSamplerDescriptor::getShaderVerifyCode (void) const
{
	string ret;

	for (deUint32 i = 0; i < m_arraySize; i++)
	{
		if (m_data[i].written || m_data[i].copiedInto)
			ret += string("if (texture(texSampler") + de::toString(m_id) + getArrayString(i) + ", vec2(0)).x != " + de::toString(m_data[i].data[0]) + ") result = 0;\n";
	}

	return ret;
}

SampledImageDescriptor::SampledImageDescriptor (deUint32	arraySize,
												deUint32	writeStart,
												deUint32	elementsToWrite,
												deUint32	numDynamicAreas)
: ImageDescriptor(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, arraySize, writeStart, elementsToWrite, 1u)
{
	DE_UNREF(numDynamicAreas);
}

SampledImageDescriptor::~SampledImageDescriptor (void)
{
}

string SampledImageDescriptor::getShaderDeclaration (void) const
{
	return string(") uniform texture2D sampledImage" + de::toString(m_id) + getArrayString(m_arraySize) + ";\n");
}

string SampledImageDescriptor::getShaderVerifyCode (void) const
{
	string ret;

	for (deUint32 i = 0; i < m_arraySize; i++)
	{
		if ((m_data[i].written || m_data[i].copiedInto) && m_samplers.size() > i)
		{
			ret += string("if (texture(sampler2D(sampledImage") + de::toString(m_id) + getArrayString(i) + ", sampler" + de::toString(m_samplers[i]->getId()) + "), vec2(0)).x != " + de::toString(m_data[i].data[0]) + ") result = 0;\n";
		}
	}

	return ret;
}

StorageImageDescriptor::StorageImageDescriptor (deUint32	arraySize,
												deUint32	writeStart,
												deUint32	elementsToWrite,
												deUint32	numDynamicAreas)
: ImageDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, arraySize, writeStart, elementsToWrite, 1u)
{
	DE_UNREF(numDynamicAreas);
}

StorageImageDescriptor::~StorageImageDescriptor (void)
{
}

string StorageImageDescriptor::getShaderDeclaration (void) const
{
	return string(", r32f) readonly uniform image2D image" + de::toString(m_id) + getArrayString(m_arraySize) + ";\n");
}

string StorageImageDescriptor::getShaderVerifyCode (void) const
{
	string ret;

	for (deUint32 i = 0; i < m_arraySize; i++)
	{
		if (m_data[i].written || m_data[i].copiedInto)
			ret += string("if (imageLoad(image") + de::toString(m_id) + getArrayString(i) + ", ivec2(0)).x != " + de::toString(m_data[i].data[0]) + ") result = 0;\n";
	}

	return ret;
}

SamplerDescriptor::SamplerDescriptor (deUint32	arraySize,
									  deUint32	writeStart,
									  deUint32	elementsToWrite,
									  deUint32	numDynamicAreas)
: Descriptor(VK_DESCRIPTOR_TYPE_SAMPLER, arraySize, writeStart, elementsToWrite, 1u)
{
	DE_UNREF(numDynamicAreas);
}

SamplerDescriptor::~SamplerDescriptor (void)
{
}

void SamplerDescriptor::init (Context&		context,
							  PipelineType	pipelineType)
{
	DE_UNREF(pipelineType);

	const DeviceInterface&	vk		= context.getDeviceInterface();
	const VkDevice			device	= context.getDevice();
	const VkFormat			format	= VK_FORMAT_R32_SFLOAT;

	// Create samplers
	for (deUint32 i = 0; i < m_arraySize; i++)
	{
		const float					borderValue		= (float)((m_id + i) % 2);
		const tcu::Sampler			sampler			= tcu::Sampler(tcu::Sampler::CLAMP_TO_BORDER, tcu::Sampler::CLAMP_TO_BORDER, tcu::Sampler::CLAMP_TO_BORDER, tcu::Sampler::NEAREST, tcu::Sampler::NEAREST, 0.0f, true, tcu::Sampler::COMPAREMODE_NONE, 0, Vec4(borderValue));
		const tcu::TextureFormat	texFormat		= mapVkFormat(format);
		const VkSamplerCreateInfo	samplerParams	= mapSampler(sampler, texFormat);

		m_samplers.push_back(VkSamplerSp(new Unique<VkSampler>(createSampler(vk, device, &samplerParams))));
	}

	// Create descriptor image infos
	for (deUint32 i = 0; i < m_arraySize; i++)
	{
		const VkDescriptorImageInfo imageInfo =
		{
			**m_samplers[i],			// VkSampler		sampler
			DE_NULL,					// VkImageView		imageView
			VK_IMAGE_LAYOUT_UNDEFINED	// VkImageLayout	imageLayout
		};

		m_descriptorImageInfos.push_back(imageInfo);
	}
}

VkWriteDescriptorSet SamplerDescriptor::getDescriptorWrite (void)
{
	const deUint32 firstElement = getFirstWrittenElement();

	// Set and binding will be overwritten later
	const VkWriteDescriptorSet	descriptorWrite	=
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,	// VkStructureType					sType
		DE_NULL,								// const void*						pNext
		(VkDescriptorSet)0u,					// VkDescriptorSet					dstSet
		0u,										// deUint32							dstBinding
		firstElement,							// deUint32							dstArrayElement
		getNumWrittenElements(),				// deUint32							descriptorCount
		getType(),								// VkDescriptorType					descriptorType
		&m_descriptorImageInfos[firstElement],	// const VkDescriptorImageInfo		pImageInfo
		DE_NULL,								// const VkDescriptorBufferInfo*	pBufferInfo
		DE_NULL									// const VkBufferView*				pTexelBufferView
	};

	return descriptorWrite;
}

string SamplerDescriptor::getShaderDeclaration (void) const
{
	return string(") uniform sampler sampler" + de::toString(m_id) + getArrayString(m_arraySize) + ";\n");
}

string SamplerDescriptor::getShaderVerifyCode (void) const
{
	string ret;

	for (deUint32 i = 0; i < m_arraySize; i++)
	{
		if ((m_data[i].written || m_data[i].copiedInto) && m_images.size() > i)
		{
			// Sample from (-1, -1) to get border color.
			ret += string("if (texture(sampler2D(sampledImage") + de::toString(m_images[i]->getId()) + ", sampler" + de::toString(m_id) + getArrayString(i) + "), vec2(-1)).x != " + de::toString(m_data[i].data[0] % 2) + ") result = 0;\n";
		}
	}

	return ret;
}

DescriptorSet::DescriptorSet (void)
{
}

DescriptorSet::~DescriptorSet (void)
{
}

void DescriptorSet::addBinding (DescriptorSp descriptor)
{
	m_bindings.push_back(descriptor);
}

DescriptorCommands::DescriptorCommands (PipelineType pipelineType)
: m_pipelineType(pipelineType)
{
	// Reset counters
	Descriptor::s_nextId = 0xabc;
	InputAttachmentDescriptor::s_nextAttachmentIndex = 0;
}

DescriptorCommands::~DescriptorCommands (void)
{
}

void DescriptorCommands::addDescriptor (DescriptorSp	descriptor,
										deUint32		descriptorSet)
{
	const VkDescriptorType type = descriptor->getType();

	// Create descriptor set objects until one with the given index exists
	while (m_descriptorSets.size() <= descriptorSet)
		m_descriptorSets.push_back(DescriptorSetSp(new DescriptorSet()));

	m_descriptorSets[descriptorSet]->addBinding(descriptor);

	// Keep track of how many descriptors of each type is needed. Inline uniform blocks cannot form arrays. We reuse the array size
	// as size of the data array for them, within a single descriptor.
	const deUint32 count = ((type == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT) ? 1u : descriptor->getArraySize());
	if (m_descriptorCounts.find(type) != m_descriptorCounts.end())
		m_descriptorCounts[type] += count;
	else
		m_descriptorCounts[type] = count;

	// Keep descriptors also in a flat list for easier iteration
	m_descriptors.push_back(descriptor);
}

void DescriptorCommands::copyDescriptor (deUint32	srcSet,
										 deUint32	srcBinding,
										 deUint32	srcArrayElement,
										 deUint32	dstSet,
										 deUint32	dstBinding,
										 deUint32	dstArrayElement,
										 deUint32	descriptorCount)
{
	// For inline uniform blocks, (src|dst)ArrayElement are data array indices and descriptorCount is the number of integers to copy.
	DescriptorCopy descriptorCopy = { srcSet, srcBinding, srcArrayElement, dstSet, dstBinding, dstArrayElement, descriptorCount };

	if (m_descriptorSets[srcSet]->getBindings()[srcBinding]->getType() == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
	{
		// For inline uniform blocks, these members of VkCopyDescriptorSet are offsets and sizes in bytes.
		const InlineUniformBlockDescriptor* iub	=	static_cast<InlineUniformBlockDescriptor*>(m_descriptorSets[srcSet]->getBindings()[srcBinding].get());
		const deUint32 elementSize				=	iub->getElementSizeInBytes();

		descriptorCopy.srcArrayElement *= elementSize;
		descriptorCopy.dstArrayElement *= elementSize;
		descriptorCopy.descriptorCount *= elementSize;
	}

	m_descriptorCopies.push_back(descriptorCopy);
	m_descriptorSets[descriptorCopy.dstSet]->getBindings()[descriptorCopy.dstBinding]->copyValue(*m_descriptorSets[descriptorCopy.srcSet]->getBindings()[descriptorCopy.srcBinding], srcArrayElement, dstArrayElement, descriptorCount);
}

// Generates shader source code for declarations of all descriptors
string DescriptorCommands::getShaderDeclarations (void) const
{
	string ret;

	for (size_t descriptorSetIdx = 0; descriptorSetIdx < m_descriptorSets.size(); descriptorSetIdx++)
	{
		const vector<DescriptorSp> bindings = m_descriptorSets[descriptorSetIdx]->getBindings();

		for (size_t bindingIdx = 0; bindingIdx < bindings.size(); bindingIdx++)
		{
			ret += "layout (set=" + de::toString(descriptorSetIdx) + ", binding=" + de::toString(bindingIdx) + bindings[bindingIdx]->getShaderDeclaration();
		}
	}

	return ret;
}

// Generates shader source code for verification of all descriptor data
string DescriptorCommands::getDescriptorVerifications (void) const
{
	string ret;

	for (size_t descriptorSetIdx = 0; descriptorSetIdx < m_descriptorSets.size(); descriptorSetIdx++)
	{
		const vector<DescriptorSp> bindings = m_descriptorSets[descriptorSetIdx]->getBindings();

		for (size_t bindingIdx = 0; bindingIdx < bindings.size(); bindingIdx++)
		{
			if (m_pipelineType == PIPELINE_TYPE_COMPUTE && descriptorSetIdx == 0 && bindingIdx == bindings.size() - 1) continue; // Skip the result buffer which is always the last descriptor of set 0
			ret += bindings[bindingIdx]->getShaderVerifyCode();
		}
	}

	return ret;
}

void DescriptorCommands::addResultBuffer (void)
{
	// Add result buffer if using compute pipeline
	if (m_pipelineType == PIPELINE_TYPE_COMPUTE)
	{
		m_resultBuffer = DescriptorSp(new StorageBufferDescriptor());
		addDescriptor(m_resultBuffer, 0u);
	}
}

// Sets the list of dynamic areas selected for each dynamic descriptor when running the verification shader
void DescriptorCommands::setDynamicAreas (vector<deUint32> areas)
{
	m_dynamicAreas = areas;
	deUint32 areaIdx = 0;

	for (vector<DescriptorSp>::iterator desc = m_descriptors.begin(); desc != m_descriptors.end(); desc++)
	{
		if ((*desc)->isDynamic())
		{
			vector<deUint32> dynamicAreas;

			for (deUint32 elementIdx = 0; elementIdx < (*desc)->getArraySize(); elementIdx++)
				dynamicAreas.push_back(areas[areaIdx++]);

			(*desc)->setDynamicAreas(dynamicAreas);
		}
	}
}

bool DescriptorCommands::hasDynamicAreas (void) const
{
	for (vector<DescriptorSp>::const_iterator desc = m_descriptors.begin(); desc != m_descriptors.end(); desc++)
		if ((*desc)->isDynamic())
			return true;

	return false;
}

tcu::TestStatus DescriptorCommands::run (Context& context)
{
	const InstanceInterface&				vki					= context.getInstanceInterface();
	const DeviceInterface&					vk					= context.getDeviceInterface();
	const VkDevice							device				= context.getDevice();
	const VkQueue							queue				= context.getUniversalQueue();
	const VkPhysicalDevice					physicalDevice		= context.getPhysicalDevice();
	const VkPhysicalDeviceLimits			limits				= getPhysicalDeviceProperties(vki, physicalDevice).limits;
	const deUint32							queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
	Allocator&								allocator			= context.getDefaultAllocator();
	tcu::TestLog&							log					= context.getTestContext().getLog();
	const Unique<VkCommandPool>				commandPool			(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>			commandBuffer		(allocateCommandBuffer(vk, device, *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	const VkShaderStageFlags				shaderStage			= m_pipelineType == PIPELINE_TYPE_COMPUTE ? VK_SHADER_STAGE_COMPUTE_BIT : VK_SHADER_STAGE_FRAGMENT_BIT;
	const VkFormat							resultFormat		= VK_FORMAT_R8G8B8A8_UNORM;
	deUint32								numTotalIUBs		= 0;
	deUint32								iubTotalBytes		= 0;
	de::MovePtr<ImageWithMemory>			resultImage;
	de::MovePtr<BufferWithMemory>			resultImageBuffer;
	Move<VkImageView>						resultImageView;
	Move<VkRenderPass>						renderPass;
	Move<VkFramebuffer>						framebuffer;
	Move<VkDescriptorPool>					descriptorPool;
	vector<VkDescriptorSetLayoutSp>			descriptorSetLayouts;
	vector<VkDescriptorSet>					descriptorSets;
	Move<VkPipelineLayout>					pipelineLayout;
	Move<VkPipeline>						pipeline;
	vector<VkAttachmentReference>			inputAttachments;
	vector<VkAttachmentDescription>			attachmentDescriptions;
	vector<VkImageView>						imageViews;

	if (limits.maxBoundDescriptorSets <= m_descriptorSets.size())
		TCU_THROW(NotSupportedError, "Maximum bound descriptor sets limit exceeded.");

	// Check if inline uniform blocks are supported.
	VkPhysicalDeviceInlineUniformBlockFeaturesEXT	iubFeatures =
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_FEATURES_EXT,
		DE_NULL,
		VK_FALSE, VK_FALSE
	};
	VkPhysicalDeviceInlineUniformBlockPropertiesEXT	iubProperties =
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_PROPERTIES_EXT,
		DE_NULL,
		0u, 0u, 0u, 0u, 0u
	};
	{
		if (context.isDeviceFunctionalitySupported("VK_EXT_inline_uniform_block"))
		{
			VkPhysicalDeviceFeatures2 features2;
			deMemset(&features2, 0, sizeof(features2));
			features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
			features2.pNext = &iubFeatures;
			vki.getPhysicalDeviceFeatures2(physicalDevice, &features2);

			VkPhysicalDeviceProperties2 properties2;
			deMemset(&properties2, 0, sizeof(properties2));
			properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
			properties2.pNext = &iubProperties;
			vki.getPhysicalDeviceProperties2(physicalDevice, &properties2);
		}
	}

	// Check physical device limits of per stage and per desriptor set descriptor count
	{
		deUint32	numPerStageSamplers			= 0;
		deUint32	numPerStageUniformBuffers	= 0;
		deUint32	numPerStageStorageBuffers	= 0;
		deUint32	numPerStageSampledImages	= 0;
		deUint32	numPerStageStorageImages	= 0;
		deUint32	numPerStageInputAttachments	= 0;
		deUint32	numPerStageTotalResources	= 0;

		for (size_t descriptorSetIdx = 0; descriptorSetIdx < m_descriptorSets.size(); descriptorSetIdx++)
		{
			deUint32					numSamplers					= 0;
			deUint32					numUniformBuffers			= 0;
			deUint32					numUniformBuffersDynamic	= 0;
			deUint32					numStorageBuffers			= 0;
			deUint32					numStorageBuffersDynamic	= 0;
			deUint32					numSampledImages			= 0;
			deUint32					numStorageImages			= 0;
			deUint32					numInputAttachments			= 0;
			deUint32					numIUBs						= 0;
			deUint32					numTotalResources			= m_pipelineType == PIPELINE_TYPE_GRAPHICS ? 1u : 0u; // Color buffer counts as a resource.

			const vector<DescriptorSp>&	bindings					= m_descriptorSets[descriptorSetIdx]->getBindings();

			for (size_t bindingIdx = 0; bindingIdx < bindings.size(); bindingIdx++)
			{
				const deUint32 arraySize = bindings[bindingIdx]->getArraySize();

				// Inline uniform blocks cannot form arrays. The array size is the size of the data array in the descriptor.
				if (bindings[bindingIdx]->getType() == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
				{
					const InlineUniformBlockDescriptor* iub		= static_cast<InlineUniformBlockDescriptor*>(bindings[bindingIdx].get());
					const deUint32						bytes	= iub->getSizeInBytes();

					// Check inline uniform block size.
					if (bytes > iubProperties.maxInlineUniformBlockSize)
					{
						std::ostringstream msg;
						msg << "Maximum size for an inline uniform block exceeded by binding "
							<< bindingIdx << " from set " << descriptorSetIdx;
						TCU_THROW(NotSupportedError, msg.str().c_str());
					}

					iubTotalBytes += bytes;
					++numTotalResources;
				}
				else
				{
					numTotalResources += arraySize;
				}

				switch (bindings[bindingIdx]->getType())
				{
					case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
						numUniformBuffers += arraySize;
						break;

					case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
						numUniformBuffers += arraySize;
						numUniformBuffersDynamic += arraySize;
						break;

					case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
						numStorageBuffers += arraySize;
						break;

					case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
						numStorageBuffers += arraySize;
						numStorageBuffersDynamic += arraySize;
						break;

					case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
						numSamplers += arraySize;
						numSampledImages += arraySize;
						break;

					case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
					case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
						numStorageImages += arraySize;
						break;

					case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
						numInputAttachments += arraySize;
						break;

					case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
					case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
						numSampledImages += arraySize;
						break;

					case VK_DESCRIPTOR_TYPE_SAMPLER:
						numSamplers += arraySize;
						break;

					case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT:
						++numIUBs;
						break;

					default:
						DE_FATAL("Unexpected descriptor type");
						break;
				}
			}

			if (numSamplers > limits.maxDescriptorSetSamplers)
				TCU_THROW(NotSupportedError, "Maximum per descriptor set sampler limit exceeded.");

			if (numUniformBuffers > limits.maxDescriptorSetUniformBuffers)
				TCU_THROW(NotSupportedError, "Maximum per descriptor set uniform buffer limit exceeded.");

			if (numUniformBuffersDynamic > limits.maxDescriptorSetUniformBuffersDynamic)
				TCU_THROW(NotSupportedError, "Maximum per descriptor set uniform buffer dynamic limit exceeded.");

			if (numStorageBuffers > limits.maxDescriptorSetStorageBuffers)
				TCU_THROW(NotSupportedError, "Maximum per descriptor set storage buffer limit exceeded.");

			if (numStorageBuffersDynamic > limits.maxDescriptorSetStorageBuffersDynamic)
				TCU_THROW(NotSupportedError, "Maximum per descriptor set storage buffer dynamic limit exceeded.");

			if (numSampledImages > limits.maxDescriptorSetSampledImages)
				TCU_THROW(NotSupportedError, "Maximum per descriptor set sampled image limit exceeded.");

			if (numStorageImages > limits.maxDescriptorSetStorageImages)
				TCU_THROW(NotSupportedError, "Maximum per descriptor set storage image limit exceeded.");

			if (numInputAttachments > limits.maxDescriptorSetInputAttachments)
				TCU_THROW(NotSupportedError, "Maximum per descriptor set input attachment limit exceeded.");

			numPerStageSamplers			+= numSamplers;
			numPerStageUniformBuffers	+= numUniformBuffers;
			numPerStageStorageBuffers	+= numStorageBuffers;
			numPerStageSampledImages	+= numSampledImages;
			numPerStageStorageImages	+= numStorageImages;
			numPerStageInputAttachments	+= numInputAttachments;
			numPerStageTotalResources	+= numTotalResources;
			numTotalIUBs				+= numIUBs;
		}

		if (numPerStageTotalResources > limits.maxPerStageResources)
			TCU_THROW(NotSupportedError, "Maximum per stage total resource limit exceeded.");

		if (numPerStageSamplers > limits.maxPerStageDescriptorSamplers)
			TCU_THROW(NotSupportedError, "Maximum per stage sampler limit exceeded.");

		if (numPerStageUniformBuffers > limits.maxPerStageDescriptorUniformBuffers)
			TCU_THROW(NotSupportedError, "Maximum per stage uniform buffer limit exceeded.");

		if (numPerStageStorageBuffers > limits.maxPerStageDescriptorStorageBuffers)
			TCU_THROW(NotSupportedError, "Maximum per stage storage buffer limit exceeded.");

		if (numPerStageSampledImages > limits.maxPerStageDescriptorSampledImages)
			TCU_THROW(NotSupportedError, "Maximum per stage sampled image limit exceeded.");

		if (numPerStageStorageImages > limits.maxPerStageDescriptorStorageImages)
			TCU_THROW(NotSupportedError, "Maximum per stage storage image limit exceeded.");

		if (numPerStageInputAttachments > limits.maxPerStageDescriptorInputAttachments)
			TCU_THROW(NotSupportedError, "Maximum per stage input attachment limit exceeded.");

		if (numTotalIUBs > iubProperties.maxDescriptorSetInlineUniformBlocks ||
			numTotalIUBs > iubProperties.maxPerStageDescriptorInlineUniformBlocks)
		{
			TCU_THROW(NotSupportedError, "Number of per stage inline uniform blocks exceeds limits.");
		}
	}

	// Initialize all descriptors
	for (vector<DescriptorSp>::iterator desc = m_descriptors.begin(); desc != m_descriptors.end(); desc++)
		(*desc)->init(context, m_pipelineType);

	// Create descriptor pool
	{
		vector<VkDescriptorPoolSize> poolSizes;

		for (map<VkDescriptorType, deUint32>::iterator i = m_descriptorCounts.begin(); i != m_descriptorCounts.end(); i++)
		{
			VkDescriptorPoolSize poolSize =
			{
				i->first,	// VkDescriptorType	type
				i->second	// deUint32			descriptorCount
			};

			// Inline uniform blocks have a special meaning for descriptorCount.
			if (poolSize.type == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
				poolSize.descriptorCount = iubTotalBytes;

			poolSizes.push_back(poolSize);
		}

		VkDescriptorPoolCreateInfo descriptorPoolCreateInfo =
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,		// VkStructureType				sType
			DE_NULL,											// const void*					pNext
			VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,	// VkDescriptorPoolCreateFlags	flags
			(deUint32)m_descriptorSets.size(),					// deUint32						maxSets
			(deUint32)poolSizes.size(),							// deUint32						poolSizeCount
			poolSizes.data(),									// const VkDescriptorPoolSize*	pPoolSizes
		};

		// Include information about inline uniform blocks if needed.
		VkDescriptorPoolInlineUniformBlockCreateInfoEXT iubPoolCreateInfo =
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_INLINE_UNIFORM_BLOCK_CREATE_INFO_EXT,
			DE_NULL,
			numTotalIUBs
		};
		if (numTotalIUBs > 0)
			descriptorPoolCreateInfo.pNext = &iubPoolCreateInfo;

		descriptorPool = createDescriptorPool(vk, device, &descriptorPoolCreateInfo);
	}

	// Create descriptor set layouts. One for each descriptor set used in this test.
	{
		for (size_t descriptorSetIdx = 0; descriptorSetIdx < m_descriptorSets.size(); descriptorSetIdx++)
		{
			vector<VkDescriptorSetLayoutBinding>	layoutBindings;
			const vector<DescriptorSp>&				bindings		= m_descriptorSets[descriptorSetIdx]->getBindings();

			for (size_t bindingIdx = 0; bindingIdx < bindings.size(); bindingIdx++)
			{
				VkDescriptorSetLayoutBinding layoutBinding =
				{
					(deUint32)bindingIdx,					// deUint32				binding
					bindings[bindingIdx]->getType(),		// VkDescriptorType		descriptorType
					bindings[bindingIdx]->getArraySize(),	// deUint32				descriptorCount
					shaderStage,							// VkShaderStageFlags	stageFlags
					DE_NULL									// const VkSampler*		pImmutableSamplers
				};

				// Inline uniform blocks have a special meaning for descriptorCount.
				if (layoutBinding.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
				{
					const InlineUniformBlockDescriptor* iub = static_cast<InlineUniformBlockDescriptor*>(bindings[bindingIdx].get());
					layoutBinding.descriptorCount = iub->getSizeInBytes();
				}

				layoutBindings.push_back(layoutBinding);
			}

			const VkDescriptorSetLayoutCreateInfo	descriptorSetLayoutCreateInfo	=
			{
				VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,	// VkStructureType						sType
				DE_NULL,												// const void*							pNext
				0u,														// VkDescriptorSetLayoutCreateFlags		flags
				(deUint32)layoutBindings.size(),						// deUint32								bindingCount
				layoutBindings.data()									// const VkDescriptorSetLayoutBinding*	pBindings
			};

			descriptorSetLayouts.push_back(VkDescriptorSetLayoutSp(new Unique<VkDescriptorSetLayout>(createDescriptorSetLayout(vk, device, &descriptorSetLayoutCreateInfo, DE_NULL))));
		}
	}

	// Create descriptor sets
	{
		for (size_t descriptorSetIdx = 0; descriptorSetIdx < m_descriptorSets.size(); descriptorSetIdx++)
		{
			const VkDescriptorSetAllocateInfo	descriptorSetAllocateInfo	=
			{
				VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,		// VkStructureType				sType
				DE_NULL,											// const void*					pNext
				*descriptorPool,									// VkDescriptorPool				descriptorPool
				1u,													// deUint32						descriptorSetCount
				&(descriptorSetLayouts[descriptorSetIdx]->get())	// const VkDescriptorSetLayout*	pSetLayouts
			};

			VkDescriptorSet descriptorSet = 0;
			VK_CHECK(vk.allocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet));
			descriptorSets.push_back(descriptorSet);
		}
	}

	// Descriptor writes and updates
	{
		vector<VkWriteDescriptorSet>	descriptorWrites;
		vector<VkCopyDescriptorSet>		descriptorCopies;

		// Write descriptors that are marked as needing initialization
		for (size_t descriptorSetIdx = 0; descriptorSetIdx < m_descriptorSets.size(); descriptorSetIdx++)
		{
			const vector<DescriptorSp>& bindings = m_descriptorSets[descriptorSetIdx]->getBindings();

			for (size_t bindingIdx = 0; bindingIdx < bindings.size(); bindingIdx++)
			{
				VkWriteDescriptorSet descriptorWrite = bindings[bindingIdx]->getDescriptorWrite();

				descriptorWrite.dstSet		= descriptorSets[descriptorSetIdx];
				descriptorWrite.dstBinding	= (deUint32)bindingIdx;

				if (descriptorWrite.descriptorCount > 0)
					descriptorWrites.push_back(descriptorWrite);
			}
		}

		for (size_t copyIdx = 0; copyIdx < m_descriptorCopies.size(); copyIdx++)
		{
			const DescriptorCopy		indices	= m_descriptorCopies[copyIdx];
			const VkCopyDescriptorSet	copy	=
			{
				VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET,	// VkStructureType	sType
				DE_NULL,								// const void*		pNext
				descriptorSets[indices.srcSet],			// VkDescriptorSet	srcSet
				indices.srcBinding,						// deUint32			srcBinding
				indices.srcArrayElement,				// deUint32			srcArrayElement
				descriptorSets[indices.dstSet],			// VkDescriptorSet	dstSet
				indices.dstBinding,						// deUint32			dstBinding
				indices.dstArrayElement,				// deUint32			dstArrayElement
				indices.descriptorCount					// deUint32			descriptorCount
			};

			descriptorCopies.push_back(copy);
		}

		// Update descriptors with writes and copies
		vk.updateDescriptorSets(device, (deUint32)descriptorWrites.size(), descriptorWrites.data(), (deUint32)descriptorCopies.size(), descriptorCopies.data());
	}

	// Create pipeline layout
	{
		vector<VkDescriptorSetLayout>		descriptorSetLayoutHandles;

		for (size_t i = 0; i < descriptorSetLayouts.size(); i++)
			descriptorSetLayoutHandles.push_back(descriptorSetLayouts[i]->get());

		const VkPipelineLayoutCreateInfo	pipelineLayoutCreateInfo	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// VkStructureType				sType
			DE_NULL,										// const void*					pNext
			0u,												// VkPipelineLayoutCreateFlags	flags
			(deUint32)descriptorSetLayoutHandles.size(),	// deUint32						setLayoutCount
			descriptorSetLayoutHandles.data(),				// const VkDescriptorSetLayout*	pSetLayouts
			0u,												// deUint32						pushConstantRangeCount
			DE_NULL											// const VkPushConstantRange*	pPushConstantRanges
		};

		pipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);
	}

	if (m_pipelineType == PIPELINE_TYPE_COMPUTE)
	{
		// Create compute pipeline
		{
			const Unique<VkShaderModule>			shaderModule	(createShaderModule(vk, device, context.getBinaryCollection().get("compute"), 0u));
			const VkPipelineShaderStageCreateInfo	shaderStageInfo	=
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType					sType
				DE_NULL,												// const void*						pNext
				(VkPipelineShaderStageCreateFlags)0,					// VkPipelineShaderStageCreateFlags	flags
				VK_SHADER_STAGE_COMPUTE_BIT,							// VkShaderStageFlagBits			stage
				*shaderModule,											// VkShaderModule					module
				"main",													// const char*						pName
				DE_NULL													// const VkSpecializationInfo*		pSpecializationInfo
			};

			const VkComputePipelineCreateInfo		pipelineInfo	=
			{
				VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,	// VkStructureType					sType
				DE_NULL,										// const void*						pNext
				(VkPipelineCreateFlags)0,						// VkPipelineCreateFlags			flags
				shaderStageInfo,								// VkPipelineShaderStageCreateInfo	stage
				*pipelineLayout,								// VkPipelineLayout					layout
				DE_NULL,										// VkPipeline						basePipelineHandle
				0												// deInt32							basePipelineIndex
			};

			pipeline = createComputePipeline(vk, device, DE_NULL, &pipelineInfo);
		}
	}
	else
	{
		// Create result image
		{
			const VkImageCreateInfo imageCreateInfo =
			{
				VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,						// VkStructureType			stype
				DE_NULL,													// const void*				pNext
				0u,															// VkImageCreateFlags		flags
				VK_IMAGE_TYPE_2D,											// VkImageType				imageType
				resultFormat,												// VkFormat					format
				{ (deUint32)renderSize.x(), (deUint32)renderSize.y(), 1 },	// VkExtent3D				extent
				1u,															// deUint32					mipLevels
				1u,															// deUint32					arrayLayers
				VK_SAMPLE_COUNT_1_BIT,										// VkSampleCountFlagBits	samples
				VK_IMAGE_TILING_OPTIMAL,									// VkImageTiling			tiling
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
				| VK_IMAGE_USAGE_TRANSFER_SRC_BIT,							// VkImageUsageFlags		usage
				VK_SHARING_MODE_EXCLUSIVE,									// VkSharingMode			sharingMode
				1u,															// deUint32					queueFamilyIndexCount
				&queueFamilyIndex,											// const deUint32*			pQueueFamilyIndices
				VK_IMAGE_LAYOUT_UNDEFINED,									// VkImageLayout			initialLayout
			};

			resultImage = de::MovePtr<ImageWithMemory>(new ImageWithMemory(vk, device, allocator, imageCreateInfo, MemoryRequirement::Any));
		}

		// Create result image view
		{
			const VkComponentMapping		componentMapping	= makeComponentMappingRGBA();

			const VkImageSubresourceRange	subresourceRange	=
			{
				VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask
				0u,							// deUint32				baseMipLevel
				1u,							// deUint32				levelCount
				0u,							// deUint32				baseArrayLayer
				1u,							// deUint32				layerCount
			};

			const VkImageViewCreateInfo		imageViewCreateInfo	=
			{
				VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// VkStructureType			sType
				DE_NULL,									// const void*				pNext
				0u,											// VkImageViewCreateFlags	flags
				**resultImage,								// VkImage					image
				VK_IMAGE_VIEW_TYPE_2D,						// VkImageViewType			viewType
				resultFormat,								// VkFormat					format
				componentMapping,							// VkComponentMapping		components
				subresourceRange							// VkImageSubresourceRange	subresourceRange
			};

			resultImageView = createImageView(vk, device, &imageViewCreateInfo);
		}

		// Create result buffer
		{
			const VkDeviceSize			bufferSize			= renderSize.x() * renderSize.y() * tcu::getPixelSize(mapVkFormat(resultFormat));
			const VkBufferCreateInfo	bufferCreateInfo	=
			{
				VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType		sType
				DE_NULL,								// const void*			pNext
				0u,										// VkBufferCreateFlags	flags
				bufferSize,								// VkDeviceSize			size
				VK_BUFFER_USAGE_TRANSFER_DST_BIT,		// VkBufferUsageFlags	usage
				VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode
				0u,										// uint32_t				queueFamilyIndexCount
				DE_NULL									// const uint32_t*		pQueueFamilyIndices
			};

			resultImageBuffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(vk, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible));
		}

		// Create render pass
		{
			const VkAttachmentReference		colorAttachmentRef		=
			{
				0u,											// deUint32			attachment
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout	layout
			};

			for (vector<DescriptorSp>::const_iterator desc = m_descriptors.begin(); desc != m_descriptors.end(); desc++)
			{
				vector<VkAttachmentReference> references = (*desc)->getAttachmentReferences();
				inputAttachments.insert(inputAttachments.end(), references.begin(), references.end());
			}

			const VkAttachmentDescription	colorAttachmentDesc		=
			{
				0u,											// VkAttachmentDescriptionFlags	flags
				VK_FORMAT_R8G8B8A8_UNORM,					// VkFormat						format
				VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits		samples
				VK_ATTACHMENT_LOAD_OP_CLEAR,				// VkAttachmentLoadOp			loadOp
				VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp			storeOp
				VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// VkAttachmentLoadOp			stencilLoadOp
				VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp			stencilStoreOp
				VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout				initialLayout
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout				finalLayout
			};

			attachmentDescriptions.push_back(colorAttachmentDesc);

			const VkAttachmentDescription	inputAttachmentDesc		=
			{
				0u,											// VkAttachmentDescriptionFlags	flags
				VK_FORMAT_R32_SFLOAT,						// VkFormat						format
				VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits		samples
				VK_ATTACHMENT_LOAD_OP_LOAD,					// VkAttachmentLoadOp			loadOp
				VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp			storeOp
				VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// VkAttachmentLoadOp			stencilLoadOp
				VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp			stencilStoreOp
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,	// VkImageLayout				initialLayout
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL	// VkImageLayout				finalLayout
			};

			for (size_t inputAttachmentIdx = 0; inputAttachmentIdx < inputAttachments.size(); inputAttachmentIdx++)
				attachmentDescriptions.push_back(inputAttachmentDesc);

			const VkSubpassDescription		subpassDescription		=
			{
				0u,																// VkSubpassDescriptionFlags	flags
				VK_PIPELINE_BIND_POINT_GRAPHICS,								// VkPipelineBindPoint			pipelineBindPoint
				(deUint32)inputAttachments.size(),								// deUint32						inputAttachmentCount
				inputAttachments.empty() ? DE_NULL : inputAttachments.data(),	// const VkAttachmentReference*	pInputAttachments
				1u,																// deUint32						colorAttachmentCount
				&colorAttachmentRef,											// const VkAttachmentReference*	pColorAttachments
				DE_NULL,														// const VkAttachmentReference*	pResolveAttachments
				DE_NULL,														// const VkAttachmentReference*	pDepthStencilAttachment
				0u,																// deUint32						preserveAttachmentCount
				DE_NULL															// const deUint32*				pPreserveAttachments
			};

			const VkRenderPassCreateInfo	renderPassCreateInfo	=
			{
				VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,	// VkStructureType					sType
				DE_NULL,									// const void*						pNext
				0u,											// VkRenderPassCreateFlags			flags
				(deUint32)attachmentDescriptions.size(),	// deUint32							attachmentCount
				attachmentDescriptions.data(),				// const VkAttachmentDescription*	pAttachments
				1u,											// deUint32							subpassCount
				&subpassDescription,						// const VkSubpassDescription*		pSubpasses
				0u,											// deUint32							dependencyCount
				DE_NULL										// const VkSubpassDependency*		pDependencies
			};

			renderPass = createRenderPass(vk, device, &renderPassCreateInfo);
		}

		// Create framebuffer
		{
			imageViews.push_back(*resultImageView);

			// Add input attachment image views
			for (vector<DescriptorSp>::const_iterator desc = m_descriptors.begin(); desc != m_descriptors.end(); desc++)
			{
				vector<VkImageViewSp> inputAttachmentViews = (*desc)->getImageViews();

				for (size_t imageViewIdx = 0; imageViewIdx < inputAttachmentViews.size(); imageViewIdx++)
					imageViews.push_back(**inputAttachmentViews[imageViewIdx]);
			}

			const VkFramebufferCreateInfo	framebufferCreateInfo	=
			{
				VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	// VkStructureType             sType
				DE_NULL,									// const void*                 pNext
				0u,											// VkFramebufferCreateFlags    flags
				*renderPass,								// VkRenderPass                renderPass
				(deUint32)imageViews.size(),				// deUint32                    attachmentCount
				imageViews.data(),							// const VkImageView*          pAttachments
				(deUint32)renderSize.x(),					// deUint32                    width
				(deUint32)renderSize.y(),					// deUint32                    height
				1u,											// deUint32                    layers
			};

			framebuffer = createFramebuffer(vk, device, &framebufferCreateInfo);
		}

		// Create graphics pipeline
		{
			const Unique<VkShaderModule>					vertexShaderModule		(createShaderModule(vk, device, context.getBinaryCollection().get("vertex"), 0u));
			const Unique<VkShaderModule>					fragmentShaderModule	(createShaderModule(vk, device, context.getBinaryCollection().get("fragment"), 0u));

			const VkPipelineVertexInputStateCreateInfo		vertexInputStateParams	=
			{
				VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType							sType
				DE_NULL,													// const void*								pNext
				(VkPipelineVertexInputStateCreateFlags)0,					// VkPipelineVertexInputStateCreateFlags	flags
				0u,															// deUint32									bindingCount
				DE_NULL,													// const VkVertexInputBindingDescription*	pVertexBindingDescriptions
				0u,															// deUint32									attributeCount
				DE_NULL,													// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions
			};

			const std::vector<VkViewport>					viewports				(1, makeViewport(renderSize));
			const std::vector<VkRect2D>						scissors				(1, makeRect2D(renderSize));

			pipeline = makeGraphicsPipeline(vk,										// const DeviceInterface&                        vk
											device,									// const VkDevice                                device
											*pipelineLayout,						// const VkPipelineLayout                        pipelineLayout
											*vertexShaderModule,					// const VkShaderModule                          vertexShaderModule
											DE_NULL,								// const VkShaderModule                          tessellationControlShaderModule
											DE_NULL,								// const VkShaderModule                          tessellationEvalShaderModule
											DE_NULL,								// const VkShaderModule                          geometryShaderModule
											*fragmentShaderModule,					// const VkShaderModule                          fragmentShaderModule
											*renderPass,							// const VkRenderPass                            renderPass
											viewports,								// const std::vector<VkViewport>&                viewports
											scissors,								// const std::vector<VkRect2D>&                  scissors
											VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,	// const VkPrimitiveTopology                     topology
											0u,										// const deUint32                                subpass
											0u,										// const deUint32                                patchControlPoints
											&vertexInputStateParams);				// const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
		}
	}

	// Run verification shader
	{
		const VkPipelineBindPoint	pipelineBindPoint	= m_pipelineType == PIPELINE_TYPE_COMPUTE ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS;
		vector<deUint32>			offsets;

		if (hasDynamicAreas())
		{
			for (size_t areaIdx = 0; areaIdx < m_dynamicAreas.size(); areaIdx++)
				offsets.push_back(m_dynamicAreas[areaIdx] * 256u);
		}

		beginCommandBuffer(vk, *commandBuffer);

		if (m_pipelineType == PIPELINE_TYPE_GRAPHICS)
		{
			const VkRect2D  renderArea  = makeRect2D(renderSize);
			const tcu::Vec4 clearColor  (1.0f, 0.0f, 0.0f, 1.0f);

			beginRenderPass(vk, *commandBuffer, *renderPass, *framebuffer, renderArea, clearColor);
		}

		vk.cmdBindPipeline(*commandBuffer, pipelineBindPoint, *pipeline);
		vk.cmdBindDescriptorSets(*commandBuffer, pipelineBindPoint, *pipelineLayout, 0u, (deUint32)descriptorSets.size(), descriptorSets.data(), (deUint32)offsets.size(), offsets.empty() ? DE_NULL : offsets.data());

		if (m_pipelineType == PIPELINE_TYPE_COMPUTE)
		{
			vk.cmdDispatch(*commandBuffer, 1u, 1u, 1u);
		}
		else
		{
			vk.cmdDraw(*commandBuffer, 6u, 1u, 0u, 0u);
			endRenderPass(vk, *commandBuffer);
			copyImageToBuffer(vk, *commandBuffer, **resultImage, resultImageBuffer->get(), renderSize);
		}

		endCommandBuffer(vk, *commandBuffer);
		submitCommandsAndWait(vk, device, queue, *commandBuffer);
	}

	if (m_pipelineType == PIPELINE_TYPE_COMPUTE)
	{
		// Invalidate result buffer
		m_resultBuffer->invalidate(context);

		// Verify result data
		const auto data = m_resultBuffer->getData();
		if (data[0] == 1)
			return tcu::TestStatus::pass("Pass");
		else
			return tcu::TestStatus::fail("Data validation failed");
	}
	else
	{
		invalidateAlloc(vk, device, resultImageBuffer->getAllocation());

		// Verify result image
		tcu::ConstPixelBufferAccess	resultBufferAccess(mapVkFormat(resultFormat), renderSize.x(), renderSize.y(), 1, resultImageBuffer->getAllocation().getHostPtr());

		for (deInt32 y = 0; y < renderSize.y(); y++)
			for (deInt32 x = 0; x < renderSize.x(); x++)
			{
				Vec4 pixel = resultBufferAccess.getPixel(x, y, 0);

				if (pixel.x() != 0.0f || pixel.y() != 1.0f || pixel.z() != 0.0f || pixel.w() != 1.0f)
				{
					// Log result image before failing.
					log << tcu::TestLog::ImageSet("Result", "") << tcu::TestLog::Image("Rendered", "Rendered image", resultBufferAccess) << tcu::TestLog::EndImageSet;
					return tcu::TestStatus::fail("Result image validation failed");
				}
			}

		return tcu::TestStatus::pass("Pass");
	}
}

DescriptorCopyTestInstance::DescriptorCopyTestInstance (Context&				context,
														DescriptorCommandsSp	commands)
: vkt::TestInstance	(context)
, m_commands		(commands)
{
}

DescriptorCopyTestInstance::~DescriptorCopyTestInstance (void)
{
}

DescriptorCopyTestCase::DescriptorCopyTestCase (tcu::TestContext&		context,
												const char*				name,
												const char*				desc,
												DescriptorCommandsSp	commands)
: vkt::TestCase	(context, name, desc)
, m_commands	(commands)
{
}

DescriptorCopyTestCase::~DescriptorCopyTestCase (void)
{
}

void DescriptorCopyTestCase::initPrograms (SourceCollections& programCollection) const
{
	if (m_commands->getPipelineType() == PIPELINE_TYPE_COMPUTE)
	{
		string computeSrc =
			"#version 430\n"
			"\n"
			+ m_commands->getShaderDeclarations() +
			"\n"
			"void main()\n"
			"{\n"
			"int result = 1;\n"
			+ m_commands->getDescriptorVerifications() +
			"storageBuffer" + de::toString(m_commands->getResultBufferId()) + ".data = result;\n"
			"}\n";

		programCollection.glslSources.add("compute") << glu::ComputeSource(computeSrc);
	}
	else
	{
		// Produce quad vertices using vertex index
		string vertexSrc =
			"#version 450\n"
			"out gl_PerVertex\n"
			"{\n"
			"    vec4 gl_Position;\n"
			"};\n"
			"void main()\n"
			"{\n"
			"    gl_Position = vec4(((gl_VertexIndex + 2) / 3) % 2 == 0 ? -1.0 : 1.0,\n"
			"                       ((gl_VertexIndex + 1) / 3) % 2 == 0 ? -1.0 : 1.0, 0.0, 1.0);\n"
			"}\n";

		programCollection.glslSources.add("vertex") << glu::VertexSource(vertexSrc);

		string fragmentSrc =
			"#version 430\n"
			"\n"
			+ m_commands->getShaderDeclarations() +
			"layout (location = 0) out vec4 outColor;\n"
			"\n"
			"void main()\n"
			"{\n"
			"int result = 1;\n"
			+ m_commands->getDescriptorVerifications() +
			"if (result == 1) outColor = vec4(0, 1, 0, 1);\n"
			"else outColor = vec4(1, 0, 1, 0);\n"
			"}\n";

		programCollection.glslSources.add("fragment") << glu::FragmentSource(fragmentSrc);
	}
}

TestInstance* DescriptorCopyTestCase::createInstance (Context& context) const
{
	return new DescriptorCopyTestInstance(context, m_commands);
}

tcu::TestStatus DescriptorCopyTestInstance::iterate (void)
{
	return m_commands->run(m_context);
}

template<class T>
void addDescriptorCopyTests (tcu::TestContext&					testCtx,
							 de::MovePtr<tcu::TestCaseGroup>&	group,
							 string								name,
							 PipelineType						pipelineType)
{
	// Simple test copying inside the same set.
	{
		DescriptorCommandsSp commands (new DescriptorCommands(pipelineType));
		commands->addDescriptor(DescriptorSp(new T(1u, 0u, 1u, 3u)), 0u);
		commands->addDescriptor(DescriptorSp(new T(1u, 0u, 1u, 2u)), 0u);

		commands->copyDescriptor(0u, 0u,	// from
								 0u, 1u);	// to

		vector<deUint32> dynamicAreas;
		dynamicAreas.push_back(2u);
		dynamicAreas.push_back(1u);
		commands->setDynamicAreas(dynamicAreas);

		commands->addResultBuffer();

		group->addChild(new DescriptorCopyTestCase(testCtx, (name + "_0").c_str(), "", commands));
	}

	// Simple test copying between different sets.
	{
		DescriptorCommandsSp commands (new DescriptorCommands(pipelineType));
		commands->addDescriptor(DescriptorSp(new T(1u, 0u, 1u, 2u)), 0u);
		commands->addDescriptor(DescriptorSp(new T(1u, 0u, 1u, 4u)), 1u);

		commands->copyDescriptor(0u, 0u,	// from
								 1u, 0u);	// to

		vector<deUint32> dynamicAreas;
		dynamicAreas.push_back(0u);
		dynamicAreas.push_back(1u);
		commands->setDynamicAreas(dynamicAreas);

		commands->addResultBuffer();

		group->addChild(new DescriptorCopyTestCase(testCtx, (name + "_1").c_str(), "", commands));
	}

	// Simple test copying between different sets. Destination not updated.
	{
		DescriptorCommandsSp commands (new DescriptorCommands(pipelineType));
		commands->addDescriptor(DescriptorSp(new T(1u, 0u, 1u, 2u)), 0u);
		commands->addDescriptor(DescriptorSp(new T(1u, 0u, 0u, 1u)), 1u);

		commands->copyDescriptor(0u, 0u,	// from
								 1u, 0u);	// to

		vector<deUint32> dynamicAreas;
		dynamicAreas.push_back(1u);
		dynamicAreas.push_back(0u);
		commands->setDynamicAreas(dynamicAreas);

		commands->addResultBuffer();

		group->addChild(new DescriptorCopyTestCase(testCtx, (name + "_2").c_str(), "", commands));
	}

	// Five sets and several copies.
	{
		DescriptorCommandsSp commands (new DescriptorCommands(pipelineType));
		commands->addDescriptor(DescriptorSp(new T(1u, 0u, 1u, 3u)), 0u);
		commands->addDescriptor(DescriptorSp(new T(1u, 0u, 1u, 4u)), 0u);
		commands->addDescriptor(DescriptorSp(new T(1u, 0u, 1u, 2u)), 1u);
		commands->addDescriptor(DescriptorSp(new T(1u, 0u, 1u, 1u)), 1u);
		commands->addDescriptor(DescriptorSp(new T(1u, 0u, 1u, 2u)), 1u);
		commands->addDescriptor(DescriptorSp(new T(1u, 0u, 1u, 5u)), 4u);

		commands->copyDescriptor(4u, 0u,	// from
								 0u, 0u);	// to

		commands->copyDescriptor(0u, 1u,	// from
								 1u, 2u);	// to

		commands->copyDescriptor(0u, 1u,	// from
								 1u, 1u);	// to

		vector<deUint32> dynamicAreas;
		dynamicAreas.push_back(1u);
		dynamicAreas.push_back(0u);
		dynamicAreas.push_back(1u);
		dynamicAreas.push_back(0u);
		dynamicAreas.push_back(0u);
		dynamicAreas.push_back(4u);
		commands->setDynamicAreas(dynamicAreas);

		commands->addResultBuffer();

		group->addChild(new DescriptorCopyTestCase(testCtx, (name + "_3").c_str(), "", commands));
	}

	// Several identical copies
	{
		DescriptorCommandsSp commands (new DescriptorCommands(pipelineType));
		commands->addDescriptor(DescriptorSp(new T(1u, 0u, 1u, 2u)), 0u);
		commands->addDescriptor(DescriptorSp(new T(1u, 0u, 1u, 4u)), 1u);
		commands->addDescriptor(DescriptorSp(new T(1u, 0u, 1u, 2u)), 1u);

		for (deUint32 i = 0; i < 100; i++)
		{
			commands->copyDescriptor(0u, 0u,	// from
									 1u, 0u);	// to
		}

		commands->copyDescriptor(1u, 1u,	// from
								 0u, 0u);	// to

		for (deUint32 i = 0; i < 100; i++)
		{
			commands->copyDescriptor(1u, 0u,	// from
									 1u, 1u);	// to
		}

		vector<deUint32> dynamicAreas;
		dynamicAreas.push_back(0u);
		dynamicAreas.push_back(1u);
		dynamicAreas.push_back(1u);
		commands->setDynamicAreas(dynamicAreas);

		commands->addResultBuffer();

		group->addChild(new DescriptorCopyTestCase(testCtx, (name + "_4").c_str(), "", commands));
	}

	// Copy descriptors back and forth
	{
		DescriptorCommandsSp commands (new DescriptorCommands(pipelineType));
		commands->addDescriptor(DescriptorSp(new T(1u, 0u, 1u, 3u)), 0u);
		commands->addDescriptor(DescriptorSp(new T(1u, 0u, 1u, 3u)), 1u);
		commands->addDescriptor(DescriptorSp(new T(1u, 0u, 1u, 3u)), 1u);

		commands->copyDescriptor(0u, 0u,	// from
								 1u, 0u);	// to

		commands->copyDescriptor(1u, 0u,	// from
								 0u, 0u);	// to

		commands->copyDescriptor(1u, 1u,	// from
								 0u, 0u);	// to

		commands->copyDescriptor(1u, 1u,	// from
								 0u, 0u);	// to

		commands->copyDescriptor(1u, 0u,	// from
								 1u, 1u);	// to

		vector<deUint32> dynamicAreas;
		dynamicAreas.push_back(1u);
		dynamicAreas.push_back(0u);
		dynamicAreas.push_back(0u);
		commands->setDynamicAreas(dynamicAreas);

		commands->addResultBuffer();

		group->addChild(new DescriptorCopyTestCase(testCtx, (name + "_5").c_str(), "", commands));
	}

	// Copy between non-consecutive descriptor sets
	{
		DescriptorCommandsSp commands (new DescriptorCommands(pipelineType));
		commands->addDescriptor(DescriptorSp(new T(1u, 0u, 1u, 3u)), 0u);
		commands->addDescriptor(DescriptorSp(new T(1u, 0u, 1u, 2u)), 5u);
		commands->addDescriptor(DescriptorSp(new T(1u, 0u, 1u, 2u)), 5u);

		commands->copyDescriptor(0u, 0u,	// from
								 5u, 1u);	// to

		vector<deUint32> dynamicAreas;
		dynamicAreas.push_back(2u);
		dynamicAreas.push_back(1u);
		dynamicAreas.push_back(1u);
		commands->setDynamicAreas(dynamicAreas);

		commands->addResultBuffer();

		group->addChild(new DescriptorCopyTestCase(testCtx, (name + "_6").c_str(), "", commands));
	}

	// Simple 3 sized array to 3 sized array inside the same set.
	{
		DescriptorCommandsSp commands (new DescriptorCommands(pipelineType));
		commands->addDescriptor(DescriptorSp(new T(3u, 0u, 3u, 3u)), 0u);
		commands->addDescriptor(DescriptorSp(new T(3u, 0u, 3u, 4u)), 0u);

		commands->copyDescriptor(0u, 0u, 0u,	// from
								 0u, 1u, 0u,	// to
								 3u);			// num descriptors


		vector<deUint32> dynamicAreas;
		dynamicAreas.push_back(1u);
		dynamicAreas.push_back(0u);
		dynamicAreas.push_back(2u);

		dynamicAreas.push_back(2u);
		dynamicAreas.push_back(1u);
		dynamicAreas.push_back(0u);
		commands->setDynamicAreas(dynamicAreas);

		commands->addResultBuffer();

		group->addChild(new DescriptorCopyTestCase(testCtx, (name + "_array0").c_str(), "", commands));
	}

	// Simple 2 sized array to 3 sized array into different set.
	{
		DescriptorCommandsSp commands (new DescriptorCommands(pipelineType));
		commands->addDescriptor(DescriptorSp(new T(2u, 0u, 2u, 2u)), 0u);
		commands->addDescriptor(DescriptorSp(new T(3u, 0u, 3u, 5u)), 1u);

		commands->copyDescriptor(0u, 0u, 0u,	// from
								 1u, 0u, 0u,	// to
								 2u);			// num descriptors

		vector<deUint32> dynamicAreas;
		dynamicAreas.push_back(1u);
		dynamicAreas.push_back(0u);

		dynamicAreas.push_back(1u);
		dynamicAreas.push_back(0u);
		dynamicAreas.push_back(1u);
		commands->setDynamicAreas(dynamicAreas);

		commands->addResultBuffer();

		group->addChild(new DescriptorCopyTestCase(testCtx, (name + "_array1").c_str(), "", commands));
	}

	// Update array partially with writes and partially with a copy
	{
		DescriptorCommandsSp commands (new DescriptorCommands(pipelineType));
		commands->addDescriptor(DescriptorSp(new T(4u, 0u, 4u, 3u)), 0u);
		commands->addDescriptor(DescriptorSp(new T(8u, 0u, 5u, 4u)), 0u);

		commands->copyDescriptor(0u, 0u, 1u,	// from
								 0u, 1u, 5u,	// to
								 3u);			// num descriptors

		vector<deUint32> dynamicAreas;
		dynamicAreas.push_back(2u);
		dynamicAreas.push_back(0u);
		dynamicAreas.push_back(1u);
		dynamicAreas.push_back(1u);

		dynamicAreas.push_back(2u);
		dynamicAreas.push_back(0u);
		dynamicAreas.push_back(1u);
		dynamicAreas.push_back(2u);
		dynamicAreas.push_back(0u);
		dynamicAreas.push_back(1u);
		dynamicAreas.push_back(1u);
		dynamicAreas.push_back(2u);
		commands->setDynamicAreas(dynamicAreas);

		commands->addResultBuffer();

		group->addChild(new DescriptorCopyTestCase(testCtx, (name + "_array2").c_str(), "", commands));
	}
}

void addSamplerCopyTests (tcu::TestContext&					testCtx,
						  de::MovePtr<tcu::TestCaseGroup>&	group,
						  PipelineType						pipelineType)
{
	// Simple copy between two samplers in the same set
	{
		DescriptorCommandsSp	commands	(new DescriptorCommands(pipelineType));
		SamplerDescriptor*		sampler0	(new SamplerDescriptor());
		SamplerDescriptor*		sampler1	(new SamplerDescriptor());
		SampledImageDescriptor*	image		(new SampledImageDescriptor());
		sampler0->addImage(image);
		sampler1->addImage(image);

		commands->addDescriptor(DescriptorSp(sampler0), 0u);
		commands->addDescriptor(DescriptorSp(sampler1), 0u);
		commands->addDescriptor(DescriptorSp(image), 0u);

		commands->copyDescriptor(0u, 0u,	// from
								 0u, 1u);	// to

		commands->addResultBuffer();

		group->addChild(new DescriptorCopyTestCase(testCtx, "sampler_0", "", commands));
	}

	// Simple 3 sized array to 3 sized array inside the same set.
	{
		DescriptorCommandsSp	commands	(new DescriptorCommands(pipelineType));
		SamplerDescriptor*		sampler0	(new SamplerDescriptor(3u, 0u, 3u));
		// One sampler in between to get the border colors to originally mismatch between sampler0 and sampler1.
		SamplerDescriptor*		sampler1	(new SamplerDescriptor());
		SamplerDescriptor*		sampler2	(new SamplerDescriptor(3u, 0u, 3u));
		SampledImageDescriptor*	image		(new SampledImageDescriptor());

		sampler0->addImage(image, 3u);
		sampler2->addImage(image, 3u);

		commands->addDescriptor(DescriptorSp(sampler0), 0u);
		commands->addDescriptor(DescriptorSp(sampler1), 0u);
		commands->addDescriptor(DescriptorSp(sampler2), 0u);
		commands->addDescriptor(DescriptorSp(image), 0u);

		commands->copyDescriptor(0u, 0u, 0u,	// from
								 0u, 2u, 0u,	// to
								 3u);			// num descriptors

		commands->addResultBuffer();

		group->addChild(new DescriptorCopyTestCase(testCtx, "sampler_array0", "", commands));
	}

	// Simple 2 sized array to 3 sized array into different set.
	{
		DescriptorCommandsSp	commands	(new DescriptorCommands(pipelineType));
		SamplerDescriptor*		sampler0	(new SamplerDescriptor(2u, 0u, 2u));
		SamplerDescriptor*		sampler1	(new SamplerDescriptor(3u, 0u, 3u));
		SampledImageDescriptor*	image		(new SampledImageDescriptor());

		sampler0->addImage(image, 2u);
		sampler1->addImage(image, 3u);

		commands->addDescriptor(DescriptorSp(sampler0), 0u);
		commands->addDescriptor(DescriptorSp(sampler1), 1u);
		commands->addDescriptor(DescriptorSp(image), 0u);

		commands->copyDescriptor(0u, 0u, 0u,	// from
								 1u, 0u, 1u,	// to
								 2u);			// num descriptors

		commands->addResultBuffer();

		group->addChild(new DescriptorCopyTestCase(testCtx, "sampler_array1", "", commands));
	}
}

void addSampledImageCopyTests (tcu::TestContext&				testCtx,
							   de::MovePtr<tcu::TestCaseGroup>&	group,
							   PipelineType						pipelineType)
{
	// Simple copy between two images in the same set
	{
		DescriptorCommandsSp	commands	(new DescriptorCommands(pipelineType));
		SamplerDescriptor*		sampler		(new SamplerDescriptor());
		SampledImageDescriptor*	image0		(new SampledImageDescriptor());
		SampledImageDescriptor*	image1		(new SampledImageDescriptor());
		image0->addSampler(sampler);
		image1->addSampler(sampler);

		commands->addDescriptor(DescriptorSp(image0), 0u);
		commands->addDescriptor(DescriptorSp(image1), 0u);
		commands->addDescriptor(DescriptorSp(sampler), 0u);

		commands->copyDescriptor(0u, 0u,	// from
								 0u, 1u);	// to

		commands->addResultBuffer();

		group->addChild(new DescriptorCopyTestCase(testCtx, "sampled_image_0", "", commands));
	}

	// Simple 3 sized array to 3 sized array inside the same set.
	{
		DescriptorCommandsSp	commands	(new DescriptorCommands(pipelineType));
		SamplerDescriptor*		sampler		(new SamplerDescriptor());
		SampledImageDescriptor*	image0		(new SampledImageDescriptor(3u, 0u, 3u));
		SampledImageDescriptor*	image1		(new SampledImageDescriptor(3u, 0u, 3u));
		image0->addSampler(sampler, 3u);
		image1->addSampler(sampler, 3u);

		commands->addDescriptor(DescriptorSp(sampler), 0u);
		commands->addDescriptor(DescriptorSp(image0), 0u);
		commands->addDescriptor(DescriptorSp(image1), 0u);

		commands->copyDescriptor(0u, 1u, 0u,	// from
								 0u, 2u, 0u,	// to
								 3u);			// num descriptors

		commands->addResultBuffer();

		group->addChild(new DescriptorCopyTestCase(testCtx, "sampled_image_array0", "", commands));
	}
}

// Mixture of different descriptors in the same test
void addMixedDescriptorCopyTests (tcu::TestContext&					testCtx,
								  de::MovePtr<tcu::TestCaseGroup>&	group,
								  PipelineType						pipelineType)
{
	{
		DescriptorCommandsSp		commands		(new DescriptorCommands(pipelineType));
		SamplerDescriptor*			sampler0		(new SamplerDescriptor());
		SamplerDescriptor*			sampler1		(new SamplerDescriptor());
		SampledImageDescriptor*		image0			(new SampledImageDescriptor());
		SampledImageDescriptor*		image1			(new SampledImageDescriptor());
		StorageBufferDescriptor*	storageBuffer0	(new StorageBufferDescriptor());
		StorageBufferDescriptor*	storageBuffer1	(new StorageBufferDescriptor());
		StorageBufferDescriptor*	storageBuffer2	= new StorageBufferDescriptor();
		sampler0->addImage(image0);
		sampler1->addImage(image1);

		commands->addDescriptor(DescriptorSp(sampler0), 0u);		// Set 0, binding 0
		commands->addDescriptor(DescriptorSp(storageBuffer0), 0u);	// Set 0, binding 1
		commands->addDescriptor(DescriptorSp(image0), 0u);			// Set 0, binding 2
		commands->addDescriptor(DescriptorSp(storageBuffer1), 0u);	// Set 0, binding 3
		commands->addDescriptor(DescriptorSp(sampler1), 1u);		// Set 1, binding 0
		commands->addDescriptor(DescriptorSp(image1), 1u);			// Set 1, binding 1
		commands->addDescriptor(DescriptorSp(storageBuffer2), 1u);	// Set 1, binding 2

		// image1 to image0
		commands->copyDescriptor(1u, 1u,	// from
								 0u, 2u);	// to

		// storageBuffer0 to storageBuffer1
		commands->copyDescriptor(0u, 1u,	// from
								 0u, 3u);	// to

		// storageBuffer1 to storageBuffer2
		commands->copyDescriptor(0u, 3u,	// from
								 1u, 2u);	// to

		commands->addResultBuffer();

		group->addChild(new DescriptorCopyTestCase(testCtx, "mix_0", "", commands));
	}

	{
		DescriptorCommandsSp			commands				(new DescriptorCommands(pipelineType));
		StorageTexelBufferDescriptor*	storageTexelBuffer0		(new StorageTexelBufferDescriptor());
		StorageTexelBufferDescriptor*	storageTexelBuffer1		(new StorageTexelBufferDescriptor());
		UniformBufferDescriptor*		uniformBuffer0			(new UniformBufferDescriptor());
		UniformBufferDescriptor*		uniformBuffer1			(new UniformBufferDescriptor());
		UniformBufferDescriptor*		uniformBuffer2			(new UniformBufferDescriptor());
		DynamicStorageBufferDescriptor*	dynamicStorageBuffer0	(new DynamicStorageBufferDescriptor(1u, 0u, 1u, 3u));
		DynamicStorageBufferDescriptor*	dynamicStorageBuffer1	(new DynamicStorageBufferDescriptor(1u, 0u, 1u, 4u));

		commands->addDescriptor(DescriptorSp(storageTexelBuffer0), 0u);		// Set 0, binding 0
		commands->addDescriptor(DescriptorSp(uniformBuffer0), 0u);			// Set 0, binding 1
		commands->addDescriptor(DescriptorSp(dynamicStorageBuffer0), 0u);	// Set 0, binding 2
		commands->addDescriptor(DescriptorSp(uniformBuffer1), 0u);			// Set 0, binding 3
		commands->addDescriptor(DescriptorSp(dynamicStorageBuffer1), 1u);	// Set 1, binding 0
		commands->addDescriptor(DescriptorSp(storageTexelBuffer1), 1u);		// Set 1, binding 1
		commands->addDescriptor(DescriptorSp(uniformBuffer2), 1u);			// Set 1, binding 2

		vector<deUint32> dynamicAreas;
		dynamicAreas.push_back(2u);
		dynamicAreas.push_back(1u);
		commands->setDynamicAreas(dynamicAreas);

		// uniformBuffer0 to uniformBuffer2
		commands->copyDescriptor(0u, 1u,	// from
								 1u, 2u);	// to

		// uniformBuffer1 to uniformBuffer2
		commands->copyDescriptor(0u, 3u,	// from
								 1u, 2u);	// to

		// storageTexelBuffer1 to storageTexelBuffer0
		commands->copyDescriptor(1u, 1u,	// from
								 0u, 0u);	// to

		// dynamicStorageBuffer0 to dynamicStorageBuffer1
		commands->copyDescriptor(0u, 2u,	// from
								 1u, 0u);	// to

		commands->addResultBuffer();

		group->addChild(new DescriptorCopyTestCase(testCtx, "mix_1", "", commands));
	}

	if (pipelineType == PIPELINE_TYPE_GRAPHICS)
	{
		// Mixture of descriptors, including input attachment.
		DescriptorCommandsSp			commands				(new DescriptorCommands(pipelineType));
		InputAttachmentDescriptor*		inputAttachment0		(new InputAttachmentDescriptor());
		InputAttachmentDescriptor*		inputAttachment1		(new InputAttachmentDescriptor());
		CombinedImageSamplerDescriptor*	combinedImageSampler0	(new CombinedImageSamplerDescriptor());
		CombinedImageSamplerDescriptor*	combinedImageSampler1	(new CombinedImageSamplerDescriptor());
		UniformTexelBufferDescriptor*	uniformTexelBuffer0		(new UniformTexelBufferDescriptor(5u, 0u, 5u));
		UniformTexelBufferDescriptor*	uniformTexelBuffer1		(new UniformTexelBufferDescriptor(3u, 1u, 1u));

		commands->addDescriptor(DescriptorSp(combinedImageSampler0), 0u);	// Set 0, binding 0
		commands->addDescriptor(DescriptorSp(inputAttachment0), 0u);		// Set 0, binding 1
		commands->addDescriptor(DescriptorSp(uniformTexelBuffer0), 0u);		// Set 0, binding 2
		commands->addDescriptor(DescriptorSp(combinedImageSampler1), 1u);	// Set 1, binding 0
		commands->addDescriptor(DescriptorSp(inputAttachment1), 1u);		// Set 1, binding 1
		commands->addDescriptor(DescriptorSp(uniformTexelBuffer1), 1u);		// Set 1, binding 2

		// uniformTexelBuffer0[1..3] to uniformTexelBuffer1[0..2]
		commands->copyDescriptor(0u, 2u, 1u,	// from
								 1u, 2u, 0u,	// to
								 3u);			// num descriptors

		// inputAttachment0 to inputAttachment1
		commands->copyDescriptor(0u, 1u,	// from
								 1u, 1u);	// to

		// combinedImageSampler0 to combinedImageSampler1
		commands->copyDescriptor(0u, 0u,	// from
								 1u, 0u);	// to

		commands->addResultBuffer();

		group->addChild(new DescriptorCopyTestCase(testCtx, "mix_2", "", commands));
	}

	if (pipelineType == PIPELINE_TYPE_GRAPHICS)
	{
		// Similar to the previous one, but adding inline uniform blocks to the mix.
		DescriptorCommandsSp			commands				(new DescriptorCommands(pipelineType));
		InlineUniformBlockDescriptor*	iub0					(new InlineUniformBlockDescriptor(4u, 0u, 4u));
		InlineUniformBlockDescriptor*	iub1					(new InlineUniformBlockDescriptor(4u, 0u, 1u));
		InputAttachmentDescriptor*		inputAttachment0		(new InputAttachmentDescriptor());
		InputAttachmentDescriptor*		inputAttachment1		(new InputAttachmentDescriptor());
		CombinedImageSamplerDescriptor*	combinedImageSampler0	(new CombinedImageSamplerDescriptor());
		CombinedImageSamplerDescriptor*	combinedImageSampler1	(new CombinedImageSamplerDescriptor());
		UniformTexelBufferDescriptor*	uniformTexelBuffer0		(new UniformTexelBufferDescriptor(5u, 0u, 5u));
		UniformTexelBufferDescriptor*	uniformTexelBuffer1		(new UniformTexelBufferDescriptor(3u, 1u, 1u));

		commands->addDescriptor(DescriptorSp(iub0), 0u);					// Set 0, binding 0
		commands->addDescriptor(DescriptorSp(combinedImageSampler0), 0u);	// Set 0, binding 1
		commands->addDescriptor(DescriptorSp(inputAttachment0), 0u);		// Set 0, binding 2
		commands->addDescriptor(DescriptorSp(uniformTexelBuffer0), 0u);		// Set 0, binding 3
		commands->addDescriptor(DescriptorSp(iub1), 1u);					// Set 1, binding 0
		commands->addDescriptor(DescriptorSp(combinedImageSampler1), 1u);	// Set 1, binding 1
		commands->addDescriptor(DescriptorSp(inputAttachment1), 1u);		// Set 1, binding 2
		commands->addDescriptor(DescriptorSp(uniformTexelBuffer1), 1u);		// Set 1, binding 3

		// iub0.data[0..2] to iub1.data[1..3]
		commands->copyDescriptor(0u, 0u, 0u,	// from
								 1u, 0u, 1u,	// to
								 3u);			// num descriptors

		// uniformTexelBuffer0[1..3] to uniformTexelBuffer1[0..2]
		commands->copyDescriptor(0u, 3u, 1u,	// from
								 1u, 3u, 0u,	// to
								 3u);			// num descriptors

		// inputAttachment0 to inputAttachment1
		commands->copyDescriptor(0u, 2u,	// from
								 1u, 2u);	// to

		// combinedImageSampler0 to combinedImageSampler1
		commands->copyDescriptor(0u, 1u,	// from
								 1u, 1u);	// to

		commands->addResultBuffer();

		group->addChild(new DescriptorCopyTestCase(testCtx, "mix_3", "", commands));
	}

	// Mixture of descriptors using descriptor arrays
	{
		DescriptorCommandsSp			commands				(new DescriptorCommands(pipelineType));
		CombinedImageSamplerDescriptor*	combinedImageSampler0	(new CombinedImageSamplerDescriptor(3u, 0u, 3u));
		CombinedImageSamplerDescriptor*	combinedImageSampler1	(new CombinedImageSamplerDescriptor(4u, 0u, 2u));
		CombinedImageSamplerDescriptor*	combinedImageSampler2	(new CombinedImageSamplerDescriptor(3u, 0u, 3u));
		StorageImageDescriptor*			storageImage0			(new StorageImageDescriptor(5u, 0u, 5u));
		StorageImageDescriptor*			storageImage1			(new StorageImageDescriptor(3u, 0u, 0u));
		StorageBufferDescriptor*		storageBuffer0			(new StorageBufferDescriptor(2u, 0u, 1u));
		StorageBufferDescriptor*		storageBuffer1			(new StorageBufferDescriptor(3u, 0u, 3u));

		commands->addDescriptor(DescriptorSp(combinedImageSampler0), 0u);	// Set 0, binding 0
		commands->addDescriptor(DescriptorSp(storageImage0), 0u);			// Set 0, binding 1
		commands->addDescriptor(DescriptorSp(combinedImageSampler1), 0u);	// Set 0, binding 2
		commands->addDescriptor(DescriptorSp(storageBuffer0), 0u);			// Set 0, binding 3
		commands->addDescriptor(DescriptorSp(storageBuffer1), 0u);			// Set 0, binding 4
		commands->addDescriptor(DescriptorSp(storageImage1), 1u);			// Set 1, binding 0
		commands->addDescriptor(DescriptorSp(combinedImageSampler2), 1u);	// Set 1, binding 1

		// combinedImageSampler0[1..2] to combinedImageSampler1[2..3]
		commands->copyDescriptor(0u, 0u, 1u,	// from
								 0u, 2u, 2u,	// to
								 2u);			// num descriptors

		// storageImage0[2..4] to storageImage1[0..2]
		commands->copyDescriptor(0u, 1u, 2u,	// from
								 1u, 0u, 0u,	// to
								 3u);			// num descriptors

		// storageBuffer1[1..2] to storageBuffer0[0..1]
		commands->copyDescriptor(0u, 4u, 1u,	// from
								 0u, 3u, 0u,	// to
								 2u);			// num descriptors

		commands->addResultBuffer();

		group->addChild(new DescriptorCopyTestCase(testCtx, "mix_array0", "", commands));
	}

	// Similar to the previous one but including inline uniform blocks.
	{
		DescriptorCommandsSp			commands				(new DescriptorCommands(pipelineType));
		InlineUniformBlockDescriptor*	iub0					(new InlineUniformBlockDescriptor(4u, 0u, 1u));
		InlineUniformBlockDescriptor*	iub1					(new InlineUniformBlockDescriptor(4u, 0u, 4u));
		CombinedImageSamplerDescriptor*	combinedImageSampler0	(new CombinedImageSamplerDescriptor(3u, 0u, 3u));
		CombinedImageSamplerDescriptor*	combinedImageSampler1	(new CombinedImageSamplerDescriptor(4u, 0u, 2u));
		CombinedImageSamplerDescriptor*	combinedImageSampler2	(new CombinedImageSamplerDescriptor(3u, 0u, 3u));
		StorageImageDescriptor*			storageImage0			(new StorageImageDescriptor(5u, 0u, 5u));
		StorageImageDescriptor*			storageImage1			(new StorageImageDescriptor(3u, 0u, 0u));
		StorageBufferDescriptor*		storageBuffer0			(new StorageBufferDescriptor(2u, 0u, 1u));
		StorageBufferDescriptor*		storageBuffer1			(new StorageBufferDescriptor(3u, 0u, 3u));

		commands->addDescriptor(DescriptorSp(iub0), 0u);					// Set 0, binding 0
		commands->addDescriptor(DescriptorSp(combinedImageSampler0), 0u);	// Set 0, binding 1
		commands->addDescriptor(DescriptorSp(storageImage0), 0u);			// Set 0, binding 2
		commands->addDescriptor(DescriptorSp(combinedImageSampler1), 0u);	// Set 0, binding 3
		commands->addDescriptor(DescriptorSp(storageBuffer0), 0u);			// Set 0, binding 4
		commands->addDescriptor(DescriptorSp(storageBuffer1), 0u);			// Set 0, binding 5
		commands->addDescriptor(DescriptorSp(combinedImageSampler2), 0u);	// Set 0, binding 6
		commands->addDescriptor(DescriptorSp(iub1), 1u);					// Set 1, binding 0
		commands->addDescriptor(DescriptorSp(storageImage1), 1u);			// Set 1, binding 1

		// iub1.data[0..2] to iub0.data[1..3]
		commands->copyDescriptor(1u, 0u, 0u,	// from
								 0u, 0u, 1u,	// to
								 3u);			// num descriptors

		// combinedImageSampler0[1..2] to combinedImageSampler1[2..3]
		commands->copyDescriptor(0u, 1u, 1u,	// from
								 0u, 3u, 2u,	// to
								 2u);			// num descriptors

		// storageImage0[2..4] to storageImage1[0..2]
		commands->copyDescriptor(0u, 2u, 2u,	// from
								 1u, 1u, 0u,	// to
								 3u);			// num descriptors

		// storageBuffer1[1..2] to storageBuffer0[0..1]
		commands->copyDescriptor(0u, 5u, 1u,	// from
								 0u, 4u, 0u,	// to
								 2u);			// num descriptors

		commands->addResultBuffer();

		group->addChild(new DescriptorCopyTestCase(testCtx, "mix_array1", "", commands));
	}
}

} // anonymous

tcu::TestCaseGroup*	createDescriptorCopyTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> descriptorCopyGroup(new tcu::TestCaseGroup(testCtx, "descriptor_copy", "Descriptor copy tests"));

	de::MovePtr<tcu::TestCaseGroup> computeGroup(new tcu::TestCaseGroup(testCtx, "compute", "Compute tests"));
	de::MovePtr<tcu::TestCaseGroup> graphicsGroup(new tcu::TestCaseGroup(testCtx, "graphics", "Graphics tests"));

	// Compute tests
	addDescriptorCopyTests<UniformBufferDescriptor>(testCtx, computeGroup, "uniform_buffer", PIPELINE_TYPE_COMPUTE);
	addDescriptorCopyTests<InlineUniformBlockDescriptor>(testCtx, computeGroup, "inline_uniform_block", PIPELINE_TYPE_COMPUTE);
	addDescriptorCopyTests<StorageBufferDescriptor>(testCtx, computeGroup, "storage_buffer", PIPELINE_TYPE_COMPUTE);
	addDescriptorCopyTests<CombinedImageSamplerDescriptor>(testCtx, computeGroup, "combined_image_sampler", PIPELINE_TYPE_COMPUTE);
	addDescriptorCopyTests<StorageImageDescriptor>(testCtx, computeGroup, "storage_image", PIPELINE_TYPE_COMPUTE);
	addDescriptorCopyTests<UniformTexelBufferDescriptor>(testCtx, computeGroup, "uniform_texel_buffer", PIPELINE_TYPE_COMPUTE);
	addDescriptorCopyTests<StorageTexelBufferDescriptor>(testCtx, computeGroup, "storage_texel_buffer", PIPELINE_TYPE_COMPUTE);
	addDescriptorCopyTests<DynamicUniformBufferDescriptor>(testCtx, computeGroup, "uniform_buffer_dynamic", PIPELINE_TYPE_COMPUTE);
	addDescriptorCopyTests<DynamicStorageBufferDescriptor>(testCtx, computeGroup, "storage_buffer_dynamic", PIPELINE_TYPE_COMPUTE);
	addSamplerCopyTests(testCtx, computeGroup, PIPELINE_TYPE_COMPUTE);
	addSampledImageCopyTests(testCtx, computeGroup, PIPELINE_TYPE_COMPUTE);
	addMixedDescriptorCopyTests(testCtx, computeGroup, PIPELINE_TYPE_COMPUTE);

	// Graphics tests
	addDescriptorCopyTests<UniformBufferDescriptor>(testCtx, graphicsGroup, "uniform_buffer", PIPELINE_TYPE_GRAPHICS);
	addDescriptorCopyTests<InlineUniformBlockDescriptor>(testCtx, graphicsGroup, "inline_uniform_block", PIPELINE_TYPE_GRAPHICS);
	addDescriptorCopyTests<StorageBufferDescriptor>(testCtx, graphicsGroup, "storage_buffer", PIPELINE_TYPE_GRAPHICS);
	addDescriptorCopyTests<CombinedImageSamplerDescriptor>(testCtx, graphicsGroup, "combined_image_sampler", PIPELINE_TYPE_GRAPHICS);
	addDescriptorCopyTests<StorageImageDescriptor>(testCtx, graphicsGroup, "storage_image", PIPELINE_TYPE_GRAPHICS);
	addDescriptorCopyTests<InputAttachmentDescriptor>(testCtx, graphicsGroup, "input_attachment", PIPELINE_TYPE_GRAPHICS);
	addDescriptorCopyTests<UniformTexelBufferDescriptor>(testCtx, graphicsGroup, "uniform_texel_buffer", PIPELINE_TYPE_GRAPHICS);
	addDescriptorCopyTests<StorageTexelBufferDescriptor>(testCtx, graphicsGroup, "storage_texel_buffer", PIPELINE_TYPE_GRAPHICS);
	addDescriptorCopyTests<DynamicUniformBufferDescriptor>(testCtx, graphicsGroup, "uniform_buffer_dynamic", PIPELINE_TYPE_GRAPHICS);
	addDescriptorCopyTests<DynamicStorageBufferDescriptor>(testCtx, graphicsGroup, "storage_buffer_dynamic", PIPELINE_TYPE_GRAPHICS);
	addSamplerCopyTests(testCtx, graphicsGroup, PIPELINE_TYPE_GRAPHICS);
	addSampledImageCopyTests(testCtx, graphicsGroup, PIPELINE_TYPE_GRAPHICS);
	addMixedDescriptorCopyTests(testCtx, graphicsGroup, PIPELINE_TYPE_GRAPHICS);

	descriptorCopyGroup->addChild(computeGroup.release());
	descriptorCopyGroup->addChild(graphicsGroup.release());

	return descriptorCopyGroup.release();
}

}	// BindingModel
}	// vkt
