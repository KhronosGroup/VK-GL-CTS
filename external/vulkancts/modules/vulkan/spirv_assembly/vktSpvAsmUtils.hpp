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
#include "deRandom.hpp"
#include "deFloat16.h"

#include <string>
#include <vector>

namespace vkt
{
namespace SpirVAssembly
{

#define SPIRV_ASSEMBLY_TYPES																				\
	"%void = OpTypeVoid\n"																					\
	"%bool = OpTypeBool\n"																					\
																											\
	"%i32 = OpTypeInt 32 1\n"																				\
	"%u32 = OpTypeInt 32 0\n"																				\
																											\
	"%f32 = OpTypeFloat 32\n"																				\
	"%v2i32 = OpTypeVector %i32 2\n"																		\
	"%v2u32 = OpTypeVector %u32 2\n"																		\
	"%v2f32 = OpTypeVector %f32 2\n"																		\
	"%v3i32 = OpTypeVector %i32 3\n"																		\
	"%v3u32 = OpTypeVector %u32 3\n"																		\
	"%v3f32 = OpTypeVector %f32 3\n"																		\
	"%v4i32 = OpTypeVector %i32 4\n"																		\
	"%v4u32 = OpTypeVector %u32 4\n"																		\
	"%v4f32 = OpTypeVector %f32 4\n"																		\
	"%v4bool = OpTypeVector %bool 4\n"																		\
																											\
	"%v4f32_v4f32_function = OpTypeFunction %v4f32 %v4f32\n"									\
	"%bool_function = OpTypeFunction %bool\n"																\
	"%voidf = OpTypeFunction %void\n"																			\
																											\
	"%ip_f32 = OpTypePointer Input %f32\n"																	\
	"%ip_i32 = OpTypePointer Input %i32\n"																	\
	"%ip_u32 = OpTypePointer Input %u32\n"																	\
	"%ip_v2f32 = OpTypePointer Input %v2f32\n"																\
	"%ip_v2i32 = OpTypePointer Input %v2i32\n"																\
	"%ip_v2u32 = OpTypePointer Input %v2u32\n"																\
	"%ip_v3f32 = OpTypePointer Input %v3f32\n"																\
	"%ip_v4f32 = OpTypePointer Input %v4f32\n"																\
	"%ip_v4i32 = OpTypePointer Input %v4i32\n"																\
	"%ip_v4u32 = OpTypePointer Input %v4u32\n"																\
																											\
	"%op_f32 = OpTypePointer Output %f32\n"																	\
	"%op_i32 = OpTypePointer Output %i32\n"																	\
	"%op_u32 = OpTypePointer Output %u32\n"																	\
	"%op_v2f32 = OpTypePointer Output %v2f32\n"																\
	"%op_v2i32 = OpTypePointer Output %v2i32\n"																\
	"%op_v2u32 = OpTypePointer Output %v2u32\n"																\
	"%op_v4f32 = OpTypePointer Output %v4f32\n"																\
	"%op_v4i32 = OpTypePointer Output %v4i32\n"																\
	"%op_v4u32 = OpTypePointer Output %v4u32\n"																\
																											\
	"%fp_f32   = OpTypePointer Function %f32\n"																\
	"%fp_i32   = OpTypePointer Function %i32\n"																\
	"%fp_v4f32 = OpTypePointer Function %v4f32\n"															\

#define SPIRV_ASSEMBLY_CONSTANTS																			\
	"%c_f32_1 = OpConstant %f32 1.0\n"																		\
	"%c_f32_0 = OpConstant %f32 0.0\n"																		\
	"%c_f32_0_5 = OpConstant %f32 0.5\n"																	\
	"%c_f32_n1  = OpConstant %f32 -1.\n"																	\
	"%c_f32_7 = OpConstant %f32 7.0\n"																		\
	"%c_f32_8 = OpConstant %f32 8.0\n"																		\
	"%c_i32_0 = OpConstant %i32 0\n"																		\
	"%c_i32_1 = OpConstant %i32 1\n"																		\
	"%c_i32_2 = OpConstant %i32 2\n"																		\
	"%c_i32_3 = OpConstant %i32 3\n"																		\
	"%c_i32_4 = OpConstant %i32 4\n"																		\
	"%c_u32_0 = OpConstant %u32 0\n"																		\
	"%c_u32_1 = OpConstant %u32 1\n"																		\
	"%c_u32_2 = OpConstant %u32 2\n"																		\
	"%c_u32_3 = OpConstant %u32 3\n"																		\
	"%c_u32_32 = OpConstant %u32 32\n"																		\
	"%c_u32_4 = OpConstant %u32 4\n"																		\
	"%c_u32_31_bits = OpConstant %u32 0x7FFFFFFF\n"															\
	"%c_v4f32_1_1_1_1 = OpConstantComposite %v4f32 %c_f32_1 %c_f32_1 %c_f32_1 %c_f32_1\n"					\
	"%c_v4f32_1_0_0_1 = OpConstantComposite %v4f32 %c_f32_1 %c_f32_0 %c_f32_0 %c_f32_1\n"					\
	"%c_v4f32_0_5_0_5_0_5_0_5 = OpConstantComposite %v4f32 %c_f32_0_5 %c_f32_0_5 %c_f32_0_5 %c_f32_0_5\n"	\

#define SPIRV_ASSEMBLY_ARRAYS																				\
	"%a1f32 = OpTypeArray %f32 %c_u32_1\n"																	\
	"%a2f32 = OpTypeArray %f32 %c_u32_2\n"																	\
	"%a3v4f32 = OpTypeArray %v4f32 %c_u32_3\n"																\
	"%a4f32 = OpTypeArray %f32 %c_u32_4\n"																	\
	"%a32v4f32 = OpTypeArray %v4f32 %c_u32_32\n"															\
	"%ip_a3v4f32 = OpTypePointer Input %a3v4f32\n"															\
	"%ip_a32v4f32 = OpTypePointer Input %a32v4f32\n"														\
	"%op_a2f32 = OpTypePointer Output %a2f32\n"																\
	"%op_a3v4f32 = OpTypePointer Output %a3v4f32\n"															\
	"%op_a4f32 = OpTypePointer Output %a4f32\n"																\

/*--------------------------------------------------------------------*//*!
 * \brief Abstract class for an input/output storage buffer object
 *//*--------------------------------------------------------------------*/
class BufferInterface
{
public:
	virtual				~BufferInterface	(void)				{}

	virtual void		getBytes			(std::vector<deUint8>& bytes) const = 0;
	virtual void		getPackedBytes		(std::vector<deUint8>& bytes) const = 0;
	virtual size_t		getByteSize			(void) const = 0;
};

typedef de::SharedPtr<BufferInterface>	BufferSp;
typedef de::MovePtr<vk::Allocation>		AllocationMp;
typedef de::SharedPtr<vk::Allocation>	AllocationSp;

class Resource
{
public:
	Resource(const BufferSp& buffer_, vk::VkDescriptorType descriptorType_ = vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, void* userData_ = NULL)
		: buffer(buffer_)
		, descriptorType(descriptorType_)
		, userData(userData_)
	{
	}

	virtual const BufferSp&			getBuffer			() const							{ return buffer; }
	virtual void					getBytes			(std::vector<deUint8>& bytes) const	{ buffer->getBytes(bytes); }
	virtual size_t					getByteSize			(void) const						{ return buffer->getByteSize(); }

	virtual void					setDescriptorType	(vk::VkDescriptorType type)		{ descriptorType = type; }
	virtual vk::VkDescriptorType	getDescriptorType	()	const						{ return descriptorType; }

	virtual void					setUserData			(void* data)					{ userData = data; }
	virtual void*					getUserData			() const						{ return userData; }

private:
	BufferSp				buffer;
	vk::VkDescriptorType	descriptorType;
	void*					userData;
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

struct VulkanFeatures
{
	vk::VkPhysicalDeviceFeatures							coreFeatures;
	vk::VkPhysicalDeviceShaderFloat16Int8Features			extFloat16Int8;
	vk::VkPhysicalDevice8BitStorageFeatures					ext8BitStorage;
	vk::VkPhysicalDevice16BitStorageFeatures				ext16BitStorage;
	vk::VkPhysicalDeviceVariablePointersFeatures			extVariablePointers;
	vk::VkPhysicalDeviceVulkanMemoryModelFeatures			extVulkanMemoryModel;
	vk::VkPhysicalDeviceShaderIntegerDotProductFeaturesKHR	extIntegerDotProduct;
	vk::VkPhysicalDeviceFloatControlsProperties				floatControlsProperties;

	VulkanFeatures				(void)
	{
		deMemset(&coreFeatures,				0, sizeof(coreFeatures));
		deMemset(&extFloat16Int8,			0, sizeof(vk::VkPhysicalDeviceShaderFloat16Int8Features));
		deMemset(&ext8BitStorage,			0, sizeof(vk::VkPhysicalDevice8BitStorageFeatures));
		deMemset(&ext16BitStorage,			0, sizeof(vk::VkPhysicalDevice16BitStorageFeatures));
		deMemset(&extVariablePointers,		0, sizeof(vk::VkPhysicalDeviceVariablePointersFeatures));
		deMemset(&extVulkanMemoryModel,		0, sizeof(vk::VkPhysicalDeviceVulkanMemoryModelFeatures));
		deMemset(&extIntegerDotProduct,		0, sizeof(vk::VkPhysicalDeviceShaderIntegerDotProductFeaturesKHR));
		deMemset(&floatControlsProperties,	0, sizeof(vk::VkPhysicalDeviceFloatControlsProperties));
		floatControlsProperties.denormBehaviorIndependence	= vk::VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_NONE_KHR;
		floatControlsProperties.roundingModeIndependence	= vk::VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_NONE_KHR;
	}
};

// Returns true if the whole VulkanFeatures is supported. If not, missingFeature will contain one feature that was missing.
bool isVulkanFeaturesSupported(const Context& context, const VulkanFeatures& toCheck, const char** missingFeature);

struct VariableLocation
{
	deUint32 set;
	deUint32 binding;

	// Returns a string representation of the structure suitable for test names.
	std::string toString() const ;

	// Returns a string representation of the structure suitable for test descriptions.
	std::string toDescription() const;
};

// Returns true if the given float controls features in `toCheck` are all supported.
bool isFloatControlsFeaturesSupported(	const Context&										context,
										const vk::VkPhysicalDeviceFloatControlsProperties&	toCheck,
										const char**										missingFeature);

deUint32 getMinRequiredVulkanVersion (const vk::SpirvVersion version);

std::string	getVulkanName (const deUint32 version);

// Performs a bitwise copy of source to the destination type Dest.
template <typename Dest, typename Src>
Dest bitwiseCast (Src source)
{
  Dest dest;
  DE_STATIC_ASSERT(sizeof(source) == sizeof(dest));
  deMemcpy(&dest, &source, sizeof(dest));
  return dest;
}

// Generate and return 64-bit integers.
//
// Expected count to be at least 16.
std::vector<deInt64> getInt64s (de::Random& rnd, const deUint32 count);

// Generate and return 32-bit integers.
//
// Expected count to be at least 16.
std::vector<deInt32> getInt32s (de::Random& rnd, const deUint32 count);

// Generate and return 16-bit integers.
//
// Expected count to be at least 8.
std::vector<deInt16> getInt16s (de::Random& rnd, const deUint32 count);

// Generate and return 8-bit integers.
//
// Expected count to be at least 8.
std::vector<deInt8> getInt8s (de::Random& rnd, const deUint32 count);

// Generate and return 64-bit floats
//
// The first 24 number pairs are manually picked, while the rest are randomly generated.
// Expected count to be at least 24 (numPicks).
std::vector<double> getFloat64s (de::Random& rnd, deUint32 count);

// Generate and return 32-bit floats
//
// The first 24 number pairs are manually picked, while the rest are randomly generated.
// Expected count to be at least 24 (numPicks).
std::vector<float> getFloat32s (de::Random& rnd, deUint32 count);

// Generate and return 16-bit floats and their corresponding 32-bit values.
//
// The first 14 number pairs are manually picked, while the rest are randomly generated.
// Expected count to be at least 14 (numPicks).
std::vector<deFloat16> getFloat16s (de::Random& rnd, deUint32 count);

// Generate an OpCapability Shader line.
std::string getOpCapabilityShader();

// Generate an unused Vertex entry point.
std::string getUnusedEntryPoint();

// Generate unused decorations for an input/output buffer.
std::string getUnusedDecorations(const VariableLocation& location);

// Generate unused types and constants, including a buffer type.
std::string getUnusedTypesAndConstants();

// Generate the declaration of an unused buffer variable.
std::string getUnusedBuffer();

// Generate the body of an unused function that uses the previous buffer.
std::string getUnusedFunctionBody();

} // SpirVAssembly
} // vkt

#endif // _VKTSPVASMUTILS_HPP
