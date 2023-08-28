/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \brief Pipeline specialization constants tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineSpecConstantTests.hpp"
#include "vktTestCase.hpp"
#include "vktPipelineSpecConstantUtil.hpp"
#include "vktPipelineMakeUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuTexture.hpp"
#include "tcuFormatUtil.hpp"
#include "tcuFloat.hpp"

#include "gluShaderUtil.hpp"

#include "vkBuilderUtil.hpp"
#include "vkPrograms.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkComputePipelineConstructionUtil.hpp"

#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"

#include <limits>

namespace vkt
{
namespace pipeline
{

using namespace vk;

namespace
{

static const char* const s_perVertexBlock =	"gl_PerVertex {\n"
											"    vec4 gl_Position;\n"
											"}";

//! Raw memory storage for values used in test cases.
//! We use it to simplify test case definitions where different types are expected in the result.
class GenericValue
{
public:
	GenericValue (void) { clear(); }

	//! Copy up to 'size' bytes of 'data'.
	GenericValue (const void* data, const deUint32 size)
	{
		DE_ASSERT(size <= sizeof(m_data));
		clear();
		deMemcpy(&m_data, data, size);
	}

private:
	deUint64 m_data;

	void clear (void) { m_data = 0; }
};

inline GenericValue makeValueBool32	 (const bool a)			{ return GenericValue(&a, sizeof(a)); }
inline GenericValue makeValueInt8    (const deInt8 a)		{ return GenericValue(&a, sizeof(a)); }
inline GenericValue makeValueUint8   (const deUint8 a)		{ return GenericValue(&a, sizeof(a)); }
inline GenericValue makeValueInt16   (const deInt16 a)		{ return GenericValue(&a, sizeof(a)); }
inline GenericValue makeValueUint16  (const deUint16 a)		{ return GenericValue(&a, sizeof(a)); }
inline GenericValue makeValueInt32	 (const deInt32 a)		{ return GenericValue(&a, sizeof(a)); }
inline GenericValue makeValueUint32	 (const deUint32 a)		{ return GenericValue(&a, sizeof(a)); }
inline GenericValue makeValueInt64   (const deInt64 a)		{ return GenericValue(&a, sizeof(a)); }
inline GenericValue makeValueUint64  (const deUint64 a)		{ return GenericValue(&a, sizeof(a)); }
inline GenericValue makeValueFloat16 (const tcu::Float16 a)	{ return GenericValue(&a, sizeof(a)); }
inline GenericValue makeValueFloat32 (const float a)		{ return GenericValue(&a, sizeof(a)); }
inline GenericValue makeValueFloat64 (const double a)		{ return GenericValue(&a, sizeof(a)); }

struct SpecConstant
{
	deUint32			specID;				//!< specialization constant ID
	std::string			declarationCode;	//!< syntax to declare the constant, use ${ID} as an ID placeholder
	deUint32			size;				//!< data size on the host, 0 = no specialized value
	GenericValue		specValue;			//!< specialized value passed by the API
	bool				forceUse;			//!< always include a VkSpecializationMapEntry for this spec constant

	SpecConstant (const deUint32 specID_, const std::string declarationCode_)
		: specID			(specID_)
		, declarationCode	(declarationCode_)
		, size				(0)
		, specValue			()
		, forceUse			(false)
	{
	}

	SpecConstant (const deUint32 specID_, const std::string declarationCode_, const deUint32 size_, const GenericValue specValue_, bool forceUse_ = false)
		: specID			(specID_)
		, declarationCode	(declarationCode_)
		, size				(size_)
		, specValue			(specValue_)
		, forceUse			(forceUse_)
	{
	}
};

//! Useful when referring to a value in a buffer (i.e. check expected values in SSBO).
struct OffsetValue
{
	deUint32		size;		//!< data size in the buffer (up to sizeof(value))
	deUint32		offset;		//!< offset into the buffer
	GenericValue	value;		//!< value expected to be there

	OffsetValue (const deUint32 size_, const deUint32 offset_, const GenericValue value_)
		: size		(size_)
		, offset	(offset_)
		, value		(value_)
	{}
};

//! Get the integer value of 'size' bytes at 'memory' location.
deUint64 memoryAsInteger (const void* memory, const deUint32 size)
{
	DE_ASSERT(size <= sizeof(deUint64));
	deUint64 value = 0;
	deMemcpy(&value, memory, size);
	return value;
}

inline std::string memoryAsHexString (const void* memory, const deUint32 size)
{
	const deUint8* memoryBytePtr = static_cast<const deUint8*>(memory);
	return de::toString(tcu::formatArray(tcu::Format::HexIterator<deUint8>(memoryBytePtr), tcu::Format::HexIterator<deUint8>(memoryBytePtr + size)));
}

void logValueMismatch (tcu::TestLog& log, const void* expected, const void* actual, const deUint32 offset, const deUint32 size)
{
	const bool canDisplayValue = (size <= sizeof(deUint64));
	log << tcu::TestLog::Message
		<< "Comparison failed for value at offset " << de::toString(offset) << ": expected "
		<< (canDisplayValue ? de::toString(memoryAsInteger(expected, size)) + " " : "") << memoryAsHexString(expected, size) << " but got "
		<< (canDisplayValue ? de::toString(memoryAsInteger(actual, size)) + " " : "") << memoryAsHexString(actual, size)
		<< tcu::TestLog::EndMessage;
}

//! Check if expected values exist in the memory.
bool verifyValues (tcu::TestLog& log, const void* memory, const std::vector<OffsetValue>& expectedValues)
{
	bool ok = true;
	log << tcu::TestLog::Section("compare", "Verify result values");

	for (std::vector<OffsetValue>::const_iterator it = expectedValues.begin(); it < expectedValues.end(); ++it)
	{
		const char* const valuePtr = static_cast<const char*>(memory) + it->offset;
		if (deMemCmp(valuePtr, &it->value, it->size) != 0)
		{
			ok = false;
			logValueMismatch(log, &it->value, valuePtr, it->offset, it->size);
		}
	}

	if (ok)
		log << tcu::TestLog::Message << "All OK" << tcu::TestLog::EndMessage;

	log << tcu::TestLog::EndSection;
	return ok;
}

//! Bundles together common test case parameters.
struct CaseDefinition
{
	std::string					name;				//!< Test case name
	std::vector<SpecConstant>	specConstants;		//!< list of specialization constants to declare
	VkDeviceSize				ssboSize;			//!< required ssbo size in bytes
	std::string					ssboCode;			//!< ssbo member definitions
	std::string					globalCode;			//!< generic shader code outside the main function (e.g. declarations)
	std::string					mainCode;			//!< generic shader code to execute in main (e.g. assignments)
	std::vector<OffsetValue>	expectedValues;		//!< list of values to check inside the ssbo buffer
	FeatureFlags				requirements;		//!< features the implementation must support to allow this test to run
	bool						packData;			//!< whether to tightly pack specialization constant data or not
};

//! Manages Vulkan structures to pass specialization data.
class Specialization
{
public:
											Specialization (const std::vector<SpecConstant>& specConstants, bool packData);

	//! Can return NULL if nothing is specialized
	const VkSpecializationInfo*				getSpecializationInfo (void) const { return m_entries.size() > 0 ? &m_specialization : DE_NULL; }

private:
	std::vector<deUint8>					m_data;
	std::vector<VkSpecializationMapEntry>	m_entries;
	VkSpecializationInfo					m_specialization;
};

Specialization::Specialization (const std::vector<SpecConstant>& specConstants, bool packData)
{
	const auto kGenericValueSize = static_cast<deUint32>(sizeof(GenericValue));

	// Reserve memory for the worst case in m_data.
	m_data.resize(specConstants.size() * kGenericValueSize, std::numeric_limits<deUint8>::max());
	m_entries.reserve(specConstants.size());

	deUint32 offset = 0u;
	for (const auto& sc : specConstants)
	{
		if (sc.size != 0u || sc.forceUse)
		{
			if (sc.size > 0u)
				deMemcpy(&m_data[offset], &sc.specValue, sc.size);
			m_entries.push_back(makeSpecializationMapEntry(sc.specID, offset, sc.size));
			offset += (packData ? sc.size : kGenericValueSize);
		}
	}

	if (m_entries.size() > 0)
	{
		m_specialization.mapEntryCount = static_cast<deUint32>(m_entries.size());
		m_specialization.pMapEntries   = m_entries.data();
		m_specialization.dataSize	   = static_cast<deUintptr>(offset);
		m_specialization.pData		   = m_data.data();
	}
	else
		deMemset(&m_specialization, 0, sizeof(m_specialization));
}

class SpecConstantTest : public TestCase
{
public:
								SpecConstantTest	(tcu::TestContext&				testCtx,
													 const PipelineConstructionType pipelineType,	//!< how pipeline is constructed
													 const VkShaderStageFlagBits	stage,			//!< which shader stage is tested
													 const CaseDefinition&			caseDef);

	void						initPrograms		(SourceCollections&		programCollection) const;
	TestInstance*				createInstance		(Context&				context) const;
	virtual void				checkSupport		(Context&				context) const;

private:
	const PipelineConstructionType	m_pipelineConstructionType;
	const VkShaderStageFlagBits		m_stage;
	const CaseDefinition			m_caseDef;
};

SpecConstantTest::SpecConstantTest (tcu::TestContext&				testCtx,
									const PipelineConstructionType	pipelineType,
									const VkShaderStageFlagBits		stage,
									const CaseDefinition&			caseDef)
	: TestCase						(testCtx, caseDef.name, "")
	, m_pipelineConstructionType	(pipelineType)
	, m_stage						(stage)
	, m_caseDef						(caseDef)
{
}

//! Build a string that declares all specialization constants, replacing ${ID} with proper ID numbers.
std::string generateSpecConstantCode (const std::vector<SpecConstant>& specConstants)
{
	std::ostringstream code;
	for (std::vector<SpecConstant>::const_iterator it = specConstants.begin(); it != specConstants.end(); ++it)
	{
		std::string decl = it->declarationCode;
		const std::string::size_type pos = decl.find("${ID}");
		if (pos != std::string::npos)
			decl.replace(pos, 5, de::toString(it->specID));
		code << decl << "\n";
	}
	code << "\n";
	return code.str();
}

std::string generateSSBOCode (const std::string& memberDeclarations)
{
	std::ostringstream code;
	code << "layout (set = 0, binding = 0, std430) writeonly buffer Output {\n"
		 << memberDeclarations
		 << "} sb_out;\n"
		 << "\n";
	return code.str();
}

void SpecConstantTest::initPrograms (SourceCollections& programCollection) const
{
	// Always add vertex and fragment to graphics stages
	VkShaderStageFlags requiredStages = m_stage;

	if (requiredStages & VK_SHADER_STAGE_ALL_GRAPHICS)
		requiredStages |= VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

	if (requiredStages & (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT))
		requiredStages |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;

	// Either graphics or compute must be defined, but not both
	DE_ASSERT(((requiredStages & VK_SHADER_STAGE_ALL_GRAPHICS) != 0) != ((requiredStages & VK_SHADER_STAGE_COMPUTE_BIT) != 0));

	// Extensions needed for some tests.
	std::ostringstream extStream;
	if (m_caseDef.requirements & FEATURE_SHADER_INT_64)
		extStream << "#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require\n";
	if (m_caseDef.requirements & FEATURE_SHADER_INT_16)
		extStream << "#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require\n";
	if (m_caseDef.requirements & FEATURE_SHADER_INT_8)
		extStream << "#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require\n";
	if (m_caseDef.requirements & FEATURE_SHADER_FLOAT_16)
		extStream << "#extension GL_EXT_shader_explicit_arithmetic_types_float16 : require\n";
	const std::string extensions = extStream.str();

	// This makes glslang avoid the UniformAndStorage* capabilities.
	const vk::ShaderBuildOptions buildOptions (programCollection.usedVulkanVersion, SPIRV_VERSION_1_3, 0u);

	if (requiredStages & VK_SHADER_STAGE_VERTEX_BIT)
	{
		const bool useSpecConst = (m_stage == VK_SHADER_STAGE_VERTEX_BIT);
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< extensions
			<< "layout(location = 0) in highp vec4 position;\n"
			<< "\n"
			<< "out " << s_perVertexBlock << ";\n"
			<< "\n"
			<< (useSpecConst ? generateSpecConstantCode(m_caseDef.specConstants) : "")
			<< (useSpecConst ? generateSSBOCode(m_caseDef.ssboCode) : "")
			<< (useSpecConst ? m_caseDef.globalCode + "\n" : "")
			<< "void main (void)\n"
			<< "{\n"
			<< (useSpecConst ? m_caseDef.mainCode + "\n" : "")
			<< "    gl_Position = position;\n"
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str()) << buildOptions;
	}

	if (requiredStages & VK_SHADER_STAGE_FRAGMENT_BIT)
	{
		const bool useSpecConst = (m_stage == VK_SHADER_STAGE_FRAGMENT_BIT);
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< extensions
			<< "layout(location = 0) out highp vec4 fragColor;\n"
			<< "\n"
			<< (useSpecConst ? generateSpecConstantCode(m_caseDef.specConstants) : "")
			<< (useSpecConst ? generateSSBOCode(m_caseDef.ssboCode) : "")
			<< (useSpecConst ? m_caseDef.globalCode + "\n" : "")
			<< "void main (void)\n"
			<< "{\n"
			<< (useSpecConst ? m_caseDef.mainCode + "\n" : "")
			<< "    fragColor = vec4(1.0, 1.0, 0.0, 1.0);\n"
			<< "}\n";

		programCollection.glslSources.add("frag") << glu::FragmentSource(src.str()) << buildOptions;
	}

	if (requiredStages & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
	{
		const bool useSpecConst = (m_stage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< extensions
			<< "layout(vertices = 3) out;\n"
			<< "\n"
			<< "in " << s_perVertexBlock << " gl_in[gl_MaxPatchVertices];\n"
			<< "\n"
			<< "out " << s_perVertexBlock << " gl_out[];\n"
			<< "\n"
			<< (useSpecConst ? generateSpecConstantCode(m_caseDef.specConstants) : "")
			<< (useSpecConst ? generateSSBOCode(m_caseDef.ssboCode) : "")
			<< (useSpecConst ? m_caseDef.globalCode + "\n" : "")
			<< "void main (void)\n"
			<< "{\n"
			<< (useSpecConst ? m_caseDef.mainCode + "\n" : "")
			<< "    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
			<< "    if (gl_InvocationID == 0)\n"
			<< "    {\n"
			<< "        gl_TessLevelInner[0] = 3;\n"
			<< "        gl_TessLevelOuter[0] = 2;\n"
			<< "        gl_TessLevelOuter[1] = 2;\n"
			<< "        gl_TessLevelOuter[2] = 2;\n"
			<< "    }\n"
			<< "}\n";

		programCollection.glslSources.add("tesc") << glu::TessellationControlSource(src.str()) << buildOptions;
	}

	if (requiredStages & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
	{
		const bool useSpecConst = (m_stage == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< extensions
			<< "layout(triangles, equal_spacing, ccw) in;\n"
			<< "\n"
			<< "in " << s_perVertexBlock << " gl_in[gl_MaxPatchVertices];\n"
			<< "\n"
			<< "out " << s_perVertexBlock << ";\n"
			<< "\n"
			<< (useSpecConst ? generateSpecConstantCode(m_caseDef.specConstants) : "")
			<< (useSpecConst ? generateSSBOCode(m_caseDef.ssboCode) : "")
			<< (useSpecConst ? m_caseDef.globalCode + "\n" : "")
			<< "void main (void)\n"
			<< "{\n"
			<< (useSpecConst ? m_caseDef.mainCode + "\n" : "")
			<< "    vec3 p0 = gl_TessCoord.x * gl_in[0].gl_Position.xyz;\n"
			<< "    vec3 p1 = gl_TessCoord.y * gl_in[1].gl_Position.xyz;\n"
			<< "    vec3 p2 = gl_TessCoord.z * gl_in[2].gl_Position.xyz;\n"
			<< "    gl_Position = vec4(p0 + p1 + p2, 1.0);\n"
			<< "}\n";

		programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(src.str()) << buildOptions;
	}

	if (requiredStages & VK_SHADER_STAGE_GEOMETRY_BIT)
	{
		const bool useSpecConst = (m_stage == VK_SHADER_STAGE_GEOMETRY_BIT);
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< extensions
			<< "layout(triangles) in;\n"
			<< "layout(triangle_strip, max_vertices = 3) out;\n"
			<< "\n"
			<< "in " << s_perVertexBlock << " gl_in[];\n"
			<< "\n"
			<< "out " << s_perVertexBlock << ";\n"
			<< "\n"
			<< (useSpecConst ? generateSpecConstantCode(m_caseDef.specConstants) : "")
			<< (useSpecConst ? generateSSBOCode(m_caseDef.ssboCode) : "")
			<< (useSpecConst ? m_caseDef.globalCode + "\n" : "")
			<< "void main (void)\n"
			<< "{\n"
			<< (useSpecConst ? m_caseDef.mainCode + "\n" : "")
			<< "    gl_Position = gl_in[0].gl_Position;\n"
			<< "    EmitVertex();\n"
			<< "\n"
			<< "    gl_Position = gl_in[1].gl_Position;\n"
			<< "    EmitVertex();\n"
			<< "\n"
			<< "    gl_Position = gl_in[2].gl_Position;\n"
			<< "    EmitVertex();\n"
			<< "\n"
			<< "    EndPrimitive();\n"
			<< "}\n";

		programCollection.glslSources.add("geom") << glu::GeometrySource(src.str()) << buildOptions;
	}

	if (requiredStages & VK_SHADER_STAGE_COMPUTE_BIT)
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< extensions
			// Don't define work group size, use the default or specialization constants
			<< "\n"
			<< generateSpecConstantCode(m_caseDef.specConstants)
			<< generateSSBOCode(m_caseDef.ssboCode)
			<< m_caseDef.globalCode + "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< m_caseDef.mainCode
			<< "}\n";

		programCollection.glslSources.add("comp") << glu::ComputeSource(src.str()) << buildOptions;
	}
}

class ComputeTestInstance : public TestInstance
{
public:
									ComputeTestInstance	(Context&							context,
														 PipelineConstructionType			pipelineConstructionType,
														 const VkDeviceSize					ssboSize,
														 const std::vector<SpecConstant>&	specConstants,
														 const std::vector<OffsetValue>&	expectedValues,
														 bool								packData);

	tcu::TestStatus					iterate				(void);

private:
	const PipelineConstructionType	m_pipelineConstructionType;
	const VkDeviceSize				m_ssboSize;
	const std::vector<SpecConstant>	m_specConstants;
	const std::vector<OffsetValue>	m_expectedValues;
	const bool						m_packData;
};

ComputeTestInstance::ComputeTestInstance (Context&							context,
										  PipelineConstructionType			pipelineConstructionType,
										  const VkDeviceSize				ssboSize,
										  const std::vector<SpecConstant>&	specConstants,
										  const std::vector<OffsetValue>&	expectedValues,
										  bool								packData)
	: TestInstance					(context)
	, m_pipelineConstructionType	(pipelineConstructionType)
	, m_ssboSize					(ssboSize)
	, m_specConstants				(specConstants)
	, m_expectedValues				(expectedValues)
	, m_packData					(packData)
{
}

tcu::TestStatus ComputeTestInstance::iterate (void)
{
	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	const VkDevice			device				= m_context.getDevice();
	const VkQueue			queue				= m_context.getUniversalQueue();
	const deUint32			queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	Allocator&				allocator			= m_context.getDefaultAllocator();

	// Descriptors

	const BufferWithMemory resultBuffer(vk, device, allocator, makeBufferCreateInfo(m_ssboSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), MemoryRequirement::HostVisible);

	const Unique<VkDescriptorSetLayout> descriptorSetLayout(DescriptorSetLayoutBuilder()
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
		.build(vk, device));

	const Unique<VkDescriptorPool> descriptorPool(DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	const Unique<VkDescriptorSet> descriptorSet        (makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));
	const VkDescriptorBufferInfo  descriptorBufferInfo = makeDescriptorBufferInfo(resultBuffer.get(), 0ull, m_ssboSize);

	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorBufferInfo)
		.update(vk, device);

	// Specialization

	const Specialization        specialization (m_specConstants, m_packData);
	const VkSpecializationInfo* pSpecInfo      = specialization.getSpecializationInfo();

	// Pipeline

	const PipelineLayoutWrapper		pipelineLayout	(m_pipelineConstructionType, vk, device, *descriptorSetLayout);
	ComputePipelineWrapper			pipeline		(vk, device, graphicsToComputeConstructionType(m_pipelineConstructionType), m_context.getBinaryCollection().get("comp"));
	pipeline.setDescriptorSetLayout(*descriptorSetLayout);
	if (pSpecInfo)
		pipeline.setSpecializationInfo(*pSpecInfo);
	pipeline.buildPipeline();
	const Unique<VkCommandPool>		cmdPool			(createCommandPool		(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>	cmdBuffer		(makeCommandBuffer		(vk, device, *cmdPool));

	beginCommandBuffer(vk, *cmdBuffer);

	pipeline.bind(*cmdBuffer);
	vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);

	vk.cmdDispatch(*cmdBuffer, 1u, 1u, 1u);

	{
		const VkBufferMemoryBarrier shaderWriteBarrier = makeBufferMemoryBarrier(
			VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *resultBuffer, 0ull, m_ssboSize);

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u,
			0u, DE_NULL, 1u, &shaderWriteBarrier, 0u, DE_NULL);
	}

	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	// Verify results

	const Allocation& resultAlloc = resultBuffer.getAllocation();
	invalidateAlloc(vk, device, resultAlloc);

	if (verifyValues(m_context.getTestContext().getLog(), resultAlloc.getHostPtr(), m_expectedValues))
		return tcu::TestStatus::pass("Success");
	else
		return tcu::TestStatus::fail("Values did not match");
}

class GraphicsTestInstance : public TestInstance
{
public:
									GraphicsTestInstance (Context&							context,
														  const PipelineConstructionType	pipelineConstructionType,
														  const VkDeviceSize				ssboSize,
														  const std::vector<SpecConstant>&	specConstants,
														  const std::vector<OffsetValue>&	expectedValues,
														  const VkShaderStageFlagBits		stage,
														  bool								packData);

	tcu::TestStatus					iterate				 (void);

private:
	const PipelineConstructionType	m_pipelineConstructionType;
	const VkDeviceSize				m_ssboSize;
	const std::vector<SpecConstant>	m_specConstants;
	const std::vector<OffsetValue>	m_expectedValues;
	const VkShaderStageFlagBits		m_stage;
	const bool						m_packData;
};

GraphicsTestInstance::GraphicsTestInstance (Context&							context,
											const PipelineConstructionType		pipelineConstructionType,
											const VkDeviceSize					ssboSize,
											const std::vector<SpecConstant>&	specConstants,
											const std::vector<OffsetValue>&		expectedValues,
											const VkShaderStageFlagBits			stage,
											bool								packData)
	: TestInstance					(context)
	, m_pipelineConstructionType	(pipelineConstructionType)
	, m_ssboSize					(ssboSize)
	, m_specConstants				(specConstants)
	, m_expectedValues				(expectedValues)
	, m_stage						(stage)
	, m_packData					(packData)
{
}

tcu::TestStatus GraphicsTestInstance::iterate (void)
{
	const InstanceInterface&	vki					= m_context.getInstanceInterface();
	const DeviceInterface&		vk					= m_context.getDeviceInterface();
	const VkPhysicalDevice		physicalDevice		= m_context.getPhysicalDevice();
	const VkDevice				device				= m_context.getDevice();
	const VkQueue				queue				= m_context.getUniversalQueue();
	const deUint32				queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	Allocator&					allocator			= m_context.getDefaultAllocator();

	// Color attachment

	const tcu::IVec2          renderSize    = tcu::IVec2(32, 32);
	const VkFormat            imageFormat   = VK_FORMAT_R8G8B8A8_UNORM;
	const ImageWithMemory     colorImage    (vk, device, allocator, makeImageCreateInfo(renderSize, imageFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT), MemoryRequirement::Any);
	const Unique<VkImageView> colorImageView(makeImageView(vk, device, *colorImage, VK_IMAGE_VIEW_TYPE_2D, imageFormat, makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u)));

	// Vertex buffer

	const deUint32			numVertices           = 3;
	const VkDeviceSize		vertexBufferSizeBytes = sizeof(tcu::Vec4) * numVertices;
	const BufferWithMemory	vertexBuffer          (vk, device, allocator, makeBufferCreateInfo(vertexBufferSizeBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), MemoryRequirement::HostVisible);

	{
		const Allocation& alloc = vertexBuffer.getAllocation();
		tcu::Vec4* pVertices = reinterpret_cast<tcu::Vec4*>(alloc.getHostPtr());

		pVertices[0] = tcu::Vec4(-1.0f, -1.0f,  0.0f,  1.0f);
		pVertices[1] = tcu::Vec4(-1.0f,  1.0f,  0.0f,  1.0f);
		pVertices[2] = tcu::Vec4( 1.0f, -1.0f,  0.0f,  1.0f);

		flushAlloc(vk, device, alloc);
		// No barrier needed, flushed memory is automatically visible
	}

	// Descriptors

	const BufferWithMemory resultBuffer(vk, device, allocator, makeBufferCreateInfo(m_ssboSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), MemoryRequirement::HostVisible);

	const Unique<VkDescriptorSetLayout> descriptorSetLayout(DescriptorSetLayoutBuilder()
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
		.build(vk, device));

	const Unique<VkDescriptorPool> descriptorPool(DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	const Unique<VkDescriptorSet> descriptorSet        (makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));
	const VkDescriptorBufferInfo  descriptorBufferInfo = makeDescriptorBufferInfo(resultBuffer.get(), 0ull, m_ssboSize);

	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorBufferInfo)
		.update(vk, device);

	// Specialization

	const Specialization        specialization (m_specConstants, m_packData);
	const VkSpecializationInfo* pSpecInfo      = specialization.getSpecializationInfo();

	// Pipeline

	RenderPassWrapper				renderPass		(m_pipelineConstructionType, vk, device, imageFormat);
	renderPass.createFramebuffer(vk, device, colorImage.get(), colorImageView.get(), static_cast<deUint32>(renderSize.x()), static_cast<deUint32>(renderSize.y()));
	const PipelineLayoutWrapper		pipelineLayout	(m_pipelineConstructionType, vk, device, *descriptorSetLayout);
	const Unique<VkCommandPool>		cmdPool			(createCommandPool (vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>	cmdBuffer		(makeCommandBuffer (vk, device, *cmdPool));

	vk::BinaryCollection&			binaryCollection(m_context.getBinaryCollection());
	VkPrimitiveTopology				topology		(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	const std::vector<VkViewport>	viewport		{ makeViewport(renderSize) };
	const std::vector<VkRect2D>		scissor			{ makeRect2D(renderSize) };

	ShaderWrapper vertShaderModule	= ShaderWrapper(vk, device, binaryCollection.get("vert"), 0u);
	ShaderWrapper tescShaderModule;
	ShaderWrapper teseShaderModule;
	ShaderWrapper geomShaderModule;
	ShaderWrapper fragShaderModule	= ShaderWrapper(vk, device, binaryCollection.get("frag"), 0u);

	if ((m_stage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) || (m_stage == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT))
	{
		tescShaderModule	= ShaderWrapper(vk, device, binaryCollection.get("tesc"), 0u);
		teseShaderModule	= ShaderWrapper(vk, device, binaryCollection.get("tese"), 0u);
		topology			= VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
	}
	if (m_stage == VK_SHADER_STAGE_GEOMETRY_BIT)
		geomShaderModule = ShaderWrapper(vk, device, binaryCollection.get("geom"), 0u);

	GraphicsPipelineWrapper graphicsPipeline(vki, vk, physicalDevice, device, m_context.getDeviceExtensions(), m_pipelineConstructionType);
	graphicsPipeline.setDefaultRasterizationState()
					.setDefaultDepthStencilState()
					.setDefaultMultisampleState()
					.setDefaultColorBlendState()
					.setDefaultTopology(topology)
					.setupVertexInputState()
					.setupPreRasterizationShaderState(viewport,
													  scissor,
													  pipelineLayout,
													  *renderPass,
													  0u,
													  vertShaderModule,
													  0u,
													  tescShaderModule,
													  teseShaderModule,
													  geomShaderModule,
													  pSpecInfo)
					.setupFragmentShaderState(pipelineLayout, *renderPass, 0u, fragShaderModule, DE_NULL, DE_NULL, pSpecInfo)
					.setupFragmentOutputState(*renderPass)
					.setMonolithicPipelineLayout(pipelineLayout)
					.buildPipeline();

	// Draw commands

	const VkRect2D		renderArea			= makeRect2D(renderSize);
	const tcu::Vec4		clearColor			(0.0f, 0.0f, 0.0f, 1.0f);
	const VkDeviceSize	vertexBufferOffset	= 0ull;

	beginCommandBuffer(vk, *cmdBuffer);

	{
		const VkImageSubresourceRange imageFullSubresourceRange              = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
		const VkImageMemoryBarrier    barrierColorAttachmentSetInitialLayout = makeImageMemoryBarrier(
			0u, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			*colorImage, imageFullSubresourceRange);

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0u,
			0u, DE_NULL, 0u, DE_NULL, 1u, &barrierColorAttachmentSetInitialLayout);
	}

	renderPass.begin(vk, *cmdBuffer, renderArea, clearColor);

	graphicsPipeline.bind(*cmdBuffer);
	vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);
	vk.cmdBindVertexBuffers (*cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);

	vk.cmdDraw(*cmdBuffer, numVertices, 1u, 0u, 0u);
	renderPass.end(vk, *cmdBuffer);

	{
		const VkBufferMemoryBarrier shaderWriteBarrier = makeBufferMemoryBarrier(
			VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *resultBuffer, 0ull, m_ssboSize);

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u,
			0u, DE_NULL, 1u, &shaderWriteBarrier, 0u, DE_NULL);
	}

	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	// Verify results

	const Allocation& resultAlloc = resultBuffer.getAllocation();
	invalidateAlloc(vk, device, resultAlloc);

	if (verifyValues(m_context.getTestContext().getLog(), resultAlloc.getHostPtr(), m_expectedValues))
		return tcu::TestStatus::pass("Success");
	else
		return tcu::TestStatus::fail("Values did not match");
}

FeatureFlags getShaderStageRequirements (const VkShaderStageFlags stageFlags)
{
	FeatureFlags features = (FeatureFlags)0;

	if (((stageFlags & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) != 0) || ((stageFlags & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) != 0))
		features |= FEATURE_TESSELLATION_SHADER;

	if ((stageFlags & VK_SHADER_STAGE_GEOMETRY_BIT) != 0)
		features |= FEATURE_GEOMETRY_SHADER;

	// All tests use SSBO writes to read back results.
	if ((stageFlags & VK_SHADER_STAGE_ALL_GRAPHICS) != 0)
	{
		if ((stageFlags & VK_SHADER_STAGE_FRAGMENT_BIT) != 0)
			features |= FEATURE_FRAGMENT_STORES_AND_ATOMICS;
		else
			features |= FEATURE_VERTEX_PIPELINE_STORES_AND_ATOMICS;
	}

	return features;
}

void SpecConstantTest::checkSupport (Context& context) const
{
	requireFeatures(context, m_caseDef.requirements | getShaderStageRequirements(m_stage));
	checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), m_pipelineConstructionType);
}

TestInstance* SpecConstantTest::createInstance (Context& context) const
{
	if (m_stage & VK_SHADER_STAGE_COMPUTE_BIT)
		return new ComputeTestInstance(context, m_pipelineConstructionType, m_caseDef.ssboSize, m_caseDef.specConstants, m_caseDef.expectedValues, m_caseDef.packData);
	else
		return new GraphicsTestInstance(context, m_pipelineConstructionType, m_caseDef.ssboSize, m_caseDef.specConstants, m_caseDef.expectedValues, m_stage, m_caseDef.packData);
}

//! Declare specialization constants but use them with default values.
tcu::TestCaseGroup* createDefaultValueTests (tcu::TestContext& testCtx, const PipelineConstructionType pipelineType, const VkShaderStageFlagBits shaderStage)
{
	de::MovePtr<tcu::TestCaseGroup> testGroup (new tcu::TestCaseGroup(testCtx, "default_value", "use default constant value"));

	CaseDefinition defs[] =
	{
		{
			"bool",
			makeVector(SpecConstant(1u, "layout(constant_id = ${ID}) const bool sc0 = true;"),
					   SpecConstant(2u, "layout(constant_id = ${ID}) const bool sc1 = false;")),
			8,
			"    bool r0;\n"
			"    bool r1;\n",
			"",
			"    sb_out.r0 = sc0;\n"
			"    sb_out.r1 = sc1;\n",
			makeVector(OffsetValue(4, 0, makeValueBool32(true)),
					   OffsetValue(4, 4, makeValueBool32(false))),
			(FeatureFlags)0,
			false,
		},
		{
			"int8",
			makeVector(SpecConstant(1u, "layout(constant_id = ${ID}) const int8_t sc0 = int8_t(1);"),
					   SpecConstant(2u, "layout(constant_id = ${ID}) const int8_t sc1 = int8_t(-2);")),
			2,
			"    int8_t r0;\n"
			"    int8_t r1;\n",
			"",
			"    int8_t aux = sc0 + sc1;\n"
			"    sb_out.r0 = sc0;\n"
			"    sb_out.r1 = sc1;\n",
			makeVector(OffsetValue(1, 0, makeValueInt8(1)),
					   OffsetValue(1, 1, makeValueInt8(-2))),
			FEATURE_SHADER_INT_8,
			false,
		},
		{
			"uint8",
			makeVector(SpecConstant(1u, "layout(constant_id = ${ID}) const uint8_t sc0 = int8_t(15);"),
					   SpecConstant(2u, "layout(constant_id = ${ID}) const uint8_t sc1 = int8_t(43);")),
			2,
			"    uint8_t r0;\n"
			"    uint8_t r1;\n",
			"",
			"    uint8_t aux = sc0 + sc1;\n"
			"    sb_out.r0 = sc0;\n"
			"    sb_out.r1 = sc1;\n",
			makeVector(OffsetValue(1, 0, makeValueUint8(15)),
					   OffsetValue(1, 1, makeValueUint8(43))),
			FEATURE_SHADER_INT_8,
			false,
		},
		{
			"int16",
			makeVector(SpecConstant(1u, "layout(constant_id = ${ID}) const int16_t sc0 = 20000s;", 2, makeValueInt16(32000)),
					   SpecConstant(2u, "layout(constant_id = ${ID}) const int16_t sc1 = -20000s;")),
			4,
			"    int16_t r0;\n"
			"    int16_t r1;\n",
			"",
			"    int16_t aux = sc0 + sc1;\n"
			"    sb_out.r0 = sc0;\n"
			"    sb_out.r1 = sc1;\n",
			makeVector(OffsetValue(2, 0, makeValueInt16(32000)),
					   OffsetValue(2, 2, makeValueInt16(-20000))),
			FEATURE_SHADER_INT_16,
			false,
		},
		{
			"uint16",
			makeVector(SpecConstant(1u, "layout(constant_id = ${ID}) const uint16_t sc0 = 64000us;"),
					   SpecConstant(2u, "layout(constant_id = ${ID}) const uint16_t sc1 = 51829us;")),
			4,
			"    uint16_t r0;\n"
			"    uint16_t r1;\n",
			"",
			"    uint16_t aux = sc0 + sc1;\n"
			"    sb_out.r0 = sc0;\n"
			"    sb_out.r1 = sc1;\n",
			makeVector(OffsetValue(2, 0, makeValueUint16(64000)),
					   OffsetValue(2, 2, makeValueUint16(51829))),
			FEATURE_SHADER_INT_16,
			false,
		},
		{
			"int",
			makeVector(SpecConstant(1u, "layout(constant_id = ${ID}) const int sc0 = -3;"),
					   SpecConstant(2u, "layout(constant_id = ${ID}) const int sc1 = 17;")),
			8,
			"    int r0;\n"
			"    int r1;\n",
			"",
			"    sb_out.r0 = sc0;\n"
			"    sb_out.r1 = sc1;\n",
			makeVector(OffsetValue(4, 0, makeValueInt32(-3)),
					   OffsetValue(4, 4, makeValueInt32(17))),
			(FeatureFlags)0,
			false,
		},
		{
			"uint",
			makeVector(SpecConstant(1u, "layout(constant_id = ${ID}) const uint sc0 = 42u;")),
			4,
			"    uint r0;\n",
			"",
			"    sb_out.r0 = sc0;\n",
			makeVector(OffsetValue(4, 0, makeValueUint32(42u))),
			(FeatureFlags)0,
			false,
		},
		{
			"int64",
			makeVector(SpecConstant(1u, "layout(constant_id = ${ID}) const int64_t sc0 = 9141386509785772560l;"),
					   SpecConstant(2u, "layout(constant_id = ${ID}) const int64_t sc1 = -9141386509785772560l;")),
			16,
			"    int64_t r0;\n"
			"    int64_t r1;\n",
			"",
			"    sb_out.r0 = sc0;\n"
			"    sb_out.r1 = sc1;\n",
			makeVector(OffsetValue(8, 0, makeValueInt64(9141386509785772560ll)),
					   OffsetValue(8, 8, makeValueInt64(-9141386509785772560ll))),
			FEATURE_SHADER_INT_64,
			false,
		},
		{
			"uint64",
			makeVector(SpecConstant(1u, "layout(constant_id = ${ID}) const uint64_t sc0 = 18364758544493064720ul;"),
					   SpecConstant(2u, "layout(constant_id = ${ID}) const uint64_t sc1 = 17298946664678735070ul;")),
			16,
			"    uint64_t r0;\n"
			"    uint64_t r1;\n",
			"",
			"    sb_out.r0 = sc0;\n"
			"    sb_out.r1 = sc1;\n",
			makeVector(OffsetValue(8, 0, makeValueUint64(18364758544493064720ull)),
					   OffsetValue(8, 8, makeValueUint64(17298946664678735070ull))),
			FEATURE_SHADER_INT_64,
			false,
		},
		{
			"float16",
			makeVector(SpecConstant(1u, "layout(constant_id = ${ID}) const float16_t sc0 = 7.5hf;"),
					   SpecConstant(2u, "layout(constant_id = ${ID}) const float16_t sc1 = 1.125hf;")),
			4,
			"    float16_t r0;\n"
			"    float16_t r1;\n",
			"",
			"    float16_t aux = sc0 + sc1;\n"
			"    sb_out.r0 = sc0;\n"
			"    sb_out.r1 = sc1;\n",
			makeVector(OffsetValue(2, 0, makeValueFloat16(tcu::Float16(7.5))),
					   OffsetValue(2, 2, makeValueFloat16(tcu::Float16(1.125)))),
			FEATURE_SHADER_FLOAT_16,
			false,
		},
		{
			"float",
			makeVector(SpecConstant(1u, "layout(constant_id = ${ID}) const float sc0 = 7.5;")),
			4,
			"    float r0;\n",
			"",
			"    sb_out.r0 = sc0;\n",
			makeVector(OffsetValue(4, 0, makeValueFloat32(7.5f))),
			(FeatureFlags)0,
			false,
		},
		{
			"double",
			makeVector(SpecConstant(1u, "layout(constant_id = ${ID}) const double sc0 = 2.75LF;")),
			8,
			"    double r0;\n",
			"",
			"    sb_out.r0 = sc0;\n",
			makeVector(OffsetValue(8, 0, makeValueFloat64(2.75))),
			FEATURE_SHADER_FLOAT_64,
			false,
		},
	};

	for (int i = 0; i < 2; ++i)
	{
		const bool packData = (i > 0);
		for (int defNdx = 0; defNdx < DE_LENGTH_OF_ARRAY(defs); ++defNdx)
		{
			auto& def = defs[defNdx];
			def.packData = packData;
			if (packData)
				def.name += "_packed";
			testGroup->addChild(new SpecConstantTest(testCtx, pipelineType, shaderStage, def));
		}
	}

	return testGroup.release();
}

//! Declare specialization constants and specify their values through API.
tcu::TestCaseGroup* createBasicSpecializationTests (tcu::TestContext& testCtx, const PipelineConstructionType pipelineType, const VkShaderStageFlagBits shaderStage)
{
	de::MovePtr<tcu::TestCaseGroup> testGroup (new tcu::TestCaseGroup(testCtx, "basic", "specialize a constant"));

	CaseDefinition defs[] =
	{
		{
			"bool",
			makeVector(SpecConstant(1u, "layout(constant_id = ${ID}) const bool sc0 = true;",  4, makeValueBool32(true)),
					   SpecConstant(2u, "layout(constant_id = ${ID}) const bool sc1 = false;", 4, makeValueBool32(false)),
					   SpecConstant(3u, "layout(constant_id = ${ID}) const bool sc2 = true;",  4, makeValueBool32(false)),
					   SpecConstant(4u, "layout(constant_id = ${ID}) const bool sc3 = false;", 4, makeValueBool32(true))),
			16,
			"    bool r0;\n"
			"    bool r1;\n"
			"    bool r2;\n"
			"    bool r3;\n",
			"",
			"    sb_out.r0 = sc0;\n"
			"    sb_out.r1 = sc1;\n"
			"    sb_out.r2 = sc2;\n"
			"    sb_out.r3 = sc3;\n",
			makeVector(OffsetValue(4,  0, makeValueBool32(true)),
					   OffsetValue(4,  4, makeValueBool32(false)),
					   OffsetValue(4,  8, makeValueBool32(false)),
					   OffsetValue(4, 12, makeValueBool32(true))),
			(FeatureFlags)0,
			false,
		},
		{
			"int8",
			makeVector(SpecConstant(1u, "layout(constant_id = ${ID}) const int8_t sc0 = int8_t(1);", 1, makeValueInt8(127)),
					   SpecConstant(2u, "layout(constant_id = ${ID}) const int8_t sc1 = int8_t(-2);")),
			2,
			"    int8_t r0;\n"
			"    int8_t r1;\n",
			"",
			"    int8_t aux = sc0 + sc1;\n"
			"    sb_out.r0 = sc0;\n"
			"    sb_out.r1 = sc1;\n",
			makeVector(OffsetValue(1, 0, makeValueInt8(127)),
					   OffsetValue(1, 1, makeValueInt8(-2))),
			FEATURE_SHADER_INT_8,
			false,
		},
		{
			"int8_2",
			makeVector(SpecConstant(1u, "layout(constant_id = ${ID}) const int8_t sc0 = int8_t(123);", 1, makeValueInt8(65)),
					   SpecConstant(2u, "layout(constant_id = ${ID}) const int8_t sc1 = int8_t(-33);", 1, makeValueInt8(-128))),
			2,
			"    int8_t r0;\n"
			"    int8_t r1;\n",
			"",
			"    int8_t aux = sc0 + sc1;\n"
			"    sb_out.r0 = sc0;\n"
			"    sb_out.r1 = sc1;\n",
			makeVector(OffsetValue(1, 0, makeValueInt8(65)),
					   OffsetValue(1, 1, makeValueInt8(-128))),
			FEATURE_SHADER_INT_8,
			false,
		},
		{
			"uint8",
			makeVector(SpecConstant(1u, "layout(constant_id = ${ID}) const uint8_t sc0 = int8_t(15);", 1, makeValueUint8(254)),
					   SpecConstant(2u, "layout(constant_id = ${ID}) const uint8_t sc1 = int8_t(43);")),
			2,
			"    uint8_t r0;\n"
			"    uint8_t r1;\n",
			"",
			"    uint8_t aux = sc0 + sc1;\n"
			"    sb_out.r0 = sc0;\n"
			"    sb_out.r1 = sc1;\n",
			makeVector(OffsetValue(1, 0, makeValueUint8(254)),
					   OffsetValue(1, 1, makeValueUint8(43))),
			FEATURE_SHADER_INT_8,
			false,
		},
		{
			"uint8_2",
			makeVector(SpecConstant(1u, "layout(constant_id = ${ID}) const uint8_t sc0 = int8_t(99);", 1, makeValueUint8(254)),
					   SpecConstant(2u, "layout(constant_id = ${ID}) const uint8_t sc1 = int8_t(81);", 1, makeValueUint8(255))),
			2,
			"    uint8_t r0;\n"
			"    uint8_t r1;\n",
			"",
			"    uint8_t aux = sc0 + sc1;\n"
			"    sb_out.r0 = sc0;\n"
			"    sb_out.r1 = sc1;\n",
			makeVector(OffsetValue(1, 0, makeValueUint8(254)),
					   OffsetValue(1, 1, makeValueUint8(255))),
			FEATURE_SHADER_INT_8,
			false,
		},
		{
			"int16",
			makeVector(SpecConstant(1u, "layout(constant_id = ${ID}) const int16_t sc0 = 20000s;", 2, makeValueInt16(32000)),
					   SpecConstant(2u, "layout(constant_id = ${ID}) const int16_t sc1 = -20000s;")),
			4,
			"    int16_t r0;\n"
			"    int16_t r1;\n",
			"",
			"    int16_t aux = sc0 + sc1;\n"
			"    sb_out.r0 = sc0;\n"
			"    sb_out.r1 = sc1;\n",
			makeVector(OffsetValue(2, 0, makeValueInt16(32000)),
					   OffsetValue(2, 2, makeValueInt16(-20000))),
			FEATURE_SHADER_INT_16,
			false,
		},
		{
			"int16_2",
			makeVector(SpecConstant(1u, "layout(constant_id = ${ID}) const int16_t sc0 = 20000s;", 2, makeValueInt16(32000)),
					   SpecConstant(2u, "layout(constant_id = ${ID}) const int16_t sc1 = -20000s;", 2, makeValueInt16(-21000))),
			4,
			"    int16_t r0;\n"
			"    int16_t r1;\n",
			"",
			"    int16_t aux = sc0 + sc1;\n"
			"    sb_out.r0 = sc0;\n"
			"    sb_out.r1 = sc1;\n",
			makeVector(OffsetValue(2, 0, makeValueInt16(32000)),
					   OffsetValue(2, 2, makeValueInt16(-21000))),
			FEATURE_SHADER_INT_16,
			false,
		},
		{
			"uint16",
			makeVector(SpecConstant(1u, "layout(constant_id = ${ID}) const uint16_t sc0 = 64000us;", 2, makeValueUint16(65000)),
					   SpecConstant(2u, "layout(constant_id = ${ID}) const uint16_t sc1 = 51829us;")),
			4,
			"    uint16_t r0;\n"
			"    uint16_t r1;\n",
			"",
			"    uint16_t aux = sc0 + sc1;\n"
			"    sb_out.r0 = sc0;\n"
			"    sb_out.r1 = sc1;\n",
			makeVector(OffsetValue(2, 0, makeValueUint16(65000)),
					   OffsetValue(2, 2, makeValueUint16(51829))),
			FEATURE_SHADER_INT_16,
			false,
		},
		{
			"uint16_2",
			makeVector(SpecConstant(1u, "layout(constant_id = ${ID}) const uint16_t sc0 = 64000us;", 2, makeValueUint16(65000)),
					   SpecConstant(2u, "layout(constant_id = ${ID}) const uint16_t sc1 = 51829us;", 2, makeValueUint16(63000))),
			4,
			"    uint16_t r0;\n"
			"    uint16_t r1;\n",
			"",
			"    uint16_t aux = sc0 + sc1;\n"
			"    sb_out.r0 = sc0;\n"
			"    sb_out.r1 = sc1;\n",
			makeVector(OffsetValue(2, 0, makeValueUint16(65000)),
					   OffsetValue(2, 2, makeValueUint16(63000))),
			FEATURE_SHADER_INT_16,
			false,
		},
		{
			"int",
			makeVector(SpecConstant(1u, "layout(constant_id = ${ID}) const int sc0 = -3;", 4, makeValueInt32(33)),
					   SpecConstant(2u, "layout(constant_id = ${ID}) const int sc1 = 91;"),
					   SpecConstant(3u, "layout(constant_id = ${ID}) const int sc2 = 17;", 4, makeValueInt32(-15))),
			12,
			"    int r0;\n"
			"    int r1;\n"
			"    int r2;\n",
			"",
			"    sb_out.r0 = sc0;\n"
			"    sb_out.r1 = sc1;\n"
			"    sb_out.r2 = sc2;\n",
			makeVector(OffsetValue(4, 0, makeValueInt32(33)),
					   OffsetValue(4, 4, makeValueInt32(91)),
					   OffsetValue(4, 8, makeValueInt32(-15))),
			(FeatureFlags)0,
			false,
		},
		{
			"uint",
			makeVector(SpecConstant(1u, "layout(constant_id = ${ID}) const uint sc0 = 42u;", 4, makeValueUint32(97u)),
					   SpecConstant(2u, "layout(constant_id = ${ID}) const uint sc1 = 7u;")),
			8,
			"    uint r0;\n"
			"    uint r1;\n",
			"",
			"    sb_out.r0 = sc0;\n"
			"    sb_out.r1 = sc1;\n",
			makeVector(OffsetValue(4, 0, makeValueUint32(97u)),
					   OffsetValue(4, 4, makeValueUint32(7u))),
			(FeatureFlags)0,
			false,
		},
		{
			"uint_2",
			makeVector(SpecConstant(1u, "layout(constant_id = ${ID}) const uint sc0 = 305419896u;", 4, makeValueUint32(1985229328u)),
					   SpecConstant(2u, "layout(constant_id = ${ID}) const uint sc1 = 591751049u;"),
					   SpecConstant(3u, "layout(constant_id = ${ID}) const uint sc2 = 878082202u;", 4, makeValueUint32(1698898186u))),
			12,
			"    uint r0;\n"
			"    uint r1;\n"
			"    uint r2;\n",
			"",
			"    sb_out.r0 = sc0;\n"
			"    sb_out.r1 = sc1;\n"
			"    sb_out.r2 = sc2;\n",
			makeVector(OffsetValue(4, 0, makeValueUint32(1985229328u)),
					   OffsetValue(4, 4, makeValueUint32(591751049u)),
					   OffsetValue(4, 8, makeValueUint32(1698898186u))),
			(FeatureFlags)0,
			false,
		},
		{
			"int64",
			makeVector(SpecConstant(1u, "layout(constant_id = ${ID}) const int64_t sc0 = 9141386509785772560l;", 8, makeValueInt64(9137147825770275585ll)),
					   SpecConstant(2u, "layout(constant_id = ${ID}) const int64_t sc1 = -9141386509785772560l;")),
			16,
			"    int64_t r0;\n"
			"    int64_t r1;\n",
			"",
			"    sb_out.r0 = sc0;\n"
			"    sb_out.r1 = sc1;\n",
			makeVector(OffsetValue(8, 0, makeValueInt64(9137147825770275585ll)),
					   OffsetValue(8, 8, makeValueInt64(-9141386509785772560ll))),
			FEATURE_SHADER_INT_64,
			false,
		},
		{
			"int64_2",
			makeVector(SpecConstant(1u, "layout(constant_id = ${ID}) const int64_t sc0 = 9141386509785772560l;", 8, makeValueInt64(9137147825770275585ll)),
					   SpecConstant(2u, "layout(constant_id = ${ID}) const int64_t sc1 = -9141386509785772560l;", 8, makeValueInt64(-9137164382869201665ll))),
			16,
			"    int64_t r0;\n"
			"    int64_t r1;\n",
			"",
			"    sb_out.r0 = sc0;\n"
			"    sb_out.r1 = sc1;\n",
			makeVector(OffsetValue(8, 0, makeValueInt64(9137147825770275585ll)),
					   OffsetValue(8, 8, makeValueInt64(-9137164382869201665ll))),
			FEATURE_SHADER_INT_64,
			false,
		},
		{
			"uint64",
			makeVector(SpecConstant(1u, "layout(constant_id = ${ID}) const uint64_t sc0 = 18364758544493064720ul;", 8, makeValueUint64(17279655951921914625ull)),
					   SpecConstant(2u, "layout(constant_id = ${ID}) const uint64_t sc1 = 17298946664678735070ul;")),
			16,
			"    uint64_t r0;\n"
			"    uint64_t r1;\n",
			"",
			"    sb_out.r0 = sc0;\n"
			"    sb_out.r1 = sc1;\n",
			makeVector(OffsetValue(8, 0, makeValueUint64(17279655951921914625ull)),
					   OffsetValue(8, 8, makeValueUint64(17298946664678735070ull))),
			FEATURE_SHADER_INT_64,
			false,
		},
		{
			"uint64_2",
			makeVector(SpecConstant(1u, "layout(constant_id = ${ID}) const uint64_t sc0 = 18364758544493064720ul;", 8, makeValueUint64(17279655951921914625ull)),
					   SpecConstant(2u, "layout(constant_id = ${ID}) const uint64_t sc1 = 17298946664678735070ul;", 8, makeValueUint64(17270123250533606145ull))),
			16,
			"    uint64_t r0;\n"
			"    uint64_t r1;\n",
			"",
			"    sb_out.r0 = sc0;\n"
			"    sb_out.r1 = sc1;\n",
			makeVector(OffsetValue(8, 0, makeValueUint64(17279655951921914625ull)),
					   OffsetValue(8, 8, makeValueUint64(17270123250533606145ull))),
			FEATURE_SHADER_INT_64,
			false,
		},
		// We create some floating point values below as unsigned integers to make sure all bytes are set to different values, avoiding special patterns and denormals.
		{
			"float16",
			makeVector(SpecConstant(1u, "layout(constant_id = ${ID}) const float16_t sc0 = 7.5hf;", 2, makeValueFloat16(tcu::Float16(15.75))),
					   SpecConstant(2u, "layout(constant_id = ${ID}) const float16_t sc1 = 1.125hf;")),
			4,
			"    float16_t r0;\n"
			"    float16_t r1;\n",
			"",
			"    float16_t aux = sc0 + sc1;\n"
			"    sb_out.r0 = sc0;\n"
			"    sb_out.r1 = sc1;\n",
			makeVector(OffsetValue(2, 0, makeValueFloat16(tcu::Float16(15.75))),
					   OffsetValue(2, 2, makeValueFloat16(tcu::Float16(1.125)))),
			FEATURE_SHADER_FLOAT_16,
			false,
		},
		{
			"float16_2",
			makeVector(SpecConstant(1u, "layout(constant_id = ${ID}) const float16_t sc0 = 7.5hf;", 2, makeValueUint16(0x0123u)),
					   SpecConstant(2u, "layout(constant_id = ${ID}) const float16_t sc1 = 1.125hf;"),
					   SpecConstant(3u, "layout(constant_id = ${ID}) const float16_t sc2 = 1.125hf;", 2, makeValueUint16(0xFEDCu))),
			6,
			"    float16_t r0;\n"
			"    float16_t r1;\n"
			"    float16_t r2;\n",
			"",
			"    float16_t aux = sc0 + sc1;\n"
			"    sb_out.r0 = sc0;\n"
			"    sb_out.r1 = sc1;\n"
			"    sb_out.r2 = sc2;\n",
			makeVector(OffsetValue(2, 0, makeValueUint16(0x0123u)),
					   OffsetValue(2, 2, makeValueFloat16(tcu::Float16(1.125))),
					   OffsetValue(2, 4, makeValueUint16(0xFEDCu))),
			FEATURE_SHADER_FLOAT_16,
			false,
		},
		{
			"float",
			makeVector(SpecConstant(1u, "layout(constant_id = ${ID}) const float sc0 = 7.5;", 4, makeValueFloat32(15.75f)),
					   SpecConstant(2u, "layout(constant_id = ${ID}) const float sc1 = 1.125;")),
			8,
			"    float r0;\n"
			"    float r1;\n",
			"",
			"    sb_out.r0 = sc0;\n"
			"    sb_out.r1 = sc1;\n",
			makeVector(OffsetValue(4, 0, makeValueFloat32(15.75f)),
					   OffsetValue(4, 4, makeValueFloat32(1.125f))),
			(FeatureFlags)0,
			false,
		},
		{
			"float_2",
			makeVector(SpecConstant(1u, "layout(constant_id = ${ID}) const float sc0 = 7.5;", 4, makeValueUint32(0x01234567u)),
					   SpecConstant(2u, "layout(constant_id = ${ID}) const float sc1 = 1.125;"),
					   SpecConstant(3u, "layout(constant_id = ${ID}) const float sc2 = 1.125;", 4, makeValueUint32(0xfedcba98u))),
			12,
			"    float r0;\n"
			"    float r1;\n"
			"    float r2;\n",
			"",
			"    sb_out.r0 = sc0;\n"
			"    sb_out.r1 = sc1;\n"
			"    sb_out.r2 = sc2;\n",
			makeVector(OffsetValue(4, 0, makeValueUint32(0x01234567u)),
					   OffsetValue(4, 4, makeValueFloat32(1.125f)),
					   OffsetValue(4, 8, makeValueUint32(0xfedcba98u))),
			(FeatureFlags)0,
			false,
		},
		{
			"double",
			makeVector(SpecConstant(1u, "layout(constant_id = ${ID}) const double sc0 = 2.75LF;", 8, makeValueUint64(0xFEDCBA9876543210ull)),
					   SpecConstant(2u, "layout(constant_id = ${ID}) const double sc1 = 9.25LF;")),
			16,
			"    double r0;\n"
			"    double r1;\n",
			"",
			"    sb_out.r0 = sc0;\n"
			"    sb_out.r1 = sc1;\n",
			makeVector(OffsetValue(8, 0, makeValueUint64(0xFEDCBA9876543210ull)),
					   OffsetValue(8, 8, makeValueFloat64(9.25))),
			FEATURE_SHADER_FLOAT_64,
			false,
		},
		{
			"double_2",
			makeVector(SpecConstant(1u, "layout(constant_id = ${ID}) const double sc0 = 2.75LF;", 8, makeValueUint64(0xFEDCBA9876543210ull)),
					   SpecConstant(2u, "layout(constant_id = ${ID}) const double sc1 = 9.25LF;", 8, makeValueUint64(0xEFCDAB8967452301ull))),
			16,
			"    double r0;\n"
			"    double r1;\n",
			"",
			"    sb_out.r0 = sc0;\n"
			"    sb_out.r1 = sc1;\n",
			makeVector(OffsetValue(8, 0, makeValueUint64(0xFEDCBA9876543210ull)),
					   OffsetValue(8, 8, makeValueUint64(0xEFCDAB8967452301ull))),
			FEATURE_SHADER_FLOAT_64,
			false,
		},
		{
			"mixed",
			makeVector(
				SpecConstant(1u, "layout(constant_id = ${ID}) const uint8_t  sc0 = uint8_t  (0);", 1, makeValueUint8(0x98)),
				SpecConstant(2u, "layout(constant_id = ${ID}) const uint16_t sc1 = uint16_t (0);", 2, makeValueUint16(0x9876)),
				SpecConstant(3u, "layout(constant_id = ${ID}) const uint     sc2 = uint     (0);", 4, makeValueUint32(0xba987654u)),
				SpecConstant(4u, "layout(constant_id = ${ID}) const uint64_t sc3 = uint64_t (0);", 8, makeValueUint64(0xfedcba9876543210ull))),
			8+4+2+1,
			"    uint64_t r0;\n"
			"    uint     r1;\n"
			"    uint16_t r2;\n"
			"    uint8_t  r3;\n",
			"",
			"    uint64_t i0 = sc3;\n"
			"    uint     i1 = sc2;\n"
			"    uint16_t i2 = sc1;\n"
			"    uint8_t  i3 = sc0;\n"
			"    sb_out.r0 = i0;\n"
			"    sb_out.r1 = i1;\n"
			"    sb_out.r2 = i2;\n"
			"    sb_out.r3 = i3;\n",
			makeVector(
				OffsetValue(8, 0, makeValueUint64(0xfedcba9876543210ull)),
				OffsetValue(4, 8, makeValueUint32(0xba987654u)),
				OffsetValue(2, 12, makeValueUint16(0x9876)),
				OffsetValue(1, 14, makeValueUint8(0x98))),
			(FEATURE_SHADER_INT_8 | FEATURE_SHADER_INT_16 | FEATURE_SHADER_INT_64),
			false,
		},
		{
			"mixed_reversed",
			makeVector(
				SpecConstant(1u, "layout(constant_id = ${ID}) const uint64_t sc3 = uint64_t (0);", 8, makeValueUint64(0xfedcba9876543210ull)),
				SpecConstant(2u, "layout(constant_id = ${ID}) const uint     sc2 = uint     (0);", 4, makeValueUint32(0xba987654u)),
				SpecConstant(3u, "layout(constant_id = ${ID}) const uint16_t sc1 = uint16_t (0);", 2, makeValueUint16(0x9876)),
				SpecConstant(4u, "layout(constant_id = ${ID}) const uint8_t  sc0 = uint8_t  (0);", 1, makeValueUint8(0x98))),
			8+4+2+1,
			"    uint64_t r0;\n"
			"    uint     r1;\n"
			"    uint16_t r2;\n"
			"    uint8_t  r3;\n",
			"",
			"    uint64_t i0 = sc3;\n"
			"    uint     i1 = sc2;\n"
			"    uint16_t i2 = sc1;\n"
			"    uint8_t  i3 = sc0;\n"
			"    sb_out.r0 = i0;\n"
			"    sb_out.r1 = i1;\n"
			"    sb_out.r2 = i2;\n"
			"    sb_out.r3 = i3;\n",
			makeVector(
				OffsetValue(8, 0, makeValueUint64(0xfedcba9876543210ull)),
				OffsetValue(4, 8, makeValueUint32(0xba987654u)),
				OffsetValue(2, 12, makeValueUint16(0x9876)),
				OffsetValue(1, 14, makeValueUint8(0x98))),
			(FEATURE_SHADER_INT_8 | FEATURE_SHADER_INT_16 | FEATURE_SHADER_INT_64),
			false,
		},
	};

	for (int i = 0; i < 2; ++i)
	{
		const bool packData = (i > 0);
		for (int defNdx = 0; defNdx < DE_LENGTH_OF_ARRAY(defs); ++defNdx)
		{
			auto& def = defs[defNdx];
			def.packData = packData;
			if (packData)
				def.name += "_packed";
			testGroup->addChild(new SpecConstantTest(testCtx, pipelineType, shaderStage, def));
		}
	}

	CaseDefinition defsUnusedCases[] =
	{
		{
			"unused_single",
			makeVector(SpecConstant(0u, "", 0u, GenericValue(), true)),
			4,
			"    int r0;\n",
			"",
			"    sb_out.r0 = 77;\n",
			makeVector(OffsetValue(4u, 0u, makeValueInt32(77))),
			(FeatureFlags)0,
			false,
		},
		{
			"unused_single_packed",
			makeVector(SpecConstant(0u, "", 0u, GenericValue(), true),
					   SpecConstant(1u, "layout(constant_id = ${ID}) const int sc1 = 0;", 4u, makeValueInt32(100))),
			4,
			"    int r1;\n",
			"",
			"    sb_out.r1 = sc1;\n",
			makeVector(OffsetValue(4u, 0u, makeValueInt32(100))),
			(FeatureFlags)0,
			true,
		},
		{
			"unused_multiple",
			makeVector(SpecConstant( 7u, "layout(constant_id = ${ID}) const int sc0 = 0;", 4u, makeValueInt32(-999)),
					   SpecConstant( 1u, "", 0u, GenericValue(), true),
					   SpecConstant(17u, "layout(constant_id = ${ID}) const int sc1 = 0;", 4u, makeValueInt32( 999)),
					   SpecConstant( 3u, "", 0u, GenericValue(), true)),
			8,
			"    int r0;\n"
			"    int r1;\n",
			"",
			"    sb_out.r0 = sc0;\n"
			"    sb_out.r1 = sc1;\n",
			makeVector(OffsetValue(4, 0, makeValueInt32(-999)),
					   OffsetValue(4, 4, makeValueInt32( 999))),
			(FeatureFlags)0,
			false,
		},
		{
			"unused_multiple_packed",
			makeVector(SpecConstant( 7u, "layout(constant_id = ${ID}) const int sc0 = 0;", 4u, makeValueInt32(-999)),
					   SpecConstant( 1u, "", 0u, GenericValue(), true),
					   SpecConstant( 3u, "", 0u, GenericValue(), true),
					   SpecConstant(17u, "layout(constant_id = ${ID}) const int sc1 = 0;", 4u, makeValueInt32( 999))),
			8,
			"    int r0;\n"
			"    int r1;\n",
			"",
			"    sb_out.r0 = sc0;\n"
			"    sb_out.r1 = sc1;\n",
			makeVector(OffsetValue(4, 0, makeValueInt32(-999)),
					   OffsetValue(4, 4, makeValueInt32( 999))),
			(FeatureFlags)0,
			true,
		},
	};

	for (const auto& caseDef : defsUnusedCases)
		testGroup->addChild(new SpecConstantTest(testCtx, pipelineType, shaderStage, caseDef));

	return testGroup.release();
}

//! Specify compute shader work group size through specialization constants.
tcu::TestCaseGroup* createWorkGroupSizeTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> testGroup (new tcu::TestCaseGroup(testCtx, "local_size", "work group size specialization"));

	const deUint32 ssboSize = 16;
	const std::string ssboDecl =
		"    uvec3 workGroupSize;\n"
		"    uint  checksum;\n";
	const std::string globalDecl = "shared uint count;\n";
	const std::string mainCode =
		"    count = 0u;\n"
		"\n"
		"    groupMemoryBarrier();\n"
		"    barrier();\n"
		"\n"
		"    atomicAdd(count, 1u);\n"
		"\n"
		"    groupMemoryBarrier();\n"
		"    barrier();\n"
		"\n"
		"    sb_out.workGroupSize = gl_WorkGroupSize;\n"
		"    sb_out.checksum      = count;\n";

	const CaseDefinition defs[] =
	{
		{
			"x",
			makeVector(SpecConstant(1u, "layout(local_size_x_id = ${ID}) in;",  4, makeValueUint32(7u))),
			ssboSize, ssboDecl, globalDecl, mainCode,
			makeVector(OffsetValue(4,  0, makeValueUint32(7u)),
					   OffsetValue(4,  4, makeValueUint32(1u)),
					   OffsetValue(4,  8, makeValueUint32(1u)),
					   OffsetValue(4, 12, makeValueUint32(7u))),
			(FeatureFlags)0,
			false,
		},
		{
			"y",
			makeVector(SpecConstant(1u, "layout(local_size_y_id = ${ID}) in;",  4, makeValueUint32(5u))),
			ssboSize, ssboDecl, globalDecl, mainCode,
			makeVector(OffsetValue(4,  0, makeValueUint32(1u)),
					   OffsetValue(4,  4, makeValueUint32(5u)),
					   OffsetValue(4,  8, makeValueUint32(1u)),
					   OffsetValue(4, 12, makeValueUint32(5u))),
			(FeatureFlags)0,
			false,
		},
		{
			"z",
			makeVector(SpecConstant(1u, "layout(local_size_z_id = ${ID}) in;",  4, makeValueUint32(3u))),
			ssboSize, ssboDecl, globalDecl, mainCode,
			makeVector(OffsetValue(4,  0, makeValueUint32(1u)),
					   OffsetValue(4,  4, makeValueUint32(1u)),
					   OffsetValue(4,  8, makeValueUint32(3u)),
					   OffsetValue(4, 12, makeValueUint32(3u))),
			(FeatureFlags)0,
			false,
		},
		{
			"xy",
			makeVector(SpecConstant(1u, "layout(local_size_x_id = ${ID}) in;",  4, makeValueUint32(6u)),
					   SpecConstant(2u, "layout(local_size_y_id = ${ID}) in;",  4, makeValueUint32(4u))),
			ssboSize, ssboDecl, globalDecl, mainCode,
			makeVector(OffsetValue(4,  0, makeValueUint32(6u)),
					   OffsetValue(4,  4, makeValueUint32(4u)),
					   OffsetValue(4,  8, makeValueUint32(1u)),
					   OffsetValue(4, 12, makeValueUint32(6u * 4u))),
			(FeatureFlags)0,
			false,
		},
		{
			"xz",
			makeVector(SpecConstant(1u, "layout(local_size_x_id = ${ID}) in;",  4, makeValueUint32(3u)),
					   SpecConstant(2u, "layout(local_size_z_id = ${ID}) in;",  4, makeValueUint32(9u))),
			ssboSize, ssboDecl, globalDecl, mainCode,
			makeVector(OffsetValue(4,  0, makeValueUint32(3u)),
					   OffsetValue(4,  4, makeValueUint32(1u)),
					   OffsetValue(4,  8, makeValueUint32(9u)),
					   OffsetValue(4, 12, makeValueUint32(3u * 9u))),
			(FeatureFlags)0,
			false,
		},
		{
			"yz",
			makeVector(SpecConstant(1u, "layout(local_size_y_id = ${ID}) in;",  4, makeValueUint32(2u)),
					   SpecConstant(2u, "layout(local_size_z_id = ${ID}) in;",  4, makeValueUint32(5u))),
			ssboSize, ssboDecl, globalDecl, mainCode,
			makeVector(OffsetValue(4,  0, makeValueUint32(1u)),
					   OffsetValue(4,  4, makeValueUint32(2u)),
					   OffsetValue(4,  8, makeValueUint32(5u)),
					   OffsetValue(4, 12, makeValueUint32(2u * 5u))),
			(FeatureFlags)0,
			false,
		},
		{
			"xyz",
			makeVector(SpecConstant(1u, "layout(local_size_x_id = ${ID}) in;",  4, makeValueUint32(3u)),
					   SpecConstant(2u, "layout(local_size_y_id = ${ID}) in;",  4, makeValueUint32(5u)),
					   SpecConstant(3u, "layout(local_size_z_id = ${ID}) in;",  4, makeValueUint32(7u))),
			ssboSize, ssboDecl, globalDecl, mainCode,
			makeVector(OffsetValue(4,  0, makeValueUint32(3u)),
					   OffsetValue(4,  4, makeValueUint32(5u)),
					   OffsetValue(4,  8, makeValueUint32(7u)),
					   OffsetValue(4, 12, makeValueUint32(3u * 5u * 7u))),
			(FeatureFlags)0,
			false,
		},
	};

	for (int defNdx = 0; defNdx < DE_LENGTH_OF_ARRAY(defs); ++defNdx)
		testGroup->addChild(new SpecConstantTest(testCtx, PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC, VK_SHADER_STAGE_COMPUTE_BIT, defs[defNdx]));

	return testGroup.release();
}

//! Override a built-in variable with specialization constant value.
tcu::TestCaseGroup* createBuiltInOverrideTests (tcu::TestContext& testCtx, const PipelineConstructionType pipelineType, const VkShaderStageFlagBits shaderStage)
{
	de::MovePtr<tcu::TestCaseGroup> testGroup (new tcu::TestCaseGroup(testCtx, "builtin", "built-in override"));

	const CaseDefinition defs[] =
	{
		{
			"default",
			makeVector(SpecConstant(1u, "layout(constant_id = ${ID}) gl_MaxImageUnits;")),
			4,
			"    bool ok;\n",
			"",
			"    sb_out.ok = (gl_MaxImageUnits >= 8);\n",	// implementation defined, 8 is the minimum
			makeVector(OffsetValue(4,  0, makeValueBool32(true))),
			(FeatureFlags)0,
			false,
		},
		{
			"specialized",
			makeVector(SpecConstant(1u, "layout(constant_id = ${ID}) gl_MaxImageUnits;", 4, makeValueInt32(12))),
			4,
			"    int maxImageUnits;\n",
			"",
			"    sb_out.maxImageUnits = gl_MaxImageUnits;\n",
			makeVector(OffsetValue(4,  0, makeValueInt32(12))),
			(FeatureFlags)0,
			false,
		},
	};

	for (int defNdx = 0; defNdx < DE_LENGTH_OF_ARRAY(defs); ++defNdx)
		testGroup->addChild(new SpecConstantTest(testCtx, pipelineType, shaderStage, defs[defNdx]));

	return testGroup.release();
}

//! Specialization constants used in expressions.
tcu::TestCaseGroup* createExpressionTests (tcu::TestContext& testCtx, const PipelineConstructionType pipelineType, const VkShaderStageFlagBits shaderStage)
{
	de::MovePtr<tcu::TestCaseGroup> testGroup (new tcu::TestCaseGroup(testCtx, "expression", "specialization constants usage in expressions"));

	const CaseDefinition defs[] =
	{
		{
			"spec_const_expression",
			makeVector(SpecConstant(1u, "layout(constant_id = ${ID}) const int sc0 = 2;"),
					   SpecConstant(2u, "layout(constant_id = ${ID}) const int sc1 = 3;", 4, makeValueInt32(5))),
			4,
			"    int result;\n",

			"const int expr0 = sc0 + 1;\n"
			"const int expr1 = sc0 + sc1;\n",

			"    sb_out.result = expr0 + expr1;\n",
			makeVector(OffsetValue(4,  0, makeValueInt32(10))),
			(FeatureFlags)0,
			false,
		},
		{
			"array_size",
			makeVector(SpecConstant(1u, "layout(constant_id = ${ID}) const int sc0 = 1;"),
					   SpecConstant(2u, "layout(constant_id = ${ID}) const int sc1 = 2;", 4, makeValueInt32(3))),
			16,
			"    int r0;\n"
			"    int r1[3];\n",

			"",

			"    int a0[sc0];\n"
			"    int a1[sc1];\n"
			"\n"
			"    for (int i = 0; i < sc0; ++i)\n"
			"        a0[i] = sc0 - i;\n"
			"    for (int i = 0; i < sc1; ++i)\n"
			"        a1[i] = sc1 - i;\n"
			"\n"
			"    sb_out.r0 = a0[0];\n"
			"    for (int i = 0; i < sc1; ++i)\n"
			"	     sb_out.r1[i] = a1[i];\n",
			makeVector(OffsetValue(4,  0, makeValueInt32(1)),
					   OffsetValue(4,  4, makeValueInt32(3)),
					   OffsetValue(4,  8, makeValueInt32(2)),
					   OffsetValue(4, 12, makeValueInt32(1))),
			(FeatureFlags)0,
			false,
		},
		{
			"array_size_expression",
			makeVector(SpecConstant(1u, "layout(constant_id = ${ID}) const int sc0 = 3;"),
					   SpecConstant(2u, "layout(constant_id = ${ID}) const int sc1 = 5;", 4, makeValueInt32(7))),
			8,
			"    int r0;\n"
			"    int r1;\n",

			"",

			"    int a0[sc0 + 3];\n"
			"    int a1[sc0 + sc1];\n"
			"\n"
			"    const int size0 = sc0 + 3;\n"
			"    const int size1 = sc0 + sc1;\n"
			"\n"
			"    for (int i = 0; i < size0; ++i)\n"
			"        a0[i] = 3 - i;\n"
			"    for (int i = 0; i < size1; ++i)\n"
			"        a1[i] = 5 - i;\n"
			"\n"
			"    sb_out.r0 = a0[size0 - 1];\n"
			"    sb_out.r1 = a1[size1 - 1];\n",
			makeVector(OffsetValue(4,  0, makeValueInt32(-2)),
					   OffsetValue(4,  4, makeValueInt32(-4))),
			(FeatureFlags)0,
			false,
		},
		{
			"array_size_spec_const_expression",
			makeVector(SpecConstant(1u, "layout(constant_id = ${ID}) const int sc0 = 3;"),
					   SpecConstant(2u, "layout(constant_id = ${ID}) const int sc1 = 5;", 4, makeValueInt32(7))),
			8,
			"    int r0;\n"
			"    int r1;\n",

			"",

			"    const int size0 = sc0 + 3;\n"
			"    const int size1 = sc0 + sc1;\n"
			"\n"
			"    int a0[size0];\n"
			"    int a1[size1];\n"
			"\n"
			"    for (int i = 0; i < size0; ++i)\n"
			"        a0[i] = 3 - i;\n"
			"    for (int i = 0; i < size1; ++i)\n"
			"        a1[i] = 5 - i;\n"
			"\n"
			"    sb_out.r0 = a0[size0 - 1];\n"
			"    sb_out.r1 = a1[size1 - 1];\n",
			makeVector(OffsetValue(4,  0, makeValueInt32(-2)),
					   OffsetValue(4,  4, makeValueInt32(-4))),
			(FeatureFlags)0,
			false,
		},
		{
			"array_size_length",
			makeVector(SpecConstant(1u, "layout(constant_id = ${ID}) const int sc0 = 1;"),
					   SpecConstant(2u, "layout(constant_id = ${ID}) const int sc1 = 2;", 4, makeValueInt32(4))),
			8,
			"    int r0;\n"
			"    int r1;\n",

			"",

			"    int a0[sc0];\n"
			"    int a1[sc1];\n"
			"\n"
			"    sb_out.r0 = a0.length();\n"
			"    sb_out.r1 = a1.length();\n",
			makeVector(OffsetValue(4,  0, makeValueInt32(1)),
					   OffsetValue(4,  4, makeValueInt32(4))),
			(FeatureFlags)0,
			false,
		},
		{
			"array_size_pass_to_function",
			makeVector(SpecConstant(1u, "layout(constant_id = ${ID}) const int sc0 = 3;"),
					   SpecConstant(2u, "layout(constant_id = ${ID}) const int sc1 = 1;", 4, makeValueInt32(3))),
			4,
			"    int result;\n",

			"int sumArrays (int a0[sc0], int a1[sc1])\n"
			"{\n"
			"    int sum = 0;\n"
			"    for (int i = 0; (i < sc0) && (i < sc1); ++i)\n"
			"        sum += a0[i] + a1[i];\n"
			"    return sum;\n"
			"}\n",

			"    int a0[sc0];\n"
			"    int a1[sc1];\n"
			"\n"
			"    for (int i = 0; i < sc0; ++i)\n"
			"        a0[i] = i + 1;\n"
			"    for (int i = 0; i < sc1; ++i)\n"
			"        a1[i] = i + 2;\n"
			"\n"
			"    sb_out.result = sumArrays(a0, a1);\n",
			makeVector(OffsetValue(4,  0, makeValueInt32(15))),
			(FeatureFlags)0,
			false,
		},
	};

	for (int defNdx = 0; defNdx < DE_LENGTH_OF_ARRAY(defs); ++defNdx)
		testGroup->addChild(new SpecConstantTest(testCtx, pipelineType, shaderStage, defs[defNdx]));

	return testGroup.release();
}

//! Helper functions internal to make*CompositeCaseDefinition functions.
namespace composite_case_internal
{

//! Generate a string like this: "1, 2, sc0, 4" or "true, true, sc0"
//! castToType = true is useful when type requires more initializer values than we are providing, e.g.:
//!    vec2(1), vec2(sc0), vec(3)
std::string generateInitializerListWithSpecConstant (const glu::DataType	type,
													 const bool				castToType,
													 const int				idxBegin,
													 const int				idxEnd,
													 const std::string&		specConstName,
													 const int				specConstNdx)
{
	std::ostringstream str;

	for (int i = idxBegin; i < idxEnd; ++i)
	{
		const std::string iVal = (i == specConstNdx ? specConstName : glu::getDataTypeScalarType(type) == glu::TYPE_BOOL ? "true" : de::toString(i + 1));
		str << (i != idxBegin ? ", " : "") << (castToType ? de::toString(glu::getDataTypeName(type)) + "(" + iVal + ")" : iVal);
	}

	return str.str();
}

std::string generateArrayConstructorString (const glu::DataType	elemType,
											const int			size1,
											const int			size2,
											const std::string&	specConstName,
											const int			specConstNdx)
{
	const bool isArrayOfArray = (size2 > 0);
	const bool doCast		  = (!isDataTypeScalar(elemType));

	std::ostringstream arrayCtorExpr;

	if (isArrayOfArray)
	{
		const std::string padding  (36, ' ');
		int               idxBegin = 0;
		int               idxEnd   = size2;

		for (int iterNdx = 0; iterNdx < size1; ++iterNdx)
		{
			// Open sub-array ctor
			arrayCtorExpr << (iterNdx != 0 ? ",\n" + padding : "") << glu::getDataTypeName(elemType) << "[" << size2 << "](";

			// Sub-array constructor elements
			arrayCtorExpr << generateInitializerListWithSpecConstant(elemType, doCast, idxBegin, idxEnd, specConstName, specConstNdx);

			// Close sub-array ctor, move to next range
			arrayCtorExpr << ")";

			idxBegin += size2;
			idxEnd += size2;
		}
	}
	else
	{
		// Array constructor elements
		arrayCtorExpr << generateInitializerListWithSpecConstant(elemType, doCast, 0, size1, specConstName, specConstNdx);
	}

	return arrayCtorExpr.str();
}

inline GenericValue makeValue (const glu::DataType type, const int specValue)
{
	if (type == glu::TYPE_DOUBLE)
		return makeValueFloat64(static_cast<double>(specValue));
	else if (type == glu::TYPE_FLOAT)
		return makeValueFloat32(static_cast<float>(specValue));
	else
		return makeValueInt32(specValue);
}

deUint32 getDataTypeScalarSizeBytes (const glu::DataType dataType)
{
	switch (getDataTypeScalarType(dataType))
	{
		case glu::TYPE_FLOAT:
		case glu::TYPE_INT:
		case glu::TYPE_UINT:
		case glu::TYPE_BOOL:
			return 4;

		case glu::TYPE_DOUBLE:
			return 8;

		default:
			DE_ASSERT(false);
			return 0;
	}
}

//! This applies to matrices/vectors/array cases. dataType must be a basic type.
std::vector<OffsetValue> computeExpectedValues (const int specValue, const glu::DataType dataType, const int numCombinations)
{
	DE_ASSERT(glu::isDataTypeScalar(dataType));

	std::vector<OffsetValue> expectedValues;

	for (int combNdx = 0; combNdx < numCombinations; ++combNdx)
	{
		int sum = 0;
		for (int i = 0; i < numCombinations; ++i)
			sum += (i == combNdx ? specValue : dataType == glu::TYPE_BOOL ? 1 : (i + 1));

		const int dataSize = getDataTypeScalarSizeBytes(dataType);
		expectedValues.push_back(OffsetValue(dataSize, dataSize * combNdx, makeValue(dataType, sum)));
	}

	return expectedValues;
}

inline std::string getFirstDataElementSubscriptString (const glu::DataType type)
{
	// Grab the first element of a matrix/vector, if dealing with non-basic types.
	return (isDataTypeMatrix(type) ? "[0][0]" : isDataTypeVector(type) ? "[0]" : "");
}

//! This code will go into the main function.
std::string generateShaderChecksumComputationCode (const glu::DataType	elemType,
												   const std::string&	varName,
												   const std::string&	accumType,
												   const int			size1,
												   const int			size2,
												   const int			numCombinations)
{
	std::ostringstream mainCode;

	// Generate main code to calculate checksums for each array
	for (int combNdx = 0; combNdx < numCombinations; ++combNdx)
		mainCode << "    "<< accumType << " sum_" << varName << combNdx << " = " << accumType << "(0);\n";

	if (size2 > 0)
	{
		mainCode << "\n"
					<< "    for (int i = 0; i < " << size1 << "; ++i)\n"
					<< "    for (int j = 0; j < " << size2 << "; ++j)\n"
					<< "    {\n";

		for (int combNdx = 0; combNdx < numCombinations; ++combNdx)
			mainCode << "        sum_" << varName << combNdx << " += " << accumType << "("
					 << varName << combNdx << "[i][j]" << getFirstDataElementSubscriptString(elemType) << ");\n";
	}
	else
	{
		mainCode << "\n"
					<< "    for (int i = 0; i < " << size1 << "; ++i)\n"
					<< "    {\n";

		for (int combNdx = 0; combNdx < numCombinations; ++combNdx)
			mainCode << "        sum_" << varName << combNdx << " += " << accumType << "("
					 << varName << combNdx << "[i]" << getFirstDataElementSubscriptString(elemType) << ");\n";
	}

	mainCode << "    }\n"
				<< "\n";

	for (int combNdx = 0; combNdx < numCombinations; ++combNdx)
		mainCode << "    sb_out.result[" << combNdx << "] = sum_" << varName << combNdx << ";\n";

	return mainCode.str();
}

SpecConstant makeSpecConstant (const std::string specConstName, const deUint32 specConstId, const glu::DataType type, const int specValue)
{
	DE_ASSERT(glu::isDataTypeScalar(type));

	const std::string typeName(glu::getDataTypeName(type));

	return SpecConstant(
		specConstId,
		"layout(constant_id = ${ID}) const " + typeName + " " + specConstName + " = " + typeName + "(1);",
		getDataTypeScalarSizeBytes(type), makeValue(type, specValue));
}

} // composite_case_internal ns

//! Generate a CaseDefinition for a composite test using a matrix or vector (a 1-column matrix)
CaseDefinition makeMatrixVectorCompositeCaseDefinition (const glu::DataType type)
{
	using namespace composite_case_internal;

	DE_ASSERT(!glu::isDataTypeScalar(type));

	const std::string   varName         = (glu::isDataTypeMatrix(type) ? "m" : "v");
	const int           numCombinations = getDataTypeScalarSize(type);
	const glu::DataType scalarType      = glu::getDataTypeScalarType(type);
	const std::string   typeName        = glu::getDataTypeName(type);
	const bool			isConst		= (scalarType != glu::TYPE_FLOAT) && (scalarType != glu::TYPE_DOUBLE);

	std::ostringstream globalCode;
	{
		// Build N matrices/vectors with specialization constant inserted at various locations in the constructor.
		for (int combNdx = 0; combNdx < numCombinations; ++combNdx)
			globalCode << ( isConst ? "const " : "" ) << typeName << " " << varName << combNdx << " = " << typeName << "("
					   << generateInitializerListWithSpecConstant(type, false, 0, numCombinations, "sc0", combNdx) << ");\n";
	}

	const bool        isBoolElement = (scalarType == glu::TYPE_BOOL);
	const int         specValue     = (isBoolElement ? 0 : 42);
	const std::string accumType     = glu::getDataTypeName(isBoolElement ? glu::TYPE_INT : scalarType);

	const int size1 = glu::isDataTypeMatrix(type) ? glu::getDataTypeMatrixNumColumns(type) : glu::getDataTypeNumComponents(type);
	const int size2 = glu::isDataTypeMatrix(type) ? glu::getDataTypeMatrixNumRows(type)    : 0;

	const CaseDefinition def =
	{
		typeName,
		makeVector(makeSpecConstant("sc0", 1u, scalarType, specValue)),
		static_cast<VkDeviceSize>(getDataTypeScalarSizeBytes(type) * numCombinations),
		"    " + accumType + " result[" + de::toString(numCombinations) + "];\n",
		globalCode.str(),
		generateShaderChecksumComputationCode(scalarType, varName, accumType, size1, size2, numCombinations),
		computeExpectedValues(specValue, scalarType, numCombinations),
		(scalarType == glu::TYPE_DOUBLE ? (FeatureFlags)FEATURE_SHADER_FLOAT_64 : (FeatureFlags)0),
		false,
	};
	return def;
}

//! Generate a CaseDefinition for a composite test using an array, or an array of array.
//! If (size1, size2) = (N, 0) -> type array[N]
//!                   = (N, M) -> type array[N][M]
CaseDefinition makeArrayCompositeCaseDefinition (const glu::DataType elemType, const int size1, const int size2 = 0)
{
	using namespace composite_case_internal;

	DE_ASSERT(size1 > 0);

	const bool        isArrayOfArray  = (size2 > 0);
	const std::string varName		  = "a";
	const std::string arraySizeDecl   = "[" + de::toString(size1) + "]" + (isArrayOfArray ? "[" + de::toString(size2) + "]" : "");
	const int         numCombinations = (isArrayOfArray ? size1 * size2 : size1);
	const std::string elemTypeName    (glu::getDataTypeName(elemType));

	std::ostringstream globalCode;
	{
		// Create several arrays with specialization constant inserted in different positions.
		for (int combNdx = 0; combNdx < numCombinations; ++combNdx)
			globalCode << elemTypeName << " " << varName << combNdx << arraySizeDecl << " = "
					   << elemTypeName << arraySizeDecl << "(" << generateArrayConstructorString(elemType, size1, size2, "sc0", combNdx) << ");\n";
	}

	const glu::DataType scalarType = glu::getDataTypeScalarType(elemType);
	const bool          isBoolData = (scalarType == glu::TYPE_BOOL);
	const int           specValue  = (isBoolData ? 0 : 19);
	const std::string   caseName   = (isArrayOfArray ? "array_" : "") + elemTypeName;
	const std::string   accumType  = (glu::getDataTypeName(isBoolData ? glu::TYPE_INT : scalarType));

	const CaseDefinition def =
	{
		caseName,
		makeVector(makeSpecConstant("sc0", 1u, scalarType, specValue)),
		static_cast<VkDeviceSize>(getDataTypeScalarSizeBytes(elemType) * numCombinations),
		"    " + accumType + " result[" + de::toString(numCombinations) + "];\n",
		globalCode.str(),
		generateShaderChecksumComputationCode(elemType, varName, accumType, size1, size2, numCombinations),
		computeExpectedValues(specValue, scalarType, numCombinations),
		(scalarType == glu::TYPE_DOUBLE ? (FeatureFlags)FEATURE_SHADER_FLOAT_64 : (FeatureFlags)0),
		false,
	};
	return def;
}

//! A basic struct case, where one member is a specialization constant, or a specialization constant composite
//! (a matrix/vector with a spec. const. element).
CaseDefinition makeStructCompositeCaseDefinition (const glu::DataType memberType)
{
	using namespace composite_case_internal;

	std::ostringstream globalCode;
	{
		globalCode << "struct Data {\n"
				   << "    int   i;\n"
				   << "    float f;\n"
				   << "    bool  b;\n"
				   << "    " << glu::getDataTypeName(memberType) << " sc;\n"
				   << "    uint  ui;\n"
				   << "};\n"
				   << "\n"
				   << "Data s0 = Data(3, 2.0, true, " << glu::getDataTypeName(memberType) << "(sc0), 8u);\n";
	}

	const glu::DataType scalarType   = glu::getDataTypeScalarType(memberType);
	const bool          isBoolData   = (scalarType == glu::TYPE_BOOL);
	const int           specValue    = (isBoolData ? 0 : 23);
	const int           checksum     = (3 + 2 + 1 + specValue + 8);  // matches the shader code
	const glu::DataType accumType    = (isBoolData ? glu::TYPE_INT : scalarType);
	const std::string   accumTypeStr = glu::getDataTypeName(accumType);

	std::ostringstream mainCode;
	{
		mainCode << "    " << accumTypeStr << " sum_s0 = " << accumTypeStr << "(0);\n"
				 << "\n"
				 << "    sum_s0 += " << accumTypeStr << "(s0.i);\n"
				 << "    sum_s0 += " << accumTypeStr << "(s0.f);\n"
				 << "    sum_s0 += " << accumTypeStr << "(s0.b);\n"
				 << "    sum_s0 += " << accumTypeStr << "(s0.sc" << getFirstDataElementSubscriptString(memberType) << ");\n"
				 << "    sum_s0 += " << accumTypeStr << "(s0.ui);\n"
				 << "\n"
				 << "    sb_out.result = sum_s0;\n";
	}

	const std::string caseName = glu::getDataTypeName(memberType);

	const CaseDefinition def =
	{
		caseName,
		makeVector(makeSpecConstant("sc0", 1u, scalarType, specValue)),
		getDataTypeScalarSizeBytes(accumType),
		"    " + accumTypeStr + " result;\n",
		globalCode.str(),
		mainCode.str(),
		makeVector(OffsetValue(getDataTypeScalarSizeBytes(memberType), 0, makeValue(scalarType, checksum))),
		(scalarType == glu::TYPE_DOUBLE ? (FeatureFlags)FEATURE_SHADER_FLOAT_64 : (FeatureFlags)0),
		false,
	};
	return def;
}

//! Specialization constants used in composites.
tcu::TestCaseGroup* createCompositeTests (tcu::TestContext& testCtx, const PipelineConstructionType pipelineType, const VkShaderStageFlagBits shaderStage)
{
	de::MovePtr<tcu::TestCaseGroup> compositeTests (new tcu::TestCaseGroup(testCtx, "composite", "specialization constants usage in composite types"));

	// Vectors
	{
		de::MovePtr<tcu::TestCaseGroup> group (new tcu::TestCaseGroup(testCtx, "vector", ""));

		const glu::DataType types[] =
		{
			glu::TYPE_FLOAT_VEC2,
			glu::TYPE_FLOAT_VEC3,
			glu::TYPE_FLOAT_VEC4,

			glu::TYPE_DOUBLE_VEC2,
			glu::TYPE_DOUBLE_VEC3,
			glu::TYPE_DOUBLE_VEC4,

			glu::TYPE_BOOL_VEC2,
			glu::TYPE_BOOL_VEC3,
			glu::TYPE_BOOL_VEC4,

			glu::TYPE_INT_VEC2,
			glu::TYPE_INT_VEC3,
			glu::TYPE_INT_VEC4,

			glu::TYPE_UINT_VEC2,
			glu::TYPE_UINT_VEC3,
			glu::TYPE_UINT_VEC4,
		};
		for (int typeNdx = 0; typeNdx < DE_LENGTH_OF_ARRAY(types); ++typeNdx)
			group->addChild(new SpecConstantTest(testCtx, pipelineType, shaderStage, makeMatrixVectorCompositeCaseDefinition(types[typeNdx])));

		compositeTests->addChild(group.release());
	}

	// Matrices
	{
		de::MovePtr<tcu::TestCaseGroup> group (new tcu::TestCaseGroup(testCtx, "matrix", ""));

		const glu::DataType types[] =
		{
			glu::TYPE_FLOAT_MAT2,
			glu::TYPE_FLOAT_MAT2X3,
			glu::TYPE_FLOAT_MAT2X4,
			glu::TYPE_FLOAT_MAT3X2,
			glu::TYPE_FLOAT_MAT3,
			glu::TYPE_FLOAT_MAT3X4,
			glu::TYPE_FLOAT_MAT4X2,
			glu::TYPE_FLOAT_MAT4X3,
			glu::TYPE_FLOAT_MAT4,

			glu::TYPE_DOUBLE_MAT2,
			glu::TYPE_DOUBLE_MAT2X3,
			glu::TYPE_DOUBLE_MAT2X4,
			glu::TYPE_DOUBLE_MAT3X2,
			glu::TYPE_DOUBLE_MAT3,
			glu::TYPE_DOUBLE_MAT3X4,
			glu::TYPE_DOUBLE_MAT4X2,
			glu::TYPE_DOUBLE_MAT4X3,
			glu::TYPE_DOUBLE_MAT4,
		};
		for (int typeNdx = 0; typeNdx < DE_LENGTH_OF_ARRAY(types); ++typeNdx)
			group->addChild(new SpecConstantTest(testCtx, pipelineType, shaderStage, makeMatrixVectorCompositeCaseDefinition(types[typeNdx])));

		compositeTests->addChild(group.release());
	}

	const glu::DataType allTypes[] =
	{
		glu::TYPE_FLOAT,
		glu::TYPE_FLOAT_VEC2,
		glu::TYPE_FLOAT_VEC3,
		glu::TYPE_FLOAT_VEC4,
		glu::TYPE_FLOAT_MAT2,
		glu::TYPE_FLOAT_MAT2X3,
		glu::TYPE_FLOAT_MAT2X4,
		glu::TYPE_FLOAT_MAT3X2,
		glu::TYPE_FLOAT_MAT3,
		glu::TYPE_FLOAT_MAT3X4,
		glu::TYPE_FLOAT_MAT4X2,
		glu::TYPE_FLOAT_MAT4X3,
		glu::TYPE_FLOAT_MAT4,

		glu::TYPE_DOUBLE,
		glu::TYPE_DOUBLE_VEC2,
		glu::TYPE_DOUBLE_VEC3,
		glu::TYPE_DOUBLE_VEC4,
		glu::TYPE_DOUBLE_MAT2,
		glu::TYPE_DOUBLE_MAT2X3,
		glu::TYPE_DOUBLE_MAT2X4,
		glu::TYPE_DOUBLE_MAT3X2,
		glu::TYPE_DOUBLE_MAT3,
		glu::TYPE_DOUBLE_MAT3X4,
		glu::TYPE_DOUBLE_MAT4X2,
		glu::TYPE_DOUBLE_MAT4X3,
		glu::TYPE_DOUBLE_MAT4,

		glu::TYPE_INT,
		glu::TYPE_INT_VEC2,
		glu::TYPE_INT_VEC3,
		glu::TYPE_INT_VEC4,

		glu::TYPE_UINT,
		glu::TYPE_UINT_VEC2,
		glu::TYPE_UINT_VEC3,
		glu::TYPE_UINT_VEC4,

		glu::TYPE_BOOL,
		glu::TYPE_BOOL_VEC2,
		glu::TYPE_BOOL_VEC3,
		glu::TYPE_BOOL_VEC4,
	};

	// Array cases
	{
		de::MovePtr<tcu::TestCaseGroup> group (new tcu::TestCaseGroup(testCtx, "array", ""));

		// Array of T
		for (int typeNdx = 0; typeNdx < DE_LENGTH_OF_ARRAY(allTypes); ++typeNdx)
			group->addChild(new SpecConstantTest(testCtx, pipelineType, shaderStage, makeArrayCompositeCaseDefinition(allTypes[typeNdx], 3)));

		// Array of array of T
		for (int typeNdx = 0; typeNdx < DE_LENGTH_OF_ARRAY(allTypes); ++typeNdx)
			group->addChild(new SpecConstantTest(testCtx, pipelineType, shaderStage, makeArrayCompositeCaseDefinition(allTypes[typeNdx], 3, 2)));

		// Special case - array of struct
		{
			const int checksum = (3 + 2 + 1) + (1 + 5 + 1) + (1 + 2 + 0);
			const CaseDefinition def =
			{
				"struct",
				makeVector(SpecConstant(1u, "layout(constant_id = ${ID}) const int   sc0 = 1;",    4, makeValueInt32  (3)),
						   SpecConstant(2u, "layout(constant_id = ${ID}) const float sc1 = 1.0;",  4, makeValueFloat32(5.0f)),
						   SpecConstant(3u, "layout(constant_id = ${ID}) const bool  sc2 = true;", 4, makeValueBool32 (false))),
				4,
				"    int result;\n",

				"struct Data {\n"
				"    int   x;\n"
				"    float y;\n"
				"    bool  z;\n"
				"};\n"
				"\n"
				"Data a0[3] = Data[3](Data(sc0, 2.0, true), Data(1, sc1, true), Data(1, 2.0, sc2));\n",

				"    int sum_a0 = 0;\n"
				"\n"
				"    for (int i = 0; i < 3; ++i)\n"
				"        sum_a0 += int(a0[i].x) + int(a0[i].y) + int(a0[i].z);\n"
				"\n"
				"    sb_out.result = sum_a0;\n",

				makeVector(OffsetValue(4,  0, makeValueInt32(checksum))),
				(FeatureFlags)0,
				false,
			};

			group->addChild(new SpecConstantTest(testCtx, pipelineType, shaderStage, def));
		}

		compositeTests->addChild(group.release());
	}

	// Struct cases
	{
		de::MovePtr<tcu::TestCaseGroup> group (new tcu::TestCaseGroup(testCtx, "struct", ""));

		// Struct with one member being a specialization constant (or spec. const. composite) of a given type
		for (int typeNdx = 0; typeNdx < DE_LENGTH_OF_ARRAY(allTypes); ++typeNdx)
			group->addChild(new SpecConstantTest(testCtx, pipelineType, shaderStage, makeStructCompositeCaseDefinition(allTypes[typeNdx])));

		// Special case - struct with array
		{
			const int checksum = (1 + 2 + 31 + 4 + 0);
			const CaseDefinition def =
			{
				"array",
				makeVector(SpecConstant(1u, "layout(constant_id = ${ID}) const float sc0 = 1.0;",  4, makeValueFloat32(31.0f))),
				4,
				"    float result;\n",

				"struct Data {\n"
				"    int  i;\n"
				"    vec3 sc[3];\n"
				"    bool b;\n"
				"};\n"
				"\n"
				"Data s0 = Data(1, vec3[3](vec3(2.0), vec3(sc0), vec3(4.0)), false);\n",

				"    float sum_s0 = 0;\n"
				"\n"
				"    sum_s0 += float(s0.i);\n"
				"    sum_s0 += float(s0.sc[0][0]);\n"
				"    sum_s0 += float(s0.sc[1][0]);\n"
				"    sum_s0 += float(s0.sc[2][0]);\n"
				"    sum_s0 += float(s0.b);\n"
				"\n"
				"    sb_out.result = sum_s0;\n",

				makeVector(OffsetValue(4,  0, makeValueFloat32(static_cast<float>(checksum)))),
				(FeatureFlags)0,
				false,
			};

			group->addChild(new SpecConstantTest(testCtx, pipelineType, shaderStage, def));
		}

		// Special case - struct of struct
		{
			const int checksum = (1 + 2 + 11 + 4 + 1);
			const CaseDefinition def =
			{
				"struct",
				makeVector(SpecConstant(1u, "layout(constant_id = ${ID}) const int sc0 = 1;",  4, makeValueInt32(11))),
				4,
				"    int result;\n",

				"struct Nested {\n"
				"    vec2  v;\n"
				"    int   sc;\n"
				"    float f;\n"
				"};\n"
				"\n"
				"struct Data {\n"
				"    uint   ui;\n"
				"    Nested s;\n"
				"    bool   b;\n"
				"};\n"
				"\n"
				"Data s0 = Data(1u, Nested(vec2(2.0), sc0, 4.0), true);\n",

				"    int sum_s0 = 0;\n"
				"\n"
				"    sum_s0 += int(s0.ui);\n"
				"    sum_s0 += int(s0.s.v[0]);\n"
				"    sum_s0 += int(s0.s.sc);\n"
				"    sum_s0 += int(s0.s.f);\n"
				"    sum_s0 += int(s0.b);\n"
				"\n"
				"    sb_out.result = sum_s0;\n",

				makeVector(OffsetValue(4,  0, makeValueInt32(checksum))),
				(FeatureFlags)0,
				false,
			};

			group->addChild(new SpecConstantTest(testCtx, pipelineType, shaderStage, def));
		}

		compositeTests->addChild(group.release());
	}

	return compositeTests.release();
}

} // anonymous ns

tcu::TestCaseGroup* createSpecConstantTests (tcu::TestContext& testCtx, PipelineConstructionType pipelineType)
{
	de::MovePtr<tcu::TestCaseGroup> allTests (new tcu::TestCaseGroup(testCtx, "spec_constant", "Specialization constants tests"));
	de::MovePtr<tcu::TestCaseGroup> graphicsGroup (new tcu::TestCaseGroup(testCtx, "graphics", ""));

	struct StageDef
	{
		tcu::TestCaseGroup*		parentGroup;
		const char*				name;
		VkShaderStageFlagBits	stage;
	};

	const StageDef stages[] =
	{
		{ graphicsGroup.get(),	"vertex",		VK_SHADER_STAGE_VERTEX_BIT					},
		{ graphicsGroup.get(),	"fragment",		VK_SHADER_STAGE_FRAGMENT_BIT				},
		{ graphicsGroup.get(),	"tess_control",	VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT	},
		{ graphicsGroup.get(),	"tess_eval",	VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT	},
		{ graphicsGroup.get(),	"geometry",		VK_SHADER_STAGE_GEOMETRY_BIT				},
		{ allTests.get(),		"compute",		VK_SHADER_STAGE_COMPUTE_BIT					},
	};

	allTests->addChild(graphicsGroup.release());

	for (int stageNdx = 0; stageNdx < DE_LENGTH_OF_ARRAY(stages); ++stageNdx)
	{
		const StageDef&		stage		= stages[stageNdx];
		const bool			isCompute	= (stage.stage == VK_SHADER_STAGE_COMPUTE_BIT);

		if (isCompute && (pipelineType != PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC))
			continue;

		de::MovePtr<tcu::TestCaseGroup> stageGroup (new tcu::TestCaseGroup(testCtx, stage.name, ""));

		stageGroup->addChild(createDefaultValueTests		(testCtx, pipelineType, stage.stage));
		stageGroup->addChild(createBasicSpecializationTests	(testCtx, pipelineType, stage.stage));
		stageGroup->addChild(createBuiltInOverrideTests		(testCtx, pipelineType, stage.stage));
		stageGroup->addChild(createExpressionTests			(testCtx, pipelineType, stage.stage));
		stageGroup->addChild(createCompositeTests			(testCtx, pipelineType, stage.stage));

		if (isCompute)
			stageGroup->addChild(createWorkGroupSizeTests(testCtx));

		stage.parentGroup->addChild(stageGroup.release());
	}

	return allTests.release();
}

} // pipeline
} // vkt
