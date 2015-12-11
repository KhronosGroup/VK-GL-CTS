/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 Google Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be
 * included in all copies or substantial portions of the Materials.
 *
 * The Materials are Confidential Information as defined by the
 * Khronos Membership Agreement until designated non-confidential by
 * Khronos, at which point this condition clause shall be removed.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
 *
 *//*!
 * \file
 * \brief Api Feature Query tests
 *//*--------------------------------------------------------------------*/

#include "vktApiFeatureInfo.hpp"

#include "vktTestCaseUtil.hpp"

#include "vkPlatform.hpp"
#include "vkStrUtil.hpp"
#include "vkRef.hpp"
#include "vkDeviceUtil.hpp"
#include "vkQueryUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuFormatUtil.hpp"

#include "deUniquePtr.hpp"
#include "deMemory.h"

namespace vkt
{
namespace api
{
namespace
{

using namespace vk;
using std::vector;
using std::string;
using tcu::TestLog;
using tcu::ScopedLogSection;

enum
{
	GUARD_SIZE	= 0x20,			//!< Number of bytes to check
	GUARD_VALUE	= 0xcd,			//!< Data pattern
};

enum LimitFormat
{
	LIMIT_FORMAT_SIGNED_INT,
	LIMIT_FORMAT_UNSIGNED_INT,
	LIMIT_FORMAT_FLOAT,
	LIMIT_FORMAT_DEVICE_SIZE,

	LIMIT_FORMAT_LAST
};

enum LimitType
{
	LIMIT_TYPE_MIN,
	LIMIT_TYPE_MAX,

	LIMIT_TYPE_LAST
};

#define FEATURE(_X_)	DE_OFFSET_OF(VkPhysicalDeviceLimits, _X_),(char*)(#_X_)
bool validateFeatureLimits(VkPhysicalDeviceProperties* properties, TestLog& log)
{
	bool					limitsOk	= true;
	VkPhysicalDeviceLimits* limits		= &properties->limits;
	struct FeatureLimitTable
	{
		deUint32		offset;
		char*			name;
		deUint32		uintVal;			//!< Format is UNSIGNED_INT
		deInt32			intVal;				//!< Format is SIGNED_INT
		deUint64		deviceSizeVal;		//!< Format is DEVICE_SIZE
		float			floatVal;			//!< Format is FLOAT
		LimitFormat		format;
		LimitType		type;
	} featureLimitTable[] =   //!< From gitlab.khronos.org/vulkan/vulkan.git:doc/specs/vulkan/chapters/features.txt@40de2de5b8167ec277ea2969d621857094323831
	{
		{ FEATURE(maxImageDimension1D),									4096, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN },
		{ FEATURE(maxImageDimension2D),									4096, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  },
		{ FEATURE(maxImageDimension3D),									256, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN   },
		{ FEATURE(maxImageDimensionCube),								4096, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  },
		{ FEATURE(maxImageArrayLayers),									256, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN    },
		{ FEATURE(maxTexelBufferElements),								65536, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN   },
		{ FEATURE(maxUniformBufferRange),								16384, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN   },
		{ FEATURE(maxPushConstantsSize),								128, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN   },
		{ FEATURE(maxMemoryAllocationCount),							4096, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  },
		{ FEATURE(bufferImageGranularity),								0, 0, 131072, 0, LIMIT_FORMAT_DEVICE_SIZE, LIMIT_TYPE_MAX },
		{ FEATURE(sparseAddressSpaceSize),								0, 0, 2UL*1024*1024*1024, 0, LIMIT_FORMAT_DEVICE_SIZE, LIMIT_TYPE_MIN },
		{ FEATURE(maxBoundDescriptorSets),								4, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN },
		{ FEATURE(maxPerStageDescriptorSamplers),						16, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN },
		{ FEATURE(maxPerStageDescriptorUniformBuffers),					12, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  },
		{ FEATURE(maxPerStageDescriptorStorageBuffers),					4, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  },
		{ FEATURE(maxPerStageDescriptorSampledImages),					16, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  },
		{ FEATURE(maxPerStageDescriptorStorageImages),					4, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  },
		{ FEATURE(maxPerStageDescriptorInputAttachments),				4, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  },
		{ FEATURE(maxDescriptorSetSamplers),							96, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN },
		{ FEATURE(maxDescriptorSetUniformBuffers),						72, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  },
		{ FEATURE(maxDescriptorSetUniformBuffersDynamic),				8, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN },
		{ FEATURE(maxDescriptorSetStorageBuffers),						24, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  },
		{ FEATURE(maxDescriptorSetStorageBuffersDynamic),				4, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN   },
		{ FEATURE(maxDescriptorSetSampledImages),						96, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN   },
		{ FEATURE(maxDescriptorSetStorageImages),						24, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN   },
		{ FEATURE(maxVertexInputAttributes),							16, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN   },
		{ FEATURE(maxVertexInputBindings),								16, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN   },
		{ FEATURE(maxVertexInputAttributeOffset),						2047, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN   },
		{ FEATURE(maxVertexInputBindingStride),							2048, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN   },
		{ FEATURE(maxVertexOutputComponents),							64, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN   },
		{ FEATURE(maxTessellationGenerationLevel),						64, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN   },
		{ FEATURE(maxTessellationPatchSize),							32, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  },
		{ FEATURE(maxTessellationControlPerVertexInputComponents),		64, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN   },
		{ FEATURE(maxTessellationControlPerVertexOutputComponents),		64, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN   },
		{ FEATURE(maxTessellationControlPerPatchOutputComponents),		120, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN   },
		{ FEATURE(maxTessellationControlTotalOutputComponents),			2048, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN   },
		{ FEATURE(maxTessellationEvaluationInputComponents),			64, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN   },
		{ FEATURE(maxTessellationEvaluationOutputComponents),			64, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN   },
		{ FEATURE(maxGeometryShaderInvocations),						32, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN   },
		{ FEATURE(maxGeometryInputComponents),							64, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN   },
		{ FEATURE(maxGeometryOutputComponents),							64, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN   },
		{ FEATURE(maxGeometryOutputVertices),							256, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN   },
		{ FEATURE(maxGeometryTotalOutputComponents),					1024, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN   },
		{ FEATURE(maxFragmentInputComponents),							64, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN   },
		{ FEATURE(maxFragmentOutputAttachments),						4, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN   },
		{ FEATURE(maxFragmentDualSrcAttachments),						1, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN   },
		{ FEATURE(maxFragmentCombinedOutputResources),					4, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN   },
		{ FEATURE(maxComputeSharedMemorySize),							16384, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN    },
		{ FEATURE(maxComputeWorkGroupCount[0]),							65536, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN    },
		{ FEATURE(maxComputeWorkGroupCount[1]),							65536, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN    },
		{ FEATURE(maxComputeWorkGroupCount[2]),							65536,  0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN    },
		{ FEATURE(maxComputeWorkGroupInvocations),						128, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN     },
		{ FEATURE(maxComputeWorkGroupSize[0]),							128, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN     },
		{ FEATURE(maxComputeWorkGroupSize[1]),							128, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN     },
		{ FEATURE(maxComputeWorkGroupSize[2]),							64, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN     },
		{ FEATURE(subPixelPrecisionBits),								4, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN     },
		{ FEATURE(subTexelPrecisionBits),								4, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN     },
		{ FEATURE(mipmapPrecisionBits),									4, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN     },
		{ FEATURE(maxDrawIndirectCount),								65535, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN     },
		{ FEATURE(maxSamplerLodBias),									0, 0, 0, 2.0f, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MIN },
		{ FEATURE(maxSamplerAnisotropy),								0, 0, 0, 16.0f, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MIN },
		{ FEATURE(maxViewports),										16, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN },
		{ FEATURE(maxViewportDimensions[0]),							4096, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  },
		{ FEATURE(maxViewportDimensions[1]),							4096, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN  },
		{ FEATURE(viewportBoundsRange[0]),								0, 0, 0, -8192, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MAX },
		{ FEATURE(viewportBoundsRange[1]),								0, 0, 0, 8191, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MIN },
		{ FEATURE(viewportSubPixelBits),								0, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN },
		{ FEATURE(minMemoryMapAlignment),								64, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN },
		{ FEATURE(minTexelBufferOffsetAlignment),						256, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MAX },
		{ FEATURE(minUniformBufferOffsetAlignment),						256, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MAX },
		{ FEATURE(minStorageBufferOffsetAlignment),						256, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MAX },
		{ FEATURE(minTexelOffset),										0, -8, 0, 0, LIMIT_FORMAT_SIGNED_INT, LIMIT_TYPE_MAX },
		{ FEATURE(maxTexelOffset),										7, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN },
		{ FEATURE(minTexelGatherOffset),								0, -8, 0, 0, LIMIT_FORMAT_SIGNED_INT, LIMIT_TYPE_MAX },
		{ FEATURE(maxTexelGatherOffset),								7, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN },
		{ FEATURE(minInterpolationOffset),								0, 0, 0, -0.5f, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MAX },
		{ FEATURE(maxInterpolationOffset),								0, 0, 0, 0.5f, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MIN },
		{ FEATURE(subPixelInterpolationOffsetBits),						4, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN },
		{ FEATURE(maxFramebufferWidth),									4096, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN },
		{ FEATURE(maxFramebufferHeight),								4096, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN },
		{ FEATURE(maxFramebufferLayers),								256, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN },
		{ FEATURE(maxColorAttachments),									4, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN },
		{ FEATURE(maxSampleMaskWords),									1, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN },
		{ FEATURE(maxClipDistances),									8, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN },
		{ FEATURE(maxCullDistances),									8, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN },
		{ FEATURE(maxCombinedClipAndCullDistances),						8, 0, 0, 0, LIMIT_FORMAT_UNSIGNED_INT, LIMIT_TYPE_MIN },
		{ FEATURE(pointSizeRange[0]),									0, 0, 0, 1.0, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MAX },
		{ FEATURE(pointSizeRange[1]),									0, 0, 0, 1.0, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MIN },
		{ FEATURE(pointSizeRange[0]),									0, 0, 0, 1.0, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MAX },
		{ FEATURE(pointSizeRange[1]),									0, 0, 0, 64.0, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MIN },
		{ FEATURE(lineWidthRange[0]),									0, 0, 0, 1.0, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MAX },
		{ FEATURE(lineWidthRange[1]),									0, 0, 0, 1.0, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MIN },
		{ FEATURE(lineWidthRange[0]),									0, 0, 0, 1.0, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MAX },
		{ FEATURE(lineWidthRange[1]),									0, 0, 0, 8.0, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MIN },
		{ FEATURE(pointSizeGranularity),								0, 0, 0, 1.0, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MAX },
		{ FEATURE(lineWidthGranularity),								0, 0, 0, 1.0, LIMIT_FORMAT_FLOAT, LIMIT_TYPE_MAX },
		{ FEATURE(nonCoherentAtomSize),									0, 0, 128, 0, LIMIT_FORMAT_DEVICE_SIZE, LIMIT_TYPE_MAX },
	};

	log << TestLog::Message << *limits << TestLog::EndMessage;

	for (deUint32 ndx = 0; ndx < DE_LENGTH_OF_ARRAY(featureLimitTable); ndx++)
	{
		switch (featureLimitTable[ndx].format)
		{
			case LIMIT_FORMAT_UNSIGNED_INT:
				if ( featureLimitTable[ndx].type == LIMIT_TYPE_MIN )
				{
					if (*((deUint32*)((char*)limits+featureLimitTable[ndx].offset)) < featureLimitTable[ndx].uintVal)
					{
						log << TestLog::Message << "limit Validation failed " << featureLimitTable[ndx].name
							<< " not valid-limit type MIN - actual is "
							<< *((deUint32*)((char*)limits + featureLimitTable[ndx].offset)) << TestLog::EndMessage;
						limitsOk = false;
					}
				}
				else
				{
					if (*((deUint32*)((char*)limits+featureLimitTable[ndx].offset)) > featureLimitTable[ndx].uintVal)
					{
						log << TestLog::Message << "limit validation failed,  " << featureLimitTable[ndx].name
							<< " not valid-limit type MAX - actual is "
							<< *((deUint32*)((char*)limits + featureLimitTable[ndx].offset)) << TestLog::EndMessage;
						limitsOk = false;
					}
				}
				break;

			case LIMIT_FORMAT_FLOAT:
				if ( featureLimitTable[ndx].type == LIMIT_TYPE_MIN )
				{
					if (*((float*)((char*)limits+featureLimitTable[ndx].offset)) < featureLimitTable[ndx].floatVal)
					{
						log << TestLog::Message << "limit validation failed, " << featureLimitTable[ndx].name
							<< " not valid-limit type MIN - actual is "
							<< *((float*)((char*)limits + featureLimitTable[ndx].offset)) << TestLog::EndMessage;
						limitsOk = false;
					}
				}
				else
				{
					if (*((float*)((char*)limits+featureLimitTable[ndx].offset)) > featureLimitTable[ndx].floatVal)
					{
						log << TestLog::Message << "limit validation failed, " << featureLimitTable[ndx].name
							<< " not valid-limit type MAX actual is "
							<< *((float*)((char*)limits + featureLimitTable[ndx].offset)) << TestLog::EndMessage;
						limitsOk = false;
					}
				}
				break;

			case LIMIT_FORMAT_SIGNED_INT:
				if (featureLimitTable[ndx].type == LIMIT_TYPE_MIN)
				{
					if (*((deInt32*)((char*)limits+featureLimitTable[ndx].offset)) < featureLimitTable[ndx].intVal)
					{
						log << TestLog::Message <<  "limit validation failed, " << featureLimitTable[ndx].name
							<< " not valid-limit type MIN actual is "
							<< *((deInt32*)((char*)limits + featureLimitTable[ndx].offset)) << TestLog::EndMessage;
						limitsOk = false;
					}
				}
				else
				{
					if (*((deInt32*)((char*)limits+featureLimitTable[ndx].offset)) > featureLimitTable[ndx].intVal)
					{
						log << TestLog::Message << "limit validation failed, " << featureLimitTable[ndx].name
							<< " not valid-limit type MAX actual is "
							<< *((deInt32*)((char*)limits + featureLimitTable[ndx].offset)) << TestLog::EndMessage;
						limitsOk = false;
					}
				}
				break;

			case LIMIT_FORMAT_DEVICE_SIZE:
				if ( featureLimitTable[ndx].type == LIMIT_TYPE_MIN )
				{
					if (*((deUint64*)((char*)limits+featureLimitTable[ndx].offset)) < featureLimitTable[ndx].deviceSizeVal)
					{
						log << TestLog::Message << "limit validation failed, " << featureLimitTable[ndx].name
							<< " not valid-limit type MIN actual is "
							<< *((deUint64*)((char*)limits + featureLimitTable[ndx].offset)) << TestLog::EndMessage;
						limitsOk = false;
					}
				}
				else
				{
					if (*((deUint64*)((char*)limits+featureLimitTable[ndx].offset)) > featureLimitTable[ndx].deviceSizeVal)
					{
						log << TestLog::Message << "limit validation failed, " << featureLimitTable[ndx].name
							<< " not valid-limit type MAX actual is "
							<< *((deUint64*)((char*)limits + featureLimitTable[ndx].offset)) << TestLog::EndMessage;
						limitsOk = false;
					}
				}
				break;

			default:
				DE_ASSERT(0);
				limitsOk = false;
		}
	}

	return limitsOk;
}

tcu::TestStatus enumeratePhysicalDevices (Context& context)
{
	TestLog&						log		= context.getTestContext().getLog();
	const vector<VkPhysicalDevice>	devices	= enumeratePhysicalDevices(context.getInstanceInterface(), context.getInstance());

	log << TestLog::Integer("NumDevices", "Number of devices", "", QP_KEY_TAG_NONE, deInt64(devices.size()));

	for (size_t ndx = 0; ndx < devices.size(); ndx++)
		log << TestLog::Message << ndx << ": " << devices[ndx] << TestLog::EndMessage;

	return tcu::TestStatus::pass("Enumerating devices succeeded");
}

tcu::TestStatus enumerateInstanceLayers (Context& context)
{
	TestLog&						log			= context.getTestContext().getLog();
	const vector<VkLayerProperties>	properties	= enumerateInstanceLayerProperties(context.getPlatformInterface());

	for (size_t ndx = 0; ndx < properties.size(); ndx++)
		log << TestLog::Message << ndx << ": " << properties[ndx] << TestLog::EndMessage;

	return tcu::TestStatus::pass("Enumerating layers succeeded");
}

tcu::TestStatus enumerateInstanceExtensions (Context& context)
{
	TestLog&	log		= context.getTestContext().getLog();

	{
		const ScopedLogSection				section		(log, "Global", "Global Extensions");
		const vector<VkExtensionProperties>	properties	= enumerateInstanceExtensionProperties(context.getPlatformInterface(), DE_NULL);

		for (size_t ndx = 0; ndx < properties.size(); ndx++)
			log << TestLog::Message << ndx << ": " << properties[ndx] << TestLog::EndMessage;
	}

	{
		const vector<VkLayerProperties>	layers	= enumerateInstanceLayerProperties(context.getPlatformInterface());

		for (vector<VkLayerProperties>::const_iterator layer = layers.begin(); layer != layers.end(); ++layer)
		{
			const ScopedLogSection				section		(log, layer->layerName, string("Layer: ") + layer->layerName);
			const vector<VkExtensionProperties>	properties	= enumerateInstanceExtensionProperties(context.getPlatformInterface(), layer->layerName);

			for (size_t extNdx = 0; extNdx < properties.size(); extNdx++)
				log << TestLog::Message << extNdx << ": " << properties[extNdx] << TestLog::EndMessage;
		}
	}

	return tcu::TestStatus::pass("Enumerating extensions succeeded");
}

tcu::TestStatus enumerateDeviceLayers (Context& context)
{
	TestLog&						log			= context.getTestContext().getLog();
	const vector<VkLayerProperties>	properties	= vk::enumerateDeviceLayerProperties(context.getInstanceInterface(), context.getPhysicalDevice());

	for (size_t ndx = 0; ndx < properties.size(); ndx++)
		log << TestLog::Message << ndx << ": " << properties[ndx] << TestLog::EndMessage;

	return tcu::TestStatus::pass("Enumerating layers succeeded");
}

tcu::TestStatus enumerateDeviceExtensions (Context& context)
{
	TestLog&	log		= context.getTestContext().getLog();

	{
		const ScopedLogSection				section		(log, "Global", "Global Extensions");
		const vector<VkExtensionProperties>	properties	= enumerateDeviceExtensionProperties(context.getInstanceInterface(), context.getPhysicalDevice(), DE_NULL);

		for (size_t ndx = 0; ndx < properties.size(); ndx++)
			log << TestLog::Message << ndx << ": " << properties[ndx] << TestLog::EndMessage;
	}

	{
		const vector<VkLayerProperties>	layers	= enumerateDeviceLayerProperties(context.getInstanceInterface(), context.getPhysicalDevice());

		for (vector<VkLayerProperties>::const_iterator layer = layers.begin(); layer != layers.end(); ++layer)
		{
			const ScopedLogSection				section		(log, layer->layerName, string("Layer: ") + layer->layerName);
			const vector<VkExtensionProperties>	properties	= enumerateDeviceExtensionProperties(context.getInstanceInterface(), context.getPhysicalDevice(), layer->layerName);

			for (size_t extNdx = 0; extNdx < properties.size(); extNdx++)
				log << TestLog::Message << extNdx << ": " << properties[extNdx] << TestLog::EndMessage;
		}
	}

	return tcu::TestStatus::pass("Enumerating extensions succeeded");
}

tcu::TestStatus deviceFeatures (Context& context)
{
	TestLog&						log			= context.getTestContext().getLog();
	VkPhysicalDeviceFeatures*		features;
	deUint8							buffer[sizeof(VkPhysicalDeviceFeatures) + GUARD_SIZE];

	deMemset(buffer, GUARD_VALUE, sizeof(buffer));
	features = reinterpret_cast<VkPhysicalDeviceFeatures*>(buffer);

	context.getInstanceInterface().getPhysicalDeviceFeatures(context.getPhysicalDevice(), features);

	log << TestLog::Message << "device = " << context.getPhysicalDevice() << TestLog::EndMessage
		<< TestLog::Message << *features << TestLog::EndMessage;

	for (int ndx = 0; ndx < GUARD_SIZE; ndx++)
	{
		if (buffer[ndx + sizeof(VkPhysicalDeviceFeatures)] != GUARD_VALUE)
		{
			log << TestLog::Message << "deviceFeatures - Guard offset " << ndx << " not valid" << TestLog::EndMessage;
			return tcu::TestStatus::fail("deviceFeatures buffer overflow");
		}
	}

	if (!validateInitComplete(context.getPhysicalDevice(), &InstanceInterface::getPhysicalDeviceFeatures, context.getInstanceInterface()))
	{
		log << TestLog::Message << "deviceFeatures - VkPhysicalDeviceFeatures not completely initialized" << TestLog::EndMessage;
		return tcu::TestStatus::fail("deviceFeatures incomplete initialization");
	}

	return tcu::TestStatus::pass("Query succeeded");
}

tcu::TestStatus deviceProperties (Context& context)
{
	TestLog&						log			= context.getTestContext().getLog();
	VkPhysicalDeviceProperties*		props;
	deUint8							buffer[sizeof(VkPhysicalDeviceProperties) + GUARD_SIZE];

	props = reinterpret_cast<VkPhysicalDeviceProperties*>(buffer);
	deMemset(props, GUARD_VALUE, sizeof(buffer));

	context.getInstanceInterface().getPhysicalDeviceProperties(context.getPhysicalDevice(), props);

	log << TestLog::Message << "device = " << context.getPhysicalDevice() << TestLog::EndMessage
		<< TestLog::Message << *props << TestLog::EndMessage;

	if (!validateFeatureLimits(props, log))
		return tcu::TestStatus::fail("deviceProperties - feature limits failed");

	for (int ndx = 0; ndx < GUARD_SIZE; ndx++)
	{
		if (buffer[ndx + sizeof(VkPhysicalDeviceProperties)] != GUARD_VALUE)
		{
			log << TestLog::Message << "deviceProperties - Guard offset " << ndx << " not valid" << TestLog::EndMessage;
			return tcu::TestStatus::fail("deviceProperties buffer overflow");
		}
	}

	if (!validateInitComplete(context.getPhysicalDevice(), &InstanceInterface::getPhysicalDeviceProperties, context.getInstanceInterface()))
	{
		log << TestLog::Message << "deviceProperties - VkPhysicalDeviceProperties not completely initialized" << TestLog::EndMessage;
		return tcu::TestStatus::fail("deviceProperties incomplete initialization");
	}

	return tcu::TestStatus::pass("DeviceProperites query succeeded");
}

tcu::TestStatus deviceQueueFamilyProperties (Context& context)
{
	TestLog&								log					= context.getTestContext().getLog();
	const vector<VkQueueFamilyProperties>	queueProperties		= getPhysicalDeviceQueueFamilyProperties(context.getInstanceInterface(), context.getPhysicalDevice());

	log << TestLog::Message << "device = " << context.getPhysicalDevice() << TestLog::EndMessage;

	for (size_t queueNdx = 0; queueNdx < queueProperties.size(); queueNdx++)
		log << TestLog::Message << queueNdx << ": " << queueProperties[queueNdx] << TestLog::EndMessage;

	return tcu::TestStatus::pass("Querying queue properties succeeded");
}

tcu::TestStatus deviceMemoryProperties (Context& context)
{
	TestLog&							log			= context.getTestContext().getLog();
	VkPhysicalDeviceMemoryProperties*	memProps;
	deUint8								buffer[sizeof(VkPhysicalDeviceMemoryProperties) + GUARD_SIZE];

	memProps = reinterpret_cast<VkPhysicalDeviceMemoryProperties*>(buffer);
	deMemset(buffer, GUARD_VALUE, sizeof(buffer));

	context.getInstanceInterface().getPhysicalDeviceMemoryProperties(context.getPhysicalDevice(), memProps);

	log << TestLog::Message << "device = " << context.getPhysicalDevice() << TestLog::EndMessage
		<< TestLog::Message << *memProps << TestLog::EndMessage;

	for (deInt32 ndx = 0; ndx < GUARD_SIZE; ndx++)
	{
		if (buffer[ndx + sizeof(VkPhysicalDeviceMemoryProperties)] != GUARD_VALUE)
		{
			log << TestLog::Message << "deviceMemoryProperties - Guard offset " << ndx << " not valid" << TestLog::EndMessage;
			return tcu::TestStatus::fail("deviceMemoryProperties buffer overflow");
		}
	}

	return tcu::TestStatus::pass("Querying memory properties succeeded");
}

} // anonymous

tcu::TestCaseGroup* createFeatureInfoTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	infoTests	(new tcu::TestCaseGroup(testCtx, "info", "Platform Information Tests"));

	{
		de::MovePtr<tcu::TestCaseGroup> instanceInfoTests	(new tcu::TestCaseGroup(testCtx, "instance", "Instance Information Tests"));

		addFunctionCase(instanceInfoTests.get(), "physical_devices",		"Physical devices",			enumeratePhysicalDevices);
		addFunctionCase(instanceInfoTests.get(), "layers",					"Layers",					enumerateInstanceLayers);
		addFunctionCase(instanceInfoTests.get(), "extensions",				"Extensions",				enumerateInstanceExtensions);

		infoTests->addChild(instanceInfoTests.release());
	}

	{
		de::MovePtr<tcu::TestCaseGroup> deviceInfoTests	(new tcu::TestCaseGroup(testCtx, "device", "Device Information Tests"));

		addFunctionCase(deviceInfoTests.get(), "features",					"Device Features",			deviceFeatures);
		addFunctionCase(deviceInfoTests.get(), "properties",				"Device Properties",		deviceProperties);
		addFunctionCase(deviceInfoTests.get(), "queue_family_properties",	"Queue family properties",	deviceQueueFamilyProperties);
		addFunctionCase(deviceInfoTests.get(), "memory_properties",			"Memory properties",		deviceMemoryProperties);
		addFunctionCase(deviceInfoTests.get(), "layers",					"Layers",					enumerateDeviceLayers);
		addFunctionCase(deviceInfoTests.get(), "extensions",				"Extensions",				enumerateDeviceExtensions);

		infoTests->addChild(deviceInfoTests.release());
	}

	return infoTests.release();
}

} // api
} // vkt
