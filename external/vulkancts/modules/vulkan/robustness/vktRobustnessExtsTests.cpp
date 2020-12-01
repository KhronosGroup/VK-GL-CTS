/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017-2019 The Khronos Group Inc.
 * Copyright (c) 2018-2020 NVIDIA Corporation
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
 * \brief Vulkan robustness2 tests
 *//*--------------------------------------------------------------------*/

#include "vktRobustnessExtsTests.hpp"

#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkImageUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vktRobustnessUtil.hpp"

#include "vktTestGroupUtil.hpp"
#include "vktTestCase.hpp"

#include "deDefs.h"
#include "deMath.h"
#include "deRandom.h"
#include "deSharedPtr.hpp"
#include "deString.h"

#include "tcuVectorType.hpp"
#include "tcuTestCase.hpp"
#include "tcuTestLog.hpp"

#include <string>
#include <sstream>
#include <algorithm>

namespace vkt
{
namespace robustness
{
namespace
{
using namespace vk;
using namespace std;
using de::SharedPtr;

enum RobustnessFeatureBits
{
	RF_IMG_ROBUSTNESS	= (1		),
	RF_ROBUSTNESS2		= (1 << 1	),
	SIF_INT64ATOMICS	= (1 << 2	),
};

using RobustnessFeatures = deUint32;

// Class to wrap a singleton device with the indicated robustness features.
template <RobustnessFeatures FEATURES>
class SingletonDevice
{
	SingletonDevice	(Context& context)
		: m_logicalDevice ()
	{
		// Note we are already checking the needed features are available in checkSupport().
		VkPhysicalDeviceRobustness2FeaturesEXT				robustness2Features				= initVulkanStructure();
		VkPhysicalDeviceImageRobustnessFeaturesEXT			imageRobustnessFeatures			= initVulkanStructure();
		VkPhysicalDeviceScalarBlockLayoutFeatures			scalarBlockLayoutFeatures		= initVulkanStructure();
		VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT	shaderImageAtomicInt64Features	= initVulkanStructure();
		VkPhysicalDeviceFeatures2							features2						= initVulkanStructure();

		features2.pNext = &scalarBlockLayoutFeatures;

		if (FEATURES & RF_IMG_ROBUSTNESS)
		{
			DE_ASSERT(context.isDeviceFunctionalitySupported("VK_EXT_image_robustness"));
			imageRobustnessFeatures.pNext = features2.pNext;
			features2.pNext = &imageRobustnessFeatures;
		}

		if (FEATURES & RF_ROBUSTNESS2)
		{
			DE_ASSERT(context.isDeviceFunctionalitySupported("VK_EXT_robustness2"));
			robustness2Features.pNext = features2.pNext;
			features2.pNext = &robustness2Features;
		}

		if (FEATURES & SIF_INT64ATOMICS)
		{
			DE_ASSERT(context.isDeviceFunctionalitySupported("VK_EXT_shader_image_atomic_int64"));
			shaderImageAtomicInt64Features.pNext = features2.pNext;
			features2.pNext = &shaderImageAtomicInt64Features;
		}

		context.getInstanceInterface().getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &features2);
		m_logicalDevice = createRobustBufferAccessDevice(context, &features2);
	}

public:
	static VkDevice getDevice(Context& context)
	{
		if (!m_singletonDevice)
			m_singletonDevice = SharedPtr<SingletonDevice>(new SingletonDevice(context));
		DE_ASSERT(m_singletonDevice);
		return m_singletonDevice->m_logicalDevice.get();
	}

	static void destroy()
	{
		m_singletonDevice.clear();
	}

private:
	Move<vk::VkDevice>							m_logicalDevice;
	static SharedPtr<SingletonDevice<FEATURES>>	m_singletonDevice;
};

template <RobustnessFeatures FEATURES>
SharedPtr<SingletonDevice<FEATURES>> SingletonDevice<FEATURES>::m_singletonDevice;

constexpr RobustnessFeatures kImageRobustness			= RF_IMG_ROBUSTNESS;
constexpr RobustnessFeatures kRobustness2				= RF_ROBUSTNESS2;
constexpr RobustnessFeatures kShaderImageInt64Atomics	= SIF_INT64ATOMICS;

using ImageRobustnessSingleton	= SingletonDevice<kImageRobustness>;
using Robustness2Singleton		= SingletonDevice<kRobustness2>;

using ImageRobustnessInt64AtomicsSingleton	= SingletonDevice<kImageRobustness | kShaderImageInt64Atomics>;
using Robustness2Int64AtomicsSingleton		= SingletonDevice<kRobustness2 | kShaderImageInt64Atomics>;

// Render target / compute grid dimensions
static const deUint32 DIM = 8;

// treated as a phony VkDescriptorType value
#define VERTEX_ATTRIBUTE_FETCH 999

typedef enum
{
	STAGE_COMPUTE = 0,
	STAGE_VERTEX,
	STAGE_FRAGMENT,
	STAGE_RAYGEN
} Stage;

struct CaseDef
{
	VkFormat format;
	Stage stage;
	VkFlags allShaderStages;
	VkFlags allPipelineStages;
	int/*VkDescriptorType*/ descriptorType;
	VkImageViewType viewType;
	VkSampleCountFlagBits samples;
	int bufferLen;
	bool unroll;
	bool vol;
	bool nullDescriptor;
	bool useTemplate;
	bool formatQualifier;
	bool pushDescriptor;
	bool testRobustness2;
	deUint32 imageDim[3]; // width, height, depth or layers
};

static bool formatIsR64(const VkFormat& f)
{
	switch (f)
	{
	case VK_FORMAT_R64_SINT:
	case VK_FORMAT_R64_UINT:
		return true;
	default:
		return false;
	}
}

// Returns the appropriate singleton device for the given case.
VkDevice getLogicalDevice (Context& ctx, const CaseDef& caseDef)
{
	if (formatIsR64(caseDef.format))
	{
		if (caseDef.testRobustness2)
			return Robustness2Int64AtomicsSingleton::getDevice(ctx);
		return ImageRobustnessInt64AtomicsSingleton::getDevice(ctx);
	}

	if (caseDef.testRobustness2)
		return Robustness2Singleton::getDevice(ctx);
	return ImageRobustnessSingleton::getDevice(ctx);
}

class Layout
{
public:
	vector<VkDescriptorSetLayoutBinding> layoutBindings;
	vector<deUint8> refData;
};


class RobustnessExtsTestInstance : public TestInstance
{
public:
						RobustnessExtsTestInstance		(Context& context, const CaseDef& data);
						~RobustnessExtsTestInstance	(void);
	tcu::TestStatus		iterate								(void);
private:
	CaseDef				m_data;
};

RobustnessExtsTestInstance::RobustnessExtsTestInstance (Context& context, const CaseDef& data)
	: vkt::TestInstance		(context)
	, m_data				(data)
{
}

RobustnessExtsTestInstance::~RobustnessExtsTestInstance (void)
{
}

class RobustnessExtsTestCase : public TestCase
{
	public:
								RobustnessExtsTestCase		(tcu::TestContext& context, const char* name, const char* desc, const CaseDef data);
								~RobustnessExtsTestCase	(void);
	virtual	void				initPrograms					(SourceCollections& programCollection) const;
	virtual TestInstance*		createInstance					(Context& context) const;
	virtual void				checkSupport					(Context& context) const;

private:
	CaseDef					m_data;
};

RobustnessExtsTestCase::RobustnessExtsTestCase (tcu::TestContext& context, const char* name, const char* desc, const CaseDef data)
	: vkt::TestCase	(context, name, desc)
	, m_data		(data)
{
}

RobustnessExtsTestCase::~RobustnessExtsTestCase	(void)
{
}

static bool formatIsFloat(const VkFormat& f)
{
	switch (f)
	{
	case VK_FORMAT_R32_SFLOAT:
	case VK_FORMAT_R32G32_SFLOAT:
	case VK_FORMAT_R32G32B32A32_SFLOAT:
		return true;
	default:
		return false;
	}
}

static bool formatIsSignedInt(const VkFormat& f)
{
	switch (f)
	{
	case VK_FORMAT_R32_SINT:
	case VK_FORMAT_R64_SINT:
	case VK_FORMAT_R32G32_SINT:
	case VK_FORMAT_R32G32B32A32_SINT:
		return true;
	default:
		return false;
	}
}

static bool supportsStores(int descriptorType)
{
	switch (descriptorType)
	{
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
	case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
	case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
		return true;
	default:
		return false;
	}
}

Move<VkPipeline> makeComputePipeline (const DeviceInterface&	vk,
									  const VkDevice			device,
									  const VkPipelineLayout	pipelineLayout,
									  const VkShaderModule		shaderModule)
{
	const VkPipelineShaderStageCreateInfo pipelineShaderStageParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType						sType;
		DE_NULL,												// const void*							pNext;
		(VkPipelineShaderStageCreateFlags)0,					// VkPipelineShaderStageCreateFlags		flags;
		VK_SHADER_STAGE_COMPUTE_BIT,							// VkShaderStageFlagBits				stage;
		shaderModule,											// VkShaderModule						module;
		"main",													// const char*							pName;
		DE_NULL,												// const VkSpecializationInfo*			pSpecializationInfo;
	};

	const VkComputePipelineCreateInfo pipelineCreateInfo =
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,		// VkStructureType					sType;
		DE_NULL,											// const void*						pNext;
		0u,													// VkPipelineCreateFlags			flags;
		pipelineShaderStageParams,							// VkPipelineShaderStageCreateInfo	stage;
		pipelineLayout,										// VkPipelineLayout					layout;
		(vk::VkPipeline)0,									// VkPipeline						basePipelineHandle;
		0,													// deInt32							basePipelineIndex;
	};

	return createComputePipeline(vk, device, DE_NULL , &pipelineCreateInfo);
}

void RobustnessExtsTestCase::checkSupport(Context& context) const
{
	const auto&	vki				= context.getInstanceInterface();
	const auto	physicalDevice	= context.getPhysicalDevice();

	// We need to query feature support using the physical device instead of using the reported context features because robustness2
	// and image robustness are always disabled in the default device but they may be available.
	VkPhysicalDeviceRobustness2FeaturesEXT				robustness2Features				= initVulkanStructure();
	VkPhysicalDeviceImageRobustnessFeaturesEXT			imageRobustnessFeatures			= initVulkanStructure();
	VkPhysicalDeviceScalarBlockLayoutFeatures			scalarLayoutFeatures			= initVulkanStructure();
	VkPhysicalDeviceFeatures2KHR						features2						= initVulkanStructure();

	context.requireInstanceFunctionality("VK_KHR_get_physical_device_properties2");

	context.requireDeviceFunctionality("VK_EXT_scalar_block_layout");
	features2.pNext = &scalarLayoutFeatures;

	if (context.isDeviceFunctionalitySupported("VK_EXT_image_robustness"))
	{
		imageRobustnessFeatures.pNext = features2.pNext;
		features2.pNext = &imageRobustnessFeatures;
	}

	if (context.isDeviceFunctionalitySupported("VK_EXT_robustness2"))
	{
		robustness2Features.pNext = features2.pNext;
		features2.pNext = &robustness2Features;
	}

	vki.getPhysicalDeviceFeatures2(physicalDevice, &features2);

	if (formatIsR64(m_data.format))
	{
		context.requireDeviceFunctionality("VK_EXT_shader_image_atomic_int64");

		VkFormatProperties formatProperties;
		vki.getPhysicalDeviceFormatProperties(context.getPhysicalDevice(), m_data.format, &formatProperties);

		switch (m_data.descriptorType)
		{
		case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
			if ((formatProperties.bufferFeatures & VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT) != VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT)
				TCU_THROW(NotSupportedError, "VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT is not supported");
			break;
		case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
			if ((formatProperties.bufferFeatures & VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT) != VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT)
				TCU_THROW(NotSupportedError, "VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT is not supported");
			break;
		case VERTEX_ATTRIBUTE_FETCH:
			if ((formatProperties.bufferFeatures & VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT) != VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT)
				TCU_THROW(NotSupportedError, "VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT is not supported");
			break;
		case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
			if ((formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)
				TCU_THROW(NotSupportedError, "VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT is not supported");
			break;
		default: DE_ASSERT(true);
		}

		if (m_data.samples > VK_SAMPLE_COUNT_1_BIT)
		{
			if ((formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)
				TCU_THROW(NotSupportedError, "VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT is not supported");
		}
	}

	// Check needed properties and features
	if (!scalarLayoutFeatures.scalarBlockLayout)
		TCU_THROW(NotSupportedError, "Scalar block layout not supported");

	if (m_data.stage == STAGE_VERTEX && !features2.features.vertexPipelineStoresAndAtomics)
		TCU_THROW(NotSupportedError, "Vertex pipeline stores and atomics not supported");

	if (m_data.stage == STAGE_RAYGEN)
		context.requireDeviceFunctionality("VK_NV_ray_tracing");

	switch (m_data.descriptorType)
	{
	default: DE_ASSERT(0); // Fallthrough
	case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
	case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
	case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
	case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
	case VERTEX_ATTRIBUTE_FETCH:
		if (m_data.testRobustness2)
		{
			if (!robustness2Features.robustBufferAccess2)
				TCU_THROW(NotSupportedError, "robustBufferAccess2 not supported");
		}
		else
		{
			// This case is not tested here.
			DE_ASSERT(false);
		}
		break;
	case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
	case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
		if (m_data.testRobustness2)
		{
			if (!robustness2Features.robustImageAccess2)
				TCU_THROW(NotSupportedError, "robustImageAccess2 not supported");
		}
		else
		{
			if (!imageRobustnessFeatures.robustImageAccess)
				TCU_THROW(NotSupportedError, "robustImageAccess not supported");
		}
		break;
	}

	if (m_data.nullDescriptor && !robustness2Features.nullDescriptor)
		TCU_THROW(NotSupportedError, "nullDescriptor not supported");

	if ((m_data.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) &&
		m_data.samples != VK_SAMPLE_COUNT_1_BIT &&
		!features2.features.shaderStorageImageMultisample)
		TCU_THROW(NotSupportedError, "shaderStorageImageMultisample not supported");

	if ((m_data.useTemplate || formatIsR64(m_data.format)) && !context.contextSupports(vk::ApiVersion(1, 1, 0)))
		TCU_THROW(NotSupportedError, "Vulkan 1.1 not supported");

	if ((m_data.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER || m_data.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) &&
		!m_data.formatQualifier &&
		(!features2.features.shaderStorageImageReadWithoutFormat || !features2.features.shaderStorageImageWriteWithoutFormat))
		TCU_THROW(NotSupportedError, "shaderStorageImageReadWithoutFormat or shaderStorageImageWriteWithoutFormat not supported");

	if (m_data.pushDescriptor)
		context.requireDeviceFunctionality("VK_KHR_push_descriptor");
}

void generateLayout(Layout &layout, const CaseDef &caseDef)
{
	vector<VkDescriptorSetLayoutBinding> &bindings = layout.layoutBindings;
	int numBindings = caseDef.descriptorType != VERTEX_ATTRIBUTE_FETCH ? 2 : 1;
	bindings = vector<VkDescriptorSetLayoutBinding>(numBindings);

	for (deUint32 b = 0; b < layout.layoutBindings.size(); ++b)
	{
		VkDescriptorSetLayoutBinding &binding = bindings[b];
		binding.binding = b;
		binding.pImmutableSamplers = NULL;
		binding.stageFlags = caseDef.allShaderStages;
		binding.descriptorCount = 1;

		// Output image
		if (b == 0)
			binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		else if (caseDef.descriptorType != VERTEX_ATTRIBUTE_FETCH)
			binding.descriptorType = (VkDescriptorType)caseDef.descriptorType;
	}

	if (caseDef.nullDescriptor)
		return;

	if (caseDef.bufferLen == 0)
	{
		// Clear color values for image tests
		static deUint32 urefData[4]		= { 0x12345678, 0x23456789, 0x34567890, 0x45678901 };
		static deUint64 urefData64[4]	= { 0x1234567887654321, 0x234567899, 0x345678909, 0x456789019 };
		static float frefData[4]		= { 123.f, 234.f, 345.f, 456.f };

		if (formatIsR64(caseDef.format))
		{
			layout.refData.resize(32);
			deUint64 *ptr = (deUint64 *)layout.refData.data();

			for (unsigned int i = 0; i < 4; ++i)
			{
				ptr[i] = urefData64[i];
			}
		}
		else
		{
			layout.refData.resize(16);
			deMemcpy(layout.refData.data(), formatIsFloat(caseDef.format) ? (const void *)frefData : (const void *)urefData, sizeof(frefData));
		}
	}
	else
	{
		layout.refData.resize(caseDef.bufferLen & (formatIsR64(caseDef.format) ? ~7: ~3));
		for (unsigned int i = 0; i < caseDef.bufferLen / (formatIsR64(caseDef.format) ? sizeof(deUint64) : sizeof(deUint32)); ++i)
		{
			if (formatIsFloat(caseDef.format))
			{
				float *f = (float *)layout.refData.data() + i;
				*f = 2.0f*(float)i + 3.0f;
			}
			if (formatIsR64(caseDef.format))
			{
				deUint64 *u = (deUint64 *)layout.refData.data() + i;
				*u = 2 * i + 3;
			}
			else
			{
				int *u = (int *)layout.refData.data() + i;
				*u = 2*i + 3;
			}
		}
	}
}

static string genFetch(const CaseDef &caseDef, int numComponents, const string& vecType, const string& coord, const string& lod)
{
	std::stringstream s;
	// Fetch from the descriptor.
	switch (caseDef.descriptorType)
	{
	case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
	case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
		s << vecType << "(ubo0_1.val[" << coord << "]";
		for (int i = numComponents; i < 4; ++i) s << ", 0";
		s << ")";
		break;
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
		s << vecType << "(ssbo0_1.val[" << coord << "]";
		for (int i = numComponents; i < 4; ++i) s << ", 0";
		s << ")";
		break;
	case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
		s << "texelFetch(texbo0_1, " << coord << ")";
		break;
	case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
		s << "imageLoad(image0_1, " << coord << ")";
		break;
	case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
		if (caseDef.samples > VK_SAMPLE_COUNT_1_BIT)
			s << "texelFetch(texture0_1, " << coord << ")";
		else
			s << "texelFetch(texture0_1, " << coord << ", " << lod << ")";
		break;
	case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
		s << "imageLoad(image0_1, " << coord << ")";
		break;
	case VERTEX_ATTRIBUTE_FETCH:
		s << "attr";
		break;
	default: DE_ASSERT(0);
	}
	return s.str();
}

static const int storeValue = 123;

// Get the value stored by genStore.
static string getStoreValue(int descriptorType, int numComponents, const string& vecType, const string& bufType)
{
	std::stringstream s;
	switch (descriptorType)
	{
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
		s << vecType  << "(" << bufType << "(" << storeValue << ")";
		for (int i = numComponents; i < 4; ++i) s << ", 0";
		s << ")";
		break;
	case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
		s << vecType << "(" << storeValue << ")";
		break;
	case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
		s << vecType << "(" << storeValue << ")";
		break;
	default: DE_ASSERT(0);
	}
	return s.str();
}

static string genStore(int descriptorType, const string& vecType, const string& bufType, const string& coord)
{
	std::stringstream s;
	// Store to the descriptor.
	switch (descriptorType)
	{
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
		s << "ssbo0_1.val[" << coord << "] = " << bufType << "(" << storeValue << ")";
		break;
	case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
		s << "imageStore(image0_1, " << coord << ", " << vecType << "(" << storeValue << "))";
		break;
	case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
		s << "imageStore(image0_1, " << coord << ", " << vecType << "(" << storeValue << "))";
		break;
	default: DE_ASSERT(0);
	}
	return s.str();
}

static string genAtomic(int descriptorType, const string& bufType, const string& coord)
{
	std::stringstream s;
	// Store to the descriptor. The value doesn't matter, since we only test out of bounds coordinates.
	switch (descriptorType)
	{
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
		s << "atomicAdd(ssbo0_1.val[" << coord << "], " << bufType << "(10))";
		break;
	case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
		s << "imageAtomicAdd(image0_1, " << coord << ", " << bufType << "(10))";
		break;
	case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
		s << "imageAtomicAdd(image0_1, " << coord << ", " << bufType << "(10))";
		break;
	default: DE_ASSERT(0);
	}
	return s.str();
}

static std::string getShaderImageFormatQualifier (const tcu::TextureFormat& format)
{
	const char* orderPart;
	const char* typePart;

	switch (format.order)
	{
		case tcu::TextureFormat::R:		orderPart = "r";	break;
		case tcu::TextureFormat::RG:	orderPart = "rg";	break;
		case tcu::TextureFormat::RGB:	orderPart = "rgb";	break;
		case tcu::TextureFormat::RGBA:	orderPart = "rgba";	break;

		default:
			DE_FATAL("Impossible");
			orderPart = DE_NULL;
	}

	switch (format.type)
	{
		case tcu::TextureFormat::FLOAT:				typePart = "32f";		break;
		case tcu::TextureFormat::HALF_FLOAT:		typePart = "16f";		break;

		case tcu::TextureFormat::UNSIGNED_INT64:	typePart = "64ui";		break;
		case tcu::TextureFormat::UNSIGNED_INT32:	typePart = "32ui";		break;
		case tcu::TextureFormat::UNSIGNED_INT16:	typePart = "16ui";		break;
		case tcu::TextureFormat::UNSIGNED_INT8:		typePart = "8ui";		break;

		case tcu::TextureFormat::SIGNED_INT64:		typePart = "64i";		break;
		case tcu::TextureFormat::SIGNED_INT32:		typePart = "32i";		break;
		case tcu::TextureFormat::SIGNED_INT16:		typePart = "16i";		break;
		case tcu::TextureFormat::SIGNED_INT8:		typePart = "8i";		break;

		case tcu::TextureFormat::UNORM_INT16:		typePart = "16";		break;
		case tcu::TextureFormat::UNORM_INT8:		typePart = "8";			break;

		case tcu::TextureFormat::SNORM_INT16:		typePart = "16_snorm";	break;
		case tcu::TextureFormat::SNORM_INT8:		typePart = "8_snorm";	break;

		default:
			DE_FATAL("Impossible");
			typePart = DE_NULL;
	}

	return std::string() + orderPart + typePart;
}

string genCoord(string c, int numCoords, VkSampleCountFlagBits samples, int dim)
{
	if (numCoords == 1)
		return c;

	if (samples != VK_SAMPLE_COUNT_1_BIT)
		numCoords--;

	string coord = "ivec" + to_string(numCoords) + "(";

	for (int i = 0; i < numCoords; ++i)
	{
		if (i == dim)
			coord += c;
		else
			coord += "0";
		if (i < numCoords - 1)
			coord += ", ";
	}
	coord += ")";

	// Append sample coordinate
	if (samples != VK_SAMPLE_COUNT_1_BIT)
	{
		coord += ", ";
		if (dim == numCoords)
			coord += c;
		else
			coord += "0";
	}
	return coord;
}

// Normalized coordinates. Divide by "imageDim" and add 0.25 so we're not on a pixel boundary.
string genCoordNorm(const CaseDef &caseDef, string c, int numCoords, int numNormalizedCoords, int dim)
{
	if (numCoords == 1)
		return c + " / float(" + to_string(caseDef.imageDim[dim]) + ")";

	string coord = "vec" + to_string(numCoords) + "(";

	for (int i = 0; i < numCoords; ++i)
	{
		if (i == dim)
			coord += c;
		else
			coord += "0.25";
		if (i < numNormalizedCoords)
			coord += " / float(" + to_string(caseDef.imageDim[dim]) + ")";
		if (i < numCoords - 1)
			coord += ", ";
	}
	coord += ")";
	return coord;
}

void RobustnessExtsTestCase::initPrograms (SourceCollections& programCollection) const
{
	VkFormat format = m_data.format;

	Layout layout;
	generateLayout(layout, m_data);

	if (layout.layoutBindings.size() > 1 &&
		layout.layoutBindings[1].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
	{
		if (format == VK_FORMAT_R64_SINT)
			format = VK_FORMAT_R32G32_SINT;

		if (format == VK_FORMAT_R64_UINT)
			format = VK_FORMAT_R32G32_UINT;
	}

	std::stringstream decls, checks;

	const string	r64		= formatIsR64(format) ? "64" : "";
	const string	i64Type	= formatIsR64(format) ? "64_t" : "";
	const string	vecType	= formatIsFloat(format) ? "vec4" : (formatIsSignedInt(format) ? ("i" + r64 + "vec4") : ("u" + r64 + "vec4"));

	decls << "uvec4 abs(uvec4 x) { return x; }\n";
	if (formatIsR64(format))
		decls << "u64vec4 abs(u64vec4 x) { return x; }\n";
	decls << "int smod(int a, int b) { if (a < 0) a += b*(abs(a)/b+1); return a%b; }\n";


	const int	componetsSize = (formatIsR64(format) ? 8 : 4);
	int			refDataNumElements = deIntRoundToPow2(((int)layout.refData.size() / componetsSize), 4);
	// Pad reference data to include zeros, up to max value of robustUniformBufferAccessSizeAlignment (256).
	// robustStorageBufferAccessSizeAlignment is 4, so no extra padding needed.
	if (m_data.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
		m_data.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
	{
		refDataNumElements = deIntRoundToPow2(refDataNumElements, 256 / (formatIsR64(format) ? 8 : 4));
	}
	if (m_data.nullDescriptor)
		refDataNumElements = 4;

	if (formatIsFloat(format))
	{
		decls << "float refData[" << refDataNumElements << "] = {";
		int i;
		for (i = 0; i < (int)layout.refData.size() / 4; ++i)
		{
			if (i != 0)
				decls << ", ";
			decls << ((const float *)layout.refData.data())[i];
		}
		while (i < refDataNumElements)
		{
			if (i != 0)
				decls << ", ";
			decls << "0";
			i++;
		}
	}
	else if (formatIsR64(format))
	{
		decls << "int" << i64Type << " refData[" << refDataNumElements << "] = {";
		int i;
		for (i = 0; i < (int)layout.refData.size() / 8; ++i)
		{
			if (i != 0)
				decls << ", ";
			decls << ((const deUint64 *)layout.refData.data())[i] << "l";
		}
		while (i < refDataNumElements)
		{
			if (i != 0)
				decls << ", ";
			decls << "0l";
			i++;
		}
	}
	else
	{
		decls << "int" << " refData[" << refDataNumElements << "] = {";
		int i;
		for (i = 0; i < (int)layout.refData.size() / 4; ++i)
		{
			if (i != 0)
				decls << ", ";
			decls << ((const int *)layout.refData.data())[i];
		}
		while (i < refDataNumElements)
		{
			if (i != 0)
				decls << ", ";
			decls << "0";
			i++;
		}
	}

	decls << "};\n";
	decls << vecType << " zzzz = " << vecType << "(0);\n";
	decls << vecType << " zzzo = " << vecType << "(0, 0, 0, 1);\n";
	decls << vecType << " expectedIB;\n";

	string imgprefix = (formatIsFloat(format) ? "" : formatIsSignedInt(format) ? "i" : "u") + r64;
	string imgqualif = (m_data.formatQualifier) ? getShaderImageFormatQualifier(mapVkFormat(format)) + ", " : "";
	string outputimgqualif = getShaderImageFormatQualifier(mapVkFormat(format));

	string imageDim = "";
	int numCoords, numNormalizedCoords;
	bool layered = false;
	switch (m_data.viewType)
	{
		default: DE_ASSERT(0); // Fallthrough
		case VK_IMAGE_VIEW_TYPE_1D:			imageDim = "1D";		numCoords = 1;	numNormalizedCoords = 1;	break;
		case VK_IMAGE_VIEW_TYPE_1D_ARRAY:	imageDim = "1DArray";	numCoords = 2;	numNormalizedCoords = 1;	layered = true;	break;
		case VK_IMAGE_VIEW_TYPE_2D:			imageDim = "2D";		numCoords = 2;	numNormalizedCoords = 2;	break;
		case VK_IMAGE_VIEW_TYPE_2D_ARRAY:	imageDim = "2DArray";	numCoords = 3;	numNormalizedCoords = 2;	layered = true;	break;
		case VK_IMAGE_VIEW_TYPE_3D:			imageDim = "3D";		numCoords = 3;	numNormalizedCoords = 3;	break;
		case VK_IMAGE_VIEW_TYPE_CUBE:		imageDim = "Cube";		numCoords = 3;	numNormalizedCoords = 3;	break;
		case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:	imageDim = "CubeArray";	numCoords = 4;	numNormalizedCoords = 3;	layered = true;	break;
	}
	if (m_data.samples > VK_SAMPLE_COUNT_1_BIT)
	{
		switch (m_data.viewType)
		{
			default: DE_ASSERT(0); // Fallthrough
			case VK_IMAGE_VIEW_TYPE_2D:			imageDim = "2DMS";		break;
			case VK_IMAGE_VIEW_TYPE_2D_ARRAY:	imageDim = "2DMSArray";	break;
		}
		numCoords++;
	}
	bool dataDependsOnLayer = (m_data.viewType == VK_IMAGE_VIEW_TYPE_1D_ARRAY || m_data.viewType == VK_IMAGE_VIEW_TYPE_2D_ARRAY) && !m_data.nullDescriptor;

	// Special case imageLoad(imageCubeArray, ...) which uses ivec3
	if (m_data.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE &&
		m_data.viewType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)
	{
		numCoords = 3;
	}

	int numComponents = tcu::getPixelSize(mapVkFormat(format)) / tcu::getChannelSize(mapVkFormat(format).type);
	string bufType;
	if (numComponents == 1)
		bufType = string(formatIsFloat(format) ? "float" : formatIsSignedInt(format) ? "int" : "uint") + i64Type;
	else
		bufType = imgprefix + "vec" + std::to_string(numComponents);

	checks << "  int inboundcoords, clampedLayer;\n";
	checks << "  " << vecType << " expectedIB2;\n";
	if (m_data.unroll)
		checks << "  [[unroll]] for (int c = -10; c <= 10; ++c) {\n";
	else
		checks << "  [[dont_unroll]] for (int c = 1050; c >= -1050; --c) {\n";

	if (m_data.descriptorType == VERTEX_ATTRIBUTE_FETCH)
		checks << "    int idx = smod(gl_VertexIndex * " << numComponents << ", " << refDataNumElements << ");\n";
	else
		checks << "    int idx = smod(c * " << numComponents << ", " << refDataNumElements << ");\n";

	decls << "layout(" << outputimgqualif << ", set = 0, binding = 0) uniform " << imgprefix << "image2D image0_0;\n";

	const char *vol = m_data.vol ? "volatile " : "";

	// Construct the declaration for the binding
	switch (m_data.descriptorType)
	{
	case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
	case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
		decls << "layout(scalar, set = 0, binding = 1) uniform ubodef0_1 { " << bufType << " val[1024]; } ubo0_1;\n";
		break;
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
		decls << "layout(scalar, set = 0, binding = 1) " << vol << "buffer sbodef0_1 { " << bufType << " val[1024]; } ssbo0_1;\n";
		decls << "layout(scalar, set = 0, binding = 1) " << vol << "buffer sbodef0_1_unsized { " << bufType << " val[]; } ssbo0_1_unsized;\n";
		decls << "layout(scalar, set = 0, binding = 1) " << vol << "buffer sbodef0_1_unsized_pad { vec4 pad; " << bufType << " val[]; } ssbo0_1_unsized_pad;\n";
		break;
	case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
		switch(format)
		{
		case VK_FORMAT_R64_SINT:
			decls << "layout(set = 0, binding = 1) uniform itextureBuffer texbo0_1;\n";
			break;
		case VK_FORMAT_R64_UINT:
			decls << "layout(set = 0, binding = 1) uniform utextureBuffer texbo0_1;\n";
			break;
		default:
			decls << "layout(set = 0, binding = 1) uniform " << imgprefix << "textureBuffer texbo0_1;\n";
		}
		break;
	case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
		decls << "layout(" << imgqualif << "set = 0, binding = 1) " << vol << "uniform " << imgprefix << "imageBuffer image0_1;\n";
		break;
	case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
		decls << "layout(" << imgqualif << "set = 0, binding = 1) " << vol << "uniform " << imgprefix << "image" << imageDim << " image0_1;\n";
		break;
	case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
		switch (format)
		{
		case VK_FORMAT_R64_SINT:
			decls << "layout(set = 0, binding = 1) uniform isampler" << imageDim << " texture0_1; \n";
			break;
		case VK_FORMAT_R64_UINT:
			decls << "layout(set = 0, binding = 1) uniform usampler" << imageDim << " texture0_1; \n";
			break;
		default:
			decls << "layout(set = 0, binding = 1) uniform " << imgprefix << "sampler" << imageDim << " texture0_1;\n";
			break;
		}
		break;
	case VERTEX_ATTRIBUTE_FETCH:
		if (formatIsR64(format))
		{
			decls << "layout(location = 0) in " << (formatIsSignedInt(format) ? ("int64_t") : ("uint64_t")) << " attr;\n";
		}
		else
		{
			decls << "layout(location = 0) in " << vecType << " attr;\n";
		}
		break;
	default: DE_ASSERT(0);
	}

	string expectedOOB;
	string defaultw;

	switch (m_data.descriptorType)
	{
	default: DE_ASSERT(0); // Fallthrough
	case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
	case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
		expectedOOB = "zzzz";
		defaultw = "0";
		break;
	case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
	case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
	case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
	case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
	case VERTEX_ATTRIBUTE_FETCH:
		if (numComponents == 1)
		{
			expectedOOB = "zzzo";
		}
		else if (numComponents == 2)
		{
			expectedOOB = "zzzo";
		}
		else
		{
			expectedOOB = "zzzz";
		}
		defaultw = "1";
		break;
	}

	string idx;
	switch (m_data.descriptorType)
	{
	default: DE_ASSERT(0); // Fallthrough
	case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
	case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
	case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
	case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
	case VERTEX_ATTRIBUTE_FETCH:
		idx = "idx";
		break;
	case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
	case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
		idx = "0";
		break;
	}

	if (m_data.nullDescriptor)
	{
		checks << "    expectedIB = zzzz;\n";
		checks << "    inboundcoords = 0;\n";
		checks << "    int paddedinboundcoords = 0;\n";
		// Vertex attribute fetch still gets format conversion applied
		if (m_data.descriptorType != VERTEX_ATTRIBUTE_FETCH)
			expectedOOB = "zzzz";
	}
	else
	{
		checks << "    expectedIB.x = refData[" << idx << "];\n";
		if (numComponents > 1)
		{
			checks << "    expectedIB.y = refData[" << idx << "+1];\n";
		}
		else
		{
			checks << "    expectedIB.y = 0;\n";
		}
		if (numComponents > 2)
		{
			checks << "    expectedIB.z = refData[" << idx << "+2];\n";
			checks << "    expectedIB.w = refData[" << idx << "+3];\n";
		}
		else
		{
			checks << "    expectedIB.z = 0;\n";
			checks << "    expectedIB.w = " << defaultw << ";\n";
		}

		switch (m_data.descriptorType)
		{
		default: DE_ASSERT(0); // Fallthrough
		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			// UBOs can either strictly bounds check against inboundcoords, or can
			// return the contents from memory for the range padded up to paddedinboundcoords.
			checks << "    int paddedinboundcoords = " << refDataNumElements / numComponents << ";\n";
			// fallthrough
		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
		case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
		case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
		case VERTEX_ATTRIBUTE_FETCH:
			checks << "    inboundcoords = " << layout.refData.size() / (formatIsR64(format) ? sizeof(deUint64) : sizeof(deUint32)) / numComponents << ";\n";
			break;
		case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
		case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
			// set per-component below
			break;
		}
	}

	if (m_data.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
		m_data.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER ||
		m_data.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
		m_data.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
	{
		for (int i = 0; i < numCoords; ++i)
		{
			// Treat i==3 coord (cube array layer) like i == 2
			deUint32 coordDim = m_data.imageDim[i == 3 ? 2 : i];
			if (!m_data.nullDescriptor && m_data.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
				checks << "    inboundcoords = " << coordDim << ";\n";

			string coord = genCoord("c", numCoords, m_data.samples, i);
			string inboundcoords =
				m_data.nullDescriptor ? "0" :
				(m_data.samples > VK_SAMPLE_COUNT_1_BIT && i == numCoords - 1) ? to_string(m_data.samples) : "inboundcoords";

			checks << "    if (c < 0 || c >= " << inboundcoords << ") " << genStore(m_data.descriptorType, vecType, bufType, coord) << ";\n";
			if (m_data.formatQualifier &&
				(format == VK_FORMAT_R32_SINT || format == VK_FORMAT_R32_UINT))
			{
				checks << "    if (c < 0 || c >= " << inboundcoords << ") " << genAtomic(m_data.descriptorType, bufType, coord) << ";\n";
			}
		}
	}

	for (int i = 0; i < numCoords; ++i)
	{
		// Treat i==3 coord (cube array layer) like i == 2
		deUint32 coordDim = m_data.imageDim[i == 3 ? 2 : i];
		if (!m_data.nullDescriptor)
		{
			switch (m_data.descriptorType)
			{
			default:
				break;
			case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
			case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
				checks << "    inboundcoords = " << coordDim << ";\n";
				break;
			}
		}

		string coord = genCoord("c", numCoords, m_data.samples, i);

		if (m_data.descriptorType == VERTEX_ATTRIBUTE_FETCH)
		{
			if (formatIsR64(format))
			{
				checks << "    temp.x = attr;\n";
				checks << "    temp.y = 0l;\n";
				checks << "    temp.z = 0l;\n";
				checks << "    temp.w = 0l;\n";
				checks << "    if (gl_VertexIndex >= 0 && gl_VertexIndex < inboundcoords) temp.x -= expectedIB.x; else temp -= zzzz;\n";
			}
			else
			{
				checks << "    temp = " << genFetch(m_data, numComponents, vecType, coord, "0") << ";\n";
				checks << "    if (gl_VertexIndex >= 0 && gl_VertexIndex < inboundcoords) temp -= expectedIB; else temp -= " << expectedOOB << ";\n";
			}
			// Accumulate any incorrect values.
			checks << "    accum += abs(temp);\n";
		}
		// Skip texelFetch testing for cube(array) - texelFetch doesn't support it
		if (m_data.descriptorType != VERTEX_ATTRIBUTE_FETCH &&
			!(m_data.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER &&
			  (m_data.viewType == VK_IMAGE_VIEW_TYPE_CUBE || m_data.viewType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)))
		{
			checks << "    temp = " << genFetch(m_data, numComponents, vecType, coord, "0") << ";\n";

			checks << "    expectedIB2 = expectedIB;\n";

			// Expected data is a function of layer, for array images. Subtract out the layer value for in-bounds coordinates.
			if (dataDependsOnLayer && i == numNormalizedCoords)
				checks << "    if (c >= 0 && c < inboundcoords) expectedIB2 += " << vecType << "(c, 0, 0, 0);\n";

			if (m_data.samples > VK_SAMPLE_COUNT_1_BIT && i == numCoords - 1)
			{
				if (m_data.nullDescriptor && m_data.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
				{
					checks << "    if (temp == zzzz) temp = " << vecType << "(0);\n";
					if (m_data.formatQualifier && numComponents < 4)
						checks << "    else if (temp == zzzo) temp = " << vecType << "(0);\n";
					checks << "    else temp = " << vecType << "(1);\n";
				}
				else
					// multisample coord doesn't have defined behavior for OOB, so just set temp to 0.
					checks << "    if (c >= 0 && c < " << m_data.samples << ") temp -= expectedIB2; else temp = " << vecType << "(0);\n";
			}
			else
			{
				// Storage buffers may be split into per-component loads. Generate a second
				// expected out of bounds value where some subset of the components are
				// actually in-bounds. If both loads and stores are split into per-component
				// accesses, then the result value can be a mix of storeValue and zero.
				string expectedOOB2 = expectedOOB;
				string expectedOOB3 = expectedOOB;
				if ((m_data.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
					 m_data.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC) &&
					 !m_data.nullDescriptor)
				{
					int len = m_data.bufferLen & (formatIsR64(format) ? ~7 : ~3);
					int mod = (int)((len / (formatIsR64(format) ? sizeof(deUint64) : sizeof(deUint32))) % numComponents);
					string sstoreValue = de::toString(storeValue);
					switch (mod)
					{
					case 0:
						break;
					case 1:
						expectedOOB2 = vecType + "(expectedIB2.x, 0, 0, 0)";
						expectedOOB3 = vecType + "(" + sstoreValue + ", 0, 0, 0)";
						break;
					case 2:
						expectedOOB2 = vecType + "(expectedIB2.xy, 0, 0)";
						expectedOOB3 = vecType + "(" + sstoreValue + ", " + sstoreValue + ", 0, 0)";
						break;
					case 3:
						expectedOOB2 = vecType + "(expectedIB2.xyz, 0)";
						expectedOOB3 = vecType + "(" + sstoreValue + ", " + sstoreValue + ", " + sstoreValue + ", 0)";
						break;
					}
				}

				// Entirely in-bounds.
				checks << "    if (c >= 0 && c < inboundcoords) {\n"
						  "       if (temp == expectedIB2) temp = " << vecType << "(0); else temp = " << vecType << "(1);\n"
						  "    }\n";

				// normal out-of-bounds value
				if (m_data.testRobustness2)
					checks << "    else if (temp == " << expectedOOB << ") temp = " << vecType << "(0);\n";
				else
					// image_robustness relaxes alpha which is allowed to be zero or one
					checks << "    else if (temp == zzzz || temp == zzzo) temp = " << vecType << "(0);\n";

				if (m_data.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
					m_data.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
				{
					checks << "    else if (c >= 0 && c < paddedinboundcoords && temp == expectedIB2) temp = " << vecType << "(0);\n";
				}

				// null descriptor loads with image format layout qualifier that doesn't include alpha may return alpha=1
				if (m_data.nullDescriptor && m_data.formatQualifier &&
					(m_data.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE || m_data.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER) &&
					numComponents < 4)
					checks << "    else if (temp == zzzo) temp = " << vecType << "(0);\n";

				// non-volatile value replaced with stored value
				if (supportsStores(m_data.descriptorType) && !m_data.vol)
					checks << "    else if (temp == " << getStoreValue(m_data.descriptorType, numComponents, vecType, bufType) << ") temp = " << vecType << "(0);\n";

				// value straddling the boundary, returning a partial vector
				if (expectedOOB2 != expectedOOB)
					checks << "    else if (c == inboundcoords && temp == " << expectedOOB2 << ") temp = " << vecType << "(0);\n";
				if (expectedOOB3 != expectedOOB)
					checks << "    else if (c == inboundcoords && temp == " << expectedOOB3 << ") temp = " << vecType << "(0);\n";

				// failure
				checks << "    else temp = " << vecType << "(1);\n";
			}
			// Accumulate any incorrect values.
			checks << "    accum += abs(temp);\n";

			// Only the full robustness2 extension provides guarantees about out-of-bounds mip levels.
			if (m_data.testRobustness2 && m_data.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER && m_data.samples == VK_SAMPLE_COUNT_1_BIT)
			{
				// Fetch from an out of bounds mip level. Expect this to always return the OOB value.
				string coord0 = genCoord("0", numCoords, m_data.samples, i);
				checks << "    if (c != 0) temp = " << genFetch(m_data, numComponents, vecType, coord0, "c") << "; else temp = " << vecType << "(0);\n";
				checks << "    if (c != 0) temp -= " << expectedOOB << ";\n";
				checks << "    accum += abs(temp);\n";
			}
		}
		if (m_data.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER &&
			m_data.samples == VK_SAMPLE_COUNT_1_BIT)
		{
			string coordNorm = genCoordNorm(m_data, "(c+0.25)", numCoords, numNormalizedCoords, i);

			checks << "    expectedIB2 = expectedIB;\n";

			// Data is a function of layer, for array images. Subtract out the layer value for in-bounds coordinates.
			if (dataDependsOnLayer && i == numNormalizedCoords)
			{
				checks << "    clampedLayer = clamp(c, 0, " << coordDim-1 << ");\n";
				checks << "    expectedIB2 += " << vecType << "(clampedLayer, 0, 0, 0);\n";
			}

			stringstream normexpected;
			// Cubemap fetches are always in-bounds. Layer coordinate is clamped, so is always in-bounds.
			if (m_data.viewType == VK_IMAGE_VIEW_TYPE_CUBE ||
				m_data.viewType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY ||
				(layered && i == numCoords-1))
				normexpected << "    temp -= expectedIB2;\n";
			else
			{
				normexpected << "    if (c >= 0 && c < inboundcoords)\n";
				normexpected << "        temp -= expectedIB2;\n";
				normexpected << "    else\n";
				if (m_data.testRobustness2)
					normexpected << "        temp -= " << expectedOOB << ";\n";
				else
					// image_robustness relaxes alpha which is allowed to be zero or one
					normexpected << "        temp = " << vecType << "((temp == zzzz || temp == zzzo) ? 0 : 1);\n";
			}

			checks << "    temp = texture(texture0_1, " << coordNorm << ");\n";
			checks << normexpected.str();
			checks << "    accum += abs(temp);\n";
			checks << "    temp = textureLod(texture0_1, " << coordNorm << ", 0.0f);\n";
			checks << normexpected.str();
			checks << "    accum += abs(temp);\n";
			checks << "    temp = textureGrad(texture0_1, " << coordNorm << ", " << genCoord("1.0", numNormalizedCoords, m_data.samples, i) << ", " << genCoord("1.0", numNormalizedCoords, m_data.samples, i) << ");\n";
			checks << normexpected.str();
			checks << "    accum += abs(temp);\n";
		}
		if (m_data.nullDescriptor)
		{
			const char *sizeswiz;
			switch (m_data.viewType)
			{
				default: DE_ASSERT(0); // Fallthrough
				case VK_IMAGE_VIEW_TYPE_1D:			sizeswiz = ".xxxx";	break;
				case VK_IMAGE_VIEW_TYPE_1D_ARRAY:	sizeswiz = ".xyxx";	break;
				case VK_IMAGE_VIEW_TYPE_2D:			sizeswiz = ".xyxx";	break;
				case VK_IMAGE_VIEW_TYPE_2D_ARRAY:	sizeswiz = ".xyzx";	break;
				case VK_IMAGE_VIEW_TYPE_3D:			sizeswiz = ".xyzx";	break;
				case VK_IMAGE_VIEW_TYPE_CUBE:		sizeswiz = ".xyxx";	break;
				case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:	sizeswiz = ".xyzx";	break;
			}
			if (m_data.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
			{
				if (m_data.samples == VK_SAMPLE_COUNT_1_BIT)
				{
					checks << "    temp = textureSize(texture0_1, 0)" << sizeswiz <<";\n";
					checks << "    accum += abs(temp);\n";
				}
				else
				{
					checks << "    temp = textureSize(texture0_1)" << sizeswiz <<";\n";
					checks << "    accum += abs(temp);\n";
					checks << "    temp = textureSamples(texture0_1).xxxx;\n";
					checks << "    accum += abs(temp);\n";
				}
			}
			if (m_data.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
			{
				if (m_data.samples == VK_SAMPLE_COUNT_1_BIT)
				{
					checks << "    temp = imageSize(image0_1)" << sizeswiz <<";\n";
					checks << "    accum += abs(temp);\n";
				}
				else
				{
					checks << "    temp = imageSize(image0_1)" << sizeswiz <<";\n";
					checks << "    accum += abs(temp);\n";
					checks << "    temp = imageSamples(image0_1).xxxx;\n";
					checks << "    accum += abs(temp);\n";
				}
			}
			if (m_data.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
				m_data.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
			{
				// expect zero for runtime-sized array .length()
				checks << "    temp = " << vecType << "(ssbo0_1_unsized.val.length());\n";
				checks << "    accum += abs(temp);\n";
				checks << "    temp = " << vecType << "(ssbo0_1_unsized_pad.val.length());\n";
				checks << "    accum += abs(temp);\n";
			}
		}
	}
	checks << "  }\n";

	const bool is64BitFormat = formatIsR64(m_data.format);
	std::string SupportR64 = (is64BitFormat ?
							std::string("#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require\n"
							"#extension GL_EXT_shader_image_int64 : require\n") :
							std::string());

	switch (m_data.stage)
	{
	default: DE_ASSERT(0); // Fallthrough
	case STAGE_COMPUTE:
		{
			std::stringstream css;
			css <<
				"#version 450 core\n"
				"#extension GL_EXT_nonuniform_qualifier : enable\n"
				"#extension GL_EXT_scalar_block_layout : enable\n"
				"#extension GL_EXT_samplerless_texture_functions : enable\n"
				"#extension GL_EXT_control_flow_attributes : enable\n"
				"#extension GL_EXT_shader_image_load_formatted : enable\n"
				<< SupportR64
				<< decls.str() <<
				"layout(local_size_x = 1, local_size_y = 1) in;\n"
				"void main()\n"
				"{\n"
				"  " << vecType << " accum = " << vecType << "(0);\n"
				"  " << vecType << " temp;\n"
				<< checks.str() <<
				"  " << vecType << " color = (accum != " << vecType << "(0)) ? " << vecType << "(0,0,0,0) : " << vecType << "(1,0,0,1);\n"
				"  imageStore(image0_0, ivec2(gl_GlobalInvocationID.xy), color);\n"
				"}\n";

			programCollection.glslSources.add("test") << glu::ComputeSource(css.str())
				<< vk::ShaderBuildOptions(programCollection.usedVulkanVersion, is64BitFormat ? vk::SPIRV_VERSION_1_3 : vk::SPIRV_VERSION_1_0, vk::ShaderBuildOptions::FLAG_ALLOW_SCALAR_OFFSETS);
			break;
		}
	case STAGE_RAYGEN:
	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_samplerless_texture_functions : enable\n"
			"#extension GL_EXT_scalar_block_layout : enable\n"
			"#extension GL_EXT_nonuniform_qualifier : enable\n"
			"#extension GL_EXT_control_flow_attributes : enable\n"
			"#extension GL_NV_ray_tracing : require\n"
			"#extension GL_EXT_shader_image_load_formatted : enable\n"
			<< SupportR64
			<< decls.str() <<
			"void main()\n"
			"{\n"
			"  " << vecType << " accum = " << vecType << "(0);\n"
			"  " << vecType << " temp;\n"
			<< checks.str() <<
			"  " << vecType << " color = (accum != " << vecType << "(0)) ? " << vecType << "(0,0,0,0) : " << vecType << "(1,0,0,1);\n"
			"  imageStore(image0_0, ivec2(gl_LaunchIDNV.xy), color);\n"
			"}\n";

		programCollection.glslSources.add("test") << glu::RaygenSource(css.str())
			<< vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_0, vk::ShaderBuildOptions::FLAG_ALLOW_SCALAR_OFFSETS);
		break;
	}
	case STAGE_VERTEX:
		{
			std::stringstream vss;
			vss <<
				"#version 450 core\n"
				"#extension GL_EXT_samplerless_texture_functions : enable\n"
				"#extension GL_EXT_scalar_block_layout : enable\n"
				"#extension GL_EXT_nonuniform_qualifier : enable\n"
				"#extension GL_EXT_control_flow_attributes : enable\n"
				"#extension GL_EXT_shader_image_load_formatted : enable\n"
				<< SupportR64
				<< decls.str() <<
				"void main()\n"
				"{\n"
				"  " << vecType << " accum = " << vecType << "(0);\n"
				"  " << vecType << " temp;\n"
				<< checks.str() <<
				"  " << vecType << " color = (accum != " << vecType << "(0)) ? " << vecType << "(0,0,0,0) : " << vecType << "(1,0,0,1);\n"
				"  imageStore(image0_0, ivec2(gl_VertexIndex % " << DIM << ", gl_VertexIndex / " << DIM << "), color);\n"
				"  gl_PointSize = 1.0f;\n"
				"  gl_Position = vec4(0.0f, 0.0f, 0.0f, 1.0f);\n"
				"}\n";

			programCollection.glslSources.add("test") << glu::VertexSource(vss.str())
				<< vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_0, vk::ShaderBuildOptions::FLAG_ALLOW_SCALAR_OFFSETS);
			break;
		}
	case STAGE_FRAGMENT:
		{
			std::stringstream vss;
			vss <<
				"#version 450 core\n"
				"void main()\n"
				"{\n"
				// full-viewport quad
				"  gl_Position = vec4( 2.0*float(gl_VertexIndex&2) - 1.0, 4.0*(gl_VertexIndex&1)-1.0, 1.0 - 2.0 * float(gl_VertexIndex&1), 1);\n"
				"}\n";

			programCollection.glslSources.add("vert") << glu::VertexSource(vss.str())
				<< vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_0, vk::ShaderBuildOptions::FLAG_ALLOW_SCALAR_OFFSETS);

			std::stringstream fss;
			fss <<
				"#version 450 core\n"
				"#extension GL_EXT_samplerless_texture_functions : enable\n"
				"#extension GL_EXT_scalar_block_layout : enable\n"
				"#extension GL_EXT_nonuniform_qualifier : enable\n"
				"#extension GL_EXT_control_flow_attributes : enable\n"
				"#extension GL_EXT_shader_image_load_formatted : enable\n"
				<< SupportR64
				<< decls.str() <<
				"void main()\n"
				"{\n"
				"  " << vecType << " accum = " << vecType << "(0);\n"
				"  " << vecType << " temp;\n"
				<< checks.str() <<
				"  " << vecType << " color = (accum != " << vecType << "(0)) ? " << vecType << "(0,0,0,0) : " << vecType << "(1,0,0,1);\n"
				"  imageStore(image0_0, ivec2(gl_FragCoord.x, gl_FragCoord.y), color);\n"
				"}\n";

			programCollection.glslSources.add("test") << glu::FragmentSource(fss.str())
				<< vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_0, vk::ShaderBuildOptions::FLAG_ALLOW_SCALAR_OFFSETS);
			break;
		}
	}

	// The 64-bit conditions below are redundant. Can we support the below shader for other than 64-bit formats?
	if ((m_data.samples > VK_SAMPLE_COUNT_1_BIT) && is64BitFormat)
	{
		const std::string	ivecCords = (m_data.viewType == VK_IMAGE_VIEW_TYPE_2D ? "ivec2(gx, gy)" : "ivec3(gx, gy, gz)");
		std::stringstream	fillShader;

		fillShader <<
			"#version 450\n"
			<< SupportR64
			<< "\n"
			"layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
			"layout (" + getShaderImageFormatQualifier(mapVkFormat(m_data.format)) + ", binding=0) volatile uniform "
			<< string(formatIsSignedInt(m_data.format) ? "i" : "u") + string(is64BitFormat ? "64" : "") << "image" << imageDim << +" u_resultImage;\n"
			"\n"
			"layout(std430, binding = 1) buffer inputBuffer\n"
			"{\n"
			"  int" << (is64BitFormat ? "64_t" : "") << " data[];\n"
			"} inBuffer;\n"
			"\n"
			"void main(void)\n"
			"{\n"
			"  int gx = int(gl_GlobalInvocationID.x);\n"
			"  int gy = int(gl_GlobalInvocationID.y);\n"
			"  int gz = int(gl_GlobalInvocationID.z);\n"
			"  uint index = gx + (gy * gl_NumWorkGroups.x) + (gz *gl_NumWorkGroups.x * gl_NumWorkGroups.y);\n";

			for(int ndx = 0; ndx < static_cast<int>(m_data.samples); ++ndx)
			{
				fillShader << "  imageStore(u_resultImage, " << ivecCords << ", " << ndx << ", i64vec4(inBuffer.data[index]));\n";
			}

			fillShader << "}\n";

		programCollection.glslSources.add("fillShader") << glu::ComputeSource(fillShader.str())
			<< vk::ShaderBuildOptions(programCollection.usedVulkanVersion, is64BitFormat ? vk::SPIRV_VERSION_1_3 : vk::SPIRV_VERSION_1_0, vk::ShaderBuildOptions::FLAG_ALLOW_SCALAR_OFFSETS);
	}

}

VkImageType imageViewTypeToImageType (VkImageViewType type)
{
	switch (type)
	{
		case VK_IMAGE_VIEW_TYPE_1D:
		case VK_IMAGE_VIEW_TYPE_1D_ARRAY:		return VK_IMAGE_TYPE_1D;
		case VK_IMAGE_VIEW_TYPE_2D:
		case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
		case VK_IMAGE_VIEW_TYPE_CUBE:
		case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:		return VK_IMAGE_TYPE_2D;
		case VK_IMAGE_VIEW_TYPE_3D:				return VK_IMAGE_TYPE_3D;
		default:
			DE_ASSERT(false);
	}

	return VK_IMAGE_TYPE_2D;
}

TestInstance* RobustnessExtsTestCase::createInstance (Context& context) const
{
	return new RobustnessExtsTestInstance(context, m_data);
}

tcu::TestStatus RobustnessExtsTestInstance::iterate (void)
{
	const InstanceInterface&	vki					= m_context.getInstanceInterface();
	const VkDevice				device				= getLogicalDevice(m_context, m_data);
	const DeviceDriver			vk					(m_context.getPlatformInterface(), m_context.getInstance(), device);
	const VkPhysicalDevice		physicalDevice		= m_context.getPhysicalDevice();
	SimpleAllocator				allocator			(vk, device, getPhysicalDeviceMemoryProperties(vki, physicalDevice));

	Layout layout;
	generateLayout(layout, m_data);

	// Get needed properties.
	VkPhysicalDeviceProperties2 properties;
	deMemset(&properties, 0, sizeof(properties));
	properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	void** pNextTail = &properties.pNext;

	VkPhysicalDeviceRayTracingPropertiesNV rayTracingProperties;
	deMemset(&rayTracingProperties, 0, sizeof(rayTracingProperties));
	rayTracingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PROPERTIES_NV;

	VkPhysicalDeviceRobustness2PropertiesEXT robustness2Properties;
	deMemset(&robustness2Properties, 0, sizeof(robustness2Properties));
	robustness2Properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_PROPERTIES_EXT;

	if (m_context.isDeviceFunctionalitySupported("VK_NV_ray_tracing"))
	{
		*pNextTail = &rayTracingProperties;
		pNextTail = &rayTracingProperties.pNext;
	}

	if (m_context.isDeviceFunctionalitySupported("VK_EXT_robustness2"))
	{
		*pNextTail = &robustness2Properties;
		pNextTail = &robustness2Properties.pNext;
	}

	vki.getPhysicalDeviceProperties2(physicalDevice, &properties);

	if (m_data.testRobustness2)
	{
		if (robustness2Properties.robustStorageBufferAccessSizeAlignment != 1 &&
			robustness2Properties.robustStorageBufferAccessSizeAlignment != 4)
			return tcu::TestStatus(QP_TEST_RESULT_FAIL, "robustStorageBufferAccessSizeAlignment must be 1 or 4");

		if (robustness2Properties.robustUniformBufferAccessSizeAlignment < 1 ||
			robustness2Properties.robustUniformBufferAccessSizeAlignment > 256 ||
			!deIntIsPow2((int)robustness2Properties.robustUniformBufferAccessSizeAlignment))
			return tcu::TestStatus(QP_TEST_RESULT_FAIL, "robustUniformBufferAccessSizeAlignment must be a power of two in [1,256]");
	}

	VkPipelineBindPoint bindPoint;

	switch (m_data.stage)
	{
	case STAGE_COMPUTE:
		bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
		break;
	case STAGE_RAYGEN:
		bindPoint = VK_PIPELINE_BIND_POINT_RAY_TRACING_NV;
		break;
	default:
		bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		break;
	}

	Move<vk::VkDescriptorSetLayout>	descriptorSetLayout;
	Move<vk::VkDescriptorPool>		descriptorPool;
	Move<vk::VkDescriptorSet>		descriptorSet;

	int formatBytes = tcu::getPixelSize(mapVkFormat(m_data.format));
	int numComponents = formatBytes / tcu::getChannelSize(mapVkFormat(m_data.format).type);

	vector<VkDescriptorSetLayoutBinding> &bindings = layout.layoutBindings;

	VkDescriptorPoolCreateFlags poolCreateFlags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	VkDescriptorSetLayoutCreateFlags layoutCreateFlags = m_data.pushDescriptor ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR : 0;

	// Create a layout and allocate a descriptor set for it.

	const VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		DE_NULL,

		layoutCreateFlags,
		(deUint32)bindings.size(),
		bindings.empty() ? DE_NULL : bindings.data()
	};

	descriptorSetLayout = vk::createDescriptorSetLayout(vk, device, &setLayoutCreateInfo);

	vk::DescriptorPoolBuilder poolBuilder;
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1);
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1);
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1);
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1);
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1);
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1);
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1);
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2);

	descriptorPool = poolBuilder.build(vk, device, poolCreateFlags, 1u, DE_NULL);

	const void *pNext = DE_NULL;

	if (!m_data.pushDescriptor)
		descriptorSet = makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout, pNext);

	de::MovePtr<BufferWithMemory> buffer;

	deUint8 *bufferPtr = DE_NULL;
	if (!m_data.nullDescriptor)
	{
		// Create a buffer to hold data for all descriptors.
		VkDeviceSize	size = de::max(
			(VkDeviceSize)(m_data.bufferLen ? m_data.bufferLen : 1),
			(VkDeviceSize)256);

		if (m_data.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
			m_data.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
		{
			size = deIntRoundToPow2((int)size, (int)robustness2Properties.robustUniformBufferAccessSizeAlignment);
		}

		if (m_data.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
			m_data.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
		{
			size = deIntRoundToPow2((int)size, (int)robustness2Properties.robustStorageBufferAccessSizeAlignment);
		}

		if (m_data.descriptorType == VERTEX_ATTRIBUTE_FETCH)
			size = m_data.bufferLen;

		buffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
			vk, device, allocator, makeBufferCreateInfo(size,
														VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
														VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
														VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT |
														VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT |
														VK_BUFFER_USAGE_VERTEX_BUFFER_BIT),
														MemoryRequirement::HostVisible));
		bufferPtr = (deUint8 *)buffer->getAllocation().getHostPtr();

		deMemset(bufferPtr, 0x3f, (size_t)size);

		deMemset(bufferPtr, 0, m_data.bufferLen);
		if (m_data.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
			m_data.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
		{
			deMemset(bufferPtr, 0, deIntRoundToPow2(m_data.bufferLen, (int)robustness2Properties.robustUniformBufferAccessSizeAlignment));
		}
		if (m_data.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
			m_data.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
		{
			deMemset(bufferPtr, 0, deIntRoundToPow2(m_data.bufferLen, (int)robustness2Properties.robustStorageBufferAccessSizeAlignment));
		}
	}

	const deUint32 queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();

	Move<VkDescriptorSetLayout>		descriptorSetLayoutR64;
	Move<VkDescriptorPool>			descriptorPoolR64;
	Move<VkDescriptorSet>			descriptorSetFillImage;
	Move<VkShaderModule>			shaderModuleFillImage;
	Move<VkPipelineLayout>			pipelineLayoutFillImage;
	Move<VkPipeline>				pipelineFillImage;

	Move<VkCommandPool>				cmdPool		= createCommandPool(vk, device, 0, queueFamilyIndex);
	Move<VkCommandBuffer>			cmdBuffer	= allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	VkQueue							queue;

	vk.getDeviceQueue(device, queueFamilyIndex, 0, &queue);

	const VkImageSubresourceRange	barrierRange				=
	{
		VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
		0u,							// deUint32				baseMipLevel;
		VK_REMAINING_MIP_LEVELS,	// deUint32				levelCount;
		0u,							// deUint32				baseArrayLayer;
		VK_REMAINING_ARRAY_LAYERS	// deUint32				layerCount;
	};

	VkImageMemoryBarrier			preImageBarrier				=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,				// VkStructureType		sType
		DE_NULL,											// const void*			pNext
		0u,													// VkAccessFlags		srcAccessMask
		VK_ACCESS_TRANSFER_WRITE_BIT,						// VkAccessFlags		dstAccessMask
		VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout		oldLayout
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,				// VkImageLayout		newLayout
		VK_QUEUE_FAMILY_IGNORED,							// uint32_t				srcQueueFamilyIndex
		VK_QUEUE_FAMILY_IGNORED,							// uint32_t				dstQueueFamilyIndex
		DE_NULL,											// VkImage				image
		barrierRange,										// VkImageSubresourceRange	subresourceRange;
	};

	VkImageMemoryBarrier			postImageBarrier			=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			srcAccessMask;
		VK_ACCESS_SHADER_READ_BIT,					// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_GENERAL,					// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
		DE_NULL,									// VkImage					image;
		barrierRange,								// VkImageSubresourceRange	subresourceRange;
	};

	vk::VkClearColorValue			clearValue;
	clearValue.uint32[0] = 0u;
	clearValue.uint32[1] = 0u;
	clearValue.uint32[2] = 0u;
	clearValue.uint32[3] = 0u;

	beginCommandBuffer(vk, *cmdBuffer, 0u);

	typedef vk::Unique<vk::VkBufferView>		BufferViewHandleUp;
	typedef de::SharedPtr<BufferViewHandleUp>	BufferViewHandleSp;
	typedef de::SharedPtr<ImageWithMemory>		ImageWithMemorySp;
	typedef de::SharedPtr<Unique<VkImageView> >	VkImageViewSp;
	typedef de::MovePtr<BufferWithMemory>		BufferWithMemoryMp;

	vector<BufferViewHandleSp>					bufferViews(1);

	VkImageCreateFlags imageCreateFlags = 0;
	if (m_data.viewType == VK_IMAGE_VIEW_TYPE_CUBE || m_data.viewType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)
		imageCreateFlags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

	const bool featureSampledImage = ((getPhysicalDeviceFormatProperties(m_context.getInstanceInterface(),
										m_context.getPhysicalDevice(),
										m_data.format).optimalTilingFeatures &
										VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) == VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);

	const VkImageUsageFlags usageSampledImage = (featureSampledImage ? VK_IMAGE_USAGE_SAMPLED_BIT : (VkImageUsageFlagBits)0);

	const VkImageCreateInfo			outputImageCreateInfo			=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType;
		DE_NULL,								// const void*				pNext;
		(VkImageCreateFlags)0u,					// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,						// VkImageType				imageType;
		m_data.format,							// VkFormat					format;
		{
			DIM,								// deUint32	width;
			DIM,								// deUint32	height;
			1u									// deUint32	depth;
		},										// VkExtent3D				extent;
		1u,										// deUint32					mipLevels;
		1u,										// deUint32					arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling			tiling;
		VK_IMAGE_USAGE_STORAGE_BIT
		| usageSampledImage
		| VK_IMAGE_USAGE_TRANSFER_SRC_BIT
		| VK_IMAGE_USAGE_TRANSFER_DST_BIT,		// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode;
		0u,										// deUint32					queueFamilyIndexCount;
		DE_NULL,								// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED				// VkImageLayout			initialLayout;
	};

	deUint32 width = m_data.imageDim[0];
	deUint32 height = m_data.viewType != VK_IMAGE_VIEW_TYPE_1D && m_data.viewType != VK_IMAGE_VIEW_TYPE_1D_ARRAY ? m_data.imageDim[1] : 1;
	deUint32 depth = m_data.viewType == VK_IMAGE_VIEW_TYPE_3D ? m_data.imageDim[2] : 1;
	deUint32 layers = m_data.viewType == VK_IMAGE_VIEW_TYPE_1D_ARRAY ? m_data.imageDim[1] :
						m_data.viewType != VK_IMAGE_VIEW_TYPE_1D &&
						m_data.viewType != VK_IMAGE_VIEW_TYPE_2D &&
						m_data.viewType != VK_IMAGE_VIEW_TYPE_3D ? m_data.imageDim[2] : 1;

	const VkImageUsageFlags usageImage = (m_data.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ? VK_IMAGE_USAGE_STORAGE_BIT : (VkImageUsageFlagBits)0);

	const VkImageCreateInfo			imageCreateInfo			=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType;
		DE_NULL,								// const void*				pNext;
		imageCreateFlags,						// VkImageCreateFlags		flags;
		imageViewTypeToImageType(m_data.viewType),	// VkImageType				imageType;
		m_data.format,							// VkFormat					format;
		{
			width,								// deUint32	width;
			height,								// deUint32	height;
			depth								// deUint32	depth;
		},										// VkExtent3D				extent;
		1u,										// deUint32					mipLevels;
		layers,									// deUint32					arrayLayers;
		m_data.samples,							// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling			tiling;
		usageImage
		| usageSampledImage
		| VK_IMAGE_USAGE_TRANSFER_SRC_BIT
		| VK_IMAGE_USAGE_TRANSFER_DST_BIT,		// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode;
		0u,										// deUint32					queueFamilyIndexCount;
		DE_NULL,								// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED				// VkImageLayout			initialLayout;
	};

	VkImageViewCreateInfo		imageViewCreateInfo		=
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		(VkImageViewCreateFlags)0u,					// VkImageViewCreateFlags	flags;
		DE_NULL,									// VkImage					image;
		VK_IMAGE_VIEW_TYPE_2D,						// VkImageViewType			viewType;
		m_data.format,								// VkFormat					format;
		{
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY
		},											// VkComponentMapping		 components;
		{
			VK_IMAGE_ASPECT_COLOR_BIT,				// VkImageAspectFlags	aspectMask;
			0u,										// deUint32				baseMipLevel;
			VK_REMAINING_MIP_LEVELS,				// deUint32				levelCount;
			0u,										// deUint32				baseArrayLayer;
			VK_REMAINING_ARRAY_LAYERS				// deUint32				layerCount;
		}											// VkImageSubresourceRange	subresourceRange;
	};

	vector<ImageWithMemorySp> images(2);
	vector<VkImageViewSp> imageViews(2);

	if (m_data.descriptorType == VERTEX_ATTRIBUTE_FETCH)
	{
		deUint32 *ptr = (deUint32 *)bufferPtr;
		deMemcpy(ptr, layout.refData.data(), layout.refData.size());
	}

	BufferWithMemoryMp				bufferImageR64;
	BufferWithMemoryMp				bufferOutputImageR64;
	const VkDeviceSize				sizeOutputR64	= 8 * outputImageCreateInfo.extent.width * outputImageCreateInfo.extent.height * outputImageCreateInfo.extent.depth;
	const VkDeviceSize				sizeOneLayers	= 8 * imageCreateInfo.extent.width * imageCreateInfo.extent.height * imageCreateInfo.extent.depth;
	const VkDeviceSize				sizeImageR64	= sizeOneLayers * layers;

	if (formatIsR64(m_data.format))
	{
		bufferOutputImageR64 = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
			vk, device, allocator,
			makeBufferCreateInfo(sizeOutputR64, VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
			MemoryRequirement::HostVisible));

		deUint64* bufferUint64Ptr = (deUint64 *)bufferOutputImageR64->getAllocation().getHostPtr();

		for (int ndx = 0; ndx < static_cast<int>(sizeOutputR64 / 8); ++ndx)
		{
			bufferUint64Ptr[ndx] = 0;
		}
		flushAlloc(vk, device, bufferOutputImageR64->getAllocation());

		bufferImageR64 = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
			vk, device, allocator,
			makeBufferCreateInfo(sizeImageR64, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
			MemoryRequirement::HostVisible));

		for (deUint32 layerNdx = 0; layerNdx < layers; ++layerNdx)
		{
			bufferUint64Ptr = (deUint64 *)bufferImageR64->getAllocation().getHostPtr();
			bufferUint64Ptr = bufferUint64Ptr + ((sizeOneLayers * layerNdx) / 8);

			for (int ndx = 0; ndx < static_cast<int>(sizeOneLayers / 8); ++ndx)
			{
				bufferUint64Ptr[ndx] = 0x1234567887654321 + ((m_data.viewType != VK_IMAGE_VIEW_TYPE_CUBE && m_data.viewType != VK_IMAGE_VIEW_TYPE_CUBE_ARRAY) ? layerNdx : 0);
			}
		}
		flushAlloc(vk, device, bufferImageR64->getAllocation());
	}

	for (size_t b = 0; b < bindings.size(); ++b)
	{
		VkDescriptorSetLayoutBinding &binding = bindings[b];

		if (binding.descriptorCount == 0)
			continue;
		if (b == 1 && m_data.nullDescriptor)
			continue;

		DE_ASSERT(binding.descriptorCount == 1);
		switch (binding.descriptorType)
		{
		default: DE_ASSERT(0); // Fallthrough
		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
			{
				deUint32 *ptr = (deUint32 *)bufferPtr;
				deMemcpy(ptr, layout.refData.data(), layout.refData.size());
			}
			break;
		case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
		case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
			{
				deUint32 *ptr = (deUint32 *)bufferPtr;
				deMemcpy(ptr, layout.refData.data(), layout.refData.size());

				const vk::VkBufferViewCreateInfo viewCreateInfo =
				{
					vk::VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,
					DE_NULL,
					(vk::VkBufferViewCreateFlags)0,
					**buffer,								// buffer
					m_data.format,							// format
					(vk::VkDeviceSize)0,					// offset
					(vk::VkDeviceSize)m_data.bufferLen		// range
				};
				vk::Move<vk::VkBufferView> bufferView = vk::createBufferView(vk, device, &viewCreateInfo);
				bufferViews[0] = BufferViewHandleSp(new BufferViewHandleUp(bufferView));
			}
			break;
		case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
		case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
			{
				if (bindings.size() > 1 &&
					bindings[1].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
				{
					if (m_data.format == VK_FORMAT_R64_SINT)
						imageViewCreateInfo.format = VK_FORMAT_R32G32_SINT;

					if (m_data.format == VK_FORMAT_R64_UINT)
						imageViewCreateInfo.format = VK_FORMAT_R32G32_UINT;
				}

				if (b == 0)
				{
					images[b] = ImageWithMemorySp(new ImageWithMemory(vk, device, allocator, outputImageCreateInfo, MemoryRequirement::Any));
					imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
				}
				else
				{
					images[b] = ImageWithMemorySp(new ImageWithMemory(vk, device, allocator, imageCreateInfo, MemoryRequirement::Any));
					imageViewCreateInfo.viewType = m_data.viewType;
				}
				imageViewCreateInfo.image = **images[b];
				imageViews[b] = VkImageViewSp(new Unique<VkImageView>(createImageView(vk, device, &imageViewCreateInfo, NULL)));

				VkImage						img			= **images[b];
				const VkBuffer&				bufferR64= ((b == 0) ? *(*bufferOutputImageR64) : *(*(bufferImageR64)));
				const VkImageCreateInfo&	imageInfo	= ((b == 0) ? outputImageCreateInfo : imageCreateInfo);
				const deUint32				clearLayers	= b == 0 ? 1 : layers;

				if (!formatIsR64(m_data.format))
				{
					preImageBarrier.image	= img;
					if (b == 1)
					{
						if (formatIsFloat(m_data.format))
						{
							deMemcpy(&clearValue.float32[0], layout.refData.data(), layout.refData.size());
						}
						else if (formatIsSignedInt(m_data.format))
						{
							deMemcpy(&clearValue.int32[0], layout.refData.data(), layout.refData.size());
						}
						else
						{
							deMemcpy(&clearValue.uint32[0], layout.refData.data(), layout.refData.size());
						}
					}
					postImageBarrier.image	= img;

					vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &preImageBarrier);

					for (unsigned int i = 0; i < clearLayers; ++i)
					{
						const VkImageSubresourceRange	clearRange				=
						{
							VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
							0u,							// deUint32				baseMipLevel;
							VK_REMAINING_MIP_LEVELS,	// deUint32				levelCount;
							i,							// deUint32				baseArrayLayer;
							1							// deUint32				layerCount;
						};

						vk.cmdClearColorImage(*cmdBuffer, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1, &clearRange);

						// Use same data for all faces for cube(array), otherwise make value a function of the layer
						if (m_data.viewType != VK_IMAGE_VIEW_TYPE_CUBE && m_data.viewType != VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)
						{
							if (formatIsFloat(m_data.format))
								clearValue.float32[0] += 1;
							else if (formatIsSignedInt(m_data.format))
								clearValue.int32[0] += 1;
							else
								clearValue.uint32[0] += 1;
						}
					}
					vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &postImageBarrier);
				}
				else
				{
					if ((m_data.samples > VK_SAMPLE_COUNT_1_BIT) && (b == 1))
					{
						const VkImageSubresourceRange	subresourceRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, clearLayers);
						const VkImageMemoryBarrier		imageBarrierPre		= makeImageMemoryBarrier(0,
																				VK_ACCESS_SHADER_WRITE_BIT,
																				VK_IMAGE_LAYOUT_UNDEFINED,
																				VK_IMAGE_LAYOUT_GENERAL,
																				img,
																				subresourceRange);
						const VkImageMemoryBarrier		imageBarrierPost	= makeImageMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT,
																				VK_ACCESS_SHADER_READ_BIT,
																				VK_IMAGE_LAYOUT_GENERAL,
																				VK_IMAGE_LAYOUT_GENERAL,
																				img,
																				subresourceRange);

						descriptorSetLayoutR64 =
							DescriptorSetLayoutBuilder()
							.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
							.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
							.build(vk, device);

						descriptorPoolR64 =
							DescriptorPoolBuilder()
							.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1)
							.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1)
							.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 2u);

						descriptorSetFillImage = makeDescriptorSet(vk,
							device,
							*descriptorPoolR64,
							*descriptorSetLayoutR64);

						shaderModuleFillImage	= createShaderModule(vk, device, m_context.getBinaryCollection().get("fillShader"), 0);
						pipelineLayoutFillImage	= makePipelineLayout(vk, device, *descriptorSetLayoutR64);
						pipelineFillImage		= makeComputePipeline(vk, device, *pipelineLayoutFillImage, *shaderModuleFillImage);

						const VkDescriptorImageInfo		descResultImageInfo		= makeDescriptorImageInfo(DE_NULL, **imageViews[b], VK_IMAGE_LAYOUT_GENERAL);
						const VkDescriptorBufferInfo	descResultBufferInfo	= makeDescriptorBufferInfo(bufferR64, 0, sizeImageR64);

						DescriptorSetUpdateBuilder()
							.writeSingle(*descriptorSetFillImage, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descResultImageInfo)
							.writeSingle(*descriptorSetFillImage, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descResultBufferInfo)
							.update(vk, device);

						vk.cmdPipelineBarrier(*cmdBuffer,
							VK_PIPELINE_STAGE_HOST_BIT,
							VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
							(VkDependencyFlags)0,
							0, (const VkMemoryBarrier*)DE_NULL,
							0, (const VkBufferMemoryBarrier*)DE_NULL,
							1, &imageBarrierPre);

						vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineFillImage);
						vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayoutFillImage, 0u, 1u, &(*descriptorSetFillImage), 0u, DE_NULL);

						vk.cmdDispatch(*cmdBuffer, imageInfo.extent.width, imageInfo.extent.height, clearLayers);

						vk.cmdPipelineBarrier(*cmdBuffer,
									VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
									VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
									(VkDependencyFlags)0,
									0, (const VkMemoryBarrier*)DE_NULL,
									0, (const VkBufferMemoryBarrier*)DE_NULL,
									1, &imageBarrierPost);
					}
					else
					{
						VkDeviceSize					size			= ((b == 0) ? sizeOutputR64 : sizeImageR64);
						const vector<VkBufferImageCopy>	bufferImageCopy	(1, makeBufferImageCopy(imageInfo.extent, makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, clearLayers)));

						copyBufferToImage(vk,
							*cmdBuffer,
							bufferR64,
							size,
							bufferImageCopy,
							VK_IMAGE_ASPECT_COLOR_BIT,
							1,
							clearLayers, img, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);
					}
				}
			}
			break;
		}
	}

	const VkSamplerCreateInfo	samplerParams	=
	{
		VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,		// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		0,											// VkSamplerCreateFlags		flags;
		VK_FILTER_NEAREST,							// VkFilter					magFilter:
		VK_FILTER_NEAREST,							// VkFilter					minFilter;
		VK_SAMPLER_MIPMAP_MODE_NEAREST,				// VkSamplerMipmapMode		mipmapMode;
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,	// VkSamplerAddressMode		addressModeU;
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,	// VkSamplerAddressMode		addressModeV;
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,	// VkSamplerAddressMode		addressModeW;
		0.0f,										// float					mipLodBias;
		VK_FALSE,									// VkBool32					anistoropyEnable;
		1.0f,										// float					maxAnisotropy;
		VK_FALSE,									// VkBool32					compareEnable;
		VK_COMPARE_OP_ALWAYS,						// VkCompareOp				compareOp;
		0.0f,										// float					minLod;
		0.0f,										// float					maxLod;
		formatIsFloat(m_data.format) ?
			VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK :
			VK_BORDER_COLOR_INT_TRANSPARENT_BLACK,	// VkBorderColor			borderColor;
		VK_FALSE									// VkBool32					unnormalizedCoordinates;
	};

	Move<VkSampler>				sampler			(createSampler(vk, device, &samplerParams));

	// Flush modified memory.
	if (!m_data.nullDescriptor)
		flushAlloc(vk, device, buffer->getAllocation());

	const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,				// sType
		DE_NULL,													// pNext
		(VkPipelineLayoutCreateFlags)0,
		1u,															// setLayoutCount
		&descriptorSetLayout.get(),									// pSetLayouts
		0u,															// pushConstantRangeCount
		DE_NULL,													// pPushConstantRanges
	};

	Move<VkPipelineLayout> pipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo, NULL);

	de::MovePtr<BufferWithMemory> copyBuffer;
	copyBuffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
		vk, device, allocator, makeBufferCreateInfo(DIM*DIM*16, VK_BUFFER_USAGE_TRANSFER_DST_BIT), MemoryRequirement::HostVisible));

	{
		vector<VkDescriptorBufferInfo> bufferInfoVec(2);
		vector<VkDescriptorImageInfo> imageInfoVec(2);
		vector<VkBufferView> bufferViewVec(2);
		vector<VkWriteDescriptorSet> writesBeforeBindVec(0);
		int vecIndex = 0;
		int numDynamic = 0;

		vector<VkDescriptorUpdateTemplateEntry> imgTemplateEntriesBefore,
												bufTemplateEntriesBefore,
												texelBufTemplateEntriesBefore;

		for (size_t b = 0; b < bindings.size(); ++b)
		{
			VkDescriptorSetLayoutBinding &binding = bindings[b];
			// Construct the declaration for the binding
			if (binding.descriptorCount > 0)
			{
				// output image
				switch (binding.descriptorType)
				{
				case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
				case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
					// Output image.
					if (b == 1 && m_data.nullDescriptor)
						imageInfoVec[vecIndex] = makeDescriptorImageInfo(*sampler, DE_NULL, VK_IMAGE_LAYOUT_GENERAL);
					else
						imageInfoVec[vecIndex] = makeDescriptorImageInfo(*sampler, **imageViews[b], VK_IMAGE_LAYOUT_GENERAL);
					break;
				case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
				case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
					if (b == 1 && m_data.nullDescriptor)
						bufferViewVec[vecIndex] = DE_NULL;
					else
						bufferViewVec[vecIndex] = **bufferViews[0];
					break;
				default:
					// Other descriptor types.
					if (b == 1 && m_data.nullDescriptor)
						bufferInfoVec[vecIndex] = makeDescriptorBufferInfo(DE_NULL, 0, VK_WHOLE_SIZE);
					else
						bufferInfoVec[vecIndex] = makeDescriptorBufferInfo(**buffer, 0, layout.refData.size());
					break;
				}

				VkWriteDescriptorSet w =
				{
					VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,				// sType
					DE_NULL,											// pNext
					m_data.pushDescriptor ? DE_NULL : *descriptorSet,	// dstSet
					(deUint32)b,										// binding
					0,													// dstArrayElement
					1u,													// descriptorCount
					binding.descriptorType,								// descriptorType
					&imageInfoVec[vecIndex],							// pImageInfo
					&bufferInfoVec[vecIndex],							// pBufferInfo
					&bufferViewVec[vecIndex],							// pTexelBufferView
				};

				VkDescriptorUpdateTemplateEntry templateEntry =
				{
					(deUint32)b,				// uint32_t				dstBinding;
					0,							// uint32_t				dstArrayElement;
					1u,							// uint32_t				descriptorCount;
					binding.descriptorType,		// VkDescriptorType		descriptorType;
					0,							// size_t				offset;
					0,							// size_t				stride;
				};

				switch (binding.descriptorType)
				{
				default: DE_ASSERT(0); // Fallthrough
				case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
				case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
					templateEntry.offset = vecIndex * sizeof(VkDescriptorImageInfo);
					imgTemplateEntriesBefore.push_back(templateEntry);
					break;
				case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
				case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
					templateEntry.offset = vecIndex * sizeof(VkBufferView);
					texelBufTemplateEntriesBefore.push_back(templateEntry);
					break;
				case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
				case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
				case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
				case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
					templateEntry.offset = vecIndex * sizeof(VkDescriptorBufferInfo);
					bufTemplateEntriesBefore.push_back(templateEntry);
					break;
				}

				vecIndex++;

				writesBeforeBindVec.push_back(w);

				// Count the number of dynamic descriptors in this set.
				if (binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
					binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
				{
					numDynamic++;
				}
			}
		}

		// Make zeros have at least one element so &zeros[0] works
		vector<deUint32> zeros(de::max(1,numDynamic));
		deMemset(&zeros[0], 0, numDynamic * sizeof(deUint32));

		// Randomly select between vkUpdateDescriptorSets and vkUpdateDescriptorSetWithTemplate
		if (m_data.useTemplate)
		{
			VkDescriptorUpdateTemplateCreateInfo templateCreateInfo =
			{
				VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO,	// VkStructureType							sType;
				NULL,														// void*									pNext;
				0,															// VkDescriptorUpdateTemplateCreateFlags	flags;
				0,															// uint32_t									descriptorUpdateEntryCount;
				DE_NULL,													// uint32_t									descriptorUpdateEntryCount;
				m_data.pushDescriptor ?
					VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS_KHR :
					VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET,		// VkDescriptorUpdateTemplateType			templateType;
				descriptorSetLayout.get(),									// VkDescriptorSetLayout					descriptorSetLayout;
				bindPoint,													// VkPipelineBindPoint						pipelineBindPoint;
				*pipelineLayout,											// VkPipelineLayout							pipelineLayout;
				0,															// uint32_t									set;
			};

			void *templateVectorData[] =
			{
				imageInfoVec.data(),
				bufferInfoVec.data(),
				bufferViewVec.data(),
			};

			vector<VkDescriptorUpdateTemplateEntry> *templateVectorsBefore[] =
			{
				&imgTemplateEntriesBefore,
				&bufTemplateEntriesBefore,
				&texelBufTemplateEntriesBefore,
			};

			if (m_data.pushDescriptor)
			{
				for (size_t i = 0; i < DE_LENGTH_OF_ARRAY(templateVectorsBefore); ++i)
				{
					if (templateVectorsBefore[i]->size())
					{
						templateCreateInfo.descriptorUpdateEntryCount = (deUint32)templateVectorsBefore[i]->size();
						templateCreateInfo.pDescriptorUpdateEntries = templateVectorsBefore[i]->data();
						Move<VkDescriptorUpdateTemplate> descriptorUpdateTemplate = createDescriptorUpdateTemplate(vk, device, &templateCreateInfo, NULL);
						vk.cmdPushDescriptorSetWithTemplateKHR(*cmdBuffer, *descriptorUpdateTemplate, *pipelineLayout, 0, templateVectorData[i]);
					}
				}
			}
			else
			{
				for (size_t i = 0; i < DE_LENGTH_OF_ARRAY(templateVectorsBefore); ++i)
				{
					if (templateVectorsBefore[i]->size())
					{
						templateCreateInfo.descriptorUpdateEntryCount = (deUint32)templateVectorsBefore[i]->size();
						templateCreateInfo.pDescriptorUpdateEntries = templateVectorsBefore[i]->data();
						Move<VkDescriptorUpdateTemplate> descriptorUpdateTemplate = createDescriptorUpdateTemplate(vk, device, &templateCreateInfo, NULL);
						vk.updateDescriptorSetWithTemplate(device, descriptorSet.get(), *descriptorUpdateTemplate, templateVectorData[i]);
					}
				}

				vk.cmdBindDescriptorSets(*cmdBuffer, bindPoint, *pipelineLayout, 0, 1, &descriptorSet.get(), numDynamic, &zeros[0]);
			}
		}
		else
		{
			if (m_data.pushDescriptor)
			{
				if (writesBeforeBindVec.size())
				{
					vk.cmdPushDescriptorSetKHR(*cmdBuffer, bindPoint, *pipelineLayout, 0, (deUint32)writesBeforeBindVec.size(), &writesBeforeBindVec[0]);
				}
			}
			else
			{
				if (writesBeforeBindVec.size())
				{
					vk.updateDescriptorSets(device, (deUint32)writesBeforeBindVec.size(), &writesBeforeBindVec[0], 0, NULL);
				}

				vk.cmdBindDescriptorSets(*cmdBuffer, bindPoint, *pipelineLayout, 0, 1, &descriptorSet.get(), numDynamic, &zeros[0]);
			}
		}
	}

	Move<VkPipeline> pipeline;
	Move<VkRenderPass> renderPass;
	Move<VkFramebuffer> framebuffer;

	de::MovePtr<BufferWithMemory> sbtBuffer;

	if (m_data.stage == STAGE_COMPUTE)
	{
		const Unique<VkShaderModule>	shader(createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0));

		pipeline = makeComputePipeline(vk, device, *pipelineLayout, *shader);

	}
	else if (m_data.stage == STAGE_RAYGEN)
	{
		const Unique<VkShaderModule>	shader(createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0));

		const VkPipelineShaderStageCreateInfo	shaderCreateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			DE_NULL,
			(VkPipelineShaderStageCreateFlags)0,
			VK_SHADER_STAGE_RAYGEN_BIT_NV,								// stage
			*shader,													// shader
			"main",
			DE_NULL,													// pSpecializationInfo
		};

		VkRayTracingShaderGroupCreateInfoNV group =
		{
			VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV,
			DE_NULL,
			VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV,			// type
			0,														// generalShader
			VK_SHADER_UNUSED_NV,									// closestHitShader
			VK_SHADER_UNUSED_NV,									// anyHitShader
			VK_SHADER_UNUSED_NV,									// intersectionShader
		};

		VkRayTracingPipelineCreateInfoNV pipelineCreateInfo = {
			VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_NV,	// sType
			DE_NULL,												// pNext
			0,														// flags
			1,														// stageCount
			&shaderCreateInfo,										// pStages
			1,														// groupCount
			&group,													// pGroups
			0,														// maxRecursionDepth
			*pipelineLayout,										// layout
			(vk::VkPipeline)0,										// basePipelineHandle
			0u,														// basePipelineIndex
		};

		pipeline = createRayTracingPipelineNV(vk, device, DE_NULL, &pipelineCreateInfo, NULL);

		sbtBuffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
			vk, device, allocator, makeBufferCreateInfo(rayTracingProperties.shaderGroupHandleSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_RAY_TRACING_BIT_NV), MemoryRequirement::HostVisible));

		deUint32 *ptr = (deUint32 *)sbtBuffer->getAllocation().getHostPtr();
		invalidateAlloc(vk, device, sbtBuffer->getAllocation());

		vk.getRayTracingShaderGroupHandlesNV(device, *pipeline, 0, 1, rayTracingProperties.shaderGroupHandleSize, ptr);
	}
	else
	{
		const VkSubpassDescription		subpassDesc				=
		{
			(VkSubpassDescriptionFlags)0,											// VkSubpassDescriptionFlags	flags
			VK_PIPELINE_BIND_POINT_GRAPHICS,										// VkPipelineBindPoint			pipelineBindPoint
			0u,																		// deUint32						inputAttachmentCount
			DE_NULL,																// const VkAttachmentReference*	pInputAttachments
			0u,																		// deUint32						colorAttachmentCount
			DE_NULL,																// const VkAttachmentReference*	pColorAttachments
			DE_NULL,																// const VkAttachmentReference*	pResolveAttachments
			DE_NULL,																// const VkAttachmentReference*	pDepthStencilAttachment
			0u,																		// deUint32						preserveAttachmentCount
			DE_NULL																	// const deUint32*				pPreserveAttachments
		};

		const VkSubpassDependency		subpassDependency		=
		{
			VK_SUBPASS_EXTERNAL,							// deUint32				srcSubpass
			0,												// deUint32				dstSubpass
			VK_PIPELINE_STAGE_TRANSFER_BIT,					// VkPipelineStageFlags	srcStageMask
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,			// VkPipelineStageFlags	dstStageMask
			VK_ACCESS_TRANSFER_WRITE_BIT,					// VkAccessFlags		srcAccessMask
			VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT,	//	dstAccessMask
			VK_DEPENDENCY_BY_REGION_BIT						// VkDependencyFlags	dependencyFlags
		};

		const VkRenderPassCreateInfo	renderPassParams		=
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,				// VkStructureTypei					sType
			DE_NULL,												// const void*						pNext
			(VkRenderPassCreateFlags)0,								// VkRenderPassCreateFlags			flags
			0u,														// deUint32							attachmentCount
			DE_NULL,												// const VkAttachmentDescription*	pAttachments
			1u,														// deUint32							subpassCount
			&subpassDesc,											// const VkSubpassDescription*		pSubpasses
			1u,														// deUint32							dependencyCount
			&subpassDependency										// const VkSubpassDependency*		pDependencies
		};

		renderPass = createRenderPass(vk, device, &renderPassParams);

		const vk::VkFramebufferCreateInfo	framebufferParams	=
		{
			vk::VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,			// sType
			DE_NULL,												// pNext
			(vk::VkFramebufferCreateFlags)0,
			*renderPass,											// renderPass
			0u,														// attachmentCount
			DE_NULL,												// pAttachments
			DIM,													// width
			DIM,													// height
			1u,														// layers
		};

		framebuffer = createFramebuffer(vk, device, &framebufferParams);

		const VkVertexInputBindingDescription			vertexInputBindingDescription		=
		{
			0u,								// deUint32			 binding
			(deUint32)formatBytes,			// deUint32			 stride
			VK_VERTEX_INPUT_RATE_VERTEX,	// VkVertexInputRate	inputRate
		};

		const VkVertexInputAttributeDescription			vertexInputAttributeDescription		=
		{
			0u,								// deUint32	location
			0u,								// deUint32	binding
			m_data.format,					// VkFormat	format
			0u								// deUint32	offset
		};

		deUint32 numAttribs = m_data.descriptorType == VERTEX_ATTRIBUTE_FETCH ? 1u : 0u;

		const VkPipelineVertexInputStateCreateInfo		vertexInputStateCreateInfo		=
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType							sType;
			DE_NULL,													// const void*								pNext;
			(VkPipelineVertexInputStateCreateFlags)0,					// VkPipelineVertexInputStateCreateFlags	flags;
			numAttribs,													// deUint32									vertexBindingDescriptionCount;
			&vertexInputBindingDescription,								// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
			numAttribs,													// deUint32									vertexAttributeDescriptionCount;
			&vertexInputAttributeDescription							// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
		};

		const VkPipelineInputAssemblyStateCreateInfo	inputAssemblyStateCreateInfo	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			(VkPipelineInputAssemblyStateCreateFlags)0,						// VkPipelineInputAssemblyStateCreateFlags	flags;
			(m_data.stage == STAGE_VERTEX) ? VK_PRIMITIVE_TOPOLOGY_POINT_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, // VkPrimitiveTopology						topology;
			VK_FALSE														// VkBool32									primitiveRestartEnable;
		};

		const VkPipelineRasterizationStateCreateInfo	rasterizationStateCreateInfo	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,		// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			(VkPipelineRasterizationStateCreateFlags)0,						// VkPipelineRasterizationStateCreateFlags	flags;
			VK_FALSE,														// VkBool32									depthClampEnable;
			(m_data.stage == STAGE_VERTEX) ? VK_TRUE : VK_FALSE,			// VkBool32									rasterizerDiscardEnable;
			VK_POLYGON_MODE_FILL,											// VkPolygonMode							polygonMode;
			VK_CULL_MODE_NONE,												// VkCullModeFlags							cullMode;
			VK_FRONT_FACE_CLOCKWISE,										// VkFrontFace								frontFace;
			VK_FALSE,														// VkBool32									depthBiasEnable;
			0.0f,															// float									depthBiasConstantFactor;
			0.0f,															// float									depthBiasClamp;
			0.0f,															// float									depthBiasSlopeFactor;
			1.0f															// float									lineWidth;
		};

		const VkPipelineMultisampleStateCreateInfo		multisampleStateCreateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType							sType
			DE_NULL,													// const void*								pNext
			0u,															// VkPipelineMultisampleStateCreateFlags	flags
			VK_SAMPLE_COUNT_1_BIT,										// VkSampleCountFlagBits					rasterizationSamples
			VK_FALSE,													// VkBool32									sampleShadingEnable
			1.0f,														// float									minSampleShading
			DE_NULL,													// const VkSampleMask*						pSampleMask
			VK_FALSE,													// VkBool32									alphaToCoverageEnable
			VK_FALSE													// VkBool32									alphaToOneEnable
		};

		VkViewport viewport = makeViewport(DIM, DIM);
		VkRect2D scissor = makeRect2D(DIM, DIM);

		const VkPipelineViewportStateCreateInfo			viewportStateCreateInfo				=
		{
			VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,	// VkStructureType							sType
			DE_NULL,												// const void*								pNext
			(VkPipelineViewportStateCreateFlags)0,					// VkPipelineViewportStateCreateFlags		flags
			1u,														// deUint32									viewportCount
			&viewport,												// const VkViewport*						pViewports
			1u,														// deUint32									scissorCount
			&scissor												// const VkRect2D*							pScissors
		};

		Move<VkShaderModule> fs;
		Move<VkShaderModule> vs;

		deUint32 numStages;
		if (m_data.stage == STAGE_VERTEX)
		{
			vs = createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0);
			fs = createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0); // bogus
			numStages = 1u;
		}
		else
		{
			vs = createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0);
			fs = createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0);
			numStages = 2u;
		}

		const VkPipelineShaderStageCreateInfo	shaderCreateInfo[2] =
		{
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				DE_NULL,
				(VkPipelineShaderStageCreateFlags)0,
				VK_SHADER_STAGE_VERTEX_BIT,									// stage
				*vs,														// shader
				"main",
				DE_NULL,													// pSpecializationInfo
			},
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				DE_NULL,
				(VkPipelineShaderStageCreateFlags)0,
				VK_SHADER_STAGE_FRAGMENT_BIT,								// stage
				*fs,														// shader
				"main",
				DE_NULL,													// pSpecializationInfo
			}
		};

		const VkGraphicsPipelineCreateInfo				graphicsPipelineCreateInfo		=
		{
			VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,	// VkStructureType									sType;
			DE_NULL,											// const void*										pNext;
			(VkPipelineCreateFlags)0,							// VkPipelineCreateFlags							flags;
			numStages,											// deUint32											stageCount;
			&shaderCreateInfo[0],								// const VkPipelineShaderStageCreateInfo*			pStages;
			&vertexInputStateCreateInfo,						// const VkPipelineVertexInputStateCreateInfo*		pVertexInputState;
			&inputAssemblyStateCreateInfo,						// const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState;
			DE_NULL,											// const VkPipelineTessellationStateCreateInfo*		pTessellationState;
			&viewportStateCreateInfo,							// const VkPipelineViewportStateCreateInfo*			pViewportState;
			&rasterizationStateCreateInfo,						// const VkPipelineRasterizationStateCreateInfo*	pRasterizationState;
			&multisampleStateCreateInfo,						// const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
			DE_NULL,											// const VkPipelineDepthStencilStateCreateInfo*		pDepthStencilState;
			DE_NULL,											// const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
			DE_NULL,											// const VkPipelineDynamicStateCreateInfo*			pDynamicState;
			pipelineLayout.get(),								// VkPipelineLayout									layout;
			renderPass.get(),									// VkRenderPass										renderPass;
			0u,													// deUint32											subpass;
			DE_NULL,											// VkPipeline										basePipelineHandle;
			0													// int												basePipelineIndex;
		};

		pipeline = createGraphicsPipeline(vk, device, DE_NULL, &graphicsPipelineCreateInfo);
	}

	const VkImageMemoryBarrier imageBarrier =
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,				// VkStructureType		sType
		DE_NULL,											// const void*			pNext
		0u,													// VkAccessFlags		srcAccessMask
		VK_ACCESS_TRANSFER_WRITE_BIT,						// VkAccessFlags		dstAccessMask
		VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout		oldLayout
		VK_IMAGE_LAYOUT_GENERAL,							// VkImageLayout		newLayout
		VK_QUEUE_FAMILY_IGNORED,							// uint32_t				srcQueueFamilyIndex
		VK_QUEUE_FAMILY_IGNORED,							// uint32_t				dstQueueFamilyIndex
		**images[0],										// VkImage				image
		{
			VK_IMAGE_ASPECT_COLOR_BIT,				// VkImageAspectFlags	aspectMask
			0u,										// uint32_t				baseMipLevel
			1u,										// uint32_t				mipLevels,
			0u,										// uint32_t				baseArray
			1u,										// uint32_t				arraySize
		}
	};

	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
							(VkDependencyFlags)0,
							0, (const VkMemoryBarrier*)DE_NULL,
							0, (const VkBufferMemoryBarrier*)DE_NULL,
							1, &imageBarrier);

	vk.cmdBindPipeline(*cmdBuffer, bindPoint, *pipeline);

	if (!formatIsR64(m_data.format))
	{
		VkImageSubresourceRange range = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
		VkClearValue clearColor = makeClearValueColorU32(0,0,0,0);

		vk.cmdClearColorImage(*cmdBuffer, **images[0], VK_IMAGE_LAYOUT_GENERAL, &clearColor.color, 1, &range);
	}
	else
	{
		const vector<VkBufferImageCopy>	bufferImageCopy(1, makeBufferImageCopy(outputImageCreateInfo.extent, makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1)));
		copyBufferToImage(vk,
			*cmdBuffer,
			*(*bufferOutputImageR64),
			sizeOutputR64,
			bufferImageCopy,
			VK_IMAGE_ASPECT_COLOR_BIT,
			1,
			1, **images[0], VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);
	}

	VkMemoryBarrier					memBarrier =
	{
		VK_STRUCTURE_TYPE_MEMORY_BARRIER,	// sType
		DE_NULL,							// pNext
		0u,									// srcAccessMask
		0u,									// dstAccessMask
	};

	memBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	memBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, m_data.allPipelineStages,
		0, 1, &memBarrier, 0, DE_NULL, 0, DE_NULL);

	if (m_data.stage == STAGE_COMPUTE)
	{
		vk.cmdDispatch(*cmdBuffer, DIM, DIM, 1);
	}
	else if (m_data.stage == STAGE_RAYGEN)
	{
		vk.cmdTraceRaysNV(*cmdBuffer,
			**sbtBuffer, 0,
			DE_NULL, 0, 0,
			DE_NULL, 0, 0,
			DE_NULL, 0, 0,
			DIM, DIM, 1);
	}
	else
	{
		beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer,
						makeRect2D(DIM, DIM),
						0, DE_NULL, VK_SUBPASS_CONTENTS_INLINE);
		// Draw a point cloud for vertex shader testing, and a single quad for fragment shader testing
		if (m_data.descriptorType == VERTEX_ATTRIBUTE_FETCH)
		{
			VkDeviceSize zeroOffset = 0;
			VkBuffer b = m_data.nullDescriptor ? DE_NULL : **buffer;
			vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &b, &zeroOffset);
			vk.cmdDraw(*cmdBuffer, 1000u, 1u, 0u, 0u);
		}
		if (m_data.stage == STAGE_VERTEX)
		{
			vk.cmdDraw(*cmdBuffer, DIM*DIM, 1u, 0u, 0u);
		}
		else
		{
			vk.cmdDraw(*cmdBuffer, 4u, 1u, 0u, 0u);
		}
		endRenderPass(vk, *cmdBuffer);
	}

	memBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	memBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
	vk.cmdPipelineBarrier(*cmdBuffer, m_data.allPipelineStages, VK_PIPELINE_STAGE_TRANSFER_BIT,
		0, 1, &memBarrier, 0, DE_NULL, 0, DE_NULL);

	const VkBufferImageCopy copyRegion = makeBufferImageCopy(makeExtent3D(DIM, DIM, 1u),
															 makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u));
	vk.cmdCopyImageToBuffer(*cmdBuffer, **images[0], VK_IMAGE_LAYOUT_GENERAL, **copyBuffer, 1u, &copyRegion);

	endCommandBuffer(vk, *cmdBuffer);

	submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

	void *ptr = copyBuffer->getAllocation().getHostPtr();

	invalidateAlloc(vk, device, copyBuffer->getAllocation());

	qpTestResult res = QP_TEST_RESULT_PASS;

	for (deUint32 i = 0; i < DIM*DIM; ++i)
	{
		if (formatIsFloat(m_data.format))
		{
			if (((float *)ptr)[i * numComponents] != 1.0f)
			{
				res = QP_TEST_RESULT_FAIL;
			}
		}
		else if (formatIsR64(m_data.format))
		{
			if (((deUint64 *)ptr)[i * numComponents] != 1)
			{
				res = QP_TEST_RESULT_FAIL;
			}
		}
		else
		{
			if (((deUint32 *)ptr)[i * numComponents] != 1)
			{
				res = QP_TEST_RESULT_FAIL;
			}
		}
	}

	return tcu::TestStatus(res, qpGetTestResultName(res));
}

}	// anonymous

static void createTests (tcu::TestCaseGroup* group, bool robustness2)
{
	tcu::TestContext& testCtx = group->getTestContext();

	typedef struct
	{
		deUint32				count;
		const char*				name;
		const char*				description;
	} TestGroupCase;

	TestGroupCase fmtCases[] =
	{
		{ VK_FORMAT_R32_SINT,				"r32i",		""		},
		{ VK_FORMAT_R32_UINT,				"r32ui",	""		},
		{ VK_FORMAT_R32_SFLOAT,				"r32f",		""		},
		{ VK_FORMAT_R32G32_SINT,			"rg32i",	""		},
		{ VK_FORMAT_R32G32_UINT,			"rg32ui",	""		},
		{ VK_FORMAT_R32G32_SFLOAT,			"rg32f",	""		},
		{ VK_FORMAT_R32G32B32A32_SINT,		"rgba32i",	""		},
		{ VK_FORMAT_R32G32B32A32_UINT,		"rgba32ui",	""		},
		{ VK_FORMAT_R32G32B32A32_SFLOAT,	"rgba32f",	""		},
		{ VK_FORMAT_R64_SINT,				"r64i",		""		},
		{ VK_FORMAT_R64_UINT,				"r64ui",	""		},
	};

	TestGroupCase fullDescCases[] =
	{
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,				"uniform_buffer",			""		},
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,				"storage_buffer",			""		},
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,		"uniform_buffer_dynamic",	""		},
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,		"storage_buffer_dynamic",	""		},
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,			"uniform_texel_buffer",		""		},
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,			"storage_texel_buffer",		""		},
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,					"storage_image",			""		},
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,		"sampled_image",			""		},
		{ VERTEX_ATTRIBUTE_FETCH,							"vertex_attribute_fetch",	""		},
	};

	TestGroupCase imgDescCases[] =
	{
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,					"storage_image",			""		},
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,		"sampled_image",			""		},
	};

	TestGroupCase fullLenCases32Bit[] =
	{
		{ ~0U,			"null_descriptor",	""		},
		{ 0,			"img",				""		},
		{ 4,			"len_4",			""		},
		{ 8,			"len_8",			""		},
		{ 12,			"len_12",			""		},
		{ 16,			"len_16",			""		},
		{ 20,			"len_20",			""		},
		{ 31,			"len_31",			""		},
		{ 32,			"len_32",			""		},
		{ 33,			"len_33",			""		},
		{ 35,			"len_35",			""		},
		{ 36,			"len_36",			""		},
		{ 39,			"len_39",			""		},
		{ 40,			"len_41",			""		},
		{ 252,			"len_252",			""		},
		{ 256,			"len_256",			""		},
		{ 260,			"len_260",			""		},
	};

	TestGroupCase fullLenCases64Bit[] =
	{
		{ ~0U,			"null_descriptor",	""		},
		{ 0,			"img",				""		},
		{ 8,			"len_8",			""		},
		{ 16,			"len_16",			""		},
		{ 24,			"len_24",			""		},
		{ 32,			"len_32",			""		},
		{ 40,			"len_40",			""		},
		{ 62,			"len_62",			""		},
		{ 64,			"len_64",			""		},
		{ 66,			"len_66",			""		},
		{ 70,			"len_70",			""		},
		{ 72,			"len_72",			""		},
		{ 78,			"len_78",			""		},
		{ 80,			"len_80",			""		},
		{ 504,			"len_504",			""		},
		{ 512,			"len_512",			""		},
		{ 520,			"len_520",			""		},
	};

	TestGroupCase imgLenCases[] =
	{
		{ 0,	"img",	""		},
	};

	TestGroupCase viewCases[] =
	{
		{ VK_IMAGE_VIEW_TYPE_1D,			"1d",			""		},
		{ VK_IMAGE_VIEW_TYPE_2D,			"2d",			""		},
		{ VK_IMAGE_VIEW_TYPE_3D,			"3d",			""		},
		{ VK_IMAGE_VIEW_TYPE_CUBE,			"cube",			""		},
		{ VK_IMAGE_VIEW_TYPE_1D_ARRAY,		"1d_array",		""		},
		{ VK_IMAGE_VIEW_TYPE_2D_ARRAY,		"2d_array",		""		},
		{ VK_IMAGE_VIEW_TYPE_CUBE_ARRAY,	"cube_array",	""		},
	};

	TestGroupCase sampCases[] =
	{
		{ VK_SAMPLE_COUNT_1_BIT,			"samples_1",	""		},
		{ VK_SAMPLE_COUNT_4_BIT,			"samples_4",	""		},
	};

	TestGroupCase stageCases[] =
	{
		{ STAGE_COMPUTE,	"comp",		"compute"	},
		{ STAGE_FRAGMENT,	"frag",		"fragment"	},
		{ STAGE_VERTEX,		"vert",		"vertex"	},
		{ STAGE_RAYGEN,		"rgen",		"raygen"	},
	};

	TestGroupCase volCases[] =
	{
		{ 0,			"nonvolatile",	""		},
		{ 1,			"volatile",		""		},
	};

	TestGroupCase unrollCases[] =
	{
		{ 0,			"dontunroll",	""		},
		{ 1,			"unroll",		""		},
	};

	TestGroupCase tempCases[] =
	{
		{ 0,			"notemplate",	""		},
		{ 1,			"template",		""		},
	};

	TestGroupCase pushCases[] =
	{
		{ 0,			"bind",			""		},
		{ 1,			"push",			""		},
	};

	TestGroupCase fmtQualCases[] =
	{
		{ 0,			"no_fmt_qual",	""		},
		{ 1,			"fmt_qual",		""		},
	};

	for (int pushNdx = 0; pushNdx < DE_LENGTH_OF_ARRAY(pushCases); pushNdx++)
	{
		de::MovePtr<tcu::TestCaseGroup> pushGroup(new tcu::TestCaseGroup(testCtx, pushCases[pushNdx].name, pushCases[pushNdx].name));
		for (int tempNdx = 0; tempNdx < DE_LENGTH_OF_ARRAY(tempCases); tempNdx++)
		{
			de::MovePtr<tcu::TestCaseGroup> tempGroup(new tcu::TestCaseGroup(testCtx, tempCases[tempNdx].name, tempCases[tempNdx].name));
			for (int fmtNdx = 0; fmtNdx < DE_LENGTH_OF_ARRAY(fmtCases); fmtNdx++)
			{
				de::MovePtr<tcu::TestCaseGroup> fmtGroup(new tcu::TestCaseGroup(testCtx, fmtCases[fmtNdx].name, fmtCases[fmtNdx].name));

				int fmtSize = tcu::getPixelSize(mapVkFormat((VkFormat)fmtCases[fmtNdx].count));

				for (int unrollNdx = 0; unrollNdx < DE_LENGTH_OF_ARRAY(unrollCases); unrollNdx++)
				{
					de::MovePtr<tcu::TestCaseGroup> unrollGroup(new tcu::TestCaseGroup(testCtx, unrollCases[unrollNdx].name, unrollCases[unrollNdx].name));
					for (int volNdx = 0; volNdx < DE_LENGTH_OF_ARRAY(volCases); volNdx++)
					{
						de::MovePtr<tcu::TestCaseGroup> volGroup(new tcu::TestCaseGroup(testCtx, volCases[volNdx].name, volCases[volNdx].name));

						int numDescCases = robustness2 ? DE_LENGTH_OF_ARRAY(fullDescCases) : DE_LENGTH_OF_ARRAY(imgDescCases);
						TestGroupCase *descCases = robustness2 ? fullDescCases : imgDescCases;

						for (int descNdx = 0; descNdx < numDescCases; descNdx++)
						{
							de::MovePtr<tcu::TestCaseGroup> descGroup(new tcu::TestCaseGroup(testCtx, descCases[descNdx].name, descCases[descNdx].name));
							for (int fmtQualNdx = 0; fmtQualNdx < DE_LENGTH_OF_ARRAY(fmtQualCases); fmtQualNdx++)
							{
								de::MovePtr<tcu::TestCaseGroup> fmtQualGroup(new tcu::TestCaseGroup(testCtx, fmtQualCases[fmtQualNdx].name, fmtQualCases[fmtQualNdx].name));

								// format qualifier is only used for storage image and storage texel buffers
								if (fmtQualCases[fmtQualNdx].count &&
									!(descCases[descNdx].count == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER || descCases[descNdx].count == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE))
									continue;

								if (pushCases[pushNdx].count &&
									(descCases[descNdx].count == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC || descCases[descNdx].count == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC || descCases[descNdx].count == VERTEX_ATTRIBUTE_FETCH))
									continue;

								const bool isR64 = formatIsR64((VkFormat)fmtCases[fmtNdx].count);
								int numLenCases = robustness2 ? DE_LENGTH_OF_ARRAY((isR64 ? fullLenCases64Bit : fullLenCases32Bit)) : DE_LENGTH_OF_ARRAY(imgLenCases);
								TestGroupCase *lenCases = robustness2 ? (isR64 ? fullLenCases64Bit : fullLenCases32Bit) : imgLenCases;

								for (int lenNdx = 0; lenNdx < numLenCases; lenNdx++)
								{
									if (lenCases[lenNdx].count != ~0U)
									{
										bool bufferLen = lenCases[lenNdx].count != 0;
										bool bufferDesc = descCases[descNdx].count != VK_DESCRIPTOR_TYPE_STORAGE_IMAGE && descCases[descNdx].count != VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
										if (bufferLen != bufferDesc)
											continue;

										// Add template tests cases only for null_descriptor cases
										if (tempCases[tempNdx].count)
											continue;
									}

									if ((descCases[descNdx].count == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER || descCases[descNdx].count == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER) &&
										((lenCases[lenNdx].count % fmtSize) != 0) &&
										lenCases[lenNdx].count != ~0U)
									{
										continue;
									}

									// "volatile" only applies to storage images/buffers
									if (volCases[volNdx].count && !supportsStores(descCases[descNdx].count))
										continue;

									de::MovePtr<tcu::TestCaseGroup> lenGroup(new tcu::TestCaseGroup(testCtx, lenCases[lenNdx].name, lenCases[lenNdx].name));
									for (int sampNdx = 0; sampNdx < DE_LENGTH_OF_ARRAY(sampCases); sampNdx++)
									{
										de::MovePtr<tcu::TestCaseGroup> sampGroup(new tcu::TestCaseGroup(testCtx, sampCases[sampNdx].name, sampCases[sampNdx].name));
										for (int viewNdx = 0; viewNdx < DE_LENGTH_OF_ARRAY(viewCases); viewNdx++)
										{
											if (viewCases[viewNdx].count != VK_IMAGE_VIEW_TYPE_1D &&
												descCases[descNdx].count != VK_DESCRIPTOR_TYPE_STORAGE_IMAGE &&
												descCases[descNdx].count != VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
											{
												// buffer descriptors don't have different dimensionalities. Only test "1D"
												continue;
											}

											if (viewCases[viewNdx].count != VK_IMAGE_VIEW_TYPE_2D && viewCases[viewNdx].count != VK_IMAGE_VIEW_TYPE_2D_ARRAY &&
												sampCases[sampNdx].count != VK_SAMPLE_COUNT_1_BIT)
											{
												continue;
											}

											de::MovePtr<tcu::TestCaseGroup> viewGroup(new tcu::TestCaseGroup(testCtx, viewCases[viewNdx].name, viewCases[viewNdx].name));
											for (int stageNdx = 0; stageNdx < DE_LENGTH_OF_ARRAY(stageCases); stageNdx++)
											{
												Stage currentStage = static_cast<Stage>(stageCases[stageNdx].count);
												VkFlags allShaderStages = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
												VkFlags allPipelineStages = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
												if ((Stage)stageCases[stageNdx].count == STAGE_RAYGEN)
												{
													allShaderStages |= VK_SHADER_STAGE_RAYGEN_BIT_NV;
													allPipelineStages |= VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV;
												}

												if (descCases[descNdx].count == VERTEX_ATTRIBUTE_FETCH &&
													currentStage != STAGE_VERTEX)
													continue;

												deUint32 imageDim[3] = {5, 11, 6};
												if (viewCases[viewNdx].count == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY ||
													viewCases[viewNdx].count == VK_IMAGE_VIEW_TYPE_CUBE)
													imageDim[1] = imageDim[0];

												CaseDef c =
												{
													(VkFormat)fmtCases[fmtNdx].count,								// VkFormat format;
													currentStage,													// Stage stage;
													allShaderStages,												// VkFlags allShaderStages;
													allPipelineStages,												// VkFlags allPipelineStages;
													(int)descCases[descNdx].count,									// VkDescriptorType descriptorType;
													(VkImageViewType)viewCases[viewNdx].count,						// VkImageViewType viewType;
													(VkSampleCountFlagBits)sampCases[sampNdx].count,				// VkSampleCountFlagBits samples;
													(int)lenCases[lenNdx].count,									// int bufferLen;
													(bool)unrollCases[unrollNdx].count,								// bool unroll;
													(bool)volCases[volNdx].count,									// bool vol;
													(bool)(lenCases[lenNdx].count == ~0U),							// bool nullDescriptor
													(bool)tempCases[tempNdx].count,									// bool useTemplate
													(bool)fmtQualCases[fmtQualNdx].count,							// bool formatQualifier
													(bool)pushCases[pushNdx].count,									// bool pushDescriptor;
													(bool)robustness2,												// bool testRobustness2;
													{ imageDim[0], imageDim[1], imageDim[2] },						// deUint32 imageDim[3];
												};

												viewGroup->addChild(new RobustnessExtsTestCase(testCtx, stageCases[stageNdx].name, stageCases[stageNdx].name, c));
											}
											sampGroup->addChild(viewGroup.release());
										}
										lenGroup->addChild(sampGroup.release());
									}
									fmtQualGroup->addChild(lenGroup.release());
								}
								descGroup->addChild(fmtQualGroup.release());
							}
							volGroup->addChild(descGroup.release());
						}
						unrollGroup->addChild(volGroup.release());
					}
					fmtGroup->addChild(unrollGroup.release());
				}
				tempGroup->addChild(fmtGroup.release());
			}
			pushGroup->addChild(tempGroup.release());
		}
		group->addChild(pushGroup.release());
	}
}

static void createRobustness2Tests (tcu::TestCaseGroup* group)
{
	createTests(group, /*robustness2=*/true);
}

static void createImageRobustnessTests (tcu::TestCaseGroup* group)
{
	createTests(group, /*robustness2=*/false);
}

static void cleanupGroup (tcu::TestCaseGroup* group)
{
	DE_UNREF(group);
	// Destroy singleton objects.
	Robustness2Int64AtomicsSingleton::destroy();
	ImageRobustnessInt64AtomicsSingleton::destroy();
	ImageRobustnessSingleton::destroy();
	Robustness2Singleton::destroy();
}

tcu::TestCaseGroup* createRobustness2Tests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "robustness2", "VK_EXT_robustness2 tests",
							createRobustness2Tests, cleanupGroup);
}

tcu::TestCaseGroup* createImageRobustnessTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "image_robustness", "VK_EXT_image_robustness tests",
							createImageRobustnessTests, cleanupGroup);
}

}	// robustness
}	// vkt
