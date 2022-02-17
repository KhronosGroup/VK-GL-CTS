/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
 * Copyright (c) 2018-2019 NVIDIA Corporation
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
	TT_NEGATE,
	TT_MATRIXTIMESSCALAR,
	TT_FUNC,
	TT_MATRIXMULADD,
	TT_COMPOSITE_ARRAY,
	TT_MATRIXMULADD_ARRAY,
} TestType;

typedef enum
{
	SC_BUFFER = 0,
	SC_WORKGROUP,
	SC_WORKGROUP_VARIABLE_POINTERS,
	SC_BUFFER_VARIABLE_POINTERS,
	SC_PHYSICAL_STORAGE_BUFFER,
} StorageClass;

const VkFlags allShaderStages = VK_SHADER_STAGE_COMPUTE_BIT;

struct CaseDef
{
	TestType testType;
	deUint32 subgroupsPerWorkgroupX;
	deUint32 subgroupsPerWorkgroupY;
	deUint32 workgroupsX;
	deUint32 workgroupsY;
	VkComponentTypeNV inputType;
	VkComponentTypeNV outputType;
	bool colMajor;
	StorageClass storageClass;
};

class CooperativeMatrixTestInstance : public TestInstance
{
public:
						CooperativeMatrixTestInstance	(Context& context, const CaseDef& data);
						~CooperativeMatrixTestInstance	(void);
	tcu::TestStatus		iterate				(void);
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

CooperativeMatrixTestCase::~CooperativeMatrixTestCase	(void)
{
}

void CooperativeMatrixTestCase::checkSupport(Context& context) const
{
	if (!context.contextSupports(vk::ApiVersion(1, 1, 0)))
	{
		TCU_THROW(NotSupportedError, "Vulkan 1.1 not supported");
	}

	if (!context.getCooperativeMatrixFeatures().cooperativeMatrix)
	{
		TCU_THROW(NotSupportedError, "cooperativeMatrix not supported");
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
		(m_data.inputType == VK_COMPONENT_TYPE_FLOAT16_NV || m_data.outputType == VK_COMPONENT_TYPE_FLOAT16_NV))
	{
		TCU_THROW(NotSupportedError, "shaderFloat16 not supported");
	}

	deUint32 propertyCount = 0;
	VkCooperativeMatrixPropertiesNV *pProperties;
	context.getInstanceInterface().getPhysicalDeviceCooperativeMatrixPropertiesNV(context.getPhysicalDevice(), &propertyCount, DE_NULL);
	if (propertyCount == 0)
		TCU_THROW(NotSupportedError, "cooperative matrices not supported");

	bool supported[2] = { false, false };
	pProperties = new VkCooperativeMatrixPropertiesNV[propertyCount];

	for (deUint32 i = 0; i < propertyCount; ++i)
	{
		VkCooperativeMatrixPropertiesNV *p = &pProperties[i];
		p->sType = VK_STRUCTURE_TYPE_COOPERATIVE_MATRIX_PROPERTIES_NV;
		p->pNext = DE_NULL;
	}

	context.getInstanceInterface().getPhysicalDeviceCooperativeMatrixPropertiesNV(context.getPhysicalDevice(), &propertyCount, pProperties);

	for (deUint32 i = 0; i < propertyCount; ++i)
	{
		VkCooperativeMatrixPropertiesNV *p = &pProperties[i];
		if (m_data.testType == TT_MATRIXMULADD ||
			m_data.testType == TT_MATRIXMULADD_ARRAY)
		{
			if (p->AType == m_data.inputType &&
				p->BType == m_data.inputType &&
				p->CType == m_data.outputType &&
				p->DType == m_data.outputType &&
				p->scope == VK_SCOPE_SUBGROUP_NV)
			{
				supported[0] = supported[1] = true;
			}
		}
		else
		{
			VkComponentTypeNV types[2] = { m_data.inputType, m_data.outputType };

			for (deUint32 j = 0; j < 2; ++j)
			{
				if (p->scope == VK_SCOPE_SUBGROUP_NV && (p->AType == types[j] || p->BType == types[j] || p->CType == types[j] || p->DType == types[j]))
				{
					supported[j] = true;
				}
			}
		}
	}

	delete [] pProperties;

	if (!supported[0] || !supported[1])
		TCU_THROW(NotSupportedError, "cooperative matrix combination not supported");
}

struct {
	const char *typeName;
	const char *coopmatTypeName;
	deUint32 bits;
} componentTypeInfo[] =
{
	{ "float16_t",	"fcoopmatNV",	16 },
	{ "float32_t",	"fcoopmatNV",	32 },
	{ "float64_t",	"fcoopmatNV",	64 },
	{ "int8_t",		"icoopmatNV",	8 },
	{ "int16_t",	"icoopmatNV",	16 },
	{ "int32_t",	"icoopmatNV",	32 },
	{ "int64_t",	"icoopmatNV",	64 },
	{ "uint8_t",	"ucoopmatNV",	8 },
	{ "uint16_t",	"ucoopmatNV",	16 },
	{ "uint32_t",	"ucoopmatNV",	32 },
	{ "uint64_t",	"ucoopmatNV",	64 },
};

static bool isFloatType(VkComponentTypeNV t)
{
	switch (t)
	{
	default:
		return false;
	case VK_COMPONENT_TYPE_FLOAT16_NV:
	case VK_COMPONENT_TYPE_FLOAT32_NV:
	case VK_COMPONENT_TYPE_FLOAT64_NV:
		return true;
	}
}

static bool isSIntType(VkComponentTypeNV t)
{
	switch (t)
	{
	default:
		return false;
	case VK_COMPONENT_TYPE_SINT8_NV:
	case VK_COMPONENT_TYPE_SINT16_NV:
	case VK_COMPONENT_TYPE_SINT32_NV:
	case VK_COMPONENT_TYPE_SINT64_NV:
		return true;
	}
}

void CooperativeMatrixTestCase::initPrograms (SourceCollections& programCollection) const
{
	std::stringstream css;
	css << "#version 450 core\n";
	css << "#pragma use_vulkan_memory_model\n";
	css <<
		"#extension GL_KHR_shader_subgroup_basic : enable\n"
		"#extension GL_KHR_memory_scope_semantics : enable\n"
		"#extension GL_NV_cooperative_matrix : enable\n"
		"#extension GL_NV_integer_cooperative_matrix : enable\n"
		"#extension GL_EXT_shader_explicit_arithmetic_types_float16 : enable\n"
		"#extension GL_EXT_shader_explicit_arithmetic_types_float32 : enable\n"
		"#extension GL_EXT_shader_explicit_arithmetic_types_int8 : enable\n"
		"#extension GL_EXT_shader_explicit_arithmetic_types_int32 : enable\n"
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

	if (m_data.testType == TT_MATRIXMULADD ||
		m_data.testType == TT_MATRIXMULADD_ARRAY)
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

	matAType  << componentTypeInfo[m_data.inputType].coopmatTypeName << "<" << componentTypeInfo[m_data.inputType].bits << ", gl_ScopeSubgroup, " << dims[0].rows << ", " << dims[0].cols << ">";
	matBType  << componentTypeInfo[m_data.inputType].coopmatTypeName << "<" << componentTypeInfo[m_data.inputType].bits << ", gl_ScopeSubgroup, " << dims[1].rows << ", " << dims[1].cols << ">";
	matCType  << componentTypeInfo[m_data.outputType].coopmatTypeName << "<" << componentTypeInfo[m_data.outputType].bits << ", gl_ScopeSubgroup, " << dims[2].rows << ", " << dims[2].cols << ">";
	outputMatType << componentTypeInfo[m_data.outputType].coopmatTypeName << "<" << componentTypeInfo[m_data.outputType].bits << ", gl_ScopeSubgroup, " << dims[3].rows << ", " << dims[3].cols << ">";

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

	const char *colMajor = (m_data.colMajor ? "true" : "false");

	if (m_data.storageClass == SC_WORKGROUP || m_data.storageClass == SC_WORKGROUP_VARIABLE_POINTERS)
	{
		css <<  "   coopMatLoadNV(matA, sharedA, elementS0, " << strides[0] << ", " << colMajor << ");\n"
				"   coopMatLoadNV(matB, sharedB, elementS1, " << strides[1] << ", " << colMajor << ");\n"
				"   coopMatLoadNV(matC, sharedC, elementS2, " << strides[2] << ", " << colMajor << ");\n";
	}
	else
	{
		css << "   coopMatLoadNV(matA, inputA.x, element0, " << strides[0] << ", " << colMajor << ");\n"
			   "   coopMatLoadNV(matB, inputB.x, element1, " << strides[1] << ", " << colMajor << ");\n"
			   "   coopMatLoadNV(matC, inputC.x, element2, " << strides[2] << ", " << colMajor << ");\n";
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
	case TT_COMPOSITE_RVALUE:
		css << "   for (int i = 0; i < matA.length(); ++i) {\n"
			   "       matO[i] = matA[i] + matB[i];\n"
			   "   }\n";
		if (m_data.testType == TT_COMPOSITE_RVALUE)
		{
			css << "   " << matAType.str() << " t = matA;\n"
				   "   matO[0] = (t += matB)[0];\n"
				   "   if (matA.length() > 0) {\n"
				   "       t = matA;\n"
				   "       matO[1] = (t += matB)[1];\n"
				   "   }\n";
		}
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
	case TT_NEGATE:
		css << "   matO = -matA;\n";
		break;
	case TT_FUNC:
		css << "   matO = f(matA);\n";
		break;
	case TT_MATRIXTIMESSCALAR:
		css << "   matO = (" << typeStrA << "(2.0)*matA)*" << typeStrA << "(3.0);\n";
		break;
	case TT_MATRIXMULADD:
		css << "   matO = coopMatMulAddNV(matA, matB, matC);\n";
		break;
	case TT_MATRIXMULADD_ARRAY:
		css << "   matOArr[1] = coopMatMulAddNV(matAArr[1], matBArr[1], matCArr[1]);\n";
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
		css << "   coopMatStoreNV(matO, sharedO, elementS3, " << sharedStride << ", " << colMajor << ");\n";
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
		css << "   coopMatStoreNV(matO, outputO.x, element3, " << strides[3] << ", " << colMajor << ");\n";
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

static void setDataFloat(void *base, VkComponentTypeNV dt, deUint32 i, float value)
{
	if (dt == VK_COMPONENT_TYPE_FLOAT32_NV)
	{
		((float *)base)[i] = value;
	}
	else
	{
		DE_ASSERT(dt == VK_COMPONENT_TYPE_FLOAT16_NV);
		((deFloat16 *)base)[i] = deFloat32To16(value);
	}
}

static float getDataFloat(void *base, VkComponentTypeNV dt, deUint32 i)
{
	if (dt == VK_COMPONENT_TYPE_FLOAT32_NV)
	{
		return ((float *)base)[i];
	}
	else
	{
		DE_ASSERT(dt == VK_COMPONENT_TYPE_FLOAT16_NV);
		return deFloat16To32(((deFloat16 *)base)[i]);
	}
}

static void setDataInt(void *base, VkComponentTypeNV dt, deUint32 i, deUint32 value)
{
	DE_ASSERT(componentTypeInfo[dt].bits <= 32);
	switch (dt) {
	default: DE_ASSERT(0); // fallthrough
	case VK_COMPONENT_TYPE_UINT8_NV:	((deUint8  *)base)[i] = (deUint8)value; break;
	case VK_COMPONENT_TYPE_UINT16_NV:	((deUint16 *)base)[i] = (deUint16)value; break;
	case VK_COMPONENT_TYPE_UINT32_NV:	((deUint32 *)base)[i] = (deUint32)value; break;
	case VK_COMPONENT_TYPE_SINT8_NV:	((deInt8  *)base)[i] = (deInt8)value; break;
	case VK_COMPONENT_TYPE_SINT16_NV:	((deInt16 *)base)[i] = (deInt16)value; break;
	case VK_COMPONENT_TYPE_SINT32_NV:	((deInt32 *)base)[i] = (deInt32)value; break;
	}
}

static deUint32 getDataInt(void *base, VkComponentTypeNV dt, deUint32 i)
{
	DE_ASSERT(componentTypeInfo[dt].bits <= 32);
	switch (dt) {
	default: DE_ASSERT(0); // fallthrough
	case VK_COMPONENT_TYPE_UINT8_NV:	return ((deUint8  *)base)[i];
	case VK_COMPONENT_TYPE_UINT16_NV:	return ((deUint16 *)base)[i];
	case VK_COMPONENT_TYPE_UINT32_NV:	return ((deUint32 *)base)[i];
	case VK_COMPONENT_TYPE_SINT8_NV:	return ((deInt8  *)base)[i];
	case VK_COMPONENT_TYPE_SINT16_NV:	return ((deInt16 *)base)[i];
	case VK_COMPONENT_TYPE_SINT32_NV:	return ((deInt32 *)base)[i];
	}
}

tcu::TestStatus CooperativeMatrixTestInstance::iterate (void)
{
	const DeviceInterface&	vk						= m_context.getDeviceInterface();
	const VkDevice			device					= m_context.getDevice();
	Allocator&				allocator				= m_context.getDefaultAllocator();
	MemoryRequirement		memoryDeviceAddress		= m_data.storageClass == SC_PHYSICAL_STORAGE_BUFFER &&
													  m_context.isDeviceFunctionalitySupported("VK_KHR_buffer_device_address") ? MemoryRequirement::DeviceAddress : MemoryRequirement::Any;
	qpTestResult			finalres				= QP_TEST_RESULT_PASS;
	tcu::TestLog&			log						= m_context.getTestContext().getLog();

	deRandom rnd;
	deRandom_init(&rnd, 1234);

	vk::VkPhysicalDeviceSubgroupProperties subgroupProperties;
	deMemset(&subgroupProperties, 0, sizeof(subgroupProperties));
	subgroupProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;

	vk::VkPhysicalDeviceProperties2 properties2;
	deMemset(&properties2, 0, sizeof(properties2));
	properties2.sType = vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	properties2.pNext = &subgroupProperties;

	m_context.getInstanceInterface().getPhysicalDeviceProperties2(m_context.getPhysicalDevice(), &properties2);

	deUint32 propertyCount = 0;
	VkCooperativeMatrixPropertiesNV *pProperties;
	m_context.getInstanceInterface().getPhysicalDeviceCooperativeMatrixPropertiesNV(m_context.getPhysicalDevice(), &propertyCount, DE_NULL);
	// Shouldn't have made it through checkSupport without any properties
	DE_ASSERT(propertyCount != 0);

	pProperties = new VkCooperativeMatrixPropertiesNV[propertyCount];

	for (deUint32 i = 0; i < propertyCount; ++i)
	{
		VkCooperativeMatrixPropertiesNV *p = &pProperties[i];
		p->sType = VK_STRUCTURE_TYPE_COOPERATIVE_MATRIX_PROPERTIES_NV;
		p->pNext = DE_NULL;
	}

	m_context.getInstanceInterface().getPhysicalDeviceCooperativeMatrixPropertiesNV(m_context.getPhysicalDevice(), &propertyCount, pProperties);

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

	if (m_data.testType == TT_MATRIXMULADD ||
		m_data.testType == TT_MATRIXMULADD_ARRAY)
	{
		for (deUint32 i = 0; i < propertyCount; ++i)
		{
			VkCooperativeMatrixPropertiesNV *p = &pProperties[i];

			if (p->AType == m_data.inputType &&
				p->BType == m_data.inputType &&
				p->CType == m_data.outputType &&
				p->DType == m_data.outputType &&
				p->scope == VK_SCOPE_SUBGROUP_NV)
			{
				testSizes.push_back(TestTuple(p->MSize, p->NSize, p->KSize));
			}
		}
	}
	else
	{
		set<TestTuple> typeSizes[2];
		VkComponentTypeNV types[2] = { m_data.inputType, m_data.outputType };

		for (deUint32 i = 0; i < propertyCount; ++i)
		{
			VkCooperativeMatrixPropertiesNV *p = &pProperties[i];

			if (p->scope != VK_SCOPE_SUBGROUP_NV)
				continue;

			for (deUint32 j = 0; j < 2; ++j)
			{
				// For these tests, m_data.M/N are always the matrix size. Check if they match
				// any input or output in the list.
				if (p->AType == types[j])
					typeSizes[j].insert(TestTuple(p->MSize, p->KSize, 0));
				if (p->BType == types[j])
					typeSizes[j].insert(TestTuple(p->KSize, p->NSize, 0));
				if (p->CType == types[j] ||
					p->DType == types[j])
					typeSizes[j].insert(TestTuple(p->MSize, p->NSize, 0));
			}
		}
		// Test those sizes that are supported for both the input and output type.
		std::set_intersection(typeSizes[0].begin(), typeSizes[0].end(),
							  typeSizes[1].begin(), typeSizes[1].end(),
							  std::back_inserter(testSizes));
	}

	delete [] pProperties;

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

		if (m_data.testType == TT_MATRIXMULADD ||
			m_data.testType == TT_MATRIXMULADD_ARRAY)
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

		VkComponentTypeNV dataTypes[4];
		size_t elementSize[4];
		VkDeviceSize bufferSizes[5];
		de::MovePtr<BufferWithMemory> buffers[5];
		vk::VkDescriptorBufferInfo bufferDescriptors[5];
		deUint32 strides[4]; // in elements
		deUint32 totalElements[4];

		for (deUint32 i = 0; i < 5; ++i)
		{
			if (i < 4)
			{
				// A/B use input type, C/D use output type
				dataTypes[i] = (i < 2) ? m_data.inputType : m_data.outputType;
				elementSize[i] = componentTypeInfo[dataTypes[i]].bits / 8;

				strides[i] = (m_data.colMajor ? dims[i].rows : dims[i].cols) * m_data.subgroupsPerWorkgroupX * m_data.workgroupsX;
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
			const bool useKHR = m_context.isDeviceFunctionalitySupported("VK_KHR_buffer_device_address");

			VkBufferDeviceAddressInfo info =
			{
				VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,		// VkStructureType	 sType;
				DE_NULL,											// const void*		 pNext;
				0,													// VkBuffer			buffer
			};
			VkDeviceAddress *addrsInMemory = (VkDeviceAddress *)ptrs[4];
			for (deUint32 i = 0; i < 4; ++i)
			{
				info.buffer = **buffers[i];
				VkDeviceAddress addr;
				if (useKHR)
					addr = vk.getBufferDeviceAddress(device, &info);
				else
					addr = vk.getBufferDeviceAddressEXT(device, &info);
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

		const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,				// sType
			DE_NULL,													// pNext
			(VkPipelineLayoutCreateFlags)0,
			1,															// setLayoutCount
			&descriptorSetLayout.get(),									// pSetLayouts
			0u,															// pushConstantRangeCount
			DE_NULL,													// pPushConstantRanges
		};

		Move<VkPipelineLayout> pipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo, NULL);

		Move<VkPipeline> pipeline;

		VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;

		const deUint32 specData[9] =
		{
			subgroupProperties.subgroupSize * m_data.subgroupsPerWorkgroupX,
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
					if (m_data.testType != TT_MATRIXMULADD &&
						m_data.testType != TT_MATRIXMULADD_ARRAY)
						setDataFloat(ptrs[i], dataTypes[i], j, ((float)(deRandom_getUint32(&rnd) & 0xff) - 64.0f)/2.0f);
					else
						setDataFloat(ptrs[i], dataTypes[i], j, ((float)(deRandom_getUint32(&rnd) & 0xf) - 4.0f)/2.0f);
				}
				else
					setDataInt(ptrs[i], dataTypes[i], j, (deRandom_getUint32(&rnd) & 0xff) - 128);
			}

		flushAlloc(vk, device, buffers[0]->getAllocation());
		flushAlloc(vk, device, buffers[1]->getAllocation());
		flushAlloc(vk, device, buffers[2]->getAllocation());
		flushAlloc(vk, device, buffers[3]->getAllocation());

		const Unique<VkShaderModule>	shader						(createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0));

		const VkPipelineShaderStageCreateInfo	shaderCreateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			DE_NULL,
			(VkPipelineShaderStageCreateFlags)0,
			VK_SHADER_STAGE_COMPUTE_BIT,								// stage
			*shader,													// shader
			"main",
			&specInfo,													// pSpecializationInfo
		};

		const VkComputePipelineCreateInfo		pipelineCreateInfo =
		{
			VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
			DE_NULL,
			0u,															// flags
			shaderCreateInfo,											// cs
			*pipelineLayout,											// layout
			(vk::VkPipeline)0,											// basePipelineHandle
			0u,															// basePipelineIndex
		};
		pipeline = createComputePipeline(vk, device, DE_NULL, &pipelineCreateInfo, NULL);

		const VkQueue					queue					= m_context.getUniversalQueue();
		Move<VkCommandPool>				cmdPool					= createCommandPool(vk, device, 0, m_context.getUniversalQueueFamilyIndex());
		Move<VkCommandBuffer>			cmdBuffer				= allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

		beginCommandBuffer(vk, *cmdBuffer, 0u);

		vk.cmdBindDescriptorSets(*cmdBuffer, bindPoint, *pipelineLayout, 0u, 1, &*descriptorSet, 0u, DE_NULL);
		vk.cmdBindPipeline(*cmdBuffer, bindPoint, *pipeline);

		vk.cmdDispatch(*cmdBuffer, m_data.workgroupsX, m_data.workgroupsY, 1);

		endCommandBuffer(vk, *cmdBuffer);

		submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

		invalidateAlloc(vk, device, buffers[3]->getAllocation());

		qpTestResult res = QP_TEST_RESULT_PASS;

		if (isFloatType(dataTypes[0]))
		{
			if (m_data.testType != TT_MATRIXMULADD &&
				m_data.testType != TT_MATRIXMULADD_ARRAY)
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
						if (output != (float)(N*M/subgroupProperties.subgroupSize) &&
							res == QP_TEST_RESULT_PASS)
							res = QP_TEST_RESULT_QUALITY_WARNING;
						break;
					case TT_CONSTANT:
						if (output != 1.0f)
							res = QP_TEST_RESULT_FAIL;
						break;
					case TT_CONVERT:
						if (output != inputA)
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
							float ulp = (m_data.inputType == VK_COMPONENT_TYPE_FLOAT16_NV) ? 1.0f/1024.0f : 1.0f/(8.0f*1024.0f*1024.0f);
							// division allows 2.5ulp, but we'll use 3.
							ulp *= 3;
							if (inputB != 0 && fabs(output - inputA / inputB) > ulp * fabs(inputA / inputB))
								res = QP_TEST_RESULT_FAIL;
						}
						break;
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
										ik = mX * M + i + strides[0] * (mY * K + k);
									else
										ik = mX * K + k + strides[0] * (mY * M + i);

									float Aik = getDataFloat(ptrs[0], dataTypes[0], ik);

									if (m_data.colMajor)
										kj = mX * K + k + strides[1] * (mY * N + j);
									else
										kj = mX * N + j + strides[1] * (mY * K + k);

									float Bkj = getDataFloat(ptrs[1], dataTypes[1], kj);

									ref += Aik*Bkj;
								}

								if (m_data.colMajor)
									ij = mX * M + i + strides[2] * (mY * N + j);
								else
									ij = mX * N + j + strides[2] * (mY * M + i);

								float Cij = getDataFloat(ptrs[2], dataTypes[2], ij);

								ref += Cij;

								float Dij = getDataFloat(ptrs[3], dataTypes[3], ij);

								if (ref != Dij)
								{
									res = QP_TEST_RESULT_FAIL;
								}
							}
						}
					}
				}
			}
		} else {
			if (m_data.testType != TT_MATRIXMULADD &&
				m_data.testType != TT_MATRIXMULADD_ARRAY)
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
						if (output != N*M/subgroupProperties.subgroupSize &&
							res == QP_TEST_RESULT_PASS)
							res = QP_TEST_RESULT_QUALITY_WARNING;
						break;
					case TT_CONSTANT:
						if (output != 1)
							res = QP_TEST_RESULT_FAIL;
						break;
					case TT_CONVERT:
						if (output != inputA)
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
										ik = mX * M + i + strides[0] * (mY * K + k);
									else
										ik = mX * K + k + strides[0] * (mY * M + i);

									deUint32 Aik = getDataInt(ptrs[0], dataTypes[0], ik);

									if (m_data.colMajor)
										kj = mX * K + k + strides[1] * (mY * N + j);
									else
										kj = mX * N + j + strides[1] * (mY * K + k);

									deUint32 Bkj = getDataInt(ptrs[1], dataTypes[1], kj);

									ref += Aik*Bkj;
								}

								if (m_data.colMajor)
									ij = mX * M + i + strides[2] * (mY * N + j);
								else
									ij = mX * N + j + strides[2] * (mY * M + i);

								deUint32 Cij = getDataInt(ptrs[2], dataTypes[2], ij);

								ref += Cij;

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
			log << tcu::TestLog::Message << "failed with M = " << M << ", N = " << N << ", K = " << K << tcu::TestLog::EndMessage;
			finalres = res;
		}
	}

	return tcu::TestStatus(finalres, qpGetTestResultName(finalres));
}

}	// anonymous

tcu::TestCaseGroup*	createCooperativeMatrixTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(
			testCtx, "cooperative_matrix", "GL_NV_cooperative_matrix tests"));

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

	TestGroupCase ttCases[] =
	{
		{ TT_LENGTH,				"length",					"OpCooperativeMatrixLengthNV"	},
		{ TT_CONSTANT,				"constant",					"OpConstantComposite"			},
		{ TT_CONVERT,				"convert",					"OpFConvert/OpSConvert/OpUConvert"	},
		{ TT_COMPOSITE,				"composite",				"OpCompositeConstruct"			},
		{ TT_COMPOSITE_RVALUE,		"composite_rvalue",			"OpCompositeExtract"			},
		{ TT_ADD,					"add",						"OpFAdd/OpIAdd"					},
		{ TT_SUB,					"sub",						"OpFSub/OpISub"					},
		{ TT_DIV,					"div",						"OpFDiv/OpSDiv/OpUDiv"			},
		{ TT_NEGATE,				"negate",					"OpFNegate/OpSNegate"			},
		{ TT_MATRIXTIMESSCALAR,		"matrixtimesscalar",		"OpMatrixTimesScalar"			},
		{ TT_FUNC,					"func",						"OpFunctionParameter"			},
		{ TT_MATRIXMULADD,			"matrixmuladd",				"OpCooperativeMatrixMulAddNV"	},
		{ TT_COMPOSITE_ARRAY,		"composite_array",			"OpCompositeConstruct w/array"			},
		{ TT_MATRIXMULADD_ARRAY,	"matrixmuladd_array",		"OpCooperativeMatrixMulAddNV w/array"	},
	};

	TestGroupCase2 dtCases[] =
	{
		{ { VK_COMPONENT_TYPE_FLOAT32_NV,	VK_COMPONENT_TYPE_FLOAT32_NV },	"float32_float32",	"A/B are fp32 C/D are fp32"		},
		{ { VK_COMPONENT_TYPE_FLOAT32_NV,	VK_COMPONENT_TYPE_FLOAT16_NV },	"float32_float16",	"A/B are fp32 C/D are fp16"		},
		{ { VK_COMPONENT_TYPE_FLOAT16_NV,	VK_COMPONENT_TYPE_FLOAT32_NV },	"float16_float32",	"A/B are fp16 C/D are fp32"		},
		{ { VK_COMPONENT_TYPE_FLOAT16_NV,	VK_COMPONENT_TYPE_FLOAT16_NV },	"float16_float16",	"A/B are fp16 C/D are fp16"		},
		{ { VK_COMPONENT_TYPE_UINT8_NV,		VK_COMPONENT_TYPE_UINT8_NV },	"uint8_uint8",		"A/B are u8 C/D are u8"			},
		{ { VK_COMPONENT_TYPE_UINT8_NV,		VK_COMPONENT_TYPE_UINT32_NV },	"uint8_uint32",		"A/B are u8 C/D are u32"		},
		{ { VK_COMPONENT_TYPE_SINT8_NV,		VK_COMPONENT_TYPE_SINT8_NV },	"sint8_sint8",		"A/B are s8 C/D are s8"			},
		{ { VK_COMPONENT_TYPE_SINT8_NV,		VK_COMPONENT_TYPE_SINT32_NV },	"sint8_sint32",		"A/B are s8 C/D are s32"		},
		{ { VK_COMPONENT_TYPE_UINT32_NV,	VK_COMPONENT_TYPE_UINT32_NV },	"uint32_uint32",	"A/B are u32 C/D are u32"		},
		{ { VK_COMPONENT_TYPE_UINT32_NV,	VK_COMPONENT_TYPE_UINT8_NV },	"uint32_uint8",		"A/B are u32 C/D are u8"		},
		{ { VK_COMPONENT_TYPE_SINT32_NV,	VK_COMPONENT_TYPE_SINT32_NV },	"sint32_sint32",	"A/B are s32 C/D are s32"		},
		{ { VK_COMPONENT_TYPE_SINT32_NV,	VK_COMPONENT_TYPE_SINT8_NV },	"sint32_sint8",		"A/B are s32 C/D are s8"		},
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

	for (int ttNdx = 0; ttNdx < DE_LENGTH_OF_ARRAY(ttCases); ttNdx++)
	{
		de::MovePtr<tcu::TestCaseGroup> ttGroup(new tcu::TestCaseGroup(testCtx, ttCases[ttNdx].name, ttCases[ttNdx].description));
		for (int dtNdx = 0; dtNdx < DE_LENGTH_OF_ARRAY(dtCases); dtNdx++)
		{
			de::MovePtr<tcu::TestCaseGroup> dtGroup(new tcu::TestCaseGroup(testCtx, dtCases[dtNdx].name, dtCases[dtNdx].description));
			for (int scNdx = 0; scNdx < DE_LENGTH_OF_ARRAY(scCases); scNdx++)
			{
				de::MovePtr<tcu::TestCaseGroup> scGroup(new tcu::TestCaseGroup(testCtx, scCases[scNdx].name, scCases[scNdx].description));
				for (int colNdx = 0; colNdx < DE_LENGTH_OF_ARRAY(colCases); colNdx++)
				{
					TestType testType = (TestType)ttCases[ttNdx].value;
					VkComponentTypeNV inputType = (VkComponentTypeNV)dtCases[dtNdx].value[0];
					VkComponentTypeNV outputType = (VkComponentTypeNV)dtCases[dtNdx].value[1];

					bool isMatrixMul = testType == TT_MATRIXMULADD || testType == TT_MATRIXMULADD_ARRAY;

					if (!isMatrixMul && testType != TT_CONVERT && inputType != outputType)
						continue;

					if (testType == TT_CONVERT && inputType == outputType)
						continue;

					if (isMatrixMul && componentTypeInfo[inputType].bits > componentTypeInfo[outputType].bits)
						continue;

					CaseDef c =
					{
						testType,							// TestType testtype;
						2u,									// deUint32 subgroupsPerWorkgroupX;
						2u,									// deUint32 subgroupsPerWorkgroupY;
						4u,									// deUint32 workgroupsX;
						4u,									// deUint32 workgroupsY;
						(VkComponentTypeNV)inputType,		// VkComponentTypeNV inputType;
						(VkComponentTypeNV)outputType,		// VkComponentTypeNV outputType;
						!!colCases[colNdx].value,			// bool colMajor;
						(StorageClass)scCases[scNdx].value,	// StorageClass storageClass;
					};

					scGroup->addChild(new CooperativeMatrixTestCase(testCtx, colCases[colNdx].name, colCases[colNdx].description, c));
				}
				dtGroup->addChild(scGroup.release());
			}
			ttGroup->addChild(dtGroup.release());
		}
		group->addChild(ttGroup.release());
	}
	return group.release();
}

}	// compute
}	// vkt
