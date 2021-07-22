#ifndef _GLCSUBGROUPSTESTSUTILS_HPP
#define _GLCSUBGROUPSTESTSUTILS_HPP
/*------------------------------------------------------------------------
 * OpenGL Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017-2019 The Khronos Group Inc.
 * Copyright (c) 2017 Codeplay Software Ltd.
 * Copyright (c) 2019 NVIDIA Corporation.
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
 */ /*!
 * \file
 * \brief Subgroups tests utility classes
 */ /*--------------------------------------------------------------------*/

#include "deDefs.hpp"
#include "deSTLUtil.hpp"
#include "deStringUtil.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "glwDefs.hpp"
#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"
#include "glcTestCase.hpp"
#include "glcSpirvUtils.hpp"

#include "tcuFormatUtil.hpp"
#include "tcuTestLog.hpp"
#include "tcuVectorUtil.hpp"

#include "gluShaderUtil.hpp"
#include "gluContextInfo.hpp"

#include "deSharedPtr.hpp"
#include "deUniquePtr.hpp"

#include <string>

namespace glc
{

enum ShaderType
{
	SHADER_TYPE_GLSL = 0,
	SHADER_TYPE_SPIRV,

	SHADER_TYPE_LAST
};

template<typename Program>
class ProgramCollection
{
public:
								ProgramCollection	(void);
								~ProgramCollection	(void);

	void						clear				(void);

	Program&					add					(const std::string& name);
	void						add					(const std::string& name, de::MovePtr<Program>& program);

	bool						contains			(const std::string& name) const;
	const Program&				get					(const std::string& name) const;

	class Iterator
	{
	private:
		typedef typename std::map<std::string, Program*>::const_iterator	IteratorImpl;

	public:
		explicit			Iterator	(const IteratorImpl& i) : m_impl(i) {}

		Iterator&			operator++	(void)			{ ++m_impl; return *this;	}
		const Program&		operator*	(void) const	{ return getProgram();		}

		const std::string&	getName		(void) const	{ return m_impl->first;		}
		const Program&		getProgram	(void) const	{ return *m_impl->second;	}

		bool				operator==	(const Iterator& other) const	{ return m_impl == other.m_impl;	}
		bool				operator!=	(const Iterator& other) const	{ return m_impl != other.m_impl;	}

	private:

		IteratorImpl	m_impl;
	};

	Iterator					begin				(void) const { return Iterator(m_programs.begin());	}
	Iterator					end					(void) const { return Iterator(m_programs.end());	}

	bool						empty				(void) const { return m_programs.empty();			}

private:
	typedef std::map<std::string, Program*>	ProgramMap;

	ProgramMap					m_programs;
};

template<typename Program>
ProgramCollection<Program>::ProgramCollection (void)
{
}

template<typename Program>
ProgramCollection<Program>::~ProgramCollection (void)
{
	clear();
}

template<typename Program>
void ProgramCollection<Program>::clear (void)
{
	for (typename ProgramMap::const_iterator i = m_programs.begin(); i != m_programs.end(); ++i)
		delete i->second;
	m_programs.clear();
}

template<typename Program>
Program& ProgramCollection<Program>::add (const std::string& name)
{
	DE_ASSERT(!contains(name));
	de::MovePtr<Program> prog = de::newMovePtr<Program>();
	m_programs[name] = prog.get();
	prog.release();
	return *m_programs[name];
}

template<typename Program>
void ProgramCollection<Program>::add (const std::string& name, de::MovePtr<Program>& program)
{
	DE_ASSERT(!contains(name));
	m_programs[name] = program.get();
	program.release();
}

template<typename Program>
bool ProgramCollection<Program>::contains (const std::string& name) const
{
	return de::contains(m_programs, name);
}

template<typename Program>
const Program& ProgramCollection<Program>::get (const std::string& name) const
{
	DE_ASSERT(contains(name));
	return *m_programs.find(name)->second;
}

struct GlslSource
{
	std::vector<std::string>	sources[glu::SHADERTYPE_LAST];

	GlslSource&					operator<< (const glu::ShaderSource& shaderSource)
	{
		sources[shaderSource.shaderType].push_back(shaderSource.source);
		return *this;
	}
};

typedef ProgramCollection<GlslSource>		SourceCollections;


class Context
{
public:
	Context (deqp::Context& deqpCtx)
		: m_deqpCtx(deqpCtx)
		, m_sourceCollection()
		, m_glslVersion(glu::getContextTypeGLSLVersion(m_deqpCtx.getRenderContext().getType()))
		, m_shaderType(SHADER_TYPE_GLSL)
		{}
	~Context (void) {}
	deqp::Context&			getDeqpContext		(void) const { return m_deqpCtx; }
	SourceCollections&		getSourceCollection (void) { return m_sourceCollection; }
	glu::GLSLVersion		getGLSLVersion		(void) { return m_glslVersion; }
	ShaderType				getShaderType		(void) { return m_shaderType; }
	void					setShaderType		(ShaderType type) { m_shaderType = type; }

protected:
	deqp::Context&		m_deqpCtx;
	SourceCollections	m_sourceCollection;
	glu::GLSLVersion	m_glslVersion;
	ShaderType			m_shaderType;
};

namespace subgroups
{

template<typename Arg0>
class SubgroupFactory : public deqp::TestCase
{
public:
	//void initPrograms(SourceCollections& programCollection, CaseDefinition caseDef)
	typedef void (*InitFunction)(SourceCollections& programCollection, Arg0 arg0);
	//void supportedCheck (Context& context, CaseDefinition caseDef)
	typedef void (*SupportFunction)(Context& context, Arg0 arg0);
	//tcu::TestStatus test(Context& context, const CaseDefinition caseDef)
	typedef tcu::TestStatus (*TestFunction)(Context& context, const Arg0 arg0);

	/* Public methods */
	SubgroupFactory(deqp::Context& context, tcu::TestNodeType type, const std::string& name, const std::string& desc,
		SupportFunction suppFunc, InitFunction initFunc, TestFunction testFunc, Arg0 arg0)
		: TestCase(context, type, name.c_str(), desc.c_str())
		, m_supportedFunc(suppFunc)
		, m_initFunc(initFunc)
		, m_testFunc(testFunc)
		, m_arg0(arg0)
		, m_glcContext(m_context)
	{}

	void init()
	{
		m_supportedFunc(m_glcContext, m_arg0);

		m_initFunc(m_glcContext.getSourceCollection(), m_arg0);
	}

	void deinit()
	{
		// nothing to do
	}

	tcu::TestNode::IterateResult iterate()
	{
		DE_ASSERT(m_testFunc);
		tcu::TestLog& log = m_testCtx.getLog();

		try {
			// do SPIRV version of tests if supported
			log << tcu::TestLog::Message << "SPIRV pass beginning..." << tcu::TestLog::EndMessage;
			spirvUtils::checkGlSpirvSupported(m_glcContext.getDeqpContext());

			m_glcContext.setShaderType(SHADER_TYPE_SPIRV);

			const tcu::TestStatus result = m_testFunc(m_glcContext, m_arg0);
			if (result.isComplete())
			{
				DE_ASSERT(m_testCtx.getTestResult() == QP_TEST_RESULT_LAST);
				if (result.getCode() == QP_TEST_RESULT_PASS)
				{
					log << tcu::TestLog::Message << "SPIRV pass completed successfully ("
						<< result.getDescription() << ")." << tcu::TestLog::EndMessage;
				} else {
					// test failed - log result and stop
					m_testCtx.setTestResult(result.getCode(), result.getDescription().c_str());
					return tcu::TestNode::STOP;
				}
			}
		} catch(tcu::NotSupportedError& e)
		{
			log << tcu::TestLog::Message << "SPIRV pass skipped ("
						<< e.getMessage() << ")." << tcu::TestLog::EndMessage;
		}

		// do GLSL version of the tests
		log << tcu::TestLog::Message << "GLSL pass beginning..." << tcu::TestLog::EndMessage;
		m_glcContext.setShaderType(SHADER_TYPE_GLSL);
		const tcu::TestStatus result = m_testFunc(m_glcContext, m_arg0);

		if (result.isComplete())
		{
			DE_ASSERT(m_testCtx.getTestResult() == QP_TEST_RESULT_LAST);
			log << tcu::TestLog::Message << "GLSL pass completed successfully ("
				<< result.getDescription() << ")." << tcu::TestLog::EndMessage;
			m_testCtx.setTestResult(result.getCode(), result.getDescription().c_str());
			return tcu::TestNode::STOP;
		}

		return tcu::TestNode::CONTINUE;
	}

	static void addFunctionCaseWithPrograms (deqp::TestCaseGroup*				group,
								  const std::string&							name,
								  const std::string&							desc,
								  SupportFunction								suppFunc,
								  InitFunction									initFunc,
								  TestFunction									testFunc,
								  Arg0											arg0)
	{
		group->addChild(new SubgroupFactory(group->getContext(), tcu::NODETYPE_SELF_VALIDATE, name, desc, suppFunc, initFunc, testFunc, arg0));
	}

private:
	SupportFunction		m_supportedFunc;
	InitFunction		m_initFunc;
	TestFunction		m_testFunc;
	Arg0				m_arg0;

	Context				m_glcContext;
};


typedef enum ShaderStageFlags
{
	SHADER_STAGE_VERTEX_BIT = GL_VERTEX_SHADER_BIT,
	SHADER_STAGE_FRAGMENT_BIT = GL_FRAGMENT_SHADER_BIT,
	SHADER_STAGE_GEOMETRY_BIT = GL_GEOMETRY_SHADER_BIT,
	SHADER_STAGE_TESS_CONTROL_BIT = GL_TESS_CONTROL_SHADER_BIT,
	SHADER_STAGE_TESS_EVALUATION_BIT = GL_TESS_EVALUATION_SHADER_BIT,
	SHADER_STAGE_COMPUTE_BIT = GL_COMPUTE_SHADER_BIT,
	SHADER_STAGE_ALL_GRAPHICS = (SHADER_STAGE_VERTEX_BIT | SHADER_STAGE_FRAGMENT_BIT | SHADER_STAGE_GEOMETRY_BIT |
								 SHADER_STAGE_TESS_CONTROL_BIT | SHADER_STAGE_TESS_EVALUATION_BIT ),
	SHADER_STAGE_ALL_VALID = (SHADER_STAGE_ALL_GRAPHICS | SHADER_STAGE_COMPUTE_BIT),
} ShaderStageFlags;

typedef enum SubgroupFeatureFlags
{
    SUBGROUP_FEATURE_BASIC_BIT = GL_SUBGROUP_FEATURE_BASIC_BIT_KHR,
    SUBGROUP_FEATURE_VOTE_BIT = GL_SUBGROUP_FEATURE_VOTE_BIT_KHR,
    SUBGROUP_FEATURE_ARITHMETIC_BIT = GL_SUBGROUP_FEATURE_ARITHMETIC_BIT_KHR,
    SUBGROUP_FEATURE_BALLOT_BIT = GL_SUBGROUP_FEATURE_BALLOT_BIT_KHR,
    SUBGROUP_FEATURE_SHUFFLE_BIT = GL_SUBGROUP_FEATURE_SHUFFLE_BIT_KHR,
    SUBGROUP_FEATURE_SHUFFLE_RELATIVE_BIT = GL_SUBGROUP_FEATURE_SHUFFLE_RELATIVE_BIT_KHR,
    SUBGROUP_FEATURE_CLUSTERED_BIT = GL_SUBGROUP_FEATURE_CLUSTERED_BIT_KHR,
    SUBGROUP_FEATURE_QUAD_BIT = GL_SUBGROUP_FEATURE_QUAD_BIT_KHR,
    SUBGROUP_FEATURE_PARTITIONED_BIT_NV = GL_SUBGROUP_FEATURE_PARTITIONED_BIT_NV,
	SUBGROUP_FEATURE_ALL_VALID = (SUBGROUP_FEATURE_BASIC_BIT | SUBGROUP_FEATURE_VOTE_BIT | SUBGROUP_FEATURE_ARITHMETIC_BIT |
								  SUBGROUP_FEATURE_BALLOT_BIT | SUBGROUP_FEATURE_SHUFFLE_BIT | SUBGROUP_FEATURE_SHUFFLE_RELATIVE_BIT |
								  SUBGROUP_FEATURE_CLUSTERED_BIT | SUBGROUP_FEATURE_QUAD_BIT | SUBGROUP_FEATURE_PARTITIONED_BIT_NV),
} SubgroupFeatureFlags;

typedef enum Format
{
	FORMAT_UNDEFINED = 0,
	FORMAT_R32_SINT = GL_R32I,
	FORMAT_R32_UINT = GL_R32UI,
	FORMAT_R32G32_SINT = GL_RG32I,
	FORMAT_R32G32_UINT = GL_RG32UI,
	FORMAT_R32G32B32_SINT = GL_RGB32I,
	FORMAT_R32G32B32_UINT = GL_RGB32UI,
	FORMAT_R32G32B32A32_SINT = GL_RGBA32I,
	FORMAT_R32G32B32A32_UINT = GL_RGBA32UI,
	FORMAT_R32_SFLOAT = GL_R32F,
	FORMAT_R32G32_SFLOAT = GL_RG32F,
	FORMAT_R32G32B32_SFLOAT = GL_RGB32F,
	FORMAT_R32G32B32A32_SFLOAT = GL_RGBA32F,
	FORMAT_R64_SFLOAT = 0x6000,
	FORMAT_R64G64_SFLOAT,
	FORMAT_R64G64B64_SFLOAT,
	FORMAT_R64G64B64A64_SFLOAT,
	FORMAT_R32_BOOL = 0x6100,
	FORMAT_R32G32_BOOL,
	FORMAT_R32G32B32_BOOL,
	FORMAT_R32G32B32A32_BOOL,
} Format;

typedef enum DescriptorType
{
	DESCRIPTOR_TYPE_UNIFORM_BUFFER = GL_UNIFORM_BUFFER,
	DESCRIPTOR_TYPE_STORAGE_BUFFER = GL_SHADER_STORAGE_BUFFER,
	DESCRIPTOR_TYPE_STORAGE_IMAGE  = GL_TEXTURE_2D,
} DescriptorType;

// A struct to represent input data to a shader
struct SSBOData
{
	SSBOData() :
		initializeType	(InitializeNone),
		layout			(LayoutStd140),
		format			(FORMAT_UNDEFINED),
		numElements		(0),
		isImage			(false),
		binding			(0u),
		stages			((ShaderStageFlags)0u)
	{}

	enum InputDataInitializeType
	{
		InitializeNone = 0,
		InitializeNonZero,
		InitializeZero,
	} initializeType;

	enum InputDataLayoutType
	{
		LayoutStd140 = 0,
		LayoutStd430,
		LayoutPacked,
	} layout;

	Format						format;
	deUint64					numElements;
	bool						isImage;
	deUint32					binding;
	ShaderStageFlags			stages;
};

std::string getSharedMemoryBallotHelper();

deUint32 getSubgroupSize(Context& context);

deUint32 maxSupportedSubgroupSize();

std::string getShaderStageName(ShaderStageFlags stage);

std::string getSubgroupFeatureName(SubgroupFeatureFlags bit);

void addNoSubgroupShader (SourceCollections& programCollection);

std::string getVertShaderForStage(ShaderStageFlags stage);

bool isSubgroupSupported(Context& context);

bool areSubgroupOperationsSupportedForStage(
	Context& context, ShaderStageFlags stage);

bool areSubgroupOperationsRequiredForStage(ShaderStageFlags stage);

bool isSubgroupFeatureSupportedForDevice(Context& context, SubgroupFeatureFlags bit);

bool isFragmentSSBOSupportedForDevice(Context& context);

bool isVertexSSBOSupportedForDevice(Context& context);

bool isImageSupportedForStageOnDevice(Context& context, const ShaderStageFlags stage);

bool isDoubleSupportedForDevice(Context& context);

bool isDoubleFormat(Format format);

std::string getFormatNameForGLSL(Format format);

void addGeometryShadersFromTemplate (const std::string& glslTemplate, SourceCollections& collection);

void setVertexShaderFrameBuffer (SourceCollections& programCollection);

void setFragmentShaderFrameBuffer (SourceCollections& programCollection);

void setFragmentShaderFrameBuffer (SourceCollections& programCollection);

void setTesCtrlShaderFrameBuffer (SourceCollections& programCollection);

void setTesEvalShaderFrameBuffer (SourceCollections& programCollection);

bool check(std::vector<const void*> datas,
	deUint32 width, deUint32 ref);

bool checkCompute(std::vector<const void*> datas,
	const deUint32 numWorkgroups[3], const deUint32 localSize[3],
	deUint32 ref);

tcu::TestStatus makeTessellationEvaluationFrameBufferTest(Context& context, Format format,
	SSBOData* extraData, deUint32 extraDataCount,
	bool (*checkResult)(std::vector<const void*> datas, deUint32 width, deUint32 subgroupSize),
	const ShaderStageFlags shaderStage = SHADER_STAGE_ALL_GRAPHICS);

tcu::TestStatus makeGeometryFrameBufferTest(Context& context, Format format, SSBOData* extraData,
	deUint32 extraDataCount,
	bool (*checkResult)(std::vector<const void*> datas, deUint32 width, deUint32 subgroupSize));

tcu::TestStatus allStages(Context& context, Format format,
	SSBOData* extraData, deUint32 extraDataCount,
	bool (*checkResult)(std::vector<const void*> datas, deUint32 width, deUint32 subgroupSize),
	const ShaderStageFlags shaderStage);

tcu::TestStatus makeVertexFrameBufferTest(Context& context, Format format,
	SSBOData* extraData, deUint32 extraDataCount,
	bool (*checkResult)(std::vector<const void*> datas, deUint32 width, deUint32 subgroupSize));

tcu::TestStatus makeFragmentFrameBufferTest(Context& context, Format format,
	SSBOData* extraData, deUint32 extraDataCount,
	bool (*checkResult)(std::vector<const void*> datas, deUint32 width,
									 deUint32 height, deUint32 subgroupSize));

tcu::TestStatus makeComputeTest(
	Context& context, Format format, SSBOData* inputs,
	deUint32 inputsCount,
	bool (*checkResult)(std::vector<const void*> datas,
		const deUint32 numWorkgroups[3], const deUint32 localSize[3],
		deUint32 subgroupSize));
} // subgroups
} // glc

#endif // _GLCSUBGROUPSTESTSUTILS_HPP
