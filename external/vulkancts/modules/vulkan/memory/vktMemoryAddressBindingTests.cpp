
/*------------------------------------------------------------------------
* Vulkan Conformance Tests
* ------------------------
*
* Copyright (c) 2020 The Khronos Group Inc.
* Copyright (c) 2020 Google LLC
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
* \brief VK_EXT_device_address_binding_report extension tests.
*//*--------------------------------------------------------------------*/

#include "vktMemoryAddressBindingTests.hpp"

#include "vktCustomInstancesDevices.hpp"
#include "vktExternalMemoryUtil.hpp"
#include "vktTestCaseUtil.hpp"

#include "vkDeviceUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"

#include "tcuCommandLine.hpp"
#include "tcuTestCase.hpp"
#include "tcuTestLog.hpp"

#include "deSharedPtr.hpp"

#include <set>
#include <vector>
#include <limits>

#define UNUSED(expr) do { (void)(expr); } while (0)

namespace vkt
{

namespace memory
{

namespace
{

#define VK_DESCRIPTOR_TYPE_LAST (VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT + 1)

using namespace vk;
using namespace vkt::ExternalMemoryUtil;
using de::MovePtr;
using de::SharedPtr;

struct BindingData
{
	VkDeviceAddress					bindingAddress;
	VkDeviceSize					size;
	VkDeviceAddressBindingTypeEXT	bindingType;
	uint64_t						objectHandle;

	bool operator==(const BindingData& rhs) const
	{
		if (bindingAddress != rhs.bindingAddress)
			return false;

		if (size != rhs.size)
			return false;

		if (objectHandle != rhs.objectHandle)
			return false;

		return true;
	}
};

class BindingCallbackRecorder
{
public:
	BindingCallbackRecorder(void) {}
	~BindingCallbackRecorder(void) = default;

	typedef std::vector<BindingData>::const_iterator	RecordIterator;

	RecordIterator getRecordsBegin (void) const
	{
		return mRecords.begin();
	}

	RecordIterator getRecordsEnd (void) const
	{
		return mRecords.end();
	}

	std::size_t getNumRecords (void) const
	{
		return mRecords.size();
	}

	void callbackInternal (const VkDebugUtilsMessengerCallbackDataEXT * pCallbackData)
	{
		const VkDeviceAddressBindingCallbackDataEXT* bindingCallbackData = static_cast<const VkDeviceAddressBindingCallbackDataEXT*>(pCallbackData->pNext);

		const BindingData bindingData =
		{
			bindingCallbackData->baseAddress,
			bindingCallbackData->size,
			bindingCallbackData->bindingType,
			pCallbackData->pObjects[0].objectHandle
		};

		mRecords.emplace_back(bindingData);
	}

	static VKAPI_ATTR VkBool32 VKAPI_CALL callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
	{
		UNUSED(messageSeverity);

		if (messageType == VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT)
		{
			reinterpret_cast<BindingCallbackRecorder*>(pUserData)->callbackInternal(pCallbackData);
		}
		return VK_FALSE;
	}

private:
	typedef std::vector<BindingData>	Records;

	Records			mRecords;
};

struct Environment
{
	const PlatformInterface&	vkp;
	const InstanceInterface&	vki;
	VkInstance					instance;
	VkPhysicalDevice			physicalDevice;
	const DeviceInterface&		vkd;
	VkDevice					device;
	deUint32					queueFamilyIndex;
	const BinaryCollection&		programBinaries;
	const tcu::CommandLine&		commandLine;
	const BindingCallbackRecorder*		recorder;

	Environment (const PlatformInterface&	vkp_,
				 const InstanceInterface&	vki_,
				 VkInstance					instance_,
				 VkPhysicalDevice			physicalDevice_,
				 const DeviceInterface&		vkd_,
				 VkDevice					device_,
				 deUint32					queueFamilyIndex_,
				 const BinaryCollection&	programBinaries_,
				 const tcu::CommandLine&	commandLine_,
				 const BindingCallbackRecorder*	recorder_)
		: vkp				(vkp_)
		, vki				(vki_)
		, instance			(instance_)
		, physicalDevice	(physicalDevice_)
		, vkd				(vkd_)
		, device			(device_)
		, queueFamilyIndex	(queueFamilyIndex_)
		, programBinaries	(programBinaries_)
		, commandLine		(commandLine_)
		, recorder			(recorder_)
	{
	}
};

template<typename Case>
struct Dependency
{
	typename Case::Resources	resources;
	Unique<typename Case::Type>	object;

	Dependency (const Environment& env, const typename Case::Parameters& params)
		: resources	(env, params)
		, object	(Case::create(env, resources, params))
	{}
};

static Move<VkDevice> createDeviceWithAdressBindingReport (	deBool								isValidationEnabled,
															const PlatformInterface&			vkp,
															VkInstance							instance,
															const InstanceInterface&			vki,
															VkPhysicalDevice					physicalDevice,
															deUint32							queueFamilyIndex)
{
	const deUint32											queueCount						= 1;
	const float												queuePriority					= 1.0f;
	const char* const										enabledExtensions[]				= {"VK_EXT_device_address_binding_report"};
	VkPhysicalDeviceFeatures								features						= getPhysicalDeviceFeatures(vki, physicalDevice);

	VkPhysicalDeviceAddressBindingReportFeaturesEXT deviceAddressBindingReportFeatures
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ADDRESS_BINDING_REPORT_FEATURES_EXT,
		DE_NULL,
		VK_TRUE
	};

	const VkPhysicalDeviceFeatures2						enabledFeatures2				=
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,							// VkStructureType						sType;
		&deviceAddressBindingReportFeatures,									// const void*							pNext;
		features																// VkPhysicalDeviceFeatures				features;
	};
	const VkDeviceQueueCreateInfo						queueCreateInfo					=
	{
		VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,								// VkStructureType						sType;
		DE_NULL,																// const void*							pNext;
		(VkDeviceQueueCreateFlags)0,											// VkDeviceQueueCreateFlags				flags;
		queueFamilyIndex,														// deUint32								queueFamilyIndex;
		queueCount,																// deUint32								queueCount;
		&queuePriority,															// const float*							pQueuePriorities;
	};
	const VkDeviceCreateInfo							deviceCreateInfo				=
	{
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,									// VkStructureType						sType;
		&enabledFeatures2,														// const void*							pNext;
		(VkDeviceCreateFlags)0,													// VkDeviceCreateFlags					flags;
		queueCount,																// uint32_t								queueCreateInfoCount;
		&queueCreateInfo,														// const VkDeviceQueueCreateInfo*		pQueueCreateInfos;
		0u,																		// uint32_t								enabledLayerCount;
		DE_NULL,																// const char* const*					ppEnabledLayerNames;
		DE_LENGTH_OF_ARRAY(enabledExtensions),									// uint32_t								enabledExtensionCount;
		DE_ARRAY_BEGIN(enabledExtensions),										// const char* const*					ppEnabledExtensionNames;
		DE_NULL,																// const VkPhysicalDeviceFeatures*		pEnabledFeatures;
	};

	return createCustomDevice(isValidationEnabled, vkp, instance, vki, physicalDevice, &deviceCreateInfo);
}


struct Device
{
	typedef VkDevice Type;

	struct Parameters
	{
		Parameters (void) {}
	};

	struct Resources
	{
		Resources (const Environment&, const Parameters&) {}
	};

	static Move<VkDevice> create (const Environment& env, const Resources&, const Parameters&)
	{
		return createDeviceWithAdressBindingReport(env.commandLine.isValidationEnabled(), env.vkp, env.instance, env.vki, env.physicalDevice, env.queueFamilyIndex);
	}
};

struct DeviceMemory
{
	typedef VkDeviceMemory Type;

	struct Parameters
	{
		VkDeviceSize	size;
		deUint32		memoryTypeIndex;

		Parameters (VkDeviceSize size_, deUint32 memoryTypeIndex_)
			: size				(size_)
			, memoryTypeIndex	(memoryTypeIndex_)
		{
			DE_ASSERT(memoryTypeIndex < VK_MAX_MEMORY_TYPES);
		}
	};

	struct Resources
	{
		Resources (const Environment&, const Parameters&) {}
	};

	static Move<VkDeviceMemory> create (const Environment& env, const Resources&, const Parameters& params)
	{
		const VkMemoryAllocateInfo	memoryAllocateInfo	=
		{
			VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,	// VkStructureType	sType;
			DE_NULL,								// const void*		pNext;
			params.size,							// VkDeviceSize		allocationSize;
			params.memoryTypeIndex,					// uint32_t			memoryTypeIndex;
		};

		return allocateMemory(env.vkd, env.device, &memoryAllocateInfo);
	}
};

DeviceMemory::Parameters getDeviceMemoryParameters (const VkMemoryRequirements& memReqs)
{
	return DeviceMemory::Parameters(memReqs.size, deCtz32(memReqs.memoryTypeBits));
}

DeviceMemory::Parameters getDeviceMemoryParameters (const Environment& env, VkImage image)
{
	return getDeviceMemoryParameters(getImageMemoryRequirements(env.vkd, env.device, image));
}

DeviceMemory::Parameters getDeviceMemoryParameters (const Environment& env, VkBuffer buffer)
{
	return getDeviceMemoryParameters(getBufferMemoryRequirements(env.vkd, env.device, buffer));
}

struct Buffer
{
	typedef VkBuffer Type;

	struct Parameters
	{
		VkDeviceSize		size;
		VkBufferUsageFlags	usage;

		Parameters (VkDeviceSize		size_,
					VkBufferUsageFlags	usage_)
			: size	(size_)
			, usage	(usage_)
		{}
	};

	struct Resources
	{
		Resources (const Environment&, const Parameters&) {}
	};

	static Move<VkBuffer> create (const Environment& env, const Resources&, const Parameters& params)
	{
		const VkBufferCreateInfo	bufferCreateInfo	=
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,								// const void*			pNext;
			(VkBufferCreateFlags)0,					// VkBufferCreateFlags	flags;
			params.size,							// VkDeviceSize			size;
			params.usage,							// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode;
			1u,										// uint32_t				queueFamilyIndexCount;
			&env.queueFamilyIndex,					// const uint32_t*		pQueueFamilyIndices;
		};

		return createBuffer(env.vkd, env.device, &bufferCreateInfo);
	}
};

struct BufferView
{
	typedef VkBufferView Type;

	struct Parameters
	{
		Buffer::Parameters	buffer;
		VkFormat			format;
		VkDeviceSize		offset;
		VkDeviceSize		range;

		Parameters (const Buffer::Parameters&	buffer_,
					VkFormat					format_,
					VkDeviceSize				offset_,
					VkDeviceSize				range_)
			: buffer	(buffer_)
			, format	(format_)
			, offset	(offset_)
			, range		(range_)
		{}
	};

	struct Resources
	{
		Dependency<Buffer>			buffer;
		Dependency<DeviceMemory>	memory;

		Resources (const Environment& env, const Parameters& params)
			: buffer(env, params.buffer)
			, memory(env, getDeviceMemoryParameters(env, *buffer.object))
		{
			VK_CHECK(env.vkd.bindBufferMemory(env.device, *buffer.object, *memory.object, 0));
		}
	};

	static Move<VkBufferView> create (const Environment& env, const Resources& res, const Parameters& params)
	{
		const VkBufferViewCreateInfo	bufferViewCreateInfo	=
		{
			VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,	// VkStructureType			sType;
			DE_NULL,									// const void*				pNext;
			(VkBufferViewCreateFlags)0,					// VkBufferViewCreateFlags	flags;
			*res.buffer.object,							// VkBuffer					buffer;
			params.format,								// VkFormat					format;
			params.offset,								// VkDeviceSize				offset;
			params.range,								// VkDeviceSize				range;
		};

		return createBufferView(env.vkd, env.device, &bufferViewCreateInfo);
	}
};

struct Image
{
	typedef VkImage Type;

	struct Parameters
	{
		VkImageCreateFlags		flags;
		VkImageType				imageType;
		VkFormat				format;
		VkExtent3D				extent;
		deUint32				mipLevels;
		deUint32				arraySize;
		VkSampleCountFlagBits	samples;
		VkImageTiling			tiling;
		VkImageUsageFlags		usage;
		VkImageLayout			initialLayout;

		Parameters (VkImageCreateFlags		flags_,
					VkImageType				imageType_,
					VkFormat				format_,
					VkExtent3D				extent_,
					deUint32				mipLevels_,
					deUint32				arraySize_,
					VkSampleCountFlagBits	samples_,
					VkImageTiling			tiling_,
					VkImageUsageFlags		usage_,
					VkImageLayout			initialLayout_)
			: flags			(flags_)
			, imageType		(imageType_)
			, format		(format_)
			, extent		(extent_)
			, mipLevels		(mipLevels_)
			, arraySize		(arraySize_)
			, samples		(samples_)
			, tiling		(tiling_)
			, usage			(usage_)
			, initialLayout	(initialLayout_)
		{}
	};

	struct Resources
	{
		Resources (const Environment&, const Parameters&) {}
	};

	static Move<VkImage> create (const Environment& env, const Resources&, const Parameters& params)
	{
		const VkImageCreateInfo		imageCreateInfo	=
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType;
			DE_NULL,								// const void*				pNext;
			params.flags,							// VkImageCreateFlags		flags;
			params.imageType,						// VkImageType				imageType;
			params.format,							// VkFormat					format;
			params.extent,							// VkExtent3D				extent;
			params.mipLevels,						// uint32_t					mipLevels;
			params.arraySize,						// uint32_t					arrayLayers;
			params.samples,							// VkSampleCountFlagBits	samples;
			params.tiling,							// VkImageTiling			tiling;
			params.usage,							// VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode;
			1u,										// uint32_t					queueFamilyIndexCount;
			&env.queueFamilyIndex,					// const uint32_t*			pQueueFamilyIndices;
			params.initialLayout,					// VkImageLayout			initialLayout;
		};

		return createImage(env.vkd, env.device, &imageCreateInfo);
	}
};

struct ImageView
{
	typedef VkImageView Type;

	struct Parameters
	{
		Image::Parameters		image;
		VkImageViewType			viewType;
		VkFormat				format;
		VkComponentMapping		components;
		VkImageSubresourceRange	subresourceRange;

		Parameters (const Image::Parameters&	image_,
					VkImageViewType				viewType_,
					VkFormat					format_,
					VkComponentMapping			components_,
					VkImageSubresourceRange		subresourceRange_)
			: image				(image_)
			, viewType			(viewType_)
			, format			(format_)
			, components		(components_)
			, subresourceRange	(subresourceRange_)
		{}
	};

	struct Resources
	{
		Dependency<Image>			image;
		Dependency<DeviceMemory>	memory;

		Resources (const Environment& env, const Parameters& params)
			: image	(env, params.image)
			, memory(env, getDeviceMemoryParameters(env, *image.object))
		{
			VK_CHECK(env.vkd.bindImageMemory(env.device, *image.object, *memory.object, 0));
		}
	};

	static Move<VkImageView> create (const Environment& env, const Resources& res, const Parameters& params)
	{
		const VkImageViewCreateInfo	imageViewCreateInfo	=
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// VkStructureType			sType;
			DE_NULL,									// const void*				pNext;
			(VkImageViewCreateFlags)0,					// VkImageViewCreateFlags	flags;
			*res.image.object,							// VkImage					image;
			params.viewType,							// VkImageViewType			viewType;
			params.format,								// VkFormat					format;
			params.components,							// VkComponentMapping		components;
			params.subresourceRange,					// VkImageSubresourceRange	subresourceRange;
		};

		return createImageView(env.vkd, env.device, &imageViewCreateInfo);
	}
};

struct Semaphore
{
	typedef VkSemaphore Type;

	struct Parameters
	{
		VkSemaphoreCreateFlags	flags;

		Parameters (VkSemaphoreCreateFlags flags_)
			: flags(flags_)
		{}
	};

	struct Resources
	{
		Resources (const Environment&, const Parameters&) {}
	};

	static Move<VkSemaphore> create (const Environment& env, const Resources&, const Parameters& params)
	{
		const VkSemaphoreCreateInfo	semaphoreCreateInfo	=
		{
			VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,	// VkStructureType			sType;
			DE_NULL,									// const void*				pNext;
			params.flags,								// VkSemaphoreCreateFlags	flags;
		};

		return createSemaphore(env.vkd, env.device, &semaphoreCreateInfo);
	}
};

struct Fence
{
	typedef VkFence Type;

	struct Parameters
	{
		VkFenceCreateFlags	flags;

		Parameters (VkFenceCreateFlags flags_)
			: flags(flags_)
		{}
	};

	struct Resources
	{
		Resources (const Environment&, const Parameters&) {}
	};

	static Move<VkFence> create (const Environment& env, const Resources&, const Parameters& params)
	{
		const VkFenceCreateInfo	fenceCreateInfo	=
		{
			VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,								// const void*			pNext;
			params.flags,							// VkFenceCreateFlags	flags;
		};

		return createFence(env.vkd, env.device, &fenceCreateInfo);
	}
};

struct Event
{
	typedef VkEvent Type;

	struct Parameters
	{
		VkEventCreateFlags	flags;

		Parameters (VkEventCreateFlags flags_)
			: flags(flags_)
		{}
	};

	struct Resources
	{
		Resources (const Environment&, const Parameters&) {}
	};

	static Move<VkEvent> create (const Environment& env, const Resources&, const Parameters& params)
	{
		const VkEventCreateInfo	eventCreateInfo	=
		{
			VK_STRUCTURE_TYPE_EVENT_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,								// const void*			pNext;
			params.flags,							// VkEventCreateFlags	flags;
		};

		return createEvent(env.vkd, env.device, &eventCreateInfo);
	}
};

struct QueryPool
{
	typedef VkQueryPool Type;

	struct Parameters
	{
		VkQueryType						queryType;
		deUint32						entryCount;
		VkQueryPipelineStatisticFlags	pipelineStatistics;

		Parameters (VkQueryType						queryType_,
					deUint32						entryCount_,
					VkQueryPipelineStatisticFlags	pipelineStatistics_)
			: queryType				(queryType_)
			, entryCount			(entryCount_)
			, pipelineStatistics	(pipelineStatistics_)
		{}
	};

	struct Resources
	{
		Resources (const Environment&, const Parameters&) {}
	};

	static Move<VkQueryPool> create (const Environment& env, const Resources&, const Parameters& params)
	{
		const VkQueryPoolCreateInfo	queryPoolCreateInfo	=
		{
			VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,	// VkStructureType					sType;
			DE_NULL,									// const void*						pNext;
			(VkQueryPoolCreateFlags)0,					// VkQueryPoolCreateFlags			flags;
			params.queryType,							// VkQueryType						queryType;
			params.entryCount,							// uint32_t							queryCount;
			params.pipelineStatistics,					// VkQueryPipelineStatisticFlags	pipelineStatistics;
		};

		return createQueryPool(env.vkd, env.device, &queryPoolCreateInfo);
	}
};

struct ShaderModule
{
	typedef VkShaderModule Type;

	struct Parameters
	{
		VkShaderStageFlagBits	shaderStage;
		std::string				binaryName;

		Parameters (VkShaderStageFlagBits	shaderStage_,
					const std::string&		binaryName_)
			: shaderStage	(shaderStage_)
			, binaryName	(binaryName_)
		{}
	};

	struct Resources
	{
		const ProgramBinary&	binary;

		Resources (const Environment& env, const Parameters& params)
			: binary(env.programBinaries.get(params.binaryName))
		{}
	};

	static const char* getSource (VkShaderStageFlagBits stage)
	{
		switch (stage)
		{
			case VK_SHADER_STAGE_VERTEX_BIT:
				return "#version 310 es\n"
					   "layout(location = 0) in highp vec4 a_position;\n"
					   "void main () { gl_Position = a_position; }\n";

			case VK_SHADER_STAGE_FRAGMENT_BIT:
				return "#version 310 es\n"
					   "layout(location = 0) out mediump vec4 o_color;\n"
					   "void main () { o_color = vec4(1.0, 0.5, 0.25, 1.0); }";

			case VK_SHADER_STAGE_COMPUTE_BIT:
				return "#version 310 es\n"
					   "layout(binding = 0) buffer Input { highp uint dataIn[]; };\n"
					   "layout(binding = 1) buffer Output { highp uint dataOut[]; };\n"
					   "void main (void)\n"
					   "{\n"
					   "	dataOut[gl_GlobalInvocationID.x] = ~dataIn[gl_GlobalInvocationID.x];\n"
					   "}\n";

			default:
				DE_FATAL("Not implemented");
				return DE_NULL;
		}
	}

	static void initPrograms (SourceCollections& dst, Parameters params)
	{
		const char* const	source	= getSource(params.shaderStage);

		DE_ASSERT(source);

		dst.glslSources.add(params.binaryName)
			<< glu::ShaderSource(getGluShaderType(params.shaderStage), source);
	}

	static Move<VkShaderModule> create (const Environment& env, const Resources& res, const Parameters&)
	{
		const VkShaderModuleCreateInfo	shaderModuleCreateInfo	=
		{
			VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,	// VkStructureType				sType;
			DE_NULL,										// const void*					pNext;
			(VkShaderModuleCreateFlags)0,					// VkShaderModuleCreateFlags	flags;
			res.binary.getSize(),							// size_t						codeSize;
			(const deUint32*)res.binary.getBinary(),		// const uint32_t*				pCode;
		};

		return createShaderModule(env.vkd, env.device, &shaderModuleCreateInfo);
	}
};

struct PipelineCache
{
	typedef VkPipelineCache Type;

	struct Parameters
	{
		Parameters (void) {}
	};

	struct Resources
	{
		Resources (const Environment&, const Parameters&) {}
	};

	static Move<VkPipelineCache> create (const Environment& env, const Resources&, const Parameters&)
	{
		const VkPipelineCacheCreateInfo	pipelineCacheCreateInfo	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,	// VkStructureType				sType;
			DE_NULL,										// const void*					pNext;
			(VkPipelineCacheCreateFlags)0u,					// VkPipelineCacheCreateFlags	flags;
			0u,												// size_t						initialDataSize;
			DE_NULL,										// const void*					pInitialData;
		};

		return createPipelineCache(env.vkd, env.device, &pipelineCacheCreateInfo);
	}
};

struct Sampler
{
	typedef VkSampler Type;

	struct Parameters
	{
		VkFilter				magFilter;
		VkFilter				minFilter;
		VkSamplerMipmapMode		mipmapMode;
		VkSamplerAddressMode	addressModeU;
		VkSamplerAddressMode	addressModeV;
		VkSamplerAddressMode	addressModeW;
		float					mipLodBias;
		VkBool32				anisotropyEnable;
		float					maxAnisotropy;
		VkBool32				compareEnable;
		VkCompareOp				compareOp;
		float					minLod;
		float					maxLod;
		VkBorderColor			borderColor;
		VkBool32				unnormalizedCoordinates;

		Parameters (void)
			: magFilter					(VK_FILTER_NEAREST)
			, minFilter					(VK_FILTER_NEAREST)
			, mipmapMode				(VK_SAMPLER_MIPMAP_MODE_NEAREST)
			, addressModeU				(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
			, addressModeV				(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
			, addressModeW				(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
			, mipLodBias				(0.0f)
			, anisotropyEnable			(VK_FALSE)
			, maxAnisotropy				(1.0f)
			, compareEnable				(VK_FALSE)
			, compareOp					(VK_COMPARE_OP_ALWAYS)
			, minLod					(-1000.f)
			, maxLod					(+1000.f)
			, borderColor				(VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK)
			, unnormalizedCoordinates	(VK_FALSE)
		{}
	};

	struct Resources
	{
		Resources (const Environment&, const Parameters&) {}
	};

	static Move<VkSampler> create (const Environment& env, const Resources&, const Parameters& params)
	{
		const VkSamplerCreateInfo	samplerCreateInfo	=
		{
			VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,								// const void*			pNext;
			(VkSamplerCreateFlags)0,				// VkSamplerCreateFlags	flags;
			params.magFilter,						// VkFilter				magFilter;
			params.minFilter,						// VkFilter				minFilter;
			params.mipmapMode,						// VkSamplerMipmapMode	mipmapMode;
			params.addressModeU,					// VkSamplerAddressMode	addressModeU;
			params.addressModeV,					// VkSamplerAddressMode	addressModeV;
			params.addressModeW,					// VkSamplerAddressMode	addressModeW;
			params.mipLodBias,						// float				mipLodBias;
			params.anisotropyEnable,				// VkBool32				anisotropyEnable;
			params.maxAnisotropy,					// float				maxAnisotropy;
			params.compareEnable,					// VkBool32				compareEnable;
			params.compareOp,						// VkCompareOp			compareOp;
			params.minLod,							// float				minLod;
			params.maxLod,							// float				maxLod;
			params.borderColor,						// VkBorderColor		borderColor;
			params.unnormalizedCoordinates,			// VkBool32				unnormalizedCoordinates;
		};

		return createSampler(env.vkd, env.device, &samplerCreateInfo);
	}
};

struct DescriptorSetLayout
{
	typedef VkDescriptorSetLayout Type;

	struct Parameters
	{
		struct Binding
		{
			deUint32			binding;
			VkDescriptorType	descriptorType;
			deUint32			descriptorCount;
			VkShaderStageFlags	stageFlags;
			bool				useImmutableSampler;

			Binding (deUint32			binding_,
					 VkDescriptorType	descriptorType_,
					 deUint32			descriptorCount_,
					 VkShaderStageFlags	stageFlags_,
					 bool				useImmutableSampler_)
				: binding				(binding_)
				, descriptorType		(descriptorType_)
				, descriptorCount		(descriptorCount_)
				, stageFlags			(stageFlags_)
				, useImmutableSampler	(useImmutableSampler_)
			{}

			Binding (void) {}
		};

		std::vector<Binding>	bindings;

		Parameters (const std::vector<Binding>& bindings_)
			: bindings(bindings_)
		{}

		static Parameters empty (void)
		{
			return Parameters(std::vector<Binding>());
		}

		static Parameters single (deUint32				binding,
								  VkDescriptorType		descriptorType,
								  deUint32				descriptorCount,
								  VkShaderStageFlags	stageFlags,
								  bool					useImmutableSampler = false)
		{
			std::vector<Binding> bindings;
			bindings.push_back(Binding(binding, descriptorType, descriptorCount, stageFlags, useImmutableSampler));
			return Parameters(bindings);
		}
	};

	struct Resources
	{
		std::vector<VkDescriptorSetLayoutBinding>	bindings;
		MovePtr<Dependency<Sampler>>				immutableSampler;
		std::vector<VkSampler>						immutableSamplersPtr;

		Resources (const Environment& env, const Parameters& params)
		{
			for (std::vector<Parameters::Binding>::const_iterator cur = params.bindings.begin(); cur != params.bindings.end(); cur++)
			{
				if (cur->useImmutableSampler && !immutableSampler)
				{
					immutableSampler = de::newMovePtr<Dependency<Sampler>>(env, Sampler::Parameters());

					if (cur->useImmutableSampler && immutableSamplersPtr.size() < (size_t)cur->descriptorCount)
						immutableSamplersPtr.resize(cur->descriptorCount, *immutableSampler->object);
				}
			}

			for (std::vector<Parameters::Binding>::const_iterator cur = params.bindings.begin(); cur != params.bindings.end(); cur++)
			{
				const VkDescriptorSetLayoutBinding	binding	=
				{
					cur->binding,														// uint32_t				binding;
					cur->descriptorType,												// VkDescriptorType		descriptorType;
					cur->descriptorCount,												// uint32_t				descriptorCount;
					cur->stageFlags,													// VkShaderStageFlags	stageFlags;
					(cur->useImmutableSampler ? &immutableSamplersPtr[0] : DE_NULL),	// const VkSampler*		pImmutableSamplers;
				};

				bindings.push_back(binding);
			}
		}
	};

	static Move<VkDescriptorSetLayout> create (const Environment& env, const Resources& res, const Parameters&)
	{
		const VkDescriptorSetLayoutCreateInfo	descriptorSetLayoutCreateInfo	=
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,	// VkStructureType						sType;
			DE_NULL,												// const void*							pNext;
			(VkDescriptorSetLayoutCreateFlags)0,					// VkDescriptorSetLayoutCreateFlags		flags;
			(deUint32)res.bindings.size(),							// uint32_t								bindingCount;
			(res.bindings.empty() ? DE_NULL : &res.bindings[0]),	// const VkDescriptorSetLayoutBinding*	pBindings;
		};

		return createDescriptorSetLayout(env.vkd, env.device, &descriptorSetLayoutCreateInfo);
	}
};

struct PipelineLayout
{
	typedef VkPipelineLayout Type;

	struct Parameters
	{
		std::vector<DescriptorSetLayout::Parameters>	descriptorSetLayouts;
		std::vector<VkPushConstantRange>				pushConstantRanges;

		Parameters (void) {}

		static Parameters empty (void)
		{
			return Parameters();
		}

		static Parameters singleDescriptorSet (const DescriptorSetLayout::Parameters& descriptorSetLayout)
		{
			Parameters params;
			params.descriptorSetLayouts.push_back(descriptorSetLayout);
			return params;
		}
	};

	struct Resources
	{
		typedef SharedPtr<Dependency<DescriptorSetLayout>>	DescriptorSetLayoutDepSp;
		typedef std::vector<DescriptorSetLayoutDepSp>		DescriptorSetLayouts;

		DescriptorSetLayouts				descriptorSetLayouts;
		std::vector<VkDescriptorSetLayout>	pSetLayouts;

		Resources (const Environment& env, const Parameters& params)
		{
			for (std::vector<DescriptorSetLayout::Parameters>::const_iterator dsParams = params.descriptorSetLayouts.begin();
				 dsParams != params.descriptorSetLayouts.end();
				 ++dsParams)
			{
				descriptorSetLayouts.push_back(DescriptorSetLayoutDepSp(new Dependency<DescriptorSetLayout>(env, *dsParams)));
				pSetLayouts.push_back(*descriptorSetLayouts.back()->object);
			}
		}
	};

	static Move<VkPipelineLayout> create (const Environment& env, const Resources& res, const Parameters& params)
	{
		const VkPipelineLayoutCreateInfo	pipelineLayoutCreateInfo	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,									// VkStructureType				sType;
			DE_NULL,																		// const void*					pNext;
			(VkPipelineLayoutCreateFlags)0,													// VkPipelineLayoutCreateFlags	flags;
			(deUint32)res.pSetLayouts.size(),												// uint32_t						setLayoutCount;
			(res.pSetLayouts.empty() ? DE_NULL : &res.pSetLayouts[0]),						// const VkDescriptorSetLayout*	pSetLayouts;
			(deUint32)params.pushConstantRanges.size(),										// uint32_t						pushConstantRangeCount;
			(params.pushConstantRanges.empty() ? DE_NULL : &params.pushConstantRanges[0]),	// const VkPushConstantRange*	pPushConstantRanges;
		};

		return createPipelineLayout(env.vkd, env.device, &pipelineLayoutCreateInfo);
	}
};

struct RenderPass
{
	typedef VkRenderPass Type;

	struct Parameters
	{
		Parameters (void) {}
	};

	struct Resources
	{
		Resources (const Environment&, const Parameters&) {}
	};

	static Move<VkRenderPass> create (const Environment& env, const Resources&, const Parameters&)
	{
		return makeRenderPass(env.vkd, env.device, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_D16_UNORM,
			VK_ATTACHMENT_LOAD_OP_CLEAR,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
	}
};

struct GraphicsPipeline
{
	typedef VkPipeline Type;

	struct Parameters
	{
		Parameters (void) {}
	};

	struct Resources
	{
		Dependency<ShaderModule>	vertexShader;
		Dependency<ShaderModule>	fragmentShader;
		Dependency<PipelineLayout>	layout;
		Dependency<RenderPass>		renderPass;
		Dependency<PipelineCache>	pipelineCache;

		Resources (const Environment& env, const Parameters&)
			: vertexShader		(env, ShaderModule::Parameters(VK_SHADER_STAGE_VERTEX_BIT, "vert"))
			, fragmentShader	(env, ShaderModule::Parameters(VK_SHADER_STAGE_FRAGMENT_BIT, "frag"))
			, layout			(env, PipelineLayout::Parameters::singleDescriptorSet(
										DescriptorSetLayout::Parameters::single(0u, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1u, VK_SHADER_STAGE_FRAGMENT_BIT, true)))
			, renderPass		(env, RenderPass::Parameters())
			, pipelineCache		(env, PipelineCache::Parameters())
		{}
	};

	static void initPrograms (SourceCollections& dst, Parameters)
	{
		ShaderModule::initPrograms(dst, ShaderModule::Parameters(VK_SHADER_STAGE_VERTEX_BIT, "vert"));
		ShaderModule::initPrograms(dst, ShaderModule::Parameters(VK_SHADER_STAGE_FRAGMENT_BIT, "frag"));
	}

	static Move<VkPipeline> create (const Environment& env, const Resources& res, const Parameters&)
	{
		const VkPipelineShaderStageCreateInfo			stages[]			=
		{
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,		// VkStructureType									sType;
				DE_NULL,													// const void*										pNext;
				(VkPipelineShaderStageCreateFlags)0,						// VkPipelineShaderStageCreateFlags					flags;
				VK_SHADER_STAGE_VERTEX_BIT,									// VkShaderStageFlagBits							stage;
				*res.vertexShader.object,									// VkShaderModule									module;
				"main",														// const char*										pName;
				DE_NULL,													// const VkSpecializationInfo*						pSpecializationInfo;
			},
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,		// VkStructureType									sType;
				DE_NULL,													// const void*										pNext;
				(VkPipelineShaderStageCreateFlags)0,						// VkPipelineShaderStageCreateFlags					flags;
				VK_SHADER_STAGE_FRAGMENT_BIT,								// VkShaderStageFlagBits							stage;
				*res.fragmentShader.object,									// VkShaderModule									module;
				"main",														// const char*										pName;
				DE_NULL,													// const VkSpecializationInfo*						pSpecializationInfo;
			}
		};
		const VkVertexInputBindingDescription			vertexBindings[]	=
		{
			{
				0u,															// uint32_t											binding;
				16u,														// uint32_t											stride;
				VK_VERTEX_INPUT_RATE_VERTEX,								// VkVertexInputRate								inputRate;
			}
		};
		const VkVertexInputAttributeDescription			vertexAttribs[]		=
		{
			{
				0u,															// uint32_t											location;
				0u,															// uint32_t											binding;
				VK_FORMAT_R32G32B32A32_SFLOAT,								// VkFormat											format;
				0u,															// uint32_t											offset;
			}
		};
		const VkPipelineVertexInputStateCreateInfo		vertexInputState	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType									sType;
			DE_NULL,														// const void*										pNext;
			(VkPipelineVertexInputStateCreateFlags)0,						// VkPipelineVertexInputStateCreateFlags			flags;
			DE_LENGTH_OF_ARRAY(vertexBindings),								// uint32_t											vertexBindingDescriptionCount;
			vertexBindings,													// const VkVertexInputBindingDescription*			pVertexBindingDescriptions;
			DE_LENGTH_OF_ARRAY(vertexAttribs),								// uint32_t											vertexAttributeDescriptionCount;
			vertexAttribs,													// const VkVertexInputAttributeDescription*			pVertexAttributeDescriptions;
		};
		const VkPipelineInputAssemblyStateCreateInfo	inputAssemblyState	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType									sType;
			DE_NULL,														// const void*										pNext;
			(VkPipelineInputAssemblyStateCreateFlags)0,						// VkPipelineInputAssemblyStateCreateFlags			flags;
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,							// VkPrimitiveTopology								topology;
			VK_FALSE,														// VkBool32											primitiveRestartEnable;
		};
		const VkViewport								viewport			= makeViewport(tcu::UVec2(64));
		const VkRect2D									scissor				= makeRect2D(tcu::UVec2(64));

		const VkPipelineViewportStateCreateInfo			viewportState		=
		{
			VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,			// VkStructureType									sType;
			DE_NULL,														// const void*										pNext;
			(VkPipelineViewportStateCreateFlags)0,							// VkPipelineViewportStateCreateFlags				flags;
			1u,																// uint32_t											viewportCount;
			&viewport,														// const VkViewport*								pViewports;
			1u,																// uint32_t											scissorCount;
			&scissor,														// const VkRect2D*									pScissors;
		};
		const VkPipelineRasterizationStateCreateInfo	rasterState			=
		{
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,		// VkStructureType									sType;
			DE_NULL,														// const void*										pNext;
			(VkPipelineRasterizationStateCreateFlags)0,						// VkPipelineRasterizationStateCreateFlags			flags;
			VK_FALSE,														// VkBool32											depthClampEnable;
			VK_FALSE,														// VkBool32											rasterizerDiscardEnable;
			VK_POLYGON_MODE_FILL,											// VkPolygonMode									polygonMode;
			VK_CULL_MODE_BACK_BIT,											// VkCullModeFlags									cullMode;
			VK_FRONT_FACE_COUNTER_CLOCKWISE,								// VkFrontFace										frontFace;
			VK_FALSE,														// VkBool32											depthBiasEnable;
			0.0f,															// float											depthBiasConstantFactor;
			0.0f,															// float											depthBiasClamp;
			0.0f,															// float											depthBiasSlopeFactor;
			1.0f,															// float											lineWidth;
		};
		const VkPipelineMultisampleStateCreateInfo		multisampleState	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,		// VkStructureType									sType;
			DE_NULL,														// const void*										pNext;
			(VkPipelineMultisampleStateCreateFlags)0,						// VkPipelineMultisampleStateCreateFlags			flags;
			VK_SAMPLE_COUNT_1_BIT,											// VkSampleCountFlagBits							rasterizationSamples;
			VK_FALSE,														// VkBool32											sampleShadingEnable;
			1.0f,															// float											minSampleShading;
			DE_NULL,														// const VkSampleMask*								pSampleMask;
			VK_FALSE,														// VkBool32											alphaToCoverageEnable;
			VK_FALSE,														// VkBool32											alphaToOneEnable;
		};
		const VkPipelineDepthStencilStateCreateInfo		depthStencilState	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,		// VkStructureType									sType;
			DE_NULL,														// const void*										pNext;
			(VkPipelineDepthStencilStateCreateFlags)0,						// VkPipelineDepthStencilStateCreateFlags			flags;
			VK_TRUE,														// VkBool32											depthTestEnable;
			VK_TRUE,														// VkBool32											depthWriteEnable;
			VK_COMPARE_OP_LESS,												// VkCompareOp										depthCompareOp;
			VK_FALSE,														// VkBool32											depthBoundsTestEnable;
			VK_FALSE,														// VkBool32											stencilTestEnable;
			{
				VK_STENCIL_OP_KEEP,											// VkStencilOp										failOp;
				VK_STENCIL_OP_KEEP,											// VkStencilOp										passOp;
				VK_STENCIL_OP_KEEP,											// VkStencilOp										depthFailOp;
				VK_COMPARE_OP_ALWAYS,										// VkCompareOp										compareOp;
				0u,															// uint32_t											compareMask;
				0u,															// uint32_t											writeMask;
				0u,															// uint32_t											reference;
			},
			{
				VK_STENCIL_OP_KEEP,											// VkStencilOp										failOp;
				VK_STENCIL_OP_KEEP,											// VkStencilOp										passOp;
				VK_STENCIL_OP_KEEP,											// VkStencilOp										depthFailOp;
				VK_COMPARE_OP_ALWAYS,										// VkCompareOp										compareOp;
				0u,															// uint32_t											compareMask;
				0u,															// uint32_t											writeMask;
				0u,															// uint32_t											reference;
			},
			0.0f,															// float											minDepthBounds;
			1.0f,															// float											maxDepthBounds;
		};
		const VkPipelineColorBlendAttachmentState		colorBlendAttState[]=
		{
			{
				VK_FALSE,													// VkBool32											blendEnable;
				VK_BLEND_FACTOR_ONE,										// VkBlendFactor									srcColorBlendFactor;
				VK_BLEND_FACTOR_ZERO,										// VkBlendFactor									dstColorBlendFactor;
				VK_BLEND_OP_ADD,											// VkBlendOp										colorBlendOp;
				VK_BLEND_FACTOR_ONE,										// VkBlendFactor									srcAlphaBlendFactor;
				VK_BLEND_FACTOR_ZERO,										// VkBlendFactor									dstAlphaBlendFactor;
				VK_BLEND_OP_ADD,											// VkBlendOp										alphaBlendOp;
				VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
				VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,		// VkColorComponentFlags							colorWriteMask;
			}
		};
		const VkPipelineColorBlendStateCreateInfo		colorBlendState		=
		{
			VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,		// VkStructureType									sType;
			DE_NULL,														// const void*										pNext;
			(VkPipelineColorBlendStateCreateFlags)0,						// VkPipelineColorBlendStateCreateFlags				flags;
			VK_FALSE,														// VkBool32											logicOpEnable;
			VK_LOGIC_OP_COPY,												// VkLogicOp										logicOp;
			DE_LENGTH_OF_ARRAY(colorBlendAttState),							// uint32_t											attachmentCount;
			colorBlendAttState,												// const VkPipelineColorBlendAttachmentState*		pAttachments;
			{ 0.0f, 0.0f, 0.0f, 0.0f },										// float											blendConstants[4];
		};
		const VkGraphicsPipelineCreateInfo				pipelineCreateInfo	=
		{
			VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,				// VkStructureType									sType;
			DE_NULL,														// const void*										pNext;
			(VkPipelineCreateFlags)0,										// VkPipelineCreateFlags							flags;
			DE_LENGTH_OF_ARRAY(stages),										// uint32_t											stageCount;
			stages,															// const VkPipelineShaderStageCreateInfo*			pStages;
			&vertexInputState,												// const VkPipelineVertexInputStateCreateInfo*		pVertexInputState;
			&inputAssemblyState,											// const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState;
			DE_NULL,														// const VkPipelineTessellationStateCreateInfo*		pTessellationState;
			&viewportState,													// const VkPipelineViewportStateCreateInfo*			pViewportState;
			&rasterState,													// const VkPipelineRasterizationStateCreateInfo*	pRasterizationState;
			&multisampleState,												// const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
			&depthStencilState,												// const VkPipelineDepthStencilStateCreateInfo*		pDepthStencilState;
			&colorBlendState,												// const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
			(const VkPipelineDynamicStateCreateInfo*)DE_NULL,				// const VkPipelineDynamicStateCreateInfo*			pDynamicState;
			*res.layout.object,												// VkPipelineLayout									layout;
			*res.renderPass.object,											// VkRenderPass										renderPass;
			0u,																// uint32_t											subpass;
			(VkPipeline)0,													// VkPipeline										basePipelineHandle;
			0,																// int32_t											basePipelineIndex;
		};

		return createGraphicsPipeline(env.vkd, env.device, *res.pipelineCache.object, &pipelineCreateInfo);
	}
};

struct ComputePipeline
{
	typedef VkPipeline Type;

	struct Parameters
	{
		Parameters (void) {}
	};

	struct Resources
	{
		Dependency<ShaderModule>	shaderModule;
		Dependency<PipelineLayout>	layout;
		Dependency<PipelineCache>	pipelineCache;

		static DescriptorSetLayout::Parameters getDescriptorSetLayout (void)
		{
			typedef DescriptorSetLayout::Parameters::Binding	Binding;

			std::vector<Binding>	bindings;

			bindings.push_back(Binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u, VK_SHADER_STAGE_COMPUTE_BIT, false));
			bindings.push_back(Binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u, VK_SHADER_STAGE_COMPUTE_BIT, false));

			return DescriptorSetLayout::Parameters(bindings);
		}

		Resources (const Environment& env, const Parameters&)
			: shaderModule		(env, ShaderModule::Parameters(VK_SHADER_STAGE_COMPUTE_BIT, "comp"))
			, layout			(env, PipelineLayout::Parameters::singleDescriptorSet(getDescriptorSetLayout()))
			, pipelineCache		(env, PipelineCache::Parameters())
		{}
	};

	static void initPrograms (SourceCollections& dst, Parameters)
	{
		ShaderModule::initPrograms(dst, ShaderModule::Parameters(VK_SHADER_STAGE_COMPUTE_BIT, "comp"));
	}

	static Move<VkPipeline> create (const Environment& env, const Resources& res, const Parameters&)
	{
		const VkComputePipelineCreateInfo	pipelineCreateInfo	=
		{
			VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,				// VkStructureType					sType;
			DE_NULL,													// const void*						pNext;
			(VkPipelineCreateFlags)0,									// VkPipelineCreateFlags			flags;
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType					sType;
				DE_NULL,												// const void*						pNext;
				(VkPipelineShaderStageCreateFlags)0,					// VkPipelineShaderStageCreateFlags	flags;
				VK_SHADER_STAGE_COMPUTE_BIT,							// VkShaderStageFlagBits			stage;
				*res.shaderModule.object,								// VkShaderModule					module;
				"main",													// const char*						pName;
				DE_NULL,												// const VkSpecializationInfo*		pSpecializationInfo;
			},
			*res.layout.object,											// VkPipelineLayout					layout;
			(VkPipeline)0,												// VkPipeline						basePipelineHandle;
			0u,															// int32_t							basePipelineIndex;
		};

		return createComputePipeline(env.vkd, env.device, *res.pipelineCache.object, &pipelineCreateInfo);
	}
};

struct DescriptorPool
{
	typedef VkDescriptorPool Type;

	struct Parameters
	{
		VkDescriptorPoolCreateFlags			flags;
		deUint32							maxSets;
		std::vector<VkDescriptorPoolSize>	poolSizes;

		Parameters (VkDescriptorPoolCreateFlags					flags_,
					deUint32									maxSets_,
					const std::vector<VkDescriptorPoolSize>&	poolSizes_)
			: flags		(flags_)
			, maxSets	(maxSets_)
			, poolSizes	(poolSizes_)
		{}

		static Parameters singleType (VkDescriptorPoolCreateFlags	flags,
									  deUint32						maxSets,
									  VkDescriptorType				type,
									  deUint32						count)
		{
			std::vector<VkDescriptorPoolSize>	poolSizes;
			poolSizes.push_back(makeDescriptorPoolSize(type, count));
			return Parameters(flags, maxSets, poolSizes);
		}
	};

	struct Resources
	{
		Resources (const Environment&, const Parameters&) {}
	};

	static Move<VkDescriptorPool> create (const Environment& env, const Resources&, const Parameters& params)
	{
		const VkDescriptorPoolCreateInfo	descriptorPoolCreateInfo	=
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,					// VkStructureType				sType;
			DE_NULL,														// const void*					pNext;
			params.flags,													// VkDescriptorPoolCreateFlags	flags;
			params.maxSets,													// uint32_t						maxSets;
			(deUint32)params.poolSizes.size(),								// uint32_t						poolSizeCount;
			(params.poolSizes.empty() ? DE_NULL : &params.poolSizes[0]),	// const VkDescriptorPoolSize*	pPoolSizes;
		};

		return createDescriptorPool(env.vkd, env.device, &descriptorPoolCreateInfo);
	}
};

struct DescriptorSet
{
	typedef VkDescriptorSet Type;

	struct Parameters
	{
		DescriptorSetLayout::Parameters	descriptorSetLayout;

		Parameters (const DescriptorSetLayout::Parameters& descriptorSetLayout_)
			: descriptorSetLayout(descriptorSetLayout_)
		{}
	};

	struct Resources
	{
		Dependency<DescriptorPool>		descriptorPool;
		Dependency<DescriptorSetLayout>	descriptorSetLayout;

		static std::vector<VkDescriptorPoolSize> computePoolSizes (const DescriptorSetLayout::Parameters& layout, int maxSets)
		{
			deUint32							countByType[VK_DESCRIPTOR_TYPE_LAST];
			std::vector<VkDescriptorPoolSize>	typeCounts;

			std::fill(DE_ARRAY_BEGIN(countByType), DE_ARRAY_END(countByType), 0u);

			for (std::vector<DescriptorSetLayout::Parameters::Binding>::const_iterator cur = layout.bindings.begin(); cur != layout.bindings.end(); cur++)
			{
				DE_ASSERT((deUint32)cur->descriptorType < VK_DESCRIPTOR_TYPE_LAST);
				countByType[cur->descriptorType] += cur->descriptorCount * maxSets;
			}

			for (deUint32 type = 0; type < VK_DESCRIPTOR_TYPE_LAST; type++)
			{
				if (countByType[type] > 0)
					typeCounts.push_back(makeDescriptorPoolSize((VkDescriptorType)type, countByType[type]));
			}

			return typeCounts;
		}

		Resources (const Environment& env, const Parameters& params)
			: descriptorPool		(env, DescriptorPool::Parameters(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u, computePoolSizes(params.descriptorSetLayout, 1u)))
			, descriptorSetLayout	(env, params.descriptorSetLayout)
		{
		}
	};

	static Move<VkDescriptorSet> create (const Environment& env, const Resources& res, const Parameters&)
	{
		const VkDescriptorSetAllocateInfo	allocateInfo	=
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,	// VkStructureType				sType;
			DE_NULL,										// const void*					pNext;
			*res.descriptorPool.object,						// VkDescriptorPool				descriptorPool;
			1u,												// uint32_t						descriptorSetCount;
			&(*res.descriptorSetLayout.object),				// const VkDescriptorSetLayout*	pSetLayouts;
		};

		return allocateDescriptorSet(env.vkd, env.device, &allocateInfo);
	}
};

struct Framebuffer
{
	typedef VkFramebuffer Type;

	struct Parameters
	{
		Parameters (void)
		{}
	};

	struct Resources
	{
		Dependency<ImageView>	colorAttachment;
		Dependency<ImageView>	depthStencilAttachment;
		Dependency<RenderPass>	renderPass;

		Resources (const Environment& env, const Parameters&)
			: colorAttachment			(env, ImageView::Parameters(Image::Parameters(0u, VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM,
																					  makeExtent3D(256, 256, 1),
																					  1u, 1u,
																					  VK_SAMPLE_COUNT_1_BIT,
																					  VK_IMAGE_TILING_OPTIMAL,
																					  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
																					  VK_IMAGE_LAYOUT_UNDEFINED),
																		 VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM,
																		 makeComponentMappingRGBA(),
																		 makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u)))
			, depthStencilAttachment	(env, ImageView::Parameters(Image::Parameters(0u, VK_IMAGE_TYPE_2D, VK_FORMAT_D16_UNORM,
																					  makeExtent3D(256, 256, 1),
																					  1u, 1u,
																					  VK_SAMPLE_COUNT_1_BIT,
																					  VK_IMAGE_TILING_OPTIMAL,
																					  VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
																					  VK_IMAGE_LAYOUT_UNDEFINED),
																		 VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_D16_UNORM,
																		 makeComponentMappingRGBA(),
																		 makeImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u)))
			, renderPass				(env, RenderPass::Parameters())
		{}
	};

	static Move<VkFramebuffer> create (const Environment& env, const Resources& res, const Parameters&)
	{
		const VkImageView				attachments[]			=
		{
			*res.colorAttachment.object,
			*res.depthStencilAttachment.object,
		};
		const VkFramebufferCreateInfo	framebufferCreateInfo	=
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	// VkStructureType			sType;
			DE_NULL,									// const void*				pNext;
			(VkFramebufferCreateFlags)0,				// VkFramebufferCreateFlags	flags;
			*res.renderPass.object,						// VkRenderPass				renderPass;
			(deUint32)DE_LENGTH_OF_ARRAY(attachments),	// uint32_t					attachmentCount;
			attachments,								// const VkImageView*		pAttachments;
			256u,										// uint32_t					width;
			256u,										// uint32_t					height;
			1u,											// uint32_t					layers;
		};

		return createFramebuffer(env.vkd, env.device, &framebufferCreateInfo);
	}
};

struct CommandPool
{
	typedef VkCommandPool Type;

	struct Parameters
	{
		VkCommandPoolCreateFlags	flags;

		Parameters (VkCommandPoolCreateFlags flags_)
			: flags(flags_)
		{}
	};

	struct Resources
	{
		Resources (const Environment&, const Parameters&) {}
	};

	static Move<VkCommandPool> create (const Environment& env, const Resources&, const Parameters& params)
	{
		const VkCommandPoolCreateInfo	commandPoolCreateInfo	=
		{
			VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,	// VkStructureType			sType;
			DE_NULL,									// const void*				pNext;
			params.flags,								// VkCommandPoolCreateFlags	flags;
			env.queueFamilyIndex,						// uint32_t					queueFamilyIndex;
		};

		return createCommandPool(env.vkd, env.device, &commandPoolCreateInfo);
	}
};

struct CommandBuffer
{
	typedef VkCommandBuffer Type;

	struct Parameters
	{
		CommandPool::Parameters		commandPool;
		VkCommandBufferLevel		level;

		Parameters (const CommandPool::Parameters&	commandPool_,
					VkCommandBufferLevel			level_)
			: commandPool	(commandPool_)
			, level			(level_)
		{}
	};

	struct Resources
	{
		Dependency<CommandPool>	commandPool;

		Resources (const Environment& env, const Parameters& params)
			: commandPool(env, params.commandPool)
		{}
	};

	static Move<VkCommandBuffer> create (const Environment& env, const Resources& res, const Parameters& params)
	{
		const VkCommandBufferAllocateInfo	allocateInfo	=
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,	// VkStructureType		sType;
			DE_NULL,										// const void*			pNext;
			*res.commandPool.object,						// VkCommandPool		commandPool;
			params.level,									// VkCommandBufferLevel	level;
			1,												// uint32_t				commandBufferCount;
		};

		return allocateCommandBuffer(env.vkd, env.device, &allocateInfo);
	}
};

template<typename Object>
struct NamedParameters
{
	const char*					name;
	typename Object::Parameters	parameters;
};

template<typename Object>
struct CaseDescription
{
	typename FunctionInstance1<typename Object::Parameters>::Function	function;
	const NamedParameters<Object>*										paramsBegin;
	const NamedParameters<Object>*										paramsEnd;
};

#define CASE_DESC(FUNCTION, CASES) \
	{ FUNCTION, DE_ARRAY_BEGIN(CASES), DE_ARRAY_END(CASES) }

struct CaseDescriptions
{
	CaseDescription<Device>					device;
	CaseDescription<DeviceMemory>			deviceMemory;
	CaseDescription<Buffer>					buffer;
	CaseDescription<BufferView>				bufferView;
	CaseDescription<Image>					image;
	CaseDescription<ImageView>				imageView;
	CaseDescription<Semaphore>				semaphore;
	CaseDescription<Event>					event;
	CaseDescription<Fence>					fence;
	CaseDescription<QueryPool>				queryPool;
	CaseDescription<ShaderModule>			shaderModule;
	CaseDescription<PipelineCache>			pipelineCache;
	CaseDescription<Sampler>				sampler;
	CaseDescription<DescriptorSetLayout>	descriptorSetLayout;
	CaseDescription<PipelineLayout>			pipelineLayout;
	CaseDescription<RenderPass>				renderPass;
	CaseDescription<GraphicsPipeline>		graphicsPipeline;
	CaseDescription<ComputePipeline>		computePipeline;
	CaseDescription<DescriptorPool>			descriptorPool;
	CaseDescription<DescriptorSet>			descriptorSet;
	CaseDescription<Framebuffer>			framebuffer;
	CaseDescription<CommandPool>			commandPool;
	CaseDescription<CommandBuffer>			commandBuffer;
};

template<typename Object>
void addCases (const MovePtr<tcu::TestCaseGroup>& group, const CaseDescription<Object>& cases)
{
	for (const NamedParameters<Object>* cur = cases.paramsBegin; cur != cases.paramsEnd; cur++)
	{
		addFunctionCase(group.get(), cur->name, "", cases.function, cur->parameters);
	}
}

template<typename Object>
void addCasesWithProgs (const MovePtr<tcu::TestCaseGroup>& group, const CaseDescription<Object>& cases)
{
	for (const NamedParameters<Object>* cur = cases.paramsBegin; cur != cases.paramsEnd; cur++)
	{
		addFunctionCaseWithPrograms(group.get(), cur->name, "", Object::initPrograms, cases.function, cur->parameters);
	}
}

tcu::TestCaseGroup* createObjectTestsGroup (tcu::TestContext& testCtx, const char* name, const char* desc, const CaseDescriptions& cases)
{
	MovePtr<tcu::TestCaseGroup>	group	(new tcu::TestCaseGroup(testCtx, name, desc));

	addCases			(group, cases.device);
	addCases			(group, cases.deviceMemory);
	addCases			(group, cases.buffer);
	addCases			(group, cases.bufferView);
	addCases			(group, cases.image);
	addCases			(group, cases.imageView);
	addCases			(group, cases.semaphore);
	addCases			(group, cases.event);
	addCases			(group, cases.fence);
	addCases			(group, cases.queryPool);
	addCasesWithProgs	(group, cases.shaderModule);
	addCases			(group, cases.pipelineCache);
	addCases			(group, cases.sampler);
	addCases			(group, cases.descriptorSetLayout);
	addCases			(group, cases.pipelineLayout);
	addCases			(group, cases.renderPass);
	addCasesWithProgs	(group, cases.graphicsPipeline);
	addCasesWithProgs	(group, cases.computePipeline);
	addCases			(group, cases.descriptorPool);
	addCases			(group, cases.descriptorSet);
	addCases			(group, cases.framebuffer);
	addCases			(group, cases.commandPool);
	addCases			(group, cases.commandBuffer);

	return group.release();
}

static deBool validateCallbackRecords (Context& context, const BindingCallbackRecorder& recorder)
{
	tcu::TestLog&							log					= context.getTestContext().getLog();

	for (auto bindRecord = recorder.getRecordsBegin(); bindRecord != recorder.getRecordsEnd(); bindRecord++)
	{
		if (bindRecord->bindingType == VK_DEVICE_ADDRESS_BINDING_TYPE_BIND_EXT)
		{
			bool matchedBoundUnbound = false;

			for (auto pairRecord = bindRecord; pairRecord != recorder.getRecordsEnd(); pairRecord++)
			{
				if (pairRecord->bindingType == VK_DEVICE_ADDRESS_BINDING_TYPE_UNBIND_EXT)
				{
					if ((*bindRecord) == (*pairRecord))
					{
						log << tcu::TestLog::Message << "Bind/Unbind base adress:"		<< bindRecord->bindingAddress	<< tcu::TestLog::EndMessage;
						log << tcu::TestLog::Message << "Bind/Unbind size:"				<< bindRecord->size				<< tcu::TestLog::EndMessage;
						log << tcu::TestLog::Message << "Bind/Unbind object handle:"	<< bindRecord->objectHandle		<< tcu::TestLog::EndMessage;

						matchedBoundUnbound = true;
						break;
					}
				}
			}

			if (matchedBoundUnbound == false)
			{
				log << tcu::TestLog::Message << "Lonely bind base adress:"		<< bindRecord->bindingAddress	<< tcu::TestLog::EndMessage;
				log << tcu::TestLog::Message << "Lonely bind size:"				<< bindRecord->size				<< tcu::TestLog::EndMessage;
				log << tcu::TestLog::Message << "Lonely bind object handle:"	<< bindRecord->objectHandle		<< tcu::TestLog::EndMessage;

				return false;
			}
		}
		else if (bindRecord->bindingType == VK_DEVICE_ADDRESS_BINDING_TYPE_UNBIND_EXT)
		{
			bool matchedBoundUnbound = false;

			for (auto pairRecord = recorder.getRecordsBegin(); pairRecord != bindRecord; pairRecord++)
			{
				if (pairRecord->bindingType == VK_DEVICE_ADDRESS_BINDING_TYPE_BIND_EXT)
				{
					if ((*bindRecord) == (*pairRecord))
					{
						matchedBoundUnbound = true;
						break;
					}
				}
			}

			if (matchedBoundUnbound == false)
			{
				log << tcu::TestLog::Message << "Lonely unbind base adress:"	<< bindRecord->bindingAddress	<< tcu::TestLog::EndMessage;
				log << tcu::TestLog::Message << "Lonely unbind size:"			<< bindRecord->size				<< tcu::TestLog::EndMessage;
				log << tcu::TestLog::Message << "Lonely unbind object handle:"	<< bindRecord->objectHandle		<< tcu::TestLog::EndMessage;

				return false;
			}
		}
	}

	return true;
}

static std::vector<std::string> getInstanceExtensions(const deUint32 instanceVersion)
{
	std::vector<std::string> instanceExtensions;

	if (!isCoreInstanceExtension(instanceVersion, "VK_KHR_get_physical_device_properties2"))
		instanceExtensions.push_back("VK_KHR_get_physical_device_properties2");

	if (!isCoreInstanceExtension(instanceVersion, "VK_EXT_debug_utils"))
		instanceExtensions.push_back("VK_EXT_debug_utils");

	return instanceExtensions;
}

static bool checkSupport(CustomInstance& customInstance, vk::VkPhysicalDevice& physicalDevice)
{
	const std::vector<VkExtensionProperties> extensions = enumerateDeviceExtensionProperties(customInstance.getDriver(), physicalDevice, DE_NULL);

	for (size_t extNdx = 0; extNdx < extensions.size(); extNdx++)
	{
		if (deStringEqual("VK_EXT_device_address_binding_report", extensions[extNdx].extensionName))
		{
			VkPhysicalDeviceAddressBindingReportFeaturesEXT deviceAddressBindingReportFeatures
			{
				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ADDRESS_BINDING_REPORT_FEATURES_EXT,
				DE_NULL,
				VK_FALSE
			};

			VkPhysicalDeviceFeatures2 availFeatures;
			availFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
			availFeatures.pNext = &deviceAddressBindingReportFeatures;

			customInstance.getDriver().getPhysicalDeviceFeatures2(physicalDevice, &availFeatures);

			if (deviceAddressBindingReportFeatures.reportAddressBinding == VK_TRUE)
				return true;
			else
				return false;
		}
	}

	return false;
}

template<typename Object>
tcu::TestStatus createDestroyObjectTest (Context& context, typename Object::Parameters params)
{
	BindingCallbackRecorder	 recorder;
	VkDebugUtilsMessengerEXT messenger;

	CustomInstance          customInstance		= createCustomInstanceWithExtensions(context, getInstanceExtensions(context.getUsedApiVersion()));
	vk::VkPhysicalDevice	physicalDevice		= chooseDevice(customInstance.getDriver(), customInstance, context.getTestContext().getCommandLine());
	deUint32				queueFamilyIndex	= 0;

	if (!checkSupport(customInstance, physicalDevice))
	{
		TCU_THROW(NotSupportedError, "Device address binding report not supported");
	}

	const std::vector<VkQueueFamilyProperties>	queueProps = getPhysicalDeviceQueueFamilyProperties(customInstance.getDriver(), physicalDevice);

	for (size_t queueNdx = 0; queueNdx < queueProps.size(); queueNdx++)
	{
		if ((queueProps[queueNdx].queueFlags & VK_QUEUE_GRAPHICS_BIT) == VK_QUEUE_GRAPHICS_BIT)
		{
			queueFamilyIndex = (deUint32)queueNdx;
			break;
		}
	}

	VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo =
		{
			VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
			nullptr,
			0,
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
			VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT,
			recorder.callback,
			&recorder
		};

	customInstance.getDriver().createDebugUtilsMessengerEXT(
			customInstance,
			&debugUtilsMessengerCreateInfo,
			nullptr,
			&messenger);

	{
		Move<VkDevice> device = createDeviceWithAdressBindingReport(
			context.getTestContext().getCommandLine().isValidationEnabled(),
			context.getPlatformInterface(),
			customInstance,
			customInstance.getDriver(),
			physicalDevice,
			queueFamilyIndex);

		de::MovePtr<DeviceDriver> deviceInterface = de::MovePtr<DeviceDriver>(new DeviceDriver(context.getPlatformInterface(), customInstance, device.get(), context.getUsedApiVersion()));

		const Environment	env	(context.getPlatformInterface(),
								customInstance.getDriver(),
								customInstance,
								physicalDevice,
								*deviceInterface.get(),
								device.get(),
								queueFamilyIndex,
								context.getBinaryCollection(),
								context.getTestContext().getCommandLine(),
								&recorder);


		{
			const typename Object::Resources	res					(env, params);
			Unique<typename Object::Type>		obj	(Object::create(env, res, params));
		}
	}

	customInstance.getDriver().destroyDebugUtilsMessengerEXT(
			customInstance,
			messenger,
			nullptr);

	if (!validateCallbackRecords(context, recorder))
	{
		return tcu::TestStatus::fail("Invalid address binding report callback");
	}

	return tcu::TestStatus::pass("Ok");
}

} // anonymous


tcu::TestCaseGroup* createAddressBindingReportTests (tcu::TestContext& testCtx)
{
	MovePtr<tcu::TestCaseGroup>	addressBindingReportTests  (new tcu::TestCaseGroup(testCtx, "address_binding_report", "Address Binding Report tests"));

	const Image::Parameters		img1D			(0u,
												 VK_IMAGE_TYPE_1D,
												 VK_FORMAT_R8G8B8A8_UNORM,
												 makeExtent3D(256, 1, 1),
												 1u,
												 4u,
												 VK_SAMPLE_COUNT_1_BIT,
												 VK_IMAGE_TILING_OPTIMAL,
												 VK_IMAGE_USAGE_SAMPLED_BIT,
												 VK_IMAGE_LAYOUT_UNDEFINED);
	const Image::Parameters		img2D			(0u,
												 VK_IMAGE_TYPE_2D,
												 VK_FORMAT_R8G8B8A8_UNORM,
												 makeExtent3D(64, 64, 1),
												 1u,
												 12u,
												 VK_SAMPLE_COUNT_1_BIT,
												 VK_IMAGE_TILING_OPTIMAL,
												 VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
												 VK_IMAGE_LAYOUT_UNDEFINED);
	const Image::Parameters		imgCube			(VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
												 VK_IMAGE_TYPE_2D,
												 VK_FORMAT_R8G8B8A8_UNORM,
												 makeExtent3D(64, 64, 1),
												 1u,
												 12u,
												 VK_SAMPLE_COUNT_1_BIT,
												 VK_IMAGE_TILING_OPTIMAL,
												 VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
												 VK_IMAGE_LAYOUT_UNDEFINED);
	const Image::Parameters		img3D			(0u,
												 VK_IMAGE_TYPE_3D,
												 VK_FORMAT_R8G8B8A8_UNORM,
												 makeExtent3D(64, 64, 4),
												 1u,
												 1u,
												 VK_SAMPLE_COUNT_1_BIT,
												 VK_IMAGE_TILING_OPTIMAL,
												 VK_IMAGE_USAGE_SAMPLED_BIT,
												 VK_IMAGE_LAYOUT_UNDEFINED);
	const ImageView::Parameters	imgView1D		(img1D,
												 VK_IMAGE_VIEW_TYPE_1D,
												 img1D.format,
												 makeComponentMappingRGBA(),
												 makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u));
	const ImageView::Parameters	imgView1DArr	(img1D,
												 VK_IMAGE_VIEW_TYPE_1D_ARRAY,
												 img1D.format,
												 makeComponentMappingRGBA(),
												 makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 4u));
	const ImageView::Parameters	imgView2D		(img2D,
												 VK_IMAGE_VIEW_TYPE_2D,
												 img2D.format,
												 makeComponentMappingRGBA(),
												 makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u));
	const ImageView::Parameters	imgView2DArr	(img2D,
												 VK_IMAGE_VIEW_TYPE_2D_ARRAY,
												 img2D.format,
												 makeComponentMappingRGBA(),
												 makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 8u));
	const ImageView::Parameters	imgViewCube		(imgCube,VK_IMAGE_VIEW_TYPE_CUBE,
												 img2D.format,
												 makeComponentMappingRGBA(),
												 makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 6u));
	const ImageView::Parameters	imgViewCubeArr	(imgCube,
												 VK_IMAGE_VIEW_TYPE_CUBE_ARRAY,
												 img2D.format,
												 makeComponentMappingRGBA(),
												 makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 12u));
	const ImageView::Parameters	imgView3D		(img3D,
												 VK_IMAGE_VIEW_TYPE_3D,
												 img3D.format,
												 makeComponentMappingRGBA(),
												 makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u));

	const DescriptorSetLayout::Parameters	singleUboDescLayout	= DescriptorSetLayout::Parameters::single(0u, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u, VK_SHADER_STAGE_VERTEX_BIT);

	const NamedParameters<Device>						s_deviceCases[]					=
	{
		{ "device",						Device::Parameters()	},
	};
	static const NamedParameters<DeviceMemory>			s_deviceMemCases[]				=
	{
		{ "device_memory_small",		DeviceMemory::Parameters(1024, 0u)	},
	};
	static const NamedParameters<Buffer>				s_bufferCases[]					=
	{
		{ "buffer_uniform_small",		Buffer::Parameters(1024u,			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT),	},
		{ "buffer_uniform_large",		Buffer::Parameters(1024u*1024u*16u,	VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT),	},
		{ "buffer_storage_small",		Buffer::Parameters(1024u,			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),	},
		{ "buffer_storage_large",		Buffer::Parameters(1024u*1024u*16u,	VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),	},
	};
	static const NamedParameters<BufferView>			s_bufferViewCases[]				=
	{
		{ "buffer_view_uniform_r8g8b8a8_unorm",	BufferView::Parameters(Buffer::Parameters(8192u, VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT), VK_FORMAT_R8G8B8A8_UNORM, 0u, 4096u)	},
		{ "buffer_view_storage_r8g8b8a8_unorm",	BufferView::Parameters(Buffer::Parameters(8192u, VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT), VK_FORMAT_R8G8B8A8_UNORM, 0u, 4096u)	},
	};
	static const NamedParameters<Image>					s_imageCases[]					=
	{
		{ "image_1d",					img1D		},
		{ "image_2d",					img2D		},
		{ "image_3d",					img3D		},
	};
	static const NamedParameters<ImageView>				s_imageViewCases[]				=
	{
		{ "image_view_1d",				imgView1D		},
		{ "image_view_1d_arr",			imgView1DArr	},
		{ "image_view_2d",				imgView2D		},
		{ "image_view_2d_arr",			imgView2DArr	},
		{ "image_view_cube",			imgViewCube		},
		{ "image_view_cube_arr",		imgViewCubeArr	},
		{ "image_view_3d",				imgView3D		},
	};
	static const NamedParameters<Semaphore>				s_semaphoreCases[]				=
	{
		{ "semaphore",					Semaphore::Parameters(0u),	}
	};
	static const NamedParameters<Event>					s_eventCases[]					=
	{
		{ "event",						Event::Parameters(0u)		}
	};
	static const NamedParameters<Fence>					s_fenceCases[]					=
	{
		{ "fence",						Fence::Parameters(0u)								},
		{ "fence_signaled",				Fence::Parameters(VK_FENCE_CREATE_SIGNALED_BIT)		}
	};
	static const NamedParameters<QueryPool>				s_queryPoolCases[]				=
	{
		{ "query_pool",					QueryPool::Parameters(VK_QUERY_TYPE_OCCLUSION, 1u, 0u)	}
	};
	static const NamedParameters<ShaderModule>			s_shaderModuleCases[]			=
	{
		{ "shader_module",				ShaderModule::Parameters(VK_SHADER_STAGE_COMPUTE_BIT, "test")	}
	};
	static const NamedParameters<PipelineCache>			s_pipelineCacheCases[]			=
	{
		{ "pipeline_cache",				PipelineCache::Parameters()		}
	};
	static const NamedParameters<Sampler>				s_samplerCases[]				=
	{
		{ "sampler",					Sampler::Parameters()	}
	};
	static const NamedParameters<DescriptorSetLayout>	s_descriptorSetLayoutCases[]	=
	{
		{ "descriptor_set_layout_empty",	DescriptorSetLayout::Parameters::empty()	},
		{ "descriptor_set_layout_single",	singleUboDescLayout							}
	};
	static const NamedParameters<PipelineLayout>		s_pipelineLayoutCases[]			=
	{
		{ "pipeline_layout_empty",		PipelineLayout::Parameters::empty()										},
		{ "pipeline_layout_single",		PipelineLayout::Parameters::singleDescriptorSet(singleUboDescLayout)	}
	};
	static const NamedParameters<RenderPass>			s_renderPassCases[]				=
	{
		{ "render_pass",				RenderPass::Parameters()		}
	};
	static const NamedParameters<GraphicsPipeline>		s_graphicsPipelineCases[]		=
	{
		{ "graphics_pipeline",			GraphicsPipeline::Parameters()	}
	};
	static const NamedParameters<ComputePipeline>		s_computePipelineCases[]		=
	{
		{ "compute_pipeline",			ComputePipeline::Parameters()	}
	};
	static const NamedParameters<DescriptorPool>		s_descriptorPoolCases[]			=
	{
		{ "descriptor_pool",						DescriptorPool::Parameters::singleType((VkDescriptorPoolCreateFlags)0,						4u, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3u)	},
		{ "descriptor_pool_free_descriptor_set",	DescriptorPool::Parameters::singleType(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,	4u, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3u)	}
	};
	static const NamedParameters<DescriptorSet>			s_descriptorSetCases[]			=
	{
		{ "descriptor_set",				DescriptorSet::Parameters(singleUboDescLayout)	}
	};
	static const NamedParameters<Framebuffer>			s_framebufferCases[]			=
	{
		{ "framebuffer",				Framebuffer::Parameters()	}
	};
	static const NamedParameters<CommandPool>			s_commandPoolCases[]			=
	{
		{ "command_pool",				CommandPool::Parameters((VkCommandPoolCreateFlags)0)			},
		{ "command_pool_transient",		CommandPool::Parameters(VK_COMMAND_POOL_CREATE_TRANSIENT_BIT)	}
	};
	static const NamedParameters<CommandBuffer>			s_commandBufferCases[]			=
	{
		{ "command_buffer_primary",		CommandBuffer::Parameters(CommandPool::Parameters((VkCommandPoolCreateFlags)0u), VK_COMMAND_BUFFER_LEVEL_PRIMARY)	},
		{ "command_buffer_secondary",	CommandBuffer::Parameters(CommandPool::Parameters((VkCommandPoolCreateFlags)0u), VK_COMMAND_BUFFER_LEVEL_SECONDARY)	}
	};

	const CaseDescriptions	s_createDestroyObjectGroup	=
	{
		CASE_DESC(createDestroyObjectTest	<Device>,				s_deviceCases),
		CASE_DESC(createDestroyObjectTest	<DeviceMemory>,			s_deviceMemCases),
		CASE_DESC(createDestroyObjectTest	<Buffer>,				s_bufferCases),
		CASE_DESC(createDestroyObjectTest	<BufferView>,			s_bufferViewCases),
		CASE_DESC(createDestroyObjectTest	<Image>,				s_imageCases),
		CASE_DESC(createDestroyObjectTest	<ImageView>,			s_imageViewCases),
		CASE_DESC(createDestroyObjectTest	<Semaphore>,			s_semaphoreCases),
		CASE_DESC(createDestroyObjectTest	<Event>,				s_eventCases),
		CASE_DESC(createDestroyObjectTest	<Fence>,				s_fenceCases),
		CASE_DESC(createDestroyObjectTest	<QueryPool>,			s_queryPoolCases),
		CASE_DESC(createDestroyObjectTest	<ShaderModule>,			s_shaderModuleCases),
		CASE_DESC(createDestroyObjectTest	<PipelineCache>,		s_pipelineCacheCases),
		CASE_DESC(createDestroyObjectTest	<Sampler>,				s_samplerCases),
		CASE_DESC(createDestroyObjectTest	<DescriptorSetLayout>,	s_descriptorSetLayoutCases),
		CASE_DESC(createDestroyObjectTest	<PipelineLayout>,		s_pipelineLayoutCases),
		CASE_DESC(createDestroyObjectTest	<RenderPass>,			s_renderPassCases),
		CASE_DESC(createDestroyObjectTest	<GraphicsPipeline>,		s_graphicsPipelineCases),
		CASE_DESC(createDestroyObjectTest	<ComputePipeline>,		s_computePipelineCases),
		CASE_DESC(createDestroyObjectTest	<DescriptorPool>,		s_descriptorPoolCases),
		CASE_DESC(createDestroyObjectTest	<DescriptorSet>,		s_descriptorSetCases),
		CASE_DESC(createDestroyObjectTest	<Framebuffer>,			s_framebufferCases),
		CASE_DESC(createDestroyObjectTest	<CommandPool>,			s_commandPoolCases),
		CASE_DESC(createDestroyObjectTest	<CommandBuffer>,		s_commandBufferCases),
	};
	addressBindingReportTests->addChild(createObjectTestsGroup(testCtx, "create_and_destroy_object", "Check emitted callbacks are properly paired", s_createDestroyObjectGroup));

	return addressBindingReportTests.release();
}

} // memory
} // vkt
