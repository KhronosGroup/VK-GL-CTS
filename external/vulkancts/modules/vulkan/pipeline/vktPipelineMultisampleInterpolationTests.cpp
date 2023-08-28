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
* \file vktPipelineMultisampleInterpolationTests.cpp
* \brief Multisample Interpolation Tests
*//*--------------------------------------------------------------------*/

#include "vktPipelineMultisampleInterpolationTests.hpp"
#include "vktPipelineMultisampleBaseResolve.hpp"
#include "vktPipelineMultisampleTestsUtil.hpp"
#include "vktPipelineMakeUtil.hpp"
#include "vktAmberTestCase.hpp"
#include "vkQueryUtil.hpp"
#include "tcuTestLog.hpp"
#include <vector>

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

struct VertexDataNdcScreen
{
	VertexDataNdcScreen (const tcu::Vec4& posNdc, const tcu::Vec2& posScreen) : positionNdc(posNdc), positionScreen(posScreen) {}

	tcu::Vec4 positionNdc;
	tcu::Vec2 positionScreen;
};

struct VertexDataNdcBarycentric
{
	VertexDataNdcBarycentric (const tcu::Vec4& posNdc, const tcu::Vec3& barCoord) : positionNdc(posNdc), barycentricCoord(barCoord) {}

	tcu::Vec4 positionNdc;
	tcu::Vec3 barycentricCoord;
};

bool checkForError (const vk::VkImageCreateInfo& imageRSInfo, const tcu::ConstPixelBufferAccess& dataRS, const deUint32 errorCompNdx)
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
class MSCase : public MultisampleCaseBase
{
public:
								MSCase			(tcu::TestContext&		testCtx,
												 const std::string&		name,
												 const ImageMSParams&	imageMSParams)
								: MultisampleCaseBase(testCtx, name, imageMSParams) {}

	void						init			(void);
	void						initPrograms	(vk::SourceCollections& programCollection) const;
	void						checkSupport	(Context&				context) const;
	TestInstance*				createInstance	(Context&				context) const;
	static MultisampleCaseBase*	createCase		(tcu::TestContext&		testCtx,
												 const std::string&		name,
												 const ImageMSParams&	imageMSParams);
};

template <typename CaseClassName>
void MSCase<CaseClassName>::checkSupport (Context& context) const
{
	checkGraphicsPipelineLibrarySupport(context);

#ifndef CTS_USES_VULKANSC
	if (context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") &&
		!context.getPortabilitySubsetFeatures().shaderSampleRateInterpolationFunctions)
	{
		TCU_THROW(NotSupportedError, "VK_KHR_portability_subset: Shader sample rate interpolation functions are not supported by this implementation");
	}
#endif // CTS_USES_VULKANSC

	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SAMPLE_RATE_SHADING);
}

template <typename CaseClassName>
MultisampleCaseBase* MSCase<CaseClassName>::createCase (tcu::TestContext& testCtx, const std::string& name, const ImageMSParams& imageMSParams)
{
	return new MSCase<CaseClassName>(testCtx, name, imageMSParams);
}

template <typename InstanceClassName>
class MSInstance : public MSInstanceBaseResolve
{
public:
					MSInstance				(Context&							context,
											 const ImageMSParams&				imageMSParams)
					: MSInstanceBaseResolve(context, imageMSParams) {}

	VertexDataDesc	getVertexDataDescripton	(void) const;
	void			uploadVertexData		(const Allocation&					vertexBufferAllocation,
											 const VertexDataDesc&				vertexDataDescripton) const;
	tcu::TestStatus	verifyImageData			(const vk::VkImageCreateInfo&		imageRSInfo,
											 const tcu::ConstPixelBufferAccess& dataRS) const;
};

class MSInstanceDistinctValues;

template<> MultisampleInstanceBase::VertexDataDesc MSInstance<MSInstanceDistinctValues>::getVertexDataDescripton (void) const
{
	VertexDataDesc vertexDataDesc;

	vertexDataDesc.verticesCount		= 3u;
	vertexDataDesc.dataStride			= sizeof(VertexDataNdc);
	vertexDataDesc.dataSize				= vertexDataDesc.verticesCount * vertexDataDesc.dataStride;
	vertexDataDesc.primitiveTopology	= VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	const VkVertexInputAttributeDescription vertexAttribPositionNdc =
	{
		0u,										// deUint32	location;
		0u,										// deUint32	binding;
		VK_FORMAT_R32G32B32A32_SFLOAT,			// VkFormat	format;
		DE_OFFSET_OF(VertexDataNdc, positionNdc),	// deUint32	offset;
	};

	vertexDataDesc.vertexAttribDescVec.push_back(vertexAttribPositionNdc);

	return vertexDataDesc;
}

template<> void MSInstance<MSInstanceDistinctValues>::uploadVertexData (const Allocation& vertexBufferAllocation, const VertexDataDesc& vertexDataDescripton) const
{
	std::vector<VertexDataNdc> vertices;

	vertices.push_back(VertexDataNdc(tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f)));
	vertices.push_back(VertexDataNdc(tcu::Vec4(-1.0f,  4.0f, 0.0f, 1.0f)));
	vertices.push_back(VertexDataNdc(tcu::Vec4( 4.0f, -1.0f, 0.0f, 1.0f)));

	deMemcpy(vertexBufferAllocation.getHostPtr(), dataPointer(vertices), static_cast<std::size_t>(vertexDataDescripton.dataSize));
}

template<> tcu::TestStatus MSInstance<MSInstanceDistinctValues>::verifyImageData (const vk::VkImageCreateInfo& imageRSInfo, const tcu::ConstPixelBufferAccess& dataRS) const
{
	const deUint32 distinctValuesExpected = static_cast<deUint32>(m_imageMSParams.numSamples) + 1u;

	std::vector<tcu::IVec4> distinctValues;

	for (deUint32 z = 0u; z < imageRSInfo.extent.depth;	 ++z)
	for (deUint32 y = 0u; y < imageRSInfo.extent.height; ++y)
	for (deUint32 x = 0u; x < imageRSInfo.extent.width;	 ++x)
	{
		const tcu::IVec4 pixel = dataRS.getPixelInt(x, y, z);

		if (std::find(distinctValues.begin(), distinctValues.end(), pixel) == distinctValues.end())
			distinctValues.push_back(pixel);
	}

	if (distinctValues.size() >= distinctValuesExpected)
		return tcu::TestStatus::pass("Passed");
	else
		return tcu::TestStatus::fail("Expected numSamples+1 different colors in the output image");
}

class MSCaseSampleQualifierDistinctValues;

template<> void MSCase<MSCaseSampleQualifierDistinctValues>::init (void)
{
	m_testCtx.getLog()
		<< tcu::TestLog::Message
		<< "Verifying that a sample qualified varying is given different values for different samples.\n"
		<< "	Render full screen traingle with quadratic function defining red/green color pattern division.\n"
		<< "	=> Resulting image should contain n+1 different colors, where n = sample count.\n"
		<< tcu::TestLog::EndMessage;

	MultisampleCaseBase::init();
}

template<> void MSCase<MSCaseSampleQualifierDistinctValues>::initPrograms (vk::SourceCollections& programCollection) const
{
	// Create vertex shader
	std::ostringstream vs;

	vs << "#version 440\n"
		<< "layout(location = 0) in vec4 vs_in_position_ndc;\n"
		<< "\n"
		<< "layout(location = 0) out vec4 vs_out_position_ndc;\n"
		<< "\n"
		<< "out gl_PerVertex {\n"
		<< "	vec4  gl_Position;\n"
		<< "};\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	gl_Position			= vs_in_position_ndc;\n"
		<< "	vs_out_position_ndc = vs_in_position_ndc;\n"
		<< "}\n";

	programCollection.glslSources.add("vertex_shader") << glu::VertexSource(vs.str());

	// Create fragment shader
	std::ostringstream fs;

	fs << "#version 440\n"
		<< "layout(location = 0) sample in vec4 fs_in_position_ndc;\n"
		<< "\n"
		<< "layout(location = 0) out vec4 fs_out_color;\n"
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	if(fs_in_position_ndc.y < -2.0*pow(0.5*(fs_in_position_ndc.x + 1.0), 2.0) + 1.0)\n"
		<< "		fs_out_color = vec4(1.0, 0.0, 0.0, 1.0);\n"
		<< "	else\n"
		<< "		fs_out_color = vec4(0.0, 1.0, 0.0, 1.0);\n"
		<< "}\n";

	programCollection.glslSources.add("fragment_shader") << glu::FragmentSource(fs.str());
}

template<> void MSCase<MSCaseSampleQualifierDistinctValues>::checkSupport (Context& context) const
{
	checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), m_imageMSParams.pipelineConstructionType);

	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SAMPLE_RATE_SHADING);
}

template<> TestInstance* MSCase<MSCaseSampleQualifierDistinctValues>::createInstance (Context& context) const
{
	return new MSInstance<MSInstanceDistinctValues>(context, m_imageMSParams);
}

class MSCaseInterpolateAtSampleDistinctValues;

template<> void MSCase<MSCaseInterpolateAtSampleDistinctValues>::init (void)
{
	m_testCtx.getLog()
		<< tcu::TestLog::Message
		<< "Verifying that a interpolateAtSample returns different values for different samples.\n"
		<< "	Render full screen traingle with quadratic function defining red/green color pattern division.\n"
		<< "	=> Resulting image should contain n+1 different colors, where n = sample count.\n"
		<< tcu::TestLog::EndMessage;

	MultisampleCaseBase::init();
}

template<> void MSCase<MSCaseInterpolateAtSampleDistinctValues>::initPrograms (vk::SourceCollections& programCollection) const
{
	// Create vertex shader
	std::ostringstream vs;

	vs << "#version 440\n"
		<< "layout(location = 0) in vec4 vs_in_position_ndc;\n"
		<< "\n"
		<< "layout(location = 0) out vec4 vs_out_position_ndc;\n"
		<< "\n"
		<< "out gl_PerVertex {\n"
		<< "	vec4  gl_Position;\n"
		<< "};\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	gl_Position			= vs_in_position_ndc;\n"
		<< "	vs_out_position_ndc = vs_in_position_ndc;\n"
		<< "}\n";

	programCollection.glslSources.add("vertex_shader") << glu::VertexSource(vs.str());

	// Create fragment shader
	std::ostringstream fs;

	fs << "#version 440\n"
		<< "layout(location = 0) in vec4 fs_in_position_ndc;\n"
		<< "\n"
		<< "layout(location = 0) out vec4 fs_out_color;\n"
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	const vec4 position_ndc_at_sample = interpolateAtSample(fs_in_position_ndc, gl_SampleID);\n"
		<< "	if(position_ndc_at_sample.y < -2.0*pow(0.5*(position_ndc_at_sample.x + 1.0), 2.0) + 1.0)\n"
		<< "		fs_out_color = vec4(0.0, 1.0, 0.0, 1.0);\n"
		<< "	else\n"
		<< "		fs_out_color = vec4(1.0, 0.0, 0.0, 1.0);\n"
		<< "}\n";

	programCollection.glslSources.add("fragment_shader") << glu::FragmentSource(fs.str());
}

template<> TestInstance* MSCase<MSCaseInterpolateAtSampleDistinctValues>::createInstance (Context& context) const
{
	return new MSInstance<MSInstanceDistinctValues>(context, m_imageMSParams);
}

class MSInstanceInterpolateScreenPosition;

template<> MSInstanceBaseResolve::VertexDataDesc MSInstance<MSInstanceInterpolateScreenPosition>::getVertexDataDescripton (void) const
{
	VertexDataDesc vertexDataDesc;

	vertexDataDesc.verticesCount		= 4u;
	vertexDataDesc.dataStride			= sizeof(VertexDataNdcScreen);
	vertexDataDesc.dataSize				= vertexDataDesc.verticesCount * vertexDataDesc.dataStride;
	vertexDataDesc.primitiveTopology	= VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

	const VkVertexInputAttributeDescription vertexAttribPositionNdc =
	{
		0u,												// deUint32	location;
		0u,												// deUint32	binding;
		VK_FORMAT_R32G32B32A32_SFLOAT,					// VkFormat	format;
		DE_OFFSET_OF(VertexDataNdcScreen, positionNdc),	// deUint32	offset;
	};

	vertexDataDesc.vertexAttribDescVec.push_back(vertexAttribPositionNdc);

	const VkVertexInputAttributeDescription vertexAttribPositionScreen =
	{
		1u,											// deUint32	location;
		0u,											// deUint32	binding;
		VK_FORMAT_R32G32_SFLOAT,					// VkFormat	format;
		DE_OFFSET_OF(VertexDataNdcScreen, positionScreen),	// deUint32	offset;
	};

	vertexDataDesc.vertexAttribDescVec.push_back(vertexAttribPositionScreen);

	return vertexDataDesc;
}

template<> void MSInstance<MSInstanceInterpolateScreenPosition>::uploadVertexData (const Allocation& vertexBufferAllocation, const VertexDataDesc& vertexDataDescripton) const
{
	const tcu::UVec3 layerSize		= getLayerSize(IMAGE_TYPE_2D, m_imageMSParams.imageSize);
	const float		 screenSizeX	= static_cast<float>(layerSize.x());
	const float		 screenSizeY	= static_cast<float>(layerSize.y());

	std::vector<VertexDataNdcScreen> vertices;

	vertices.push_back(VertexDataNdcScreen(tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f), tcu::Vec2(0.0f, 0.0f)));
	vertices.push_back(VertexDataNdcScreen(tcu::Vec4( 1.0f, -1.0f, 0.0f, 1.0f), tcu::Vec2(screenSizeX, 0.0f)));
	vertices.push_back(VertexDataNdcScreen(tcu::Vec4(-1.0f,  1.0f, 0.0f, 1.0f), tcu::Vec2(0.0f, screenSizeY)));
	vertices.push_back(VertexDataNdcScreen(tcu::Vec4( 1.0f,  1.0f, 0.0f, 1.0f), tcu::Vec2(screenSizeX, screenSizeY)));

	deMemcpy(vertexBufferAllocation.getHostPtr(), dataPointer(vertices), static_cast<std::size_t>(vertexDataDescripton.dataSize));
}

template<> tcu::TestStatus MSInstance<MSInstanceInterpolateScreenPosition>::verifyImageData (const vk::VkImageCreateInfo& imageRSInfo, const tcu::ConstPixelBufferAccess& dataRS) const
{
	if (checkForError(imageRSInfo, dataRS, 0))
		return tcu::TestStatus::fail("Failed");

	return tcu::TestStatus::pass("Passed");
}

class MSCaseInterpolateAtSampleSingleSample;

template<> void MSCase<MSCaseInterpolateAtSampleSingleSample>::init (void)
{
	m_testCtx.getLog()
		<< tcu::TestLog::Message
		<< "Verifying that using interpolateAtSample with multisample buffers not available returns sample evaluated at the center of the pixel.\n"
		<< "	Interpolate varying containing screen space location.\n"
		<< "	=> fract(screen space location) should be (about) (0.5, 0.5)\n"
		<< tcu::TestLog::EndMessage;

	MultisampleCaseBase::init();
}

template<> void MSCase<MSCaseInterpolateAtSampleSingleSample>::initPrograms (vk::SourceCollections& programCollection) const
{
	// Create vertex shader
	std::ostringstream vs;

	vs << "#version 440\n"
		<< "layout(location = 0) in vec4 vs_in_position_ndc;\n"
		<< "layout(location = 1) in vec2 vs_in_position_screen;\n"
		<< "\n"
		<< "layout(location = 0) out vec2 vs_out_position_screen;\n"
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

	fs << "#version 440\n"
		<< "layout(location = 0) in vec2 fs_in_position_screen;\n"
		<< "\n"
		<< "layout(location = 0) out vec4 fs_out_color;\n"
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	const float threshold					= 0.15625;\n"
		<< "	const vec2  position_screen_at_sample	= interpolateAtSample(fs_in_position_screen, 0);\n"
		<< "	const vec2  position_inside_pixel		= fract(position_screen_at_sample);\n"
		<< "\n"
		<< "	if (abs(position_inside_pixel.x - 0.5) <= threshold && abs(position_inside_pixel.y - 0.5) <= threshold)\n"
		<< "		fs_out_color = vec4(0.0, 1.0, 0.0, 1.0);\n"
		<< "	else\n"
		<< "		fs_out_color = vec4(1.0, 0.0, 0.0, 1.0);\n"
		<< "}\n";

	programCollection.glslSources.add("fragment_shader") << glu::FragmentSource(fs.str());
}

template<> TestInstance* MSCase<MSCaseInterpolateAtSampleSingleSample>::createInstance (Context& context) const
{
	return new MSInstance<MSInstanceInterpolateScreenPosition>(context, m_imageMSParams);
}

class MSCaseInterpolateAtSampleIgnoresCentroid;

template<> void MSCase<MSCaseInterpolateAtSampleIgnoresCentroid>::init (void)
{
	m_testCtx.getLog()
		<< tcu::TestLog::Message
		<< "Verifying that interpolateAtSample ignores centroid qualifier.\n"
		<< "	Interpolate varying containing screen space location with centroid and sample qualifiers.\n"
		<< "	=> interpolateAtSample(screenSample, n) ~= interpolateAtSample(screenCentroid, n)\n"
		<< tcu::TestLog::EndMessage;

	MultisampleCaseBase::init();
}

template<> void MSCase<MSCaseInterpolateAtSampleIgnoresCentroid>::initPrograms (vk::SourceCollections& programCollection) const
{
	// Create vertex shader
	std::ostringstream vs;

	vs << "#version 440\n"
		<< "layout(location = 0) in vec4 vs_in_position_ndc;\n"
		<< "layout(location = 1) in vec2 vs_in_position_screen;\n"
		<< "\n"
		<< "layout(location = 0) out vec2 vs_out_pos_screen_centroid;\n"
		<< "layout(location = 1) out vec2 vs_out_pos_screen_fragment;\n"
		<< "\n"
		<< "out gl_PerVertex {\n"
		<< "	vec4  gl_Position;\n"
		<< "};\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	gl_Position					= vs_in_position_ndc;\n"
		<< "	vs_out_pos_screen_centroid	= vs_in_position_screen;\n"
		<< "	vs_out_pos_screen_fragment	= vs_in_position_screen;\n"
		<< "}\n";

	programCollection.glslSources.add("vertex_shader") << glu::VertexSource(vs.str());

	// Create fragment shader
	std::ostringstream fs;

	fs << "#version 440\n"
		<< "layout(location = 0) centroid in vec2 fs_in_pos_screen_centroid;\n"
		<< "layout(location = 1)		  in vec2 fs_in_pos_screen_fragment;\n"
		<< "\n"
		<< "layout(location = 0) out vec4 fs_out_color;\n"
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	const float threshold = 0.0005;\n"
		<< "\n"
		<< "	const vec2 position_a  = interpolateAtSample(fs_in_pos_screen_centroid, gl_SampleID);\n"
		<< "	const vec2 position_b  = interpolateAtSample(fs_in_pos_screen_fragment, gl_SampleID);\n"
		<< "	const bool valuesEqual = all(lessThan(abs(position_a - position_b), vec2(threshold)));\n"
		<< "\n"
		<< "	if (valuesEqual)\n"
		<< "		fs_out_color = vec4(0.0, 1.0, 0.0, 1.0);\n"
		<< "	else\n"
		<< "		fs_out_color = vec4(1.0, 0.0, 0.0, 1.0);\n"
		<< "}\n";

	programCollection.glslSources.add("fragment_shader") << glu::FragmentSource(fs.str());
}

template<> TestInstance* MSCase<MSCaseInterpolateAtSampleIgnoresCentroid>::createInstance (Context& context) const
{
	return new MSInstance<MSInstanceInterpolateScreenPosition>(context, m_imageMSParams);
}

class MSCaseInterpolateAtSampleConsistency;

template<> void MSCase<MSCaseInterpolateAtSampleConsistency>::init (void)
{
	const std::string	indexStr = de::toString(m_imageMSParams.componentData.index);
	std::string			componentMsg;

	switch (m_imageMSParams.componentData.source)
	{
	case multisample::ComponentSource::CONSTANT:		componentMsg = "Using single constant component " + indexStr;			break;
	case multisample::ComponentSource::PUSH_CONSTANT:	componentMsg = "Using single component via push constant " + indexStr;	break;
	default: break;
	}

	m_testCtx.getLog()
		<< tcu::TestLog::Message
		<< "Verifying that interpolateAtSample with the sample set to the current sampleID returns consistent values.\n"
		<< (componentMsg.empty() ? std::string() : componentMsg + "\n")
		<< "	Interpolate varying containing screen space location with centroid and sample qualifiers.\n"
		<< "	=> interpolateAtSample(screenCentroid, sampleID) = screenSample\n"
		<< tcu::TestLog::EndMessage;

	MultisampleCaseBase::init();
}

template<> void MSCase<MSCaseInterpolateAtSampleConsistency>::initPrograms (vk::SourceCollections& programCollection) const
{
	// Create vertex shader
	std::ostringstream vs;

	vs << "#version 440\n"
		<< "layout(location = 0) in vec4 vs_in_position_ndc;\n"
		<< "layout(location = 1) in vec2 vs_in_position_screen;\n"
		<< "\n"
		<< "layout(location = 0) out vec2 vs_out_pos_screen_centroid;\n"
		<< "layout(location = 1) out vec2 vs_out_pos_screen_sample;\n"
		<< "\n"
		<< "out gl_PerVertex {\n"
		<< "	vec4  gl_Position;\n"
		<< "};\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	gl_Position					= vs_in_position_ndc;\n"
		<< "	vs_out_pos_screen_centroid	= vs_in_position_screen;\n"
		<< "	vs_out_pos_screen_sample	= vs_in_position_screen;\n"
		<< "}\n";

	programCollection.glslSources.add("vertex_shader") << glu::VertexSource(vs.str());

	// Create fragment shader
	std::ostringstream fs;

	fs	<< "#version 440\n"
		<< "layout(location = 0) centroid in vec2 fs_in_pos_screen_centroid;\n"
		<< "layout(location = 1) sample   in vec2 fs_in_pos_screen_sample;\n"
		<< "\n"
		<< "layout(location = 0) out vec4 fs_out_color;\n"
		<< "\n";

	if (m_imageMSParams.componentData.source == multisample::ComponentSource::PUSH_CONSTANT)
	{
		fs	<< "layout(push_constant) uniform PushConstants {\n"
			<< "   uint component;\n"
			<< "};\n"
			<< "\n";
	}

	fs	<< "void main (void)\n"
		<< "{\n"
		<< "	const float threshold = 0.15625;\n"
		<< "\n";

	if (m_imageMSParams.componentData.source == multisample::ComponentSource::NONE)
	{
		fs	<< "	const vec2  pos_interpolated_at_sample = interpolateAtSample(fs_in_pos_screen_centroid, gl_SampleID);\n"
			<< "	const bool  valuesEqual                = all(lessThan(abs(pos_interpolated_at_sample - fs_in_pos_screen_sample), vec2(threshold)));\n";
	}
	else if (m_imageMSParams.componentData.source == multisample::ComponentSource::CONSTANT)
	{
		const auto& index = m_imageMSParams.componentData.index;
		fs	<< "	const float pos_interpolated_at_sample = interpolateAtSample(fs_in_pos_screen_centroid[" << index << "], gl_SampleID);\n"
			<< "	const bool  valuesEqual                = (abs(pos_interpolated_at_sample - fs_in_pos_screen_sample[" << index << "]) < threshold);\n";
	}
	else // multisample::ComponentSource::PUSH_CONSTANT
	{
		fs	<< "	const float pos_interpolated_at_sample = interpolateAtSample(fs_in_pos_screen_centroid[component], gl_SampleID);\n"
			<< "	const bool  valuesEqual                = (abs(pos_interpolated_at_sample - fs_in_pos_screen_sample[component]) < threshold);\n";
	}

	fs	<< "\n"
		<< "	if (valuesEqual)\n"
		<< "		fs_out_color = vec4(0.0, 1.0, 0.0, 1.0);\n"
		<< "	else\n"
		<< "		fs_out_color = vec4(1.0, 0.0, 0.0, 1.0);\n"
		<< "}\n";

	programCollection.glslSources.add("fragment_shader") << glu::FragmentSource(fs.str());
}

template<> TestInstance* MSCase<MSCaseInterpolateAtSampleConsistency>::createInstance (Context& context) const
{
	return new MSInstance<MSInstanceInterpolateScreenPosition>(context, m_imageMSParams);
}

class MSCaseInterpolateAtCentroidConsistency;

template<> void MSCase<MSCaseInterpolateAtCentroidConsistency>::init (void)
{
	const std::string	indexStr = de::toString(m_imageMSParams.componentData.index);
	std::string			componentMsg;

	switch (m_imageMSParams.componentData.source)
	{
	case multisample::ComponentSource::CONSTANT:		componentMsg = "Using single constant component " + indexStr;			break;
	case multisample::ComponentSource::PUSH_CONSTANT:	componentMsg = "Using single component via push constant " + indexStr;	break;
	default: break;
	}

	m_testCtx.getLog()
		<< tcu::TestLog::Message
		<< "Verifying that interpolateAtCentroid does not return different values than a corresponding centroid qualified varying.\n"
		<< (componentMsg.empty() ? std::string() : componentMsg + "\n")
		<< "	Interpolate varying containing screen space location with sample and centroid qualifiers.\n"
		<< "	=> interpolateAtCentroid(screenSample) = screenCentroid\n"
		<< tcu::TestLog::EndMessage;

	MultisampleCaseBase::init();
}

template<> void MSCase<MSCaseInterpolateAtCentroidConsistency>::initPrograms (vk::SourceCollections& programCollection) const
{
	// Create vertex shader
	std::ostringstream vs;

	vs << "#version 440\n"
		<< "layout(location = 0) in vec4 vs_in_position_ndc;\n"
		<< "layout(location = 1) in vec2 vs_in_position_screen;\n"
		<< "\n"
		<< "layout(location = 0) out vec2 vs_out_pos_screen_sample[2];\n"
		<< "layout(location = 2) out vec2 vs_out_pos_screen_centroid[2];\n"
		<< "\n"
		<< "out gl_PerVertex {\n"
		<< "	vec4  gl_Position;\n"
		<< "};\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	gl_Position				      = vs_in_position_ndc;\n"
		// Index 0 is never read, so we'll populate them with bad values
		<< "	vs_out_pos_screen_sample[0]	  = vec2(-70.3, 42.1);\n"
		<< "	vs_out_pos_screen_centroid[0] = vec2(7.7, -3.2);\n"
		// Actual coordinates in index 1:
		<< "	vs_out_pos_screen_sample[1]	  = vs_in_position_screen;\n"
		<< "	vs_out_pos_screen_centroid[1] = vs_in_position_screen;\n"
		<< "}\n";

	programCollection.glslSources.add("vertex_shader") << glu::VertexSource(vs.str());

	// Create fragment shader
	std::ostringstream fs;

	fs	<< "#version 440\n"
		<< "layout(location = 0) sample   in vec2 fs_in_pos_screen_sample[2];\n"
		<< "layout(location = 2) centroid in vec2 fs_in_pos_screen_centroid[2];\n"
		<< "\n"
		<< "layout(location = 0) out vec4 fs_out_color;\n"
		<< "\n";

	if (m_imageMSParams.componentData.source == multisample::ComponentSource::PUSH_CONSTANT)
	{
		fs	<< "layout(push_constant) uniform PushConstants {\n"
			<< "   uint component;\n"
			<< "};\n"
			<< "\n";
	}

	fs	<< "void main (void)\n"
		<< "{\n"
		<< "	const float threshold = 0.0005;\n"
		<< "\n";

	if (m_imageMSParams.componentData.source == multisample::ComponentSource::NONE)
	{
		fs	<< "	const vec2 pos_interpolated_at_centroid = interpolateAtCentroid(fs_in_pos_screen_sample[1]);\n"
			<< "	const bool valuesEqual                  = all(lessThan(abs(pos_interpolated_at_centroid - fs_in_pos_screen_centroid[1]), vec2(threshold)));\n";
	}
	else if (m_imageMSParams.componentData.source == multisample::ComponentSource::CONSTANT)
	{
		const auto& index = m_imageMSParams.componentData.index;
		fs	<< "	const float pos_interpolated_at_centroid = interpolateAtCentroid(fs_in_pos_screen_sample[1][" << index << "]);\n"
			<< "	const bool  valuesEqual                  = (abs(pos_interpolated_at_centroid - fs_in_pos_screen_centroid[1][" << index << "]) < threshold);\n";
	}
	else // multisample::ComponentSource::PUSH_CONSTANT
	{
		fs	<< "	const float pos_interpolated_at_centroid = interpolateAtCentroid(fs_in_pos_screen_sample[1][component]);\n"
			<< "	const bool  valuesEqual                  = (abs(pos_interpolated_at_centroid - fs_in_pos_screen_centroid[1][component]) < threshold);\n";
	}

	fs	<< "\n"
		<< "	if (valuesEqual)\n"
		<< "		fs_out_color = vec4(0.0, 1.0, 0.0, 1.0);\n"
		<< "	else\n"
		<< "		fs_out_color = vec4(1.0, 0.0, 0.0, 1.0);\n"
		<< "}\n";

	programCollection.glslSources.add("fragment_shader") << glu::FragmentSource(fs.str());
}

template<> TestInstance* MSCase<MSCaseInterpolateAtCentroidConsistency>::createInstance (Context& context) const
{
	return new MSInstance<MSInstanceInterpolateScreenPosition>(context, m_imageMSParams);
}

class MSCaseInterpolateAtOffsetPixelCenter;

template<> void MSCase<MSCaseInterpolateAtOffsetPixelCenter>::init (void)
{
	m_testCtx.getLog()
		<< tcu::TestLog::Message
		<< "Verifying that interpolateAtOffset returns value sampled at an offset from the center of the pixel.\n"
		<< "	Interpolate varying containing screen space location.\n"
		<< "	=> interpolateAtOffset(screen, offset) should be \"varying value at the pixel center\" + offset"
		<< tcu::TestLog::EndMessage;

	MultisampleCaseBase::init();
}

template<> void MSCase<MSCaseInterpolateAtOffsetPixelCenter>::initPrograms (vk::SourceCollections& programCollection) const
{
	// Create vertex shader
	std::ostringstream vs;

	vs << "#version 440\n"
		<< "layout(location = 0) in vec4 vs_in_position_ndc;\n"
		<< "layout(location = 1) in vec2 vs_in_position_screen;\n"
		<< "\n"
		<< "layout(location = 0) out vec2 vs_out_pos_screen;\n"
		<< "layout(location = 1) out vec2 vs_out_offset;\n"
		<< "\n"
		<< "out gl_PerVertex {\n"
		<< "	vec4  gl_Position;\n"
		<< "};\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	gl_Position			= vs_in_position_ndc;\n"
		<< "	vs_out_pos_screen	= vs_in_position_screen;\n"
		<< "	vs_out_offset		= vs_in_position_ndc.xy * 0.5;\n"
		<< "}\n";

	programCollection.glslSources.add("vertex_shader") << glu::VertexSource(vs.str());

	// Create fragment shader
	std::ostringstream fs;

	fs << "#version 440\n"
		<< "layout(location = 0) in  vec2 fs_in_pos_screen;\n"
		<< "layout(location = 1) in  vec2 fs_in_offset;\n"
		<< "\n"
		<< "layout(location = 0) out vec4 fs_out_color;\n"
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "    const vec2  frag_center = interpolateAtOffset(fs_in_pos_screen, vec2(0.0));\n"
		<< "    const vec2  center_diff = abs(frag_center - fs_in_pos_screen);\n"
		<< "    const float threshold   = 0.125;\n"
		<< "    bool        valuesEqual = false;\n"
		<< "\n"
		<< "    if (all(lessThan(center_diff, vec2(0.5 + threshold)))) {\n"
		<< "        const vec2 pos_interpolated_at_offset = interpolateAtOffset(fs_in_pos_screen, fs_in_offset);\n"
		<< "        const vec2 reference_value            = frag_center + fs_in_offset;\n"
		<< "\n"
		<< "        valuesEqual = all(lessThan(abs(pos_interpolated_at_offset - reference_value), vec2(threshold)));\n"
		<< "    }\n"
		<< "\n"
		<< "    if (valuesEqual)\n"
		<< "        fs_out_color = vec4(0.0, 1.0, 0.0, 1.0);\n"
		<< "    else\n"
		<< "        fs_out_color = vec4(1.0, 0.0, 0.0, 1.0);\n"
		<< "}\n";

	programCollection.glslSources.add("fragment_shader") << glu::FragmentSource(fs.str());
}

template<> TestInstance* MSCase<MSCaseInterpolateAtOffsetPixelCenter>::createInstance (Context& context) const
{
	return new MSInstance<MSInstanceInterpolateScreenPosition>(context, m_imageMSParams);
}

class MSCaseInterpolateAtOffsetSamplePosition;

template<> void MSCase<MSCaseInterpolateAtOffsetSamplePosition>::init (void)
{
	const std::string	indexStr = de::toString(m_imageMSParams.componentData.index);
	std::string			componentMsg;

	switch (m_imageMSParams.componentData.source)
	{
	case multisample::ComponentSource::CONSTANT:		componentMsg = "Using single constant component " + indexStr;			break;
	case multisample::ComponentSource::PUSH_CONSTANT:	componentMsg = "Using single component via push constant " + indexStr;	break;
	default: break;
	}

	m_testCtx.getLog()
		<< tcu::TestLog::Message
		<< "Verifying that interpolateAtOffset of screen position with the offset of current sample position returns value "
		<< "similar to screen position interpolated at sample.\n"
		<< (componentMsg.empty() ? std::string() : componentMsg + "\n")
		<< "	Interpolate varying containing screen space location with and without sample qualifier.\n"
		<< "	=> interpolateAtOffset(screenFragment, samplePosition - (0.5,0.5)) = screenSample"
		<< tcu::TestLog::EndMessage;

	MultisampleCaseBase::init();
}

template<> void MSCase<MSCaseInterpolateAtOffsetSamplePosition>::initPrograms (vk::SourceCollections& programCollection) const
{
	// Create vertex shader
	std::ostringstream vs;

	vs << "#version 440\n"
		<< "layout(location = 0) in vec4 vs_in_position_ndc;\n"
		<< "layout(location = 1) in vec2 vs_in_position_screen;\n"
		<< "\n"
		<< "layout(location = 0) out vec2 vs_out_pos_screen_fragment;\n"
		<< "layout(location = 1) out vec2 vs_out_pos_screen_sample;\n"
		<< "\n"
		<< "out gl_PerVertex {\n"
		<< "	vec4  gl_Position;\n"
		<< "};\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	gl_Position					= vs_in_position_ndc;\n"
		<< "	vs_out_pos_screen_fragment	= vs_in_position_screen;\n"
		<< "	vs_out_pos_screen_sample	= vs_in_position_screen;\n"
		<< "}\n";

	programCollection.glslSources.add("vertex_shader") << glu::VertexSource(vs.str());

	// Create fragment shader
	std::ostringstream fs;

	fs	<< "#version 440\n"
		<< "layout(location = 0)		in vec2 fs_in_pos_screen_fragment;\n"
		<< "layout(location = 1) sample in vec2 fs_in_pos_screen_sample;\n"
		<< "\n"
		<< "layout(location = 0) out vec4 fs_out_color;\n"
		<< "\n";

	if (m_imageMSParams.componentData.source == multisample::ComponentSource::PUSH_CONSTANT)
	{
		fs	<< "layout(push_constant) uniform PushConstants {\n"
			<< "   uint component;\n"
			<< "};\n"
			<< "\n";
	}

	fs	<< "void main (void)\n"
		<< "{\n"
		<< "	const float threshold = 0.15625;\n"
		<< "\n"
		<< "	const vec2 offset                     = gl_SamplePosition - vec2(0.5, 0.5);\n";

	if (m_imageMSParams.componentData.source == multisample::ComponentSource::NONE)
	{
		fs	<< "	const vec2 pos_interpolated_at_offset = interpolateAtOffset(fs_in_pos_screen_fragment, offset);\n"
			<< "	const bool valuesEqual                = all(lessThan(abs(pos_interpolated_at_offset - fs_in_pos_screen_sample), vec2(threshold)));\n";
	}
	else if (m_imageMSParams.componentData.source == multisample::ComponentSource::CONSTANT)
	{
		const auto& index = m_imageMSParams.componentData.index;
		fs	<< "	const float pos_interpolated_at_offset = interpolateAtOffset(fs_in_pos_screen_fragment[" << index << "], offset);\n"
			<< "	const bool valuesEqual                 = (abs(pos_interpolated_at_offset - fs_in_pos_screen_sample[" << index << "]) < threshold);\n";
	}
	else // multisample::ComponentSource::PUSH_CONSTANT
	{
		fs	<< "	const float pos_interpolated_at_offset = interpolateAtOffset(fs_in_pos_screen_fragment[component], offset);\n"
			<< "	const bool valuesEqual                 = (abs(pos_interpolated_at_offset - fs_in_pos_screen_sample[component]) < threshold);\n";
	}

	fs	<< "\n"
		<< "	if (valuesEqual)\n"
		<< "		fs_out_color = vec4(0.0, 1.0, 0.0, 1.0);\n"
		<< "	else\n"
		<< "		fs_out_color = vec4(1.0, 0.0, 0.0, 1.0);\n"
		<< "}\n";

	programCollection.glslSources.add("fragment_shader") << glu::FragmentSource(fs.str());
}

template<> TestInstance* MSCase<MSCaseInterpolateAtOffsetSamplePosition>::createInstance (Context& context) const
{
	return new MSInstance<MSInstanceInterpolateScreenPosition>(context, m_imageMSParams);
}

class MSInstanceInterpolateBarycentricCoordinates;

template<> MSInstanceBaseResolve::VertexDataDesc MSInstance<MSInstanceInterpolateBarycentricCoordinates>::getVertexDataDescripton (void) const
{
	VertexDataDesc vertexDataDesc;

	vertexDataDesc.verticesCount		= 3u;
	vertexDataDesc.dataStride			= sizeof(VertexDataNdcBarycentric);
	vertexDataDesc.dataSize				= vertexDataDesc.verticesCount * vertexDataDesc.dataStride;
	vertexDataDesc.primitiveTopology	= VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	const VkVertexInputAttributeDescription vertexAttribPositionNdc =
	{
		0u,															// deUint32	location;
		0u,															// deUint32	binding;
		VK_FORMAT_R32G32B32A32_SFLOAT,								// VkFormat	format;
		DE_OFFSET_OF(VertexDataNdcBarycentric, positionNdc),		// deUint32	offset;
	};

	vertexDataDesc.vertexAttribDescVec.push_back(vertexAttribPositionNdc);

	const VkVertexInputAttributeDescription vertexAttrBarCoord =
	{
		1u,															// deUint32	location;
		0u,															// deUint32	binding;
		VK_FORMAT_R32G32B32_SFLOAT,									// VkFormat	format;
		DE_OFFSET_OF(VertexDataNdcBarycentric, barycentricCoord),	// deUint32	offset;
	};

	vertexDataDesc.vertexAttribDescVec.push_back(vertexAttrBarCoord);

	return vertexDataDesc;
}

template<> void MSInstance<MSInstanceInterpolateBarycentricCoordinates>::uploadVertexData (const Allocation& vertexBufferAllocation, const VertexDataDesc& vertexDataDescripton) const
{
	// Create buffer storing vertex data
	std::vector<VertexDataNdcBarycentric> vertices;

	vertices.push_back(VertexDataNdcBarycentric(tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f), tcu::Vec3(0.0f, 0.0f, 1.0f)));
	vertices.push_back(VertexDataNdcBarycentric(tcu::Vec4(-1.0f,  1.0f, 0.0f, 1.0f), tcu::Vec3(1.0f, 0.0f, 0.0f)));
	vertices.push_back(VertexDataNdcBarycentric(tcu::Vec4( 1.0f, -1.0f, 0.0f, 1.0f), tcu::Vec3(0.0f, 1.0f, 0.0f)));

	deMemcpy(vertexBufferAllocation.getHostPtr(), dataPointer(vertices), static_cast<std::size_t>(vertexDataDescripton.dataSize));
}

template<> tcu::TestStatus MSInstance<MSInstanceInterpolateBarycentricCoordinates>::verifyImageData (const vk::VkImageCreateInfo& imageRSInfo, const tcu::ConstPixelBufferAccess& dataRS) const
{
	if (checkForError(imageRSInfo, dataRS, 0))
		return tcu::TestStatus::fail("Failed");

	return tcu::TestStatus::pass("Passed");
}

class MSCaseCentroidQualifierInsidePrimitive;

template<> void MSCase<MSCaseCentroidQualifierInsidePrimitive>::init (void)
{
	m_testCtx.getLog()
		<< tcu::TestLog::Message
		<< "Verifying that varying qualified with centroid is interpolated at location inside both the pixel and the primitive being processed.\n"
		<< "	Interpolate triangle's barycentric coordinates with centroid qualifier.\n"
		<< "	=> After interpolation we expect barycentric.xyz >= 0.0 && barycentric.xyz <= 1.0\n"
		<< tcu::TestLog::EndMessage;

	MultisampleCaseBase::init();
}

template<> void MSCase<MSCaseCentroidQualifierInsidePrimitive>::initPrograms (vk::SourceCollections& programCollection) const
{
	// Create vertex shader
	std::ostringstream vs;

	vs << "#version 440\n"
		<< "layout(location = 0) in vec4 vs_in_position_ndc;\n"
		<< "layout(location = 1) in vec3 vs_in_barCoord;\n"
		<< "\n"
		<< "layout(location = 0) out vec3 vs_out_barCoord;\n"
		<< "\n"
		<< "out gl_PerVertex {\n"
		<< "	vec4  gl_Position;\n"
		<< "};\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	gl_Position		= vs_in_position_ndc;\n"
		<< "	vs_out_barCoord = vs_in_barCoord;\n"
		<< "}\n";

	programCollection.glslSources.add("vertex_shader") << glu::VertexSource(vs.str());

	// Create fragment shader
	std::ostringstream fs;

	fs << "#version 440\n"
		<< "layout(location = 0) centroid in vec3 fs_in_barCoord;\n"
		<< "\n"
		<< "layout(location = 0) out vec4 fs_out_color;\n"
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	if( all(greaterThanEqual(fs_in_barCoord, vec3(0.0))) && all(lessThanEqual(fs_in_barCoord, vec3(1.0))) )\n"
		<< "			fs_out_color = vec4(0.0, 1.0, 0.0, 1.0);\n"
		<< "	else\n"
		<< "			fs_out_color = vec4(1.0, 0.0, 0.0, 1.0);\n"
		<< "}\n";

	programCollection.glslSources.add("fragment_shader") << glu::FragmentSource(fs.str());
}

template<> void MSCase<MSCaseCentroidQualifierInsidePrimitive>::checkSupport (Context& context) const
{
	checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), m_imageMSParams.pipelineConstructionType);
}

template<> TestInstance* MSCase<MSCaseCentroidQualifierInsidePrimitive>::createInstance (Context& context) const
{
	return new MSInstance<MSInstanceInterpolateBarycentricCoordinates>(context, m_imageMSParams);
}

} // multisample

tcu::TestCaseGroup* createMultisampleInterpolationTests (tcu::TestContext& testCtx, vk::PipelineConstructionType pipelineConstructionType)
{
	de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "multisample_interpolation", "Multisample Interpolation"));

	const tcu::UVec3 imageSizes[] =
	{
		tcu::UVec3(128u, 128u, 1u),
		tcu::UVec3(137u, 191u, 1u),
	};

	const deUint32 sizesElemCount = static_cast<deUint32>(sizeof(imageSizes) / sizeof(tcu::UVec3));

	const vk::VkSampleCountFlagBits imageSamples[] =
	{
		vk::VK_SAMPLE_COUNT_2_BIT,
		vk::VK_SAMPLE_COUNT_4_BIT,
		vk::VK_SAMPLE_COUNT_8_BIT,
		vk::VK_SAMPLE_COUNT_16_BIT,
		vk::VK_SAMPLE_COUNT_32_BIT,
		vk::VK_SAMPLE_COUNT_64_BIT,
	};

	const deUint32 samplesElemCount = static_cast<deUint32>(sizeof(imageSamples) / sizeof(vk::VkSampleCountFlagBits));

	de::MovePtr<tcu::TestCaseGroup> caseGroup(new tcu::TestCaseGroup(testCtx, "sample_interpolate_at_single_sample", ""));

	for (deUint32 imageSizeNdx = 0u; imageSizeNdx < sizesElemCount; ++imageSizeNdx)
	{
		const tcu::UVec3	imageSize = imageSizes[imageSizeNdx];
		std::ostringstream	imageSizeStream;

		imageSizeStream << imageSize.x() << "_" << imageSize.y() << "_" << imageSize.z();

		de::MovePtr<tcu::TestCaseGroup> sizeGroup(new tcu::TestCaseGroup(testCtx, imageSizeStream.str().c_str(), ""));

		multisample::ImageMSParams imageParams
		{
			pipelineConstructionType,
			vk::VK_SAMPLE_COUNT_1_BIT,
			imageSize,
			multisample::ComponentData{},
			1.0f,
		};
		sizeGroup->addChild(multisample::MSCase<multisample::MSCaseInterpolateAtSampleSingleSample>::createCase(testCtx, "samples_" + de::toString(1), imageParams));

		caseGroup->addChild(sizeGroup.release());
	}

	testGroup->addChild(caseGroup.release());

	testGroup->addChild(makeMSGroup<multisample::MSCase<multisample::MSCaseInterpolateAtSampleDistinctValues> >	(testCtx, "sample_interpolate_at_distinct_values", pipelineConstructionType, imageSizes, sizesElemCount, imageSamples, samplesElemCount));
	testGroup->addChild(makeMSGroup<multisample::MSCase<multisample::MSCaseInterpolateAtSampleIgnoresCentroid> >(testCtx, "sample_interpolate_at_ignores_centroid", pipelineConstructionType, imageSizes, sizesElemCount, imageSamples, samplesElemCount));

	de::MovePtr<tcu::TestCaseGroup> sampleGroup(new tcu::TestCaseGroup(testCtx, "sample_interpolation_consistency", "Test consistency in sample interpolation function"));
	sampleGroup->addChild(makeMSGroup<multisample::MSCase<multisample::MSCaseInterpolateAtSampleConsistency> >	(testCtx, "all_components",		pipelineConstructionType, imageSizes, sizesElemCount, imageSamples, samplesElemCount));
	sampleGroup->addChild(makeMSGroup<multisample::MSCase<multisample::MSCaseInterpolateAtSampleConsistency> >	(testCtx, "component_0",		pipelineConstructionType, imageSizes, sizesElemCount, imageSamples, samplesElemCount, multisample::ComponentData{multisample::ComponentSource::CONSTANT, 0u}));
	sampleGroup->addChild(makeMSGroup<multisample::MSCase<multisample::MSCaseInterpolateAtSampleConsistency> >	(testCtx, "component_1",		pipelineConstructionType, imageSizes, sizesElemCount, imageSamples, samplesElemCount, multisample::ComponentData{multisample::ComponentSource::CONSTANT, 1u}));
	sampleGroup->addChild(makeMSGroup<multisample::MSCase<multisample::MSCaseInterpolateAtSampleConsistency> >	(testCtx, "pushc_component_0",	pipelineConstructionType, imageSizes, sizesElemCount, imageSamples, samplesElemCount, multisample::ComponentData{multisample::ComponentSource::PUSH_CONSTANT, 0u}));
	sampleGroup->addChild(makeMSGroup<multisample::MSCase<multisample::MSCaseInterpolateAtSampleConsistency> >	(testCtx, "pushc_component_1",	pipelineConstructionType, imageSizes, sizesElemCount, imageSamples, samplesElemCount, multisample::ComponentData{multisample::ComponentSource::PUSH_CONSTANT, 1u}));
	testGroup->addChild(sampleGroup.release());

	testGroup->addChild(makeMSGroup<multisample::MSCase<multisample::MSCaseSampleQualifierDistinctValues> >		(testCtx, "sample_qualifier_distinct_values", pipelineConstructionType, imageSizes, sizesElemCount, imageSamples, samplesElemCount));

	de::MovePtr<tcu::TestCaseGroup> centroidGroup(new tcu::TestCaseGroup(testCtx, "centroid_interpolation_consistency", "Test consistency in centroid interpolation function"));
	centroidGroup->addChild(makeMSGroup<multisample::MSCase<multisample::MSCaseInterpolateAtCentroidConsistency> >	(testCtx, "all_components",		pipelineConstructionType, imageSizes, sizesElemCount, imageSamples, samplesElemCount));
	centroidGroup->addChild(makeMSGroup<multisample::MSCase<multisample::MSCaseInterpolateAtCentroidConsistency> >	(testCtx, "component_0",		pipelineConstructionType, imageSizes, sizesElemCount, imageSamples, samplesElemCount, multisample::ComponentData{multisample::ComponentSource::CONSTANT, 0u}));
	centroidGroup->addChild(makeMSGroup<multisample::MSCase<multisample::MSCaseInterpolateAtCentroidConsistency> >	(testCtx, "component_1",		pipelineConstructionType, imageSizes, sizesElemCount, imageSamples, samplesElemCount, multisample::ComponentData{multisample::ComponentSource::CONSTANT, 1u}));
	centroidGroup->addChild(makeMSGroup<multisample::MSCase<multisample::MSCaseInterpolateAtCentroidConsistency> >	(testCtx, "pushc_component_0",	pipelineConstructionType, imageSizes, sizesElemCount, imageSamples, samplesElemCount, multisample::ComponentData{multisample::ComponentSource::PUSH_CONSTANT, 0u}));
	centroidGroup->addChild(makeMSGroup<multisample::MSCase<multisample::MSCaseInterpolateAtCentroidConsistency> >	(testCtx, "pushc_component_1",	pipelineConstructionType, imageSizes, sizesElemCount, imageSamples, samplesElemCount, multisample::ComponentData{multisample::ComponentSource::PUSH_CONSTANT, 1u}));
	testGroup->addChild(centroidGroup.release());

#ifndef CTS_USES_VULKANSC
	// there is no support for pipelineConstructionType in amber
	if (pipelineConstructionType == vk::PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
	{
		de::MovePtr<tcu::TestCaseGroup> reInterpolationGroup(new tcu::TestCaseGroup(testCtx, "reinterpolation_consistency", "Test consistency in reinterpolation"));
		std::vector<std::string> requirements;
		requirements.push_back("Features.sampleRateShading");
		reInterpolationGroup->addChild(cts_amber::createAmberTestCase(testCtx, "interpolate_at_centroid", "", "pipeline", "reinterpolate_at_centroid.amber", requirements));
		reInterpolationGroup->addChild(cts_amber::createAmberTestCase(testCtx, "interpolate_at_sample", "", "pipeline", "reinterpolate_at_sample.amber", requirements));
		testGroup->addChild(reInterpolationGroup.release());

		de::MovePtr<tcu::TestCaseGroup> nonuniformInterpolantIndexingGroup(new tcu::TestCaseGroup(testCtx, "nonuniform_interpolant_indexing", "Test nonuniform interpolant indexing"));
		std::vector<std::string> requirementsNonuniformIntepolantIndexing;
		requirementsNonuniformIntepolantIndexing.push_back("Features.sampleRateShading");
		nonuniformInterpolantIndexingGroup->addChild(cts_amber::createAmberTestCase(testCtx, "centroid", "", "pipeline/nonuniform_interpolant_indexing", "centroid.amber", requirementsNonuniformIntepolantIndexing));
		nonuniformInterpolantIndexingGroup->addChild(cts_amber::createAmberTestCase(testCtx, "sample", "", "pipeline/nonuniform_interpolant_indexing", "sample.amber", requirementsNonuniformIntepolantIndexing));
		nonuniformInterpolantIndexingGroup->addChild(cts_amber::createAmberTestCase(testCtx, "offset", "", "pipeline/nonuniform_interpolant_indexing", "offset.amber", requirementsNonuniformIntepolantIndexing));
		testGroup->addChild(nonuniformInterpolantIndexingGroup.release());
	}

	testGroup->addChild(makeMSGroup<multisample::MSCase<multisample::MSCaseCentroidQualifierInsidePrimitive> >	(testCtx, "centroid_qualifier_inside_primitive",	pipelineConstructionType, imageSizes, sizesElemCount, imageSamples, samplesElemCount));
#endif // CTS_USES_VULKANSC
	testGroup->addChild(makeMSGroup<multisample::MSCase<multisample::MSCaseInterpolateAtOffsetPixelCenter> >	(testCtx, "offset_interpolate_at_pixel_center",		pipelineConstructionType, imageSizes, sizesElemCount, imageSamples, samplesElemCount));

	de::MovePtr<tcu::TestCaseGroup> offsetGroup(new tcu::TestCaseGroup(testCtx, "offset_interpolation_at_sample_position", "Test interpolation at offset function works for sample positions"));
	offsetGroup->addChild(makeMSGroup<multisample::MSCase<multisample::MSCaseInterpolateAtOffsetSamplePosition> >	(testCtx, "all_components",		pipelineConstructionType, imageSizes, sizesElemCount, imageSamples, samplesElemCount));
	offsetGroup->addChild(makeMSGroup<multisample::MSCase<multisample::MSCaseInterpolateAtOffsetSamplePosition> >	(testCtx, "component_0",		pipelineConstructionType, imageSizes, sizesElemCount, imageSamples, samplesElemCount, multisample::ComponentData{multisample::ComponentSource::CONSTANT, 0u}));
	offsetGroup->addChild(makeMSGroup<multisample::MSCase<multisample::MSCaseInterpolateAtOffsetSamplePosition> >	(testCtx, "component_1",		pipelineConstructionType, imageSizes, sizesElemCount, imageSamples, samplesElemCount, multisample::ComponentData{multisample::ComponentSource::CONSTANT, 1u}));
	offsetGroup->addChild(makeMSGroup<multisample::MSCase<multisample::MSCaseInterpolateAtOffsetSamplePosition> >	(testCtx, "pushc_component_0",	pipelineConstructionType, imageSizes, sizesElemCount, imageSamples, samplesElemCount, multisample::ComponentData{multisample::ComponentSource::PUSH_CONSTANT, 0u}));
	offsetGroup->addChild(makeMSGroup<multisample::MSCase<multisample::MSCaseInterpolateAtOffsetSamplePosition> >	(testCtx, "pushc_component_1",	pipelineConstructionType, imageSizes, sizesElemCount, imageSamples, samplesElemCount, multisample::ComponentData{multisample::ComponentSource::PUSH_CONSTANT, 1u}));
	testGroup->addChild(offsetGroup.release());

	return testGroup.release();
}

} // pipeline
} // vkt
