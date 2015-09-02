#ifndef _GLSSHADERLIBRARYCASE_HPP
#define _GLSSHADERLIBRARYCASE_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL (ES) Module
 * -----------------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
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
 * \brief Shader test case.
 *//*--------------------------------------------------------------------*/

#include "gluDefs.hpp"
#include "gluShaderUtil.hpp"
#include "gluRenderContext.hpp"
#include "gluShaderProgram.hpp"
#include "tcuTestCase.hpp"
#include "tcuSurface.hpp"

#include <string>
#include <vector>

namespace deqp
{
namespace gls
{
namespace sl
{

// ShaderCase node.

class ShaderCase : public tcu::TestCase
{
public:
	enum CaseType
	{
		CASETYPE_COMPLETE = 0,		//!< Has all shaders specified separately.
		CASETYPE_VERTEX_ONLY,		//!< "Both" case, vertex shader sub case.
		CASETYPE_FRAGMENT_ONLY,		//!< "Both" case, fragment shader sub case.

		CASETYPE_LAST
	};

	enum ExpectResult
	{
		EXPECT_PASS = 0,
		EXPECT_COMPILE_FAIL,
		EXPECT_LINK_FAIL,
		EXPECT_COMPILE_LINK_FAIL,
		EXPECT_VALIDATION_FAIL,
		EXPECT_BUILD_SUCCESSFUL,

		EXPECT_LAST
	};

	struct Value
	{
		enum StorageType
		{
			STORAGE_UNIFORM,
			STORAGE_INPUT,
			STORAGE_OUTPUT,

			STORAGE_LAST
		};

		/* \todo [2010-03-31 petri] Replace with another vector to allow a) arrays, b) compact representation */
		union Element
		{
			float		float32;
			deInt32		int32;
			deInt32		bool32;
		};

		StorageType				storageType;
		std::string				valueName;
		glu::DataType			dataType;
		int						arrayLength;	// Number of elements in array (currently always 1).
		std::vector<Element>	elements;		// Scalar values (length dataType.scalarSize * arrayLength).
	};

	struct ValueBlock
	{
							ValueBlock (void);

		int					arrayLength;		// Combined array length of each value (lengths must be same, or one).
		std::vector<Value>	values;
	};

	class CaseRequirement
	{
	public:
		enum RequirementType
		{
			REQUIREMENTTYPE_EXTENSION = 0,
			REQUIREMENTTYPE_IMPLEMENTATION_LIMIT,
			REQUIREMENTTYPE_FULL_GLSL_ES_100_SPEC,	//!< Full support (as opposed to limited as specified for GLES 2.0 (See GLSL Appendix A)) cannot be queried

			REQUIREMENTTYPE_LAST
		};

									CaseRequirement								(void);

		static CaseRequirement		createAnyExtensionRequirement				(const std::vector<std::string>& requirements, deUint32 effectiveShaderStageFlags);
		static CaseRequirement		createLimitRequirement						(deUint32 enumName, int ref);
		static CaseRequirement		createFullGLSLES100SpecificationRequirement	(void);
		void						checkRequirements							(glu::RenderContext& renderCtx, const glu::ContextInfo& contextInfo);

		RequirementType				getType										(void) const { return m_type; };
		std::string					getSupportedExtension						(void) const { DE_ASSERT(m_type == REQUIREMENTTYPE_EXTENSION); DE_ASSERT(m_supportedExtensionNdx >= 0); return m_extensions[m_supportedExtensionNdx]; }
		deUint32					getAffectedExtensionStageFlags				(void) const { DE_ASSERT(m_type == REQUIREMENTTYPE_EXTENSION); return m_effectiveShaderStageFlags; }

	private:
		RequirementType				m_type;

		// REQUIREMENTTYPE_EXTENSION:
		std::vector<std::string>	m_extensions;
		int							m_supportedExtensionNdx;
		deUint32					m_effectiveShaderStageFlags;

		// REQUIREMENTTYPE_IMPLEMENTATION_LIMIT:
		deUint32					m_enumName;
		int							m_referenceValue;
	};

	struct ShaderCaseSpecification
	{
										ShaderCaseSpecification				(void);

		static ShaderCaseSpecification	generateSharedSourceVertexCase		(ExpectResult expectResult_, glu::GLSLVersion targetVersion_, const std::vector<ValueBlock>& values, const std::string& sharedSource);
		static ShaderCaseSpecification	generateSharedSourceFragmentCase	(ExpectResult expectResult_, glu::GLSLVersion targetVersion_, const std::vector<ValueBlock>& values, const std::string& sharedSource);

		ExpectResult					expectResult;
		glu::GLSLVersion				targetVersion;
		CaseType						caseType;
		std::vector<CaseRequirement>	requirements;
		std::vector<ValueBlock>			valueBlocks;
		std::vector<std::string>		vertexSources;
		std::vector<std::string>		fragmentSources;
		std::vector<std::string>		tessCtrlSources;
		std::vector<std::string>		tessEvalSources;
		std::vector<std::string>		geometrySources;
	};

	struct PipelineProgram
	{
		deUint32						activeStageBits;
		std::vector<CaseRequirement>	requirements;
		std::vector<std::string>		vertexSources;
		std::vector<std::string>		fragmentSources;
		std::vector<std::string>		tessCtrlSources;
		std::vector<std::string>		tessEvalSources;
		std::vector<std::string>		geometrySources;
	};

	struct PipelineCaseSpecification
	{
		ExpectResult					expectResult;
		glu::GLSLVersion				targetVersion;
		CaseType						caseType;
		std::vector<ValueBlock>			valueBlocks;
		std::vector<PipelineProgram>	programs;
	};

	// Methods.
									ShaderCase										(tcu::TestContext&				testCtx,
																					 glu::RenderContext&			renderCtx,
																					 const glu::ContextInfo&		contextInfo,
																					 const char*					caseName,
																					 const char*					description,
																					 const ShaderCaseSpecification&	specification);
									ShaderCase										(tcu::TestContext&					testCtx,
																					 glu::RenderContext&				renderCtx,
																					 const glu::ContextInfo&			contextInfo,
																					 const char*						caseName,
																					 const char*						description,
																					 const PipelineCaseSpecification&	specification);
	virtual							~ShaderCase										(void);

private:
	void							init											(void);
	bool							execute											(void);
	IterateResult					iterate											(void);

									ShaderCase										(const ShaderCase&);		// not allowed!
	ShaderCase&						operator=										(const ShaderCase&);		// not allowed!

	std::string						genVertexShader									(const ValueBlock& valueBlock) const;
	std::string						genFragmentShader								(const ValueBlock& valueBlock) const;
	std::string						specializeVertexShader							(const char* src, const ValueBlock& valueBlock) const;
	std::string						specializeFragmentShader						(const char* src, const ValueBlock& valueBlock) const;
	void							specializeVertexShaders							(glu::ProgramSources& dst, const std::vector<std::string>& sources, const ValueBlock& valueBlock, const std::vector<ShaderCase::CaseRequirement>& requirements) const;
	void							specializeFragmentShaders						(glu::ProgramSources& dst, const std::vector<std::string>& sources, const ValueBlock& valueBlock, const std::vector<ShaderCase::CaseRequirement>& requirements) const;
	void							specializeGeometryShaders						(glu::ProgramSources& dst, const std::vector<std::string>& sources, const ValueBlock& valueBlock, const std::vector<ShaderCase::CaseRequirement>& requirements) const;
	void							specializeTessControlShaders					(glu::ProgramSources& dst, const std::vector<std::string>& sources, const ValueBlock& valueBlock, const std::vector<ShaderCase::CaseRequirement>& requirements) const;
	void							specializeTessEvalShaders						(glu::ProgramSources& dst, const std::vector<std::string>& sources, const ValueBlock& valueBlock, const std::vector<ShaderCase::CaseRequirement>& requirements) const;
	bool							isTessellationPresent							(void) const;
	bool							anyProgramRequiresFullGLSLES100Specification	(void) const;

	void							dumpValues										(const ValueBlock& valueBlock, int arrayNdx);

	bool 							checkPixels										(tcu::Surface& surface, int minX, int maxX, int minY, int maxY);

	struct ProgramObject
	{
		glu::ProgramSources		programSources;
		PipelineProgram			spec;
	};

	// Member variables.
	glu::RenderContext&				m_renderCtx;
	const glu::ContextInfo&			m_contextInfo;
	const CaseType					m_caseType;
	const ExpectResult				m_expectResult;
	const glu::GLSLVersion			m_targetVersion;
	const bool						m_separatePrograms;
	std::vector<ValueBlock>			m_valueBlocks;
	std::vector<ProgramObject>		m_programs;
};

} // sl
} // gls
} // deqp

#endif // _GLSSHADERLIBRARYCASE_HPP
