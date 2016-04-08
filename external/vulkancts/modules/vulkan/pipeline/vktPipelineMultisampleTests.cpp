/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Imagination Technologies Ltd.
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
 * \brief Multisample Tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineMultisampleTests.hpp"
#include "vktPipelineClearUtil.hpp"
#include "vktPipelineImageUtil.hpp"
#include "vktPipelineVertexUtil.hpp"
#include "vktPipelineReferenceRenderer.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "tcuImageCompare.hpp"
#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"
#include "deMemory.h"

#include <sstream>
#include <vector>
#include <map>

namespace vkt
{
namespace pipeline
{

using namespace vk;

namespace
{
enum GeometryType
{
	GEOMETRY_TYPE_OPAQUE_TRIANGLE,
	GEOMETRY_TYPE_OPAQUE_LINE,
	GEOMETRY_TYPE_OPAQUE_POINT,
	GEOMETRY_TYPE_OPAQUE_QUAD,
	GEOMETRY_TYPE_TRANSLUCENT_QUAD,
	GEOMETRY_TYPE_INVISIBLE_QUAD,
	GEOMETRY_TYPE_GRADIENT_QUAD
};


bool									isSupportedSampleCount				(const InstanceInterface& instanceInterface, VkPhysicalDevice physicalDevice, VkSampleCountFlagBits rasterizationSamples);
VkPipelineColorBlendAttachmentState		getDefaultColorBlendAttachmentState	(void);
deUint32								getUniqueColorsCount				(const tcu::ConstPixelBufferAccess& image);
void									initMultisamplePrograms				(SourceCollections& sources, GeometryType geometryType);

class MultisampleTest : public vkt::TestCase
{
public:

												MultisampleTest						(tcu::TestContext&								testContext,
																					 const std::string&								name,
																					 const std::string&								description,
																					 const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
																					 const VkPipelineColorBlendAttachmentState&		blendState,
																					 GeometryType									geometryType);
	virtual										~MultisampleTest					(void);

	virtual void								initPrograms						(SourceCollections& programCollection) const;
	virtual TestInstance*						createInstance						(Context& context) const;

protected:
	virtual TestInstance*						createMultisampleTestInstance		(Context&										context,
																					 VkPrimitiveTopology							topology,
																					 const std::vector<Vertex4RGBA>&				vertices,
																					 const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
																					 const VkPipelineColorBlendAttachmentState&		colorBlendState) const = 0;
	VkPipelineMultisampleStateCreateInfo		m_multisampleStateParams;
	const VkPipelineColorBlendAttachmentState	m_colorBlendState;
	const GeometryType							m_geometryType;
	std::vector<VkSampleMask>					m_sampleMask;
};

class RasterizationSamplesTest : public MultisampleTest
{
public:
												RasterizationSamplesTest			(tcu::TestContext&		testContext,
																					 const std::string&		name,
																					 const std::string&		description,
																					 VkSampleCountFlagBits	rasterizationSamples,
																					 GeometryType			geometryType);
	virtual										~RasterizationSamplesTest			(void) {}

protected:
	virtual TestInstance*						createMultisampleTestInstance		(Context&										context,
																					 VkPrimitiveTopology							topology,
																					 const std::vector<Vertex4RGBA>&				vertices,
																					 const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
																					 const VkPipelineColorBlendAttachmentState&		colorBlendState) const;

	static VkPipelineMultisampleStateCreateInfo	getRasterizationSamplesStateParams	(VkSampleCountFlagBits rasterizationSamples);
};

class MinSampleShadingTest : public MultisampleTest
{
public:
												MinSampleShadingTest				(tcu::TestContext&		testContext,
																					 const std::string&		name,
																					 const std::string&		description,
																					 VkSampleCountFlagBits	rasterizationSamples,
																					 float					minSampleShading,
																					 GeometryType			geometryType);
	virtual										~MinSampleShadingTest				(void) {}

protected:
	virtual TestInstance*						createMultisampleTestInstance		(Context&										context,
																					 VkPrimitiveTopology							topology,
																					 const std::vector<Vertex4RGBA>&				vertices,
																					 const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
																					 const VkPipelineColorBlendAttachmentState&		colorBlendState) const;

	static VkPipelineMultisampleStateCreateInfo	getMinSampleShadingStateParams		(VkSampleCountFlagBits rasterizationSamples, float minSampleShading);
};

class SampleMaskTest : public MultisampleTest
{
public:
												SampleMaskTest						(tcu::TestContext&					testContext,
																					 const std::string&					name,
																					 const std::string&					description,
																					 VkSampleCountFlagBits				rasterizationSamples,
																					 const std::vector<VkSampleMask>&	sampleMask,
																					 GeometryType						geometryType);

	virtual										~SampleMaskTest						(void) {}

protected:
	virtual TestInstance*						createMultisampleTestInstance		(Context&										context,
																					 VkPrimitiveTopology							topology,
																					 const std::vector<Vertex4RGBA>&				vertices,
																					 const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
																					 const VkPipelineColorBlendAttachmentState&		colorBlendState) const;

	static VkPipelineMultisampleStateCreateInfo	getSampleMaskStateParams			(VkSampleCountFlagBits rasterizationSamples, const std::vector<VkSampleMask>& sampleMask);
};

class AlphaToOneTest : public MultisampleTest
{
public:
												AlphaToOneTest					(tcu::TestContext&					testContext,
																				 const std::string&					name,
																				 const std::string&					description,
																				 VkSampleCountFlagBits				rasterizationSamples);

	virtual										~AlphaToOneTest					(void) {}

protected:
	virtual TestInstance*						createMultisampleTestInstance	(Context&										context,
																				 VkPrimitiveTopology							topology,
																				 const std::vector<Vertex4RGBA>&				vertices,
																				 const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
																				 const VkPipelineColorBlendAttachmentState&		colorBlendState) const;

	static VkPipelineMultisampleStateCreateInfo	getAlphaToOneStateParams		(VkSampleCountFlagBits rasterizationSamples);
	static VkPipelineColorBlendAttachmentState	getAlphaToOneBlendState			(void);
};

class AlphaToCoverageTest : public MultisampleTest
{
public:
												AlphaToCoverageTest				(tcu::TestContext&		testContext,
																				 const std::string&		name,
																				 const std::string&		description,
																				 VkSampleCountFlagBits	rasterizationSamples,
																				 GeometryType			geometryType);

	virtual										~AlphaToCoverageTest			(void) {}

protected:
	virtual TestInstance*						createMultisampleTestInstance	(Context&										context,
																				 VkPrimitiveTopology							topology,
																				 const std::vector<Vertex4RGBA>&				vertices,
																				 const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
																				 const VkPipelineColorBlendAttachmentState&		colorBlendState) const;

	static VkPipelineMultisampleStateCreateInfo	getAlphaToCoverageStateParams	(VkSampleCountFlagBits rasterizationSamples);

	GeometryType								m_geometryType;
};

class MultisampleRenderer
{
public:
												MultisampleRenderer			(Context&										context,
																			 VkFormat										colorFormat,
																			 const tcu::IVec2&								renderSize,
																			 VkPrimitiveTopology							topology,
																			 const std::vector<Vertex4RGBA>&				vertices,
																			 const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
																			 const VkPipelineColorBlendAttachmentState&		blendState);

	virtual										~MultisampleRenderer		(void);

	de::MovePtr<tcu::TextureLevel>				render						(void);

protected:
	Context&									m_context;

	const VkFormat								m_colorFormat;
	tcu::IVec2									m_renderSize;

	const VkPipelineMultisampleStateCreateInfo	m_multisampleStateParams;
	const VkPipelineColorBlendAttachmentState	m_colorBlendState;

	Move<VkImage>								m_colorImage;
	de::MovePtr<Allocation>						m_colorImageAlloc;
	Move<VkImageView>							m_colorAttachmentView;

	Move<VkImage>								m_resolveImage;
	de::MovePtr<Allocation>						m_resolveImageAlloc;
	Move<VkImageView>							m_resolveAttachmentView;

	Move<VkRenderPass>							m_renderPass;
	Move<VkFramebuffer>							m_framebuffer;

	Move<VkShaderModule>						m_vertexShaderModule;
	Move<VkShaderModule>						m_fragmentShaderModule;

	Move<VkBuffer>								m_vertexBuffer;
	de::MovePtr<Allocation>						m_vertexBufferAlloc;

	Move<VkPipelineLayout>						m_pipelineLayout;
	Move<VkPipeline>							m_graphicsPipeline;

	Move<VkCommandPool>							m_cmdPool;
	Move<VkCommandBuffer>						m_cmdBuffer;

	Move<VkFence>								m_fence;
};

class RasterizationSamplesInstance : public vkt::TestInstance
{
public:
									RasterizationSamplesInstance	(Context&										context,
																	 VkPrimitiveTopology							topology,
																	 const std::vector<Vertex4RGBA>&				vertices,
																	 const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
																	 const VkPipelineColorBlendAttachmentState&		blendState);
	virtual							~RasterizationSamplesInstance	(void) {}

	virtual tcu::TestStatus			iterate							(void);

protected:
	virtual tcu::TestStatus			verifyImage						(const tcu::ConstPixelBufferAccess& result);

	const VkFormat					m_colorFormat;
	const tcu::IVec2				m_renderSize;
	const VkPrimitiveTopology		m_primitiveTopology;
	const std::vector<Vertex4RGBA>	m_vertices;
	MultisampleRenderer				m_multisampleRenderer;
};

class MinSampleShadingInstance : public vkt::TestInstance
{
public:
												MinSampleShadingInstance	(Context&										context,
																			 VkPrimitiveTopology							topology,
																			 const std::vector<Vertex4RGBA>&				vertices,
																			 const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
																			 const VkPipelineColorBlendAttachmentState&		blendState);
	virtual										~MinSampleShadingInstance	(void) {}

	virtual tcu::TestStatus						iterate						(void);

protected:
	virtual tcu::TestStatus						verifyImage					(const tcu::ConstPixelBufferAccess& testShadingImage,
																			 const tcu::ConstPixelBufferAccess& minShadingImage,
																			 const tcu::ConstPixelBufferAccess& maxShadingImage);
	const VkFormat								m_colorFormat;
	const tcu::IVec2							m_renderSize;
	const VkPrimitiveTopology					m_primitiveTopology;
	const std::vector<Vertex4RGBA>				m_vertices;
	const VkPipelineMultisampleStateCreateInfo	m_multisampleStateParams;
	const VkPipelineColorBlendAttachmentState	m_colorBlendState;
};

class SampleMaskInstance : public vkt::TestInstance
{
public:
												SampleMaskInstance			(Context&										context,
																			 VkPrimitiveTopology							topology,
																			 const std::vector<Vertex4RGBA>&				vertices,
																			 const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
																			 const VkPipelineColorBlendAttachmentState&		blendState);
	virtual										~SampleMaskInstance			(void) {}

	virtual tcu::TestStatus						iterate						(void);

protected:
	virtual tcu::TestStatus						verifyImage					(const tcu::ConstPixelBufferAccess& testShadingImage,
																			 const tcu::ConstPixelBufferAccess& minShadingImage,
																			 const tcu::ConstPixelBufferAccess& maxShadingImage);
	const VkFormat								m_colorFormat;
	const tcu::IVec2							m_renderSize;
	const VkPrimitiveTopology					m_primitiveTopology;
	const std::vector<Vertex4RGBA>				m_vertices;
	const VkPipelineMultisampleStateCreateInfo	m_multisampleStateParams;
	const VkPipelineColorBlendAttachmentState	m_colorBlendState;
};

class AlphaToOneInstance : public vkt::TestInstance
{
public:
												AlphaToOneInstance			(Context&										context,
																			 VkPrimitiveTopology							topology,
																			 const std::vector<Vertex4RGBA>&				vertices,
																			 const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
																			 const VkPipelineColorBlendAttachmentState&		blendState);
	virtual										~AlphaToOneInstance			(void) {}

	virtual tcu::TestStatus						iterate						(void);

protected:
	virtual tcu::TestStatus						verifyImage					(const tcu::ConstPixelBufferAccess& alphaOneImage,
																			 const tcu::ConstPixelBufferAccess& noAlphaOneImage);
	const VkFormat								m_colorFormat;
	const tcu::IVec2							m_renderSize;
	const VkPrimitiveTopology					m_primitiveTopology;
	const std::vector<Vertex4RGBA>				m_vertices;
	const VkPipelineMultisampleStateCreateInfo	m_multisampleStateParams;
	const VkPipelineColorBlendAttachmentState	m_colorBlendState;
};

class AlphaToCoverageInstance : public vkt::TestInstance
{
public:
												AlphaToCoverageInstance		(Context&										context,
																			 VkPrimitiveTopology							topology,
																			 const std::vector<Vertex4RGBA>&				vertices,
																			 const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
																			 const VkPipelineColorBlendAttachmentState&		blendState,
																			 GeometryType									geometryType);
	virtual										~AlphaToCoverageInstance	(void) {}

	virtual tcu::TestStatus						iterate						(void);

protected:
	virtual tcu::TestStatus						verifyImage					(const tcu::ConstPixelBufferAccess& result);
	const VkFormat								m_colorFormat;
	const tcu::IVec2							m_renderSize;
	const VkPrimitiveTopology					m_primitiveTopology;
	const std::vector<Vertex4RGBA>				m_vertices;
	const VkPipelineMultisampleStateCreateInfo	m_multisampleStateParams;
	const VkPipelineColorBlendAttachmentState	m_colorBlendState;
	const GeometryType							m_geometryType;
};


// Helper functions

void initMultisamplePrograms (SourceCollections& sources, GeometryType geometryType)
{
	std::ostringstream vertexSource;

	vertexSource <<
		"#version 310 es\n"
		"layout(location = 0) in vec4 position;\n"
		"layout(location = 1) in vec4 color;\n"
		"layout(location = 0) out highp vec4 vtxColor;\n"
		"void main (void)\n"
		"{\n"
		"	gl_Position = position;\n"
		"	vtxColor = color;\n"
		<< (geometryType == GEOMETRY_TYPE_OPAQUE_POINT ? "	gl_PointSize = 3.0f;\n"
														 : "" )
		<< "}\n";

	static const char* fragmentSource =
		"#version 310 es\n"
		"layout(location = 0) in highp vec4 vtxColor;\n"
		"layout(location = 0) out highp vec4 fragColor;\n"
		"void main (void)\n"
		"{\n"
		"	fragColor = vtxColor;\n"
		"}\n";

	sources.glslSources.add("color_vert") << glu::VertexSource(vertexSource.str());
	sources.glslSources.add("color_frag") << glu::FragmentSource(fragmentSource);
}

bool isSupportedSampleCount (const InstanceInterface& instanceInterface, VkPhysicalDevice physicalDevice, VkSampleCountFlagBits rasterizationSamples)
{
	VkPhysicalDeviceProperties deviceProperties;

	instanceInterface.getPhysicalDeviceProperties(physicalDevice, &deviceProperties);

	return !!(deviceProperties.limits.framebufferColorSampleCounts & rasterizationSamples);
}

VkPipelineColorBlendAttachmentState getDefaultColorBlendAttachmentState (void)
{
	const VkPipelineColorBlendAttachmentState colorBlendState =
	{
		false,														// VkBool32					blendEnable;
		VK_BLEND_FACTOR_ONE,										// VkBlendFactor			srcColorBlendFactor;
		VK_BLEND_FACTOR_ZERO,										// VkBlendFactor			dstColorBlendFactor;
		VK_BLEND_OP_ADD,											// VkBlendOp				colorBlendOp;
		VK_BLEND_FACTOR_ONE,										// VkBlendFactor			srcAlphaBlendFactor;
		VK_BLEND_FACTOR_ZERO,										// VkBlendFactor			dstAlphaBlendFactor;
		VK_BLEND_OP_ADD,											// VkBlendOp				alphaBlendOp;
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |		// VkColorComponentFlags	colorWriteMask;
			VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
	};

	return colorBlendState;
}

deUint32 getUniqueColorsCount (const tcu::ConstPixelBufferAccess& image)
{
	DE_ASSERT(image.getFormat().getPixelSize() == 4);

	std::map<deUint32, deUint32>	histogram; // map<pixel value, number of occurrences>
	const deUint32					pixelCount	= image.getWidth() * image.getHeight() * image.getDepth();

	for (deUint32 pixelNdx = 0; pixelNdx < pixelCount; pixelNdx++)
	{
		const deUint32 pixelValue = *((const deUint32*)image.getDataPtr() + pixelNdx);

		if (histogram.find(pixelValue) != histogram.end())
			histogram[pixelValue]++;
		else
			histogram[pixelValue] = 1;
	}

	return (deUint32)histogram.size();
}


// MultisampleTest

MultisampleTest::MultisampleTest (tcu::TestContext&								testContext,
								  const std::string&							name,
								  const std::string&							description,
								  const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
								  const VkPipelineColorBlendAttachmentState&	blendState,
								  GeometryType									geometryType)
	: vkt::TestCase				(testContext, name, description)
	, m_multisampleStateParams	(multisampleStateParams)
	, m_colorBlendState			(blendState)
	, m_geometryType			(geometryType)
{
	if (m_multisampleStateParams.pSampleMask)
	{
		// Copy pSampleMask to avoid dependencies with other classes

		const deUint32 maskCount = deCeilFloatToInt32(float(m_multisampleStateParams.rasterizationSamples) / 32);

		for (deUint32 maskNdx = 0; maskNdx < maskCount; maskNdx++)
			m_sampleMask.push_back(m_multisampleStateParams.pSampleMask[maskNdx]);

		m_multisampleStateParams.pSampleMask = m_sampleMask.data();
	}
}

MultisampleTest::~MultisampleTest (void)
{
}

void MultisampleTest::initPrograms (SourceCollections& programCollection) const
{
	initMultisamplePrograms(programCollection, m_geometryType);
}

TestInstance* MultisampleTest::createInstance (Context& context) const
{
	VkPrimitiveTopology			topology;
	std::vector<Vertex4RGBA>	vertices;

	switch (m_geometryType)
	{
		case GEOMETRY_TYPE_OPAQUE_TRIANGLE:
		{
			topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			const Vertex4RGBA vertexData[3] =
			{
				{
					tcu::Vec4(-0.75f, 0.0f, 0.0f, 1.0f),
					tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f)
				},
				{
					tcu::Vec4(0.75f, 0.125f, 0.0f, 1.0f),
					tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f)
				},
				{
					tcu::Vec4(0.75f, -0.125f, 0.0f, 1.0f),
					tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f)
				}
			};

			vertices = std::vector<Vertex4RGBA>(vertexData, vertexData + 3);
			break;
		}

		case GEOMETRY_TYPE_OPAQUE_LINE:
		{
			topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

			const Vertex4RGBA vertexData[2] =
			{
				{
					tcu::Vec4(-0.75f, 0.25f, 0.0f, 1.0f),
					tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f)
				},
				{
					tcu::Vec4(0.75f, -0.25f, 0.0f, 1.0f),
					tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f)
				}
			};

			vertices = std::vector<Vertex4RGBA>(vertexData, vertexData + 2);
			break;
		}

		case GEOMETRY_TYPE_OPAQUE_POINT:
		{
			topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

			const Vertex4RGBA vertex =
			{
				tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f),
				tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f)
			};

			vertices = std::vector<Vertex4RGBA>(1, vertex);
			break;
		}

		case GEOMETRY_TYPE_OPAQUE_QUAD:
		case GEOMETRY_TYPE_TRANSLUCENT_QUAD:
		case GEOMETRY_TYPE_INVISIBLE_QUAD:
		case GEOMETRY_TYPE_GRADIENT_QUAD:
		{
			topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

			Vertex4RGBA vertexData[4] =
			{
				{
					tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f),
					tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f)
				},
				{
					tcu::Vec4(1.0f, -1.0f, 0.0f, 1.0f),
					tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f)
				},
				{
					tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f),
					tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f)
				},
				{
					tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f),
					tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f)
				}
			};

			if (m_geometryType == GEOMETRY_TYPE_TRANSLUCENT_QUAD)
			{
				for (int i = 0; i < 4; i++)
					vertexData[i].color.w() = 0.25f;
			}
			else if (m_geometryType == GEOMETRY_TYPE_INVISIBLE_QUAD)
			{
				for (int i = 0; i < 4; i++)
					vertexData[i].color.w() = 0.0f;
			}
			else if (m_geometryType == GEOMETRY_TYPE_GRADIENT_QUAD)
			{
				vertexData[0].color.w() = 0.0f;
				vertexData[2].color.w() = 0.0f;
			}

			vertices = std::vector<Vertex4RGBA>(vertexData, vertexData + 4);
			break;
		}

		default:
			topology = VK_PRIMITIVE_TOPOLOGY_LAST;
			DE_ASSERT(false);
	}

	return createMultisampleTestInstance(context, topology, vertices, m_multisampleStateParams, m_colorBlendState);
}


// RasterizationSamplesTest

RasterizationSamplesTest::RasterizationSamplesTest (tcu::TestContext&		testContext,
													const std::string&		name,
													const std::string&		description,
													VkSampleCountFlagBits	rasterizationSamples,
													GeometryType			geometryType)
	: MultisampleTest	(testContext, name, description, getRasterizationSamplesStateParams(rasterizationSamples), getDefaultColorBlendAttachmentState(), geometryType)
{
}

VkPipelineMultisampleStateCreateInfo RasterizationSamplesTest::getRasterizationSamplesStateParams (VkSampleCountFlagBits rasterizationSamples)
{
	const VkPipelineMultisampleStateCreateInfo multisampleStateParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,													// const void*								pNext;
		0u,															// VkPipelineMultisampleStateCreateFlags	flags;
		rasterizationSamples,										// VkSampleCountFlagBits					rasterizationSamples;
		false,														// VkBool32									sampleShadingEnable;
		0.0f,														// float									minSampleShading;
		DE_NULL,													// const VkSampleMask*						pSampleMask;
		false,														// VkBool32									alphaToCoverageEnable;
		false														// VkBool32									alphaToOneEnable;
	};

	return multisampleStateParams;
}

TestInstance* RasterizationSamplesTest::createMultisampleTestInstance (Context&										context,
																	   VkPrimitiveTopology							topology,
																	   const std::vector<Vertex4RGBA>&				vertices,
																	   const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
																	   const VkPipelineColorBlendAttachmentState&	colorBlendState) const
{
	return new RasterizationSamplesInstance(context, topology, vertices, multisampleStateParams, colorBlendState);
}


// MinSampleShadingTest

MinSampleShadingTest::MinSampleShadingTest (tcu::TestContext&		testContext,
											const std::string&		name,
											const std::string&		description,
											VkSampleCountFlagBits	rasterizationSamples,
											float					minSampleShading,
											GeometryType			geometryType)
	: MultisampleTest	(testContext, name, description, getMinSampleShadingStateParams(rasterizationSamples, minSampleShading), getDefaultColorBlendAttachmentState(), geometryType)
{
}

TestInstance* MinSampleShadingTest::createMultisampleTestInstance (Context&										context,
																   VkPrimitiveTopology							topology,
																   const std::vector<Vertex4RGBA>&				vertices,
																   const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
																   const VkPipelineColorBlendAttachmentState&	colorBlendState) const
{
	return new MinSampleShadingInstance(context, topology, vertices, multisampleStateParams, colorBlendState);
}

VkPipelineMultisampleStateCreateInfo MinSampleShadingTest::getMinSampleShadingStateParams (VkSampleCountFlagBits rasterizationSamples, float minSampleShading)
{
	const VkPipelineMultisampleStateCreateInfo multisampleStateParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,													// const void*								pNext;
		0u,															// VkPipelineMultisampleStateCreateFlags	flags;
		rasterizationSamples,										// VkSampleCountFlagBits					rasterizationSamples;
		true,														// VkBool32									sampleShadingEnable;
		minSampleShading,											// float									minSampleShading;
		DE_NULL,													// const VkSampleMask*						pSampleMask;
		false,														//  VkBool32								alphaToCoverageEnable;
		false														//  VkBool32								alphaToOneEnable;
	};

	return multisampleStateParams;
}


// SampleMaskTest

SampleMaskTest::SampleMaskTest (tcu::TestContext&					testContext,
								const std::string&					name,
								const std::string&					description,
								VkSampleCountFlagBits				rasterizationSamples,
								const std::vector<VkSampleMask>&	sampleMask,
								GeometryType						geometryType)
	: MultisampleTest	(testContext, name, description, getSampleMaskStateParams(rasterizationSamples, sampleMask), getDefaultColorBlendAttachmentState(), geometryType)
{
}

TestInstance* SampleMaskTest::createMultisampleTestInstance (Context&										context,
															 VkPrimitiveTopology							topology,
															 const std::vector<Vertex4RGBA>&				vertices,
															 const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
															 const VkPipelineColorBlendAttachmentState&		colorBlendState) const
{
	return new SampleMaskInstance(context, topology,vertices, multisampleStateParams, colorBlendState);
}

VkPipelineMultisampleStateCreateInfo SampleMaskTest::getSampleMaskStateParams (VkSampleCountFlagBits rasterizationSamples, const std::vector<VkSampleMask>& sampleMask)
{
	const VkPipelineMultisampleStateCreateInfo multisampleStateParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,													// const void*								pNext;
		0u,															// VkPipelineMultisampleStateCreateFlags	flags;
		rasterizationSamples,										// VkSampleCountFlagBits					rasterizationSamples;
		false,														// VkBool32									sampleShadingEnable;
		0.0f,														// float									minSampleShading;
		sampleMask.data(),											// const VkSampleMask*						pSampleMask;
		false,														// VkBool32									alphaToCoverageEnable;
		false														// VkBool32									alphaToOneEnable;
	};

	return multisampleStateParams;
}


// AlphaToOneTest

AlphaToOneTest::AlphaToOneTest (tcu::TestContext&		testContext,
								const std::string&		name,
								const std::string&		description,
								VkSampleCountFlagBits	rasterizationSamples)
	: MultisampleTest	(testContext, name, description, getAlphaToOneStateParams(rasterizationSamples), getAlphaToOneBlendState(), GEOMETRY_TYPE_GRADIENT_QUAD)
{
}

TestInstance* AlphaToOneTest::createMultisampleTestInstance (Context&										context,
															 VkPrimitiveTopology							topology,
															 const std::vector<Vertex4RGBA>&				vertices,
															 const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
															 const VkPipelineColorBlendAttachmentState&		colorBlendState) const
{
	return new AlphaToOneInstance(context, topology, vertices, multisampleStateParams, colorBlendState);
}

VkPipelineMultisampleStateCreateInfo AlphaToOneTest::getAlphaToOneStateParams (VkSampleCountFlagBits rasterizationSamples)
{
	const VkPipelineMultisampleStateCreateInfo multisampleStateParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,													// const void*								pNext;
		0u,															// VkPipelineMultisampleStateCreateFlags	flags;
		rasterizationSamples,										// VkSampleCountFlagBits					rasterizationSamples;
		false,														// VkBool32									sampleShadingEnable;
		0.0f,														// float									minSampleShading;
		DE_NULL,													// const VkSampleMask*						pSampleMask;
		false,														// VkBool32									alphaToCoverageEnable;
		true														// VkBool32									alphaToOneEnable;
	};

	return multisampleStateParams;
}

VkPipelineColorBlendAttachmentState AlphaToOneTest::getAlphaToOneBlendState (void)
{
	const VkPipelineColorBlendAttachmentState colorBlendState =
	{
		true,														// VkBool32					blendEnable;
		VK_BLEND_FACTOR_SRC_ALPHA,									// VkBlendFactor			srcColorBlendFactor;
		VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,						// VkBlendFactor			dstColorBlendFactor;
		VK_BLEND_OP_ADD,											// VkBlendOp				colorBlendOp;
		VK_BLEND_FACTOR_SRC_ALPHA,									// VkBlendFactor			srcAlphaBlendFactor;
		VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,						// VkBlendFactor			dstAlphaBlendFactor;
		VK_BLEND_OP_ADD,											// VkBlendOp				alphaBlendOp;
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |		// VkColorComponentFlags	colorWriteMask;
			VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
	};

	return colorBlendState;
}


// AlphaToCoverageTest

AlphaToCoverageTest::AlphaToCoverageTest (tcu::TestContext&			testContext,
										  const std::string&		name,
										  const std::string&		description,
										  VkSampleCountFlagBits		rasterizationSamples,
										  GeometryType				geometryType)
	: MultisampleTest	(testContext, name, description, getAlphaToCoverageStateParams(rasterizationSamples), getDefaultColorBlendAttachmentState(), geometryType)
	, m_geometryType	(geometryType)
{
}

TestInstance* AlphaToCoverageTest::createMultisampleTestInstance (Context&										context,
																  VkPrimitiveTopology							topology,
																  const std::vector<Vertex4RGBA>&				vertices,
																  const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
																  const VkPipelineColorBlendAttachmentState&	colorBlendState) const
{
	return new AlphaToCoverageInstance(context, topology, vertices, multisampleStateParams, colorBlendState, m_geometryType);
}

VkPipelineMultisampleStateCreateInfo AlphaToCoverageTest::getAlphaToCoverageStateParams (VkSampleCountFlagBits rasterizationSamples)
{
	const VkPipelineMultisampleStateCreateInfo multisampleStateParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,													// const void*								pNext;
		0u,															// VkPipelineMultisampleStateCreateFlags	flags;
		rasterizationSamples,										// VkSampleCountFlagBits					rasterizationSamples;
		false,														// VkBool32									sampleShadingEnable;
		0.0f,														// float									minSampleShading;
		DE_NULL,													// const VkSampleMask*						pSampleMask;
		true,														// VkBool32									alphaToCoverageEnable;
		false														// VkBool32									alphaToOneEnable;
	};

	return multisampleStateParams;
}

// RasterizationSamplesInstance

RasterizationSamplesInstance::RasterizationSamplesInstance (Context&										context,
															VkPrimitiveTopology								topology,
															const std::vector<Vertex4RGBA>&					vertices,
															const VkPipelineMultisampleStateCreateInfo&		multisampleStateParams,
															const VkPipelineColorBlendAttachmentState&		blendState)
	: vkt::TestInstance		(context)
	, m_colorFormat			(VK_FORMAT_R8G8B8A8_UNORM)
	, m_renderSize			(32, 32)
	, m_primitiveTopology	(topology)
	, m_vertices			(vertices)
	, m_multisampleRenderer	(context, m_colorFormat, m_renderSize, topology, vertices, multisampleStateParams, blendState)
{
}

tcu::TestStatus RasterizationSamplesInstance::iterate (void)
{
	de::MovePtr<tcu::TextureLevel> level(m_multisampleRenderer.render());
	return verifyImage(level->getAccess());
}

tcu::TestStatus RasterizationSamplesInstance::verifyImage (const tcu::ConstPixelBufferAccess& result)
{
	// Verify range of unique pixels
	{
		const deUint32	numUniqueColors = getUniqueColorsCount(result);
		const deUint32	minUniqueColors	= 3;

		tcu::TestLog& log = m_context.getTestContext().getLog();

		log << tcu::TestLog::Message
			<< "\nMin. unique colors expected: " << minUniqueColors << "\n"
			<< "Unique colors found: " << numUniqueColors << "\n"
			<< tcu::TestLog::EndMessage;

		if (numUniqueColors < minUniqueColors)
			return tcu::TestStatus::fail("Unique colors out of expected bounds");
	}

	// Verify shape of the rendered primitive (fuzzy-compare)
	{
		const tcu::TextureFormat	tcuColorFormat	= mapVkFormat(m_colorFormat);
		const tcu::TextureFormat	tcuDepthFormat	= tcu::TextureFormat();
		const ColorVertexShader		vertexShader;
		const ColorFragmentShader	fragmentShader	(tcuColorFormat, tcuDepthFormat);
		const rr::Program			program			(&vertexShader, &fragmentShader);
		ReferenceRenderer			refRenderer		(m_renderSize.x(), m_renderSize.y(), 1, tcuColorFormat, tcuDepthFormat, &program);
		rr::RenderState				renderState		(refRenderer.getViewportState());

		if (m_primitiveTopology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST)
		{
			VkPhysicalDeviceProperties deviceProperties;

			m_context.getInstanceInterface().getPhysicalDeviceProperties(m_context.getPhysicalDevice(), &deviceProperties);

			// gl_PointSize is clamped to pointSizeRange
			renderState.point.pointSize = deFloatMin(3.0f, deviceProperties.limits.pointSizeRange[1]);
		}

		refRenderer.colorClear(tcu::Vec4(0.0f));
		refRenderer.draw(renderState, mapVkPrimitiveTopology(m_primitiveTopology), m_vertices);

		if (!tcu::fuzzyCompare(m_context.getTestContext().getLog(), "FuzzyImageCompare", "Image comparison", refRenderer.getAccess(), result, 0.05f, tcu::COMPARE_LOG_RESULT))
			return tcu::TestStatus::fail("Primitive has unexpected shape");

	}

	return tcu::TestStatus::pass("Primitive rendered, unique colors within expected bounds");
}


// MinSampleShadingInstance

MinSampleShadingInstance::MinSampleShadingInstance (Context&									context,
													VkPrimitiveTopology							topology,
													const std::vector<Vertex4RGBA>&				vertices,
													const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
													const VkPipelineColorBlendAttachmentState&	colorBlendState)
	: vkt::TestInstance			(context)
	, m_colorFormat				(VK_FORMAT_R8G8B8A8_UNORM)
	, m_renderSize				(32, 32)
	, m_primitiveTopology		(topology)
	, m_vertices				(vertices)
	, m_multisampleStateParams	(multisampleStateParams)
	, m_colorBlendState			(colorBlendState)
{
	VkPhysicalDeviceFeatures deviceFeatures;

	m_context.getInstanceInterface().getPhysicalDeviceFeatures(m_context.getPhysicalDevice(), &deviceFeatures);

	if (!deviceFeatures.sampleRateShading)
		throw tcu::NotSupportedError("Sample shading is not supported");
}

tcu::TestStatus MinSampleShadingInstance::iterate (void)
{
	de::MovePtr<tcu::TextureLevel>				testShadingImage;
	de::MovePtr<tcu::TextureLevel>				minShadingImage;
	de::MovePtr<tcu::TextureLevel>				maxShadingImage;

	// Render with test minSampleShading
	{
		MultisampleRenderer renderer (m_context, m_colorFormat, m_renderSize, m_primitiveTopology, m_vertices, m_multisampleStateParams, m_colorBlendState);
		testShadingImage = renderer.render();
	}

	// Render with minSampleShading = 0.0f
	{
		VkPipelineMultisampleStateCreateInfo	multisampleParams	= m_multisampleStateParams;
		multisampleParams.minSampleShading = 0.0f;

		MultisampleRenderer renderer (m_context, m_colorFormat, m_renderSize, m_primitiveTopology, m_vertices, multisampleParams, m_colorBlendState);
		minShadingImage = renderer.render();
	}

	// Render with minSampleShading = 1.0f
	{
		VkPipelineMultisampleStateCreateInfo	multisampleParams	= m_multisampleStateParams;
		multisampleParams.minSampleShading = 1.0f;

		MultisampleRenderer renderer (m_context, m_colorFormat, m_renderSize, m_primitiveTopology, m_vertices, multisampleParams, m_colorBlendState);
		maxShadingImage = renderer.render();
	}

	return verifyImage(testShadingImage->getAccess(), minShadingImage->getAccess(), maxShadingImage->getAccess());
}

tcu::TestStatus MinSampleShadingInstance::verifyImage (const tcu::ConstPixelBufferAccess& testShadingImage, const tcu::ConstPixelBufferAccess& minShadingImage, const tcu::ConstPixelBufferAccess& maxShadingImage)
{
	const deUint32	testColorCount	= getUniqueColorsCount(testShadingImage);
	const deUint32	minColorCount	= getUniqueColorsCount(minShadingImage);
	const deUint32	maxColorCount	= getUniqueColorsCount(maxShadingImage);

	tcu::TestLog& log = m_context.getTestContext().getLog();

	log << tcu::TestLog::Message
		<< "\nColors found: " << testColorCount << "\n"
		<< "Min. colors expected: " << minColorCount << "\n"
		<< "Max. colors expected: " << maxColorCount << "\n"
		<< tcu::TestLog::EndMessage;

	if (minColorCount > testColorCount || testColorCount > maxColorCount)
		return tcu::TestStatus::fail("Unique colors out of expected bounds");
	else
		return tcu::TestStatus::pass("Unique colors within expected bounds");
}

SampleMaskInstance::SampleMaskInstance (Context&										context,
										VkPrimitiveTopology								topology,
										const std::vector<Vertex4RGBA>&					vertices,
										const VkPipelineMultisampleStateCreateInfo&		multisampleStateParams,
										const VkPipelineColorBlendAttachmentState&		blendState)
	: vkt::TestInstance			(context)
	, m_colorFormat				(VK_FORMAT_R8G8B8A8_UNORM)
	, m_renderSize				(32, 32)
	, m_primitiveTopology		(topology)
	, m_vertices				(vertices)
	, m_multisampleStateParams	(multisampleStateParams)
	, m_colorBlendState			(blendState)
{
}

tcu::TestStatus SampleMaskInstance::iterate (void)
{
	de::MovePtr<tcu::TextureLevel>				testSampleMaskImage;
	de::MovePtr<tcu::TextureLevel>				minSampleMaskImage;
	de::MovePtr<tcu::TextureLevel>				maxSampleMaskImage;

	// Render with test flags
	{
		MultisampleRenderer renderer (m_context, m_colorFormat, m_renderSize, m_primitiveTopology, m_vertices, m_multisampleStateParams, m_colorBlendState);
		testSampleMaskImage = renderer.render();
	}

	// Render with all flags off
	{
		VkPipelineMultisampleStateCreateInfo	multisampleParams	= m_multisampleStateParams;
		const std::vector<VkSampleMask>			sampleMask			(multisampleParams.rasterizationSamples / 32, (VkSampleMask)0);

		multisampleParams.pSampleMask = sampleMask.data();

		MultisampleRenderer renderer (m_context, m_colorFormat, m_renderSize, m_primitiveTopology, m_vertices, multisampleParams, m_colorBlendState);
		minSampleMaskImage = renderer.render();
	}

	// Render with all flags on
	{
		VkPipelineMultisampleStateCreateInfo	multisampleParams	= m_multisampleStateParams;
		const std::vector<VkSampleMask>			sampleMask			(multisampleParams.rasterizationSamples / 32, ~((VkSampleMask)0));

		multisampleParams.pSampleMask = sampleMask.data();

		MultisampleRenderer renderer (m_context, m_colorFormat, m_renderSize, m_primitiveTopology, m_vertices, multisampleParams, m_colorBlendState);
		maxSampleMaskImage = renderer.render();
	}

	return verifyImage(testSampleMaskImage->getAccess(), minSampleMaskImage->getAccess(), maxSampleMaskImage->getAccess());
}

tcu::TestStatus SampleMaskInstance::verifyImage (const tcu::ConstPixelBufferAccess& testSampleMaskImage,
												 const tcu::ConstPixelBufferAccess& minSampleMaskImage,
												 const tcu::ConstPixelBufferAccess& maxSampleMaskImage)
{
	const deUint32	testColorCount	= getUniqueColorsCount(testSampleMaskImage);
	const deUint32	minColorCount	= getUniqueColorsCount(minSampleMaskImage);
	const deUint32	maxColorCount	= getUniqueColorsCount(maxSampleMaskImage);

	tcu::TestLog& log = m_context.getTestContext().getLog();

	log << tcu::TestLog::Message
		<< "\nColors found: " << testColorCount << "\n"
		<< "Min. colors expected: " << minColorCount << "\n"
		<< "Max. colors expected: " << maxColorCount << "\n"
		<< tcu::TestLog::EndMessage;

	if (minColorCount > testColorCount || testColorCount > maxColorCount)
		return tcu::TestStatus::fail("Unique colors out of expected bounds");
	else
		return tcu::TestStatus::pass("Unique colors within expected bounds");
}

tcu::TestStatus testRasterSamplesConsistency (Context& context, GeometryType geometryType)
{
	// Use triangle only.
	DE_UNREF(geometryType);

	const VkSampleCountFlagBits samples[] =
	{
		VK_SAMPLE_COUNT_1_BIT,
		VK_SAMPLE_COUNT_2_BIT,
		VK_SAMPLE_COUNT_4_BIT,
		VK_SAMPLE_COUNT_8_BIT,
		VK_SAMPLE_COUNT_16_BIT,
		VK_SAMPLE_COUNT_32_BIT,
		VK_SAMPLE_COUNT_64_BIT
	};

	const Vertex4RGBA vertexData[3] =
	{
		{
			tcu::Vec4(-0.75f, 0.0f, 0.0f, 1.0f),
			tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f)
		},
		{
			tcu::Vec4(0.75f, 0.125f, 0.0f, 1.0f),
			tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f)
		},
		{
			tcu::Vec4(0.75f, -0.125f, 0.0f, 1.0f),
			tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f)
		}
	};

	const std::vector<Vertex4RGBA>	vertices			(vertexData, vertexData + 3);
	deUint32						prevUniqueColors	= 2;
	int								renderCount			= 0;

	// Do not render with 1 sample (start with samplesNdx = 1).
	for (int samplesNdx = 1; samplesNdx < DE_LENGTH_OF_ARRAY(samples); samplesNdx++)
	{
		if (!isSupportedSampleCount(context.getInstanceInterface(), context.getPhysicalDevice(), samples[samplesNdx]))
			continue;

		const VkPipelineMultisampleStateCreateInfo multisampleStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType							sType;
			DE_NULL,													// const void*								pNext;
			0u,															// VkPipelineMultisampleStateCreateFlags	flags;
			samples[samplesNdx],										// VkSampleCountFlagBits					rasterizationSamples;
			false,														// VkBool32									sampleShadingEnable;
			0.0f,														// float									minSampleShading;
			DE_NULL,													// const VkSampleMask*						pSampleMask;
			false,														// VkBool32									alphaToCoverageEnable;
			false														// VkBool32									alphaToOneEnable;
		};

		MultisampleRenderer				renderer		(context, VK_FORMAT_R8G8B8A8_UNORM, tcu::IVec2(32, 32), VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, vertices, multisampleStateParams, getDefaultColorBlendAttachmentState());
		de::MovePtr<tcu::TextureLevel>	result			= renderer.render();
		const deUint32					uniqueColors	= getUniqueColorsCount(result->getAccess());

		renderCount++;

		if (prevUniqueColors > uniqueColors)
		{
			std::ostringstream message;

			message << "More unique colors generated with " << samples[samplesNdx - 1] << " than with " << samples[samplesNdx];
			return tcu::TestStatus::fail(message.str());
		}

		prevUniqueColors = uniqueColors;
	}

	if (renderCount == 0)
		throw tcu::NotSupportedError("Multisampling is unsupported");

	return tcu::TestStatus::pass("Number of unique colors increases as the sample count increases");
}


// AlphaToOneInstance

AlphaToOneInstance::AlphaToOneInstance (Context&									context,
										VkPrimitiveTopology							topology,
										const std::vector<Vertex4RGBA>&				vertices,
										const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
										const VkPipelineColorBlendAttachmentState&	blendState)
	: vkt::TestInstance			(context)
	, m_colorFormat				(VK_FORMAT_R8G8B8A8_UNORM)
	, m_renderSize				(32, 32)
	, m_primitiveTopology		(topology)
	, m_vertices				(vertices)
	, m_multisampleStateParams	(multisampleStateParams)
	, m_colorBlendState			(blendState)
{
	VkPhysicalDeviceFeatures deviceFeatures;

	context.getInstanceInterface().getPhysicalDeviceFeatures(context.getPhysicalDevice(), &deviceFeatures);

	if (!deviceFeatures.alphaToOne)
		throw tcu::NotSupportedError("Alpha-to-one is not supported");
}

tcu::TestStatus AlphaToOneInstance::iterate	(void)
{
	DE_ASSERT(m_multisampleStateParams.alphaToOneEnable);
	DE_ASSERT(m_colorBlendState.blendEnable);

	de::MovePtr<tcu::TextureLevel>	alphaOneImage;
	de::MovePtr<tcu::TextureLevel>	noAlphaOneImage;

	// Render with blend enabled and alpha to one on
	{
		MultisampleRenderer renderer (m_context, m_colorFormat, m_renderSize, m_primitiveTopology, m_vertices, m_multisampleStateParams, m_colorBlendState);
		alphaOneImage = renderer.render();
	}

	// Render with blend enabled and alpha to one off
	{
		VkPipelineMultisampleStateCreateInfo	multisampleParams	= m_multisampleStateParams;
		multisampleParams.alphaToOneEnable = false;

		MultisampleRenderer renderer (m_context, m_colorFormat, m_renderSize, m_primitiveTopology, m_vertices, multisampleParams, m_colorBlendState);
		noAlphaOneImage = renderer.render();
	}

	return verifyImage(alphaOneImage->getAccess(), noAlphaOneImage->getAccess());
}

tcu::TestStatus AlphaToOneInstance::verifyImage (const tcu::ConstPixelBufferAccess&	alphaOneImage,
												 const tcu::ConstPixelBufferAccess&	noAlphaOneImage)
{
	for (int y = 0; y < m_renderSize.y(); y++)
	{
		for (int x = 0; x < m_renderSize.x(); x++)
		{
			if (!tcu::boolAll(tcu::greaterThanEqual(alphaOneImage.getPixel(x, y), noAlphaOneImage.getPixel(x, y))))
			{
				std::ostringstream message;
				message << "Unsatisfied condition: " << alphaOneImage.getPixel(x, y) << " >= " << noAlphaOneImage.getPixel(x, y);
				return tcu::TestStatus::fail(message.str());
			}
		}
	}

	return tcu::TestStatus::pass("Image rendered with alpha-to-one contains pixels of image rendered with no alpha-to-one");
}


// AlphaToCoverageInstance

AlphaToCoverageInstance::AlphaToCoverageInstance (Context&										context,
												  VkPrimitiveTopology							topology,
												  const std::vector<Vertex4RGBA>&				vertices,
												  const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
												  const VkPipelineColorBlendAttachmentState&	blendState,
												  GeometryType									geometryType)
	: vkt::TestInstance			(context)
	, m_colorFormat				(VK_FORMAT_R8G8B8A8_UNORM)
	, m_renderSize				(32, 32)
	, m_primitiveTopology		(topology)
	, m_vertices				(vertices)
	, m_multisampleStateParams	(multisampleStateParams)
	, m_colorBlendState			(blendState)
	, m_geometryType			(geometryType)
{
}

tcu::TestStatus AlphaToCoverageInstance::iterate (void)
{
	DE_ASSERT(m_multisampleStateParams.alphaToCoverageEnable);

	de::MovePtr<tcu::TextureLevel>	result;
	MultisampleRenderer				renderer	(m_context, m_colorFormat, m_renderSize, m_primitiveTopology, m_vertices, m_multisampleStateParams, m_colorBlendState);

	result = renderer.render();

	return verifyImage(result->getAccess());
}

tcu::TestStatus AlphaToCoverageInstance::verifyImage (const tcu::ConstPixelBufferAccess&	result)
{
	float maxColorValue;

	switch (m_geometryType)
	{
		case GEOMETRY_TYPE_OPAQUE_QUAD:
			maxColorValue = 1.01f;
			break;

		case GEOMETRY_TYPE_TRANSLUCENT_QUAD:
			maxColorValue = 0.52f;
			break;

		case GEOMETRY_TYPE_INVISIBLE_QUAD:
			maxColorValue = 0.01f;
			break;

		default:
			maxColorValue = 0.0f;
			DE_ASSERT(false);
	}

	for (int y = 0; y < m_renderSize.y(); y++)
	{
		for (int x = 0; x < m_renderSize.x(); x++)
		{
			if (result.getPixel(x, y).x() > maxColorValue)
			{
				std::ostringstream message;
				message << "Pixel is not below the threshold value (" << result.getPixel(x, y).x() << " > " << maxColorValue << ")";
				return tcu::TestStatus::fail(message.str());
			}
		}
	}

	return tcu::TestStatus::pass("Image matches reference value");
}


// MultisampleRenderer

MultisampleRenderer::MultisampleRenderer (Context&										context,
										  VkFormat										colorFormat,
										  const tcu::IVec2&								renderSize,
										  VkPrimitiveTopology							topology,
										  const std::vector<Vertex4RGBA>&				vertices,
										  const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
										  const VkPipelineColorBlendAttachmentState&	blendState)

	: m_context					(context)
	, m_colorFormat				(colorFormat)
	, m_renderSize				(renderSize)
	, m_multisampleStateParams	(multisampleStateParams)
	, m_colorBlendState			(blendState)
{
	if (!isSupportedSampleCount(context.getInstanceInterface(), context.getPhysicalDevice(), multisampleStateParams.rasterizationSamples))
		throw tcu::NotSupportedError("Unsupported number of rasterization samples");

	const DeviceInterface&		vk						= context.getDeviceInterface();
	const VkDevice				vkDevice				= context.getDevice();
	const deUint32				queueFamilyIndex		= context.getUniversalQueueFamilyIndex();
	SimpleAllocator				memAlloc				(vk, vkDevice, getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), context.getPhysicalDevice()));
	const VkComponentMapping	componentMappingRGBA	= { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };

	// Create color image
	{
		const VkImageCreateInfo colorImageParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,										// VkStructureType			sType;
			DE_NULL,																	// const void*				pNext;
			0u,																			// VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,															// VkImageType				imageType;
			m_colorFormat,																// VkFormat					format;
			{ (deUint32)m_renderSize.x(), (deUint32)m_renderSize.y(), 1u },				// VkExtent3D				extent;
			1u,																			// deUint32					mipLevels;
			1u,																			// deUint32					arrayLayers;
			m_multisampleStateParams.rasterizationSamples,								// VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,													// VkImageTiling			tiling;
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,		// VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,													// VkSharingMode			sharingMode;
			1u,																			// deUint32					queueFamilyIndexCount;
			&queueFamilyIndex,															// const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED,													// VkImageLayout			initialLayout;
		};

		m_colorImage			= createImage(vk, vkDevice, &colorImageParams);

		// Allocate and bind color image memory
		m_colorImageAlloc		= memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_colorImage), MemoryRequirement::Any);
		VK_CHECK(vk.bindImageMemory(vkDevice, *m_colorImage, m_colorImageAlloc->getMemory(), m_colorImageAlloc->getOffset()));
	}

	// Create resolve image
	{
		const VkImageCreateInfo resolveImageParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,											// VkStructureType			sType;
			DE_NULL,																		// const void*				pNext;
			0u,																				// VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,																// VkImageType				imageType;
			m_colorFormat,																	// VkFormat					format;
			{ (deUint32)m_renderSize.x(), (deUint32)m_renderSize.y(), 1u },					// VkExtent3D				extent;
			1u,																				// deUint32					mipLevels;
			1u,																				// deUint32					arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,															// VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,														// VkImageTiling			tiling;
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |			// VkImageUsageFlags		usage;
				VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			VK_SHARING_MODE_EXCLUSIVE,														// VkSharingMode			sharingMode;
			1u,																				// deUint32					queueFamilyIndexCount;
			&queueFamilyIndex,																// const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED														// VkImageLayout			initialLayout;
		};

		m_resolveImage = createImage(vk, vkDevice, &resolveImageParams);

		// Allocate and bind resolve image memory
		m_resolveImageAlloc = memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_resolveImage), MemoryRequirement::Any);
		VK_CHECK(vk.bindImageMemory(vkDevice, *m_resolveImage, m_resolveImageAlloc->getMemory(), m_resolveImageAlloc->getOffset()));
	}

	// Create color attachment view
	{
		const VkImageViewCreateInfo colorAttachmentViewParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,		// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			0u,												// VkImageViewCreateFlags	flags;
			*m_colorImage,									// VkImage					image;
			VK_IMAGE_VIEW_TYPE_2D,							// VkImageViewType			viewType;
			m_colorFormat,									// VkFormat					format;
			componentMappingRGBA,							// VkComponentMapping		components;
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u }	// VkImageSubresourceRange	subresourceRange;
		};

		m_colorAttachmentView = createImageView(vk, vkDevice, &colorAttachmentViewParams);
	}

	// Create resolve attachment view
	{
		const VkImageViewCreateInfo resolveAttachmentViewParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,		// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			0u,												// VkImageViewCreateFlags	flags;
			*m_resolveImage,								// VkImage					image;
			VK_IMAGE_VIEW_TYPE_2D,							// VkImageViewType			viewType;
			m_colorFormat,									// VkFormat					format;
			componentMappingRGBA,							// VkComponentMapping		components;
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u }	// VkImageSubresourceRange	subresourceRange;
		};

		m_resolveAttachmentView = createImageView(vk, vkDevice, &resolveAttachmentViewParams);
	}

	// Create render pass
	{
		const VkAttachmentDescription attachmentDescriptions[2] =
		{
			{
				0u,													// VkAttachmentDescriptionFlags		flags;
				m_colorFormat,										// VkFormat							format;
				m_multisampleStateParams.rasterizationSamples,		// VkSampleCountFlagBits			samples;
				VK_ATTACHMENT_LOAD_OP_CLEAR,						// VkAttachmentLoadOp				loadOp;
				VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp				storeOp;
				VK_ATTACHMENT_LOAD_OP_DONT_CARE,					// VkAttachmentLoadOp				stencilLoadOp;
				VK_ATTACHMENT_STORE_OP_DONT_CARE,					// VkAttachmentStoreOp				stencilStoreOp;
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,			// VkImageLayout					initialLayout;
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL			// VkImageLayout					finalLayout;
			},
			{
				0u,													// VkAttachmentDescriptionFlags		flags;
				m_colorFormat,										// VkFormat							format;
				VK_SAMPLE_COUNT_1_BIT,								// VkSampleCountFlagBits			samples;
				VK_ATTACHMENT_LOAD_OP_CLEAR,						// VkAttachmentLoadOp				loadOp;
				VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp				storeOp;
				VK_ATTACHMENT_LOAD_OP_DONT_CARE,					// VkAttachmentLoadOp				stencilLoadOp;
				VK_ATTACHMENT_STORE_OP_DONT_CARE,					// VkAttachmentStoreOp				stencilStoreOp;
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,			// VkImageLayout					initialLayout;
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL			// VkImageLayout					finalLayout;
			}
		};

		const VkAttachmentReference colorAttachmentReference =
		{
			0u,													// deUint32			attachment;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL			// VkImageLayout	layout;
		};

		const VkAttachmentReference resolveAttachmentReference =
		{
			1u,													// deUint32			attachment;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL			// VkImageLayout	layout;
		};

		const VkSubpassDescription subpassDescription =
		{
			0u,														// VkSubpassDescriptionFlags	flags;
			VK_PIPELINE_BIND_POINT_GRAPHICS,						// VkPipelineBindPoint			pipelineBindPoint;
			0u,														// deUint32						inputAttachmentCount;
			DE_NULL,												// const VkAttachmentReference*	pInputAttachments;
			1u,														// deUint32						colorAttachmentCount;
			&colorAttachmentReference,								// const VkAttachmentReference*	pColorAttachments;
			&resolveAttachmentReference,							// const VkAttachmentReference*	pResolveAttachments;
			DE_NULL,												// const VkAttachmentReference*	pDepthStencilAttachment;
			0u,														// deUint32						preserveAttachmentCount;
			DE_NULL													// const VkAttachmentReference*	pPreserveAttachments;
		};

		const VkRenderPassCreateInfo renderPassParams =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,			// VkStructureType					sType;
			DE_NULL,											// const void*						pNext;
			0u,													// VkRenderPassCreateFlags			flags;
			2u,													// deUint32							attachmentCount;
			attachmentDescriptions,								// const VkAttachmentDescription*	pAttachments;
			1u,													// deUint32							subpassCount;
			&subpassDescription,								// const VkSubpassDescription*		pSubpasses;
			0u,													// deUint32							dependencyCount;
			DE_NULL												// const VkSubpassDependency*		pDependencies;
		};

		m_renderPass = createRenderPass(vk, vkDevice, &renderPassParams);
	}

	// Create framebuffer
	{
		const VkImageView attachments[2] =
		{
			*m_colorAttachmentView,
			*m_resolveAttachmentView
		};

		const VkFramebufferCreateInfo framebufferParams =
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,			// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			0u,													// VkFramebufferCreateFlags		flags;
			*m_renderPass,										// VkRenderPass					renderPass;
			2u,													// deUint32						attachmentCount;
			attachments,										// const VkImageView*			pAttachments;
			(deUint32)m_renderSize.x(),							// deUint32						width;
			(deUint32)m_renderSize.y(),							// deUint32						height;
			1u													// deUint32						layers;
		};

		m_framebuffer = createFramebuffer(vk, vkDevice, &framebufferParams);
	}

	// Create pipeline layout
	{
		const VkPipelineLayoutCreateInfo pipelineLayoutParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType					sType;
			DE_NULL,											// const void*						pNext;
			0u,													// VkPipelineLayoutCreateFlags		flags;
			0u,													// deUint32							setLayoutCount;
			DE_NULL,											// const VkDescriptorSetLayout*		pSetLayouts;
			0u,													// deUint32							pushConstantRangeCount;
			DE_NULL												// const VkPushConstantRange*		pPushConstantRanges;
		};

		m_pipelineLayout = createPipelineLayout(vk, vkDevice, &pipelineLayoutParams);
	}

	m_vertexShaderModule	= createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("color_vert"), 0);
	m_fragmentShaderModule	= createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("color_frag"), 0);

	// Create pipeline
	{
		const VkPipelineShaderStageCreateInfo shaderStageParams[2] =
		{
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,		// VkStructureType						sType;
				DE_NULL,													// const void*							pNext;
				0u,															// VkPipelineShaderStageCreateFlags		flags;
				VK_SHADER_STAGE_VERTEX_BIT,									// VkShaderStageFlagBits				stage;
				*m_vertexShaderModule,										// VkShaderModule						module;
				"main",														// const char*							pName;
				DE_NULL														// const VkSpecializationInfo*			pSpecializationInfo;
			},
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,		// VkStructureType						sType;
				DE_NULL,													// const void*							pNext;
				0u,															// VkPipelineShaderStageCreateFlags		flags;
				VK_SHADER_STAGE_FRAGMENT_BIT,								// VkShaderStageFlagBits				stage;
				*m_fragmentShaderModule,									// VkShaderModule						module;
				"main",														// const char*							pName;
				DE_NULL														// const VkSpecializationInfo*			pSpecializationInfo;
			}
		};

		const VkVertexInputBindingDescription vertexInputBindingDescription =
		{
			0u,									// deUint32				binding;
			sizeof(Vertex4RGBA),				// deUint32				stride;
			VK_VERTEX_INPUT_RATE_VERTEX			// VkVertexInputRate	inputRate;
		};

		const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[2] =
		{
			{
				0u,									// deUint32	location;
				0u,									// deUint32	binding;
				VK_FORMAT_R32G32B32A32_SFLOAT,		// VkFormat	format;
				0u									// deUint32	offset;
			},
			{
				1u,									// deUint32	location;
				0u,									// deUint32	binding;
				VK_FORMAT_R32G32B32A32_SFLOAT,		// VkFormat	format;
				DE_OFFSET_OF(Vertex4RGBA, color),	// deUint32	offset;
			}
		};

		const VkPipelineVertexInputStateCreateInfo vertexInputStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			0u,																// VkPipelineVertexInputStateCreateFlags	flags;
			1u,																// deUint32									vertexBindingDescriptionCount;
			&vertexInputBindingDescription,									// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
			2u,																// deUint32									vertexAttributeDescriptionCount;
			vertexInputAttributeDescriptions								// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
		};

		const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			0u,																// VkPipelineInputAssemblyStateCreateFlags	flags;
			topology,														// VkPrimitiveTopology						topology;
			false															// VkBool32									primitiveRestartEnable;
		};

		const VkViewport viewport =
		{
			0.0f,						// float	x;
			0.0f,						// float	y;
			(float)m_renderSize.x(),	// float	width;
			(float)m_renderSize.y(),	// float	height;
			0.0f,						// float	minDepth;
			1.0f						// float	maxDepth;
		};

		const VkRect2D scissor =
		{
			{ 0, 0 },													// VkOffset2D  offset;
			{ (deUint32)m_renderSize.x(), (deUint32)m_renderSize.y() }	// VkExtent2D  extent;
		};

		const VkPipelineViewportStateCreateInfo viewportStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,			// VkStructureType						sType;
			DE_NULL,														// const void*							pNext;
			0u,																// VkPipelineViewportStateCreateFlags	flags;
			1u,																// deUint32								viewportCount;
			&viewport,														// const VkViewport*					pViewports;
			1u,																// deUint32								scissorCount;
			&scissor														// const VkRect2D*						pScissors;
		};

		const VkPipelineRasterizationStateCreateInfo rasterStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,		// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			0u,																// VkPipelineRasterizationStateCreateFlags	flags;
			false,															// VkBool32									depthClampEnable;
			false,															// VkBool32									rasterizerDiscardEnable;
			VK_POLYGON_MODE_FILL,											// VkPolygonMode							polygonMode;
			VK_CULL_MODE_NONE,												// VkCullModeFlags							cullMode;
			VK_FRONT_FACE_COUNTER_CLOCKWISE,								// VkFrontFace								frontFace;
			VK_FALSE,														// VkBool32									depthBiasEnable;
			0.0f,															// float									depthBiasConstantFactor;
			0.0f,															// float									depthBiasClamp;
			0.0f,															// float									depthBiasSlopeFactor;
			1.0f															// float									lineWidth;
		};

		const VkPipelineColorBlendStateCreateInfo colorBlendStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType								sType;
			DE_NULL,													// const void*									pNext;
			0u,															// VkPipelineColorBlendStateCreateFlags			flags;
			false,														// VkBool32										logicOpEnable;
			VK_LOGIC_OP_COPY,											// VkLogicOp									logicOp;
			1u,															// deUint32										attachmentCount;
			&m_colorBlendState,											// const VkPipelineColorBlendAttachmentState*	pAttachments;
			{ 0.0f, 0.0f, 0.0f, 0.0f }									// float										blendConstants[4];
		};

		const VkPipelineDepthStencilStateCreateInfo depthStencilStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,	// VkStructureType							sType;
			DE_NULL,													// const void*								pNext;
			0u,															// VkPipelineDepthStencilStateCreateFlags	flags;
			false,														// VkBool32									depthTestEnable;
			false,														// VkBool32									depthWriteEnable;
			VK_COMPARE_OP_LESS,											// VkCompareOp								depthCompareOp;
			false,														// VkBool32									depthBoundsTestEnable;
			false,														// VkBool32									stencilTestEnable;
			// VkStencilOpState	front;
			{
				VK_STENCIL_OP_KEEP,		// VkStencilOp	failOp;
				VK_STENCIL_OP_KEEP,		// VkStencilOp	passOp;
				VK_STENCIL_OP_KEEP,		// VkStencilOp	depthFailOp;
				VK_COMPARE_OP_NEVER,	// VkCompareOp	compareOp;
				0u,						// deUint32		compareMask;
				0u,						// deUint32		writeMask;
				0u,						// deUint32		reference;
			},
			// VkStencilOpState	back;
			{
				VK_STENCIL_OP_KEEP,		// VkStencilOp	failOp;
				VK_STENCIL_OP_KEEP,		// VkStencilOp	passOp;
				VK_STENCIL_OP_KEEP,		// VkStencilOp	depthFailOp;
				VK_COMPARE_OP_NEVER,	// VkCompareOp	compareOp;
				0u,						// deUint32		compareMask;
				0u,						// deUint32		writeMask;
				0u,						// deUint32		reference;
			},
			0.0f,														// float			minDepthBounds;
			1.0f,														// float			maxDepthBounds;
		};

		const VkGraphicsPipelineCreateInfo graphicsPipelineParams =
		{
			VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,	// VkStructureType									sType;
			DE_NULL,											// const void*										pNext;
			0u,													// VkPipelineCreateFlags							flags;
			2u,													// deUint32											stageCount;
			shaderStageParams,									// const VkPipelineShaderStageCreateInfo*			pStages;
			&vertexInputStateParams,							// const VkPipelineVertexInputStateCreateInfo*		pVertexInputState;
			&inputAssemblyStateParams,							// const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState;
			DE_NULL,											// const VkPipelineTessellationStateCreateInfo*		pTessellationState;
			&viewportStateParams,								// const VkPipelineViewportStateCreateInfo*			pViewportState;
			&rasterStateParams,									// const VkPipelineRasterizationStateCreateInfo*	pRasterizationState;
			&m_multisampleStateParams,							// const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
			&depthStencilStateParams,							// const VkPipelineDepthStencilStateCreateInfo*		pDepthStencilState;
			&colorBlendStateParams,								// const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
			(const VkPipelineDynamicStateCreateInfo*)DE_NULL,	// const VkPipelineDynamicStateCreateInfo*			pDynamicState;
			*m_pipelineLayout,									// VkPipelineLayout									layout;
			*m_renderPass,										// VkRenderPass										renderPass;
			0u,													// deUint32											subpass;
			0u,													// VkPipeline										basePipelineHandle;
			0u													// deInt32											basePipelineIndex;
		};

		m_graphicsPipeline	= createGraphicsPipeline(vk, vkDevice, DE_NULL, &graphicsPipelineParams);
	}

	// Create vertex buffer
	{
		const VkBufferCreateInfo vertexBufferParams =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			0u,											// VkBufferCreateFlags	flags;
			1024u,										// VkDeviceSize			size;
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,			// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
			1u,											// deUint32				queueFamilyIndexCount;
			&queueFamilyIndex							// const deUint32*		pQueueFamilyIndices;
		};

		m_vertexBuffer		= createBuffer(vk, vkDevice, &vertexBufferParams);
		m_vertexBufferAlloc	= memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_vertexBuffer), MemoryRequirement::HostVisible);

		VK_CHECK(vk.bindBufferMemory(vkDevice, *m_vertexBuffer, m_vertexBufferAlloc->getMemory(), m_vertexBufferAlloc->getOffset()));

		// Load vertices into vertex buffer
		deMemcpy(m_vertexBufferAlloc->getHostPtr(), vertices.data(), vertices.size() * sizeof(Vertex4RGBA));
		flushMappedMemoryRange(vk, vkDevice, m_vertexBufferAlloc->getMemory(), m_vertexBufferAlloc->getOffset(), vertexBufferParams.size);
	}

	// Create command pool
	{
		const VkCommandPoolCreateInfo cmdPoolParams =
		{
			VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,		// VkStructureType				sType;
			DE_NULL,										// const void*					pNext;
			VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,			// VkCommandPoolCreateFlags		flags;
			queueFamilyIndex,								// deUint32						queueFamilyIndex;
		};

		m_cmdPool = createCommandPool(vk, vkDevice, &cmdPoolParams);
	}

	// Create command buffer
	{
		const VkCommandBufferAllocateInfo cmdBufferAllocateInfo =
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,	// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			*m_cmdPool,										// VkCommandPool			commandPool;
			VK_COMMAND_BUFFER_LEVEL_PRIMARY,				// VkCommandBufferLevel	level;
			1u												// deUint32				bufferCount;
		};

		const VkCommandBufferBeginInfo cmdBufferBeginInfo =
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	// VkStructureType					sType;
			DE_NULL,										// const void*						pNext;
			0u,												// VkCommandBufferUsageFlags		flags;
			(const VkCommandBufferInheritanceInfo*)DE_NULL,
		};

		VkClearValue colorClearValue;
		colorClearValue.color.float32[0] = 0.0f;
		colorClearValue.color.float32[1] = 0.0f;
		colorClearValue.color.float32[2] = 0.0f;
		colorClearValue.color.float32[3] = 0.0f;

		const VkClearValue clearValues[2] =
		{
			colorClearValue,
			colorClearValue
		};

		const VkRenderPassBeginInfo renderPassBeginInfo =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,				// VkStructureType		sType;
			DE_NULL,												// const void*			pNext;
			*m_renderPass,											// VkRenderPass			renderPass;
			*m_framebuffer,											// VkFramebuffer		framebuffer;
			{
				{ 0, 0 },
				{ (deUint32)m_renderSize.x(), (deUint32)m_renderSize.y() }
			},														// VkRect2D				renderArea;
			2,														// deUint32				clearValueCount;
			clearValues												// const VkClearValue*	pClearValues;
		};

		const VkImageMemoryBarrier imageLayoutBarriers[] =
		{
			// color attachment image
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// VkStructureType			sType;
				DE_NULL,										// const void*				pNext;
				0u,												// VkAccessFlags			srcAccessMask;
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,			// VkAccessFlags			dstAccessMask;
				VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout			oldLayout;
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,		// VkImageLayout			newLayout;
				VK_QUEUE_FAMILY_IGNORED,						// deUint32					srcQueueFamilyIndex;
				VK_QUEUE_FAMILY_IGNORED,						// deUint32					dstQueueFamilyIndex;
				*m_colorImage,									// VkImage					image;
				{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u },	// VkImageSubresourceRange	subresourceRange;
			},
			// resolve attachment image
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// VkStructureType			sType;
				DE_NULL,										// const void*				pNext;
				0u,												// VkAccessFlags			srcAccessMask;
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,			// VkAccessFlags			dstAccessMask;
				VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout			oldLayout;
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,		// VkImageLayout			newLayout;
				VK_QUEUE_FAMILY_IGNORED,						// deUint32					srcQueueFamilyIndex;
				VK_QUEUE_FAMILY_IGNORED,						// deUint32					dstQueueFamilyIndex;
				*m_resolveImage,								// VkImage					image;
				{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u },	// VkImageSubresourceRange	subresourceRange;
			},
		};

		m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, &cmdBufferAllocateInfo);

		VK_CHECK(vk.beginCommandBuffer(*m_cmdBuffer, &cmdBufferBeginInfo));

		vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, (VkDependencyFlags)0,
			0u, DE_NULL, 0u, DE_NULL, DE_LENGTH_OF_ARRAY(imageLayoutBarriers), imageLayoutBarriers);

		vk.cmdBeginRenderPass(*m_cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		VkDeviceSize vertexBufferOffset = 0u;

		vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipeline);
		vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);
		vk.cmdDraw(*m_cmdBuffer, (deUint32)vertices.size(), 1, 0, 0);

		vk.cmdEndRenderPass(*m_cmdBuffer);

		VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));
	}

	// Create fence
	{
		const VkFenceCreateInfo fenceParams =
		{
			VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,								// const void*			pNext;
			0u										// VkFenceCreateFlags	flags;
		};

		m_fence = createFence(vk, vkDevice, &fenceParams);
	}
}

MultisampleRenderer::~MultisampleRenderer (void)
{
}

de::MovePtr<tcu::TextureLevel> MultisampleRenderer::render (void)
{
	const DeviceInterface&		vk					= m_context.getDeviceInterface();
	const VkDevice				vkDevice			= m_context.getDevice();
	const VkQueue				queue				= m_context.getUniversalQueue();
	const deUint32				queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	SimpleAllocator				allocator			(vk, vkDevice, getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));
	const VkSubmitInfo			submitInfo	=
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,	// VkStructureType			sType;
		DE_NULL,						// const void*				pNext;
		0u,								// deUint32					waitSemaphoreCount;
		DE_NULL,						// const VkSemaphore*		pWaitSemaphores;
		(const VkPipelineStageFlags*)DE_NULL,
		1u,								// deUint32					commandBufferCount;
		&m_cmdBuffer.get(),				// const VkCommandBuffer*	pCommandBuffers;
		0u,								// deUint32					signalSemaphoreCount;
		DE_NULL							// const VkSemaphore*		pSignalSemaphores;
	};

	VK_CHECK(vk.resetFences(vkDevice, 1, &m_fence.get()));
	VK_CHECK(vk.queueSubmit(queue, 1, &submitInfo, *m_fence));
	VK_CHECK(vk.waitForFences(vkDevice, 1, &m_fence.get(), true, ~(0ull) /* infinity*/));

	return readColorAttachment(vk, vkDevice, queue, queueFamilyIndex, allocator, *m_resolveImage, m_colorFormat, m_renderSize.cast<deUint32>());
}

} // anonymous

tcu::TestCaseGroup* createMultisampleTests (tcu::TestContext& testCtx)
{
	const VkSampleCountFlagBits samples[] =
	{
		VK_SAMPLE_COUNT_2_BIT,
		VK_SAMPLE_COUNT_4_BIT,
		VK_SAMPLE_COUNT_8_BIT,
		VK_SAMPLE_COUNT_16_BIT,
		VK_SAMPLE_COUNT_32_BIT,
		VK_SAMPLE_COUNT_64_BIT
	};

	de::MovePtr<tcu::TestCaseGroup> multisampleTests (new tcu::TestCaseGroup(testCtx, "multisample", ""));

	// Rasterization samples tests
	{
		de::MovePtr<tcu::TestCaseGroup> rasterizationSamplesTests(new tcu::TestCaseGroup(testCtx, "raster_samples", ""));

		for (int samplesNdx = 0; samplesNdx < DE_LENGTH_OF_ARRAY(samples); samplesNdx++)
		{
			std::ostringstream caseName;
			caseName << "samples_" << samples[samplesNdx];

			de::MovePtr<tcu::TestCaseGroup> samplesTests	(new tcu::TestCaseGroup(testCtx, caseName.str().c_str(), ""));

			samplesTests->addChild(new RasterizationSamplesTest(testCtx, "primitive_triangle", "", samples[samplesNdx], GEOMETRY_TYPE_OPAQUE_TRIANGLE));
			samplesTests->addChild(new RasterizationSamplesTest(testCtx, "primitive_line", "", samples[samplesNdx], GEOMETRY_TYPE_OPAQUE_LINE));
			samplesTests->addChild(new RasterizationSamplesTest(testCtx, "primitive_point", "", samples[samplesNdx], GEOMETRY_TYPE_OPAQUE_POINT));

			rasterizationSamplesTests->addChild(samplesTests.release());
		}

		multisampleTests->addChild(rasterizationSamplesTests.release());
	}

	// Raster samples consistency check
	{
		de::MovePtr<tcu::TestCaseGroup> rasterSamplesConsistencyTests(new tcu::TestCaseGroup(testCtx, "raster_samples_consistency", ""));

		addFunctionCaseWithPrograms(rasterSamplesConsistencyTests.get(),
									"unique_colors_check",
									"",
									initMultisamplePrograms,
									testRasterSamplesConsistency,
									GEOMETRY_TYPE_OPAQUE_TRIANGLE);

		multisampleTests->addChild(rasterSamplesConsistencyTests.release());
	}

	// minSampleShading tests
	{
		struct TestConfig
		{
			const char*	name;
			float		minSampleShading;
		};

		const TestConfig testConfigs[] =
		{
			{ "min_0_0",	0.0f },
			{ "min_0_25",	0.25f },
			{ "min_0_5",	0.5f },
			{ "min_0_75",	0.75f },
			{ "min_1_0",	1.0f }
		};

		de::MovePtr<tcu::TestCaseGroup> minSampleShadingTests(new tcu::TestCaseGroup(testCtx, "min_sample_shading", ""));

		for (int configNdx = 0; configNdx < DE_LENGTH_OF_ARRAY(testConfigs); configNdx++)
		{
			const TestConfig&				testConfig				= testConfigs[configNdx];
			de::MovePtr<tcu::TestCaseGroup>	minShadingValueTests	(new tcu::TestCaseGroup(testCtx, testConfigs[configNdx].name, ""));

			for (int samplesNdx = 0; samplesNdx < DE_LENGTH_OF_ARRAY(samples); samplesNdx++)
			{
				std::ostringstream caseName;
				caseName << "samples_" << samples[samplesNdx];

				de::MovePtr<tcu::TestCaseGroup> samplesTests	(new tcu::TestCaseGroup(testCtx, caseName.str().c_str(), ""));

				samplesTests->addChild(new MinSampleShadingTest(testCtx, "primitive_triangle", "", samples[samplesNdx], testConfig.minSampleShading, GEOMETRY_TYPE_OPAQUE_TRIANGLE));
				samplesTests->addChild(new MinSampleShadingTest(testCtx, "primitive_line", "", samples[samplesNdx], testConfig.minSampleShading, GEOMETRY_TYPE_OPAQUE_LINE));
				samplesTests->addChild(new MinSampleShadingTest(testCtx, "primitive_point", "", samples[samplesNdx], testConfig.minSampleShading, GEOMETRY_TYPE_OPAQUE_POINT));

				minShadingValueTests->addChild(samplesTests.release());
			}

			minSampleShadingTests->addChild(minShadingValueTests.release());
		}

		multisampleTests->addChild(minSampleShadingTests.release());
	}

	// pSampleMask tests
	{
		struct TestConfig
		{
			const char*		name;
			const char*		description;
			VkSampleMask	sampleMask;
		};

		const TestConfig testConfigs[] =
		{
			{ "mask_all_on",	"All mask bits are off",			0x0 },
			{ "mask_all_off",	"All mask bits are on",				0xFFFFFFFF },
			{ "mask_one",		"All mask elements are 0x1",		0x1},
			{ "mask_random",	"All mask elements are 0xAAAAAAAA",	0xAAAAAAAA },
		};

		de::MovePtr<tcu::TestCaseGroup> sampleMaskTests(new tcu::TestCaseGroup(testCtx, "sample_mask", ""));

		for (int configNdx = 0; configNdx < DE_LENGTH_OF_ARRAY(testConfigs); configNdx++)
		{
			const TestConfig&				testConfig				= testConfigs[configNdx];
			de::MovePtr<tcu::TestCaseGroup>	sampleMaskValueTests	(new tcu::TestCaseGroup(testCtx, testConfig.name, testConfig.description));

			for (int samplesNdx = 0; samplesNdx < DE_LENGTH_OF_ARRAY(samples); samplesNdx++)
			{
				std::ostringstream caseName;
				caseName << "samples_" << samples[samplesNdx];

				const deUint32					sampleMaskCount	= samples[samplesNdx] / 32;
				de::MovePtr<tcu::TestCaseGroup> samplesTests	(new tcu::TestCaseGroup(testCtx, caseName.str().c_str(), ""));

				std::vector<VkSampleMask> mask;
				for (deUint32 maskNdx = 0; maskNdx < sampleMaskCount; maskNdx++)
					mask.push_back(testConfig.sampleMask);

				samplesTests->addChild(new SampleMaskTest(testCtx, "primitive_triangle", "", samples[samplesNdx], mask, GEOMETRY_TYPE_OPAQUE_TRIANGLE));
				samplesTests->addChild(new SampleMaskTest(testCtx, "primitive_line", "", samples[samplesNdx], mask, GEOMETRY_TYPE_OPAQUE_LINE));
				samplesTests->addChild(new SampleMaskTest(testCtx, "primitive_point", "", samples[samplesNdx], mask, GEOMETRY_TYPE_OPAQUE_POINT));

				sampleMaskValueTests->addChild(samplesTests.release());
			}

			sampleMaskTests->addChild(sampleMaskValueTests.release());
		}

		multisampleTests->addChild(sampleMaskTests.release());

	}

	// AlphaToOne tests
	{
		de::MovePtr<tcu::TestCaseGroup> alphaToOneTests(new tcu::TestCaseGroup(testCtx, "alpha_to_one", ""));

		for (int samplesNdx = 0; samplesNdx < DE_LENGTH_OF_ARRAY(samples); samplesNdx++)
		{
			std::ostringstream caseName;
			caseName << "samples_" << samples[samplesNdx];

			alphaToOneTests->addChild(new AlphaToOneTest(testCtx, caseName.str(), "", samples[samplesNdx]));
		}

		multisampleTests->addChild(alphaToOneTests.release());
	}

	// AlphaToCoverageEnable tests
	{
		de::MovePtr<tcu::TestCaseGroup> alphaToCoverageTests (new tcu::TestCaseGroup(testCtx, "alpha_to_coverage", ""));

		for (int samplesNdx = 0; samplesNdx < DE_LENGTH_OF_ARRAY(samples); samplesNdx++)
		{
			std::ostringstream caseName;
			caseName << "samples_" << samples[samplesNdx];

			de::MovePtr<tcu::TestCaseGroup> samplesTests	(new tcu::TestCaseGroup(testCtx, caseName.str().c_str(), ""));

			samplesTests->addChild(new AlphaToCoverageTest(testCtx, "alpha_opaque", "", samples[samplesNdx], GEOMETRY_TYPE_OPAQUE_QUAD));
			samplesTests->addChild(new AlphaToCoverageTest(testCtx, "alpha_translucent", "", samples[samplesNdx], GEOMETRY_TYPE_TRANSLUCENT_QUAD));
			samplesTests->addChild(new AlphaToCoverageTest(testCtx, "alpha_invisible", "", samples[samplesNdx], GEOMETRY_TYPE_INVISIBLE_QUAD));

			alphaToCoverageTests->addChild(samplesTests.release());
		}
		multisampleTests->addChild(alphaToCoverageTests.release());
	}

	return multisampleTests.release();
}

} // pipeline
} // vkt
