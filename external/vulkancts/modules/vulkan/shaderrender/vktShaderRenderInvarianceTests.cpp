/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
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
 * \brief Invariant decoration tests.
 *//*--------------------------------------------------------------------*/

#include "vktShaderRenderInvarianceTests.hpp"
#include "vktShaderRender.hpp"
#include "tcuImageCompare.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuTestLog.hpp"
#include "vktDrawUtil.hpp"
#include "deMath.h"
#include "deRandom.hpp"

using namespace vk;

namespace vkt
{
using namespace drawutil;

namespace sr
{

namespace
{

class FormatArgument
{
public:
						FormatArgument (const char* name, const std::string& value);

private:
	friend class FormatArgumentList;

	const char* const	m_name;
	const std::string	m_value;
};

FormatArgument::FormatArgument (const char* name, const std::string& value)
	: m_name	(name)
	, m_value	(value)
{
}

class FormatArgumentList
{
public:
												FormatArgumentList	(void);

	FormatArgumentList&							operator<<			(const FormatArgument&);
	const std::map<std::string, std::string>&	getArguments		(void) const;

private:
	std::map<std::string, std::string>			m_formatArguments;
};

FormatArgumentList::FormatArgumentList (void)
{
}

FormatArgumentList&	FormatArgumentList::operator<< (const FormatArgument& arg)
{
	m_formatArguments[arg.m_name] = arg.m_value;
	return *this;
}

const std::map<std::string, std::string>& FormatArgumentList::getArguments (void) const
{
	return m_formatArguments;
}

static std::string formatGLSL(const char* templateString, const FormatArgumentList& args)
{
	const std::map<std::string, std::string>& params = args.getArguments();

	return tcu::StringTemplate(std::string(templateString)).specialize(params);
}

class InvarianceTest : public vkt::TestCase
{
public:
								InvarianceTest(tcu::TestContext& ctx, const char* name, const char* desc, const std::string& vertexShader1, const std::string& vertexShader2, const std::string& fragmentShader = "");

	void						initPrograms	(SourceCollections& sourceCollections) const override;
	vkt::TestInstance*			createInstance	(vkt::Context& context) const override;

private:
	const std::string			m_vertexShader1;
	const std::string			m_vertexShader2;
	const std::string			m_fragmentShader;
};

class InvarianceTestInstance : public vkt::TestInstance
{
public:
						InvarianceTestInstance(vkt::Context &context);
	tcu::TestStatus		iterate(void) override;
	bool				checkImage(const tcu::ConstPixelBufferAccess& image) const;
	const int			m_renderSize = 256;
};

 InvarianceTest::InvarianceTest(tcu::TestContext& ctx, const char* name, const char* desc, const std::string& vertexShader1, const std::string& vertexShader2, const std::string& fragmentShader)
	: vkt::TestCase(ctx, name, desc)
	, m_vertexShader1(vertexShader1)
	, m_vertexShader2(vertexShader2)
	, m_fragmentShader(fragmentShader)

{
}

void InvarianceTest::initPrograms(SourceCollections& sourceCollections) const
{
	sourceCollections.glslSources.add("vertex1") << glu::VertexSource(m_vertexShader1);
	sourceCollections.glslSources.add("vertex2") << glu::VertexSource(m_vertexShader2);
	sourceCollections.glslSources.add("fragment") << glu::FragmentSource(m_fragmentShader);
}

vkt::TestInstance* InvarianceTest::createInstance(Context& context) const
{
	return new InvarianceTestInstance(context);
}

InvarianceTestInstance::InvarianceTestInstance(vkt::Context &context)
	: vkt::TestInstance(context)
{
}

static tcu::Vec4 genRandomVector(de::Random& rnd)
{
	tcu::Vec4 retVal;

	retVal.x() = rnd.getFloat(-1.0f, 1.0f);
	retVal.y() = rnd.getFloat(-1.0f, 1.0f);
	retVal.z() = rnd.getFloat(-1.0f, 1.0f);
	retVal.w() = rnd.getFloat(0.2f, 1.0f);

	return retVal;
}

struct ColorUniform
{
	tcu::Vec4 color;
};

tcu::TestStatus InvarianceTestInstance::iterate(void)
{
	const VkDevice			device			= m_context.getDevice();
	const DeviceInterface&	vk				= m_context.getDeviceInterface();
	Allocator&				allocator		= m_context.getDefaultAllocator();
	tcu::TestLog&			log				= m_context.getTestContext().getLog();

	const int				numTriangles	= 72;
	de::Random				rnd				(123);
	std::vector<tcu::Vec4>	vertices		(numTriangles * 3 * 2);

	{
		// Narrow triangle pattern
		for (int triNdx = 0; triNdx < numTriangles; ++triNdx)
		{
			const tcu::Vec4 vertex1 = genRandomVector(rnd);
			const tcu::Vec4 vertex2 = genRandomVector(rnd);
			const tcu::Vec4 vertex3 = vertex2 + genRandomVector(rnd) * 0.01f; // generate narrow triangles

			vertices[triNdx * 3 + 0] = vertex1;
			vertices[triNdx * 3 + 1] = vertex2;
			vertices[triNdx * 3 + 2] = vertex3;
		}

		// Normal triangle pattern
		for (int triNdx = 0; triNdx < numTriangles; ++triNdx)
		{
			vertices[(numTriangles + triNdx) * 3 + 0] = genRandomVector(rnd);
			vertices[(numTriangles + triNdx) * 3 + 1] = genRandomVector(rnd);
			vertices[(numTriangles + triNdx) * 3 + 2] = genRandomVector(rnd);
		}
	}

	Move<VkDescriptorSetLayout>		descriptorSetLayout;
	Move<VkDescriptorPool>			descriptorPool;
	Move<VkBuffer>					uniformBuffer[2];
	de::MovePtr<Allocation>			uniformBufferAllocation[2];
	Move<VkDescriptorSet>			descriptorSet[2];
	const tcu::Vec4					red		= tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f);
	const tcu::Vec4					green	= tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f);

	// Descriptors
	{
		DescriptorSetLayoutBuilder	layoutBuilder;
		layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
		descriptorSetLayout = layoutBuilder.build(vk, device);
		descriptorPool = DescriptorPoolBuilder()
				.addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2u)
				.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 2u);

		const VkDescriptorSetAllocateInfo descriptorSetAllocInfo =
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			DE_NULL,
			*descriptorPool,
			1u,
			&descriptorSetLayout.get()
		};

		const VkBufferCreateInfo uniformBufferCreateInfo =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,					// VkStructureType		sType
			DE_NULL,												// const void*			pNext
			(VkBufferCreateFlags)0,									// VkBufferCreateFlags	flags
			sizeof(ColorUniform),									// VkDeviceSize			size
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,						// VkBufferUsageFlags	usage
			VK_SHARING_MODE_EXCLUSIVE,								// VkSharingMode		sharingMode
			0u,														// deUint32				queueFamilyIndexCount
			DE_NULL													// pQueueFamilyIndices
		};

		for (deUint32 passNdx = 0; passNdx < 2; ++passNdx)
		{
			uniformBuffer[passNdx]				= createBuffer(vk, device, &uniformBufferCreateInfo, DE_NULL);
			uniformBufferAllocation[passNdx]	= allocator.allocate(getBufferMemoryRequirements(vk, device, *uniformBuffer[passNdx]), MemoryRequirement::HostVisible);
			VK_CHECK(vk.bindBufferMemory(device, *uniformBuffer[passNdx], uniformBufferAllocation[passNdx]->getMemory(), uniformBufferAllocation[passNdx]->getOffset()));

			{
				ColorUniform* bufferData	= (ColorUniform*)(uniformBufferAllocation[passNdx]->getHostPtr());
				bufferData->color			= (passNdx == 0) ? (red) : (green);
				flushAlloc(vk, device, *uniformBufferAllocation[passNdx]);
			}
			descriptorSet[passNdx] = allocateDescriptorSet(vk, device, &descriptorSetAllocInfo);

			const VkDescriptorBufferInfo bufferInfo =
			{
				*uniformBuffer[passNdx],
				0u,
				VK_WHOLE_SIZE
			};

			DescriptorSetUpdateBuilder()
				.writeSingle(*descriptorSet[passNdx], DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &bufferInfo)
				.update(vk, device);
		}
	}

	// pick first available depth buffer format
	const std::vector<VkFormat>	depthFormats	{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_X8_D24_UNORM_PACK32, VK_FORMAT_D24_UNORM_S8_UINT };
	VkFormat					depthFormat		= VK_FORMAT_UNDEFINED;
	const InstanceInterface&	vki				= m_context.getInstanceInterface();
	const VkPhysicalDevice		vkPhysDevice	= m_context.getPhysicalDevice();
	for (const auto& df : depthFormats)
	{
		const VkFormatProperties	properties = getPhysicalDeviceFormatProperties(vki, vkPhysDevice, df);
		if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
		{
			depthFormat = df;
			break;
		}
	}
	if(depthFormat == VK_FORMAT_UNDEFINED)
		return tcu::TestStatus::fail("There must be at least one depth depth format handled (Vulkan spec 37.3, table 65)");

	FrameBufferState				frameBufferState(m_renderSize, m_renderSize);
	frameBufferState.depthFormat	= depthFormat;
	PipelineState					pipelineState(m_context.getDeviceProperties().limits.subPixelPrecisionBits);
	DrawCallData					drawCallData(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, vertices);
	VulkanDrawContext				vulkanDrawContext(m_context, frameBufferState);

	const std::vector<std::string>				vertexShaderNames	= { "vertex1", "vertex2" };

	log << tcu::TestLog::Message << "Testing position invariance." << tcu::TestLog::EndMessage;

	for (deUint32 passNdx = 0; passNdx < 2; ++passNdx)
	{
		std::vector<VulkanShader>			shaders;
		shaders.push_back(VulkanShader(VK_SHADER_STAGE_VERTEX_BIT, m_context.getBinaryCollection().get(vertexShaderNames[passNdx])));
		shaders.push_back(VulkanShader(VK_SHADER_STAGE_FRAGMENT_BIT, m_context.getBinaryCollection().get("fragment")));
		VulkanProgram						vulkanProgram(shaders);
		vulkanProgram.descriptorSetLayout	= *descriptorSetLayout;
		vulkanProgram.descriptorSet			= *descriptorSet[passNdx];

		const char* const			colorStr = (passNdx == 0) ? ("red - purple") : ("green");
		log << tcu::TestLog::Message << "Drawing position test pattern using shader " << (passNdx + 1) << ". Primitive color: " << colorStr << "." << tcu::TestLog::EndMessage;

		vulkanDrawContext.registerDrawObject(pipelineState, vulkanProgram, drawCallData);
	}
	vulkanDrawContext.draw();

	tcu::ConstPixelBufferAccess	resultImage(
		tcu::TextureFormat(vulkanDrawContext.getColorPixels().getFormat()),
		vulkanDrawContext.getColorPixels().getWidth(),
		vulkanDrawContext.getColorPixels().getHeight(),
		1,
		vulkanDrawContext.getColorPixels().getDataPtr());

	log << tcu::TestLog::Message << "Verifying output. Expecting only green or background colored pixels." << tcu::TestLog::EndMessage;
	if( !checkImage(resultImage) )
		return tcu::TestStatus::fail("Detected variance between two invariant values");

	return tcu::TestStatus::pass("Passed");
}

bool InvarianceTestInstance::checkImage(const tcu::ConstPixelBufferAccess& image) const
{
	const tcu::IVec4	okColor		(0, 255, 0, 255);
	const tcu::RGBA		errColor	(255, 0, 0, 255);
	bool				error		= false;
	tcu::Surface		errorMask	(image.getWidth(), image.getHeight());

	tcu::clear(errorMask.getAccess(), okColor);

	for (int y = 0; y < m_renderSize; ++y)
		for (int x = 0; x < m_renderSize; ++x)
		{
			const tcu::IVec4 col = image.getPixelInt(x, y);

			if (col.x() != 0)
			{
				errorMask.setPixel(x, y, errColor);
				error = true;
			}
		}

	// report error
	if (error)
	{
		m_context.getTestContext().getLog() << tcu::TestLog::Message << "Invalid pixels found (fragments from first render pass found). Variance detected." << tcu::TestLog::EndMessage;
		m_context.getTestContext().getLog()
			<< tcu::TestLog::ImageSet("Results", "Result verification")
			<< tcu::TestLog::Image("Result", "Result", image)
			<< tcu::TestLog::Image("Error mask", "Error mask", errorMask)
			<< tcu::TestLog::EndImageSet;

		return false;
	}
	else
	{
		m_context.getTestContext().getLog() << tcu::TestLog::Message << "No variance found." << tcu::TestLog::EndMessage;
		m_context.getTestContext().getLog()
			<< tcu::TestLog::ImageSet("Results", "Result verification")
			<< tcu::TestLog::Image("Result", "Result", image)
			<< tcu::TestLog::EndImageSet;

		return true;
	}
}

} // namespace

tcu::TestCaseGroup* createShaderInvarianceTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> invarianceGroup(new tcu::TestCaseGroup(testCtx, "invariance", "Invariance tests"));

	static const struct PrecisionCase
	{
		glu::Precision	prec;
		const char*		name;

		// set literals in the glsl to be in the representable range
		const char*		highValue;		// !< highValue < maxValue
		const char*		invHighValue;
		const char*		mediumValue;	// !< mediumValue^2 < maxValue
		const char*		lowValue;		// !< lowValue^4 < maxValue
		const char*		invlowValue;
		int				loopIterations;
		int				loopPartialIterations;
		int				loopNormalizationExponent;
		const char*		loopNormalizationConstantLiteral;
		const char*		loopMultiplier;
		const char*		sumLoopNormalizationConstantLiteral;
	} precisions[] =
	{
		{ glu::PRECISION_HIGHP,		"highp",	"1.0e20",	"1.0e-20",	"1.0e14",	"1.0e9",	"1.0e-9",	14,	11,	2,	"1.0e4",	"1.9",	"1.0e3"	},
		{ glu::PRECISION_MEDIUMP,	"mediump",	"1.0e4",	"1.0e-4",	"1.0e2",	"1.0e1",	"1.0e-1",	13,	11,	2,	"1.0e4",	"1.9",	"1.0e3"	},
		{ glu::PRECISION_LOWP,		"lowp",		"0.9",		"1.1",		"1.1",		"1.15",		"0.87",		6,	2,	0,	"2.0",		"1.1",	"1.0"	},
	};

	// gl_Position must always be invariant for comparisons on gl_Position to be valid.
	static const std::string invariantDeclaration[]	= { "invariant gl_Position;", "invariant gl_Position;\nlayout(location = 1) invariant highp out vec4 v_value;" };
	static const std::string invariantAssignment0[]	= { "gl_Position", "v_value" };
	static const std::string invariantAssignment1[]	= { "", "gl_Position = v_value;" };
	static const std::string fragDeclaration[]		= { "", "layout(location = 1) highp in vec4 v_value;" };

	static const char* basicFragmentShader = "${VERSION}"
		"precision mediump float;\n"
		"${IN} vec4 v_unrelated;\n"
		"${FRAG_DECLARATION}\n"
		"layout(binding = 0) uniform ColorUniform\n"
		"{\n"
		"	vec4 u_color;\n"
		"} ucolor;\n"
		"layout(location = 0) out vec4 fragColor;\n"
		"void main ()\n"
		"{\n"
		"	float blue = dot(v_unrelated, vec4(1.0, 1.0, 1.0, 1.0));\n"
		"	fragColor = vec4(ucolor.u_color.r, ucolor.u_color.g, blue, ucolor.u_color.a);\n"
		"}\n";

	for (int precNdx = 0; precNdx < DE_LENGTH_OF_ARRAY(precisions); ++precNdx)
	{
		const char* const			precisionName = precisions[precNdx].name;
		const glu::Precision		precision = precisions[precNdx].prec;
		tcu::TestCaseGroup* const	group = new tcu::TestCaseGroup(testCtx, precisionName, "Invariance tests using the given precision.");

		const deUint32 VAR_GROUP_SIZE = 2u;
		tcu::TestCaseGroup* varGroup[VAR_GROUP_SIZE];
		varGroup[0] = new tcu::TestCaseGroup(testCtx, "gl_position", "Invariance tests using gl_Position variable");
		varGroup[1] = new tcu::TestCaseGroup(testCtx, "user_defined", "Invariance tests using user defined variable");
		FormatArgumentList	args[VAR_GROUP_SIZE];
		for (deUint32 groupNdx = 0u; groupNdx < VAR_GROUP_SIZE; ++groupNdx)
		{
			group->addChild(varGroup[groupNdx]);
			args[groupNdx] = FormatArgumentList()
				<< FormatArgument("VERSION",				"#version 450\n")
				<< FormatArgument("IN",						"layout(location = 0) in")
				<< FormatArgument("OUT",					"layout(location = 0) out")
				<< FormatArgument("IN_PREC",				precisionName)
				<< FormatArgument("INVARIANT_DECLARATION",	invariantDeclaration[groupNdx])
				<< FormatArgument("INVARIANT_ASSIGN_0",		invariantAssignment0[groupNdx])
				<< FormatArgument("INVARIANT_ASSIGN_1",		invariantAssignment1[groupNdx])
				<< FormatArgument("FRAG_DECLARATION",		fragDeclaration[groupNdx])
				<< FormatArgument("HIGH_VALUE",				de::toString(precisions[precNdx].highValue))
				<< FormatArgument("HIGH_VALUE_INV",			de::toString(precisions[precNdx].invHighValue))
				<< FormatArgument("MEDIUM_VALUE",			de::toString(precisions[precNdx].mediumValue))
				<< FormatArgument("LOW_VALUE",				de::toString(precisions[precNdx].lowValue))
				<< FormatArgument("LOW_VALUE_INV",			de::toString(precisions[precNdx].invlowValue))
				<< FormatArgument("LOOP_ITERS",				de::toString(precisions[precNdx].loopIterations))
				<< FormatArgument("LOOP_ITERS_PARTIAL",		de::toString(precisions[precNdx].loopPartialIterations))
				<< FormatArgument("LOOP_NORM_FRACT_EXP",	de::toString(precisions[precNdx].loopNormalizationExponent))
				<< FormatArgument("LOOP_NORM_LITERAL",		precisions[precNdx].loopNormalizationConstantLiteral)
				<< FormatArgument("LOOP_MULTIPLIER",		precisions[precNdx].loopMultiplier)
				<< FormatArgument("SUM_LOOP_NORM_LITERAL",	precisions[precNdx].sumLoopNormalizationConstantLiteral);
		}

		// subexpression cases
		for (deUint32 groupNdx = 0u; groupNdx < VAR_GROUP_SIZE; ++groupNdx)
		{
			// First shader shares "${HIGH_VALUE}*a_input.x*a_input.xxxx + ${HIGH_VALUE}*a_input.y*a_input.yyyy" with unrelated output variable. Reordering might result in accuracy loss
			// due to the high exponent. In the second shader, the high exponent may be removed during compilation.

			varGroup[groupNdx]->addChild(new InvarianceTest(testCtx, "common_subexpression_0", "Shader shares a subexpression with an unrelated variable.",
				formatGLSL("${VERSION}"
					"${IN} ${IN_PREC} vec4 a_input;\n"
					"${OUT} mediump vec4 v_unrelated;\n"
					"${INVARIANT_DECLARATION}\n"
					"void main ()\n"
					"{\n"
					"	v_unrelated = a_input.xzxz + (${HIGH_VALUE}*a_input.x*a_input.xxxx + ${HIGH_VALUE}*a_input.y*a_input.yyyy) * (1.08 * a_input.zyzy * a_input.xzxz) * ${HIGH_VALUE_INV} * (a_input.z * a_input.zzxz - a_input.z * a_input.zzxz) + (${HIGH_VALUE}*a_input.x*a_input.xxxx + ${HIGH_VALUE}*a_input.y*a_input.yyyy) / ${HIGH_VALUE};\n"
					"	${INVARIANT_ASSIGN_0} = a_input + (${HIGH_VALUE}*a_input.x*a_input.xxxx + ${HIGH_VALUE}*a_input.y*a_input.yyyy) * ${HIGH_VALUE_INV};\n"
					"	${INVARIANT_ASSIGN_1}\n"
					"}\n", args[groupNdx]),
				formatGLSL("${VERSION}"
					"${IN} ${IN_PREC} vec4 a_input;\n"
					"${OUT} mediump vec4 v_unrelated;\n"
					"${INVARIANT_DECLARATION}\n"
					"void main ()\n"
					"{\n"
					"	v_unrelated = vec4(0.0, 0.0, 0.0, 0.0);\n"
					"	${INVARIANT_ASSIGN_0} = a_input + (${HIGH_VALUE}*a_input.x*a_input.xxxx + ${HIGH_VALUE}*a_input.y*a_input.yyyy) * ${HIGH_VALUE_INV};\n"
					"	${INVARIANT_ASSIGN_1}\n"
					"}\n", args[groupNdx]),
				formatGLSL(basicFragmentShader, args[groupNdx])));

			// In the first shader, the unrelated variable "d" has mathematically the same expression as "e", but the different
			// order of calculation might cause different results.

			varGroup[groupNdx]->addChild(new InvarianceTest(testCtx, "common_subexpression_1", "Shader shares a subexpression with an unrelated variable.",
				formatGLSL("${VERSION}"
					"${IN} ${IN_PREC} vec4 a_input;\n"
					"${OUT} mediump vec4 v_unrelated;\n"
					"${INVARIANT_DECLARATION}\n"
					"void main ()\n"
					"{\n"
					"	${IN_PREC} vec4 a = ${HIGH_VALUE} * a_input.zzxx + a_input.xzxy - ${HIGH_VALUE} * a_input.zzxx;\n"
					"	${IN_PREC} vec4 b = ${HIGH_VALUE} * a_input.zzxx;\n"
					"	${IN_PREC} vec4 c = b - ${HIGH_VALUE} * a_input.zzxx + a_input.xzxy;\n"
					"	${IN_PREC} vec4 d = (${LOW_VALUE} * a_input.yzxx) * (${LOW_VALUE} * a_input.yzzw) * (1.1*${LOW_VALUE_INV} * a_input.yzxx) * (${LOW_VALUE_INV} * a_input.xzzy);\n"
					"	${IN_PREC} vec4 e = ((${LOW_VALUE} * a_input.yzxx) * (1.1*${LOW_VALUE_INV} * a_input.yzxx)) * ((${LOW_VALUE_INV} * a_input.xzzy) * (${LOW_VALUE} * a_input.yzzw));\n"
					"	v_unrelated = a + b + c + d + e;\n"
					"	${INVARIANT_ASSIGN_0} = a_input + fract(c) + e;\n"
					"	${INVARIANT_ASSIGN_1}\n"
					"}\n", args[groupNdx]),
				formatGLSL("${VERSION}"
					"${IN} ${IN_PREC} vec4 a_input;\n"
					"${OUT} mediump vec4 v_unrelated;\n"
					"${INVARIANT_DECLARATION}\n"
					"void main ()\n"
					"{\n"
					"	${IN_PREC} vec4 b = ${HIGH_VALUE} * a_input.zzxx;\n"
					"	${IN_PREC} vec4 c = b - ${HIGH_VALUE} * a_input.zzxx + a_input.xzxy;\n"
					"	${IN_PREC} vec4 e = ((${LOW_VALUE} * a_input.yzxx) * (1.1*${LOW_VALUE_INV} * a_input.yzxx)) * ((${LOW_VALUE_INV} * a_input.xzzy) * (${LOW_VALUE} * a_input.yzzw));\n"
					"	v_unrelated = vec4(0.0, 0.0, 0.0, 0.0);\n"
					"	${INVARIANT_ASSIGN_0} = a_input + fract(c) + e;\n"
					"	${INVARIANT_ASSIGN_1}\n"
					"}\n", args[groupNdx]),
				formatGLSL(basicFragmentShader, args[groupNdx])));

			// Intermediate values used by an unrelated output variable

			varGroup[groupNdx]->addChild(new InvarianceTest(testCtx, "common_subexpression_2", "Shader shares a subexpression with an unrelated variable.",
				formatGLSL("${VERSION}"
					"${IN} ${IN_PREC} vec4 a_input;\n"
					"${OUT} mediump vec4 v_unrelated;\n"
					"${INVARIANT_DECLARATION}\n"
					"void main ()\n"
					"{\n"
					"	${IN_PREC} vec4 a = ${MEDIUM_VALUE} * (a_input.xxxx + a_input.yyyy);\n"
					"	${IN_PREC} vec4 b = (${MEDIUM_VALUE} * (a_input.xxxx + a_input.yyyy)) * (${MEDIUM_VALUE} * (a_input.xxxx + a_input.yyyy)) / ${MEDIUM_VALUE} / ${MEDIUM_VALUE};\n"
					"	${IN_PREC} vec4 c = a * a;\n"
					"	${IN_PREC} vec4 d = c / ${MEDIUM_VALUE} / ${MEDIUM_VALUE};\n"
					"	v_unrelated = a + b + c + d;\n"
					"	${INVARIANT_ASSIGN_0} = a_input + d;\n"
					"	${INVARIANT_ASSIGN_1}\n"
					"}\n", args[groupNdx]),
				formatGLSL("${VERSION}"
					"${IN} ${IN_PREC} vec4 a_input;\n"
					"${OUT} mediump vec4 v_unrelated;\n"
					"${INVARIANT_DECLARATION}\n"
					"void main ()\n"
					"{\n"
					"	${IN_PREC} vec4 a = ${MEDIUM_VALUE} * (a_input.xxxx + a_input.yyyy);\n"
					"	${IN_PREC} vec4 c = a * a;\n"
					"	${IN_PREC} vec4 d = c / ${MEDIUM_VALUE} / ${MEDIUM_VALUE};\n"
					"	v_unrelated = vec4(0.0, 0.0, 0.0, 0.0);\n"
					"	${INVARIANT_ASSIGN_0} = a_input + d;\n"
					"	${INVARIANT_ASSIGN_1}\n"
					"}\n", args[groupNdx]),
				formatGLSL(basicFragmentShader, args[groupNdx])));

			// Invariant value can be calculated using unrelated value

			varGroup[groupNdx]->addChild(new InvarianceTest(testCtx, "common_subexpression_3", "Shader shares a subexpression with an unrelated variable.",
				formatGLSL("${VERSION}"
					"${IN} ${IN_PREC} vec4 a_input;\n"
					"${OUT} mediump vec4 v_unrelated;\n"
					"${INVARIANT_DECLARATION}\n"
					"void main ()\n"
					"{\n"
					"	${IN_PREC} float x = a_input.x * 0.2;\n"
					"	${IN_PREC} vec4 a = a_input.xxyx * 0.7;\n"
					"	${IN_PREC} vec4 b = a_input.yxyz * 0.7;\n"
					"	${IN_PREC} vec4 c = a_input.zxyx * 0.5;\n"
					"	${IN_PREC} vec4 f = x*a + x*b + x*c;\n"
					"	v_unrelated = f;\n"
					"	${IN_PREC} vec4 g = x * (a + b + c);\n"
					"	${INVARIANT_ASSIGN_0} = a_input + g;\n"
					"	${INVARIANT_ASSIGN_1}\n"
					"}\n", args[groupNdx]),
				formatGLSL("${VERSION}"
					"${IN} ${IN_PREC} vec4 a_input;\n"
					"${OUT} mediump vec4 v_unrelated;\n"
					"${INVARIANT_DECLARATION}\n"
					"void main ()\n"
					"{\n"
					"	${IN_PREC} float x = a_input.x * 0.2;\n"
					"	${IN_PREC} vec4 a = a_input.xxyx * 0.7;\n"
					"	${IN_PREC} vec4 b = a_input.yxyz * 0.7;\n"
					"	${IN_PREC} vec4 c = a_input.zxyx * 0.5;\n"
					"	v_unrelated = vec4(0.0, 0.0, 0.0, 0.0);\n"
					"	${IN_PREC} vec4 g = x * (a + b + c);\n"
					"	${INVARIANT_ASSIGN_0} = a_input + g;\n"
					"	${INVARIANT_ASSIGN_1}\n"
					"}\n", args[groupNdx]),
				formatGLSL(basicFragmentShader, args[groupNdx])));
		}

		// shared subexpression of different precision
		for (deUint32 groupNdx = 0u; groupNdx < VAR_GROUP_SIZE; ++groupNdx)
		{
			for (int precisionOther = glu::PRECISION_LOWP; precisionOther != glu::PRECISION_LAST; ++precisionOther)
			{
				const char* const		unrelatedPrec = glu::getPrecisionName((glu::Precision)precisionOther);
				const glu::Precision	minPrecision = (precisionOther < (int)precision) ? ((glu::Precision)precisionOther) : (precision);
				const char* const		multiplierStr = (minPrecision == glu::PRECISION_LOWP) ? ("0.8, 0.4, -0.2, 0.3") : ("1.0e1, 5.0e2, 2.0e2, 1.0");
				const char* const		normalizationStrUsed = (minPrecision == glu::PRECISION_LOWP) ? ("vec4(fract(used2).xyz, 0.0)") : ("vec4(fract(used2 / 1.0e2).xyz - fract(used2 / 1.0e3).xyz, 0.0)");
				const char* const		normalizationStrUnrelated = (minPrecision == glu::PRECISION_LOWP) ? ("vec4(fract(unrelated2).xyz, 0.0)") : ("vec4(fract(unrelated2 / 1.0e2).xyz - fract(unrelated2 / 1.0e3).xyz, 0.0)");

				varGroup[groupNdx]->addChild(new InvarianceTest(testCtx, ("subexpression_precision_" + std::string(unrelatedPrec)).c_str(), "Shader shares subexpression of different precision with an unrelated variable.",
					formatGLSL("${VERSION}"
						"${IN} ${IN_PREC} vec4 a_input;\n"
						"${OUT} ${UNRELATED_PREC} vec4 v_unrelated;\n"
						"${INVARIANT_DECLARATION}\n"
						"void main ()\n"
						"{\n"
						"	${UNRELATED_PREC} vec4 unrelated0 = a_input + vec4(0.1, 0.2, 0.3, 0.4);\n"
						"	${UNRELATED_PREC} vec4 unrelated1 = vec4(${MULTIPLIER}) * unrelated0.xywz + unrelated0;\n"
						"	${UNRELATED_PREC} vec4 unrelated2 = refract(unrelated1, unrelated0, distance(unrelated0, unrelated1));\n"
						"	v_unrelated = a_input + 0.02 * ${NORMALIZE_UNRELATED};\n"
						"	${IN_PREC} vec4 used0 = a_input + vec4(0.1, 0.2, 0.3, 0.4);\n"
						"	${IN_PREC} vec4 used1 = vec4(${MULTIPLIER}) * used0.xywz + used0;\n"
						"	${IN_PREC} vec4 used2 = refract(used1, used0, distance(used0, used1));\n"
						"	${INVARIANT_ASSIGN_0} = a_input + 0.02 * ${NORMALIZE_USED};\n"
						"	${INVARIANT_ASSIGN_1}\n"
						"}\n", FormatArgumentList(args[groupNdx])
						<< FormatArgument("UNRELATED_PREC", unrelatedPrec)
						<< FormatArgument("MULTIPLIER", multiplierStr)
						<< FormatArgument("NORMALIZE_USED", normalizationStrUsed)
						<< FormatArgument("NORMALIZE_UNRELATED", normalizationStrUnrelated)),
					formatGLSL("${VERSION}"
						"${IN} ${IN_PREC} vec4 a_input;\n"
						"${OUT} ${UNRELATED_PREC} vec4 v_unrelated;\n"
						"${INVARIANT_DECLARATION}\n"
						"void main ()\n"
						"{\n"
						"	v_unrelated = vec4(0.0, 0.0, 0.0, 0.0);\n"
						"	${IN_PREC} vec4 used0 = a_input + vec4(0.1, 0.2, 0.3, 0.4);\n"
						"	${IN_PREC} vec4 used1 = vec4(${MULTIPLIER}) * used0.xywz + used0;\n"
						"	${IN_PREC} vec4 used2 = refract(used1, used0, distance(used0, used1));\n"
						"	${INVARIANT_ASSIGN_0} = a_input + 0.02 * ${NORMALIZE_USED};\n"
						"	${INVARIANT_ASSIGN_1}\n"
						"}\n", FormatArgumentList(args[groupNdx])
						<< FormatArgument("UNRELATED_PREC", unrelatedPrec)
						<< FormatArgument("MULTIPLIER", multiplierStr)
						<< FormatArgument("NORMALIZE_USED", normalizationStrUsed)
						<< FormatArgument("NORMALIZE_UNRELATED", normalizationStrUnrelated)),
					formatGLSL("${VERSION}"
						"precision mediump float;\n"
						"${IN} ${UNRELATED_PREC} vec4 v_unrelated;\n"
						"${FRAG_DECLARATION}\n"
						"layout(binding = 0) uniform ColorUniform\n"
						"{\n"
						"	vec4 u_color;\n"
						"} ucolor;\n"
						"${OUT} vec4 fragColor;\n"
						"void main ()\n"
						"{\n"
						"	float blue = dot(v_unrelated, vec4(1.0, 1.0, 1.0, 1.0));\n"
						"	fragColor = vec4(ucolor.u_color.r, ucolor.u_color.g, blue, ucolor.u_color.a);\n"
						"}\n", FormatArgumentList(args[groupNdx])
						<< FormatArgument("UNRELATED_PREC", unrelatedPrec)
						<< FormatArgument("MULTIPLIER", multiplierStr)
						<< FormatArgument("NORMALIZE_USED", normalizationStrUsed)
						<< FormatArgument("NORMALIZE_UNRELATED", normalizationStrUnrelated))));
			}
		}

		// loops
		for (deUint32 groupNdx = 0u; groupNdx < VAR_GROUP_SIZE; ++groupNdx)
		{
			varGroup[groupNdx]->addChild(new InvarianceTest(testCtx, "loop_0", "Invariant value set using a loop",
				formatGLSL("${VERSION}"
					"${IN} ${IN_PREC} vec4 a_input;\n"
					"${OUT} highp vec4 v_unrelated;\n"
					"${INVARIANT_DECLARATION}\n"
					"void main ()\n"
					"{\n"
					"	${IN_PREC} vec4 value = a_input;\n"
					"	v_unrelated = vec4(0.0, 0.0, 0.0, 0.0);\n"
					"	for (mediump int i = 0; i < ${LOOP_ITERS}; ++i)\n"
					"	{\n"
					"		value *= ${LOOP_MULTIPLIER};\n"
					"		v_unrelated += value;\n"
					"	}\n"
					"	${INVARIANT_ASSIGN_0} = vec4(value.xyz / ${LOOP_NORM_LITERAL} + a_input.xyz * 0.1, 1.0);\n"
					"	${INVARIANT_ASSIGN_1}\n"
					"}\n", args[groupNdx]),
				formatGLSL("${VERSION}"
					"${IN} ${IN_PREC} vec4 a_input;\n"
					"${OUT} highp vec4 v_unrelated;\n"
					"${INVARIANT_DECLARATION}\n"
					"void main ()\n"
					"{\n"
					"	${IN_PREC} vec4 value = a_input;\n"
					"	v_unrelated = vec4(0.0, 0.0, 0.0, 0.0);\n"
					"	for (mediump int i = 0; i < ${LOOP_ITERS}; ++i)\n"
					"	{\n"
					"		value *= ${LOOP_MULTIPLIER};\n"
					"	}\n"
					"	${INVARIANT_ASSIGN_0} = vec4(value.xyz / ${LOOP_NORM_LITERAL} + a_input.xyz * 0.1, 1.0);\n"
					"	${INVARIANT_ASSIGN_1}\n"
					"}\n", args[groupNdx]),
				formatGLSL("${VERSION}"
					"precision mediump float;\n"
					"layout(location=0) in highp vec4 v_unrelated;\n"
					"${FRAG_DECLARATION}\n"
					"layout(binding = 0) uniform ColorUniform\n"
					"{\n"
					"	vec4 u_color;\n"
					"} ucolor;\n"
					"layout(location = 0) out vec4 fragColor;\n"
					"void main ()\n"
					"{\n"
					"	float blue = dot(v_unrelated, vec4(1.0, 1.0, 1.0, 1.0));\n"
					"	fragColor = vec4(ucolor.u_color.r, ucolor.u_color.g, blue, ucolor.u_color.a);\n"
					"}\n", args[groupNdx])));

			varGroup[groupNdx]->addChild(new InvarianceTest(testCtx, "loop_1", "Invariant value set using a loop",
				formatGLSL("${VERSION}"
					"${IN} ${IN_PREC} vec4 a_input;\n"
					"${OUT} mediump vec4 v_unrelated;\n"
					"${INVARIANT_DECLARATION}\n"
					"void main ()\n"
					"{\n"
					"	${IN_PREC} vec4 value = a_input;\n"
					"	for (mediump int i = 0; i < ${LOOP_ITERS}; ++i)\n"
					"	{\n"
					"		value *= ${LOOP_MULTIPLIER};\n"
					"		if (i == ${LOOP_ITERS_PARTIAL})\n"
					"			v_unrelated = value;\n"
					"	}\n"
					"	${INVARIANT_ASSIGN_0} = vec4(value.xyz / ${LOOP_NORM_LITERAL} + a_input.xyz * 0.1, 1.0);\n"
					"	${INVARIANT_ASSIGN_1}\n"
					"}\n", args[groupNdx]),
				formatGLSL("${VERSION}"
					"${IN} ${IN_PREC} vec4 a_input;\n"
					"${OUT} mediump vec4 v_unrelated;\n"
					"${INVARIANT_DECLARATION}\n"
					"void main ()\n"
					"{\n"
					"	${IN_PREC} vec4 value = a_input;\n"
					"	v_unrelated = vec4(0.0, 0.0, 0.0, 0.0);\n"
					"	for (mediump int i = 0; i < ${LOOP_ITERS}; ++i)\n"
					"	{\n"
					"		value *= ${LOOP_MULTIPLIER};\n"
					"	}\n"
					"	${INVARIANT_ASSIGN_0} = vec4(value.xyz / ${LOOP_NORM_LITERAL} + a_input.xyz * 0.1, 1.0);\n"
					"	${INVARIANT_ASSIGN_1}\n"
					"}\n", args[groupNdx]),
				formatGLSL(basicFragmentShader, args[groupNdx])));

			varGroup[groupNdx]->addChild(new InvarianceTest(testCtx, "loop_2", "Invariant value set using a loop",
				formatGLSL("${VERSION}"
					"${IN} ${IN_PREC} vec4 a_input;\n"
					"${OUT} mediump vec4 v_unrelated;\n"
					"${INVARIANT_DECLARATION}\n"
					"void main ()\n"
					"{\n"
					"	${IN_PREC} vec4 value = a_input;\n"
					"	v_unrelated = vec4(0.0, 0.0, -1.0, 1.0);\n"
					"	for (mediump int i = 0; i < ${LOOP_ITERS}; ++i)\n"
					"	{\n"
					"		value *= ${LOOP_MULTIPLIER};\n"
					"		if (i == ${LOOP_ITERS_PARTIAL})\n"
					"			${INVARIANT_ASSIGN_0} = a_input + 0.05 * vec4(fract(value.xyz / 1.0e${LOOP_NORM_FRACT_EXP}), 1.0);\n"
					"		else\n"
					"			v_unrelated = value + a_input;\n"
					"	${INVARIANT_ASSIGN_1}\n"
					"	}\n"
					"}\n", args[groupNdx]),
				formatGLSL("${VERSION}"
					"${IN} ${IN_PREC} vec4 a_input;\n"
					"${OUT} mediump vec4 v_unrelated;\n"
					"${INVARIANT_DECLARATION}\n"
					"void main ()\n"
					"{\n"
					"	${IN_PREC} vec4 value = a_input;\n"
					"	v_unrelated = vec4(0.0, 0.0, -1.0, 1.0);\n"
					"	for (mediump int i = 0; i < ${LOOP_ITERS}; ++i)\n"
					"	{\n"
					"		value *= ${LOOP_MULTIPLIER};\n"
					"		if (i == ${LOOP_ITERS_PARTIAL})\n"
					"			${INVARIANT_ASSIGN_0} = a_input + 0.05 * vec4(fract(value.xyz / 1.0e${LOOP_NORM_FRACT_EXP}), 1.0);\n"
					"		else\n"
					"			v_unrelated = vec4(0.0, 0.0, 0.0, 0.0);\n"
					"	${INVARIANT_ASSIGN_1}\n"
					"	}\n"
					"}\n", args[groupNdx]),
				formatGLSL(basicFragmentShader, args[groupNdx])));

			varGroup[groupNdx]->addChild(new InvarianceTest(testCtx, "loop_3", "Invariant value set using a loop",
				formatGLSL("${VERSION}"
					"${IN} ${IN_PREC} vec4 a_input;\n"
					"${OUT} mediump vec4 v_unrelated;\n"
					"${INVARIANT_DECLARATION}\n"
					"void main ()\n"
					"{\n"
					"	${IN_PREC} vec4 value = a_input;\n"
					"	${INVARIANT_ASSIGN_0} = vec4(0.0, 0.0, 0.0, 0.0);\n"
					"	v_unrelated = vec4(0.0, 0.0, 0.0, 0.0);\n"
					"	for (mediump int i = 0; i < ${LOOP_ITERS}; ++i)\n"
					"	{\n"
					"		value *= ${LOOP_MULTIPLIER};\n"
					"		${INVARIANT_ASSIGN_0} += vec4(value.xyz / ${SUM_LOOP_NORM_LITERAL} + a_input.xyz * 0.1, 1.0);\n"
					"		v_unrelated = ${INVARIANT_ASSIGN_0}.xyzx * a_input;\n"
					"	}\n"
					"	${INVARIANT_ASSIGN_1}\n"
					"}\n", args[groupNdx]),
				formatGLSL("${VERSION}"
					"${IN} ${IN_PREC} vec4 a_input;\n"
					"${OUT} mediump vec4 v_unrelated;\n"
					"${INVARIANT_DECLARATION}\n"
					"void main ()\n"
					"{\n"
					"	${IN_PREC} vec4 value = a_input;\n"
					"	${INVARIANT_ASSIGN_0} = vec4(0.0, 0.0, 0.0, 0.0);\n"
					"	v_unrelated = vec4(0.0, 0.0, 0.0, 0.0);\n"
					"	for (mediump int i = 0; i < ${LOOP_ITERS}; ++i)\n"
					"	{\n"
					"		value *= ${LOOP_MULTIPLIER};\n"
					"		${INVARIANT_ASSIGN_0} += vec4(value.xyz / ${SUM_LOOP_NORM_LITERAL} + a_input.xyz * 0.1, 1.0);\n"
					"	}\n"
					"	${INVARIANT_ASSIGN_1}\n"
					"}\n", args[groupNdx]),
				formatGLSL(basicFragmentShader, args[groupNdx])));

			varGroup[groupNdx]->addChild(new InvarianceTest(testCtx, "loop_4", "Invariant value set using a loop",
				formatGLSL("${VERSION}"
					"${IN} ${IN_PREC} vec4 a_input;\n"
					"${OUT} mediump vec4 v_unrelated;\n"
					"${INVARIANT_DECLARATION}\n"
					"void main ()\n"
					"{\n"
					"	${IN_PREC} vec4 position = vec4(0.0, 0.0, 0.0, 0.0);\n"
					"	${IN_PREC} vec4 value1 = a_input;\n"
					"	${IN_PREC} vec4 value2 = a_input;\n"
					"	v_unrelated = vec4(0.0, 0.0, 0.0, 0.0);\n"
					"	for (mediump int i = 0; i < ${LOOP_ITERS}; ++i)\n"
					"	{\n"
					"		value1 *= ${LOOP_MULTIPLIER};\n"
					"		v_unrelated = v_unrelated*1.3 + a_input.xyzx * value1.xyxw;\n"
					"	}\n"
					"	for (mediump int i = 0; i < ${LOOP_ITERS}; ++i)\n"
					"	{\n"
					"		value2 *= ${LOOP_MULTIPLIER};\n"
					"		position = position*1.3 + a_input.xyzx * value2.xyxw;\n"
					"	}\n"
					"	${INVARIANT_ASSIGN_0} = a_input + 0.05 * vec4(fract(position.xyz / 1.0e${LOOP_NORM_FRACT_EXP}), 1.0);\n"
					"	${INVARIANT_ASSIGN_1}\n"
					"}\n", args[groupNdx]),
				formatGLSL("${VERSION}"
					"${IN} ${IN_PREC} vec4 a_input;\n"
					"${OUT} mediump vec4 v_unrelated;\n"
					"${INVARIANT_DECLARATION}\n"
					"void main ()\n"
					"{\n"
					"	${IN_PREC} vec4 position = vec4(0.0, 0.0, 0.0, 0.0);\n"
					"	${IN_PREC} vec4 value2 = a_input;\n"
					"	v_unrelated = vec4(0.0, 0.0, 0.0, 0.0);\n"
					"	for (mediump int i = 0; i < ${LOOP_ITERS}; ++i)\n"
					"	{\n"
					"		value2 *= ${LOOP_MULTIPLIER};\n"
					"		position = position*1.3 + a_input.xyzx * value2.xyxw;\n"
					"	}\n"
					"	${INVARIANT_ASSIGN_0} = a_input + 0.05 * vec4(fract(position.xyz / 1.0e${LOOP_NORM_FRACT_EXP}), 1.0);\n"
					"	${INVARIANT_ASSIGN_1}\n"
					"}\n", args[groupNdx]),
				formatGLSL(basicFragmentShader, args[groupNdx])));
		}
		invarianceGroup->addChild(group);
	}
	return invarianceGroup.release();
}

} // namespace sr

} // namespace vkt
