/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \brief Vulkan Statistics Query Tests
 *//*--------------------------------------------------------------------*/

#include "vktQueryPoolStatisticsTests.hpp"
#include "vktTestCase.hpp"

#include "vktDrawImageObjectUtil.hpp"
#include "vktDrawBufferObjectUtil.hpp"
#include "vktDrawCreateInfoUtil.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vkBuilderUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkPrograms.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#ifdef CTS_USES_VULKANSC
#include "vkSafetyCriticalUtil.hpp"
#endif // CTS_USES_VULKANSC

#include "deMath.h"

#include "tcuTestLog.hpp"
#include "tcuResource.hpp"
#include "tcuImageCompare.hpp"
#include "vkImageUtil.hpp"
#include "tcuCommandLine.hpp"
#include "tcuRGBA.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuMaybe.hpp"

#include <vector>
#include <utility>
#include <algorithm>

using std::vector;
using std::pair;

namespace vkt
{
namespace QueryPool
{
namespace
{

using namespace vk;
using namespace Draw;

//Test parameters
enum
{
	WIDTH	= 64,
	HEIGHT	= 64
};

enum ResetType
{
	RESET_TYPE_NORMAL = 0,
	RESET_TYPE_HOST,
	RESET_TYPE_BEFORE_COPY,
	RESET_TYPE_AFTER_COPY,
	RESET_TYPE_LAST
};

enum CopyType
{
	COPY_TYPE_GET = 0,
	COPY_TYPE_CMD,
};

enum StrideType
{
	STRIDE_TYPE_VALID = 0,
	STRIDE_TYPE_ZERO,
};

enum ClearOperation
{
	CLEAR_NOOP,
	CLEAR_COLOR,
	CLEAR_DEPTH
};

std::string inputTypeToGLString (const VkPrimitiveTopology& inputType)
{
	switch (inputType)
	{
		case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
			return "points";
		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
			return "lines";
		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
			return "lines_adjacency";
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
			return "triangles";
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
			return "triangles_adjacency";
		default:
			DE_ASSERT(DE_FALSE);
			return "error";
	}
}

std::string outputTypeToGLString (const VkPrimitiveTopology& outputType)
{
	switch (outputType)
	{
		case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
			return "points";
		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
			return "line_strip";
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
			return "triangle_strip";
		default:
			DE_ASSERT(DE_FALSE);
			return "error";
	}
}

uint32_t findNonGraphicsQueueFamilyIndex (const InstanceInterface& vki, const VkPhysicalDevice physicalDevice)
{
	const VkQueueFlags mandatoryFlags = VK_QUEUE_COMPUTE_BIT;
	const VkQueueFlags forbiddenFlags = VK_QUEUE_GRAPHICS_BIT;

	uint32_t qfIndex = findQueueFamilyIndexWithCaps(vki, physicalDevice, mandatoryFlags, forbiddenFlags);
	return qfIndex;
}

void checkSupportForNonGraphicsQueueFamily (const InstanceInterface& vki, const VkPhysicalDevice physicalDevice)
{
	findNonGraphicsQueueFamilyIndex(vki, physicalDevice);
}

// Device helper: this is needed in some tests when we create custom devices.
class DeviceHelper
{
public:
	virtual ~DeviceHelper () {}
	virtual const DeviceInterface&			getDeviceInterface	(void) const = 0;
	virtual VkDevice						getDevice			(void) const = 0;
	virtual uint32_t						getQueueFamilyIndex	(void) const = 0;
	virtual VkQueue							getQueue			(void) const = 0;
	virtual Allocator&						getAllocator		(void) const = 0;
	virtual const std::vector<std::string>&	getDeviceExtensions	(void) const = 0;
};

// This one just reuses the default device from the context.
class ContextDeviceHelper : public DeviceHelper
{
public:
	ContextDeviceHelper (Context& context)
		: m_deviceInterface		(context.getDeviceInterface())
		, m_device				(context.getDevice())
		, m_queueFamilyIndex	(context.getUniversalQueueFamilyIndex())
		, m_queue				(context.getUniversalQueue())
		, m_allocator			(context.getDefaultAllocator())
		, m_extensions			(context.getDeviceExtensions())
		{}

	virtual ~ContextDeviceHelper () {}

	const DeviceInterface&			getDeviceInterface	(void) const override	{ return m_deviceInterface;		}
	VkDevice						getDevice			(void) const override	{ return m_device;				}
	uint32_t						getQueueFamilyIndex	(void) const override	{ return m_queueFamilyIndex;	}
	VkQueue							getQueue			(void) const override	{ return m_queue;				}
	Allocator&						getAllocator		(void) const override	{ return m_allocator;			}
	const std::vector<std::string>&	getDeviceExtensions	(void) const override	{ return m_extensions;			}

protected:
	const DeviceInterface&		m_deviceInterface;
	const VkDevice				m_device;
	const uint32_t				m_queueFamilyIndex;
	const VkQueue				m_queue;
	Allocator&					m_allocator;
	std::vector<std::string>	m_extensions;
};

// This one creates a new device with a single compute-only queue.
class ComputeQueueDeviceHelper : public DeviceHelper
{
public:
	ComputeQueueDeviceHelper (Context& context)
	{
		const auto&	vkp				= context.getPlatformInterface();
		const auto&	vki				= context.getInstanceInterface();
		const auto	instance		= context.getInstance();
		const auto	physicalDevice	= context.getPhysicalDevice();
		const auto	queuePriority	= 1.0f;

		// Queue index. Support for this type of queue needs to be checked first.
		m_queueFamilyIndex			= findNonGraphicsQueueFamilyIndex(vki, physicalDevice);

		// Create a universal queue that supports graphics and compute.
		const VkDeviceQueueCreateInfo queueParams =
		{
			VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,	// VkStructureType				sType;
			DE_NULL,									// const void*					pNext;
			0u,											// VkDeviceQueueCreateFlags		flags;
			m_queueFamilyIndex,							// deUint32						queueFamilyIndex;
			1u,											// deUint32						queueCount;
			&queuePriority								// const float*					pQueuePriorities;
		};

		// Enable all available base features except for robust buffer access.
		// Enable host query reset if available.
		// Enable portability features if available.
		// Enable the same extensions as the context device.
		const bool										hostQueryResetSupport	= (context.isDeviceFunctionalitySupported("VK_EXT_host_query_reset"));
		const bool										portabilitySupport		= (context.isDeviceFunctionalitySupported("VK_KHR_portability_subset"));
#ifdef CTS_USES_VULKANSC
		const bool										inVulkanSC				= true;
#else
		const bool										inVulkanSC				= false;
#endif // CTS_USES_VULKANSC
		const bool										useFeatures2			= (hostQueryResetSupport || portabilitySupport || inVulkanSC);
		VkPhysicalDeviceFeatures						baseFeatures;
		VkPhysicalDeviceFeatures2						features2				= initVulkanStructure();
		VkPhysicalDeviceHostQueryResetFeatures			hostQueryResetFeatures	= initVulkanStructure();
#ifndef CTS_USES_VULKANSC
		VkPhysicalDevicePortabilitySubsetFeaturesKHR	portabilityFeatures		= initVulkanStructure();
#endif // CTS_USES_VULKANSC
		const auto										addFeatures				= makeStructChainAdder(&features2);

		if (useFeatures2)
		{
			if (hostQueryResetSupport)
				addFeatures(&hostQueryResetFeatures);

#ifndef CTS_USES_VULKANSC
			if (portabilitySupport)
				addFeatures(&portabilityFeatures);
#endif // CTS_USES_VULKANSC

			vki.getPhysicalDeviceFeatures2(physicalDevice, &features2);
			features2.features.robustBufferAccess = VK_FALSE;
		}
		else
		{
			vki.getPhysicalDeviceFeatures(physicalDevice, &baseFeatures);
			baseFeatures.robustBufferAccess = VK_FALSE;
		}

		const auto	creationExtensions	= context.getDeviceCreationExtensions();

#ifdef CTS_USES_VULKANSC
		const auto&							cmdLine				= context.getTestContext().getCommandLine();
		VkDeviceObjectReservationCreateInfo	memReservationInfo	= cmdLine.isSubProcess()
																? context.getResourceInterface()->getStatMax()
																: resetDeviceObjectReservationCreateInfo();

		addFeatures(&memReservationInfo);

		VkPipelineCacheCreateInfo			pcCI;
		std::vector<VkPipelinePoolSize>		poolSizes;

		if (cmdLine.isSubProcess())
		{
			if (context.getResourceInterface()->getCacheDataSize() > 0)
			{
				pcCI =
				{
					VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,				// VkStructureType				sType;
					DE_NULL,													// const void*					pNext;
					VK_PIPELINE_CACHE_CREATE_READ_ONLY_BIT |
						VK_PIPELINE_CACHE_CREATE_USE_APPLICATION_STORAGE_BIT,	// VkPipelineCacheCreateFlags	flags;
					context.getResourceInterface()->getCacheDataSize(),			// deUintptr					initialDataSize;
					context.getResourceInterface()->getCacheData()				// const void*					pInitialData;
				};
				memReservationInfo.pipelineCacheCreateInfoCount = 1;
				memReservationInfo.pPipelineCacheCreateInfos = &pcCI;
			}

			poolSizes = context.getResourceInterface()->getPipelinePoolSizes();
			if (!poolSizes.empty())
			{
				memReservationInfo.pipelinePoolSizeCount	= de::sizeU32(poolSizes);
				memReservationInfo.pPipelinePoolSizes		= de::dataOrNull(poolSizes);
			}
		}
#endif // CTS_USES_VULKANSC

		const VkDeviceCreateInfo deviceCreateInfo =
		{
			VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,					//sType;
			(useFeatures2 ? &features2 : nullptr),					//pNext;
			0u,														//flags
			1u,														//queueRecordCount;
			&queueParams,											//pRequestedQueues;
			0u,														//layerCount;
			nullptr,												//ppEnabledLayerNames;
			de::sizeU32(creationExtensions),						// deUint32							enabledExtensionCount;
			de::dataOrNull(creationExtensions),						// const char* const*				ppEnabledExtensionNames;
			(useFeatures2 ? nullptr : &baseFeatures),				//pEnabledFeatures;
		};

		m_device	= createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(), vkp, instance, vki, physicalDevice, &deviceCreateInfo);
		m_vkd		.reset(new DeviceDriver(vkp, instance, m_device.get(), context.getUsedApiVersion()));
		m_queue		= getDeviceQueue(*m_vkd, *m_device, m_queueFamilyIndex, 0u);
		m_allocator	.reset(new SimpleAllocator(*m_vkd, m_device.get(), getPhysicalDeviceMemoryProperties(vki, physicalDevice)));
	}

	virtual ~ComputeQueueDeviceHelper () {}

	const DeviceInterface&			getDeviceInterface	(void) const override	{ return *m_vkd;				}
	VkDevice						getDevice			(void) const override	{ return m_device.get();		}
	uint32_t						getQueueFamilyIndex	(void) const override	{ return m_queueFamilyIndex;	}
	VkQueue							getQueue			(void) const override	{ return m_queue;				}
	Allocator&						getAllocator		(void) const override	{ return *m_allocator;			}
	const std::vector<std::string>&	getDeviceExtensions	(void) const override	{ return m_extensions;			}

protected:
	Move<VkDevice>						m_device;
	std::unique_ptr<DeviceDriver>		m_vkd;
	deUint32							m_queueFamilyIndex;
	VkQueue								m_queue;
	std::unique_ptr<SimpleAllocator>	m_allocator;
	std::vector<std::string>			m_extensions;
};

std::unique_ptr<DeviceHelper> g_computeQueueDeviceHelper;
std::unique_ptr<DeviceHelper> g_contextDeviceHelper;

DeviceHelper& getDeviceHelper (Context& context, bool computeQueue)
{
	if (computeQueue)
	{
		if (!g_computeQueueDeviceHelper)
			g_computeQueueDeviceHelper.reset(new ComputeQueueDeviceHelper(context));
		return *g_computeQueueDeviceHelper;
	}

	if (!g_contextDeviceHelper)
		g_contextDeviceHelper.reset(new ContextDeviceHelper(context));
	return *g_contextDeviceHelper;
}

void destroyDeviceHelpers ()
{
	// Destroy singleton objects
	g_computeQueueDeviceHelper.reset(nullptr);
	g_contextDeviceHelper.reset(nullptr);
}

using Pair32						= pair<deUint32, deUint32>;
using Pair64						= pair<deUint64, deUint64>;
using ResultsVector					= vector<deUint64>;
using ResultsVectorWithAvailability	= vector<Pair64>;

// Get query pool results as a vector. Note results are always converted to
// deUint64, but the actual vkGetQueryPoolResults call will use the 64-bits flag
// or not depending on your preferences.
vk::VkResult GetQueryPoolResultsVector(
	ResultsVector& output, const DeviceInterface& vk, vk::VkDevice device, vk::VkQueryPool queryPool, deUint32 firstQuery, deUint32 queryCount, VkQueryResultFlags flags)
{
	if (flags & vk::VK_QUERY_RESULT_WITH_AVAILABILITY_BIT)
		TCU_THROW(InternalError, "Availability flag passed when expecting results as ResultsVector");

	vk::VkResult result;
	output.resize(queryCount);

	if (flags & vk::VK_QUERY_RESULT_64_BIT)
	{
		constexpr size_t	stride		= sizeof(ResultsVector::value_type);
		const size_t		totalSize	= stride * output.size();
		result = vk.getQueryPoolResults(device, queryPool, firstQuery, queryCount, totalSize, output.data(), stride, flags);
	}
	else
	{
		using IntermediateVector = vector<deUint32>;

		IntermediateVector	intermediate(queryCount);

		// Try to preserve existing data if possible.
		std::transform(begin(output), end(output), begin(intermediate), [](deUint64 v) { return static_cast<deUint32>(v); });

		constexpr size_t	stride		= sizeof(decltype(intermediate)::value_type);
		const size_t		totalSize	= stride * intermediate.size();

		// Get and copy results.
		result = vk.getQueryPoolResults(device, queryPool, firstQuery, queryCount, totalSize, intermediate.data(), stride, flags);
		std::copy(begin(intermediate), end(intermediate), begin(output));
	}

	return result;
}

// Same as the normal GetQueryPoolResultsVector but returning the availability
// bit associated to each query in addition to the query value.
vk::VkResult GetQueryPoolResultsVector(
	ResultsVectorWithAvailability& output, const DeviceInterface& vk, vk::VkDevice device, vk::VkQueryPool queryPool, deUint32 firstQuery, deUint32 queryCount, VkQueryResultFlags flags)
{
	flags |= vk::VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;

	vk::VkResult result;
	output.resize(queryCount);

	if (flags & vk::VK_QUERY_RESULT_64_BIT)
	{
		constexpr size_t	stride		= sizeof(ResultsVectorWithAvailability::value_type);
		const size_t		totalSize	= stride * output.size();
		result = vk.getQueryPoolResults(device, queryPool, firstQuery, queryCount, totalSize, output.data(), stride, flags);
	}
	else
	{
		using IntermediateVector = vector<Pair32>;

		IntermediateVector	intermediate(queryCount);

		// Try to preserve existing output data if possible.
		std::transform(begin(output), end(output), begin(intermediate), [](const Pair64& p) { return Pair32{static_cast<deUint32>(p.first), static_cast<deUint32>(p.second)}; });

		constexpr size_t	stride		= sizeof(decltype(intermediate)::value_type);
		const size_t		totalSize	= stride * intermediate.size();

		// Get and copy.
		result = vk.getQueryPoolResults(device, queryPool, firstQuery, queryCount, totalSize, intermediate.data(), stride, flags);
		std::transform(begin(intermediate), end(intermediate), begin(output), [](const Pair32& p) { return Pair64{p.first, p.second}; });
	}

	return result;
}

// Get query pool results as a vector. Note results are always converted to
// deUint64, but the actual vkCmdCopyQueryPoolResults call will use the 64-bits flag
// or not depending on your preferences.
void cmdCopyQueryPoolResultsVector(
	  ResultsVector& output, const DeviceInterface& vk, vk::VkDevice device, const vk::Allocation& allocation, deUint32 queryCount, VkQueryResultFlags flags, deBool dstOffset)
{
	if (flags & vk::VK_QUERY_RESULT_WITH_AVAILABILITY_BIT)
		TCU_THROW(InternalError, "Availability flag passed when expecting results as ResultsVector");

	output.resize(queryCount);

	void* allocationData = allocation.getHostPtr();
	vk::invalidateAlloc(vk, device, allocation);

	if (flags & vk::VK_QUERY_RESULT_64_BIT)
	{
		constexpr size_t	stride		= sizeof(ResultsVector::value_type);
		const size_t		totalSize	= stride * output.size();
		const deUint32		offset		= dstOffset ? 1u : 0u;
		deMemcpy(output.data(), (reinterpret_cast<ResultsVector::value_type*>(allocationData) + offset), totalSize);
	}
	else
	{
		using IntermediateVector = vector<deUint32>;

		IntermediateVector	intermediate(queryCount);

		// Try to preserve existing data if possible.
		std::transform(begin(output), end(output), begin(intermediate), [](deUint64 v) { return static_cast<deUint32>(v); });

		constexpr size_t	stride		= sizeof(decltype(intermediate)::value_type);
		const size_t		totalSize	= stride * intermediate.size();
		const deUint32		offset		= dstOffset ? 1u : 0u;
		// Get and copy results.
		deMemcpy(intermediate.data(), (reinterpret_cast<decltype(intermediate)::value_type*>(allocationData) + offset), totalSize);
		std::copy(begin(intermediate), end(intermediate), begin(output));
	}
}

// Same as the normal cmdCopyQueryPoolResultsVector but returning the availability
// bit associated to each query in addition to the query value.
void cmdCopyQueryPoolResultsVector(
   ResultsVectorWithAvailability& output, const DeviceInterface& vk, vk::VkDevice device, const vk::Allocation& allocation, deUint32 queryCount, VkQueryResultFlags flags, deBool dstOffset)
{
	flags |= vk::VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;

	output.resize(queryCount);

	void* allocationData = allocation.getHostPtr();
	vk::invalidateAlloc(vk, device, allocation);

	if (flags & vk::VK_QUERY_RESULT_64_BIT)
	{
		constexpr size_t	stride		= sizeof(ResultsVectorWithAvailability::value_type);
		const size_t		totalSize	= stride * output.size();
		const deUint32		offset		= dstOffset ? 1u : 0u;
		deMemcpy(output.data(), (reinterpret_cast<ResultsVectorWithAvailability::value_type*>(allocationData) + offset), totalSize);
	}
	else
	{
		using IntermediateVector = vector<Pair32>;

		IntermediateVector	intermediate(queryCount);

		// Try to preserve existing output data if possible.
		std::transform(begin(output), end(output), begin(intermediate), [](const Pair64& p) { return Pair32{static_cast<deUint32>(p.first), static_cast<deUint32>(p.second)}; });

		constexpr size_t	stride		= sizeof(decltype(intermediate)::value_type);
		const size_t		totalSize	= stride * intermediate.size();
		const deUint32		offset		= dstOffset ? 1u : 0u;

		// Get and copy.
		deMemcpy(intermediate.data(), (reinterpret_cast<decltype(intermediate)::value_type*>(allocationData) + offset), totalSize);
		std::transform(begin(intermediate), end(intermediate), begin(output), [](const Pair32& p) { return Pair64{p.first, p.second}; });
	}
}

// Generic parameters structure.
struct GenericParameters
{
	ResetType	resetType;
	CopyType	copyType;
	deBool		query64Bits;
	deBool		dstOffset;
	StrideType	strideType;

	GenericParameters (ResetType resetType_, CopyType copyType_, deBool query64Bits_, deBool dstOffset_, StrideType strideType_)
		: resetType{resetType_}, copyType{copyType_}, query64Bits{query64Bits_}, dstOffset{dstOffset_}, strideType{strideType_}
		{}

	VkQueryResultFlags querySizeFlags () const
	{
		return (query64Bits ? static_cast<VkQueryResultFlags>(vk::VK_QUERY_RESULT_64_BIT) : 0u);
	}
};

void beginSecondaryCommandBuffer (const DeviceInterface&				vk,
								  const VkCommandBuffer					commandBuffer,
								  const VkQueryPipelineStatisticFlags	queryFlags,
								  const VkRenderPass					renderPass = (VkRenderPass)0u,
								  const VkFramebuffer					framebuffer = (VkFramebuffer)0u,
								  const VkCommandBufferUsageFlags		bufferUsageFlags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT)
{
	const VkCommandBufferInheritanceInfo	secCmdBufInheritInfo	=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
		DE_NULL,
		renderPass,					// renderPass
		0u,							// subpass
		framebuffer,				// framebuffer
		VK_FALSE,					// occlusionQueryEnable
		(VkQueryControlFlags)0u,	// queryFlags
		queryFlags,					// pipelineStatistics
	};

	const VkCommandBufferBeginInfo			info					=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	// VkStructureType							sType;
		DE_NULL,										// const void*								pNext;
		bufferUsageFlags,								// VkCommandBufferUsageFlags				flags;
		&secCmdBufInheritInfo,							// const VkCommandBufferInheritanceInfo*	pInheritanceInfo;
	};
	VK_CHECK(vk.beginCommandBuffer(commandBuffer, &info));
}

Move<VkQueryPool> makeQueryPool (const DeviceInterface& vk, const VkDevice device, deUint32 queryCount, VkQueryPipelineStatisticFlags statisticFlags )
{
	const VkQueryPoolCreateInfo queryPoolCreateInfo =
	{
		VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,	// VkStructureType					sType
		DE_NULL,									// const void*						pNext
		(VkQueryPoolCreateFlags)0,					// VkQueryPoolCreateFlags			flags
		VK_QUERY_TYPE_PIPELINE_STATISTICS ,			// VkQueryType						queryType
		queryCount,									// deUint32							entryCount
		statisticFlags,								// VkQueryPipelineStatisticFlags	pipelineStatistics
	};
	return createQueryPool(vk, device, &queryPoolCreateInfo);
}

double calculatePearsonCorrelation(const std::vector<deUint64>& x, const ResultsVector& y)
{
	// This function calculates Pearson correlation coefficient ( https://en.wikipedia.org/wiki/Pearson_correlation_coefficient )
	// Two statistical variables are linear ( == fully corellated ) when fabs( Pearson corelation coefficient ) == 1
	// Two statistical variables are independent when pearson corelation coefficient == 0
	// If fabs( Pearson coefficient ) is > 0.8 then these two variables are almost linear

	DE_ASSERT(x.size() == y.size());
	DE_ASSERT(x.size() > 1);

	// calculate mean values
	double xMean = 0.0, yMean = 0.0;
	for (deUint32 i = 0; i < x.size(); ++i)
	{
		xMean += static_cast<double>(x[i]);
		yMean += static_cast<double>(y[i]);
	}
	xMean /= static_cast<double>(x.size());
	yMean /= static_cast<double>(x.size());

	// calculate standard deviations
	double xS = 0.0, yS = 0.0;
	for (deUint32 i = 0; i < x.size(); ++i)
	{
		double xv = static_cast<double>(x[i]) - xMean;
		double yv = static_cast<double>(y[i]) - yMean;

		xS += xv * xv;
		yS += yv * yv;
	}
	xS = sqrt( xS / static_cast<double>(x.size() - 1) );
	yS = sqrt( yS / static_cast<double>(x.size() - 1) );

	// calculate Pearson coefficient
	double pearson = 0.0;
	for (deUint32 i = 0; i < x.size(); ++i)
	{
		double xv = (static_cast<double>(x[i]) - xMean ) / xS;
		double yv = (static_cast<double>(y[i]) - yMean ) / yS;
		pearson   += xv * yv;
	}

	return pearson / static_cast<double>(x.size() - 1);
}

double calculatePearsonCorrelation(const std::vector<deUint64>& x, const ResultsVectorWithAvailability& ya)
{
	ResultsVector y;
	for (const auto& elt : ya)
		y.push_back(elt.first);
	return calculatePearsonCorrelation(x, y);
}

using BufferPtr = de::SharedPtr<Buffer>;

void clearBuffer (const DeviceInterface& vk, const VkDevice device, const BufferPtr buffer, const VkDeviceSize bufferSizeBytes)
{
	const std::vector<deUint8>	data			((size_t)bufferSizeBytes, 0u);
	const Allocation&			allocation		= buffer->getBoundMemory();
	void*						allocationData	= allocation.getHostPtr();
	invalidateAlloc(vk, device, allocation);
	deMemcpy(allocationData, &data[0], (size_t)bufferSizeBytes);
	flushAlloc(vk, device, allocation);
}

class StatisticQueryTestInstance : public TestInstance
{
public:
	StatisticQueryTestInstance	(Context& context, deUint32 queryCount, deBool dstOffset, bool useComputeQueue);

protected:
	struct ValueAndAvailability
	{
		deUint64 value;
		deUint64 availability;
	};

	VkDeviceSize	m_resetBufferSize;
	BufferPtr		m_resetBuffer;
	deBool			dstOffset;
	const bool		m_useComputeQueue;

	virtual void			checkExtensions		(deBool hostResetQueryEnabled);
	BufferPtr				createResetBuffer	(void) const;
	void					fillResetBuffer		(const BufferPtr& buffer) const;
	tcu::TestStatus			verifyUnavailable	();
};

BufferPtr StatisticQueryTestInstance::createResetBuffer (void) const
{
	const auto&	deviceHelper = getDeviceHelper(m_context, m_useComputeQueue);

	return Buffer::createAndAlloc(deviceHelper.getDeviceInterface(),
								  deviceHelper.getDevice(),
								  BufferCreateInfo(m_resetBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT),
								  deviceHelper.getAllocator(),
								  vk::MemoryRequirement::HostVisible);
}

void StatisticQueryTestInstance::fillResetBuffer (const BufferPtr& buffer) const
{
	const auto&	deviceHelper = getDeviceHelper(m_context, m_useComputeQueue);

	const vk::Allocation& allocation = buffer->getBoundMemory();
	void* allocationData = allocation.getHostPtr();
	deMemset(allocationData, 0xff, static_cast<size_t>(m_resetBufferSize));
	flushAlloc(deviceHelper.getDeviceInterface(), deviceHelper.getDevice(), allocation);
}

StatisticQueryTestInstance::StatisticQueryTestInstance (Context& context, deUint32 queryCount, deBool dstOffset_, bool useComputeQueue)
	: TestInstance		(context)
	, m_resetBufferSize	((queryCount + (dstOffset_ ? 1u : 0u)) * sizeof(ValueAndAvailability))
	, m_resetBuffer		()
	, dstOffset			(dstOffset_)
	, m_useComputeQueue	(useComputeQueue)
{
	m_resetBuffer = createResetBuffer();
	fillResetBuffer(m_resetBuffer);
}

void StatisticQueryTestInstance::checkExtensions (deBool hostResetQueryEnabled)
{
	if (!m_context.getDeviceFeatures().pipelineStatisticsQuery)
		throw tcu::NotSupportedError("Pipeline statistics queries are not supported");

	if (hostResetQueryEnabled == DE_TRUE)
	{
		// Check VK_EXT_host_query_reset is supported
		m_context.requireDeviceFunctionality("VK_EXT_host_query_reset");
		if(m_context.getHostQueryResetFeatures().hostQueryReset == VK_FALSE)
			throw tcu::NotSupportedError(std::string("Implementation doesn't support resetting queries from the host").c_str());
	}
}

tcu::TestStatus	StatisticQueryTestInstance::verifyUnavailable ()
{
	const vk::Allocation& allocation	= m_resetBuffer->getBoundMemory();
	const void* allocationData			= allocation.getHostPtr();
	deUint32 size						= dstOffset ? 2 : 1;
	std::vector<ValueAndAvailability>	va;
	va.resize(size);

	vk::invalidateAlloc(m_context.getDeviceInterface(), m_context.getDevice(), allocation);
	deMemcpy(va.data(), allocationData, size * sizeof(ValueAndAvailability));

	deBool failed = false;
	for (deUint32 idx = 0u; idx < size; idx++)
	{
		if (dstOffset && idx == 0)
		{
			// Check that the contents between 0 and dstOffset were not overwritten.
			failed |= va[idx].availability != 0xfffffffffffffffful || va[idx].value != 0xfffffffffffffffful;
			continue;
		}

		failed |= va[idx].availability != 0;
	}

	return failed ? tcu::TestStatus::fail("Availability bit nonzero after resetting query or dstOffset wrong values") : tcu::TestStatus::pass("Pass");
}

class ComputeInvocationsTestInstance : public StatisticQueryTestInstance
{
public:
	struct ParametersCompute : public GenericParameters
	{
		ParametersCompute (const tcu::UVec3& localSize_, const tcu::UVec3& groupSize_, const std::string& shaderName_, ResetType resetType_, CopyType copyType_, deBool query64Bits_, bool dstOffset_, StrideType strideType_, bool useComputeQueue_)
			: GenericParameters{resetType_, copyType_, query64Bits_, dstOffset_, strideType_}
			, localSize(localSize_)
			, groupSize(groupSize_)
			, shaderName(shaderName_)
			, useComputeQueue(useComputeQueue_)
			{}

		tcu::UVec3	localSize;
		tcu::UVec3	groupSize;
		std::string	shaderName;
		const bool	useComputeQueue;
	};
							ComputeInvocationsTestInstance		(Context& context, const std::vector<ParametersCompute>& parameters);
	tcu::TestStatus			iterate								(void);
protected:
	virtual tcu::TestStatus	executeTest							(const VkCommandPool&			cmdPool,
																 const VkPipelineLayout			pipelineLayout,
																 const VkDescriptorSet&			descriptorSet,
																 const BufferPtr				buffer,
																 const VkDeviceSize				bufferSizeBytes);
	deUint32				getComputeExecution					(const ParametersCompute& parm) const
		{
			return parm.localSize.x() * parm.localSize.y() *parm.localSize.z() * parm.groupSize.x() * parm.groupSize.y() * parm.groupSize.z();
		}
	const std::vector<ParametersCompute>&	m_parameters;
};

ComputeInvocationsTestInstance::ComputeInvocationsTestInstance (Context& context, const std::vector<ParametersCompute>& parameters)
	: StatisticQueryTestInstance	(context, 1u, parameters[0].dstOffset, parameters[0].useComputeQueue)
	, m_parameters					(parameters)
{
}

tcu::TestStatus	ComputeInvocationsTestInstance::iterate (void)
{
	// These should have the same value throughout the whole vector.
	const deBool						hostQueryReset			= ((m_parameters[0].resetType == RESET_TYPE_HOST)? DE_TRUE : DE_FALSE);

	checkExtensions(hostQueryReset);

	const auto&							deviceHelper			= getDeviceHelper(m_context, m_useComputeQueue);
	const DeviceInterface&				vk						= deviceHelper.getDeviceInterface();
	const VkDevice						device					= deviceHelper.getDevice();
	deUint32							maxSize					= 0u;

	for(size_t parametersNdx = 0; parametersNdx < m_parameters.size(); ++parametersNdx)
		maxSize = deMaxu32(maxSize, getComputeExecution(m_parameters[parametersNdx]));

	const VkDeviceSize					bufferSizeBytes			= static_cast<VkDeviceSize>(deAlignSize(static_cast<size_t>(sizeof(deUint32) * maxSize),
																							static_cast<size_t>(m_context.getDeviceProperties().limits.nonCoherentAtomSize)));
	BufferPtr							buffer					= Buffer::createAndAlloc(vk, device, BufferCreateInfo(bufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
																							 deviceHelper.getAllocator(), MemoryRequirement::HostVisible);

	const Unique<VkDescriptorSetLayout>	descriptorSetLayout		(DescriptorSetLayoutBuilder()
			.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
			.build(vk, device));

	const Unique<VkPipelineLayout>		pipelineLayout			(makePipelineLayout(vk, device, *descriptorSetLayout));

	const Unique<VkDescriptorPool>		descriptorPool			(DescriptorPoolBuilder()
			.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
			.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	const VkDescriptorSetAllocateInfo allocateParams		=
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,	// VkStructureType				sType;
		DE_NULL,										// const void*					pNext;
		*descriptorPool,								// VkDescriptorPool				descriptorPool;
		1u,												// deUint32						setLayoutCount;
		&(*descriptorSetLayout),						// const VkDescriptorSetLayout*	pSetLayouts;
	};

	const Unique<VkDescriptorSet>		descriptorSet		(allocateDescriptorSet(vk, device, &allocateParams));
	const VkDescriptorBufferInfo		descriptorInfo		=
	{
		buffer->object(),	//VkBuffer		buffer;
		0ull,				//VkDeviceSize	offset;
		bufferSizeBytes,	//VkDeviceSize	range;
	};

	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorInfo)
		.update(vk, device);

	const CmdPoolCreateInfo			cmdPoolCreateInfo	(deviceHelper.getQueueFamilyIndex());
	const Unique<VkCommandPool>		cmdPool				(createCommandPool(vk, device, &cmdPoolCreateInfo));

	return executeTest (*cmdPool, *pipelineLayout, *descriptorSet, buffer, bufferSizeBytes);
}

tcu::TestStatus ComputeInvocationsTestInstance::executeTest (const VkCommandPool&			cmdPool,
															 const VkPipelineLayout			pipelineLayout,
															 const VkDescriptorSet&			descriptorSet,
															 const BufferPtr				buffer,
															 const VkDeviceSize				bufferSizeBytes)
{
	const auto&							deviceHelper			= getDeviceHelper(m_context, m_useComputeQueue);
	const DeviceInterface&				vk						= deviceHelper.getDeviceInterface();
	const VkDevice						device					= deviceHelper.getDevice();
	const VkQueue						queue					= deviceHelper.getQueue();
	const VkBufferMemoryBarrier			computeFinishBarrier	=
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,					// VkStructureType	sType;
		DE_NULL,													// const void*		pNext;
		VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,		// VkAccessFlags	srcAccessMask;
		VK_ACCESS_HOST_READ_BIT,									// VkAccessFlags	dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,									// deUint32			srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,									// deUint32			destQueueFamilyIndex;
		buffer->object(),											// VkBuffer			buffer;
		0ull,														// VkDeviceSize		offset;
		bufferSizeBytes,											// VkDeviceSize		size;
	};

	for(size_t parametersNdx = 0u; parametersNdx < m_parameters.size(); ++parametersNdx)
	{
		clearBuffer(vk, device, buffer, bufferSizeBytes);
		const Unique<VkShaderModule>			shaderModule				(createShaderModule(vk, device,
																			m_context.getBinaryCollection().get(m_parameters[parametersNdx].shaderName), (VkShaderModuleCreateFlags)0u));

		const VkPipelineShaderStageCreateInfo	pipelineShaderStageParams	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType						sType;
			DE_NULL,												// const void*							pNext;
			(VkPipelineShaderStageCreateFlags)0u,					// VkPipelineShaderStageCreateFlags		flags;
			VK_SHADER_STAGE_COMPUTE_BIT,							// VkShaderStageFlagBits				stage;
			*shaderModule,											// VkShaderModule						module;
			"main",													// const char*							pName;
			DE_NULL,												// const VkSpecializationInfo*			pSpecializationInfo;
		};

		const VkComputePipelineCreateInfo		pipelineCreateInfo			=
		{
			VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,	// VkStructureType					sType;
			DE_NULL,										// const void*						pNext;
			(VkPipelineCreateFlags)0u,						// VkPipelineCreateFlags			flags;
			pipelineShaderStageParams,						// VkPipelineShaderStageCreateInfo	stage;
			pipelineLayout,									// VkPipelineLayout					layout;
			DE_NULL,										// VkPipeline						basePipelineHandle;
			0,												// deInt32							basePipelineIndex;
		};
		const Unique<VkPipeline> pipeline(createComputePipeline(vk, device, DE_NULL , &pipelineCreateInfo));

		const Unique<VkCommandBuffer>	cmdBuffer			(allocateCommandBuffer(vk, device, cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
		const Unique<VkQueryPool>		queryPool			(makeQueryPool(vk, device, 1u, VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT));

		beginCommandBuffer(vk, *cmdBuffer);
			if (m_parameters[0].resetType != RESET_TYPE_HOST)
				vk.cmdResetQueryPool(*cmdBuffer, *queryPool, 0u, 1u);

			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
			vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0u, 1u, &descriptorSet, 0u, DE_NULL);

			vk.cmdBeginQuery(*cmdBuffer, *queryPool, 0u, (VkQueryControlFlags)0u);
			vk.cmdDispatch(*cmdBuffer, m_parameters[parametersNdx].groupSize.x(), m_parameters[parametersNdx].groupSize.y(), m_parameters[parametersNdx].groupSize.z());
			vk.cmdEndQuery(*cmdBuffer, *queryPool, 0u);

			if (m_parameters[0].resetType == RESET_TYPE_BEFORE_COPY || m_parameters[0].resetType == RESET_TYPE_AFTER_COPY || m_parameters[0].copyType == COPY_TYPE_CMD)
			{
				VkDeviceSize stride = m_parameters[0].querySizeFlags() ? sizeof(deUint64) : sizeof(deUint32);
				vk::VkQueryResultFlags flags = m_parameters[0].querySizeFlags() | VK_QUERY_RESULT_WAIT_BIT;

				if (m_parameters[0].resetType == RESET_TYPE_HOST)
				{
					flags |= VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
					stride *= 2u;
				}

				if (m_parameters[0].resetType == RESET_TYPE_BEFORE_COPY)
				{
					vk.cmdResetQueryPool(*cmdBuffer, *queryPool, 0u, 1u);
					flags = VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
					stride = sizeof(ValueAndAvailability);
				}

				VkDeviceSize dstOffsetQuery = (m_parameters[0].dstOffset) ? stride : 0;
				VkDeviceSize copyStride = stride;

				if (m_parameters[0].strideType == STRIDE_TYPE_ZERO)
					copyStride = 0u;

				vk.cmdCopyQueryPoolResults(*cmdBuffer, *queryPool, 0, 1u, m_resetBuffer->object(), dstOffsetQuery, copyStride, flags);

				if (m_parameters[0].resetType == RESET_TYPE_AFTER_COPY)
					vk.cmdResetQueryPool(*cmdBuffer, *queryPool, 0u, 1u);

				const VkBufferMemoryBarrier barrier =
				{
					VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	//  VkStructureType	sType;
					DE_NULL,									//  const void*		pNext;
					VK_ACCESS_TRANSFER_WRITE_BIT,				//  VkAccessFlags	srcAccessMask;
					VK_ACCESS_HOST_READ_BIT,					//  VkAccessFlags	dstAccessMask;
					VK_QUEUE_FAMILY_IGNORED,					//  deUint32		srcQueueFamilyIndex;
					VK_QUEUE_FAMILY_IGNORED,					//  deUint32		destQueueFamilyIndex;
					m_resetBuffer->object(),					//  VkBuffer		buffer;
					0u,											//  VkDeviceSize	offset;
				    1u * stride + dstOffsetQuery,				//  VkDeviceSize	size;
				};
				vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1u, &barrier, 0, (const VkImageMemoryBarrier*)DE_NULL);
			}

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
				(VkDependencyFlags)0u, 0u, (const VkMemoryBarrier*)DE_NULL, 1u, &computeFinishBarrier, 0u, (const VkImageMemoryBarrier*)DE_NULL);

		endCommandBuffer(vk, *cmdBuffer);

		m_context.getTestContext().getLog() << tcu::TestLog::Message << "Compute shader invocations: " << getComputeExecution(m_parameters[parametersNdx]) << tcu::TestLog::EndMessage;

		if (m_parameters[0].resetType == RESET_TYPE_HOST)
			vk.resetQueryPool(device, *queryPool, 0u, 1u);

		// Wait for completion
		submitCommandsAndWait(vk, device, queue, *cmdBuffer);

		// Validate the results
		const Allocation& bufferAllocation = buffer->getBoundMemory();
		invalidateAlloc(vk, device, bufferAllocation);

		if (m_parameters[0].resetType == RESET_TYPE_NORMAL || m_parameters[0].resetType == RESET_TYPE_AFTER_COPY)
		{
			ResultsVector data;

			if (m_parameters[0].copyType == COPY_TYPE_CMD)
			{
				const vk::Allocation& allocation = m_resetBuffer->getBoundMemory();
				cmdCopyQueryPoolResultsVector(data, vk, device, allocation, 1u, (VK_QUERY_RESULT_WAIT_BIT | m_parameters[0].querySizeFlags()), m_parameters[0].dstOffset);
			}
			else
			{
				VK_CHECK(GetQueryPoolResultsVector(data, vk, device, *queryPool, 0u, 1u, (VK_QUERY_RESULT_WAIT_BIT | m_parameters[0].querySizeFlags())));
			}

			if (getComputeExecution(m_parameters[parametersNdx]) != data[0])
				return tcu::TestStatus::fail("QueryPoolResults incorrect");
		}
		else if (m_parameters[0].resetType == RESET_TYPE_HOST)
		{
			ResultsVectorWithAvailability data;

			if (m_parameters[0].copyType == COPY_TYPE_CMD)
			{
				const vk::Allocation& allocation = m_resetBuffer->getBoundMemory();
				cmdCopyQueryPoolResultsVector(data, vk, device, allocation, 1u, (VK_QUERY_RESULT_WAIT_BIT | m_parameters[0].querySizeFlags() | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT), m_parameters[0].dstOffset);
			}
			else
			{
				VK_CHECK(GetQueryPoolResultsVector(data, vk, device, *queryPool, 0u, 1u, (VK_QUERY_RESULT_WAIT_BIT | m_parameters[0].querySizeFlags() | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT)));
			}

			if (getComputeExecution(m_parameters[parametersNdx]) != data[0].first || data[0].second == 0)
				return tcu::TestStatus::fail("QueryPoolResults incorrect");

			deUint64 temp = data[0].first;

			vk.resetQueryPool(device, *queryPool, 0, 1u);
			vk::VkResult res = GetQueryPoolResultsVector(data, vk, device, *queryPool, 0u, 1u, (m_parameters[0].querySizeFlags() | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT));
			/* From Vulkan spec:
			 *
			 * If VK_QUERY_RESULT_WAIT_BIT and VK_QUERY_RESULT_PARTIAL_BIT are both not set then no result values are written to pData
			 * for queries that are in the unavailable state at the time of the call, and vkGetQueryPoolResults returns VK_NOT_READY.
			 * However, availability state is still written to pData for those queries if VK_QUERY_RESULT_WITH_AVAILABILITY_BIT is set.
			 */
			if (res != vk::VK_NOT_READY || data[0].first != temp || data[0].second != 0u)
				return tcu::TestStatus::fail("QueryPoolResults incorrect reset");
		}
		else
		{
			// With RESET_TYPE_BEFORE_COPY, we only need to verify the result after the copy include an availability bit set as zero.
			return verifyUnavailable();
		}

		const deUint32* bufferPtr = static_cast<deUint32*>(bufferAllocation.getHostPtr());
		for (deUint32 ndx = 0u; ndx < getComputeExecution(m_parameters[parametersNdx]); ++ndx)
		{
			if (bufferPtr[ndx] != ndx)
				return tcu::TestStatus::fail("Compute shader didn't write data to the buffer");
		}
	}
	return tcu::TestStatus::pass("Pass");
}

class ComputeInvocationsSecondaryTestInstance : public ComputeInvocationsTestInstance
{
public:
							ComputeInvocationsSecondaryTestInstance	(Context& context, const std::vector<ParametersCompute>& parameters);
protected:
	tcu::TestStatus			executeTest								(const VkCommandPool&			cmdPool,
																	 const VkPipelineLayout			pipelineLayout,
																	 const VkDescriptorSet&			descriptorSet,
																	 const BufferPtr				buffer,
																	 const VkDeviceSize				bufferSizeBytes);
	virtual tcu::TestStatus	checkResult								(const BufferPtr				buffer,
																	 const VkQueryPool				queryPool);
};

ComputeInvocationsSecondaryTestInstance::ComputeInvocationsSecondaryTestInstance	(Context& context, const std::vector<ParametersCompute>& parameters)
	: ComputeInvocationsTestInstance	(context, parameters)
{
}

tcu::TestStatus ComputeInvocationsSecondaryTestInstance::executeTest (const VkCommandPool&			cmdPool,
																	  const VkPipelineLayout		pipelineLayout,
																	  const VkDescriptorSet&		descriptorSet,
																	  const BufferPtr				buffer,
																	  const VkDeviceSize			bufferSizeBytes)
{
	typedef de::SharedPtr<Unique<VkShaderModule> >	VkShaderModuleSp;
	typedef de::SharedPtr<Unique<VkPipeline> >		VkPipelineSp;

	const auto&								deviceHelper				= getDeviceHelper(m_context, m_useComputeQueue);
	const DeviceInterface&					vk							= deviceHelper.getDeviceInterface();
	const VkDevice							device						= deviceHelper.getDevice();
	const VkQueue							queue						= deviceHelper.getQueue();

	const VkBufferMemoryBarrier				computeShaderWriteBarrier	=
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,					// VkStructureType	sType;
		DE_NULL,													// const void*		pNext;
		VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,		// VkAccessFlags	srcAccessMask;
		VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,		// VkAccessFlags	dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,									// deUint32			srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,									// deUint32			destQueueFamilyIndex;
		buffer->object(),											// VkBuffer			buffer;
		0ull,														// VkDeviceSize		offset;
		bufferSizeBytes,											// VkDeviceSize		size;
	};

	const VkBufferMemoryBarrier				computeFinishBarrier		=
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,					// VkStructureType	sType;
		DE_NULL,													// const void*		pNext;
		VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,		// VkAccessFlags	srcAccessMask;
		VK_ACCESS_HOST_READ_BIT,									// VkAccessFlags	dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,									// deUint32			srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,									// deUint32			destQueueFamilyIndex;
		buffer->object(),											// VkBuffer			buffer;
		0ull,														// VkDeviceSize		offset;
		bufferSizeBytes,											// VkDeviceSize		size;
	};

	std::vector<VkShaderModuleSp>			shaderModule;
	std::vector<VkPipelineSp>				pipeline;
	for(size_t parametersNdx = 0; parametersNdx < m_parameters.size(); ++parametersNdx)
	{
		shaderModule.push_back(VkShaderModuleSp(new Unique<VkShaderModule>(createShaderModule(vk, device, m_context.getBinaryCollection().get(m_parameters[parametersNdx].shaderName), (VkShaderModuleCreateFlags)0u))));
		const VkPipelineShaderStageCreateInfo	pipelineShaderStageParams	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType						sType;
			DE_NULL,												// const void*							pNext;
			0u,														// VkPipelineShaderStageCreateFlags		flags;
			VK_SHADER_STAGE_COMPUTE_BIT,							// VkShaderStageFlagBits				stage;
			shaderModule.back().get()->get(),						// VkShaderModule						module;
			"main",													// const char*							pName;
			DE_NULL,												// const VkSpecializationInfo*			pSpecializationInfo;
		};

		const VkComputePipelineCreateInfo		pipelineCreateInfo			=
		{
			VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,	// VkStructureType					sType;
			DE_NULL,										// const void*						pNext;
			0u,												// VkPipelineCreateFlags			flags;
			pipelineShaderStageParams,						// VkPipelineShaderStageCreateInfo	stage;
			pipelineLayout,									// VkPipelineLayout					layout;
			DE_NULL,										// VkPipeline						basePipelineHandle;
			0,												// deInt32							basePipelineIndex;
		};
		pipeline.push_back(VkPipelineSp(new Unique<VkPipeline>(createComputePipeline(vk, device, DE_NULL , &pipelineCreateInfo))));
	}

	const Unique<VkCommandBuffer>				primaryCmdBuffer			(allocateCommandBuffer(vk, device, cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	const Unique<VkCommandBuffer>				secondaryCmdBuffer			(allocateCommandBuffer(vk, device, cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY));

	const Unique<VkQueryPool>					queryPool					(makeQueryPool(vk, device, 1u, VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT));

	clearBuffer(vk, device, buffer, bufferSizeBytes);
	beginSecondaryCommandBuffer(vk, *secondaryCmdBuffer, VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT);
		vk.cmdBindDescriptorSets(*secondaryCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0u, 1u, &descriptorSet, 0u, DE_NULL);
		if (m_parameters[0].resetType != RESET_TYPE_HOST)
			vk.cmdResetQueryPool(*secondaryCmdBuffer, *queryPool, 0u, 1u);
		vk.cmdBeginQuery(*secondaryCmdBuffer, *queryPool, 0u, (VkQueryControlFlags)0u);
		for(size_t parametersNdx = 0; parametersNdx < m_parameters.size(); ++parametersNdx)
		{
				vk.cmdBindPipeline(*secondaryCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline[parametersNdx].get()->get());
				vk.cmdDispatch(*secondaryCmdBuffer, m_parameters[parametersNdx].groupSize.x(), m_parameters[parametersNdx].groupSize.y(), m_parameters[parametersNdx].groupSize.z());

				vk.cmdPipelineBarrier(*secondaryCmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
					(VkDependencyFlags)0u, 0u, (const VkMemoryBarrier*)DE_NULL, 1u, &computeShaderWriteBarrier, 0u, (const VkImageMemoryBarrier*)DE_NULL);
		}
		vk.cmdEndQuery(*secondaryCmdBuffer, *queryPool, 0u);

		if (m_parameters[0].resetType == RESET_TYPE_BEFORE_COPY || m_parameters[0].resetType == RESET_TYPE_AFTER_COPY || m_parameters[0].copyType == COPY_TYPE_CMD)
		{
			VkDeviceSize stride = m_parameters[0].querySizeFlags() ? sizeof(deUint64) : sizeof(deUint32);
			vk::VkQueryResultFlags flags = m_parameters[0].querySizeFlags() | VK_QUERY_RESULT_WAIT_BIT;

			if (m_parameters[0].resetType == RESET_TYPE_HOST)
			{
				flags |= VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
				stride *= 2u;
			}

			if (m_parameters[0].resetType == RESET_TYPE_BEFORE_COPY)
			{
				vk.cmdResetQueryPool(*secondaryCmdBuffer, *queryPool, 0u, 1u);
				flags = VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
				stride = sizeof(ValueAndAvailability);
			}

			VkDeviceSize dstOffsetQuery = (m_parameters[0].dstOffset) ? stride : 0;
			VkDeviceSize copyStride = stride;
			if (m_parameters[0].strideType == STRIDE_TYPE_ZERO)
				copyStride = 0u;

			vk.cmdCopyQueryPoolResults(*secondaryCmdBuffer, *queryPool, 0, 1u, m_resetBuffer->object(), dstOffsetQuery, copyStride, flags);

			if (m_parameters[0].resetType == RESET_TYPE_AFTER_COPY)
				vk.cmdResetQueryPool(*secondaryCmdBuffer, *queryPool, 0u, 1u);

			const VkBufferMemoryBarrier barrier =
			{
				VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	//  VkStructureType	sType;
				DE_NULL,									//  const void*		pNext;
				VK_ACCESS_TRANSFER_WRITE_BIT,				//  VkAccessFlags	srcAccessMask;
				VK_ACCESS_HOST_READ_BIT,					//  VkAccessFlags	dstAccessMask;
				VK_QUEUE_FAMILY_IGNORED,					//  deUint32		srcQueueFamilyIndex;
				VK_QUEUE_FAMILY_IGNORED,					//  deUint32		destQueueFamilyIndex;
				m_resetBuffer->object(),					//  VkBuffer		buffer;
				0u,											//  VkDeviceSize	offset;
				1u * stride + dstOffsetQuery,				//  VkDeviceSize	size;
			};
			vk.cmdPipelineBarrier(*secondaryCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1u, &barrier, 0, (const VkImageMemoryBarrier*)DE_NULL);
		}

	endCommandBuffer(vk, *secondaryCmdBuffer);

	beginCommandBuffer(vk, *primaryCmdBuffer);
		vk.cmdExecuteCommands(*primaryCmdBuffer, 1u, &secondaryCmdBuffer.get());

		vk.cmdPipelineBarrier(*primaryCmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
			(VkDependencyFlags)0u, 0u, (const VkMemoryBarrier*)DE_NULL, 1u, &computeFinishBarrier, 0u, (const VkImageMemoryBarrier*)DE_NULL);

	endCommandBuffer(vk, *primaryCmdBuffer);

	// Secondary buffer is emitted only once, so it is safe to reset the query pool here.
	if (m_parameters[0].resetType == RESET_TYPE_HOST)
		vk.resetQueryPool(device, *queryPool, 0u, 1u);

	// Wait for completion
	submitCommandsAndWait(vk, device, queue, *primaryCmdBuffer);
	return checkResult(buffer, *queryPool);
}

tcu::TestStatus ComputeInvocationsSecondaryTestInstance::checkResult (const BufferPtr buffer, const VkQueryPool queryPool)
{
	const auto&				deviceHelper		= getDeviceHelper(m_context, m_useComputeQueue);
	const DeviceInterface&	vk					= deviceHelper.getDeviceInterface();
	const VkDevice			device				= deviceHelper.getDevice();
	{
		deUint64 expected	= 0u;
		for(size_t parametersNdx = 0; parametersNdx < m_parameters.size(); ++parametersNdx)
			expected += getComputeExecution(m_parameters[parametersNdx]);

		if (m_parameters[0].resetType == RESET_TYPE_NORMAL || m_parameters[0].resetType == RESET_TYPE_AFTER_COPY)
		{
			ResultsVector results;
			if (m_parameters[0].copyType == COPY_TYPE_CMD)
			{
				const vk::Allocation& allocation = m_resetBuffer->getBoundMemory();
				cmdCopyQueryPoolResultsVector(results, vk, device, allocation, 1u, (VK_QUERY_RESULT_WAIT_BIT | m_parameters[0].querySizeFlags()), m_parameters[0].dstOffset);
			}
			else
			{
				VK_CHECK(GetQueryPoolResultsVector(results, vk, device, queryPool, 0u, 1u, (VK_QUERY_RESULT_WAIT_BIT | m_parameters[0].querySizeFlags())));
			}

			if (expected != results[0])
				return tcu::TestStatus::fail("QueryPoolResults incorrect");
		}
		else if (m_parameters[0].resetType == RESET_TYPE_HOST)
		{
			ResultsVectorWithAvailability results;

			if (m_parameters[0].copyType == COPY_TYPE_CMD)
			{
				const vk::Allocation& allocation = m_resetBuffer->getBoundMemory();
				cmdCopyQueryPoolResultsVector(results, vk, device, allocation, 1u, (VK_QUERY_RESULT_WAIT_BIT | m_parameters[0].querySizeFlags() | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT), m_parameters[0].dstOffset);
			}
			else
			{
				VK_CHECK(GetQueryPoolResultsVector(results, vk, device, queryPool, 0u, 1u, (VK_QUERY_RESULT_WAIT_BIT | m_parameters[0].querySizeFlags() | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT)));
			}

			if (expected != results[0].first || results[0].second == 0u)
				return tcu::TestStatus::fail("QueryPoolResults incorrect");

			deUint64 temp = results[0].first;

			vk.resetQueryPool(device, queryPool, 0u, 1u);
			vk::VkResult res = GetQueryPoolResultsVector(results, vk, device, queryPool, 0u, 1u, (m_parameters[0].querySizeFlags() | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT));
			/* From Vulkan spec:
			 *
			 * If VK_QUERY_RESULT_WAIT_BIT and VK_QUERY_RESULT_PARTIAL_BIT are both not set then no result values are written to pData
			 * for queries that are in the unavailable state at the time of the call, and vkGetQueryPoolResults returns VK_NOT_READY.
			 * However, availability state is still written to pData for those queries if VK_QUERY_RESULT_WITH_AVAILABILITY_BIT is set.
			 */
			if (res != vk::VK_NOT_READY || results[0].first != temp || results[0].second != 0u)
				return tcu::TestStatus::fail("QueryPoolResults incorrect reset");
		}
		else
		{
			// With RESET_TYPE_BEFORE_COPY, we only need to verify the result after the copy include an availability bit set as zero.
			return verifyUnavailable();
		}

	}

	{
		// Validate the results
		const Allocation&	bufferAllocation	= buffer->getBoundMemory();
		invalidateAlloc(vk, device, bufferAllocation);
		const deUint32*		bufferPtr			= static_cast<deUint32*>(bufferAllocation.getHostPtr());
		deUint32			minSize				= ~0u;
		for(size_t parametersNdx = 0; parametersNdx < m_parameters.size(); ++parametersNdx)
			minSize = deMinu32(minSize, getComputeExecution(m_parameters[parametersNdx]));
		for (deUint32 ndx = 0u; ndx < minSize; ++ndx)
		{
			if (bufferPtr[ndx] != ndx * m_parameters.size())
				return tcu::TestStatus::fail("Compute shader didn't write data to the buffer");
		}
	}
	return tcu::TestStatus::pass("Pass");
}

class ComputeInvocationsSecondaryInheritedTestInstance : public ComputeInvocationsSecondaryTestInstance
{
public:
					ComputeInvocationsSecondaryInheritedTestInstance	(Context& context, const std::vector<ParametersCompute>& parameters);
protected:
	virtual void	checkExtensions							(deBool hostResetQueryEnabled);

	tcu::TestStatus	executeTest								(const VkCommandPool&			cmdPool,
															 const VkPipelineLayout			pipelineLayout,
															 const VkDescriptorSet&			descriptorSet,
															 const BufferPtr				buffer,
															 const VkDeviceSize				bufferSizeBytes);
};

ComputeInvocationsSecondaryInheritedTestInstance::ComputeInvocationsSecondaryInheritedTestInstance	(Context& context, const std::vector<ParametersCompute>& parameters)
	: ComputeInvocationsSecondaryTestInstance	(context, parameters)
{
}

void ComputeInvocationsSecondaryInheritedTestInstance::checkExtensions (deBool hostResetQueryEnabled)
{
	StatisticQueryTestInstance::checkExtensions(hostResetQueryEnabled);
	if (!m_context.getDeviceFeatures().inheritedQueries)
		throw tcu::NotSupportedError("Inherited queries are not supported");
}

tcu::TestStatus ComputeInvocationsSecondaryInheritedTestInstance::executeTest (const VkCommandPool&			cmdPool,
																			  const VkPipelineLayout		pipelineLayout,
																			  const VkDescriptorSet&		descriptorSet,
																			  const BufferPtr				buffer,
																			  const VkDeviceSize			bufferSizeBytes)
{
	typedef de::SharedPtr<Unique<VkShaderModule> >	VkShaderModuleSp;
	typedef de::SharedPtr<Unique<VkPipeline> >		VkPipelineSp;

	const auto&									deviceHelper					= getDeviceHelper(m_context, m_useComputeQueue);
	const DeviceInterface&						vk								= deviceHelper.getDeviceInterface();
	const VkDevice								device							= deviceHelper.getDevice();
	const VkQueue								queue							= deviceHelper.getQueue();

	const VkBufferMemoryBarrier					computeShaderWriteBarrier		=
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,					// VkStructureType	sType;
		DE_NULL,													// const void*		pNext;
		VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,		// VkAccessFlags	srcAccessMask;
		VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,		// VkAccessFlags	dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,									// deUint32			srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,									// deUint32			destQueueFamilyIndex;
		buffer->object(),											// VkBuffer			buffer;
		0ull,														// VkDeviceSize		offset;
		bufferSizeBytes,											// VkDeviceSize		size;
	};

	const VkBufferMemoryBarrier					computeFinishBarrier			=
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,					// VkStructureType	sType;
		DE_NULL,													// const void*		pNext;
		VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,		// VkAccessFlags	srcAccessMask;
		VK_ACCESS_HOST_READ_BIT,									// VkAccessFlags	dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,									// deUint32			srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,									// deUint32			destQueueFamilyIndex;
		buffer->object(),											// VkBuffer			buffer;
		0ull,														// VkDeviceSize		offset;
		bufferSizeBytes,											// VkDeviceSize		size;
	};

	std::vector<VkShaderModuleSp>				shaderModule;
	std::vector<VkPipelineSp>					pipeline;
	for(size_t parametersNdx = 0u; parametersNdx < m_parameters.size(); ++parametersNdx)
	{
		shaderModule.push_back(VkShaderModuleSp(new Unique<VkShaderModule>(createShaderModule(vk, device, m_context.getBinaryCollection().get(m_parameters[parametersNdx].shaderName), (VkShaderModuleCreateFlags)0u))));
		const VkPipelineShaderStageCreateInfo	pipelineShaderStageParams		=
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType					sType;
			DE_NULL,												// const void*						pNext;
			0u,														// VkPipelineShaderStageCreateFlags	flags;
			VK_SHADER_STAGE_COMPUTE_BIT,							// VkShaderStageFlagBits			stage;
			shaderModule.back().get()->get(),						// VkShaderModule					module;
			"main",													// const char*						pName;
			DE_NULL,												// const VkSpecializationInfo*		pSpecializationInfo;
		};

		const VkComputePipelineCreateInfo		pipelineCreateInfo				=
		{
			VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,	// VkStructureType					sType;
			DE_NULL,										// const void*						pNext;
			0u,												// VkPipelineCreateFlags			flags;
			pipelineShaderStageParams,						// VkPipelineShaderStageCreateInfo	stage;
			pipelineLayout,									// VkPipelineLayout					layout;
			DE_NULL,										// VkPipeline						basePipelineHandle;
			0,												// deInt32							basePipelineIndex;
		};
		pipeline.push_back(VkPipelineSp(new Unique<VkPipeline>(createComputePipeline(vk, device, DE_NULL , &pipelineCreateInfo))));
	}

	const Unique<VkCommandBuffer>				primaryCmdBuffer			(allocateCommandBuffer(vk, device, cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	const Unique<VkCommandBuffer>				secondaryCmdBuffer			(allocateCommandBuffer(vk, device, cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY));

	const Unique<VkQueryPool>					queryPool					(makeQueryPool(vk, device, 1u, VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT));

	clearBuffer(vk, device, buffer, bufferSizeBytes);
	beginSecondaryCommandBuffer(vk, *secondaryCmdBuffer, VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT);
		vk.cmdBindDescriptorSets(*secondaryCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0u, 1u, &descriptorSet, 0u, DE_NULL);
		for(size_t parametersNdx = 1; parametersNdx < m_parameters.size(); ++parametersNdx)
		{
				vk.cmdBindPipeline(*secondaryCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline[parametersNdx].get()->get());
				vk.cmdDispatch(*secondaryCmdBuffer, m_parameters[parametersNdx].groupSize.x(), m_parameters[parametersNdx].groupSize.y(), m_parameters[parametersNdx].groupSize.z());

				vk.cmdPipelineBarrier(*secondaryCmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
					(VkDependencyFlags)0u, 0u, (const VkMemoryBarrier*)DE_NULL, 1u, &computeShaderWriteBarrier, 0u, (const VkImageMemoryBarrier*)DE_NULL);
		}
	endCommandBuffer(vk, *secondaryCmdBuffer);

	beginCommandBuffer(vk, *primaryCmdBuffer);
		if (m_parameters[0].resetType != RESET_TYPE_HOST)
			vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, 1u);
		vk.cmdBindDescriptorSets(*primaryCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0u, 1u, &descriptorSet, 0u, DE_NULL);
		vk.cmdBindPipeline(*primaryCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline[0].get()->get());

		vk.cmdBeginQuery(*primaryCmdBuffer, *queryPool, 0u, (VkQueryControlFlags)0u);
		vk.cmdDispatch(*primaryCmdBuffer, m_parameters[0].groupSize.x(), m_parameters[0].groupSize.y(), m_parameters[0].groupSize.z());

		vk.cmdPipelineBarrier(*primaryCmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				(VkDependencyFlags)0u, 0u, (const VkMemoryBarrier*)DE_NULL, 1u, &computeShaderWriteBarrier, 0u, (const VkImageMemoryBarrier*)DE_NULL);

		vk.cmdExecuteCommands(*primaryCmdBuffer, 1u, &secondaryCmdBuffer.get());

		vk.cmdPipelineBarrier(*primaryCmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
			(VkDependencyFlags)0u, 0u, (const VkMemoryBarrier*)DE_NULL, 1u, &computeFinishBarrier, 0u, (const VkImageMemoryBarrier*)DE_NULL);

		vk.cmdEndQuery(*primaryCmdBuffer, *queryPool, 0u);

		if (m_parameters[0].resetType == RESET_TYPE_BEFORE_COPY || m_parameters[0].resetType == RESET_TYPE_AFTER_COPY || m_parameters[0].copyType == COPY_TYPE_CMD)
		{
			VkDeviceSize stride = m_parameters[0].querySizeFlags() ? sizeof(deUint64) : sizeof(deUint32);
			vk::VkQueryResultFlags flags = m_parameters[0].querySizeFlags() | VK_QUERY_RESULT_WAIT_BIT;

			if (m_parameters[0].resetType == RESET_TYPE_HOST)
			{
				flags |= VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
				stride *= 2u;
			}

			if (m_parameters[0].resetType == RESET_TYPE_BEFORE_COPY)
			{
				vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, 1u);
				flags = VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
				stride = sizeof(ValueAndAvailability);
			}

			VkDeviceSize dstOffsetQuery = (m_parameters[0].dstOffset) ? stride : 0;
			VkDeviceSize copyStride = stride;
			if (m_parameters[0].strideType == STRIDE_TYPE_ZERO)
				copyStride = 0u;

			vk.cmdCopyQueryPoolResults(*primaryCmdBuffer, *queryPool, 0, 1u, m_resetBuffer->object(), dstOffsetQuery, copyStride, flags);

			if (m_parameters[0].resetType == RESET_TYPE_AFTER_COPY)
				vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, 1u);

			const VkBufferMemoryBarrier barrier =
			{
				VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	//  VkStructureType	sType;
				DE_NULL,									//  const void*		pNext;
				VK_ACCESS_TRANSFER_WRITE_BIT,				//  VkAccessFlags	srcAccessMask;
				VK_ACCESS_HOST_READ_BIT,					//  VkAccessFlags	dstAccessMask;
				VK_QUEUE_FAMILY_IGNORED,					//  deUint32		srcQueueFamilyIndex;
				VK_QUEUE_FAMILY_IGNORED,					//  deUint32		destQueueFamilyIndex;
				m_resetBuffer->object(),					//  VkBuffer		buffer;
				0u,											//  VkDeviceSize	offset;
			    1u * stride + dstOffsetQuery,				//  VkDeviceSize	size;
			};
			vk.cmdPipelineBarrier(*primaryCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1u, &barrier, 0, (const VkImageMemoryBarrier*)DE_NULL);
		}

	endCommandBuffer(vk, *primaryCmdBuffer);

	if (m_parameters[0].resetType == RESET_TYPE_HOST)
		vk.resetQueryPool(device, *queryPool, 0u, 1u);

	// Wait for completion
	submitCommandsAndWait(vk, device, queue, *primaryCmdBuffer);
	return checkResult(buffer, *queryPool);
}

class GraphicBasicTestInstance : public StatisticQueryTestInstance
{
public:
	struct VertexData
	{
		VertexData (const tcu::Vec4 position_, const tcu::Vec4 color_)
			: position	(position_)
			, color		(color_)
		{}
		tcu::Vec4	position;
		tcu::Vec4	color;
	};

	struct ParametersGraphic : public GenericParameters
	{
		ParametersGraphic (const VkQueryPipelineStatisticFlags queryStatisticFlags_, const VkPrimitiveTopology primitiveTopology_, const ResetType resetType_, const CopyType copyType_, const deBool query64Bits_, const deBool vertexOnlyPipe_ = DE_FALSE, const deBool dstOffset_ = DE_FALSE, const ClearOperation clearOp_ = CLEAR_NOOP, const deBool noColorAttachments_ = DE_FALSE, const StrideType strideType_ = STRIDE_TYPE_VALID, const deBool hasTess_ = false)
			: GenericParameters		{resetType_, copyType_, query64Bits_, dstOffset_, strideType_}
			, queryStatisticFlags	(queryStatisticFlags_)
			, primitiveTopology		(primitiveTopology_)
			, vertexOnlyPipe		(vertexOnlyPipe_)
			, clearOp				(clearOp_)
			, noColorAttachments			(noColorAttachments_)
			, hasTess				(hasTess_)
			{}

		VkQueryPipelineStatisticFlags	queryStatisticFlags;
		VkPrimitiveTopology				primitiveTopology;
		deBool							vertexOnlyPipe;
		ClearOperation					clearOp;
		deBool							noColorAttachments;
		deBool							hasTess;
	};
											GraphicBasicTestInstance			(vkt::Context&					context,
																				 const std::vector<VertexData>&	data,
																				 const ParametersGraphic&		parametersGraphic,
																				 const std::vector<deUint64>&	drawRepeats );
	tcu::TestStatus							iterate								(void);
protected:
	BufferPtr								creatAndFillVertexBuffer			(void);
	virtual void							createPipeline						(void) = 0;
	void									commandClearAttachment				(const vk::DeviceInterface&	vk,
																				 const vk::VkCommandBuffer	commandBuffer);
	void									creatColorAttachmentAndRenderPass	(void);
	bool									checkImage							(void);
	virtual tcu::TestStatus					executeTest							(void) = 0;
	virtual tcu::TestStatus					checkResult							(VkQueryPool queryPool) = 0;
	virtual void							draw								(VkCommandBuffer cmdBuffer) = 0;

	const VkFormat						m_colorAttachmentFormat;
	de::SharedPtr<Image>				m_colorAttachmentImage;
	de::SharedPtr<Image>				m_depthImage;
	Move<VkImageView>					m_attachmentView;
	Move<VkImageView>					m_depthView;
	Move<VkRenderPass>					m_renderPass;
	Move<VkFramebuffer>					m_framebuffer;
	Move<VkPipeline>					m_pipeline;
	Move<VkPipelineLayout>				m_pipelineLayout;
	const std::vector<VertexData>&		m_data;
	const ParametersGraphic&			m_parametersGraphic;
	std::vector<deUint64>				m_drawRepeats;
};

GraphicBasicTestInstance::GraphicBasicTestInstance (vkt::Context&					context,
													const std::vector<VertexData>&	data,
													const ParametersGraphic&		parametersGraphic,
													const std::vector<deUint64>&	drawRepeats )
	: StatisticQueryTestInstance	(context, static_cast<deUint32>(drawRepeats.size()), parametersGraphic.dstOffset, false)
	, m_colorAttachmentFormat		(VK_FORMAT_R8G8B8A8_UNORM)
	, m_data						(data)
	, m_parametersGraphic			(parametersGraphic)
	, m_drawRepeats					(drawRepeats)
{
}

tcu::TestStatus GraphicBasicTestInstance::iterate (void)
{
	checkExtensions((m_parametersGraphic.resetType == RESET_TYPE_HOST)? DE_TRUE : DE_FALSE);
	creatColorAttachmentAndRenderPass();
	createPipeline();
	return executeTest();
}

BufferPtr GraphicBasicTestInstance::creatAndFillVertexBuffer (void)
{
	const DeviceInterface&		vk				= m_context.getDeviceInterface();
	const VkDevice				device			= m_context.getDevice();

	const VkDeviceSize			dataSize		= static_cast<VkDeviceSize>(deAlignSize(static_cast<size_t>( m_data.size() * sizeof(VertexData)),
		static_cast<size_t>(m_context.getDeviceProperties().limits.nonCoherentAtomSize)));

	BufferPtr					vertexBuffer	= Buffer::createAndAlloc(vk, device, BufferCreateInfo(dataSize,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), m_context.getDefaultAllocator(), MemoryRequirement::HostVisible);

	deUint8*					ptr				= reinterpret_cast<deUint8*>(vertexBuffer->getBoundMemory().getHostPtr());
	deMemcpy(ptr, &m_data[0], static_cast<size_t>( m_data.size() * sizeof(VertexData)));

	flushMappedMemoryRange(vk, device, vertexBuffer->getBoundMemory().getMemory(), vertexBuffer->getBoundMemory().getOffset(), dataSize);
	return vertexBuffer;
}

void GraphicBasicTestInstance::commandClearAttachment (const vk::DeviceInterface&	vk,
													   const vk::VkCommandBuffer	commandBuffer)
{
	const vk::VkOffset2D offset = vk::makeOffset2D(0, 0);
	const vk::VkExtent2D extent = vk::makeExtent2D(WIDTH, HEIGHT);

	const vk::VkClearAttachment	attachment =
	{
		m_parametersGraphic.clearOp == CLEAR_COLOR ? vk::VK_IMAGE_ASPECT_COLOR_BIT	: vk::VK_IMAGE_ASPECT_DEPTH_BIT, // VkImageAspectFlags	aspectMask;
		m_parametersGraphic.clearOp == CLEAR_COLOR ? 0u : 1u,														 // uint32_t			colorAttachment;
		m_parametersGraphic.clearOp == CLEAR_COLOR ? vk::makeClearValueColor(tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f)) :
													 vk::makeClearValueDepthStencil(0.0f, 0u)						 // VkClearValue		clearValue;
	};

	const vk::VkClearRect rect =
	{
		{ offset, extent },		// VkRect2D		rect;
		0u,						// uint32_t		baseArrayLayer;
		1u,						// uint32_t		layerCount;
	};

	vk.cmdClearAttachments(commandBuffer, 1u, &attachment, 1u, &rect);
}

void GraphicBasicTestInstance::creatColorAttachmentAndRenderPass (void)
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDevice			device	= m_context.getDevice();

	VkExtent3D					imageExtent				=
	{
		WIDTH,	// width;
		HEIGHT,	// height;
		1u		// depth;
	};

	if (!m_parametersGraphic.noColorAttachments)
	{

		const ImageCreateInfo		colorImageCreateInfo	(VK_IMAGE_TYPE_2D, m_colorAttachmentFormat, imageExtent, 1, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL,
															VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

		m_colorAttachmentImage	= Image::createAndAlloc(vk, device, colorImageCreateInfo, m_context.getDefaultAllocator(), m_context.getUniversalQueueFamilyIndex());

		const ImageViewCreateInfo	attachmentViewInfo		(m_colorAttachmentImage->object(), VK_IMAGE_VIEW_TYPE_2D, m_colorAttachmentFormat);
		m_attachmentView			= createImageView(vk, device, &attachmentViewInfo);
	}


	ImageCreateInfo				depthImageCreateInfo	(vk::VK_IMAGE_TYPE_2D, VK_FORMAT_D16_UNORM, imageExtent, 1, 1, vk::VK_SAMPLE_COUNT_1_BIT, vk::VK_IMAGE_TILING_OPTIMAL,
														 vk::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
	m_depthImage				= Image::createAndAlloc(vk, device, depthImageCreateInfo, m_context.getDefaultAllocator(), m_context.getUniversalQueueFamilyIndex());

	// Construct a depth  view from depth image
	const ImageViewCreateInfo	depthViewInfo			(m_depthImage->object(), vk::VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_D16_UNORM);
	m_depthView				= vk::createImageView(vk, device, &depthViewInfo);

	// Renderpass and Framebuffer
	if (m_parametersGraphic.noColorAttachments)
	{
		RenderPassCreateInfo renderPassCreateInfo;

		renderPassCreateInfo.addAttachment(AttachmentDescription(VK_FORMAT_D16_UNORM,										// format
																 vk::VK_SAMPLE_COUNT_1_BIT,									// samples
																 vk::VK_ATTACHMENT_LOAD_OP_CLEAR,							// loadOp
																 vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,						// storeOp
																 vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,						// stencilLoadOp
																 vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,						// stencilLoadOp
																 vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,		// initialLauout
																 vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));	// finalLayout

		const VkAttachmentReference depthAttachmentReference =
		{
			0u,															// attachment
			vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL		// layout
		};

		renderPassCreateInfo.addSubpass(SubpassDescription(vk::VK_PIPELINE_BIND_POINT_GRAPHICS,					// pipelineBindPoint
														   0,													// flags
														   0,													// inputCount
														   DE_NULL,												// pInputAttachments
														   0,													// colorCount
														   DE_NULL,												// pColorAttachments
														   DE_NULL,												// pResolveAttachments
														   depthAttachmentReference,							// depthStencilAttachment
														   0,													// preserveCount
														   DE_NULL));											// preserveAttachments
		m_renderPass = vk::createRenderPass(vk, device, &renderPassCreateInfo);

		std::vector<vk::VkImageView> attachments(1);
		attachments[0] = *m_depthView;

		FramebufferCreateInfo framebufferCreateInfo(*m_renderPass, attachments, WIDTH, HEIGHT, 1);
		m_framebuffer = vk::createFramebuffer(vk, device, &framebufferCreateInfo);
	}
	else
	{
		RenderPassCreateInfo		renderPassCreateInfo;
		renderPassCreateInfo.addAttachment(AttachmentDescription(m_colorAttachmentFormat,						// format
																	VK_SAMPLE_COUNT_1_BIT,						// samples
																	VK_ATTACHMENT_LOAD_OP_CLEAR,				// loadOp
																	VK_ATTACHMENT_STORE_OP_STORE ,				// storeOp
																	VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// stencilLoadOp
																	VK_ATTACHMENT_STORE_OP_STORE ,				// stencilLoadOp
																	VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// initialLauout
																	VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));	// finalLayout

		renderPassCreateInfo.addAttachment(AttachmentDescription(VK_FORMAT_D16_UNORM,										// format
																 vk::VK_SAMPLE_COUNT_1_BIT,									// samples
																 vk::VK_ATTACHMENT_LOAD_OP_CLEAR,							// loadOp
																 vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,						// storeOp
																 vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,						// stencilLoadOp
																 vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,						// stencilLoadOp
																 vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,		// initialLauout
																 vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));	// finalLayout

		const VkAttachmentReference	colorAttachmentReference =
		{
			0u,											// attachment
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// layout
		};

		const VkAttachmentReference depthAttachmentReference =
		{
			1u,															// attachment
			vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL		// layout
		};

		const VkSubpassDescription	subpass =
		{
			(VkSubpassDescriptionFlags) 0,		//VkSubpassDescriptionFlags		flags;
			VK_PIPELINE_BIND_POINT_GRAPHICS,	//VkPipelineBindPoint			pipelineBindPoint;
			0u,									//deUint32						inputAttachmentCount;
			DE_NULL,							//const VkAttachmentReference*	pInputAttachments;
			1u,									//deUint32						colorAttachmentCount;
			&colorAttachmentReference,			//const VkAttachmentReference*	pColorAttachments;
			DE_NULL,							//const VkAttachmentReference*	pResolveAttachments;
			&depthAttachmentReference,			//const VkAttachmentReference*	pDepthStencilAttachment;
			0u,									//deUint32						preserveAttachmentCount;
			DE_NULL,							//const deUint32*				pPreserveAttachments;
		};

		renderPassCreateInfo.addSubpass(subpass);
		m_renderPass = createRenderPass(vk, device, &renderPassCreateInfo);

		std::vector<vk::VkImageView> attachments(2);
		attachments[0] = *m_attachmentView;
		attachments[1] = *m_depthView;

		FramebufferCreateInfo		framebufferCreateInfo(*m_renderPass, attachments, WIDTH, HEIGHT, 1);
		m_framebuffer = createFramebuffer(vk, device, &framebufferCreateInfo);
	}
}

bool GraphicBasicTestInstance::checkImage (void)
{
	if (m_parametersGraphic.vertexOnlyPipe)
		return true;

	const VkQueue						queue			= m_context.getUniversalQueue();
	const VkOffset3D					zeroOffset		= { 0, 0, 0 };
	const tcu::ConstPixelBufferAccess	renderedFrame	= m_colorAttachmentImage->readSurface(queue, m_context.getDefaultAllocator(),
															VK_IMAGE_LAYOUT_GENERAL, zeroOffset, WIDTH, HEIGHT, VK_IMAGE_ASPECT_COLOR_BIT);
	int									colorNdx		= 0;
	tcu::Texture2D						referenceFrame	(mapVkFormat(m_colorAttachmentFormat), WIDTH, HEIGHT);
	referenceFrame.allocLevel(0);

	for (int y = 0; y < HEIGHT/2; ++y)
	for (int x = 0; x < WIDTH/2; ++x)
			referenceFrame.getLevel(0).setPixel(m_data[colorNdx].color, x, y);

	colorNdx += 4;
	for (int y =  HEIGHT/2; y < HEIGHT; ++y)
	for (int x = 0; x < WIDTH/2; ++x)
			referenceFrame.getLevel(0).setPixel(m_data[colorNdx].color, x, y);

	colorNdx += 4;
	for (int y = 0; y < HEIGHT/2; ++y)
	for (int x =  WIDTH/2; x < WIDTH; ++x)
			referenceFrame.getLevel(0).setPixel(m_data[colorNdx].color, x, y);

	colorNdx += 4;
	for (int y =  HEIGHT/2; y < HEIGHT; ++y)
	for (int x =  WIDTH/2; x < WIDTH; ++x)
			referenceFrame.getLevel(0).setPixel(m_data[colorNdx].color, x, y);

	return tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Result", "Image comparison result", referenceFrame.getLevel(0), renderedFrame, tcu::Vec4(0.01f), tcu::COMPARE_LOG_ON_ERROR);
}

class VertexShaderTestInstance : public GraphicBasicTestInstance
{
public:
							VertexShaderTestInstance	(vkt::Context& context, const std::vector<VertexData>& data, const ParametersGraphic& parametersGraphic, const std::vector<deUint64>& drawRepeats);
protected:
	virtual void			createPipeline				(void);
	virtual tcu::TestStatus	executeTest					(void);
	virtual tcu::TestStatus	checkResult					(VkQueryPool queryPool);
	void					draw						(VkCommandBuffer cmdBuffer);
};

VertexShaderTestInstance::VertexShaderTestInstance (vkt::Context& context, const std::vector<VertexData>& data, const ParametersGraphic& parametersGraphic, const std::vector<deUint64>& drawRepeats)
	: GraphicBasicTestInstance	(context, data, parametersGraphic, drawRepeats )
{
}

void VertexShaderTestInstance::createPipeline (void)
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDevice			device	= m_context.getDevice();

	switch (m_parametersGraphic.primitiveTopology)
	{
		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
			if (!m_context.getDeviceFeatures().geometryShader)
				throw tcu::NotSupportedError("Geometry shader are not supported");
			break;
		default:
			break;
	}

	// Pipeline
	Unique<VkShaderModule>	vs(createShaderModule(vk, device, m_context.getBinaryCollection().get("vertex"), 0));
	Move<VkShaderModule>	fs;

	if (!m_parametersGraphic.vertexOnlyPipe)
		fs = createShaderModule(vk, device, m_context.getBinaryCollection().get("fragment"), 0);

	const PipelineCreateInfo::ColorBlendState::Attachment attachmentState;

	const PipelineLayoutCreateInfo pipelineLayoutCreateInfo;
	m_pipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);

	const VkVertexInputBindingDescription vertexInputBindingDescription		=
	{
		0,											// binding;
		static_cast<deUint32>(sizeof(VertexData)),	// stride;
		VK_VERTEX_INPUT_RATE_VERTEX				// inputRate
	};

	const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[] =
	{
		{
			0u,
			0u,
			VK_FORMAT_R32G32B32A32_SFLOAT,
			0u
		},	// VertexElementData::position
		{
			1u,
			0u,
			VK_FORMAT_R32G32B32A32_SFLOAT,
			static_cast<deUint32>(sizeof(tcu::Vec4))
		},	// VertexElementData::color
	};

	const VkPipelineVertexInputStateCreateInfo vf_info			=
	{																	// sType;
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// pNext;
		NULL,															// flags;
		0u,																// vertexBindingDescriptionCount;
		1u,																// pVertexBindingDescriptions;
		&vertexInputBindingDescription,									// vertexAttributeDescriptionCount;
		2u,																// pVertexAttributeDescriptions;
		vertexInputAttributeDescriptions
	};

	PipelineCreateInfo pipelineCreateInfo(*m_pipelineLayout, *m_renderPass, 0, (VkPipelineCreateFlags)0);
	pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*vs, "main", VK_SHADER_STAGE_VERTEX_BIT));
	if (!m_parametersGraphic.vertexOnlyPipe)
		pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*fs, "main", VK_SHADER_STAGE_FRAGMENT_BIT));
	pipelineCreateInfo.addState(PipelineCreateInfo::DepthStencilState());
	pipelineCreateInfo.addState(PipelineCreateInfo::InputAssemblerState(m_parametersGraphic.primitiveTopology));
	pipelineCreateInfo.addState(PipelineCreateInfo::ColorBlendState(1, &attachmentState));

	const VkViewport	viewport	= makeViewport(WIDTH, HEIGHT);
	const VkRect2D		scissor		= makeRect2D(WIDTH, HEIGHT);
	pipelineCreateInfo.addState(PipelineCreateInfo::ViewportState(1u, std::vector<VkViewport>(1, viewport), std::vector<VkRect2D>(1, scissor)));
	pipelineCreateInfo.addState(PipelineCreateInfo::DepthStencilState());
	pipelineCreateInfo.addState(PipelineCreateInfo::RasterizerState());
	pipelineCreateInfo.addState(PipelineCreateInfo::MultiSampleState());
	pipelineCreateInfo.addState(vf_info);
	m_pipeline = createGraphicsPipeline(vk, device, DE_NULL, &pipelineCreateInfo);
}

tcu::TestStatus VertexShaderTestInstance::executeTest (void)
{
	const DeviceInterface&					vk						= m_context.getDeviceInterface();
	const VkDevice							device					= m_context.getDevice();
	const VkQueue							queue					= m_context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();

	const CmdPoolCreateInfo					cmdPoolCreateInfo		(queueFamilyIndex);
	const Move<VkCommandPool>				cmdPool					= createCommandPool(vk, device, &cmdPoolCreateInfo);
	const deUint32							queryCount				= static_cast<deUint32>(m_drawRepeats.size());
	const Unique<VkQueryPool>				queryPool				(makeQueryPool(vk, device, queryCount, m_parametersGraphic.queryStatisticFlags));

	const VkDeviceSize						vertexBufferOffset		= 0u;
	const BufferPtr							vertexBufferSp			= creatAndFillVertexBuffer();
	const VkBuffer							vertexBuffer			= vertexBufferSp->object();

	const Unique<VkCommandBuffer>			cmdBuffer				(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	beginCommandBuffer(vk, *cmdBuffer);
	{
		std::vector<VkClearValue>	renderPassClearValues	(2);
		deMemset(&renderPassClearValues[0], 0, static_cast<int>(renderPassClearValues.size()) * sizeof(VkClearValue));

		if (!m_parametersGraphic.noColorAttachments)
			initialTransitionColor2DImage(vk, *cmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
										  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
		initialTransitionDepth2DImage(vk, *cmdBuffer, m_depthImage->object(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
									  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

		if (m_parametersGraphic.resetType != RESET_TYPE_HOST)
			vk.cmdResetQueryPool(*cmdBuffer, *queryPool, 0u, queryCount);

		beginRenderPass(vk, *cmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, WIDTH, HEIGHT), (deUint32)renderPassClearValues.size(), &renderPassClearValues[0]);

		for (deUint32 i = 0; i < queryCount; ++i)
		{
			vk.cmdBeginQuery(*cmdBuffer, *queryPool, i, (VkQueryControlFlags)0u);
			vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);

			for(deUint64 j=0; j<m_drawRepeats[i]; ++j)
				draw(*cmdBuffer);

			commandClearAttachment(vk, *cmdBuffer);
			vk.cmdEndQuery(*cmdBuffer, *queryPool, i);
		}

		endRenderPass(vk, *cmdBuffer);

		if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY || m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY || m_parametersGraphic.copyType == COPY_TYPE_CMD)
		{
			VkDeviceSize stride = m_parametersGraphic.querySizeFlags() ? sizeof(deUint64) : sizeof(deUint32);
			vk::VkQueryResultFlags flags = m_parametersGraphic.querySizeFlags() | VK_QUERY_RESULT_WAIT_BIT;

			if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
			{
				flags |= VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
				stride *= 2u;
			}

			if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY)
			{
				vk.cmdResetQueryPool(*cmdBuffer, *queryPool, 0u, queryCount);
				flags = VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
				stride = sizeof(ValueAndAvailability);
			}

			VkDeviceSize dstOffsetQuery = (m_parametersGraphic.dstOffset) ? stride : 0;
			vk.cmdCopyQueryPoolResults(*cmdBuffer, *queryPool, 0, queryCount, m_resetBuffer->object(), dstOffsetQuery, stride, flags);

			if (m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY)
				vk.cmdResetQueryPool(*cmdBuffer, *queryPool, 0u, queryCount);

			const VkBufferMemoryBarrier barrier =
			{
				VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	//  VkStructureType	sType;
				DE_NULL,									//  const void*		pNext;
				VK_ACCESS_TRANSFER_WRITE_BIT,				//  VkAccessFlags	srcAccessMask;
				VK_ACCESS_HOST_READ_BIT,					//  VkAccessFlags	dstAccessMask;
				VK_QUEUE_FAMILY_IGNORED,					//  deUint32		srcQueueFamilyIndex;
				VK_QUEUE_FAMILY_IGNORED,					//  deUint32		destQueueFamilyIndex;
				m_resetBuffer->object(),					//  VkBuffer		buffer;
				0u,											//  VkDeviceSize	offset;
				queryCount * stride + dstOffsetQuery,		//  VkDeviceSize	size;
			};
			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1u, &barrier, 0, (const VkImageMemoryBarrier*)DE_NULL);
		}

		if (!m_parametersGraphic.noColorAttachments)
			transition2DImage(vk, *cmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
							  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	}
	endCommandBuffer(vk, *cmdBuffer);

	if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
		vk.resetQueryPool(device, *queryPool, 0u, queryCount);

	// Wait for completion
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);
	return checkResult (*queryPool);
}

tcu::TestStatus VertexShaderTestInstance::checkResult (VkQueryPool queryPool)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			device		= m_context.getDevice();
	deUint64				expectedMin	= 0u;
	deBool					hasMax		= false;
	deUint64				expectedMax	= 0u;

	switch(m_parametersGraphic.queryStatisticFlags)
	{
		case VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT:
			expectedMin = 16u;
			break;
		case VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT:
			expectedMin =	m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST					? 15u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY			?  8u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY		? 14u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY		?  6u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY	?  8u :
							16u;
			break;
		case VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT:
			expectedMin =	m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST						? 16u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST						?  8u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP						? 15u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST					?  5u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP					?  8u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN						? 14u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY			?  4u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY		? 13u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY		?  2u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY	?  6u :
							0u;
			break;
		case VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT:
			expectedMin =	m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST						?     9u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST						?   192u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP						?   448u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST					?  2016u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP					?  4096u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN						? 10208u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY			?   128u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY		?   416u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY		?   992u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY	?  3072u :
							0u;
			break;
		case VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT:
					hasMax = true;
					expectedMax = 0;
			break;
		default:
			DE_FATAL("Unexpected type of statistics query");
			break;
	}

	const deUint32 queryCount = static_cast<deUint32>(m_drawRepeats.size());

	if (m_parametersGraphic.resetType == RESET_TYPE_NORMAL || m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY)
	{
		ResultsVector results(queryCount, 0u);

		if (m_parametersGraphic.copyType == COPY_TYPE_CMD)
		{
			const vk::Allocation& allocation = m_resetBuffer->getBoundMemory();
			cmdCopyQueryPoolResultsVector(results, vk, device, allocation, queryCount, (VK_QUERY_RESULT_WAIT_BIT | m_parametersGraphic.querySizeFlags()), m_parametersGraphic.dstOffset);
		}
		else
		{
			VK_CHECK(GetQueryPoolResultsVector(results, vk, device, queryPool, 0u, queryCount, (VK_QUERY_RESULT_WAIT_BIT | m_parametersGraphic.querySizeFlags())));
		}

		if (results[0] < expectedMin || (hasMax && results[0] > expectedMax))
			return tcu::TestStatus::fail("QueryPoolResults incorrect");
		if (queryCount > 1)
		{
			double pearson = calculatePearsonCorrelation(m_drawRepeats, results);
			if ( fabs( pearson ) < 0.8 )
				return tcu::TestStatus::fail("QueryPoolResults are nonlinear");
		}
	}
	else if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
	{
		ResultsVectorWithAvailability results(queryCount, pair<deUint64, deUint64>(0u,0u));

		if (m_parametersGraphic.copyType == COPY_TYPE_CMD)
		{
			const vk::Allocation& allocation = m_resetBuffer->getBoundMemory();
			cmdCopyQueryPoolResultsVector(results, vk, device, allocation, queryCount, (VK_QUERY_RESULT_WAIT_BIT | m_parametersGraphic.querySizeFlags() | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT), m_parametersGraphic.dstOffset);
		}
		else
		{
			VK_CHECK(GetQueryPoolResultsVector(results, vk, device, queryPool, 0u, queryCount, (VK_QUERY_RESULT_WAIT_BIT | m_parametersGraphic.querySizeFlags() | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT)));
		}

		if (results[0].first < expectedMin || (hasMax && results[0].first > expectedMax) || results[0].second == 0)
			return tcu::TestStatus::fail("QueryPoolResults incorrect");

		if (queryCount > 1)
		{
			double pearson = calculatePearsonCorrelation(m_drawRepeats, results);
			if ( fabs( pearson ) < 0.8 )
				return tcu::TestStatus::fail("QueryPoolResults are nonlinear");
		}

		deUint64 temp = results[0].first;

		vk.resetQueryPool(device, queryPool, 0, queryCount);
		vk::VkResult res = GetQueryPoolResultsVector(results, vk, device, queryPool, 0u, queryCount, (m_parametersGraphic.querySizeFlags() | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT));
		/* From Vulkan spec:
		 *
		 * If VK_QUERY_RESULT_WAIT_BIT and VK_QUERY_RESULT_PARTIAL_BIT are both not set then no result values are written to pData
		 * for queries that are in the unavailable state at the time of the call, and vkGetQueryPoolResults returns VK_NOT_READY.
		 * However, availability state is still written to pData for those queries if VK_QUERY_RESULT_WITH_AVAILABILITY_BIT is set.
		 */
		if (res != vk::VK_NOT_READY || results[0].first != temp || results[0].second != 0)
			return tcu::TestStatus::fail("QueryPoolResults incorrect reset");
	}
	else
	{
		// With RESET_TYPE_BEFORE_COPY, we only need to verify the result after the copy include an availability bit set as zero.
		return verifyUnavailable();
	}

	// Don't need to check the result image when clearing operations are executed.
	if (m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP && m_parametersGraphic.clearOp == CLEAR_NOOP && !m_parametersGraphic.noColorAttachments && !checkImage())
		return tcu::TestStatus::fail("Result image doesn't match expected image.");

	return tcu::TestStatus::pass("Pass");
}

void VertexShaderTestInstance::draw (VkCommandBuffer cmdBuffer)
{
	const DeviceInterface& vk = m_context.getDeviceInterface();
	switch(m_parametersGraphic.primitiveTopology)
	{
		case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
			vk.cmdDraw(cmdBuffer, 16u, 1u, 0u, 0u);
			break;
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
			vk.cmdDraw(cmdBuffer, 4u, 1u, 0u,  0u);
			vk.cmdDraw(cmdBuffer, 4u, 1u, 4u,  1u);
			vk.cmdDraw(cmdBuffer, 4u, 1u, 8u,  2u);
			vk.cmdDraw(cmdBuffer, 4u, 1u, 12u, 3u);
			break;
		default:
			DE_ASSERT(0);
			break;
	}
}

class VertexShaderSecondaryTestInstance : public VertexShaderTestInstance
{
public:
							VertexShaderSecondaryTestInstance	(vkt::Context& context, const std::vector<VertexData>& data, const ParametersGraphic& parametersGraphic, const std::vector<deUint64>& drawRepeats);
protected:
	virtual tcu::TestStatus	executeTest							(void);
};

VertexShaderSecondaryTestInstance::VertexShaderSecondaryTestInstance (vkt::Context& context, const std::vector<VertexData>& data, const ParametersGraphic& parametersGraphic, const std::vector<deUint64>& drawRepeats)
	: VertexShaderTestInstance	(context, data, parametersGraphic, drawRepeats)
{
}

typedef de::SharedPtr<vk::Unique<VkCommandBuffer>> VkCommandBufferSp;

tcu::TestStatus VertexShaderSecondaryTestInstance::executeTest (void)
{
	const DeviceInterface&					vk						= m_context.getDeviceInterface();
	const VkDevice							device					= m_context.getDevice();
	const VkQueue							queue					= m_context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();

	const CmdPoolCreateInfo					cmdPoolCreateInfo		(queueFamilyIndex);
	const Move<VkCommandPool>				cmdPool					= createCommandPool(vk, device, &cmdPoolCreateInfo);
	const deUint32							queryCount				= static_cast<deUint32>(m_drawRepeats.size());
	const Unique<VkQueryPool>				queryPool				(makeQueryPool(vk, device, queryCount, m_parametersGraphic.queryStatisticFlags));

	const VkDeviceSize						vertexBufferOffset		= 0u;
	const BufferPtr							vertexBufferSp			= creatAndFillVertexBuffer();
	const VkBuffer							vertexBuffer			= vertexBufferSp->object();

	const Unique<VkCommandBuffer>			primaryCmdBuffer		(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	std::vector<VkCommandBufferSp>			secondaryCmdBuffers(queryCount);

	for (deUint32 i = 0; i < queryCount; ++i)
		secondaryCmdBuffers[i] = VkCommandBufferSp(new vk::Unique<VkCommandBuffer>(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY)));

	for (deUint32 i = 0; i < queryCount; ++i)
	{
		beginSecondaryCommandBuffer(vk, secondaryCmdBuffers[i]->get(), m_parametersGraphic.queryStatisticFlags, *m_renderPass, *m_framebuffer, VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);
		vk.cmdBeginQuery(secondaryCmdBuffers[i]->get(), *queryPool, i, (VkQueryControlFlags)0u);
		vk.cmdBindPipeline(secondaryCmdBuffers[i]->get(), VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		vk.cmdBindVertexBuffers(secondaryCmdBuffers[i]->get(), 0u, 1u, &vertexBuffer, &vertexBufferOffset);
		for(deUint32 j=0; j<m_drawRepeats[i]; ++j)
			draw(secondaryCmdBuffers[i]->get());
		commandClearAttachment(vk, secondaryCmdBuffers[i]->get());
		vk.cmdEndQuery(secondaryCmdBuffers[i]->get(), *queryPool, i);
		endCommandBuffer(vk, secondaryCmdBuffers[i]->get());
	}

	beginCommandBuffer(vk, *primaryCmdBuffer);
	{
		std::vector<VkClearValue>	renderPassClearValues	(2);
		deMemset(&renderPassClearValues[0], 0, static_cast<int>(renderPassClearValues.size()) * sizeof(VkClearValue));

		if (!m_parametersGraphic.noColorAttachments)
			initialTransitionColor2DImage(vk, *primaryCmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
										  vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

		initialTransitionDepth2DImage(vk, *primaryCmdBuffer, m_depthImage->object(), vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
									  vk::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, vk::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | vk::VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

		if (m_parametersGraphic.resetType != RESET_TYPE_HOST)
			vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);

		beginRenderPass(vk, *primaryCmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, WIDTH, HEIGHT), (deUint32)renderPassClearValues.size(), &renderPassClearValues[0], VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
		for (deUint32 i = 0; i < queryCount; ++i)
			vk.cmdExecuteCommands(*primaryCmdBuffer, 1u, &(secondaryCmdBuffers[i]->get()));
		endRenderPass(vk, *primaryCmdBuffer);

		if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY || m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY || m_parametersGraphic.copyType == COPY_TYPE_CMD)
		{
			VkDeviceSize stride = m_parametersGraphic.querySizeFlags() ? sizeof(deUint64) : sizeof(deUint32);
			vk::VkQueryResultFlags flags = m_parametersGraphic.querySizeFlags() | VK_QUERY_RESULT_WAIT_BIT;

			if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
			{
				flags |= VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
				stride *= 2u;
			}

			if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY)
			{
				vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);
				flags = VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
				stride = sizeof(ValueAndAvailability);
			}

			VkDeviceSize dstOffsetQuery = (m_parametersGraphic.dstOffset) ? stride : 0;
			vk.cmdCopyQueryPoolResults(*primaryCmdBuffer, *queryPool, 0, queryCount, m_resetBuffer->object(), dstOffsetQuery, stride, flags);

			if (m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY)
				vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);

			const VkBufferMemoryBarrier barrier =
			{
				VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	//  VkStructureType	sType;
				DE_NULL,									//  const void*		pNext;
				VK_ACCESS_TRANSFER_WRITE_BIT,				//  VkAccessFlags	srcAccessMask;
				VK_ACCESS_HOST_READ_BIT,					//  VkAccessFlags	dstAccessMask;
				VK_QUEUE_FAMILY_IGNORED,					//  deUint32		srcQueueFamilyIndex;
				VK_QUEUE_FAMILY_IGNORED,					//  deUint32		destQueueFamilyIndex;
				m_resetBuffer->object(),					//  VkBuffer		buffer;
				0u,											//  VkDeviceSize	offset;
				queryCount * stride + dstOffsetQuery,		//  VkDeviceSize	size;
			};
			vk.cmdPipelineBarrier(*primaryCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1u, &barrier, 0, (const VkImageMemoryBarrier*)DE_NULL);
		}

		if (!m_parametersGraphic.noColorAttachments)
			transition2DImage(vk, *primaryCmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
							  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	}
	endCommandBuffer(vk, *primaryCmdBuffer);

	if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
		vk.resetQueryPool(device, *queryPool, 0u, queryCount);

	// Wait for completion
	submitCommandsAndWait(vk, device, queue, *primaryCmdBuffer);
	return checkResult (*queryPool);
}

class VertexShaderSecondaryInheritedTestInstance : public VertexShaderTestInstance
{
public:
							VertexShaderSecondaryInheritedTestInstance	(vkt::Context& context, const std::vector<VertexData>& data, const ParametersGraphic& parametersGraphic, const std::vector<deUint64>& drawRepeats);
protected:
	virtual void			checkExtensions						(deBool hostQueryResetEnabled);
	virtual tcu::TestStatus	executeTest							(void);
};

VertexShaderSecondaryInheritedTestInstance::VertexShaderSecondaryInheritedTestInstance (vkt::Context& context, const std::vector<VertexData>& data, const ParametersGraphic& parametersGraphic, const std::vector<deUint64>& drawRepeats)
	: VertexShaderTestInstance	(context, data, parametersGraphic, drawRepeats)
{
}

void VertexShaderSecondaryInheritedTestInstance::checkExtensions (deBool hostQueryResetEnabled)
{
	StatisticQueryTestInstance::checkExtensions(hostQueryResetEnabled);
	if (!m_context.getDeviceFeatures().inheritedQueries)
		throw tcu::NotSupportedError("Inherited queries are not supported");
}

tcu::TestStatus VertexShaderSecondaryInheritedTestInstance::executeTest (void)
{
	const DeviceInterface&					vk						= m_context.getDeviceInterface();
	const VkDevice							device					= m_context.getDevice();
	const VkQueue							queue					= m_context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();

	const CmdPoolCreateInfo					cmdPoolCreateInfo		(queueFamilyIndex);
	const Move<VkCommandPool>				cmdPool					= createCommandPool(vk, device, &cmdPoolCreateInfo);
	const deUint32							queryCount				= static_cast<deUint32>(m_drawRepeats.size());
	const Unique<VkQueryPool>				queryPool				(makeQueryPool(vk, device, queryCount, m_parametersGraphic.queryStatisticFlags));

	const VkDeviceSize						vertexBufferOffset		= 0u;
	const BufferPtr							vertexBufferSp			= creatAndFillVertexBuffer();
	const VkBuffer							vertexBuffer			= vertexBufferSp->object();

	const Unique<VkCommandBuffer>			primaryCmdBuffer		(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	std::vector<VkCommandBufferSp>			secondaryCmdBuffers(queryCount);

	for (deUint32 i = 0; i < queryCount; ++i)
		secondaryCmdBuffers[i] = VkCommandBufferSp(new vk::Unique<VkCommandBuffer>(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY)));

	for (deUint32 i = 0; i < queryCount; ++i)
	{
		beginSecondaryCommandBuffer(vk, secondaryCmdBuffers[i]->get(), m_parametersGraphic.queryStatisticFlags, *m_renderPass, *m_framebuffer, VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);
		vk.cmdBindPipeline(secondaryCmdBuffers[i]->get(), VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		vk.cmdBindVertexBuffers(secondaryCmdBuffers[i]->get(), 0u, 1u, &vertexBuffer, &vertexBufferOffset);
		for (deUint32 j = 0; j<m_drawRepeats[i]; ++j)
			draw(secondaryCmdBuffers[i]->get());
		endCommandBuffer(vk, secondaryCmdBuffers[i]->get());
	}

	beginCommandBuffer(vk, *primaryCmdBuffer);
	{
		std::vector<VkClearValue>	renderPassClearValues	(2);
		deMemset(&renderPassClearValues[0], 0, static_cast<int>(renderPassClearValues.size()) * sizeof(VkClearValue));

		if (!m_parametersGraphic.noColorAttachments)
			initialTransitionColor2DImage(vk, *primaryCmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
										  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
		initialTransitionDepth2DImage(vk, *primaryCmdBuffer, m_depthImage->object(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
									  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

		if (m_parametersGraphic.resetType != RESET_TYPE_HOST)
			vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);

		for (deUint32 i = 0; i < queryCount; ++i)
		{
			vk.cmdBeginQuery(*primaryCmdBuffer, *queryPool, i, (VkQueryControlFlags)0u);
			beginRenderPass(vk, *primaryCmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, WIDTH, HEIGHT), (deUint32)renderPassClearValues.size(), &renderPassClearValues[0], VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
			vk.cmdExecuteCommands(*primaryCmdBuffer, 1u, &(secondaryCmdBuffers[i]->get()));
			endRenderPass(vk, *primaryCmdBuffer);
			vk.cmdEndQuery(*primaryCmdBuffer, *queryPool, i);
		}

		if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY || m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY || m_parametersGraphic.copyType == COPY_TYPE_CMD)
		{
			VkDeviceSize stride = m_parametersGraphic.querySizeFlags() ? sizeof(deUint64) : sizeof(deUint32);
			vk::VkQueryResultFlags flags = m_parametersGraphic.querySizeFlags() | VK_QUERY_RESULT_WAIT_BIT;

			if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
			{
				flags |= VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
				stride *= 2u;
			}

			if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY)
			{
				vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);
				flags = VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
				stride = sizeof(ValueAndAvailability);
			}

			VkDeviceSize dstOffsetQuery = (m_parametersGraphic.dstOffset) ? stride : 0;
			vk.cmdCopyQueryPoolResults(*primaryCmdBuffer, *queryPool, 0, queryCount, m_resetBuffer->object(), dstOffsetQuery, stride, flags);

			if (m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY)
				vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);

			const VkBufferMemoryBarrier barrier =
			{
				VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	//  VkStructureType	sType;
				DE_NULL,									//  const void*		pNext;
				VK_ACCESS_TRANSFER_WRITE_BIT,				//  VkAccessFlags	srcAccessMask;
				VK_ACCESS_HOST_READ_BIT,					//  VkAccessFlags	dstAccessMask;
				VK_QUEUE_FAMILY_IGNORED,					//  deUint32		srcQueueFamilyIndex;
				VK_QUEUE_FAMILY_IGNORED,					//  deUint32		destQueueFamilyIndex;
				m_resetBuffer->object(),					//  VkBuffer		buffer;
				0u,											//  VkDeviceSize	offset;
			    queryCount * stride + dstOffsetQuery,		//  VkDeviceSize	size;
			};
			vk.cmdPipelineBarrier(*primaryCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1u, &barrier, 0, (const VkImageMemoryBarrier*)DE_NULL);
		}

		if (!m_parametersGraphic.noColorAttachments)
			transition2DImage(vk, *primaryCmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
							  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	}
	endCommandBuffer(vk, *primaryCmdBuffer);

	if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
		vk.resetQueryPool(device, *queryPool, 0u, queryCount);

	// Wait for completion
	submitCommandsAndWait(vk, device, queue, *primaryCmdBuffer);
	return checkResult (*queryPool);
}

class GeometryShaderTestInstance : public GraphicBasicTestInstance
{
public:
							GeometryShaderTestInstance	(vkt::Context& context, const std::vector<VertexData>& data, const ParametersGraphic& parametersGraphic, const std::vector<deUint64>& drawRepeats);
protected:
	virtual void			checkExtensions				(deBool hostQueryResetEnabled);
	virtual void			createPipeline				(void);
	virtual tcu::TestStatus	executeTest					(void);
	tcu::TestStatus			checkResult					(VkQueryPool queryPool);
	void					draw						(VkCommandBuffer cmdBuffer);
};

GeometryShaderTestInstance::GeometryShaderTestInstance (vkt::Context& context, const std::vector<VertexData>& data, const ParametersGraphic& parametersGraphic, const std::vector<deUint64>& drawRepeats)
	: GraphicBasicTestInstance(context, data, parametersGraphic, drawRepeats)
{
}

void GeometryShaderTestInstance::checkExtensions (deBool hostQueryResetEnabled)
{
	StatisticQueryTestInstance::checkExtensions(hostQueryResetEnabled);
	if (!m_context.getDeviceFeatures().geometryShader)
		throw tcu::NotSupportedError("Geometry shader are not supported");
}

void GeometryShaderTestInstance::createPipeline (void)
{
	const DeviceInterface&	vk						= m_context.getDeviceInterface();
	const VkDevice			device					= m_context.getDevice();
	const VkBool32			useGeomPointSize		= m_context.getDeviceFeatures().shaderTessellationAndGeometryPointSize && (m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST);

	// Pipeline
	Unique<VkShaderModule> vs(createShaderModule(vk, device, m_context.getBinaryCollection().get("vertex"), (VkShaderModuleCreateFlags)0));
	Unique<VkShaderModule> gs(createShaderModule(vk, device, m_context.getBinaryCollection().get(useGeomPointSize ? "geometry_point_size" : "geometry"), (VkShaderModuleCreateFlags)0));
	Unique<VkShaderModule> fs(createShaderModule(vk, device, m_context.getBinaryCollection().get("fragment"), (VkShaderModuleCreateFlags)0));

	const PipelineCreateInfo::ColorBlendState::Attachment attachmentState;

	const PipelineLayoutCreateInfo pipelineLayoutCreateInfo;
	m_pipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);

	const VkVertexInputBindingDescription vertexInputBindingDescription		=
	{
		0u,											// binding;
		static_cast<deUint32>(sizeof(VertexData)),	// stride;
		VK_VERTEX_INPUT_RATE_VERTEX					// inputRate
	};

	const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[] =
	{
		{
			0u,
			0u,
			VK_FORMAT_R32G32B32A32_SFLOAT,
			0u
		},	// VertexElementData::position
		{
			1u,
			0u,
			VK_FORMAT_R32G32B32A32_SFLOAT,
			static_cast<deUint32>(sizeof(tcu::Vec4))
		},	// VertexElementData::color
	};

	const VkPipelineVertexInputStateCreateInfo vf_info			=
	{																	// sType;
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// pNext;
		NULL,															// flags;
		0u,																// vertexBindingDescriptionCount;
		1,																// pVertexBindingDescriptions;
		&vertexInputBindingDescription,									// vertexAttributeDescriptionCount;
		2,																// pVertexAttributeDescriptions;
		vertexInputAttributeDescriptions
	};

	PipelineCreateInfo pipelineCreateInfo(*m_pipelineLayout, *m_renderPass, 0, (VkPipelineCreateFlags)0);
	pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*vs, "main", VK_SHADER_STAGE_VERTEX_BIT));
	pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*gs, "main", VK_SHADER_STAGE_GEOMETRY_BIT));
	pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*fs, "main", VK_SHADER_STAGE_FRAGMENT_BIT));
	pipelineCreateInfo.addState(PipelineCreateInfo::InputAssemblerState(m_parametersGraphic.primitiveTopology));
	pipelineCreateInfo.addState(PipelineCreateInfo::ColorBlendState(1, &attachmentState));

	const VkViewport	viewport	= makeViewport(WIDTH, HEIGHT);
	const VkRect2D		scissor		= makeRect2D(WIDTH, HEIGHT);

	pipelineCreateInfo.addState(PipelineCreateInfo::ViewportState(1, std::vector<VkViewport>(1, viewport), std::vector<VkRect2D>(1, scissor)));

	if (m_context.getDeviceFeatures().depthBounds)
		pipelineCreateInfo.addState(PipelineCreateInfo::DepthStencilState(true, true, VK_COMPARE_OP_GREATER_OR_EQUAL, true));
	else
		pipelineCreateInfo.addState(PipelineCreateInfo::DepthStencilState());

	pipelineCreateInfo.addState(PipelineCreateInfo::RasterizerState(false));
	pipelineCreateInfo.addState(PipelineCreateInfo::MultiSampleState());
	pipelineCreateInfo.addState(vf_info);
	m_pipeline = createGraphicsPipeline(vk, device, DE_NULL, &pipelineCreateInfo);
}

tcu::TestStatus GeometryShaderTestInstance::executeTest (void)
{
	const DeviceInterface&					vk						= m_context.getDeviceInterface();
	const VkDevice							device					= m_context.getDevice();
	const VkQueue							queue					= m_context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();

	const CmdPoolCreateInfo					cmdPoolCreateInfo		(queueFamilyIndex);
	const Move<VkCommandPool>				cmdPool					= createCommandPool(vk, device, &cmdPoolCreateInfo);
	const deUint32							queryCount				= static_cast<deUint32>(m_drawRepeats.size());
	const Unique<VkQueryPool>				queryPool				(makeQueryPool(vk, device, queryCount, m_parametersGraphic.queryStatisticFlags));

	const VkDeviceSize						vertexBufferOffset		= 0u;
	const BufferPtr							vertexBufferSp			= creatAndFillVertexBuffer();
	const VkBuffer							vertexBuffer			= vertexBufferSp->object();

	const Unique<VkCommandBuffer>			cmdBuffer				(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	beginCommandBuffer(vk, *cmdBuffer);
	{
		std::vector<VkClearValue>	renderPassClearValues	(2);
		deMemset(&renderPassClearValues[0], 0, static_cast<int>(renderPassClearValues.size()) * sizeof(VkClearValue));

		if (!m_parametersGraphic.noColorAttachments)
			initialTransitionColor2DImage(vk, *cmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
										  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

		initialTransitionDepth2DImage(vk, *cmdBuffer, m_depthImage->object(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
									  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

		if (m_parametersGraphic.resetType != RESET_TYPE_HOST)
			vk.cmdResetQueryPool(*cmdBuffer, *queryPool, 0u, queryCount);

		beginRenderPass(vk, *cmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, WIDTH, HEIGHT), (deUint32)renderPassClearValues.size(), &renderPassClearValues[0]);

		for (deUint32 i = 0; i < queryCount; ++i)
		{
			vk.cmdBeginQuery(*cmdBuffer, *queryPool, i, (VkQueryControlFlags)0u);
			vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);

			for (deUint64 j = 0; j<m_drawRepeats[i]; ++j)
				draw(*cmdBuffer);

			vk.cmdEndQuery(*cmdBuffer, *queryPool, i);
		}

		endRenderPass(vk, *cmdBuffer);

		if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY || m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY || m_parametersGraphic.copyType == COPY_TYPE_CMD)
		{
			VkDeviceSize stride = m_parametersGraphic.querySizeFlags() ? sizeof(deUint64) : sizeof(deUint32);
			vk::VkQueryResultFlags flags = m_parametersGraphic.querySizeFlags() | VK_QUERY_RESULT_WAIT_BIT;

			if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
			{
				flags |= VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
				stride *= 2u;
			}

			if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY)
			{
				vk.cmdResetQueryPool(*cmdBuffer, *queryPool, 0u, queryCount);
				flags = VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
				stride = sizeof(ValueAndAvailability);
			}

			VkDeviceSize dstOffsetQuery = (m_parametersGraphic.dstOffset) ? stride : 0;
			vk.cmdCopyQueryPoolResults(*cmdBuffer, *queryPool, 0, queryCount, m_resetBuffer->object(), dstOffsetQuery, stride, flags);

			if (m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY)
				vk.cmdResetQueryPool(*cmdBuffer, *queryPool, 0u, queryCount);

			const VkBufferMemoryBarrier barrier =
			{
				VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	//  VkStructureType	sType;
				DE_NULL,									//  const void*		pNext;
				VK_ACCESS_TRANSFER_WRITE_BIT,				//  VkAccessFlags	srcAccessMask;
				VK_ACCESS_HOST_READ_BIT,					//  VkAccessFlags	dstAccessMask;
				VK_QUEUE_FAMILY_IGNORED,					//  deUint32		srcQueueFamilyIndex;
				VK_QUEUE_FAMILY_IGNORED,					//  deUint32		destQueueFamilyIndex;
				m_resetBuffer->object(),					//  VkBuffer		buffer;
				0u,											//  VkDeviceSize	offset;
				queryCount * stride + dstOffsetQuery,		//  VkDeviceSize	size;
			};
			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1u, &barrier, 0, (const VkImageMemoryBarrier*)DE_NULL);
		}

		if (!m_parametersGraphic.noColorAttachments)
			transition2DImage(vk, *cmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
							  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	}
	endCommandBuffer(vk, *cmdBuffer);

	if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
		vk.resetQueryPool(device, *queryPool, 0u, queryCount);

	// Wait for completion
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);
	return checkResult(*queryPool);
}

tcu::TestStatus GeometryShaderTestInstance::checkResult (VkQueryPool queryPool)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			device		= m_context.getDevice();
	deUint64				expectedMin	= 0u;

	switch(m_parametersGraphic.queryStatisticFlags)
	{
		case VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT:
			expectedMin =	m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST						? 16u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST						? 8u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP						? 15u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST					? 4u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP					? 4u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN						? 14u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY			? 4u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY		? 13u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY		? 2u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY	? 6u :
							0u;
			break;
		case VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT:
		case VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT:
		case VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT:
			expectedMin =	m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST						? 112u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST						? 32u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP						? 60u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST					? 8u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP					? 8u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN						? 28u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY			? 16u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY		? 52u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY		? 4u :
							m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY	? 12u :
							0u;
			break;
		default:
			DE_FATAL("Unexpected type of statistics query");
			break;
	}

	const deUint32 queryCount = static_cast<deUint32>(m_drawRepeats.size());

	if (m_parametersGraphic.resetType == RESET_TYPE_NORMAL || m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY)
	{
		ResultsVector results(queryCount, 0u);

		if (m_parametersGraphic.copyType == COPY_TYPE_CMD)
		{
			const vk::Allocation& allocation = m_resetBuffer->getBoundMemory();
			cmdCopyQueryPoolResultsVector(results, vk, device, allocation, queryCount, (VK_QUERY_RESULT_WAIT_BIT | m_parametersGraphic.querySizeFlags()), m_parametersGraphic.dstOffset);
		}
		else
		{
			VK_CHECK(GetQueryPoolResultsVector(results, vk, device, queryPool, 0u, queryCount, (VK_QUERY_RESULT_WAIT_BIT | m_parametersGraphic.querySizeFlags())));
		}

		if (results[0] < expectedMin)
			return tcu::TestStatus::fail("QueryPoolResults incorrect");
		if (queryCount > 1)
		{
			double pearson = calculatePearsonCorrelation(m_drawRepeats, results);
			if ( fabs( pearson ) < 0.8 )
				return tcu::TestStatus::fail("QueryPoolResults are nonlinear");
		}
	}
	else if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
	{
		ResultsVectorWithAvailability results(queryCount, pair<deUint64, deUint64>(0u, 0u));
		if (m_parametersGraphic.copyType == COPY_TYPE_CMD)
		{
			const vk::Allocation& allocation = m_resetBuffer->getBoundMemory();
			cmdCopyQueryPoolResultsVector(results, vk, device, allocation, queryCount, (VK_QUERY_RESULT_WAIT_BIT | m_parametersGraphic.querySizeFlags() | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT), m_parametersGraphic.dstOffset);
		}
		else
		{
			VK_CHECK(GetQueryPoolResultsVector(results, vk, device, queryPool, 0u, queryCount, (VK_QUERY_RESULT_WAIT_BIT | m_parametersGraphic.querySizeFlags() | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT)));
		}

		if (results[0].first < expectedMin || results[0].second == 0u)
			return tcu::TestStatus::fail("QueryPoolResults incorrect");

		if (queryCount > 1)
		{
			double pearson = calculatePearsonCorrelation(m_drawRepeats, results);
			if ( fabs( pearson ) < 0.8 )
				return tcu::TestStatus::fail("QueryPoolResults are nonlinear");
		}

		deUint64 temp = results[0].first;

		vk.resetQueryPool(device, queryPool, 0, queryCount);
		vk::VkResult res = GetQueryPoolResultsVector(results, vk, device, queryPool, 0u, queryCount, (m_parametersGraphic.querySizeFlags() | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT));
		/* From Vulkan spec:
		 *
		 * If VK_QUERY_RESULT_WAIT_BIT and VK_QUERY_RESULT_PARTIAL_BIT are both not set then no result values are written to pData
		 * for queries that are in the unavailable state at the time of the call, and vkGetQueryPoolResults returns VK_NOT_READY.
		 * However, availability state is still written to pData for those queries if VK_QUERY_RESULT_WITH_AVAILABILITY_BIT is set.
		 */
		if (res != vk::VK_NOT_READY || results[0].first != temp || results[0].second != 0u)
			return tcu::TestStatus::fail("QueryPoolResults incorrect reset");
	}
	else
	{
		// With RESET_TYPE_BEFORE_COPY, we only need to verify the result after the copy include an availability bit set as zero.
		return verifyUnavailable();
	}

	if ( (m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST || m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP ) && !checkImage())
		return tcu::TestStatus::fail("Result image doesn't match expected image.");

	return tcu::TestStatus::pass("Pass");
}

void GeometryShaderTestInstance::draw (VkCommandBuffer cmdBuffer)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	if (m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP ||
		m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
	{
		vk.cmdDraw(cmdBuffer, 3u, 1u,  0u,  1u);
		vk.cmdDraw(cmdBuffer, 3u, 1u,  4u,  1u);
		vk.cmdDraw(cmdBuffer, 3u, 1u,  8u,  2u);
		vk.cmdDraw(cmdBuffer, 3u, 1u, 12u,  3u);
	}
	else
	{
		vk.cmdDraw(cmdBuffer, 16u, 1u, 0u,  0u);
	}
}

class GeometryShaderSecondaryTestInstance : public GeometryShaderTestInstance
{
public:
							GeometryShaderSecondaryTestInstance	(vkt::Context& context, const std::vector<VertexData>& data, const ParametersGraphic& parametersGraphic, const std::vector<deUint64>& drawRepeats);
protected:
	virtual tcu::TestStatus	executeTest							(void);
};

GeometryShaderSecondaryTestInstance::GeometryShaderSecondaryTestInstance (vkt::Context& context, const std::vector<VertexData>& data, const ParametersGraphic& parametersGraphic, const std::vector<deUint64>& drawRepeats)
	: GeometryShaderTestInstance	(context, data, parametersGraphic, drawRepeats)
{
}

tcu::TestStatus GeometryShaderSecondaryTestInstance::executeTest (void)
{
	const DeviceInterface&					vk						= m_context.getDeviceInterface();
	const VkDevice							device					= m_context.getDevice();
	const VkQueue							queue					= m_context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();

	const CmdPoolCreateInfo					cmdPoolCreateInfo		(queueFamilyIndex);
	const Move<VkCommandPool>				cmdPool					= createCommandPool(vk, device, &cmdPoolCreateInfo);
	const deUint32							queryCount				= static_cast<deUint32>(m_drawRepeats.size());
	const Unique<VkQueryPool>				queryPool				(makeQueryPool(vk, device, queryCount, m_parametersGraphic.queryStatisticFlags));

	const VkDeviceSize						vertexBufferOffset		= 0;
	const BufferPtr							vertexBufferSp			= creatAndFillVertexBuffer();
	const VkBuffer							vertexBuffer			= vertexBufferSp->object();

	const Unique<VkCommandBuffer>			primaryCmdBuffer		(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	std::vector<VkCommandBufferSp>			secondaryCmdBuffers(queryCount);

	for (deUint32 i = 0; i < queryCount; ++i)
		secondaryCmdBuffers[i] = VkCommandBufferSp(new vk::Unique<VkCommandBuffer>(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY)));

	for (deUint32 i = 0; i < queryCount; ++i)
	{
		beginSecondaryCommandBuffer(vk, secondaryCmdBuffers[i]->get(), m_parametersGraphic.queryStatisticFlags, *m_renderPass, *m_framebuffer, VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);
		vk.cmdBeginQuery(secondaryCmdBuffers[i]->get(), *queryPool, i, (VkQueryControlFlags)0u);
		vk.cmdBindPipeline(secondaryCmdBuffers[i]->get(), VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		vk.cmdBindVertexBuffers(secondaryCmdBuffers[i]->get(), 0u, 1u, &vertexBuffer, &vertexBufferOffset);
		for (deUint32 j = 0; j<m_drawRepeats[i]; ++j)
			draw(secondaryCmdBuffers[i]->get());
		vk.cmdEndQuery(secondaryCmdBuffers[i]->get(), *queryPool, i);
		endCommandBuffer(vk, secondaryCmdBuffers[i]->get());
	}

	beginCommandBuffer(vk, *primaryCmdBuffer);
	{
		std::vector<VkClearValue>	renderPassClearValues	(2);
		deMemset(&renderPassClearValues[0], 0, static_cast<int>(renderPassClearValues.size()) * sizeof(VkClearValue));

		if (!m_parametersGraphic.noColorAttachments)
			initialTransitionColor2DImage(vk, *primaryCmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
										  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

		initialTransitionDepth2DImage(vk, *primaryCmdBuffer, m_depthImage->object(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
									  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

		if (m_parametersGraphic.resetType != RESET_TYPE_HOST)
			vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);
		beginRenderPass(vk, *primaryCmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, WIDTH, HEIGHT), (deUint32)renderPassClearValues.size(), &renderPassClearValues[0], VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
		for (deUint32 i = 0; i < queryCount; ++i)
			vk.cmdExecuteCommands(*primaryCmdBuffer, 1u, &(secondaryCmdBuffers[i]->get()));
		endRenderPass(vk, *primaryCmdBuffer);

		if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY || m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY || m_parametersGraphic.copyType == COPY_TYPE_CMD)
		{
			VkDeviceSize stride = m_parametersGraphic.querySizeFlags() ? sizeof(deUint64) : sizeof(deUint32);
			vk::VkQueryResultFlags flags = m_parametersGraphic.querySizeFlags() | VK_QUERY_RESULT_WAIT_BIT;

			if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
			{
				flags |= VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
				stride *= 2u;
			}

			if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY)
			{
				vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);
				flags = VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
				stride = sizeof(ValueAndAvailability);
			}

			VkDeviceSize dstOffsetQuery = (m_parametersGraphic.dstOffset) ? stride : 0;
			vk.cmdCopyQueryPoolResults(*primaryCmdBuffer, *queryPool, 0, queryCount, m_resetBuffer->object(), dstOffsetQuery, stride, flags);

			if (m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY)
				vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);

			const VkBufferMemoryBarrier barrier =
			{
				VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	//  VkStructureType	sType;
				DE_NULL,									//  const void*		pNext;
				VK_ACCESS_TRANSFER_WRITE_BIT,				//  VkAccessFlags	srcAccessMask;
				VK_ACCESS_HOST_READ_BIT,					//  VkAccessFlags	dstAccessMask;
				VK_QUEUE_FAMILY_IGNORED,					//  deUint32		srcQueueFamilyIndex;
				VK_QUEUE_FAMILY_IGNORED,					//  deUint32		destQueueFamilyIndex;
				m_resetBuffer->object(),					//  VkBuffer		buffer;
				0u,											//  VkDeviceSize	offset;
				queryCount * stride + dstOffsetQuery,		//  VkDeviceSize	size;
			};
			vk.cmdPipelineBarrier(*primaryCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1u, &barrier, 0, (const VkImageMemoryBarrier*)DE_NULL);
		}

		if (!m_parametersGraphic.noColorAttachments)
			transition2DImage(vk, *primaryCmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
							  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	}
	endCommandBuffer(vk, *primaryCmdBuffer);

	if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
		vk.resetQueryPool(device, *queryPool, 0u, queryCount);

	// Wait for completion
	submitCommandsAndWait(vk, device, queue, *primaryCmdBuffer);
	return checkResult(*queryPool);
}

class GeometryShaderSecondaryInheritedTestInstance : public GeometryShaderTestInstance
{
public:
							GeometryShaderSecondaryInheritedTestInstance	(vkt::Context& context, const std::vector<VertexData>& data, const ParametersGraphic& parametersGraphic, const std::vector<deUint64>& drawRepeats);
protected:
	virtual void			checkExtensions						(deBool hostQueryResetEnabled);
	virtual tcu::TestStatus	executeTest							(void);
};

GeometryShaderSecondaryInheritedTestInstance::GeometryShaderSecondaryInheritedTestInstance (vkt::Context& context, const std::vector<VertexData>& data, const ParametersGraphic& parametersGraphic, const std::vector<deUint64>& drawRepeats)
	: GeometryShaderTestInstance	(context, data, parametersGraphic, drawRepeats)
{
}

void GeometryShaderSecondaryInheritedTestInstance::checkExtensions (deBool hostQueryResetEnabled)
{
	GeometryShaderTestInstance::checkExtensions(hostQueryResetEnabled);
	if (!m_context.getDeviceFeatures().inheritedQueries)
		throw tcu::NotSupportedError("Inherited queries are not supported");
}

tcu::TestStatus GeometryShaderSecondaryInheritedTestInstance::executeTest (void)
{
	const DeviceInterface&					vk						= m_context.getDeviceInterface();
	const VkDevice							device					= m_context.getDevice();
	const VkQueue							queue					= m_context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();

	const CmdPoolCreateInfo					cmdPoolCreateInfo		(queueFamilyIndex);
	const Move<VkCommandPool>				cmdPool					= createCommandPool(vk, device, &cmdPoolCreateInfo);
	const deUint32							queryCount				= static_cast<deUint32>(m_drawRepeats.size());
	const Unique<VkQueryPool>				queryPool				(makeQueryPool(vk, device, queryCount, m_parametersGraphic.queryStatisticFlags));

	const VkDeviceSize						vertexBufferOffset		= 0u;
	const BufferPtr							vertexBufferSp			= creatAndFillVertexBuffer();
	const VkBuffer							vertexBuffer			= vertexBufferSp->object();

	const Unique<VkCommandBuffer>			primaryCmdBuffer		(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	std::vector<VkCommandBufferSp>			secondaryCmdBuffers(queryCount);

	for (deUint32 i = 0; i < queryCount; ++i)
		secondaryCmdBuffers[i] = VkCommandBufferSp(new vk::Unique<VkCommandBuffer>(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY)));

	for (deUint32 i = 0; i < queryCount; ++i)
	{
		beginSecondaryCommandBuffer(vk, secondaryCmdBuffers[i]->get(), m_parametersGraphic.queryStatisticFlags, *m_renderPass, *m_framebuffer, VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);
		vk.cmdBindPipeline(secondaryCmdBuffers[i]->get(), VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		vk.cmdBindVertexBuffers(secondaryCmdBuffers[i]->get(), 0u, 1u, &vertexBuffer, &vertexBufferOffset);
		for (deUint32 j = 0; j<m_drawRepeats[i]; ++j)
			draw(secondaryCmdBuffers[i]->get());
		endCommandBuffer(vk, secondaryCmdBuffers[i]->get());
	}

	beginCommandBuffer(vk, *primaryCmdBuffer);
	{
		std::vector<VkClearValue>	renderPassClearValues	(2);
		deMemset(&renderPassClearValues[0], 0, static_cast<int>(renderPassClearValues.size()) * sizeof(VkClearValue));

		if (!m_parametersGraphic.noColorAttachments)
			initialTransitionColor2DImage(vk, *primaryCmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
										  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

		initialTransitionDepth2DImage(vk, *primaryCmdBuffer, m_depthImage->object(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
									  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

		if (m_parametersGraphic.resetType != RESET_TYPE_HOST)
			vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);

		for (deUint32 i = 0; i < queryCount; ++i)
		{
			vk.cmdBeginQuery(*primaryCmdBuffer, *queryPool, i, (VkQueryControlFlags)0u);
			beginRenderPass(vk, *primaryCmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, WIDTH, HEIGHT), (deUint32)renderPassClearValues.size(), &renderPassClearValues[0], VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
			vk.cmdExecuteCommands(*primaryCmdBuffer, 1u, &(secondaryCmdBuffers[i]->get()));
			endRenderPass(vk, *primaryCmdBuffer);
			vk.cmdEndQuery(*primaryCmdBuffer, *queryPool, i);
		}

		if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY || m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY || m_parametersGraphic.copyType == COPY_TYPE_CMD)
		{
			VkDeviceSize stride = m_parametersGraphic.querySizeFlags() ? sizeof(deUint64) : sizeof(deUint32);
			vk::VkQueryResultFlags flags = m_parametersGraphic.querySizeFlags() | VK_QUERY_RESULT_WAIT_BIT;

			if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
			{
				flags |= VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
				stride *= 2u;
			}

			if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY)
			{
				vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);
				flags = VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
				stride = sizeof(ValueAndAvailability);
			}

			VkDeviceSize dstOffsetQuery = (m_parametersGraphic.dstOffset) ? stride : 0;
			vk.cmdCopyQueryPoolResults(*primaryCmdBuffer, *queryPool, 0, queryCount, m_resetBuffer->object(), dstOffsetQuery, stride, flags);

			if (m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY)
				vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);

			const VkBufferMemoryBarrier barrier =
			{
				VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	//  VkStructureType	sType;
				DE_NULL,									//  const void*		pNext;
				VK_ACCESS_TRANSFER_WRITE_BIT,				//  VkAccessFlags	srcAccessMask;
				VK_ACCESS_HOST_READ_BIT,					//  VkAccessFlags	dstAccessMask;
				VK_QUEUE_FAMILY_IGNORED,					//  deUint32		srcQueueFamilyIndex;
				VK_QUEUE_FAMILY_IGNORED,					//  deUint32		destQueueFamilyIndex;
				m_resetBuffer->object(),					//  VkBuffer		buffer;
				0u,											//  VkDeviceSize	offset;
			    queryCount * stride + dstOffsetQuery,		//  VkDeviceSize	size;
			};
			vk.cmdPipelineBarrier(*primaryCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1u, &barrier, 0, (const VkImageMemoryBarrier*)DE_NULL);
		}

		if (!m_parametersGraphic.noColorAttachments)
			transition2DImage(vk, *primaryCmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
							  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	}
	endCommandBuffer(vk, *primaryCmdBuffer);

	if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
		vk.resetQueryPool(device, *queryPool, 0u, queryCount);

	// Wait for completion
	submitCommandsAndWait(vk, device, queue, *primaryCmdBuffer);
	return checkResult(*queryPool);
}

class TessellationShaderTestInstance : public GraphicBasicTestInstance
{
public:
							TessellationShaderTestInstance	(vkt::Context& context, const std::vector<VertexData>& data, const ParametersGraphic& parametersGraphic, const std::vector<deUint64>& drawRepeats);
protected:
	virtual	void			checkExtensions				(deBool hostQueryResetEnabled);
	virtual void			createPipeline				(void);
	virtual tcu::TestStatus	executeTest					(void);
	virtual tcu::TestStatus	checkResult					(VkQueryPool queryPool);
	void					draw						(VkCommandBuffer cmdBuffer);
};

TessellationShaderTestInstance::TessellationShaderTestInstance	(vkt::Context& context, const std::vector<VertexData>& data, const ParametersGraphic& parametersGraphic, const std::vector<deUint64>& drawRepeats)
	: GraphicBasicTestInstance	(context, data, parametersGraphic, drawRepeats)
{
}

void TessellationShaderTestInstance::checkExtensions (deBool hostQueryResetEnabled)
{
	StatisticQueryTestInstance::checkExtensions(hostQueryResetEnabled);
	if (!m_context.getDeviceFeatures().tessellationShader)
		throw tcu::NotSupportedError("Tessellation shader are not supported");
}


void TessellationShaderTestInstance::createPipeline (void)
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDevice			device	= m_context.getDevice();

	// Pipeline
	Unique<VkShaderModule> vs(createShaderModule(vk, device, m_context.getBinaryCollection().get("vertex"), (VkShaderModuleCreateFlags)0));
	Unique<VkShaderModule> tc(createShaderModule(vk, device, m_context.getBinaryCollection().get("tessellation_control"), (VkShaderModuleCreateFlags)0));
	Unique<VkShaderModule> te(createShaderModule(vk, device, m_context.getBinaryCollection().get("tessellation_evaluation"), (VkShaderModuleCreateFlags)0));
	Unique<VkShaderModule> fs(createShaderModule(vk, device, m_context.getBinaryCollection().get("fragment"), (VkShaderModuleCreateFlags)0));

	const PipelineCreateInfo::ColorBlendState::Attachment attachmentState;

	const PipelineLayoutCreateInfo pipelineLayoutCreateInfo;
	m_pipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);

	const VkVertexInputBindingDescription vertexInputBindingDescription		=
	{
		0u,											// binding;
		static_cast<deUint32>(sizeof(VertexData)),	// stride;
		VK_VERTEX_INPUT_RATE_VERTEX					// inputRate
	};

	const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[] =
	{
		{
			0u,
			0u,
			VK_FORMAT_R32G32B32A32_SFLOAT,
			0u
		},	// VertexElementData::position
		{
			1u,
			0u,
			VK_FORMAT_R32G32B32A32_SFLOAT,
			static_cast<deUint32>(sizeof(tcu::Vec4))
		},	// VertexElementData::color
	};

	const VkPipelineVertexInputStateCreateInfo vf_info			=
	{																	// sType;
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// pNext;
		NULL,															// flags;
		0u,																// vertexBindingDescriptionCount;
		1u,																// pVertexBindingDescriptions;
		&vertexInputBindingDescription,									// vertexAttributeDescriptionCount;
		2u,																// pVertexAttributeDescriptions;
		vertexInputAttributeDescriptions
	};

	PipelineCreateInfo pipelineCreateInfo(*m_pipelineLayout, *m_renderPass, 0, (VkPipelineCreateFlags)0);
	pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*vs, "main", VK_SHADER_STAGE_VERTEX_BIT));
	pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*tc, "main", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT));
	pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*te, "main", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT));
	pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*fs, "main", VK_SHADER_STAGE_FRAGMENT_BIT));
	pipelineCreateInfo.addState	(PipelineCreateInfo::TessellationState(4));
	pipelineCreateInfo.addState(PipelineCreateInfo::InputAssemblerState(VK_PRIMITIVE_TOPOLOGY_PATCH_LIST));
	pipelineCreateInfo.addState(PipelineCreateInfo::ColorBlendState(1, &attachmentState));

	const VkViewport	viewport	= makeViewport(WIDTH, HEIGHT);
	const VkRect2D		scissor		= makeRect2D(WIDTH, HEIGHT);

	pipelineCreateInfo.addState(PipelineCreateInfo::ViewportState(1, std::vector<VkViewport>(1, viewport), std::vector<VkRect2D>(1, scissor)));
	pipelineCreateInfo.addState(PipelineCreateInfo::DepthStencilState());
	pipelineCreateInfo.addState(PipelineCreateInfo::RasterizerState());
	pipelineCreateInfo.addState(PipelineCreateInfo::MultiSampleState());
	pipelineCreateInfo.addState(vf_info);
	m_pipeline = createGraphicsPipeline(vk, device, DE_NULL, &pipelineCreateInfo);
}

tcu::TestStatus	TessellationShaderTestInstance::executeTest (void)
{
	const DeviceInterface&					vk						= m_context.getDeviceInterface();
	const VkDevice							device					= m_context.getDevice();
	const VkQueue							queue					= m_context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();

	const CmdPoolCreateInfo					cmdPoolCreateInfo		(queueFamilyIndex);
	const Move<VkCommandPool>				cmdPool					= createCommandPool(vk, device, &cmdPoolCreateInfo);
	const deUint32							queryCount				= static_cast<deUint32>(m_drawRepeats.size());
	const Unique<VkQueryPool>				queryPool				(makeQueryPool(vk, device, queryCount, m_parametersGraphic.queryStatisticFlags));

	const VkDeviceSize						vertexBufferOffset		= 0u;
	const BufferPtr							vertexBufferSp			= creatAndFillVertexBuffer();
	const VkBuffer							vertexBuffer			= vertexBufferSp->object();

	const Unique<VkCommandBuffer>			cmdBuffer				(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	beginCommandBuffer(vk, *cmdBuffer);
	{
		std::vector<VkClearValue>	renderPassClearValues	(2);
		deMemset(&renderPassClearValues[0], 0, static_cast<int>(renderPassClearValues.size()) * sizeof(VkClearValue));

		if (!m_parametersGraphic.noColorAttachments)
			initialTransitionColor2DImage(vk, *cmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
										  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

		initialTransitionDepth2DImage(vk, *cmdBuffer, m_depthImage->object(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
									  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

		if (m_parametersGraphic.resetType != RESET_TYPE_HOST)
			vk.cmdResetQueryPool(*cmdBuffer, *queryPool, 0u, queryCount);

		beginRenderPass(vk, *cmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, WIDTH, HEIGHT), (deUint32)renderPassClearValues.size(), &renderPassClearValues[0]);

		for (deUint32 i = 0; i < queryCount; ++i)
		{
			vk.cmdBeginQuery(*cmdBuffer, *queryPool, i, (VkQueryControlFlags)0u);
			vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);

			for (deUint64 j = 0; j<m_drawRepeats[i]; ++j)
				draw(*cmdBuffer);

			vk.cmdEndQuery(*cmdBuffer, *queryPool, i);
		}

		endRenderPass(vk, *cmdBuffer);

		if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY || m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY || m_parametersGraphic.copyType == COPY_TYPE_CMD)
		{
			VkDeviceSize stride = m_parametersGraphic.querySizeFlags() ? sizeof(deUint64) : sizeof(deUint32);
			vk::VkQueryResultFlags flags = m_parametersGraphic.querySizeFlags() | VK_QUERY_RESULT_WAIT_BIT;

			if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
			{
				flags |= VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
				stride *= 2u;
			}

			if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY)
			{
				vk.cmdResetQueryPool(*cmdBuffer, *queryPool, 0u, queryCount);
				flags = VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
				stride = sizeof(ValueAndAvailability);
			}

			VkDeviceSize dstOffsetQuery = (m_parametersGraphic.dstOffset) ? stride : 0;
			vk.cmdCopyQueryPoolResults(*cmdBuffer, *queryPool, 0, queryCount, m_resetBuffer->object(), dstOffsetQuery, stride, flags);

			if (m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY)
				vk.cmdResetQueryPool(*cmdBuffer, *queryPool, 0u, queryCount);

			const VkBufferMemoryBarrier barrier =
			{
				VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	//  VkStructureType	sType;
				DE_NULL,									//  const void*		pNext;
				VK_ACCESS_TRANSFER_WRITE_BIT,				//  VkAccessFlags	srcAccessMask;
				VK_ACCESS_HOST_READ_BIT,					//  VkAccessFlags	dstAccessMask;
				VK_QUEUE_FAMILY_IGNORED,					//  deUint32		srcQueueFamilyIndex;
				VK_QUEUE_FAMILY_IGNORED,					//  deUint32		destQueueFamilyIndex;
				m_resetBuffer->object(),					//  VkBuffer		buffer;
				0u,											//  VkDeviceSize	offset;
			    queryCount * stride + dstOffsetQuery,		//  VkDeviceSize	size;
			};
			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1u, &barrier, 0, (const VkImageMemoryBarrier*)DE_NULL);
		}

		if (!m_parametersGraphic.noColorAttachments)
			transition2DImage(vk, *cmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
							  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	}
	endCommandBuffer(vk, *cmdBuffer);

	if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
		vk.resetQueryPool(device, *queryPool, 0u, queryCount);

	// Wait for completion
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);
	return checkResult (*queryPool);
}

tcu::TestStatus TessellationShaderTestInstance::checkResult (VkQueryPool queryPool)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			device		= m_context.getDevice();
	deUint64				expectedMin	= 0u;
	bool					hasMax		= false;
	deUint64				expectedMax = 0u;

	switch(m_parametersGraphic.queryStatisticFlags)
	{
		case VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT:
			expectedMin = 4u;
			break;
		case VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT:
			expectedMin = 100u;
			break;
		case VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT:
					expectedMin = 0;
					expectedMax = 0;
					hasMax = true;
			break;
		default:
			DE_FATAL("Unexpected type of statistics query");
			break;
	}

	const deUint32 queryCount = static_cast<deUint32>(m_drawRepeats.size());

	if (m_parametersGraphic.resetType == RESET_TYPE_NORMAL || m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY)
	{
		ResultsVector results(queryCount, 0u);
		if (m_parametersGraphic.copyType == COPY_TYPE_CMD)
		{
			const vk::Allocation& allocation = m_resetBuffer->getBoundMemory();
			cmdCopyQueryPoolResultsVector(results, vk, device, allocation, queryCount, (VK_QUERY_RESULT_WAIT_BIT | m_parametersGraphic.querySizeFlags()), m_parametersGraphic.dstOffset);
		}
		else
		{
			VK_CHECK(GetQueryPoolResultsVector(results, vk, device, queryPool, 0u, queryCount, (VK_QUERY_RESULT_WAIT_BIT | m_parametersGraphic.querySizeFlags())));
		}

		if (results[0] < expectedMin || (hasMax && results[0] > expectedMax))
			return tcu::TestStatus::fail("QueryPoolResults incorrect");
		if (queryCount > 1)
		{
			double pearson = calculatePearsonCorrelation(m_drawRepeats, results);
			if ( fabs( pearson ) < 0.8 )
				return tcu::TestStatus::fail("QueryPoolResults are nonlinear");
		}

		if (!m_parametersGraphic.noColorAttachments && !checkImage())
			return tcu::TestStatus::fail("Result image doesn't match expected image.");
	}
	else if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
	{
		ResultsVectorWithAvailability results(queryCount, pair<deUint64,deUint64>(0u,0u));
		if (m_parametersGraphic.copyType == COPY_TYPE_CMD)
		{
			const vk::Allocation& allocation = m_resetBuffer->getBoundMemory();
			cmdCopyQueryPoolResultsVector(results, vk, device, allocation, queryCount, (VK_QUERY_RESULT_WAIT_BIT | m_parametersGraphic.querySizeFlags() | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT), m_parametersGraphic.dstOffset);
		}
		else
		{
			VK_CHECK(GetQueryPoolResultsVector(results, vk, device, queryPool, 0u, queryCount, (VK_QUERY_RESULT_WAIT_BIT | m_parametersGraphic.querySizeFlags() | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT)));
		}

		if (results[0].first < expectedMin || (hasMax && results[0].first > expectedMax) || results[0].second == 0u)
			return tcu::TestStatus::fail("QueryPoolResults incorrect");

		if (queryCount > 1)
		{
			double pearson = calculatePearsonCorrelation(m_drawRepeats, results);
			if ( fabs( pearson ) < 0.8 )
				return tcu::TestStatus::fail("QueryPoolResults are nonlinear");
		}

		deUint64 temp = results[0].first;

		vk.resetQueryPool(device, queryPool, 0, queryCount);
		vk::VkResult res = GetQueryPoolResultsVector(results, vk, device, queryPool, 0u, queryCount, (m_parametersGraphic.querySizeFlags() | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT));
		/* From Vulkan spec:
		 *
		 * If VK_QUERY_RESULT_WAIT_BIT and VK_QUERY_RESULT_PARTIAL_BIT are both not set then no result values are written to pData
		 * for queries that are in the unavailable state at the time of the call, and vkGetQueryPoolResults returns VK_NOT_READY.
		 * However, availability state is still written to pData for those queries if VK_QUERY_RESULT_WITH_AVAILABILITY_BIT is set.
		 */
		if (res != vk::VK_NOT_READY || results[0].first != temp || results[0].second != 0u)
			return tcu::TestStatus::fail("QueryPoolResults incorrect reset");
	}
	else
	{
		// With RESET_TYPE_BEFORE_COPY, we only need to verify the result after the copy include an availability bit set as zero.
		return verifyUnavailable();
	}
	return tcu::TestStatus::pass("Pass");
}

void TessellationShaderTestInstance::draw (VkCommandBuffer cmdBuffer)
{
	const DeviceInterface& vk = m_context.getDeviceInterface();
	vk.cmdDraw(cmdBuffer, static_cast<deUint32>(m_data.size()), 1u, 0u, 0u);
}

class TessellationShaderSecondrayTestInstance : public TessellationShaderTestInstance
{
public:
							TessellationShaderSecondrayTestInstance	(vkt::Context& context, const std::vector<VertexData>& data, const ParametersGraphic& parametersGraphic, const std::vector<deUint64>& drawRepeats);
protected:
	virtual tcu::TestStatus	executeTest								(void);
};

TessellationShaderSecondrayTestInstance::TessellationShaderSecondrayTestInstance (vkt::Context& context, const std::vector<VertexData>& data, const ParametersGraphic& parametersGraphic, const std::vector<deUint64>& drawRepeats)
	: TessellationShaderTestInstance	(context, data, parametersGraphic, drawRepeats)
{
}

tcu::TestStatus	TessellationShaderSecondrayTestInstance::executeTest (void)
{
	const DeviceInterface&					vk						= m_context.getDeviceInterface();
	const VkDevice							device					= m_context.getDevice();
	const VkQueue							queue					= m_context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();

	const CmdPoolCreateInfo					cmdPoolCreateInfo		(queueFamilyIndex);
	const Move<VkCommandPool>				cmdPool					= createCommandPool(vk, device, &cmdPoolCreateInfo);
	const deUint32							queryCount				= static_cast<deUint32>(m_drawRepeats.size());
	const Unique<VkQueryPool>				queryPool				(makeQueryPool(vk, device, queryCount, m_parametersGraphic.queryStatisticFlags));

	const VkDeviceSize						vertexBufferOffset		= 0u;
	const BufferPtr							vertexBufferSp			= creatAndFillVertexBuffer();
	const VkBuffer							vertexBuffer			= vertexBufferSp->object();

	const Unique<VkCommandBuffer>			primaryCmdBuffer		(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	std::vector<VkCommandBufferSp>			secondaryCmdBuffers(queryCount);

	for (deUint32 i = 0; i < queryCount; ++i)
		secondaryCmdBuffers[i] = VkCommandBufferSp(new vk::Unique<VkCommandBuffer>(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY)));

	for (deUint32 i = 0; i < queryCount; ++i)
	{
		beginSecondaryCommandBuffer(vk, secondaryCmdBuffers[i]->get(), m_parametersGraphic.queryStatisticFlags, *m_renderPass, *m_framebuffer, VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);
		vk.cmdBeginQuery(secondaryCmdBuffers[i]->get(), *queryPool, i, (VkQueryControlFlags)0u);
		vk.cmdBindPipeline(secondaryCmdBuffers[i]->get(), VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		vk.cmdBindVertexBuffers(secondaryCmdBuffers[i]->get(), 0u, 1u, &vertexBuffer, &vertexBufferOffset);
		for (deUint32 j = 0; j<m_drawRepeats[i]; ++j)
			draw(secondaryCmdBuffers[i]->get());
		vk.cmdEndQuery(secondaryCmdBuffers[i]->get(), *queryPool, i);
		endCommandBuffer(vk, secondaryCmdBuffers[i]->get());
	}

	beginCommandBuffer(vk, *primaryCmdBuffer);
	{
		std::vector<VkClearValue>	renderPassClearValues	(2);
		deMemset(&renderPassClearValues[0], 0, static_cast<int>(renderPassClearValues.size()) * sizeof(VkClearValue));

		if (!m_parametersGraphic.noColorAttachments)
			initialTransitionColor2DImage(vk, *primaryCmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
										  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

		initialTransitionDepth2DImage(vk, *primaryCmdBuffer, m_depthImage->object(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
									  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

		vk.cmdBindVertexBuffers(*primaryCmdBuffer, 0u, 1u, &vertexBuffer, &vertexBufferOffset);
		vk.cmdBindPipeline(*primaryCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);

		if (m_parametersGraphic.resetType != RESET_TYPE_HOST)
			vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);

		beginRenderPass(vk, *primaryCmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, WIDTH, HEIGHT), (deUint32)renderPassClearValues.size(), &renderPassClearValues[0], VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
		for (deUint32 i = 0; i < queryCount; ++i)
			vk.cmdExecuteCommands(*primaryCmdBuffer, 1u, &(secondaryCmdBuffers[i]->get()));
		endRenderPass(vk, *primaryCmdBuffer);

		if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY || m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY || m_parametersGraphic.copyType == COPY_TYPE_CMD)
		{
			VkDeviceSize stride = m_parametersGraphic.querySizeFlags() ? sizeof(deUint64) : sizeof(deUint32);
			vk::VkQueryResultFlags flags = m_parametersGraphic.querySizeFlags() | VK_QUERY_RESULT_WAIT_BIT;
			deUint32 queryCountTess = queryCount;

			if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
			{
				flags |= VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
				stride *= 2u;
			}

			if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY)
			{
				vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);
				flags = VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
				stride = sizeof(ValueAndAvailability);
				queryCountTess = 1u;
			}

			VkDeviceSize dstOffsetQuery = (m_parametersGraphic.dstOffset) ? stride : 0;
			vk.cmdCopyQueryPoolResults(*primaryCmdBuffer, *queryPool, 0, queryCountTess, m_resetBuffer->object(), dstOffsetQuery, stride, flags);

			if (m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY)
				vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);

			const VkBufferMemoryBarrier barrier =
			{
				VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	//  VkStructureType	sType;
				DE_NULL,									//  const void*		pNext;
				VK_ACCESS_TRANSFER_WRITE_BIT,				//  VkAccessFlags	srcAccessMask;
				VK_ACCESS_HOST_READ_BIT,					//  VkAccessFlags	dstAccessMask;
				VK_QUEUE_FAMILY_IGNORED,					//  deUint32		srcQueueFamilyIndex;
				VK_QUEUE_FAMILY_IGNORED,					//  deUint32		destQueueFamilyIndex;
				m_resetBuffer->object(),					//  VkBuffer		buffer;
				0u,											//  VkDeviceSize	offset;
				queryCountTess * stride + dstOffsetQuery,	//  VkDeviceSize	size;
			};
			vk.cmdPipelineBarrier(*primaryCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1u, &barrier, 0, (const VkImageMemoryBarrier*)DE_NULL);
		}

		if (!m_parametersGraphic.noColorAttachments)
			transition2DImage(vk, *primaryCmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
							  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	}
	endCommandBuffer(vk, *primaryCmdBuffer);

	if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
		vk.resetQueryPool(device, *queryPool, 0u, queryCount);

	// Wait for completion
	submitCommandsAndWait(vk, device, queue, *primaryCmdBuffer);
	return checkResult (*queryPool);
}

class TessellationShaderSecondrayInheritedTestInstance : public TessellationShaderTestInstance
{
public:
							TessellationShaderSecondrayInheritedTestInstance	(vkt::Context& context, const std::vector<VertexData>& data, const ParametersGraphic& parametersGraphic, const std::vector<deUint64>& drawRepeats);
protected:
	virtual void			checkExtensions							(deBool hostQueryResetEnabled);
	virtual tcu::TestStatus	executeTest								(void);
};

TessellationShaderSecondrayInheritedTestInstance::TessellationShaderSecondrayInheritedTestInstance (vkt::Context& context, const std::vector<VertexData>& data, const ParametersGraphic& parametersGraphic, const std::vector<deUint64>& drawRepeats)
	: TessellationShaderTestInstance	(context, data, parametersGraphic, drawRepeats)
{
}

void TessellationShaderSecondrayInheritedTestInstance::checkExtensions (deBool hostQueryResetEnabled)
{
	TessellationShaderTestInstance::checkExtensions(hostQueryResetEnabled);
	if (!m_context.getDeviceFeatures().inheritedQueries)
		throw tcu::NotSupportedError("Inherited queries are not supported");
}

tcu::TestStatus	TessellationShaderSecondrayInheritedTestInstance::executeTest (void)
{
	const DeviceInterface&					vk						= m_context.getDeviceInterface();
	const VkDevice							device					= m_context.getDevice();
	const VkQueue							queue					= m_context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();

	const CmdPoolCreateInfo					cmdPoolCreateInfo		(queueFamilyIndex);
	const Move<VkCommandPool>				cmdPool					= createCommandPool(vk, device, &cmdPoolCreateInfo);
	const deUint32							queryCount				= static_cast<deUint32>(m_drawRepeats.size());
	const Unique<VkQueryPool>				queryPool				(makeQueryPool(vk, device, queryCount, m_parametersGraphic.queryStatisticFlags));

	const VkDeviceSize						vertexBufferOffset		= 0u;
	const BufferPtr							vertexBufferSp			= creatAndFillVertexBuffer();
	const VkBuffer							vertexBuffer			= vertexBufferSp->object();

	const Unique<VkCommandBuffer>			primaryCmdBuffer		(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	std::vector<VkCommandBufferSp>			secondaryCmdBuffers(queryCount);

	for (deUint32 i = 0; i < queryCount; ++i)
		secondaryCmdBuffers[i] = VkCommandBufferSp(new vk::Unique<VkCommandBuffer>(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY)));

	for (deUint32 i = 0; i < queryCount; ++i)
	{
		beginSecondaryCommandBuffer(vk, secondaryCmdBuffers[i]->get(), m_parametersGraphic.queryStatisticFlags, *m_renderPass, *m_framebuffer, VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);
		vk.cmdBindPipeline(secondaryCmdBuffers[i]->get(), VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		vk.cmdBindVertexBuffers(secondaryCmdBuffers[i]->get(), 0u, 1u, &vertexBuffer, &vertexBufferOffset);
		for (deUint32 j = 0; j<m_drawRepeats[i]; ++j)
			draw(secondaryCmdBuffers[i]->get());
		endCommandBuffer(vk, secondaryCmdBuffers[i]->get());
	}

	beginCommandBuffer(vk, *primaryCmdBuffer);
	{
		std::vector<VkClearValue>	renderPassClearValues	(2);
		deMemset(&renderPassClearValues[0], 0, static_cast<int>(renderPassClearValues.size()) * sizeof(VkClearValue));

		if (!m_parametersGraphic.noColorAttachments)
			initialTransitionColor2DImage(vk, *primaryCmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
										  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

		initialTransitionDepth2DImage(vk, *primaryCmdBuffer, m_depthImage->object(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
									  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

		if (m_parametersGraphic.resetType != RESET_TYPE_HOST)
			vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);

		for (deUint32 i = 0; i < queryCount; ++i)
		{
			vk.cmdBeginQuery(*primaryCmdBuffer, *queryPool, i, (VkQueryControlFlags)0u);
			beginRenderPass(vk, *primaryCmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, WIDTH, HEIGHT), (deUint32)renderPassClearValues.size(), &renderPassClearValues[0], VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
			vk.cmdExecuteCommands(*primaryCmdBuffer, 1u, &(secondaryCmdBuffers[i]->get()));
			endRenderPass(vk, *primaryCmdBuffer);
			vk.cmdEndQuery(*primaryCmdBuffer, *queryPool, i);
		}

		if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY || m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY || m_parametersGraphic.copyType == COPY_TYPE_CMD)
		{
			VkDeviceSize stride = m_parametersGraphic.querySizeFlags() ? sizeof(deUint64) : sizeof(deUint32);
			vk::VkQueryResultFlags flags = m_parametersGraphic.querySizeFlags() | VK_QUERY_RESULT_WAIT_BIT;

			if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
			{
				flags |= VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
				stride *= 2u;
			}

			if (m_parametersGraphic.resetType == RESET_TYPE_BEFORE_COPY)
			{
				vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);
				flags = VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
				stride = sizeof(ValueAndAvailability);
			}

			VkDeviceSize dstOffsetQuery = (m_parametersGraphic.dstOffset) ? stride : 0;
			vk.cmdCopyQueryPoolResults(*primaryCmdBuffer, *queryPool, 0, queryCount, m_resetBuffer->object(), dstOffsetQuery, stride, flags);

			if (m_parametersGraphic.resetType == RESET_TYPE_AFTER_COPY)
				vk.cmdResetQueryPool(*primaryCmdBuffer, *queryPool, 0u, queryCount);

			const VkBufferMemoryBarrier barrier =
			{
				VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	//  VkStructureType	sType;
				DE_NULL,									//  const void*		pNext;
				VK_ACCESS_TRANSFER_WRITE_BIT,				//  VkAccessFlags	srcAccessMask;
				VK_ACCESS_HOST_READ_BIT,					//  VkAccessFlags	dstAccessMask;
				VK_QUEUE_FAMILY_IGNORED,					//  deUint32		srcQueueFamilyIndex;
				VK_QUEUE_FAMILY_IGNORED,					//  deUint32		destQueueFamilyIndex;
				m_resetBuffer->object(),					//  VkBuffer		buffer;
				0u,											//  VkDeviceSize	offset;
				queryCount * stride + dstOffsetQuery,		//  VkDeviceSize	size;
			};
			vk.cmdPipelineBarrier(*primaryCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1u, &barrier, 0, (const VkImageMemoryBarrier*)DE_NULL);
		}

		if (!m_parametersGraphic.noColorAttachments)
			transition2DImage(vk, *primaryCmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
							  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	}
	endCommandBuffer(vk, *primaryCmdBuffer);

	if (m_parametersGraphic.resetType == RESET_TYPE_HOST)
		vk.resetQueryPool(device, *queryPool, 0u, queryCount);

	// Wait for completion
	submitCommandsAndWait(vk, device, queue, *primaryCmdBuffer);
	return checkResult (*queryPool);
}

template<class Instance>
class QueryPoolComputeStatsTest : public TestCase
{
public:
	QueryPoolComputeStatsTest (tcu::TestContext &context, const std::string& name, const std::string& description, const ResetType resetType, const CopyType copyType, deBool query64Bits, const bool useComputeQueue, deBool dstOffset = DE_FALSE, const StrideType strideType = STRIDE_TYPE_VALID)
		: TestCase			(context, name.c_str(), description.c_str())
		, m_useComputeQueue	(useComputeQueue)
	{
		const tcu::UVec3	localSize[]		=
		{
			tcu::UVec3	(2u,			2u,	2u),
			tcu::UVec3	(1u,			1u,	1u),
			tcu::UVec3	(WIDTH/(7u*3u),	7u,	3u),
		};

		const tcu::UVec3	groupSize[]		=
		{
			tcu::UVec3	(2u,			2u,	2u),
			tcu::UVec3	(WIDTH/(7u*3u),	7u,	3u),
			tcu::UVec3	(1u,			1u,	1u),
		};

		DE_ASSERT(DE_LENGTH_OF_ARRAY(localSize) == DE_LENGTH_OF_ARRAY(groupSize));

		for(int shaderNdx = 0; shaderNdx < DE_LENGTH_OF_ARRAY(localSize); ++shaderNdx)
		{
			std::ostringstream	shaderName;
			shaderName<< "compute_" << shaderNdx;
			const ComputeInvocationsTestInstance::ParametersCompute	parameters(
				localSize[shaderNdx],
				groupSize[shaderNdx],
				shaderName.str(),
				resetType,
				copyType,
				query64Bits,
				dstOffset,
				strideType,
				m_useComputeQueue
			);
			m_parameters.push_back(parameters);
		}
	}

	vkt::TestInstance* createInstance (vkt::Context& context) const override
	{
		return new Instance(context, m_parameters);
	}

	void checkSupport (Context& context) const override
	{
		if (m_useComputeQueue)
		{
			const auto&	vki				= context.getInstanceInterface();
			const auto	physicalDevice	= context.getPhysicalDevice();

			checkSupportForNonGraphicsQueueFamily(vki, physicalDevice);
		}
	}

	void initPrograms(SourceCollections& sourceCollections) const override
	{
		std::ostringstream	source;
		source	<< "layout(binding = 0) writeonly buffer Output {\n"
				<< "	uint values[];\n"
				<< "} sb_out;\n\n"
				<< "void main (void) {\n"
				<< "	uvec3 indexUvec3 = uvec3 (gl_GlobalInvocationID.x,\n"
				<< "	                          gl_GlobalInvocationID.y * gl_NumWorkGroups.x * gl_WorkGroupSize.x,\n"
				<< "	                          gl_GlobalInvocationID.z * gl_NumWorkGroups.x * gl_NumWorkGroups.y * gl_WorkGroupSize.x * gl_WorkGroupSize.y);\n"
				<< "	uint index = indexUvec3.x + indexUvec3.y + indexUvec3.z;\n"
				<< "	sb_out.values[index] += index;\n"
				<< "}\n";

		for(size_t shaderNdx = 0; shaderNdx < m_parameters.size(); ++shaderNdx)
		{
			std::ostringstream	src;
			src	<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
				<< "layout (local_size_x = " << m_parameters[shaderNdx].localSize.x() << ", local_size_y = " << m_parameters[shaderNdx].localSize.y() << ", local_size_z = " << m_parameters[shaderNdx].localSize.z() << ") in;\n"
				<< source.str();
			sourceCollections.glslSources.add(m_parameters[shaderNdx].shaderName) << glu::ComputeSource(src.str());
		}
	}
private:
	std::vector<ComputeInvocationsTestInstance::ParametersCompute>	m_parameters;
	const bool														m_useComputeQueue;
};

template<class Instance>
class QueryPoolGraphicStatisticsTest : public TestCase
{
public:
	QueryPoolGraphicStatisticsTest (tcu::TestContext &context, const std::string& name, const std::string& description, const GraphicBasicTestInstance::ParametersGraphic parametersGraphic, const std::vector<deUint64>& drawRepeats)
		: TestCase				(context, name.c_str(), description.c_str())
		, m_parametersGraphic	(parametersGraphic)
		, m_drawRepeats			( drawRepeats )
	{
		m_data.push_back(GraphicBasicTestInstance::VertexData(tcu::Vec4(-1.0f,-1.0f, 1.0f, 1.0f), tcu::RGBA::red().toVec()));
		m_data.push_back(GraphicBasicTestInstance::VertexData(tcu::Vec4(-1.0f, 0.0f, 1.0f, 1.0f), tcu::RGBA::red().toVec()));
		m_data.push_back(GraphicBasicTestInstance::VertexData(tcu::Vec4( 0.0f,-1.0f, 1.0f, 1.0f), tcu::RGBA::red().toVec()));
		m_data.push_back(GraphicBasicTestInstance::VertexData(tcu::Vec4( 0.0f, 0.0f, 1.0f, 1.0f), tcu::RGBA::red().toVec()));

		m_data.push_back(GraphicBasicTestInstance::VertexData(tcu::Vec4(-1.0f, 0.0f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
		m_data.push_back(GraphicBasicTestInstance::VertexData(tcu::Vec4(-1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
		m_data.push_back(GraphicBasicTestInstance::VertexData(tcu::Vec4( 0.0f, 0.0f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
		m_data.push_back(GraphicBasicTestInstance::VertexData(tcu::Vec4( 0.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));

		m_data.push_back(GraphicBasicTestInstance::VertexData(tcu::Vec4( 0.0f,-1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));
		m_data.push_back(GraphicBasicTestInstance::VertexData(tcu::Vec4( 0.0f, 0.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));
		m_data.push_back(GraphicBasicTestInstance::VertexData(tcu::Vec4( 1.0f,-1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));
		m_data.push_back(GraphicBasicTestInstance::VertexData(tcu::Vec4( 1.0f, 0.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));

		m_data.push_back(GraphicBasicTestInstance::VertexData(tcu::Vec4( 0.0f, 0.0f, 1.0f, 1.0f), tcu::RGBA::gray().toVec()));
		m_data.push_back(GraphicBasicTestInstance::VertexData(tcu::Vec4( 0.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::gray().toVec()));
		m_data.push_back(GraphicBasicTestInstance::VertexData(tcu::Vec4( 1.0f, 0.0f, 1.0f, 1.0f), tcu::RGBA::gray().toVec()));
		m_data.push_back(GraphicBasicTestInstance::VertexData(tcu::Vec4( 1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::gray().toVec()));
	}

	void checkSupport (vkt::Context& context) const
	{
#ifndef CTS_USES_VULKANSC
		if (m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN &&
			context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") &&
			!context.getPortabilitySubsetFeatures().triangleFans)
		{
			TCU_THROW(NotSupportedError, "VK_KHR_portability_subset: Triangle fans are not supported by this implementation");
		}
#else
		DE_UNREF(context);
#endif // CTS_USES_VULKANSC
	}

	vkt::TestInstance* createInstance (vkt::Context& context) const
	{
		return new Instance(context, m_data, m_parametersGraphic, m_drawRepeats);
	}

	void initPrograms(SourceCollections& sourceCollections) const
	{
		{ // Vertex Shader
			std::ostringstream	source;
			source	<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
					<< "layout(location = 0) in highp vec4 in_position;\n"
					<< "layout(location = 1) in vec4 in_color;\n"
					<< "layout(location = 0) out vec4 out_color;\n"
					<< "void main (void)\n"
					<< "{\n"
					<< "	gl_PointSize = 1.0;\n"
					<< "	gl_Position = in_position;\n"
					<< "	out_color = in_color;\n"
					<< "}\n";
			sourceCollections.glslSources.add("vertex") << glu::VertexSource(source.str());
		}

		if (m_parametersGraphic.hasTess)
		{// Tessellation control & evaluation
			std::ostringstream source_tc;
			source_tc	<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
						<< "#extension GL_EXT_tessellation_shader : require\n"
						<< "layout(vertices = 4) out;\n"
						<< "layout(location = 0) in vec4 in_color[];\n"
						<< "layout(location = 0) out vec4 out_color[];\n"
						<< "\n"
						<< "void main (void)\n"
						<< "{\n"
						<< "	if( gl_InvocationID == 0 )\n"
						<< "	{\n"
						<< "		gl_TessLevelInner[0] = 4.0f;\n"
						<< "		gl_TessLevelInner[1] = 4.0f;\n"
						<< "		gl_TessLevelOuter[0] = 4.0f;\n"
						<< "		gl_TessLevelOuter[1] = 4.0f;\n"
						<< "		gl_TessLevelOuter[2] = 4.0f;\n"
						<< "		gl_TessLevelOuter[3] = 4.0f;\n"
						<< "	}\n"
						<< "	out_color[gl_InvocationID] = in_color[gl_InvocationID];\n"
						<< "	gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
						<< "}\n";
			sourceCollections.glslSources.add("tessellation_control") << glu::TessellationControlSource(source_tc.str());

			std::ostringstream source_te;
			source_te	<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
						<< "#extension GL_EXT_tessellation_shader : require\n"
						<< "layout( quads, equal_spacing, ccw ) in;\n"
						<< "layout(location = 0) in vec4 in_color[];\n"
						<< "layout(location = 0) out vec4 out_color;\n"
						<< "void main (void)\n"
						<< "{\n"
						<< "	const float u = gl_TessCoord.x;\n"
						<< "	const float v = gl_TessCoord.y;\n"
						<< "	const float w = gl_TessCoord.z;\n"
						<< "	gl_Position = (1 - u) * (1 - v) * gl_in[0].gl_Position +(1 - u) * v * gl_in[1].gl_Position + u * (1 - v) * gl_in[2].gl_Position + u * v * gl_in[3].gl_Position;\n"
						<< "	out_color = in_color[0];\n"
						<< "}\n";
			sourceCollections.glslSources.add("tessellation_evaluation") << glu::TessellationEvaluationSource(source_te.str());
		}

		if(m_parametersGraphic.queryStatisticFlags & (VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT | VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT |
									VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT | VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT))
		{ // Geometry Shader
			const bool isTopologyPointSize = m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
			std::ostringstream	source;
			source	<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
					<< "layout("<<inputTypeToGLString(m_parametersGraphic.primitiveTopology)<<") in;\n"
					<< "layout("<<outputTypeToGLString (m_parametersGraphic.primitiveTopology)<<", max_vertices = 16) out;\n"
					<< "layout(location = 0) in vec4 in_color[];\n"
					<< "layout(location = 0) out vec4 out_color;\n"
					<< "void main (void)\n"
					<< "{\n"
					<< "	out_color = in_color[0];\n"
					<< (isTopologyPointSize ? "${pointSize}" : "" )
					<< "	gl_Position = gl_in[0].gl_Position;\n"
					<< "	EmitVertex();\n"
					<< "	EndPrimitive();\n"
					<< "\n"
					<< "	out_color = in_color[0];\n"
					<< (isTopologyPointSize ? "${pointSize}" : "")
					<< "	gl_Position = vec4(1.0, 1.0, 1.0, 1.0);\n"
					<< "	EmitVertex();\n"
					<< "	out_color = in_color[0];\n"
					<< (isTopologyPointSize ? "${pointSize}" : "")
					<< "	gl_Position = vec4(-1.0, -1.0, 1.0, 1.0);\n"
					<< "	EmitVertex();\n"
					<< "	EndPrimitive();\n"
					<< "\n";
			if (m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP ||
				m_parametersGraphic.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
			{
				source	<< "\n"
						<< "	out_color = in_color[0];\n"
						<< "	gl_Position = gl_in[0].gl_Position;\n"
						<< "	EmitVertex();\n"
						<< "	out_color = in_color[0];\n"
						<< "	gl_Position = gl_in[1].gl_Position;\n"
						<< "	EmitVertex();\n"
						<< "	out_color = in_color[0];\n"
						<< "	gl_Position = gl_in[2].gl_Position;\n"
						<< "	EmitVertex();\n"
						<< "	out_color = in_color[0];\n"
						<< "	gl_Position = vec4(gl_in[2].gl_Position.x, gl_in[1].gl_Position.y, 1.0, 1.0);\n"
						<< "	EmitVertex();\n"
						<< "	EndPrimitive();\n";
			}
			else
			{
				source	<< "	out_color = in_color[0];\n"
						<< (isTopologyPointSize ? "${pointSize}" : "")
						<< "	gl_Position = vec4(1.0, 1.0, 1.0, 1.0);\n"
						<< "	EmitVertex();\n"
						<< "	out_color = in_color[0];\n"
						<< (isTopologyPointSize ? "${pointSize}" : "")
						<< "	gl_Position = vec4(1.0, -1.0, 1.0, 1.0);\n"
						<< "	EmitVertex();\n"
						<< "	out_color = in_color[0];\n"
						<< (isTopologyPointSize ? "${pointSize}" : "")
						<< "	gl_Position = vec4(-1.0, 1.0, 1.0, 1.0);\n"
						<< "	EmitVertex();\n"
						<< "	out_color = in_color[0];\n"
						<< (isTopologyPointSize ? "${pointSize}" : "")
						<< "	gl_Position = vec4(-1.0, -1.0, 1.0, 1.0);\n"
						<< "	EmitVertex();\n"
						<< "	EndPrimitive();\n";
			}
			source	<< "}\n";

			if (isTopologyPointSize)
			{
				// Add geometry shader codes with and without gl_PointSize if the primitive topology is VK_PRIMITIVE_TOPOLOGY_POINT_LIST

				tcu::StringTemplate sourceTemplate(source.str());

				std::map<std::string, std::string> pointSize;
				std::map<std::string, std::string> noPointSize;

				pointSize["pointSize"]		= "	gl_PointSize = gl_in[0].gl_PointSize;\n";
				noPointSize["pointSize"]	= "";

				sourceCollections.glslSources.add("geometry") << glu::GeometrySource(sourceTemplate.specialize(noPointSize));
				sourceCollections.glslSources.add("geometry_point_size") << glu::GeometrySource(sourceTemplate.specialize(pointSize));
			}
			else
			{
				sourceCollections.glslSources.add("geometry") << glu::GeometrySource(source.str());
			}
		}

		if (!m_parametersGraphic.vertexOnlyPipe)
		{ // Fragment Shader
			std::ostringstream	source;
			source	<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
					<< "layout(location = 0) in vec4 in_color;\n"
					<< "layout(location = 0) out vec4 out_color;\n"
					<< "void main()\n"
					<<"{\n"
					<< "	out_color = in_color;\n"
					<< "}\n";
			sourceCollections.glslSources.add("fragment") << glu::FragmentSource(source.str());
		}
	}
private:
	std::vector<GraphicBasicTestInstance::VertexData>	m_data;
	const GraphicBasicTestInstance::ParametersGraphic	m_parametersGraphic;
	std::vector<deUint64>								m_drawRepeats;
};

#define NUM_QUERY_STATISTICS 4

class StatisticMultipleQueryTestInstance : public TestInstance
{
public:
					StatisticMultipleQueryTestInstance	(Context& context, const deUint32 queryCount);
protected:
	BufferPtr				m_queryBuffer;

	virtual void			checkExtensions		();
};

StatisticMultipleQueryTestInstance::StatisticMultipleQueryTestInstance (Context& context, const deUint32 queryCount)
	: TestInstance	(context)
	, m_queryBuffer (Buffer::createAndAlloc(context.getDeviceInterface(),
											context.getDevice(),
											BufferCreateInfo(NUM_QUERY_STATISTICS * sizeof(deUint64) * queryCount, VK_BUFFER_USAGE_TRANSFER_DST_BIT),
											context.getDefaultAllocator(),
											vk::MemoryRequirement::HostVisible))
{
	const vk::Allocation& allocation = m_queryBuffer->getBoundMemory();
	void* allocationData = allocation.getHostPtr();
	deMemset(allocationData, 0xff, NUM_QUERY_STATISTICS * sizeof(deUint64) * queryCount);
}

void StatisticMultipleQueryTestInstance::checkExtensions ()
{
	if (!m_context.getDeviceFeatures().pipelineStatisticsQuery)
		throw tcu::NotSupportedError("Pipeline statistics queries are not supported");
}

class GraphicBasicMultipleQueryTestInstance : public StatisticMultipleQueryTestInstance
{
public:
	struct VertexData
	{
		VertexData (const tcu::Vec4 position_, const tcu::Vec4 color_)
			: position	(position_)
			, color		(color_)
		{}
		tcu::Vec4	position;
		tcu::Vec4	color;
	};

	struct ParametersGraphic : public GenericParameters
	{
		ParametersGraphic (const VkQueryPipelineStatisticFlags queryStatisticFlags_, const VkQueryResultFlags queryFlags_, const deUint32 queryCount_, const deBool vertexOnlyPipe_, const CopyType copyType_, const deUint32 dstOffset_, const StrideType strideType_, const ClearOperation clearOp_ = CLEAR_NOOP)
			: GenericParameters		{ RESET_TYPE_NORMAL, copyType_, (queryFlags_ & VK_QUERY_RESULT_64_BIT) != 0u, dstOffset_ != 0u, strideType_ }
			, queryStatisticFlags	(queryStatisticFlags_)
			, vertexOnlyPipe		(vertexOnlyPipe_)
			, queryFlags			(queryFlags_)
			, queryCount			(queryCount_)
			, dstOffset				(dstOffset_)
			, clearOp				(clearOp_)
			{}

		VkQueryPipelineStatisticFlags	queryStatisticFlags;
		VkPrimitiveTopology				primitiveTopology;
		deBool							vertexOnlyPipe;
		VkQueryResultFlags				queryFlags;
		deUint32						queryCount;
		deUint32						dstOffset;
		ClearOperation					clearOp;
	};
											GraphicBasicMultipleQueryTestInstance			(vkt::Context&					context,
																							 const std::vector<VertexData>&	data,
																							 const ParametersGraphic&		parametersGraphic);
	tcu::TestStatus							iterate								(void);
protected:
	BufferPtr								creatAndFillVertexBuffer			(void);
	virtual void							createPipeline						(void) = 0;
	void									creatColorAttachmentAndRenderPass	(void);
	virtual tcu::TestStatus					executeTest							(void) = 0;
	virtual tcu::TestStatus					checkResult							(VkQueryPool queryPool) = 0;
	virtual void							draw								(VkCommandBuffer cmdBuffer) = 0;

	const VkFormat						m_colorAttachmentFormat;
	de::SharedPtr<Image>				m_colorAttachmentImage;
	de::SharedPtr<Image>				m_depthImage;
	Move<VkImageView>					m_attachmentView;
	Move<VkImageView>					m_depthView;
	Move<VkRenderPass>					m_renderPass;
	Move<VkFramebuffer>					m_framebuffer;
	Move<VkPipeline>					m_pipeline;
	Move<VkPipelineLayout>				m_pipelineLayout;
	const std::vector<VertexData>&		m_data;
	const ParametersGraphic&			m_parametersGraphic;
};

GraphicBasicMultipleQueryTestInstance::GraphicBasicMultipleQueryTestInstance (vkt::Context&					context,
																			  const std::vector<VertexData>&	data,
																			  const ParametersGraphic&		parametersGraphic)
	: StatisticMultipleQueryTestInstance	(context, (parametersGraphic.queryCount + (parametersGraphic.dstOffset != 0u ? 1u : 0u)))
	, m_colorAttachmentFormat		(VK_FORMAT_R8G8B8A8_UNORM)
	, m_data						(data)
	, m_parametersGraphic			(parametersGraphic)
{
}

tcu::TestStatus GraphicBasicMultipleQueryTestInstance::iterate (void)
{
	checkExtensions();
	creatColorAttachmentAndRenderPass();
	createPipeline();
	return executeTest();
}

BufferPtr GraphicBasicMultipleQueryTestInstance::creatAndFillVertexBuffer (void)
{
	const DeviceInterface&		vk				= m_context.getDeviceInterface();
	const VkDevice				device			= m_context.getDevice();

	const VkDeviceSize			dataSize		= static_cast<VkDeviceSize>(deAlignSize(static_cast<size_t>( m_data.size() * sizeof(VertexData)),
		static_cast<size_t>(m_context.getDeviceProperties().limits.nonCoherentAtomSize)));

	BufferPtr					vertexBuffer	= Buffer::createAndAlloc(vk, device, BufferCreateInfo(dataSize,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), m_context.getDefaultAllocator(), MemoryRequirement::HostVisible);

	deUint8*					ptr				= reinterpret_cast<deUint8*>(vertexBuffer->getBoundMemory().getHostPtr());
	deMemcpy(ptr, &m_data[0], static_cast<size_t>( m_data.size() * sizeof(VertexData)));

	flushMappedMemoryRange(vk, device, vertexBuffer->getBoundMemory().getMemory(), vertexBuffer->getBoundMemory().getOffset(), dataSize);
	return vertexBuffer;
}

void GraphicBasicMultipleQueryTestInstance::creatColorAttachmentAndRenderPass (void)
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDevice			device	= m_context.getDevice();

	{
		VkExtent3D					imageExtent				=
		{
			WIDTH,	// width;
			HEIGHT,	// height;
			1u		// depth;
		};

		const ImageCreateInfo		colorImageCreateInfo	(VK_IMAGE_TYPE_2D, m_colorAttachmentFormat, imageExtent, 1, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL,
															VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

		m_colorAttachmentImage	= Image::createAndAlloc(vk, device, colorImageCreateInfo, m_context.getDefaultAllocator(), m_context.getUniversalQueueFamilyIndex());

		const ImageViewCreateInfo	attachmentViewInfo		(m_colorAttachmentImage->object(), VK_IMAGE_VIEW_TYPE_2D, m_colorAttachmentFormat);
		m_attachmentView			= createImageView(vk, device, &attachmentViewInfo);

		ImageCreateInfo				depthImageCreateInfo	(vk::VK_IMAGE_TYPE_2D, VK_FORMAT_D16_UNORM, imageExtent, 1, 1, vk::VK_SAMPLE_COUNT_1_BIT, vk::VK_IMAGE_TILING_OPTIMAL,
															 vk::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

		m_depthImage				= Image::createAndAlloc(vk, device, depthImageCreateInfo, m_context.getDefaultAllocator(), m_context.getUniversalQueueFamilyIndex());

		// Construct a depth  view from depth image
		const ImageViewCreateInfo	depthViewInfo			(m_depthImage->object(), vk::VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_D16_UNORM);
		m_depthView				= vk::createImageView(vk, device, &depthViewInfo);
	}

	{
		// Renderpass and Framebuffer
		RenderPassCreateInfo		renderPassCreateInfo;
		renderPassCreateInfo.addAttachment(AttachmentDescription(m_colorAttachmentFormat,						// format
																	VK_SAMPLE_COUNT_1_BIT,						// samples
																	VK_ATTACHMENT_LOAD_OP_CLEAR,				// loadOp
																	VK_ATTACHMENT_STORE_OP_STORE ,				// storeOp
																	VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// stencilLoadOp
																	VK_ATTACHMENT_STORE_OP_STORE ,				// stencilLoadOp
																	VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// initialLauout
																	VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));	// finalLayout

		renderPassCreateInfo.addAttachment(AttachmentDescription(VK_FORMAT_D16_UNORM,										// format
																 vk::VK_SAMPLE_COUNT_1_BIT,									// samples
																 vk::VK_ATTACHMENT_LOAD_OP_CLEAR,							// loadOp
																 vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,						// storeOp
																 vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,						// stencilLoadOp
																 vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,						// stencilLoadOp
																 vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,		// initialLauout
																 vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));	// finalLayout

		const VkAttachmentReference	colorAttachmentReference =
		{
			0u,											// attachment
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// layout
		};

		const VkAttachmentReference depthAttachmentReference =
		{
			1u,															// attachment
			vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL		// layout
		};

		const VkSubpassDescription	subpass =
		{
			(VkSubpassDescriptionFlags) 0,		//VkSubpassDescriptionFlags		flags;
			VK_PIPELINE_BIND_POINT_GRAPHICS,	//VkPipelineBindPoint			pipelineBindPoint;
			0u,									//deUint32						inputAttachmentCount;
			DE_NULL,							//const VkAttachmentReference*	pInputAttachments;
			1u,									//deUint32						colorAttachmentCount;
			&colorAttachmentReference,			//const VkAttachmentReference*	pColorAttachments;
			DE_NULL,							//const VkAttachmentReference*	pResolveAttachments;
			&depthAttachmentReference,			//const VkAttachmentReference*	pDepthStencilAttachment;
			0u,									//deUint32						preserveAttachmentCount;
			DE_NULL,							//const deUint32*				pPreserveAttachments;
		};

		renderPassCreateInfo.addSubpass(subpass);
		m_renderPass = createRenderPass(vk, device, &renderPassCreateInfo);

		std::vector<vk::VkImageView> attachments(2);
		attachments[0] = *m_attachmentView;
		attachments[1] = *m_depthView;

		FramebufferCreateInfo		framebufferCreateInfo(*m_renderPass, attachments, WIDTH, HEIGHT, 1);
		m_framebuffer = createFramebuffer(vk, device, &framebufferCreateInfo);
	}
}

class VertexShaderMultipleQueryTestInstance : public GraphicBasicMultipleQueryTestInstance
{
public:
							VertexShaderMultipleQueryTestInstance	(vkt::Context& context, const std::vector<VertexData>& data, const ParametersGraphic& parametersGraphic);
protected:
	virtual void			createPipeline				(void);
	virtual tcu::TestStatus	executeTest					(void);
	virtual tcu::TestStatus	checkResult					(VkQueryPool queryPool);
	void					draw						(VkCommandBuffer cmdBuffer);
	deUint64				calculateExpectedMin		(VkQueryResultFlags flag);
	deUint64				calculateExpectedMax		(VkQueryResultFlags flag);
};

VertexShaderMultipleQueryTestInstance::VertexShaderMultipleQueryTestInstance (vkt::Context& context, const std::vector<VertexData>& data, const ParametersGraphic& parametersGraphic)
	: GraphicBasicMultipleQueryTestInstance	(context, data, parametersGraphic)
{
}

void VertexShaderMultipleQueryTestInstance::createPipeline (void)
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDevice			device	= m_context.getDevice();

	// Pipeline
	Unique<VkShaderModule>	vs(createShaderModule(vk, device, m_context.getBinaryCollection().get("vertex"), 0));
	Move<VkShaderModule>	fs;

	if (!m_parametersGraphic.vertexOnlyPipe)
		fs = createShaderModule(vk, device, m_context.getBinaryCollection().get("fragment"), 0);

	const PipelineCreateInfo::ColorBlendState::Attachment attachmentState;

	const PipelineLayoutCreateInfo pipelineLayoutCreateInfo;
	m_pipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);

	const VkVertexInputBindingDescription vertexInputBindingDescription		=
	{
		0,											// binding;
		static_cast<deUint32>(sizeof(VertexData)),	// stride;
		VK_VERTEX_INPUT_RATE_VERTEX				// inputRate
	};

	const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[] =
	{
		{
			0u,
			0u,
			VK_FORMAT_R32G32B32A32_SFLOAT,
			0u
		},	// VertexElementData::position
		{
			1u,
			0u,
			VK_FORMAT_R32G32B32A32_SFLOAT,
			static_cast<deUint32>(sizeof(tcu::Vec4))
		},	// VertexElementData::color
	};

	const VkPipelineVertexInputStateCreateInfo vf_info			=
	{																	// sType;
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// pNext;
		NULL,															// flags;
		0u,																// vertexBindingDescriptionCount;
		1u,																// pVertexBindingDescriptions;
		&vertexInputBindingDescription,									// vertexAttributeDescriptionCount;
		2u,																// pVertexAttributeDescriptions;
		vertexInputAttributeDescriptions
	};

	PipelineCreateInfo pipelineCreateInfo(*m_pipelineLayout, *m_renderPass, 0, (VkPipelineCreateFlags)0);
	pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*vs, "main", VK_SHADER_STAGE_VERTEX_BIT));
	if (!m_parametersGraphic.vertexOnlyPipe)
		pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*fs, "main", VK_SHADER_STAGE_FRAGMENT_BIT));
	pipelineCreateInfo.addState(PipelineCreateInfo::DepthStencilState());
	pipelineCreateInfo.addState(PipelineCreateInfo::InputAssemblerState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST));
	pipelineCreateInfo.addState(PipelineCreateInfo::ColorBlendState(1, &attachmentState));

	const VkViewport	viewport	= makeViewport(WIDTH, HEIGHT);
	const VkRect2D		scissor		= makeRect2D(WIDTH, HEIGHT);
	pipelineCreateInfo.addState(PipelineCreateInfo::ViewportState(1u, std::vector<VkViewport>(1, viewport), std::vector<VkRect2D>(1, scissor)));
	pipelineCreateInfo.addState(PipelineCreateInfo::DepthStencilState());
	pipelineCreateInfo.addState(PipelineCreateInfo::RasterizerState());
	pipelineCreateInfo.addState(PipelineCreateInfo::MultiSampleState());
	pipelineCreateInfo.addState(vf_info);
	m_pipeline = createGraphicsPipeline(vk, device, DE_NULL, &pipelineCreateInfo);
}

tcu::TestStatus VertexShaderMultipleQueryTestInstance::executeTest (void)
{
	const DeviceInterface&					vk						= m_context.getDeviceInterface();
	const VkDevice							device					= m_context.getDevice();
	const VkQueue							queue					= m_context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();

	const CmdPoolCreateInfo					cmdPoolCreateInfo		(queueFamilyIndex);
	const Move<VkCommandPool>				cmdPool					= createCommandPool(vk, device, &cmdPoolCreateInfo);
	const Unique<VkQueryPool>				queryPool				(makeQueryPool(vk, device, m_parametersGraphic.queryCount, m_parametersGraphic.queryStatisticFlags));

	const VkDeviceSize						vertexBufferOffset		= 0u;
	const BufferPtr							vertexBufferSp			= creatAndFillVertexBuffer();
	const VkBuffer							vertexBuffer			= vertexBufferSp->object();

	const Unique<VkCommandBuffer>			cmdBuffer				(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	beginCommandBuffer(vk, *cmdBuffer);
	{
		std::vector<VkClearValue>	renderPassClearValues	(2);
		deMemset(&renderPassClearValues[0], 0, static_cast<int>(renderPassClearValues.size()) * sizeof(VkClearValue));

		initialTransitionColor2DImage(vk, *cmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
									  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
		initialTransitionDepth2DImage(vk, *cmdBuffer, m_depthImage->object(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
									  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

		vk.cmdResetQueryPool(*cmdBuffer, *queryPool, 0u, m_parametersGraphic.queryCount);

		beginRenderPass(vk, *cmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, WIDTH, HEIGHT), (deUint32)renderPassClearValues.size(), &renderPassClearValues[0]);

		vk.cmdBeginQuery(*cmdBuffer, *queryPool, 0u, (VkQueryControlFlags)0u);
		vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);

		draw(*cmdBuffer);

		vk.cmdEndQuery(*cmdBuffer, *queryPool, 0u);

		endRenderPass(vk, *cmdBuffer);

		if (m_parametersGraphic.copyType == COPY_TYPE_CMD)
		{
			VkDeviceSize copyStride = NUM_QUERY_STATISTICS * sizeof(deUint64);
			if (m_parametersGraphic.queryCount == 1u && m_parametersGraphic.strideType == STRIDE_TYPE_ZERO)
				copyStride = 0u;

			vk.cmdCopyQueryPoolResults(*cmdBuffer, *queryPool, 0, m_parametersGraphic.queryCount, m_queryBuffer->object(), m_parametersGraphic.dstOffset, copyStride, m_parametersGraphic.queryFlags);

			const VkDeviceSize bufferSize = NUM_QUERY_STATISTICS * sizeof(deUint64) * m_parametersGraphic.queryCount;
			const VkBufferMemoryBarrier barrier =
			{
				VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	//  VkStructureType	sType;
				DE_NULL,									//  const void*		pNext;
				VK_ACCESS_TRANSFER_WRITE_BIT,				//  VkAccessFlags	srcAccessMask;
				VK_ACCESS_HOST_READ_BIT,					//  VkAccessFlags	dstAccessMask;
				VK_QUEUE_FAMILY_IGNORED,					//  deUint32		srcQueueFamilyIndex;
				VK_QUEUE_FAMILY_IGNORED,					//  deUint32		destQueueFamilyIndex;
				m_queryBuffer->object(),					//  VkBuffer		buffer;
				0u,											//  VkDeviceSize	offset;
				bufferSize,									//  VkDeviceSize	size;
			};

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1u, &barrier, 0, (const VkImageMemoryBarrier*)DE_NULL);
		}

		transition2DImage(vk, *cmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
						  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	}
	endCommandBuffer(vk, *cmdBuffer);

	// Wait for completion
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);
	return checkResult (*queryPool);
}

deUint64 VertexShaderMultipleQueryTestInstance::calculateExpectedMin(VkQueryResultFlags flag)
{
	deUint64	expectedMin	= 0u;
	switch (flag)
	{
	case VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT:
		expectedMin = 15u;
		break;

	case VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT:
		expectedMin = 5u;
		break;

	case VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT:
		expectedMin = 15u;
		break;

	case VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT:
		expectedMin = 2016u;
		break;
	default:
		DE_FATAL("Unexpected type of statistics query");
		break;
	}
	return expectedMin;
}

/* This is just to check that driver doesn't return garbage for the partial, no wait case.
 * TODO: adjust the values accordingly, in case some driver returns higher values.
 */
deUint64 VertexShaderMultipleQueryTestInstance::calculateExpectedMax(VkQueryResultFlags flag)
{
	deUint64	expectedMax	= 0u;
	switch (flag)
	{
	case VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT:
		expectedMax = 16u;
		break;

	case VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT:
		expectedMax = 5u;
		break;

	case VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT:
		expectedMax = 15u;
		break;

	case VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT:
		expectedMax = 2304u;
		break;
	default:
		DE_FATAL("Unexpected type of statistics query");
		break;
	}
	return expectedMax;
}

tcu::TestStatus VertexShaderMultipleQueryTestInstance::checkResult (VkQueryPool queryPool)
{
	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	const VkDevice			device				= m_context.getDevice();
	deUint32				queryCount			= (m_parametersGraphic.queryCount + (m_parametersGraphic.dstOffset ? 1u : 0u));
	deUint32				size				= NUM_QUERY_STATISTICS * queryCount;
	std::vector<deUint64>   results;
	results.resize(size);

	deBool					hasPartialFlag		= (deBool)(m_parametersGraphic.queryFlags & VK_QUERY_RESULT_PARTIAL_BIT);
	deBool					hasWaitFlag			= (deBool)(m_parametersGraphic.queryFlags & VK_QUERY_RESULT_WAIT_BIT);
	// Use the last value of each query to store the availability bit for the vertexOnlyPipe case.
	VkQueryResultFlags		queryFlags			= m_parametersGraphic.queryFlags;

	if (m_parametersGraphic.copyType == COPY_TYPE_CMD)
	{
		const vk::Allocation& allocation = m_queryBuffer->getBoundMemory();
		const void* allocationData = allocation.getHostPtr();

		vk::invalidateAlloc(m_context.getDeviceInterface(), m_context.getDevice(), allocation);
		deMemcpy(results.data(), allocationData, size * sizeof(deUint64));
	}
	else
	{
		VkResult result = vk.getQueryPoolResults(device, queryPool, 0u, m_parametersGraphic.queryCount, size * sizeof(deUint64), results.data(), NUM_QUERY_STATISTICS * sizeof(deUint64), queryFlags);

		if (!(result == VK_SUCCESS || (!hasWaitFlag && result == VK_NOT_READY)))
			return tcu::TestStatus::fail("Unexpected getQueryPoolResults() returned value: " + de::toString(getResultStr(result)));
	}

	for (deUint32 queryIdx = 0; queryIdx < queryCount; queryIdx++)
	{
		deInt32					queryMask			= m_parametersGraphic.queryStatisticFlags;
		deUint32				index				= queryIdx * NUM_QUERY_STATISTICS;
		// Last element of each query is the availability value for the vertexOnlyPipe case.
		deBool availableQuery = results[index + (NUM_QUERY_STATISTICS - 1)] != 0u;

		// Check dstOffset values were not overwritten.
		if (m_parametersGraphic.dstOffset != 0u && queryIdx == 0u)
		{
			const deUint64 refVal = 0xfffffffffffffffful;
			for (; index < NUM_QUERY_STATISTICS; index++)
			{
				if (results[index] != refVal)
					return tcu::TestStatus::fail("dstOffset values were overwritten");
			}
			continue;
		}

		if (hasWaitFlag && !hasPartialFlag && !availableQuery)
			return tcu::TestStatus::fail("Results should be available");

		while(queryMask)
		{
			deInt32		statisticBit = deInt32BitScan(&queryMask);
			deUint64	expectedMin	= calculateExpectedMin((1u << statisticBit));
			deUint64	expectedMax	= calculateExpectedMax((1u << statisticBit));

			if (availableQuery && (results[index] < expectedMin))
				return tcu::TestStatus::fail("QueryPoolResults incorrect: wrong value (" + de::toString(results[index]) + ") is lower than expected (" + de::toString(expectedMin) + ")");

			/* From the spec:
			 *
			 *    If VK_QUERY_RESULT_PARTIAL_BIT is set, VK_QUERY_RESULT_WAIT_BIT is not set,
			 *    and the query's status is unavailable, an intermediate result value between zero
			 *    and the final result value is written to pData for that query.
			 */
			if (hasPartialFlag && !hasWaitFlag && !availableQuery && results[index] > expectedMax)
				return tcu::TestStatus::fail("QueryPoolResults incorrect: wrong partial value (" + de::toString(results[index]) + ") is greater than expected (" + de::toString(expectedMax) + ")");

			index++;
		}
	}

	return tcu::TestStatus::pass("Pass");
}

void VertexShaderMultipleQueryTestInstance::draw (VkCommandBuffer cmdBuffer)
{
	const DeviceInterface& vk = m_context.getDeviceInterface();
	vk.cmdDraw(cmdBuffer, 16u, 1u, 0u, 0u);
}

template<class Instance>
class QueryPoolGraphicMultipleQueryStatisticsTest : public TestCase
{
public:
	QueryPoolGraphicMultipleQueryStatisticsTest (tcu::TestContext &context, const std::string& name, const std::string& description, const GraphicBasicMultipleQueryTestInstance::ParametersGraphic parametersGraphic)
		: TestCase				(context, name.c_str(), description.c_str())
		, m_parametersGraphic	(parametersGraphic)
	{
		m_data.push_back(GraphicBasicMultipleQueryTestInstance::VertexData(tcu::Vec4(-1.0f,-1.0f, 1.0f, 1.0f), tcu::RGBA::red().toVec()));
		m_data.push_back(GraphicBasicMultipleQueryTestInstance::VertexData(tcu::Vec4(-1.0f, 0.0f, 1.0f, 1.0f), tcu::RGBA::red().toVec()));
		m_data.push_back(GraphicBasicMultipleQueryTestInstance::VertexData(tcu::Vec4( 0.0f,-1.0f, 1.0f, 1.0f), tcu::RGBA::red().toVec()));
		m_data.push_back(GraphicBasicMultipleQueryTestInstance::VertexData(tcu::Vec4( 0.0f, 0.0f, 1.0f, 1.0f), tcu::RGBA::red().toVec()));

		m_data.push_back(GraphicBasicMultipleQueryTestInstance::VertexData(tcu::Vec4(-1.0f, 0.0f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
		m_data.push_back(GraphicBasicMultipleQueryTestInstance::VertexData(tcu::Vec4(-1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
		m_data.push_back(GraphicBasicMultipleQueryTestInstance::VertexData(tcu::Vec4( 0.0f, 0.0f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
		m_data.push_back(GraphicBasicMultipleQueryTestInstance::VertexData(tcu::Vec4( 0.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));

		m_data.push_back(GraphicBasicMultipleQueryTestInstance::VertexData(tcu::Vec4( 0.0f,-1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));
		m_data.push_back(GraphicBasicMultipleQueryTestInstance::VertexData(tcu::Vec4( 0.0f, 0.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));
		m_data.push_back(GraphicBasicMultipleQueryTestInstance::VertexData(tcu::Vec4( 1.0f,-1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));
		m_data.push_back(GraphicBasicMultipleQueryTestInstance::VertexData(tcu::Vec4( 1.0f, 0.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));

		m_data.push_back(GraphicBasicMultipleQueryTestInstance::VertexData(tcu::Vec4( 0.0f, 0.0f, 1.0f, 1.0f), tcu::RGBA::gray().toVec()));
		m_data.push_back(GraphicBasicMultipleQueryTestInstance::VertexData(tcu::Vec4( 0.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::gray().toVec()));
		m_data.push_back(GraphicBasicMultipleQueryTestInstance::VertexData(tcu::Vec4( 1.0f, 0.0f, 1.0f, 1.0f), tcu::RGBA::gray().toVec()));
		m_data.push_back(GraphicBasicMultipleQueryTestInstance::VertexData(tcu::Vec4( 1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::gray().toVec()));
	}

	vkt::TestInstance* createInstance (vkt::Context& context) const
	{
		return new Instance(context, m_data, m_parametersGraphic);
	}

	void initPrograms(SourceCollections& sourceCollections) const
	{
		{ // Vertex Shader
			std::ostringstream	source;
			source	<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
					<< "layout(location = 0) in highp vec4 in_position;\n"
					<< "layout(location = 1) in vec4 in_color;\n"
					<< "layout(location = 0) out vec4 out_color;\n"
					<< "void main (void)\n"
					<< "{\n"
					<< "	gl_PointSize = 1.0;\n"
					<< "	gl_Position = in_position;\n"
					<< "	out_color = in_color;\n"
					<< "}\n";
			sourceCollections.glslSources.add("vertex") << glu::VertexSource(source.str());
		}

		if (!m_parametersGraphic.vertexOnlyPipe)
		{ // Fragment Shader
			std::ostringstream	source;
			source	<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
					<< "layout(location = 0) in vec4 in_color;\n"
					<< "layout(location = 0) out vec4 out_color;\n"
					<< "void main()\n"
					<<"{\n"
					<< "	out_color = in_color;\n"
					<< "}\n";
			sourceCollections.glslSources.add("fragment") << glu::FragmentSource(source.str());
		}
	}
private:
	std::vector<GraphicBasicMultipleQueryTestInstance::VertexData>	m_data;
	const GraphicBasicMultipleQueryTestInstance::ParametersGraphic	m_parametersGraphic;
};

class BlitBetweenIncompatibleFormatsTestInstance : public StatisticMultipleQueryTestInstance
{
public:
	BlitBetweenIncompatibleFormatsTestInstance	(vkt::Context& context);
protected:
	virtual tcu::TestStatus	iterate				(void);
};

BlitBetweenIncompatibleFormatsTestInstance::BlitBetweenIncompatibleFormatsTestInstance(vkt::Context& context)
	: StatisticMultipleQueryTestInstance(context, 1u)
{
}

tcu::TestStatus BlitBetweenIncompatibleFormatsTestInstance::iterate(void)
{
	const DeviceInterface&			vk					= m_context.getDeviceInterface();
	const VkDevice					device				= m_context.getDevice();
	const VkQueue					queue				= m_context.getUniversalQueue();
	auto&							alloc				= m_context.getDefaultAllocator();
	const deUint32					queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();

	const CmdPoolCreateInfo			cmdPoolCreateInfo	(queueFamilyIndex);
	const Move<VkCommandPool>		cmdPool				(createCommandPool(vk, device, &cmdPoolCreateInfo));
	const Unique<VkQueryPool>		queryPool			(makeQueryPool(vk, device, 1u, VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT));
	const Unique<VkCommandBuffer>	cmdBuffer			(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const VkImageSubresourceLayers	subresourceLayers	{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u };
	const VkImageSubresourceRange	subresourceRange	{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u };
	const VkClearColorValue			clearColor			{ { 0.0f, 1.0f, 0.0f, 1.0f } };
	const VkImageBlit				blitRegion
	{
		subresourceLayers,
		{ { 8,  0, 0}, {16, 16, 1} },
		subresourceLayers,
		{ {0, 8, 0}, {8, 16, 1} }
	};

	VkImageCreateInfo imageCreateInfo
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType		sType;
		DE_NULL,								// const void*			pNext;
		0,										// VkImageCreateFlags	flags;
		VK_IMAGE_TYPE_2D,						// VkImageType			imageType;
		VK_FORMAT_R32G32B32A32_SFLOAT,			// VkFormat				format;
		{ 16u, 16u, 1u },						// VkExtent3D			extent;
		1u,										// deUint32				mipLevels;
		1u,										// deUint32				arraySize;
		VK_SAMPLE_COUNT_1_BIT,					// deUint32				samples;
		VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling		tiling;
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
			VK_IMAGE_USAGE_TRANSFER_DST_BIT,	// VkImageUsageFlags	usage;
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode;
		1u,										// deUint32				queueFamilyIndexCount;
		&queueFamilyIndex,						// const deUint32*		pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				// VkImageLayout		initialLayout;
	};

	de::MovePtr<ImageWithMemory>	srcImage	(new ImageWithMemory(vk, device, alloc, imageCreateInfo, MemoryRequirement::Any));
	imageCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	de::MovePtr<ImageWithMemory>	dstImage	(new ImageWithMemory(vk, device, alloc, imageCreateInfo, MemoryRequirement::Any));

	VkImageMemoryBarrier imageBarriers[2];
	imageBarriers[0] =
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,	// VkStructureType			sType;
		DE_NULL,								// const void*				pNext;
		0,										// VkAccessFlags			srcAccessMask;
		VK_ACCESS_TRANSFER_WRITE_BIT,			// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_UNDEFINED,				// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,	// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,				// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,				// deUint32					dstQueueFamilyIndex;
		**srcImage,								// VkImage					image;
		subresourceRange						// VkImageSubresourceRange	subresourceRange;
	};
	imageBarriers[1] = imageBarriers[0];
	imageBarriers[1].image = **dstImage;

	beginCommandBuffer(vk, *cmdBuffer);

	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 2, imageBarriers);
	vk.cmdClearColorImage(*cmdBuffer, **srcImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &subresourceRange);

	imageBarriers[0].srcAccessMask	= VK_ACCESS_TRANSFER_WRITE_BIT;
	imageBarriers[0].dstAccessMask	= VK_ACCESS_TRANSFER_READ_BIT;
	imageBarriers[0].oldLayout		= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	imageBarriers[0].newLayout		= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, imageBarriers);

	vk.cmdResetQueryPool(*cmdBuffer, *queryPool, 0u, 1u);
	vk.cmdBeginQuery(*cmdBuffer, *queryPool, 0u, (VkQueryControlFlags)0u);
	vk.cmdBlitImage(*cmdBuffer, **srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, **dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blitRegion, VK_FILTER_NEAREST);
	vk.cmdEndQuery(*cmdBuffer, *queryPool, 0u);

	endCommandBuffer(vk, *cmdBuffer);

	// Wait for completion
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	deUint64 queryResult = 1;
	VkResult result = vk.getQueryPoolResults(device, *queryPool, 0u, 1u, sizeof(deUint64), &queryResult, sizeof(deUint64), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
	if (result != VK_SUCCESS)
		return tcu::TestStatus::fail("getQueryPoolResults() returned: " + de::toString(getResultStr(result)));

	if (queryResult == 0)
		return tcu::TestStatus::pass("pass");

	return tcu::TestStatus::fail("QueryPoolResults incorrect result");
}

class BlitBetweenIncompatibleFormatsTestCase : public TestCase
{
public:
	BlitBetweenIncompatibleFormatsTestCase(tcu::TestContext& context, const std::string& name);

	void checkSupport(vkt::Context& context) const;

	vkt::TestInstance* createInstance(vkt::Context& context) const;
};

BlitBetweenIncompatibleFormatsTestCase::BlitBetweenIncompatibleFormatsTestCase(tcu::TestContext& context, const std::string& name)
	: TestCase(context, name.c_str(), "")
{
}

void BlitBetweenIncompatibleFormatsTestCase::checkSupport(vkt::Context& context) const
{
	if (!context.getDeviceFeatures().pipelineStatisticsQuery)
		TCU_THROW(NotSupportedError, "Pipeline statistics queries are not supported");
}

vkt::TestInstance* BlitBetweenIncompatibleFormatsTestCase::createInstance(vkt::Context& context) const
{
	return new BlitBetweenIncompatibleFormatsTestInstance(context);
}

} //anonymous

QueryPoolStatisticsTests::QueryPoolStatisticsTests (tcu::TestContext &testCtx)
	: TestCaseGroup(testCtx, "statistics_query", "Tests for statistics queries")
{
}

inline std::string bitPrefix(deBool query64bits, deBool dstOffset)
{
	std::string prefix = (query64bits ? "64bits_" : "32bits_");
	prefix += (dstOffset ? "dstoffset_" : "");
	return prefix;
}

void QueryPoolStatisticsTests::init (void)
{
	std::string topology_name [VK_PRIMITIVE_TOPOLOGY_LAST] =
	{
		"point_list",
		"line_list",
		"line_strip",
		"triangle_list",
		"triangle_strip",
		"triangle_fan",
		"line_list_with_adjacency",
		"line_strip_with_adjacency",
		"triangle_list_with_adjacency",
		"triangle_strip_with_adjacency",
		"patch_list"
	};

	std::vector<deUint64> sixRepeats								= { 1, 3, 5, 8, 15, 24 };
	de::MovePtr<TestCaseGroup>	computeShaderInvocationsGroup		(new TestCaseGroup(m_testCtx, "compute_shader_invocations",			"Query pipeline statistic compute shader invocations"));
	de::MovePtr<TestCaseGroup>	inputAssemblyVertices				(new TestCaseGroup(m_testCtx, "input_assembly_vertices",			"Query pipeline statistic input assembly vertices"));
	de::MovePtr<TestCaseGroup>	inputAssemblyPrimitives				(new TestCaseGroup(m_testCtx, "input_assembly_primitives",			"Query pipeline statistic input assembly primitives"));
	de::MovePtr<TestCaseGroup>	vertexShaderInvocations				(new TestCaseGroup(m_testCtx, "vertex_shader_invocations",			"Query pipeline statistic vertex shader invocation"));
	de::MovePtr<TestCaseGroup>	fragmentShaderInvocations			(new TestCaseGroup(m_testCtx, "fragment_shader_invocations",		"Query pipeline statistic fragment shader invocation"));
	de::MovePtr<TestCaseGroup>	geometryShaderInvocations			(new TestCaseGroup(m_testCtx, "geometry_shader_invocations",		"Query pipeline statistic geometry shader invocation"));
	de::MovePtr<TestCaseGroup>	geometryShaderPrimitives			(new TestCaseGroup(m_testCtx, "geometry_shader_primitives",			"Query pipeline statistic geometry shader primitives"));
	de::MovePtr<TestCaseGroup>	clippingInvocations					(new TestCaseGroup(m_testCtx, "clipping_invocations",				"Query pipeline statistic clipping invocations"));
	de::MovePtr<TestCaseGroup>	clippingPrimitives					(new TestCaseGroup(m_testCtx, "clipping_primitives",				"Query pipeline statistic clipping primitives"));
	de::MovePtr<TestCaseGroup>	tesControlPatches					(new TestCaseGroup(m_testCtx, "tes_control_patches",				"Query pipeline statistic tessellation control shader patches"));
	de::MovePtr<TestCaseGroup>	tesEvaluationShaderInvocations		(new TestCaseGroup(m_testCtx, "tes_evaluation_shader_invocations",	"Query pipeline statistic tessellation evaluation shader invocations"));

	de::MovePtr<TestCaseGroup>	vertexOnlyGroup									(new TestCaseGroup(m_testCtx, "vertex_only",						"Use only vertex shader in a graphics pipeline"));
	de::MovePtr<TestCaseGroup>	inputAssemblyVerticesVertexOnly					(new TestCaseGroup(m_testCtx, "input_assembly_vertices",			"Query pipeline statistic input assembly vertices"));
	de::MovePtr<TestCaseGroup>	inputAssemblyPrimitivesVertexOnly				(new TestCaseGroup(m_testCtx, "input_assembly_primitives",			"Query pipeline statistic input assembly primitives"));
	de::MovePtr<TestCaseGroup>	vertexShaderInvocationsVertexOnly				(new TestCaseGroup(m_testCtx, "vertex_shader_invocations",			"Query pipeline statistic vertex shader invocation"));

	de::MovePtr<TestCaseGroup>	hostQueryResetGroup								(new TestCaseGroup(m_testCtx, "host_query_reset",					"Check host query reset pipeline statistic compute shader invocations"));
	de::MovePtr<TestCaseGroup>	computeShaderInvocationsGroupHostQueryReset		(new TestCaseGroup(m_testCtx, "compute_shader_invocations",			"Query pipeline statistic compute shader invocations"));
	de::MovePtr<TestCaseGroup>	inputAssemblyVerticesHostQueryReset				(new TestCaseGroup(m_testCtx, "input_assembly_vertices",			"Query pipeline statistic input assembly vertices"));
	de::MovePtr<TestCaseGroup>	inputAssemblyPrimitivesHostQueryReset			(new TestCaseGroup(m_testCtx, "input_assembly_primitives",			"Query pipeline statistic input assembly primitives"));
	de::MovePtr<TestCaseGroup>	vertexShaderInvocationsHostQueryReset			(new TestCaseGroup(m_testCtx, "vertex_shader_invocations",			"Query pipeline statistic vertex shader invocation"));
	de::MovePtr<TestCaseGroup>	fragmentShaderInvocationsHostQueryReset			(new TestCaseGroup(m_testCtx, "fragment_shader_invocations",		"Query pipeline statistic fragment shader invocation"));
	de::MovePtr<TestCaseGroup>	geometryShaderInvocationsHostQueryReset			(new TestCaseGroup(m_testCtx, "geometry_shader_invocations",		"Query pipeline statistic geometry shader invocation"));
	de::MovePtr<TestCaseGroup>	geometryShaderPrimitivesHostQueryReset			(new TestCaseGroup(m_testCtx, "geometry_shader_primitives",			"Query pipeline statistic geometry shader primitives"));
	de::MovePtr<TestCaseGroup>	clippingInvocationsHostQueryReset				(new TestCaseGroup(m_testCtx, "clipping_invocations",				"Query pipeline statistic clipping invocations"));
	de::MovePtr<TestCaseGroup>	clippingPrimitivesHostQueryReset				(new TestCaseGroup(m_testCtx, "clipping_primitives",				"Query pipeline statistic clipping primitives"));
	de::MovePtr<TestCaseGroup>	tesControlPatchesHostQueryReset					(new TestCaseGroup(m_testCtx, "tes_control_patches",				"Query pipeline statistic tessellation control shader patches"));
	de::MovePtr<TestCaseGroup>	tesEvaluationShaderInvocationsHostQueryReset	(new TestCaseGroup(m_testCtx, "tes_evaluation_shader_invocations",	"Query pipeline statistic tessellation evaluation shader invocations"));

	de::MovePtr<TestCaseGroup>	resetBeforeCopyGroup							(new TestCaseGroup(m_testCtx, "reset_before_copy",					"Check pipeline statistic unavailability when resetting before copying"));
	de::MovePtr<TestCaseGroup>	computeShaderInvocationsGroupResetBeforeCopy	(new TestCaseGroup(m_testCtx, "compute_shader_invocations",			"Query pipeline statistic compute shader invocations"));
	de::MovePtr<TestCaseGroup>	inputAssemblyVerticesResetBeforeCopy			(new TestCaseGroup(m_testCtx, "input_assembly_vertices",			"Query pipeline statistic input assembly vertices"));
	de::MovePtr<TestCaseGroup>	inputAssemblyPrimitivesResetBeforeCopy			(new TestCaseGroup(m_testCtx, "input_assembly_primitives",			"Query pipeline statistic input assembly primitives"));
	de::MovePtr<TestCaseGroup>	vertexShaderInvocationsResetBeforeCopy			(new TestCaseGroup(m_testCtx, "vertex_shader_invocations",			"Query pipeline statistic vertex shader invocation"));
	de::MovePtr<TestCaseGroup>	fragmentShaderInvocationsResetBeforeCopy		(new TestCaseGroup(m_testCtx, "fragment_shader_invocations",		"Query pipeline statistic fragment shader invocation"));
	de::MovePtr<TestCaseGroup>	geometryShaderInvocationsResetBeforeCopy		(new TestCaseGroup(m_testCtx, "geometry_shader_invocations",		"Query pipeline statistic geometry shader invocation"));
	de::MovePtr<TestCaseGroup>	geometryShaderPrimitivesResetBeforeCopy			(new TestCaseGroup(m_testCtx, "geometry_shader_primitives",			"Query pipeline statistic geometry shader primitives"));
	de::MovePtr<TestCaseGroup>	clippingInvocationsResetBeforeCopy				(new TestCaseGroup(m_testCtx, "clipping_invocations",				"Query pipeline statistic clipping invocations"));
	de::MovePtr<TestCaseGroup>	clippingPrimitivesResetBeforeCopy				(new TestCaseGroup(m_testCtx, "clipping_primitives",				"Query pipeline statistic clipping primitives"));
	de::MovePtr<TestCaseGroup>	tesControlPatchesResetBeforeCopy				(new TestCaseGroup(m_testCtx, "tes_control_patches",				"Query pipeline statistic tessellation control shader patches"));
	de::MovePtr<TestCaseGroup>	tesEvaluationShaderInvocationsResetBeforeCopy	(new TestCaseGroup(m_testCtx, "tes_evaluation_shader_invocations",	"Query pipeline statistic tessellation evaluation shader invocations"));

	de::MovePtr<TestCaseGroup>	resetAfterCopyGroup								(new TestCaseGroup(m_testCtx, "reset_after_copy",					"Check pipeline statistic are not affected when resetting after copying"));
	de::MovePtr<TestCaseGroup>	computeShaderInvocationsGroupResetAfterCopy		(new TestCaseGroup(m_testCtx, "compute_shader_invocations",			"Query pipeline statistic compute shader invocations"));
	de::MovePtr<TestCaseGroup>	inputAssemblyVerticesResetAfterCopy				(new TestCaseGroup(m_testCtx, "input_assembly_vertices",			"Query pipeline statistic input assembly vertices"));
	de::MovePtr<TestCaseGroup>	inputAssemblyPrimitivesResetAfterCopy			(new TestCaseGroup(m_testCtx, "input_assembly_primitives",			"Query pipeline statistic input assembly primitives"));
	de::MovePtr<TestCaseGroup>	vertexShaderInvocationsResetAfterCopy			(new TestCaseGroup(m_testCtx, "vertex_shader_invocations",			"Query pipeline statistic vertex shader invocation"));
	de::MovePtr<TestCaseGroup>	fragmentShaderInvocationsResetAfterCopy			(new TestCaseGroup(m_testCtx, "fragment_shader_invocations",		"Query pipeline statistic fragment shader invocation"));
	de::MovePtr<TestCaseGroup>	geometryShaderInvocationsResetAfterCopy			(new TestCaseGroup(m_testCtx, "geometry_shader_invocations",		"Query pipeline statistic geometry shader invocation"));
	de::MovePtr<TestCaseGroup>	geometryShaderPrimitivesResetAfterCopy			(new TestCaseGroup(m_testCtx, "geometry_shader_primitives",			"Query pipeline statistic geometry shader primitives"));
	de::MovePtr<TestCaseGroup>	clippingInvocationsResetAfterCopy				(new TestCaseGroup(m_testCtx, "clipping_invocations",				"Query pipeline statistic clipping invocations"));
	de::MovePtr<TestCaseGroup>	clippingPrimitivesResetAfterCopy				(new TestCaseGroup(m_testCtx, "clipping_primitives",				"Query pipeline statistic clipping primitives"));
	de::MovePtr<TestCaseGroup>	tesControlPatchesResetAfterCopy					(new TestCaseGroup(m_testCtx, "tes_control_patches",				"Query pipeline statistic tessellation control shader patches"));
	de::MovePtr<TestCaseGroup>	tesEvaluationShaderInvocationsResetAfterCopy	(new TestCaseGroup(m_testCtx, "tes_evaluation_shader_invocations",	"Query pipeline statistic tessellation evaluation shader invocations"));

	de::MovePtr<TestCaseGroup>	vertexShaderMultipleQueries						(new TestCaseGroup(m_testCtx, "multiple_queries",					"Query pipeline statistics related to vertex and fragment shaders"));
	de::MovePtr<TestCaseGroup>	gsInvocationsNoGs								(new TestCaseGroup(m_testCtx, "gs_invocations_no_gs",				"Query pipeline statistics GS invocations with no GS"));

	CopyType copyType[]															= { COPY_TYPE_GET,	COPY_TYPE_CMD };
	std::string copyTypeStr[]													= { "",			"cmdcopyquerypoolresults_" };

	StrideType	strideType[]													= { STRIDE_TYPE_VALID, STRIDE_TYPE_ZERO };
	std::string	strideTypeStr[]													= { "",			"stride_zero_" };

	for (deUint32 copyTypeIdx = 0; copyTypeIdx < DE_LENGTH_OF_ARRAY(copyType); copyTypeIdx++)
	{
		for (deUint32 i = 0; i < 4; ++i)
		{
			deBool query64Bits = (i & 1);
			deBool dstOffset = (i & 2);
			std::string prefix = bitPrefix(query64Bits, dstOffset);

			// It makes no sense to use dstOffset with vkGetQueryPoolResults()
			if (copyType[copyTypeIdx] == COPY_TYPE_GET && dstOffset)
				continue;

			//VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT

			for (const auto computeQueue : { false, true })
			{
				const std::string cqSuffix = (computeQueue ? "_cq" : "");

				for (deUint32 strideTypeIdx = 0; strideTypeIdx < DE_LENGTH_OF_ARRAY(strideType); strideTypeIdx++)
				{
					if (strideType[strideTypeIdx] == STRIDE_TYPE_ZERO && copyType[copyTypeIdx] != COPY_TYPE_CMD)
						continue;

					computeShaderInvocationsGroup->addChild(new QueryPoolComputeStatsTest<ComputeInvocationsTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + strideTypeStr[strideTypeIdx] + "primary" + cqSuffix,				"", RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, computeQueue, dstOffset, strideType[strideTypeIdx]));
					computeShaderInvocationsGroup->addChild(new QueryPoolComputeStatsTest<ComputeInvocationsSecondaryTestInstance>			(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + strideTypeStr[strideTypeIdx] + "secondary" + cqSuffix,				"", RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, computeQueue, dstOffset, strideType[strideTypeIdx]));
					computeShaderInvocationsGroup->addChild(new QueryPoolComputeStatsTest<ComputeInvocationsSecondaryInheritedTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + strideTypeStr[strideTypeIdx] + "secondary_inherited" + cqSuffix,	"", RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, computeQueue, dstOffset, strideType[strideTypeIdx]));
				}

				computeShaderInvocationsGroupHostQueryReset->addChild(new QueryPoolComputeStatsTest<ComputeInvocationsTestInstance>						(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "primary" + cqSuffix,				"", RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits, computeQueue, dstOffset));
				computeShaderInvocationsGroupHostQueryReset->addChild(new QueryPoolComputeStatsTest<ComputeInvocationsSecondaryTestInstance>			(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "secondary" + cqSuffix,				"", RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits, computeQueue, dstOffset));
				computeShaderInvocationsGroupHostQueryReset->addChild(new QueryPoolComputeStatsTest<ComputeInvocationsSecondaryInheritedTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "secondary_inherited" + cqSuffix,	"", RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits, computeQueue, dstOffset));

				computeShaderInvocationsGroupResetBeforeCopy->addChild(new QueryPoolComputeStatsTest<ComputeInvocationsTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "primary" + cqSuffix,				"", RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx], query64Bits, computeQueue, dstOffset));
				computeShaderInvocationsGroupResetBeforeCopy->addChild(new QueryPoolComputeStatsTest<ComputeInvocationsSecondaryTestInstance>			(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "secondary" + cqSuffix,				"", RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx], query64Bits, computeQueue, dstOffset));
				computeShaderInvocationsGroupResetBeforeCopy->addChild(new QueryPoolComputeStatsTest<ComputeInvocationsSecondaryInheritedTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "secondary_inherited" + cqSuffix,	"", RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx], query64Bits, computeQueue, dstOffset));

				if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
				{
					computeShaderInvocationsGroupResetAfterCopy->addChild(new QueryPoolComputeStatsTest<ComputeInvocationsTestInstance>						(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "primary" + cqSuffix,				"", RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx], query64Bits, computeQueue, dstOffset));
					computeShaderInvocationsGroupResetAfterCopy->addChild(new QueryPoolComputeStatsTest<ComputeInvocationsSecondaryTestInstance>			(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "secondary" + cqSuffix,				"", RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx], query64Bits, computeQueue, dstOffset));
					computeShaderInvocationsGroupResetAfterCopy->addChild(new QueryPoolComputeStatsTest<ComputeInvocationsSecondaryInheritedTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "secondary_inherited" + cqSuffix,	"", RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx], query64Bits, computeQueue, dstOffset));
				}
			}

			//VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT

			// Tests with no attachments for only primary command to reduce # of test cases.
			inputAssemblyVertices->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>								(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "primary_with_no_color_attachments",				"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_TRUE), sixRepeats));
			inputAssemblyVerticesVertexOnly->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>						(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "primary_with_no_color_attachments",				"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_TRUE), sixRepeats));
			inputAssemblyVerticesHostQueryReset->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "primary_with_no_color_attachments",				"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_TRUE), sixRepeats));
			inputAssemblyVerticesResetBeforeCopy->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "primary_with_no_color_attachments",				"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_TRUE), sixRepeats));

			if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
				inputAssemblyVerticesResetAfterCopy->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>				(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "primary_with_no_color_attachments",				"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_TRUE), sixRepeats));

			/* Tests for clear operation within a statistics query activated.
			 * The query shouldn't count internal driver operations relevant to the clear operations.
			 */
			const ClearOperation clearOp[] =	{ CLEAR_NOOP, CLEAR_COLOR, CLEAR_DEPTH };
			const char* const	 clearOpStr[] = { "", "_clear_color", "_clear_depth" };

			for (int clearOpIdx = 0; clearOpIdx < DE_LENGTH_OF_ARRAY(clearOp); ++clearOpIdx) {
				inputAssemblyVertices->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>							(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "primary" + clearOpStr[clearOpIdx],				"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));
				inputAssemblyVertices->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "secondary" + clearOpStr[clearOpIdx],			"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));

				inputAssemblyVerticesVertexOnly->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "primary" + clearOpStr[clearOpIdx],				"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_TRUE, dstOffset, clearOp[clearOpIdx]), sixRepeats));
				inputAssemblyVerticesVertexOnly->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>			(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "secondary" + clearOpStr[clearOpIdx],			"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_TRUE, dstOffset, clearOp[clearOpIdx]), sixRepeats));

				inputAssemblyVerticesHostQueryReset->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>				(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "primary" + clearOpStr[clearOpIdx],				"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));
				inputAssemblyVerticesHostQueryReset->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>		(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "secondary" + clearOpStr[clearOpIdx],			"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));

				inputAssemblyVerticesResetBeforeCopy->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>				(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "primary" + clearOpStr[clearOpIdx],				"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));
				inputAssemblyVerticesResetBeforeCopy->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "secondary" + clearOpStr[clearOpIdx],			"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));

				if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
				{
					inputAssemblyVerticesResetAfterCopy->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>			(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "primary" + clearOpStr[clearOpIdx],				"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));
					inputAssemblyVerticesResetAfterCopy->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "secondary" + clearOpStr[clearOpIdx],			"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));
				}
			}

			inputAssemblyVertices->addChild						(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "secondary_inherited",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset), sixRepeats));

			inputAssemblyVerticesVertexOnly->addChild			(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "secondary_inherited",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_TRUE, dstOffset), sixRepeats));

			inputAssemblyVerticesHostQueryReset->addChild		(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "secondary_inherited",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset), sixRepeats));

			inputAssemblyVerticesResetBeforeCopy->addChild		(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "secondary_inherited",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset), sixRepeats));

			if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
				inputAssemblyVerticesResetAfterCopy->addChild	(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "secondary_inherited",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset), sixRepeats));
		}
	}

	//VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT
	{
		de::MovePtr<TestCaseGroup>	primary				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondary			(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInherited	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		de::MovePtr<TestCaseGroup>	primaryVertexOnly				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondaryVertexOnly				(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInheritedVertexOnly	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		de::MovePtr<TestCaseGroup>	primaryHostQueryReset				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondaryHostQueryReset				(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInheritedHostQueryReset	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		de::MovePtr<TestCaseGroup>	primaryResetBeforeCopy				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondaryResetBeforeCopy			(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInheritedResetBeforeCopy	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		de::MovePtr<TestCaseGroup>	primaryResetAfterCopy				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondaryResetAfterCopy				(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInheritedResetAfterCopy	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		for (deUint32 copyTypeIdx = 0; copyTypeIdx < DE_LENGTH_OF_ARRAY(copyType); copyTypeIdx++)
		{
			for (int topologyNdx = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; topologyNdx < VK_PRIMITIVE_TOPOLOGY_PATCH_LIST; ++topologyNdx)
			{
				for (deUint32 i = 0; i < 4; ++i)
				{
					deBool query64Bits = (i & 1);
					deBool dstOffset = (i & 2);
					std::string prefix = bitPrefix(query64Bits, dstOffset);

					// It makes no sense to use dstOffset with vkGetQueryPoolResults()
					if (copyType[copyTypeIdx] == COPY_TYPE_GET && dstOffset)
						continue;

					// Tests with no attachments for only primary command to reduce # of test cases.
					primary->addChild								(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>						(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "_with_no_color_attachments",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_TRUE), sixRepeats));

					primaryHostQueryReset->addChild					(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>						(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "_with_no_color_attachments",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_TRUE), sixRepeats));

					primaryVertexOnly->addChild						(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>						(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "_with_no_color_attachments",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_TRUE), sixRepeats));

					primaryResetBeforeCopy->addChild				(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>						(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "_with_no_color_attachments",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_TRUE), sixRepeats));

					if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
						primaryResetAfterCopy->addChild				(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>						(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "_with_no_color_attachments",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_TRUE), sixRepeats));

					/* Tests for clear operation within a statistics query activated.
					 * Nothing for secondary_inherited cases can be done since it violates the specification.
					 *
					 * The query shouldn't count internal driver operations relevant to the clear operations.
					 */
					const ClearOperation clearOp[] =	{ CLEAR_NOOP, CLEAR_COLOR, CLEAR_DEPTH };
					const char* const	 clearOpStr[] = { "", "_clear_color", "_clear_depth" };

					for (int clearOpIdx = 0; clearOpIdx < DE_LENGTH_OF_ARRAY(clearOp); ++clearOpIdx) {
						primary->addChild			(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>									(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));
						secondary->addChild			(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>							(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));

						primaryHostQueryReset->addChild				(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));
						secondaryHostQueryReset->addChild			(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>			(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));

						primaryVertexOnly->addChild					(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_TRUE, dstOffset, clearOp[clearOpIdx]), sixRepeats));
						secondaryVertexOnly->addChild				(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>			(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_TRUE, dstOffset, clearOp[clearOpIdx]), sixRepeats));

						primaryResetBeforeCopy->addChild			(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));
						secondaryResetBeforeCopy->addChild			(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>			(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));

						if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
						{
							primaryResetAfterCopy->addChild				(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>				(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));
							secondaryResetAfterCopy->addChild			(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>		(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));
						}
					}

					secondaryInherited->addChild					(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset), sixRepeats));
					secondaryInheritedHostQueryReset->addChild		(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset), sixRepeats));
					secondaryInheritedVertexOnly->addChild			(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_TRUE, dstOffset), sixRepeats));
					secondaryInheritedResetBeforeCopy->addChild		(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset), sixRepeats));
					if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
						secondaryInheritedResetAfterCopy->addChild	(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset), sixRepeats));
				}
			}
		}

		inputAssemblyPrimitives->addChild(primary.release());
		inputAssemblyPrimitives->addChild(secondary.release());
		inputAssemblyPrimitives->addChild(secondaryInherited.release());

		inputAssemblyPrimitivesVertexOnly->addChild(primaryVertexOnly.release());
		inputAssemblyPrimitivesVertexOnly->addChild(secondaryVertexOnly.release());
		inputAssemblyPrimitivesVertexOnly->addChild(secondaryInheritedVertexOnly.release());

		inputAssemblyPrimitivesHostQueryReset->addChild(primaryHostQueryReset.release());
		inputAssemblyPrimitivesHostQueryReset->addChild(secondaryHostQueryReset.release());
		inputAssemblyPrimitivesHostQueryReset->addChild(secondaryInheritedHostQueryReset.release());

		inputAssemblyPrimitivesResetBeforeCopy->addChild(primaryResetBeforeCopy.release());
		inputAssemblyPrimitivesResetBeforeCopy->addChild(secondaryResetBeforeCopy.release());
		inputAssemblyPrimitivesResetBeforeCopy->addChild(secondaryInheritedResetBeforeCopy.release());

		inputAssemblyPrimitivesResetAfterCopy->addChild(primaryResetAfterCopy.release());
		inputAssemblyPrimitivesResetAfterCopy->addChild(secondaryResetAfterCopy.release());
		inputAssemblyPrimitivesResetAfterCopy->addChild(secondaryInheritedResetAfterCopy.release());
	}

	//VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT
	{
		de::MovePtr<TestCaseGroup>	primary				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondary			(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInherited	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		de::MovePtr<TestCaseGroup>	primaryVertexOnly				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondaryVertexOnly				(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInheritedVertexOnly	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		de::MovePtr<TestCaseGroup>	primaryHostQueryReset				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondaryHostQueryReset				(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInheritedHostQueryReset	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		de::MovePtr<TestCaseGroup>	primaryResetBeforeCopy				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondaryResetBeforeCopy			(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInheritedResetBeforeCopy	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		de::MovePtr<TestCaseGroup>	primaryResetAfterCopy				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondaryResetAfterCopy				(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInheritedResetAfterCopy	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		for (deUint32 copyTypeIdx = 0; copyTypeIdx < DE_LENGTH_OF_ARRAY(copyType); copyTypeIdx++)
		{
			for (int topologyNdx = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; topologyNdx < VK_PRIMITIVE_TOPOLOGY_PATCH_LIST; ++topologyNdx)
			{
				for (deUint32 i = 0; i < 4; ++i)
				{
					deBool query64Bits = (i & 1);
					deBool dstOffset = (i & 2);
					std::string prefix = bitPrefix(query64Bits, dstOffset);

					// It makes no sense to use dstOffset with vkGetQueryPoolResults()
					if (copyType[copyTypeIdx] == COPY_TYPE_GET && dstOffset)
						continue;

					// Tests with no attachments for only primary command to reduce # of test cases.
					primary->addChild						(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "with_no_color_attachments",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_TRUE), sixRepeats));

					primaryVertexOnly->addChild				(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "with_no_color_attachments",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_TRUE), sixRepeats));

					primaryHostQueryReset->addChild			(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "with_no_color_attachments",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_TRUE), sixRepeats));

					primaryResetBeforeCopy->addChild		(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "with_no_color_attachments",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_TRUE), sixRepeats));

					if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
						primaryResetAfterCopy->addChild		(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "with_no_color_attachments",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_TRUE), sixRepeats));

					/* Tests for clear operation within a statistics query activated.
					 * Nothing for secondary_inherited cases can be done since it violates the specification.
					 *
					 * The query shouldn't count internal driver operations relevant to the clear operations.
					 */
					const ClearOperation clearOp[] =	{ CLEAR_NOOP, CLEAR_COLOR, CLEAR_DEPTH };
					const char* const	 clearOpStr[] = { "", "_clear_color", "_clear_depth" };

					for (int clearOpIdx = 0; clearOpIdx < DE_LENGTH_OF_ARRAY(clearOp); ++clearOpIdx) {
						primary->addChild						(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));
						secondary->addChild						(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>			(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));

						primaryVertexOnly->addChild				(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_TRUE, dstOffset, clearOp[clearOpIdx]), sixRepeats));
						secondaryVertexOnly->addChild			(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>			(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_TRUE, dstOffset, clearOp[clearOpIdx]), sixRepeats));

						primaryHostQueryReset->addChild			(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));
						secondaryHostQueryReset->addChild		(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>			(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));

						primaryResetBeforeCopy->addChild		(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));
						secondaryResetBeforeCopy->addChild		(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>			(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));

						if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
						{
							primaryResetAfterCopy->addChild		(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));
							secondaryResetAfterCopy->addChild	(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>			(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));
						}
					}

					secondaryInherited->addChild					(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset), sixRepeats));
					secondaryInheritedVertexOnly->addChild			(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_TRUE, dstOffset), sixRepeats));
					secondaryInheritedHostQueryReset->addChild		(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset), sixRepeats));
					secondaryInheritedResetBeforeCopy->addChild		(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset), sixRepeats));
					if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
						secondaryInheritedResetAfterCopy->addChild	(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset), sixRepeats));
				}
			}
		}

		vertexShaderInvocations->addChild(primary.release());
		vertexShaderInvocations->addChild(secondary.release());
		vertexShaderInvocations->addChild(secondaryInherited.release());

		vertexShaderInvocationsVertexOnly->addChild(primaryVertexOnly.release());
		vertexShaderInvocationsVertexOnly->addChild(secondaryVertexOnly.release());
		vertexShaderInvocationsVertexOnly->addChild(secondaryInheritedVertexOnly.release());

		vertexShaderInvocationsHostQueryReset->addChild(primaryHostQueryReset.release());
		vertexShaderInvocationsHostQueryReset->addChild(secondaryHostQueryReset.release());
		vertexShaderInvocationsHostQueryReset->addChild(secondaryInheritedHostQueryReset.release());

		vertexShaderInvocationsResetBeforeCopy->addChild(primaryResetBeforeCopy.release());
		vertexShaderInvocationsResetBeforeCopy->addChild(secondaryResetBeforeCopy.release());
		vertexShaderInvocationsResetBeforeCopy->addChild(secondaryInheritedResetBeforeCopy.release());

		vertexShaderInvocationsResetAfterCopy->addChild(primaryResetAfterCopy.release());
		vertexShaderInvocationsResetAfterCopy->addChild(secondaryResetAfterCopy.release());
		vertexShaderInvocationsResetAfterCopy->addChild(secondaryInheritedResetAfterCopy.release());
	}

	//VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT
	{
		de::MovePtr<TestCaseGroup>	primary				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondary			(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInherited	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		de::MovePtr<TestCaseGroup>	primaryHostQueryReset				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondaryHostQueryReset				(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInheritedHostQueryReset	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		de::MovePtr<TestCaseGroup>	primaryResetBeforeCopy				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondaryResetBeforeCopy			(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInheritedResetBeforeCopy	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		de::MovePtr<TestCaseGroup>	primaryResetAfterCopy				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondaryResetAfterCopy				(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInheritedResetAfterCopy	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		for (deUint32 copyTypeIdx = 0; copyTypeIdx < DE_LENGTH_OF_ARRAY(copyType); copyTypeIdx++)
		{
			for (int topologyNdx = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; topologyNdx < VK_PRIMITIVE_TOPOLOGY_PATCH_LIST; ++topologyNdx)
			{
				for (deUint32 i = 0; i < 4; ++i)
				{
					deBool query64Bits = (i & 1);
					deBool dstOffset = (i & 2);
					std::string prefix = bitPrefix(query64Bits, dstOffset);

					// It makes no sense to use dstOffset with vkGetQueryPoolResults()
					if (copyType[copyTypeIdx] == COPY_TYPE_GET && dstOffset)
						continue;

					// Tests with no attachments for only primary command to reduce # of test cases.
					primary->addChild					(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "with_no_color_attachments",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_TRUE), sixRepeats));

					primaryHostQueryReset->addChild		(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "with_no_color_attachments",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_TRUE), sixRepeats));

					primaryResetBeforeCopy->addChild	(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "with_no_color_attachments",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_TRUE), sixRepeats));

					if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
						primaryResetAfterCopy->addChild	(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "with_no_color_attachments",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_TRUE), sixRepeats));

					/* Tests for clear operation within a statistics query activated.
					 * Nothing for secondary_inherited cases can be done since it violates the specification.
					 *
					 * The query shouldn't count internal driver operations relevant to the clear operations.
					 */
					const ClearOperation clearOp[] =	{ CLEAR_NOOP, CLEAR_COLOR, CLEAR_DEPTH };
					const char* const	 clearOpStr[] = { "", "_clear_color", "_clear_depth" };

					for (int clearOpIdx = 0; clearOpIdx < DE_LENGTH_OF_ARRAY(clearOp); ++clearOpIdx) {
						primary->addChild					(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));
						secondary->addChild					(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>			(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset), sixRepeats));

						primaryHostQueryReset->addChild		(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));
						secondaryHostQueryReset->addChild	(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>			(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));

						primaryResetBeforeCopy->addChild	(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));
						secondaryResetBeforeCopy->addChild	(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>			(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));

						if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
						{
							primaryResetAfterCopy->addChild	(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));
							secondaryResetAfterCopy->addChild(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryTestInstance>			(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));
						}
					}

					secondaryInherited->addChild					(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset), sixRepeats));
					secondaryInheritedHostQueryReset->addChild		(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset), sixRepeats));
					secondaryInheritedResetBeforeCopy->addChild		(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset), sixRepeats));
					if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
						secondaryInheritedResetAfterCopy->addChild	(new QueryPoolGraphicStatisticsTest<VertexShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset), sixRepeats));
				}
			}
		}

		fragmentShaderInvocations->addChild(primary.release());
		fragmentShaderInvocations->addChild(secondary.release());
		fragmentShaderInvocations->addChild(secondaryInherited.release());

		fragmentShaderInvocationsHostQueryReset->addChild(primaryHostQueryReset.release());
		fragmentShaderInvocationsHostQueryReset->addChild(secondaryHostQueryReset.release());
		fragmentShaderInvocationsHostQueryReset->addChild(secondaryInheritedHostQueryReset.release());

		fragmentShaderInvocationsResetBeforeCopy->addChild(primaryResetBeforeCopy.release());
		fragmentShaderInvocationsResetBeforeCopy->addChild(secondaryResetBeforeCopy.release());
		fragmentShaderInvocationsResetBeforeCopy->addChild(secondaryInheritedResetBeforeCopy.release());

		fragmentShaderInvocationsResetAfterCopy->addChild(primaryResetAfterCopy.release());
		fragmentShaderInvocationsResetAfterCopy->addChild(secondaryResetAfterCopy.release());
		fragmentShaderInvocationsResetAfterCopy->addChild(secondaryInheritedResetAfterCopy.release());
	}

	//VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT
	{
		de::MovePtr<TestCaseGroup>	primary				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondary			(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInherited	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		de::MovePtr<TestCaseGroup>	primaryHostQueryReset				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondaryHostQueryReset				(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInheritedHostQueryReset	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		de::MovePtr<TestCaseGroup>	primaryResetBeforeCopy				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondaryResetBeforeCopy			(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInheritedResetBeforeCopy	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		de::MovePtr<TestCaseGroup>	primaryResetAfterCopy				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondaryResetAfterCopy				(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInheritedResetAfterCopy	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		for (deUint32 copyTypeIdx = 0; copyTypeIdx < DE_LENGTH_OF_ARRAY(copyType); copyTypeIdx++)
		{
			for (int topologyNdx = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; topologyNdx < VK_PRIMITIVE_TOPOLOGY_PATCH_LIST; ++topologyNdx)
			{
				for (deUint32 i = 0; i < 4; ++i)
				{
					deBool query64Bits = (i & 1);
					deBool dstOffset = (i & 2);
					std::string prefix = bitPrefix(query64Bits, dstOffset);

					// It makes no sense to use dstOffset with vkGetQueryPoolResults()
					if (copyType[copyTypeIdx] == COPY_TYPE_GET && dstOffset)
						continue;

					// Tests with no attachments for only primary command to reduce # of test cases.
					primary->addChild					(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "with_no_color_attachments",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_TRUE), sixRepeats));

					primaryHostQueryReset->addChild		(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "with_no_color_attachments",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_TRUE), sixRepeats));

					primaryResetBeforeCopy->addChild	(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "with_no_color_attachments",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_TRUE), sixRepeats));

					if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
						primaryResetAfterCopy->addChild	(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "with_no_color_attachments",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_TRUE), sixRepeats));

					/* Tests for clear operation within a statistics query activated.
					 * Nothing for secondary_inherited cases can be done since it violates the specification.
					 *
					 * The query shouldn't count internal driver operations relevant to the clear operations.
					 */
					const ClearOperation clearOp[] =	{ CLEAR_NOOP, CLEAR_COLOR, CLEAR_DEPTH };
					const char* const	 clearOpStr[] = { "", "_clear_color", "_clear_depth" };

					for (int clearOpIdx = 0; clearOpIdx < DE_LENGTH_OF_ARRAY(clearOp); ++clearOpIdx) {
						primary->addChild						(new QueryPoolGraphicStatisticsTest<GeometryShaderTestInstance>						(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));
						secondary->addChild						(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryTestInstance>			(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));

						primaryHostQueryReset->addChild			(new QueryPoolGraphicStatisticsTest<GeometryShaderTestInstance>						(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));
						secondaryHostQueryReset->addChild		(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryTestInstance>			(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));

						primaryResetBeforeCopy->addChild		(new QueryPoolGraphicStatisticsTest<GeometryShaderTestInstance>						(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));
						secondaryResetBeforeCopy->addChild		(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryTestInstance>			(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));

						if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
						{
							primaryResetAfterCopy->addChild		(new QueryPoolGraphicStatisticsTest<GeometryShaderTestInstance>						(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));
							secondaryResetAfterCopy->addChild	(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryTestInstance>			(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));
						}
					}

					secondaryInherited->addChild					(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset), sixRepeats));
					secondaryInheritedHostQueryReset->addChild		(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset), sixRepeats));
					secondaryInheritedResetBeforeCopy->addChild		(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset), sixRepeats));
					if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
						secondaryInheritedResetAfterCopy->addChild	(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset), sixRepeats));
				}
			}
		}

		geometryShaderInvocations->addChild(primary.release());
		geometryShaderInvocations->addChild(secondary.release());
		geometryShaderInvocations->addChild(secondaryInherited.release());

		geometryShaderInvocationsHostQueryReset->addChild(primaryHostQueryReset.release());
		geometryShaderInvocationsHostQueryReset->addChild(secondaryHostQueryReset.release());
		geometryShaderInvocationsHostQueryReset->addChild(secondaryInheritedHostQueryReset.release());

		geometryShaderInvocationsResetBeforeCopy->addChild(primaryResetBeforeCopy.release());
		geometryShaderInvocationsResetBeforeCopy->addChild(secondaryResetBeforeCopy.release());
		geometryShaderInvocationsResetBeforeCopy->addChild(secondaryInheritedResetBeforeCopy.release());

		geometryShaderInvocationsResetAfterCopy->addChild(primaryResetAfterCopy.release());
		geometryShaderInvocationsResetAfterCopy->addChild(secondaryResetAfterCopy.release());
		geometryShaderInvocationsResetAfterCopy->addChild(secondaryInheritedResetAfterCopy.release());
	}

	//VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT
	{
		de::MovePtr<TestCaseGroup>	primary				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondary			(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInherited	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		de::MovePtr<TestCaseGroup>	primaryHostQueryReset				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondaryHostQueryReset				(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInheritedHostQueryReset	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		de::MovePtr<TestCaseGroup>	primaryResetBeforeCopy				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondaryResetBeforeCopy			(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInheritedResetBeforeCopy	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		de::MovePtr<TestCaseGroup>	primaryResetAfterCopy				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondaryResetAfterCopy				(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInheritedResetAfterCopy	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		for (deUint32 copyTypeIdx = 0; copyTypeIdx < DE_LENGTH_OF_ARRAY(copyType); copyTypeIdx++)
		{
			for (int topologyNdx = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; topologyNdx < VK_PRIMITIVE_TOPOLOGY_PATCH_LIST; ++topologyNdx)
			{
				for (deUint32 i = 0; i < 4; ++i)
				{
					deBool query64Bits = (i & 1);
					deBool dstOffset = (i & 2);
					std::string prefix = bitPrefix(query64Bits, dstOffset);

					// It makes no sense to use dstOffset with vkGetQueryPoolResults()
					if (copyType[copyTypeIdx] == COPY_TYPE_GET && dstOffset)
						continue;

					// Tests with no attachments.
					primary->addChild						(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "with_no_color_attachments",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_TRUE), sixRepeats));

					primaryHostQueryReset->addChild			(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "with_no_color_attachments",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_TRUE), sixRepeats));

					primaryResetBeforeCopy->addChild		(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "with_no_color_attachments",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_TRUE), sixRepeats));

					if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
						primaryResetAfterCopy->addChild		(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "with_no_color_attachments",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_TRUE), sixRepeats));

					/* Tests for clear operation within a statistics query activated.
					 * Nothing for secondary_inherited cases can be done since it violates the specification.
					 *
					 * The query shouldn't count internal driver operations relevant to the clear operations.
					 */
					const ClearOperation clearOp[] =	{ CLEAR_NOOP, CLEAR_COLOR, CLEAR_DEPTH };
					const char* const	 clearOpStr[] = { "", "_clear_color", "_clear_depth" };

					for (int clearOpIdx = 0; clearOpIdx < DE_LENGTH_OF_ARRAY(clearOp); ++clearOpIdx) {
						primary->addChild						(new QueryPoolGraphicStatisticsTest<GeometryShaderTestInstance>						(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));
						secondary->addChild						(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryTestInstance>			(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));

						primaryHostQueryReset->addChild			(new QueryPoolGraphicStatisticsTest<GeometryShaderTestInstance>						(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));
						secondaryHostQueryReset->addChild		(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryTestInstance>			(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));

						primaryResetBeforeCopy->addChild		(new QueryPoolGraphicStatisticsTest<GeometryShaderTestInstance>						(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));
						secondaryResetBeforeCopy->addChild		(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryTestInstance>			(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));

						if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
						{
							primaryResetAfterCopy->addChild		(new QueryPoolGraphicStatisticsTest<GeometryShaderTestInstance>						(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));
							secondaryResetAfterCopy->addChild	(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryTestInstance>			(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));
						}
					}
					secondaryInherited->addChild					(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset), sixRepeats));
					secondaryInheritedHostQueryReset->addChild		(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset), sixRepeats));
					secondaryInheritedResetBeforeCopy->addChild		(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset), sixRepeats));
					if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
						secondaryInheritedResetAfterCopy->addChild	(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset), sixRepeats));
				}
			}
		}

		geometryShaderPrimitives->addChild(primary.release());
		geometryShaderPrimitives->addChild(secondary.release());
		geometryShaderPrimitives->addChild(secondaryInherited.release());

		geometryShaderPrimitivesHostQueryReset->addChild(primaryHostQueryReset.release());
		geometryShaderPrimitivesHostQueryReset->addChild(secondaryHostQueryReset.release());
		geometryShaderPrimitivesHostQueryReset->addChild(secondaryInheritedHostQueryReset.release());

		geometryShaderPrimitivesResetBeforeCopy->addChild(primaryResetBeforeCopy.release());
		geometryShaderPrimitivesResetBeforeCopy->addChild(secondaryResetBeforeCopy.release());
		geometryShaderPrimitivesResetBeforeCopy->addChild(secondaryInheritedResetBeforeCopy.release());

		geometryShaderPrimitivesResetAfterCopy->addChild(primaryResetAfterCopy.release());
		geometryShaderPrimitivesResetAfterCopy->addChild(secondaryResetAfterCopy.release());
		geometryShaderPrimitivesResetAfterCopy->addChild(secondaryInheritedResetAfterCopy.release());
	}

	//VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT
	{
		de::MovePtr<TestCaseGroup>	primary				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondary			(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInherited	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		de::MovePtr<TestCaseGroup>	primaryHostQueryReset				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondaryHostQueryReset				(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInheritedHostQueryReset	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		de::MovePtr<TestCaseGroup>	primaryResetBeforeCopy				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondaryResetBeforeCopy			(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInheritedResetBeforeCopy	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		de::MovePtr<TestCaseGroup>	primaryResetAfterCopy				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondaryResetAfterCopy				(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInheritedResetAfterCopy	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		for (deUint32 copyTypeIdx = 0; copyTypeIdx < DE_LENGTH_OF_ARRAY(copyType); copyTypeIdx++)
		{
			for (int topologyNdx = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; topologyNdx < VK_PRIMITIVE_TOPOLOGY_PATCH_LIST; ++topologyNdx)
			{
				for (deUint32 i = 0; i < 4; ++i)
				{
					deBool query64Bits = (i & 1);
					deBool dstOffset = (i & 2);
					std::string prefix = bitPrefix(query64Bits, dstOffset);

					// It makes no sense to use dstOffset with vkGetQueryPoolResults()
					if (copyType[copyTypeIdx] == COPY_TYPE_GET && dstOffset)
						continue;

					// Tests with no attachments for only primary command to reduce # of test cases.
					primary->addChild					(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "with_no_color_attachments",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_TRUE), sixRepeats));

					primaryHostQueryReset->addChild		(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "with_no_color_attachments",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_TRUE), sixRepeats));

					primaryResetBeforeCopy->addChild	(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "with_no_color_attachments",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_TRUE), sixRepeats));

					if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
						primaryResetAfterCopy->addChild	(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "with_no_color_attachments",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_TRUE), sixRepeats));

					/* Tests for clear operation within a statistics query activated.
					 * Nothing for secondary_inherited cases can be done since it violates the specification.
					 *
					 * The query shouldn't count internal driver operations relevant to the clear operations.
					 */
					const ClearOperation clearOp[] =	{ CLEAR_NOOP, CLEAR_COLOR, CLEAR_DEPTH };
					const char* const	 clearOpStr[] = { "", "_clear_color", "_clear_depth" };

					for (int clearOpIdx = 0; clearOpIdx < DE_LENGTH_OF_ARRAY(clearOp); ++clearOpIdx) {
						primary->addChild						(new QueryPoolGraphicStatisticsTest<GeometryShaderTestInstance>						(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));
						secondary->addChild						(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryTestInstance>			(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));

						primaryHostQueryReset->addChild			(new QueryPoolGraphicStatisticsTest<GeometryShaderTestInstance>						(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));
						secondaryHostQueryReset->addChild		(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryTestInstance>			(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));

						primaryResetBeforeCopy->addChild		(new QueryPoolGraphicStatisticsTest<GeometryShaderTestInstance>						(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));
						secondaryResetBeforeCopy->addChild		(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryTestInstance>			(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));

						if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
						{
							primaryResetAfterCopy->addChild		(new QueryPoolGraphicStatisticsTest<GeometryShaderTestInstance>						(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));
							secondaryResetAfterCopy->addChild	(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryTestInstance>			(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));
						}
					}

					secondaryInherited->addChild					(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset), sixRepeats));
					secondaryInheritedHostQueryReset->addChild		(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset), sixRepeats));
					secondaryInheritedResetBeforeCopy->addChild		(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset), sixRepeats));
					if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
						secondaryInheritedResetAfterCopy->addChild	(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset), sixRepeats));
				}
			}
		}

		// test corner case: on some implementations running queries while blitting between incompatible formats
		// falls back to a draw shader, and this may end up erroneously incrementing pipeline statistics results
		primary->addChild(new BlitBetweenIncompatibleFormatsTestCase(m_testCtx, "blit_between_incompatible_formats"));

		clippingInvocations->addChild(primary.release());
		clippingInvocations->addChild(secondary.release());
		clippingInvocations->addChild(secondaryInherited.release());

		clippingInvocationsHostQueryReset->addChild(primaryHostQueryReset.release());
		clippingInvocationsHostQueryReset->addChild(secondaryHostQueryReset.release());
		clippingInvocationsHostQueryReset->addChild(secondaryInheritedHostQueryReset.release());

		clippingInvocationsResetBeforeCopy->addChild(primaryResetBeforeCopy.release());
		clippingInvocationsResetBeforeCopy->addChild(secondaryResetBeforeCopy.release());
		clippingInvocationsResetBeforeCopy->addChild(secondaryInheritedResetBeforeCopy.release());

		clippingInvocationsResetAfterCopy->addChild(primaryResetAfterCopy.release());
		clippingInvocationsResetAfterCopy->addChild(secondaryResetAfterCopy.release());
		clippingInvocationsResetAfterCopy->addChild(secondaryInheritedResetAfterCopy.release());
	}

	//VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT
	{
		de::MovePtr<TestCaseGroup>	primary				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondary			(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInherited	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		de::MovePtr<TestCaseGroup>	primaryHostQueryReset				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondaryHostQueryReset				(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInheritedHostQueryReset	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		de::MovePtr<TestCaseGroup>	primaryResetBeforeCopy				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondaryResetBeforeCopy				(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInheritedResetBeforeCopy	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		de::MovePtr<TestCaseGroup>	primaryResetAfterCopy				(new TestCaseGroup(m_testCtx, "primary",			""));
		de::MovePtr<TestCaseGroup>	secondaryResetAfterCopy				(new TestCaseGroup(m_testCtx, "secondary",			""));
		de::MovePtr<TestCaseGroup>	secondaryInheritedResetAfterCopy	(new TestCaseGroup(m_testCtx, "secondary_inherited",""));

		for (deUint32 copyTypeIdx = 0; copyTypeIdx < DE_LENGTH_OF_ARRAY(copyType); copyTypeIdx++)
		{
			for (int topologyNdx = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; topologyNdx < VK_PRIMITIVE_TOPOLOGY_PATCH_LIST; ++topologyNdx)
			{
				for (deUint32 i = 0; i < 4; ++i)
				{
					deBool query64Bits = (i & 1);
					deBool dstOffset = (i & 2);
					std::string prefix = bitPrefix(query64Bits, dstOffset);

					// It makes no sense to use dstOffset with vkGetQueryPoolResults()
					if (copyType[copyTypeIdx] == COPY_TYPE_GET && dstOffset)
						continue;

					// Tests with no attachments for only primary command to reduce # of test cases.
					primary->addChild					(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "with_no_color_attachments",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_TRUE), sixRepeats));

					primaryHostQueryReset->addChild		(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "with_no_color_attachments",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_TRUE), sixRepeats));

					primaryResetBeforeCopy->addChild	(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "with_no_color_attachments",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_TRUE), sixRepeats));

					if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
						primaryResetAfterCopy->addChild	(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + "with_no_color_attachments",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_TRUE), sixRepeats));

					/* Tests for clear operation within a statistics query activated.
					 * Nothing for secondary_inherited cases can be done since it violates the specification.
					 *
					 * The query shouldn't count internal driver operations relevant to the clear operations.
					 */
					const ClearOperation clearOp[] =	{ CLEAR_NOOP, CLEAR_COLOR, CLEAR_DEPTH };
					const char* const	 clearOpStr[] = { "", "_clear_color", "_clear_depth" };

					for (int clearOpIdx = 0; clearOpIdx < DE_LENGTH_OF_ARRAY(clearOp); ++clearOpIdx) {
						primary->addChild						(new QueryPoolGraphicStatisticsTest<GeometryShaderTestInstance>						(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));
						secondary->addChild						(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryTestInstance>			(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));

						primaryHostQueryReset->addChild			(new QueryPoolGraphicStatisticsTest<GeometryShaderTestInstance>						(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));
						secondaryHostQueryReset->addChild		(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryTestInstance>			(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));

						primaryResetBeforeCopy->addChild		(new QueryPoolGraphicStatisticsTest<GeometryShaderTestInstance>						(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));
						secondaryResetBeforeCopy->addChild		(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryTestInstance>			(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));

						if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
						{
							primaryResetAfterCopy->addChild		(new QueryPoolGraphicStatisticsTest<GeometryShaderTestInstance>						(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));
							secondaryResetAfterCopy->addChild	(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryTestInstance>			(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx] + clearOpStr[clearOpIdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx]), sixRepeats));
						}
					}

					secondaryInherited->addChild					(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset), sixRepeats));
					secondaryInheritedHostQueryReset->addChild		(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset), sixRepeats));
					secondaryInheritedResetBeforeCopy->addChild		(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset), sixRepeats));
					if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
						secondaryInheritedResetAfterCopy->addChild	(new QueryPoolGraphicStatisticsTest<GeometryShaderSecondaryInheritedTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + topology_name[topologyNdx],	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT, (VkPrimitiveTopology)topologyNdx, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset), sixRepeats));
				}
			}
		}

		clippingPrimitives->addChild(primary.release());
		clippingPrimitives->addChild(secondary.release());
		clippingPrimitives->addChild(secondaryInherited.release());

		clippingPrimitivesHostQueryReset->addChild(primaryHostQueryReset.release());
		clippingPrimitivesHostQueryReset->addChild(secondaryHostQueryReset.release());
		clippingPrimitivesHostQueryReset->addChild(secondaryInheritedHostQueryReset.release());

		clippingPrimitivesResetBeforeCopy->addChild(primaryResetBeforeCopy.release());
		clippingPrimitivesResetBeforeCopy->addChild(secondaryResetBeforeCopy.release());
		clippingPrimitivesResetBeforeCopy->addChild(secondaryInheritedResetBeforeCopy.release());

		clippingPrimitivesResetAfterCopy->addChild(primaryResetAfterCopy.release());
		clippingPrimitivesResetAfterCopy->addChild(secondaryResetAfterCopy.release());
		clippingPrimitivesResetAfterCopy->addChild(secondaryInheritedResetAfterCopy.release());
	}

	//VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT
	//VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT
	for (deUint32 copyTypeIdx = 0; copyTypeIdx < DE_LENGTH_OF_ARRAY(copyType); copyTypeIdx++)
	{
		for (deUint32 i = 0; i < 4; ++i)
		{
			deBool query64Bits = (i & 1);
			deBool dstOffset = (i & 2);
			std::string prefix = bitPrefix(query64Bits, dstOffset);

			// It makes no sense to use dstOffset with vkGetQueryPoolResults()
			if (copyType[copyTypeIdx] == COPY_TYPE_GET && dstOffset)
				continue;

			// Tests with no attachments for only primary command to reduce # of test cases.
			tesControlPatches->addChild						(new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "tes_control_patches_with_no_color_attachments", "",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_TRUE, STRIDE_TYPE_VALID, DE_TRUE), sixRepeats));

			tesControlPatchesHostQueryReset->addChild		(new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "tes_control_patches_with_no_color_attachments",						"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_TRUE, STRIDE_TYPE_VALID, DE_TRUE), sixRepeats));

			tesControlPatchesResetBeforeCopy->addChild		(new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "tes_control_patches_with_no_color_attachments",						"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_TRUE, STRIDE_TYPE_VALID, DE_TRUE), sixRepeats));

			if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
				tesControlPatchesResetAfterCopy->addChild	(new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "tes_control_patches_with_no_color_attachments",						"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_TRUE, STRIDE_TYPE_VALID, DE_TRUE), sixRepeats));

			tesEvaluationShaderInvocations->addChild					(new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>		(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "tes_evaluation_shader_invocations_with_no_color_attachments",						"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_TRUE, STRIDE_TYPE_VALID, DE_TRUE), sixRepeats));

			tesEvaluationShaderInvocationsHostQueryReset->addChild		(new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>		(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "tes_evaluation_shader_invocations_with_no_color_attachments",						"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_TRUE, STRIDE_TYPE_VALID, DE_TRUE), sixRepeats));

			tesEvaluationShaderInvocationsResetBeforeCopy->addChild		(new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>		(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "tes_evaluation_shader_invocations_with_no_color_attachments",						"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_TRUE, STRIDE_TYPE_VALID, DE_TRUE), sixRepeats));

			if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
				tesEvaluationShaderInvocationsResetAfterCopy->addChild	(new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>		(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "tes_evaluation_shader_invocations_with_no_color_attachments",						"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_TRUE, STRIDE_TYPE_VALID, DE_TRUE), sixRepeats));

			/* Tests for clear operation within a statistics query activated.
			 * Nothing for secondary_inherited cases can be done since it violates the specification.
			 *
			 * The query shouldn't count internal driver operations relevant to the clear operations.
			 */
			const ClearOperation clearOp[] =	{ CLEAR_NOOP, CLEAR_COLOR, CLEAR_DEPTH };
			const char* const	 clearOpStr[] = { "", "_clear_color", "_clear_depth" };

			for (int clearOpIdx = 0; clearOpIdx < DE_LENGTH_OF_ARRAY(clearOp); ++clearOpIdx) {
				tesControlPatches->addChild(new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>							(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "tes_control_patches" + clearOpStr[clearOpIdx],						"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx], DE_FALSE, STRIDE_TYPE_VALID, DE_TRUE), sixRepeats));
				tesControlPatches->addChild(new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayTestInstance>					(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "tes_control_patches_secondary" + clearOpStr[clearOpIdx],			"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx], DE_FALSE, STRIDE_TYPE_VALID, DE_TRUE), sixRepeats));

				tesControlPatchesHostQueryReset->addChild(new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>			(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "tes_control_patches" + clearOpStr[clearOpIdx],						"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx], DE_FALSE, STRIDE_TYPE_VALID, DE_TRUE), sixRepeats));
				tesControlPatchesHostQueryReset->addChild(new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "tes_control_patches_secondary" + clearOpStr[clearOpIdx],			"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx], DE_FALSE, STRIDE_TYPE_VALID, DE_TRUE), sixRepeats));

				tesControlPatchesResetBeforeCopy->addChild(new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>			(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "tes_control_patches" + clearOpStr[clearOpIdx],						"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx], DE_FALSE, STRIDE_TYPE_VALID, DE_TRUE), sixRepeats));
				tesControlPatchesResetBeforeCopy->addChild(new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "tes_control_patches_secondary" + clearOpStr[clearOpIdx],			"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx], DE_FALSE, STRIDE_TYPE_VALID, DE_TRUE), sixRepeats));

				if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
				{
					tesControlPatchesResetAfterCopy->addChild(new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>			(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "tes_control_patches" + clearOpStr[clearOpIdx],						"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx], DE_FALSE, STRIDE_TYPE_VALID, DE_TRUE), sixRepeats));
					tesControlPatchesResetAfterCopy->addChild(new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "tes_control_patches_secondary" + clearOpStr[clearOpIdx],			"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx], DE_FALSE, STRIDE_TYPE_VALID, DE_TRUE), sixRepeats));
				}

				tesEvaluationShaderInvocations->addChild(new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>					 (m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "tes_evaluation_shader_invocations" + clearOpStr[clearOpIdx],						"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx], DE_FALSE, STRIDE_TYPE_VALID, DE_TRUE), sixRepeats));
				tesEvaluationShaderInvocations->addChild(new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayTestInstance>		 (m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "tes_evaluation_shader_invocations_secondary" + clearOpStr[clearOpIdx],				"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx], DE_FALSE, STRIDE_TYPE_VALID, DE_TRUE), sixRepeats));

				tesEvaluationShaderInvocationsHostQueryReset->addChild(new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>			(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "tes_evaluation_shader_invocations" + clearOpStr[clearOpIdx],						"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx], DE_FALSE, STRIDE_TYPE_VALID, DE_TRUE), sixRepeats));
				tesEvaluationShaderInvocationsHostQueryReset->addChild(new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "tes_evaluation_shader_invocations_secondary" + clearOpStr[clearOpIdx],				"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx], DE_FALSE, STRIDE_TYPE_VALID, DE_TRUE), sixRepeats));

				tesEvaluationShaderInvocationsResetBeforeCopy->addChild(new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>			(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "tes_evaluation_shader_invocations" + clearOpStr[clearOpIdx],						"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx], DE_FALSE, STRIDE_TYPE_VALID, DE_TRUE), sixRepeats));
				tesEvaluationShaderInvocationsResetBeforeCopy->addChild(new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "tes_evaluation_shader_invocations_secondary" + clearOpStr[clearOpIdx],				"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx], DE_FALSE, STRIDE_TYPE_VALID, DE_TRUE), sixRepeats));

				if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
				{
					tesEvaluationShaderInvocationsResetAfterCopy->addChild(new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>		(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "tes_evaluation_shader_invocations" + clearOpStr[clearOpIdx],						"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx], DE_FALSE, STRIDE_TYPE_VALID, DE_TRUE), sixRepeats));
					tesEvaluationShaderInvocationsResetAfterCopy->addChild(new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "tes_evaluation_shader_invocations_secondary" + clearOpStr[clearOpIdx],				"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, clearOp[clearOpIdx], DE_FALSE, STRIDE_TYPE_VALID, DE_TRUE), sixRepeats));
				}
			}

			tesControlPatches->addChild									(new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayInheritedTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "tes_control_patches_secondary_inherited",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_FALSE, STRIDE_TYPE_VALID, DE_TRUE), sixRepeats));
			tesControlPatchesHostQueryReset->addChild					(new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayInheritedTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "tes_control_patches_secondary_inherited",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_FALSE, STRIDE_TYPE_VALID, DE_TRUE), sixRepeats));
			tesControlPatchesResetBeforeCopy->addChild					(new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayInheritedTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "tes_control_patches_secondary_inherited",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_FALSE, STRIDE_TYPE_VALID, DE_TRUE), sixRepeats));
			if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
				tesControlPatchesResetAfterCopy->addChild				(new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayInheritedTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "tes_control_patches_secondary_inherited",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_FALSE, STRIDE_TYPE_VALID, DE_TRUE), sixRepeats));

			tesEvaluationShaderInvocations->addChild(new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayInheritedTestInstance>(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "tes_evaluation_shader_invocations_secondary_inherited",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_NORMAL, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_FALSE, STRIDE_TYPE_VALID, DE_TRUE), sixRepeats));
			tesEvaluationShaderInvocationsHostQueryReset->addChild		(new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayInheritedTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "tes_evaluation_shader_invocations_secondary_inherited",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_HOST, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_FALSE, STRIDE_TYPE_VALID, DE_TRUE), sixRepeats));
			tesEvaluationShaderInvocationsResetBeforeCopy->addChild		(new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayInheritedTestInstance>(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "tes_evaluation_shader_invocations_secondary_inherited",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_BEFORE_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_FALSE, STRIDE_TYPE_VALID, DE_TRUE), sixRepeats));
			if (copyType[copyTypeIdx] == COPY_TYPE_CMD)
				tesEvaluationShaderInvocationsResetAfterCopy->addChild	(new QueryPoolGraphicStatisticsTest<TessellationShaderSecondrayInheritedTestInstance>	(m_testCtx,	prefix + copyTypeStr[copyTypeIdx] + "tes_evaluation_shader_invocations_secondary_inherited",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_AFTER_COPY, copyType[copyTypeIdx], query64Bits, DE_FALSE, dstOffset, CLEAR_NOOP, DE_FALSE, STRIDE_TYPE_VALID, DE_TRUE), sixRepeats));
		}
	}

	// number of GS invocations with TES/TSC/vert
	{
		for (deUint32 copyTypeIdx = 0; copyTypeIdx < DE_LENGTH_OF_ARRAY(copyType); copyTypeIdx++)
		{
			gsInvocationsNoGs->addChild								(new QueryPoolGraphicStatisticsTest<VertexShaderTestInstance>						(m_testCtx,	copyTypeStr[copyTypeIdx] + "gs_invocations_no_gs_vtx",	"",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, RESET_TYPE_NORMAL, copyType[copyTypeIdx], DE_FALSE, DE_FALSE, DE_FALSE, CLEAR_NOOP, DE_TRUE, STRIDE_TYPE_VALID), sixRepeats));
			gsInvocationsNoGs->addChild						(new QueryPoolGraphicStatisticsTest<TessellationShaderTestInstance>					(m_testCtx,	copyTypeStr[copyTypeIdx] + "gs_invocations_no_gs_tes",  "",	GraphicBasicTestInstance::ParametersGraphic(VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, RESET_TYPE_NORMAL, copyType[copyTypeIdx], DE_FALSE, DE_FALSE, 0, CLEAR_NOOP, DE_TRUE, STRIDE_TYPE_VALID, true), sixRepeats));
		}
	}

	// Multiple statistics query flags enabled
	{
		VkQueryResultFlags	partialFlags[]		=	{ 0u, VK_QUERY_RESULT_PARTIAL_BIT };
		const char* const	partialFlagsStr[]	=	{ "", "_partial" };
		VkQueryResultFlags	waitFlags[]			=	{ 0u, VK_QUERY_RESULT_WAIT_BIT };
		const char* const	waitFlagsStr[]		=	{ "", "_wait" };

		const CopyType		copyTypes[]			=	{ COPY_TYPE_GET, COPY_TYPE_CMD, COPY_TYPE_CMD };
		const char* const   copyTypesStr[]		=	{ "", "_cmdcopy", "_cmdcopy_dstoffset" };

		const StrideType	strideTypes[]		=	{ STRIDE_TYPE_VALID, STRIDE_TYPE_ZERO };
		const char* const   strideTypesStr[]	=	{ "", "_stride_zero" };

		const VkQueryPipelineStatisticFlags statisticsFlags =
			VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT |
			VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT;

		for (deUint32 partialFlagsIdx = 0u; partialFlagsIdx < DE_LENGTH_OF_ARRAY(partialFlags); partialFlagsIdx++)
		{
			for (deUint32 waitFlagsIdx = 0u; waitFlagsIdx < DE_LENGTH_OF_ARRAY(waitFlags); waitFlagsIdx++)
			{
				for (deUint32 copyTypesIdx = 0u; copyTypesIdx < DE_LENGTH_OF_ARRAY(copyTypes); copyTypesIdx++)
				{
					for (deUint32 strideTypesIdx = 0u; strideTypesIdx < DE_LENGTH_OF_ARRAY(strideTypes); strideTypesIdx++)
					{
						deUint32 dstOffset = copyTypesIdx == 2u ? deUint32(NUM_QUERY_STATISTICS * sizeof(deUint64)) : deUint32(0u);
						/* Avoid waiting infinite time for the queries, when one of them is not going to be issued in
						 * the partial case.
						 */
						if ((deBool)(partialFlags[partialFlagsIdx] & VK_QUERY_RESULT_PARTIAL_BIT) &&
							(deBool)(waitFlags[waitFlagsIdx] & VK_QUERY_RESULT_WAIT_BIT))
							continue;

						// Skip stride bogus tests when there are more than one query count.
						if (partialFlags[partialFlagsIdx] && strideTypes[strideTypesIdx] == STRIDE_TYPE_ZERO)
							continue;

						if (strideTypes[strideTypesIdx] == STRIDE_TYPE_ZERO && copyType[copyTypesIdx] != COPY_TYPE_CMD)
							continue;

						VkQueryResultFlags	queryFlags	= VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT | partialFlags[partialFlagsIdx] | waitFlags[waitFlagsIdx];
						deUint32			queryCount	= partialFlagsIdx ? 2u : 1u;
						{
							std::ostringstream testName;
							testName	<< "input_assembly_vertex_fragment"
										<< partialFlagsStr[partialFlagsIdx]
										<< waitFlagsStr[waitFlagsIdx]
										<< copyTypesStr[copyTypesIdx]
										<< strideTypesStr[strideTypesIdx];
							GraphicBasicMultipleQueryTestInstance::ParametersGraphic param(statisticsFlags | VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT, queryFlags, queryCount, DE_FALSE, copyTypes[copyTypesIdx], dstOffset, strideType[strideTypesIdx]);
							vertexShaderMultipleQueries->addChild(new QueryPoolGraphicMultipleQueryStatisticsTest<VertexShaderMultipleQueryTestInstance>(m_testCtx, testName.str().c_str(), "", param));
						}

						{
							// No fragment shader case
							std::ostringstream testName;
							testName	<< "input_assembly_vertex"
										<< partialFlagsStr[partialFlagsIdx]
										<< waitFlagsStr[waitFlagsIdx]
										<< copyTypesStr[copyTypesIdx]
										<< strideTypesStr[strideTypesIdx];
							GraphicBasicMultipleQueryTestInstance::ParametersGraphic param(statisticsFlags | VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, queryFlags, queryCount, DE_TRUE, copyTypes[copyTypesIdx], dstOffset, strideType[strideTypesIdx]);
							vertexShaderMultipleQueries->addChild(new QueryPoolGraphicMultipleQueryStatisticsTest<VertexShaderMultipleQueryTestInstance>(m_testCtx, testName.str().c_str(), "", param));
						}
					}
				}
			}
		}
	}

	addChild(computeShaderInvocationsGroup.release());
	addChild(inputAssemblyVertices.release());
	addChild(inputAssemblyPrimitives.release());
	addChild(vertexShaderInvocations.release());
	addChild(fragmentShaderInvocations.release());
	addChild(geometryShaderInvocations.release());
	addChild(geometryShaderPrimitives.release());
	addChild(clippingInvocations.release());
	addChild(clippingPrimitives.release());
	addChild(tesControlPatches.release());
	addChild(tesEvaluationShaderInvocations.release());
	addChild(gsInvocationsNoGs.release());

	vertexOnlyGroup->addChild(inputAssemblyVerticesVertexOnly.release());
	vertexOnlyGroup->addChild(inputAssemblyPrimitivesVertexOnly.release());
	vertexOnlyGroup->addChild(vertexShaderInvocationsVertexOnly.release());
	addChild(vertexOnlyGroup.release());

	hostQueryResetGroup->addChild(computeShaderInvocationsGroupHostQueryReset.release());
	hostQueryResetGroup->addChild(inputAssemblyVerticesHostQueryReset.release());
	hostQueryResetGroup->addChild(inputAssemblyPrimitivesHostQueryReset.release());
	hostQueryResetGroup->addChild(vertexShaderInvocationsHostQueryReset.release());
	hostQueryResetGroup->addChild(fragmentShaderInvocationsHostQueryReset.release());
	hostQueryResetGroup->addChild(geometryShaderInvocationsHostQueryReset.release());
	hostQueryResetGroup->addChild(geometryShaderPrimitivesHostQueryReset.release());
	hostQueryResetGroup->addChild(clippingInvocationsHostQueryReset.release());
	hostQueryResetGroup->addChild(clippingPrimitivesHostQueryReset.release());
	hostQueryResetGroup->addChild(tesControlPatchesHostQueryReset.release());
	hostQueryResetGroup->addChild(tesEvaluationShaderInvocationsHostQueryReset.release());
	addChild(hostQueryResetGroup.release());

	resetBeforeCopyGroup->addChild(computeShaderInvocationsGroupResetBeforeCopy.release());
	resetBeforeCopyGroup->addChild(inputAssemblyVerticesResetBeforeCopy.release());
	resetBeforeCopyGroup->addChild(inputAssemblyPrimitivesResetBeforeCopy.release());
	resetBeforeCopyGroup->addChild(vertexShaderInvocationsResetBeforeCopy.release());
	resetBeforeCopyGroup->addChild(fragmentShaderInvocationsResetBeforeCopy.release());
	resetBeforeCopyGroup->addChild(geometryShaderInvocationsResetBeforeCopy.release());
	resetBeforeCopyGroup->addChild(geometryShaderPrimitivesResetBeforeCopy.release());
	resetBeforeCopyGroup->addChild(clippingInvocationsResetBeforeCopy.release());
	resetBeforeCopyGroup->addChild(clippingPrimitivesResetBeforeCopy.release());
	resetBeforeCopyGroup->addChild(tesControlPatchesResetBeforeCopy.release());
	resetBeforeCopyGroup->addChild(tesEvaluationShaderInvocationsResetBeforeCopy.release());
	addChild(resetBeforeCopyGroup.release());

	resetAfterCopyGroup->addChild(computeShaderInvocationsGroupResetAfterCopy.release());
	resetAfterCopyGroup->addChild(inputAssemblyVerticesResetAfterCopy.release());
	resetAfterCopyGroup->addChild(inputAssemblyPrimitivesResetAfterCopy.release());
	resetAfterCopyGroup->addChild(vertexShaderInvocationsResetAfterCopy.release());
	resetAfterCopyGroup->addChild(fragmentShaderInvocationsResetAfterCopy.release());
	resetAfterCopyGroup->addChild(geometryShaderInvocationsResetAfterCopy.release());
	resetAfterCopyGroup->addChild(geometryShaderPrimitivesResetAfterCopy.release());
	resetAfterCopyGroup->addChild(clippingInvocationsResetAfterCopy.release());
	resetAfterCopyGroup->addChild(clippingPrimitivesResetAfterCopy.release());
	resetAfterCopyGroup->addChild(tesControlPatchesResetAfterCopy.release());
	resetAfterCopyGroup->addChild(tesEvaluationShaderInvocationsResetAfterCopy.release());
	addChild(resetAfterCopyGroup.release());

	addChild(vertexShaderMultipleQueries.release());
}

void QueryPoolStatisticsTests::deinit (void)
{
	destroyDeviceHelpers();
}

} //QueryPool
} //vkt
