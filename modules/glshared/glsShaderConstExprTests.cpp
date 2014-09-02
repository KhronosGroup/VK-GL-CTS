/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL Shared Module
 * -------------------------------------------------
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
 * \brief Shared shader constant expression test components
 *//*--------------------------------------------------------------------*/

#include "glsShaderConstExprTests.hpp"
#include "glsShaderLibrary.hpp"
#include "glsShaderLibraryCase.hpp"

#include "tcuStringTemplate.hpp"

#include "deStringUtil.hpp"
#include "deMath.h"

namespace deqp
{
namespace gls
{
namespace ShaderConstExpr
{

std::vector<tcu::TestNode*> createTests (tcu::TestContext&			testContext,
										 glu::RenderContext&		renderContext,
										 const glu::ContextInfo&	contextInfo,
										 const TestParams*			cases,
										 int						numCases,
										 glu::GLSLVersion			version,
										 TestShaderStage			testStage)
{
	using std::string;
	using std::vector;
	using gls::sl::ShaderCase;

	// Needed for autogenerating shader code for increased component counts
	DE_STATIC_ASSERT(glu::TYPE_FLOAT+1 == glu::TYPE_FLOAT_VEC2);
	DE_STATIC_ASSERT(glu::TYPE_FLOAT+2 == glu::TYPE_FLOAT_VEC3);
	DE_STATIC_ASSERT(glu::TYPE_FLOAT+3 == glu::TYPE_FLOAT_VEC4);

	DE_STATIC_ASSERT(glu::TYPE_INT+1 == glu::TYPE_INT_VEC2);
	DE_STATIC_ASSERT(glu::TYPE_INT+2 == glu::TYPE_INT_VEC3);
	DE_STATIC_ASSERT(glu::TYPE_INT+3 == glu::TYPE_INT_VEC4);

	DE_STATIC_ASSERT(glu::TYPE_UINT+1 == glu::TYPE_UINT_VEC2);
	DE_STATIC_ASSERT(glu::TYPE_UINT+2 == glu::TYPE_UINT_VEC3);
	DE_STATIC_ASSERT(glu::TYPE_UINT+3 == glu::TYPE_UINT_VEC4);

	DE_STATIC_ASSERT(glu::TYPE_BOOL+1 == glu::TYPE_BOOL_VEC2);
	DE_STATIC_ASSERT(glu::TYPE_BOOL+2 == glu::TYPE_BOOL_VEC3);
	DE_STATIC_ASSERT(glu::TYPE_BOOL+3 == glu::TYPE_BOOL_VEC4);

	DE_ASSERT(testStage);

	const char* shaderTemplateSrc =
		"#version ${GLES_VERSION}\n"
		"precision highp float;\n"
		"precision highp int;\n"
		"${DECLARATIONS}\n"
		"void main()\n"
		"{\n"
		"	const ${CASE_BASE_TYPE} cval = ${CASE_EXPRESSION};\n"
		"	out0 = cval;\n"
		"	${OUTPUT}\n"
		"}\n";

	const tcu::StringTemplate		shaderTemplate	(shaderTemplateSrc);
	vector<tcu::TestNode*>			ret;
	vector<ShaderCase::ValueBlock>	shaderOutput	(1);

	shaderOutput[0].arrayLength = 1;
	shaderOutput[0].values.push_back(ShaderCase::Value());
	shaderOutput[0].values[0].storageType		= ShaderCase::Value::STORAGE_OUTPUT;
	shaderOutput[0].values[0].valueName			= "out0";
	shaderOutput[0].values[0].dataType			= glu::TYPE_FLOAT;
	shaderOutput[0].values[0].arrayLength		= 1;
	shaderOutput[0].values[0].elements.push_back(ShaderCase::Value::Element());

	for (int caseNdx = 0; caseNdx < numCases; caseNdx++)
	{
		std::map<string, string>	shaderTemplateParams;
		const int					minComponents	= cases[caseNdx].minComponents;
		const int					maxComponents	= cases[caseNdx].maxComponents;
		const DataType				inType			= cases[caseNdx].inType;
		const DataType				outType			= cases[caseNdx].outType;
		const string				expression		= cases[caseNdx].expression;
		// Check for presence of func(vec, scalar) style specialization, use as gatekeeper for applying said specialization
		const bool					alwaysScalar	= expression.find("${MT}")!=string::npos;
		ShaderCase::Value&			expected		= shaderOutput[0].values[0];

		switch (outType)
		{
			case glu::TYPE_INT:
				expected.elements[0].int32 = (int)cases[caseNdx].output;
				break;

			case glu::TYPE_UINT:
				expected.elements[0].int32 = (unsigned int)cases[caseNdx].output;
				break;

			case glu::TYPE_BOOL:
				expected.elements[0].bool32 = cases[caseNdx].output!=0.0f;
				break;

			case glu::TYPE_FLOAT:
				expected.elements[0].float32 = cases[caseNdx].output;
				break;

			default:
				DE_ASSERT(false);
		}

		expected.dataType = outType;

		shaderTemplateParams["GLES_VERSION"]	= version == glu::GLSL_VERSION_300_ES ? "300 es" : "100";
		shaderTemplateParams["CASE_BASE_TYPE"]	= glu::getDataTypeName(outType);
		shaderTemplateParams["DECLARATIONS"]	= "${DECLARATIONS}";
		shaderTemplateParams["OUTPUT"]			= "${OUTPUT}";

		for (int compCount = minComponents-1; compCount < maxComponents; compCount++)
		{
			vector<tcu::TestNode*>		children;
			std::map<string, string>	expressionTemplateParams;
			string						typeName			= glu::getDataTypeName((glu::DataType)(inType + compCount)); // results in float, vec2, vec3, vec4 progression (same for other primitive types)
			const char*					componentAccess[]	= {"", ".y", ".z", ".w"};
			const tcu::StringTemplate	expressionTemplate	(expression);
			// Add type to case name if we are generating multiple versions
			const string				caseName			= string(cases[caseNdx].name) + (minComponents==maxComponents ? "" : ("_" + typeName));

			// ${T} => final type, ${MT} => final type but with scalar version usable even when T is a vector
			expressionTemplateParams["T"]			= typeName;
			expressionTemplateParams["MT"]			= typeName;

			shaderTemplateParams["CASE_EXPRESSION"]	= expressionTemplate.specialize(expressionTemplateParams) + componentAccess[compCount]; // Add vector access to expression as needed

			{
				const string mapped = shaderTemplate.specialize(shaderTemplateParams);

				if (testStage & SHADER_VERTEX)
					ret.push_back(new ShaderCase(testContext,
												 renderContext,
												 contextInfo,
												 (caseName + "_vertex").c_str(),
												 "",
												 ShaderCase::ShaderCaseSpecification::generateSharedSourceVertexCase(ShaderCase::EXPECT_PASS,
																													 version,
																													 shaderOutput,
																													 mapped)));

				if (testStage & SHADER_FRAGMENT)
					ret.push_back(new ShaderCase(testContext,
												 renderContext,
												 contextInfo,
												 (caseName + "_fragment").c_str(),
												 "",
												 ShaderCase::ShaderCaseSpecification::generateSharedSourceFragmentCase(ShaderCase::EXPECT_PASS,
																													   version,
																													   shaderOutput,
																													   mapped)));
			}

			// Deal with functions that allways accept one ore more scalar parameters even when others are vectors
			if (alwaysScalar && compCount > 0)
			{
				const string	scalarCaseName	= string(cases[caseNdx].name) + "_" + typeName + "_" + glu::getDataTypeName(inType);

				expressionTemplateParams["MT"] = glu::getDataTypeName(inType);
				shaderTemplateParams["CASE_EXPRESSION"]	= expressionTemplate.specialize(expressionTemplateParams) + componentAccess[compCount];

				{
					const string mapped = shaderTemplate.specialize(shaderTemplateParams);

					if (testStage & SHADER_VERTEX)
						ret.push_back(new ShaderCase(testContext,
													 renderContext,
													 contextInfo,
													 (scalarCaseName + "_vertex").c_str(),
													 "",
													 ShaderCase::ShaderCaseSpecification::generateSharedSourceVertexCase(ShaderCase::EXPECT_PASS,
																														 version,
																														 shaderOutput,
																														 mapped)));

					if (testStage & SHADER_FRAGMENT)
						ret.push_back(new ShaderCase(testContext,
													 renderContext,
													 contextInfo,
													 (scalarCaseName + "_fragment").c_str(),
													 "",
													 ShaderCase::ShaderCaseSpecification::generateSharedSourceFragmentCase(ShaderCase::EXPECT_PASS,
																														   version,
																														   shaderOutput,
																														   mapped)));
				}
			}
		}
	}

	return ret;
}

} // ShaderConstExpr
} // gls
} // deqp
