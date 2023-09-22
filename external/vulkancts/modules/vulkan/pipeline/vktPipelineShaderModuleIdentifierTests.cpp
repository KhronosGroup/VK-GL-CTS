/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 The Khronos Group Inc.
 * Copyright (c) 2022 Valve Corporation.
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
 * \brief VK_EXT_shader_module_identifier tests
 *//*--------------------------------------------------------------------*/
#include "vktPipelineShaderModuleIdentifierTests.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktCustomInstancesDevices.hpp"

#include "vkQueryUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkRayTracingUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkPipelineConstructionUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "tcuMaybe.hpp"
#include "tcuCommandLine.hpp"
#include "tcuFormatUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTestLog.hpp"

#include "deUniquePtr.hpp"
#include "deRandom.hpp"

#include <vector>
#include <utility>
#include <string>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <memory>
#include <set>
#include <limits>

namespace vkt
{
namespace pipeline
{

namespace
{

using GroupPtr	= de::MovePtr<tcu::TestCaseGroup>;
using StringVec	= std::vector<std::string>;
using namespace vk;

using ShaderModuleId	= std::vector<uint8_t>;
using ShaderModuleIdPtr	= std::unique_ptr<ShaderModuleId>;
using ShaderStageIdPtr	= std::unique_ptr<VkPipelineShaderStageModuleIdentifierCreateInfoEXT>;

// Helper function to create a shader module identifier from a VkShaderModuleIdentifierEXT structure.
ShaderModuleId makeShaderModuleId (const VkShaderModuleIdentifierEXT& idExt)
{
	if (idExt.identifierSize == 0u || idExt.identifierSize > VK_MAX_SHADER_MODULE_IDENTIFIER_SIZE_EXT)
		TCU_FAIL("Invalid identifierSize returned");

	ShaderModuleId identifier(idExt.identifier, idExt.identifier + idExt.identifierSize);
	return identifier;
}

// Helper function to obtain the shader module identifier for a VkShaderModule as a return value.
ShaderModuleId getShaderModuleIdentifier (const DeviceInterface& vkd, const VkDevice device, const VkShaderModule module)
{
	VkShaderModuleIdentifierEXT idExt = initVulkanStructure();
	vkd.getShaderModuleIdentifierEXT(device, module, &idExt);
	return makeShaderModuleId(idExt);
}

// Helper function to obtain the shader module identifier from a VkShaderModuleCreateInfo structure as a return value.
ShaderModuleId getShaderModuleIdentifier (const DeviceInterface& vkd, const VkDevice device, const VkShaderModuleCreateInfo& createInfo)
{
	VkShaderModuleIdentifierEXT idExt = initVulkanStructure();
	vkd.getShaderModuleCreateInfoIdentifierEXT(device, &createInfo, &idExt);
	return makeShaderModuleId(idExt);
}

// Helper function to create a VkShaderModuleCreateInfo structure.
VkShaderModuleCreateInfo makeShaderModuleCreateInfo (size_t codeSize, const uint32_t* pCode, const VkShaderModuleCreateFlags createFlags = 0u, const void* pNext = nullptr)
{
	const VkShaderModuleCreateInfo moduleCreateInfo =
	{
		VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,	//	VkStructureType				sType;
		pNext,											//	const void*					pNext;
		createFlags,									//	VkShaderModuleCreateFlags	flags;
		codeSize,										//	size_t						codeSize;
		pCode,											//	const uint32_t*				pCode;
	};
	return moduleCreateInfo;
}

// On the actual pipeline in use, will we use module IDs or other data?
enum class UseModuleCase
{
	ID = 0,
	ZERO_LEN_ID,
	ZERO_LEN_ID_NULL_PTR,
	ZERO_LEN_ID_GARBAGE_PTR,
	ALL_ZEROS,
	ALL_ONES,
	PSEUDORANDOM_ID,
};

bool isZeroLen (UseModuleCase usage)
{
	bool zeroLen = false;

	switch (usage)
	{
	case UseModuleCase::ZERO_LEN_ID:
	case UseModuleCase::ZERO_LEN_ID_NULL_PTR:
	case UseModuleCase::ZERO_LEN_ID_GARBAGE_PTR:
		zeroLen = true;
		break;
	default:
		break;
	}

	return zeroLen;
}

bool expectCacheMiss (UseModuleCase usage)
{
	bool miss = false;

	switch (usage)
	{
	case UseModuleCase::ALL_ZEROS:
	case UseModuleCase::ALL_ONES:
	case UseModuleCase::PSEUDORANDOM_ID:
		miss = true;
		break;
	default:
		break;
	}

	return miss;
}

// Modify a shader module id according to the type of use.
void maybeMangleShaderModuleId (ShaderModuleId& moduleId, UseModuleCase moduleUse, de::Random& rnd)
{
	if (moduleUse == UseModuleCase::ALL_ZEROS || moduleUse == UseModuleCase::ALL_ONES)
	{
		deMemset(moduleId.data(), ((moduleUse == UseModuleCase::ALL_ZEROS) ? 0 : 0xFF), de::dataSize(moduleId));
	}
	else if (moduleUse == UseModuleCase::PSEUDORANDOM_ID)
	{
		for (auto& byte : moduleId)
			byte = rnd.getUint8();
	}
}

// Helper function to create a VkPipelineShaderStageModuleIdentifierCreateInfoEXT structure.
ShaderStageIdPtr makeShaderStageModuleIdentifierCreateInfo (const ShaderModuleId& moduleId, UseModuleCase moduleUse, de::Random* rnd = nullptr)
{
	ShaderStageIdPtr createInfo(new VkPipelineShaderStageModuleIdentifierCreateInfoEXT(initVulkanStructure()));

	createInfo->identifierSize = (isZeroLen(moduleUse) ? 0u : de::sizeU32(moduleId));

	switch (moduleUse)
	{
	case UseModuleCase::ID:
	case UseModuleCase::ZERO_LEN_ID:
	case UseModuleCase::ALL_ZEROS: // For this one and below, the module id will have been modified outside.
	case UseModuleCase::ALL_ONES:
	case UseModuleCase::PSEUDORANDOM_ID:
		createInfo->pIdentifier = de::dataOrNull(moduleId);
		break;
	case UseModuleCase::ZERO_LEN_ID_NULL_PTR:
		break; // Already null as part of initVulkanStructure().
	case UseModuleCase::ZERO_LEN_ID_GARBAGE_PTR:
		{
			DE_ASSERT(rnd);

			// Fill pointer with random bytes.
			auto pIdentifierPtr = reinterpret_cast<uint8_t*>(&(createInfo->pIdentifier));

			for (size_t i = 0; i < sizeof(createInfo->pIdentifier); ++i)
				pIdentifierPtr[i] = rnd->getUint8();
		}
		break;
	default:
		DE_ASSERT(false);
		break;
	}

	return createInfo;
}

ShaderWrapper* retUsedModule (ShaderWrapper* module, UseModuleCase moduleUse)
{
	static ShaderWrapper emptyWrapper = ShaderWrapper();
	return (isZeroLen(moduleUse) ? module : &emptyWrapper);
}

enum class PipelineType
{
	COMPUTE = 0,
	GRAPHICS,
	RAY_TRACING,
};

enum class GraphicsShaderType
{
	VERTEX = 0,
	TESS_CONTROL,
	TESS_EVAL,
	GEOMETRY,
	FRAG,
};

enum class RayTracingShaderType
{
	RAY_GEN = 0,
	CLOSEST_HIT,
	ANY_HIT,
	INTERSECTION,
	MISS,
	CALLABLE,
};

using GraphicsShaderVec	= std::vector<GraphicsShaderType>;
using RTShaderVec		= std::vector<RayTracingShaderType>;

std::ostream& operator<<(std::ostream& out, GraphicsShaderType type)
{
	switch (type)
	{
	case GraphicsShaderType::VERTEX:			out << "vert";		break;
	case GraphicsShaderType::TESS_CONTROL:		out << "tesc";		break;
	case GraphicsShaderType::TESS_EVAL:			out << "tese";		break;
	case GraphicsShaderType::GEOMETRY:			out << "geom";		break;
	case GraphicsShaderType::FRAG:				out << "frag";		break;
	default:
		DE_ASSERT(false);
		break;
	}
	return out;
}

std::ostream& operator<<(std::ostream& out, RayTracingShaderType type)
{
	switch (type)
	{
	case RayTracingShaderType::RAY_GEN:			out << "rgen";		break;
	case RayTracingShaderType::CLOSEST_HIT:		out << "chit";		break;
	case RayTracingShaderType::ANY_HIT:			out << "ahit";		break;
	case RayTracingShaderType::INTERSECTION:	out << "isec";		break;
	case RayTracingShaderType::MISS:			out << "miss";		break;
	case RayTracingShaderType::CALLABLE:		out << "call";		break;
	default:
		DE_ASSERT(false);
		break;
	}
	return out;
}

template <class T>
std::string toString(const std::vector<T>& vec)
{
	std::ostringstream out;
	for (size_t i = 0; i < vec.size(); ++i)
		out << ((i == 0) ? "" : "_") << vec.at(i);
	return out.str();
}

// Pipeline executable properties helpers.
struct PipelineExecutableStat
{
	PipelineExecutableStat (std::string name_, std::string description_, VkPipelineExecutableStatisticFormatKHR format_, VkPipelineExecutableStatisticValueKHR value_)
		: name			(std::move(name_))
		, description	(std::move(description_))
		, format		(format_)
		, value			(value_)
		{}

	const std::string								name;
	const std::string								description;
	const VkPipelineExecutableStatisticFormatKHR	format;
	const VkPipelineExecutableStatisticValueKHR		value;
};

struct PipelineExecutableInternalRepresentation
{
	PipelineExecutableInternalRepresentation (std::string name_, std::string description_, bool isText_, const std::vector<uint8_t>& data)
		: name			(std::move(name_))
		, description	(std::move(description_))
		, m_isText		(isText_)
		, m_text		()
		, m_bytes		()
	{
		if (m_isText)
			m_text = std::string(reinterpret_cast<const char*>(data.data()));
		else
			m_bytes = data;
	}

	const std::string name;
	const std::string description;

	bool						isText		(void) const { return m_isText; }
	const std::string&			getText		(void) const { DE_ASSERT(isText()); return m_text; }
	const std::vector<uint8_t>&	getBytes	(void) const { DE_ASSERT(!isText()); return m_bytes; }

protected:
	const bool				m_isText;
	std::string				m_text;
	std::vector<uint8_t>	m_bytes;
};

struct PipelineExecutableProperty
{
	PipelineExecutableProperty (VkShaderStageFlags stageFlags_, std::string name_, std::string description_, uint32_t subgroupSize_)
		: stageFlags	(stageFlags_)
		, name			(std::move(name_))
		, description	(std::move(description_))
		, subgroupSize	(subgroupSize_)
		, m_stats		()
		, m_irs			()
		{}

	const VkShaderStageFlags	stageFlags;
	const std::string			name;
	const std::string			description;
	const uint32_t				subgroupSize;

	void addStat	(const PipelineExecutableStat& stat)					{ m_stats.emplace_back(stat); }
	void addIR		(const PipelineExecutableInternalRepresentation& ir)	{ m_irs.emplace_back(ir); }

	const std::vector<PipelineExecutableStat>&						getStats	(void) const { return m_stats; }
	const std::vector<PipelineExecutableInternalRepresentation>&	getIRs		(void) const { return m_irs; }

protected:
	std::vector<PipelineExecutableStat>						m_stats;
	std::vector<PipelineExecutableInternalRepresentation>	m_irs;
};

// This will NOT compare stats and IRs, only flags, name, description and subgroup sizes.
bool operator== (const PipelineExecutableProperty& a, const PipelineExecutableProperty& b)
{
	return (a.stageFlags == b.stageFlags && a.name == b.name && a.description == b.description && a.subgroupSize == b.subgroupSize);
}

// For sorting if used as a map key or in a set. Based on the property name.
bool operator< (const PipelineExecutableProperty& a, const PipelineExecutableProperty& b)
{
	return (a.name < b.name);
}

std::ostream& operator<< (std::ostream& out, const PipelineExecutableProperty& prop)
{
	out << "PipelineExecutableProperty("
		<< "stageFlags=\"" << prop.stageFlags << "\", "
		<< "name=\"" << prop.name << "\", "
		<< "description=\"" << prop.description << "\", "
		<< "subgroupSize=\"" << prop.subgroupSize << "\")";
	return out;
}

// What to capture from a pipeline.
enum class CapturedPropertiesBits
{
	NONE	= 0,
	STATS	= 1,
	IRS		= 2,
};
using CapturedPropertiesFlags = uint32_t;

VkPipelineCreateFlags getPipelineCreateFlags (CapturedPropertiesFlags capturedProperties)
{
	VkPipelineCreateFlags createFlags = 0u;

	if (capturedProperties & static_cast<int>(CapturedPropertiesBits::STATS))
		createFlags |= VK_PIPELINE_CREATE_CAPTURE_STATISTICS_BIT_KHR;

	if (capturedProperties & static_cast<int>(CapturedPropertiesBits::IRS))
		createFlags |= VK_PIPELINE_CREATE_CAPTURE_INTERNAL_REPRESENTATIONS_BIT_KHR;

	return createFlags;
}

VkPipelineInfoKHR makePipelineInfo (VkPipeline pipeline)
{
	VkPipelineInfoKHR pipelineInfo = initVulkanStructure();
	pipelineInfo.pipeline = pipeline;
	return pipelineInfo;
}

VkPipelineExecutableInfoKHR makePipelineExecutableInfo (VkPipeline pipeline, size_t executableIndex)
{
	VkPipelineExecutableInfoKHR info = initVulkanStructure();
	info.pipeline			= pipeline;
	info.executableIndex	= static_cast<uint32_t>(executableIndex);
	return info;
}

using PipelineExecutablePropertyVec = std::vector<PipelineExecutableProperty>;

std::ostream& operator<< (std::ostream& out, const PipelineExecutablePropertyVec& vec)
{
	bool first = true;
	out << "[";
	for (const auto& prop : vec)
	{
		out << (first ? "" : ", ") << prop;
		first = false;
	}
	out << "]";
	return out;
}

PipelineExecutablePropertyVec getPipelineExecutableProperties (const DeviceInterface& vkd, VkDevice device, VkPipeline pipeline, CapturedPropertiesFlags captureFlags)
{
	PipelineExecutablePropertyVec	properties;
	const auto						pipelineInfo	= makePipelineInfo(pipeline);
	uint32_t						executableCount	= 0u;

	std::vector<VkPipelineExecutablePropertiesKHR> propertiesKHR;
	VK_CHECK(vkd.getPipelineExecutablePropertiesKHR(device, &pipelineInfo, &executableCount, nullptr));

	// No properties?
	if (executableCount == 0u)
		return properties;

	propertiesKHR.resize(executableCount, initVulkanStructure());
	VK_CHECK(vkd.getPipelineExecutablePropertiesKHR(device, &pipelineInfo, &executableCount, propertiesKHR.data()));

	// Make a property with every result structure.
	properties.reserve(propertiesKHR.size());
	for (const auto& prop : propertiesKHR)
		properties.emplace_back(prop.stages, prop.name, prop.description, prop.subgroupSize);

	// Query stats if requested.
	if (captureFlags & static_cast<int>(CapturedPropertiesBits::STATS))
	{
		for (size_t exeIdx = 0; exeIdx < properties.size(); ++exeIdx)
		{
			uint32_t										statCount		= 0u;
			std::vector<VkPipelineExecutableStatisticKHR>	statsKHR;
			const auto										executableInfo	= makePipelineExecutableInfo(pipeline, exeIdx);

			VK_CHECK(vkd.getPipelineExecutableStatisticsKHR(device, &executableInfo, &statCount, nullptr));

			if (statCount == 0u)
				continue;

			statsKHR.resize(statCount, initVulkanStructure());
			VK_CHECK(vkd.getPipelineExecutableStatisticsKHR(device, &executableInfo, &statCount, statsKHR.data()));

			for (const auto& stat : statsKHR)
				properties[exeIdx].addStat(PipelineExecutableStat(stat.name, stat.description, stat.format, stat.value));
		}
	}

	// Query IRs if requested.
	if (captureFlags & static_cast<int>(CapturedPropertiesBits::IRS))
	{
		for (size_t exeIdx = 0; exeIdx < properties.size(); ++exeIdx)
		{
			uint32_t													irsCount		= 0u;
			std::vector<VkPipelineExecutableInternalRepresentationKHR>	irsKHR;
			std::vector<std::vector<uint8_t>>							irsData;
			const auto													executableInfo	= makePipelineExecutableInfo(pipeline, exeIdx);

			// Get count.
			VK_CHECK(vkd.getPipelineExecutableInternalRepresentationsKHR(device, &executableInfo, &irsCount, nullptr));

			if (irsCount == 0u)
				continue;

			// Get data sizes.
			irsData.resize(irsCount);
			irsKHR.resize(irsCount, initVulkanStructure());
			VK_CHECK(vkd.getPipelineExecutableInternalRepresentationsKHR(device, &executableInfo, &irsCount, irsKHR.data()));

			// Get data.
			for (size_t irIdx = 0; irIdx < irsKHR.size(); ++irIdx)
			{
				auto& dataBuffer	= irsData[irIdx];
				auto& irKHR			= irsKHR[irIdx];

				dataBuffer.resize(irKHR.dataSize);
				irKHR.pData = dataBuffer.data();
			}
			VK_CHECK(vkd.getPipelineExecutableInternalRepresentationsKHR(device, &executableInfo, &irsCount, irsKHR.data()));

			// Append IRs to property.
			for (size_t irIdx = 0; irIdx < irsKHR.size(); ++irIdx)
			{
				const auto& ir = irsKHR[irIdx];
				properties[exeIdx].addIR(PipelineExecutableInternalRepresentation(ir.name, ir.description, ir.isText, irsData[irIdx]));
			}
		}
	}

	return properties;
}

struct BaseParams
{
	const PipelineType			pipelineType;
	GraphicsShaderVec			graphicsShaders;
	RTShaderVec					rtShaders;
	const uint8_t				pipelineCount;
	const tcu::Maybe<uint8_t>	pipelineToRun;
	const bool					useSpecializationConstants;
	const bool					useCache;
	const bool					useMaintenance5;

	BaseParams (PipelineType				pipelineType_,
				GraphicsShaderVec			graphicsShaders_,
				RTShaderVec					rtShaders_,
				uint8_t						pipelineCount_,
				const tcu::Maybe<uint8_t>&	pipelineToRun_,
				bool						useSCs_,
				bool						useCache_,
				bool						useMaintenance5_)
		: pipelineType					(pipelineType_)
		, graphicsShaders				(std::move(graphicsShaders_))
		, rtShaders						(std::move(rtShaders_))
		, pipelineCount					(pipelineCount_)
		, pipelineToRun					(pipelineToRun_)
		, useSpecializationConstants	(useSCs_)
		, useCache						(useCache_)
		, useMaintenance5				(useMaintenance5_)
	{
		if (pipelineType != PipelineType::GRAPHICS)
			DE_ASSERT(graphicsShaders.empty());
		else if (pipelineType != PipelineType::RAY_TRACING)
			DE_ASSERT(rtShaders.empty());

		if (static_cast<bool>(pipelineToRun))
			DE_ASSERT(pipelineToRun.get() < pipelineCount);

		// We'll use one descriptor set per pipeline, so we only want a few pipelines.
		DE_ASSERT(static_cast<uint32_t>(pipelineCount) <= 4u);
	}

	// Make the class polymorphic, needed below.
	virtual ~BaseParams () {}

	size_t stageCountPerPipeline (void) const
	{
		size_t stageCount = 0;

		switch (pipelineType)
		{
		case PipelineType::COMPUTE:			stageCount = 1u;						break;
		case PipelineType::GRAPHICS:		stageCount = graphicsShaders.size();	break;
		case PipelineType::RAY_TRACING:		stageCount = rtShaders.size();			break;
		default:
			DE_ASSERT(false);
			break;
		}

		return stageCount;
	}

protected:
	bool hasGraphicsStage (GraphicsShaderType stage) const
	{
		if (pipelineType != PipelineType::GRAPHICS)
			return false;
		return de::contains(begin(graphicsShaders), end(graphicsShaders), stage);
	}

	bool hasRTStage (RayTracingShaderType stage) const
	{
		if (pipelineType != PipelineType::RAY_TRACING)
			return false;
		return de::contains(begin(rtShaders), end(rtShaders), stage);
	}

public:
	bool hasGeom (void) const
	{
		return hasGraphicsStage(GraphicsShaderType::GEOMETRY);
	}

	bool hasTess (void) const
	{
		return (hasGraphicsStage(GraphicsShaderType::TESS_CONTROL) || hasGraphicsStage(GraphicsShaderType::TESS_EVAL));
	}

	bool hasVertexPipelineStage (void) const
	{
		return (hasGraphicsStage(GraphicsShaderType::VERTEX) || hasTess() || hasGeom());
	}

	bool hasFrag (void) const
	{
		return hasGraphicsStage(GraphicsShaderType::FRAG);
	}

	bool hasRayTracing (void) const
	{
		return (pipelineType == PipelineType::RAY_TRACING);
	}

	bool hasHit (void) const
	{
		return (hasRTStage(RayTracingShaderType::ANY_HIT) || hasRTStage(RayTracingShaderType::CLOSEST_HIT) || hasRTStage(RayTracingShaderType::INTERSECTION));
	}

	bool hasISec (void) const
	{
		return hasRTStage(RayTracingShaderType::INTERSECTION);
	}

	bool hasMiss (void) const
	{
		return hasRTStage(RayTracingShaderType::MISS);
	}

	VkPipelineStageFlags getPipelineStageFlags (void) const
	{
		if (pipelineType == PipelineType::COMPUTE)
			return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

		if (pipelineType == PipelineType::RAY_TRACING)
			return VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;

		if (pipelineType == PipelineType::GRAPHICS)
		{
			VkPipelineStageFlags stageFlags = 0u;

			for (const auto& stage : graphicsShaders)
			{
				switch (stage)
				{
				case GraphicsShaderType::VERTEX:		stageFlags |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;					break;
				case GraphicsShaderType::TESS_CONTROL:	stageFlags |= VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT;	break;
				case GraphicsShaderType::TESS_EVAL:		stageFlags |= VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;	break;
				case GraphicsShaderType::GEOMETRY:		stageFlags |= VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;				break;
				case GraphicsShaderType::FRAG:			stageFlags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;				break;
				default:
					DE_ASSERT(false);
					break;
				}
			}

			return stageFlags;
		}

		DE_ASSERT(false);
		return 0u;
	}

	VkShaderStageFlags getShaderStageFlags (void) const
	{
		if (pipelineType == PipelineType::COMPUTE)
			return VK_SHADER_STAGE_COMPUTE_BIT;

		if (pipelineType == PipelineType::RAY_TRACING)
		{
			VkShaderStageFlags stageFlags = 0u;

			for (const auto& stage : rtShaders)
			{
				switch (stage)
				{
				case RayTracingShaderType::RAY_GEN:			stageFlags |= VK_SHADER_STAGE_RAYGEN_BIT_KHR;		break;
				case RayTracingShaderType::CLOSEST_HIT:		stageFlags |= VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;	break;
				case RayTracingShaderType::ANY_HIT:			stageFlags |= VK_SHADER_STAGE_ANY_HIT_BIT_KHR;		break;
				case RayTracingShaderType::INTERSECTION:	stageFlags |= VK_SHADER_STAGE_INTERSECTION_BIT_KHR;	break;
				case RayTracingShaderType::MISS:			stageFlags |= VK_SHADER_STAGE_MISS_BIT_KHR;			break;
				case RayTracingShaderType::CALLABLE:		stageFlags |= VK_SHADER_STAGE_CALLABLE_BIT_KHR;		break;
				default:
					DE_ASSERT(false);
					break;
				}
			}

			return stageFlags;
		}

		if (pipelineType == PipelineType::GRAPHICS)
		{
			VkShaderStageFlags stageFlags = 0u;

			for (const auto& stage : graphicsShaders)
			{
				switch (stage)
				{
				case GraphicsShaderType::VERTEX:		stageFlags |= VK_SHADER_STAGE_VERTEX_BIT;					break;
				case GraphicsShaderType::TESS_CONTROL:	stageFlags |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;		break;
				case GraphicsShaderType::TESS_EVAL:		stageFlags |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;	break;
				case GraphicsShaderType::GEOMETRY:		stageFlags |= VK_SHADER_STAGE_GEOMETRY_BIT;					break;
				case GraphicsShaderType::FRAG:			stageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;					break;
				default:
					DE_ASSERT(false);
					break;
				}
			}

			return stageFlags;
		}

		DE_ASSERT(false);
		return 0u;
	}
};

using BaseParamsPtr = std::unique_ptr<BaseParams>;

void checkShaderModuleIdentifierSupport (Context& context)
{
	context.requireDeviceFunctionality("VK_EXT_shader_module_identifier");
}

void getTwoShaderIdentifierProperties	(Context& context,
										 VkPhysicalDeviceShaderModuleIdentifierPropertiesEXT* properties1,
										 VkPhysicalDeviceShaderModuleIdentifierPropertiesEXT* properties2)
{
	*properties1 = initVulkanStructure();
	*properties2 = initVulkanStructure();

	const auto&					vki				= context.getInstanceInterface();
	const auto					physicalDevice	= context.getPhysicalDevice();
	VkPhysicalDeviceProperties2	main			= initVulkanStructure(properties1);

	vki.getPhysicalDeviceProperties2(physicalDevice, &main);
	main.pNext = properties2;
	vki.getPhysicalDeviceProperties2(physicalDevice, &main);
}

tcu::TestStatus constantAlgorithmUUIDCase (Context& context)
{
	VkPhysicalDeviceShaderModuleIdentifierPropertiesEXT properties1, properties2;
	getTwoShaderIdentifierProperties(context, &properties1, &properties2);

	const auto uuidSize = static_cast<size_t>(VK_UUID_SIZE);

	if (deMemCmp(properties1.shaderModuleIdentifierAlgorithmUUID, properties2.shaderModuleIdentifierAlgorithmUUID, uuidSize) != 0)
		return tcu::TestStatus::fail("shaderModuleIdentifierAlgorithmUUID not constant accross calls");

	uint8_t nullUUID[uuidSize];
	deMemset(nullUUID, 0, uuidSize);

	if (deMemCmp(properties1.shaderModuleIdentifierAlgorithmUUID, nullUUID, uuidSize) == 0)
		return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "shaderModuleIdentifierAlgorithmUUID is all zeros");

	return tcu::TestStatus::pass("Pass");
}

std::vector<uint32_t> generateShaderConstants (PipelineType pipelineType, uint8_t pipelineCount, size_t stageCount)
{
	std::vector<uint32_t> shaderConstants;

	for (uint8_t pipelineIdx = 0; pipelineIdx < pipelineCount; ++pipelineIdx)
		for (size_t stageIdx = 0; stageIdx < stageCount; ++stageIdx)
			shaderConstants.push_back(0xEB000000u
				| ((static_cast<uint32_t>(pipelineType) & 0xFFu)  << 16)
				| ((static_cast<uint32_t>(pipelineIdx)  & 0xFFu)  <<  8)
				| ((static_cast<uint32_t>(stageIdx)     & 0xFFu)       )
			);

	return shaderConstants;
}

size_t getShaderIdx (uint8_t pipelineIdx, size_t stageIdx, size_t stageCount)
{
	const auto pIdx = static_cast<size_t>(pipelineIdx);
	return (pIdx * stageCount + stageIdx);
}

void generateSources (SourceCollections& programCollection, const BaseParams* params_)
{
	const auto&	params			= *params_;
	const auto	stageCount		= params.stageCountPerPipeline();
	const auto	constantValues	= generateShaderConstants(params.pipelineType, params.pipelineCount, stageCount);

	StringVec constantDecls;	// Per pipeline and stage.
	StringVec pipelineAdds;		// Per pipeline.
	StringVec stageStores;		// Per stage.

	std::string ssboDecl;		// Universal.
	std::string uboDecls;		// Universal.
	std::string outValueDecl	= "    uint outValue = stageConstant;\n";	// Universal.

	// Each stage in each pipeline will have one specific constant value.
	{
		for (uint8_t pipelineIdx = 0; pipelineIdx < params.pipelineCount; ++pipelineIdx)
			for (size_t stageIdx = 0; stageIdx < stageCount; ++stageIdx)
			{
				constantDecls.push_back(params.useSpecializationConstants
					? "layout (constant_id=0) const uint stageConstant = 0u;\n"
					: "const uint stageConstant = " + std::to_string(constantValues.at(getShaderIdx(pipelineIdx, stageIdx, stageCount))) + "u;\n");
			}
	}

	// Each pipeline will have slightly different code by adding more values to the constant in each shader.
	// The values will come from UBOs and, in practice, will contain zeros.
	{
		pipelineAdds.reserve(params.pipelineCount);

		for (uint8_t pipelineIdx = 0; pipelineIdx < params.pipelineCount; ++pipelineIdx)
		{
			std::string	additions;
			const auto	addCount	= static_cast<size_t>(pipelineIdx + 1);

			for (size_t addIdx = 0; addIdx < addCount; ++addIdx)
			{
				const auto uboId = addIdx + 1;
				additions += "    outValue += ubo_" + std::to_string(uboId) + ".value;\n";
			}

			pipelineAdds.push_back(additions);
		}
	}

	// Each stage will write the output value to an SSBO position.
	{
		stageStores.reserve(stageCount);

		for (size_t stageIdx = 0; stageIdx < stageCount; ++stageIdx)
		{
			const auto stageStore = "    ssbo.values[" + std::to_string(stageIdx) + "] = outValue;\n";
			stageStores.push_back(stageStore);
		}
	}

	// The SSBO declaration is constant.
	ssboDecl = "layout (set=0, binding=0, std430) buffer SSBOBlock { uint values[]; } ssbo;\n";

	// The UBO declarations are constant. We need one UBO per pipeline, but all pipelines declare them all.
	{
		for (uint8_t pipelineIdx = 0; pipelineIdx < params.pipelineCount; ++pipelineIdx)
		{
			const auto uboId = pipelineIdx + 1;
			const auto idStr = std::to_string(uboId);
			uboDecls += "layout (set=0, binding=" + idStr + ") uniform UBOBlock" + idStr + " { uint value; } ubo_" + idStr + ";\n";
		}
	}

	if (params.pipelineType == PipelineType::COMPUTE)
	{
		const std::string localSize	= (params.useSpecializationConstants
									? "layout (local_size_x_id=1, local_size_y_id=2, local_size_z_id=3) in;\n"
									: "layout (local_size_x=1, local_size_y=1, local_size_z=1) in;\n");

		for (uint8_t pipelineIdx = 0; pipelineIdx < params.pipelineCount; ++pipelineIdx)
		{
			const auto			plIdxSz		= static_cast<size_t>(pipelineIdx);
			const std::string	shaderName	= "comp_" + std::to_string(plIdxSz);
			const auto			shaderIdx	= getShaderIdx(pipelineIdx, 0, stageCount);

			std::ostringstream comp;
			comp
				<< "#version 450\n"
				<< localSize
				<< ssboDecl
				<< uboDecls
				<< constantDecls.at(shaderIdx)
				<< "void main (void) {\n"
				<< outValueDecl
				<< pipelineAdds.at(plIdxSz)
				<< "    if (gl_LocalInvocationIndex == 0u) {\n"
				<< stageStores.at(0)
				<< "    }\n"
				<< "}\n"
				;
			programCollection.glslSources.add(shaderName) << glu::ComputeSource(comp.str());
		}
	}
	else if (params.pipelineType == PipelineType::GRAPHICS)
	{
		bool hasVertex			= false;
		bool hasTessControl		= false;
		bool hasTessEval		= false;
		bool hasGeom			= false;
		bool hasFrag			= false;

		// Assign a unique index to each active shader type.
		size_t vertShaderIdx	= 0u;
		size_t tescShaderIdx	= 0u;
		size_t teseShaderIdx	= 0u;
		size_t geomShaderIdx	= 0u;
		size_t fragShaderIdx	= 0u;
		size_t curShaderIdx		= 0u;

		const std::set<GraphicsShaderType> uniqueStages (begin(params.graphicsShaders), end(params.graphicsShaders));

		for (const auto& stage : uniqueStages)
		{
			switch (stage)
			{
			case GraphicsShaderType::VERTEX:		hasVertex		= true;	vertShaderIdx = curShaderIdx++;	break;
			case GraphicsShaderType::TESS_CONTROL:	hasTessControl	= true;	tescShaderIdx = curShaderIdx++;	break;
			case GraphicsShaderType::TESS_EVAL:		hasTessEval		= true;	teseShaderIdx = curShaderIdx++;	break;
			case GraphicsShaderType::GEOMETRY:		hasGeom			= true;	geomShaderIdx = curShaderIdx++;	break;
			case GraphicsShaderType::FRAG:			hasFrag			= true;	fragShaderIdx = curShaderIdx++;	break;
			default:								DE_ASSERT(false);										break;
			}
		}

		const bool hasTess = (hasTessControl || hasTessEval);

		for (uint8_t pipelineIdx = 0; pipelineIdx < params.pipelineCount; ++pipelineIdx)
		{
			const auto plIdxSz = static_cast<size_t>(pipelineIdx);

			if (hasVertex)
			{
				const std::string	shaderName	= "vert_" + std::to_string(plIdxSz);
				const auto			shaderIdx	= getShaderIdx(pipelineIdx, vertShaderIdx, stageCount);

				std::ostringstream vert;
				vert
					<< "#version 450\n"
					<< "out gl_PerVertex\n"
					<< "{\n"
					<< "    vec4 gl_Position;\n"
					<< (hasTess ? "" : "    float gl_PointSize;\n")
					<< "};\n"
					;

				if (hasTess)
				{
					vert
						<< "vec2 vertexPositions[3] = vec2[](\n"
						<< "    vec2( 0.0, -0.5),\n"
						<< "    vec2( 0.5,  0.5),\n"
						<< "    vec2(-0.5,  0.5)\n"
						<< ");\n"
						;
				}

				vert
					<< ssboDecl
					<< uboDecls
					<< constantDecls.at(shaderIdx)
					<< "void main (void) {\n"
					<< outValueDecl
					<< pipelineAdds.at(plIdxSz)
					<< stageStores.at(vertShaderIdx)
					;

				if (hasTess)
				{
					vert << "    gl_Position = vec4(vertexPositions[gl_VertexIndex], 0.0, 1.0);\n";
				}
				else
				{
					vert
						<< "    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);\n"
						<< "    gl_PointSize = 1.0;\n"
						;
				}

				vert << "}\n";

				programCollection.glslSources.add(shaderName) << glu::VertexSource(vert.str());
			}

			if (hasFrag)
			{
				const std::string	shaderName	= "frag_" + std::to_string(plIdxSz);
				const auto			shaderIdx	= getShaderIdx(pipelineIdx, fragShaderIdx, stageCount);

				std::ostringstream frag;
				frag
					<< "#version 450\n"
					<< "layout (location=0) out vec4 outColor;\n"
					<< ssboDecl
					<< uboDecls
					<< constantDecls.at(shaderIdx)
					<< "void main (void) {\n"
					<< outValueDecl
					<< pipelineAdds.at(plIdxSz)
					<< stageStores.at(fragShaderIdx)
					<< "    outColor = vec4(0.0, 0.0, 1.0, 1.0);\n"
					<< "}\n"
					;
				programCollection.glslSources.add(shaderName) << glu::FragmentSource(frag.str());
			}

			if (hasTessControl)
			{
				const std::string	shaderName	= "tesc_" + std::to_string(plIdxSz);
				const auto			shaderIdx	= getShaderIdx(pipelineIdx, tescShaderIdx, stageCount);

				std::ostringstream tesc;
				tesc
					<< "#version 450\n"
					<< "layout (vertices=3) out;\n"
					<< "in gl_PerVertex\n"
					<< "{\n"
					<< "    vec4 gl_Position;\n"
					<< "} gl_in[gl_MaxPatchVertices];\n"
					<< "out gl_PerVertex\n"
					<< "{\n"
					<< "    vec4 gl_Position;\n"
					<< "} gl_out[];\n"
					<< ssboDecl
					<< uboDecls
					<< constantDecls.at(shaderIdx)
					<< "void main (void) {\n"
					<< outValueDecl
					<< pipelineAdds.at(plIdxSz)
					<< stageStores.at(tescShaderIdx)
					<< "    gl_TessLevelInner[0] = 1.0;\n"
					<< "    gl_TessLevelInner[1] = 1.0;\n"
					<< "    gl_TessLevelOuter[0] = 1.0;\n"
					<< "    gl_TessLevelOuter[1] = 1.0;\n"
					<< "    gl_TessLevelOuter[2] = 1.0;\n"
					<< "    gl_TessLevelOuter[3] = 1.0;\n"
					<< "    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
					<< "}\n"
					;
				programCollection.glslSources.add(shaderName) << glu::TessellationControlSource(tesc.str());
			}

			if (hasTessEval)
			{
				const std::string	shaderName	= "tese_" + std::to_string(plIdxSz);
				const auto			shaderIdx	= getShaderIdx(pipelineIdx, teseShaderIdx, stageCount);

				std::ostringstream tese;
				tese
					<< "#version 450\n"
					<< "layout (triangles, fractional_odd_spacing, cw) in;\n"
					<< "in gl_PerVertex\n"
					<< "{\n"
					<< "    vec4 gl_Position;\n"
					<< "} gl_in[gl_MaxPatchVertices];\n"
					<< "out gl_PerVertex\n"
					<< "{\n"
					<< "    vec4 gl_Position;\n"
					<< "};\n"
					<< ssboDecl
					<< uboDecls
					<< constantDecls.at(shaderIdx)
					<< "void main (void) {\n"
					<< outValueDecl
					<< pipelineAdds.at(plIdxSz)
					<< stageStores.at(teseShaderIdx)
					<< "    gl_Position = (gl_TessCoord.x * gl_in[0].gl_Position) +\n"
					<< "                  (gl_TessCoord.y * gl_in[1].gl_Position) +\n"
					<< "                  (gl_TessCoord.z * gl_in[2].gl_Position);\n"
					<< "}\n"
					;
				programCollection.glslSources.add(shaderName) << glu::TessellationEvaluationSource(tese.str());
			}

			if (hasGeom)
			{
				const std::string	shaderName	= "geom_" + std::to_string(plIdxSz);
				const auto			shaderIdx	= getShaderIdx(pipelineIdx, geomShaderIdx, stageCount);
				const auto			inputPrim	= (hasTess ? "triangles" : "points");
				const auto			outputPrim	= (hasTess ? "triangle_strip" : "points");
				const auto			vertexCount	= (hasTess ? 3u : 1u);

				std::ostringstream geom;
				geom
					<< "#version 450\n"
					<< "layout (" << inputPrim << ") in;\n"
					<< "layout (" << outputPrim << ", max_vertices=" << vertexCount << ") out;\n"
					<< "in gl_PerVertex\n"
					<< "{\n"
					<< "    vec4 gl_Position;\n"
					<< (hasTess ? "" : "    float gl_PointSize;\n")
					<< "} gl_in[" << vertexCount << "];\n"
					<< "out gl_PerVertex\n"
					<< "{\n"
					<< "    vec4 gl_Position;\n"
					<< (hasTess ? "" : "    float gl_PointSize;\n")
					<< "};\n"
					<< ssboDecl
					<< uboDecls
					<< constantDecls.at(shaderIdx)
					<< "void main (void) {\n"
					<< outValueDecl
					<< pipelineAdds.at(plIdxSz)
					<< stageStores.at(geomShaderIdx)
					;

				for (uint32_t i = 0; i < vertexCount; ++i)
				{
					geom
						<< "    gl_Position = gl_in[" << i << "].gl_Position;\n"
						<< (hasTess ? "" : "    gl_PointSize = gl_in[" + std::to_string(i) + "].gl_PointSize;\n")
						<< "    EmitVertex();\n"
						;
				}

				geom << "}\n";

				programCollection.glslSources.add(shaderName) << glu::GeometrySource(geom.str());
			}
		}
	}
	else if (params.pipelineType == PipelineType::RAY_TRACING)
	{
		bool hasRayGen			= false;
		bool hasAnyHit			= false;
		bool hasClosestHit		= false;
		bool hasIntersection	= false;
		bool hasMiss			= false;
		bool hasCallable		= false;

		// Assign a unique index to each active shader type.
		size_t rgenShaderIdx	= 0u;
		size_t ahitShaderIdx	= 0u;
		size_t chitShaderIdx	= 0u;
		size_t isecShaderIdx	= 0u;
		size_t missShaderIdx	= 0u;
		size_t callShaderIdx	= 0u;
		size_t curShaderIdx		= 0u;

		const std::set<RayTracingShaderType> uniqueStages (begin(params.rtShaders), end(params.rtShaders));

		for (const auto& stage : uniqueStages)
		{
			switch (stage)
			{
			case RayTracingShaderType::RAY_GEN:			hasRayGen		= true;	rgenShaderIdx = curShaderIdx++;	break;
			case RayTracingShaderType::ANY_HIT:			hasAnyHit		= true;	ahitShaderIdx = curShaderIdx++;	break;
			case RayTracingShaderType::CLOSEST_HIT:		hasClosestHit	= true;	chitShaderIdx = curShaderIdx++;	break;
			case RayTracingShaderType::INTERSECTION:	hasIntersection	= true;	isecShaderIdx = curShaderIdx++;	break;
			case RayTracingShaderType::MISS:			hasMiss			= true;	missShaderIdx = curShaderIdx++;	break;
			case RayTracingShaderType::CALLABLE:		hasCallable		= true; callShaderIdx = curShaderIdx++;	break;
			default:									DE_ASSERT(false);										break;
			}
		}

		const vk::ShaderBuildOptions	buildOptions	(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true/* allow SPIR-V 1.4 */);
		const bool						needsRayTraced	= (hasAnyHit || hasClosestHit || hasIntersection || hasMiss);

		for (uint8_t pipelineIdx = 0; pipelineIdx < params.pipelineCount; ++pipelineIdx)
		{
			const auto plIdxSz = static_cast<size_t>(pipelineIdx);

			if (hasRayGen)
			{
				const std::string	shaderName	= "rgen_" + std::to_string(plIdxSz);
				const auto			shaderIdx	= getShaderIdx(pipelineIdx, rgenShaderIdx, stageCount);

				std::ostringstream rgen;
				rgen
					<< "#version 460\n"
					<< "#extension GL_EXT_ray_tracing : require\n"
					<< (needsRayTraced ? "layout (location=0) rayPayloadEXT vec3 hitValue;\n" : "")
					<< (hasCallable ? "layout (location=0) callableDataEXT float unused;\n" : "")
					// Ray tracing pipelines will use a separate set for the acceleration structure.
					<< "layout (set=1, binding=0) uniform accelerationStructureEXT topLevelAS;\n"
					<< ssboDecl
					<< uboDecls
					<< constantDecls.at(shaderIdx)
					<< "void main (void) {\n"
					<< outValueDecl
					<< pipelineAdds.at(plIdxSz)
					<< "    if (gl_LaunchIDEXT.x == 0u) {\n"
					<< stageStores.at(rgenShaderIdx)
					<< "    }\n"
					<< "    uint  rayFlags = 0;\n"
					<< "    uint  cullMask = 0xFF;\n"
					<< "    float tmin     = 0.0;\n"
					<< "    float tmax     = 10.0;\n"
					// Rays will be traced towards +Z and geometry should be in the [0, 1] range in both X and Y, possibly at Z=5.
					// If a hit and a miss shader are used, a second ray will be traced starting at X=1.5, which should result in a miss.
					<< "    vec3  origin   = vec3(float(gl_LaunchIDEXT.x) + 0.5f, 0.5, 0.0);\n"
					<< "    vec3  direct   = vec3(0.0, 0.0, 1.0);\n"
					<< (needsRayTraced ? "    traceRayEXT(topLevelAS, rayFlags, cullMask, 0, 0, 0, origin, tmin, direct, tmax, 0);\n" : "")
					<< (hasCallable ? "    executeCallableEXT(0, 0);\n" : "")
					<< "}\n"
					;
				programCollection.glslSources.add(shaderName) << glu::RaygenSource(rgen.str()) << buildOptions;
			}

			if (hasAnyHit)
			{
				const std::string	shaderName	= "ahit_" + std::to_string(plIdxSz);
				const auto			shaderIdx	= getShaderIdx(pipelineIdx, ahitShaderIdx, stageCount);

				// VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR should be used.
				std::stringstream ahit;
				ahit
					<< "#version 460\n"
					<< "#extension GL_EXT_ray_tracing : require\n"
					<< "layout (location=0) rayPayloadInEXT vec3 hitValue;\n"
					<< "hitAttributeEXT vec3 attribs;\n"
					<< ssboDecl
					<< uboDecls
					<< constantDecls.at(shaderIdx)
					<< "void main()\n"
					<< "{\n"
					<< outValueDecl
					<< pipelineAdds.at(plIdxSz)
					<< stageStores.at(ahitShaderIdx)
					<< "}\n"
					;

				programCollection.glslSources.add(shaderName) << glu::AnyHitSource(ahit.str()) << buildOptions;
			}

			if (hasClosestHit)
			{
				const std::string	shaderName	= "chit_" + std::to_string(plIdxSz);
				const auto			shaderIdx	= getShaderIdx(pipelineIdx, chitShaderIdx, stageCount);

				std::stringstream chit;
				chit
					<< "#version 460\n"
					<< "#extension GL_EXT_ray_tracing : require\n"
					<< "layout (location=0) rayPayloadInEXT vec3 hitValue;\n"
					<< "hitAttributeEXT vec3 attribs;\n"
					<< ssboDecl
					<< uboDecls
					<< constantDecls.at(shaderIdx)
					<< "void main()\n"
					<< "{\n"
					<< outValueDecl
					<< pipelineAdds.at(plIdxSz)
					<< stageStores.at(chitShaderIdx)
					<< "}\n"
					;

				programCollection.glslSources.add(shaderName) << glu::ClosestHitSource(chit.str()) << buildOptions;
			}

			if (hasIntersection)
			{
				const std::string	shaderName	= "isec_" + std::to_string(plIdxSz);
				const auto			shaderIdx	= getShaderIdx(pipelineIdx, isecShaderIdx, stageCount);

				std::stringstream isec;
				isec
					<< "#version 460\n"
					<< "#extension GL_EXT_ray_tracing : require\n"
					<< "hitAttributeEXT vec3 hitAttribute;\n"
					<< ssboDecl
					<< uboDecls
					<< constantDecls.at(shaderIdx)
					<< "void main()\n"
					<< "{\n"
					<< outValueDecl
					<< pipelineAdds.at(plIdxSz)
					<< stageStores.at(isecShaderIdx)
					<< "  hitAttribute = vec3(0.0, 0.0, 0.0);\n"
					<< "  reportIntersectionEXT(5.0, 0);\n"
					<< "}\n"
					;

				programCollection.glslSources.add(shaderName) << glu::IntersectionSource(isec.str()) << buildOptions;
			}

			if (hasMiss)
			{
				const std::string	shaderName	= "miss_" + std::to_string(plIdxSz);
				const auto			shaderIdx	= getShaderIdx(pipelineIdx, missShaderIdx, stageCount);

				std::stringstream miss;
				miss
					<< "#version 460\n"
					<< "#extension GL_EXT_ray_tracing : require\n"
					<< "layout (location=0) rayPayloadInEXT vec3 hitValue;\n"
					<< ssboDecl
					<< uboDecls
					<< constantDecls.at(shaderIdx)
					<< "void main()\n"
					<< "{\n"
					<< outValueDecl
					<< pipelineAdds.at(plIdxSz)
					<< stageStores.at(missShaderIdx)
					<< "}\n"
					;

				programCollection.glslSources.add(shaderName) << glu::MissSource(miss.str()) << buildOptions;
			}

			if (hasCallable)
			{
				const std::string	shaderName	= "call_" + std::to_string(plIdxSz);
				const auto			shaderIdx	= getShaderIdx(pipelineIdx, callShaderIdx, stageCount);

				std::stringstream call;
				call
					<< "#version 460\n"
					<< "#extension GL_EXT_ray_tracing : require\n"
					<< "layout (location=0) callableDataInEXT float unused;\n"
					<< ssboDecl
					<< uboDecls
					<< constantDecls.at(shaderIdx)
					<< "void main()\n"
					<< "{\n"
					<< outValueDecl
					<< pipelineAdds.at(plIdxSz)
					<< stageStores.at(callShaderIdx)
					<< "}\n"
					;

				programCollection.glslSources.add(shaderName) << glu::CallableSource(call.str()) << buildOptions;
			}
		}
	}
	else
		DE_ASSERT(false);
}

// Virtual base class that uses the functions above to generate sources and check for support.
class SourcesAndSupportFromParamsBase : public vkt::TestCase
{
public:
					SourcesAndSupportFromParamsBase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, BaseParamsPtr&& params)
						: vkt::TestCase(testCtx, name, description)
						, m_params(std::move(params))
						{}
	virtual			~SourcesAndSupportFromParamsBase	(void) {}
	void			initPrograms						(vk::SourceCollections& programCollection) const override;
	void			checkSupport						(Context& context) const override;

protected:
	const BaseParamsPtr m_params;
};

void SourcesAndSupportFromParamsBase::initPrograms (vk::SourceCollections &programCollection) const
{
	generateSources(programCollection, m_params.get());
}

void SourcesAndSupportFromParamsBase::checkSupport (Context &context) const
{
	checkShaderModuleIdentifierSupport(context);

	if (m_params->hasVertexPipelineStage())
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_VERTEX_PIPELINE_STORES_AND_ATOMICS);

	if (m_params->hasFrag())
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_FRAGMENT_STORES_AND_ATOMICS);

	if (m_params->hasTess())
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_TESSELLATION_SHADER);

	if (m_params->hasGeom())
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);

	if (m_params->hasRayTracing())
	{
		context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
		context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");
	}
}

// Check shader module identifiers are constant across different API calls.
class ConstantModuleIdentifiersInstance : public vkt::TestInstance
{
public:
	enum class APICall { MODULE = 0, CREATE_INFO, BOTH };

	struct Params : public BaseParams
	{
		APICall	apiCall;
		bool	differentDevices;

		Params (PipelineType				pipelineType_,
				GraphicsShaderVec			graphicsShaders_,
				RTShaderVec					rtShaders_,
				uint8_t						pipelineCount_,
				const tcu::Maybe<uint8_t>&	pipelineToRun_,
				bool						useSCs_,
				bool						useCache_,
				APICall						apiCall_,
				bool						differentDevices_)
			: BaseParams		(pipelineType_, graphicsShaders_, rtShaders_, pipelineCount_, pipelineToRun_, useSCs_, useCache_, false)
			, apiCall			(apiCall_)
			, differentDevices	(differentDevices_)
			{}

		virtual ~Params () {}

		bool needsVkModule (void) const
		{
			return (apiCall != APICall::CREATE_INFO);
		}
	};

	using ParamsPtr = std::unique_ptr<Params>;

					ConstantModuleIdentifiersInstance	(Context& context, const Params* params)
						: vkt::TestInstance(context)
						, m_params(params)
						{}
	virtual			~ConstantModuleIdentifiersInstance	(void) {}
	tcu::TestStatus	runTest								(const DeviceInterface& vkd1, const VkDevice device1,
														 const DeviceInterface& vkd2, const VkDevice device2);
	tcu::TestStatus	iterate								(void) override;

protected:
	const Params* m_params;
};

tcu::TestStatus ConstantModuleIdentifiersInstance::runTest (const DeviceInterface& vkd1, const VkDevice device1,
															const DeviceInterface& vkd2, const VkDevice device2)
{
	const auto& binaries = m_context.getBinaryCollection();
	DE_ASSERT(!binaries.empty());

	std::set<ShaderModuleId>	uniqueIds;
	bool						pass		= true;
	size_t						binaryCount	= 0u;

	for (const auto& binary : binaries)
	{
		++binaryCount;
		binary.setUsed();

		const auto binSize			= binary.getSize();
		const auto binData			= reinterpret_cast<const uint32_t*>(binary.getBinary());
		const auto shaderModule1	= (m_params->needsVkModule() ? createShaderModule(vkd1, device1, binary) : Move<VkShaderModule>());
		const auto shaderModule2	= (m_params->needsVkModule() ? createShaderModule(vkd2, device2, binary) : Move<VkShaderModule>());

		// The first one will be a VkShaderModule if needed.
		const auto id1				= (m_params->needsVkModule()
									? getShaderModuleIdentifier(vkd1, device1, shaderModule1.get())
									: getShaderModuleIdentifier(vkd1, device1, makeShaderModuleCreateInfo(binSize, binData)));

		// The second one will be a VkShaderModule only when comparing shader modules.
		const auto id2				= ((m_params->apiCall == APICall::MODULE)
									? getShaderModuleIdentifier(vkd2, device2, shaderModule2.get())
									: getShaderModuleIdentifier(vkd2, device2, makeShaderModuleCreateInfo(binSize, binData)));

		if (id1 != id2)
			pass = false;

		uniqueIds.insert(id1);
	}

	if (!pass)
		return tcu::TestStatus::fail("The same shader module returned different identifiers");

	if (uniqueIds.size() != binaryCount)
		return tcu::TestStatus::fail("Different modules share the same identifier");

	return tcu::TestStatus::pass("Pass");
}

// Helper to create a new device supporting shader module identifiers.
struct DeviceHelper
{
	Move<VkDevice>						device;
	std::unique_ptr<DeviceDriver>		vkd;
	deUint32							queueFamilyIndex;
	VkQueue								queue;
	std::unique_ptr<SimpleAllocator>	allocator;

	// Forbid copy and assignment.
	DeviceHelper (const DeviceHelper&) = delete;
	DeviceHelper& operator= (const DeviceHelper& other) = delete;

	DeviceHelper (Context& context, bool enableRayTracing = false)
	{
		const auto&	vkp					= context.getPlatformInterface();
		const auto&	vki					= context.getInstanceInterface();
		const auto	instance			= context.getInstance();
		const auto	physicalDevice		= context.getPhysicalDevice();

		queueFamilyIndex = context.getUniversalQueueFamilyIndex();

		// Get device features (these have to be checked in the test case).
		VkPhysicalDeviceShaderModuleIdentifierFeaturesEXT		shaderIdFeatures		= initVulkanStructure();
		VkPhysicalDevicePipelineCreationCacheControlFeaturesEXT	cacheControlFeatures	= initVulkanStructure(&shaderIdFeatures);

		VkPhysicalDeviceDescriptorIndexingFeaturesEXT			descriptorIdxFeatures	= initVulkanStructure(&cacheControlFeatures);
		VkPhysicalDeviceBufferDeviceAddressFeaturesKHR			deviceAddressFeatures	= initVulkanStructure(&descriptorIdxFeatures);

		VkPhysicalDeviceFeatures2								deviceFeatures			= initVulkanStructure(enableRayTracing
																												? reinterpret_cast<void*>(&deviceAddressFeatures)
																												: reinterpret_cast<void*>(&cacheControlFeatures));

		vki.getPhysicalDeviceFeatures2(physicalDevice, &deviceFeatures);

		// Make sure robust buffer access is disabled as in the default device.
		deviceFeatures.features.robustBufferAccess = VK_FALSE;

		const auto queuePriority = 1.0f;
		const VkDeviceQueueCreateInfo queueInfo
		{
			VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,			//	VkStructureType					sType;
			nullptr,											//	const void*						pNext;
			0u,													//	VkDeviceQueueCreateFlags		flags;
			queueFamilyIndex,									//	deUint32						queueFamilyIndex;
			1u,													//	deUint32						queueCount;
			&queuePriority,										//	const float*					pQueuePriorities;
		};

		// Required extensions. Note: many of these require VK_KHR_get_physical_device_properties2, which is an instance extension.
		std::vector<const char*> requiredExtensions
		{
			"VK_EXT_pipeline_creation_cache_control",
			"VK_EXT_shader_module_identifier",
		};

		if (enableRayTracing)
		{
			requiredExtensions.push_back("VK_KHR_maintenance3");
			requiredExtensions.push_back("VK_EXT_descriptor_indexing");
			requiredExtensions.push_back("VK_KHR_buffer_device_address");
			requiredExtensions.push_back("VK_KHR_deferred_host_operations");
			requiredExtensions.push_back("VK_KHR_acceleration_structure");
			requiredExtensions.push_back("VK_KHR_shader_float_controls");
			requiredExtensions.push_back("VK_KHR_spirv_1_4");
			requiredExtensions.push_back("VK_KHR_ray_tracing_pipeline");
		}

		const VkDeviceCreateInfo createInfo
		{
			VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,				//	VkStructureType					sType;
			deviceFeatures.pNext,								//	const void*						pNext;
			0u,													//	VkDeviceCreateFlags				flags;
			1u,													//	deUint32						queueCreateInfoCount;
			&queueInfo,											//	const VkDeviceQueueCreateInfo*	pQueueCreateInfos;
			0u,													//	deUint32						enabledLayerCount;
			nullptr,											//	const char* const*				ppEnabledLayerNames;
			de::sizeU32(requiredExtensions),					//	deUint32						enabledExtensionCount;
			de::dataOrNull(requiredExtensions),					//	const char* const*				ppEnabledExtensionNames;
			&deviceFeatures.features,							//	const VkPhysicalDeviceFeatures*	pEnabledFeatures;
		};

		// Create custom device and related objects
		device = createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(), vkp, instance, vki, physicalDevice, &createInfo);
		vkd.reset(new DeviceDriver(vkp, instance, device.get(), context.getUsedApiVersion()));
		queue = getDeviceQueue(*vkd, *device, queueFamilyIndex, 0u);
		allocator.reset(new SimpleAllocator(*vkd, device.get(), getPhysicalDeviceMemoryProperties(vki, physicalDevice)));
	}
};

tcu::TestStatus ConstantModuleIdentifiersInstance::iterate (void)
{
	// The second device may be the one from the context or a new device for the cases that require different devices.
	const auto&							vkd		= m_context.getDeviceInterface();
	const auto							device	= m_context.getDevice();
	const std::unique_ptr<DeviceHelper>	helper	(m_params->differentDevices ? new DeviceHelper(m_context) : nullptr);

	const auto&		di1		= vkd;
	const auto		dev1	= device;
	const auto&		di2		= (m_params->differentDevices ? *helper->vkd : vkd);
	const auto		dev2	= (m_params->differentDevices ? helper->device.get() : device);

	return runTest(di1, dev1, di2, dev2);
}

class ConstantModuleIdentifiersCase : public SourcesAndSupportFromParamsBase
{
public:
	using Params	= ConstantModuleIdentifiersInstance::Params;
	using ParamsPtr	= ConstantModuleIdentifiersInstance::ParamsPtr;

					ConstantModuleIdentifiersCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, ParamsPtr&& params)
						: SourcesAndSupportFromParamsBase(testCtx, name, description, BaseParamsPtr(static_cast<BaseParams*>(params.release())))
						{}
	virtual			~ConstantModuleIdentifiersCase	(void) {}
	TestInstance*	createInstance					(Context& context) const override;
};

TestInstance* ConstantModuleIdentifiersCase::createInstance (Context &context) const
{
	const auto paramsPtr = dynamic_cast<Params*>(m_params.get());
	DE_ASSERT(paramsPtr);

	return new ConstantModuleIdentifiersInstance(context, paramsPtr);
}

// Tests that create one or more pipelines using several shaders, obtain the shader ids from one of the pipelines and use them to
// attempt creation of a new pipeline to be used normally.
class CreateAndUseIdsInstance : public vkt::TestInstance
{
public:
	using RndGenPtr = std::shared_ptr<de::Random>;

	struct Params : public BaseParams
	{
		PipelineConstructionType	constructionType;
		bool						useRTLibraries;		// Use ray tracing libraries? For monolithic builds only.
		bool						useMaintenance5;
		UseModuleCase				moduleUseCase;
		CapturedPropertiesFlags		capturedProperties;	// For UseModuleCase::ID only.
		RndGenPtr					rnd;

		Params (PipelineType				pipelineType_,
				GraphicsShaderVec			graphicsShaders_,
				RTShaderVec					rtShaders_,
				uint8_t						pipelineCount_,
				const tcu::Maybe<uint8_t>&	pipelineToRun_,
				bool						useSCs_,
				bool						useCache_,
				bool						useMaintenance5_,
				PipelineConstructionType	constructionType_,
				bool						useRTLibraries_,
				UseModuleCase				moduleUseCase_,
				CapturedPropertiesFlags		capturedProperties_)
			: BaseParams		(pipelineType_, graphicsShaders_, rtShaders_, pipelineCount_, pipelineToRun_, useSCs_, useCache_, useMaintenance5_)
			, constructionType	(constructionType_)
			, useRTLibraries	(useRTLibraries_)
			, useMaintenance5	(false)
			, moduleUseCase		(moduleUseCase_)
			, capturedProperties(capturedProperties_)
			, rnd				()
			{
				DE_ASSERT(!useRTLibraries || hasRayTracing());
				DE_ASSERT(!useRTLibraries || constructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC);
				DE_ASSERT(capturedProperties == 0u || moduleUseCase == UseModuleCase::ID);

				// We will only be capturing properties if using one pipeline that will be run later.
				DE_ASSERT(capturedProperties == 0u || (pipelineCount == uint8_t{1} && static_cast<bool>(pipelineToRun)));
			}

		virtual ~Params () {}

		// Convenience helper method.
		de::Random& getRndGen (void) const
		{
			return *rnd;
		}

		// Copy parameters resetting the random number generator with a new seed.
		BaseParamsPtr copy (uint32_t newSeed)
		{
			std::unique_ptr<Params> clone (new Params(*this));
			clone->rnd.reset(new de::Random(newSeed));
			return BaseParamsPtr(clone.release());
		}
	};

	using ParamsPtr = std::unique_ptr<Params>;

						CreateAndUseIdsInstance		(Context& context, const Params* params)
							: vkt::TestInstance	(context)
							, m_params			(params)
							{}
	virtual				~CreateAndUseIdsInstance	(void) {}

	tcu::TestStatus		iterate						(void) override;

protected:
	const Params* m_params;
};

class CreateAndUseIdsCase : public SourcesAndSupportFromParamsBase
{
public:
					CreateAndUseIdsCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, BaseParamsPtr&& params)
						: SourcesAndSupportFromParamsBase	(testCtx, name, description, std::move(params))
						, m_createAndUseIdsParams			(dynamic_cast<const CreateAndUseIdsInstance::Params*>(m_params.get()))
						{
							DE_ASSERT(m_createAndUseIdsParams);
						}
	virtual			~CreateAndUseIdsCase	(void) {}
	void			checkSupport			(Context& context) const override;
	TestInstance*	createInstance			(Context& context) const override;

protected:
	const CreateAndUseIdsInstance::Params* m_createAndUseIdsParams;
};

void CreateAndUseIdsCase::checkSupport (Context &context) const
{
	SourcesAndSupportFromParamsBase::checkSupport(context);

	checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), m_createAndUseIdsParams->constructionType);

	if (m_createAndUseIdsParams->useRTLibraries)
		context.requireDeviceFunctionality("VK_KHR_pipeline_library");

	if (m_createAndUseIdsParams->capturedProperties != 0u)
		context.requireDeviceFunctionality("VK_KHR_pipeline_executable_properties");

	if ((m_params->pipelineType == PipelineType::COMPUTE || m_params->hasRayTracing()) && static_cast<bool>(m_params->pipelineToRun)) {
		const auto features = context.getPipelineCreationCacheControlFeatures();
		if (features.pipelineCreationCacheControl == DE_FALSE)
			TCU_THROW(NotSupportedError, "Feature 'pipelineCreationCacheControl' is not enabled");
	}

	if (m_params->useMaintenance5)
		context.requireDeviceFunctionality("VK_KHR_maintenance5");
}

TestInstance* CreateAndUseIdsCase::createInstance (Context &context) const
{
	return new CreateAndUseIdsInstance(context, m_createAndUseIdsParams);
}

using SpecInfoPtr	= std::unique_ptr<VkSpecializationInfo>;
using SCMapEntryVec	= std::vector<VkSpecializationMapEntry>;

SpecInfoPtr maybeMakeSpecializationInfo (bool makeIt, const VkSpecializationMapEntry* entry, std::vector<uint32_t>::const_iterator& iter)
{
	if (!makeIt)
		return nullptr;

	DE_ASSERT(entry);
	SpecInfoPtr info (new VkSpecializationInfo);

	info->mapEntryCount	= 1u;
	info->pMapEntries	= entry;
	info->dataSize		= sizeof(uint32_t);
	info->pData			= &(*(iter++));

	return info;
}

VkPipelineRasterizationStateCreateInfo makeRasterizationState (bool rasterizationDisabled)
{
	VkPipelineRasterizationStateCreateInfo state = initVulkanStructure();
	state.rasterizerDiscardEnable = (rasterizationDisabled ? VK_TRUE : VK_FALSE);
	state.lineWidth = 1.0f;
	return state;
}

class PipelineStageInfo
{
protected:
	ShaderWrapper		m_shader;
	ShaderModuleId		m_moduleId;
	ShaderStageIdPtr	m_moduleIdCreateInfo;
	SpecInfoPtr			m_specInfo;

public:
	PipelineStageInfo ()
		: m_shader				()
		, m_moduleId			()
		, m_moduleIdCreateInfo	()
		, m_specInfo			()
		{}

	void setModule (const DeviceInterface &vkd, const VkDevice device, const ShaderWrapper shader, UseModuleCase moduleUse, de::Random& rnd)
	{
		m_shader				= shader;

		m_moduleId				= getShaderModuleIdentifier(vkd, device, shader.getModule());
		maybeMangleShaderModuleId(m_moduleId, moduleUse, rnd);

		m_moduleIdCreateInfo	= makeShaderStageModuleIdentifierCreateInfo(m_moduleId, moduleUse, &rnd);
	}

	void setSpecInfo (SpecInfoPtr&& specInfo)
	{
		m_specInfo = std::move(specInfo);
	}

	ShaderWrapper getModule (void) const
	{
		return m_shader;
	}

	ShaderWrapper* getUsedModule (UseModuleCase moduleUse)
	{
		return retUsedModule(&m_shader, moduleUse);
	}

	const VkPipelineShaderStageModuleIdentifierCreateInfoEXT* getModuleIdCreateInfo (void) const
	{
		return m_moduleIdCreateInfo.get();
	}

	const VkSpecializationInfo* getSpecInfo (void) const
	{
		return m_specInfo.get();
	}

	// Forbid copy and assignment. This would break the relationship between moduleId and moduleIdCreateInfo.
	PipelineStageInfo (const PipelineStageInfo&) = delete;
	PipelineStageInfo& operator=(const PipelineStageInfo&) = delete;
};

std::vector<uint32_t> makeComputeSpecConstants (uint32_t stageConstant)
{
	return std::vector<uint32_t>{stageConstant, 1u, 1u, 1u};
}

SCMapEntryVec makeComputeSpecMapEntries (void)
{
	const auto		kNumEntries	= 4u; // Matches the vector above.
	const auto		entrySizeSz	= sizeof(uint32_t);
	const auto		entrySize	= static_cast<uint32_t>(entrySizeSz);
	SCMapEntryVec	entries;

	entries.reserve(kNumEntries);
	for (uint32_t i = 0u; i < kNumEntries; ++i)
	{
		const VkSpecializationMapEntry entry =
		{
			i,					//	uint32_t	constantID;
			(entrySize * i),	//	uint32_t	offset;
			entrySizeSz,		//	size_t		size;
		};
		entries.push_back(entry);
	}

	return entries;
}

SpecInfoPtr makeComputeSpecInfo (const SCMapEntryVec& scEntries, const std::vector<uint32_t>& scData)
{
	SpecInfoPtr scInfo (new VkSpecializationInfo);

	scInfo->mapEntryCount	= de::sizeU32(scEntries);
	scInfo->pMapEntries		= de::dataOrNull(scEntries);
	scInfo->dataSize		= de::dataSize(scData);
	scInfo->pData			= de::dataOrNull(scData);

	return scInfo;
}

tcu::TestStatus CreateAndUseIdsInstance::iterate (void)
{
	const auto&			vki				= m_context.getInstanceInterface();
	const auto&			vkd				= m_context.getDeviceInterface();
	const auto			physicalDevice	= m_context.getPhysicalDevice();
	const auto			device			= m_context.getDevice();
	auto&				alloc			= m_context.getDefaultAllocator();
	const auto			queue			= m_context.getUniversalQueue();
	const auto			queueIndex		= m_context.getUniversalQueueFamilyIndex();

	const auto			pipelineStages	= m_params->getPipelineStageFlags();
	const auto			shaderStages	= m_params->getShaderStageFlags();
	const auto			captureFlags	= getPipelineCreateFlags(m_params->capturedProperties);
	const bool			needsCapture	= (captureFlags != 0u);
	const auto			isGraphics		= (m_params->pipelineType == PipelineType::GRAPHICS);
	const auto			isCompute		= (m_params->pipelineType == PipelineType::COMPUTE);
	const auto			fbFormat		= VK_FORMAT_R8G8B8A8_UNORM;
	const auto			tcuFbFormat		= mapVkFormat(fbFormat);
	const auto			pixelSize		= tcu::getPixelSize(tcuFbFormat);
	const auto			fbExtent		= makeExtent3D(1u, 1u, 1u);
	const tcu::IVec3	iExtent			(static_cast<int>(fbExtent.width), static_cast<int>(fbExtent.height), static_cast<int>(fbExtent.depth));
	const auto			isRT			= m_params->hasRayTracing();
	const auto			hasHit			= m_params->hasHit();
	const auto			hasHitAndMiss	= hasHit && m_params->hasMiss();
	const auto			stagesCount		= m_params->stageCountPerPipeline();
	const auto			pipelineCount32	= static_cast<uint32_t>(m_params->pipelineCount);
	const auto			hasTess			= m_params->hasTess();
	const auto			topology		= (hasTess ? VK_PRIMITIVE_TOPOLOGY_PATCH_LIST : VK_PRIMITIVE_TOPOLOGY_POINT_LIST);
	const auto			patchCPs		= (hasTess ? 3u : 0u);
	const auto			useSCs			= m_params->useSpecializationConstants;
	const auto			shaderConstants	= generateShaderConstants(m_params->pipelineType, m_params->pipelineCount, stagesCount);
	const auto			runOnePipeline	= static_cast<bool>(m_params->pipelineToRun);
	const bool			reqCacheMiss	= expectCacheMiss(m_params->moduleUseCase);
	const bool			qualityWarn		= (m_params->useCache && !needsCapture);
	const tcu::Vec4		clearColor		(0.0f, 0.0f, 0.0f, 0.0f);
	const tcu::Vec4		blueColor		(0.0f, 0.0f, 1.0f, 1.0f); // Must match fragment shader above.

	// Used when capturing pipeline executable properties.
	PipelineExecutablePropertyVec classicExeProps;
	PipelineExecutablePropertyVec identifierExeProps;

	// Command pool and buffer.
	const auto cmdPool		= makeCommandPool(vkd, device, queueIndex);
	const auto cmdBufferPtr	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	// Begin command buffer. We may need it below for RT.
	beginCommandBuffer(vkd, cmdBuffer);

	// Descriptor set layouts. Typically 1 but ray tracing tests use a separate set for the acceleration structure.
	std::vector<VkDescriptorSetLayout> setLayouts;

	DescriptorSetLayoutBuilder setLayoutBuilder;
	setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, shaderStages);
	for (uint8_t i = 0; i < m_params->pipelineCount; ++i)
		setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, shaderStages);
	const auto mainSetLayout = setLayoutBuilder.build(vkd, device);
	setLayouts.push_back(mainSetLayout.get());

	const auto auxSetLayout	= (isRT
							? DescriptorSetLayoutBuilder().addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, shaderStages).build(vkd, device)
							: Move<VkDescriptorSetLayout>());
	if (isRT)
		setLayouts.push_back(auxSetLayout.get());

	// Pipeline layout.
	PipelineLayoutWrapper pipelineLayout (m_params->constructionType, vkd, device, de::sizeU32(setLayouts), de::dataOrNull(setLayouts));

	// Descriptor pool.
	DescriptorPoolBuilder poolBuilder;
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, pipelineCount32);
	if (isRT)
		poolBuilder.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
	const auto descriptorPool = poolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, de::sizeU32(setLayouts));

	// Descriptor buffers.
	const auto			storageBufferSize	= static_cast<VkDeviceSize>(sizeof(uint32_t) * stagesCount);
	const auto			storageBufferInfo	= makeBufferCreateInfo(storageBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	BufferWithMemory	storageBuffer		(vkd, device, alloc, storageBufferInfo, MemoryRequirement::HostVisible);
	auto&				storageBufferAlloc	= storageBuffer.getAllocation();
	void*				storageBufferData	= storageBufferAlloc.getHostPtr();

	// For the uniform buffers we'll use a single allocation.
	const auto			deviceProperties	= getPhysicalDeviceProperties(vki, physicalDevice);
	const auto			minBlock			= de::roundUp(static_cast<VkDeviceSize>(sizeof(uint32_t)), deviceProperties.limits.minUniformBufferOffsetAlignment);
	const auto			uniformBufferSize	= minBlock * pipelineCount32;
	const auto			uniformBufferInfo	= makeBufferCreateInfo(uniformBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	BufferWithMemory	uniformBuffer		(vkd, device, alloc, uniformBufferInfo, MemoryRequirement::HostVisible);
	auto&				uniformBufferAlloc	= uniformBuffer.getAllocation();
	void*				uniformBufferData	= uniformBufferAlloc.getHostPtr();

	deMemset(storageBufferData, 0, static_cast<size_t>(storageBufferSize));
	deMemset(uniformBufferData, 0, static_cast<size_t>(uniformBufferSize));
	flushAlloc(vkd, device, storageBufferAlloc);
	flushAlloc(vkd, device, uniformBufferAlloc);

	// Acceleration structures if needed.
	using TLASPtr = de::MovePtr<TopLevelAccelerationStructure>;
	using BLASPtr = de::SharedPtr<BottomLevelAccelerationStructure>;

	TLASPtr tlas;
	BLASPtr blas;

	if (isRT)
	{
		tlas = makeTopLevelAccelerationStructure();
		blas = BLASPtr(makeBottomLevelAccelerationStructure().release());

		// If we don't want hits we move the geometry way off in the X axis.
		// If we want hits and misses we launch 2 rays (see raygen shader).
		const float xOffset = (hasHit ? 0.0f : 100.0f);

		if (m_params->hasISec())
		{
			// AABB around (0.5, 0.5, 5).
			const std::vector<tcu::Vec3> geometry
			{
				tcu::Vec3(0.0f + xOffset, 0.0f, 4.0f),
				tcu::Vec3(1.0f + xOffset, 1.0f, 6.0f),
			};

			blas->addGeometry(geometry, false/*isTriangles*/, VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR);
		}
		else
		{
			// Triangle surrounding (0.5, 0.5, 5).
			const std::vector<tcu::Vec3> geometry
			{
				tcu::Vec3(0.25f + xOffset, 0.25f, 5.0f),
				tcu::Vec3(0.75f + xOffset, 0.25f, 5.0f),
				tcu::Vec3(0.5f  + xOffset, 0.75f, 5.0f),
			};

			blas->addGeometry(geometry, true/*isTriangles*/, VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR);
		}
		blas->createAndBuild(vkd, device, cmdBuffer, alloc);
		tlas->setInstanceCount(1u);
		tlas->addInstance(blas, identityMatrix3x4, 0u, 0xFFu, 0u, VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR);

		tlas->createAndBuild(vkd, device, cmdBuffer, alloc);
	}

	// Graphics pipeline data if needed.
	std::unique_ptr<ImageWithMemory>	colorAtt;
	VkImageSubresourceRange				colorSRR;
	VkImageSubresourceLayers			colorSRL;
	Move<VkImageView>					colorAttView;
	RenderPassWrapper					renderPass;
	std::unique_ptr<BufferWithMemory>	verifBuffer;
	std::vector<VkViewport>				viewports;
	std::vector<VkRect2D>				scissors;

	// This is constant for all shader stages.
	const VkSpecializationMapEntry scMapEntry =
	{
		0u,					//	uint32_t	constantID;
		0u,					//	uint32_t	offset;
		sizeof(uint32_t),	//	size_t		size;
	};

	if (isGraphics)
	{
		const VkImageCreateInfo colorAttCreateInfo =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,										//	VkStructureType			sType;
			nullptr,																	//	const void*				pNext;
			0u,																			//	VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,															//	VkImageType				imageType;
			fbFormat,																	//	VkFormat				format;
			fbExtent,																	//	VkExtent3D				extent;
			1u,																			//	uint32_t				mipLevels;
			1u,																			//	uint32_t				arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,														//	VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,													//	VkImageTiling			tiling;
			(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT),	//	VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,													//	VkSharingMode			sharingMode;
			0u,																			//	uint32_t				queueFamilyIndexCount;
			nullptr,																	//	const uint32_t*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED,													//	VkImageLayout			initialLayout;
		};

		colorAtt		.reset(new ImageWithMemory(vkd, device, alloc, colorAttCreateInfo, MemoryRequirement::Any));
		colorSRR		= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
		colorSRL		= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
		colorAttView	= makeImageView(vkd, device, colorAtt->get(), VK_IMAGE_VIEW_TYPE_2D, fbFormat, colorSRR);
		renderPass		= RenderPassWrapper(m_params->constructionType, vkd, device, fbFormat);
		renderPass.createFramebuffer(vkd, device, **colorAtt, colorAttView.get(), fbExtent.width, fbExtent.height);

		DE_ASSERT(fbExtent.width == 1u && fbExtent.height == 1u && fbExtent.depth == 1u);
		const auto verifBufferSize = static_cast<VkDeviceSize>(pixelSize);
		const auto verifBufferInfo = makeBufferCreateInfo(verifBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
		verifBuffer.reset(new BufferWithMemory(vkd, device, alloc, verifBufferInfo, MemoryRequirement::HostVisible));

		viewports.push_back(makeViewport(fbExtent));
		scissors.push_back(makeRect2D(fbExtent));
	}

	// Descriptor sets.
	const auto mainDescriptorSet	= makeDescriptorSet(vkd, device, descriptorPool.get(), mainSetLayout.get());
	const auto auxDescriptorSet		= (isRT ? makeDescriptorSet(vkd, device, descriptorPool.get(), auxSetLayout.get()) : Move<VkDescriptorSet>());

	std::vector<VkDescriptorSet> rawDescriptorSets;
	rawDescriptorSets.push_back(mainDescriptorSet.get());
	if (isRT)
		rawDescriptorSets.push_back(auxDescriptorSet.get());

	// Update descriptor sets.
	DescriptorSetUpdateBuilder updateBuilder;
	{
		const auto storageDescInfo = makeDescriptorBufferInfo(storageBuffer.get(), 0ull, storageBufferSize);
		updateBuilder.writeSingle(mainDescriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &storageDescInfo);
	}
	for (uint32_t uboIdx = 0u; uboIdx < pipelineCount32; ++uboIdx)
	{
		const auto uboDescInfo = makeDescriptorBufferInfo(uniformBuffer.get(), minBlock * uboIdx, minBlock);
		updateBuilder.writeSingle(mainDescriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(uboIdx + 1u), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &uboDescInfo);
	}
	if (isRT)
	{
		const VkWriteDescriptorSetAccelerationStructureKHR accelDescInfo =
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
			nullptr,
			1u,
			tlas.get()->getPtr(),
		};

		updateBuilder.writeSingle(auxDescriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &accelDescInfo);
	}
	updateBuilder.update(vkd, device);

	// Make pipelines.
	using ModuleVec			= std::vector<ShaderWrapper>;
	using PipelinePtrVec	= std::vector<Move<VkPipeline>>;
	using PipelineVec		= std::vector<VkPipeline>;
	using WrapperVec		= std::vector<std::unique_ptr<GraphicsPipelineWrapper>>;
	using BufferPtr			= de::MovePtr<BufferWithMemory>;

	ModuleVec vertModules;
	ModuleVec tescModules;
	ModuleVec teseModules;
	ModuleVec geomModules;
	ModuleVec fragModules;

	ModuleVec compModules;

	ModuleVec rgenModules;
	ModuleVec ahitModules;
	ModuleVec chitModules;
	ModuleVec isecModules;
	ModuleVec missModules;
	ModuleVec callModules;

	BufferPtr rgenSBT;
	BufferPtr xhitSBT;
	BufferPtr missSBT;
	BufferPtr callSBT;

	VkStridedDeviceAddressRegionKHR rgenRegion = makeStridedDeviceAddressRegionKHR(DE_NULL, 0ull, 0ull);
	VkStridedDeviceAddressRegionKHR xhitRegion = makeStridedDeviceAddressRegionKHR(DE_NULL, 0ull, 0ull);
	VkStridedDeviceAddressRegionKHR missRegion = makeStridedDeviceAddressRegionKHR(DE_NULL, 0ull, 0ull);
	VkStridedDeviceAddressRegionKHR callRegion = makeStridedDeviceAddressRegionKHR(DE_NULL, 0ull, 0ull);

	WrapperVec				pipelineWrappers;	// For graphics pipelines.
	PipelinePtrVec			pipelinePtrs;		// For other pipelines.
	PipelineVec				pipelines;
	Move<VkPipelineCache>	pipelineCache;

	if (m_params->useCache)
	{
		const VkPipelineCacheCreateInfo cacheCreateInfo = initVulkanStructure();
		pipelineCache = createPipelineCache(vkd, device, &cacheCreateInfo);
	}

	const auto& binaries = m_context.getBinaryCollection();

	if (isGraphics)
	{
		const VkPipelineVertexInputStateCreateInfo		vertexInputState			= initVulkanStructure();
		const VkPipelineInputAssemblyStateCreateInfo	inputAssemblyState			=
		{
			VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	//	VkStructureType							sType;
			nullptr,														//	const void*								pNext;
			0u,																//	VkPipelineInputAssemblyStateCreateFlags	flags;
			topology,														//	VkPrimitiveTopology						topology;
			VK_FALSE,														//	VkBool32								primitiveRestartEnable;
		};
		const VkPipelineDepthStencilStateCreateInfo		depthStencilState			= initVulkanStructure();
		VkPipelineMultisampleStateCreateInfo			multisampleState			= initVulkanStructure();
		multisampleState.rasterizationSamples										= VK_SAMPLE_COUNT_1_BIT;
		VkPipelineColorBlendAttachmentState				colorBlendAttachmentState;
		deMemset(&colorBlendAttachmentState, 0, sizeof(colorBlendAttachmentState));
		colorBlendAttachmentState.colorWriteMask									= (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);
		const VkPipelineColorBlendStateCreateInfo		colorBlendState				=
		{
			VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	//	VkStructureType								sType
			nullptr,													//	const void*									pNext
			0u,															//	VkPipelineColorBlendStateCreateFlags		flags
			VK_FALSE,													//	VkBool32									logicOpEnable
			VK_LOGIC_OP_CLEAR,											//	VkLogicOp									logicOp
			1u,															//	deUint32									attachmentCount
			&colorBlendAttachmentState,									//	const VkPipelineColorBlendAttachmentState*	pAttachments
			{ 0.0f, 0.0f, 0.0f, 0.0f }									//	float										blendConstants[4]
		};

		auto shaderConstIt = shaderConstants.begin();

		// In case we have to run a pipeline.
		PipelineStageInfo vertToRun;
		PipelineStageInfo tescToRun;
		PipelineStageInfo teseToRun;
		PipelineStageInfo geomToRun;
		PipelineStageInfo fragToRun;

		for (uint32_t i = 0; i < pipelineCount32; ++i)
		{
			const auto runThis	= (runOnePipeline && static_cast<uint32_t>(m_params->pipelineToRun.get()) == i);
			const auto suffix	= "_" + std::to_string(i);
			const auto vertName	= "vert" + suffix;
			const auto tescName	= "tesc" + suffix;
			const auto teseName	= "tese" + suffix;
			const auto geomName	= "geom" + suffix;
			const auto fragName	= "frag" + suffix;

			pipelineWrappers.emplace_back(new GraphicsPipelineWrapper(vki, vkd, physicalDevice, device, m_context.getDeviceExtensions(), m_params->constructionType, captureFlags));
			auto& wrapper = *pipelineWrappers.back();

			ShaderWrapper	vertModule;
			ShaderWrapper	tescModule;
			ShaderWrapper	teseModule;
			ShaderWrapper	geomModule;
			ShaderWrapper	fragModule;

			SpecInfoPtr		vertSpecInfo;
			SpecInfoPtr		tescSpecInfo;
			SpecInfoPtr		teseSpecInfo;
			SpecInfoPtr		geomSpecInfo;
			SpecInfoPtr		fragSpecInfo;

			vertModules		.push_back(ShaderWrapper(vkd, device, binaries.get(vertName)));
			vertModule		= vertModules.back();
			vertSpecInfo	= maybeMakeSpecializationInfo(useSCs, &scMapEntry, shaderConstIt);

			if (binaries.contains(tescName))
			{
				tescModules		.push_back(ShaderWrapper(vkd, device, binaries.get(tescName)));
				tescModule		= tescModules.back();
				tescSpecInfo	= maybeMakeSpecializationInfo(useSCs, &scMapEntry, shaderConstIt);
			}

			if (binaries.contains(teseName))
			{
				teseModules		.push_back(ShaderWrapper(vkd, device, binaries.get(teseName)));
				teseModule		= teseModules.back();
				teseSpecInfo	= maybeMakeSpecializationInfo(useSCs, &scMapEntry, shaderConstIt);
			}

			if (binaries.contains(geomName))
			{
				geomModules		.push_back(ShaderWrapper(vkd, device, binaries.get(geomName)));
				geomModule		= geomModules.back();
				geomSpecInfo	= maybeMakeSpecializationInfo(useSCs, &scMapEntry, shaderConstIt);
			}

			if (binaries.contains(fragName))
			{
				fragModules		.push_back(ShaderWrapper(vkd, device, binaries.get(fragName)));
				fragModule		= fragModules.back();
				fragSpecInfo	= maybeMakeSpecializationInfo(useSCs, &scMapEntry, shaderConstIt);
			}

			const auto rasterizationState = makeRasterizationState(!fragModule.isSet());

			if (m_params->useMaintenance5)
				wrapper.setPipelineCreateFlags2(translateCreateFlag(captureFlags));

			wrapper	.setDefaultPatchControlPoints(patchCPs)
					.setupVertexInputState(&vertexInputState, &inputAssemblyState, pipelineCache.get())
					.setupPreRasterizationShaderState2(
						viewports,
						scissors,
						pipelineLayout,
						renderPass.get(),
						0u,
						vertModule,
						&rasterizationState,
						tescModule,
						teseModule,
						geomModule,
						vertSpecInfo.get(),
						tescSpecInfo.get(),
						teseSpecInfo.get(),
						geomSpecInfo.get(),
						nullptr,
						PipelineRenderingCreateInfoWrapper(),
						pipelineCache.get())
					.setupFragmentShaderState(
						pipelineLayout,
						renderPass.get(),
						0u,
						fragModule,
						&depthStencilState,
						&multisampleState,
						fragSpecInfo.get(),
						pipelineCache.get())
					.setupFragmentOutputState(*renderPass, 0u, &colorBlendState, &multisampleState, pipelineCache.get())
					.setMonolithicPipelineLayout(pipelineLayout)
					.buildPipeline(pipelineCache.get());

			pipelines.push_back(wrapper.getPipeline());

			// Capture properties if needed.
			if (needsCapture)
				classicExeProps = getPipelineExecutableProperties(vkd, device, pipelines.back(), m_params->capturedProperties);

			if (runThis)
			{
				vertToRun.setModule(vkd, device, vertModule, m_params->moduleUseCase, m_params->getRndGen());
				vertToRun.setSpecInfo(std::move(vertSpecInfo));

				if (tescModule.isSet())
				{
					tescToRun.setModule(vkd, device, tescModule, m_params->moduleUseCase, m_params->getRndGen());
					tescToRun.setSpecInfo(std::move(tescSpecInfo));
				}

				if (teseModule.isSet())
				{
					teseToRun.setModule(vkd, device, teseModule, m_params->moduleUseCase, m_params->getRndGen());
					teseToRun.setSpecInfo(std::move(teseSpecInfo));
				}

				if (geomModule.isSet())
				{
					geomToRun.setModule(vkd, device, geomModule, m_params->moduleUseCase, m_params->getRndGen());
					geomToRun.setSpecInfo(std::move(geomSpecInfo));
				}

				if (fragModule.isSet())
				{
					fragToRun.setModule(vkd, device, fragModule, m_params->moduleUseCase, m_params->getRndGen());
					fragToRun.setSpecInfo(std::move(fragSpecInfo));
				}
			}
		}

		if (runOnePipeline)
		{
			// Append the pipeline to run at the end of the vector.
			pipelineWrappers.emplace_back(new GraphicsPipelineWrapper(vki, vkd, physicalDevice, device, m_context.getDeviceExtensions(), m_params->constructionType, captureFlags));
			auto& wrapper = *pipelineWrappers.back();

			const auto fragModule			= fragToRun.getModule();
			const auto rasterizationState	= makeRasterizationState(!fragModule.isSet());

			try
			{
				wrapper	.setDefaultPatchControlPoints(patchCPs)
						.setupVertexInputState(&vertexInputState, &inputAssemblyState, pipelineCache.get())
						.setupPreRasterizationShaderState3(
							viewports,
							scissors,
							pipelineLayout,
							renderPass.get(),
							0u,
							*vertToRun.getUsedModule(m_params->moduleUseCase),
							PipelineShaderStageModuleIdentifierCreateInfoWrapper(vertToRun.getModuleIdCreateInfo()),
							&rasterizationState,
							*tescToRun.getUsedModule(m_params->moduleUseCase),
							PipelineShaderStageModuleIdentifierCreateInfoWrapper(tescToRun.getModuleIdCreateInfo()),
							*teseToRun.getUsedModule(m_params->moduleUseCase),
							PipelineShaderStageModuleIdentifierCreateInfoWrapper(teseToRun.getModuleIdCreateInfo()),
							*geomToRun.getUsedModule(m_params->moduleUseCase),
							PipelineShaderStageModuleIdentifierCreateInfoWrapper(geomToRun.getModuleIdCreateInfo()),
							vertToRun.getSpecInfo(),
							tescToRun.getSpecInfo(),
							teseToRun.getSpecInfo(),
							geomToRun.getSpecInfo(),
							nullptr,
							PipelineRenderingCreateInfoWrapper(),
							pipelineCache.get())
						.setupFragmentShaderState2(
							pipelineLayout,
							renderPass.get(),
							0u,
							*fragToRun.getUsedModule(m_params->moduleUseCase),
							fragToRun.getModuleIdCreateInfo(),
							&depthStencilState,
							&multisampleState,
							fragToRun.getSpecInfo(),
							pipelineCache.get())
						.setupFragmentOutputState(*renderPass, 0u, &colorBlendState, &multisampleState, pipelineCache.get())
						.setMonolithicPipelineLayout(pipelineLayout)
						.buildPipeline(pipelineCache.get());

				if (reqCacheMiss)
					TCU_FAIL("Cache miss expected");
			}
			catch (const PipelineCompileRequiredError& err)
			{
				if (reqCacheMiss)
					return tcu::TestStatus::pass("Pass");

				if (qualityWarn)
					return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "VK_PIPELINE_COMPILE_REQUIRED despite passing a pipeline cache");
				return tcu::TestStatus::pass("VK_PIPELINE_COMPILE_REQUIRED"); // ;_;
			}

			pipelines.push_back(wrapper.getPipeline());

			if (needsCapture)
				identifierExeProps = getPipelineExecutableProperties(vkd, device, pipelines.back(), m_params->capturedProperties);
		}
	}
	else if (isCompute)
	{
		const auto	invalidPipelineIdx	= std::numeric_limits<uint32_t>::max();
		auto		idxToRun			= invalidPipelineIdx;

		for (uint32_t i = 0; i < pipelineCount32; ++i)
		{
			const auto runThis		= (runOnePipeline && static_cast<uint32_t>(m_params->pipelineToRun.get()) == i);
			const auto suffix		= "_" + std::to_string(i);
			const auto compName		= "comp" + suffix;

			const auto scData		= (useSCs ? makeComputeSpecConstants(shaderConstants.at(i)) : std::vector<uint32_t>());
			const auto scEntries	= (useSCs ? makeComputeSpecMapEntries() : std::vector<VkSpecializationMapEntry>());
			const auto scInfo		= (useSCs ? makeComputeSpecInfo(scEntries, scData) : nullptr);

			compModules.push_back(ShaderWrapper(vkd, device, binaries.get(compName)));
			pipelinePtrs.push_back(makeComputePipeline(vkd, device, pipelineLayout.get(), captureFlags, nullptr, compModules.back().getModule(), 0u, scInfo.get(), pipelineCache.get()));
			pipelines.push_back(pipelinePtrs.back().get());

			if (runThis)
				idxToRun = i;

			if (needsCapture)
				classicExeProps = getPipelineExecutableProperties(vkd, device, pipelines.back(), m_params->capturedProperties);
		}

		if (idxToRun != invalidPipelineIdx)
		{
			auto& compModule		= compModules.at(idxToRun);
			auto moduleId			= getShaderModuleIdentifier(vkd, device, compModule.getModule());

			maybeMangleShaderModuleId(moduleId, m_params->moduleUseCase, m_params->getRndGen());

			const auto modInfo		= makeShaderStageModuleIdentifierCreateInfo(moduleId, m_params->moduleUseCase, &(m_params->getRndGen()));
			const auto scData		= (useSCs ? makeComputeSpecConstants(shaderConstants.at(idxToRun)) : std::vector<uint32_t>());
			const auto scEntries	= (useSCs ? makeComputeSpecMapEntries() : std::vector<VkSpecializationMapEntry>());
			const auto scInfo		= (useSCs ? makeComputeSpecInfo(scEntries, scData) : nullptr);

			// Append the pipeline to run at the end of the vector.
			{
				const auto pipelineFlags = (VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT | captureFlags);

				const VkPipelineShaderStageCreateInfo pipelineShaderStageParams =
				{
					VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,				// VkStructureType						sType;
					modInfo.get(),														// const void*							pNext;
					0u,																	// VkPipelineShaderStageCreateFlags		flags;
					VK_SHADER_STAGE_COMPUTE_BIT,										// VkShaderStageFlagBits				stage;
					retUsedModule(&compModule, m_params->moduleUseCase)->getModule(),	// VkShaderModule						module;
					"main",																// const char*							pName;
					scInfo.get(),														// const VkSpecializationInfo*			pSpecializationInfo;
				};

				const VkComputePipelineCreateInfo pipelineCreateInfo =
				{
					VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,		// VkStructureType					sType;
					nullptr,											// const void*						pNext;
					pipelineFlags,										// VkPipelineCreateFlags			flags;
					pipelineShaderStageParams,							// VkPipelineShaderStageCreateInfo	stage;
					pipelineLayout.get(),								// VkPipelineLayout					layout;
					DE_NULL,											// VkPipeline						basePipelineHandle;
					0,													// deInt32							basePipelineIndex;
				};

				VkPipeline	pipeline;
				VkResult	creationResult = vkd.createComputePipelines(device, pipelineCache.get(), 1u, &pipelineCreateInfo, nullptr, &pipeline);

				if (creationResult == VK_PIPELINE_COMPILE_REQUIRED)
				{
					if (reqCacheMiss)
						return tcu::TestStatus::pass("Pass");

					if (qualityWarn)
						return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "VK_PIPELINE_COMPILE_REQUIRED despite passing a pipeline cache");
					return tcu::TestStatus::pass("VK_PIPELINE_COMPILE_REQUIRED"); // ;_;
				}
				VK_CHECK(creationResult);

				if (reqCacheMiss)
					TCU_FAIL("Cache miss expected");

				Move<VkPipeline> pipelinePtr(check<VkPipeline>(pipeline), Deleter<VkPipeline>(vkd, device, nullptr));
				pipelinePtrs.emplace_back(pipelinePtr);
				pipelines.push_back(pipeline);

				if (needsCapture)
					identifierExeProps = getPipelineExecutableProperties(vkd, device, pipelines.back(), m_params->capturedProperties);
			}
		}
	}
	else if (isRT)
	{
		// Get some ray tracing properties and constants.
		const auto rayTracingPropertiesKHR	= makeRayTracingProperties(vki, physicalDevice);
		const auto shaderGroupHandleSize	= rayTracingPropertiesKHR->getShaderGroupHandleSize();
		const auto shaderGroupBaseAlignment	= rayTracingPropertiesKHR->getShaderGroupBaseAlignment();
		const auto vec3Size					= static_cast<uint32_t>(sizeof(tcu::Vec3));

		// Empty pipeline vector, needed in a couple places.
		const std::vector<VkPipeline> emptyPipelinesVec;

		auto shaderConstIt = shaderConstants.begin();

		// In case we have to run a pipeline.
		PipelineStageInfo rgenToRun;
		PipelineStageInfo chitToRun;
		PipelineStageInfo ahitToRun;
		PipelineStageInfo isecToRun;
		PipelineStageInfo missToRun;
		PipelineStageInfo callToRun;

		for (uint32_t i = 0; i < pipelineCount32; ++i)
		{
			const auto runThis	= (runOnePipeline && static_cast<uint32_t>(m_params->pipelineToRun.get()) == i);
			const auto suffix	= "_" + std::to_string(i);
			const auto rgenName	= "rgen" + suffix;
			const auto chitName	= "chit" + suffix;
			const auto ahitName	= "ahit" + suffix;
			const auto isecName	= "isec" + suffix;
			const auto missName	= "miss" + suffix;
			const auto callName	= "call" + suffix;

			ShaderWrapper	rgenModule;
			ShaderWrapper	chitModule;
			ShaderWrapper	ahitModule;
			ShaderWrapper	isecModule;
			ShaderWrapper	missModule;
			ShaderWrapper	callModule;

			SpecInfoPtr		rgenSpecInfo;
			SpecInfoPtr		chitSpecInfo;
			SpecInfoPtr		ahitSpecInfo;
			SpecInfoPtr		isecSpecInfo;
			SpecInfoPtr		missSpecInfo;
			SpecInfoPtr		callSpecInfo;

			uint32_t				groupCount	= 1u;
			const uint32_t			rgenGroup	= 0u;
			tcu::Maybe<uint32_t>	xhitGroup;
			tcu::Maybe<uint32_t>	missGroup;
			tcu::Maybe<uint32_t>	callGroup;

			rgenModules		.push_back(ShaderWrapper(vkd, device, binaries.get(rgenName)));
			rgenModule		= rgenModules.back();
			rgenSpecInfo	= maybeMakeSpecializationInfo(useSCs, &scMapEntry, shaderConstIt);

			if (binaries.contains(chitName))
			{
				chitModules		.push_back(ShaderWrapper(vkd, device, binaries.get(chitName)));
				chitModule		= chitModules.back();
				chitSpecInfo	= maybeMakeSpecializationInfo(useSCs, &scMapEntry, shaderConstIt);
				xhitGroup		= (static_cast<bool>(xhitGroup) ? xhitGroup : tcu::just(groupCount++));
			}

			if (binaries.contains(ahitName))
			{
				ahitModules		.push_back(ShaderWrapper(vkd, device, binaries.get(ahitName)));
				ahitModule		= ahitModules.back();
				ahitSpecInfo	= maybeMakeSpecializationInfo(useSCs, &scMapEntry, shaderConstIt);
				xhitGroup		= (static_cast<bool>(xhitGroup) ? xhitGroup : tcu::just(groupCount++));
			}

			if (binaries.contains(isecName))
			{
				isecModules		.push_back(ShaderWrapper(vkd, device, binaries.get(isecName)));
				isecModule		= isecModules.back();
				isecSpecInfo	= maybeMakeSpecializationInfo(useSCs, &scMapEntry, shaderConstIt);
				xhitGroup		= (static_cast<bool>(xhitGroup) ? xhitGroup : tcu::just(groupCount++));
			}

			if (binaries.contains(missName))
			{
				missModules		.push_back(ShaderWrapper(vkd, device, binaries.get(missName)));
				missModule		= missModules.back();
				missSpecInfo	= maybeMakeSpecializationInfo(useSCs, &scMapEntry, shaderConstIt);
				missGroup		= tcu::just(groupCount++);
			}

			if (binaries.contains(callName))
			{
				callModules		.push_back(ShaderWrapper(vkd, device, binaries.get(callName)));
				callModule		= callModules.back();
				callSpecInfo	= maybeMakeSpecializationInfo(useSCs, &scMapEntry, shaderConstIt);
				callGroup		= tcu::just(groupCount++);
			}

			{
				const auto rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();

				// These have to match the shaders.
				rayTracingPipeline->setMaxPayloadSize(vec3Size);
				rayTracingPipeline->setMaxAttributeSize(vec3Size);

				// Make it a library if we are using libraries.
				rayTracingPipeline->setCreateFlags(captureFlags | (m_params->useRTLibraries ? VK_PIPELINE_CREATE_LIBRARY_BIT_KHR : 0));

				rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, rgenModule.getModule(), rgenGroup, rgenSpecInfo.get());

				if (chitModule.isSet())
					rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, chitModule.getModule(), xhitGroup.get(), chitSpecInfo.get());

				if (ahitModule.isSet())
					rayTracingPipeline->addShader(VK_SHADER_STAGE_ANY_HIT_BIT_KHR, ahitModule.getModule(), xhitGroup.get(), ahitSpecInfo.get());

				if (isecModule.isSet())
					rayTracingPipeline->addShader(VK_SHADER_STAGE_INTERSECTION_BIT_KHR, isecModule.getModule(), xhitGroup.get(), isecSpecInfo.get());

				if (missModule.isSet())
					rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR, missModule.getModule(), missGroup.get(), missSpecInfo.get());

				if (callModule.isSet())
					rayTracingPipeline->addShader(VK_SHADER_STAGE_CALLABLE_BIT_KHR, callModule.getModule(), callGroup.get(), callSpecInfo.get());

				pipelinePtrs.emplace_back(rayTracingPipeline->createPipeline(vkd, device, pipelineLayout.get(), emptyPipelinesVec, pipelineCache.get()));
				pipelines.push_back(pipelinePtrs.back().get());

				// We may need to link the pipeline just like we'll do with shader module identifiers below.
				if (m_params->useRTLibraries)
				{
					const auto linkedPipeline = de::newMovePtr<RayTracingPipeline>();

					linkedPipeline->setMaxPayloadSize(vec3Size);
					linkedPipeline->setMaxAttributeSize(vec3Size);
					linkedPipeline->setCreateFlags(captureFlags);

					const std::vector<VkPipeline> rawPipelines(1u, pipelines.back());
					pipelinePtrs.emplace_back(linkedPipeline->createPipeline(vkd, device, pipelineLayout.get(), rawPipelines, pipelineCache.get()));
					pipelines.push_back(pipelinePtrs.back().get());
				}

				if (needsCapture)
					classicExeProps = getPipelineExecutableProperties(vkd, device, pipelines.back(), m_params->capturedProperties);
			}

			if (runThis)
			{
				rgenToRun.setModule(vkd, device, rgenModule, m_params->moduleUseCase, m_params->getRndGen());
				rgenToRun.setSpecInfo(std::move(rgenSpecInfo));

				if (chitModule.isSet())
				{
					chitToRun.setModule(vkd, device, chitModule, m_params->moduleUseCase, m_params->getRndGen());
					chitToRun.setSpecInfo(std::move(chitSpecInfo));
				}

				if (ahitModule.isSet())
				{
					ahitToRun.setModule(vkd, device, ahitModule, m_params->moduleUseCase, m_params->getRndGen());
					ahitToRun.setSpecInfo(std::move(ahitSpecInfo));
				}

				if (isecModule.isSet())
				{
					isecToRun.setModule(vkd, device, isecModule, m_params->moduleUseCase, m_params->getRndGen());
					isecToRun.setSpecInfo(std::move(isecSpecInfo));
				}

				if (missModule.isSet())
				{
					missToRun.setModule(vkd, device, missModule, m_params->moduleUseCase, m_params->getRndGen());
					missToRun.setSpecInfo(std::move(missSpecInfo));
				}

				if (callModule.isSet())
				{
					callToRun.setModule(vkd, device, callModule, m_params->moduleUseCase, m_params->getRndGen());
					callToRun.setSpecInfo(std::move(callSpecInfo));
				}
			}
		}

		if (runOnePipeline)
		{
			uint32_t				groupCount	= 1u;
			const uint32_t			rgenGroup	= 0u;
			tcu::Maybe<uint32_t>	xhitGroup;
			tcu::Maybe<uint32_t>	missGroup;
			tcu::Maybe<uint32_t>	callGroup;

			const auto rgenModule = rgenToRun.getModule(); DE_UNREF(rgenModule);
			const auto chitModule = chitToRun.getModule();
			const auto ahitModule = ahitToRun.getModule();
			const auto isecModule = isecToRun.getModule();
			const auto missModule = missToRun.getModule();
			const auto callModule = callToRun.getModule();

			if (chitModule.isSet())
				xhitGroup = (xhitGroup ? xhitGroup : tcu::just(groupCount++));
			if (ahitModule.isSet())
				xhitGroup = (xhitGroup ? xhitGroup : tcu::just(groupCount++));
			if (isecModule.isSet())
				xhitGroup = (xhitGroup ? xhitGroup : tcu::just(groupCount++));

			if (missModule.isSet())
				missGroup = tcu::just(groupCount++);

			if (callModule.isSet())
				callGroup = tcu::just(groupCount++);

			const auto shaderOwningPipelinePtr	= makeVkSharedPtr(de::newMovePtr<RayTracingPipeline>());
			const auto shaderOwningPipeline		= shaderOwningPipelinePtr->get();

			de::SharedPtr<de::MovePtr<RayTracingPipeline>>	auxiliaryPipelinePtr;
			RayTracingPipeline*								auxiliaryPipeline		= nullptr;

			if (m_params->useRTLibraries)
			{
				// The shader-owning pipeline will be a library and auxiliaryPipeline will be the bound pipeline helper.
				auxiliaryPipelinePtr	= makeVkSharedPtr(de::newMovePtr<RayTracingPipeline>());
				auxiliaryPipeline		= auxiliaryPipelinePtr->get();
			}

			// The bound pipeline is the shader-owning pipeline if not using libraries, or the auxiliary pipeline otherwise.
			RayTracingPipeline* boundPipeline = (m_params->useRTLibraries ? auxiliaryPipeline : shaderOwningPipeline);

			shaderOwningPipeline->setMaxPayloadSize(vec3Size);
			shaderOwningPipeline->setMaxAttributeSize(vec3Size);
			{
				VkPipelineCreateFlags creationFlags = (VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT_EXT | captureFlags);
				if (m_params->useRTLibraries)
					creationFlags |= VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;
				shaderOwningPipeline->setCreateFlags(creationFlags);
			}

			shaderOwningPipeline->addShader(
				VK_SHADER_STAGE_RAYGEN_BIT_KHR,
				rgenToRun.getUsedModule(m_params->moduleUseCase)->getModule(),
				rgenGroup,
				rgenToRun.getSpecInfo(), 0,
				rgenToRun.getModuleIdCreateInfo());

			if (chitModule.isSet())
			{
				shaderOwningPipeline->addShader(
					VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
					chitToRun.getUsedModule(m_params->moduleUseCase)->getModule(),
					xhitGroup.get(),
					chitToRun.getSpecInfo(), 0,
					chitToRun.getModuleIdCreateInfo());
			}

			if (ahitModule.isSet())
			{
				shaderOwningPipeline->addShader(
					VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
					ahitToRun.getUsedModule(m_params->moduleUseCase)->getModule(),
					xhitGroup.get(),
					ahitToRun.getSpecInfo(), 0,
					ahitToRun.getModuleIdCreateInfo());
			}

			if (isecModule.isSet())
			{
				shaderOwningPipeline->addShader(
					VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
					isecToRun.getUsedModule(m_params->moduleUseCase)->getModule(),
					xhitGroup.get(),
					isecToRun.getSpecInfo(), 0,
					isecToRun.getModuleIdCreateInfo());
			}

			if (missModule.isSet())
			{
				shaderOwningPipeline->addShader(
					VK_SHADER_STAGE_MISS_BIT_KHR,
					missToRun.getUsedModule(m_params->moduleUseCase)->getModule(),
					missGroup.get(),
					missToRun.getSpecInfo(), 0,
					missToRun.getModuleIdCreateInfo());
			}

			if (callModule.isSet())
			{
				shaderOwningPipeline->addShader(
					VK_SHADER_STAGE_CALLABLE_BIT_KHR,
					callToRun.getUsedModule(m_params->moduleUseCase)->getModule(),
					callGroup.get(),
					callToRun.getSpecInfo(), 0,
					callToRun.getModuleIdCreateInfo());
			}

			// Append the pipeline, SBTs and regions to use at the end of their vectors.
			try
			{
				pipelinePtrs.emplace_back(shaderOwningPipeline->createPipeline(vkd, device, pipelineLayout.get(), emptyPipelinesVec, pipelineCache.get()));
				pipelines.push_back(pipelinePtrs.back().get());
			}
			catch (const RayTracingPipeline::CompileRequiredError& err)
			{
				if (reqCacheMiss)
					return tcu::TestStatus::pass("Pass");

				if (qualityWarn)
					return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "VK_PIPELINE_COMPILE_REQUIRED despite passing a pipeline cache");
				return tcu::TestStatus::pass("VK_PIPELINE_COMPILE_REQUIRED"); // ;_;
			}

			if (m_params->useRTLibraries)
			{
				// Create a new pipeline using the library created above, and use it as the active pipeline.
				auxiliaryPipeline->setMaxPayloadSize(vec3Size);
				auxiliaryPipeline->setMaxAttributeSize(vec3Size);
				auxiliaryPipeline->setCreateFlags(VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT_EXT | captureFlags);

				try
				{
					const std::vector<VkPipeline> rawPipelines(1u, pipelines.back());
					pipelinePtrs.emplace_back(auxiliaryPipeline->createPipeline(vkd, device, pipelineLayout.get(), rawPipelines, pipelineCache.get()));
					pipelines.push_back(pipelinePtrs.back().get());

					if (reqCacheMiss)
						TCU_FAIL("Cache miss expected");
				}
				catch (const RayTracingPipeline::CompileRequiredError& err)
				{
					if (reqCacheMiss)
						return tcu::TestStatus::pass("Pass");

					if (qualityWarn)
						return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "VK_PIPELINE_COMPILE_REQUIRED on library use despite passing a pipeline cache");
					return tcu::TestStatus::pass("VK_PIPELINE_COMPILE_REQUIRED on library use"); // ;_;
				}
			}
			else if (reqCacheMiss)
				TCU_FAIL("Cache miss expected");

			if (needsCapture)
				identifierExeProps = getPipelineExecutableProperties(vkd, device, pipelines.back(), m_params->capturedProperties);

			const auto pipeline = pipelines.back();

			rgenSBT		= boundPipeline->createShaderBindingTable(vkd, device, pipeline, alloc, shaderGroupHandleSize, shaderGroupBaseAlignment, rgenGroup, 1u);
			rgenRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, rgenSBT->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);

			if (xhitGroup)
			{
				xhitSBT		= boundPipeline->createShaderBindingTable(vkd, device, pipeline, alloc, shaderGroupHandleSize, shaderGroupBaseAlignment, xhitGroup.get(), 1u);
				xhitRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, xhitSBT->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
			}

			if (missGroup)
			{
				missSBT		= boundPipeline->createShaderBindingTable(vkd, device, pipeline, alloc, shaderGroupHandleSize, shaderGroupBaseAlignment, missGroup.get(), 1u);
				missRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, missSBT->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
			}

			if (callGroup)
			{
				callSBT		= boundPipeline->createShaderBindingTable(vkd, device, pipeline, alloc, shaderGroupHandleSize, shaderGroupBaseAlignment, callGroup.get(), 1u);
				callRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, callSBT->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
			}
		}
	}
	else
	{
		DE_ASSERT(false);
	}

	// Early exit if we don't need to run any pipeline.
	if (!runOnePipeline)
		return tcu::TestStatus::pass("Pass (not using any pipeline)");

	// Compare executable properties if captured.
	if (needsCapture)
	{
		using PipelineExecutablePropertySet = std::set<PipelineExecutableProperty>;

		const PipelineExecutablePropertySet classicProps	(begin(classicExeProps), end(classicExeProps));
		const PipelineExecutablePropertySet identifierProps	(begin(identifierExeProps), end(identifierExeProps));

		if (classicProps != identifierProps)
		{
			auto& log = m_context.getTestContext().getLog();

			log << tcu::TestLog::Message << "Properties without identifiers: " << classicExeProps << tcu::TestLog::EndMessage;
			log << tcu::TestLog::Message << "Properties with    identifiers: " << identifierExeProps << tcu::TestLog::EndMessage;

			TCU_FAIL("Pipeline executable properties differ (check log for details)");
		}
	}

	if (isGraphics)
	{
		const auto		bindPoint	= VK_PIPELINE_BIND_POINT_GRAPHICS;
		const auto		vertexCount	= (m_params->hasTess() ? 3u : 1u);

		renderPass.begin(vkd, cmdBuffer, scissors.at(0u), clearColor);
		vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, pipelineLayout.get(), 0u, de::sizeU32(rawDescriptorSets), de::dataOrNull(rawDescriptorSets), 0u, nullptr);
		vkd.cmdBindPipeline(cmdBuffer, bindPoint, pipelines.back());
		vkd.cmdDraw(cmdBuffer, vertexCount, 1u, 0u, 0u);
		renderPass.end(vkd, cmdBuffer);

		const auto copyRegion			= makeBufferImageCopy(fbExtent, colorSRL);
		const auto preHostBarrier		= makeMemoryBarrier((VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT), VK_ACCESS_HOST_READ_BIT);
		const auto postRenderBarrier	= makeImageMemoryBarrier(
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			colorAtt->get(), colorSRR);

		// Copy color attachment to verification buffer.
		cmdPipelineImageMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &postRenderBarrier);
		vkd.cmdCopyImageToBuffer(cmdBuffer, colorAtt->get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, verifBuffer->get(), 1u, &copyRegion);

		// Synchronize SSBO and verification buffer reads from the host.
		cmdPipelineMemoryBarrier(vkd, cmdBuffer, (VK_PIPELINE_STAGE_TRANSFER_BIT | pipelineStages), VK_PIPELINE_STAGE_HOST_BIT, &preHostBarrier);
	}
	else if (isCompute)
	{
		const auto	bindPoint		= VK_PIPELINE_BIND_POINT_COMPUTE;
		const auto	preHostBarrier	= makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);

		vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, pipelineLayout.get(), 0u, de::sizeU32(rawDescriptorSets), de::dataOrNull(rawDescriptorSets), 0u, nullptr);
		vkd.cmdBindPipeline(cmdBuffer, bindPoint, pipelines.back());
		vkd.cmdDispatch(cmdBuffer, 1u, 1u, 1u);
		cmdPipelineMemoryBarrier(vkd, cmdBuffer, pipelineStages, VK_PIPELINE_STAGE_HOST_BIT, &preHostBarrier);
	}
	else if (isRT)
	{
		const auto	bindPoint		= VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;
		const auto	preHostBarrier	= makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
		const auto	rayCount		= (hasHitAndMiss ? 2u : 1u);

		vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, pipelineLayout.get(), 0u, de::sizeU32(rawDescriptorSets), de::dataOrNull(rawDescriptorSets), 0u, nullptr);
		vkd.cmdBindPipeline(cmdBuffer, bindPoint, pipelines.back());
		vkd.cmdTraceRaysKHR(cmdBuffer, &rgenRegion, &missRegion, &xhitRegion, &callRegion, rayCount, 1u, 1u);
		cmdPipelineMemoryBarrier(vkd, cmdBuffer, pipelineStages, VK_PIPELINE_STAGE_HOST_BIT, &preHostBarrier);
	}
	else
	{
		DE_ASSERT(false);
	}

	// Finish and submit command buffer.
	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Verify framebuffer if used.
	if (isGraphics)
	{
		auto& verifBufferAlloc	= verifBuffer->getAllocation();
		void* verifBufferData	= verifBufferAlloc.getHostPtr();

		invalidateAlloc(vkd, device, verifBufferAlloc);

		tcu::ConstPixelBufferAccess	resultAccess	(tcuFbFormat, iExtent, verifBufferData);
		const tcu::Vec4				expectedColor	= (m_params->hasFrag() ? blueColor : clearColor);
		const auto					resultColor		= resultAccess.getPixel(0, 0);

		if (resultColor != expectedColor)
		{
			std::ostringstream msg;
			msg << "Unexpected color found in Framebuffer: expected " << expectedColor << " but found " << resultColor;
			TCU_FAIL(msg.str());
		}
	}

	// Verify SSBO data.
	{
		invalidateAlloc(vkd, device, storageBufferAlloc);
		std::vector<uint32_t> outputData(stagesCount, 0u);
		deMemcpy(outputData.data(), storageBufferData, de::dataSize(outputData));

		for (size_t stageIdx = 0u; stageIdx < stagesCount; ++stageIdx)
		{
			const auto& expected	= shaderConstants.at(getShaderIdx(m_params->pipelineToRun.get(), stageIdx, stagesCount));
			const auto& result		= outputData.at(stageIdx);

			if (expected != result)
			{
				std::ostringstream msg;
				msg << "Unexpected data found for stage " << stageIdx << std::hex << ": expected 0x" << expected << " but found 0x" << result;
				TCU_FAIL(msg.str());
			}
		}
	}

	return tcu::TestStatus::pass("Pass");
}

enum class Winding
{
	CW = 0,
	CCW,
};

enum class Partitioning
{
	INTEGER = 0,
	FRACTIONAL_ODD,
};

std::ostream& operator<<(std::ostream& out, Winding w)
{
	return (out << ((w == Winding::CW) ? "triangle_cw" : "triangle_ccw"));
}

std::ostream& operator<<(std::ostream& out, Partitioning p)
{
	return (out << ((p == Partitioning::INTEGER) ? "integer" : "fractional_odd"));
}

class HLSLTessellationInstance : public vkt::TestInstance
{
public:
						HLSLTessellationInstance	(Context& context, PipelineConstructionType constructionType)
							: vkt::TestInstance		(context)
							, m_constructionType	(constructionType)
							{}
	virtual				~HLSLTessellationInstance	(void) {}

	tcu::TestStatus		iterate						(void) override;

protected:
	const PipelineConstructionType m_constructionType;
};

class HLSLTessellationCase : public vkt::TestCase
{
public:
					HLSLTessellationCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, PipelineConstructionType constructionType)
						: vkt::TestCase			(testCtx, name, description)
						, m_constructionType	(constructionType)
						{}
	virtual			~HLSLTessellationCase	(void) {}

	void			checkSupport			(Context& context) const override;
	void			initPrograms			(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance			(Context& context) const override { return new HLSLTessellationInstance(context, m_constructionType); }

	static std::vector<tcu::Vec4> getOutputColors (void);

protected:
	const PipelineConstructionType m_constructionType;
};

std::vector<tcu::Vec4> HLSLTessellationCase::getOutputColors (void)
{
	std::vector<tcu::Vec4> outColors
	{
		tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f),
		tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f),
		tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f),
		tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f),
	};

	return outColors;
}

void HLSLTessellationCase::checkSupport (Context &context) const
{
	const auto&	vki				= context.getInstanceInterface();
	const auto	physicalDevice	= context.getPhysicalDevice();

	checkPipelineConstructionRequirements(vki, physicalDevice, m_constructionType);
	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_TESSELLATION_SHADER);
	checkShaderModuleIdentifierSupport(context);
}

void HLSLTessellationCase::initPrograms (vk::SourceCollections &programCollection) const
{
	// Vertex shader.
	{
		// Full-screen triangle.
		std::ostringstream vert;
		vert
			<< "#version 450\n"
			<< "out gl_PerVertex\n"
			<< "{\n"
			<< "    vec4 gl_Position;\n"
			<< "};\n"
			<< "vec2 vertexPositions[3] = vec2[](\n"
			<< "    vec2(-1.0, -1.0),\n"
			<< "    vec2( 3.0, -1.0),\n"
			<< "    vec2(-1.0,  3.0)\n"
			<< ");\n"
			<< "void main (void) {\n"
			<< "    gl_Position = vec4(vertexPositions[gl_VertexIndex], 0.0, 1.0);\n"
			<< "}\n"
			;

		programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());
	}

	// Fragment shader, which outputs the color from the previous stages.
	{
		std::ostringstream frag;
		frag
			<< "#version 450\n"
			<< "layout (location=0) in vec4 inColor;\n"
			<< "layout (location=0) out vec4 outColor;\n"
			<< "void main (void) {\n"
			<< "    outColor = inColor;\n"
			<< "}\n"
			;

		programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
	}

	// Tessellation evaluation shader (AKA domain shader) in HLSL, common for every pipeline.
	// Contrary to GLSL, HLSL allows us to omit execution modes in the "tese" shader and specify them on the "tesc" shader.
	{
		std::ostringstream tese;
		tese
			<< "struct HullShaderOutput\n"
			<< "{\n"
			<< "    float4 Position : SV_Position;\n"
			<< "    [[vk::location(0)]] float4 Color : COLOR0;\n"
			<< "};\n"
			<< "\n"
			<< "struct HullShaderConstantOutput\n"
			<< "{\n"
			<< "    float TessLevelOuter[4] : SV_TessFactor;\n"
			<< "    float TessLevelInner[2] : SV_InsideTessFactor;\n"
			<< "};\n"
			<< "\n"
			<< "struct DomainShaderOutput\n"
			<< "{\n"
			<< "    float4 Position : SV_Position;\n"
			<< "    [[vk::location(0)]] float4 Color : COLOR0;\n"
			<< "};\n"
			<< "\n"
			<< "DomainShaderOutput main (HullShaderConstantOutput input, float3 TessCoord : SV_DomainLocation, const OutputPatch<HullShaderOutput, 3> patch)\n"
			<< "{\n"
			<< "    DomainShaderOutput output = (DomainShaderOutput)0;\n"
			<< "\n"
			<< "    output.Position = (TessCoord.x * patch[0].Position) +\n"
			<< "                      (TessCoord.y * patch[1].Position) +\n"
			<< "                      (TessCoord.z * patch[2].Position);\n"
			<< "\n"
			<< "    output.Color = (TessCoord.x * patch[0].Color) +\n"
			<< "                   (TessCoord.y * patch[1].Color) +\n"
			<< "                   (TessCoord.z * patch[2].Color);\n"
			<< "\n"
			<< "    return output;\n"
			<< "}\n"
			;

		programCollection.hlslSources.add("tese") << glu::TessellationEvaluationSource(tese.str());
	}

	// Tessellation control shaders. Create 4 combinations with different execution modes. Each combination will also assign a different color to the vertices.
	// We will later run each pipeline to draw a pixel in a framebuffer (using viewports and scissors) to end up with 4 distinct colors.
	{
		const auto	outColors	= getOutputColors();
		size_t		colorIdx	= 0;

		const Winding		windings[]		= { Winding::CW, Winding::CCW };
		const Partitioning	partitionings[]	= { Partitioning::INTEGER, Partitioning::FRACTIONAL_ODD };

		for (const auto& winding : windings)
			for (const auto& partitioning : partitionings)
			{
				std::ostringstream tesc;
				tesc
					<< "struct VertexShaderOutput\n"
					<< "{\n"
					<< "    float4 Position : SV_Position;\n"
					<< "};\n"
					<< "\n"
					<< "struct HullShaderOutput\n"
					<< "{\n"
					<< "    float4 Position : SV_Position;\n"
					<< "    [[vk::location(0)]] float4 Color : COLOR0;\n"
					<< "};\n"
					<< "\n"
					<< "struct HullShaderConstantOutput\n"
					<< "{\n"
					<< "    float TessLevelOuter[4] : SV_TessFactor;\n"
					<< "    float TessLevelInner[2] : SV_InsideTessFactor;\n"
					<< "};\n"
					<< "\n"
					<< "[domain(\"tri\")]\n"
					<< "[partitioning(\"" << partitioning << "\")]\n"
					<< "[outputtopology(\"" << winding << "\")]\n"
					<< "[outputcontrolpoints(3)]\n"
					<< "[patchconstantfunc(\"PCF\")]\n"
					<< "HullShaderOutput main (InputPatch<VertexShaderOutput, 3> patch, uint InvocationID : SV_OutputControlPointID)\n"
					<< "{\n"
					<< "    HullShaderOutput output = (HullShaderOutput)0;\n"
					<< "    output.Position = patch[InvocationID].Position;\n"
					<< "    output.Color = float4" << outColors.at(colorIdx) << ";\n"
					<< "    return output;\n"
					<< "}\n"
					<< "\n"
					<< "HullShaderConstantOutput PCF (InputPatch<VertexShaderOutput, 3> patch, uint InvocationID : SV_PrimitiveID)\n"
					<< "{\n"
					<< "    HullShaderConstantOutput output = (HullShaderConstantOutput)0;\n"
					<< "\n"
					<< "    output.TessLevelOuter[0] = 1;\n"
					<< "    output.TessLevelOuter[1] = 1;\n"
					<< "    output.TessLevelOuter[2] = 1;\n"
					<< "    output.TessLevelOuter[3] = 1;\n"
					<< "\n"
					<< "    output.TessLevelInner[0] = 1;\n"
					<< "    output.TessLevelInner[1] = 1;\n"
					<< "\n"
					<< "    return output;\n"
					<< "}\n"
					;

				const auto idxStr = std::to_string(colorIdx);
				programCollection.hlslSources.add("tesc" + idxStr) << glu::TessellationControlSource(tesc.str());

				++colorIdx;
			}
	}
}

tcu::TestStatus HLSLTessellationInstance::iterate (void)
{
	const auto&			vki				= m_context.getInstanceInterface();
	const auto&			vkd				= m_context.getDeviceInterface();
	const auto			physicalDevice	= m_context.getPhysicalDevice();
	const auto			device			= m_context.getDevice();
	auto&				alloc			= m_context.getDefaultAllocator();
	const auto			queue			= m_context.getUniversalQueue();
	const auto			queueIndex		= m_context.getUniversalQueueFamilyIndex();

	const auto			fbFormat		= VK_FORMAT_R8G8B8A8_UNORM;
	const auto			fbExtent		= makeExtent3D(2u, 2u, 1u);
	const tcu::IVec3	iExtent			(static_cast<int>(fbExtent.width), static_cast<int>(fbExtent.height), static_cast<int>(fbExtent.depth));
	const auto			tcuFbFormat		= mapVkFormat(fbFormat);
	const auto			pixelSize		= tcu::getPixelSize(tcuFbFormat);
	const auto			topology		= VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
	const auto			patchCPs		= 3u;
	const tcu::Vec4		clearColor		(0.0f, 0.0f, 0.0f, 1.0f);
	const auto			bindPoint		= VK_PIPELINE_BIND_POINT_GRAPHICS;

	const std::vector<VkViewport>	rpViewports	(1u, makeViewport(fbExtent));
	const std::vector<VkRect2D>		rpScissors	(1u, makeRect2D(fbExtent));

	// Color attachment.
	const VkImageCreateInfo colorAttCreateInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,										//	VkStructureType			sType;
		nullptr,																	//	const void*				pNext;
		0u,																			//	VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,															//	VkImageType				imageType;
		fbFormat,																	//	VkFormat				format;
		fbExtent,																	//	VkExtent3D				extent;
		1u,																			//	uint32_t				mipLevels;
		1u,																			//	uint32_t				arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,														//	VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,													//	VkImageTiling			tiling;
		(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT),	//	VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,													//	VkSharingMode			sharingMode;
		0u,																			//	uint32_t				queueFamilyIndexCount;
		nullptr,																	//	const uint32_t*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,													//	VkImageLayout			initialLayout;
	};

	ImageWithMemory		colorAtt		(vkd, device, alloc, colorAttCreateInfo, MemoryRequirement::Any);
	const auto			colorSRR		= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const auto			colorSRL		= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
	const auto			colorAttView	= makeImageView(vkd, device, colorAtt.get(), VK_IMAGE_VIEW_TYPE_2D, fbFormat, colorSRR);
	RenderPassWrapper	renderPass		(m_constructionType, vkd, device, fbFormat);
	renderPass.createFramebuffer(vkd, device, colorAtt.get(), colorAttView.get(), fbExtent.width, fbExtent.height);

	// Verification buffer.
	DE_ASSERT(fbExtent.depth == 1u);
	const auto			verifBufferSize	= static_cast<VkDeviceSize>(pixelSize) * fbExtent.width * fbExtent.height;
	const auto			verifBufferInfo	= makeBufferCreateInfo(verifBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	BufferWithMemory	verifBuffer		(vkd, device, alloc, verifBufferInfo, MemoryRequirement::HostVisible);

	// Create shader modules, obtain IDs and verify all of them differ.
	const auto&	binaries	= m_context.getBinaryCollection();
	const auto	vertModule	= ShaderWrapper(vkd, device, binaries.get("vert"));
	const auto	fragModule	= ShaderWrapper(vkd, device, binaries.get("frag"));
	const auto	teseModule	= ShaderWrapper(vkd, device, binaries.get("tese"));

	std::vector<ShaderWrapper>	tescModules;
	{
		size_t tescIdx = 0;

		for (;;)
		{
			const auto shaderName = "tesc" + std::to_string(tescIdx);
			if (!binaries.contains(shaderName))
				break;
			tescModules.emplace_back(ShaderWrapper(vkd, device, binaries.get(shaderName)));

			++tescIdx;
		}
	}

	const auto vertId = getShaderModuleIdentifier(vkd, device, vertModule.getModule());
	const auto fragId = getShaderModuleIdentifier(vkd, device, fragModule.getModule());
	const auto teseId = getShaderModuleIdentifier(vkd, device, teseModule.getModule());
	std::vector<ShaderModuleId> tescIds;
	for (const auto& mod : tescModules)
		tescIds.emplace_back(getShaderModuleIdentifier(vkd, device, mod.getModule()));

	// Verify all of them are unique.
	{
		std::vector<ShaderModuleId> allIds;
		allIds.emplace_back(vertId);
		allIds.emplace_back(fragId);
		allIds.emplace_back(teseId);
		for (const auto& id : tescIds)
			allIds.emplace_back(id);

		std::set<ShaderModuleId> uniqueIds (begin(allIds), end(allIds));

		if (allIds.size() != uniqueIds.size())
			TCU_FAIL("Not every module has a unique ID");
	}

	// Constant structures used when creating pipelines.
	const VkPipelineVertexInputStateCreateInfo		vertexInputState			= initVulkanStructure();
	const VkPipelineInputAssemblyStateCreateInfo	inputAssemblyState			=
	{
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	//	VkStructureType							sType;
		nullptr,														//	const void*								pNext;
		0u,																//	VkPipelineInputAssemblyStateCreateFlags	flags;
		topology,														//	VkPrimitiveTopology						topology;
		VK_FALSE,														//	VkBool32								primitiveRestartEnable;
	};
	const VkPipelineDepthStencilStateCreateInfo		depthStencilState			= initVulkanStructure();
	VkPipelineMultisampleStateCreateInfo			multisampleState			= initVulkanStructure();
	multisampleState.rasterizationSamples										= VK_SAMPLE_COUNT_1_BIT;
	VkPipelineColorBlendAttachmentState				colorBlendAttachmentState;
	deMemset(&colorBlendAttachmentState, 0, sizeof(colorBlendAttachmentState));
	colorBlendAttachmentState.colorWriteMask									= (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);
	const VkPipelineColorBlendStateCreateInfo		colorBlendState				=
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	//	VkStructureType								sType
		nullptr,													//	const void*									pNext
		0u,															//	VkPipelineColorBlendStateCreateFlags		flags
		VK_FALSE,													//	VkBool32									logicOpEnable
		VK_LOGIC_OP_CLEAR,											//	VkLogicOp									logicOp
		1u,															//	deUint32									attachmentCount
		&colorBlendAttachmentState,									//	const VkPipelineColorBlendAttachmentState*	pAttachments
		{ 0.0f, 0.0f, 0.0f, 0.0f }									//	float										blendConstants[4]
	};
	const auto										rasterizationState			= makeRasterizationState(false/*rasterizationDisabled*/);

	// Pipeline cache.
	const VkPipelineCacheCreateInfo cacheCreateInfo = initVulkanStructure();
	const auto pipelineCache = createPipelineCache(vkd, device, &cacheCreateInfo);

	// Empty pipeline layout.
	const PipelineLayoutWrapper pipelineLayout (m_constructionType, vkd, device);

	using GraphicsPipelineWrapperPtr = std::unique_ptr<GraphicsPipelineWrapper>;

	// Create temporary pipelines with them to prime the cache.
	{
		for (const auto& tescModule : tescModules)
		{
			GraphicsPipelineWrapperPtr wrapper (new GraphicsPipelineWrapper(vki, vkd, physicalDevice, device, m_context.getDeviceExtensions(), m_constructionType));

			try
			{
				wrapper->setDefaultPatchControlPoints(patchCPs)
						.setupVertexInputState(&vertexInputState, &inputAssemblyState, pipelineCache.get())
						.setupPreRasterizationShaderState2(
							rpViewports,
							rpScissors,
							pipelineLayout,
							renderPass.get(),
							0u,
							vertModule,
							&rasterizationState,
							tescModule,
							teseModule,
							ShaderWrapper(),
							nullptr,
							nullptr,
							nullptr,
							nullptr,
							nullptr,
							PipelineRenderingCreateInfoWrapper(),
							pipelineCache.get())
						.setupFragmentShaderState(
							pipelineLayout,
							renderPass.get(),
							0u,
							fragModule,
							&depthStencilState,
							&multisampleState,
							nullptr,
							pipelineCache.get())
						.setupFragmentOutputState(
							*renderPass,
							0u,
							&colorBlendState,
							&multisampleState,
							pipelineCache.get())
						.setMonolithicPipelineLayout(pipelineLayout)
						.buildPipeline(pipelineCache.get());
			}
			catch (const PipelineCompileRequiredError& err)
			{
				TCU_FAIL("PipelineCompileRequiredError received while priming pipeline cache");
			}
		}
	}

	// Create pipelines using shader module ids. These will actually be run. Note the changing viewports and scissors.
	std::vector<GraphicsPipelineWrapperPtr>	pipelineWrappers;
	std::vector<VkViewport>					viewports;
	std::vector<VkRect2D>					scissors;

	const auto vertIdInfo = makeShaderStageModuleIdentifierCreateInfo(vertId, UseModuleCase::ID);
	const auto fragIdInfo = makeShaderStageModuleIdentifierCreateInfo(fragId, UseModuleCase::ID);
	const auto teseIdInfo = makeShaderStageModuleIdentifierCreateInfo(teseId, UseModuleCase::ID);
	std::vector<ShaderStageIdPtr> tescIdInfos;
	for (const auto& tescId : tescIds)
		tescIdInfos.emplace_back(makeShaderStageModuleIdentifierCreateInfo(tescId, UseModuleCase::ID));

	for (size_t tescIdx = 0; tescIdx < tescModules.size(); ++tescIdx)
	{
		const auto	row	= tescIdx / fbExtent.width;
		const auto	col	= tescIdx % fbExtent.width;

		viewports.emplace_back(makeViewport(static_cast<float>(col), static_cast<float>(row), 1.0f, 1.0f, 0.0f, 1.0f));
		scissors.emplace_back(makeRect2D(static_cast<int32_t>(col), static_cast<int32_t>(row), 1u, 1u));
		pipelineWrappers.emplace_back(new GraphicsPipelineWrapper(vki, vkd, physicalDevice, device, m_context.getDeviceExtensions(), m_constructionType));

		const auto& wrapper = pipelineWrappers.back();

		try
		{
			wrapper->setDefaultPatchControlPoints(patchCPs)
					.setupVertexInputState(&vertexInputState, &inputAssemblyState, pipelineCache.get())
					.setupPreRasterizationShaderState3(
						std::vector<VkViewport>(1u, viewports.back()),
						std::vector<VkRect2D>(1u, scissors.back()),
						pipelineLayout,
						renderPass.get(),
						0u,
						ShaderWrapper(),
						PipelineShaderStageModuleIdentifierCreateInfoWrapper(vertIdInfo.get()),
						&rasterizationState,
						ShaderWrapper(),
						PipelineShaderStageModuleIdentifierCreateInfoWrapper(tescIdInfos.at(tescIdx).get()),
						ShaderWrapper(),
						PipelineShaderStageModuleIdentifierCreateInfoWrapper(teseIdInfo.get()),
						ShaderWrapper(),
						PipelineShaderStageModuleIdentifierCreateInfoWrapper(),
						nullptr,
						nullptr,
						nullptr,
						nullptr,
						nullptr,
						PipelineRenderingCreateInfoWrapper(),
						pipelineCache.get())
					.setupFragmentShaderState2(
						pipelineLayout,
						renderPass.get(),
						0u,
						ShaderWrapper(),
						PipelineShaderStageModuleIdentifierCreateInfoWrapper(fragIdInfo.get()),
						&depthStencilState,
						&multisampleState,
						nullptr,
						pipelineCache.get())
					.setupFragmentOutputState(
						*renderPass,
						0u,
						&colorBlendState,
						&multisampleState,
						pipelineCache.get())
					.setMonolithicPipelineLayout(pipelineLayout)
					.buildPipeline(pipelineCache.get());
		}
		catch (const PipelineCompileRequiredError& err)
		{
			return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "PipelineCompileRequiredError received despite using pipeline cache");
		}
	}

	// Use pipelines in a render pass.
	const auto cmdPool = makeCommandPool(vkd, device, queueIndex);
	const auto cmdBufferPtr = allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer = cmdBufferPtr.get();

	beginCommandBuffer(vkd, cmdBuffer);
	renderPass.begin(vkd, cmdBuffer, rpScissors.at(0u), clearColor);
	for (const auto& wrapper : pipelineWrappers)
	{
		vkd.cmdBindPipeline(cmdBuffer, bindPoint, wrapper->getPipeline());
		vkd.cmdDraw(cmdBuffer, 3u, 1u, 0u, 0u);
	}
	renderPass.end(vkd, cmdBuffer);

	// Transfer color attachment to verification buffer.
	const auto copyRegion			= makeBufferImageCopy(fbExtent, colorSRL);
	const auto preHostBarrier		= makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
	const auto postRenderBarrier	= makeImageMemoryBarrier(
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		colorAtt.get(), colorSRR);

	cmdPipelineImageMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &postRenderBarrier);
	vkd.cmdCopyImageToBuffer(cmdBuffer, colorAtt.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, verifBuffer.get(), 1u, &copyRegion);
	cmdPipelineMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, &preHostBarrier);

	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Verify result.
	{
		auto&		log					= m_context.getTestContext().getLog();
		const auto	outColors			= HLSLTessellationCase::getOutputColors();
		auto&		verifBufferAlloc	= verifBuffer.getAllocation();
		void*		verifBufferData		= verifBufferAlloc.getHostPtr();

		invalidateAlloc(vkd, device, verifBufferAlloc);

		tcu::ConstPixelBufferAccess	resultAccess	(tcuFbFormat, iExtent, verifBufferData);
		tcu::TextureLevel			referenceLevel	(tcuFbFormat, iExtent.x(), iExtent.y());
		const auto					referenceAccess	= referenceLevel.getAccess();
		const tcu::Vec4				threshold		(0.0f, 0.0f, 0.0f, 0.0f);

		for (int x = 0; x < iExtent.x(); ++x)
			for (int y = 0; y < iExtent.y(); ++y)
				referenceAccess.setPixel(outColors.at(y*iExtent.x() + x), x, y);

		tcu::floatThresholdCompare(log, "Result", "", referenceAccess, resultAccess, threshold, tcu::COMPARE_LOG_EVERYTHING);
	}

	return tcu::TestStatus::pass("Pass");
}

} // anonymous namespace

tcu::TestCaseGroup* createShaderModuleIdentifierTests (tcu::TestContext& testCtx, vk::PipelineConstructionType constructionType)
{
	// No pipelines are actually constructed in some of these variants, so adding them to a single group is fine.
	GroupPtr mainGroup (new tcu::TestCaseGroup(testCtx, "shader_module_identifier", "Tests for VK_EXT_shader_module_identifier"));

	if (constructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
	{
		// Property tests.
		GroupPtr propertiesGroup (new tcu::TestCaseGroup(testCtx, "properties", "Test shader module identifier extension properties"));

		addFunctionCase(propertiesGroup.get(), "constant_algorithm_uuid", "", checkShaderModuleIdentifierSupport, constantAlgorithmUUIDCase);

		mainGroup->addChild(propertiesGroup.release());
	}

	const struct
	{
		PipelineType		pipelineType;
		bool				useRTLibraries;
		const char*			name;
	} pipelineTypeCases[] =
	{
		{ PipelineType::COMPUTE,		false,	"compute"			},
		{ PipelineType::GRAPHICS,		false,	"graphics"			},
		{ PipelineType::RAY_TRACING,	false,	"ray_tracing"		},
		{ PipelineType::RAY_TRACING,	true,	"ray_tracing_libs"	},
	};

	const uint8_t pipelineCountCases[] = { uint8_t{1}, uint8_t{4} };

	const std::vector<GraphicsShaderVec> graphicsShadersCases
	{
		{ GraphicsShaderType::VERTEX },
		{ GraphicsShaderType::VERTEX, GraphicsShaderType::FRAG },
		{ GraphicsShaderType::VERTEX, GraphicsShaderType::TESS_CONTROL, GraphicsShaderType::TESS_EVAL, GraphicsShaderType::FRAG },
		{ GraphicsShaderType::VERTEX, GraphicsShaderType::GEOMETRY, GraphicsShaderType::FRAG },
		{ GraphicsShaderType::VERTEX, GraphicsShaderType::TESS_CONTROL, GraphicsShaderType::TESS_EVAL, GraphicsShaderType::GEOMETRY, GraphicsShaderType::FRAG },
	};

	const std::vector<RTShaderVec> rtShadersCases
	{
		{ RayTracingShaderType::RAY_GEN, RayTracingShaderType::MISS },
		{ RayTracingShaderType::RAY_GEN, RayTracingShaderType::CLOSEST_HIT, RayTracingShaderType::MISS },
		{ RayTracingShaderType::RAY_GEN, RayTracingShaderType::ANY_HIT, RayTracingShaderType::CLOSEST_HIT, RayTracingShaderType::MISS },
		{ RayTracingShaderType::RAY_GEN, RayTracingShaderType::INTERSECTION, RayTracingShaderType::ANY_HIT, RayTracingShaderType::CLOSEST_HIT, RayTracingShaderType::MISS },
		{ RayTracingShaderType::RAY_GEN, RayTracingShaderType::CALLABLE },
	};

	const struct
	{
		bool		useSCs;
		const char*	name;
	} useSCCases[] =
	{
		{ false,	"no_spec_constants"		},
		{ true,		"use_spec_constants"	},
	};

	// Tests checking the identifiers are constant.
	if (constructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
	{
		// Constant and unique module identifier tests.
		GroupPtr constantIdsGroup (new tcu::TestCaseGroup(testCtx, "constant_identifiers", "Test shader modules have constant and unique identifiers"));

		const struct
		{
			ConstantModuleIdentifiersInstance::APICall	apiCall;
			const char*									name;
		} apiCallCases[] =
		{
			{ ConstantModuleIdentifiersInstance::APICall::MODULE,		"module_id"			},
			{ ConstantModuleIdentifiersInstance::APICall::CREATE_INFO,	"create_info_id"	},
			{ ConstantModuleIdentifiersInstance::APICall::BOTH,			"both_ids"			},
		};

		const struct
		{
			bool			differentDevice;
			const char*		name;
		} differentDeviceCases[] =
		{
			{ false,	"same_device"		},
			{ true,		"different_devices"	},
		};

		for (const auto& pipelineTypeCase : pipelineTypeCases)
		{
			// Skip this case for constant module identifiers.
			if (pipelineTypeCase.useRTLibraries)
				continue;

			GroupPtr pipelineTypeGroup (new tcu::TestCaseGroup(testCtx, pipelineTypeCase.name, ""));

			for (const auto& pipelineCountCase : pipelineCountCases)
			{
				const auto countGroupName = std::to_string(static_cast<int>(pipelineCountCase)) + "_variants";

				GroupPtr pipelineCountGroup (new tcu::TestCaseGroup(testCtx, countGroupName.c_str(), ""));

				for (const auto& useSCCase : useSCCases)
				{
					GroupPtr useSCGroup (new tcu::TestCaseGroup(testCtx, useSCCase.name, ""));

					for (const auto& apiCallCase : apiCallCases)
					{
						GroupPtr apiCallGroup (new tcu::TestCaseGroup(testCtx, apiCallCase.name, ""));

						for (const auto& differentDeviceCase : differentDeviceCases)
						{
							GroupPtr differentDeviceGroup (new tcu::TestCaseGroup(testCtx, differentDeviceCase.name, ""));

							using Params = ConstantModuleIdentifiersInstance::Params;

							Params commonParams(
								pipelineTypeCase.pipelineType,
								{}, {}, pipelineCountCase, tcu::Nothing,
								useSCCase.useSCs, false, apiCallCase.apiCall, differentDeviceCase.differentDevice);

							if (pipelineTypeCase.pipelineType == PipelineType::GRAPHICS)
							{
								for (const auto& graphicsShadersCase : graphicsShadersCases)
								{
									std::unique_ptr<Params> params (new Params(commonParams));
									params->graphicsShaders = graphicsShadersCase;
									differentDeviceGroup->addChild(new ConstantModuleIdentifiersCase(testCtx, toString(graphicsShadersCase), "", std::move(params)));
								}
							}
							else if (pipelineTypeCase.pipelineType == PipelineType::RAY_TRACING)
							{
								for (const auto& rtShadersCase : rtShadersCases)
								{
									std::unique_ptr<Params> params (new Params(commonParams));
									params->rtShaders = rtShadersCase;
									differentDeviceGroup->addChild(new ConstantModuleIdentifiersCase(testCtx, toString(rtShadersCase), "", std::move(params)));
								}
							}
							else	// Compute
							{
								std::unique_ptr<Params> params (new Params(commonParams));
								differentDeviceGroup->addChild(new ConstantModuleIdentifiersCase(testCtx, "comp", "", std::move(params)));
							}

							apiCallGroup->addChild(differentDeviceGroup.release());
						}

						useSCGroup->addChild(apiCallGroup.release());
					}

					pipelineCountGroup->addChild(useSCGroup.release());
				}

				pipelineTypeGroup->addChild(pipelineCountGroup.release());
			}

			constantIdsGroup->addChild(pipelineTypeGroup.release());
		}

		mainGroup->addChild(constantIdsGroup.release());
	}

	// Tests creating pipelines using the module id extension structures.
	{
		const struct
		{
			bool		useVkPipelineCache;
			const char*	name;
		} pipelineCacheCases[] =
		{
			{ false,	"no_pipeline_cache"		},
			{ true,		"use_pipeline_cache"	},
		};

		const struct
		{
			UseModuleCase		moduleUse;
			const char*			name;
		} moduleUsageCases[] =
		{
			{ UseModuleCase::ID,						"use_id"					},
			{ UseModuleCase::ZERO_LEN_ID,				"zero_len_id"				},
			{ UseModuleCase::ZERO_LEN_ID_NULL_PTR,		"zero_len_id_null_ptr"		},
			{ UseModuleCase::ZERO_LEN_ID_GARBAGE_PTR,	"zero_len_id_garbage_ptr"	},
			{ UseModuleCase::ALL_ZEROS,					"all_zeros_id"				},
			{ UseModuleCase::ALL_ONES,					"all_ones_id"				},
			{ UseModuleCase::PSEUDORANDOM_ID,			"pseudorandom_id"			},
		};

		const struct
		{
			CapturedPropertiesBits		capturedProperties;
			const char*					name;
		} capturingCases[] =
		{
			{ CapturedPropertiesBits::NONE,				"no_exec_properties"		},
			{ CapturedPropertiesBits::STATS,			"capture_stats"				},
			{ CapturedPropertiesBits::IRS,				"capture_irs"				},
		};

		uint32_t rndSeed = 1651848014u;

		// Tests using pipelines created using shader identifiers.
		GroupPtr pipelineFromIdsGroup (new tcu::TestCaseGroup(testCtx, "pipeline_from_id", "Test creating and using pipelines from shader module identifiers"));

		for (const auto& pipelineTypeCase : pipelineTypeCases)
		{
			if (pipelineTypeCase.pipelineType != PipelineType::GRAPHICS && constructionType != PipelineConstructionType::PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
				continue;

			GroupPtr pipelineTypeGroup (new tcu::TestCaseGroup(testCtx, pipelineTypeCase.name, ""));

			for (const auto& pipelineCountCase : pipelineCountCases)
			{
				const auto countGroupName = std::to_string(static_cast<int>(pipelineCountCase)) + "_variants";

				GroupPtr pipelineCountGroup (new tcu::TestCaseGroup(testCtx, countGroupName.c_str(), ""));

				for (const auto& useSCCase : useSCCases)
				{
					GroupPtr useSCGroup (new tcu::TestCaseGroup(testCtx, useSCCase.name, ""));

					for (const auto& pipelineCacheCase : pipelineCacheCases)
					{
						GroupPtr pipelineCacheGroup (new tcu::TestCaseGroup(testCtx, pipelineCacheCase.name, ""));

						for (const auto& moduleUsageCase : moduleUsageCases)
						{
							GroupPtr moduleUsageGroup (new tcu::TestCaseGroup(testCtx, moduleUsageCase.name, ""));

							for (const auto& capturingCase : capturingCases)
							{
								// We are only going to attempt to capture properties in a specific subset of the tests.
								if (capturingCase.capturedProperties != CapturedPropertiesBits::NONE &&
									(pipelineCountCase > 1u || moduleUsageCase.moduleUse != UseModuleCase::ID))
									continue;

								GroupPtr captureGroup (new tcu::TestCaseGroup(testCtx, capturingCase.name, ""));

								DE_ASSERT(pipelineCountCase > 0u);
								const uint8_t pipelineToRun = (pipelineCountCase == 1u ? uint8_t{0} : static_cast<uint8_t>(pipelineCountCase - 2u));

								CreateAndUseIdsInstance::Params baseParams(
									pipelineTypeCase.pipelineType,
									{}, {}, pipelineCountCase, tcu::just(pipelineToRun),
									useSCCase.useSCs, pipelineCacheCase.useVkPipelineCache, false,
									constructionType, pipelineTypeCase.useRTLibraries,
									moduleUsageCase.moduleUse,
									static_cast<CapturedPropertiesFlags>(capturingCase.capturedProperties));

								if (pipelineTypeCase.pipelineType == PipelineType::GRAPHICS)
								{
									for (const auto& graphicsShadersCase : graphicsShadersCases)
									{
										BaseParamsPtr params = baseParams.copy(rndSeed++);
										params->graphicsShaders = graphicsShadersCase;
										captureGroup->addChild(new CreateAndUseIdsCase(testCtx, toString(graphicsShadersCase), "", std::move(params)));
									}
								}
								else if (pipelineTypeCase.pipelineType == PipelineType::RAY_TRACING)
								{
									for (const auto& rtShadersCase : rtShadersCases)
									{
										BaseParamsPtr params = baseParams.copy(rndSeed++);
										params->rtShaders = rtShadersCase;
										captureGroup->addChild(new CreateAndUseIdsCase(testCtx, toString(rtShadersCase), "", std::move(params)));
									}
								}
								else	// Compute
								{
									BaseParamsPtr params = baseParams.copy(rndSeed++);
									captureGroup->addChild(new CreateAndUseIdsCase(testCtx, "comp", "", std::move(params)));
								}

								moduleUsageGroup->addChild(captureGroup.release());
							}

							pipelineCacheGroup->addChild(moduleUsageGroup.release());
						}

						useSCGroup->addChild(pipelineCacheGroup.release());
					}

					pipelineCountGroup->addChild(useSCGroup.release());
				}

				pipelineTypeGroup->addChild(pipelineCountGroup.release());
			}

			pipelineFromIdsGroup->addChild(pipelineTypeGroup.release());
		}

		mainGroup->addChild(pipelineFromIdsGroup.release());
	}

	// HLSL tessellation test.
	{
		GroupPtr hlslTessGroup (new tcu::TestCaseGroup(testCtx, "hlsl_tessellation", "Tests checking HLSL tessellation shaders with module identifiers"));
		hlslTessGroup->addChild(new HLSLTessellationCase(testCtx, "test", "", constructionType));
		mainGroup->addChild(hlslTessGroup.release());
	}

	// misc tests
	if (constructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
	{
		const uint8_t pipelineToRun = 0u;
		CreateAndUseIdsInstance::Params baseParams(
			PipelineType::GRAPHICS,
			{}, {}, uint8_t{1u}, tcu::just(uint8_t{pipelineToRun}),
			false, false, true,
			constructionType, false,
			UseModuleCase::ID,
			static_cast<CapturedPropertiesFlags>(CapturedPropertiesBits::STATS));
		baseParams.graphicsShaders = graphicsShadersCases[1];

		GroupPtr miscGroup(new tcu::TestCaseGroup(testCtx, "misc", ""));

		BaseParamsPtr params = baseParams.copy(1);
		miscGroup->addChild(new CreateAndUseIdsCase(testCtx, "capture_statistics_maintenance5", "", std::move(params)));

		baseParams.capturedProperties = static_cast<CapturedPropertiesFlags>(CapturedPropertiesBits::IRS);
		params = baseParams.copy(2);
		miscGroup->addChild(new CreateAndUseIdsCase(testCtx, "capture_internal_representations_maintenance5", "", std::move(params)));

		mainGroup->addChild(miscGroup.release());
	}

	return mainGroup.release();
}

} // pipeline
} // vkt
