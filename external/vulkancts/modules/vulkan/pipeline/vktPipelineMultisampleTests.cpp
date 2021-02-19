/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Imagination Technologies Ltd.
 * Copyright (c) 2017 Google Inc.
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
#include "vktPipelineMultisampleImageTests.hpp"
#include "vktPipelineMultisampleSampleLocationsExtTests.hpp"
#include "vktPipelineMultisampleMixedAttachmentSamplesTests.hpp"
#include "vktPipelineMultisampleShaderFragmentMaskTests.hpp"
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
#include "vkCmdUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTestLog.hpp"
#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"
#include "deStringUtil.hpp"
#include "deMemory.h"

#include <sstream>
#include <vector>
#include <map>
#include <memory>
#include <algorithm>
#include <set>
#include <array>

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
	GEOMETRY_TYPE_OPAQUE_QUAD_NONZERO_DEPTH,	//!< placed at z = 0.5
	GEOMETRY_TYPE_TRANSLUCENT_QUAD,
	GEOMETRY_TYPE_INVISIBLE_TRIANGLE,
	GEOMETRY_TYPE_INVISIBLE_QUAD,
	GEOMETRY_TYPE_GRADIENT_QUAD
};

enum TestModeBits
{
	TEST_MODE_DEPTH_BIT		= 1u,
	TEST_MODE_STENCIL_BIT	= 2u,
};
typedef deUint32 TestModeFlags;

enum RenderType
{
	// resolve multisample rendering to single sampled image
	RENDER_TYPE_RESOLVE				= 0u,

	// copy samples to an array of single sampled images
	RENDER_TYPE_COPY_SAMPLES		= 1u,

	// render first with only depth/stencil and then with color + depth/stencil
	RENDER_TYPE_DEPTHSTENCIL_ONLY	= 2u,

	// render using color attachment at location 1 and location 0 set as unused
	RENDER_TYPE_UNUSED_ATTACHMENT	= 3u
};

enum ImageBackingMode
{
	IMAGE_BACKING_MODE_REGULAR	= 0u,
	IMAGE_BACKING_MODE_SPARSE
};

struct MultisampleTestParams
{
	GeometryType		geometryType;
	float				pointSize;
	ImageBackingMode	backingMode;
};

void									initMultisamplePrograms				(SourceCollections& sources, MultisampleTestParams params);
bool									isSupportedSampleCount				(const InstanceInterface& instanceInterface, VkPhysicalDevice physicalDevice, VkSampleCountFlagBits rasterizationSamples);
bool									isSupportedDepthStencilFormat		(const InstanceInterface& vki, const VkPhysicalDevice physDevice, const VkFormat format);
VkPipelineColorBlendAttachmentState		getDefaultColorBlendAttachmentState	(void);
deUint32								getUniqueColorsCount				(const tcu::ConstPixelBufferAccess& image);
VkImageAspectFlags						getImageAspectFlags					(const VkFormat format);
VkPrimitiveTopology						getPrimitiveTopology				(const GeometryType geometryType);
std::vector<Vertex4RGBA>				generateVertices					(const GeometryType geometryType);
VkFormat								findSupportedDepthStencilFormat		(Context& context, const bool useDepth, const bool useStencil);

class MultisampleTest : public vkt::TestCase
{
public:

												MultisampleTest						(tcu::TestContext&								testContext,
																					 const std::string&								name,
																					 const std::string&								description,
																					 const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
																					 const VkPipelineColorBlendAttachmentState&		blendState,
																					 GeometryType									geometryType,
																					 float											pointSize,
																					 ImageBackingMode								backingMode);
	virtual										~MultisampleTest					(void) {}

	virtual void								initPrograms						(SourceCollections& programCollection) const;
	virtual TestInstance*						createInstance						(Context& context) const;
	virtual void								checkSupport						(Context& context) const;

protected:
	virtual TestInstance*						createMultisampleTestInstance		(Context&										context,
																					 VkPrimitiveTopology							topology,
																					 float											pointSize,
																					 const std::vector<Vertex4RGBA>&				vertices,
																					 const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
																					 const VkPipelineColorBlendAttachmentState&		colorBlendState) const = 0;
	VkPipelineMultisampleStateCreateInfo		m_multisampleStateParams;
	const VkPipelineColorBlendAttachmentState	m_colorBlendState;
	const GeometryType							m_geometryType;
	const float									m_pointSize;
	const ImageBackingMode						m_backingMode;
	std::vector<VkSampleMask>					m_sampleMask;
};

class RasterizationSamplesTest : public MultisampleTest
{
public:
												RasterizationSamplesTest			(tcu::TestContext&		testContext,
																					 const std::string&		name,
																					 const std::string&		description,
																					 VkSampleCountFlagBits	rasterizationSamples,
																					 GeometryType			geometryType,
																					 float					pointSize,
																					 ImageBackingMode		backingMode,
																					 TestModeFlags			modeFlags				= 0u);
	virtual										~RasterizationSamplesTest			(void) {}

protected:
	virtual TestInstance*						createMultisampleTestInstance		(Context&										context,
																					 VkPrimitiveTopology							topology,
																					 float											pointSize,
																					 const std::vector<Vertex4RGBA>&				vertices,
																					 const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
																					 const VkPipelineColorBlendAttachmentState&		colorBlendState) const;

	static VkPipelineMultisampleStateCreateInfo	getRasterizationSamplesStateParams	(VkSampleCountFlagBits rasterizationSamples);

	const ImageBackingMode						m_backingMode;
	const TestModeFlags							m_modeFlags;
};

class MinSampleShadingTest : public MultisampleTest
{
public:
												MinSampleShadingTest				(tcu::TestContext&		testContext,
																					 const std::string&		name,
																					 const std::string&		description,
																					 VkSampleCountFlagBits	rasterizationSamples,
																					 float					minSampleShading,
																					 GeometryType			geometryType,
																					 float					pointSize,
																					 ImageBackingMode		backingMode,
																					 const bool				minSampleShadingEnabled = true);
	virtual										~MinSampleShadingTest				(void) {}

protected:
	virtual void								initPrograms						(SourceCollections& programCollection) const;
	virtual void								checkSupport						(Context& context) const;
	virtual TestInstance*						createMultisampleTestInstance		(Context&										context,
																					 VkPrimitiveTopology							topology,
																					 float											pointSize,
																					 const std::vector<Vertex4RGBA>&				vertices,
																					 const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
																					 const VkPipelineColorBlendAttachmentState&		colorBlendState) const;

	static VkPipelineMultisampleStateCreateInfo	getMinSampleShadingStateParams		(VkSampleCountFlagBits	rasterizationSamples,
																					 float					minSampleShading,
																					 bool					minSampleShadingEnabled);

	const float									m_pointSize;
	const ImageBackingMode						m_backingMode;
	const bool									m_minSampleShadingEnabled;
};

class SampleMaskTest : public MultisampleTest
{
public:
												SampleMaskTest						(tcu::TestContext&					testContext,
																					 const std::string&					name,
																					 const std::string&					description,
																					 VkSampleCountFlagBits				rasterizationSamples,
																					 const std::vector<VkSampleMask>&	sampleMask,
																					 GeometryType						geometryType,
																					 float								pointSize,
																					 ImageBackingMode					backingMode);

	virtual										~SampleMaskTest						(void) {}

protected:
	virtual TestInstance*						createMultisampleTestInstance		(Context&										context,
																					 VkPrimitiveTopology							topology,
																					 float											pointSize,
																					 const std::vector<Vertex4RGBA>&				vertices,
																					 const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
																					 const VkPipelineColorBlendAttachmentState&		colorBlendState) const;

	static VkPipelineMultisampleStateCreateInfo	getSampleMaskStateParams			(VkSampleCountFlagBits rasterizationSamples, const std::vector<VkSampleMask>& sampleMask);

	const ImageBackingMode						m_backingMode;
};

class AlphaToOneTest : public MultisampleTest
{
public:
												AlphaToOneTest					(tcu::TestContext&					testContext,
																				 const std::string&					name,
																				 const std::string&					description,
																				 VkSampleCountFlagBits				rasterizationSamples,
																				 ImageBackingMode					backingMode);

	virtual										~AlphaToOneTest					(void) {}

protected:
	virtual void								checkSupport					(Context& context) const;
	virtual TestInstance*						createMultisampleTestInstance	(Context&										context,
																				 VkPrimitiveTopology							topology,
																				 float											pointSize,
																				 const std::vector<Vertex4RGBA>&				vertices,
																				 const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
																				 const VkPipelineColorBlendAttachmentState&		colorBlendState) const;

	static VkPipelineMultisampleStateCreateInfo	getAlphaToOneStateParams		(VkSampleCountFlagBits rasterizationSamples);
	static VkPipelineColorBlendAttachmentState	getAlphaToOneBlendState			(void);

	const ImageBackingMode						m_backingMode;
};

class AlphaToCoverageTest : public MultisampleTest
{
public:
												AlphaToCoverageTest				(tcu::TestContext&		testContext,
																				 const std::string&		name,
																				 const std::string&		description,
																				 VkSampleCountFlagBits	rasterizationSamples,
																				 GeometryType			geometryType,
																				 ImageBackingMode		backingMode);

	virtual										~AlphaToCoverageTest			(void) {}

protected:
	virtual TestInstance*						createMultisampleTestInstance	(Context&										context,
																				 VkPrimitiveTopology							topology,
																				 float											pointSize,
																				 const std::vector<Vertex4RGBA>&				vertices,
																				 const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
																				 const VkPipelineColorBlendAttachmentState&		colorBlendState) const;

	static VkPipelineMultisampleStateCreateInfo	getAlphaToCoverageStateParams	(VkSampleCountFlagBits rasterizationSamples);

	GeometryType								m_geometryType;
	const ImageBackingMode						m_backingMode;
};

class AlphaToCoverageNoColorAttachmentTest : public MultisampleTest
{
public:
												AlphaToCoverageNoColorAttachmentTest	(tcu::TestContext&		testContext,
																						 const std::string&		name,
																						 const std::string&		description,
																						 VkSampleCountFlagBits	rasterizationSamples,
																						 GeometryType			geometryType,
																						 ImageBackingMode		backingMode);

	virtual										~AlphaToCoverageNoColorAttachmentTest	(void) {}

protected:
	virtual TestInstance*						createMultisampleTestInstance			(Context&										context,
																						 VkPrimitiveTopology							topology,
																						 float											pointSize,
																						 const std::vector<Vertex4RGBA>&				vertices,
																						 const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
																						 const VkPipelineColorBlendAttachmentState&		colorBlendState) const;

	static VkPipelineMultisampleStateCreateInfo	getStateParams							(VkSampleCountFlagBits rasterizationSamples);

	GeometryType								m_geometryType;
	const ImageBackingMode						m_backingMode;
};

class AlphaToCoverageColorUnusedAttachmentTest : public MultisampleTest
{
public:
												AlphaToCoverageColorUnusedAttachmentTest	(tcu::TestContext&		testContext,
																							 const std::string&		name,
																							 const std::string&		description,
																							 VkSampleCountFlagBits	rasterizationSamples,
																							 GeometryType			geometryType,
																							 ImageBackingMode		backingMode);

	virtual										~AlphaToCoverageColorUnusedAttachmentTest	(void) {}

protected:
	virtual void								initPrograms								(SourceCollections& programCollection) const;

	virtual TestInstance*						createMultisampleTestInstance				(Context&										context,
																							 VkPrimitiveTopology							topology,
																							 float											pointSize,
																							 const std::vector<Vertex4RGBA>&				vertices,
																							 const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
																							 const VkPipelineColorBlendAttachmentState&		colorBlendState) const;

	static VkPipelineMultisampleStateCreateInfo	getStateParams								(VkSampleCountFlagBits rasterizationSamples);

	GeometryType								m_geometryType;
	const ImageBackingMode						m_backingMode;
};

class SampleMaskWithDepthTestTest : public vkt::TestCase
{
public:
												SampleMaskWithDepthTestTest		(tcu::TestContext&				testContext,
																				 const std::string&				name,
																				 const std::string&				description,
																				 const VkSampleCountFlagBits	rasterizationSamples,
																				 const bool						enablePostDepthCoverage		= false);

												~SampleMaskWithDepthTestTest	(void) {}

	void										initPrograms					(SourceCollections&		programCollection)	const;
	TestInstance*								createInstance					(Context&				context)			const;
	virtual void								checkSupport					(Context&				context)			const;
private:
	const VkSampleCountFlagBits					m_rasterizationSamples;
	const bool									m_enablePostDepthCoverage;
};

typedef de::SharedPtr<Unique<VkPipeline> > VkPipelineSp;

class MultisampleRenderer
{
public:
												MultisampleRenderer			(Context&										context,
																			 const VkFormat									colorFormat,
																			 const tcu::IVec2&								renderSize,
																			 const VkPrimitiveTopology						topology,
																			 const std::vector<Vertex4RGBA>&				vertices,
																			 const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
																			 const VkPipelineColorBlendAttachmentState&		blendState,
																			 const RenderType								renderType,
																			 const ImageBackingMode							backingMode);

												MultisampleRenderer			(Context&										context,
																			 const VkFormat									colorFormat,
																			 const VkFormat									depthStencilFormat,
																			 const tcu::IVec2&								renderSize,
																			 const bool										useDepth,
																			 const bool										useStencil,
																			 const deUint32									numTopologies,
																			 const VkPrimitiveTopology*						pTopology,
																			 const std::vector<Vertex4RGBA>*				pVertices,
																			 const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
																			 const VkPipelineColorBlendAttachmentState&		blendState,
																			 const RenderType								renderType,
																			 const ImageBackingMode							backingMode,
																			 const float									depthClearValue			= 1.0f);

	virtual										~MultisampleRenderer		(void);

	de::MovePtr<tcu::TextureLevel>				render						(void);
	de::MovePtr<tcu::TextureLevel>				getSingleSampledImage		(deUint32 sampleId);

protected:
	void										initialize					(Context&										context,
																			 const deUint32									numTopologies,
																			 const VkPrimitiveTopology*						pTopology,
																			 const std::vector<Vertex4RGBA>*				pVertices);

	Context&									m_context;

	const Unique<VkSemaphore>					m_bindSemaphore;

	const VkFormat								m_colorFormat;
	const VkFormat								m_depthStencilFormat;
	tcu::IVec2									m_renderSize;
	const bool									m_useDepth;
	const bool									m_useStencil;

	const VkPipelineMultisampleStateCreateInfo	m_multisampleStateParams;
	const VkPipelineColorBlendAttachmentState	m_colorBlendState;

	const RenderType							m_renderType;

	Move<VkImage>								m_colorImage;
	de::MovePtr<Allocation>						m_colorImageAlloc;
	Move<VkImageView>							m_colorAttachmentView;

	Move<VkImage>								m_resolveImage;
	de::MovePtr<Allocation>						m_resolveImageAlloc;
	Move<VkImageView>							m_resolveAttachmentView;

	struct PerSampleImage
	{
		Move<VkImage>								m_image;
		de::MovePtr<Allocation>						m_imageAlloc;
		Move<VkImageView>							m_attachmentView;
	};
	std::vector<de::SharedPtr<PerSampleImage> >	m_perSampleImages;

	Move<VkImage>								m_depthStencilImage;
	de::MovePtr<Allocation>						m_depthStencilImageAlloc;
	Move<VkImageView>							m_depthStencilAttachmentView;

	Move<VkRenderPass>							m_renderPass;
	Move<VkFramebuffer>							m_framebuffer;

	Move<VkShaderModule>						m_vertexShaderModule;
	Move<VkShaderModule>						m_fragmentShaderModule;

	Move<VkShaderModule>						m_copySampleVertexShaderModule;
	Move<VkShaderModule>						m_copySampleFragmentShaderModule;

	Move<VkBuffer>								m_vertexBuffer;
	de::MovePtr<Allocation>						m_vertexBufferAlloc;

	Move<VkPipelineLayout>						m_pipelineLayout;
	std::vector<VkPipelineSp>					m_graphicsPipelines;

	Move<VkDescriptorSetLayout>					m_copySampleDesciptorLayout;
	Move<VkDescriptorPool>						m_copySampleDesciptorPool;
	Move<VkDescriptorSet>						m_copySampleDesciptorSet;

	Move<VkPipelineLayout>						m_copySamplePipelineLayout;
	std::vector<VkPipelineSp>					m_copySamplePipelines;

	Move<VkCommandPool>							m_cmdPool;
	Move<VkCommandBuffer>						m_cmdBuffer;

	std::vector<de::SharedPtr<Allocation> >		m_allocations;

	ImageBackingMode							m_backingMode;
	const float									m_depthClearValue;
};

class RasterizationSamplesInstance : public vkt::TestInstance
{
public:
										RasterizationSamplesInstance	(Context&										context,
																		 VkPrimitiveTopology							topology,
																		 float											pointSize,
																		 const std::vector<Vertex4RGBA>&				vertices,
																		 const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
																		 const VkPipelineColorBlendAttachmentState&		blendState,
																		 const TestModeFlags							modeFlags,
																		 ImageBackingMode								backingMode);
	virtual								~RasterizationSamplesInstance	(void) {}

	virtual tcu::TestStatus				iterate							(void);

protected:
	virtual tcu::TestStatus				verifyImage						(const tcu::ConstPixelBufferAccess& result);

	const VkFormat						m_colorFormat;
	const tcu::IVec2					m_renderSize;
	const VkPrimitiveTopology			m_primitiveTopology;
	const float							m_pointSize;
	const std::vector<Vertex4RGBA>		m_vertices;
	const std::vector<Vertex4RGBA>		m_fullQuadVertices;			//!< used by depth/stencil case
	const TestModeFlags					m_modeFlags;
	de::MovePtr<MultisampleRenderer>	m_multisampleRenderer;
};

class MinSampleShadingInstance : public vkt::TestInstance
{
public:
												MinSampleShadingInstance	(Context&										context,
																			 VkPrimitiveTopology							topology,
																			 float											pointSize,
																			 const std::vector<Vertex4RGBA>&				vertices,
																			 const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
																			 const VkPipelineColorBlendAttachmentState&		blendState,
																			 ImageBackingMode								backingMode);
	virtual										~MinSampleShadingInstance	(void) {}

	virtual tcu::TestStatus						iterate						(void);

protected:
	virtual tcu::TestStatus						verifySampleShadedImage		(const std::vector<tcu::TextureLevel>& testShadingImages,
																			 const tcu::ConstPixelBufferAccess& noSampleshadingImage);

	const VkFormat								m_colorFormat;
	const tcu::IVec2							m_renderSize;
	const VkPrimitiveTopology					m_primitiveTopology;
	const std::vector<Vertex4RGBA>				m_vertices;
	const VkPipelineMultisampleStateCreateInfo	m_multisampleStateParams;
	const VkPipelineColorBlendAttachmentState	m_colorBlendState;
	const ImageBackingMode						m_backingMode;
};

class MinSampleShadingDisabledInstance : public MinSampleShadingInstance
{
public:
												MinSampleShadingDisabledInstance	(Context&										context,
																					 VkPrimitiveTopology							topology,
																					 float											pointSize,
																					 const std::vector<Vertex4RGBA>&				vertices,
																					 const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
																					 const VkPipelineColorBlendAttachmentState&		blendState,
																					 ImageBackingMode								backingMode);
	virtual										~MinSampleShadingDisabledInstance	(void) {}

protected:
	virtual tcu::TestStatus						verifySampleShadedImage				(const std::vector<tcu::TextureLevel>&	sampleShadedImages,
																					 const tcu::ConstPixelBufferAccess&		noSampleshadingImage);
};

class SampleMaskInstance : public vkt::TestInstance
{
public:
												SampleMaskInstance			(Context&										context,
																			 VkPrimitiveTopology							topology,
																			 float											pointSize,
																			 const std::vector<Vertex4RGBA>&				vertices,
																			 const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
																			 const VkPipelineColorBlendAttachmentState&		blendState,
																			 ImageBackingMode								backingMode);
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
	const ImageBackingMode						m_backingMode;
};

class AlphaToOneInstance : public vkt::TestInstance
{
public:
												AlphaToOneInstance			(Context&										context,
																			 VkPrimitiveTopology							topology,
																			 const std::vector<Vertex4RGBA>&				vertices,
																			 const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
																			 const VkPipelineColorBlendAttachmentState&		blendState,
																			 ImageBackingMode								backingMode);
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
	const ImageBackingMode						m_backingMode;
};

class AlphaToCoverageInstance : public vkt::TestInstance
{
public:
												AlphaToCoverageInstance		(Context&										context,
																			 VkPrimitiveTopology							topology,
																			 const std::vector<Vertex4RGBA>&				vertices,
																			 const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
																			 const VkPipelineColorBlendAttachmentState&		blendState,
																			 GeometryType									geometryType,
																			 ImageBackingMode								backingMode);
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
	const ImageBackingMode						m_backingMode;
};

class AlphaToCoverageNoColorAttachmentInstance : public vkt::TestInstance
{
public:
												AlphaToCoverageNoColorAttachmentInstance	(Context&										context,
																							 VkPrimitiveTopology							topology,
																							 const std::vector<Vertex4RGBA>&				vertices,
																							 const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
																							 const VkPipelineColorBlendAttachmentState&		blendState,
																							 GeometryType									geometryType,
																							 ImageBackingMode								backingMode);
	virtual										~AlphaToCoverageNoColorAttachmentInstance	(void) {}

	virtual tcu::TestStatus						iterate										(void);

protected:
	virtual tcu::TestStatus						verifyImage									(const tcu::ConstPixelBufferAccess& result);
	const VkFormat								m_colorFormat;
	const VkFormat								m_depthStencilFormat;
	const tcu::IVec2							m_renderSize;
	const VkPrimitiveTopology					m_primitiveTopology;
	const std::vector<Vertex4RGBA>				m_vertices;
	const VkPipelineMultisampleStateCreateInfo	m_multisampleStateParams;
	const VkPipelineColorBlendAttachmentState	m_colorBlendState;
	const GeometryType							m_geometryType;
	const ImageBackingMode						m_backingMode;
};

class AlphaToCoverageColorUnusedAttachmentInstance : public vkt::TestInstance
{
public:
												AlphaToCoverageColorUnusedAttachmentInstance	(Context&										context,
																								 VkPrimitiveTopology							topology,
																								 const std::vector<Vertex4RGBA>&				vertices,
																								 const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
																								 const VkPipelineColorBlendAttachmentState&		blendState,
																								 GeometryType									geometryType,
																								 ImageBackingMode								backingMode);
	virtual										~AlphaToCoverageColorUnusedAttachmentInstance	(void) {}

	virtual tcu::TestStatus						iterate											(void);

protected:
	virtual tcu::TestStatus						verifyImage										(const tcu::ConstPixelBufferAccess& result);
	const VkFormat								m_colorFormat;
	const tcu::IVec2							m_renderSize;
	const VkPrimitiveTopology					m_primitiveTopology;
	const std::vector<Vertex4RGBA>				m_vertices;
	const VkPipelineMultisampleStateCreateInfo	m_multisampleStateParams;
	const VkPipelineColorBlendAttachmentState	m_colorBlendState;
	const GeometryType							m_geometryType;
	const ImageBackingMode						m_backingMode;
};

class SampleMaskWithDepthTestInstance : public vkt::TestInstance
{
public:
													SampleMaskWithDepthTestInstance		(Context&							context,
																						 const VkSampleCountFlagBits		rasterizationSamples,
																						 const bool							enablePostDepthCoverage);
													~SampleMaskWithDepthTestInstance	(void) {}

	tcu::TestStatus									iterate								(void);

protected:
	VkPipelineMultisampleStateCreateInfo			getMultisampleState					(const VkSampleCountFlagBits		rasterizationSamples);
	std::vector<Vertex4RGBA>						generateVertices					(void);
	tcu::TestStatus									verifyImage							(const tcu::ConstPixelBufferAccess&	result);

	struct SampleCoverage
	{
		SampleCoverage() {};
		SampleCoverage(deUint32 min_, deUint32 max_)
			: min(min_), max(max_) {};

		deUint32	min;
		deUint32	max;
	};

	const VkSampleCountFlagBits						m_rasterizationSamples;
	const bool										m_enablePostDepthCoverage;
	const VkFormat									m_colorFormat;
	const VkFormat									m_depthStencilFormat;
	const tcu::IVec2								m_renderSize;
	const bool										m_useDepth;
	const bool										m_useStencil;
	const VkPrimitiveTopology						m_topology;
	const tcu::Vec4									m_renderColor;
	const std::vector<Vertex4RGBA>					m_vertices;
	const VkPipelineMultisampleStateCreateInfo		m_multisampleStateParams;
	const VkPipelineColorBlendAttachmentState		m_blendState;
	const RenderType								m_renderType;
	const ImageBackingMode							m_imageBackingMode;
	const float										m_depthClearValue;
	std::map<VkSampleCountFlagBits, SampleCoverage>	m_refCoverageAfterDepthTest;
};


// Helper functions

void initMultisamplePrograms (SourceCollections& sources, MultisampleTestParams params)
{
	const std::string	pointSize		= params.geometryType == GEOMETRY_TYPE_OPAQUE_POINT ? (std::string("	gl_PointSize = ") + de::toString(params.pointSize) + ".0f;\n") : std::string("");
	std::ostringstream	vertexSource;

	vertexSource <<
		"#version 310 es\n"
		"layout(location = 0) in vec4 position;\n"
		"layout(location = 1) in vec4 color;\n"
		"layout(location = 0) out highp vec4 vtxColor;\n"
		"void main (void)\n"
		"{\n"
		"	gl_Position = position;\n"
		"	vtxColor = color;\n"
		<< pointSize
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

void initSampleShadingPrograms (SourceCollections& sources, MultisampleTestParams params)
{
	{
		const std::string	pointSize		= params.geometryType == GEOMETRY_TYPE_OPAQUE_POINT ? (std::string("	gl_PointSize = ") + de::toString(params.pointSize) + ".0f;\n") : std::string("");
		std::ostringstream	vertexSource;

		vertexSource <<
			"#version 440\n"
			"layout(location = 0) in vec4 position;\n"
			"layout(location = 1) in vec4 color;\n"
			"void main (void)\n"
			"{\n"
			"	gl_Position = position;\n"
			<< pointSize
			<< "}\n";

		static const char* fragmentSource =
			"#version 440\n"
			"layout(location = 0) out highp vec4 fragColor;\n"
			"void main (void)\n"
			"{\n"
			"	fragColor = vec4(fract(gl_FragCoord.xy), 0.0, 1.0);\n"
			"}\n";

		sources.glslSources.add("color_vert") << glu::VertexSource(vertexSource.str());
		sources.glslSources.add("color_frag") << glu::FragmentSource(fragmentSource);
	}

	{
		static const char*  vertexSource =
			"#version 440\n"
			"void main (void)\n"
			"{\n"
			"	const vec4 positions[4] = vec4[4](\n"
			"		vec4(-1.0, -1.0, 0.0, 1.0),\n"
			"		vec4(-1.0,  1.0, 0.0, 1.0),\n"
			"		vec4( 1.0, -1.0, 0.0, 1.0),\n"
			"		vec4( 1.0,  1.0, 0.0, 1.0)\n"
			"	);\n"
			"	gl_Position = positions[gl_VertexIndex];\n"
			"}\n";

		static const char* fragmentSource =
			"#version 440\n"
			"precision highp float;\n"
			"layout(location = 0) out highp vec4 fragColor;\n"
			"layout(set = 0, binding = 0, input_attachment_index = 0) uniform subpassInputMS imageMS;\n"
			"layout(push_constant) uniform PushConstantsBlock\n"
			"{\n"
			"	int sampleId;\n"
			"} pushConstants;\n"
			"void main (void)\n"
			"{\n"
			"	fragColor = subpassLoad(imageMS, pushConstants.sampleId);\n"
			"}\n";

		sources.glslSources.add("quad_vert") << glu::VertexSource(vertexSource);
		sources.glslSources.add("copy_sample_frag") << glu::FragmentSource(fragmentSource);
	}
}

void initAlphaToCoverageColorUnusedAttachmentPrograms (SourceCollections& sources)
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
		"}\n";

	// Location 0 is unused, but the alpha for coverage is written there. Location 1 has no alpha channel.
	static const char* fragmentSource =
		"#version 310 es\n"
		"layout(location = 0) in highp vec4 vtxColor;\n"
		"layout(location = 0) out highp vec4 fragColor0;\n"
		"layout(location = 1) out highp vec3 fragColor1;\n"
		"void main (void)\n"
		"{\n"
		"	fragColor0 = vtxColor;\n"
		"	fragColor1 = vtxColor.rgb;\n"
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

VkImageAspectFlags getImageAspectFlags (const VkFormat format)
{
	const tcu::TextureFormat tcuFormat = mapVkFormat(format);

	if      (tcuFormat.order == tcu::TextureFormat::DS)		return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	else if (tcuFormat.order == tcu::TextureFormat::D)		return VK_IMAGE_ASPECT_DEPTH_BIT;
	else if (tcuFormat.order == tcu::TextureFormat::S)		return VK_IMAGE_ASPECT_STENCIL_BIT;

	DE_ASSERT(false);
	return 0u;
}

std::vector<Vertex4RGBA> generateVertices (const GeometryType geometryType)
{
	std::vector<Vertex4RGBA> vertices;

	switch (geometryType)
	{
		case GEOMETRY_TYPE_OPAQUE_TRIANGLE:
		case GEOMETRY_TYPE_INVISIBLE_TRIANGLE:
		{
			Vertex4RGBA vertexData[3] =
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

			if (geometryType == GEOMETRY_TYPE_INVISIBLE_TRIANGLE)
			{
				for (int i = 0; i < 3; i++)
					vertexData[i].color = tcu::Vec4();
			}

			vertices = std::vector<Vertex4RGBA>(vertexData, vertexData + 3);
			break;
		}

		case GEOMETRY_TYPE_OPAQUE_LINE:
		{
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
			const Vertex4RGBA vertex =
			{
				tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f),
				tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f)
			};

			vertices = std::vector<Vertex4RGBA>(1, vertex);
			break;
		}

		case GEOMETRY_TYPE_OPAQUE_QUAD:
		case GEOMETRY_TYPE_OPAQUE_QUAD_NONZERO_DEPTH:
		case GEOMETRY_TYPE_TRANSLUCENT_QUAD:
		case GEOMETRY_TYPE_INVISIBLE_QUAD:
		case GEOMETRY_TYPE_GRADIENT_QUAD:
		{
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

			if (geometryType == GEOMETRY_TYPE_TRANSLUCENT_QUAD)
			{
				for (int i = 0; i < 4; i++)
					vertexData[i].color.w() = 0.25f;
			}
			else if (geometryType == GEOMETRY_TYPE_INVISIBLE_QUAD)
			{
				for (int i = 0; i < 4; i++)
					vertexData[i].color.w() = 0.0f;
			}
			else if (geometryType == GEOMETRY_TYPE_GRADIENT_QUAD)
			{
				vertexData[0].color.w() = 0.0f;
				vertexData[2].color.w() = 0.0f;
			}
			else if (geometryType == GEOMETRY_TYPE_OPAQUE_QUAD_NONZERO_DEPTH)
			{
				for (int i = 0; i < 4; i++)
					vertexData[i].position.z() = 0.5f;
			}

			vertices = std::vector<Vertex4RGBA>(vertexData, vertexData + 4);
			break;
		}

		default:
			DE_ASSERT(false);
	}
	return vertices;
}

VkPrimitiveTopology getPrimitiveTopology (const GeometryType geometryType)
{
	switch (geometryType)
	{
		case GEOMETRY_TYPE_OPAQUE_TRIANGLE:
		case GEOMETRY_TYPE_INVISIBLE_TRIANGLE:			return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		case GEOMETRY_TYPE_OPAQUE_LINE:					return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
		case GEOMETRY_TYPE_OPAQUE_POINT:				return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

		case GEOMETRY_TYPE_OPAQUE_QUAD:
		case GEOMETRY_TYPE_OPAQUE_QUAD_NONZERO_DEPTH:
		case GEOMETRY_TYPE_TRANSLUCENT_QUAD:
		case GEOMETRY_TYPE_INVISIBLE_QUAD:
		case GEOMETRY_TYPE_GRADIENT_QUAD:				return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

		default:
			DE_ASSERT(false);
			return VK_PRIMITIVE_TOPOLOGY_LAST;
	}
}

bool isSupportedDepthStencilFormat (const InstanceInterface& vki, const VkPhysicalDevice physDevice, const VkFormat format)
{
	VkFormatProperties formatProps;
	vki.getPhysicalDeviceFormatProperties(physDevice, format, &formatProps);
	return (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0;
}

VkFormat findSupportedDepthStencilFormat (Context& context, const bool useDepth, const bool useStencil)
{
	if (useDepth && !useStencil)
		return VK_FORMAT_D16_UNORM;		// must be supported

	const InstanceInterface&	vki			= context.getInstanceInterface();
	const VkPhysicalDevice		physDevice	= context.getPhysicalDevice();

	// One of these formats must be supported.

	if (isSupportedDepthStencilFormat(vki, physDevice, VK_FORMAT_D24_UNORM_S8_UINT))
		return VK_FORMAT_D24_UNORM_S8_UINT;

	if (isSupportedDepthStencilFormat(vki, physDevice, VK_FORMAT_D32_SFLOAT_S8_UINT))
		return VK_FORMAT_D32_SFLOAT_S8_UINT;

	return VK_FORMAT_UNDEFINED;
}


// MultisampleTest

MultisampleTest::MultisampleTest (tcu::TestContext&								testContext,
								  const std::string&							name,
								  const std::string&							description,
								  const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
								  const VkPipelineColorBlendAttachmentState&	blendState,
								  GeometryType									geometryType,
								  float											pointSize,
								  ImageBackingMode								backingMode)
	: vkt::TestCase				(testContext, name, description)
	, m_multisampleStateParams	(multisampleStateParams)
	, m_colorBlendState			(blendState)
	, m_geometryType			(geometryType)
	, m_pointSize				(pointSize)
	, m_backingMode				(backingMode)
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

void MultisampleTest::initPrograms (SourceCollections& programCollection) const
{
	MultisampleTestParams params = {m_geometryType, m_pointSize, m_backingMode};
	initMultisamplePrograms(programCollection, params);
}

TestInstance* MultisampleTest::createInstance (Context& context) const
{
	return createMultisampleTestInstance(context, getPrimitiveTopology(m_geometryType), m_pointSize, generateVertices(m_geometryType), m_multisampleStateParams, m_colorBlendState);
}

void MultisampleTest::checkSupport (Context& context) const
{
	if (m_geometryType == GEOMETRY_TYPE_OPAQUE_POINT && m_pointSize > 1.0f)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_LARGE_POINTS);
}

// RasterizationSamplesTest

RasterizationSamplesTest::RasterizationSamplesTest (tcu::TestContext&		testContext,
													const std::string&		name,
													const std::string&		description,
													VkSampleCountFlagBits	rasterizationSamples,
													GeometryType			geometryType,
													float					pointSize,
													ImageBackingMode		backingMode,
													TestModeFlags			modeFlags)
	: MultisampleTest	(testContext, name, description, getRasterizationSamplesStateParams(rasterizationSamples), getDefaultColorBlendAttachmentState(), geometryType, pointSize, backingMode)
	, m_backingMode		(backingMode)
	, m_modeFlags		(modeFlags)
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
																	   float										pointSize,
																	   const std::vector<Vertex4RGBA>&				vertices,
																	   const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
																	   const VkPipelineColorBlendAttachmentState&	colorBlendState) const
{
	return new RasterizationSamplesInstance(context, topology, pointSize, vertices, multisampleStateParams, colorBlendState, m_modeFlags, m_backingMode);
}


// MinSampleShadingTest

MinSampleShadingTest::MinSampleShadingTest (tcu::TestContext&		testContext,
											const std::string&		name,
											const std::string&		description,
											VkSampleCountFlagBits	rasterizationSamples,
											float					minSampleShading,
											GeometryType			geometryType,
											float					pointSize,
											ImageBackingMode		backingMode,
											const bool				minSampleShadingEnabled)
	: MultisampleTest			(testContext, name, description, getMinSampleShadingStateParams(rasterizationSamples, minSampleShading, minSampleShadingEnabled), getDefaultColorBlendAttachmentState(), geometryType, pointSize, backingMode)
	, m_pointSize				(pointSize)
	, m_backingMode				(backingMode)
	, m_minSampleShadingEnabled	(minSampleShadingEnabled)
{
}

void MinSampleShadingTest::checkSupport (Context& context) const
{
	MultisampleTest::checkSupport(context);

	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SAMPLE_RATE_SHADING);
}

void MinSampleShadingTest::initPrograms (SourceCollections& programCollection) const
{
	MultisampleTestParams params = {m_geometryType, m_pointSize, m_backingMode};
	initSampleShadingPrograms(programCollection, params);
}

TestInstance* MinSampleShadingTest::createMultisampleTestInstance (Context&										context,
																   VkPrimitiveTopology							topology,
																   float										pointSize,
																   const std::vector<Vertex4RGBA>&				vertices,
																   const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
																   const VkPipelineColorBlendAttachmentState&	colorBlendState) const
{
	if (m_minSampleShadingEnabled)
		return new MinSampleShadingInstance(context, topology, pointSize, vertices, multisampleStateParams, colorBlendState, m_backingMode);
	else
		return new MinSampleShadingDisabledInstance(context, topology, pointSize, vertices, multisampleStateParams, colorBlendState, m_backingMode);
}

VkPipelineMultisampleStateCreateInfo MinSampleShadingTest::getMinSampleShadingStateParams (VkSampleCountFlagBits rasterizationSamples, float minSampleShading, bool minSampleShadingEnabled)
{
	const VkPipelineMultisampleStateCreateInfo multisampleStateParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,													// const void*								pNext;
		0u,															// VkPipelineMultisampleStateCreateFlags	flags;
		rasterizationSamples,										// VkSampleCountFlagBits					rasterizationSamples;
		minSampleShadingEnabled ? VK_TRUE : VK_FALSE,				// VkBool32									sampleShadingEnable;
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
								GeometryType						geometryType,
								float								pointSize,
								ImageBackingMode					backingMode)
	: MultisampleTest	(testContext, name, description, getSampleMaskStateParams(rasterizationSamples, sampleMask), getDefaultColorBlendAttachmentState(), geometryType, pointSize, backingMode)
	, m_backingMode		(backingMode)
{
}

TestInstance* SampleMaskTest::createMultisampleTestInstance (Context&										context,
															 VkPrimitiveTopology							topology,
															 float											pointSize,
															 const std::vector<Vertex4RGBA>&				vertices,
															 const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
															 const VkPipelineColorBlendAttachmentState&		colorBlendState) const
{
	DE_UNREF(pointSize);
	return new SampleMaskInstance(context, topology, pointSize, vertices, multisampleStateParams, colorBlendState, m_backingMode);
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
								VkSampleCountFlagBits	rasterizationSamples,
								ImageBackingMode		backingMode)
	: MultisampleTest	(testContext, name, description, getAlphaToOneStateParams(rasterizationSamples), getAlphaToOneBlendState(), GEOMETRY_TYPE_GRADIENT_QUAD, 1.0f, backingMode)
	, m_backingMode(backingMode)
{
}

void AlphaToOneTest::checkSupport (Context& context) const
{
	MultisampleTest::checkSupport(context);

	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_ALPHA_TO_ONE);
}

TestInstance* AlphaToOneTest::createMultisampleTestInstance (Context&										context,
															 VkPrimitiveTopology							topology,
															 float											pointSize,
															 const std::vector<Vertex4RGBA>&				vertices,
															 const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
															 const VkPipelineColorBlendAttachmentState&		colorBlendState) const
{
	DE_UNREF(pointSize);
	return new AlphaToOneInstance(context, topology, vertices, multisampleStateParams, colorBlendState, m_backingMode);
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
										  GeometryType				geometryType,
										  ImageBackingMode			backingMode)
	: MultisampleTest	(testContext, name, description, getAlphaToCoverageStateParams(rasterizationSamples), getDefaultColorBlendAttachmentState(), geometryType, 1.0f, backingMode)
	, m_geometryType	(geometryType)
	, m_backingMode		(backingMode)
{
}

TestInstance* AlphaToCoverageTest::createMultisampleTestInstance (Context&										context,
																  VkPrimitiveTopology							topology,
																  float											pointSize,
																  const std::vector<Vertex4RGBA>&				vertices,
																  const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
																  const VkPipelineColorBlendAttachmentState&	colorBlendState) const
{
	DE_UNREF(pointSize);
	return new AlphaToCoverageInstance(context, topology, vertices, multisampleStateParams, colorBlendState, m_geometryType, m_backingMode);
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

// AlphaToCoverageNoColorAttachmentTest

AlphaToCoverageNoColorAttachmentTest::AlphaToCoverageNoColorAttachmentTest (tcu::TestContext&		testContext,
																			const std::string&		name,
																			const std::string&		description,
																			VkSampleCountFlagBits	rasterizationSamples,
																			GeometryType			geometryType,
																			ImageBackingMode		backingMode)
	: MultisampleTest	(testContext, name, description, getStateParams(rasterizationSamples), getDefaultColorBlendAttachmentState(), geometryType, 1.0f, backingMode)
	, m_geometryType	(geometryType)
	, m_backingMode		(backingMode)
{
}

TestInstance* AlphaToCoverageNoColorAttachmentTest::createMultisampleTestInstance (Context&										context,
																				   VkPrimitiveTopology							topology,
																				   float										pointSize,
																				   const std::vector<Vertex4RGBA>&				vertices,
																				   const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
																				   const VkPipelineColorBlendAttachmentState&	colorBlendState) const
{
	DE_UNREF(pointSize);
	return new AlphaToCoverageNoColorAttachmentInstance(context, topology, vertices, multisampleStateParams, colorBlendState, m_geometryType, m_backingMode);
}

VkPipelineMultisampleStateCreateInfo AlphaToCoverageNoColorAttachmentTest::getStateParams (VkSampleCountFlagBits rasterizationSamples)
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

// AlphaToCoverageColorUnusedAttachmentTest

AlphaToCoverageColorUnusedAttachmentTest::AlphaToCoverageColorUnusedAttachmentTest (tcu::TestContext&		testContext,
																					const std::string&		name,
																					const std::string&		description,
																					VkSampleCountFlagBits	rasterizationSamples,
																					GeometryType			geometryType,
																					ImageBackingMode		backingMode)
	: MultisampleTest	(testContext, name, description, getStateParams(rasterizationSamples), getDefaultColorBlendAttachmentState(), geometryType, 1.0f, backingMode)
	, m_geometryType	(geometryType)
	, m_backingMode		(backingMode)
{
}

void AlphaToCoverageColorUnusedAttachmentTest::initPrograms (SourceCollections& programCollection) const
{
	initAlphaToCoverageColorUnusedAttachmentPrograms(programCollection);
}

TestInstance* AlphaToCoverageColorUnusedAttachmentTest::createMultisampleTestInstance (Context&										context,
																					   VkPrimitiveTopology							topology,
																					   float										pointSize,
																					   const std::vector<Vertex4RGBA>&				vertices,
																					   const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
																					   const VkPipelineColorBlendAttachmentState&	colorBlendState) const
{
	DE_UNREF(pointSize);
	return new AlphaToCoverageColorUnusedAttachmentInstance(context, topology, vertices, multisampleStateParams, colorBlendState, m_geometryType, m_backingMode);
}

VkPipelineMultisampleStateCreateInfo AlphaToCoverageColorUnusedAttachmentTest::getStateParams (VkSampleCountFlagBits rasterizationSamples)
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

// SampleMaskWithDepthTestTest

SampleMaskWithDepthTestTest::SampleMaskWithDepthTestTest (tcu::TestContext&					testContext,
														  const std::string&				name,
														  const std::string&				description,
														  const VkSampleCountFlagBits		rasterizationSamples,
														  const bool						enablePostDepthCoverage)
	: vkt::TestCase				(testContext, name, description)
	, m_rasterizationSamples	(rasterizationSamples)
	, m_enablePostDepthCoverage	(enablePostDepthCoverage)
{
}

void SampleMaskWithDepthTestTest::checkSupport (Context& context) const
{
	if (!context.getDeviceProperties().limits.standardSampleLocations)
		TCU_THROW(NotSupportedError, "standardSampleLocations required");

	context.requireDeviceFunctionality("VK_EXT_post_depth_coverage");
}

void SampleMaskWithDepthTestTest::initPrograms (SourceCollections& programCollection) const
{
	DE_ASSERT((int)m_rasterizationSamples <= 32);

	static const char* vertexSource =
		"#version 440\n"
		"layout(location = 0) in vec4 position;\n"
		"layout(location = 1) in vec4 color;\n"
		"layout(location = 0) out vec4 vtxColor;\n"
		"out gl_PerVertex\n"
		"{\n"
		"    vec4 gl_Position;\n"
		"};\n"
		"\n"
		"void main (void)\n"
		"{\n"
		"    gl_Position = position;\n"
		"    vtxColor = color;\n"
		"}\n";

	std::ostringstream fragmentSource;
	fragmentSource <<
		"#version 440\n"
		<< (m_enablePostDepthCoverage ? "#extension GL_ARB_post_depth_coverage : require\n" : "") <<
		"layout(early_fragment_tests) in;\n"
		<< (m_enablePostDepthCoverage ? "layout(post_depth_coverage) in;\n" : "") <<
		"layout(location = 0) in vec4 vtxColor;\n"
		"layout(location = 0) out vec4 fragColor;\n"
		"void main (void)\n"
		"{\n"
		"    const int coveredSamples = bitCount(gl_SampleMaskIn[0]);\n"
		"    fragColor = vtxColor * (1.0 / " << (int)m_rasterizationSamples << " * coveredSamples);\n"
		"}\n";

	programCollection.glslSources.add("color_vert") << glu::VertexSource(vertexSource);
	programCollection.glslSources.add("color_frag") << glu::FragmentSource(fragmentSource.str());
}

TestInstance* SampleMaskWithDepthTestTest::createInstance (Context& context) const
{
	return new SampleMaskWithDepthTestInstance(context, m_rasterizationSamples, m_enablePostDepthCoverage);
}

// RasterizationSamplesInstance

RasterizationSamplesInstance::RasterizationSamplesInstance (Context&										context,
															VkPrimitiveTopology								topology,
															float											pointSize,
															const std::vector<Vertex4RGBA>&					vertices,
															const VkPipelineMultisampleStateCreateInfo&		multisampleStateParams,
															const VkPipelineColorBlendAttachmentState&		blendState,
															const TestModeFlags								modeFlags,
															ImageBackingMode								backingMode)
	: vkt::TestInstance		(context)
	, m_colorFormat			(VK_FORMAT_R8G8B8A8_UNORM)
	, m_renderSize			(32, 32)
	, m_primitiveTopology	(topology)
	, m_pointSize			(pointSize)
	, m_vertices			(vertices)
	, m_fullQuadVertices	(generateVertices(GEOMETRY_TYPE_OPAQUE_QUAD_NONZERO_DEPTH))
	, m_modeFlags			(modeFlags)
{
	if (m_modeFlags != 0)
	{
		const bool		useDepth			= (m_modeFlags & TEST_MODE_DEPTH_BIT) != 0;
		const bool		useStencil			= (m_modeFlags & TEST_MODE_STENCIL_BIT) != 0;
		const VkFormat	depthStencilFormat	= findSupportedDepthStencilFormat(context, useDepth, useStencil);

		if (depthStencilFormat == VK_FORMAT_UNDEFINED)
			TCU_THROW(NotSupportedError, "Required depth/stencil format is not supported");

		const VkPrimitiveTopology		pTopology[2] = { m_primitiveTopology, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP };
		const std::vector<Vertex4RGBA>	pVertices[2] = { m_vertices, m_fullQuadVertices };

		m_multisampleRenderer = de::MovePtr<MultisampleRenderer>(
			new MultisampleRenderer(
				context, m_colorFormat, depthStencilFormat, m_renderSize, useDepth, useStencil, 2u, pTopology, pVertices, multisampleStateParams, blendState, RENDER_TYPE_RESOLVE, backingMode));
	}
	else
	{
		m_multisampleRenderer = de::MovePtr<MultisampleRenderer>(
			new MultisampleRenderer(context, m_colorFormat, m_renderSize, topology, vertices, multisampleStateParams, blendState, RENDER_TYPE_RESOLVE, backingMode));
	}
}

tcu::TestStatus RasterizationSamplesInstance::iterate (void)
{
	de::MovePtr<tcu::TextureLevel> level(m_multisampleRenderer->render());
	return verifyImage(level->getAccess());
}

tcu::TestStatus RasterizationSamplesInstance::verifyImage (const tcu::ConstPixelBufferAccess& result)
{
	// Verify range of unique pixels
	{
		const deUint32	numUniqueColors = getUniqueColorsCount(result);
		const deUint32	minUniqueColors	= (m_primitiveTopology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST && m_pointSize == 1.0f) ? 2 : 3;

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
		rr::RenderState				renderState		(refRenderer.getViewportState(), m_context.getDeviceProperties().limits.subPixelPrecisionBits);

		if (m_primitiveTopology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST)
		{
			VkPhysicalDeviceProperties deviceProperties;

			m_context.getInstanceInterface().getPhysicalDeviceProperties(m_context.getPhysicalDevice(), &deviceProperties);

			// gl_PointSize is clamped to pointSizeRange
			renderState.point.pointSize = deFloatMin(m_pointSize, deviceProperties.limits.pointSizeRange[1]);
		}

		if (m_modeFlags == 0)
		{
			refRenderer.colorClear(tcu::Vec4(0.0f));
			refRenderer.draw(renderState, mapVkPrimitiveTopology(m_primitiveTopology), m_vertices);
		}
		else
		{
			// For depth/stencil case the primitive is invisible and the surroundings are filled red.
			refRenderer.colorClear(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
			refRenderer.draw(renderState, mapVkPrimitiveTopology(m_primitiveTopology), m_vertices);
		}

		if (!tcu::fuzzyCompare(m_context.getTestContext().getLog(), "FuzzyImageCompare", "Image comparison", refRenderer.getAccess(), result, 0.05f, tcu::COMPARE_LOG_RESULT))
			return tcu::TestStatus::fail("Primitive has unexpected shape");
	}

	return tcu::TestStatus::pass("Primitive rendered, unique colors within expected bounds");
}


// MinSampleShadingInstance

MinSampleShadingInstance::MinSampleShadingInstance (Context&									context,
													VkPrimitiveTopology							topology,
													float										pointSize,
													const std::vector<Vertex4RGBA>&				vertices,
													const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
													const VkPipelineColorBlendAttachmentState&	colorBlendState,
													ImageBackingMode							backingMode)
	: vkt::TestInstance			(context)
	, m_colorFormat				(VK_FORMAT_R8G8B8A8_UNORM)
	, m_renderSize				(32, 32)
	, m_primitiveTopology		(topology)
	, m_vertices				(vertices)
	, m_multisampleStateParams	(multisampleStateParams)
	, m_colorBlendState			(colorBlendState)
	, m_backingMode				(backingMode)
{
	DE_UNREF(pointSize);
}

tcu::TestStatus MinSampleShadingInstance::iterate (void)
{
	de::MovePtr<tcu::TextureLevel>	noSampleshadingImage;
	std::vector<tcu::TextureLevel>	sampleShadedImages;

	// Render and resolve without sample shading
	{
		VkPipelineMultisampleStateCreateInfo multisampleStateParms = m_multisampleStateParams;
		multisampleStateParms.sampleShadingEnable	= VK_FALSE;
		multisampleStateParms.minSampleShading		= 0.0;

		MultisampleRenderer renderer (m_context, m_colorFormat, m_renderSize, m_primitiveTopology, m_vertices, multisampleStateParms, m_colorBlendState, RENDER_TYPE_RESOLVE, m_backingMode);
		noSampleshadingImage  = renderer.render();
	}

	// Render with test minSampleShading and collect per-sample images
	{
		MultisampleRenderer renderer (m_context, m_colorFormat, m_renderSize, m_primitiveTopology, m_vertices, m_multisampleStateParams, m_colorBlendState, RENDER_TYPE_COPY_SAMPLES, m_backingMode);
		renderer.render();

		sampleShadedImages.resize(m_multisampleStateParams.rasterizationSamples);
		for (deUint32 sampleId = 0; sampleId < sampleShadedImages.size(); sampleId++)
		{
			sampleShadedImages[sampleId] = *renderer.getSingleSampledImage(sampleId);
		}
	}

	// Log images
	{
		tcu::TestLog& testLog	= m_context.getTestContext().getLog();

		testLog << tcu::TestLog::ImageSet("Images", "Images")
				<< tcu::TestLog::Image("noSampleshadingImage", "Image rendered without sample shading", noSampleshadingImage->getAccess());

		for (deUint32 sampleId = 0; sampleId < sampleShadedImages.size(); sampleId++)
		{
			testLog << tcu::TestLog::Image("sampleShadedImage", "One sample of sample shaded image", sampleShadedImages[sampleId].getAccess());
		}
		testLog << tcu::TestLog::EndImageSet;
	}

	return verifySampleShadedImage(sampleShadedImages, noSampleshadingImage->getAccess());
}

tcu::TestStatus MinSampleShadingInstance::verifySampleShadedImage (const std::vector<tcu::TextureLevel>& sampleShadedImages, const tcu::ConstPixelBufferAccess& noSampleshadingImage)
{
	const deUint32	pixelCount	= noSampleshadingImage.getWidth() * noSampleshadingImage.getHeight() * noSampleshadingImage.getDepth();

	bool anyPixelCovered		= false;

	for (deUint32 pixelNdx = 0; pixelNdx < pixelCount; pixelNdx++)
	{
		const deUint32 noSampleShadingValue = *((const deUint32*)noSampleshadingImage.getDataPtr() + pixelNdx);

		if (noSampleShadingValue == 0)
		{
			// non-covered pixel, continue
			continue;
		}
		else
		{
			anyPixelCovered = true;
		}

		int numNotCoveredSamples = 0;

		std::map<deUint32, deUint32>	histogram; // map<pixel value, number of occurrences>

		// Collect histogram of occurrences or each pixel across all samples
		for (size_t i = 0; i < sampleShadedImages.size(); ++i)
		{
			const deUint32 sampleShadedValue = *((const deUint32*)sampleShadedImages[i].getAccess().getDataPtr() + pixelNdx);

			if (sampleShadedValue == 0)
			{
				numNotCoveredSamples++;
				continue;
			}

			if (histogram.find(sampleShadedValue) != histogram.end())
				histogram[sampleShadedValue]++;
			else
				histogram[sampleShadedValue] = 1;
		}

		if (numNotCoveredSamples == static_cast<int>(sampleShadedImages.size()))
		{
			return tcu::TestStatus::fail("Got uncovered pixel, where covered samples were expected");
		}

		const int uniqueColorsCount				= (int)histogram.size();
		const int expectedUniqueSamplesCount	= static_cast<int>(m_multisampleStateParams.minSampleShading * static_cast<float>(sampleShadedImages.size()) + 0.5f);

		if (uniqueColorsCount + numNotCoveredSamples < expectedUniqueSamplesCount)
		{
			return tcu::TestStatus::fail("Got less unique colors than requested through minSampleShading");
		}
	}

	if (!anyPixelCovered)
	{
		return tcu::TestStatus::fail("Did not get any covered pixel, cannot test minSampleShading");
	}

	return tcu::TestStatus::pass("Got proper count of unique colors");
}

MinSampleShadingDisabledInstance::MinSampleShadingDisabledInstance	(Context&										context,
																	 VkPrimitiveTopology							topology,
																	 float											pointSize,
																	 const std::vector<Vertex4RGBA>&				vertices,
																	 const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
																	 const VkPipelineColorBlendAttachmentState&		blendState,
																	 ImageBackingMode								backingMode)
	: MinSampleShadingInstance	(context, topology, pointSize, vertices, multisampleStateParams, blendState, backingMode)
{
}

tcu::TestStatus MinSampleShadingDisabledInstance::verifySampleShadedImage	(const std::vector<tcu::TextureLevel>&	sampleShadedImages,
																			 const tcu::ConstPixelBufferAccess&		noSampleshadingImage)
{
	const deUint32		samplesCount		= (int)sampleShadedImages.size();
	const deUint32		width				= noSampleshadingImage.getWidth();
	const deUint32		height				= noSampleshadingImage.getHeight();
	const deUint32		depth				= noSampleshadingImage.getDepth();
	const tcu::UVec4	zeroPixel			= tcu::UVec4();
	bool				anyPixelCovered		= false;

	DE_ASSERT(depth == 1);
	DE_UNREF(depth);

	for (deUint32 y = 0; y < height; ++y)
	for (deUint32 x = 0; x < width; ++x)
	{
		const tcu::UVec4	noSampleShadingValue	= noSampleshadingImage.getPixelUint(x, y);

		if (noSampleShadingValue == zeroPixel)
			continue;

		anyPixelCovered = true;
		tcu::UVec4	sampleShadingValue	= tcu::UVec4();

		// Collect histogram of occurrences or each pixel across all samples
		for (size_t i = 0; i < samplesCount; ++i)
		{
			const tcu::UVec4	sampleShadedValue	= sampleShadedImages[i].getAccess().getPixelUint(x, y);

			sampleShadingValue += sampleShadedValue;
		}

		sampleShadingValue = sampleShadingValue / samplesCount;

		if (sampleShadingValue.w() != 255)
		{
			return tcu::TestStatus::fail("Invalid Alpha channel value");
		}

		if (sampleShadingValue != noSampleShadingValue)
		{
			return tcu::TestStatus::fail("Invalid color");
		}
	}

	if (!anyPixelCovered)
	{
		return tcu::TestStatus::fail("Did not get any covered pixel, cannot test minSampleShadingDisabled");
	}

	return tcu::TestStatus::pass("Got proper count of unique colors");
}

SampleMaskInstance::SampleMaskInstance (Context&										context,
										VkPrimitiveTopology								topology,
										float											pointSize,
										const std::vector<Vertex4RGBA>&					vertices,
										const VkPipelineMultisampleStateCreateInfo&		multisampleStateParams,
										const VkPipelineColorBlendAttachmentState&		blendState,
										ImageBackingMode								backingMode)
	: vkt::TestInstance			(context)
	, m_colorFormat				(VK_FORMAT_R8G8B8A8_UNORM)
	, m_renderSize				(32, 32)
	, m_primitiveTopology		(topology)
	, m_vertices				(vertices)
	, m_multisampleStateParams	(multisampleStateParams)
	, m_colorBlendState			(blendState)
	, m_backingMode				(backingMode)
{
	DE_UNREF(pointSize);
}

tcu::TestStatus SampleMaskInstance::iterate (void)
{
	de::MovePtr<tcu::TextureLevel>				testSampleMaskImage;
	de::MovePtr<tcu::TextureLevel>				minSampleMaskImage;
	de::MovePtr<tcu::TextureLevel>				maxSampleMaskImage;

	// Render with test flags
	{
		MultisampleRenderer renderer (m_context, m_colorFormat, m_renderSize, m_primitiveTopology, m_vertices, m_multisampleStateParams, m_colorBlendState, RENDER_TYPE_RESOLVE, m_backingMode);
		testSampleMaskImage = renderer.render();
	}

	// Render with all flags off
	{
		VkPipelineMultisampleStateCreateInfo	multisampleParams	= m_multisampleStateParams;
		const std::vector<VkSampleMask>			sampleMask			(multisampleParams.rasterizationSamples / 32, (VkSampleMask)0);

		multisampleParams.pSampleMask = sampleMask.data();

		MultisampleRenderer renderer (m_context, m_colorFormat, m_renderSize, m_primitiveTopology, m_vertices, multisampleParams, m_colorBlendState, RENDER_TYPE_RESOLVE, m_backingMode);
		minSampleMaskImage = renderer.render();
	}

	// Render with all flags on
	{
		VkPipelineMultisampleStateCreateInfo	multisampleParams	= m_multisampleStateParams;
		const std::vector<VkSampleMask>			sampleMask			(multisampleParams.rasterizationSamples / 32, ~((VkSampleMask)0));

		multisampleParams.pSampleMask = sampleMask.data();

		MultisampleRenderer renderer (m_context, m_colorFormat, m_renderSize, m_primitiveTopology, m_vertices, multisampleParams, m_colorBlendState, RENDER_TYPE_RESOLVE, m_backingMode);
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

tcu::TestStatus testRasterSamplesConsistency (Context& context, MultisampleTestParams params)
{
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

		MultisampleRenderer				renderer		(context, VK_FORMAT_R8G8B8A8_UNORM, tcu::IVec2(32, 32), VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, vertices, multisampleStateParams, getDefaultColorBlendAttachmentState(), RENDER_TYPE_RESOLVE, params.backingMode);
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
										const VkPipelineColorBlendAttachmentState&	blendState,
										ImageBackingMode							backingMode)
	: vkt::TestInstance			(context)
	, m_colorFormat				(VK_FORMAT_R8G8B8A8_UNORM)
	, m_renderSize				(32, 32)
	, m_primitiveTopology		(topology)
	, m_vertices				(vertices)
	, m_multisampleStateParams	(multisampleStateParams)
	, m_colorBlendState			(blendState)
	, m_backingMode				(backingMode)
{
}

tcu::TestStatus AlphaToOneInstance::iterate	(void)
{
	DE_ASSERT(m_multisampleStateParams.alphaToOneEnable);
	DE_ASSERT(m_colorBlendState.blendEnable);

	de::MovePtr<tcu::TextureLevel>	alphaOneImage;
	de::MovePtr<tcu::TextureLevel>	noAlphaOneImage;

	// Render with blend enabled and alpha to one on
	{
		MultisampleRenderer renderer (m_context, m_colorFormat, m_renderSize, m_primitiveTopology, m_vertices, m_multisampleStateParams, m_colorBlendState, RENDER_TYPE_RESOLVE, m_backingMode);
		alphaOneImage = renderer.render();
	}

	// Render with blend enabled and alpha to one off
	{
		VkPipelineMultisampleStateCreateInfo	multisampleParams	= m_multisampleStateParams;
		multisampleParams.alphaToOneEnable = false;

		MultisampleRenderer renderer (m_context, m_colorFormat, m_renderSize, m_primitiveTopology, m_vertices, multisampleParams, m_colorBlendState, RENDER_TYPE_RESOLVE, m_backingMode);
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
			if (alphaOneImage.getPixel(x, y).w() != 1.0)
			{
				std::ostringstream message;
				message << "Unsatisfied condition: " << alphaOneImage.getPixel(x, y) << " doesn't have alpha set to 1";
				return tcu::TestStatus::fail(message.str());
			}

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
												  GeometryType									geometryType,
												  ImageBackingMode								backingMode)
	: vkt::TestInstance			(context)
	, m_colorFormat				(VK_FORMAT_R8G8B8A8_UNORM)
	, m_renderSize				(32, 32)
	, m_primitiveTopology		(topology)
	, m_vertices				(vertices)
	, m_multisampleStateParams	(multisampleStateParams)
	, m_colorBlendState			(blendState)
	, m_geometryType			(geometryType)
	, m_backingMode				(backingMode)
{
}

tcu::TestStatus AlphaToCoverageInstance::iterate (void)
{
	DE_ASSERT(m_multisampleStateParams.alphaToCoverageEnable);

	de::MovePtr<tcu::TextureLevel>	result;
	MultisampleRenderer				renderer	(m_context, m_colorFormat, m_renderSize, m_primitiveTopology, m_vertices, m_multisampleStateParams, m_colorBlendState, RENDER_TYPE_RESOLVE, m_backingMode);

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

// AlphaToCoverageNoColorAttachmentInstance

AlphaToCoverageNoColorAttachmentInstance::AlphaToCoverageNoColorAttachmentInstance (Context&									context,
																					VkPrimitiveTopology							topology,
																					const std::vector<Vertex4RGBA>&				vertices,
																					const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
																					const VkPipelineColorBlendAttachmentState&	blendState,
																					GeometryType								geometryType,
																					ImageBackingMode							backingMode)
	: vkt::TestInstance			(context)
	, m_colorFormat				(VK_FORMAT_R8G8B8A8_UNORM)
	, m_depthStencilFormat		(VK_FORMAT_D16_UNORM)
	, m_renderSize				(32, 32)
	, m_primitiveTopology		(topology)
	, m_vertices				(vertices)
	, m_multisampleStateParams	(multisampleStateParams)
	, m_colorBlendState			(blendState)
	, m_geometryType			(geometryType)
	, m_backingMode				(backingMode)
{
}

tcu::TestStatus AlphaToCoverageNoColorAttachmentInstance::iterate (void)
{
	DE_ASSERT(m_multisampleStateParams.alphaToCoverageEnable);

	de::MovePtr<tcu::TextureLevel>	result;
	MultisampleRenderer				renderer	(m_context, m_colorFormat, m_depthStencilFormat, m_renderSize, true, false, 1u, &m_primitiveTopology, &m_vertices, m_multisampleStateParams, m_colorBlendState, RENDER_TYPE_DEPTHSTENCIL_ONLY, m_backingMode, 1.0f);

	result = renderer.render();

	return verifyImage(result->getAccess());
}

tcu::TestStatus AlphaToCoverageNoColorAttachmentInstance::verifyImage (const tcu::ConstPixelBufferAccess&	result)
{
	for (int y = 0; y < m_renderSize.y(); y++)
	{
		for (int x = 0; x < m_renderSize.x(); x++)
		{
			// Expect full red for each pixel. Fail if clear color is showing.
			if (result.getPixel(x, y).x() < 1.0f)
			{
				// Log result image when failing.
				m_context.getTestContext().getLog() << tcu::TestLog::ImageSet("Result", "Result image") << tcu::TestLog::Image("Rendered", "Rendered image", result) << tcu::TestLog::EndImageSet;

				return tcu::TestStatus::fail("Fail");
			}
		}
	}

	return tcu::TestStatus::pass("Pass");
}

// AlphaToCoverageColorUnusedAttachmentInstance

AlphaToCoverageColorUnusedAttachmentInstance::AlphaToCoverageColorUnusedAttachmentInstance (Context&									context,
																							VkPrimitiveTopology							topology,
																							const std::vector<Vertex4RGBA>&				vertices,
																							const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
																							const VkPipelineColorBlendAttachmentState&	blendState,
																							GeometryType								geometryType,
																							ImageBackingMode							backingMode)
	: vkt::TestInstance			(context)
	, m_colorFormat				(VK_FORMAT_R5G6B5_UNORM_PACK16)
	, m_renderSize				(32, 32)
	, m_primitiveTopology		(topology)
	, m_vertices				(vertices)
	, m_multisampleStateParams	(multisampleStateParams)
	, m_colorBlendState			(blendState)
	, m_geometryType			(geometryType)
	, m_backingMode				(backingMode)
{
}

tcu::TestStatus AlphaToCoverageColorUnusedAttachmentInstance::iterate (void)
{
	DE_ASSERT(m_multisampleStateParams.alphaToCoverageEnable);

	de::MovePtr<tcu::TextureLevel>	result;
	MultisampleRenderer				renderer	(m_context, m_colorFormat, m_renderSize, m_primitiveTopology, m_vertices, m_multisampleStateParams, m_colorBlendState, RENDER_TYPE_UNUSED_ATTACHMENT, m_backingMode);

	result = renderer.render();

	return verifyImage(result->getAccess());
}

tcu::TestStatus AlphaToCoverageColorUnusedAttachmentInstance::verifyImage (const tcu::ConstPixelBufferAccess&	result)
{
	for (int y = 0; y < m_renderSize.y(); y++)
	{
		for (int x = 0; x < m_renderSize.x(); x++)
		{
			// Quad color gets written to color buffer at location 1, and the alpha value to location 0 which is unused.
			// The coverage should still be affected by the alpha written to location 0.
			if ((m_geometryType == GEOMETRY_TYPE_OPAQUE_QUAD && result.getPixel(x, y).x() < 1.0f)
				|| (m_geometryType == GEOMETRY_TYPE_INVISIBLE_QUAD && result.getPixel(x, y).x() > 0.0f))
			{
				// Log result image when failing.
				m_context.getTestContext().getLog() << tcu::TestLog::ImageSet("Result", "Result image") << tcu::TestLog::Image("Rendered", "Rendered image", result) << tcu::TestLog::EndImageSet;

				return tcu::TestStatus::fail("Fail");
			}
		}
	}

	return tcu::TestStatus::pass("Pass");
}

// SampleMaskWithDepthTestInstance

SampleMaskWithDepthTestInstance::SampleMaskWithDepthTestInstance (Context&						context,
																  const VkSampleCountFlagBits	rasterizationSamples,
																  const bool					enablePostDepthCoverage)
	: vkt::TestInstance			(context)
	, m_rasterizationSamples	(rasterizationSamples)
	, m_enablePostDepthCoverage	(enablePostDepthCoverage)
	, m_colorFormat				(VK_FORMAT_R8G8B8A8_UNORM)
	, m_depthStencilFormat		(VK_FORMAT_D16_UNORM)
	, m_renderSize				(tcu::IVec2(3, 3))
	, m_useDepth				(true)
	, m_useStencil				(false)
	, m_topology				(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
	, m_renderColor				(tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f))
	, m_vertices				(generateVertices())
	, m_multisampleStateParams	(getMultisampleState(rasterizationSamples))
	, m_blendState				(getDefaultColorBlendAttachmentState())
	, m_renderType				(RENDER_TYPE_RESOLVE)
	, m_imageBackingMode		(IMAGE_BACKING_MODE_REGULAR)
	, m_depthClearValue			(0.667f)
{
	m_refCoverageAfterDepthTest[VK_SAMPLE_COUNT_2_BIT]	= SampleCoverage(1u, 1u);	// !< Sample coverage of the diagonally halved pixel,
	m_refCoverageAfterDepthTest[VK_SAMPLE_COUNT_4_BIT]	= SampleCoverage(2u, 2u);	// !< with max possible subPixelPrecisionBits threshold
	m_refCoverageAfterDepthTest[VK_SAMPLE_COUNT_8_BIT]	= SampleCoverage(2u, 6u);	// !<
	m_refCoverageAfterDepthTest[VK_SAMPLE_COUNT_16_BIT]	= SampleCoverage(6u, 11u);	// !<
}

tcu::TestStatus SampleMaskWithDepthTestInstance::iterate (void)
{
	de::MovePtr<tcu::TextureLevel>	result;

	MultisampleRenderer renderer (m_context, m_colorFormat, m_depthStencilFormat, m_renderSize, m_useDepth, m_useStencil, 1u, &m_topology,
								  &m_vertices, m_multisampleStateParams, m_blendState, m_renderType, m_imageBackingMode, m_depthClearValue);
	result = renderer.render();

	return verifyImage(result->getAccess());
}

VkPipelineMultisampleStateCreateInfo SampleMaskWithDepthTestInstance::getMultisampleState (const VkSampleCountFlagBits rasterizationSamples)
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

std::vector<Vertex4RGBA> SampleMaskWithDepthTestInstance::generateVertices (void)
{
	std::vector<Vertex4RGBA> vertices;

	{
		const Vertex4RGBA vertexInput = { tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f), m_renderColor };
		vertices.push_back(vertexInput);
	}
	{
		const Vertex4RGBA vertexInput = { tcu::Vec4(1.0f, -1.0f, 1.0f, 1.0f), m_renderColor };
		vertices.push_back(vertexInput);
	}
	{
		const Vertex4RGBA vertexInput = { tcu::Vec4(-1.0f,  1.0f, 1.0f, 1.0f), m_renderColor };
		vertices.push_back(vertexInput);
	}

	return vertices;
}

tcu::TestStatus SampleMaskWithDepthTestInstance::verifyImage (const tcu::ConstPixelBufferAccess& result)
{
	bool			pass	= true;
	const int		width	= result.getWidth();
	const int		height	= result.getHeight();
	tcu::TestLog&	log		= m_context.getTestContext().getLog();

	DE_ASSERT(width == 3);
	DE_ASSERT(height == 3);

	const tcu::Vec4 clearColor = tcu::Vec4(0.0f);

	for (int x = 0; x < width; ++x)
	for (int y = 0; y < height; ++y)
	{
		const tcu::Vec4 resultPixel = result.getPixel(x, y);

		if (x + y == 0)
		{
			if (resultPixel != m_renderColor)
			{
				log << tcu::TestLog::Message << "x: " << x << " y: " << y << " Result: " << resultPixel
					<< " Reference: " << m_renderColor << tcu::TestLog::EndMessage;
				pass = false;
			}
		}
		else if (x + y == 1)
		{
			// default: m_rasterizationSamples bits set in FS's gl_SampleMaskIn[0] (before depth test)
			// post_depth_coverage: m_refCoverageAfterDepthTest[m_rasterizationSamples] bits set in FS's gl_SampleMaskIn[0] (after depth test)
			const float		threshold	= 0.02f;
			const float		minCoverage	= (m_enablePostDepthCoverage ? (float)m_refCoverageAfterDepthTest[m_rasterizationSamples].min / (float)m_rasterizationSamples : 1.0f)
										* ((float)m_refCoverageAfterDepthTest[m_rasterizationSamples].min / (float)m_rasterizationSamples);
			const float		maxCoverage	= (m_enablePostDepthCoverage ? (float)m_refCoverageAfterDepthTest[m_rasterizationSamples].max / (float)m_rasterizationSamples : 1.0f)
										* ((float)m_refCoverageAfterDepthTest[m_rasterizationSamples].max / (float)m_rasterizationSamples);

			bool			localPass	= true;
			for (deUint32 componentNdx = 0u; componentNdx < m_renderColor.SIZE; ++componentNdx)
			{
				if (m_renderColor[componentNdx] != 0.0f && (resultPixel[componentNdx] <= m_renderColor[componentNdx] * (minCoverage - threshold)
															|| resultPixel[componentNdx] >= m_renderColor[componentNdx] * (maxCoverage + threshold)))
					localPass = false;
			}

			if (!localPass)
			{
				log << tcu::TestLog::Message << "x: " << x << " y: " << y << " Result: " << resultPixel
					<< " Reference range ( " << m_renderColor * (minCoverage - threshold) << " ; " << m_renderColor * (maxCoverage + threshold) << " )" << tcu::TestLog::EndMessage;
				pass = false;
			}
		}
		else
		{
			if (resultPixel != clearColor)
			{
				log << tcu::TestLog::Message << "x: " << x << " y: " << y << " Result: " << resultPixel
					<< " Reference: " << clearColor << tcu::TestLog::EndMessage;
				pass = false;
			}
		}
	}

	if (pass)
		return tcu::TestStatus::pass("Passed");
	else
		return tcu::TestStatus::fail("Failed");
}

// MultisampleRenderer

MultisampleRenderer::MultisampleRenderer (Context&										context,
										  const VkFormat								colorFormat,
										  const tcu::IVec2&								renderSize,
										  const VkPrimitiveTopology						topology,
										  const std::vector<Vertex4RGBA>&				vertices,
										  const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
										  const VkPipelineColorBlendAttachmentState&	blendState,
										  const RenderType								renderType,
										  const ImageBackingMode						backingMode)
	: m_context					(context)
	, m_bindSemaphore			(createSemaphore(context.getDeviceInterface(), context.getDevice()))
	, m_colorFormat				(colorFormat)
	, m_depthStencilFormat		(VK_FORMAT_UNDEFINED)
	, m_renderSize				(renderSize)
	, m_useDepth				(false)
	, m_useStencil				(false)
	, m_multisampleStateParams	(multisampleStateParams)
	, m_colorBlendState			(blendState)
	, m_renderType				(renderType)
	, m_backingMode				(backingMode)
	, m_depthClearValue			(1.0f)
{
	initialize(context, 1u, &topology, &vertices);
}

MultisampleRenderer::MultisampleRenderer (Context&										context,
										  const VkFormat								colorFormat,
										  const VkFormat								depthStencilFormat,
										  const tcu::IVec2&								renderSize,
										  const bool									useDepth,
										  const bool									useStencil,
										  const deUint32								numTopologies,
										  const VkPrimitiveTopology*					pTopology,
										  const std::vector<Vertex4RGBA>*				pVertices,
										  const VkPipelineMultisampleStateCreateInfo&	multisampleStateParams,
										  const VkPipelineColorBlendAttachmentState&	blendState,
										  const RenderType								renderType,
										  const ImageBackingMode						backingMode,
										  const float									depthClearValue)
	: m_context					(context)
	, m_bindSemaphore			(createSemaphore(context.getDeviceInterface(), context.getDevice()))
	, m_colorFormat				(colorFormat)
	, m_depthStencilFormat		(depthStencilFormat)
	, m_renderSize				(renderSize)
	, m_useDepth				(useDepth)
	, m_useStencil				(useStencil)
	, m_multisampleStateParams	(multisampleStateParams)
	, m_colorBlendState			(blendState)
	, m_renderType				(renderType)
	, m_backingMode				(backingMode)
	, m_depthClearValue			(depthClearValue)
{
	initialize(context, numTopologies, pTopology, pVertices);
}

void MultisampleRenderer::initialize (Context&									context,
									  const deUint32							numTopologies,
									  const VkPrimitiveTopology*				pTopology,
									  const std::vector<Vertex4RGBA>*			pVertices)
{
	if (!isSupportedSampleCount(context.getInstanceInterface(), context.getPhysicalDevice(), m_multisampleStateParams.rasterizationSamples))
		throw tcu::NotSupportedError("Unsupported number of rasterization samples");

	const DeviceInterface&			vk						= context.getDeviceInterface();
	const VkDevice					vkDevice				= context.getDevice();
	const VkPhysicalDeviceFeatures	features				= context.getDeviceFeatures();
	const deUint32					queueFamilyIndices[]	= { context.getUniversalQueueFamilyIndex(), context.getSparseQueueFamilyIndex() };
	const bool						sparse					= m_backingMode == IMAGE_BACKING_MODE_SPARSE;
	const VkComponentMapping		componentMappingRGBA	= { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
	const VkImageCreateFlags		imageCreateFlags		= sparse ? (VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT) : 0u;
	const VkSharingMode				sharingMode				= (sparse && context.getUniversalQueueFamilyIndex() != context.getSparseQueueFamilyIndex()) ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
	Allocator&						memAlloc				= m_context.getDefaultAllocator();
	const bool						usesResolveImage		= m_renderType == RENDER_TYPE_RESOLVE || m_renderType == RENDER_TYPE_DEPTHSTENCIL_ONLY || m_renderType == RENDER_TYPE_UNUSED_ATTACHMENT;

	if (sparse)
	{
		bool sparseSamplesSupported = false;
		switch(m_multisampleStateParams.rasterizationSamples)
		{
			case VK_SAMPLE_COUNT_2_BIT:
				sparseSamplesSupported = features.sparseResidency2Samples;
				break;
			case VK_SAMPLE_COUNT_4_BIT:
				sparseSamplesSupported = features.sparseResidency4Samples;
				break;
			case VK_SAMPLE_COUNT_8_BIT:
				sparseSamplesSupported = features.sparseResidency8Samples;
				break;
			case VK_SAMPLE_COUNT_16_BIT:
				sparseSamplesSupported = features.sparseResidency16Samples;
				break;
			default:
				break;
		}

		if (!sparseSamplesSupported)
			throw tcu::NotSupportedError("Unsupported number of rasterization samples for sparse residency");
	}

	if (sparse && !context.getDeviceFeatures().sparseBinding)
		throw tcu::NotSupportedError("No sparseBinding support");

	// Create color image
	{
		const VkImageUsageFlags	imageUsageFlags		= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
			(m_renderType == RENDER_TYPE_COPY_SAMPLES ? VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT : (VkImageUsageFlagBits)0u);

		const VkImageCreateInfo colorImageParams	=
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,										// VkStructureType			sType;
			DE_NULL,																	// const void*				pNext;
			imageCreateFlags,															// VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,															// VkImageType				imageType;
			m_colorFormat,																// VkFormat					format;
			{ (deUint32)m_renderSize.x(), (deUint32)m_renderSize.y(), 1u },				// VkExtent3D				extent;
			1u,																			// deUint32					mipLevels;
			1u,																			// deUint32					arrayLayers;
			m_multisampleStateParams.rasterizationSamples,								// VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,													// VkImageTiling			tiling;
			imageUsageFlags,															// VkImageUsageFlags		usage;
			sharingMode,																// VkSharingMode			sharingMode;
			sharingMode == VK_SHARING_MODE_CONCURRENT ? 2u : 1u,						// deUint32					queueFamilyIndexCount;
			queueFamilyIndices,															// const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED,													// VkImageLayout			initialLayout;
		};

		if (sparse && !checkSparseImageFormatSupport(context.getPhysicalDevice(), context.getInstanceInterface(), colorImageParams))
			TCU_THROW(NotSupportedError, "The image format does not support sparse operations.");

		m_colorImage = createImage(vk, vkDevice, &colorImageParams);

		// Allocate and bind color image memory
		if (sparse)
		{
			allocateAndBindSparseImage(vk, vkDevice, context.getPhysicalDevice(), context.getInstanceInterface(), colorImageParams, *m_bindSemaphore, context.getSparseQueue(), memAlloc, m_allocations, mapVkFormat(m_colorFormat), *m_colorImage);
		}
		else
		{
			m_colorImageAlloc = memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_colorImage), MemoryRequirement::Any);
			VK_CHECK(vk.bindImageMemory(vkDevice, *m_colorImage, m_colorImageAlloc->getMemory(), m_colorImageAlloc->getOffset()));
		}
	}

	// Create resolve image
	if (usesResolveImage)
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
			queueFamilyIndices,																// const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED														// VkImageLayout			initialLayout;
		};

		m_resolveImage = createImage(vk, vkDevice, &resolveImageParams);

		// Allocate and bind resolve image memory
		m_resolveImageAlloc = memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_resolveImage), MemoryRequirement::Any);
		VK_CHECK(vk.bindImageMemory(vkDevice, *m_resolveImage, m_resolveImageAlloc->getMemory(), m_resolveImageAlloc->getOffset()));

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
	}

	// Create per-sample output images
	if (m_renderType == RENDER_TYPE_COPY_SAMPLES)
	{
		const VkImageCreateInfo perSampleImageParams =
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
			queueFamilyIndices,																// const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED														// VkImageLayout			initialLayout;
		};

		m_perSampleImages.resize(static_cast<size_t>(m_multisampleStateParams.rasterizationSamples));

		for (size_t i = 0; i < m_perSampleImages.size(); ++i)
		{
			m_perSampleImages[i]	= de::SharedPtr<PerSampleImage>(new PerSampleImage);
			PerSampleImage& image	= *m_perSampleImages[i];

			image.m_image			= createImage(vk, vkDevice, &perSampleImageParams);

			// Allocate and bind image memory
			image.m_imageAlloc		= memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *image.m_image), MemoryRequirement::Any);
			VK_CHECK(vk.bindImageMemory(vkDevice, *image.m_image, image.m_imageAlloc->getMemory(), image.m_imageAlloc->getOffset()));

			// Create per-sample attachment view
			{
				const VkImageViewCreateInfo perSampleAttachmentViewParams =
				{
					VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,		// VkStructureType			sType;
					DE_NULL,										// const void*				pNext;
					0u,												// VkImageViewCreateFlags	flags;
					*image.m_image,									// VkImage					image;
					VK_IMAGE_VIEW_TYPE_2D,							// VkImageViewType			viewType;
					m_colorFormat,									// VkFormat					format;
					componentMappingRGBA,							// VkComponentMapping		components;
					{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u }	// VkImageSubresourceRange	subresourceRange;
				};

				image.m_attachmentView = createImageView(vk, vkDevice, &perSampleAttachmentViewParams);
			}
		}
	}

	// Create a depth/stencil image
	if (m_useDepth || m_useStencil)
	{
		const VkImageCreateInfo depthStencilImageParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,											// VkStructureType			sType;
			DE_NULL,																		// const void*				pNext;
			0u,																				// VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,																// VkImageType				imageType;
			m_depthStencilFormat,															// VkFormat					format;
			{ (deUint32)m_renderSize.x(), (deUint32)m_renderSize.y(), 1u },					// VkExtent3D				extent;
			1u,																				// deUint32					mipLevels;
			1u,																				// deUint32					arrayLayers;
			m_multisampleStateParams.rasterizationSamples,									// VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,														// VkImageTiling			tiling;
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,									// VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,														// VkSharingMode			sharingMode;
			1u,																				// deUint32					queueFamilyIndexCount;
			queueFamilyIndices,																// const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED														// VkImageLayout			initialLayout;
		};

		m_depthStencilImage = createImage(vk, vkDevice, &depthStencilImageParams);

		// Allocate and bind depth/stencil image memory
		m_depthStencilImageAlloc = memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_depthStencilImage), MemoryRequirement::Any);
		VK_CHECK(vk.bindImageMemory(vkDevice, *m_depthStencilImage, m_depthStencilImageAlloc->getMemory(), m_depthStencilImageAlloc->getOffset()));
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

	VkImageAspectFlags	depthStencilAttachmentAspect	= (VkImageAspectFlagBits)0;

	// Create depth/stencil attachment view
	if (m_useDepth || m_useStencil)
	{
		depthStencilAttachmentAspect = getImageAspectFlags(m_depthStencilFormat);

		const VkImageViewCreateInfo depthStencilAttachmentViewParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,			// VkStructureType			sType;
			DE_NULL,											// const void*				pNext;
			0u,													// VkImageViewCreateFlags	flags;
			*m_depthStencilImage,								// VkImage					image;
			VK_IMAGE_VIEW_TYPE_2D,								// VkImageViewType			viewType;
			m_depthStencilFormat,								// VkFormat					format;
			componentMappingRGBA,								// VkComponentMapping		components;
			{ depthStencilAttachmentAspect, 0u, 1u, 0u, 1u }	// VkImageSubresourceRange	subresourceRange;
		};

		m_depthStencilAttachmentView = createImageView(vk, vkDevice, &depthStencilAttachmentViewParams);
	}

	// Create render pass
	{
		std::vector<VkAttachmentDescription> attachmentDescriptions;
		{
			const VkAttachmentDescription colorAttachmentDescription =
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
			};
			attachmentDescriptions.push_back(colorAttachmentDescription);
		}

		deUint32 resolveAttachmentIndex = VK_ATTACHMENT_UNUSED;

		if (usesResolveImage)
		{
			resolveAttachmentIndex = static_cast<deUint32>(attachmentDescriptions.size());

			const VkAttachmentDescription resolveAttachmentDescription =
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
			};
			attachmentDescriptions.push_back(resolveAttachmentDescription);
		}

		deUint32 perSampleAttachmentIndex = VK_ATTACHMENT_UNUSED;

		if (m_renderType == RENDER_TYPE_COPY_SAMPLES)
		{
			perSampleAttachmentIndex = static_cast<deUint32>(attachmentDescriptions.size());

			const VkAttachmentDescription perSampleAttachmentDescription =
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
			};

			for (size_t i = 0; i < m_perSampleImages.size(); ++i)
			{
				attachmentDescriptions.push_back(perSampleAttachmentDescription);
			}
		}

		deUint32 depthStencilAttachmentIndex = VK_ATTACHMENT_UNUSED;

		if (m_useDepth || m_useStencil)
		{
			depthStencilAttachmentIndex = static_cast<deUint32>(attachmentDescriptions.size());

			const VkAttachmentDescription depthStencilAttachmentDescription =
			{
				0u,																					// VkAttachmentDescriptionFlags		flags;
				m_depthStencilFormat,																// VkFormat							format;
				m_multisampleStateParams.rasterizationSamples,										// VkSampleCountFlagBits			samples;
				(m_useDepth ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE),		// VkAttachmentLoadOp				loadOp;
				(m_useDepth ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE),		// VkAttachmentStoreOp				storeOp;
				(m_useStencil ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE),		// VkAttachmentStoreOp				stencilLoadOp;
				(m_useStencil ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE),	// VkAttachmentStoreOp				stencilStoreOp;
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,									// VkImageLayout					initialLayout;
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL									// VkImageLayout					finalLayout;
			};
			attachmentDescriptions.push_back(depthStencilAttachmentDescription);
		};

		const VkAttachmentReference colorAttachmentReference =
		{
			0u,													// deUint32			attachment;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL			// VkImageLayout	layout;
		};

		const VkAttachmentReference inputAttachmentReference =
		{
			0u,													// deUint32			attachment;
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL			// VkImageLayout	layout;
		};

		const VkAttachmentReference resolveAttachmentReference =
		{
			resolveAttachmentIndex,								// deUint32			attachment;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL			// VkImageLayout	layout;
		};

		const VkAttachmentReference colorAttachmentReferencesUnusedAttachment[] =
		{
			{
				VK_ATTACHMENT_UNUSED,		// deUint32			attachment
				VK_IMAGE_LAYOUT_UNDEFINED	// VkImageLayout	layout
			},
			{
				0u,											// deUint32			attachment
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout	layout
			}
		};

		const VkAttachmentReference resolveAttachmentReferencesUnusedAttachment[] =
		{
			{
				VK_ATTACHMENT_UNUSED,		// deUint32			attachment
				VK_IMAGE_LAYOUT_UNDEFINED	// VkImageLayout	layout
			},
			{
				resolveAttachmentIndex,						// deUint32			attachment
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout	layout
			}
		};

		std::vector<VkAttachmentReference> perSampleAttachmentReferences(m_perSampleImages.size());
		if (m_renderType == RENDER_TYPE_COPY_SAMPLES)
		{
			for (size_t i = 0; i < m_perSampleImages.size(); ++i)
			{
				const VkAttachmentReference perSampleAttachmentReference =
				{
					perSampleAttachmentIndex + static_cast<deUint32>(i),	// deUint32			attachment;
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL				// VkImageLayout	layout;
				};
				perSampleAttachmentReferences[i] = perSampleAttachmentReference;
			}
		}

		const VkAttachmentReference depthStencilAttachmentReference =
		{
			depthStencilAttachmentIndex,						// deUint32			attachment;
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL	// VkImageLayout	layout;
		};

		std::vector<VkSubpassDescription>	subpassDescriptions;
		std::vector<VkSubpassDependency>	subpassDependencies;

		if (m_renderType == RENDER_TYPE_DEPTHSTENCIL_ONLY)
		{
				const VkSubpassDescription	subpassDescription0	=
				{
					0u,										// VkSubpassDescriptionFlags	flags
					VK_PIPELINE_BIND_POINT_GRAPHICS,		// VkPipelineBindPoint			pipelineBindPoint
					0u,										// deUint32						inputAttachmentCount
					DE_NULL,								// const VkAttachmentReference*	pInputAttachments
					0u,										// deUint32						colorAttachmentCount
					DE_NULL,								// const VkAttachmentReference*	pColorAttachments
					DE_NULL,								// const VkAttachmentReference*	pResolveAttachments
					&depthStencilAttachmentReference,		// const VkAttachmentReference*	pDepthStencilAttachment
					0u,										// deUint32						preserveAttachmentCount
					DE_NULL									// const VkAttachmentReference*	pPreserveAttachments
				};

				const VkSubpassDescription	subpassDescription1	=
				{
					0u,									// VkSubpassDescriptionFlags	flags
					VK_PIPELINE_BIND_POINT_GRAPHICS,	// VkPipelineBindPoint			pipelineBindPoint
					0u,									// deUint32						inputAttachmentCount
					DE_NULL,							// const VkAttachmentReference*	pInputAttachments
					1u,									// deUint32						colorAttachmentCount
					&colorAttachmentReference,			// const VkAttachmentReference*	pColorAttachments
					&resolveAttachmentReference,		// const VkAttachmentReference*	pResolveAttachments
					&depthStencilAttachmentReference,	// const VkAttachmentReference*	pDepthStencilAttachment
					0u,									// deUint32						preserveAttachmentCount
					DE_NULL								// const VkAttachmentReference*	pPreserveAttachments
				};

				const VkSubpassDependency	subpassDependency	=
				{
					0u,												// deUint32				srcSubpass
					1u,												// deUint32				dstSubpass
					VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,		// VkPipelineStageFlags	srcStageMask
					VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,		// VkPipelineStageFlags	dstStageMask
					VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,	// VkAccessFlags		srcAccessMask
					VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,	// VkAccessFlags		dstAccessMask
					0u												// VkDependencyFlags	dependencyFlags
				};

				subpassDescriptions.push_back(subpassDescription0);
				subpassDescriptions.push_back(subpassDescription1);
				subpassDependencies.push_back(subpassDependency);
		}
		else if (m_renderType == RENDER_TYPE_UNUSED_ATTACHMENT)
		{
			const VkSubpassDescription renderSubpassDescription =
			{
				0u,												// VkSubpassDescriptionFlags	flags
				VK_PIPELINE_BIND_POINT_GRAPHICS,				// VkPipelineBindPoint			pipelineBindPoint
				0u,												// deUint32						inputAttachmentCount
				DE_NULL,										// const VkAttachmentReference*	pInputAttachments
				2u,												// deUint32						colorAttachmentCount
				colorAttachmentReferencesUnusedAttachment,		// const VkAttachmentReference*	pColorAttachments
				resolveAttachmentReferencesUnusedAttachment,	// const VkAttachmentReference*	pResolveAttachments
				DE_NULL,										// const VkAttachmentReference*	pDepthStencilAttachment
				0u,												// deUint32						preserveAttachmentCount
				DE_NULL											// const VkAttachmentReference*	pPreserveAttachments
			};

			subpassDescriptions.push_back(renderSubpassDescription);
		}
		else
		{
			{
				const VkSubpassDescription renderSubpassDescription =
				{
					0u,																				// VkSubpassDescriptionFlags	flags;
					VK_PIPELINE_BIND_POINT_GRAPHICS,												// VkPipelineBindPoint			pipelineBindPoint;
					0u,																				// deUint32						inputAttachmentCount;
					DE_NULL,																		// const VkAttachmentReference*	pInputAttachments;
					1u,																				// deUint32						colorAttachmentCount;
					&colorAttachmentReference,														// const VkAttachmentReference*	pColorAttachments;
					usesResolveImage ? &resolveAttachmentReference : DE_NULL,						// const VkAttachmentReference*	pResolveAttachments;
					(m_useDepth || m_useStencil ? &depthStencilAttachmentReference : DE_NULL),		// const VkAttachmentReference*	pDepthStencilAttachment;
					0u,																				// deUint32						preserveAttachmentCount;
					DE_NULL																			// const VkAttachmentReference*	pPreserveAttachments;
				};
				subpassDescriptions.push_back(renderSubpassDescription);
			}

			if (m_renderType == RENDER_TYPE_COPY_SAMPLES)
			{

				for (size_t i = 0; i < m_perSampleImages.size(); ++i)
				{
					const VkSubpassDescription copySampleSubpassDescription =
					{
						0u,													// VkSubpassDescriptionFlags		flags;
						VK_PIPELINE_BIND_POINT_GRAPHICS,					// VkPipelineBindPoint				pipelineBindPoint;
						1u,													// deUint32							inputAttachmentCount;
						&inputAttachmentReference,							// const VkAttachmentReference*		pInputAttachments;
						1u,													// deUint32							colorAttachmentCount;
						&perSampleAttachmentReferences[i],					// const VkAttachmentReference*		pColorAttachments;
						DE_NULL,											// const VkAttachmentReference*		pResolveAttachments;
						DE_NULL,											// const VkAttachmentReference*		pDepthStencilAttachment;
						0u,													// deUint32							preserveAttachmentCount;
						DE_NULL												// const VkAttachmentReference*		pPreserveAttachments;
					};
					subpassDescriptions.push_back(copySampleSubpassDescription);

					const VkSubpassDependency copySampleSubpassDependency =
					{
						0u,													// deUint32							srcSubpass
						1u + static_cast<deUint32>(i),						// deUint32							dstSubpass
						VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,		// VkPipelineStageFlags				srcStageMask
						VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,				// VkPipelineStageFlags				dstStageMask
						VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,				// VkAccessFlags					srcAccessMask
						VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,				// VkAccessFlags					dstAccessMask
						0u,													// VkDependencyFlags				dependencyFlags
					};
					subpassDependencies.push_back(copySampleSubpassDependency);
				}
				// the very last sample pass must synchronize with all prior subpasses
				for (size_t i = 0; i < (m_perSampleImages.size() - 1); ++i)
				{
					const VkSubpassDependency storeSubpassDependency =
					{
						1u + static_cast<deUint32>(i),						// deUint32							srcSubpass
						static_cast<deUint32>(m_perSampleImages.size()),    // deUint32							dstSubpass
						VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,		// VkPipelineStageFlags				srcStageMask
						VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,				// VkPipelineStageFlags				dstStageMask
						VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,				// VkAccessFlags					srcAccessMask
						VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,				// VkAccessFlags					dstAccessMask
						0u,													// VkDependencyFlags				dependencyFlags
					};
					subpassDependencies.push_back(storeSubpassDependency);
				}
			}
		}

		const VkRenderPassCreateInfo renderPassParams =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,					// VkStructureType					sType;
			DE_NULL,													// const void*						pNext;
			0u,															// VkRenderPassCreateFlags			flags;
			(deUint32)attachmentDescriptions.size(),					// deUint32							attachmentCount;
			&attachmentDescriptions[0],									// const VkAttachmentDescription*	pAttachments;
			(deUint32)subpassDescriptions.size(),						// deUint32							subpassCount;
			&subpassDescriptions[0],									// const VkSubpassDescription*		pSubpasses;
			(deUint32)subpassDependencies.size(),						// deUint32							dependencyCount;
			subpassDependencies.size() != 0 ? &subpassDependencies[0] : DE_NULL
		};

		m_renderPass = createRenderPass(vk, vkDevice, &renderPassParams);
	}

	// Create framebuffer
	{
		std::vector<VkImageView> attachments;
		attachments.push_back(*m_colorAttachmentView);
		if (usesResolveImage)
		{
			attachments.push_back(*m_resolveAttachmentView);
		}
		if (m_renderType == RENDER_TYPE_COPY_SAMPLES)
		{
			for (size_t i = 0; i < m_perSampleImages.size(); ++i)
			{
				attachments.push_back(*m_perSampleImages[i]->m_attachmentView);
			}
		}

		if (m_useDepth || m_useStencil)
		{
			attachments.push_back(*m_depthStencilAttachmentView);
		}

		const VkFramebufferCreateInfo framebufferParams =
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,			// VkStructureType					sType;
			DE_NULL,											// const void*						pNext;
			0u,													// VkFramebufferCreateFlags			flags;
			*m_renderPass,										// VkRenderPass						renderPass;
			(deUint32)attachments.size(),						// deUint32							attachmentCount;
			&attachments[0],									// const VkImageView*				pAttachments;
			(deUint32)m_renderSize.x(),							// deUint32							width;
			(deUint32)m_renderSize.y(),							// deUint32							height;
			1u													// deUint32							layers;
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

		if (m_renderType == RENDER_TYPE_COPY_SAMPLES)
		{

			// Create descriptor set layout
			const VkDescriptorSetLayoutBinding		layoutBinding					=
			{
				0u,															// deUint32								binding;
				VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,						// VkDescriptorType						descriptorType;
				1u,															// deUint32								descriptorCount;
				VK_SHADER_STAGE_FRAGMENT_BIT,								// VkShaderStageFlags					stageFlags;
				DE_NULL,													// const VkSampler*						pImmutableSamplers;
			};

			const VkDescriptorSetLayoutCreateInfo	descriptorSetLayoutParams		=
			{
				VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,		// VkStructureType						sType
				DE_NULL,													// const void*							pNext
				0u,															// VkDescriptorSetLayoutCreateFlags		flags
				1u,															// deUint32								bindingCount
				&layoutBinding												// const VkDescriptorSetLayoutBinding*	pBindings
			};
			m_copySampleDesciptorLayout	= createDescriptorSetLayout(vk, vkDevice, &descriptorSetLayoutParams);

			// Create pipeline layout

			const VkPushConstantRange				pushConstantRange				=
			{
				VK_SHADER_STAGE_FRAGMENT_BIT,								// VkShaderStageFlags					stageFlags;
				0u,															// deUint32								offset;
				sizeof(deInt32)												// deUint32								size;
			};
			const VkPipelineLayoutCreateInfo		copySamplePipelineLayoutParams	=
			{
				VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,				// VkStructureType						sType;
				DE_NULL,													// const void*							pNext;
				0u,															// VkPipelineLayoutCreateFlags			flags;
				1u,															// deUint32								setLayoutCount;
				&m_copySampleDesciptorLayout.get(),							// const VkDescriptorSetLayout*			pSetLayouts;
				1u,															// deUint32								pushConstantRangeCount;
				&pushConstantRange											// const VkPushConstantRange*			pPushConstantRanges;
			};
			m_copySamplePipelineLayout		= createPipelineLayout(vk, vkDevice, &copySamplePipelineLayoutParams);
		}
	}

	m_vertexShaderModule	= createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("color_vert"), 0);
	m_fragmentShaderModule	= createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("color_frag"), 0);

	if (m_renderType == RENDER_TYPE_COPY_SAMPLES)
	{
		m_copySampleVertexShaderModule		= createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("quad_vert"), 0);
		m_copySampleFragmentShaderModule	= createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("copy_sample_frag"), 0);
	}

	// Create pipeline
	{
		const VkVertexInputBindingDescription	vertexInputBindingDescription =
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

		const std::vector<VkViewport>	viewports		(1, makeViewport(m_renderSize));
		const std::vector<VkRect2D>		scissors		(1, makeRect2D(m_renderSize));

		const deUint32					attachmentCount	= m_renderType == RENDER_TYPE_UNUSED_ATTACHMENT ? 2u : 1u;

		std::vector<VkPipelineColorBlendAttachmentState> attachments;

		for (deUint32 attachmentIdx = 0; attachmentIdx < attachmentCount; attachmentIdx++)
			attachments.push_back(m_colorBlendState);

		const VkPipelineColorBlendStateCreateInfo colorBlendStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType								sType;
			DE_NULL,													// const void*									pNext;
			0u,															// VkPipelineColorBlendStateCreateFlags			flags;
			false,														// VkBool32										logicOpEnable;
			VK_LOGIC_OP_COPY,											// VkLogicOp									logicOp;
			attachmentCount,											// deUint32										attachmentCount;
			attachments.data(),											// const VkPipelineColorBlendAttachmentState*	pAttachments;
			{ 0.0f, 0.0f, 0.0f, 0.0f }									// float										blendConstants[4];
		};

		const VkStencilOpState stencilOpState =
		{
			VK_STENCIL_OP_KEEP,						// VkStencilOp	failOp;
			VK_STENCIL_OP_REPLACE,					// VkStencilOp	passOp;
			VK_STENCIL_OP_KEEP,						// VkStencilOp	depthFailOp;
			VK_COMPARE_OP_GREATER,					// VkCompareOp	compareOp;
			1u,										// deUint32		compareMask;
			1u,										// deUint32		writeMask;
			1u,										// deUint32		reference;
		};

		const VkPipelineDepthStencilStateCreateInfo depthStencilStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,	// VkStructureType							sType;
			DE_NULL,													// const void*								pNext;
			0u,															// VkPipelineDepthStencilStateCreateFlags	flags;
			m_useDepth,													// VkBool32									depthTestEnable;
			m_useDepth,													// VkBool32									depthWriteEnable;
			VK_COMPARE_OP_LESS,											// VkCompareOp								depthCompareOp;
			false,														// VkBool32									depthBoundsTestEnable;
			m_useStencil,												// VkBool32									stencilTestEnable;
			stencilOpState,												// VkStencilOpState							front;
			stencilOpState,												// VkStencilOpState							back;
			0.0f,														// float									minDepthBounds;
			1.0f,														// float									maxDepthBounds;
		};

		const deUint32 numSubpasses = m_renderType == RENDER_TYPE_DEPTHSTENCIL_ONLY ? 2u : 1u;

		for (deUint32 subpassIdx = 0; subpassIdx < numSubpasses; subpassIdx++)
			for (deUint32 i = 0u; i < numTopologies; ++i)
			{
				m_graphicsPipelines.push_back(VkPipelineSp(new Unique<VkPipeline>(makeGraphicsPipeline(vk,							// const DeviceInterface&                        vk
																									   vkDevice,					// const VkDevice                                device
																									   *m_pipelineLayout,			// const VkPipelineLayout                        pipelineLayout
																									   *m_vertexShaderModule,		// const VkShaderModule                          vertexShaderModule
																									   DE_NULL,						// const VkShaderModule                          tessellationControlModule
																									   DE_NULL,						// const VkShaderModule                          tessellationEvalModule
																									   DE_NULL,						// const VkShaderModule                          geometryShaderModule
																									   *m_fragmentShaderModule,		// const VkShaderModule                          fragmentShaderModule
																									   *m_renderPass,				// const VkRenderPass                            renderPass
																									   viewports,					// const std::vector<VkViewport>&                viewports
																									   scissors,					// const std::vector<VkRect2D>&                  scissors
																									   pTopology[i],				// const VkPrimitiveTopology                     topology
																									   subpassIdx,					// const deUint32                                subpass
																									   0u,							// const deUint32                                patchControlPoints
																									   &vertexInputStateParams,		// const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
																									   DE_NULL,						// const VkPipelineRasterizationStateCreateInfo* rasterizationStateCreateInfo
																									   &m_multisampleStateParams,	// const VkPipelineMultisampleStateCreateInfo*   multisampleStateCreateInfo
																									   &depthStencilStateParams,	// const VkPipelineDepthStencilStateCreateInfo*  depthStencilStateCreateInfo
																									   &colorBlendStateParams))));	// const VkPipelineColorBlendStateCreateInfo*    colorBlendStateCreateInfo
			}
	}

	if (m_renderType == RENDER_TYPE_COPY_SAMPLES)
	{
		// Create pipelines for copying samples to single sampled images
		{
			const VkPipelineVertexInputStateCreateInfo vertexInputStateParams =
			{
				VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType							sType;
				DE_NULL,														// const void*								pNext;
				0u,																// VkPipelineVertexInputStateCreateFlags	flags;
				0u,																// deUint32									vertexBindingDescriptionCount;
				DE_NULL,														// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
				0u,																// deUint32									vertexAttributeDescriptionCount;
				DE_NULL															// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
			};

			const std::vector<VkViewport>	viewports	(1, makeViewport(m_renderSize));
			const std::vector<VkRect2D>		scissors	(1, makeRect2D(m_renderSize));

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

			for (size_t i = 0; i < m_perSampleImages.size(); ++i)
			{
				// Pipeline is to be used in subpasses subsequent to sample-shading subpass
				m_copySamplePipelines.push_back(VkPipelineSp(new Unique<VkPipeline>(makeGraphicsPipeline(vk,									// const DeviceInterface&                        vk
																										 vkDevice,								// const VkDevice                                device
																										 *m_copySamplePipelineLayout,			// const VkPipelineLayout                        pipelineLayout
																										 *m_copySampleVertexShaderModule,		// const VkShaderModule                          vertexShaderModule
																										 DE_NULL,								// const VkShaderModule                          tessellationControlModule
																										 DE_NULL,								// const VkShaderModule                          tessellationEvalModule
																										 DE_NULL,								// const VkShaderModule                          geometryShaderModule
																										 *m_copySampleFragmentShaderModule,		// const VkShaderModule                          fragmentShaderModule
																										 *m_renderPass,							// const VkRenderPass                            renderPass
																										 viewports,								// const std::vector<VkViewport>&                viewports
																										 scissors,								// const std::vector<VkRect2D>&                  scissors
																										 VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,	// const VkPrimitiveTopology                     topology
																										 1u + (deUint32)i,						// const deUint32                                subpass
																										 0u,									// const deUint32                                patchControlPoints
																										 &vertexInputStateParams,				// const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
																										 DE_NULL,								// const VkPipelineRasterizationStateCreateInfo* rasterizationStateCreateInfo
																										 DE_NULL,								// const VkPipelineMultisampleStateCreateInfo*   multisampleStateCreateInfo
																										 DE_NULL,								// const VkPipelineDepthStencilStateCreateInfo*  depthStencilStateCreateInfo
																										 &colorBlendStateParams))));			// const VkPipelineColorBlendStateCreateInfo*    colorBlendStateCreateInfo
			}
		}


		const VkDescriptorPoolSize			descriptorPoolSize			=
		{
			VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,					// VkDescriptorType					type;
			1u														// deUint32							descriptorCount;
		};

		const VkDescriptorPoolCreateInfo	descriptorPoolCreateInfo	=
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,			// VkStructureType					sType
			DE_NULL,												// const void*						pNext
			VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,		// VkDescriptorPoolCreateFlags		flags
			1u,													// deUint32							maxSets
			1u,														// deUint32							poolSizeCount
			&descriptorPoolSize										// const VkDescriptorPoolSize*		pPoolSizes
		};

		m_copySampleDesciptorPool = createDescriptorPool(vk, vkDevice, &descriptorPoolCreateInfo);

		const VkDescriptorSetAllocateInfo	descriptorSetAllocateInfo	=
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,			// VkStructureType					sType
			DE_NULL,												// const void*						pNext
			*m_copySampleDesciptorPool,								// VkDescriptorPool					descriptorPool
			1u,														// deUint32							descriptorSetCount
			&m_copySampleDesciptorLayout.get(),						// const VkDescriptorSetLayout*		pSetLayouts
		};

		m_copySampleDesciptorSet = allocateDescriptorSet(vk, vkDevice, &descriptorSetAllocateInfo);

		const VkDescriptorImageInfo			imageInfo					=
		{
			DE_NULL,
			*m_colorAttachmentView,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};
		const VkWriteDescriptorSet			descriptorWrite				=
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,			// VkStructureType					sType;
			DE_NULL,										// const void*						pNext;
			*m_copySampleDesciptorSet,						// VkDescriptorSet					dstSet;
			0u,												// deUint32							dstBinding;
			0u,												// deUint32							dstArrayElement;
			1u,												// deUint32							descriptorCount;
			VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,			// VkDescriptorType					descriptorType;
			&imageInfo,										// const VkDescriptorImageInfo*		pImageInfo;
			DE_NULL,										// const VkDescriptorBufferInfo*	pBufferInfo;
			DE_NULL,										// const VkBufferView*				pTexelBufferView;
		};
		vk.updateDescriptorSets(vkDevice, 1u, &descriptorWrite, 0u, DE_NULL);
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
			&queueFamilyIndices[0]						// const deUint32*		pQueueFamilyIndices;
		};

		m_vertexBuffer		= createBuffer(vk, vkDevice, &vertexBufferParams);
		m_vertexBufferAlloc	= memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_vertexBuffer), MemoryRequirement::HostVisible);

		VK_CHECK(vk.bindBufferMemory(vkDevice, *m_vertexBuffer, m_vertexBufferAlloc->getMemory(), m_vertexBufferAlloc->getOffset()));

		// Load vertices into vertex buffer
		{
			Vertex4RGBA* pDst = static_cast<Vertex4RGBA*>(m_vertexBufferAlloc->getHostPtr());

			if (m_renderType == RENDER_TYPE_DEPTHSTENCIL_ONLY)
			{
				DE_ASSERT(numTopologies == 1);

				std::vector<Vertex4RGBA> vertices = pVertices[0];

				// Set alpha to zero for the first draw. This should prevent depth writes because of zero coverage.
				for (size_t i = 0; i < vertices.size(); i++)
					vertices[i].color.w() = 0.0f;

				deMemcpy(pDst, &vertices[0], vertices.size() * sizeof(Vertex4RGBA));

				pDst += vertices.size();

				// The second draw uses original vertices which are pure red.
				deMemcpy(pDst, &pVertices[0][0], pVertices[0].size() * sizeof(Vertex4RGBA));
			}
			else
			{
				for (deUint32 i = 0u; i < numTopologies; ++i)
				{
					deMemcpy(pDst, &pVertices[i][0], pVertices[i].size() * sizeof(Vertex4RGBA));
					pDst += pVertices[i].size();
				}
			}
		}
		flushAlloc(vk, vkDevice, *m_vertexBufferAlloc);
	}

	// Create command pool
	m_cmdPool = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndices[0]);

	// Create command buffer
	{
		VkClearValue colorClearValue;
		if (m_renderType == RENDER_TYPE_DEPTHSTENCIL_ONLY)
		{
			colorClearValue.color.float32[0] = 0.25;
			colorClearValue.color.float32[1] = 0.25;
			colorClearValue.color.float32[2] = 0.25;
			colorClearValue.color.float32[3] = 1.0f;
		}
		else
		{
			colorClearValue.color.float32[0] = 0.0f;
			colorClearValue.color.float32[1] = 0.0f;
			colorClearValue.color.float32[2] = 0.0f;
			colorClearValue.color.float32[3] = 0.0f;
		}

		VkClearValue depthStencilClearValue;
		depthStencilClearValue.depthStencil.depth = m_depthClearValue;
		depthStencilClearValue.depthStencil.stencil = 0u;

		std::vector<VkClearValue> clearValues;
		clearValues.push_back(colorClearValue);
		if (usesResolveImage)
		{
			clearValues.push_back(colorClearValue);
		}
		if (m_renderType == RENDER_TYPE_COPY_SAMPLES)
		{
			for (size_t i = 0; i < m_perSampleImages.size(); ++i)
			{
				clearValues.push_back(colorClearValue);
			}
		}
		if (m_useDepth || m_useStencil)
		{
			clearValues.push_back(depthStencilClearValue);
		}

		vk::VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		std::vector<VkImageMemoryBarrier> imageLayoutBarriers;

		{
			const VkImageMemoryBarrier colorImageBarrier =
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
			};
			imageLayoutBarriers.push_back(colorImageBarrier);
		}
		if (usesResolveImage)
		{
			const VkImageMemoryBarrier resolveImageBarrier =
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
			};
			imageLayoutBarriers.push_back(resolveImageBarrier);
		}
		if (m_renderType == RENDER_TYPE_COPY_SAMPLES)
		{
			for (size_t i = 0; i < m_perSampleImages.size(); ++i)
			{
				const VkImageMemoryBarrier perSampleImageBarrier =
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
					*m_perSampleImages[i]->m_image,					// VkImage					image;
					{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u },	// VkImageSubresourceRange	subresourceRange;
				};
				imageLayoutBarriers.push_back(perSampleImageBarrier);
			}
		}
		if (m_useDepth || m_useStencil)
		{
			const VkImageMemoryBarrier depthStencilImageBarrier =
			// depth/stencil attachment image
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,				// VkStructureType			sType;
				DE_NULL,											// const void*				pNext;
				0u,													// VkAccessFlags			srcAccessMask;
				VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,		// VkAccessFlags			dstAccessMask;
				VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout			oldLayout;
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,	// VkImageLayout			newLayout;
				VK_QUEUE_FAMILY_IGNORED,							// deUint32					srcQueueFamilyIndex;
				VK_QUEUE_FAMILY_IGNORED,							// deUint32					dstQueueFamilyIndex;
				*m_depthStencilImage,								// VkImage					image;
				{ depthStencilAttachmentAspect, 0u, 1u, 0u, 1u },	// VkImageSubresourceRange	subresourceRange;
			};
			imageLayoutBarriers.push_back(depthStencilImageBarrier);
			dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		};

		m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

		beginCommandBuffer(vk, *m_cmdBuffer, 0u);

		vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, dstStageMask, (VkDependencyFlags)0,
			0u, DE_NULL, 0u, DE_NULL, (deUint32)imageLayoutBarriers.size(), &imageLayoutBarriers[0]);

		beginRenderPass(vk, *m_cmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, m_renderSize.x(), m_renderSize.y()), (deUint32)clearValues.size(), &clearValues[0]);

		VkDeviceSize vertexBufferOffset = 0u;

		for (deUint32 i = 0u; i < numTopologies; ++i)
		{
			vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, **m_graphicsPipelines[i]);
			vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);
			vk.cmdDraw(*m_cmdBuffer, (deUint32)pVertices[i].size(), 1, 0, 0);

			vertexBufferOffset += static_cast<VkDeviceSize>(pVertices[i].size() * sizeof(Vertex4RGBA));
		}

		if (m_renderType == RENDER_TYPE_DEPTHSTENCIL_ONLY)
		{
			// The first draw was without color buffer and zero coverage. The depth buffer is expected to still have the clear value.
			vk.cmdNextSubpass(*m_cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);
			vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, **m_graphicsPipelines[1]);
			vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);
			// The depth test should pass as the first draw didn't touch the depth buffer.
			vk.cmdDraw(*m_cmdBuffer, (deUint32)pVertices[0].size(), 1, 0, 0);
		}
		else if (m_renderType == RENDER_TYPE_COPY_SAMPLES)
		{
			// Copy each sample id to single sampled image
			for (deInt32 sampleId = 0; sampleId < (deInt32)m_perSampleImages.size(); ++sampleId)
			{
				vk.cmdNextSubpass(*m_cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);
				vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, **m_copySamplePipelines[sampleId]);
				vk.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_copySamplePipelineLayout, 0u, 1u, &m_copySampleDesciptorSet.get(), 0u, DE_NULL);
				vk.cmdPushConstants(*m_cmdBuffer, *m_copySamplePipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(deInt32), &sampleId);
				vk.cmdDraw(*m_cmdBuffer, 4, 1, 0, 0);
			}
		}

		endRenderPass(vk, *m_cmdBuffer);

		endCommandBuffer(vk, *m_cmdBuffer);
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

	submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());

	if (m_renderType == RENDER_TYPE_RESOLVE || m_renderType == RENDER_TYPE_DEPTHSTENCIL_ONLY || m_renderType == RENDER_TYPE_UNUSED_ATTACHMENT)
	{
		return readColorAttachment(vk, vkDevice, queue, queueFamilyIndex, m_context.getDefaultAllocator(), *m_resolveImage, m_colorFormat, m_renderSize.cast<deUint32>());
	}
	else
	{
		return de::MovePtr<tcu::TextureLevel>();
	}
}

de::MovePtr<tcu::TextureLevel> MultisampleRenderer::getSingleSampledImage (deUint32 sampleId)
{
	return readColorAttachment(m_context.getDeviceInterface(), m_context.getDevice(), m_context.getUniversalQueue(), m_context.getUniversalQueueFamilyIndex(), m_context.getDefaultAllocator(), *m_perSampleImages[sampleId]->m_image, m_colorFormat, m_renderSize.cast<deUint32>());
}

// Multisample tests with subpasses using no attachments.
class VariableRateTestCase : public vkt::TestCase
{
public:
	using SampleCounts = std::vector<vk::VkSampleCountFlagBits>;

	struct PushConstants
	{
		int width;
		int height;
		int samples;
	};

	struct TestParams
	{
		bool						nonEmptyFramebuffer;	// Empty framebuffer or not.
		vk::VkSampleCountFlagBits	fbCount;				// If not empty, framebuffer sample count.
		bool						unusedAttachment;		// If not empty, create unused attachment or not.
		SampleCounts				subpassCounts;			// Counts for the different subpasses.
	};

	static const deInt32 kWidth		= 256u;
	static const deInt32 kHeight	= 256u;

									VariableRateTestCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& params);
	virtual							~VariableRateTestCase	(void) {}

	virtual void					initPrograms			(vk::SourceCollections& programCollection) const;
	virtual TestInstance*			createInstance			(Context& context) const;
	virtual void					checkSupport			(Context& context) const;

	static constexpr vk::VkFormat	kColorFormat			= vk::VK_FORMAT_R8G8B8A8_UNORM;

private:
	TestParams m_params;
};

class VariableRateTestInstance : public vkt::TestInstance
{
public:
	using TestParams = VariableRateTestCase::TestParams;

								VariableRateTestInstance	(Context& context, const TestParams& counts);
	virtual						~VariableRateTestInstance	(void) {}

	virtual tcu::TestStatus		iterate						(void);

private:
	TestParams m_params;
};

VariableRateTestCase::VariableRateTestCase (tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& params)
	: vkt::TestCase	(testCtx, name, description)
	, m_params		(params)
{
}

void VariableRateTestCase::initPrograms (vk::SourceCollections& programCollection) const
{
	std::stringstream vertSrc;

	vertSrc	<< "#version 450\n"
			<< "\n"
			<< "layout(location=0) in vec2 inPos;\n"
			<< "\n"
			<< "void main() {\n"
			<< "    gl_Position = vec4(inPos, 0.0, 1.0);\n"
			<< "}\n"
			;

	std::stringstream fragSrc;

	fragSrc	<< "#version 450\n"
			<< "\n"
			<< "layout(set=0, binding=0, std430) buffer OutBuffer {\n"
			<< "    int coverage[];\n"
			<< "} out_buffer;\n"
			<< "\n"
			<< "layout(push_constant) uniform PushConstants {\n"
			<< "    int width;\n"
			<< "    int height;\n"
			<< "    int samples;\n"
			<< "} push_constants;\n"
			<< "\n"
			<< "void main() {\n"
			<< "   ivec2 coord = ivec2(floor(gl_FragCoord.xy));\n"
			<< "   int pos = ((coord.y * push_constants.width) + coord.x) * push_constants.samples + int(gl_SampleID);\n"
			<< "   out_buffer.coverage[pos] = 1;\n"
			<< "}\n"
			;

	programCollection.glslSources.add("vert") << glu::VertexSource(vertSrc.str());
	programCollection.glslSources.add("frag") << glu::FragmentSource(fragSrc.str());
}

TestInstance* VariableRateTestCase::createInstance (Context& context) const
{
	return new VariableRateTestInstance(context, m_params);
}

void VariableRateTestCase::checkSupport (Context& context) const
{
	const auto&	vki				= context.getInstanceInterface();
	const auto	physicalDevice	= context.getPhysicalDevice();

	// When using multiple subpasses, require variableMultisampleRate.
	if (m_params.subpassCounts.size() > 1)
	{
		if (!vk::getPhysicalDeviceFeatures(vki, physicalDevice).variableMultisampleRate)
			TCU_THROW(NotSupportedError, "Variable multisample rate not supported");
	}

	// Check if sampleRateShading is supported.
	if(!vk::getPhysicalDeviceFeatures(vki, physicalDevice).sampleRateShading)
		TCU_THROW(NotSupportedError, "Sample rate shading is not supported");

	// Make sure all subpass sample counts are supported.
	const auto	properties		= vk::getPhysicalDeviceProperties(vki, physicalDevice);
	const auto&	supportedCounts	= properties.limits.framebufferNoAttachmentsSampleCounts;

	for (const auto count : m_params.subpassCounts)
	{
		if ((supportedCounts & count) == 0u)
			TCU_THROW(NotSupportedError, "Sample count combination not supported");
	}

	if (m_params.nonEmptyFramebuffer)
	{
		// Check the framebuffer sample count is supported.
		const auto formatProperties = vk::getPhysicalDeviceImageFormatProperties(vki, physicalDevice, kColorFormat, vk::VK_IMAGE_TYPE_2D, vk::VK_IMAGE_TILING_OPTIMAL, vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 0u);
		if ((formatProperties.sampleCounts & m_params.fbCount) == 0u)
			TCU_THROW(NotSupportedError, "Sample count of " + de::toString(m_params.fbCount) + " not supported for color attachment");
	}
}

void zeroOutAndFlush(const vk::DeviceInterface& vkd, vk::VkDevice device, vk::BufferWithMemory& buffer, vk::VkDeviceSize size)
{
	auto& alloc = buffer.getAllocation();
	deMemset(alloc.getHostPtr(), 0, static_cast<size_t>(size));
	vk::flushAlloc(vkd, device, alloc);
}

VariableRateTestInstance::VariableRateTestInstance (Context& context, const TestParams& params)
	: vkt::TestInstance	(context)
	, m_params			(params)
{
}

tcu::TestStatus VariableRateTestInstance::iterate (void)
{
	using PushConstants = VariableRateTestCase::PushConstants;

	const auto&	vkd			= m_context.getDeviceInterface();
	const auto	device		= m_context.getDevice();
	auto&		allocator	= m_context.getDefaultAllocator();
	const auto&	queue		= m_context.getUniversalQueue();
	const auto	queueIndex	= m_context.getUniversalQueueFamilyIndex();

	const vk::VkDeviceSize	kWidth			= static_cast<vk::VkDeviceSize>(VariableRateTestCase::kWidth);
	const vk::VkDeviceSize	kHeight			= static_cast<vk::VkDeviceSize>(VariableRateTestCase::kHeight);
	constexpr auto			kColorFormat	= VariableRateTestCase::kColorFormat;

	const auto kWidth32		= static_cast<deUint32>(kWidth);
	const auto kHeight32	= static_cast<deUint32>(kHeight);

	std::vector<std::unique_ptr<vk::BufferWithMemory>>	referenceBuffers;
	std::vector<std::unique_ptr<vk::BufferWithMemory>>	outputBuffers;
	std::vector<size_t>									bufferNumElements;
	std::vector<vk::VkDeviceSize>						bufferSizes;

	// Create reference and output buffers.
	for (const auto count : m_params.subpassCounts)
	{
		bufferNumElements.push_back(static_cast<size_t>(kWidth * kHeight * count));
		bufferSizes.push_back(bufferNumElements.back() * sizeof(deInt32));
		const auto bufferCreateInfo = vk::makeBufferCreateInfo(bufferSizes.back(), vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

		referenceBuffers.emplace_back	(new vk::BufferWithMemory{vkd, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible});
		outputBuffers.emplace_back		(new vk::BufferWithMemory{vkd, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible});
	}

	// Descriptor set layout.
	vk::DescriptorSetLayoutBuilder builder;
	builder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_FRAGMENT_BIT);
	const auto descriptorSetLayout = builder.build(vkd, device);

	// Pipeline layout.
	const vk::VkPushConstantRange pushConstantRange =
	{
		vk::VK_SHADER_STAGE_FRAGMENT_BIT,				//	VkShaderStageFlags	stageFlags;
		0u,												//	deUint32			offset;
		static_cast<deUint32>(sizeof(PushConstants)),	//	deUint32			size;
	};

	const vk::VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	//	VkStructureType					sType;
		nullptr,											//	const void*						pNext;
		0u,													//	VkPipelineLayoutCreateFlags		flags;
		1u,													//	deUint32						setLayoutCount;
		&descriptorSetLayout.get(),							//	const VkDescriptorSetLayout*	pSetLayouts;
		1u,													//	deUint32						pushConstantRangeCount;
		&pushConstantRange,									//	const VkPushConstantRange*		pPushConstantRanges;
	};
	const auto pipelineLayout = vk::createPipelineLayout(vkd, device, &pipelineLayoutCreateInfo);

	// Subpass with no attachments.
	const vk::VkSubpassDescription emptySubpassDescription =
	{
		0u,										//	VkSubpassDescriptionFlags		flags;
		vk::VK_PIPELINE_BIND_POINT_GRAPHICS,	//	VkPipelineBindPoint				pipelineBindPoint;
		0u,										//	deUint32						inputAttachmentCount;
		nullptr,								//	const VkAttachmentReference*	pInputAttachments;
		0u,										//	deUint32						colorAttachmentCount;
		nullptr,								//	const VkAttachmentReference*	pColorAttachments;
		nullptr,								//	const VkAttachmentReference*	pResolveAttachments;
		nullptr,								//	const VkAttachmentReference*	pDepthStencilAttachment;
		0u,										//	deUint32						preserveAttachmentCount;
		nullptr,								//	const deUint32*					pPreserveAttachments;
	};

	// Unused attachment reference.
	const vk::VkAttachmentReference unusedAttachmentReference =
	{
		VK_ATTACHMENT_UNUSED,							//	deUint32		attachment;
		vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	//	VkImageLayout	layout;
	};

	// Subpass with unused attachment.
	const vk::VkSubpassDescription unusedAttachmentSubpassDescription =
	{
		0u,										//	VkSubpassDescriptionFlags		flags;
		vk::VK_PIPELINE_BIND_POINT_GRAPHICS,	//	VkPipelineBindPoint				pipelineBindPoint;
		0u,										//	deUint32						inputAttachmentCount;
		nullptr,								//	const VkAttachmentReference*	pInputAttachments;
		1u,										//	deUint32						colorAttachmentCount;
		&unusedAttachmentReference,				//	const VkAttachmentReference*	pColorAttachments;
		nullptr,								//	const VkAttachmentReference*	pResolveAttachments;
		nullptr,								//	const VkAttachmentReference*	pDepthStencilAttachment;
		0u,										//	deUint32						preserveAttachmentCount;
		nullptr,								//	const deUint32*					pPreserveAttachments;
	};

	// Renderpass with multiple subpasses.
	vk::VkRenderPassCreateInfo renderPassCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,	//	VkStructureType					sType;
		nullptr,										//	const void*						pNext;
		0u,												//	VkRenderPassCreateFlags			flags;
		0u,												//	deUint32						attachmentCount;
		nullptr,										//	const VkAttachmentDescription*	pAttachments;
		0u,												//	deUint32						subpassCount;
		nullptr,										//	const VkSubpassDescription*		pSubpasses;
		0u,												//	deUint32						dependencyCount;
		nullptr,										//	const VkSubpassDependency*		pDependencies;
	};

	std::vector<vk::VkSubpassDescription> subpassesVector;

	for (size_t i = 0; i < m_params.subpassCounts.size(); ++i)
		subpassesVector.push_back(emptySubpassDescription);
	renderPassCreateInfo.subpassCount	= static_cast<deUint32>(subpassesVector.size());
	renderPassCreateInfo.pSubpasses		= subpassesVector.data();
	const auto renderPassMultiplePasses = vk::createRenderPass(vkd, device, &renderPassCreateInfo);

	// Render pass with single subpass.
	const vk::VkAttachmentDescription colorAttachmentDescription =
	{
		0u,												//	VkAttachmentDescriptionFlags	flags;
		kColorFormat,									//	VkFormat						format;
		m_params.fbCount,								//	VkSampleCountFlagBits			samples;
		vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,			//	VkAttachmentLoadOp				loadOp;
		vk::VK_ATTACHMENT_STORE_OP_STORE,				//	VkAttachmentStoreOp				storeOp;
		vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,			//	VkAttachmentLoadOp				stencilLoadOp;
		vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,			//	VkAttachmentStoreOp				stencilStoreOp;
		vk::VK_IMAGE_LAYOUT_UNDEFINED,					//	VkImageLayout					initialLayout;
		vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	//	VkImageLayout					finalLayout;
	};

	if (m_params.nonEmptyFramebuffer)
	{
		renderPassCreateInfo.attachmentCount = 1u;
		renderPassCreateInfo.pAttachments = &colorAttachmentDescription;
	}
	renderPassCreateInfo.subpassCount	= 1u;
	renderPassCreateInfo.pSubpasses		= ((m_params.nonEmptyFramebuffer && m_params.unusedAttachment) ? &unusedAttachmentSubpassDescription : &emptySubpassDescription);
	const auto renderPassSingleSubpass	= vk::createRenderPass(vkd, device, &renderPassCreateInfo);

	// Framebuffers.
	vk::VkFramebufferCreateInfo framebufferCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	//	VkStructureType				sType;
		nullptr,										//	const void*					pNext;
		0u,												//	VkFramebufferCreateFlags	flags;
		DE_NULL,										//	VkRenderPass				renderPass;
		0u,												//	deUint32					attachmentCount;
		nullptr,										//	const VkImageView*			pAttachments;
		kWidth32,										//	deUint32					width;
		kHeight32,										//	deUint32					height;
		1u,												//	deUint32					layers;
	};

	// Framebuffer for multiple-subpasses render pass.
	framebufferCreateInfo.renderPass		= renderPassMultiplePasses.get();
	const auto framebufferMultiplePasses	= vk::createFramebuffer(vkd, device, &framebufferCreateInfo);

	// Framebuffer for single-subpass render pass.
	std::unique_ptr<vk::ImageWithMemory>	imagePtr;
	vk::Move<vk::VkImageView>				imageView;

	if (m_params.nonEmptyFramebuffer)
	{
		const vk::VkImageCreateInfo imageCreateInfo =
		{
			vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
			nullptr,									//	const void*				pNext;
			0u,											//	VkImageCreateFlags		flags;
			vk::VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
			kColorFormat,								//	VkFormat				format;
			vk::makeExtent3D(kWidth32, kHeight32, 1u),	//	VkExtent3D				extent;
			1u,											//	deUint32				mipLevels;
			1u,											//	deUint32				arrayLayers;
			m_params.fbCount,							//	VkSampleCountFlagBits	samples;
			vk::VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
			vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,	//	VkImageUsageFlags		usage;
			vk::VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
			0u,											//	deUint32				queueFamilyIndexCount;
			nullptr,									//	const deUint32*			pQueueFamilyIndices;
			vk::VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
		};
		imagePtr.reset(new vk::ImageWithMemory{vkd, device, allocator, imageCreateInfo, MemoryRequirement::Any});

		const auto subresourceRange	= vk::makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
		imageView					= vk::makeImageView(vkd, device, imagePtr->get(), vk::VK_IMAGE_VIEW_TYPE_2D, kColorFormat, subresourceRange);

		framebufferCreateInfo.attachmentCount	= 1u;
		framebufferCreateInfo.pAttachments		= &imageView.get();
	}
	framebufferCreateInfo.renderPass	= renderPassSingleSubpass.get();
	const auto framebufferSingleSubpass	= vk::createFramebuffer(vkd, device, &framebufferCreateInfo);

	// Shader modules and stages.
	const auto vertModule = vk::createShaderModule(vkd, device, m_context.getBinaryCollection().get("vert"), 0u);
	const auto fragModule = vk::createShaderModule(vkd, device, m_context.getBinaryCollection().get("frag"), 0u);

	std::vector<vk::VkPipelineShaderStageCreateInfo> shaderStages;

	vk::VkPipelineShaderStageCreateInfo shaderStageCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	//	VkStructureType						sType;
		nullptr,													//	const void*							pNext;
		0u,															//	VkPipelineShaderStageCreateFlags	flags;
		vk::VK_SHADER_STAGE_VERTEX_BIT,								//	VkShaderStageFlagBits				stage;
		vertModule.get(),											//	VkShaderModule						module;
		"main",														//	const char*							pName;
		nullptr,													//	const VkSpecializationInfo*			pSpecializationInfo;
	};

	shaderStages.push_back(shaderStageCreateInfo);
	shaderStageCreateInfo.stage		= vk::VK_SHADER_STAGE_FRAGMENT_BIT;
	shaderStageCreateInfo.module	= fragModule.get();
	shaderStages.push_back(shaderStageCreateInfo);

	// Vertices, input state and assembly.
	const std::vector<tcu::Vec2> vertices =
	{
		{ -0.987f, -0.964f },
		{  0.982f, -0.977f },
		{  0.005f,  0.891f },
	};

	const auto vertexBinding	= vk::makeVertexInputBindingDescription(0u, static_cast<deUint32>(sizeof(decltype(vertices)::value_type)), vk::VK_VERTEX_INPUT_RATE_VERTEX);
	const auto vertexAttribute	= vk::makeVertexInputAttributeDescription(0u, 0u, vk::VK_FORMAT_R32G32_SFLOAT, 0u);

	const vk::VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	//	VkStructureType								sType;
		nullptr,														//	const void*									pNext;
		0u,																//	VkPipelineVertexInputStateCreateFlags		flags;
		1u,																//	deUint32									vertexBindingDescriptionCount;
		&vertexBinding,													//	const VkVertexInputBindingDescription*		pVertexBindingDescriptions;
		1u,																//	deUint32									vertexAttributeDescriptionCount;
		&vertexAttribute,												//	const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
	};

	const vk::VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	//	VkStructureType							sType;
		nullptr,															//	const void*								pNext;
		0u,																	//	VkPipelineInputAssemblyStateCreateFlags	flags;
		vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,							//	VkPrimitiveTopology						topology;
		VK_FALSE,															//	VkBool32								primitiveRestartEnable;
	};

	// Graphics pipelines to create output buffers.
	const auto viewport	= vk::makeViewport(kWidth32, kHeight32);
	const auto scissor	= vk::makeRect2D(kWidth32, kHeight32);

	const vk::VkPipelineViewportStateCreateInfo viewportStateCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,	//	VkStructureType						sType;
		nullptr,													//	const void*							pNext;
		0u,															//	VkPipelineViewportStateCreateFlags	flags;
		1u,															//	deUint32							viewportCount;
		&viewport,													//	const VkViewport*					pViewports;
		1u,															//	deUint32							scissorCount;
		&scissor,													//	const VkRect2D*						pScissors;
	};

	const vk::VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,	//	VkStructureType							sType;
		nullptr,														//	const void*								pNext;
		0u,																//	VkPipelineRasterizationStateCreateFlags	flags;
		VK_FALSE,														//	VkBool32								depthClampEnable;
		VK_FALSE,														//	VkBool32								rasterizerDiscardEnable;
		vk::VK_POLYGON_MODE_FILL,										//	VkPolygonMode							polygonMode;
		vk::VK_CULL_MODE_NONE,											//	VkCullModeFlags							cullMode;
		vk::VK_FRONT_FACE_CLOCKWISE,									//	VkFrontFace								frontFace;
		VK_FALSE,														//	VkBool32								depthBiasEnable;
		0.0f,															//	float									depthBiasConstantFactor;
		0.0f,															//	float									depthBiasClamp;
		0.0f,															//	float									depthBiasSlopeFactor;
		1.0f,															//	float									lineWidth;
	};

	vk::VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	//	VkStructureType							sType;
		nullptr,														//	const void*								pNext;
		0u,																//	VkPipelineMultisampleStateCreateFlags	flags;
		vk::VK_SAMPLE_COUNT_1_BIT,										//	VkSampleCountFlagBits					rasterizationSamples;
		VK_FALSE,														//	VkBool32								sampleShadingEnable;
		0.0f,															//	float									minSampleShading;
		nullptr,														//	const VkSampleMask*						pSampleMask;
		VK_FALSE,														//	VkBool32								alphaToCoverageEnable;
		VK_FALSE,														//	VkBool32								alphaToOneEnable;
	};

	std::vector<vk::Move<vk::VkPipeline>> outputPipelines;

	for (const auto samples : m_params.subpassCounts)
	{
		multisampleStateCreateInfo.rasterizationSamples = samples;

		const vk::VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo =
		{
			vk::VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,	//	VkStructureType									sType;
			nullptr,												//	const void*										pNext;
			0u,														//	VkPipelineCreateFlags							flags;
			static_cast<deUint32>(shaderStages.size()),				//	deUint32										stageCount;
			shaderStages.data(),									//	const VkPipelineShaderStageCreateInfo*			pStages;
			&vertexInputStateCreateInfo,							//	const VkPipelineVertexInputStateCreateInfo*		pVertexInputState;
			&inputAssemblyStateCreateInfo,							//	const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState;
			nullptr,												//	const VkPipelineTessellationStateCreateInfo*	pTessellationState;
			&viewportStateCreateInfo,								//	const VkPipelineViewportStateCreateInfo*		pViewportState;
			&rasterizationStateCreateInfo,							//	const VkPipelineRasterizationStateCreateInfo*	pRasterizationState;
			&multisampleStateCreateInfo,							//	const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
			nullptr,												//	const VkPipelineDepthStencilStateCreateInfo*	pDepthStencilState;
			nullptr,												//	const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
			nullptr,												//	const VkPipelineDynamicStateCreateInfo*			pDynamicState;
			pipelineLayout.get(),									//	VkPipelineLayout								layout;
			renderPassSingleSubpass.get(),							//	VkRenderPass									renderPass;
			0u,														//	deUint32										subpass;
			DE_NULL,												//	VkPipeline										basePipelineHandle;
			0,														//	deInt32											basePipelineIndex;
		};

		outputPipelines.push_back(vk::createGraphicsPipeline(vkd, device, DE_NULL, &graphicsPipelineCreateInfo));
	}

	// Graphics pipelines with variable rate but using several subpasses.
	std::vector<vk::Move<vk::VkPipeline>> referencePipelines;

	for (size_t i = 0; i < m_params.subpassCounts.size(); ++i)
	{
		multisampleStateCreateInfo.rasterizationSamples = m_params.subpassCounts[i];

		const vk::VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo =
		{
			vk::VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,	//	VkStructureType									sType;
			nullptr,												//	const void*										pNext;
			0u,														//	VkPipelineCreateFlags							flags;
			static_cast<deUint32>(shaderStages.size()),				//	deUint32										stageCount;
			shaderStages.data(),									//	const VkPipelineShaderStageCreateInfo*			pStages;
			&vertexInputStateCreateInfo,							//	const VkPipelineVertexInputStateCreateInfo*		pVertexInputState;
			&inputAssemblyStateCreateInfo,							//	const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState;
			nullptr,												//	const VkPipelineTessellationStateCreateInfo*	pTessellationState;
			&viewportStateCreateInfo,								//	const VkPipelineViewportStateCreateInfo*		pViewportState;
			&rasterizationStateCreateInfo,							//	const VkPipelineRasterizationStateCreateInfo*	pRasterizationState;
			&multisampleStateCreateInfo,							//	const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
			nullptr,												//	const VkPipelineDepthStencilStateCreateInfo*	pDepthStencilState;
			nullptr,												//	const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
			nullptr,												//	const VkPipelineDynamicStateCreateInfo*			pDynamicState;
			pipelineLayout.get(),									//	VkPipelineLayout								layout;
			renderPassMultiplePasses.get(),							//	VkRenderPass									renderPass;
			static_cast<deUint32>(i),								//	deUint32										subpass;
			DE_NULL,												//	VkPipeline										basePipelineHandle;
			0,														//	deInt32											basePipelineIndex;
		};

		referencePipelines.push_back(vk::createGraphicsPipeline(vkd, device, DE_NULL, &graphicsPipelineCreateInfo));
	}

	// Prepare vertex, reference and output buffers.
	const auto				vertexBufferSize		= vertices.size() * sizeof(decltype(vertices)::value_type);
	const auto				vertexBufferCreateInfo	= vk::makeBufferCreateInfo(static_cast<VkDeviceSize>(vertexBufferSize), vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	vk::BufferWithMemory	vertexBuffer			{vkd, device, allocator, vertexBufferCreateInfo, MemoryRequirement::HostVisible};
	auto&					vertexAlloc				= vertexBuffer.getAllocation();

	deMemcpy(vertexAlloc.getHostPtr(), vertices.data(), vertexBufferSize);
	vk::flushAlloc(vkd, device, vertexAlloc);

	for (size_t i = 0; i < referenceBuffers.size(); ++i)
	{
		zeroOutAndFlush(vkd, device, *referenceBuffers[i], bufferSizes[i]);
		zeroOutAndFlush(vkd, device, *outputBuffers[i], bufferSizes[i]);
	}

	// Prepare descriptor sets.
	const deUint32				totalSets		= static_cast<deUint32>(referenceBuffers.size() * 2u);
	vk::DescriptorPoolBuilder	poolBuilder;
	poolBuilder.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, static_cast<deUint32>(referenceBuffers.size() * 2u));
	const auto descriptorPool = poolBuilder.build(vkd, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, totalSets);

	std::vector<vk::Move<vk::VkDescriptorSet>> referenceSets	(referenceBuffers.size());
	std::vector<vk::Move<vk::VkDescriptorSet>> outputSets		(outputBuffers.size());

	for (auto& set : referenceSets)
		set = vk::makeDescriptorSet(vkd, device, descriptorPool.get(), descriptorSetLayout.get());
	for (auto& set : outputSets)
		set = vk::makeDescriptorSet(vkd, device, descriptorPool.get(), descriptorSetLayout.get());

	vk::DescriptorSetUpdateBuilder updateBuilder;

	for (size_t i = 0; i < referenceSets.size(); ++i)
	{
		const auto descriptorBufferInfo = vk::makeDescriptorBufferInfo(referenceBuffers[i]->get(), 0u, bufferSizes[i]);
		updateBuilder.writeSingle(referenceSets[i].get(), vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorBufferInfo);
	}
	for (size_t i = 0; i < outputSets.size(); ++i)
	{
		const auto descriptorBufferInfo = vk::makeDescriptorBufferInfo(outputBuffers[i]->get(), 0u, bufferSizes[i]);
		updateBuilder.writeSingle(outputSets[i].get(), vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorBufferInfo);
	}

	updateBuilder.update(vkd, device);

	// Prepare command pool.
	const auto cmdPool		= vk::makeCommandPool(vkd, device, queueIndex);
	const auto cmdBufferPtr	= vk::allocateCommandBuffer(vkd , device, cmdPool.get(), vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	vk::VkBufferMemoryBarrier storageBufferDevToHostBarrier =
	{
		vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	//	VkStructureType	sType;
		nullptr,										//	const void*		pNext;
		vk::VK_ACCESS_SHADER_WRITE_BIT,					//	VkAccessFlags	srcAccessMask;
		vk::VK_ACCESS_HOST_READ_BIT,					//	VkAccessFlags	dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,						//	deUint32		srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,						//	deUint32		dstQueueFamilyIndex;
		DE_NULL,										//	VkBuffer		buffer;
		0u,												//	VkDeviceSize	offset;
		VK_WHOLE_SIZE,									//	VkDeviceSize	size;
	};

	// Record command buffer.
	const vk::VkDeviceSize	vertexBufferOffset	= 0u;
	const auto				renderArea			= vk::makeRect2D(kWidth32, kHeight32);
	PushConstants			pushConstants		= { static_cast<int>(kWidth), static_cast<int>(kHeight), 0 };

	vk::beginCommandBuffer(vkd, cmdBuffer);

	// Render output buffers.
	vk::beginRenderPass(vkd, cmdBuffer, renderPassSingleSubpass.get(), framebufferSingleSubpass.get(), renderArea);
	for (size_t i = 0; i < outputBuffers.size(); ++i)
	{
		vkd.cmdBindPipeline(cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, outputPipelines[i].get());
		vkd.cmdBindDescriptorSets(cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0u, 1u, &outputSets[i].get(), 0u, nullptr);
		vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
		pushConstants.samples = static_cast<int>(m_params.subpassCounts[i]);
		vkd.cmdPushConstants(cmdBuffer, pipelineLayout.get(), pushConstantRange.stageFlags, pushConstantRange.offset, pushConstantRange.size, &pushConstants);
		vkd.cmdDraw(cmdBuffer, static_cast<deUint32>(vertices.size()), 1u, 0u, 0u);
	}
	vk::endRenderPass(vkd, cmdBuffer);
	for (size_t i = 0; i < outputBuffers.size(); ++i)
	{
		storageBufferDevToHostBarrier.buffer = outputBuffers[i]->get();
		vkd.cmdPipelineBarrier(cmdBuffer, vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, nullptr, 1u, &storageBufferDevToHostBarrier, 0u, nullptr);
	}

	// Render reference buffers.
	vk::beginRenderPass(vkd, cmdBuffer, renderPassMultiplePasses.get(), framebufferMultiplePasses.get(), renderArea);
	for (size_t i = 0; i < referenceBuffers.size(); ++i)
	{
		if (i > 0)
			vkd.cmdNextSubpass(cmdBuffer, vk::VK_SUBPASS_CONTENTS_INLINE);
		vkd.cmdBindPipeline(cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, referencePipelines[i].get());
		vkd.cmdBindDescriptorSets(cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0u, 1u, &referenceSets[i].get(), 0u, nullptr);
		vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
		pushConstants.samples = static_cast<int>(m_params.subpassCounts[i]);
		vkd.cmdPushConstants(cmdBuffer, pipelineLayout.get(), pushConstantRange.stageFlags, pushConstantRange.offset, pushConstantRange.size, &pushConstants);
		vkd.cmdDraw(cmdBuffer, static_cast<deUint32>(vertices.size()), 1u, 0u, 0u);
	}
	vk::endRenderPass(vkd, cmdBuffer);
	for (size_t i = 0; i < referenceBuffers.size(); ++i)
	{
		storageBufferDevToHostBarrier.buffer = referenceBuffers[i]->get();
		vkd.cmdPipelineBarrier(cmdBuffer, vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, nullptr, 1u, &storageBufferDevToHostBarrier, 0u, nullptr);
	}

	vk::endCommandBuffer(vkd, cmdBuffer);

	// Run all pipelines.
	vk::submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Invalidate reference allocs.
#undef LOG_BUFFER_CONTENTS
#ifdef LOG_BUFFER_CONTENTS
	auto& log = m_context.getTestContext().getLog();
#endif
	for (size_t i = 0; i < referenceBuffers.size(); ++i)
	{
		auto& buffer	= referenceBuffers[i];
		auto& alloc		= buffer->getAllocation();
		vk::invalidateAlloc(vkd, device, alloc);

#ifdef LOG_BUFFER_CONTENTS
		std::vector<deInt32> bufferValues(bufferNumElements[i]);
		deMemcpy(bufferValues.data(), alloc.getHostPtr(), bufferSizes[i]);

		std::ostringstream msg;
		for (const auto value : bufferValues)
			msg << " " << value;
		log << tcu::TestLog::Message << "Reference buffer values with " << m_params[i] << " samples:" << msg.str() << tcu::TestLog::EndMessage;
#endif
	}

	for (size_t i = 0; i < outputBuffers.size(); ++i)
	{
		auto& buffer	= outputBuffers[i];
		auto& alloc		= buffer->getAllocation();
		vk::invalidateAlloc(vkd, device, alloc);

#ifdef LOG_BUFFER_CONTENTS
		std::vector<deInt32> bufferValues(bufferNumElements[i]);
		deMemcpy(bufferValues.data(), alloc.getHostPtr(), bufferSizes[i]);

		std::ostringstream msg;
		for (const auto value : bufferValues)
			msg << " " << value;
		log << tcu::TestLog::Message << "Output buffer values with " << m_params[i] << " samples:" << msg.str() << tcu::TestLog::EndMessage;
#endif

		if (deMemCmp(alloc.getHostPtr(), referenceBuffers[i]->getAllocation().getHostPtr(), static_cast<size_t>(bufferSizes[i])) != 0)
			return tcu::TestStatus::fail("Buffer mismatch in output buffer " + de::toString(i));
	}

	return tcu::TestStatus::pass("Pass");
}

using ElementsVector	= std::vector<vk::VkSampleCountFlagBits>;
using CombinationVector	= std::vector<ElementsVector>;

void combinationsRecursive(const ElementsVector& elements, size_t requestedSize, CombinationVector& solutions, ElementsVector& partial)
{
	if (partial.size() == requestedSize)
		solutions.push_back(partial);
	else
	{
		for (const auto& elem : elements)
		{
			partial.push_back(elem);
			combinationsRecursive(elements, requestedSize, solutions, partial);
			partial.pop_back();
		}
	}
}

CombinationVector combinations(const ElementsVector& elements, size_t requestedSize)
{
	CombinationVector solutions;
	ElementsVector partial;

	combinationsRecursive(elements, requestedSize, solutions, partial);
	return solutions;
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

			samplesTests->addChild(new RasterizationSamplesTest(testCtx, "primitive_triangle", "",	samples[samplesNdx], GEOMETRY_TYPE_OPAQUE_TRIANGLE, 1.0f, IMAGE_BACKING_MODE_REGULAR));
			samplesTests->addChild(new RasterizationSamplesTest(testCtx, "primitive_line", "",		samples[samplesNdx], GEOMETRY_TYPE_OPAQUE_LINE, 1.0f, IMAGE_BACKING_MODE_REGULAR));
			samplesTests->addChild(new RasterizationSamplesTest(testCtx, "primitive_point_1px", "",	samples[samplesNdx], GEOMETRY_TYPE_OPAQUE_POINT, 1.0f, IMAGE_BACKING_MODE_REGULAR));
			samplesTests->addChild(new RasterizationSamplesTest(testCtx, "primitive_point", "",		samples[samplesNdx], GEOMETRY_TYPE_OPAQUE_POINT, 3.0f, IMAGE_BACKING_MODE_REGULAR));

			samplesTests->addChild(new RasterizationSamplesTest(testCtx, "depth", "",			samples[samplesNdx], GEOMETRY_TYPE_INVISIBLE_TRIANGLE, 1.0f, IMAGE_BACKING_MODE_REGULAR, TEST_MODE_DEPTH_BIT));
			samplesTests->addChild(new RasterizationSamplesTest(testCtx, "stencil", "",			samples[samplesNdx], GEOMETRY_TYPE_INVISIBLE_TRIANGLE, 1.0f, IMAGE_BACKING_MODE_REGULAR, TEST_MODE_STENCIL_BIT));
			samplesTests->addChild(new RasterizationSamplesTest(testCtx, "depth_stencil", "",	samples[samplesNdx], GEOMETRY_TYPE_INVISIBLE_TRIANGLE, 1.0f, IMAGE_BACKING_MODE_REGULAR, TEST_MODE_DEPTH_BIT | TEST_MODE_STENCIL_BIT));

			samplesTests->addChild(new RasterizationSamplesTest(testCtx, "primitive_triangle_sparse", "",	samples[samplesNdx], GEOMETRY_TYPE_OPAQUE_TRIANGLE, 1.0f, IMAGE_BACKING_MODE_SPARSE));
			samplesTests->addChild(new RasterizationSamplesTest(testCtx, "primitive_line_sparse", "",		samples[samplesNdx], GEOMETRY_TYPE_OPAQUE_LINE, 1.0f, IMAGE_BACKING_MODE_SPARSE));
			samplesTests->addChild(new RasterizationSamplesTest(testCtx, "primitive_point_1px_sparse", "",	samples[samplesNdx], GEOMETRY_TYPE_OPAQUE_POINT, 1.0f, IMAGE_BACKING_MODE_SPARSE));
			samplesTests->addChild(new RasterizationSamplesTest(testCtx, "primitive_point_sparse", "",		samples[samplesNdx], GEOMETRY_TYPE_OPAQUE_POINT, 3.0f, IMAGE_BACKING_MODE_SPARSE));

			samplesTests->addChild(new RasterizationSamplesTest(testCtx, "depth_sparse", "",			samples[samplesNdx], GEOMETRY_TYPE_INVISIBLE_TRIANGLE, 1.0f, IMAGE_BACKING_MODE_SPARSE, TEST_MODE_DEPTH_BIT));
			samplesTests->addChild(new RasterizationSamplesTest(testCtx, "stencil_sparse", "",			samples[samplesNdx], GEOMETRY_TYPE_INVISIBLE_TRIANGLE, 1.0f, IMAGE_BACKING_MODE_SPARSE, TEST_MODE_STENCIL_BIT));
			samplesTests->addChild(new RasterizationSamplesTest(testCtx, "depth_stencil_sparse", "",	samples[samplesNdx], GEOMETRY_TYPE_INVISIBLE_TRIANGLE, 1.0f, IMAGE_BACKING_MODE_SPARSE, TEST_MODE_DEPTH_BIT | TEST_MODE_STENCIL_BIT));

			rasterizationSamplesTests->addChild(samplesTests.release());
		}

		multisampleTests->addChild(rasterizationSamplesTests.release());
	}

	// Raster samples consistency check
	{
		de::MovePtr<tcu::TestCaseGroup> rasterSamplesConsistencyTests	(new tcu::TestCaseGroup(testCtx, "raster_samples_consistency", ""));
		MultisampleTestParams			paramsRegular					= {GEOMETRY_TYPE_OPAQUE_TRIANGLE, 1.0f, IMAGE_BACKING_MODE_REGULAR};
		MultisampleTestParams			paramsSparse					= {GEOMETRY_TYPE_OPAQUE_TRIANGLE, 1.0f, IMAGE_BACKING_MODE_SPARSE};

		addFunctionCaseWithPrograms(rasterSamplesConsistencyTests.get(),
									"unique_colors_check",
									"",
									initMultisamplePrograms,
									testRasterSamplesConsistency,
									paramsRegular);

		addFunctionCaseWithPrograms(rasterSamplesConsistencyTests.get(),
									"unique_colors_check_sparse",
									"",
									initMultisamplePrograms,
									testRasterSamplesConsistency,
									paramsSparse);

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

		{
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

					samplesTests->addChild(new MinSampleShadingTest(testCtx, "primitive_triangle",	"", samples[samplesNdx], testConfig.minSampleShading, GEOMETRY_TYPE_OPAQUE_TRIANGLE, 1.0f, IMAGE_BACKING_MODE_REGULAR));
					samplesTests->addChild(new MinSampleShadingTest(testCtx, "primitive_line",		"", samples[samplesNdx], testConfig.minSampleShading, GEOMETRY_TYPE_OPAQUE_LINE, 1.0f, IMAGE_BACKING_MODE_REGULAR));
					samplesTests->addChild(new MinSampleShadingTest(testCtx, "primitive_point_1px",	"", samples[samplesNdx], testConfig.minSampleShading, GEOMETRY_TYPE_OPAQUE_POINT, 1.0f, IMAGE_BACKING_MODE_REGULAR));
					samplesTests->addChild(new MinSampleShadingTest(testCtx, "primitive_point",		"", samples[samplesNdx], testConfig.minSampleShading, GEOMETRY_TYPE_OPAQUE_POINT, 3.0f, IMAGE_BACKING_MODE_REGULAR));

					samplesTests->addChild(new MinSampleShadingTest(testCtx, "primitive_triangle_sparse",	"", samples[samplesNdx], testConfig.minSampleShading, GEOMETRY_TYPE_OPAQUE_TRIANGLE, 1.0f, IMAGE_BACKING_MODE_SPARSE));
					samplesTests->addChild(new MinSampleShadingTest(testCtx, "primitive_line_sparse",		"", samples[samplesNdx], testConfig.minSampleShading, GEOMETRY_TYPE_OPAQUE_LINE, 1.0f, IMAGE_BACKING_MODE_SPARSE));
					samplesTests->addChild(new MinSampleShadingTest(testCtx, "primitive_point_1px_sparse",	"", samples[samplesNdx], testConfig.minSampleShading, GEOMETRY_TYPE_OPAQUE_POINT, 1.0f, IMAGE_BACKING_MODE_SPARSE));
					samplesTests->addChild(new MinSampleShadingTest(testCtx, "primitive_point_sparse",		"", samples[samplesNdx], testConfig.minSampleShading, GEOMETRY_TYPE_OPAQUE_POINT, 3.0f, IMAGE_BACKING_MODE_SPARSE));

					minShadingValueTests->addChild(samplesTests.release());
				}

				minSampleShadingTests->addChild(minShadingValueTests.release());
			}

			multisampleTests->addChild(minSampleShadingTests.release());
		}

		{
			de::MovePtr<tcu::TestCaseGroup> minSampleShadingTests(new tcu::TestCaseGroup(testCtx, "min_sample_shading_enabled", ""));

			for (int configNdx = 0; configNdx < DE_LENGTH_OF_ARRAY(testConfigs); configNdx++)
			{
				const TestConfig&				testConfig				= testConfigs[configNdx];
				de::MovePtr<tcu::TestCaseGroup>	minShadingValueTests	(new tcu::TestCaseGroup(testCtx, testConfigs[configNdx].name, ""));

				for (int samplesNdx = 0; samplesNdx < DE_LENGTH_OF_ARRAY(samples); samplesNdx++)
				{
					std::ostringstream caseName;
					caseName << "samples_" << samples[samplesNdx];

					de::MovePtr<tcu::TestCaseGroup> samplesTests	(new tcu::TestCaseGroup(testCtx, caseName.str().c_str(), ""));

					samplesTests->addChild(new MinSampleShadingTest(testCtx, "quad",	"", samples[samplesNdx], testConfig.minSampleShading, GEOMETRY_TYPE_OPAQUE_QUAD, 1.0f, IMAGE_BACKING_MODE_REGULAR, true));

					minShadingValueTests->addChild(samplesTests.release());
				}

				minSampleShadingTests->addChild(minShadingValueTests.release());
			}

			multisampleTests->addChild(minSampleShadingTests.release());
		}

		{
			de::MovePtr<tcu::TestCaseGroup> minSampleShadingTests(new tcu::TestCaseGroup(testCtx, "min_sample_shading_disabled", ""));

			for (int configNdx = 0; configNdx < DE_LENGTH_OF_ARRAY(testConfigs); configNdx++)
			{
				const TestConfig&				testConfig				= testConfigs[configNdx];
				de::MovePtr<tcu::TestCaseGroup>	minShadingValueTests	(new tcu::TestCaseGroup(testCtx, testConfigs[configNdx].name, ""));

				for (int samplesNdx = 0; samplesNdx < DE_LENGTH_OF_ARRAY(samples); samplesNdx++)
				{
					std::ostringstream caseName;
					caseName << "samples_" << samples[samplesNdx];

					de::MovePtr<tcu::TestCaseGroup> samplesTests	(new tcu::TestCaseGroup(testCtx, caseName.str().c_str(), ""));

					samplesTests->addChild(new MinSampleShadingTest(testCtx, "quad",	"", samples[samplesNdx], testConfig.minSampleShading, GEOMETRY_TYPE_OPAQUE_QUAD, 1.0f, IMAGE_BACKING_MODE_REGULAR, false));

					minShadingValueTests->addChild(samplesTests.release());
				}

				minSampleShadingTests->addChild(minShadingValueTests.release());
			}

			multisampleTests->addChild(minSampleShadingTests.release());
		}
	}

	// SampleMask tests
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

				samplesTests->addChild(new SampleMaskTest(testCtx, "primitive_triangle", "", samples[samplesNdx], mask, GEOMETRY_TYPE_OPAQUE_TRIANGLE, 1.0f, IMAGE_BACKING_MODE_REGULAR));
				samplesTests->addChild(new SampleMaskTest(testCtx, "primitive_line", "", samples[samplesNdx], mask, GEOMETRY_TYPE_OPAQUE_LINE, 1.0f, IMAGE_BACKING_MODE_REGULAR));
				samplesTests->addChild(new SampleMaskTest(testCtx, "primitive_point_1px", "", samples[samplesNdx], mask, GEOMETRY_TYPE_OPAQUE_POINT, 1.0f, IMAGE_BACKING_MODE_REGULAR));
				samplesTests->addChild(new SampleMaskTest(testCtx, "primitive_point", "", samples[samplesNdx], mask, GEOMETRY_TYPE_OPAQUE_POINT, 3.0f, IMAGE_BACKING_MODE_REGULAR));

				samplesTests->addChild(new SampleMaskTest(testCtx, "primitive_triangle_sparse", "", samples[samplesNdx], mask, GEOMETRY_TYPE_OPAQUE_TRIANGLE, 1.0f, IMAGE_BACKING_MODE_SPARSE));
				samplesTests->addChild(new SampleMaskTest(testCtx, "primitive_line_sparse", "", samples[samplesNdx], mask, GEOMETRY_TYPE_OPAQUE_LINE, 1.0f, IMAGE_BACKING_MODE_SPARSE));
				samplesTests->addChild(new SampleMaskTest(testCtx, "primitive_point_1px_sparse", "", samples[samplesNdx], mask, GEOMETRY_TYPE_OPAQUE_POINT, 1.0f, IMAGE_BACKING_MODE_SPARSE));
				samplesTests->addChild(new SampleMaskTest(testCtx, "primitive_point_sparse", "", samples[samplesNdx], mask, GEOMETRY_TYPE_OPAQUE_POINT, 3.0f, IMAGE_BACKING_MODE_SPARSE));

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

			alphaToOneTests->addChild(new AlphaToOneTest(testCtx, caseName.str(), "", samples[samplesNdx], IMAGE_BACKING_MODE_REGULAR));

			caseName << "_sparse";
			alphaToOneTests->addChild(new AlphaToOneTest(testCtx, caseName.str(), "", samples[samplesNdx], IMAGE_BACKING_MODE_SPARSE));
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

			samplesTests->addChild(new AlphaToCoverageTest(testCtx, "alpha_opaque", "", samples[samplesNdx], GEOMETRY_TYPE_OPAQUE_QUAD, IMAGE_BACKING_MODE_REGULAR));
			samplesTests->addChild(new AlphaToCoverageTest(testCtx, "alpha_translucent", "", samples[samplesNdx], GEOMETRY_TYPE_TRANSLUCENT_QUAD, IMAGE_BACKING_MODE_REGULAR));
			samplesTests->addChild(new AlphaToCoverageTest(testCtx, "alpha_invisible", "", samples[samplesNdx], GEOMETRY_TYPE_INVISIBLE_QUAD, IMAGE_BACKING_MODE_REGULAR));

			samplesTests->addChild(new AlphaToCoverageTest(testCtx, "alpha_opaque_sparse", "", samples[samplesNdx], GEOMETRY_TYPE_OPAQUE_QUAD, IMAGE_BACKING_MODE_SPARSE));
			samplesTests->addChild(new AlphaToCoverageTest(testCtx, "alpha_translucent_sparse", "", samples[samplesNdx], GEOMETRY_TYPE_TRANSLUCENT_QUAD, IMAGE_BACKING_MODE_SPARSE));
			samplesTests->addChild(new AlphaToCoverageTest(testCtx, "alpha_invisible_sparse", "", samples[samplesNdx], GEOMETRY_TYPE_INVISIBLE_QUAD, IMAGE_BACKING_MODE_SPARSE));

			alphaToCoverageTests->addChild(samplesTests.release());
		}
		multisampleTests->addChild(alphaToCoverageTests.release());
	}

	// AlphaToCoverageEnable without color buffer tests
	{
		de::MovePtr<tcu::TestCaseGroup> alphaToCoverageNoColorAttachmentTests (new tcu::TestCaseGroup(testCtx, "alpha_to_coverage_no_color_attachment", ""));

		for (int samplesNdx = 0; samplesNdx < DE_LENGTH_OF_ARRAY(samples); samplesNdx++)
		{
			std::ostringstream caseName;
			caseName << "samples_" << samples[samplesNdx];

			de::MovePtr<tcu::TestCaseGroup> samplesTests	(new tcu::TestCaseGroup(testCtx, caseName.str().c_str(), ""));

			samplesTests->addChild(new AlphaToCoverageNoColorAttachmentTest(testCtx, "alpha_opaque", "", samples[samplesNdx], GEOMETRY_TYPE_OPAQUE_QUAD, IMAGE_BACKING_MODE_REGULAR));
			samplesTests->addChild(new AlphaToCoverageNoColorAttachmentTest(testCtx, "alpha_opaque_sparse", "", samples[samplesNdx], GEOMETRY_TYPE_OPAQUE_QUAD, IMAGE_BACKING_MODE_SPARSE));

			alphaToCoverageNoColorAttachmentTests->addChild(samplesTests.release());
		}
		multisampleTests->addChild(alphaToCoverageNoColorAttachmentTests.release());
	}

	// AlphaToCoverageEnable with unused color attachment:
	// Set color output at location 0 as unused, but use the alpha write to control coverage for rendering to color buffer at location 1.
	{
		de::MovePtr<tcu::TestCaseGroup> alphaToCoverageColorUnusedAttachmentTests (new tcu::TestCaseGroup(testCtx, "alpha_to_coverage_unused_attachment", ""));

		for (int samplesNdx = 0; samplesNdx < DE_LENGTH_OF_ARRAY(samples); samplesNdx++)
		{
			std::ostringstream caseName;
			caseName << "samples_" << samples[samplesNdx];

			de::MovePtr<tcu::TestCaseGroup> samplesTests	(new tcu::TestCaseGroup(testCtx, caseName.str().c_str(), ""));

			samplesTests->addChild(new AlphaToCoverageColorUnusedAttachmentTest(testCtx, "alpha_opaque", "", samples[samplesNdx], GEOMETRY_TYPE_OPAQUE_QUAD, IMAGE_BACKING_MODE_REGULAR));
			samplesTests->addChild(new AlphaToCoverageColorUnusedAttachmentTest(testCtx, "alpha_opaque_sparse", "", samples[samplesNdx], GEOMETRY_TYPE_OPAQUE_QUAD, IMAGE_BACKING_MODE_SPARSE));
			samplesTests->addChild(new AlphaToCoverageColorUnusedAttachmentTest(testCtx, "alpha_invisible", "", samples[samplesNdx], GEOMETRY_TYPE_INVISIBLE_QUAD, IMAGE_BACKING_MODE_REGULAR));
			samplesTests->addChild(new AlphaToCoverageColorUnusedAttachmentTest(testCtx, "alpha_invisible_sparse", "", samples[samplesNdx], GEOMETRY_TYPE_INVISIBLE_QUAD, IMAGE_BACKING_MODE_SPARSE));

			alphaToCoverageColorUnusedAttachmentTests->addChild(samplesTests.release());
		}
		multisampleTests->addChild(alphaToCoverageColorUnusedAttachmentTests.release());
	}

	// Sampling from a multisampled image texture (texelFetch)
	{
		multisampleTests->addChild(createMultisampleSampledImageTests(testCtx));
	}

	// Load/store on a multisampled rendered image (different kinds of access: color attachment write, storage image, etc.)
	{
		multisampleTests->addChild(createMultisampleStorageImageTests(testCtx));
	}

	// Sampling from a multisampled image texture (texelFetch), checking supersample positions
	{
		multisampleTests->addChild(createMultisampleStandardSamplePositionTests(testCtx));
	}

	// VK_EXT_sample_locations
	{
		multisampleTests->addChild(createMultisampleSampleLocationsExtTests(testCtx));
	}

	// VK_AMD_mixed_attachment samples and VK_AMD_shader_fragment_mask
	{
		multisampleTests->addChild(createMultisampleMixedAttachmentSamplesTests(testCtx));
		multisampleTests->addChild(createMultisampleShaderFragmentMaskTests(testCtx));
	}

	// Sample mask with and without vk_ext_post_depth_coverage
	{
		const vk::VkSampleCountFlagBits standardSamplesSet[] =
		{
			vk::VK_SAMPLE_COUNT_2_BIT,
			vk::VK_SAMPLE_COUNT_4_BIT,
			vk::VK_SAMPLE_COUNT_8_BIT,
			vk::VK_SAMPLE_COUNT_16_BIT
		};

		de::MovePtr<tcu::TestCaseGroup> sampleMaskWithDepthTestGroup(new tcu::TestCaseGroup(testCtx, "sample_mask_with_depth_test", ""));

		for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(standardSamplesSet); ++ndx)
		{
			std::ostringstream caseName;
			caseName << "samples_" << standardSamplesSet[ndx];

			sampleMaskWithDepthTestGroup->addChild(new SampleMaskWithDepthTestTest(testCtx, caseName.str(), "", standardSamplesSet[ndx]));

			caseName << "_post_depth_coverage";
			sampleMaskWithDepthTestGroup->addChild(new SampleMaskWithDepthTestTest(testCtx, caseName.str(), "", standardSamplesSet[ndx], true));

		}
		multisampleTests->addChild(sampleMaskWithDepthTestGroup.release());
	}

	{
		static const std::vector<vk::VkSampleCountFlagBits> kSampleCounts =
		{
			vk::VK_SAMPLE_COUNT_1_BIT,
			vk::VK_SAMPLE_COUNT_2_BIT,
			vk::VK_SAMPLE_COUNT_4_BIT,
			vk::VK_SAMPLE_COUNT_8_BIT,
			vk::VK_SAMPLE_COUNT_16_BIT,
			vk::VK_SAMPLE_COUNT_32_BIT,
			vk::VK_SAMPLE_COUNT_64_BIT,
		};

		static const std::array<bool, 2> unusedAttachmentFlag = {{ false, true }};

		{
			de::MovePtr<tcu::TestCaseGroup> variableRateGroup(new tcu::TestCaseGroup(testCtx, "variable_rate", "Tests for multisample variable rate in subpasses"));

			// 2 and 3 subpasses should be good enough.
			static const std::vector<size_t> combinationSizes = { 2, 3 };

			// Basic cases.
			for (const auto size : combinationSizes)
			{
				const auto combs = combinations(kSampleCounts, size);
				for (const auto& comb : combs)
				{
					// Check sample counts actually vary between some of the subpasses.
					std::set<vk::VkSampleCountFlagBits> uniqueVals(begin(comb), end(comb));
					if (uniqueVals.size() < 2)
						continue;

					std::ostringstream name;
					std::ostringstream desc;

					bool first = true;
					for (const auto& count : comb)
					{
						name << (first ? "" : "_") << count;
						desc << (first ? "Subpasses with counts " : ", ") << count;
						first = false;
					}

					const VariableRateTestCase::TestParams params =
					{
						false,						//	bool						nonEmptyFramebuffer;
						vk::VK_SAMPLE_COUNT_1_BIT,	//	vk::VkSampleCountFlagBits	fbCount;
						false,						//	bool						unusedAttachment;
						comb,						//	SampleCounts				subpassCounts;
					};
					variableRateGroup->addChild(new VariableRateTestCase(testCtx, name.str(), desc.str(), params));
				}
			}

			// Cases with non-empty framebuffers: only 2 subpasses to avoid a large number of combinations.
			{
				// Use one more sample count for the framebuffer attachment. It will be taken from the last item.
				auto combs = combinations(kSampleCounts, 2 + 1);
				for (auto& comb : combs)
				{
					// Framebuffer sample count.
					const auto fbCount = comb.back();
					comb.pop_back();

					// Check sample counts actually vary between some of the subpasses.
					std::set<vk::VkSampleCountFlagBits> uniqueVals(begin(comb), end(comb));
					if (uniqueVals.size() < 2)
						continue;

					for (const auto flag : unusedAttachmentFlag)
					{
						std::ostringstream name;
						std::ostringstream desc;

						desc << "Framebuffer with sample count " << fbCount << " and subpasses with counts ";

						bool first = true;
						for (const auto& count : comb)
						{
							name << (first ? "" : "_") << count;
							desc << (first ? "" : ", ") << count;
							first = false;
						}

						name << "_fb_" << fbCount;

						if (flag)
						{
							name << "_unused";
							desc << " and unused attachments";
						}

						const VariableRateTestCase::TestParams params =
						{
							true,						//	bool						nonEmptyFramebuffer;
							fbCount,					//	vk::VkSampleCountFlagBits	fbCount;
							flag,						//	bool						unusedAttachment;
							comb,						//	SampleCounts				subpassCounts;
						};
						variableRateGroup->addChild(new VariableRateTestCase(testCtx, name.str(), desc.str(), params));
					}
				}
			}

			multisampleTests->addChild(variableRateGroup.release());
		}

		{
			de::MovePtr<tcu::TestCaseGroup> mixedCountGroup(new tcu::TestCaseGroup(testCtx, "mixed_count", "Tests for mixed sample count in empty subpass and framebuffer"));

			const auto combs = combinations(kSampleCounts, 2);
			for (const auto& comb : combs)
			{
				// Check different sample count.
				DE_ASSERT(comb.size() == 2u);
				const auto& fbCount		= comb[0];
				const auto& emptyCount	= comb[1];

				if (fbCount == emptyCount)
					continue;

				const std::string fbCountStr	= de::toString(fbCount);
				const std::string emptyCountStr	= de::toString(emptyCount);

				for (const auto flag : unusedAttachmentFlag)
				{
					const std::string nameSuffix	= (flag ? "unused" : "");
					const std::string descSuffix	= (flag ? "one unused attachment reference" : "no attachment references");
					const std::string name			= fbCountStr + "_" + emptyCountStr + (nameSuffix.empty() ? "" : "_") + nameSuffix;
					const std::string desc			= "Framebuffer with " + fbCountStr + " samples, subpass with " + emptyCountStr + " samples and " + descSuffix;

					const VariableRateTestCase::TestParams params =
					{
						true,												//	bool						nonEmptyFramebuffer;
						fbCount,											//	vk::VkSampleCountFlagBits	fbCount;
						flag,												//	bool						unusedAttachment;
						VariableRateTestCase::SampleCounts(1u, emptyCount),	//	SampleCounts				subpassCounts;
					};
					mixedCountGroup->addChild(new VariableRateTestCase(testCtx, name, desc, params));
				}
			}

			multisampleTests->addChild(mixedCountGroup.release());
		}
	}

	return multisampleTests.release();
}

} // pipeline
} // vkt
