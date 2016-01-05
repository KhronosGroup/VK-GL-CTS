/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be included
 * in all copies or substantial portions of the Materials.
 *
 * The Materials are Confidential Information as defined by the
 * Khronos Membership Agreement until designated non-confidential by Khronos,
 * at which point this condition clause shall be removed.
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
 * \brief Shader built-in constant tests.
 *//*--------------------------------------------------------------------*/

#include "vktShaderBuiltinConstantTests.hpp"
#include "vktShaderExecutor.hpp"
#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"
#include "tcuTestLog.hpp"

using std::string;
using std::vector;
using tcu::TestLog;
using namespace vk;

namespace vkt
{
namespace shaderexecutor
{

namespace
{

static deUint32 getUint32 (deUint32 VkPhysicalDeviceLimits::* ptr, Context &ctx)
{
	VkPhysicalDeviceProperties properties;
	ctx.getInstanceInterface().getPhysicalDeviceProperties(ctx.getPhysicalDevice(), &properties);
	return properties.limits.*ptr;
}

template<deUint32 VkPhysicalDeviceLimits::* ptr>
static deUint32 getUint32 (Context &ctx)
{
	return getUint32(ptr, ctx);
}

#define GET_UINT32(name) getUint32<&VkPhysicalDeviceLimits::name>

static deInt32 getInt32 (deInt32 VkPhysicalDeviceLimits::* ptr, Context &ctx)
{
	VkPhysicalDeviceProperties properties;
	ctx.getInstanceInterface().getPhysicalDeviceProperties(ctx.getPhysicalDevice(), &properties);
	return properties.limits.*ptr;
}

template<deInt32 VkPhysicalDeviceLimits::* ptr>
static deInt32 getInt32 (Context &ctx)
{
	return getInt32(ptr, ctx);
}

#define GET_INT32(name) getInt32<&VkPhysicalDeviceLimits::name>

static tcu::UVec3 getUVec3 (deUint32 (VkPhysicalDeviceLimits::*ptr)[3], Context &ctx)
{
	VkPhysicalDeviceProperties properties;
	ctx.getInstanceInterface().getPhysicalDeviceProperties(ctx.getPhysicalDevice(), &properties);
	return tcu::UVec3((properties.limits.*ptr)[0], (properties.limits.*ptr)[1], (properties.limits.*ptr)[2]);
}

template<deUint32 (VkPhysicalDeviceLimits::*ptr)[3]>
static tcu::UVec3 getUVec3 (Context &ctx)
{
	return getUVec3(ptr, ctx);
}

#define GET_UVEC3(name) getUVec3<&VkPhysicalDeviceLimits::name>

static std::string makeCaseName (const std::string& varName, glu::ShaderType shaderType)
{
	DE_ASSERT(varName.length() > 3);
	DE_ASSERT(varName.substr(0,3) == "gl_");

	std::ostringstream name;
	name << de::toLower(char(varName[3]));

	for (size_t ndx = 4; ndx < varName.length(); ndx++)
	{
		const char c = char(varName[ndx]);
		if (de::isUpper(c))
			name << '_' << de::toLower(c);
		else
			name << c;
	}
	name << '_' << glu::getShaderTypeName(glu::ShaderType(shaderType));
	return name.str();
}

enum
{
	VS = (1<<glu::SHADERTYPE_VERTEX),
	TC = (1<<glu::SHADERTYPE_TESSELLATION_CONTROL),
	TE = (1<<glu::SHADERTYPE_TESSELLATION_EVALUATION),
	GS = (1<<glu::SHADERTYPE_GEOMETRY),
	FS = (1<<glu::SHADERTYPE_FRAGMENT),
	CS = (1<<glu::SHADERTYPE_COMPUTE),

	SHADER_TYPES = VS|TC|TE|GS|FS|CS
};

template<typename DataType>
class ShaderBuiltinConstantTestInstance;

template<typename DataType>
class ShaderBuiltinConstantCase : public TestCase
{
public:
	typedef DataType (*GetConstantValueFunc) (Context &);

								ShaderBuiltinConstantCase	(tcu::TestContext& testCtx, const char* varName, glu::ShaderType shaderType, GetConstantValueFunc getValue, const char* requiredExt);
	virtual						~ShaderBuiltinConstantCase	(void) {};

	virtual	void				initPrograms				(vk::SourceCollections& programCollection) const
								{
									m_executor->setShaderSources(programCollection);
								}

	virtual TestInstance* createInstance (Context& context) const { return new ShaderBuiltinConstantTestInstance<DataType>(context, m_getValue, *m_executor, m_varName); };

private:
	const std::string					m_varName;
	const GetConstantValueFunc			m_getValue;
	const std::string					m_requiredExt;
	ShaderExecutor*						m_executor;
	glu::ShaderType						m_shaderType;
	ShaderSpec							m_spec;
};

template<typename T>
struct GLConstantTypeForVKType {};

template<>
struct GLConstantTypeForVKType<tcu::UVec3>
{
	typedef tcu::IVec3 GLConstantType;
};

template<>
struct GLConstantTypeForVKType<deUint32>
{
	typedef deInt32 GLConstantType;
};

template<>
struct GLConstantTypeForVKType<deInt32>
{
	typedef deInt32 GLConstantType;
};

template<typename DataType>
ShaderBuiltinConstantCase<DataType>::ShaderBuiltinConstantCase (tcu::TestContext& testCtx, const char* varName, glu::ShaderType shaderType, GetConstantValueFunc getValue, const char* requiredExt)
	: TestCase		(testCtx, makeCaseName(varName, shaderType).c_str(), varName)
	, m_varName		(varName)
	, m_getValue	(getValue)
	, m_requiredExt	(requiredExt ? requiredExt : "")
	, m_shaderType	(shaderType)
{
	DE_ASSERT(!requiredExt == m_requiredExt.empty());

	ShaderSpec	shaderSpec;
	shaderSpec.source	= string("result = ") + m_varName + ";\n";
	shaderSpec.outputs.push_back(Symbol("result", glu::VarType(glu::dataTypeOf<typename GLConstantTypeForVKType<DataType>::GLConstantType>(), glu::PRECISION_HIGHP)));

	if (!m_requiredExt.empty())
		shaderSpec.globalDeclarations = "#extension " + m_requiredExt + " : require\n";

	m_executor = createExecutor(shaderType, shaderSpec);
}

template<typename DataType>
static void logVarValue (tcu::TestLog& log, const std::string& varName, DataType value)
{
	log << TestLog::Message << varName << " = " << value << TestLog::EndMessage;
}

template<>
void logVarValue<int> (tcu::TestLog& log, const std::string& varName, int value)
{
	log << TestLog::Integer(varName, varName, "", QP_KEY_TAG_NONE, value);
}

// ShaderBuiltinConstantTestInstance

template<typename DataType>
class ShaderBuiltinConstantTestInstance : public TestInstance
{
public:
								ShaderBuiltinConstantTestInstance (Context& ctx, typename ShaderBuiltinConstantCase<DataType>::GetConstantValueFunc getValue, ShaderExecutor& executor, const std::string varName )
									: TestInstance	(ctx)
									, m_getValue	(getValue)
									, m_testCtx		(ctx.getTestContext())
									, m_executor	(executor)
									, m_varName		(varName)
								{}
	virtual tcu::TestStatus		iterate (void);

private:
	const typename ShaderBuiltinConstantCase<DataType>::GetConstantValueFunc	m_getValue;
	tcu::TestContext&															m_testCtx;
	ShaderExecutor&																m_executor;
	const std::string															m_varName;

	typedef typename GLConstantTypeForVKType<DataType>::GLConstantType GLConstantType;
	GLConstantType getReference (void);
};

template<typename DataType>
typename ShaderBuiltinConstantTestInstance<DataType>::GLConstantType ShaderBuiltinConstantTestInstance<DataType>::getReference (void)
{
	return m_getValue(m_context);
}

template<>
typename ShaderBuiltinConstantTestInstance<tcu::UVec3>::GLConstantType ShaderBuiltinConstantTestInstance<tcu::UVec3>::getReference (void)
{
	return m_getValue(m_context).asInt();
}

template<typename DataType>
tcu::TestStatus ShaderBuiltinConstantTestInstance<DataType>::iterate (void)
{
	GLConstantType			reference	= getReference();
	GLConstantType			result		= GLConstantType(-1);
	void* const				outputs		= &result;

	m_executor.execute(m_context, 1, DE_NULL, &outputs);
	logVarValue(m_testCtx.getLog(), m_varName, result);

	if (result != reference)
	{
		m_testCtx.getLog() << TestLog::Message << "ERROR: Expected " << m_varName << " = " << reference << TestLog::EndMessage;
		return tcu::TestStatus::fail("Invalid builtin constant value");
	}
	else
		return tcu::TestStatus::pass("Pass");
}

// createShaderBuiltinConstantCase

template<typename DataType>
void createShaderBuiltinConstantCase(tcu::TestCaseGroup* group, tcu::TestContext& testCtx, const char* varName, typename ShaderBuiltinConstantCase<DataType>::GetConstantValueFunc getValue, const char* requiredExt)
{
	for (int shaderType = 0; shaderType < glu::SHADERTYPE_LAST; shaderType++)
	{
		if ((SHADER_TYPES & (1<<shaderType)) != 0)
			group->addChild(new ShaderBuiltinConstantCase<DataType>(testCtx, varName, static_cast<glu::ShaderType>(shaderType), getValue, requiredExt));
	}
}

} // anonymous

ShaderBuiltinConstantTests::ShaderBuiltinConstantTests (tcu::TestContext& testCtx)
	: TestCaseGroup(testCtx, "constant", "Built-in Constant Tests")
{
}

ShaderBuiltinConstantTests::~ShaderBuiltinConstantTests (void)
{
}

void ShaderBuiltinConstantTests::init (void)
{
	// Core builtin constants
	{
		static const struct
		{
			const char*													varName;
			ShaderBuiltinConstantCase<deUint32>::GetConstantValueFunc	getValue;
		} uintConstants[] =
		{
			{ "gl_MaxVertexAttribs",					GET_UINT32(maxVertexInputAttributes)					},
			{ "gl_MaxVertexOutputVectors",				GET_UINT32(maxVertexOutputComponents)					},
			{ "gl_MaxFragmentInputVectors",				GET_UINT32(maxFragmentInputComponents)					},
			{ "gl_MaxDrawBuffers",						GET_UINT32(maxColorAttachments)							},
			{ "gl_MaxProgramTexelOffset",				GET_UINT32(maxTexelOffset)								},
		};

		static const struct
		{
			const char*													varName;
			ShaderBuiltinConstantCase<tcu::UVec3>::GetConstantValueFunc	getValue;
		} uvec3Constants[] =
		{
			{ "gl_MaxComputeWorkGroupCount",			GET_UVEC3(maxComputeWorkGroupCount)						},
			{ "gl_MaxComputeWorkGroupSize",				GET_UVEC3(maxComputeWorkGroupSize)						},
		};

		tcu::TestCaseGroup* const coreGroup = new tcu::TestCaseGroup(m_testCtx, "core", "Core Specification");
		addChild(coreGroup);

		for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(uintConstants); ndx++)
			createShaderBuiltinConstantCase<deUint32>(coreGroup, m_testCtx, uintConstants[ndx].varName, uintConstants[ndx].getValue, DE_NULL);

		createShaderBuiltinConstantCase<deInt32>(coreGroup, m_testCtx, "gl_MinProgramTexelOffset", GET_INT32(minTexelOffset), DE_NULL);

		for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(uvec3Constants); ndx++)
			createShaderBuiltinConstantCase<tcu::UVec3>(coreGroup, m_testCtx, uvec3Constants[ndx].varName, uvec3Constants[ndx].getValue, DE_NULL);
	}

	// EXT_geometry_shader
	{
		static const struct
		{
			const char*													varName;
			ShaderBuiltinConstantCase<deUint32>::GetConstantValueFunc	getValue;
		} uintConstants[] =
		{
			{ "gl_MaxGeometryInputComponents",			GET_UINT32(maxGeometryInputComponents)					},
			{ "gl_MaxGeometryOutputComponents",			GET_UINT32(maxGeometryOutputComponents)					},
			{ "gl_MaxGeometryOutputVertices",			GET_UINT32(maxGeometryOutputVertices)					},
			{ "gl_MaxGeometryTotalOutputComponents",	GET_UINT32(maxGeometryTotalOutputComponents)			},
		};

		tcu::TestCaseGroup* const geomGroup = new tcu::TestCaseGroup(m_testCtx, "geometry_shader", "GL_EXT_geometry_shader");
		addChild(geomGroup);

		for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(uintConstants); ndx++)
			createShaderBuiltinConstantCase<deUint32>(geomGroup, m_testCtx, uintConstants[ndx].varName, uintConstants[ndx].getValue, "GL_EXT_geometry_shader");
	}

	// EXT_tessellation_shader
	{
		static const struct
		{
			const char*													varName;
			ShaderBuiltinConstantCase<deUint32>::GetConstantValueFunc	getValue;
		} uintConstants[] =
		{
			{ "gl_MaxTessControlInputComponents",			GET_UINT32(maxTessellationControlPerVertexInputComponents)		},
			{ "gl_MaxTessControlOutputComponents",			GET_UINT32(maxTessellationControlPerVertexOutputComponents)		},
			{ "gl_MaxTessControlTotalOutputComponents",		GET_UINT32(maxTessellationControlTotalOutputComponents)			},

			{ "gl_MaxTessEvaluationInputComponents",		GET_UINT32(maxTessellationEvaluationInputComponents)			},
			{ "gl_MaxTessEvaluationOutputComponents",		GET_UINT32(maxTessellationEvaluationOutputComponents)			},

			{ "gl_MaxTessPatchComponents",					GET_UINT32(maxTessellationControlPerPatchOutputComponents)		},

			{ "gl_MaxPatchVertices",						GET_UINT32(maxTessellationPatchSize)							},
			{ "gl_MaxTessGenLevel",							GET_UINT32(maxTessellationGenerationLevel)						},
		};

		tcu::TestCaseGroup* const tessGroup = new tcu::TestCaseGroup(m_testCtx, "tessellation_shader", "GL_EXT_tessellation_shader");
		addChild(tessGroup);

		for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(uintConstants); ndx++)
			createShaderBuiltinConstantCase<deUint32>(tessGroup, m_testCtx, uintConstants[ndx].varName, uintConstants[ndx].getValue, "GL_EXT_tessellation_shader");
	}
}

} // shaderexecutor
} // vkt
