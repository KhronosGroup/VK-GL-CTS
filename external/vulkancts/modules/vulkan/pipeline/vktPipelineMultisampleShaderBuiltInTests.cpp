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
*//*
* \file vktPipelineMultisampleShaderBuiltInTests.cpp
* \brief Multisample Shader BuiltIn Tests
*//*--------------------------------------------------------------------*/

#include "vktPipelineMultisampleShaderBuiltInTests.hpp"
#include "vktPipelineMultisampleBaseResolveAndPerSampleFetch.hpp"
#include "vktPipelineMakeUtil.hpp"

#include "vkBuilderUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBarrierUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkTypeUtil.hpp"

#include "tcuVectorUtil.hpp"
#include "tcuTestLog.hpp"

#include <set>
#include <cmath>

using std::set;

namespace vkt
{
namespace pipeline
{
namespace multisample
{

using namespace vk;

struct VertexDataNdc
{
	VertexDataNdc (const tcu::Vec4& posNdc) : positionNdc(posNdc) {}

	tcu::Vec4 positionNdc;
};

MultisampleInstanceBase::VertexDataDesc getVertexDataDescriptonNdc (void)
{
	MultisampleInstanceBase::VertexDataDesc vertexDataDesc;

	vertexDataDesc.verticesCount	 = 4u;
	vertexDataDesc.dataStride		 = sizeof(VertexDataNdc);
	vertexDataDesc.dataSize			 = vertexDataDesc.verticesCount * vertexDataDesc.dataStride;
	vertexDataDesc.primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

	const VkVertexInputAttributeDescription vertexAttribPositionNdc =
	{
		0u,											// deUint32	location;
		0u,											// deUint32	binding;
		VK_FORMAT_R32G32B32A32_SFLOAT,				// VkFormat	format;
		DE_OFFSET_OF(VertexDataNdc, positionNdc),	// deUint32	offset;
	};

	vertexDataDesc.vertexAttribDescVec.push_back(vertexAttribPositionNdc);

	return vertexDataDesc;
}

void uploadVertexDataNdc (const Allocation& vertexBufferAllocation, const MultisampleInstanceBase::VertexDataDesc& vertexDataDescripton)
{
	std::vector<VertexDataNdc> vertices;

	vertices.push_back(VertexDataNdc(tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f)));
	vertices.push_back(VertexDataNdc(tcu::Vec4( 1.0f, -1.0f, 0.0f, 1.0f)));
	vertices.push_back(VertexDataNdc(tcu::Vec4(-1.0f,  1.0f, 0.0f, 1.0f)));
	vertices.push_back(VertexDataNdc(tcu::Vec4( 1.0f,  1.0f, 0.0f, 1.0f)));

	deMemcpy(vertexBufferAllocation.getHostPtr(), dataPointer(vertices), static_cast<std::size_t>(vertexDataDescripton.dataSize));
}

struct VertexDataNdcScreen
{
	VertexDataNdcScreen (const tcu::Vec4& posNdc, const tcu::Vec2& posScreen) : positionNdc(posNdc), positionScreen(posScreen) {}

	tcu::Vec4 positionNdc;
	tcu::Vec2 positionScreen;
};

MultisampleInstanceBase::VertexDataDesc getVertexDataDescriptonNdcScreen (void)
{
	MultisampleInstanceBase::VertexDataDesc vertexDataDesc;

	vertexDataDesc.verticesCount	 = 4u;
	vertexDataDesc.dataStride		 = sizeof(VertexDataNdcScreen);
	vertexDataDesc.dataSize			 = vertexDataDesc.verticesCount * vertexDataDesc.dataStride;
	vertexDataDesc.primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

	const VkVertexInputAttributeDescription vertexAttribPositionNdc =
	{
		0u,													// deUint32	location;
		0u,													// deUint32	binding;
		VK_FORMAT_R32G32B32A32_SFLOAT,						// VkFormat	format;
		DE_OFFSET_OF(VertexDataNdcScreen, positionNdc),		// deUint32	offset;
	};

	vertexDataDesc.vertexAttribDescVec.push_back(vertexAttribPositionNdc);

	const VkVertexInputAttributeDescription vertexAttribPositionScreen =
	{
		1u,													// deUint32	location;
		0u,													// deUint32	binding;
		VK_FORMAT_R32G32_SFLOAT,							// VkFormat	format;
		DE_OFFSET_OF(VertexDataNdcScreen, positionScreen),	// deUint32	offset;
	};

	vertexDataDesc.vertexAttribDescVec.push_back(vertexAttribPositionScreen);

	return vertexDataDesc;
}

void uploadVertexDataNdcScreen (const Allocation& vertexBufferAllocation, const MultisampleInstanceBase::VertexDataDesc& vertexDataDescripton, const tcu::Vec2& screenSize)
{
	std::vector<VertexDataNdcScreen> vertices;

	vertices.push_back(VertexDataNdcScreen(tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f), tcu::Vec2(0.0f, 0.0f)));
	vertices.push_back(VertexDataNdcScreen(tcu::Vec4( 1.0f, -1.0f, 0.0f, 1.0f), tcu::Vec2(screenSize.x(), 0.0f)));
	vertices.push_back(VertexDataNdcScreen(tcu::Vec4(-1.0f,  1.0f, 0.0f, 1.0f), tcu::Vec2(0.0f, screenSize.y())));
	vertices.push_back(VertexDataNdcScreen(tcu::Vec4( 1.0f,  1.0f, 0.0f, 1.0f), tcu::Vec2(screenSize.x(), screenSize.y())));

	deMemcpy(vertexBufferAllocation.getHostPtr(), dataPointer(vertices), static_cast<std::size_t>(vertexDataDescripton.dataSize));
}

bool checkForErrorMS (const vk::VkImageCreateInfo& imageMSInfo, const std::vector<tcu::ConstPixelBufferAccess>& dataPerSample, const deUint32 errorCompNdx)
{
	const deUint32 numSamples = static_cast<deUint32>(imageMSInfo.samples);

	for (deUint32 z = 0u; z < imageMSInfo.extent.depth;  ++z)
	for (deUint32 y = 0u; y < imageMSInfo.extent.height; ++y)
	for (deUint32 x = 0u; x < imageMSInfo.extent.width;  ++x)
	{
		for (deUint32 sampleNdx = 0u; sampleNdx < numSamples; ++sampleNdx)
		{
			const deUint32 errorComponent = dataPerSample[sampleNdx].getPixelUint(x, y, z)[errorCompNdx];

			if (errorComponent > 0)
				return true;
		}
	}

	return false;
}

bool checkForErrorRS (const vk::VkImageCreateInfo& imageRSInfo, const tcu::ConstPixelBufferAccess& dataRS, const deUint32 errorCompNdx)
{
	for (deUint32 z = 0u; z < imageRSInfo.extent.depth;  ++z)
	for (deUint32 y = 0u; y < imageRSInfo.extent.height; ++y)
	for (deUint32 x = 0u; x < imageRSInfo.extent.width;  ++x)
	{
		const deUint32 errorComponent = dataRS.getPixelUint(x, y, z)[errorCompNdx];

		if (errorComponent > 0)
			return true;
	}

	return false;
}

template <typename CaseClassName>
class MSCase : public MSCaseBaseResolveAndPerSampleFetch
{
public:
								MSCase			(tcu::TestContext&		testCtx,
												 const std::string&		name,
												 const ImageMSParams&	imageMSParams)
								: MSCaseBaseResolveAndPerSampleFetch(testCtx, name, imageMSParams) {}

	virtual void				checkSupport	(Context& context) const;
	void						init			(void);
	void						initPrograms	(vk::SourceCollections& programCollection) const;
	TestInstance*				createInstance	(Context&				context) const;
	static MultisampleCaseBase*	createCase		(tcu::TestContext&		testCtx,
												 const std::string&		name,
												 const ImageMSParams&	imageMSParams);
};
#ifndef CTS_USES_VULKANSC
template <typename CaseClassName>
void MSCase<CaseClassName>::checkSupport(Context& context) const
{
	checkGraphicsPipelineLibrarySupport(context);
}
#endif // CTS_USES_VULKANSC

template <typename CaseClassName>
MultisampleCaseBase* MSCase<CaseClassName>::createCase (tcu::TestContext& testCtx, const std::string& name, const ImageMSParams& imageMSParams)
{
	return new MSCase<CaseClassName>(testCtx, name, imageMSParams);
}

template <typename InstanceClassName>
class MSInstance : public MSInstanceBaseResolveAndPerSampleFetch
{
public:
													MSInstance				(Context&											context,
																			 const ImageMSParams&								imageMSParams)
													: MSInstanceBaseResolveAndPerSampleFetch(context, imageMSParams) {}

	VertexDataDesc									getVertexDataDescripton	(void) const;
	void											uploadVertexData		(const Allocation&									vertexBufferAllocation,
																			 const VertexDataDesc&								vertexDataDescripton) const;

	tcu::TestStatus									verifyImageData			(const vk::VkImageCreateInfo&						imageMSInfo,
																			 const vk::VkImageCreateInfo&						imageRSInfo,
																			 const std::vector<tcu::ConstPixelBufferAccess>&	dataPerSample,
																			 const tcu::ConstPixelBufferAccess&					dataRS) const;

	virtual VkPipelineMultisampleStateCreateInfo	getMSStateCreateInfo	(const ImageMSParams&								imageMSParams) const
	{
		return MSInstanceBaseResolveAndPerSampleFetch::getMSStateCreateInfo(imageMSParams);
	}
};

class MSInstanceSampleID;

template<> MultisampleInstanceBase::VertexDataDesc MSInstance<MSInstanceSampleID>::getVertexDataDescripton (void) const
{
	return getVertexDataDescriptonNdc();
}

template<> void MSInstance<MSInstanceSampleID>::uploadVertexData (const Allocation& vertexBufferAllocation, const VertexDataDesc& vertexDataDescripton) const
{
	uploadVertexDataNdc(vertexBufferAllocation, vertexDataDescripton);
}

template<> tcu::TestStatus MSInstance<MSInstanceSampleID>::verifyImageData	(const vk::VkImageCreateInfo&						imageMSInfo,
																			 const vk::VkImageCreateInfo&						imageRSInfo,
																			 const std::vector<tcu::ConstPixelBufferAccess>&	dataPerSample,
																			 const tcu::ConstPixelBufferAccess&					dataRS) const
{
	DE_UNREF(imageRSInfo);
	DE_UNREF(dataRS);

	const deUint32 numSamples = static_cast<deUint32>(imageMSInfo.samples);

	for (deUint32 sampleNdx = 0u; sampleNdx < numSamples; ++sampleNdx)
	{
		for (deUint32 z = 0u; z < imageMSInfo.extent.depth;  ++z)
		for (deUint32 y = 0u; y < imageMSInfo.extent.height; ++y)
		for (deUint32 x = 0u; x < imageMSInfo.extent.width;  ++x)
		{
			const deUint32 sampleID = dataPerSample[sampleNdx].getPixelUint(x, y, z).x();

			if (sampleID != sampleNdx)
				return tcu::TestStatus::fail("gl_SampleID does not have correct value");
		}
	}

	return tcu::TestStatus::pass("Passed");
}

class MSCaseSampleID;

template<> void MSCase<MSCaseSampleID>::checkSupport (Context& context) const
{
	checkGraphicsPipelineLibrarySupport(context);
	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SAMPLE_RATE_SHADING);
}

template<> void MSCase<MSCaseSampleID>::init (void)
{
	m_testCtx.getLog()
		<< tcu::TestLog::Message
		<< "Writing gl_SampleID to the red channel of the texture and verifying texture values.\n"
		<< "Expecting value N at sample index N of a multisample texture.\n"
		<< tcu::TestLog::EndMessage;

	MultisampleCaseBase::init();
}

template<> void MSCase<MSCaseSampleID>::initPrograms (vk::SourceCollections& programCollection) const
{
	MSCaseBaseResolveAndPerSampleFetch::initPrograms(programCollection);

	// Create vertex shader
	std::ostringstream vs;

	vs << "#version 440\n"
		<< "layout(location = 0) in vec4 vs_in_position_ndc;\n"
		<< "\n"
		<< "out gl_PerVertex {\n"
		<< "	vec4  gl_Position;\n"
		<< "};\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	gl_Position	= vs_in_position_ndc;\n"
		<< "}\n";

	programCollection.glslSources.add("vertex_shader") << glu::VertexSource(vs.str());

	// Create fragment shader
	std::ostringstream fs;

	fs << "#version 440\n"
		<< "\n"
		<< "layout(location = 0) out vec4 fs_out_color;\n"
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	fs_out_color = vec4(float(gl_SampleID) / float(255), 0.0, 0.0, 1.0);\n"
		<< "}\n";

	programCollection.glslSources.add("fragment_shader") << glu::FragmentSource(fs.str());
}

template<> TestInstance* MSCase<MSCaseSampleID>::createInstance (Context& context) const
{
	return new MSInstance<MSInstanceSampleID>(context, m_imageMSParams);
}

class MSInstanceSamplePosDistribution;

template<> MultisampleInstanceBase::VertexDataDesc MSInstance<MSInstanceSamplePosDistribution>::getVertexDataDescripton (void) const
{
	return getVertexDataDescriptonNdc();
}

template<> void MSInstance<MSInstanceSamplePosDistribution>::uploadVertexData (const Allocation& vertexBufferAllocation, const VertexDataDesc& vertexDataDescripton) const
{
	uploadVertexDataNdc(vertexBufferAllocation, vertexDataDescripton);
}

template<> tcu::TestStatus MSInstance<MSInstanceSamplePosDistribution>::verifyImageData	(const vk::VkImageCreateInfo&						imageMSInfo,
																						 const vk::VkImageCreateInfo&						imageRSInfo,
																						 const std::vector<tcu::ConstPixelBufferAccess>&	dataPerSample,
																						 const tcu::ConstPixelBufferAccess&					dataRS) const
{
	const deUint32 numSamples = static_cast<deUint32>(imageMSInfo.samples);

	// approximate Bates distribution as normal
	const float variance = (1.0f / (12.0f * (float)numSamples));
	const float standardDeviation = deFloatSqrt(variance);

	// 95% of means of sample positions are within 2 standard deviations if
	// they were randomly assigned. Sample patterns are expected to be more
	// uniform than a random pattern.
	const float distanceThreshold = 2.0f * standardDeviation;

	for (deUint32 z = 0u; z < imageRSInfo.extent.depth;  ++z)
	for (deUint32 y = 0u; y < imageRSInfo.extent.height; ++y)
	for (deUint32 x = 0u; x < imageRSInfo.extent.width;  ++x)
	{
		const deUint32 errorComponent = dataRS.getPixelUint(x, y, z).z();

		if (errorComponent > 0)
			return tcu::TestStatus::fail("gl_SamplePosition is not within interval [0,1]");

		if (numSamples >= VK_SAMPLE_COUNT_4_BIT)
		{
			const tcu::Vec2 averageSamplePos	= tcu::Vec2((float)dataRS.getPixelUint(x, y, z).x() / 255.0f, (float)dataRS.getPixelUint(x, y, z).y() / 255.0f);
			const tcu::Vec2	distanceFromCenter	= tcu::abs(averageSamplePos - tcu::Vec2(0.5f, 0.5f));

			if (distanceFromCenter.x() > distanceThreshold || distanceFromCenter.y() > distanceThreshold)
				return tcu::TestStatus::fail("Sample positions are not uniformly distributed within the pixel");
		}
	}

	for (deUint32 z = 0u; z < imageMSInfo.extent.depth;  ++z)
	for (deUint32 y = 0u; y < imageMSInfo.extent.height; ++y)
	for (deUint32 x = 0u; x < imageMSInfo.extent.width;  ++x)
	{
		std::vector<tcu::Vec2> samplePositions(numSamples);

		for (deUint32 sampleNdx = 0u; sampleNdx < numSamples; ++sampleNdx)
		{
			const deUint32 errorComponent = dataPerSample[sampleNdx].getPixelUint(x, y, z).z();

			if (errorComponent > 0)
				return tcu::TestStatus::fail("gl_SamplePosition is not within interval [0,1]");

			samplePositions[sampleNdx] = tcu::Vec2( (float)dataPerSample[sampleNdx].getPixelUint(x, y, z).x() / 255.0f,
													(float)dataPerSample[sampleNdx].getPixelUint(x, y, z).y() / 255.0f);
		}

		for (deUint32 sampleNdxA = 0u;				sampleNdxA < numSamples; ++sampleNdxA)
		for (deUint32 sampleNdxB = sampleNdxA + 1u; sampleNdxB < numSamples; ++sampleNdxB)
		{
			if (samplePositions[sampleNdxA] == samplePositions[sampleNdxB])
				return tcu::TestStatus::fail("Two samples have the same position");
		}

		if (numSamples >= VK_SAMPLE_COUNT_4_BIT)
		{
			tcu::Vec2 averageSamplePos(0.0f, 0.0f);

			for (deUint32 sampleNdx = 0u; sampleNdx < numSamples; ++sampleNdx)
			{
				averageSamplePos.x() += samplePositions[sampleNdx].x();
				averageSamplePos.y() += samplePositions[sampleNdx].y();
			}

			averageSamplePos.x() /= (float)numSamples;
			averageSamplePos.y() /= (float)numSamples;

			const tcu::Vec2	distanceFromCenter = tcu::abs(averageSamplePos - tcu::Vec2(0.5f, 0.5f));

			if (distanceFromCenter.x() > distanceThreshold || distanceFromCenter.y() > distanceThreshold)
				return tcu::TestStatus::fail("Sample positions are not uniformly distributed within the pixel");
		}
	}

	return tcu::TestStatus::pass("Passed");
}

class MSCaseSamplePosDistribution;

template<> void MSCase<MSCaseSamplePosDistribution>::checkSupport (Context& context) const
{
	checkGraphicsPipelineLibrarySupport(context);
	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SAMPLE_RATE_SHADING);
}

template<> void MSCase<MSCaseSamplePosDistribution>::init (void)
{
	m_testCtx.getLog()
		<< tcu::TestLog::Message
		<< "Verifying gl_SamplePosition value with multisample targets:\n"
		<< "	a) Expect legal sample position.\n"
		<< "	b) Sample position is unique within the set of all sample positions of a pixel.\n"
		<< "	c) Sample position distribution is uniform or almost uniform.\n"
		<< tcu::TestLog::EndMessage;

	MultisampleCaseBase::init();
}

template<> void MSCase<MSCaseSamplePosDistribution>::initPrograms (vk::SourceCollections& programCollection) const
{
	MSCaseBaseResolveAndPerSampleFetch::initPrograms(programCollection);

	// Create vertex shader
	std::ostringstream vs;

	vs << "#version 440\n"
		<< "layout(location = 0) in vec4 vs_in_position_ndc;\n"
		<< "\n"
		<< "out gl_PerVertex {\n"
		<< "	vec4  gl_Position;\n"
		<< "};\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	gl_Position	= vs_in_position_ndc;\n"
		<< "}\n";

	programCollection.glslSources.add("vertex_shader") << glu::VertexSource(vs.str());

	// Create fragment shader
	std::ostringstream fs;

	fs << "#version 440\n"
		<< "\n"
		<< "layout(location = 0) out vec4 fs_out_color;\n"
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	if (gl_SamplePosition.x < 0.0 || gl_SamplePosition.x > 1.0 || gl_SamplePosition.y < 0.0 || gl_SamplePosition.y > 1.0)\n"
		"		fs_out_color = vec4(0.0, 0.0, 1.0, 1.0);\n"
		"	else\n"
		"		fs_out_color = vec4(gl_SamplePosition.x, gl_SamplePosition.y, 0.0, 1.0);\n"
		"}\n";

	programCollection.glslSources.add("fragment_shader") << glu::FragmentSource(fs.str());
}

template<> TestInstance* MSCase<MSCaseSamplePosDistribution>::createInstance (Context& context) const
{
	return new MSInstance<MSInstanceSamplePosDistribution>(context, m_imageMSParams);
}

class MSInstanceSamplePosCorrectness;

template<> MultisampleInstanceBase::VertexDataDesc MSInstance<MSInstanceSamplePosCorrectness>::getVertexDataDescripton (void) const
{
	return getVertexDataDescriptonNdcScreen();
}

template<> void MSInstance<MSInstanceSamplePosCorrectness>::uploadVertexData (const Allocation& vertexBufferAllocation, const VertexDataDesc& vertexDataDescripton) const
{
	const tcu::UVec3 layerSize = getLayerSize(IMAGE_TYPE_2D, m_imageMSParams.imageSize);

	uploadVertexDataNdcScreen(vertexBufferAllocation, vertexDataDescripton, tcu::Vec2(static_cast<float>(layerSize.x()), static_cast<float>(layerSize.y())));
}

template<> tcu::TestStatus MSInstance<MSInstanceSamplePosCorrectness>::verifyImageData	(const vk::VkImageCreateInfo&						imageMSInfo,
																						 const vk::VkImageCreateInfo&						imageRSInfo,
																						 const std::vector<tcu::ConstPixelBufferAccess>&	dataPerSample,
																						 const tcu::ConstPixelBufferAccess&					dataRS) const
{
	if (checkForErrorMS(imageMSInfo, dataPerSample, 0))
		return tcu::TestStatus::fail("Varying values are not sampled at gl_SamplePosition");

	if (checkForErrorRS(imageRSInfo, dataRS, 0))
		return tcu::TestStatus::fail("Varying values are not sampled at gl_SamplePosition");

	return tcu::TestStatus::pass("Passed");
}

class MSCaseSamplePosCorrectness;

template<> void MSCase<MSCaseSamplePosCorrectness>::checkSupport (Context& context) const
{
	checkGraphicsPipelineLibrarySupport(context);
	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SAMPLE_RATE_SHADING);
}

template<> void MSCase<MSCaseSamplePosCorrectness>::init (void)
{
	m_testCtx.getLog()
		<< tcu::TestLog::Message
		<< "Verifying gl_SamplePosition correctness:\n"
		<< "	1) Varying values should be sampled at the sample position.\n"
		<< "		=> fract(position_screen) == gl_SamplePosition\n"
		<< tcu::TestLog::EndMessage;

	MultisampleCaseBase::init();
}

template<> void MSCase<MSCaseSamplePosCorrectness>::initPrograms (vk::SourceCollections& programCollection) const
{
	MSCaseBaseResolveAndPerSampleFetch::initPrograms(programCollection);

	// Create vertex shaders
	std::ostringstream vs;

	vs	<< "#version 440\n"
		<< "layout(location = 0) in vec4 vs_in_position_ndc;\n"
		<< "layout(location = 1) in vec2 vs_in_position_screen;\n"
		<< "\n"
		<< "layout(location = 0) sample out vec2 vs_out_position_screen;\n"
		<< "\n"
		<< "out gl_PerVertex {\n"
		<< "	vec4  gl_Position;\n"
		<< "};\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	gl_Position				= vs_in_position_ndc;\n"
		<< "	vs_out_position_screen	= vs_in_position_screen;\n"
		<< "}\n";

	programCollection.glslSources.add("vertex_shader") << glu::VertexSource(vs.str());

	// Create fragment shader
	std::ostringstream fs;

	fs	<< "#version 440\n"
		<< "layout(location = 0) sample in vec2 fs_in_position_screen;\n"
		<< "\n"
		<< "layout(location = 0) out vec4 fs_out_color;\n"
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	const float threshold = 0.15625; // 4 subpixel bits. Assume 3 accurate bits + 0.03125 for other errors\n"
		<< "	const ivec2 nearby_pixel = ivec2(floor(fs_in_position_screen));\n"
		<< "	bool ok	= false;\n"
		<< "\n"
		<< "	// sample at edge + inaccuaries may cause us to round to any neighboring pixel\n"
		<< "	// check all neighbors for any match\n"
		<< "	for (int dy = -1; dy <= 1; ++dy)\n"
		<< "	for (int dx = -1; dx <= 1; ++dx)\n"
		<< "	{\n"
		<< "		ivec2 current_pixel			= nearby_pixel + ivec2(dx, dy);\n"
		<< "		vec2 position_inside_pixel	= vec2(current_pixel) + gl_SamplePosition;\n"
		<< "		vec2 position_diff			= abs(position_inside_pixel - fs_in_position_screen);\n"
		<< "\n"
		<< "		if (all(lessThan(position_diff, vec2(threshold))))\n"
		<< "			ok = true;\n"
		<< "	}\n"
		<< "\n"
		<< "	if (ok)\n"
		<< "		fs_out_color = vec4(0.0, 1.0, 0.0, 1.0);\n"
		<< "	else\n"
		<< "		fs_out_color = vec4(1.0, 0.0, 0.0, 1.0);\n"
		<< "}\n";

	programCollection.glslSources.add("fragment_shader") << glu::FragmentSource(fs.str());
}

template<> TestInstance* MSCase<MSCaseSamplePosCorrectness>::createInstance (Context& context) const
{
	return new MSInstance<MSInstanceSamplePosCorrectness>(context, m_imageMSParams);
}

class MSInstanceSampleMaskPattern : public MSInstanceBaseResolveAndPerSampleFetch
{
public:
											MSInstanceSampleMaskPattern	(Context&											context,
																		 const ImageMSParams&								imageMSParams);

	VkPipelineMultisampleStateCreateInfo	getMSStateCreateInfo		(const ImageMSParams&								imageMSParams) const;

	const VkDescriptorSetLayout*			createMSPassDescSetLayout	(const ImageMSParams&								imageMSParams);

	const VkDescriptorSet*					createMSPassDescSet			(const ImageMSParams&								imageMSParams,
																		 const VkDescriptorSetLayout*						descSetLayout);

	VertexDataDesc							getVertexDataDescripton		(void) const;

	void									uploadVertexData			(const Allocation&									vertexBufferAllocation,
																		 const VertexDataDesc&								vertexDataDescripton) const;

	tcu::TestStatus							verifyImageData				(const vk::VkImageCreateInfo&						imageMSInfo,
																		 const vk::VkImageCreateInfo&						imageRSInfo,
																		 const std::vector<tcu::ConstPixelBufferAccess>&	dataPerSample,
																		 const tcu::ConstPixelBufferAccess&					dataRS) const;
protected:

	VkSampleMask					m_sampleMask;
	Move<VkDescriptorSetLayout>		m_descriptorSetLayout;
	Move<VkDescriptorPool>			m_descriptorPool;
	Move<VkDescriptorSet>			m_descriptorSet;
	de::MovePtr<BufferWithMemory>	m_buffer;
};

MSInstanceSampleMaskPattern::MSInstanceSampleMaskPattern (Context& context, const ImageMSParams& imageMSParams) : MSInstanceBaseResolveAndPerSampleFetch(context, imageMSParams)
{
	m_sampleMask = 0xAAAAAAAAu & ((1u << imageMSParams.numSamples) - 1u);
}

VkPipelineMultisampleStateCreateInfo MSInstanceSampleMaskPattern::getMSStateCreateInfo (const ImageMSParams& imageMSParams) const
{
	const VkPipelineMultisampleStateCreateInfo multisampleStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,		// VkStructureType							sType;
		DE_NULL,														// const void*								pNext;
		(VkPipelineMultisampleStateCreateFlags)0u,						// VkPipelineMultisampleStateCreateFlags	flags;
		imageMSParams.numSamples,										// VkSampleCountFlagBits					rasterizationSamples;
		VK_FALSE,														// VkBool32									sampleShadingEnable;
		imageMSParams.shadingRate,										// float									minSampleShading;
		&m_sampleMask,													// const VkSampleMask*						pSampleMask;
		VK_FALSE,														// VkBool32									alphaToCoverageEnable;
		VK_FALSE,														// VkBool32									alphaToOneEnable;
	};

	return multisampleStateInfo;
}

const VkDescriptorSetLayout* MSInstanceSampleMaskPattern::createMSPassDescSetLayout (const ImageMSParams& imageMSParams)
{
	DE_UNREF(imageMSParams);

	const DeviceInterface&		deviceInterface = m_context.getDeviceInterface();
	const VkDevice				device			= m_context.getDevice();

	// Create descriptor set layout
	m_descriptorSetLayout = DescriptorSetLayoutBuilder()
		.addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
		.build(deviceInterface, device);

	return &m_descriptorSetLayout.get();
}

const VkDescriptorSet* MSInstanceSampleMaskPattern::createMSPassDescSet (const ImageMSParams& imageMSParams, const VkDescriptorSetLayout* descSetLayout)
{
	DE_UNREF(imageMSParams);

	const DeviceInterface&		deviceInterface = m_context.getDeviceInterface();
	const VkDevice				device			= m_context.getDevice();
	Allocator&					allocator		= m_context.getDefaultAllocator();

	// Create descriptor pool
	m_descriptorPool = DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u)
		.build(deviceInterface, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

	// Create descriptor set
	m_descriptorSet = makeDescriptorSet(deviceInterface, device, *m_descriptorPool, *descSetLayout);

	const VkBufferCreateInfo bufferSampleMaskInfo = makeBufferCreateInfo(sizeof(VkSampleMask), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

	m_buffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(deviceInterface, device, allocator, bufferSampleMaskInfo, MemoryRequirement::HostVisible));

	deMemcpy(m_buffer->getAllocation().getHostPtr(), &m_sampleMask, sizeof(VkSampleMask));

	flushAlloc(deviceInterface, device, m_buffer->getAllocation());

	const VkDescriptorBufferInfo descBufferInfo = makeDescriptorBufferInfo(**m_buffer, 0u, sizeof(VkSampleMask));

	DescriptorSetUpdateBuilder()
		.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &descBufferInfo)
		.update(deviceInterface, device);

	return &m_descriptorSet.get();
}

MultisampleInstanceBase::VertexDataDesc MSInstanceSampleMaskPattern::getVertexDataDescripton (void) const
{
	return getVertexDataDescriptonNdc();
}

void MSInstanceSampleMaskPattern::uploadVertexData (const Allocation& vertexBufferAllocation, const VertexDataDesc& vertexDataDescripton) const
{
	uploadVertexDataNdc(vertexBufferAllocation, vertexDataDescripton);
}

tcu::TestStatus	MSInstanceSampleMaskPattern::verifyImageData	(const vk::VkImageCreateInfo&						imageMSInfo,
																 const vk::VkImageCreateInfo&						imageRSInfo,
																 const std::vector<tcu::ConstPixelBufferAccess>&	dataPerSample,
																 const tcu::ConstPixelBufferAccess&					dataRS) const
{
	DE_UNREF(imageRSInfo);
	DE_UNREF(dataRS);

	if (checkForErrorMS(imageMSInfo, dataPerSample, 0))
		return tcu::TestStatus::fail("gl_SampleMaskIn bits have not been killed by pSampleMask state");

	return tcu::TestStatus::pass("Passed");
}

class MSCaseSampleMaskPattern;

template<> void MSCase<MSCaseSampleMaskPattern>::init (void)
{
	m_testCtx.getLog()
		<< tcu::TestLog::Message
		<< "Verifying gl_SampleMaskIn value with pSampleMask state. gl_SampleMaskIn does not contain any bits set that are have been killed by pSampleMask state. Expecting:\n"
		<< "Expected result: gl_SampleMaskIn AND ~(pSampleMask) should be zero.\n"
		<< tcu::TestLog::EndMessage;

	MultisampleCaseBase::init();
}

template<> void MSCase<MSCaseSampleMaskPattern>::initPrograms (vk::SourceCollections& programCollection) const
{
	MSCaseBaseResolveAndPerSampleFetch::initPrograms(programCollection);

	// Create vertex shader
	std::ostringstream vs;

	vs << "#version 440\n"
		<< "layout(location = 0) in vec4 vs_in_position_ndc;\n"
		<< "\n"
		<< "out gl_PerVertex {\n"
		<< "	vec4  gl_Position;\n"
		<< "};\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	gl_Position	= vs_in_position_ndc;\n"
		<< "}\n";

	programCollection.glslSources.add("vertex_shader") << glu::VertexSource(vs.str());

	// Create fragment shader
	std::ostringstream fs;

	fs << "#version 440\n"
		<< "\n"
		<< "layout(location = 0) out vec4 fs_out_color;\n"
		<< "\n"
		<< "layout(set = 0, binding = 0, std140) uniform SampleMaskBlock\n"
		<< "{\n"
		<< "	int sampleMaskPattern;\n"
		<< "};"
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	if ((gl_SampleMaskIn[0] & ~sampleMaskPattern) != 0)\n"
		<< "		fs_out_color = vec4(1.0, 0.0, 0.0, 1.0);\n"
		<< "	else\n"
		<< "		fs_out_color = vec4(0.0, 1.0, 0.0, 1.0);\n"
		<< "}\n";

	programCollection.glslSources.add("fragment_shader") << glu::FragmentSource(fs.str());
}

template<> TestInstance* MSCase<MSCaseSampleMaskPattern>::createInstance (Context& context) const
{
	return new MSInstanceSampleMaskPattern(context, m_imageMSParams);
}

class MSInstanceSampleMaskBitCount;

template<> MultisampleInstanceBase::VertexDataDesc MSInstance<MSInstanceSampleMaskBitCount>::getVertexDataDescripton (void) const
{
	return getVertexDataDescriptonNdc();
}

template<> void MSInstance<MSInstanceSampleMaskBitCount>::uploadVertexData (const Allocation& vertexBufferAllocation, const VertexDataDesc& vertexDataDescripton) const
{
	uploadVertexDataNdc(vertexBufferAllocation, vertexDataDescripton);
}

template<> tcu::TestStatus MSInstance<MSInstanceSampleMaskBitCount>::verifyImageData	(const vk::VkImageCreateInfo&						imageMSInfo,
																						 const vk::VkImageCreateInfo&						imageRSInfo,
																						 const std::vector<tcu::ConstPixelBufferAccess>&	dataPerSample,
																						 const tcu::ConstPixelBufferAccess&					dataRS) const
{
	DE_UNREF(imageRSInfo);
	DE_UNREF(dataRS);

	if (checkForErrorMS(imageMSInfo, dataPerSample, 0))
		return tcu::TestStatus::fail("gl_SampleMaskIn has an illegal number of bits for some shader invocations");

	return tcu::TestStatus::pass("Passed");
}

class MSCaseSampleMaskBitCount;

template<> void MSCase<MSCaseSampleMaskBitCount>::checkSupport (Context& context) const
{
	checkGraphicsPipelineLibrarySupport(context);
	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SAMPLE_RATE_SHADING);
}

template<> void MSCase<MSCaseSampleMaskBitCount>::init (void)
{
	m_testCtx.getLog()
		<< tcu::TestLog::Message
		<< "Verifying gl_SampleMaskIn.\n"
		<< "	Fragment shader will be invoked numSamples times.\n"
		<< "	=> gl_SampleMaskIn should have a number of bits that depends on the shading rate.\n"
		<< tcu::TestLog::EndMessage;

	MultisampleCaseBase::init();
}

template<> void MSCase<MSCaseSampleMaskBitCount>::initPrograms (vk::SourceCollections& programCollection) const
{
	MSCaseBaseResolveAndPerSampleFetch::initPrograms(programCollection);

	// Create vertex shader
	std::ostringstream vs;

	vs << "#version 440\n"
		<< "layout(location = 0) in vec4 vs_in_position_ndc;\n"
		<< "\n"
		<< "out gl_PerVertex {\n"
		<< "	vec4  gl_Position;\n"
		<< "};\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	gl_Position	= vs_in_position_ndc;\n"
		<< "}\n";

	programCollection.glslSources.add("vertex_shader") << glu::VertexSource(vs.str());

	// Create fragment shader
	std::ostringstream fs;

	// The worst case scenario would be all invocations except one covering a single sample, and then one invocation covering the rest.
	const int minInvocations	= static_cast<int>(std::ceil(static_cast<float>(m_imageMSParams.numSamples) * m_imageMSParams.shadingRate));
	const int minCount			= 1;
	const int maxCount			= m_imageMSParams.numSamples - (minInvocations - 1);

	fs << "#version 440\n"
		<< "\n"
		<< "layout(location = 0) out vec4 fs_out_color;\n"
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	const int maskBitCount = bitCount(gl_SampleMaskIn[0]);\n"
		<< "\n"
		<< "	if (maskBitCount < " << minCount << " || maskBitCount > " << maxCount << ")\n"
		<< "		fs_out_color = vec4(1.0, 0.0, 0.0, 1.0);\n"
		<< "	else\n"
		<< "		fs_out_color = vec4(0.0, 1.0, 0.0, 1.0);\n"
		<< "}\n";

	programCollection.glslSources.add("fragment_shader") << glu::FragmentSource(fs.str());
}

template<> TestInstance* MSCase<MSCaseSampleMaskBitCount>::createInstance (Context& context) const
{
	return new MSInstance<MSInstanceSampleMaskBitCount>(context, m_imageMSParams);
}

class MSInstanceSampleMaskCorrectBit;

template<> MultisampleInstanceBase::VertexDataDesc MSInstance<MSInstanceSampleMaskCorrectBit>::getVertexDataDescripton (void) const
{
	return getVertexDataDescriptonNdc();
}

template<> void MSInstance<MSInstanceSampleMaskCorrectBit>::uploadVertexData (const Allocation& vertexBufferAllocation, const VertexDataDesc& vertexDataDescripton) const
{
	uploadVertexDataNdc(vertexBufferAllocation, vertexDataDescripton);
}

template<> tcu::TestStatus MSInstance<MSInstanceSampleMaskCorrectBit>::verifyImageData	(const vk::VkImageCreateInfo&						imageMSInfo,
																						 const vk::VkImageCreateInfo&						imageRSInfo,
																						 const std::vector<tcu::ConstPixelBufferAccess>&	dataPerSample,
																						 const tcu::ConstPixelBufferAccess&					dataRS) const
{
	DE_UNREF(imageRSInfo);
	DE_UNREF(dataRS);

	if (checkForErrorMS(imageMSInfo, dataPerSample, 0))
		return tcu::TestStatus::fail("The bit corresponsing to current gl_SampleID is not set in gl_SampleMaskIn");

	return tcu::TestStatus::pass("Passed");
}

class MSCaseSampleMaskCorrectBit;

template<> void MSCase<MSCaseSampleMaskCorrectBit>::checkSupport (Context& context) const
{
	checkGraphicsPipelineLibrarySupport(context);
	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SAMPLE_RATE_SHADING);
}

template<> void MSCase<MSCaseSampleMaskCorrectBit>::init (void)
{
	m_testCtx.getLog()
		<< tcu::TestLog::Message
		<< "Verifying gl_SampleMaskIn.\n"
		<< "	Fragment shader will be invoked numSamples times.\n"
		<< "	=> In each invocation gl_SampleMaskIn should have the bit set that corresponds to gl_SampleID.\n"
		<< tcu::TestLog::EndMessage;

	MultisampleCaseBase::init();
}

template<> void MSCase<MSCaseSampleMaskCorrectBit>::initPrograms (vk::SourceCollections& programCollection) const
{
	MSCaseBaseResolveAndPerSampleFetch::initPrograms(programCollection);

	// Create vertex shader
	std::ostringstream vs;

	vs << "#version 440\n"
		<< "layout(location = 0) in vec4 vs_in_position_ndc;\n"
		<< "\n"
		<< "out gl_PerVertex {\n"
		<< "	vec4  gl_Position;\n"
		<< "};\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	gl_Position	= vs_in_position_ndc;\n"
		<< "}\n";

	programCollection.glslSources.add("vertex_shader") << glu::VertexSource(vs.str());

	// Create fragment shader
	std::ostringstream fs;

	fs << "#version 440\n"
		<< "\n"
		<< "layout(location = 0) out vec4 fs_out_color;\n"
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	if (((gl_SampleMaskIn[0] >> gl_SampleID) & 0x01) == 0x01)\n"
		<< "		fs_out_color = vec4(0.0, 1.0, 0.0, 1.0);\n"
		<< "	else\n"
		<< "		fs_out_color = vec4(1.0, 0.0, 0.0, 1.0);\n"
		<< "}\n";

	programCollection.glslSources.add("fragment_shader") << glu::FragmentSource(fs.str());
}

template<> TestInstance* MSCase<MSCaseSampleMaskCorrectBit>::createInstance (Context& context) const
{
	return new MSInstance<MSInstanceSampleMaskCorrectBit>(context, m_imageMSParams);
}

class MSInstanceSampleMaskWrite;

template<> MultisampleInstanceBase::VertexDataDesc MSInstance<MSInstanceSampleMaskWrite>::getVertexDataDescripton (void) const
{
	return getVertexDataDescriptonNdc();
}

template<> void MSInstance<MSInstanceSampleMaskWrite>::uploadVertexData (const Allocation& vertexBufferAllocation, const VertexDataDesc& vertexDataDescripton) const
{
	uploadVertexDataNdc(vertexBufferAllocation, vertexDataDescripton);
}

//! Creates VkPipelineMultisampleStateCreateInfo with sample shading disabled.
template<> VkPipelineMultisampleStateCreateInfo MSInstance<MSInstanceSampleMaskWrite>::getMSStateCreateInfo (const ImageMSParams& imageMSParams) const
{
	const VkPipelineMultisampleStateCreateInfo multisampleStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,													// const void*								pNext;
		(VkPipelineMultisampleStateCreateFlags)0u,					// VkPipelineMultisampleStateCreateFlags	flags;
		imageMSParams.numSamples,									// VkSampleCountFlagBits					rasterizationSamples;
		VK_FALSE,													// VkBool32									sampleShadingEnable;
		imageMSParams.shadingRate,									// float									minSampleShading;
		DE_NULL,													// const VkSampleMask*						pSampleMask;
		VK_FALSE,													// VkBool32									alphaToCoverageEnable;
		VK_FALSE,													// VkBool32									alphaToOneEnable;
	};

	return multisampleStateInfo;
}

template<> tcu::TestStatus MSInstance<MSInstanceSampleMaskWrite>::verifyImageData	(const vk::VkImageCreateInfo&						imageMSInfo,
																					 const vk::VkImageCreateInfo&						imageRSInfo,
																					 const std::vector<tcu::ConstPixelBufferAccess>&	dataPerSample,
																					 const tcu::ConstPixelBufferAccess&					dataRS) const
{
	const deUint32 numSamples = static_cast<deUint32>(imageMSInfo.samples);

	for (deUint32 z = 0u; z < imageMSInfo.extent.depth;  ++z)
	for (deUint32 y = 0u; y < imageMSInfo.extent.height; ++y)
	for (deUint32 x = 0u; x < imageMSInfo.extent.width;  ++x)
	{
		for (deUint32 sampleNdx = 0u; sampleNdx < numSamples; ++sampleNdx)
		{
			const deUint32 firstComponent = dataPerSample[sampleNdx].getPixelUint(x, y, z)[0];

			if (firstComponent != 0u && firstComponent != 255u)
				return tcu::TestStatus::fail("Expected color to be zero or saturated on the first channel");
		}
	}

	for (deUint32 z = 0u; z < imageRSInfo.extent.depth;  ++z)
	for (deUint32 y = 0u; y < imageRSInfo.extent.height; ++y)
	for (deUint32 x = 0u; x < imageRSInfo.extent.width;  ++x)
	{
		const float firstComponent = dataRS.getPixel(x, y, z)[0];

		if (deFloatAbs(firstComponent - 0.5f) > 0.02f)
			return tcu::TestStatus::fail("Expected resolve color to be half intensity on the first channel");
	}

	return tcu::TestStatus::pass("Passed");
}

class MSCaseSampleMaskWrite;

template<> void MSCase<MSCaseSampleMaskWrite>::init (void)
{
	m_testCtx.getLog()
		<< tcu::TestLog::Message
		<< "Discarding half of the samples using gl_SampleMask."
		<< "Expecting half intensity on multisample targets (numSamples > 1)\n"
		<< tcu::TestLog::EndMessage;

	MultisampleCaseBase::init();
}

template<> void MSCase<MSCaseSampleMaskWrite>::initPrograms (vk::SourceCollections& programCollection) const
{
	MSCaseBaseResolveAndPerSampleFetch::initPrograms(programCollection);

	// Create vertex shader
	std::ostringstream vs;

	vs << "#version 440\n"
		<< "layout(location = 0) in vec4 vs_in_position_ndc;\n"
		<< "\n"
		<< "out gl_PerVertex {\n"
		<< "	vec4  gl_Position;\n"
		<< "};\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	gl_Position	= vs_in_position_ndc;\n"
		<< "}\n";

	programCollection.glslSources.add("vertex_shader") << glu::VertexSource(vs.str());

	// Create fragment shader
	std::ostringstream fs;

	fs << "#version 440\n"
		<< "\n"
		<< "layout(location = 0) out vec4 fs_out_color;\n"
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	gl_SampleMask[0] = 0xAAAAAAAA;\n"
		<< "\n"
		<< "	fs_out_color = vec4(1.0, 0.0, 0.0, 1.0);\n"
		<< "}\n";

	programCollection.glslSources.add("fragment_shader") << glu::FragmentSource(fs.str());
}

template<> TestInstance* MSCase<MSCaseSampleMaskWrite>::createInstance (Context& context) const
{
	return new MSInstance<MSInstanceSampleMaskWrite>(context, m_imageMSParams);
}

const set<deUint32> kValidSquareSampleCounts =
{
	vk::VK_SAMPLE_COUNT_1_BIT,
	vk::VK_SAMPLE_COUNT_2_BIT,
	vk::VK_SAMPLE_COUNT_4_BIT,
	vk::VK_SAMPLE_COUNT_8_BIT,
	vk::VK_SAMPLE_COUNT_16_BIT,
};

void assertSquareSampleCount (deUint32 sampleCount)
{
	DE_ASSERT(kValidSquareSampleCounts.find(sampleCount) != kValidSquareSampleCounts.end());
	DE_UNREF(sampleCount); // for release builds.
}

// When dealing with N samples, each coordinate (x, y) will be used to decide which samples will be written to, using N/2 bits for
// each of the X and Y values. Take into account this returns 0 for 1 sample.
deUint32 bitsPerCoord (deUint32 numSamples)
{
	assertSquareSampleCount(numSamples);
	return (numSamples / 2u);
}

// These tests will try to verify all write or mask bit combinations for the given sample count, and will verify one combination per
// image pixel. This means the following image sizes need to be used:
//		- 2 samples: 2x2
//		- 4 samples: 4x4
//		- 8 samples: 16x16
//		- 16 samples: 256x256
// In other words, images will be square with 2^(samples-1) pixels on each side.
vk::VkExtent2D imageSize (deUint32 sampleCount)
{
	assertSquareSampleCount(sampleCount);

	// Special case: 2x1 image (not actually square).
	if (sampleCount == vk::VK_SAMPLE_COUNT_1_BIT)
		return vk::VkExtent2D{2u, 1u};

	// Other cases: square image as described above.
	const auto dim = (1u<<(sampleCount>>1u));
	return vk::VkExtent2D{dim, dim};
}

vk::VkExtent3D getExtent3D (deUint32 sampleCount)
{
	const auto size = imageSize(sampleCount);
	return vk::VkExtent3D{size.width, size.height, 1u};
}

std::string getShaderDecl (const tcu::Vec4& color)
{
	std::ostringstream declaration;
	declaration << "vec4(" << color.x() << ", " << color.y() << ", " << color.z() << ", " << color.w() << ")";
	return declaration.str();
}

struct WriteSampleParams
{
	vk::PipelineConstructionType	pipelineConstructionType;
	vk::VkSampleCountFlagBits		sampleCount;
};

class WriteSampleTest : public vkt::TestCase
{
public:
									WriteSampleTest		(tcu::TestContext& testCtx, const std::string& name, const std::string& desc, const WriteSampleParams& params)
										: vkt::TestCase(testCtx, name, desc), m_params(params)
										{}
	virtual							~WriteSampleTest	(void) {}

	virtual void					initPrograms		(vk::SourceCollections& programCollection) const;
	virtual vkt::TestInstance*		createInstance		(Context& context) const;
	virtual void					checkSupport		(Context& context) const;

	static const tcu::Vec4			kClearColor;
	static const tcu::Vec4			kBadColor;
	static const tcu::Vec4			kGoodColor;
	static const tcu::Vec4			kWriteColor;

	static constexpr vk::VkFormat	kImageFormat		= vk::VK_FORMAT_R8G8B8A8_UNORM;

	// Keep these two in sync.
	static constexpr vk::VkImageUsageFlags		kUsageFlags		= (vk::VK_IMAGE_USAGE_STORAGE_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT | vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	static constexpr vk::VkFormatFeatureFlags	kFeatureFlags	= (vk::VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT | vk::VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | vk::VK_FORMAT_FEATURE_TRANSFER_DST_BIT);

private:
	WriteSampleParams		m_params;
};

const tcu::Vec4 WriteSampleTest::kClearColor	{0.0f, 0.0f, 0.0f, 1.0f};
const tcu::Vec4 WriteSampleTest::kBadColor		{1.0f, 0.0f, 0.0f, 1.0f};
const tcu::Vec4 WriteSampleTest::kGoodColor		{0.0f, 1.0f, 0.0f, 1.0f};
const tcu::Vec4 WriteSampleTest::kWriteColor	{0.0f, 0.0f, 1.0f, 1.0f};

class WriteSampleTestInstance : public vkt::TestInstance
{
public:
								WriteSampleTestInstance		(vkt::Context& context, const WriteSampleParams& params)
									: vkt::TestInstance(context), m_params(params)
									{}

	virtual						~WriteSampleTestInstance	(void) {}

	virtual tcu::TestStatus		iterate						(void);

private:
	WriteSampleParams			m_params;
};

void WriteSampleTest::checkSupport (Context& context) const
{
	const auto&	vki				= context.getInstanceInterface();
	const auto	physicalDevice	= context.getPhysicalDevice();

	// Check multisample storage images support.
	const auto features = vk::getPhysicalDeviceFeatures(vki, physicalDevice);
	if (!features.shaderStorageImageMultisample)
		TCU_THROW(NotSupportedError, "Using multisample images as storage is not supported");

	// Check the specific image format.
	const auto properties = vk::getPhysicalDeviceFormatProperties(vki, physicalDevice, kImageFormat);
	if (!(properties.optimalTilingFeatures & kFeatureFlags))
		TCU_THROW(NotSupportedError, "Format does not support the required features");

	// Check the supported sample count.
	const auto imgProps = vk::getPhysicalDeviceImageFormatProperties(vki, physicalDevice, kImageFormat, vk::VK_IMAGE_TYPE_2D, vk::VK_IMAGE_TILING_OPTIMAL, kUsageFlags, 0u);
	if (!(imgProps.sampleCounts & m_params.sampleCount))
		TCU_THROW(NotSupportedError, "Format does not support the required sample count");

	checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), m_params.pipelineConstructionType);
}

void WriteSampleTest::initPrograms (vk::SourceCollections& programCollection) const
{
	std::ostringstream writeColorDecl, goodColorDecl, badColorDecl, clearColorDecl, allColorDecl;

	writeColorDecl	<< "        vec4  wcolor   = " << getShaderDecl(kWriteColor)	<< ";\n";
	goodColorDecl	<< "        vec4  bcolor   = " << getShaderDecl(kBadColor)		<< ";\n";
	badColorDecl	<< "        vec4  gcolor   = " << getShaderDecl(kGoodColor)		<< ";\n";
	clearColorDecl	<< "        vec4  ccolor   = " << getShaderDecl(kClearColor)	<< ";\n";
	allColorDecl	<< writeColorDecl.str() << goodColorDecl.str() << badColorDecl.str() << clearColorDecl.str();

	std::ostringstream shaderWrite;

	const auto bpc		= de::toString(bitsPerCoord(m_params.sampleCount));
	const auto count	= de::toString(m_params.sampleCount);

	shaderWrite
		<< "#version 450\n"
		<< "\n"
		<< "layout (rgba8, set=0, binding=0) uniform image2DMS writeImg;\n"
		<< "layout (rgba8, set=0, binding=1) uniform image2D   verificationImg;\n"
		<< "\n"
		<< "void main()\n"
		<< "{\n"
		<< writeColorDecl.str()
		<< "        uvec2 ucoords  = uvec2(gl_GlobalInvocationID.xy);\n"
		<< "        ivec2 icoords  = ivec2(ucoords);\n"
		<< "        uint writeMask = ((ucoords.x << " << bpc << ") | ucoords.y);\n"
		<< "        for (uint i = 0; i < " << count << "; ++i)\n"
		<< "        {\n"
		<< "                if ((writeMask & (1 << i)) != 0)\n"
		<< "                        imageStore(writeImg, icoords, int(i), wcolor);\n"
		<< "        }\n"
		<< "}\n"
		;

	std::ostringstream shaderVerify;

	shaderVerify
		<< "#version 450\n"
		<< "\n"
		<< "layout (rgba8, set=0, binding=0) uniform image2DMS writeImg;\n"
		<< "layout (rgba8, set=0, binding=1) uniform image2D   verificationImg;\n"
		<< "\n"
		<< "void main()\n"
		<< "{\n"
		<< allColorDecl.str()
		<< "        uvec2 ucoords  = uvec2(gl_GlobalInvocationID.xy);\n"
		<< "        ivec2 icoords  = ivec2(ucoords);\n"
		<< "        uint writeMask = ((ucoords.x << " << bpc << ") | ucoords.y);\n"
		<< "        bool ok = true;\n"
		<< "        for (uint i = 0; i < " << count << "; ++i)\n"
		<< "        {\n"
		<< "                bool expectWrite = ((writeMask & (1 << i)) != 0);\n"
		<< "                vec4 sampleColor = imageLoad(writeImg, icoords, int(i));\n"
		<< "                vec4 wantedColor = (expectWrite ? wcolor : ccolor);\n"
		<< "                ok = ok && (sampleColor == wantedColor);\n"
		<< "        }\n"
		<< "        vec4 resultColor = (ok ? gcolor : bcolor);\n"
		<< "        imageStore(verificationImg, icoords, resultColor);\n"
		<< "}\n"
		;

	programCollection.glslSources.add("write")	<< glu::ComputeSource(shaderWrite.str());
	programCollection.glslSources.add("verify")	<< glu::ComputeSource(shaderVerify.str());
}

vkt::TestInstance* WriteSampleTest::createInstance (Context& context) const
{
	return new WriteSampleTestInstance{context, m_params};
}

tcu::TestStatus WriteSampleTestInstance::iterate (void)
{
	const auto&	vkd			= m_context.getDeviceInterface();
	const auto	device		= m_context.getDevice();
	auto&		allocator	= m_context.getDefaultAllocator();
	const auto	queue		= m_context.getUniversalQueue();
	const auto	queueIndex	= m_context.getUniversalQueueFamilyIndex();
	const auto	extent3D	= getExtent3D(m_params.sampleCount);

	// Create storage image and verification image.
	const vk::VkImageCreateInfo storageImageInfo =
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType;
		nullptr,									// const void*				pNext;
		0u,											// VkImageCreateFlags		flags;
		vk::VK_IMAGE_TYPE_2D,						// VkImageType				imageType;
		WriteSampleTest::kImageFormat,				// VkFormat					format;
		extent3D,									// VkExtent3D				extent;
		1u,											// deUint32					mipLevels;
		1u,											// deUint32					arrayLayers;
		m_params.sampleCount,						// VkSampleCountFlagBits	samples;
		vk::VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling			tiling;
		WriteSampleTest::kUsageFlags,				// VkImageUsageFlags		usage;
		vk::VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode;
		1u,											// deUint32					queueFamilyIndexCount;
		&queueIndex,								// const deUint32*			pQueueFamilyIndices;
		vk::VK_IMAGE_LAYOUT_UNDEFINED,				// VkImageLayout			initialLayout;
	};

	const vk::VkImageCreateInfo verificationImageInfo =
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType;
		nullptr,									// const void*				pNext;
		0u,											// VkImageCreateFlags		flags;
		vk::VK_IMAGE_TYPE_2D,						// VkImageType				imageType;
		WriteSampleTest::kImageFormat,				// VkFormat					format;
		extent3D,									// VkExtent3D				extent;
		1u,											// deUint32					mipLevels;
		1u,											// deUint32					arrayLayers;
		vk::VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits	samples;
		vk::VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling			tiling;
		WriteSampleTest::kUsageFlags,				// VkImageUsageFlags		usage;
		vk::VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode;
		1u,											// deUint32					queueFamilyIndexCount;
		&queueIndex,								// const deUint32*			pQueueFamilyIndices;
		vk::VK_IMAGE_LAYOUT_UNDEFINED,				// VkImageLayout			initialLayout;
	};

	vk::ImageWithMemory storageImgPrt		{vkd, device, allocator, storageImageInfo, vk::MemoryRequirement::Any};
	vk::ImageWithMemory verificationImgPtr	{vkd, device, allocator, verificationImageInfo, vk::MemoryRequirement::Any};

	const vk::VkImageSubresourceRange kSubresourceRange =
	{
		vk::VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
		0u,								// deUint32				baseMipLevel;
		1u,								// deUint32				levelCount;
		0u,								// deUint32				baseArrayLayer;
		1u,								// deUint32				layerCount;
	};

	auto storageImgViewPtr		= vk::makeImageView(vkd, device, storageImgPrt.get(), vk::VK_IMAGE_VIEW_TYPE_2D, WriteSampleTest::kImageFormat, kSubresourceRange);
	auto verificationImgViewPtr	= vk::makeImageView(vkd, device, verificationImgPtr.get(), vk::VK_IMAGE_VIEW_TYPE_2D, WriteSampleTest::kImageFormat, kSubresourceRange);

	// Prepare a staging buffer to check verification image.
	const auto				tcuFormat			= vk::mapVkFormat(WriteSampleTest::kImageFormat);
	const VkDeviceSize		bufferSize			= extent3D.width * extent3D.height * extent3D.depth * tcu::getPixelSize(tcuFormat);
	const auto				stagingBufferInfo	= vk::makeBufferCreateInfo(bufferSize, vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	vk::BufferWithMemory	stagingBuffer		{vkd, device, allocator, stagingBufferInfo, MemoryRequirement::HostVisible};

	// Descriptor set layout.
	vk::DescriptorSetLayoutBuilder layoutBuilder;
	layoutBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, vk::VK_SHADER_STAGE_COMPUTE_BIT);
	layoutBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, vk::VK_SHADER_STAGE_COMPUTE_BIT);
	auto descriptorSetLayout = layoutBuilder.build(vkd, device);

	// Descriptor pool.
	vk::DescriptorPoolBuilder poolBuilder;
	poolBuilder.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2u);
	auto descriptorPool = poolBuilder.build(vkd, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

	// Descriptor set.
	const auto descriptorSet = vk::makeDescriptorSet(vkd, device, descriptorPool.get(), descriptorSetLayout.get());

	// Update descriptor set using the images.
	const auto storageImgDescriptorInfo			= vk::makeDescriptorImageInfo(DE_NULL, storageImgViewPtr.get(), vk::VK_IMAGE_LAYOUT_GENERAL);
	const auto verificationImgDescriptorInfo	= vk::makeDescriptorImageInfo(DE_NULL, verificationImgViewPtr.get(), vk::VK_IMAGE_LAYOUT_GENERAL);

	vk::DescriptorSetUpdateBuilder updateBuilder;
	updateBuilder.writeSingle(descriptorSet.get(), vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &storageImgDescriptorInfo);
	updateBuilder.writeSingle(descriptorSet.get(), vk::DescriptorSetUpdateBuilder::Location::binding(1u), vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &verificationImgDescriptorInfo);
	updateBuilder.update(vkd, device);

	// Create write and verification compute pipelines.
	auto shaderWriteModule	= ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("write"), 0u);
	auto shaderVerifyModule	= ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("verify"), 0u);
	auto pipelineLayout		= vk::makePipelineLayout(vkd, device, descriptorSetLayout.get());

	const vk::VkComputePipelineCreateInfo writePipelineCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		nullptr,
		0u,															// flags
		{															// compute shader
			vk::VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType						sType;
			nullptr,													// const void*							pNext;
			0u,															// VkPipelineShaderStageCreateFlags		flags;
			vk::VK_SHADER_STAGE_COMPUTE_BIT,							// VkShaderStageFlagBits				stage;
			shaderWriteModule.getModule(),								// VkShaderModule						module;
			"main",														// const char*							pName;
			nullptr,													// const VkSpecializationInfo*			pSpecializationInfo;
		},
		pipelineLayout.get(),										// layout
		DE_NULL,													// basePipelineHandle
		0,															// basePipelineIndex
	};

	auto verificationPipelineCreateInfo = writePipelineCreateInfo;
	verificationPipelineCreateInfo.stage.module = shaderVerifyModule.getModule();

	auto writePipeline			= vk::createComputePipeline(vkd, device, DE_NULL, &writePipelineCreateInfo);
	auto verificationPipeline	= vk::createComputePipeline(vkd, device, DE_NULL, &verificationPipelineCreateInfo);

	// Transition images to the correct layout and buffers at different stages.
	auto storageImgPreClearBarrier			= vk::makeImageMemoryBarrier(0, vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_IMAGE_LAYOUT_UNDEFINED, vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, storageImgPrt.get(), kSubresourceRange);
	auto storageImgPreShaderBarrier			= vk::makeImageMemoryBarrier(vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_ACCESS_SHADER_WRITE_BIT, vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, vk::VK_IMAGE_LAYOUT_GENERAL, storageImgPrt.get(), kSubresourceRange);
	auto verificationImgPreShaderBarrier	= vk::makeImageMemoryBarrier(0, vk::VK_ACCESS_SHADER_WRITE_BIT, vk::VK_IMAGE_LAYOUT_UNDEFINED, vk::VK_IMAGE_LAYOUT_GENERAL, verificationImgPtr.get(), kSubresourceRange);
	auto storageImgPreVerificationBarrier	= vk::makeImageMemoryBarrier(vk::VK_ACCESS_SHADER_WRITE_BIT, vk::VK_ACCESS_SHADER_READ_BIT, vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_IMAGE_LAYOUT_GENERAL, storageImgPrt.get(), kSubresourceRange);
	auto verificationImgPostBarrier			= vk::makeImageMemoryBarrier(vk::VK_ACCESS_SHADER_WRITE_BIT, vk::VK_ACCESS_TRANSFER_READ_BIT, vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, verificationImgPtr.get(), kSubresourceRange);
	auto bufferBarrier						= vk::makeBufferMemoryBarrier(vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_ACCESS_HOST_READ_BIT, stagingBuffer.get(), 0ull, bufferSize);

	// Command buffer.
	auto cmdPool		= vk::makeCommandPool(vkd, device, queueIndex);
	auto cmdBufferPtr	= vk::allocateCommandBuffer(vkd, device, cmdPool.get(), vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	auto cmdBuffer		= cmdBufferPtr.get();

	// Clear color for the storage image.
	const auto clearColor = vk::makeClearValueColor(WriteSampleTest::kClearColor);

	const vk::VkBufferImageCopy	copyRegion =
	{
		0ull,									// VkDeviceSize				bufferOffset;
		extent3D.width,							// deUint32					bufferRowLength;
		extent3D.height,						// deUint32					bufferImageHeight;
		{										// VkImageSubresourceLayers	imageSubresource;
			vk::VK_IMAGE_ASPECT_COLOR_BIT,			// VkImageAspectFlags	aspectMask;
			0u,										// deUint32				mipLevel;
			0u,										// deUint32				baseArrayLayer;
			1u,										// deUint32				layerCount;
		},
		{ 0, 0, 0 },							// VkOffset3D				imageOffset;
		extent3D,								// VkExtent3D				imageExtent;
	};

	// Record and submit commands.
	vk::beginCommandBuffer(vkd, cmdBuffer);
		// Clear storage image.
		vkd.cmdPipelineBarrier(cmdBuffer, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u, &storageImgPreClearBarrier);
		vkd.cmdClearColorImage(cmdBuffer, storageImgPrt.get(), vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor.color, 1u, &kSubresourceRange);

		// Bind write pipeline and descriptor set.
		vkd.cmdBindPipeline(cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, writePipeline.get());
		vkd.cmdBindDescriptorSets(cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout.get(), 0, 1u, &descriptorSet.get(), 0u, nullptr);

		// Transition images to the appropriate layout before running the shader.
		vkd.cmdPipelineBarrier(cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u, &storageImgPreShaderBarrier);
		vkd.cmdPipelineBarrier(cmdBuffer, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u, &verificationImgPreShaderBarrier);

		// Run shader.
		vkd.cmdDispatch(cmdBuffer, extent3D.width, extent3D.height, extent3D.depth);

		// Bind verification pipeline.
		vkd.cmdBindPipeline(cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, verificationPipeline.get());

		// Make sure writes happen before reads in the second dispatch for the storage image.
		vkd.cmdPipelineBarrier(cmdBuffer, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u, &storageImgPreVerificationBarrier);

		// Run verification shader.
		vkd.cmdDispatch(cmdBuffer, extent3D.width, extent3D.height, extent3D.depth);

		// Change verification image layout to prepare the transfer.
		vkd.cmdPipelineBarrier(cmdBuffer, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u, &verificationImgPostBarrier);

		// Copy verification image to staging buffer.
		vkd.cmdCopyImageToBuffer(cmdBuffer, verificationImgPtr.get(), vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer.get(), 1u, &copyRegion);
		vkd.cmdPipelineBarrier(cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, 0, 0u, nullptr, 1u, &bufferBarrier, 0u, nullptr);

	vk::endCommandBuffer(vkd, cmdBuffer);

	// Run shaders.
	vk::submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Read buffer pixels.
	const auto& bufferAlloc = stagingBuffer.getAllocation();
	vk::invalidateAlloc(vkd, device, bufferAlloc);

	// Copy buffer data to texture level and verify all pixels have the proper color.
	tcu::TextureLevel texture {tcuFormat, static_cast<int>(extent3D.width), static_cast<int>(extent3D.height), static_cast<int>(extent3D.depth)};
	const auto access = texture.getAccess();
	deMemcpy(access.getDataPtr(), reinterpret_cast<char*>(bufferAlloc.getHostPtr()) + bufferAlloc.getOffset(), static_cast<size_t>(bufferSize));

	for (int i = 0; i < access.getWidth(); ++i)
	for (int j = 0; j < access.getHeight(); ++j)
	for (int k = 0; k < access.getDepth(); ++k)
	{
		if (access.getPixel(i, j, k) != WriteSampleTest::kGoodColor)
		{
			std::ostringstream msg;
			msg << "Invalid result at pixel (" << i << ", " << j << ", " << k << "); check error mask for more details";
			m_context.getTestContext().getLog() << tcu::TestLog::Image("ErrorMask", "Indicates which pixels have unexpected values", access);
			return tcu::TestStatus::fail(msg.str());
		}
	}

	return tcu::TestStatus::pass("Pass");
}

using WriteSampleMaskParams = WriteSampleParams;

class WriteSampleMaskTestCase : public vkt::TestCase
{
public:
							WriteSampleMaskTestCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const WriteSampleMaskParams& params);
	virtual					~WriteSampleMaskTestCase	(void) {}

	virtual void			checkSupport				(Context& context) const;
	virtual void			initPrograms				(vk::SourceCollections& programCollection) const;
	virtual TestInstance*	createInstance				(Context& context) const;
	static deUint32			getBufferElems				(deUint32 sampleCount);

	static const tcu::Vec4						kClearColor;
	static const tcu::Vec4						kWriteColor;

	static constexpr vk::VkFormat				kImageFormat	= vk::VK_FORMAT_R8G8B8A8_UNORM;
	static constexpr vk::VkImageUsageFlags		kUsageFlags		= (vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);
	static constexpr vk::VkFormatFeatureFlags	kFeatureFlags	= (vk::VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT);

private:
	WriteSampleMaskParams	m_params;
};

const tcu::Vec4 WriteSampleMaskTestCase::kClearColor	{0.0f, 0.0f, 0.0f, 1.0f};
const tcu::Vec4 WriteSampleMaskTestCase::kWriteColor	{0.0f, 0.0f, 1.0f, 1.0f};

class WriteSampleMaskTestInstance : public vkt::TestInstance
{
public:
								WriteSampleMaskTestInstance		(Context& context, const WriteSampleMaskParams& params);
	virtual						~WriteSampleMaskTestInstance	(void) {}

	virtual tcu::TestStatus		iterate							(void);

private:
	WriteSampleMaskParams		m_params;
};

WriteSampleMaskTestCase::WriteSampleMaskTestCase (tcu::TestContext& testCtx, const std::string& name, const std::string& description, const WriteSampleMaskParams& params)
	: vkt::TestCase	(testCtx, name, description)
	, m_params		(params)
{}

void WriteSampleMaskTestCase::checkSupport (Context& context) const
{
	const auto&	vki				= context.getInstanceInterface();
	const auto	physicalDevice	= context.getPhysicalDevice();

	// Check if sampleRateShading is supported.
	if(!vk::getPhysicalDeviceFeatures(vki, physicalDevice).sampleRateShading)
		TCU_THROW(NotSupportedError, "Sample rate shading is not supported");

	// Check the specific image format.
	const auto properties = vk::getPhysicalDeviceFormatProperties(vki, physicalDevice, kImageFormat);
	if (!(properties.optimalTilingFeatures & kFeatureFlags))
		TCU_THROW(NotSupportedError, "Format does not support the required features");

	// Check the supported sample count.
	const auto imgProps = vk::getPhysicalDeviceImageFormatProperties(vki, physicalDevice, kImageFormat, vk::VK_IMAGE_TYPE_2D, vk::VK_IMAGE_TILING_OPTIMAL, kUsageFlags, 0u);
	if (!(imgProps.sampleCounts & m_params.sampleCount))
		TCU_THROW(NotSupportedError, "Format does not support the required sample count");

	checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), m_params.pipelineConstructionType);
}

void WriteSampleMaskTestCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const auto bpc			= de::toString(bitsPerCoord(m_params.sampleCount));
	const auto size			= imageSize(m_params.sampleCount);
	const auto bufferElems	= getBufferElems(m_params.sampleCount);

	// Passthrough vertex shader.
	std::ostringstream vertShader;

	vertShader
		<< "#version 450\n"
		<< "layout (location=0) in vec2 inPos;\n"
		<< "void main()\n"
		<< "{\n"
		<< "    gl_Position = vec4(inPos, 0.0, 1.0);\n"
		<< "}\n"
		;

	// Fragment shader common header.
	std::ostringstream fragHeader;

	fragHeader
		<< "#version 450\n"
		<< "\n"
		// The color attachment is useless for the second subpass but avoids having to use an empty subpass and verifying the sample
		// count is valid for it.
		<< "layout (location=0) out vec4 outColor;\n"
		<< "\n"
		<< "vec4 wcolor = " << getShaderDecl(kWriteColor) << ";\n"
		<< "vec4 ccolor = " << getShaderDecl(kClearColor) << ";\n"
		<< "\n"
		;

	const auto fragHeaderStr = fragHeader.str();

	// Fragment shader setting the sample mask and writing to the output color attachment. The sample mask will guarantee each image
	// pixel gets a different combination of sample bits set, allowing the fragment shader to write in that sample or not, from all
	// zeros in pixel (0, 0) to all ones in the opposite corner.
	std::ostringstream fragShaderWrite;

	fragShaderWrite
		<< fragHeaderStr
		<< "void main()\n"
		<< "{\n"
		<< "    uvec2 ucoords    = uvec2(gl_FragCoord);\n"
		<< "    ivec2 icoords    = ivec2(ucoords);\n"
		<< "    gl_SampleMask[0] = int((ucoords.x << " << bpc << ") | ucoords.y);\n"
		<< "    outColor         = wcolor;\n"
		<< "}\n"
		;

	// Fragment shader reading from the previous output color attachment and copying the state to an SSBO for verification.
	std::ostringstream fragShaderCheck;

	const bool isMultiSample = (m_params.sampleCount != vk::VK_SAMPLE_COUNT_1_BIT);
	fragShaderCheck
		<< fragHeaderStr
		<< "layout(set=0, binding=0, input_attachment_index=0) uniform subpassInput" << (isMultiSample ? "MS" : "") << " inputAttachment;\n"
		<< "layout(set=0, binding=1, std430) buffer StorageBuffer {\n"
		<< "    int writeFlags[" << bufferElems << "];\n"
		<< "} sb;\n"
		<< "\n"
		<< "void main()\n"
		<< "{\n"
		<< "    uvec2 ucoords          = uvec2(gl_FragCoord);\n"
		<< "    ivec2 icoords          = ivec2(ucoords);\n"
		<< "    uint  bufferp          = ((ucoords.y * " << size.width << " + ucoords.x) * " << m_params.sampleCount << ") + uint(gl_SampleID);\n"
		<< "    vec4  storedc          = subpassLoad(inputAttachment" << (isMultiSample ? ", gl_SampleID" : "") << ");\n"
		<< "    sb.writeFlags[bufferp] = ((storedc == wcolor) ? 1 : ((storedc == ccolor) ? 0 : 2));\n"
		<< "    outColor               = storedc;\n"
		<< "}\n"
		;

	programCollection.glslSources.add("vert")		<< glu::VertexSource(vertShader.str());
	programCollection.glslSources.add("frag_write")	<< glu::FragmentSource(fragShaderWrite.str());
	programCollection.glslSources.add("frag_check")	<< glu::FragmentSource(fragShaderCheck.str());
}

TestInstance* WriteSampleMaskTestCase::createInstance (Context& context) const
{
	return new WriteSampleMaskTestInstance(context, m_params);
}

deUint32 WriteSampleMaskTestCase::getBufferElems (deUint32 sampleCount)
{
	const auto imgSize = imageSize(sampleCount);
	return (imgSize.width * imgSize.height * sampleCount);
}

WriteSampleMaskTestInstance::WriteSampleMaskTestInstance (Context& context, const WriteSampleMaskParams& params)
	: vkt::TestInstance	(context)
	, m_params			(params)
{}

tcu::TestStatus WriteSampleMaskTestInstance::iterate (void)
{
	const auto&		vki					= m_context.getInstanceInterface();
	const auto&		vkd					= m_context.getDeviceInterface();
	const auto		physicalDevice		= m_context.getPhysicalDevice();
	const auto		device				= m_context.getDevice();
	auto&			alloc				= m_context.getDefaultAllocator();
	const auto		queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	const auto		queue				= m_context.getUniversalQueue();

	static constexpr auto	kImageFormat	= WriteSampleMaskTestCase::kImageFormat;
	static constexpr auto	kImageUsage		= WriteSampleMaskTestCase::kUsageFlags;
	const auto				kImageExtent	= getExtent3D(m_params.sampleCount);
	const auto				kBufferElems	= WriteSampleMaskTestCase::getBufferElems(m_params.sampleCount);
	const auto				kBufferSize		= static_cast<vk::VkDeviceSize>(kBufferElems * sizeof(deInt32));

	// Create image.
	const vk::VkImageCreateInfo imageCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
		nullptr,									//	const void*				pNext;
		0u,											//	VkImageCreateFlags		flags;
		vk::VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
		kImageFormat,								//	VkFormat				format;
		kImageExtent,								//	VkExtent3D				extent;
		1u,											//	deUint32				mipLevels;
		1u,											//	deUint32				arrayLayers;
		m_params.sampleCount,						//	VkSampleCountFlagBits	samples;
		vk::VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
		kImageUsage,								//	VkImageUsageFlags		usage;
		vk::VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
		0u,											//	deUint32				queueFamilyIndexCount;
		nullptr,									//	const deUint32*			pQueueFamilyIndices;
		vk::VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
	};

	const vk::ImageWithMemory colorImage	(vkd, device, alloc, imageCreateInfo, vk::MemoryRequirement::Any);
	const vk::ImageWithMemory auxiliarImage	(vkd, device, alloc, imageCreateInfo, vk::MemoryRequirement::Any);	// For the second subpass.

	// Image views.
	const auto subresourceRange		= vk::makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const auto colorImageView		= vk::makeImageView(vkd, device, colorImage.get(), vk::VK_IMAGE_VIEW_TYPE_2D, kImageFormat, subresourceRange);
	const auto auxiliarImageView	= vk::makeImageView(vkd, device, auxiliarImage.get(), vk::VK_IMAGE_VIEW_TYPE_2D, kImageFormat, subresourceRange);

	// Create storage buffer used to verify results.
	const vk::BufferWithMemory storageBuffer(vkd, device, alloc, vk::makeBufferCreateInfo(kBufferSize, vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), vk::MemoryRequirement::HostVisible);

	// Full-screen quad.
	const std::vector<tcu::Vec2> quadVertices =
	{
		tcu::Vec2(-1.0f,  1.0f),	// Lower left
		tcu::Vec2( 1.0f,  1.0f),	// Lower right
		tcu::Vec2( 1.0f, -1.0f),	// Top right.
		tcu::Vec2(-1.0f,  1.0f),	// Lower left
		tcu::Vec2( 1.0f, -1.0f),	// Top right.
		tcu::Vec2(-1.0f, -1.0f),	// Top left.
	};

	// Vertex buffer.
	const auto					vertexBufferSize	= static_cast<vk::VkDeviceSize>(quadVertices.size() * sizeof(decltype(quadVertices)::value_type));
	const vk::BufferWithMemory	vertexBuffer		(vkd, device, alloc, vk::makeBufferCreateInfo(vertexBufferSize, vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), vk::MemoryRequirement::HostVisible);
	const auto&					vertexBufferAlloc	= vertexBuffer.getAllocation();
	void*						vertexBufferPtr		= vertexBufferAlloc.getHostPtr();
	const vk::VkDeviceSize		vertexBufferOffset	= 0;
	deMemcpy(vertexBufferPtr, quadVertices.data(), static_cast<size_t>(vertexBufferSize));
	vk::flushAlloc(vkd, device, vertexBufferAlloc);

	// Descriptor set layout.
	vk::DescriptorSetLayoutBuilder setLayoutBuilder;
	setLayoutBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, vk::VK_SHADER_STAGE_FRAGMENT_BIT);
	setLayoutBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_FRAGMENT_BIT);
	const auto descriptorSetLayout = setLayoutBuilder.build(vkd, device);

	// Descriptor pool and set.
	vk::DescriptorPoolBuilder poolBuilder;
	poolBuilder.addType(vk::VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1u);
	poolBuilder.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u);
	const auto descriptorPool	= poolBuilder.build(vkd, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	const auto descriptorSet	= vk::makeDescriptorSet(vkd, device, descriptorPool.get(), descriptorSetLayout.get());

	// Render pass.
	const std::vector<vk::VkAttachmentDescription> attachments =
	{
		// Main color attachment.
		{
			0u,												//	VkAttachmentDescriptionFlags	flags;
			kImageFormat,									//	VkFormat						format;
			m_params.sampleCount,							//	VkSampleCountFlagBits			samples;
			vk::VK_ATTACHMENT_LOAD_OP_CLEAR,				//	VkAttachmentLoadOp				loadOp;
			vk::VK_ATTACHMENT_STORE_OP_STORE,				//	VkAttachmentStoreOp				storeOp;
			vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,			//	VkAttachmentLoadOp				stencilLoadOp;
			vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,			//	VkAttachmentStoreOp				stencilStoreOp;
			vk::VK_IMAGE_LAYOUT_UNDEFINED,					//	VkImageLayout					initialLayout;
			vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,	//	VkImageLayout					finalLayout;
		},
		// Auxiliar color attachment for the check pass.
		{
			0u,												//	VkAttachmentDescriptionFlags	flags;
			kImageFormat,									//	VkFormat						format;
			m_params.sampleCount,							//	VkSampleCountFlagBits			samples;
			vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,			//	VkAttachmentLoadOp				loadOp;
			vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,			//	VkAttachmentStoreOp				storeOp;
			vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,			//	VkAttachmentLoadOp				stencilLoadOp;
			vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,			//	VkAttachmentStoreOp				stencilStoreOp;
			vk::VK_IMAGE_LAYOUT_UNDEFINED,					//	VkImageLayout					initialLayout;
			vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	//	VkImageLayout					finalLayout;
		},
	};

	const vk::VkAttachmentReference colorAttachmentReference =
	{
		0u,												//	deUint32		attachment;
		vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	//	VkImageLayout	layout;
	};

	const vk::VkAttachmentReference colorAsInputAttachment =
	{
		0u,												//	deUint32		attachment;
		vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,	//	VkImageLayout	layout;
	};

	const vk::VkAttachmentReference auxiliarAttachmentReference =
	{
		1u,												//	deUint32		attachment;
		vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	//	VkImageLayout	layout;
	};

	const std::vector<vk::VkSubpassDescription> subpasses =
	{
		// First subpass writing to the main attachment.
		{
			0u,										//	VkSubpassDescriptionFlags		flags;
			vk::VK_PIPELINE_BIND_POINT_GRAPHICS,	//	VkPipelineBindPoint				pipelineBindPoint;
			0u,										//	deUint32						inputAttachmentCount;
			nullptr,								//	const VkAttachmentReference*	pInputAttachments;
			1u,										//	deUint32						colorAttachmentCount;
			&colorAttachmentReference,				//	const VkAttachmentReference*	pColorAttachments;
			nullptr,								//	const VkAttachmentReference*	pResolveAttachments;
			nullptr,								//	const VkAttachmentReference*	pDepthStencilAttachment;
			0u,										//	deUint32						preserveAttachmentCount;
			nullptr,								//	const deUint32*					pPreserveAttachments;
		},
		// Second subpass writing to the auxiliar attachment.
		{
			0u,										//	VkSubpassDescriptionFlags		flags;
			vk::VK_PIPELINE_BIND_POINT_GRAPHICS,	//	VkPipelineBindPoint				pipelineBindPoint;
			1u,										//	deUint32						inputAttachmentCount;
			&colorAsInputAttachment,				//	const VkAttachmentReference*	pInputAttachments;
			1u,										//	deUint32						colorAttachmentCount;
			&auxiliarAttachmentReference,			//	const VkAttachmentReference*	pColorAttachments;
			nullptr,								//	const VkAttachmentReference*	pResolveAttachments;
			nullptr,								//	const VkAttachmentReference*	pDepthStencilAttachment;
			0u,										//	deUint32						preserveAttachmentCount;
			nullptr,								//	const deUint32*					pPreserveAttachments;
		},
	};

	const std::vector<vk::VkSubpassDependency> subpassDependencies =
	{
		// First subpass writes to the color attachment and second subpass reads it as an input attachment.
		{
			0u,													//	deUint32				srcSubpass;
			1u,													//	deUint32				dstSubpass;
			vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,	//	VkPipelineStageFlags	srcStageMask;
			vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,			//	VkPipelineStageFlags	dstStageMask;
			vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,			//	VkAccessFlags			srcAccessMask;
			vk::VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,			//	VkAccessFlags			dstAccessMask;
			0u,													//	VkDependencyFlags		dependencyFlags;
		},
	};

	const vk::VkRenderPassCreateInfo renderPassInfo =
	{
		vk::VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,		//	VkStructureType					sType;
		nullptr,											//	const void*						pNext;
		0u,													//	VkRenderPassCreateFlags			flags;
		static_cast<deUint32>(attachments.size()),			//	deUint32						attachmentCount;
		attachments.data(),									//	const VkAttachmentDescription*	pAttachments;
		static_cast<deUint32>(subpasses.size()),			//	deUint32						subpassCount;
		subpasses.data(),									//	const VkSubpassDescription*		pSubpasses;
		static_cast<deUint32>(subpassDependencies.size()),	//	deUint32						dependencyCount;
		subpassDependencies.data(),							//	const VkSubpassDependency*		pDependencies;
	};
	RenderPassWrapper renderPass (m_params.pipelineConstructionType, vkd, device, &renderPassInfo);

	// Framebuffer.
	const std::vector<vk::VkImage>		images		=
	{
		colorImage.get(),
		auxiliarImage.get(),
	};
	const std::vector<vk::VkImageView>	imageViews	=
	{
		colorImageView.get(),
		auxiliarImageView.get(),
	};
	renderPass.createFramebuffer(vkd, device, static_cast<deUint32>(imageViews.size()), images.data(), imageViews.data(), kImageExtent.width, kImageExtent.height);

	// Empty pipeline layout for the first subpass.
	const PipelineLayoutWrapper emptyPipelineLayout (m_params.pipelineConstructionType, vkd, device);

	// Pipeline layout for the second subpass.
	const PipelineLayoutWrapper checkPipelineLayout (m_params.pipelineConstructionType, vkd, device, descriptorSetLayout.get());

	// Shader modules.
	const auto vertModule	= ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("vert"), 0u);
	const auto writeModule	= ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("frag_write"), 0u);
	const auto checkModule	= ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("frag_check"), 0u);

	const std::vector<vk::VkVertexInputBindingDescription> vertexBindings =
	{
		{
			0u,																	//	deUint32			binding;
			static_cast<deUint32>(sizeof(decltype(quadVertices)::value_type)),	//	deUint32			stride;
			vk::VK_VERTEX_INPUT_RATE_VERTEX,									//	VkVertexInputRate	inputRate;
		},
	};

	const std::vector<vk::VkVertexInputAttributeDescription> vertexAttributes =
	{
		{
			0u,								//	deUint32	location;
			0u,								//	deUint32	binding;
			vk::VK_FORMAT_R32G32_SFLOAT,	//	VkFormat	format;
			0u,								//	deUint32	offset;
		},
	};

	const vk::VkPipelineVertexInputStateCreateInfo vertexInputInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	//	VkStructureType								sType;
		nullptr,														//	const void*									pNext;
		0u,																//	VkPipelineVertexInputStateCreateFlags		flags;
		static_cast<deUint32>(vertexBindings.size()),					//	deUint32									vertexBindingDescriptionCount;
		vertexBindings.data(),											//	const VkVertexInputBindingDescription*		pVertexBindingDescriptions;
		static_cast<deUint32>(vertexAttributes.size()),					//	deUint32									vertexAttributeDescriptionCount;
		vertexAttributes.data(),										//	const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
	};

	const vk::VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	//	VkStructureType							sType;
		nullptr,															//	const void*								pNext;
		0u,																	//	VkPipelineInputAssemblyStateCreateFlags	flags;
		vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,							//	VkPrimitiveTopology						topology;
		VK_FALSE,															//	VkBool32								primitiveRestartEnable;
	};

	const std::vector<VkViewport>	viewport	{ vk::makeViewport(kImageExtent) };
	const std::vector<VkRect2D>		scissor		{ vk::makeRect2D(kImageExtent) };

	const vk::VkPipelineMultisampleStateCreateInfo multisampleInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	//	VkStructureType							sType;
		nullptr,														//	const void*								pNext;
		0u,																//	VkPipelineMultisampleStateCreateFlags	flags;
		m_params.sampleCount,											//	VkSampleCountFlagBits					rasterizationSamples;
		VK_FALSE,														//	VkBool32								sampleShadingEnable;
		1.0f,															//	float									minSampleShading;
		nullptr,														//	const VkSampleMask*						pSampleMask;
		VK_FALSE,														//	VkBool32								alphaToCoverageEnable;
		VK_FALSE,														//	VkBool32								alphaToOneEnable;
	};

	const auto stencilState = vk::makeStencilOpState(vk::VK_STENCIL_OP_KEEP, vk::VK_STENCIL_OP_KEEP, vk::VK_STENCIL_OP_KEEP, vk::VK_COMPARE_OP_ALWAYS, 0xFFu, 0xFFu, 0u);

	const vk::VkPipelineDepthStencilStateCreateInfo depthStencilInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,	//	VkStructureType							sType;
		nullptr,														//	const void*								pNext;
		0u,																//	VkPipelineDepthStencilStateCreateFlags	flags;
		VK_FALSE,														//	VkBool32								depthTestEnable;
		VK_FALSE,														//	VkBool32								depthWriteEnable;
		vk::VK_COMPARE_OP_ALWAYS,										//	VkCompareOp								depthCompareOp;
		VK_FALSE,														//	VkBool32								depthBoundsTestEnable;
		VK_FALSE,														//	VkBool32								stencilTestEnable;
		stencilState,													//	VkStencilOpState						front;
		stencilState,													//	VkStencilOpState						back;
		0.0f,															//	float									minDepthBounds;
		1.0f,															//	float									maxDepthBounds;
	};

	const vk::VkPipelineColorBlendAttachmentState colorBlendAttachmentState =
	{
		VK_FALSE,					//	VkBool32				blendEnable;
		vk::VK_BLEND_FACTOR_ZERO,	//	VkBlendFactor			srcColorBlendFactor;
		vk::VK_BLEND_FACTOR_ZERO,	//	VkBlendFactor			dstColorBlendFactor;
		vk::VK_BLEND_OP_ADD,		//	VkBlendOp				colorBlendOp;
		vk::VK_BLEND_FACTOR_ZERO,	//	VkBlendFactor			srcAlphaBlendFactor;
		vk::VK_BLEND_FACTOR_ZERO,	//	VkBlendFactor			dstAlphaBlendFactor;
		vk::VK_BLEND_OP_ADD,		//	VkBlendOp				alphaBlendOp;
		(							//	VkColorComponentFlags	colorWriteMask;
			vk::VK_COLOR_COMPONENT_R_BIT	|
			vk::VK_COLOR_COMPONENT_G_BIT	|
			vk::VK_COLOR_COMPONENT_B_BIT	|
			vk::VK_COLOR_COMPONENT_A_BIT	),
	};

	const vk::VkPipelineColorBlendStateCreateInfo colorBlendInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	//	VkStructureType								sType;
		nullptr,														//	const void*									pNext;
		0u,																//	VkPipelineColorBlendStateCreateFlags		flags;
		VK_FALSE,														//	VkBool32									logicOpEnable;
		vk::VK_LOGIC_OP_NO_OP,											//	VkLogicOp									logicOp;
		1u,																//	deUint32									attachmentCount;
		&colorBlendAttachmentState,										//	const VkPipelineColorBlendAttachmentState*	pAttachments;
		{ .0f, .0f, .0f, .0f },											//	float										blendConstants[4];
	};

	const vk::VkPipelineDynamicStateCreateInfo dynamicStateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,	//	VkStructureType						sType;
		nullptr,													//	const void*							pNext;
		0u,															//	VkPipelineDynamicStateCreateFlags	flags;
		0u,															//	deUint32							dynamicStateCount;
		nullptr,													//	const VkDynamicState*				pDynamicStates;
	};

	// Pipeline for the first subpass.
	vk::GraphicsPipelineWrapper firstSubpassPipeline(vki, vkd, physicalDevice, device, m_context.getDeviceExtensions(), m_params.pipelineConstructionType);
	firstSubpassPipeline.setDynamicState(&dynamicStateInfo)
						.setDefaultRasterizationState()
						.setupVertexInputState(&vertexInputInfo, &inputAssemblyInfo)
						.setupPreRasterizationShaderState(viewport, scissor, emptyPipelineLayout, *renderPass, 0u, vertModule)
						.setupFragmentShaderState(emptyPipelineLayout, *renderPass, 0u, writeModule, &depthStencilInfo, &multisampleInfo)
						.setupFragmentOutputState(*renderPass, 0u, &colorBlendInfo, &multisampleInfo)
						.setMonolithicPipelineLayout(emptyPipelineLayout)
						.buildPipeline();

	// Pipeline for the second subpass.
	vk::GraphicsPipelineWrapper secondSubpassPipeline(vki, vkd, physicalDevice, device, m_context.getDeviceExtensions(), m_params.pipelineConstructionType);
	secondSubpassPipeline.setDynamicState(&dynamicStateInfo)
						.setDefaultRasterizationState()
						.setupVertexInputState(&vertexInputInfo, &inputAssemblyInfo)
						.setupPreRasterizationShaderState(viewport, scissor, checkPipelineLayout, *renderPass, 1u, vertModule)
						.setupFragmentShaderState(checkPipelineLayout, *renderPass, 1u, checkModule, &depthStencilInfo, &multisampleInfo)
						.setupFragmentOutputState(*renderPass, 1u, &colorBlendInfo, &multisampleInfo)
						.setMonolithicPipelineLayout(checkPipelineLayout)
						.buildPipeline();

	// Command pool and command buffer.
	const auto cmdPool		= vk::makeCommandPool(vkd, device, queueFamilyIndex);
	const auto cmdBufferPtr	= vk::allocateCommandBuffer(vkd, device, cmdPool.get(), vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	// Update descriptor set.
	vk::DescriptorSetUpdateBuilder updateBuilder;
	const auto imageInfo	= vk::makeDescriptorImageInfo(DE_NULL, colorImageView.get(), vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	const auto bufferInfo	= vk::makeDescriptorBufferInfo(storageBuffer.get(), 0u, VK_WHOLE_SIZE);
	updateBuilder.writeSingle(descriptorSet.get(), vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, &imageInfo);
	updateBuilder.writeSingle(descriptorSet.get(), vk::DescriptorSetUpdateBuilder::Location::binding(1u), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferInfo);
	updateBuilder.update(vkd, device);

	// Output buffer pipeline barrier.
	const auto bufferBarrier = vk::makeBufferMemoryBarrier(vk::VK_ACCESS_SHADER_WRITE_BIT, vk::VK_ACCESS_HOST_READ_BIT, storageBuffer.get(), 0ull, VK_WHOLE_SIZE);

	// Run pipelines.
	vk::beginCommandBuffer(vkd, cmdBuffer);

	renderPass.begin(vkd, cmdBuffer, vk::makeRect2D(kImageExtent), WriteSampleMaskTestCase::kClearColor);
	vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
	firstSubpassPipeline.bind(cmdBuffer);
	vkd.cmdDraw(cmdBuffer, static_cast<deUint32>(quadVertices.size()), 1u, 0u, 0u);

	renderPass.nextSubpass(vkd, cmdBuffer, vk::VK_SUBPASS_CONTENTS_INLINE);
	secondSubpassPipeline.bind(cmdBuffer);
	vkd.cmdBindDescriptorSets(cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, checkPipelineLayout.get(), 0u, 1u, &descriptorSet.get(), 0u, nullptr);
	vkd.cmdDraw(cmdBuffer, static_cast<deUint32>(quadVertices.size()), 1u, 0u, 0u);

	renderPass.end(vkd, cmdBuffer);
	vkd.cmdPipelineBarrier(cmdBuffer, vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, nullptr, 1u, &bufferBarrier, 0u, nullptr);
	vk::endCommandBuffer(vkd, cmdBuffer);

	vk::submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Check buffer contents.
	auto&					bufferAlloc		= storageBuffer.getAllocation();
	const void*				bufferPtr		= bufferAlloc.getHostPtr();
	std::vector<deInt32>	bufferContents	(kBufferElems, 0);

	vk::invalidateAlloc(vkd, device, bufferAlloc);
	deMemcpy(bufferContents.data(), bufferPtr, static_cast<size_t>(kBufferSize));

	const auto sampleCount	= static_cast<deUint32>(m_params.sampleCount);
	const auto bpc			= bitsPerCoord(sampleCount);

	for (deUint32 x = 0; x < kImageExtent.width; ++x)
	for (deUint32 y = 0; y < kImageExtent.height; ++y)
	{
		// Samples on which we expect writes.
		const deUint32 sampleMask = ((x << bpc) | y);

		// Starting location for the pixel sample values in the buffer.
		const deUint32 pixelOffset = (y * kImageExtent.width + x) * sampleCount;

		for (deUint32 s = 0; s < sampleCount; ++s)
		{
			const deUint32 sampleIndex	= pixelOffset + s;
			const deInt32& value		= bufferContents[sampleIndex];

			if (value != 0 && value != 1)
			{
				// Garbage!
				std::ostringstream msg;
				msg << "Found garbage value " << value << " in buffer position " << sampleIndex << " (x=" << x << ", y=" << y << ", sample=" << s << ")";
				return tcu::TestStatus::fail(msg.str());
			}

			const deInt32 expected = (((sampleMask & (1u << s)) != 0u) ? 1 : 0);
			if (value != expected)
			{
				std::ostringstream msg;
				msg << "Read " << value << " while expecting " << expected << " in buffer position " << sampleIndex << " (x=" << x << ", y=" << y << ", sample=" << s << ")";
				return tcu::TestStatus::fail(msg.str());
			}
		}
	}

	return tcu::TestStatus::pass("Pass");
}

} // multisample

tcu::TestCaseGroup* createMultisampleShaderBuiltInTests (tcu::TestContext& testCtx, vk::PipelineConstructionType pipelineConstructionType)
{
	de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "multisample_shader_builtin", "Multisample Shader BuiltIn Tests"));

	const tcu::UVec3 imageSizes[] =
	{
		tcu::UVec3(128u, 128u, 1u),
		tcu::UVec3(137u, 191u, 1u),
	};

	const deUint32 sizesElemCount = static_cast<deUint32>(sizeof(imageSizes) / sizeof(tcu::UVec3));

	const vk::VkSampleCountFlagBits samplesSetFull[] =
	{
		vk::VK_SAMPLE_COUNT_2_BIT,
		vk::VK_SAMPLE_COUNT_4_BIT,
		vk::VK_SAMPLE_COUNT_8_BIT,
		vk::VK_SAMPLE_COUNT_16_BIT,
		vk::VK_SAMPLE_COUNT_32_BIT,
		vk::VK_SAMPLE_COUNT_64_BIT,
	};

	const deUint32 samplesSetFullCount = static_cast<deUint32>(sizeof(samplesSetFull) / sizeof(vk::VkSampleCountFlagBits));

	testGroup->addChild(makeMSGroup<multisample::MSCase<multisample::MSCaseSampleID> >(testCtx, "sample_id", pipelineConstructionType, imageSizes, sizesElemCount, samplesSetFull, samplesSetFullCount));

	de::MovePtr<tcu::TestCaseGroup> samplePositionGroup(new tcu::TestCaseGroup(testCtx, "sample_position", "Sample Position Tests"));

	samplePositionGroup->addChild(makeMSGroup<multisample::MSCase<multisample::MSCaseSamplePosDistribution> >(testCtx, "distribution", pipelineConstructionType, imageSizes, sizesElemCount, samplesSetFull, samplesSetFullCount));
	samplePositionGroup->addChild(makeMSGroup<multisample::MSCase<multisample::MSCaseSamplePosCorrectness> > (testCtx, "correctness", pipelineConstructionType, imageSizes, sizesElemCount, samplesSetFull, samplesSetFullCount));

	testGroup->addChild(samplePositionGroup.release());

	const vk::VkSampleCountFlagBits samplesSetReduced[] =
	{
		vk::VK_SAMPLE_COUNT_2_BIT,
		vk::VK_SAMPLE_COUNT_4_BIT,
		vk::VK_SAMPLE_COUNT_8_BIT,
		vk::VK_SAMPLE_COUNT_16_BIT,
		vk::VK_SAMPLE_COUNT_32_BIT,
	};

	const deUint32 samplesSetReducedCount = static_cast<deUint32>(DE_LENGTH_OF_ARRAY(samplesSetReduced));

	de::MovePtr<tcu::TestCaseGroup> sampleMaskGroup(new tcu::TestCaseGroup(testCtx, "sample_mask", "Sample Mask Tests"));

	sampleMaskGroup->addChild(makeMSGroup<multisample::MSCase<multisample::MSCaseSampleMaskPattern> >	(testCtx, "pattern",		pipelineConstructionType, imageSizes, sizesElemCount, samplesSetReduced, samplesSetReducedCount));
	sampleMaskGroup->addChild(makeMSGroup<multisample::MSCase<multisample::MSCaseSampleMaskBitCount> >	(testCtx, "bit_count",		pipelineConstructionType, imageSizes, sizesElemCount, samplesSetReduced, samplesSetReducedCount));
	sampleMaskGroup->addChild(makeMSGroup<multisample::MSCase<multisample::MSCaseSampleMaskBitCount> >	(testCtx, "bit_count_0_5",	pipelineConstructionType, imageSizes, sizesElemCount, samplesSetReduced, samplesSetReducedCount, multisample::ComponentData{}, 0.5f));
	sampleMaskGroup->addChild(makeMSGroup<multisample::MSCase<multisample::MSCaseSampleMaskCorrectBit> >(testCtx, "correct_bit",	pipelineConstructionType, imageSizes, sizesElemCount, samplesSetReduced, samplesSetReducedCount));
	sampleMaskGroup->addChild(makeMSGroup<multisample::MSCase<multisample::MSCaseSampleMaskWrite> >		(testCtx, "write",			pipelineConstructionType, imageSizes, sizesElemCount, samplesSetReduced, samplesSetReducedCount));

	testGroup->addChild(sampleMaskGroup.release());

	// Write image sample tests using a storage images (tests construct only compute pipeline).
	if (pipelineConstructionType == vk::PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
	{
		de::MovePtr<tcu::TestCaseGroup> imageWriteSampleGroup(new tcu::TestCaseGroup(testCtx, "image_write_sample", "Test OpImageWrite with a sample ID"));

		for (auto count : multisample::kValidSquareSampleCounts)
		{
			if (count == vk::VK_SAMPLE_COUNT_1_BIT)
				continue;

			multisample::WriteSampleParams params { pipelineConstructionType, static_cast<vk::VkSampleCountFlagBits>(count) };
			const auto countStr = de::toString(count);
			imageWriteSampleGroup->addChild(new multisample::WriteSampleTest(testCtx, countStr + "_samples", "Test image with " + countStr + " samples", params));
		}

		testGroup->addChild(imageWriteSampleGroup.release());
	}

	// Write to gl_SampleMask from the fragment shader.
	{
		de::MovePtr<tcu::TestCaseGroup> writeSampleMaskGroup(new tcu::TestCaseGroup(testCtx, "write_sample_mask", "Test writes to SampleMask variable"));

		for (auto count : multisample::kValidSquareSampleCounts)
		{
			multisample::WriteSampleMaskParams params { pipelineConstructionType, static_cast<vk::VkSampleCountFlagBits>(count) };
			const auto countStr = de::toString(count);
			writeSampleMaskGroup->addChild(new multisample::WriteSampleMaskTestCase(testCtx, countStr + "_samples", "Test image with " + countStr + " samples", params));
		}

		testGroup->addChild(writeSampleMaskGroup.release());
	}

	return testGroup.release();
}

} // pipeline
} // vkt
