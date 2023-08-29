/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
 * Copyright (c) 2018-2019 NVIDIA Corporation
 * Copyright (c) 2023 LunarG, Inc.
 * Copyright (c) 2023 Nintendo
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
 * \brief Vulkan Cooperative Matrix tests
 *//*--------------------------------------------------------------------*/

#include "vktComputeCooperativeMatrixTests.hpp"

#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkQueryUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkObjUtil.hpp"

#include "vktTestGroupUtil.hpp"
#include "vktTestCase.hpp"

#include "deDefs.h"
#include "deFloat16.h"
#include "deMath.h"
#include "deRandom.h"
#include "deSharedPtr.hpp"
#include "deString.h"

#include "tcuTestCase.hpp"
#include "tcuTestLog.hpp"

#include <string>
#include <sstream>
#include <set>
#include <algorithm>

namespace vkt
{
namespace compute
{
namespace
{
using namespace vk;
using namespace std;

//#define COOPERATIVE_MATRIX_EXTENDED_DEBUG 1

DE_STATIC_ASSERT((uint32_t)VK_COMPONENT_TYPE_FLOAT16_KHR == (uint32_t)VK_COMPONENT_TYPE_FLOAT16_NV);
DE_STATIC_ASSERT((uint32_t)VK_COMPONENT_TYPE_FLOAT32_KHR == (uint32_t)VK_COMPONENT_TYPE_FLOAT32_NV);
DE_STATIC_ASSERT((uint32_t)VK_COMPONENT_TYPE_FLOAT64_KHR == (uint32_t)VK_COMPONENT_TYPE_FLOAT64_NV);
DE_STATIC_ASSERT((uint32_t)VK_COMPONENT_TYPE_SINT8_KHR   == (uint32_t)VK_COMPONENT_TYPE_SINT8_NV  );
DE_STATIC_ASSERT((uint32_t)VK_COMPONENT_TYPE_SINT16_KHR  == (uint32_t)VK_COMPONENT_TYPE_SINT16_NV );
DE_STATIC_ASSERT((uint32_t)VK_COMPONENT_TYPE_SINT32_KHR  == (uint32_t)VK_COMPONENT_TYPE_SINT32_NV );
DE_STATIC_ASSERT((uint32_t)VK_COMPONENT_TYPE_SINT64_KHR  == (uint32_t)VK_COMPONENT_TYPE_SINT64_NV );
DE_STATIC_ASSERT((uint32_t)VK_COMPONENT_TYPE_UINT8_KHR   == (uint32_t)VK_COMPONENT_TYPE_UINT8_NV  );
DE_STATIC_ASSERT((uint32_t)VK_COMPONENT_TYPE_UINT16_KHR  == (uint32_t)VK_COMPONENT_TYPE_UINT16_NV );
DE_STATIC_ASSERT((uint32_t)VK_COMPONENT_TYPE_UINT32_KHR  == (uint32_t)VK_COMPONENT_TYPE_UINT32_NV );
DE_STATIC_ASSERT((uint32_t)VK_COMPONENT_TYPE_UINT64_KHR  == (uint32_t)VK_COMPONENT_TYPE_UINT64_NV );

DE_STATIC_ASSERT((uint32_t)VK_SCOPE_DEVICE_KHR       == (uint32_t)VK_SCOPE_DEVICE_NV);
DE_STATIC_ASSERT((uint32_t)VK_SCOPE_WORKGROUP_KHR    == (uint32_t)VK_SCOPE_WORKGROUP_NV);
DE_STATIC_ASSERT((uint32_t)VK_SCOPE_SUBGROUP_KHR     == (uint32_t)VK_SCOPE_SUBGROUP_NV);
DE_STATIC_ASSERT((uint32_t)VK_SCOPE_QUEUE_FAMILY_KHR == (uint32_t)VK_SCOPE_QUEUE_FAMILY_NV);

typedef enum
{
	UT_NV = 0,
	UT_KHR_A,
	UT_KHR_B,
	UT_KHR_Result,
} UseType;

typedef enum
{
	TT_LENGTH = 0,
	TT_CONSTANT,
	TT_CONVERT,
	TT_COMPOSITE,
	TT_COMPOSITE_RVALUE,
	TT_ADD,
	TT_SUB,
	TT_DIV,
	TT_MUL,
	TT_NEGATE,
	TT_MATRIXTIMESSCALAR,
	TT_FUNC,
	TT_MATRIXMULADD,
	TT_COMPOSITE_ARRAY,
	TT_MATRIXMULADD_ARRAY,
	TT_MATRIXMULADD_SATURATED,
	TT_MATRIXMULADD_WRAPPING,
	TT_MATRIXMULADD_STRIDE0,
} TestType;

typedef enum
{
	SC_BUFFER = 0,
	SC_WORKGROUP,
	SC_WORKGROUP_VARIABLE_POINTERS,
	SC_BUFFER_VARIABLE_POINTERS,
	SC_PHYSICAL_STORAGE_BUFFER,
} StorageClass;

enum SubgroupSizeMode
{
	SUBGROUP_SIZE_NONE = 0,
	SUBGROUP_SIZE_MIN = 1,
	SUBGROUP_SIZE_MAX = 2,
};

const VkFlags allShaderStages = VK_SHADER_STAGE_COMPUTE_BIT;

struct CaseDef
{
	TestType							testType;
	deUint32							subgroupsPerWorkgroupX;
	deUint32							subgroupsPerWorkgroupY;
	deUint32							workgroupsX;
	deUint32							workgroupsY;
	VkComponentTypeKHR					inputType;
	VkComponentTypeKHR					outputType;
	bool								colMajor;
	StorageClass						storageClass;
	UseType								useType;
	SubgroupSizeMode					subgroupSizeMode;
	vk::ComputePipelineConstructionType	computePipelineConstructionType;
};

bool isKhr (UseType useType)
{
	return useType != UT_NV;
}

bool isMatrixMulAddOp (TestType testType)
{
	return testType == TT_MATRIXMULADD || testType == TT_MATRIXMULADD_ARRAY || testType == TT_MATRIXMULADD_SATURATED || testType == TT_MATRIXMULADD_WRAPPING || testType == TT_MATRIXMULADD_STRIDE0;
}

template<typename T>
VkResult getCooperativeMatrixProperties (const InstanceInterface&, VkPhysicalDevice, uint32_t*, T*)
{
	TCU_THROW(InternalError, "Not Implementetd");
}

VkResult getCooperativeMatrixProperties (const InstanceInterface& vki, VkPhysicalDevice physicalDevice, uint32_t* pPropertyCount, VkCooperativeMatrixPropertiesKHR* pProperties)
{
	return vki.getPhysicalDeviceCooperativeMatrixPropertiesKHR(physicalDevice, pPropertyCount, pProperties);
}

VkResult getCooperativeMatrixProperties (const InstanceInterface& vki, VkPhysicalDevice physicalDevice, uint32_t* pPropertyCount, VkCooperativeMatrixPropertiesNV* pProperties)
{
	return vki.getPhysicalDeviceCooperativeMatrixPropertiesNV(physicalDevice, pPropertyCount, pProperties);
}

VkCooperativeMatrixPropertiesKHR convertCooperativeMatrixProperties (const VkCooperativeMatrixPropertiesNV& properties)
{
	VkCooperativeMatrixPropertiesKHR result = initVulkanStructure();

	result.sType					= (VkStructureType)		properties.sType;
	result.pNext					= (void*)				properties.pNext;
	result.MSize					= (uint32_t)			properties.MSize;
	result.NSize					= (uint32_t)			properties.NSize;
	result.KSize					= (uint32_t)			properties.KSize;
	result.AType					= (VkComponentTypeKHR)	properties.AType;
	result.BType					= (VkComponentTypeKHR)	properties.BType;
	result.CType					= (VkComponentTypeKHR)	properties.CType;
	result.ResultType				= (VkComponentTypeKHR)	properties.DType;
	result.saturatingAccumulation	= (VkBool32)			VK_FALSE;
	result.scope					= (VkScopeKHR)			properties.scope;

	return result;
}

std::vector<VkCooperativeMatrixPropertiesKHR> convertCooperativeMatrixProperties (const std::vector <VkCooperativeMatrixPropertiesNV>& properties)
{
	std::vector<VkCooperativeMatrixPropertiesKHR> result(properties.size());

	for (size_t i = 0; i < properties.size(); ++i)
		result[i] = convertCooperativeMatrixProperties(properties[i]);

	return result;
}

template<typename T>
void getCooperativeMatrixPropertiesAll (Context& context, std::vector<T>& properties)
{
	deUint32	propertyCount	= 0;

	VK_CHECK(getCooperativeMatrixProperties(context.getInstanceInterface(), context.getPhysicalDevice(), &propertyCount, (T*)DE_NULL));

	if (propertyCount > 0)
	{
		const T sample = initVulkanStructureConst();

		properties.resize(propertyCount, sample);

		VK_CHECK(getCooperativeMatrixProperties(context.getInstanceInterface(), context.getPhysicalDevice(), &propertyCount, properties.data()));
	}
	else
	{
		properties.clear();
	}
}

std::vector<VkCooperativeMatrixPropertiesKHR> getCooperativeMatrixPropertiesConverted (Context& context, const bool khr)
{
	std::vector<VkCooperativeMatrixPropertiesKHR> properties;

	if (khr)
	{
		getCooperativeMatrixPropertiesAll(context, properties);
	}
	else
	{
		std::vector<VkCooperativeMatrixPropertiesNV> propertiesNV;

		getCooperativeMatrixPropertiesAll(context, propertiesNV);

		properties = convertCooperativeMatrixProperties(propertiesNV);
	}

	return properties;
}

deUint32 getSubgroupSizeFromMode (Context&					context,
								  const SubgroupSizeMode	subgroupSizeMode)
{
#ifndef CTS_USES_VULKANSC
	const VkPhysicalDeviceSubgroupSizeControlProperties&	subgroupSizeControlProperties = context.getSubgroupSizeControlProperties();
#else
	const VkPhysicalDeviceSubgroupSizeControlPropertiesEXT&	subgroupSizeControlProperties = context.getSubgroupSizeControlPropertiesEXT();
#endif // CTS_USES_VULKANSC

	switch (subgroupSizeMode)
	{
		case SUBGROUP_SIZE_MAX:		return subgroupSizeControlProperties.maxSubgroupSize;
		case SUBGROUP_SIZE_MIN:		return subgroupSizeControlProperties.minSubgroupSize;
		case SUBGROUP_SIZE_NONE:	return context.getSubgroupProperties().subgroupSize;
		default:					TCU_THROW(NotSupportedError, "Unsupported Subgroup size");
	}
}


class CooperativeMatrixTestInstance : public TestInstance
{
public:
						CooperativeMatrixTestInstance	(Context& context, const CaseDef& data);
						~CooperativeMatrixTestInstance	(void);
	tcu::TestStatus		iterate							(void);
private:
	CaseDef			m_data;
};

CooperativeMatrixTestInstance::CooperativeMatrixTestInstance (Context& context, const CaseDef& data)
	: vkt::TestInstance		(context)
	, m_data				(data)
{
}

CooperativeMatrixTestInstance::~CooperativeMatrixTestInstance (void)
{
}

class CooperativeMatrixTestCase : public TestCase
{
	public:
								CooperativeMatrixTestCase		(tcu::TestContext& context, const char* name, const char* desc, const CaseDef data);
								~CooperativeMatrixTestCase	(void);
	virtual	void				initPrograms		(SourceCollections& programCollection) const;
	virtual TestInstance*		createInstance		(Context& context) const;
	virtual void				checkSupport		(Context& context) const;

private:
	CaseDef					m_data;
};

CooperativeMatrixTestCase::CooperativeMatrixTestCase (tcu::TestContext& context, const char* name, const char* desc, const CaseDef data)
	: vkt::TestCase	(context, name, desc)
	, m_data		(data)
{
}

CooperativeMatrixTestCase::~CooperativeMatrixTestCase (void)
{
}

void CooperativeMatrixTestCase::checkSupport (Context& context) const
{
	if (!context.contextSupports(vk::ApiVersion(0, 1, 1, 0)))
	{
		TCU_THROW(NotSupportedError, "Vulkan 1.1 not supported");
	}

	if (isKhr(m_data.useType))
	{
		if (!context.getCooperativeMatrixFeatures().cooperativeMatrix)
		{
			TCU_THROW(NotSupportedError, "VkPhysicalDeviceCooperativeMatrixFeaturesKHR::cooperativeMatrix not supported");
		}
	}
	else
	{
		if (!context.getCooperativeMatrixFeaturesNV().cooperativeMatrix)
		{
			TCU_THROW(NotSupportedError, "VkPhysicalDeviceCooperativeMatrixFeaturesNV::cooperativeMatrix not supported");
		}
	}

	if (!context.getVulkanMemoryModelFeatures().vulkanMemoryModel)
	{
		TCU_THROW(NotSupportedError, "vulkanMemoryModel not supported");
	}

	if ((m_data.storageClass == SC_WORKGROUP_VARIABLE_POINTERS || m_data.storageClass == SC_BUFFER_VARIABLE_POINTERS) &&
		!context.getVariablePointersFeatures().variablePointers)
	{
		TCU_THROW(NotSupportedError, "variable pointers not supported");
	}

	if (m_data.storageClass == SC_PHYSICAL_STORAGE_BUFFER && !context.isBufferDeviceAddressSupported())
	{
		TCU_THROW(NotSupportedError, "buffer device address not supported");
	}

	if (!context.getShaderFloat16Int8Features().shaderFloat16 &&
		(m_data.inputType == VK_COMPONENT_TYPE_FLOAT16_KHR || m_data.outputType == VK_COMPONENT_TYPE_FLOAT16_KHR))
	{
		TCU_THROW(NotSupportedError, "shaderFloat16 not supported");
	}

	std::vector<VkCooperativeMatrixPropertiesKHR>	properties		= getCooperativeMatrixPropertiesConverted(context, isKhr(m_data.useType));
	bool											supported[2]	= { false, false };

	for (size_t i = 0; i < properties.size(); ++i)
	{
		const VkCooperativeMatrixPropertiesKHR*	p	= &properties[i];

		if (p->scope != VK_SCOPE_SUBGROUP_KHR)
			continue;

		if (m_data.testType == TT_MATRIXMULADD_SATURATED && p->saturatingAccumulation == DE_FALSE)
			continue;

		if (isMatrixMulAddOp(m_data.testType))
		{
			if (p->AType == m_data.inputType &&
				p->BType == m_data.inputType &&
				p->CType == m_data.outputType &&
				p->ResultType == m_data.outputType)
			{
				supported[0] = supported[1] = true;
			}
		}
		else
		{
			const VkComponentTypeKHR types[2] = { m_data.inputType, m_data.outputType };

			for (deUint32 j = 0; j < 2; ++j)
			{
				switch (m_data.useType)
				{
					case UT_NV:
					{
						if (p->AType == types[j] || p->BType == types[j] || p->CType == types[j] || p->ResultType == types[j])
							supported[j] = true;

						break;
					}
					case UT_KHR_A:
					{
						if (p->AType == types[j])
							supported[j] = true;

						break;
					}
					case UT_KHR_B:
					{
						if (p->BType == types[j])
							supported[j] = true;

						break;
					}
					case UT_KHR_Result:
					{
						if (p->ResultType == types[j])
							supported[j] = true;

						break;
					}
					default:
						TCU_THROW(InternalError, "Unsupported use type");
				}
			}
		}
	}

	if (!supported[0] || !supported[1])
		TCU_THROW(NotSupportedError, "cooperative matrix combination not supported");

	checkShaderObjectRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), m_data.computePipelineConstructionType);
}

struct {
	const char *typeName;
	const char *coopmatTypeName;
	deUint32 bits;
	bool isSigned;
} componentTypeInfo[] =
{
	{ "float16_t",	"fcoopmatNV",	16, true },
	{ "float32_t",	"fcoopmatNV",	32, true },
	{ "float64_t",	"fcoopmatNV",	64, true },
	{ "int8_t",		"icoopmatNV",	8, true },
	{ "int16_t",	"icoopmatNV",	16, true },
	{ "int32_t",	"icoopmatNV",	32, true },
	{ "int64_t",	"icoopmatNV",	64, true },
	{ "uint8_t",	"ucoopmatNV",	8, false },
	{ "uint16_t",	"ucoopmatNV",	16, false },
	{ "uint32_t",	"ucoopmatNV",	32, false },
	{ "uint64_t",	"ucoopmatNV",	64, false },
};

bool isFloatType (VkComponentTypeKHR t)
{
	switch (t)
	{
		case VK_COMPONENT_TYPE_FLOAT16_KHR:
		case VK_COMPONENT_TYPE_FLOAT32_KHR:
		case VK_COMPONENT_TYPE_FLOAT64_KHR:
			return true;
		default:
			return false;
	}
}

bool isSIntType (VkComponentTypeKHR t)
{
	switch (t)
	{
		case VK_COMPONENT_TYPE_SINT8_KHR:
		case VK_COMPONENT_TYPE_SINT16_KHR:
		case VK_COMPONENT_TYPE_SINT32_KHR:
		case VK_COMPONENT_TYPE_SINT64_KHR:
			return true;
		default:
			return false;
	}
}

void CooperativeMatrixTestCase::initPrograms (SourceCollections& programCollection) const
{
	const char*			suffix	= (isKhr(m_data.useType) ? "" : "NV");
	const char*			ext		= isKhr(m_data.useType)
								? "#extension GL_KHR_cooperative_matrix : enable\n"
								: "#extension GL_NV_cooperative_matrix : enable\n"
								  "#extension GL_NV_integer_cooperative_matrix : enable\n";
	const char*			sat		= (m_data.testType == TT_MATRIXMULADD_SATURATED) ? ", gl_MatrixOperandsSaturatingAccumulation" : "";
	std::stringstream	css;
	css << "#version 450 core\n";
	css << "#pragma use_vulkan_memory_model\n";
	css <<
		"#extension GL_KHR_shader_subgroup_basic : enable\n"
		"#extension GL_KHR_memory_scope_semantics : enable\n"
		<< ext <<
		"#extension GL_EXT_shader_explicit_arithmetic_types : enable\n"
		"#extension GL_EXT_buffer_reference : enable\n"
		"// strides overriden by spec constants\n"
		"layout(constant_id = 2) const int AStride = 1;\n"
		"layout(constant_id = 3) const int BStride = 1;\n"
		"layout(constant_id = 4) const int CStride = 1;\n"
		"layout(constant_id = 5) const int OStride = 1;\n"
		"layout(constant_id = 6) const int M = 1;\n"
		"layout(constant_id = 7) const int N = 1;\n"
		"layout(constant_id = 8) const int K = 1;\n"
		"layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z = 1) in;\n";

	if (m_data.storageClass == SC_BUFFER_VARIABLE_POINTERS || m_data.storageClass == SC_WORKGROUP_VARIABLE_POINTERS)
		css << "#pragma use_variable_pointers\n";

	struct
	{
		string rows, cols;
	} dims[4];

	if (isMatrixMulAddOp(m_data.testType))
	{
		dims[0].rows = "M";
		dims[0].cols = "K";
		dims[1].rows = "K";
		dims[1].cols = "N";
		dims[2].rows = "M";
		dims[2].cols = "N";
		dims[3].rows = "M";
		dims[3].cols = "N";
	}
	else
	{
		dims[0].rows = "M";
		dims[0].cols = "N";
		dims[1].rows = "M";
		dims[1].cols = "N";
		dims[2].rows = "M";
		dims[2].cols = "N";
		dims[3].rows = "M";
		dims[3].cols = "N";
	}

	const char *typeStrA = componentTypeInfo[m_data.inputType].typeName;
	const char *typeStrB = componentTypeInfo[m_data.inputType].typeName;
	const char *typeStrC = componentTypeInfo[m_data.outputType].typeName;
	const char *typeStrO = componentTypeInfo[m_data.outputType].typeName;

	css << "const int workgroupsX = " << m_data.workgroupsX << ";\n";
	css << "const uvec2 subgroupsPerWG = uvec2(" << m_data.subgroupsPerWorkgroupX << ", " << m_data.subgroupsPerWorkgroupY << ");\n";

	if (m_data.storageClass == SC_PHYSICAL_STORAGE_BUFFER)
	{
		css << "layout(buffer_reference) buffer InputA { " << typeStrA << " x[]; };\n";
		css << "layout(buffer_reference) buffer InputB { " << typeStrB << " x[]; };\n";
		css << "layout(buffer_reference) buffer InputC { " << typeStrC << " x[]; };\n";
		css << "layout(buffer_reference) buffer Output { " << typeStrO << " x[]; };\n";
		css << "layout(set=0, binding=4) buffer Params { InputA inputA; InputB inputB; InputC inputC; Output outputO; } params;\n";
	}
	else
	{
		css << "layout(set=0, binding=0) coherent buffer InputA { " << typeStrA << " x[]; } inputA;\n";
		css << "layout(set=0, binding=1) coherent buffer InputB { " << typeStrB << " x[]; } inputB;\n";
		css << "layout(set=0, binding=2) coherent buffer InputC { " << typeStrC << " x[]; } inputC;\n";
		css << "layout(set=0, binding=3) coherent buffer Output { " << typeStrO << " x[]; } outputO;\n";
	}

	if (m_data.storageClass == SC_WORKGROUP || m_data.storageClass == SC_WORKGROUP_VARIABLE_POINTERS)
	{
		css << "shared " << typeStrA << " sharedA[" << dims[0].rows << " * " << dims[0].cols << " * subgroupsPerWG.x * subgroupsPerWG.y];\n";
		css << "shared " << typeStrB << " sharedB[" << dims[1].rows << " * " << dims[1].cols << " * subgroupsPerWG.x * subgroupsPerWG.y];\n";
		css << "shared " << typeStrC << " sharedC[" << dims[2].rows << " * " << dims[2].cols << " * subgroupsPerWG.x * subgroupsPerWG.y];\n";
		css << "shared " << typeStrO << " sharedO[" << dims[3].rows << " * " << dims[3].cols << " * subgroupsPerWG.x * subgroupsPerWG.y];\n";
	}

	std::stringstream matAType, matBType, matCType, outputMatType;

	if (isKhr(m_data.useType))
	{
		const bool	useSame		= !isMatrixMulAddOp(m_data.testType);
		const char*	sameType	= m_data.useType == UT_KHR_A ? "gl_MatrixUseA"
								: m_data.useType == UT_KHR_B ? "gl_MatrixUseB"
								: m_data.useType == UT_KHR_Result ? "gl_MatrixUseAccumulator"
								: "Invalid use";
		const char*	atype		= useSame ? sameType : "gl_MatrixUseA";
		const char*	btype		= useSame ? sameType : "gl_MatrixUseB";
		const char*	ctype		= useSame ? sameType : "gl_MatrixUseAccumulator";
		const char*	rtype		= useSame ? sameType : "gl_MatrixUseAccumulator";

		matAType << "coopmat<" << componentTypeInfo[m_data.inputType].typeName << ", gl_ScopeSubgroup, " << dims[0].rows << ", " << dims[0].cols << ", " << atype << ">";
		matBType << "coopmat<" << componentTypeInfo[m_data.inputType].typeName << ", gl_ScopeSubgroup, " << dims[1].rows << ", " << dims[1].cols << ", " << btype << ">";
		matCType << "coopmat<" << componentTypeInfo[m_data.outputType].typeName << ", gl_ScopeSubgroup, " << dims[2].rows << ", " << dims[2].cols << ", " << ctype << ">";
		outputMatType << "coopmat<" << componentTypeInfo[m_data.outputType].typeName << ", gl_ScopeSubgroup, " << dims[3].rows << ", " << dims[3].cols << ", " << rtype << ">";
	}
	else
	{
		matAType << componentTypeInfo[m_data.inputType].coopmatTypeName << "<" << componentTypeInfo[m_data.inputType].bits << ", gl_ScopeSubgroup, " << dims[0].rows << ", " << dims[0].cols << ">";
		matBType << componentTypeInfo[m_data.inputType].coopmatTypeName << "<" << componentTypeInfo[m_data.inputType].bits << ", gl_ScopeSubgroup, " << dims[1].rows << ", " << dims[1].cols << ">";
		matCType << componentTypeInfo[m_data.outputType].coopmatTypeName << "<" << componentTypeInfo[m_data.outputType].bits << ", gl_ScopeSubgroup, " << dims[2].rows << ", " << dims[2].cols << ">";
		outputMatType << componentTypeInfo[m_data.outputType].coopmatTypeName << "<" << componentTypeInfo[m_data.outputType].bits << ", gl_ScopeSubgroup, " << dims[3].rows << ", " << dims[3].cols << ">";
	}

	css << matAType.str() << " matA;\n";
	css << matBType.str() << " matB;\n";
	css << matCType.str() << " matC;\n";
	css << outputMatType.str() << " matO;\n";

	if (m_data.testType == TT_CONSTANT)
		css << "const " << outputMatType.str() << " matConst = " << outputMatType.str() << "(1.0);\n";

	if (m_data.testType == TT_FUNC)
		css << matAType.str() << " f(" << matAType.str() << " m) { return -m; }\n";

	css <<
		"void main()\n"
		"{\n"
		// matrixID is the x,y index of the matrix owned by this subgroup.
		"   uvec2 subgroupXY = uvec2(gl_SubgroupID % subgroupsPerWG.x, gl_SubgroupID / subgroupsPerWG.x);\n"
		"   uvec2 matrixID = uvec2(gl_WorkGroupID.xy) * subgroupsPerWG + subgroupXY;\n";

	if (m_data.storageClass == SC_PHYSICAL_STORAGE_BUFFER)
	{
		css << "   InputA inputA = params.inputA;\n";
		css << "   InputB inputB = params.inputB;\n";
		css << "   InputC inputC = params.inputC;\n";
		css << "   Output outputO = params.outputO;\n";
	}

	string strides[4];
	for (deUint32 i = 0; i < 4; ++i)
	{
		strides[i] = (m_data.colMajor ? dims[i].rows : dims[i].cols) + string(" * ") + de::toString(m_data.subgroupsPerWorkgroupX * m_data.workgroupsX);
	}

	// element<i> is the starting element in buffer memory.
	// elementS<i> is the starting element in shared memory.
	css << "   uint element0 = " << strides[0] << " * " << (m_data.colMajor ? dims[0].cols : dims[0].rows) << " * matrixID.y + " << (m_data.colMajor ? dims[0].rows : dims[0].cols) << " * matrixID.x;\n"
		   "   uint element1 = " << strides[1] << " * " << (m_data.colMajor ? dims[1].cols : dims[1].rows) << " * matrixID.y + " << (m_data.colMajor ? dims[1].rows : dims[1].cols) << " * matrixID.x;\n"
		   "   uint element2 = " << strides[2] << " * " << (m_data.colMajor ? dims[2].cols : dims[2].rows) << " * matrixID.y + " << (m_data.colMajor ? dims[2].rows : dims[2].cols) << " * matrixID.x;\n"
		   "   uint element3 = " << strides[3] << " * " << (m_data.colMajor ? dims[3].cols : dims[3].rows) << " * matrixID.y + " << (m_data.colMajor ? dims[3].rows : dims[3].cols) << " * matrixID.x;\n"
		   "   uint elementS0, elementS1, elementS2, elementS3;\n";

	// For shared memory tests, copy the matrix from buffer memory into
	// workgroup memory. For simplicity, do it all on a single thread.
	if (m_data.storageClass == SC_WORKGROUP || m_data.storageClass == SC_WORKGROUP_VARIABLE_POINTERS)
	{
		const char *name[] =
		{
			"sharedA",
			"sharedB",
			"sharedC",
		};
		const char *inputName[] =
		{
			"inputA",
			"inputB",
			"inputC",
		};
		for (deUint32 m = 0; m < 4; ++m)
		{
			string sharedStride = strides[m] + " / workgroupsX";
			css << "       elementS" << m << " = " << sharedStride << " * " << (m_data.colMajor ? dims[m].cols : dims[m].rows) << " * subgroupXY.y + " << (m_data.colMajor ? dims[m].rows : dims[m].cols) << " * subgroupXY.x;\n";
		}
		css << "   if (subgroupElect()) {\n";
		// copy all three input buffers.
		for (deUint32 m = 0; m < 3; ++m)
		{
			string sharedStride = strides[m] + " / workgroupsX";
			css <<  "       for (int i = 0; i < " << dims[m].rows << "; ++i) {\n"
					"       for (int j = 0; j < " << dims[m].cols << "; ++j) {\n"
					"           int localElementInput = " << strides[m] << " * " << (m_data.colMajor ? "j" : "i") << " + " << (m_data.colMajor ? "i" : "j") << ";\n"
					"           int localElementShared = " << sharedStride << " * " << (m_data.colMajor ? "j" : "i") << " + " << (m_data.colMajor ? "i" : "j") << ";\n"
					"           " << name[m] << "[elementS" << m << " + localElementShared] = " << inputName[m] << ".x[element" << m << " + localElementInput];\n"
					"       }\n"
					"       }\n";
			strides[m] = sharedStride;
		}
		css << "   }\n";
		css << "   controlBarrier(gl_ScopeSubgroup, gl_ScopeSubgroup, gl_StorageSemanticsShared, gl_SemanticsAcquireRelease);\n";
	}

	const char *colMajorNV  = (m_data.colMajor ? "true" : "false");
	const char* colMajorKHR = (m_data.colMajor ? "gl_CooperativeMatrixLayoutColumnMajor" : "gl_CooperativeMatrixLayoutRowMajor");
	const char* colMajor    = (isKhr(m_data.useType) ? colMajorKHR : colMajorNV);

	string loadStrides[3] = { strides[0], strides[1], strides[2] };
	// Load with a stride of 0
	if (m_data.testType == TT_MATRIXMULADD_STRIDE0)
		loadStrides[0] = loadStrides[1] = loadStrides[2] = "0";

	if (m_data.storageClass == SC_WORKGROUP || m_data.storageClass == SC_WORKGROUP_VARIABLE_POINTERS)
	{
		css <<  "   coopMatLoad" << suffix << "(matA, sharedA, elementS0, " << loadStrides[0] << ", " << colMajor << ");\n"
				"   coopMatLoad" << suffix << "(matB, sharedB, elementS1, " << loadStrides[1] << ", " << colMajor << ");\n"
				"   coopMatLoad" << suffix << "(matC, sharedC, elementS2, " << loadStrides[2] << ", " << colMajor << ");\n";
	}
	else
	{
		css << "   coopMatLoad" << suffix << "(matA, inputA.x, element0, " << loadStrides[0] << ", " << colMajor << ");\n"
			   "   coopMatLoad" << suffix << "(matB, inputB.x, element1, " << loadStrides[1] << ", " << colMajor << ");\n"
			   "   coopMatLoad" << suffix << "(matC, inputC.x, element2, " << loadStrides[2] << ", " << colMajor << ");\n";
	}

	if (m_data.testType == TT_COMPOSITE_ARRAY ||
		m_data.testType == TT_MATRIXMULADD_ARRAY)
	{
		css << "   " << matAType.str() << " matAArr[2];\n    matAArr[1] = matA; matAArr[0] = " << matAType.str() << "(0.0);\n"
			   "   " << matBType.str() << " matBArr[2];\n    matBArr[1] = matB; matBArr[0] = " << matBType.str() << "(0.0);\n"
			   "   " << matCType.str() << " matCArr[2];\n    matCArr[1] = matC; matCArr[0] = " << matCType.str() << "(0.0);\n"
			   "   " << outputMatType.str() << " matOArr[2];\n";
	}

	switch (m_data.testType)
	{
	default:
		DE_ASSERT(0);
		// fall through
	case TT_LENGTH:
		css << "   matO = " << outputMatType.str() << "(matO.length());\n";
		break;
	case TT_CONSTANT:
		css << "   matO = matConst;\n";
		break;
	case TT_CONVERT:
		css << "   matO = " << outputMatType.str() << "(matA);\n";
		break;
	case TT_COMPOSITE:
		css << "   " << matAType.str() << " t = " << matAType.str() << "(matB[0]);\n"
			"   for (int i = 1; i < matA.length(); ++i) {\n"
			"       matO[i] = matA[i] + matB[i];\n"
			"   }\n"
			"   if (matA.length() > 0)\n"
			"       matO[0] = matA[0] + t[0];\n";
		break;
	case TT_COMPOSITE_RVALUE:
		css << "   for (int i = 1; i < matA.length(); ++i) {\n"
			   "       matO[i] = matA[i] + matB[i];\n"
			   "   }\n"
			   "   " << matAType.str() << " t = matA;\n"
			   "   if (matA.length() > 0) {\n"
			   "       matO[0] = (t += matB)[0];\n"
			   "   }\n";
		break;
	case TT_COMPOSITE_ARRAY:
		css << "   for (int i = 0; i < matA.length(); ++i) {\n"
			   "       matOArr[1][i] = matAArr[1][i] + matBArr[1][i];\n"
			   "   }\n";
		break;
	case TT_ADD:
		css << "   matO = matA + matB;\n";
		break;
	case TT_SUB:
		css << "   matO = matA - matB;\n";
		break;
	case TT_DIV:
		css << "   matO = matA / matB;\n";
		break;
	case TT_MUL:
		css << "   matO = matA * matB;\n";
		break;
	case TT_NEGATE:
		css << "   matO = -matA;\n";
		break;
	case TT_FUNC:
		css << "   matO = f(matA);\n";
		break;
	case TT_MATRIXTIMESSCALAR:
		css << "   matO = (" << typeStrA << "(2.0)*matA)*" << typeStrA << "(3.0);\n";
		break;
	case TT_MATRIXMULADD_STRIDE0:
	case TT_MATRIXMULADD_WRAPPING:
	case TT_MATRIXMULADD_SATURATED:
	case TT_MATRIXMULADD:
		css << "   matO = coopMatMulAdd" << suffix << "(matA, matB, matC" << sat << ");\n";
		break;
	case TT_MATRIXMULADD_ARRAY:
		css << "   matOArr[1] = coopMatMulAdd" << suffix << "(matAArr[1], matBArr[1], matCArr[1]);\n";
		break;
	}

	if (m_data.testType == TT_COMPOSITE_ARRAY ||
		m_data.testType == TT_MATRIXMULADD_ARRAY)
	{
		css << "   matOArr[0] = " << outputMatType.str() << "(0.0);\n";
		css << "   matO = matOArr[1];\n";
	}

	if (m_data.storageClass == SC_WORKGROUP || m_data.storageClass == SC_WORKGROUP_VARIABLE_POINTERS)
	{
		string sharedStride = strides[3] + " / workgroupsX";
		css << "   coopMatStore" << suffix << "(matO, sharedO, elementS3, " << sharedStride << ", " << colMajor << ");\n";
		css << "   controlBarrier(gl_ScopeSubgroup, gl_ScopeSubgroup, gl_StorageSemanticsShared, gl_SemanticsAcquireRelease);\n";
		css << "   if (subgroupElect()) {\n";
		css << "       for (int i = 0; i < " << dims[3].rows << "; ++i) {\n"
			   "       for (int j = 0; j < " << dims[3].cols << "; ++j) {\n"
			   "           int localElementInput = " << strides[3] << " * " << (m_data.colMajor ? "j" : "i") << " + " << (m_data.colMajor ? "i" : "j") << ";\n"
			   "           int localElementShared = " << sharedStride << " * " << (m_data.colMajor ? "j" : "i") << " + " << (m_data.colMajor ? "i" : "j") << ";\n"
			   "           outputO.x[element3 + localElementInput] = sharedO[elementS3 + localElementShared];\n"
			   "       }\n"
			   "       }\n";
		css << "   }\n";
	}
	else
	{
		css << "   coopMatStore" << suffix << "(matO, outputO.x, element3, " << strides[3] << ", " << colMajor << ");\n";
	}

	css <<
		"}\n";

	const vk::ShaderBuildOptions	buildOptions	(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);

	programCollection.glslSources.add("test") << glu::ComputeSource(css.str()) << buildOptions;
}

TestInstance* CooperativeMatrixTestCase::createInstance (Context& context) const
{
	return new CooperativeMatrixTestInstance(context, m_data);
}

void setDataFloat (void *base, VkComponentTypeKHR dt, deUint32 i, float value)
{
	if (dt == VK_COMPONENT_TYPE_FLOAT32_KHR)
	{
		((float *)base)[i] = value;
	}
	else
	{
		DE_ASSERT(dt == VK_COMPONENT_TYPE_FLOAT16_KHR);
		((deFloat16 *)base)[i] = deFloat32To16(value);
	}
}

float getDataFloat (void *base, VkComponentTypeKHR dt, deUint32 i)
{
	if (dt == VK_COMPONENT_TYPE_FLOAT32_KHR)
	{
		return ((float *)base)[i];
	}
	else
	{
		DE_ASSERT(dt == VK_COMPONENT_TYPE_FLOAT16_KHR);
		return deFloat16To32(((deFloat16 *)base)[i]);
	}
}

void setDataInt (void *base, VkComponentTypeKHR dt, deUint32 i, deUint32 value)
{
	DE_ASSERT(componentTypeInfo[dt].bits <= 32);

	switch (dt)
	{
		case VK_COMPONENT_TYPE_UINT8_KHR:	((deUint8  *)base)[i] = (deUint8)value; break;
		case VK_COMPONENT_TYPE_UINT16_KHR:	((deUint16 *)base)[i] = (deUint16)value; break;
		case VK_COMPONENT_TYPE_UINT32_KHR:	((deUint32 *)base)[i] = (deUint32)value; break;
		case VK_COMPONENT_TYPE_SINT8_KHR:	((deInt8  *)base)[i] = (deInt8)value; break;
		case VK_COMPONENT_TYPE_SINT16_KHR:	((deInt16 *)base)[i] = (deInt16)value; break;
		case VK_COMPONENT_TYPE_SINT32_KHR:	((deInt32 *)base)[i] = (deInt32)value; break;
		default:							TCU_THROW(InternalError, "Unsupported type");
	}
}

deUint32 getDataInt (void *base, VkComponentTypeKHR dt, deUint32 i)
{
	DE_ASSERT(componentTypeInfo[dt].bits <= 32);

	switch (dt)
	{
		case VK_COMPONENT_TYPE_UINT8_KHR:	return ((deUint8*)base)[i];
		case VK_COMPONENT_TYPE_UINT16_KHR:	return ((deUint16*)base)[i];
		case VK_COMPONENT_TYPE_UINT32_KHR:	return ((deUint32*)base)[i];
		case VK_COMPONENT_TYPE_SINT8_KHR:	return ((deInt8*)base)[i];
		case VK_COMPONENT_TYPE_SINT16_KHR:	return ((deInt16*)base)[i];
		case VK_COMPONENT_TYPE_SINT32_KHR:	return ((deInt32 *)base)[i];
		default:							TCU_THROW(InternalError, "Unsupported type");
	}
}

template <typename T>
T getDataConvertedToT (void *base, VkComponentTypeKHR dt, deUint32 i)
{
	DE_ASSERT(componentTypeInfo[dt].bits <= 32);

	switch (dt)
	{
		case VK_COMPONENT_TYPE_UINT8_KHR:	return (T)((deUint8*)base)[i];
		case VK_COMPONENT_TYPE_UINT16_KHR:	return (T)((deUint16*)base)[i];
		case VK_COMPONENT_TYPE_UINT32_KHR:	return (T)((deUint32*)base)[i];
		case VK_COMPONENT_TYPE_SINT8_KHR:	return (T)((deInt8*)base)[i];
		case VK_COMPONENT_TYPE_SINT16_KHR:	return (T)((deInt16*)base)[i];
		case VK_COMPONENT_TYPE_SINT32_KHR:	return (T)((deInt32 *)base)[i];
		case VK_COMPONENT_TYPE_FLOAT32_KHR:
		{
			float temp = ((float *)base)[i];
			if (std::numeric_limits<T>::min() == 0)
				temp = std::max(temp, 0.0f);
			return (T)temp;
		}
		case VK_COMPONENT_TYPE_FLOAT16_KHR:
		{
			float temp = deFloat16To32(((deFloat16 *)base)[i]);
			if (std::numeric_limits<T>::min() == 0)
				temp = std::max(temp, 0.0f);
			return (T)temp;
		}
		default:
			TCU_THROW(InternalError, "Unsupported type");
	}
}

template<typename T>
T satAdd(T a, T b)
{
	if (a > 0)
	{
		if (b > std::numeric_limits<T>::max() - a)
			return std::numeric_limits<T>::max();
	}
	else if (b < std::numeric_limits<T>::min() - a)
	{
		return std::numeric_limits<T>::min();
	}

	return (T)(a + b);
}

deUint32 satAddData (VkComponentTypeKHR dt, deUint32 a, deUint32 b)
{
	DE_ASSERT(componentTypeInfo[dt].bits <= 32);

	switch (dt)
	{
		case VK_COMPONENT_TYPE_UINT8_KHR:	return deMinu32(a + b, std::numeric_limits<deUint8>::max());
		case VK_COMPONENT_TYPE_UINT16_KHR:	return deMinu32(a + b, std::numeric_limits<deUint16>::max());
		case VK_COMPONENT_TYPE_UINT32_KHR:	return (a + b >= a) ? a + b : std::numeric_limits<deUint32>::max();
		case VK_COMPONENT_TYPE_SINT8_KHR:	return (deUint32)satAdd((deInt8)a,  (deInt8)b);
		case VK_COMPONENT_TYPE_SINT16_KHR:	return (deUint32)satAdd((deInt16)a, (deInt16)b);
		case VK_COMPONENT_TYPE_SINT32_KHR:	return (deUint32)satAdd((deInt32)a, (deInt32)b);
		default:							TCU_THROW(InternalError, "Unsupported type");
	}
}

deUint32 getLimit (VkComponentTypeKHR dt, bool positive)
{
	DE_ASSERT(componentTypeInfo[dt].bits <= 32);

	switch (dt)
	{
		case VK_COMPONENT_TYPE_UINT8_KHR:	return deUint32(positive ? std::numeric_limits<deUint8>::max()  : std::numeric_limits<deUint8>::min());
		case VK_COMPONENT_TYPE_UINT16_KHR:	return deUint32(positive ? std::numeric_limits<deUint16>::max() : std::numeric_limits<deUint16>::min());
		case VK_COMPONENT_TYPE_UINT32_KHR:	return deUint32(positive ? std::numeric_limits<deUint32>::max() : std::numeric_limits<deUint32>::min());
		case VK_COMPONENT_TYPE_SINT8_KHR:	return deUint32(positive ? std::numeric_limits<deInt8>::max()   : std::numeric_limits<deInt8>::min());
		case VK_COMPONENT_TYPE_SINT16_KHR:	return deUint32(positive ? std::numeric_limits<deInt16>::max()  : std::numeric_limits<deInt16>::min());
		case VK_COMPONENT_TYPE_SINT32_KHR:	return deUint32(positive ? std::numeric_limits<deInt32>::max()  : std::numeric_limits<deInt32>::min());
		default:							TCU_THROW(InternalError, "Unsupported type");
	}
}

void setSingleElementInt (void *data, VkComponentTypeKHR dt, deUint32 start, deUint32 count, deUint32 step, deUint32 at, deUint32 val)
{
	for (deUint32 i = 0; i < count; i++)
		setDataInt(data, dt, start + i * step, (i == at) ? val : 0);
}

#ifdef COOPERATIVE_MATRIX_EXTENDED_DEBUG
string dumpWholeMatrix (void* data, VkComponentTypeKHR dt, bool colMajor, deUint32 matrixElemCount, deUint32 stride)
{
	const deUint32		rowsCount	= colMajor ? stride : matrixElemCount / stride;
	const deUint32		colsCount	= colMajor ? matrixElemCount / stride : stride;
	bool				floatType	= isFloatType(dt);
	bool				sIntType	= isSIntType(dt);
	std::stringstream	ss;

	DE_ASSERT(rowsCount * colsCount == matrixElemCount);

	for (deUint32 r = 0; r < rowsCount; r++)
	{
		for (deUint32 c = 0; c < colsCount; c++)
		{
			const deUint32 i = colMajor ? rowsCount * c + r : colsCount * r + c;

			if (floatType)
				ss << getDataFloat(data, dt, i) << "\t";
			else if (sIntType)
				ss << (deInt32)getDataInt(data, dt, i) << "\t";
			else
				ss << getDataInt(data, dt, i) << "\t";
		}

		ss << std::endl;
	}

	return ss.str();
}
#endif

tcu::TestStatus CooperativeMatrixTestInstance::iterate (void)
{
	const DeviceInterface&	vk						= m_context.getDeviceInterface();
	const VkDevice			device					= m_context.getDevice();
	Allocator&				allocator				= m_context.getDefaultAllocator();
	MemoryRequirement		memoryDeviceAddress		= m_data.storageClass == SC_PHYSICAL_STORAGE_BUFFER &&
													  m_context.isDeviceFunctionalitySupported("VK_KHR_buffer_device_address") ? MemoryRequirement::DeviceAddress : MemoryRequirement::Any;
	qpTestResult			finalres				= QP_TEST_RESULT_NOT_SUPPORTED;
	tcu::TestLog&			log						= m_context.getTestContext().getLog();
	const bool				saturated				= (m_data.testType == TT_MATRIXMULADD_SATURATED);
	const deUint32			subgroupSize			= getSubgroupSizeFromMode(m_context, m_data.subgroupSizeMode);
	const float				epsilon					= 1.0f / float(1ull<<17); // 131072 is epsilon circa 1e-5

	deRandom rnd;
	deRandom_init(&rnd, 1234);

	std::vector<VkCooperativeMatrixPropertiesKHR>	properties = getCooperativeMatrixPropertiesConverted(m_context, isKhr(m_data.useType));

	struct TestTuple
	{
		TestTuple() {}
		TestTuple(deUint32 m, deUint32 n, deUint32 k) : M(m), N(n), K(k) {}

		bool operator<(const TestTuple &other) const
		{
			return M < other.M ||
				   (M == other.M && N < other.N) ||
				   (M == other.M && N == other.N && K < other.K);
		}

		deUint32 M, N, K;
	};

	vector<TestTuple> testSizes;

	if (isMatrixMulAddOp(m_data.testType))
	{
		for (size_t i = 0; i < properties.size(); ++i)
		{
			VkCooperativeMatrixPropertiesKHR *p = &properties[i];

			if (p->AType == m_data.inputType &&
				p->BType == m_data.inputType &&
				p->CType == m_data.outputType &&
				p->ResultType == m_data.outputType &&
				p->scope == VK_SCOPE_SUBGROUP_KHR)
			{
				testSizes.push_back(TestTuple(p->MSize, p->NSize, p->KSize));
			}
		}
	}
	else
	{
		set<TestTuple> typeSizes[2];
		VkComponentTypeKHR types[2] = { m_data.inputType, m_data.outputType };
		const bool aType = (m_data.useType == UT_KHR_A) || (m_data.useType == UT_NV);
		const bool bType = (m_data.useType == UT_KHR_B) || (m_data.useType == UT_NV);
		const bool rType = (m_data.useType == UT_KHR_Result) || (m_data.useType == UT_NV);

		for (deUint32 i = 0; i < properties.size(); ++i)
		{
			VkCooperativeMatrixPropertiesKHR *p = &properties[i];

			if (p->scope != VK_SCOPE_SUBGROUP_KHR)
				continue;

			for (deUint32 j = 0; j < 2; ++j)
			{
				// For these tests, m_data.M/N are always the matrix size. Check if they match
				// any input or output in the list.
				if (aType && p->AType == types[j]) typeSizes[j].insert(TestTuple(p->MSize, p->KSize, 0));
				if (bType && p->BType == types[j]) typeSizes[j].insert(TestTuple(p->KSize, p->NSize, 0));
				if (rType && (p->CType == types[j] || p->ResultType == types[j])) typeSizes[j].insert(TestTuple(p->MSize, p->NSize, 0));
			}
		}
		// Test those sizes that are supported for both the input and output type.
		std::set_intersection(typeSizes[0].begin(), typeSizes[0].end(),
							  typeSizes[1].begin(), typeSizes[1].end(),
							  std::back_inserter(testSizes));
	}

	properties.resize(0);

	for (unsigned int s = 0; s < testSizes.size(); ++s)
	{
		// When testing a multiply, MxNxK is the type of matrix multiply.
		// Otherwise, MxN is the size of the input/output matrices
		deUint32 M, N, K;
		M = testSizes[s].M;
		N = testSizes[s].N;
		K = testSizes[s].K;

		log << tcu::TestLog::Message << "Testing M = " << M << ", N = " << N << ", K = " << K << tcu::TestLog::EndMessage;

		struct
		{
			deUint32 rows, cols;
		} dims[4];

		if (isMatrixMulAddOp(m_data.testType))
		{
			dims[0].rows = M;
			dims[0].cols = K;
			dims[1].rows = K;
			dims[1].cols = N;
			dims[2].rows = M;
			dims[2].cols = N;
			dims[3].rows = M;
			dims[3].cols = N;
		}
		else
		{
			dims[0].rows = M;
			dims[0].cols = N;
			dims[1].rows = M;
			dims[1].cols = N;
			dims[2].rows = M;
			dims[2].cols = N;
			dims[3].rows = M;
			dims[3].cols = N;
		}

		VkComponentTypeKHR dataTypes[4];
		size_t elementSize[4];
		VkDeviceSize bufferSizes[5];
		de::MovePtr<BufferWithMemory> buffers[5];
		vk::VkDescriptorBufferInfo bufferDescriptors[5];
		deUint32 strides[4]; // in elements
		deUint32 loadStrides[4];
		deUint32 totalElements[4];

		for (deUint32 i = 0; i < 5; ++i)
		{
			if (i < 4)
			{
				// A/B use input type, C/D use output type
				dataTypes[i] = (i < 2) ? m_data.inputType : m_data.outputType;
				elementSize[i] = componentTypeInfo[dataTypes[i]].bits / 8;

				strides[i] = (m_data.colMajor ? dims[i].rows : dims[i].cols) * m_data.subgroupsPerWorkgroupX * m_data.workgroupsX;
				loadStrides[i] = strides[i];
				totalElements[i] = strides[i] * (m_data.colMajor ? dims[i].cols : dims[i].rows) * m_data.subgroupsPerWorkgroupY * m_data.workgroupsY;

				bufferSizes[i] = totalElements[i] * elementSize[i];
			}
			else
			{
				bufferSizes[4] = sizeof(VkDeviceAddress)*4;
			}

			try
			{
				buffers[i] = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
					vk, device, allocator, makeBufferCreateInfo(bufferSizes[i], VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT|
					(memoryDeviceAddress == MemoryRequirement::DeviceAddress ?  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_EXT : 0)),
					MemoryRequirement::HostVisible | MemoryRequirement::Cached | MemoryRequirement::Coherent | memoryDeviceAddress));
			}
			catch (const tcu::NotSupportedError&)
			{
				buffers[i] = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
					vk, device, allocator, makeBufferCreateInfo(bufferSizes[i], VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT|
					(memoryDeviceAddress == MemoryRequirement::DeviceAddress ?  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_EXT : 0)),
					MemoryRequirement::HostVisible | memoryDeviceAddress));
			}

			bufferDescriptors[i] = makeDescriptorBufferInfo(**buffers[i], 0, bufferSizes[i]);
		}

		// Load with a stride of 0
		if (m_data.testType == TT_MATRIXMULADD_STRIDE0)
			loadStrides[0] = loadStrides[1] = loadStrides[2] = loadStrides[3] = 0;

		void *ptrs[5];
		for (deUint32 i = 0; i < 5; ++i)
		{
			ptrs[i] = buffers[i]->getAllocation().getHostPtr();
		}

		vk::DescriptorSetLayoutBuilder layoutBuilder;

		layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, allShaderStages);
		layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, allShaderStages);
		layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, allShaderStages);
		layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, allShaderStages);
		layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, allShaderStages);

		vk::Unique<vk::VkDescriptorSetLayout>	descriptorSetLayout(layoutBuilder.build(vk, device));

		vk::Unique<vk::VkDescriptorPool>		descriptorPool(vk::DescriptorPoolBuilder()
			.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 5u)
			.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));
		vk::Unique<vk::VkDescriptorSet>			descriptorSet		(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

		vk::DescriptorSetUpdateBuilder setUpdateBuilder;
		if (m_data.storageClass == SC_PHYSICAL_STORAGE_BUFFER)
		{
			VkBufferDeviceAddressInfo info
			{
				VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,		// VkStructureType	 sType;
				DE_NULL,											// const void*		 pNext;
				0,													// VkBuffer			buffer
			};
			VkDeviceAddress *addrsInMemory = (VkDeviceAddress *)ptrs[4];
			for (deUint32 i = 0; i < 4; ++i)
			{
				info.buffer = **buffers[i];
				VkDeviceAddress addr = vk.getBufferDeviceAddress(device, &info);
				addrsInMemory[i] = addr;
			}
			setUpdateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(4),
				VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptors[4]);
		}
		else
		{
			setUpdateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0),
				VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptors[0]);
			setUpdateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(1),
				VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptors[1]);
			setUpdateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(2),
				VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptors[2]);
			setUpdateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(3),
				VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptors[3]);
		}

		setUpdateBuilder.update(vk, device);

		VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;

		const deUint32 specData[9] =
		{
			subgroupSize * m_data.subgroupsPerWorkgroupX,
			m_data.subgroupsPerWorkgroupY,
			strides[0],
			strides[1],
			strides[2],
			strides[3],
			M,
			N,
			K,
		};

		const vk::VkSpecializationMapEntry entries[9] =
		{
			{0, (deUint32)(sizeof(deUint32) * 0), sizeof(deUint32)},
			{1, (deUint32)(sizeof(deUint32) * 1), sizeof(deUint32)},
			{2, (deUint32)(sizeof(deUint32) * 2), sizeof(deUint32)},
			{3, (deUint32)(sizeof(deUint32) * 3), sizeof(deUint32)},
			{4, (deUint32)(sizeof(deUint32) * 4), sizeof(deUint32)},
			{5, (deUint32)(sizeof(deUint32) * 5), sizeof(deUint32)},
			{6, (deUint32)(sizeof(deUint32) * 6), sizeof(deUint32)},
			{7, (deUint32)(sizeof(deUint32) * 7), sizeof(deUint32)},
			{8, (deUint32)(sizeof(deUint32) * 8), sizeof(deUint32)},
		};

		const vk::VkSpecializationInfo specInfo =
		{
			9,						// mapEntryCount
			entries,				// pMapEntries
			sizeof(specData),		// dataSize
			specData				// pData
		};

		for (deUint32 i = 0; i < 4; ++i)
			for (deUint32 j = 0; j < totalElements[i]; ++j)
			{
				if (isFloatType(dataTypes[i]))
				{
					if (!isMatrixMulAddOp(m_data.testType))
						setDataFloat(ptrs[i], dataTypes[i], j, ((float)(deRandom_getUint32(&rnd) & 0xff) - 64.0f)/2.0f);
					else
						setDataFloat(ptrs[i], dataTypes[i], j, ((float)(deRandom_getUint32(&rnd) & 0xf) - 4.0f)/2.0f);
				}
				else
				{
					if (m_data.testType == TT_MATRIXMULADD_WRAPPING)
					{
						// Choose matrix values that should cause overflow and underflow, to
						// verify wrapping behavior. Use the full range of values for A and B.
						// For matrix C, use values clustered near where the type wraps (zero
						// for unsigned, 2^(N-1) for signed).
						deUint32 bits = componentTypeInfo[dataTypes[i]].bits;
						deUint32 value;
						if (i == 2) {
							value = (deRandom_getUint32(&rnd) & 0xff) - 128;
							if (componentTypeInfo[dataTypes[i]].isSigned)
								value += (1U << (bits - 1));
						} else {
							deUint32 mask = (bits == 32) ? 0xFFFFFFFFU : ((1U << bits) - 1U);
							value = deRandom_getUint32(&rnd) & mask;
						}
						setDataInt(ptrs[i], dataTypes[i], j, value);
					}
					else if (m_data.testType == TT_MATRIXMULADD_SATURATED)
					{
						setDataInt(ptrs[i], dataTypes[i], j, 0);
					}
					else
					{
						deUint32 value = (deRandom_getUint32(&rnd) & 0xff) - 128;
						setDataInt(ptrs[i], dataTypes[i], j, value);
					}
				}
			}

		if (m_data.testType == TT_MATRIXMULADD_SATURATED)
		{
			// Set 1st row of A to 1,0,0...
			setSingleElementInt(ptrs[0], dataTypes[0], 0, dims[0].cols, (m_data.colMajor ? strides[0] : 1), 0, 1);

			// Set 1st column of B to 1,0,0...
			setSingleElementInt(ptrs[1], dataTypes[1], 0, dims[1].rows, (m_data.colMajor ? 1 : strides[1]), 0, 1);

			// Set C element at {0,0} to maximum type value, thus we will have overflow at plus operation in D=A*B+C for this element
			setDataInt(ptrs[2], dataTypes[2], 0, getLimit(dataTypes[2], true));

			// Check underflow if all involved elements support negative values
			if (isSIntType(dataTypes[1]) && isSIntType(dataTypes[2]) && isSIntType(dataTypes[3]))
			{
				// Set 2nd row of A to 0,1,0,0...
				setSingleElementInt(ptrs[0], dataTypes[0], (m_data.colMajor ? 1 : strides[0]), dims[0].cols, (m_data.colMajor ? strides[0] : 1), 1, 1);

				// Set 2nd column of B to 0,-1,0,0...
				setSingleElementInt(ptrs[1], dataTypes[1], (m_data.colMajor ? strides[1] : 1), dims[1].rows, (m_data.colMajor ? 1 : strides[1]), 1, -1);

				// Set C element at {1,1} to minimum type value, thus we will have underflow at plus operation in D=A*B+C for this element
				setDataInt(ptrs[2], dataTypes[2], strides[2] + 1, getLimit(dataTypes[2], false));
			}
		}

		flushAlloc(vk, device, buffers[0]->getAllocation());
		flushAlloc(vk, device, buffers[1]->getAllocation());
		flushAlloc(vk, device, buffers[2]->getAllocation());
		flushAlloc(vk, device, buffers[3]->getAllocation());

		ComputePipelineWrapper			pipeline(vk, device, m_data.computePipelineConstructionType, m_context.getBinaryCollection().get("test"));
		pipeline.setDescriptorSetLayout(descriptorSetLayout.get());
		pipeline.setSpecializationInfo(specInfo);
		pipeline.setSubgroupSize(m_data.subgroupSizeMode == SUBGROUP_SIZE_NONE ? 0 : getSubgroupSizeFromMode(m_context, m_data.subgroupSizeMode));
		pipeline.buildPipeline();

		const VkQueue					queue					= m_context.getUniversalQueue();
		Move<VkCommandPool>				cmdPool					= createCommandPool(vk, device, 0, m_context.getUniversalQueueFamilyIndex());
		Move<VkCommandBuffer>			cmdBuffer				= allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

		beginCommandBuffer(vk, *cmdBuffer, 0u);

		vk.cmdBindDescriptorSets(*cmdBuffer, bindPoint, pipeline.getPipelineLayout(), 0u, 1, &*descriptorSet, 0u, DE_NULL);
		pipeline.bind(*cmdBuffer);

		vk.cmdDispatch(*cmdBuffer, m_data.workgroupsX, m_data.workgroupsY, 1);

		endCommandBuffer(vk, *cmdBuffer);

		submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

		invalidateAlloc(vk, device, buffers[3]->getAllocation());

		qpTestResult res = QP_TEST_RESULT_PASS;

		if (m_data.testType == TT_CONVERT)
		{
			for (deUint32 i = 0; i < totalElements[3]; ++i)
			{
				// Store results as double, which has enough range to hold all the other types exactly.
				double inputA, output;

				// This loads the data according to dataTypes[0], and then converts to the template parameter type
				switch (dataTypes[3]) {
				case VK_COMPONENT_TYPE_UINT8_KHR:	inputA = getDataConvertedToT<uint8_t>(ptrs[0], dataTypes[0], i); break;
				case VK_COMPONENT_TYPE_UINT16_KHR:	inputA = getDataConvertedToT<uint16_t>(ptrs[0], dataTypes[0], i); break;
				case VK_COMPONENT_TYPE_UINT32_KHR:	inputA = getDataConvertedToT<uint32_t>(ptrs[0], dataTypes[0], i); break;
				case VK_COMPONENT_TYPE_SINT8_KHR:	inputA = getDataConvertedToT<int8_t>(ptrs[0], dataTypes[0], i); break;
				case VK_COMPONENT_TYPE_SINT16_KHR:	inputA = getDataConvertedToT<int16_t>(ptrs[0], dataTypes[0], i); break;
				case VK_COMPONENT_TYPE_SINT32_KHR:	inputA = getDataConvertedToT<int32_t>(ptrs[0], dataTypes[0], i); break;
				case VK_COMPONENT_TYPE_FLOAT32_KHR: inputA = getDataConvertedToT<float>(ptrs[0], dataTypes[0], i); break;
				case VK_COMPONENT_TYPE_FLOAT16_KHR:
				{
					float temp = getDataConvertedToT<float>(ptrs[0], dataTypes[0], i);
					inputA = deFloat16To32(deFloat32To16(temp));
					break;
				}
				default: TCU_THROW(InternalError, "Unexpected type");
				}

				switch (dataTypes[3]) {
				case VK_COMPONENT_TYPE_UINT8_KHR:	output = getDataConvertedToT<uint8_t>(ptrs[3], dataTypes[3], i); break;
				case VK_COMPONENT_TYPE_UINT16_KHR:	output = getDataConvertedToT<uint16_t>(ptrs[3], dataTypes[3], i); break;
				case VK_COMPONENT_TYPE_UINT32_KHR:	output = getDataConvertedToT<uint32_t>(ptrs[3], dataTypes[3], i); break;
				case VK_COMPONENT_TYPE_SINT8_KHR:	output = getDataConvertedToT<int8_t>(ptrs[3], dataTypes[3], i); break;
				case VK_COMPONENT_TYPE_SINT16_KHR:	output = getDataConvertedToT<int16_t>(ptrs[3], dataTypes[3], i); break;
				case VK_COMPONENT_TYPE_SINT32_KHR:	output = getDataConvertedToT<int32_t>(ptrs[3], dataTypes[3], i); break;
				case VK_COMPONENT_TYPE_FLOAT32_KHR: output = getDataConvertedToT<float>(ptrs[3], dataTypes[3], i); break;
				case VK_COMPONENT_TYPE_FLOAT16_KHR:
				{
					float temp = getDataConvertedToT<float>(ptrs[3], dataTypes[3], i);
					output = deFloat16To32(deFloat32To16(temp));
					break;
				}
				default: TCU_THROW(InternalError, "Unexpected type");
				}

				if (inputA != output) {
					res = QP_TEST_RESULT_FAIL;
					break;
				}
			}
		}
		else if (isFloatType(dataTypes[0]))
		{
			if (!isMatrixMulAddOp(m_data.testType))
			{
				for (deUint32 i = 0; i < totalElements[3]; ++i)
				{
					float inputA = getDataFloat(ptrs[0], dataTypes[0], i);
					float inputB = getDataFloat(ptrs[1], dataTypes[1], i);
					float output = getDataFloat(ptrs[3], dataTypes[3], i);
					switch (m_data.testType)
					{
					case TT_LENGTH:
						if (output < 1.0f || output > (float)(N*M))
							res = QP_TEST_RESULT_FAIL;
						// We expect the matrix to be spread evenly across invocations, it is
						// surprising (but not necessarily illegal) if not
						if (output != (float)(N*M/subgroupSize) &&
							res == QP_TEST_RESULT_PASS)
							res = QP_TEST_RESULT_QUALITY_WARNING;
						break;
					case TT_CONSTANT:
						if (output != 1.0f)
							res = QP_TEST_RESULT_FAIL;
						break;
					case TT_COMPOSITE:
					case TT_COMPOSITE_RVALUE:
					case TT_COMPOSITE_ARRAY:
					case TT_ADD:
						if (output != inputA + inputB)
							res = QP_TEST_RESULT_FAIL;
						break;
					case TT_SUB:
						if (output != inputA - inputB)
							res = QP_TEST_RESULT_FAIL;
						break;
					case TT_DIV:
						{
							float ulp = (m_data.inputType == VK_COMPONENT_TYPE_FLOAT16_KHR) ? 1.0f/1024.0f : 1.0f/(8.0f*1024.0f*1024.0f);
							// division allows 2.5ulp, but we'll use 3.
							ulp *= 3;
							if (inputB != 0 && fabs(output - inputA / inputB) > ulp * fabs(inputA / inputB))
								res = QP_TEST_RESULT_FAIL;
						}
						break;
					case TT_MUL:
					{
						if (dataTypes[0] == VK_COMPONENT_TYPE_FLOAT16_KHR)
						{
							const float		expected32	= inputA * inputB;
							const deFloat16	expected16	= deFloat32To16(expected32);
							const float		expected	= deFloat16To32(expected16);

							if (output != expected)
								res = QP_TEST_RESULT_FAIL;
						}
						else
						{
							if (output != inputA * inputB)
								res = QP_TEST_RESULT_FAIL;
						}
						break;
					}
					case TT_NEGATE:
					case TT_FUNC:
						if (output != -inputA)
							res = QP_TEST_RESULT_FAIL;
						break;
					case TT_MATRIXTIMESSCALAR:
						if (output != 6.0*inputA)
							res = QP_TEST_RESULT_FAIL;
						break;
					default:
						break;
					}
				}
			}
			else
			{
				deUint32 ik, kj, ij;
				for (deUint32 mX = 0; mX < m_data.subgroupsPerWorkgroupX*m_data.workgroupsX; ++mX)
				{
					for (deUint32 mY = 0; mY < m_data.subgroupsPerWorkgroupY*m_data.workgroupsY; ++mY)
					{
						for (deUint32 i = 0; i < M; ++i)
						{
							for (deUint32 j = 0; j < N; ++j)
							{
								float ref = 0;
								for (deUint32 k = 0; k < K; ++k)
								{
									if (m_data.colMajor)
										ik = mX * M + i + strides[0] * mY * K + loadStrides[0] * k;
									else
										ik = mX * K + k + strides[0] * mY * M + loadStrides[0] * i;

									float Aik = getDataFloat(ptrs[0], dataTypes[0], ik);

									if (m_data.colMajor)
										kj = mX * K + k + strides[1] * mY * N + loadStrides[1] * j;
									else
										kj = mX * N + j + strides[1] * mY * K + loadStrides[1] * k;

									float Bkj = getDataFloat(ptrs[1], dataTypes[1], kj);

									ref += Aik*Bkj;
								}

								if (m_data.colMajor)
									ij = mX * M + i + strides[2] * mY * N + loadStrides[2] * j;
								else
									ij = mX * N + j + strides[2] * mY * M + loadStrides[2] * i;

								float Cij = getDataFloat(ptrs[2], dataTypes[2], ij);

								ref += Cij;

								// When loading with stride 0, ij for matrix D is different from matrix C
								if (m_data.colMajor)
									ij = mX * M + i + strides[2] * (mY * N + j);
								else
									ij = mX * N + j + strides[2] * (mY * M + i);

								float Dij = getDataFloat(ptrs[3], dataTypes[3], ij);

								if (fabs(ref - Dij) > epsilon)
								{
									res = QP_TEST_RESULT_FAIL;
								}
							}
						}
					}
				}
			}
		} else {
			if (!isMatrixMulAddOp(m_data.testType))
			{
				for (deUint32 i = 0; i < totalElements[3]; ++i)
				{
					deUint32 inputA = getDataInt(ptrs[0], dataTypes[0], i);
					deUint32 inputB = getDataInt(ptrs[1], dataTypes[1], i);
					deUint32 output = getDataInt(ptrs[3], dataTypes[3], i);
					int resultSize = componentTypeInfo[dataTypes[3]].bits;
					deUint32 mask = resultSize == 32 ? ~0 : ((1 << resultSize) - 1);
					switch (m_data.testType)
					{
					case TT_LENGTH:
						if (output < 1 || output > N*M)
							res = QP_TEST_RESULT_FAIL;
						// We expect the matrix to be spread evenly across invocations, it is
						// surprising (but not necessarily illegal) if not
						if (output != N*M/subgroupSize &&
							res == QP_TEST_RESULT_PASS)
							res = QP_TEST_RESULT_QUALITY_WARNING;
						break;
					case TT_CONSTANT:
						if (output != 1)
							res = QP_TEST_RESULT_FAIL;
						break;
					case TT_COMPOSITE:
					case TT_COMPOSITE_RVALUE:
					case TT_COMPOSITE_ARRAY:
					case TT_ADD:
						if ((output & mask) != ((inputA + inputB) & mask)) {
							res = QP_TEST_RESULT_FAIL;
						}
						break;
					case TT_SUB:
						if ((output & mask) != ((inputA - inputB) & mask))
							res = QP_TEST_RESULT_FAIL;
						break;
					case TT_DIV:
						{
							if (isSIntType(dataTypes[3]))
							{
								if (inputB != 0 && ((deInt32)output & mask) != (((deInt32)inputA / (deInt32)inputB) & mask))
									res = QP_TEST_RESULT_FAIL;
							} else
							{
								if (inputB != 0 && output != inputA / inputB)
									res = QP_TEST_RESULT_FAIL;
							}
						}
						break;
					case TT_MUL:
					{
						if (((deInt32)output & mask) != (((deInt32)inputA * (deInt32)inputB) & mask))
						{
							res = QP_TEST_RESULT_FAIL;
						}

						break;
					}
					case TT_NEGATE:
					case TT_FUNC:
						if ((output & mask) != ((-(deInt32)inputA) & mask))
							res = QP_TEST_RESULT_FAIL;
						break;
					case TT_MATRIXTIMESSCALAR:
						if ((output & mask) != ((6*inputA) & mask)) {
							res = QP_TEST_RESULT_FAIL;
						}
						break;
					default:
						break;
					}
				}
			}
			else
			{
				deUint32 ik, kj, ij;
				for (deUint32 mX = 0; mX < m_data.subgroupsPerWorkgroupX*m_data.workgroupsX; ++mX)
				{
					for (deUint32 mY = 0; mY < m_data.subgroupsPerWorkgroupY*m_data.workgroupsY; ++mY)
					{
						for (deUint32 i = 0; i < M; ++i)
						{
							for (deUint32 j = 0; j < N; ++j)
							{
								deUint32 ref = 0;

								for (deUint32 k = 0; k < K; ++k)
								{
									if (m_data.colMajor)
										ik = mX * M + i + strides[0] * mY * K + loadStrides[0] * k;
									else
										ik = mX * K + k + strides[0] * mY * M + loadStrides[0] * i;

									deUint32 Aik = getDataInt(ptrs[0], dataTypes[0], ik);

									if (m_data.colMajor)
										kj = mX * K + k + strides[1] * mY * N + loadStrides[1] * j;
									else
										kj = mX * N + j + strides[1] * mY * K + loadStrides[1] * k;

									deUint32 Bkj = getDataInt(ptrs[1], dataTypes[1], kj);

									ref += Aik*Bkj;
								}

								if (m_data.colMajor)
									ij = mX * M + i + strides[2] * mY * N + loadStrides[2] * j;
								else
									ij = mX * N + j + strides[2] * mY * M + loadStrides[2] * i;

								deUint32 Cij = getDataInt(ptrs[2], dataTypes[2], ij);

								if (saturated)
								{
									ref = satAddData(dataTypes[2], ref, Cij);
								}
								else
								{
									ref += Cij;
									// truncate the result to the size of C's type.
									deUint32 bits = componentTypeInfo[dataTypes[3]].bits;
									deUint32 mask = (bits == 32) ? 0xFFFFFFFFU : ((1U << bits) - 1U);
									ref &= mask;
								}

								// When loading with stride 0, ij for matrix D is different from matrix C
								if (m_data.colMajor)
									ij = mX * M + i + strides[2] * (mY * N + j);
								else
									ij = mX * N + j + strides[2] * (mY * M + i);

								deUint32 Dij = getDataInt(ptrs[3], dataTypes[3], ij);

								if (ref != Dij)
								{
									res = QP_TEST_RESULT_FAIL;
								}
							}
						}
					}
				}
			}
		}

		if (res != QP_TEST_RESULT_PASS)
		{
			finalres = res;

			log << tcu::TestLog::Message << "failed with M = " << M << ", N = " << N << ", K = " << K << tcu::TestLog::EndMessage;

#ifdef COOPERATIVE_MATRIX_EXTENDED_DEBUG
			for (int i = 0; i < 4; i++)
			{
				const char* matrixNames[] = { "A", "B", "C", "D" };

				log << tcu::TestLog::Message
					<< "Matrix " << matrixNames[i]
					<< "[rows="
					<< m_data.subgroupsPerWorkgroupY * m_data.workgroupsY * dims[i].rows
					<< ", cols="
					<< m_data.subgroupsPerWorkgroupX * m_data.workgroupsX * dims[i].cols << "]:\n"
					<< dumpWholeMatrix(ptrs[i], dataTypes[i], m_data.colMajor, totalElements[i], strides[i])
					<< tcu::TestLog::EndMessage;
			}
#endif
		}
		else
		{
			if (finalres == QP_TEST_RESULT_NOT_SUPPORTED)
				finalres = res;
		}
	}

	return tcu::TestStatus(finalres, qpGetTestResultName(finalres));
}

const char* getUseType (UseType useType)
{
	switch (useType)
	{
		case UT_NV:			return "nv";
		case UT_KHR_A:		return "khr_a";
		case UT_KHR_B:		return "khr_b";
		case UT_KHR_Result:	return "khr_r";
		default:			TCU_THROW(InternalError, "Unknown use type");
	}
}

tcu::TestCaseGroup*	createCooperativeMatrixTestsInternal (tcu::TestContext& testCtx, vk::ComputePipelineConstructionType computePipelineConstructionType, UseType useType)
{
	de::MovePtr<tcu::TestCaseGroup> group	(new tcu::TestCaseGroup(testCtx, getUseType(useType), ""));

	typedef struct
	{
		deUint32				value;
		const char*				name;
		const char*				description;
	} TestGroupCase;

	typedef struct
	{
		deUint32				value[2];
		const char*				name;
		const char*				description;
	} TestGroupCase2;

	typedef struct
	{
		SubgroupSizeMode		value;
		const char*				name;
		const char*				description;
	} SubGroubSizes;

	TestGroupCase ttCases[] =
	{
		{ TT_LENGTH,				"length",					"OpCooperativeMatrixLength"					},
		{ TT_CONSTANT,				"constant",					"OpConstantComposite"						},
		{ TT_COMPOSITE,				"composite",				"OpCompositeConstruct"						},
		{ TT_COMPOSITE_RVALUE,		"composite_rvalue",			"OpCompositeExtract"						},
		{ TT_ADD,					"add",						"OpFAdd/OpIAdd"								},
		{ TT_SUB,					"sub",						"OpFSub/OpISub"								},
		{ TT_DIV,					"div",						"OpFDiv/OpSDiv/OpUDiv"						},
		{ TT_MUL,					"mul",						"OpFMul/OpIMul"								},
		{ TT_NEGATE,				"negate",					"OpFNegate/OpSNegate"						},
		{ TT_MATRIXTIMESSCALAR,		"matrixtimesscalar",		"OpMatrixTimesScalar"						},
		{ TT_FUNC,					"func",						"OpFunctionParameter"						},
		{ TT_MATRIXMULADD,			"matrixmuladd",				"OpCooperativeMatrixMulAdd"					},
		{ TT_COMPOSITE_ARRAY,		"composite_array",			"OpCompositeConstruct w/array"				},
		{ TT_MATRIXMULADD_ARRAY,	"matrixmuladd_array",		"OpCooperativeMatrixMulAdd w/array"			},
		{ TT_MATRIXMULADD_SATURATED,"matrixmuladd_saturated",	"OpCooperativeMatrixMulAdd w/saturations"	},
		{ TT_MATRIXMULADD_WRAPPING,	"matrixmuladd_wrapping",	"OpCooperativeMatrixMulAdd w/wrapping"		},
		{ TT_MATRIXMULADD_STRIDE0,	"matrixmuladd_stride0",		"OpCooperativeMatrixMulAdd w/stride==0"		},
	};
	TestGroupCase2 dtCases[] =
	{
		{ { VK_COMPONENT_TYPE_FLOAT32_KHR,	VK_COMPONENT_TYPE_FLOAT32_KHR },	"float32_float32",	"A/B are fp32 C/D are fp32"		},
		{ { VK_COMPONENT_TYPE_FLOAT32_KHR,	VK_COMPONENT_TYPE_FLOAT16_KHR },	"float32_float16",	"A/B are fp32 C/D are fp16"		},
		{ { VK_COMPONENT_TYPE_FLOAT16_KHR,	VK_COMPONENT_TYPE_FLOAT32_KHR },	"float16_float32",	"A/B are fp16 C/D are fp32"		},
		{ { VK_COMPONENT_TYPE_FLOAT16_KHR,	VK_COMPONENT_TYPE_FLOAT16_KHR },	"float16_float16",	"A/B are fp16 C/D are fp16"		},
		{ { VK_COMPONENT_TYPE_UINT8_KHR,	VK_COMPONENT_TYPE_UINT8_KHR },		"uint8_uint8",		"A/B are u8 C/D are u8"			},
		{ { VK_COMPONENT_TYPE_UINT8_KHR,	VK_COMPONENT_TYPE_UINT32_KHR },		"uint8_uint32",		"A/B are u8 C/D are u32"		},
		{ { VK_COMPONENT_TYPE_SINT8_KHR,	VK_COMPONENT_TYPE_SINT8_KHR },		"sint8_sint8",		"A/B are s8 C/D are s8"			},
		{ { VK_COMPONENT_TYPE_SINT8_KHR,	VK_COMPONENT_TYPE_SINT32_KHR },		"sint8_sint32",		"A/B are s8 C/D are s32"		},
		{ { VK_COMPONENT_TYPE_UINT8_KHR,	VK_COMPONENT_TYPE_SINT32_KHR },		"uint8_sint32",		"A/B are u8 C/D are s32"		},
		{ { VK_COMPONENT_TYPE_UINT32_KHR,	VK_COMPONENT_TYPE_UINT32_KHR },		"uint32_uint32",	"A/B are u32 C/D are u32"		},
		{ { VK_COMPONENT_TYPE_UINT32_KHR,	VK_COMPONENT_TYPE_UINT8_KHR },		"uint32_uint8",		"A/B are u32 C/D are u8"		},
		{ { VK_COMPONENT_TYPE_SINT32_KHR,	VK_COMPONENT_TYPE_SINT32_KHR },		"sint32_sint32",	"A/B are s32 C/D are s32"		},
		{ { VK_COMPONENT_TYPE_SINT32_KHR,	VK_COMPONENT_TYPE_SINT8_KHR },		"sint32_sint8",		"A/B are s32 C/D are s8"		},
	};
	SubGroubSizes sgsCases[] =
	{
		{ SUBGROUP_SIZE_NONE,	"",		"Default subgroup size" },
		{ SUBGROUP_SIZE_MIN,	"_min",	"Minimum subgroup size" },
		{ SUBGROUP_SIZE_MAX,	"_max",	"Maximum subgroup size" },
	};

	TestGroupCase colCases[] =
	{
		{ 0,		"rowmajor",	"row major"		},
		{ 1,		"colmajor",	"col major"		},
	};

	TestGroupCase scCases[] =
	{
		{ SC_BUFFER,						"buffer",			"SSBO"				},
		{ SC_WORKGROUP,						"workgroup",		"shared memory"		},
		{ SC_BUFFER_VARIABLE_POINTERS,		"buffer_varptr",	"SSBO w/variable pointers"		},
		{ SC_WORKGROUP_VARIABLE_POINTERS,	"workgroup_varptr",	"shared memory w/variable pointers"		},
		{ SC_PHYSICAL_STORAGE_BUFFER,		"physical_buffer",	"physical_storage_buffer"				},
	};

	// Types tested for conversions. Excludes 64b types.
	VkComponentTypeKHR allTypes[] =
	{
		VK_COMPONENT_TYPE_FLOAT16_KHR,
		VK_COMPONENT_TYPE_FLOAT32_KHR,
		VK_COMPONENT_TYPE_SINT8_KHR,
		VK_COMPONENT_TYPE_SINT16_KHR,
		VK_COMPONENT_TYPE_SINT32_KHR,
		VK_COMPONENT_TYPE_UINT8_KHR,
		VK_COMPONENT_TYPE_UINT16_KHR,
		VK_COMPONENT_TYPE_UINT32_KHR,
	};

	for (int ttNdx = 0; ttNdx < DE_LENGTH_OF_ARRAY(ttCases); ttNdx++)
	{
		const TestType	testType = (TestType)ttCases[ttNdx].value;

		for (int sgsNdx = 0; sgsNdx < DE_LENGTH_OF_ARRAY(sgsCases); sgsNdx++)
		{
			if (testType != TT_MATRIXMULADD && sgsCases[sgsNdx].value != SUBGROUP_SIZE_NONE)
				continue;

			if (testType == TT_MATRIXMULADD && sgsCases[sgsNdx].value != SUBGROUP_SIZE_NONE && useType == UT_NV)
				continue;

			const string					name	= string(ttCases[ttNdx].name) + sgsCases[sgsNdx].name;
			const string					desc	= string(ttCases[ttNdx].description) + " " + sgsCases[sgsNdx].description;
			de::MovePtr<tcu::TestCaseGroup>	ttGroup	(new tcu::TestCaseGroup(testCtx, name.c_str(), desc.c_str()));

			for (int dtNdx = 0; dtNdx < DE_LENGTH_OF_ARRAY(dtCases); dtNdx++)
			{
				de::MovePtr<tcu::TestCaseGroup> dtGroup(new tcu::TestCaseGroup(testCtx, dtCases[dtNdx].name, dtCases[dtNdx].description));
				for (int scNdx = 0; scNdx < DE_LENGTH_OF_ARRAY(scCases); scNdx++)
				{
					de::MovePtr<tcu::TestCaseGroup> scGroup(new tcu::TestCaseGroup(testCtx, scCases[scNdx].name, scCases[scNdx].description));
					for (int colNdx = 0; colNdx < DE_LENGTH_OF_ARRAY(colCases); colNdx++)
					{
						const VkComponentTypeKHR	inputType = (VkComponentTypeKHR)dtCases[dtNdx].value[0];
						const VkComponentTypeKHR	outputType = (VkComponentTypeKHR)dtCases[dtNdx].value[1];
						const bool					isMatrixMul = isMatrixMulAddOp(testType);

						// useType isn't used for matrixmul shaders. Don't generate 3 copies of those tests.
						if (isMatrixMul && (useType == UT_KHR_A || useType == UT_KHR_B)) {
							continue;
						}

						// NV extension doesn't support mixing signedness
						if (isMatrixMul && (useType == UT_NV) && isSIntType(inputType) != isSIntType(outputType)) {
							continue;
						}

						if (!isMatrixMul && inputType != outputType)
							continue;

						if (isMatrixMul && componentTypeInfo[inputType].bits > componentTypeInfo[outputType].bits)
							continue;

						if (testType == TT_MUL && useType == UT_NV)
							continue;

						if (testType == TT_MATRIXMULADD_SATURATED && (isFloatType(inputType) || useType == UT_NV))
							continue;

						if (testType == TT_MATRIXMULADD_WRAPPING && (isFloatType(inputType) || useType == UT_NV))
							continue;

						if (testType == TT_MATRIXMULADD_STRIDE0 && useType == UT_NV)
							continue;

						if (testType == TT_LENGTH && useType != UT_NV && (outputType == VK_COMPONENT_TYPE_SINT8_KHR || outputType == VK_COMPONENT_TYPE_UINT8_KHR))
							continue;

						CaseDef c =
						{
							testType,							//  TestType							testtype;
							2u,									//  deUint32							subgroupsPerWorkgroupX;
							2u,									//  deUint32							subgroupsPerWorkgroupY;
							4u,									//  deUint32							workgroupsX;
							4u,									//  deUint32							workgroupsY;
							inputType,							//  VkComponentTypeKHR					inputType;
							outputType,							//  VkComponentTypeKHR					outputType;
							!!colCases[colNdx].value,			//  bool								colMajor;
							(StorageClass)scCases[scNdx].value,	//  StorageClass						storageClass;
							useType,							//  UseType								useType;
							sgsCases[sgsNdx].value,				//  SubgroupSizeMode					subgroupSizeMode;
							computePipelineConstructionType,	//  vk::ComputePipelineConstructionType	computePipelineConstructionType;
						};

						scGroup->addChild(new CooperativeMatrixTestCase(testCtx, colCases[colNdx].name, colCases[colNdx].description, c));
					}
					dtGroup->addChild(scGroup.release());
				}
				ttGroup->addChild(dtGroup.release());
			}
			group->addChild(ttGroup.release());
		}
	}

	{
		const string					name	= string("convert");
		const string					desc	= string("OpFConvert/OpSConvert/OpUConvert/OpBitcast");
		de::MovePtr<tcu::TestCaseGroup>	ttGroup	(new tcu::TestCaseGroup(testCtx, name.c_str(), desc.c_str()));

		for (int dtNdx1 = 0; dtNdx1 < DE_LENGTH_OF_ARRAY(allTypes); dtNdx1++)
		{
			for (int dtNdx2 = 0; dtNdx2 < DE_LENGTH_OF_ARRAY(allTypes); dtNdx2++)
			{
				const VkComponentTypeKHR	inputType = (VkComponentTypeKHR)allTypes[dtNdx1];
				const VkComponentTypeKHR	outputType = (VkComponentTypeKHR)allTypes[dtNdx2];
				const string			name2	= string("input_") + string(componentTypeInfo[inputType].typeName) + string("_output_") + string(componentTypeInfo[outputType].typeName);
				de::MovePtr<tcu::TestCaseGroup> dtGroup(new tcu::TestCaseGroup(testCtx, name2.c_str(), ""));
				for (int scNdx = 0; scNdx < DE_LENGTH_OF_ARRAY(scCases); scNdx++)
				{
					de::MovePtr<tcu::TestCaseGroup> scGroup(new tcu::TestCaseGroup(testCtx, scCases[scNdx].name, scCases[scNdx].description));
					for (int colNdx = 0; colNdx < DE_LENGTH_OF_ARRAY(colCases); colNdx++)
					{

						CaseDef c =
						{
							TT_CONVERT,							//  TestType							testtype;
							2u,									//  deUint32							subgroupsPerWorkgroupX;
							2u,									//  deUint32							subgroupsPerWorkgroupY;
							4u,									//  deUint32							workgroupsX;
							4u,									//  deUint32							workgroupsY;
							inputType,							//  VkComponentTypeKHR					inputType;
							outputType,							//  VkComponentTypeKHR					outputType;
							!!colCases[colNdx].value,			//  bool								colMajor;
							(StorageClass)scCases[scNdx].value,	//  StorageClass						storageClass;
							useType,							//  UseType								useType;
							SUBGROUP_SIZE_NONE,					//  SubgroupSizeMode					subgroupSizeMode;
							computePipelineConstructionType,	//  vk::ComputePipelineConstructionType	computePipelineConstructionType;
						};

						scGroup->addChild(new CooperativeMatrixTestCase(testCtx, colCases[colNdx].name, colCases[colNdx].description, c));
					}
					dtGroup->addChild(scGroup.release());
				}
				ttGroup->addChild(dtGroup.release());
			}
		}
		group->addChild(ttGroup.release());
	}

	return group.release();
}

}	// anonymous

tcu::TestCaseGroup* createCooperativeMatrixTests (tcu::TestContext& testCtx, vk::ComputePipelineConstructionType computePipelineConstructionType)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "cooperative_matrix", "Cooperative matrix tests"));

	group->addChild(createCooperativeMatrixTestsInternal(testCtx, computePipelineConstructionType, UT_NV));
	group->addChild(createCooperativeMatrixTestsInternal(testCtx, computePipelineConstructionType, UT_KHR_A));
	group->addChild(createCooperativeMatrixTestsInternal(testCtx, computePipelineConstructionType, UT_KHR_B));
	group->addChild(createCooperativeMatrixTestsInternal(testCtx, computePipelineConstructionType, UT_KHR_Result));

	return group.release();
}

}	// compute
}	// vkt
