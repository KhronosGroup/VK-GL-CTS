#ifndef _VKTSPVASMUTILS_HPP
#define _VKTSPVASMUTILS_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 Google Inc.
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
 * \brief Utilities for Vulkan SPIR-V assembly tests
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "vkMemUtil.hpp"
#include "vkRef.hpp"
#include "vkTypeUtil.hpp"
#include "vktTestCase.hpp"

#include "deMemory.h"
#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"

#include <string>
#include <vector>

namespace vkt
{
namespace SpirVAssembly
{
/*--------------------------------------------------------------------*//*!
 * \brief Abstract class for an input/output storage buffer object
 *//*--------------------------------------------------------------------*/
class BufferInterface
{
public:
	virtual				~BufferInterface	(void)				{}

	virtual void		getBytes			(std::vector<deUint8>& bytes) const = 0;
	virtual size_t		getByteSize			(void) const = 0;
};

typedef de::SharedPtr<BufferInterface>	BufferSp;
typedef de::MovePtr<vk::Allocation>		AllocationMp;
typedef de::SharedPtr<vk::Allocation>	AllocationSp;

class Resource
{
public:
	Resource(const BufferSp& buffer_, vk::VkDescriptorType descriptorType_ = vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		: buffer(buffer_)
		, descriptorType(descriptorType_)
	{
	}

	virtual const BufferSp&			getBuffer			() const							{ return buffer; }
	virtual void					getBytes			(std::vector<deUint8>& bytes) const	{ buffer->getBytes(bytes); }
	virtual size_t					getByteSize			(void) const						{ return buffer->getByteSize(); }

	virtual void					setDescriptorType	(vk::VkDescriptorType type)		{ descriptorType = type; }
	virtual vk::VkDescriptorType	getDescriptorType	()	const						{ return descriptorType; }

private:
	BufferSp				buffer;
	vk::VkDescriptorType	descriptorType;
};

typedef bool (*VerifyIOFunc) (const std::vector<Resource>&		inputs,
							  const std::vector<AllocationSp>&	outputAllocations,
							  const std::vector<Resource>&		expectedOutputs,
							  tcu::TestLog&						log);

struct SpecConstants
{
public:
							SpecConstants (void)
							{}

	bool					empty (void) const
							{
								return valuesBuffer.empty();
							}

	size_t					getValuesCount (void) const
							{
								return sizesBuffer.size();
							}

	size_t					getValueSize (const size_t valueIndex) const
							{
								return sizesBuffer[valueIndex];
							}

	const void*				getValuesBuffer (void) const
							{
								if (valuesBuffer.size() == 0)
									return DE_NULL;
								else
									return static_cast<const void*>(&valuesBuffer[0]);
							}

	template<typename T>
	void					append (const T value)
							{
								append(&value, sizeof(value));
							}

	void					append (const void* buf, const size_t byteSize)
							{
								DE_ASSERT(byteSize > 0);

								valuesBuffer.resize(valuesBuffer.size() + byteSize);
								deMemcpy(&valuesBuffer[valuesBuffer.size() - byteSize], buf, byteSize);

								sizesBuffer.push_back(byteSize);
							}

private:
	std::vector<deUint8>	valuesBuffer;
	std::vector<size_t>		sizesBuffer;
};

enum Extension8BitStorageFeatureBits
{
	EXT8BITSTORAGEFEATURES_STORAGE_BUFFER			= (1u << 1),
	EXT8BITSTORAGEFEATURES_UNIFORM_STORAGE_BUFFER	= (1u << 2),
	EXT8BITSTORAGEFEATURES_PUSH_CONSTANT			= (1u << 3),
};
typedef deUint32 Extension8BitStorageFeatures;

enum Extension16BitStorageFeatureBits
{
	EXT16BITSTORAGEFEATURES_UNIFORM_BUFFER_BLOCK	= (1u << 1),
	EXT16BITSTORAGEFEATURES_UNIFORM					= (1u << 2),
	EXT16BITSTORAGEFEATURES_PUSH_CONSTANT			= (1u << 3),
	EXT16BITSTORAGEFEATURES_INPUT_OUTPUT			= (1u << 4),
};
typedef deUint32 Extension16BitStorageFeatures;

enum ExtensionVariablePointersFeaturesBits
{
	EXTVARIABLEPOINTERSFEATURES_VARIABLE_POINTERS_STORAGEBUFFER	= (1u << 1),
	EXTVARIABLEPOINTERSFEATURES_VARIABLE_POINTERS				= (1u << 2),
};
typedef deUint32 ExtensionVariablePointersFeatures;

struct VulkanFeatures
{
	vk::VkPhysicalDeviceFeatures		coreFeatures;
	Extension16BitStorageFeatures		ext16BitStorage;
	ExtensionVariablePointersFeatures	extVariablePointers;
	Extension8BitStorageFeatures		ext8BitStorage;

	VulkanFeatures				(void)
		: coreFeatures			(vk::VkPhysicalDeviceFeatures())
		, ext16BitStorage		(0)
		, extVariablePointers	(0)
		, ext8BitStorage		(0)
	{
		deMemset(&coreFeatures, 0, sizeof(coreFeatures));
	}
};

// Returns true if the given 8bit storage extension features in `toCheck` are all supported.
bool is8BitStorageFeaturesSupported (const Context&						context,
									  Extension8BitStorageFeatures		toCheck);

// Returns true if the given 16bit storage extension features in `toCheck` are all supported.
bool isCoreFeaturesSupported (const Context&						context,
							  const vk::VkPhysicalDeviceFeatures&	toCheck,
							  const char**							missingFeature);

// Returns true if the given 16bit storage extension features in `toCheck` are all supported.
bool is16BitStorageFeaturesSupported (const Context&				context,
									  Extension16BitStorageFeatures	toCheck);

// Returns true if the given variable pointers extension features in `toCheck` are all supported.
bool isVariablePointersFeaturesSupported (const Context&					context,
										  ExtensionVariablePointersFeatures	toCheck);

deUint32 getMinRequiredVulkanVersion (const vk::SpirvVersion version);

std::string	getVulkanName (const deUint32 version);

} // SpirVAssembly
} // vkt

#endif // _VKTSPVASMUTILS_HPP
