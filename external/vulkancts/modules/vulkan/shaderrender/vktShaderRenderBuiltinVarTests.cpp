/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 * Copyright (c) 2016 The Android Open Source Project
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
 * \brief Shader builtin variable tests.
 *//*--------------------------------------------------------------------*/

#include "vktShaderRenderBuiltinVarTests.hpp"
#include "vktShaderRender.hpp"
#include "gluShaderUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuTestLog.hpp"

#include "deMath.h"
#include "deRandom.hpp"

#include <map>

using namespace std;
using namespace tcu;
using namespace vk;

namespace vkt
{
namespace sr
{

namespace
{

class BuiltinGlFrontFacingCaseInstance : public ShaderRenderCaseInstance
{
public:
					BuiltinGlFrontFacingCaseInstance	(Context& context);

	TestStatus		iterate								(void);
	virtual void	setupDefaultInputs					(void);
};

BuiltinGlFrontFacingCaseInstance::BuiltinGlFrontFacingCaseInstance (Context& context)
	: ShaderRenderCaseInstance	(context)
{
}

TestStatus BuiltinGlFrontFacingCaseInstance::iterate (void)
{
	const UVec2		viewportSize	= getViewportSize();
	const int		width			= viewportSize.x();
	const int		height			= viewportSize.y();
	const RGBA		threshold		(2, 2, 2, 2);
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

	for (int y = 0; y < refImage.getHeight(); y++)
	{
		for (int x = 0; x < refImage.getWidth()/2; x++)
			refImage.setPixel(x, y, RGBA::green());

		for (int x = refImage.getWidth()/2; x < refImage.getWidth(); x++)
			refImage.setPixel(x, y, RGBA::blue());
	}

	compareOk = pixelThresholdCompare(m_context.getTestContext().getLog(), "Result", "Image comparison result", refImage, resImage, threshold, COMPARE_LOG_RESULT);

	if (compareOk)
		return TestStatus::pass("Result image matches reference");
	else
		return TestStatus::fail("Image mismatch");
}

void BuiltinGlFrontFacingCaseInstance::setupDefaultInputs (void)
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

	addAttribute(0u, VK_FORMAT_R32G32B32A32_SFLOAT, deUint32(sizeof(float) * 4), 6, vertices);
}

class BuiltinGlFrontFacingCase : public TestCase
{
public:
								BuiltinGlFrontFacingCase	(TestContext& testCtx, const string& name, const string& description);
	virtual						~BuiltinGlFrontFacingCase	(void);

	void						initPrograms				(SourceCollections& dst) const;
	TestInstance*				createInstance				(Context& context) const;

private:
								BuiltinGlFrontFacingCase	(const BuiltinGlFrontFacingCase&);	// not allowed!
	BuiltinGlFrontFacingCase&	operator=					(const BuiltinGlFrontFacingCase&);	// not allowed!
};

BuiltinGlFrontFacingCase::BuiltinGlFrontFacingCase (TestContext& testCtx, const string& name, const string& description)
	: TestCase(testCtx, name, description)
{
}

BuiltinGlFrontFacingCase::~BuiltinGlFrontFacingCase (void)
{
}

void BuiltinGlFrontFacingCase::initPrograms (SourceCollections& dst) const
{
	dst.glslSources.add("vert") << glu::VertexSource(
		"#version 310 es\n"
		"layout(location = 0) in highp vec4 a_position;\n"
		"void main (void)\n"
		"{\n"
		"       gl_Position = a_position;\n"
		"}\n");

	dst.glslSources.add("frag") << glu::FragmentSource(
		"#version 310 es\n"
		"layout(location = 0) out lowp vec4 o_color;\n"
		"void main (void)\n"
		"{\n"
		"       if (gl_FrontFacing)\n"
		"               o_color = vec4(0.0, 1.0, 0.0, 1.0);\n"
		"       else\n"
		"               o_color = vec4(0.0, 0.0, 1.0, 1.0);\n"
		"}\n");
}

TestInstance* BuiltinGlFrontFacingCase::createInstance (Context& context) const
{
	return new BuiltinGlFrontFacingCaseInstance(context);
}

class BuiltinGlFragCoordXYZCaseInstance : public ShaderRenderCaseInstance
{
public:
					BuiltinGlFragCoordXYZCaseInstance	(Context& context);

	TestStatus		iterate								(void);
	virtual void	setupDefaultInputs					(void);
};

BuiltinGlFragCoordXYZCaseInstance::BuiltinGlFragCoordXYZCaseInstance (Context& context)
	: ShaderRenderCaseInstance	(context)
{
}

TestStatus BuiltinGlFragCoordXYZCaseInstance::iterate (void)
{
	const UVec2		viewportSize	= getViewportSize();
	const int		width			= viewportSize.x();
	const int		height			= viewportSize.y();
	const tcu::Vec3	scale			(1.f / float(width), 1.f / float(height), 1.0f);
	const tcu::RGBA	threshold		(2, 2, 2, 2);
	Surface			resImage		(width, height);
	Surface			refImage		(width, height);
	bool			compareOk		= false;
	const deUint16	indices[6]		=
	{
		2, 1, 3,
		0, 1, 2,
	};

	setup();
	addUniform(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, scale);

	render(4, 2, indices);
	copy(resImage.getAccess(), getResultImage().getAccess());

	// Reference image
	for (int y = 0; y < refImage.getHeight(); y++)
	{
		for (int x = 0; x < refImage.getWidth(); x++)
		{
			const float	xf			= (float(x)+.5f) / float(refImage.getWidth());
			const float	yf			= (float(refImage.getHeight()-y-1)+.5f) / float(refImage.getHeight());
			const float	z			= (xf + yf) / 2.0f;
			const Vec3	fragCoord	(float(x)+.5f, float(y)+.5f, z);
			const Vec3	scaledFC	= fragCoord*scale;
			const Vec4	color		(scaledFC.x(), scaledFC.y(), scaledFC.z(), 1.0f);

			refImage.setPixel(x, y, RGBA(color));
		}
	}

	compareOk = pixelThresholdCompare(m_context.getTestContext().getLog(), "Result", "Image comparison result", refImage, resImage, threshold, COMPARE_LOG_RESULT);

	if (compareOk)
		return TestStatus::pass("Result image matches reference");
	else
		return TestStatus::fail("Image mismatch");
}

void BuiltinGlFragCoordXYZCaseInstance::setupDefaultInputs (void)
{
	const float		vertices[]		=
	{
		-1.0f,  1.0f,  0.0f, 1.0f,
		-1.0f, -1.0f,  0.5f, 1.0f,
		 1.0f,  1.0f,  0.5f, 1.0f,
		 1.0f, -1.0f,  1.0f, 1.0f,
	};

	addAttribute(0u, VK_FORMAT_R32G32B32A32_SFLOAT, deUint32(sizeof(float) * 4), 4, vertices);
}

class BuiltinGlFragCoordXYZCase : public TestCase
{
public:
								BuiltinGlFragCoordXYZCase	(TestContext& testCtx, const string& name, const string& description);
	virtual						~BuiltinGlFragCoordXYZCase	(void);

	void						initPrograms				(SourceCollections& dst) const;
	TestInstance*				createInstance				(Context& context) const;

private:
								BuiltinGlFragCoordXYZCase	(const BuiltinGlFragCoordXYZCase&);	// not allowed!
	BuiltinGlFragCoordXYZCase&	operator=					(const BuiltinGlFragCoordXYZCase&);	// not allowed!
};

BuiltinGlFragCoordXYZCase::BuiltinGlFragCoordXYZCase (TestContext& testCtx, const string& name, const string& description)
	: TestCase(testCtx, name, description)
{
}

BuiltinGlFragCoordXYZCase::~BuiltinGlFragCoordXYZCase (void)
{
}

void BuiltinGlFragCoordXYZCase::initPrograms (SourceCollections& dst) const
{
	dst.glslSources.add("vert") << glu::VertexSource(
		"#version 310 es\n"
		"layout(location = 0) in highp vec4 a_position;\n"
		"void main (void)\n"
		"{\n"
		"       gl_Position = a_position;\n"
		"}\n");

	dst.glslSources.add("frag") << glu::FragmentSource(
		"#version 310 es\n"
		"layout(set=0, binding=0) uniform Scale { highp vec3 u_scale; };\n"
		"layout(location = 0) out highp vec4 o_color;\n"
		"void main (void)\n"
		"{\n"
		"       o_color = vec4(gl_FragCoord.xyz * u_scale, 1.0);\n"
		"}\n");
}

TestInstance* BuiltinGlFragCoordXYZCase::createInstance (Context& context) const
{
	return new BuiltinGlFragCoordXYZCaseInstance(context);
}

inline float projectedTriInterpolate (const Vec3& s, const Vec3& w, float nx, float ny)
{
	return (s[0]*(1.0f-nx-ny)/w[0] + s[1]*ny/w[1] + s[2]*nx/w[2]) / ((1.0f-nx-ny)/w[0] + ny/w[1] + nx/w[2]);
}

class BuiltinGlFragCoordWCaseInstance : public ShaderRenderCaseInstance
{
public:
					BuiltinGlFragCoordWCaseInstance	(Context& context);

	TestStatus		iterate							(void);
	virtual void	setupDefaultInputs				(void);

private:

	const Vec4		m_w;

};

BuiltinGlFragCoordWCaseInstance::BuiltinGlFragCoordWCaseInstance (Context& context)
	: ShaderRenderCaseInstance	(context)
	, m_w						(1.7f, 2.0f, 1.2f, 1.0f)
{
}

TestStatus BuiltinGlFragCoordWCaseInstance::iterate (void)
{
	const UVec2		viewportSize	= getViewportSize();
	const int		width			= viewportSize.x();
	const int		height			= viewportSize.y();
	const tcu::RGBA	threshold		(2, 2, 2, 2);
	Surface			resImage		(width, height);
	Surface			refImage		(width, height);
	bool			compareOk		= false;
	const deUint16	indices[6]		=
	{
		2, 1, 3,
		0, 1, 2,
	};

	setup();
	render(4, 2, indices);
	copy(resImage.getAccess(), getResultImage().getAccess());

	// Reference image
	for (int y = 0; y < refImage.getHeight(); y++)
	{
		for (int x = 0; x < refImage.getWidth(); x++)
		{
			const float	xf			= (float(x)+.5f) / float(refImage.getWidth());
			const float	yf			= (float(refImage.getHeight()-y-1)+.5f) / float(refImage.getHeight());
			const float	oow			= ((xf + yf) < 1.0f)
										? projectedTriInterpolate(Vec3(m_w[0], m_w[1], m_w[2]), Vec3(m_w[0], m_w[1], m_w[2]), xf, yf)
										: projectedTriInterpolate(Vec3(m_w[3], m_w[2], m_w[1]), Vec3(m_w[3], m_w[2], m_w[1]), 1.0f-xf, 1.0f-yf);
			const Vec4	color		(0.0f, oow - 1.0f, 0.0f, 1.0f);

			refImage.setPixel(x, y, RGBA(color));
		}
	}

	compareOk = pixelThresholdCompare(m_context.getTestContext().getLog(), "Result", "Image comparison result", refImage, resImage, threshold, COMPARE_LOG_RESULT);

	if (compareOk)
		return TestStatus::pass("Result image matches reference");
	else
		return TestStatus::fail("Image mismatch");
}

void BuiltinGlFragCoordWCaseInstance::setupDefaultInputs (void)
{
	const float vertices[] =
	{
		-m_w[0],  m_w[0], 0.0f, m_w[0],
		-m_w[1], -m_w[1], 0.0f, m_w[1],
		 m_w[2],  m_w[2], 0.0f, m_w[2],
		 m_w[3], -m_w[3], 0.0f, m_w[3]
	};

	addAttribute(0u, VK_FORMAT_R32G32B32A32_SFLOAT, deUint32(sizeof(float) * 4), 4, vertices);
}

class BuiltinGlFragCoordWCase : public TestCase
{
public:
								BuiltinGlFragCoordWCase		(TestContext& testCtx, const string& name, const string& description);
	virtual						~BuiltinGlFragCoordWCase	(void);

	void						initPrograms				(SourceCollections& dst) const;
	TestInstance*				createInstance				(Context& context) const;

private:
								BuiltinGlFragCoordWCase		(const BuiltinGlFragCoordWCase&);	// not allowed!
	BuiltinGlFragCoordWCase&	operator=					(const BuiltinGlFragCoordWCase&);	// not allowed!
};

BuiltinGlFragCoordWCase::BuiltinGlFragCoordWCase (TestContext& testCtx, const string& name, const string& description)
	: TestCase(testCtx, name, description)
{
}

BuiltinGlFragCoordWCase::~BuiltinGlFragCoordWCase (void)
{
}

void BuiltinGlFragCoordWCase::initPrograms (SourceCollections& dst) const
{
	dst.glslSources.add("vert") << glu::VertexSource(
		"#version 310 es\n"
		"layout(location = 0) in highp vec4 a_position;\n"
		"void main (void)\n"
		"{\n"
		"       gl_Position = a_position;\n"
		"}\n");

	dst.glslSources.add("frag") << glu::FragmentSource(
		"#version 310 es\n"
		"layout(location = 0) out highp vec4 o_color;\n"
		"void main (void)\n"
		"{\n"
		"       o_color = vec4(0.0, 1.0 / gl_FragCoord.w - 1.0, 0.0, 1.0);\n"
		"}\n");
}

TestInstance* BuiltinGlFragCoordWCase::createInstance (Context& context) const
{
	return new BuiltinGlFragCoordWCaseInstance(context);
}

class BuiltinGlPointCoordCaseInstance : public ShaderRenderCaseInstance
{
public:
					BuiltinGlPointCoordCaseInstance	(Context& context);

	TestStatus		iterate								(void);
	virtual void	setupDefaultInputs					(void);
};

BuiltinGlPointCoordCaseInstance::BuiltinGlPointCoordCaseInstance (Context& context)
	: ShaderRenderCaseInstance	(context)
{
}

TestStatus BuiltinGlPointCoordCaseInstance::iterate (void)
{
	const UVec2				viewportSize	= getViewportSize();
	const int				width			= viewportSize.x();
	const int				height			= viewportSize.y();
	const float				threshold		= 0.02f;
	const int				numPoints		= 16;
	vector<Vec3>			coords			(numPoints);
	de::Random				rnd				(0x145fa);
	Surface					resImage		(width, height);
	Surface					refImage		(width, height);
	bool					compareOk		= false;

	// Compute coordinates.
	{
		const VkPhysicalDeviceLimits&	limits					= m_context.getDeviceProperties().limits;
		const float						minPointSize			= limits.pointSizeRange[0];
		const float						maxPointSize			= limits.pointSizeRange[1];
		const int						pointSizeDeltaMultiples	= de::max(1, deCeilFloatToInt32((maxPointSize - minPointSize) / limits.pointSizeGranularity));

		TCU_CHECK(minPointSize <= maxPointSize);

		for (vector<Vec3>::iterator coord = coords.begin(); coord != coords.end(); ++coord)
		{
			coord->x() = rnd.getFloat(-0.9f, 0.9f);
			coord->y() = rnd.getFloat(-0.9f, 0.9f);
			coord->z() = de::min(maxPointSize, minPointSize + float(rnd.getInt(0, pointSizeDeltaMultiples)) * limits.pointSizeGranularity);
		}
	}

	setup();
	addAttribute(0u, VK_FORMAT_R32G32B32_SFLOAT, deUint32(sizeof(Vec3)), numPoints, &coords[0]);
	render(numPoints, 0, DE_NULL, VK_PRIMITIVE_TOPOLOGY_POINT_LIST);
	copy(resImage.getAccess(), getResultImage().getAccess());

	// Draw reference
	clear(refImage.getAccess(), m_clearColor);

	for (vector<Vec3>::const_iterator pointIter = coords.begin(); pointIter != coords.end(); ++pointIter)
	{
		const float	centerX	= float(width) *(pointIter->x()*0.5f + 0.5f);
		const float	centerY	= float(height)*(pointIter->y()*0.5f + 0.5f);
		const float	size	= pointIter->z();
		const int	x0		= deRoundFloatToInt32(centerX - size*0.5f);
		const int	y0		= deRoundFloatToInt32(centerY - size*0.5f);
		const int	x1		= deRoundFloatToInt32(centerX + size*0.5f);
		const int	y1		= deRoundFloatToInt32(centerY + size*0.5f);
		const int	w		= x1-x0;
		const int	h		= y1-y0;

		for (int yo = 0; yo < h; yo++)
		{
			for (int xo = 0; xo < w; xo++)
			{
				const int		dx		= x0+xo;
				const int		dy		= y0+yo;
				const float		fragX	= float(dx) + 0.5f;
				const float		fragY	= float(dy) + 0.5f;
				const float		s		= 0.5f + (fragX - centerX) / size;
				const float		t		= 0.5f + (fragY - centerY) / size;
				const Vec4		color	(s, t, 0.0f, 1.0f);

				if (de::inBounds(dx, 0, refImage.getWidth()) && de::inBounds(dy, 0, refImage.getHeight()))
					refImage.setPixel(dx, dy, RGBA(color));
			}
		}
	}

	compareOk = fuzzyCompare(m_context.getTestContext().getLog(), "Result", "Image comparison result", refImage, resImage, threshold, COMPARE_LOG_RESULT);

	if (compareOk)
		return TestStatus::pass("Result image matches reference");
	else
		return TestStatus::fail("Image mismatch");
}

void BuiltinGlPointCoordCaseInstance::setupDefaultInputs (void)
{
}

class BuiltinGlPointCoordCase : public TestCase
{
public:
								BuiltinGlPointCoordCase	(TestContext& testCtx, const string& name, const string& description);
	virtual						~BuiltinGlPointCoordCase	(void);

	void						initPrograms				(SourceCollections& dst) const;
	TestInstance*				createInstance				(Context& context) const;

private:
								BuiltinGlPointCoordCase	(const BuiltinGlPointCoordCase&);	// not allowed!
	BuiltinGlPointCoordCase&	operator=					(const BuiltinGlPointCoordCase&);	// not allowed!
};

BuiltinGlPointCoordCase::BuiltinGlPointCoordCase (TestContext& testCtx, const string& name, const string& description)
	: TestCase(testCtx, name, description)
{
}

BuiltinGlPointCoordCase::~BuiltinGlPointCoordCase (void)
{
}

void BuiltinGlPointCoordCase::initPrograms (SourceCollections& dst) const
{
	dst.glslSources.add("vert") << glu::VertexSource(
		"#version 310 es\n"
		"layout(location = 0) in highp vec3 a_position;\n"
		"void main (void)\n"
		"{\n"
		"    gl_Position = vec4(a_position.xy, 0.0, 1.0);\n"
		"    gl_PointSize = a_position.z;\n"
		"}\n");

	dst.glslSources.add("frag") << glu::FragmentSource(
		"#version 310 es\n"
		"layout(location = 0) out lowp vec4 o_color;\n"
		"void main (void)\n"
		"{\n"
		"    o_color = vec4(gl_PointCoord, 0.0, 1.0);\n"
		"}\n");
}

TestInstance* BuiltinGlPointCoordCase::createInstance (Context& context) const
{
	return new BuiltinGlPointCoordCaseInstance(context);
}

enum ShaderInputTypeBits
{
	SHADER_INPUT_BUILTIN_BIT	= 0x01,
	SHADER_INPUT_VARYING_BIT	= 0x02,
	SHADER_INPUT_CONSTANT_BIT	= 0x04
};

typedef deUint16 ShaderInputTypes;

string shaderInputTypeToString (ShaderInputTypes type)
{
	string typeString = "input";

	if (type == 0)
		return "input_none";

	if (type & SHADER_INPUT_BUILTIN_BIT)
		typeString += "_builtin";

	if (type & SHADER_INPUT_VARYING_BIT)
		typeString += "_varying";

	if (type & SHADER_INPUT_CONSTANT_BIT)
		typeString += "_constant";

	return typeString;
}

class BuiltinInputVariationsCaseInstance : public ShaderRenderCaseInstance
{
public:
							BuiltinInputVariationsCaseInstance	(Context& context, const ShaderInputTypes shaderInputTypes);

	TestStatus				iterate								(void);
	virtual void			setupDefaultInputs					(void);
	virtual void			updatePushConstants					(vk::VkCommandBuffer commandBuffer, vk::VkPipelineLayout pipelineLayout);

private:
	const ShaderInputTypes	m_shaderInputTypes;
	const Vec4				m_constantColor;
};

BuiltinInputVariationsCaseInstance::BuiltinInputVariationsCaseInstance (Context& context, const ShaderInputTypes shaderInputTypes)
	: ShaderRenderCaseInstance	(context)
	, m_shaderInputTypes		(shaderInputTypes)
	, m_constantColor			(0.1f, 0.05f, 0.2f, 0.0f)
{
}

TestStatus BuiltinInputVariationsCaseInstance::iterate (void)
{
	const UVec2					viewportSize	= getViewportSize();
	const int					width			= viewportSize.x();
	const int					height			= viewportSize.y();
	const tcu::RGBA				threshold		(2, 2, 2, 2);
	Surface						resImage		(width, height);
	Surface						refImage		(width, height);
	bool						compareOk		= false;
	const VkPushConstantRange	pcRanges		=
	{
		VK_SHADER_STAGE_FRAGMENT_BIT,	// VkShaderStageFlags	stageFlags;
		0u,								// deUint32				offset;
		sizeof(Vec4)					// deUint32				size;
	};
	const deUint16				indices[12]		=
	{
		0, 4, 1,
		0, 5, 4,
		1, 2, 3,
		1, 3, 4
	};

	setup();

	if (m_shaderInputTypes & SHADER_INPUT_CONSTANT_BIT)
		setPushConstantRanges(1, &pcRanges);

	render(6, 4, indices);
	copy(resImage.getAccess(), getResultImage().getAccess());

	// Reference image
	for (int y = 0; y < refImage.getHeight(); y++)
	{
		for (int x = 0; x < refImage.getWidth(); x++)
		{
			Vec4 color (0.1f, 0.2f, 0.3f, 1.0f);

			if (((m_shaderInputTypes & SHADER_INPUT_BUILTIN_BIT) && (x < refImage.getWidth() / 2)) ||
				!(m_shaderInputTypes & SHADER_INPUT_BUILTIN_BIT))
			{
				if (m_shaderInputTypes & SHADER_INPUT_VARYING_BIT)
				{
					const float xf = (float(x)+.5f) / float(refImage.getWidth());
					color += Vec4(0.6f * (1 - xf), 0.6f * xf, 0.0f, 0.0f);
				}
				else
					color += Vec4(0.3f, 0.2f, 0.1f, 0.0f);
			}

			if (m_shaderInputTypes & SHADER_INPUT_CONSTANT_BIT)
				color += m_constantColor;

			refImage.setPixel(x, y, RGBA(color));
		}
	}

	compareOk = pixelThresholdCompare(m_context.getTestContext().getLog(), "Result", "Image comparison result", refImage, resImage, threshold, COMPARE_LOG_RESULT);

	if (compareOk)
		return TestStatus::pass("Result image matches reference");
	else
		return TestStatus::fail("Image mismatch");
}

void BuiltinInputVariationsCaseInstance::setupDefaultInputs (void)
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

	addAttribute(0u, VK_FORMAT_R32G32B32A32_SFLOAT, deUint32(sizeof(float) * 4), 6, vertices);

	if (m_shaderInputTypes & SHADER_INPUT_VARYING_BIT)
	{
		const float colors[] =
		{
			 0.6f,  0.0f, 0.0f, 1.0f,
			 0.3f,  0.3f, 0.0f, 1.0f,
			 0.0f,  0.6f, 0.0f, 1.0f,
			 0.0f,  0.6f, 0.0f, 1.0f,
			 0.3f,  0.3f, 0.0f, 1.0f,
			 0.6f,  0.0f, 0.0f, 1.0f
		};
		addAttribute(1u, VK_FORMAT_R32G32B32A32_SFLOAT, deUint32(sizeof(float) * 4), 6, colors);
	}
}

void BuiltinInputVariationsCaseInstance::updatePushConstants (vk::VkCommandBuffer commandBuffer, vk::VkPipelineLayout pipelineLayout)
{
	if (m_shaderInputTypes & SHADER_INPUT_CONSTANT_BIT)
	{
		const DeviceInterface& vk = m_context.getDeviceInterface();
		vk.cmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Vec4), &m_constantColor);
	}
}

class BuiltinInputVariationsCase : public TestCase
{
public:
								BuiltinInputVariationsCase	(TestContext& testCtx, const string& name, const string& description, const ShaderInputTypes shaderInputTypes);
	virtual						~BuiltinInputVariationsCase	(void);

	void						initPrograms				(SourceCollections& dst) const;
	TestInstance*				createInstance				(Context& context) const;

private:
								BuiltinInputVariationsCase	(const BuiltinInputVariationsCase&);	// not allowed!
	BuiltinInputVariationsCase&	operator=					(const BuiltinInputVariationsCase&);	// not allowed!
	const ShaderInputTypes		m_shaderInputTypes;
};

BuiltinInputVariationsCase::BuiltinInputVariationsCase (TestContext& testCtx, const string& name, const string& description, ShaderInputTypes shaderInputTypes)
	: TestCase				(testCtx, name, description)
	, m_shaderInputTypes	(shaderInputTypes)
{
}

BuiltinInputVariationsCase::~BuiltinInputVariationsCase (void)
{
}

void BuiltinInputVariationsCase::initPrograms (SourceCollections& dst) const
{
	map<string, string>			vertexParams;
	map<string, string>			fragmentParams;
	const tcu::StringTemplate	vertexCodeTemplate		(
		"#version 450\n"
		"layout(location = 0) in highp vec4 a_position;\n"
		"out gl_PerVertex {\n"
		"	vec4 gl_Position;\n"
		"};\n"
		"${VARYING_DECL}"
		"void main (void)\n"
		"{\n"
		"    gl_Position = a_position;\n"
		"    ${VARYING_USAGE}"
		"}\n");

	const tcu::StringTemplate	fragmentCodeTemplate	(
		"#version 450\n"
		"${VARYING_DECL}"
		"${CONSTANT_DECL}"
		"layout(location = 0) out highp vec4 o_color;\n"
		"void main (void)\n"
		"{\n"
		"    o_color = vec4(0.1, 0.2, 0.3, 1.0);\n"
		"    ${BUILTIN_USAGE}"
		"    ${VARYING_USAGE}"
		"    ${CONSTANT_USAGE}"
		"}\n");

	vertexParams["VARYING_DECL"]		=
		m_shaderInputTypes & SHADER_INPUT_VARYING_BIT	? "layout(location = 1) in highp vec4 a_color;\n"
														  "layout(location = 0) out highp vec4 v_color;\n"
														: "";

	vertexParams["VARYING_USAGE"]		=
		m_shaderInputTypes & SHADER_INPUT_VARYING_BIT	? "v_color = a_color;\n"
														: "";

	fragmentParams["VARYING_DECL"]		=
		m_shaderInputTypes & SHADER_INPUT_VARYING_BIT	? "layout(location = 0) in highp vec4 a_color;\n"
														: "";

	fragmentParams["CONSTANT_DECL"]		=
		m_shaderInputTypes & SHADER_INPUT_CONSTANT_BIT	? "layout(push_constant) uniform PCBlock {\n"
														  "  vec4 color;\n"
														  "} pc;\n"
														: "";

	fragmentParams["BUILTIN_USAGE"]		=
		m_shaderInputTypes & SHADER_INPUT_BUILTIN_BIT	? "if (gl_FrontFacing)\n"
														: "";

	fragmentParams["VARYING_USAGE"]		=
		m_shaderInputTypes & SHADER_INPUT_VARYING_BIT	? "o_color += vec4(a_color.xyz, 0.0);\n"
														: "o_color += vec4(0.3, 0.2, 0.1, 0.0);\n";


	fragmentParams["CONSTANT_USAGE"]	=
		m_shaderInputTypes & SHADER_INPUT_CONSTANT_BIT	? "o_color += pc.color;\n"
														: "";

	dst.glslSources.add("vert") << glu::VertexSource(vertexCodeTemplate.specialize(vertexParams));
	dst.glslSources.add("frag") << glu::FragmentSource(fragmentCodeTemplate.specialize(fragmentParams));
}

TestInstance* BuiltinInputVariationsCase::createInstance (Context& context) const
{
	return new BuiltinInputVariationsCaseInstance(context, m_shaderInputTypes);
}

} // anonymous

TestCaseGroup* createBuiltinVarTests (TestContext& testCtx)
{
	de::MovePtr<TestCaseGroup> builtinGroup			(new TestCaseGroup(testCtx, "builtin_var", "Shader builtin variable tests."));
	de::MovePtr<TestCaseGroup> simpleGroup			(new TestCaseGroup(testCtx, "simple", "Simple cases."));
	de::MovePtr<TestCaseGroup> inputVariationsGroup	(new TestCaseGroup(testCtx, "input_variations", "Input type variation tests."));

	simpleGroup->addChild(new BuiltinGlFrontFacingCase(testCtx, "frontfacing", "FrontFacing test"));
	simpleGroup->addChild(new BuiltinGlFragCoordXYZCase(testCtx, "fragcoord_xyz", "FragCoord xyz test"));
	simpleGroup->addChild(new BuiltinGlFragCoordWCase(testCtx, "fragcoord_w", "FragCoord w test"));
	simpleGroup->addChild(new BuiltinGlPointCoordCase(testCtx, "pointcoord", "PointCoord test"));

	builtinGroup->addChild(simpleGroup.release());

	for (deUint16 shaderType = 0; shaderType <= (SHADER_INPUT_BUILTIN_BIT | SHADER_INPUT_VARYING_BIT | SHADER_INPUT_CONSTANT_BIT); ++shaderType)
	{
		inputVariationsGroup->addChild(new BuiltinInputVariationsCase(testCtx, shaderInputTypeToString(shaderType), "Input variation test", shaderType));
	}

	builtinGroup->addChild(inputVariationsGroup.release());
	return builtinGroup.release();
}

} // sr
} // vkt
