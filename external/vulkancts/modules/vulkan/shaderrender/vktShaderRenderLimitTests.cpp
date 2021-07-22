/*------------------------------------------------------------------------
* Vulkan Conformance Tests
* ------------------------
*
* Copyright (c) 2018 Google Inc.
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
* \brief Shader limit tests.
*//*--------------------------------------------------------------------*/

#include "vktShaderRenderLimitTests.hpp"
#include "vktShaderRender.hpp"
#include "tcuImageCompare.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuTestLog.hpp"
#include "vktDrawUtil.hpp"
#include "deMath.h"

using namespace std;
using namespace tcu;
using namespace vk;
using namespace de;

namespace vkt
{
using namespace drawutil;

namespace sr
{

namespace
{

class FragmentInputComponentCaseInstance : public ShaderRenderCaseInstance
{
public:
	FragmentInputComponentCaseInstance (Context& context);

	TestStatus		iterate(void);
	virtual void	setupDefaultInputs(void);

private:
	const Vec4		m_constantColor;
};

FragmentInputComponentCaseInstance::FragmentInputComponentCaseInstance (Context& context)
	: ShaderRenderCaseInstance (context)
	, m_constantColor	(0.1f, 0.05f, 0.2f, 0.0f)
{
}

TestStatus FragmentInputComponentCaseInstance::iterate (void)
{
	const UVec2		viewportSize	= getViewportSize();
	const int		width			= viewportSize.x();
	const int		height			= viewportSize.y();
	const tcu::RGBA	threshold		(2, 2, 2, 2);
	Surface			resImage		(width, height);
	Surface			refImage		(width, height);
	bool			compareOk		= false;

	const deUint16	indices[12]		=
	{
		0, 4, 1,
		0, 5, 4,
		1, 2, 3,
		1, 3, 4
	};

	setup();
	render(6, 4, indices);
	copy(resImage.getAccess(), getResultImage().getAccess());

	// Reference image
	for (int y = 0; y < refImage.getHeight(); y++)
	{
		for (int x = 0; x < refImage.getWidth(); x++)
			refImage.setPixel(x, y, RGBA(0, 255, 0, 255));
	}

	compareOk = pixelThresholdCompare(m_context.getTestContext().getLog(), "Result", "Image comparison result", refImage, resImage, threshold, COMPARE_LOG_RESULT);

	if (compareOk)
		return TestStatus::pass("Result image matches reference");
	else
		return TestStatus::fail("Image mismatch");
}

void FragmentInputComponentCaseInstance::setupDefaultInputs (void)
{
	const float vertices[] =
	{
		-1.0f, -1.0f, 0.0f, 1.0f,
		 0.0f, -1.0f, 0.0f, 1.0f,
		 1.0f, -1.0f, 0.0f, 1.0f,
		 1.0f,  1.0f, 0.0f, 1.0f,
		 0.0f,  1.0f, 0.0f, 1.0f,
		-1.0f,  1.0f, 0.0f, 1.0f
	};

	addAttribute(0u, VK_FORMAT_R32G32B32A32_SFLOAT, deUint16(sizeof(float) * 4), 6, vertices);
}

class FragmentInputComponentCase : public TestCase
{
public:
	FragmentInputComponentCase	(TestContext& testCtx, const string& name, const string& description, const deUint16 inputComponents);
	virtual						~FragmentInputComponentCase(void);

	void						initPrograms(SourceCollections& dst) const;
	TestInstance*				createInstance(Context& context) const;

private:
	FragmentInputComponentCase	(const FragmentInputComponentCase&);
	const deUint16				m_inputComponents;
};

FragmentInputComponentCase::FragmentInputComponentCase (TestContext& testCtx, const string& name, const string& description, const deUint16 inputComponents)
	: TestCase			(testCtx, name, description)
	, m_inputComponents	(inputComponents)
{
}

FragmentInputComponentCase::~FragmentInputComponentCase (void)
{
}

void FragmentInputComponentCase::initPrograms (SourceCollections& dst) const
{
	const tcu::StringTemplate	vertexCodeTemplate(
		"#version 450\n"
		"layout(location = 0) in highp vec4 a_position;\n"
		"${VARYING_OUT}"
		"void main (void)\n"
		"{\n"
		"    gl_Position = a_position;\n"
		    "${VARYING_DECL}"
		"}\n");

	const tcu::StringTemplate	fragmentCodeTemplate(
		"#version 450\n"
		"layout(location = 0) out highp vec4 o_color;\n"
		"${VARYING_IN}"
		"void main (void)\n"
		"{\n"
		"    int errorCount = 0;\n"
		    "${VERIFY}"
		"\n"
		"    if (errorCount == 0)\n"
		"        o_color = vec4(0.0, 1.0, 0.0, 1.0);\n"
		"    else\n"
		"        o_color = vec4(1.0, 0.0, 0.0, 1.0);\n"
		"}\n");

	//
	// The number of vertex output/fragment input components is *inclusive* of any built-ins being used,
	// since gl_Position is always output by the shader, this actually means that there are n - 4 components
	// available as user specified output data.
	//
	// [14.1.4. Location Assignment, para 11]
	//
	// "The number of input and output locations available for a shader input or output
	//  interface are limited, and dependent on the shader stage as described in Shader
	//  Input and Output Locations. All variables in both the built-in interface block
	//  and the user-defined variable interface count against these limits."
	//
	// So, as an example, the '128' component variant of this test will specify 124 user
	// declared outputs in addition to gl_Position.

	deUint16					maxLocations	= (deUint16)deCeilToInt32((float)(m_inputComponents - 4) / 4u);
	string						varyingType;
	map<string, string>			vertexParams;
	map<string, string>			fragmentParams;

	for (deUint16 loc = 0; loc < maxLocations; loc++)
	{
		if (loc == (maxLocations - 1u))
		{
			switch (m_inputComponents - loc * 4u)
			{
			case 1:
				varyingType = "float";
				break;
			case 2:
				varyingType = "vec2";
				break;
			case 3:
				varyingType = "vec3";
				break;
			default:
				varyingType = "vec4";
			}
		}
		else
			varyingType = "vec4";

		vertexParams["VARYING_OUT"]		+= "layout(location = "			+ de::toString(loc)	+ ") out highp "	+ varyingType	+ " o_color"	+ de::toString(loc)	+ ";\n";
		vertexParams["VARYING_DECL"]	+= "    o_color"				+ de::toString(loc)	+ " = "				+ varyingType	+ "("			+ de::toString(loc)	+ ".0);\n";
		fragmentParams["VARYING_IN"]	+= "layout(location = "			+ de::toString(loc)	+ ") in highp "		+ varyingType	+ " i_color"	+ de::toString(loc)	+ ";\n";
		fragmentParams["VERIFY"]		+= "    errorCount += (i_color"	+ de::toString(loc)	+ " == "			+ varyingType	+ "("			+ de::toString(loc)	+ ".0)) ? 0 : 1;\n";
	}

	dst.glslSources.add("vert") << glu::VertexSource(vertexCodeTemplate.specialize(vertexParams));
	dst.glslSources.add("frag") << glu::FragmentSource(fragmentCodeTemplate.specialize(fragmentParams));
}

TestInstance* FragmentInputComponentCase::createInstance (Context& context) const
{
	const InstanceInterface&		vki							= context.getInstanceInterface();
	const VkPhysicalDevice			physDevice					= context.getPhysicalDevice();
	const VkPhysicalDeviceLimits	limits						= getPhysicalDeviceProperties(vki, physDevice).limits;
	const deUint16					maxFragmentInputComponents	= (deUint16)limits.maxFragmentInputComponents;
	const deUint16					maxVertexOutputComponents	= (deUint16)limits.maxVertexOutputComponents;

	if (m_inputComponents > maxFragmentInputComponents)
	{
		const std::string notSupportedStr = "Unsupported number of fragment input components (" +
											de::toString(m_inputComponents) +
											") maxFragmentInputComponents=" + de::toString(maxFragmentInputComponents);
		TCU_THROW(NotSupportedError, notSupportedStr.c_str());
	}

	// gl_Position counts as an output component as well, so outputComponents = inputComponents + 4
	if (m_inputComponents + 4 > maxVertexOutputComponents)
	{
		const std::string notSupportedStr = "Unsupported number of user specified vertex output components (" +
											de::toString(m_inputComponents + 4) +
											") maxVertexOutputComponents=" + de::toString(maxVertexOutputComponents);
		TCU_THROW(NotSupportedError, notSupportedStr.c_str());
	}

	return new FragmentInputComponentCaseInstance(context);
}
} // anonymous

TestCaseGroup* createLimitTests (TestContext& testCtx)
{
	de::MovePtr<TestCaseGroup> limitGroup			(new TestCaseGroup(testCtx,	"limits",			"Shader device limit tests"));
	de::MovePtr<TestCaseGroup> nearGroup			(new TestCaseGroup(testCtx, "near_max",			"Shaders near maximum values"));

	de::MovePtr<TestCaseGroup> inputComponentsGroup	(new TestCaseGroup(testCtx,	"fragment_input",	"Fragment input component variations"));

	// Fragment input component case
	deUint16 fragmentComponentMaxLimits [] = { 64u, 128u, 256u };

	for (deUint16 limitNdx = 0; limitNdx < DE_LENGTH_OF_ARRAY(fragmentComponentMaxLimits); limitNdx++)
	{
		for (deInt16 cases = 5; cases > 0; cases--)
			inputComponentsGroup->addChild(new FragmentInputComponentCase(testCtx, "components_" + de::toString(fragmentComponentMaxLimits[limitNdx] - cases), "Input component count", (deUint16)(fragmentComponentMaxLimits[limitNdx] - cases)));
	}

	nearGroup->addChild(inputComponentsGroup.release());
	limitGroup->addChild(nearGroup.release());
	return limitGroup.release();
}

} // sr
} // vkt
