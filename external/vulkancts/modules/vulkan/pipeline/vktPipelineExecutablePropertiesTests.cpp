/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
 * Copyright (c) 2019 Intel Corporation.
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
 * \brief VK_KHR_pipeline_executable_properties
 *
 * These tests creates compute and graphics pipelines with a variety of
 * stages both with and without a pipeline cache and exercise the new
 * queries provided by VK_KHR_pipeline_executable_properties.
 *
 * For each query type, it asserts that the query works and doesn't crash
 * and returns consistent results:
 *
 *  - The tests assert that the same set of pipeline executables is
 *    reported regardless of whether or not a pipeline cache is used.
 *
 *  - For each pipeline executable, the tests assert that the same set of
 *    statistics is returned regardless of whether or not a pipeline cache
 *    is used.
 *
 *  - For each pipeline executable, the tests assert that the same set of
 *    statistics is returned regardless of whether or not
 *    CAPTURE_INTERNAL_REPRESENTATIONS_BIT is set.
 *
 *  - For each pipeline executable, the tests assert that the same set of
 *    internal representations is returned regardless of whether or not a
 *    pipeline cache is used.
 *
 *  - For each string returned (statistic names, etc.) the tests assert
 *    that the string is NULL terminated.
 *
 *  - For each statistic, the tests compare the results of the two
 *    compilations and report any differences.  (Statistics differing
 *    between two compilations is not considered a failure.)
 *
 *  - For each binary internal representation, the tests attempt to assert
 *    that the amount of data returned by the implementation matches the
 *    amount the implementation claims.  (It's impossible to exactly do
 *    this but the tests give it a good try.)
 *
 * All of the returned data is recorded in the output file.
 *
 *//*--------------------------------------------------------------------*/

#include "vktPipelineExecutablePropertiesTests.hpp"
#include "vktPipelineVertexUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkObjUtil.hpp"
#include "tcuTestLog.hpp"

#include <sstream>
#include <vector>

namespace vkt
{
namespace pipeline
{

using namespace vk;

namespace
{
enum
{
	VK_MAX_SHADER_STAGES = 6,
};

enum
{
	PIPELINE_CACHE_NDX_INITIAL = 0,
	PIPELINE_CACHE_NDX_CACHED = 1,
	PIPELINE_CACHE_NDX_COUNT,
};

// helper functions

std::string getShaderFlagStr(const VkShaderStageFlags	shader,
							 bool						isDescription)
{
	std::ostringstream desc;
	if (shader & VK_SHADER_STAGE_COMPUTE_BIT)
	{
		desc << ((isDescription) ? "compute stage" : "compute_stage");
	}
	else
	{
		desc << ((isDescription) ? "vertex stage" : "vertex_stage");
		if (shader & VK_SHADER_STAGE_GEOMETRY_BIT)
			desc << ((isDescription) ? " geometry stage" : "_geometry_stage");
		if (shader & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
			desc << ((isDescription) ? " tessellation control stage" : "_tessellation_control_stage");
		if (shader & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
			desc << ((isDescription) ? " tessellation evaluation stage" : "_tessellation_evaluation_stage");
		desc << ((isDescription) ? " fragment stage" : "_fragment_stage");
	}

	return desc.str();
}

std::string getShaderFlagsStr (const VkShaderStageFlags flags)
{
	std::ostringstream stream;
	bool empty = true;
	for (deUint32 b = 0; b < 8 * sizeof(flags); b++)
	{
		if (flags & (1u << b))
		{
			if (empty)
			{
				empty = false;
			}
			else
			{
				stream << ", ";
			}

			stream << getShaderFlagStr((VkShaderStageFlagBits)(1u << b), true);
		}
	}

	if (empty)
	{
		stream << "none";
	}

	return stream.str();
}

// helper classes
class ExecutablePropertiesTestParam
{
public:
								ExecutablePropertiesTestParam	(PipelineConstructionType		pipelineConstructionType,
																 const VkShaderStageFlags		shaders,
																 deBool							testStatistics,
																 deBool							testInternalRepresentations);
	virtual						~ExecutablePropertiesTestParam	(void)	= default;
	virtual const std::string	generateTestName				(void)	const;
	virtual const std::string	generateTestDescription			(void)	const;
	PipelineConstructionType	getPipelineConstructionType		(void)	const	{ return m_pipelineConstructionType; }
	VkShaderStageFlags			getShaderFlags					(void)	const	{ return m_shaders; }
	deBool						getTestStatistics				(void)	const	{ return m_testStatistics; }
	deBool						getTestInternalRepresentations	(void)	const	{ return m_testInternalRepresentations; }

protected:
	PipelineConstructionType	m_pipelineConstructionType;
	VkShaderStageFlags			m_shaders;
	bool						m_testStatistics;
	bool						m_testInternalRepresentations;
};

ExecutablePropertiesTestParam::ExecutablePropertiesTestParam (PipelineConstructionType pipelineConstructionType, const VkShaderStageFlags shaders, deBool testStatistics, deBool testInternalRepresentations)
	: m_pipelineConstructionType	(pipelineConstructionType)
	, m_shaders						(shaders)
	, m_testStatistics				(testStatistics)
	, m_testInternalRepresentations	(testInternalRepresentations)
{
}

const std::string ExecutablePropertiesTestParam::generateTestName (void) const
{
	std::string result(getShaderFlagStr(m_shaders, false));

	if (m_testStatistics)
		result += "_statistics";
	if (m_testInternalRepresentations)
		result += "_internal_representations";

	return result;
}

const std::string ExecutablePropertiesTestParam::generateTestDescription (void) const
{
	std::string result;
	if (m_testStatistics)
	{
		result += "Get pipeline executable statistics";
		if (m_testInternalRepresentations)
		{
			result += " and internal representations";
		}
	}
	else if (m_testInternalRepresentations)
	{
		result += "Get pipeline executable internal representations";
	}
	else
	{
		result += "Get pipeline executable properties";
	}

	result += " with " + getShaderFlagStr(m_shaders, true);

	return result;
}

template <class Test>
vkt::TestCase* newTestCase (tcu::TestContext&		testContext,
							const ExecutablePropertiesTestParam*	testParam)
{
	return new Test(testContext,
					testParam->generateTestName().c_str(),
					testParam->generateTestDescription().c_str(),
					testParam);
}

// Test Classes
class ExecutablePropertiesTest : public vkt::TestCase
{
public:
							ExecutablePropertiesTest(tcu::TestContext&		testContext,
										   const std::string&		name,
										   const std::string&		description,
										   const ExecutablePropertiesTestParam*	param)
								: vkt::TestCase (testContext, name, description)
								, m_param (*param)
								{ }
	virtual					~ExecutablePropertiesTest (void) { }
protected:
	const ExecutablePropertiesTestParam	m_param;
};

class ExecutablePropertiesTestInstance : public vkt::TestInstance
{
public:
							ExecutablePropertiesTestInstance			(Context&				context,
															 const ExecutablePropertiesTestParam*	param);
	virtual					~ExecutablePropertiesTestInstance			(void);
	virtual tcu::TestStatus iterate							(void);
protected:
	virtual tcu::TestStatus verifyStatistics				(deUint32 binaryNdx);
	virtual tcu::TestStatus verifyInternalRepresentations	(deUint32 binaryNdx);
	virtual tcu::TestStatus verifyTestResult				(void);
protected:
	const ExecutablePropertiesTestParam*	m_param;

	Move<VkPipelineCache>	m_cache;
	deBool					m_extensions;

	Move<VkPipeline>		m_pipeline[PIPELINE_CACHE_NDX_COUNT];

	virtual VkPipeline		getPipeline(deUint32 ndx) { return m_pipeline[ndx].get(); }
};

ExecutablePropertiesTestInstance::ExecutablePropertiesTestInstance (Context&					context,
												const ExecutablePropertiesTestParam*	param)
	: TestInstance		(context)
	, m_param			(param)
	, m_extensions		(m_context.requireDeviceFunctionality("VK_KHR_pipeline_executable_properties"))
{
	const DeviceInterface&	vk				= m_context.getDeviceInterface();
	const VkDevice			vkDevice		= m_context.getDevice();

	const VkPipelineCacheCreateInfo pipelineCacheCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,			// VkStructureType				sType;
		DE_NULL,												// const void*					pNext;
		0u,														// VkPipelineCacheCreateFlags	flags;
		0u,														// deUintptr					initialDataSize;
		DE_NULL,												// const void*					pInitialData;
	};

	m_cache = createPipelineCache(vk, vkDevice, &pipelineCacheCreateInfo);
}

ExecutablePropertiesTestInstance::~ExecutablePropertiesTestInstance (void)
{
}

tcu::TestStatus ExecutablePropertiesTestInstance::iterate (void)
{
	return verifyTestResult();
}

bool
checkString(const char *string, size_t size)
{
	size_t i = 0;
	for (; i < size; i++)
	{
		if (string[i] == 0)
		{
			break;
		}
	}

	// The string needs to be non-empty and null terminated
	if (i == 0 || i >= size)
	{
		return false;
	}

	return true;
}

tcu::TestStatus ExecutablePropertiesTestInstance::verifyStatistics (deUint32 executableNdx)
{
	const DeviceInterface&		vk			= m_context.getDeviceInterface();
	const VkDevice				vkDevice	= m_context.getDevice();
	tcu::TestLog				&log		= m_context.getTestContext().getLog();

	std::vector<VkPipelineExecutableStatisticKHR> statistics[PIPELINE_CACHE_NDX_COUNT];

	for (deUint32 ndx = 0; ndx < PIPELINE_CACHE_NDX_COUNT; ndx++)
	{
		const VkPipelineExecutableInfoKHR pipelineExecutableInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_INFO_KHR,	// VkStructureType					sType;
			DE_NULL,										// const void*						pNext;
			getPipeline(ndx),								// VkPipeline						pipeline;
			executableNdx,									// uint32_t							executableIndex;
		};

		deUint32 statisticCount = 0;
		VK_CHECK(vk.getPipelineExecutableStatisticsKHR(vkDevice, &pipelineExecutableInfo, &statisticCount, DE_NULL));

		if (statisticCount == 0)
		{
			continue;
		}

		statistics[ndx].resize(statisticCount);
		for (deUint32 statNdx = 0; statNdx < statisticCount; statNdx++)
		{
			deMemset(&statistics[ndx][statNdx], 0, sizeof(statistics[ndx][statNdx]));
			statistics[ndx][statNdx].sType = VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_STATISTIC_KHR;
			statistics[ndx][statNdx].pNext = DE_NULL;
		}
		VK_CHECK(vk.getPipelineExecutableStatisticsKHR(vkDevice, &pipelineExecutableInfo, &statisticCount, &statistics[ndx][0]));

		for (deUint32 statNdx = 0; statNdx < statisticCount; statNdx++)
		{
			if (!checkString(statistics[ndx][statNdx].name, DE_LENGTH_OF_ARRAY(statistics[ndx][statNdx].name)))
			{
				return tcu::TestStatus::fail("Invalid statistic name string");
			}

			for (deUint32 otherNdx = 0; otherNdx < statNdx; otherNdx++)
			{
				if (deMemCmp(statistics[ndx][statNdx].name, statistics[ndx][otherNdx].name,
							 DE_LENGTH_OF_ARRAY(statistics[ndx][statNdx].name)) == 0)
				{
					return tcu::TestStatus::fail("Statistic name string not unique within the executable");
				}
			}

			if (!checkString(statistics[ndx][statNdx].description, DE_LENGTH_OF_ARRAY(statistics[ndx][statNdx].description)))
			{
				return tcu::TestStatus::fail("Invalid statistic description string");
			}

			if (statistics[ndx][statNdx].format == VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_BOOL32_KHR)
			{
				if (statistics[ndx][statNdx].value.b32 != VK_TRUE && statistics[ndx][statNdx].value.b32 != VK_FALSE)
				{
					return tcu::TestStatus::fail("Boolean statistic is neither VK_TRUE nor VK_FALSE");
				}
			}
		}
	}

	if (statistics[0].size() != statistics[1].size())
	{
		return tcu::TestStatus::fail("Identical pipelines have different numbers of statistics");
	}

	if (statistics[0].size() == 0)
	{
		return tcu::TestStatus::pass("No statistics reported");
	}

	// Both compiles had better have specified the same infos
	for (deUint32 statNdx0 = 0; statNdx0 < statistics[0].size(); statNdx0++)
	{
		deUint32 statNdx1 = 0;
		for (; statNdx1 < statistics[1].size(); statNdx1++)
		{
			if (deMemCmp(statistics[0][statNdx0].name, statistics[1][statNdx1].name,
						 DE_LENGTH_OF_ARRAY(statistics[0][statNdx0].name)) == 0)
			{
				break;
			}
		}
		if (statNdx1 >= statistics[1].size())
		{
			return tcu::TestStatus::fail("Identical pipelines have different statistics");
		}

		if (deMemCmp(statistics[0][statNdx0].description, statistics[1][statNdx1].description,
					 DE_LENGTH_OF_ARRAY(statistics[0][statNdx0].description)) != 0)
		{
			return tcu::TestStatus::fail("Invalid binary description string");
		}

		if (statistics[0][statNdx0].format != statistics[1][statNdx1].format)
		{
			return tcu::TestStatus::fail("Identical pipelines have statistics with different formats");
		}

		switch (statistics[0][statNdx0].format)
		{
			case VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_BOOL32_KHR:
			{
				bool match = statistics[0][statNdx0].value.b32 == statistics[1][statNdx1].value.b32;
				log << tcu::TestLog::Message
					<< statistics[0][statNdx0].name << ": "
					<< (statistics[0][statNdx0].value.b32 ? "VK_TRUE" : "VK_FALSE")
					<< (match ? "" : " (non-deterministic)")
					<< " (" << statistics[0][statNdx0].description << ")"
					<< tcu::TestLog::EndMessage;
				break;
			}
			case VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_INT64_KHR:
			{
				bool match = statistics[0][statNdx0].value.i64 == statistics[1][statNdx1].value.i64;
				log << tcu::TestLog::Message
					<< statistics[0][statNdx0].name << ": "
					<< statistics[0][statNdx0].value.i64
					<< (match ? "" : " (non-deterministic)")
					<< " (" << statistics[0][statNdx0].description << ")"
					<< tcu::TestLog::EndMessage;
				break;
			}
			case VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_UINT64_KHR:
			{
				bool match = statistics[0][statNdx0].value.u64 == statistics[1][statNdx1].value.u64;
				log << tcu::TestLog::Message
					<< statistics[0][statNdx0].name << ": "
					<< statistics[0][statNdx0].value.u64
					<< (match ? "" : " (non-deterministic)")
					<< " (" << statistics[0][statNdx0].description << ")"
					<< tcu::TestLog::EndMessage;
				break;
			}
			case VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_FLOAT64_KHR:
			{
				bool match = statistics[0][statNdx0].value.f64 == statistics[1][statNdx1].value.f64;
				log << tcu::TestLog::Message
					<< statistics[0][statNdx0].name << ": "
					<< statistics[0][statNdx0].value.f64
					<< (match ? "" : " (non-deterministic)")
					<< " (" << statistics[0][statNdx0].description << ")"
					<< tcu::TestLog::EndMessage;
				break;
			}
			default:
				return tcu::TestStatus::fail("Invalid statistic format");
		}
	}

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus ExecutablePropertiesTestInstance::verifyInternalRepresentations (deUint32 executableNdx)
{
	const DeviceInterface&		vk			= m_context.getDeviceInterface();
	const VkDevice				vkDevice	= m_context.getDevice();
	tcu::TestLog				&log		= m_context.getTestContext().getLog();

	// We only care about internal representations on the second pipeline.
	// We still compile twice to ensure that we still get the right thing
	// even if the pipeline is hot in the cache.
	const VkPipelineExecutableInfoKHR pipelineExecutableInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_INFO_KHR,	// VkStructureType					sType;
		DE_NULL,										// const void*						pNext;
		getPipeline(1),									// VkPipeline						pipeline;
		executableNdx,									// uint32_t							executableIndex;
	};

	std::vector<VkPipelineExecutableInternalRepresentationKHR> irs;
	std::vector<std::vector<deUint8>> irDatas;

	deUint32 irCount = 0;
	VK_CHECK(vk.getPipelineExecutableInternalRepresentationsKHR(vkDevice, &pipelineExecutableInfo, &irCount, DE_NULL));

	if (irCount == 0)
	{
		return tcu::TestStatus::pass("No internal representations reported");
	}

	irs.resize(irCount);
	irDatas.resize(irCount);
	for (deUint32 irNdx = 0; irNdx < irCount; irNdx++)
	{
		deMemset(&irs[irNdx], 0, sizeof(irs[irNdx]));
		irs[irNdx].sType = VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_INTERNAL_REPRESENTATION_KHR;
		irs[irNdx].pNext = DE_NULL;
	}
	VK_CHECK(vk.getPipelineExecutableInternalRepresentationsKHR(vkDevice, &pipelineExecutableInfo, &irCount, &irs[0]));

	for (deUint32 irNdx = 0; irNdx < irCount; irNdx++)
	{
		if (!checkString(irs[irNdx].name, DE_LENGTH_OF_ARRAY(irs[irNdx].name)))
		{
			return tcu::TestStatus::fail("Invalid internal representation name string");
		}

		for (deUint32 otherNdx = 0; otherNdx < irNdx; otherNdx++)
		{
			if (deMemCmp(irs[irNdx].name, irs[otherNdx].name,
						 DE_LENGTH_OF_ARRAY(irs[irNdx].name)) == 0)
			{
				return tcu::TestStatus::fail("Internal representation name string not unique within the executable");
			}
		}

		if (!checkString(irs[irNdx].description, DE_LENGTH_OF_ARRAY(irs[irNdx].description)))
		{
			return tcu::TestStatus::fail("Invalid binary description string");
		}

		if (irs[irNdx].dataSize == 0)
		{
			return tcu::TestStatus::fail("Internal representation has no data");
		}

		irDatas[irNdx].resize(irs[irNdx].dataSize);
		irs[irNdx].pData = &irDatas[irNdx][0];
		if (irs[irNdx].isText)
		{
			// For binary data the size is important.  We check that the
			// implementation fills the whole buffer by filling it with
			// garbage first and then looking for that same garbage later.
			for (size_t i = 0; i < irs[irNdx].dataSize; i++)
			{
				irDatas[irNdx][i] = (deUint8)(37 * (17 + i));
			}
		}
	}

	VK_CHECK(vk.getPipelineExecutableInternalRepresentationsKHR(vkDevice, &pipelineExecutableInfo, &irCount, &irs[0]));

	for (deUint32 irNdx = 0; irNdx < irCount; irNdx++)
	{
		if (irs[irNdx].isText)
		{
			if (!checkString((char *)irs[irNdx].pData, irs[irNdx].dataSize))
			{
				return tcu::TestStatus::fail("Textual internal representation isn't a valid string");
			}
			log << tcu::TestLog::Section(irs[irNdx].name, irs[irNdx].description)
				<< tcu::LogKernelSource((char *)irs[irNdx].pData)
				<< tcu::TestLog::EndSection;
		}
		else
		{
			size_t maxMatchingChunkSize = 0;
			size_t matchingChunkSize = 0;
			for (size_t i = 0; i < irs[irNdx].dataSize; i++)
			{
				if (irDatas[irNdx][i] == (deUint8)(37 * (17 + i)))
				{
					matchingChunkSize++;
					if (matchingChunkSize > maxMatchingChunkSize)
					{
						maxMatchingChunkSize = matchingChunkSize;
					}
				}
				else
				{
					matchingChunkSize = 0;
				}
			}

			// 64 bytes of our random data still being in the buffer probably
			// isn't a coincidence
			if (matchingChunkSize == irs[irNdx].dataSize || matchingChunkSize >= 64)
			{
				return tcu::TestStatus::fail("Implementation didn't fill the whole internal representation data buffer");
			}

			log << tcu::TestLog::Section(irs[irNdx].name, irs[irNdx].description)
				<< tcu::TestLog::Message << "Received " << irs[irNdx].dataSize << "B of binary data" << tcu::TestLog::EndMessage
				<< tcu::TestLog::EndSection;
		}
	}

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus ExecutablePropertiesTestInstance::verifyTestResult (void)
{
	const DeviceInterface&		vk			= m_context.getDeviceInterface();
	const VkDevice				vkDevice	= m_context.getDevice();
	tcu::TestLog				&log		= m_context.getTestContext().getLog();

	std::vector<VkPipelineExecutablePropertiesKHR> props[PIPELINE_CACHE_NDX_COUNT];

	for (deUint32 ndx = 0; ndx < PIPELINE_CACHE_NDX_COUNT; ndx++)
	{
		const VkPipelineInfoKHR pipelineInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_INFO_KHR,	// VkStructureType					sType;
			DE_NULL,								// const void*						pNext;
			getPipeline(ndx),						// VkPipeline						pipeline;

		};
		deUint32 executableCount = 0;
		VK_CHECK(vk.getPipelineExecutablePropertiesKHR(vkDevice, &pipelineInfo, &executableCount, DE_NULL));

		if (executableCount == 0)
		{
			continue;
		}

		props[ndx].resize(executableCount);
		for (deUint32 execNdx = 0; execNdx < executableCount; execNdx++)
		{
			deMemset(&props[ndx][execNdx], 0, sizeof(props[ndx][execNdx]));
			props[ndx][execNdx].sType = VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_PROPERTIES_KHR;
			props[ndx][execNdx].pNext = DE_NULL;
		}
		VK_CHECK(vk.getPipelineExecutablePropertiesKHR(vkDevice, &pipelineInfo, &executableCount, &props[ndx][0]));

		for (deUint32 execNdx = 0; execNdx < executableCount; execNdx++)
		{
			if (!checkString(props[ndx][execNdx].name, DE_LENGTH_OF_ARRAY(props[ndx][execNdx].name)))
			{
				return tcu::TestStatus::fail("Invalid binary name string");
			}

			for (deUint32 otherNdx = 0; otherNdx < execNdx; otherNdx++)
			{
				if (deMemCmp(props[ndx][execNdx].name, props[ndx][otherNdx].name,
							 DE_LENGTH_OF_ARRAY(props[ndx][execNdx].name)) == 0)
				{
					return tcu::TestStatus::fail("Binary name string not unique within the pipeline");
				}
			}

			if (!checkString(props[ndx][execNdx].description, DE_LENGTH_OF_ARRAY(props[ndx][execNdx].description)))
			{
				return tcu::TestStatus::fail("Invalid binary description string");
			}

			// Check that the binary only contains stages actually used to
			// compile the pipeline
			VkShaderStageFlags stages = props[ndx][execNdx].stages;
			stages &= ~m_param->getShaderFlags();
			if (stages != 0)
			{
				return tcu::TestStatus::fail("Binary uses unprovided stage");
			}
		}
	}

	if (props[0].size() != props[1].size())
	{
		return tcu::TestStatus::fail("Identical pipelines have different numbers of props");
	}

	if (props[0].size() == 0)
	{
		return tcu::TestStatus::pass("No executables reported");
	}

	// Both compiles had better have specified the same infos
	for (deUint32 execNdx0 = 0; execNdx0 < props[0].size(); execNdx0++)
	{
		deUint32 execNdx1 = 0;
		for (; execNdx1 < props[1].size(); execNdx1++)
		{
			if (deMemCmp(props[0][execNdx0].name, props[1][execNdx1].name,
						 DE_LENGTH_OF_ARRAY(props[0][execNdx0].name)) == 0)
			{
				break;
			}
		}
		if (execNdx1 >= props[1].size())
		{
			return tcu::TestStatus::fail("Identical pipelines have different sets of executables");
		}

		if (deMemCmp(props[0][execNdx0].description, props[1][execNdx1].description,
					 DE_LENGTH_OF_ARRAY(props[0][execNdx0].description)) != 0)
		{
			return tcu::TestStatus::fail("Same binary has different descriptions");
		}

		if (props[0][execNdx0].stages != props[1][execNdx1].stages)
		{
			return tcu::TestStatus::fail("Same binary has different stages");
		}

		if (props[0][execNdx0].subgroupSize != props[1][execNdx1].subgroupSize)
		{
			return tcu::TestStatus::fail("Same binary has different subgroup sizes");
		}
	}

	log << tcu::TestLog::Section("Binaries", "Binaries reported for this pipeline");
	log << tcu::TestLog::Message << "Pipeline reported " << props[0].size() << " props" << tcu::TestLog::EndMessage;

	tcu::TestStatus status = tcu::TestStatus::pass("Pass");
	for (deUint32 execNdx = 0; execNdx < props[0].size(); execNdx++)
	{
		log << tcu::TestLog::Section(props[0][execNdx].name, props[0][execNdx].description);
		log << tcu::TestLog::Message << "Name: " << props[0][execNdx].name << tcu::TestLog::EndMessage;
		log << tcu::TestLog::Message << "Description: " << props[0][execNdx].description << tcu::TestLog::EndMessage;
		log << tcu::TestLog::Message << "Stages: " << getShaderFlagsStr(props[0][execNdx].stages) << tcu::TestLog::EndMessage;
		log << tcu::TestLog::Message << "Subgroup Size: " << props[0][execNdx].subgroupSize << tcu::TestLog::EndMessage;

		if (m_param->getTestStatistics())
		{
			status = verifyStatistics(execNdx);
			if (status.getCode() != QP_TEST_RESULT_PASS)
			{
				log << tcu::TestLog::EndSection;
				break;
			}
		}

		if (m_param->getTestInternalRepresentations())
		{
			status = verifyInternalRepresentations(execNdx);
			if (status.getCode() != QP_TEST_RESULT_PASS)
			{
				log << tcu::TestLog::EndSection;
				break;
			}
		}

		log << tcu::TestLog::EndSection;
	}

	log << tcu::TestLog::EndSection;

	return status;
}

class GraphicsExecutablePropertiesTest : public ExecutablePropertiesTest
{
public:
							GraphicsExecutablePropertiesTest	(tcu::TestContext&		testContext,
													 const std::string&	name,
													 const std::string&	description,
													 const ExecutablePropertiesTestParam*	param)
								: ExecutablePropertiesTest (testContext, name, description, param)
								{ }
	virtual					~GraphicsExecutablePropertiesTest	(void) { }
	virtual void			initPrograms		(SourceCollections&	programCollection) const;
	virtual TestInstance*	createInstance		(Context&				context) const;
	void					checkSupport		(Context& context) const;
};

class GraphicsExecutablePropertiesTestInstance : public ExecutablePropertiesTestInstance
{
public:
							GraphicsExecutablePropertiesTestInstance	(Context&								context,
																		 const ExecutablePropertiesTestParam*	param);
	virtual					~GraphicsExecutablePropertiesTestInstance	(void);

protected:
	void					preparePipelineWrapper						(GraphicsPipelineWrapper&	gpw,
																		 ShaderWrapper				vertShaderModule,
																		 ShaderWrapper				tescShaderModule,
																		 ShaderWrapper				teseShaderModule,
																		 ShaderWrapper				geomShaderModule,
																		 ShaderWrapper				fragShaderModule);

	VkPipeline				getPipeline									(deUint32 ndx) override
	{
		return m_pipelineWrapper[ndx].getPipeline();
	};

protected:
	const tcu::UVec2					m_renderSize;
	const VkFormat						m_colorFormat;
	const VkFormat						m_depthFormat;
	PipelineLayoutWrapper				m_pipelineLayout;
	GraphicsPipelineWrapper				m_pipelineWrapper[PIPELINE_CACHE_NDX_COUNT];
	Move<VkRenderPass>					m_renderPass;
};

void GraphicsExecutablePropertiesTest::initPrograms (SourceCollections& programCollection) const
{
	programCollection.glslSources.add("color_vert") << glu::VertexSource(
				"#version 310 es\n"
				"layout(location = 0) in vec4 position;\n"
				"layout(location = 1) in vec4 color;\n"
				"layout(location = 0) out highp vec4 vtxColor;\n"
				"void main (void)\n"
				"{\n"
				"  gl_Position = position;\n"
				"  vtxColor = color;\n"
				"}\n");

	programCollection.glslSources.add("color_frag") << glu::FragmentSource(
				"#version 310 es\n"
				"layout(location = 0) in highp vec4 vtxColor;\n"
				"layout(location = 0) out highp vec4 fragColor;\n"
				"void main (void)\n"
				"{\n"
				"  fragColor = vtxColor;\n"
				"}\n");

	if (m_param.getShaderFlags() & VK_SHADER_STAGE_GEOMETRY_BIT)
	{
		programCollection.glslSources.add("dummy_geo") << glu::GeometrySource(
					"#version 450 \n"
					"layout(triangles) in;\n"
					"layout(triangle_strip, max_vertices = 3) out;\n"
					"layout(location = 0) in highp vec4 in_vtxColor[];\n"
					"layout(location = 0) out highp vec4 vtxColor;\n"
					"out gl_PerVertex { vec4 gl_Position; float gl_PointSize; };\n"
					"in gl_PerVertex { vec4 gl_Position; float gl_PointSize; } gl_in[];\n"
					"void main (void)\n"
					"{\n"
					"  for(int ndx=0; ndx<3; ndx++)\n"
					"  {\n"
					"    gl_Position = gl_in[ndx].gl_Position;\n"
					"    gl_PointSize = gl_in[ndx].gl_PointSize;\n"
					"    vtxColor    = in_vtxColor[ndx];\n"
					"    EmitVertex();\n"
					"  }\n"
					"  EndPrimitive();\n"
						"}\n");
	}
	if (m_param.getShaderFlags() & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
	{
		programCollection.glslSources.add("basic_tcs") << glu::TessellationControlSource(
					"#version 450 \n"
					"layout(vertices = 3) out;\n"
					"layout(location = 0) in highp vec4 color[];\n"
					"layout(location = 0) out highp vec4 vtxColor[];\n"
					"out gl_PerVertex { vec4 gl_Position; float gl_PointSize; } gl_out[3];\n"
					"in gl_PerVertex { vec4 gl_Position; float gl_PointSize; } gl_in[gl_MaxPatchVertices];\n"
					"void main()\n"
					"{\n"
					"  gl_TessLevelOuter[0] = 4.0;\n"
					"  gl_TessLevelOuter[1] = 4.0;\n"
					"  gl_TessLevelOuter[2] = 4.0;\n"
					"  gl_TessLevelInner[0] = 4.0;\n"
					"  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
					"  gl_out[gl_InvocationID].gl_PointSize = gl_in[gl_InvocationID].gl_PointSize;\n"
					"  vtxColor[gl_InvocationID] = color[gl_InvocationID];\n"
					"}\n");

		programCollection.glslSources.add("basic_tes") << glu::TessellationEvaluationSource(
					"#version 450 \n"
					"layout(triangles, fractional_even_spacing, ccw) in;\n"
					"layout(location = 0) in highp vec4 colors[];\n"
					"layout(location = 0) out highp vec4 vtxColor;\n"
					"out gl_PerVertex { vec4 gl_Position; float gl_PointSize; };\n"
					"in gl_PerVertex { vec4 gl_Position; float gl_PointSize; } gl_in[gl_MaxPatchVertices];\n"
					"void main() \n"
					"{\n"
					"  float u = gl_TessCoord.x;\n"
					"  float v = gl_TessCoord.y;\n"
					"  float w = gl_TessCoord.z;\n"
					"  vec4 pos = vec4(0);\n"
					"  vec4 color = vec4(0);\n"
					"  pos.xyz += u * gl_in[0].gl_Position.xyz;\n"
					"  color.xyz += u * colors[0].xyz;\n"
					"  pos.xyz += v * gl_in[1].gl_Position.xyz;\n"
					"  color.xyz += v * colors[1].xyz;\n"
					"  pos.xyz += w * gl_in[2].gl_Position.xyz;\n"
					"  color.xyz += w * colors[2].xyz;\n"
					"  pos.w = 1.0;\n"
					"  color.w = 1.0;\n"
					"  gl_Position = pos;\n"
					"  gl_PointSize = gl_in[0].gl_PointSize;"
					"  vtxColor = color;\n"
					"}\n");
	}
}

TestInstance* GraphicsExecutablePropertiesTest::createInstance (Context& context) const
{
	return new GraphicsExecutablePropertiesTestInstance(context, &m_param);
}

void GraphicsExecutablePropertiesTest::checkSupport(Context& context) const
{
	VkShaderStageFlags			shaderFlags	= m_param.getShaderFlags();
	VkPhysicalDeviceFeatures	features	= context.getDeviceFeatures();
	if ((shaderFlags & VK_SHADER_STAGE_GEOMETRY_BIT) && (features.geometryShader == VK_FALSE))
		TCU_THROW(NotSupportedError, "Geometry Shader Not Supported");
	if ((shaderFlags & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) ||
		(shaderFlags & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT))
	{
		if (features.tessellationShader == VK_FALSE)
			TCU_THROW(NotSupportedError, "Tessellation Not Supported");
	}

	checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), m_param.getPipelineConstructionType());
}

GraphicsExecutablePropertiesTestInstance::GraphicsExecutablePropertiesTestInstance (Context&					context,
																const ExecutablePropertiesTestParam*	param)
	: ExecutablePropertiesTestInstance		(context, param)
	, m_renderSize							(32u, 32u)
	, m_colorFormat							(VK_FORMAT_R8G8B8A8_UNORM)
	, m_depthFormat							(VK_FORMAT_D16_UNORM)
	, m_pipelineWrapper
	{
		{ m_context.getInstanceInterface(), m_context.getDeviceInterface(), m_context.getPhysicalDevice(), m_context.getDevice(), m_context.getDeviceExtensions(), param->getPipelineConstructionType(),
						(param->getTestStatistics() ? deUint32(VK_PIPELINE_CREATE_CAPTURE_STATISTICS_BIT_KHR) : 0u) },
		{ m_context.getInstanceInterface(), m_context.getDeviceInterface(), m_context.getPhysicalDevice(), m_context.getDevice(), m_context.getDeviceExtensions(),param->getPipelineConstructionType(),
						(param->getTestStatistics() ? deUint32(VK_PIPELINE_CREATE_CAPTURE_STATISTICS_BIT_KHR) : 0u) |
						// Only check gather internal representations on the second pipeline.
						// This way, it's more obvious if they failed to capture due to the pipeline being cached.
						(param->getTestInternalRepresentations() ? deUint32(VK_PIPELINE_CREATE_CAPTURE_INTERNAL_REPRESENTATIONS_BIT_KHR) : 0u) },
	}
{
	const DeviceInterface&	vk				= m_context.getDeviceInterface();
	const VkDevice			vkDevice		= m_context.getDevice();

	// Create pipeline layout
	{
		const VkPipelineLayoutCreateInfo pipelineLayoutParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType					sType;
			DE_NULL,											// const void*						pNext;
			0u,													// VkPipelineLayoutCreateFlags		flags;
			0u,													// deUint32							setLayoutCount;
			DE_NULL,											// const VkDescriptorSetLayout*		pSetLayouts;
			0u,													// deUint32							pushConstantRangeCount;
			DE_NULL												// const VkPushConstantRange*		pPushConstantRanges;
		};

		m_pipelineLayout = PipelineLayoutWrapper(m_param->getPipelineConstructionType(), vk, vkDevice, &pipelineLayoutParams);
	}

	// Create render pass
	m_renderPass = makeRenderPass(vk, vkDevice, m_colorFormat, m_depthFormat);

	// Bind shader stages
	ShaderWrapper vertShaderModule = ShaderWrapper(vk, vkDevice, context.getBinaryCollection().get("color_vert"), 0);
	ShaderWrapper fragShaderModule = ShaderWrapper(vk, vkDevice, context.getBinaryCollection().get("color_frag"), 0);
	ShaderWrapper tescShaderModule;
	ShaderWrapper teseShaderModule;
	ShaderWrapper geomShaderModule;

	VkShaderStageFlags shaderFlags = m_param->getShaderFlags();
	if (shaderFlags & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
		tescShaderModule = ShaderWrapper(vk, vkDevice, context.getBinaryCollection().get("basic_tcs"), 0);
	if (shaderFlags & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
		teseShaderModule = ShaderWrapper(vk, vkDevice, context.getBinaryCollection().get("basic_tes"), 0);
	if (shaderFlags & VK_SHADER_STAGE_GEOMETRY_BIT)
		geomShaderModule = ShaderWrapper(vk, vkDevice, context.getBinaryCollection().get("dummy_geo"), 0);

	for (deUint32 ndx = 0; ndx < PIPELINE_CACHE_NDX_COUNT; ndx++)
		preparePipelineWrapper(m_pipelineWrapper[ndx], vertShaderModule, tescShaderModule, teseShaderModule, geomShaderModule, fragShaderModule);
}

GraphicsExecutablePropertiesTestInstance::~GraphicsExecutablePropertiesTestInstance (void)
{
}

void GraphicsExecutablePropertiesTestInstance::preparePipelineWrapper(GraphicsPipelineWrapper&		gpw,
																	  ShaderWrapper					vertShaderModule,
																	  ShaderWrapper					tescShaderModule,
																	  ShaderWrapper					teseShaderModule,
																	  ShaderWrapper					geomShaderModule,
																	  ShaderWrapper					fragShaderModule)
{
	// Create pipeline
	const VkVertexInputBindingDescription vertexInputBindingDescription =
	{
		0u,									// deUint32				binding;
		sizeof(Vertex4RGBA),				// deUint32				strideInBytes;
		VK_VERTEX_INPUT_RATE_VERTEX,		// VkVertexInputRate	inputRate;
	};

	const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[2] =
	{
		{
			0u,									// deUint32 location;
			0u,									// deUint32 binding;
			VK_FORMAT_R32G32B32A32_SFLOAT,		// VkFormat format;
			0u									// deUint32 offsetInBytes;
		},
		{
			1u,									// deUint32 location;
			0u,									// deUint32 binding;
			VK_FORMAT_R32G32B32A32_SFLOAT,		// VkFormat format;
			DE_OFFSET_OF(Vertex4RGBA, color),	// deUint32 offsetInBytes;
		}
	};

	const VkPipelineVertexInputStateCreateInfo vertexInputStateParams
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType							sType;
		DE_NULL,														// const void*								pNext;
		0u,																// VkPipelineVertexInputStateCreateFlags	flags;
		1u,																// deUint32									vertexBindingDescriptionCount;
		&vertexInputBindingDescription,									// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
		2u,																// deUint32									vertexAttributeDescriptionCount;
		vertexInputAttributeDescriptions,								// const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
	};

	const std::vector<VkViewport>	viewport{ makeViewport(m_renderSize) };
	const std::vector<VkRect2D>		scissor{ makeRect2D(m_renderSize) };

	const VkPipelineColorBlendAttachmentState colorBlendAttachmentState
	{
		VK_FALSE,														// VkBool32		blendEnable;
		VK_BLEND_FACTOR_ONE,											// VkBlendFactor	srcColorBlendFactor;
		VK_BLEND_FACTOR_ZERO,											// VkBlendFactor	dstColorBlendFactor;
		VK_BLEND_OP_ADD,												// VkBlendOp		colorBlendOp;
		VK_BLEND_FACTOR_ONE,											// VkBlendFactor	srcAlphaBlendFactor;
		VK_BLEND_FACTOR_ZERO,											// VkBlendFactor	dstAlphaBlendFactor;
		VK_BLEND_OP_ADD,												// VkBlendOp		alphaBlendOp;
		VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT										// VkColorComponentFlags    colorWriteMask;
	};

	const VkPipelineColorBlendStateCreateInfo colorBlendStateParams
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType								sType;
		DE_NULL,													// const void*									pNext;
		0u,															// VkPipelineColorBlendStateCreateFlags			flags;
		VK_FALSE,													// VkBool32										logicOpEnable;
		VK_LOGIC_OP_COPY,											// VkLogicOp									logicOp;
		1u,															// deUint32										attachmentCount;
		&colorBlendAttachmentState,									// const VkPipelineColorBlendAttachmentState*	pAttachments;
		{ 0.0f, 0.0f, 0.0f, 0.0f },									// float										blendConst[4];
	};

	VkPipelineDepthStencilStateCreateInfo depthStencilStateParams
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, // VkStructureType							sType;
		DE_NULL,													// const void*								pNext;
		0u,															// VkPipelineDepthStencilStateCreateFlags	flags;
		VK_TRUE,													// VkBool32									depthTestEnable;
		VK_TRUE,													// VkBool32									depthWriteEnable;
		VK_COMPARE_OP_LESS_OR_EQUAL,								// VkCompareOp								depthCompareOp;
		VK_FALSE,													// VkBool32									depthBoundsTestEnable;
		VK_FALSE,													// VkBool32									stencilTestEnable;
		// VkStencilOpState front;
		{
			VK_STENCIL_OP_KEEP,		// VkStencilOp	failOp;
			VK_STENCIL_OP_KEEP,		// VkStencilOp	passOp;
			VK_STENCIL_OP_KEEP,		// VkStencilOp	depthFailOp;
			VK_COMPARE_OP_NEVER,	// VkCompareOp	compareOp;
			0u,						// deUint32		compareMask;
			0u,						// deUint32		writeMask;
			0u,						// deUint32		reference;
		},
		// VkStencilOpState back;
		{
			VK_STENCIL_OP_KEEP,		// VkStencilOp	failOp;
			VK_STENCIL_OP_KEEP,		// VkStencilOp	passOp;
			VK_STENCIL_OP_KEEP,		// VkStencilOp	depthFailOp;
			VK_COMPARE_OP_NEVER,	// VkCompareOp	compareOp;
			0u,						// deUint32		compareMask;
			0u,						// deUint32		writeMask;
			0u,						// deUint32		reference;
		},
		0.0f,														// float									minDepthBounds;
		1.0f,														// float									maxDepthBounds;
	};

	gpw.setDefaultTopology((!tescShaderModule.isSet()) ? VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST : VK_PRIMITIVE_TOPOLOGY_PATCH_LIST)
	   .setDefaultRasterizationState()
	   .setDefaultMultisampleState()
	   .setupVertexInputState(&vertexInputStateParams)
	   .setupPreRasterizationShaderState(viewport,
			scissor,
			m_pipelineLayout,
			*m_renderPass,
			0u,
			vertShaderModule,
			DE_NULL,
			tescShaderModule,
			teseShaderModule,
			geomShaderModule)
	   .setupFragmentShaderState(m_pipelineLayout, *m_renderPass, 0u, fragShaderModule, &depthStencilStateParams)
	   .setupFragmentOutputState(*m_renderPass, 0u, &colorBlendStateParams)
	   .setMonolithicPipelineLayout(m_pipelineLayout)
	   .buildPipeline(*m_cache);
}

class ComputeExecutablePropertiesTest : public ExecutablePropertiesTest
{
public:
							ComputeExecutablePropertiesTest	(tcu::TestContext&		testContext,
													 const std::string&		name,
													 const std::string&		description,
													 const ExecutablePropertiesTestParam*	param)
								: ExecutablePropertiesTest	(testContext, name, description, param)
								{ }
	virtual					~ComputeExecutablePropertiesTest	(void) { }
	virtual void			initPrograms			(SourceCollections&	programCollection) const;
	virtual TestInstance*	createInstance			(Context&				context) const;
};

class ComputeExecutablePropertiesTestInstance : public ExecutablePropertiesTestInstance
{
public:
							ComputeExecutablePropertiesTestInstance	(Context&				context,
															 const ExecutablePropertiesTestParam*	param);
	virtual					~ComputeExecutablePropertiesTestInstance	(void);
protected:
	void					buildDescriptorSets				(deUint32 ndx);
	void					buildShader						(deUint32 ndx);
	void					buildPipeline					(deUint32 ndx);
protected:
	Move<VkBuffer>					m_inputBuf;
	de::MovePtr<Allocation>			m_inputBufferAlloc;
	Move<VkShaderModule>			m_computeShaderModule[PIPELINE_CACHE_NDX_COUNT];

	Move<VkBuffer>					m_outputBuf[PIPELINE_CACHE_NDX_COUNT];
	de::MovePtr<Allocation>			m_outputBufferAlloc[PIPELINE_CACHE_NDX_COUNT];

	Move<VkDescriptorPool>			m_descriptorPool[PIPELINE_CACHE_NDX_COUNT];
	Move<VkDescriptorSetLayout>		m_descriptorSetLayout[PIPELINE_CACHE_NDX_COUNT];
	Move<VkDescriptorSet>			m_descriptorSet[PIPELINE_CACHE_NDX_COUNT];

	Move<VkPipelineLayout>			m_pipelineLayout[PIPELINE_CACHE_NDX_COUNT];
};

void ComputeExecutablePropertiesTest::initPrograms (SourceCollections& programCollection) const
{
	programCollection.glslSources.add("basic_compute") << glu::ComputeSource(
		"#version 310 es\n"
		"layout(local_size_x = 1) in;\n"
		"layout(std430) buffer;\n"
		"layout(binding = 0) readonly buffer Input0\n"
		"{\n"
		"  vec4 elements[];\n"
		"} input_data0;\n"
		"layout(binding = 1) writeonly buffer Output\n"
		"{\n"
		"  vec4 elements[];\n"
		"} output_data;\n"
		"void main()\n"
		"{\n"
		"  uint ident = gl_GlobalInvocationID.x;\n"
		"  output_data.elements[ident] = input_data0.elements[ident] * input_data0.elements[ident];\n"
		"}");
}

TestInstance* ComputeExecutablePropertiesTest::createInstance (Context& context) const
{
	return new ComputeExecutablePropertiesTestInstance(context, &m_param);
}

void ComputeExecutablePropertiesTestInstance::buildDescriptorSets (deUint32 ndx)
{
	const DeviceInterface&	vk				= m_context.getDeviceInterface();
	const VkDevice			vkDevice		= m_context.getDevice();

	// Create descriptor set layout
	DescriptorSetLayoutBuilder descLayoutBuilder;
	for (deUint32 bindingNdx = 0u; bindingNdx < 2u; bindingNdx++)
		descLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	m_descriptorSetLayout[ndx] = descLayoutBuilder.build(vk, vkDevice);
}

void ComputeExecutablePropertiesTestInstance::buildShader (deUint32 ndx)
{
	const DeviceInterface&	vk				= m_context.getDeviceInterface();
	const VkDevice			vkDevice		= m_context.getDevice();

	// Create compute shader
	VkShaderModuleCreateInfo shaderModuleCreateInfo =
	{
		VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,									// VkStructureType				sType;
		DE_NULL,																		// const void*					pNext;
		0u,																				// VkShaderModuleCreateFlags	flags;
		m_context.getBinaryCollection().get("basic_compute").getSize(),					// deUintptr					codeSize;
		(deUint32*)m_context.getBinaryCollection().get("basic_compute").getBinary(),	// const deUint32*				pCode;
	};
	m_computeShaderModule[ndx] = createShaderModule(vk, vkDevice, &shaderModuleCreateInfo);
}

void ComputeExecutablePropertiesTestInstance::buildPipeline (deUint32 ndx)
{
	const DeviceInterface&	vk				 = m_context.getDeviceInterface();
	const VkDevice			vkDevice		 = m_context.getDevice();

	// Create compute pipeline layout
	const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,					// VkStructureType					sType;
		DE_NULL,														// const void*						pNext;
		0u,																// VkPipelineLayoutCreateFlags		flags;
		1u,																// deUint32							setLayoutCount;
		&m_descriptorSetLayout[ndx].get(),								// const VkDescriptorSetLayout*		pSetLayouts;
		0u,																// deUint32							pushConstantRangeCount;
		DE_NULL,														// const VkPushConstantRange*		pPushConstantRanges;
	};

	m_pipelineLayout[ndx] = createPipelineLayout(vk, vkDevice, &pipelineLayoutCreateInfo);

	const VkPipelineShaderStageCreateInfo stageCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,			// VkStructureType					sType;
		DE_NULL,														// const void*						pNext;
		0u,																// VkPipelineShaderStageCreateFlags	flags;
		VK_SHADER_STAGE_COMPUTE_BIT,									// VkShaderStageFlagBits			stage;
		*m_computeShaderModule[ndx],									// VkShaderModule					module;
		"main",															// const char*						pName;
		DE_NULL,														// const VkSpecializationInfo*		pSpecializationInfo;
	};

	VkPipelineCreateFlags flags = 0;
	if (m_param->getTestStatistics())
	{
		flags |= VK_PIPELINE_CREATE_CAPTURE_STATISTICS_BIT_KHR;
	}

	// Only check gather internal representations on the second
	// pipeline.  This way, it's more obvious if they failed to capture
	// due to the pipeline being cached.
	if (ndx == PIPELINE_CACHE_NDX_CACHED && m_param->getTestInternalRepresentations())
	{
		flags |= VK_PIPELINE_CREATE_CAPTURE_INTERNAL_REPRESENTATIONS_BIT_KHR;
	}

	const VkComputePipelineCreateInfo pipelineCreateInfo =
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,				// VkStructureType					sType;
		DE_NULL,													// const void*						pNext;
		flags,														// VkPipelineCreateFlags			flags;
		stageCreateInfo,											// VkPipelineShaderStageCreateInfo	stage;
		*m_pipelineLayout[ndx],										// VkPipelineLayout					layout;
		(VkPipeline)0,												// VkPipeline						basePipelineHandle;
		0u,															// deInt32							basePipelineIndex;
	};

	m_pipeline[ndx] = createComputePipeline(vk, vkDevice, *m_cache, &pipelineCreateInfo, DE_NULL);
}

ComputeExecutablePropertiesTestInstance::ComputeExecutablePropertiesTestInstance (Context&				context,
													const ExecutablePropertiesTestParam*	param)
	: ExecutablePropertiesTestInstance (context, param)
{
	for (deUint32 ndx = 0; ndx < PIPELINE_CACHE_NDX_COUNT; ndx++)
	{
		buildDescriptorSets(ndx);
		buildShader(ndx);
		buildPipeline(ndx);
	}
}

ComputeExecutablePropertiesTestInstance::~ComputeExecutablePropertiesTestInstance (void)
{
}

} // anonymous

tcu::TestCaseGroup* createExecutablePropertiesTests (tcu::TestContext& testCtx, PipelineConstructionType pipelineConstructionType)
{

	de::MovePtr<tcu::TestCaseGroup> binaryInfoTests (new tcu::TestCaseGroup(testCtx, "executable_properties", "pipeline binary statistics tests"));

	// Graphics Pipeline Tests
	{
		de::MovePtr<tcu::TestCaseGroup> graphicsTests (new tcu::TestCaseGroup(testCtx, "graphics", "Test pipeline binary info with graphics pipeline."));

		const VkShaderStageFlags vertFragStages		= VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		const VkShaderStageFlags vertGeomFragStages	= vertFragStages | VK_SHADER_STAGE_GEOMETRY_BIT;
		const VkShaderStageFlags vertTessFragStages	= vertFragStages | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;

		const ExecutablePropertiesTestParam testParams[]
		{
			{ pipelineConstructionType, vertFragStages,		DE_FALSE, DE_FALSE },
			{ pipelineConstructionType, vertGeomFragStages,	DE_FALSE, DE_FALSE },
			{ pipelineConstructionType, vertTessFragStages,	DE_FALSE, DE_FALSE },
			{ pipelineConstructionType, vertFragStages,		DE_TRUE, DE_FALSE },
			{ pipelineConstructionType, vertGeomFragStages,	DE_TRUE, DE_FALSE },
			{ pipelineConstructionType, vertTessFragStages,	DE_TRUE, DE_FALSE },
			{ pipelineConstructionType, vertFragStages,		DE_FALSE, DE_TRUE },
			{ pipelineConstructionType, vertGeomFragStages,	DE_FALSE, DE_TRUE },
			{ pipelineConstructionType, vertTessFragStages,	DE_FALSE, DE_TRUE },
			{ pipelineConstructionType, vertFragStages,		DE_TRUE, DE_TRUE },
			{ pipelineConstructionType, vertGeomFragStages,	DE_TRUE, DE_TRUE },
			{ pipelineConstructionType, vertTessFragStages,	DE_TRUE, DE_TRUE },
		};

		for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(testParams); i++)
			graphicsTests->addChild(newTestCase<GraphicsExecutablePropertiesTest>(testCtx, &testParams[i]));

		binaryInfoTests->addChild(graphicsTests.release());
	}

	// Compute Pipeline Tests - don't repeat those tests for graphics pipeline library
	if (pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
	{
		de::MovePtr<tcu::TestCaseGroup> computeTests (new tcu::TestCaseGroup(testCtx, "compute", "Test pipeline binary info with compute pipeline."));

		const ExecutablePropertiesTestParam testParams[]
		{
			{ pipelineConstructionType, VK_SHADER_STAGE_COMPUTE_BIT, DE_FALSE, DE_FALSE },
			{ pipelineConstructionType, VK_SHADER_STAGE_COMPUTE_BIT, DE_TRUE, DE_FALSE },
			{ pipelineConstructionType, VK_SHADER_STAGE_COMPUTE_BIT, DE_FALSE, DE_TRUE },
			{ pipelineConstructionType, VK_SHADER_STAGE_COMPUTE_BIT, DE_TRUE, DE_TRUE },
		};

		for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(testParams); i++)
			computeTests->addChild(newTestCase<ComputeExecutablePropertiesTest>(testCtx, &testParams[i]));

		binaryInfoTests->addChild(computeTests.release());
	}

	return binaryInfoTests.release();
}

} // pipeline

} // vkt
