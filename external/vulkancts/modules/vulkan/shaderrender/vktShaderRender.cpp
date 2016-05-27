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
 * \brief Vulkan ShaderRenderCase
 *//*--------------------------------------------------------------------*/

#include "vktShaderRender.hpp"

#include "tcuImageCompare.hpp"
#include "tcuImageIO.hpp"
#include "tcuTestLog.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuSurface.hpp"
#include "tcuVector.hpp"

#include "deFilePath.hpp"
#include "deMath.h"
#include "deUniquePtr.hpp"

#include "vkDeviceUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkPlatform.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkStrUtil.hpp"
#include "vkTypeUtil.hpp"

#include <vector>
#include <string>

namespace vkt
{
namespace sr
{

using namespace vk;

namespace
{

static const int		GRID_SIZE			= 2;
static const deUint32	MAX_RENDER_WIDTH	= 128;
static const deUint32	MAX_RENDER_HEIGHT	= 128;
static const tcu::Vec4	DEFAULT_CLEAR_COLOR	= tcu::Vec4(0.125f, 0.25f, 0.5f, 1.0f);

static bool isSupportedLinearTilingFormat (const InstanceInterface& instanceInterface, VkPhysicalDevice device, VkFormat format)
{
	VkFormatProperties formatProps;

	instanceInterface.getPhysicalDeviceFormatProperties(device, format, &formatProps);

	return (formatProps.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0u;
}

static bool isSupportedOptimalTilingFormat (const InstanceInterface& instanceInterface, VkPhysicalDevice device, VkFormat format)
{
	VkFormatProperties formatProps;

	instanceInterface.getPhysicalDeviceFormatProperties(device, format, &formatProps);

	return (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0u;
}

static VkImageMemoryBarrier createImageMemoryBarrier (const VkImage&	image,
													  VkAccessFlags		srcAccessMask,
													  VkAccessFlags		dstAccessMask,
													  VkImageLayout		oldLayout,
													  VkImageLayout		newLayout)
{
	VkImageMemoryBarrier imageMemoryBarrier	=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType				sType;
		DE_NULL,									// const void*					pNext;
		srcAccessMask,								// VkAccessFlags				srcAccessMask;
		dstAccessMask,								// VkAccessFlags				dstAccessMask;
		oldLayout,									// VkImageLayout				oldLayout;
		newLayout,									// VkImageLayout				newLayout;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32						srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32						dstQueueFamilyIndex;
		image,										// VkImage						image;
		{
			VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
			0,							// deUint32				baseMipLevel;
			1,							// deUint32				mipLevels;
			0,							// deUint32				baseArrayLayer;
			1							// deUint32				arraySize;
		}											// VkImageSubresourceRange		subresourceRange;
	};
	return imageMemoryBarrier;
}

} // anonymous

// QuadGrid.

class QuadGrid
{
public:
											QuadGrid				(int									gridSize,
																	 int									screenWidth,
																	 int									screenHeight,
																	 const tcu::Vec4&						constCoords,
																	 const std::vector<tcu::Mat4>&			userAttribTransforms,
																	 const std::vector<TextureBindingSp>&	textures);
											~QuadGrid				(void);

	int										getGridSize				(void) const { return m_gridSize; }
	int										getNumVertices			(void) const { return m_numVertices; }
	int										getNumTriangles			(void) const { return m_numTriangles; }
	const tcu::Vec4&						getConstCoords			(void) const { return m_constCoords; }
	const std::vector<tcu::Mat4>			getUserAttribTransforms	(void) const { return m_userAttribTransforms; }
	const std::vector<TextureBindingSp>&	getTextures				(void) const { return m_textures; }

	const tcu::Vec4*						getPositions			(void) const { return &m_positions[0]; }
	const float*							getAttribOne			(void) const { return &m_attribOne[0]; }
	const tcu::Vec4*						getCoords				(void) const { return &m_coords[0]; }
	const tcu::Vec4*						getUnitCoords			(void) const { return &m_unitCoords[0]; }

	const tcu::Vec4*						getUserAttrib			(int attribNdx) const { return &m_userAttribs[attribNdx][0]; }
	const deUint16*							getIndices				(void) const { return &m_indices[0]; }

	tcu::Vec4								getCoords				(float sx, float sy) const;
	tcu::Vec4								getUnitCoords			(float sx, float sy) const;

	int										getNumUserAttribs		(void) const { return (int)m_userAttribTransforms.size(); }
	tcu::Vec4								getUserAttrib			(int attribNdx, float sx, float sy) const;

private:
	const int								m_gridSize;
	const int								m_numVertices;
	const int								m_numTriangles;
	const tcu::Vec4							m_constCoords;
	const std::vector<tcu::Mat4>			m_userAttribTransforms;

	const std::vector<TextureBindingSp>&	m_textures;

	std::vector<tcu::Vec4>					m_screenPos;
	std::vector<tcu::Vec4>					m_positions;
	std::vector<tcu::Vec4>					m_coords;		//!< Near-unit coordinates, roughly [-2.0 .. 2.0].
	std::vector<tcu::Vec4>					m_unitCoords;	//!< Positive-only coordinates [0.0 .. 1.5].
	std::vector<float>						m_attribOne;
	std::vector<tcu::Vec4>					m_userAttribs[ShaderEvalContext::MAX_TEXTURES];
	std::vector<deUint16>					m_indices;
};

QuadGrid::QuadGrid (int										gridSize,
					int										width,
					int										height,
					const tcu::Vec4&						constCoords,
					const std::vector<tcu::Mat4>&			userAttribTransforms,
					const std::vector<TextureBindingSp>&	textures)
	: m_gridSize				(gridSize)
	, m_numVertices				((gridSize + 1) * (gridSize + 1))
	, m_numTriangles			(gridSize * gridSize * 2)
	, m_constCoords				(constCoords)
	, m_userAttribTransforms	(userAttribTransforms)
	, m_textures				(textures)
{
	const tcu::Vec4 viewportScale	((float)width, (float)height, 0.0f, 0.0f);

	// Compute vertices.
	m_screenPos.resize(m_numVertices);
	m_positions.resize(m_numVertices);
	m_coords.resize(m_numVertices);
	m_unitCoords.resize(m_numVertices);
	m_attribOne.resize(m_numVertices);

	// User attributes.
	for (int attrNdx = 0; attrNdx < DE_LENGTH_OF_ARRAY(m_userAttribs); attrNdx++)
		m_userAttribs[attrNdx].resize(m_numVertices);

	for (int y = 0; y < gridSize+1; y++)
	for (int x = 0; x < gridSize+1; x++)
	{
		float		sx			= (float)x / (float)gridSize;
		float		sy			= (float)y / (float)gridSize;
		float		fx			= 2.0f * sx - 1.0f;
		float		fy			= 2.0f * sy - 1.0f;
		int			vtxNdx		= ((y * (gridSize+1)) + x);

		m_positions[vtxNdx]		= tcu::Vec4(fx, fy, 0.0f, 1.0f);
		m_coords[vtxNdx]		= getCoords(sx, sy);
		m_unitCoords[vtxNdx]	= getUnitCoords(sx, sy);
		m_attribOne[vtxNdx]		= 1.0f;

		m_screenPos[vtxNdx]		= tcu::Vec4(sx, sy, 0.0f, 1.0f) * viewportScale;

		for (int attribNdx = 0; attribNdx < getNumUserAttribs(); attribNdx++)
			m_userAttribs[attribNdx][vtxNdx] = getUserAttrib(attribNdx, sx, sy);
	}

	// Compute indices.
	m_indices.resize(3 * m_numTriangles);
	for (int y = 0; y < gridSize; y++)
	for (int x = 0; x < gridSize; x++)
	{
		int stride				= gridSize + 1;
		int v00					= (y * stride) + x;
		int v01					= (y * stride) + x + 1;
		int v10					= ((y+1) * stride) + x;
		int v11					= ((y+1) * stride) + x + 1;

		int baseNdx				= ((y * gridSize) + x) * 6;
		m_indices[baseNdx + 0]	= (deUint16)v10;
		m_indices[baseNdx + 1]	= (deUint16)v00;
		m_indices[baseNdx + 2]	= (deUint16)v01;

		m_indices[baseNdx + 3]	= (deUint16)v10;
		m_indices[baseNdx + 4]	= (deUint16)v01;
		m_indices[baseNdx + 5]	= (deUint16)v11;
	}
}

QuadGrid::~QuadGrid (void)
{
}

inline tcu::Vec4 QuadGrid::getCoords (float sx, float sy) const
{
	const float fx = 2.0f * sx - 1.0f;
	const float fy = 2.0f * sy - 1.0f;
	return tcu::Vec4(fx, fy, -fx + 0.33f*fy, -0.275f*fx - fy);
}

inline tcu::Vec4 QuadGrid::getUnitCoords (float sx, float sy) const
{
	return tcu::Vec4(sx, sy, 0.33f*sx + 0.5f*sy, 0.5f*sx + 0.25f*sy);
}

inline tcu::Vec4 QuadGrid::getUserAttrib (int attribNdx, float sx, float sy) const
{
	// homogeneous normalized screen-space coordinates
	return m_userAttribTransforms[attribNdx] * tcu::Vec4(sx, sy, 0.0f, 1.0f);
}

// TextureBinding

TextureBinding::TextureBinding (const tcu::Archive&	archive,
								const char*			filename,
								const Type			type,
								const tcu::Sampler&	sampler)
	: m_type	(type)
	, m_sampler	(sampler)
{
	switch(m_type)
	{
		case TYPE_2D: m_binding.tex2D = loadTexture2D(archive, filename).release(); break;
		default:
			DE_FATAL("Unsupported texture type");
	}
}

TextureBinding::~TextureBinding (void)
{
	switch(m_type)
	{
		case TYPE_2D: delete m_binding.tex2D; break;
		default: break;
	}
}


de::MovePtr<tcu::Texture2D> TextureBinding::loadTexture2D (const tcu::Archive& archive, const char* filename)
{
	tcu::TextureLevel level;
	tcu::ImageIO::loadImage(level, archive, filename);

	TCU_CHECK_INTERNAL(level.getFormat() == tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8) ||
					   level.getFormat() == tcu::TextureFormat(tcu::TextureFormat::RGB, tcu::TextureFormat::UNORM_INT8));

	// \todo [2015-10-08 elecro] for some reason we get better when using RGBA texture even in RGB case, this needs to be investigated
	de::MovePtr<tcu::Texture2D> texture(new tcu::Texture2D(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8), level.getWidth(), level.getHeight()));

	// Fill level 0.
	texture->allocLevel(0);
	tcu::copy(texture->getLevel(0), level.getAccess());

	return texture;
}

// ShaderEvalContext.

ShaderEvalContext::ShaderEvalContext (const QuadGrid& quadGrid)
	: constCoords	(quadGrid.getConstCoords())
	, isDiscarded	(false)
	, m_quadGrid	(quadGrid)
{
	const std::vector<TextureBindingSp>& bindings = m_quadGrid.getTextures();
	DE_ASSERT((int)bindings.size() <= MAX_TEXTURES);

	// Fill in texture array.
	for (int ndx = 0; ndx < (int)bindings.size(); ndx++)
	{
		const TextureBinding& binding = *bindings[ndx];

		if (binding.getType() == TextureBinding::TYPE_NONE)
			continue;

		textures[ndx].sampler = binding.getSampler();

		switch (binding.getType())
		{
			case TextureBinding::TYPE_2D:		textures[ndx].tex2D			= &binding.get2D();		break;
			// \todo [2015-09-07 elecro] Add support for the other binding types
			/*
			case TextureBinding::TYPE_CUBE_MAP:	textures[ndx].texCube		= binding.getCube();	break;
			case TextureBinding::TYPE_2D_ARRAY:	textures[ndx].tex2DArray	= binding.get2DArray();	break;
			case TextureBinding::TYPE_3D:		textures[ndx].tex3D			= binding.get3D();		break;
			*/
			default:
				TCU_THROW(InternalError, "Handling of texture binding type not implemented");
		}
	}
}

ShaderEvalContext::~ShaderEvalContext (void)
{
}

void ShaderEvalContext::reset (float sx, float sy)
{
	// Clear old values
	color		= tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
	isDiscarded	= false;

	// Compute coords
	coords		= m_quadGrid.getCoords(sx, sy);
	unitCoords	= m_quadGrid.getUnitCoords(sx, sy);

	// Compute user attributes.
	const int numAttribs = m_quadGrid.getNumUserAttribs();
	DE_ASSERT(numAttribs <= MAX_USER_ATTRIBS);
	for (int attribNdx = 0; attribNdx < numAttribs; attribNdx++)
		in[attribNdx] = m_quadGrid.getUserAttrib(attribNdx, sx, sy);
}

tcu::Vec4 ShaderEvalContext::texture2D (int unitNdx, const tcu::Vec2& texCoords)
{
	if (textures[unitNdx].tex2D)
		return textures[unitNdx].tex2D->sample(textures[unitNdx].sampler, texCoords.x(), texCoords.y(), 0.0f);
	else
		return tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
}

// ShaderEvaluator.

ShaderEvaluator::ShaderEvaluator (void)
	: m_evalFunc(DE_NULL)
{
}

ShaderEvaluator::ShaderEvaluator (ShaderEvalFunc evalFunc)
	: m_evalFunc(evalFunc)
{
}

ShaderEvaluator::~ShaderEvaluator (void)
{
}

void ShaderEvaluator::evaluate (ShaderEvalContext& ctx) const
{
	DE_ASSERT(m_evalFunc);
	m_evalFunc(ctx);
}

// UniformSetup.

UniformSetup::UniformSetup (void)
	: m_setupFunc(DE_NULL)
{
}

UniformSetup::UniformSetup (UniformSetupFunc setupFunc)
	: m_setupFunc(setupFunc)
{
}

UniformSetup::~UniformSetup (void)
{
}

void UniformSetup::setup (ShaderRenderCaseInstance& instance, const tcu::Vec4& constCoords) const
{
	if (m_setupFunc)
		m_setupFunc(instance, constCoords);
}

// ShaderRenderCase.

ShaderRenderCase::ShaderRenderCase (tcu::TestContext&			testCtx,
									const std::string&			name,
									const std::string&			description,
									const bool					isVertexCase,
									const ShaderEvalFunc		evalFunc,
									const UniformSetup*			uniformSetup,
									const AttributeSetupFunc	attribFunc)
	: vkt::TestCase		(testCtx, name, description)
	, m_isVertexCase	(isVertexCase)
	, m_evaluator		(new ShaderEvaluator(evalFunc))
	, m_uniformSetup	(uniformSetup ? uniformSetup : new UniformSetup())
	, m_attribFunc		(attribFunc)
{}

ShaderRenderCase::ShaderRenderCase (tcu::TestContext&			testCtx,
									const std::string&			name,
									const std::string&			description,
									const bool					isVertexCase,
									const ShaderEvaluator*		evaluator,
									const UniformSetup*			uniformSetup,
									const AttributeSetupFunc	attribFunc)
	: vkt::TestCase		(testCtx, name, description)
	, m_isVertexCase	(isVertexCase)
	, m_evaluator		(evaluator)
	, m_uniformSetup	(uniformSetup ? uniformSetup : new UniformSetup())
	, m_attribFunc		(attribFunc)
{}

ShaderRenderCase::~ShaderRenderCase (void)
{
}

void ShaderRenderCase::initPrograms (vk::SourceCollections& programCollection) const
{
	programCollection.glslSources.add("vert") << glu::VertexSource(m_vertShaderSource);
	programCollection.glslSources.add("frag") << glu::FragmentSource(m_fragShaderSource);
}

TestInstance* ShaderRenderCase::createInstance (Context& context) const
{
	DE_ASSERT(m_evaluator != DE_NULL);
	DE_ASSERT(m_uniformSetup != DE_NULL);
	return new ShaderRenderCaseInstance(context, m_isVertexCase, *m_evaluator, *m_uniformSetup, m_attribFunc);
}

// ShaderRenderCaseInstance.

ShaderRenderCaseInstance::ShaderRenderCaseInstance (Context&					context,
													const bool					isVertexCase,
													const ShaderEvaluator&		evaluator,
													const UniformSetup&			uniformSetup,
													const AttributeSetupFunc	attribFunc)
	: vkt::TestInstance	(context)
	, m_clearColor		(DEFAULT_CLEAR_COLOR)
	, m_memAlloc		(context.getDefaultAllocator())
	, m_isVertexCase	(isVertexCase)
	, m_evaluator		(evaluator)
	, m_uniformSetup	(uniformSetup)
	, m_attribFunc		(attribFunc)
	, m_renderSize		(128, 128)
	, m_colorFormat		(VK_FORMAT_R8G8B8A8_UNORM)
{
}

ShaderRenderCaseInstance::~ShaderRenderCaseInstance (void)
{
}

tcu::TestStatus ShaderRenderCaseInstance::iterate (void)
{
	setup();

	// Create quad grid.
	const tcu::UVec2	viewportSize	= getViewportSize();
	const int			width			= viewportSize.x();
	const int			height			= viewportSize.y();

	QuadGrid			quadGrid		(m_isVertexCase ? GRID_SIZE : 4, width, height, tcu::Vec4(0.125f, 0.25f, 0.5f, 1.0f), m_userAttribTransforms, m_textures);

	// Render result.
	tcu::Surface		resImage		(width, height);
	render(resImage, quadGrid);

	// Compute reference.
	tcu::Surface		refImage		(width, height);
	if (m_isVertexCase)
		computeVertexReference(refImage, quadGrid);
	else
		computeFragmentReference(refImage, quadGrid);

	// Compare.
	const bool			compareOk		= compareImages(resImage, refImage, 0.05f);

	if (compareOk)
		return tcu::TestStatus::pass("Result image matches reference");
	else
		return tcu::TestStatus::fail("Image mismatch");
}

void ShaderRenderCaseInstance::setupUniformData (deUint32 bindingLocation, size_t size, const void* dataPtr)
{
	const VkDevice					vkDevice			= m_context.getDevice();
	const DeviceInterface&			vk					= m_context.getDeviceInterface();
	const deUint32					queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();

	const VkBufferCreateInfo		uniformBufferParams	=
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
		DE_NULL,									// const void*			pNext;
		0u,											// VkBufferCreateFlags	flags;
		size,										// VkDeviceSize			size;
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,			// VkBufferUsageFlags	usage;
		VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
		1u,											// deUint32				queueFamilyCount;
		&queueFamilyIndex							// const deUint32*		pQueueFamilyIndices;
	};

	Move<VkBuffer>					buffer				= createBuffer(vk, vkDevice, &uniformBufferParams);
	de::MovePtr<Allocation>			alloc				= m_memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *buffer), MemoryRequirement::HostVisible);
	VK_CHECK(vk.bindBufferMemory(vkDevice, *buffer, alloc->getMemory(), alloc->getOffset()));

	deMemcpy(alloc->getHostPtr(), dataPtr, size);
	flushMappedMemoryRange(vk, vkDevice, alloc->getMemory(), alloc->getOffset(), size);

	de::MovePtr<BufferUniform> uniformInfo(new BufferUniform());
	uniformInfo->type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uniformInfo->descriptor = makeDescriptorBufferInfo(*buffer, 0u, size);
	uniformInfo->location = bindingLocation;
	uniformInfo->buffer = VkBufferSp(new vk::Unique<VkBuffer>(buffer));
	uniformInfo->alloc = AllocationSp(alloc.release());

	m_uniformInfos.push_back(UniformInfoSp(new de::UniquePtr<UniformInfo>(uniformInfo)));
}

void ShaderRenderCaseInstance::addUniform (deUint32 bindingLocation, vk::VkDescriptorType descriptorType, size_t dataSize, const void* data)
{
	m_descriptorSetLayoutBuilder.addSingleBinding(descriptorType, vk::VK_SHADER_STAGE_ALL);
	m_descriptorPoolBuilder.addType(descriptorType);

	setupUniformData(bindingLocation, dataSize, data);
}

void ShaderRenderCaseInstance::addAttribute (deUint32		bindingLocation,
											 vk::VkFormat	format,
											 deUint32		sizePerElement,
											 deUint32		count,
											 const void*	dataPtr)
{
	// Add binding specification
	const deUint32							binding					= (deUint32)m_vertexBindingDescription.size();
	const VkVertexInputBindingDescription	bindingDescription		=
	{
		binding,							// deUint32				binding;
		sizePerElement,						// deUint32				stride;
		VK_VERTEX_INPUT_RATE_VERTEX			// VkVertexInputRate	stepRate;
	};

	m_vertexBindingDescription.push_back(bindingDescription);

	// Add location and format specification
	const VkVertexInputAttributeDescription	attributeDescription	=
	{
		bindingLocation,			// deUint32	location;
		binding,					// deUint32	binding;
		format,						// VkFormat	format;
		0u,							// deUint32	offset;
	};

	m_vertexattributeDescription.push_back(attributeDescription);

	// Upload data to buffer
	const VkDevice							vkDevice				= m_context.getDevice();
	const DeviceInterface&					vk						= m_context.getDeviceInterface();
	const deUint32							queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();

	const VkDeviceSize						inputSize				= sizePerElement * count;
	const VkBufferCreateInfo				vertexBufferParams		=
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
		DE_NULL,									// const void*			pNext;
		0u,											// VkBufferCreateFlags	flags;
		inputSize,									// VkDeviceSize			size;
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,			// VkBufferUsageFlags	usage;
		VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
		1u,											// deUint32				queueFamilyCount;
		&queueFamilyIndex							// const deUint32*		pQueueFamilyIndices;
	};

	Move<VkBuffer>							buffer					= createBuffer(vk, vkDevice, &vertexBufferParams);
	de::MovePtr<vk::Allocation>				alloc					= m_memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *buffer), MemoryRequirement::HostVisible);
	VK_CHECK(vk.bindBufferMemory(vkDevice, *buffer, alloc->getMemory(), alloc->getOffset()));

	deMemcpy(alloc->getHostPtr(), dataPtr, (size_t)inputSize);
	flushMappedMemoryRange(vk, vkDevice, alloc->getMemory(), alloc->getOffset(), inputSize);

	m_vertexBuffers.push_back(VkBufferSp(new vk::Unique<VkBuffer>(buffer)));
	m_vertexBufferAllocs.push_back(AllocationSp(alloc.release()));
}

void ShaderRenderCaseInstance::useAttribute (deUint32 bindingLocation, BaseAttributeType type)
{
	const EnabledBaseAttribute attribute =
	{
		bindingLocation,	// deUint32				location;
		type				// BaseAttributeType	type;
	};
	m_enabledBaseAttributes.push_back(attribute);
}

void ShaderRenderCaseInstance::setup (void)
{
}

void ShaderRenderCaseInstance::setupUniforms (const tcu::Vec4& constCoords)
{
	m_uniformSetup.setup(*this, constCoords);
}

void ShaderRenderCaseInstance::useUniform (deUint32 bindingLocation, BaseUniformType type)
{
	#define UNIFORM_CASE(type, value) case type: addUniform(bindingLocation, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, value); break

	switch(type)
	{
		// Bool
		UNIFORM_CASE(UB_FALSE,	0);
		UNIFORM_CASE(UB_TRUE,	1);

		// BVec4
		UNIFORM_CASE(UB4_FALSE,	tcu::Vec4(0));
		UNIFORM_CASE(UB4_TRUE,	tcu::Vec4(1));

		// Integer
		UNIFORM_CASE(UI_ZERO,	0);
		UNIFORM_CASE(UI_ONE,	1);
		UNIFORM_CASE(UI_TWO,	2);
		UNIFORM_CASE(UI_THREE,	3);
		UNIFORM_CASE(UI_FOUR,	4);
		UNIFORM_CASE(UI_FIVE,	5);
		UNIFORM_CASE(UI_SIX,	6);
		UNIFORM_CASE(UI_SEVEN,	7);
		UNIFORM_CASE(UI_EIGHT,	8);
		UNIFORM_CASE(UI_ONEHUNDREDONE, 101);

		// IVec2
		UNIFORM_CASE(UI2_MINUS_ONE,	tcu::IVec2(-1));
		UNIFORM_CASE(UI2_ZERO,		tcu::IVec2(0));
		UNIFORM_CASE(UI2_ONE,		tcu::IVec2(1));
		UNIFORM_CASE(UI2_TWO,		tcu::IVec2(2));
		UNIFORM_CASE(UI2_THREE,		tcu::IVec2(3));
		UNIFORM_CASE(UI2_FOUR,		tcu::IVec2(4));
		UNIFORM_CASE(UI2_FIVE,		tcu::IVec2(5));

		// IVec3
		UNIFORM_CASE(UI3_MINUS_ONE,	tcu::IVec3(-1));
		UNIFORM_CASE(UI3_ZERO,		tcu::IVec3(0));
		UNIFORM_CASE(UI3_ONE,		tcu::IVec3(1));
		UNIFORM_CASE(UI3_TWO,		tcu::IVec3(2));
		UNIFORM_CASE(UI3_THREE,		tcu::IVec3(3));
		UNIFORM_CASE(UI3_FOUR,		tcu::IVec3(4));
		UNIFORM_CASE(UI3_FIVE,		tcu::IVec3(5));

		// IVec4
		UNIFORM_CASE(UI4_MINUS_ONE, tcu::IVec4(-1));
		UNIFORM_CASE(UI4_ZERO,		tcu::IVec4(0));
		UNIFORM_CASE(UI4_ONE,		tcu::IVec4(1));
		UNIFORM_CASE(UI4_TWO,		tcu::IVec4(2));
		UNIFORM_CASE(UI4_THREE,		tcu::IVec4(3));
		UNIFORM_CASE(UI4_FOUR,		tcu::IVec4(4));
		UNIFORM_CASE(UI4_FIVE,		tcu::IVec4(5));

		// Float
		UNIFORM_CASE(UF_ZERO,		0.0f);
		UNIFORM_CASE(UF_ONE,		1.0f);
		UNIFORM_CASE(UF_TWO,		2.0f);
		UNIFORM_CASE(UF_THREE,		3.0f);
		UNIFORM_CASE(UF_FOUR,		4.0f);
		UNIFORM_CASE(UF_FIVE,		5.0f);
		UNIFORM_CASE(UF_SIX,		6.0f);
		UNIFORM_CASE(UF_SEVEN,		7.0f);
		UNIFORM_CASE(UF_EIGHT,		8.0f);

		UNIFORM_CASE(UF_HALF,		1.0f / 2.0f);
		UNIFORM_CASE(UF_THIRD,		1.0f / 3.0f);
		UNIFORM_CASE(UF_FOURTH,		1.0f / 4.0f);
		UNIFORM_CASE(UF_FIFTH,		1.0f / 5.0f);
		UNIFORM_CASE(UF_SIXTH,		1.0f / 6.0f);
		UNIFORM_CASE(UF_SEVENTH,	1.0f / 7.0f);
		UNIFORM_CASE(UF_EIGHTH,		1.0f / 8.0f);

		// Vec2
		UNIFORM_CASE(UV2_MINUS_ONE,	tcu::Vec2(-1.0f));
		UNIFORM_CASE(UV2_ZERO,		tcu::Vec2(0.0f));
		UNIFORM_CASE(UV2_ONE,		tcu::Vec2(1.0f));
		UNIFORM_CASE(UV2_TWO,		tcu::Vec2(2.0f));
		UNIFORM_CASE(UV2_THREE,		tcu::Vec2(3.0f));

		UNIFORM_CASE(UV2_HALF,		tcu::Vec2(1.0f / 2.0f));

		// Vec3
		UNIFORM_CASE(UV3_MINUS_ONE,	tcu::Vec3(-1.0f));
		UNIFORM_CASE(UV3_ZERO,		tcu::Vec3(0.0f));
		UNIFORM_CASE(UV3_ONE,		tcu::Vec3(1.0f));
		UNIFORM_CASE(UV3_TWO,		tcu::Vec3(2.0f));
		UNIFORM_CASE(UV3_THREE,		tcu::Vec3(3.0f));

		UNIFORM_CASE(UV3_HALF,		tcu::Vec3(1.0f / 2.0f));

		// Vec4
		UNIFORM_CASE(UV4_MINUS_ONE,	tcu::Vec4(-1.0f));
		UNIFORM_CASE(UV4_ZERO,		tcu::Vec4(0.0f));
		UNIFORM_CASE(UV4_ONE,		tcu::Vec4(1.0f));
		UNIFORM_CASE(UV4_TWO,		tcu::Vec4(2.0f));
		UNIFORM_CASE(UV4_THREE,		tcu::Vec4(3.0f));

		UNIFORM_CASE(UV4_HALF,		tcu::Vec4(1.0f / 2.0f));

		UNIFORM_CASE(UV4_BLACK,		tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
		UNIFORM_CASE(UV4_GRAY,		tcu::Vec4(0.5f, 0.5f, 0.5f, 1.0f));
		UNIFORM_CASE(UV4_WHITE,		tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));

		default:
			m_context.getTestContext().getLog() << tcu::TestLog::Message << "Unknown Uniform type: " << type << tcu::TestLog::EndMessage;
			break;
	}

	#undef UNIFORM_CASE
}

const tcu::UVec2 ShaderRenderCaseInstance::getViewportSize (void) const
{
	return tcu::UVec2(de::min(m_renderSize.x(), MAX_RENDER_WIDTH),
					  de::min(m_renderSize.y(), MAX_RENDER_HEIGHT));
}

Move<VkImage> ShaderRenderCaseInstance::createImage2D (const tcu::Texture2D&	texture,
													   const VkFormat			format,
													   const VkImageUsageFlags	usage,
													   const VkImageTiling		tiling)
{
	const VkDevice			vkDevice			= m_context.getDevice();
	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	const deUint32			queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();

	const VkImageCreateInfo	imageCreateInfo		=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,						// VkStructureType			sType;
		DE_NULL,													// const void*				pNext;
		0,															// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,											// VkImageType				imageType;
		format,														// VkFormat					format;
		{
			(deUint32)texture.getWidth(),
			(deUint32)texture.getHeight(),
			1u
		},															// VkExtend3D				extent;
		1u,															// deUint32					mipLevels;
		1u,															// deUint32					arraySize;
		VK_SAMPLE_COUNT_1_BIT,										// deUint32					samples;
		tiling,														// VkImageTiling			tiling;
		usage,														// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,									// VkSharingMode			sharingMode;
		1,															// deuint32					queueFamilyCount;
		&queueFamilyIndex,											// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,									// VkImageLayout			initialLayout;
	};

	Move<VkImage>			vkTexture			= createImage(vk, vkDevice, &imageCreateInfo);
	return vkTexture;
}

de::MovePtr<Allocation> ShaderRenderCaseInstance::uploadImage2D (const tcu::Texture2D&	refTexture,
																 const VkImage&			vkTexture)
{
	const VkDevice				vkDevice	= m_context.getDevice();
	const DeviceInterface&		vk			= m_context.getDeviceInterface();

	de::MovePtr<Allocation>		allocation	= m_memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, vkTexture), MemoryRequirement::HostVisible);
	VK_CHECK(vk.bindImageMemory(vkDevice, vkTexture, allocation->getMemory(), allocation->getOffset()));

	const VkImageSubresource	subres				=
	{
		VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
		0u,							// deUint32				mipLevel;
		0u							// deUint32				arraySlice
	};

	VkSubresourceLayout layout;
	vk.getImageSubresourceLayout(vkDevice, vkTexture, &subres, &layout);

	tcu::ConstPixelBufferAccess	access		= refTexture.getLevel(0);
	tcu::PixelBufferAccess		destAccess	(refTexture.getFormat(), refTexture.getWidth(), refTexture.getHeight(), 1, allocation->getHostPtr());

	tcu::copy(destAccess, access);

	flushMappedMemoryRange(vk, vkDevice, allocation->getMemory(), allocation->getOffset(), layout.size);

	return allocation;
}

void ShaderRenderCaseInstance::copyTilingImageToOptimal	(const vk::VkImage&	srcImage,
														 const vk::VkImage&	dstImage,
														 deUint32			width,
														 deUint32			height)
{
	const VkDevice						vkDevice			= m_context.getDevice();
	const DeviceInterface&				vk					= m_context.getDeviceInterface();
	const VkQueue						queue				= m_context.getUniversalQueue();
	const deUint32						queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();

	// Create command pool
	const VkCommandPoolCreateInfo		cmdPoolParams		=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,	// VkStructureType		sType;
		DE_NULL,									// const void*			pNext;
		VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,		// VkCmdPoolCreateFlags	flags;
		queueFamilyIndex,							// deUint32				queueFamilyIndex;
	};

	Move<VkCommandPool>					cmdPool				= createCommandPool(vk, vkDevice, &cmdPoolParams);

	// Create command buffer
	const VkCommandBufferAllocateInfo	cmdBufferParams		=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,	// VkStructureType			sType;
		DE_NULL,										// const void*				pNext;
		*cmdPool,										// VkCommandPool			commandPool;
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,				// VkCommandBufferLevel		level;
		1u												// deUint32					bufferCount;
	};

	const VkCommandBufferUsageFlags		usageFlags			= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	const VkCommandBufferBeginInfo		cmdBufferBeginInfo	=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	// VkStructureType				sType;
		DE_NULL,										// const void*					pNext;
		usageFlags,										// VkCommandBufferUsageFlags	flags;
		(const VkCommandBufferInheritanceInfo*)DE_NULL,
	};

	Move<VkCommandBuffer>				cmdBuffer			= allocateCommandBuffer(vk, vkDevice, &cmdBufferParams);

	VK_CHECK(vk.beginCommandBuffer(*cmdBuffer, &cmdBufferBeginInfo));

	// Add image barriers
	const VkImageMemoryBarrier			layoutBarriers[2]	=
	{
		createImageMemoryBarrier(srcImage, (VkAccessFlags)0u, (VkAccessFlags)0u, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL),
		createImageMemoryBarrier(dstImage, (VkAccessFlags)0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	};

	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, (VkDependencyFlags)0,
						  0, (const VkMemoryBarrier*)DE_NULL,
						  0, (const VkBufferMemoryBarrier*)DE_NULL,
						  DE_LENGTH_OF_ARRAY(layoutBarriers), layoutBarriers);

	// Add image copy
	const VkImageCopy				imageCopy			=
	{
		{
			VK_IMAGE_ASPECT_COLOR_BIT,		// VkImageAspect	aspect;
			0u,								// deUint32			mipLevel;
			0u,								// deUint32			arrayLayer;
			1u								// deUint32			arraySize;
		},											// VkImageSubresourceCopy	srcSubresource;
		{
			0,								// int32			x;
			0,								// int32			y;
			0								// int32			z;
		},											// VkOffset3D				srcOffset;
		{
			VK_IMAGE_ASPECT_COLOR_BIT,		// VkImageAspect	aspect;
			0u,								// deUint32			mipLevel;
			0u,								// deUint32			arrayLayer;
			1u								// deUint32			arraySize;
		},											// VkImageSubresourceCopy	destSubResource;
		{
			0,								// int32			x;
			0,								// int32			y;
			0								// int32			z;
		},											// VkOffset3D				dstOffset;
		{
			width,							// int32			width;
			height,							// int32			height;
			1,								// int32			depth
		}	// VkExtent3D					extent;
	};

	vk.cmdCopyImage(*cmdBuffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageCopy);

	// Add destination barrier
	const VkImageMemoryBarrier		dstBarrier			=
			createImageMemoryBarrier(dstImage, VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, 0u, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, (VkDependencyFlags)0,
						  0, (const VkMemoryBarrier*)DE_NULL,
						  0, (const VkBufferMemoryBarrier*)DE_NULL,
						  1, &dstBarrier);

	VK_CHECK(vk.endCommandBuffer(*cmdBuffer));

	const VkFenceCreateInfo			fenceParams			=
	{
		VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,	// VkStructureType		sType;
		DE_NULL,								// const void*			pNext;
		0u										// VkFenceCreateFlags	flags;
	};
	const Unique<VkFence>			fence				(createFence(vk, vkDevice, &fenceParams));
	const VkSubmitInfo				submitInfo			=
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,
		DE_NULL,
		0u,
		(const VkSemaphore*)DE_NULL,
		(const VkPipelineStageFlags*)DE_NULL,
		1u,
		&cmdBuffer.get(),
		0u,
		(const VkSemaphore*)DE_NULL,
	};


	// Execute copy
	VK_CHECK(vk.resetFences(vkDevice, 1, &fence.get()));
	VK_CHECK(vk.queueSubmit(queue, 1, &submitInfo, *fence));
	VK_CHECK(vk.waitForFences(vkDevice, 1, &fence.get(), true, ~(0ull) /* infinity*/));
}

void ShaderRenderCaseInstance::useSampler2D (deUint32 bindingLocation, deUint32 textureID)
{
	DE_ASSERT(textureID < m_textures.size());

	const VkDevice					vkDevice		= m_context.getDevice();
	const DeviceInterface&			vk				= m_context.getDeviceInterface();
	const TextureBinding&			textureBinding	= *m_textures[textureID];
	const tcu::Texture2D&			refTexture		= textureBinding.get2D();
	const tcu::Sampler&				refSampler		= textureBinding.getSampler();
	const VkFormat					format			= refTexture.getFormat() == tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8)
														? VK_FORMAT_R8G8B8A8_UNORM
														: VK_FORMAT_R8G8B8_UNORM;

	// Create & alloc the image
	Move<VkImage>					vkTexture;
	de::MovePtr<Allocation>			allocation;

	if (isSupportedLinearTilingFormat(m_context.getInstanceInterface(), m_context.getPhysicalDevice(), format))
	{
		vkTexture = createImage2D(refTexture, format, VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_TILING_LINEAR);
		allocation = uploadImage2D(refTexture, *vkTexture);
	}
	else if (isSupportedOptimalTilingFormat(m_context.getInstanceInterface(), m_context.getPhysicalDevice(), format))
	{
		Move<VkImage>				stagingTexture	(createImage2D(refTexture, format, VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_TILING_LINEAR));
		de::MovePtr<Allocation>		stagingAlloc	(uploadImage2D(refTexture, *stagingTexture));

		const VkImageUsageFlags		dstUsageFlags	= VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		vkTexture = createImage2D(refTexture, format, dstUsageFlags, VK_IMAGE_TILING_OPTIMAL);
		allocation = m_memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *vkTexture), MemoryRequirement::Any);
		VK_CHECK(vk.bindImageMemory(vkDevice, *vkTexture, allocation->getMemory(), allocation->getOffset()));

		copyTilingImageToOptimal(*stagingTexture, *vkTexture, refTexture.getWidth(), refTexture.getHeight());
	}
	else
	{
		TCU_THROW(InternalError, "Unable to create 2D image");
	}

	// Create sampler
	const VkSamplerCreateInfo		samplerParams	= mapSampler(refSampler, refTexture.getFormat());
	Move<VkSampler>					sampler			= createSampler(vk, vkDevice, &samplerParams);

	const VkImageViewCreateInfo		viewParams		=
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// VkStructureType			sType;
		NULL,										// const voide*				pNext;
		0u,											// VkImageViewCreateFlags	flags;
		*vkTexture,									// VkImage					image;
		VK_IMAGE_VIEW_TYPE_2D,						// VkImageViewType			viewType;
		format,										// VkFormat					format;
		{
			VK_COMPONENT_SWIZZLE_R,			// VkChannelSwizzle		r;
			VK_COMPONENT_SWIZZLE_G,			// VkChannelSwizzle		g;
			VK_COMPONENT_SWIZZLE_B,			// VkChannelSwizzle		b;
			VK_COMPONENT_SWIZZLE_A			// VkChannelSwizzle		a;
		},											// VkChannelMapping			channels;
		{
			VK_IMAGE_ASPECT_COLOR_BIT,		// VkImageAspectFlags	aspectMask;
			0,								// deUint32				baseMipLevel;
			1,								// deUint32				mipLevels;
			0,								// deUint32				baseArraySlice;
			1								// deUint32				arraySize;
		},											// VkImageSubresourceRange	subresourceRange;
	};

	Move<VkImageView>				imageView		= createImageView(vk, vkDevice, &viewParams);

	const vk::VkDescriptorImageInfo	descriptor		=
	{
		sampler.get(),								// VkSampler				sampler;
		imageView.get(),							// VkImageView				imageView;
		VK_IMAGE_LAYOUT_GENERAL,					// VkImageLayout			imageLayout;
	};

	de::MovePtr<SamplerUniform> uniform(new SamplerUniform());
	uniform->type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	uniform->descriptor = descriptor;
	uniform->location = bindingLocation;
	uniform->image = VkImageSp(new vk::Unique<VkImage>(vkTexture));
	uniform->imageView = VkImageViewSp(new vk::Unique<VkImageView>(imageView));
	uniform->sampler = VkSamplerSp(new vk::Unique<VkSampler>(sampler));
	uniform->alloc = AllocationSp(allocation.release());

	m_descriptorSetLayoutBuilder.addSingleSamplerBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, vk::VK_SHADER_STAGE_ALL, &uniform->descriptor.sampler);
	m_descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

	m_uniformInfos.push_back(UniformInfoSp(new de::UniquePtr<UniformInfo>(uniform)));
}

void ShaderRenderCaseInstance::setupDefaultInputs (const QuadGrid& quadGrid)
{
	/* Configuration of the vertex input attributes:
		a_position   is at location 0
		a_coords     is at location 1
		a_unitCoords is at location 2
		a_one        is at location 3

	  User attributes starts from at the location 4.
	*/
	addAttribute(0u, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(tcu::Vec4), quadGrid.getNumVertices(), quadGrid.getPositions());
	addAttribute(1u, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(tcu::Vec4), quadGrid.getNumVertices(), quadGrid.getCoords());
	addAttribute(2u, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(tcu::Vec4), quadGrid.getNumVertices(), quadGrid.getUnitCoords());
	addAttribute(3u, VK_FORMAT_R32_SFLOAT, sizeof(float), quadGrid.getNumVertices(), quadGrid.getAttribOne());

	static const struct
	{
		BaseAttributeType	type;
		int					userNdx;
	} userAttributes[] =
	{
		{ A_IN0, 0 },
		{ A_IN1, 1 },
		{ A_IN2, 2 },
		{ A_IN3, 3 }
	};

	static const struct
	{
		BaseAttributeType	matrixType;
		int					numCols;
		int					numRows;
	} matrices[] =
	{
		{ MAT2,		2, 2 },
		{ MAT2x3,	2, 3 },
		{ MAT2x4,	2, 4 },
		{ MAT3x2,	3, 2 },
		{ MAT3,		3, 3 },
		{ MAT3x4,	3, 4 },
		{ MAT4x2,	4, 2 },
		{ MAT4x3,	4, 3 },
		{ MAT4,		4, 4 }
	};

	for (size_t attrNdx = 0; attrNdx < m_enabledBaseAttributes.size(); attrNdx++)
	{
		for (int userNdx = 0; userNdx < DE_LENGTH_OF_ARRAY(userAttributes); userNdx++)
		{
			if (userAttributes[userNdx].type != m_enabledBaseAttributes[attrNdx].type)
				continue;

			addAttribute(m_enabledBaseAttributes[attrNdx].location, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(tcu::Vec4), quadGrid.getNumVertices(), quadGrid.getUserAttrib(userNdx));
		}

		for (int matNdx = 0; matNdx < DE_LENGTH_OF_ARRAY(matrices); matNdx++)
		{

			if (matrices[matNdx].matrixType != m_enabledBaseAttributes[attrNdx].type)
				continue;

			const int numCols = matrices[matNdx].numCols;

			for (int colNdx = 0; colNdx < numCols; colNdx++)
			{
				addAttribute(m_enabledBaseAttributes[attrNdx].location + colNdx, VK_FORMAT_R32G32B32A32_SFLOAT, (deUint32)(4 * sizeof(float)), quadGrid.getNumVertices(), quadGrid.getUserAttrib(colNdx));
			}
		}
	}
}

void ShaderRenderCaseInstance::render (tcu::Surface& result, const QuadGrid& quadGrid)
{
	const VkDevice										vkDevice					= m_context.getDevice();
	const DeviceInterface&								vk							= m_context.getDeviceInterface();
	const VkQueue										queue						= m_context.getUniversalQueue();
	const deUint32										queueFamilyIndex			= m_context.getUniversalQueueFamilyIndex();

	// Create color image
	{
		const VkImageCreateInfo							colorImageParams			=
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,										// VkStructureType		sType;
			DE_NULL,																	// const void*			pNext;
			0u,																			// VkImageCreateFlags	flags;
			VK_IMAGE_TYPE_2D,															// VkImageType			imageType;
			m_colorFormat,																// VkFormat				format;
			{ m_renderSize.x(), m_renderSize.y(), 1u },									// VkExtent3D			extent;
			1u,																			// deUint32				mipLevels;
			1u,																			// deUint32				arraySize;
			VK_SAMPLE_COUNT_1_BIT,														// deUint32				samples;
			VK_IMAGE_TILING_OPTIMAL,													// VkImageTiling		tiling;
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,		// VkImageUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,													// VkSharingMode		sharingMode;
			1u,																			// deUint32				queueFamilyCount;
			&queueFamilyIndex,															// const deUint32*		pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED,													// VkImageLayout		initialLayout;
		};

		m_colorImage = createImage(vk, vkDevice, &colorImageParams);

		// Allocate and bind color image memory
		m_colorImageAlloc = m_memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_colorImage), MemoryRequirement::Any);
		VK_CHECK(vk.bindImageMemory(vkDevice, *m_colorImage, m_colorImageAlloc->getMemory(), m_colorImageAlloc->getOffset()));
	}

	// Create color attachment view
	{
		const VkImageViewCreateInfo						colorImageViewParams		=
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,			// VkStructureType			sType;
			DE_NULL,											// const void*				pNext;
			0u,													// VkImageViewCreateFlags	flags;
			*m_colorImage,										// VkImage					image;
			VK_IMAGE_VIEW_TYPE_2D,								// VkImageViewType			viewType;
			m_colorFormat,										// VkFormat					format;
			{
				VK_COMPONENT_SWIZZLE_R,			// VkChannelSwizzle		r;
				VK_COMPONENT_SWIZZLE_G,			// VkChannelSwizzle		g;
				VK_COMPONENT_SWIZZLE_B,			// VkChannelSwizzle		b;
				VK_COMPONENT_SWIZZLE_A			// VkChannelSwizzle		a;
			},													// VkChannelMapping			channels;
			{
				VK_IMAGE_ASPECT_COLOR_BIT,		// VkImageAspectFlags	aspectMask;
				0,								// deUint32				baseMipLevel;
				1,								// deUint32				mipLevels;
				0,								// deUint32				baseArraySlice;
				1								// deUint32				arraySize;
			},													// VkImageSubresourceRange	subresourceRange;
		};

		m_colorImageView = createImageView(vk, vkDevice, &colorImageViewParams);
	}

	// Create render pass
	{
		const VkAttachmentDescription					attachmentDescription		=
		{
			(VkAttachmentDescriptionFlags)0,
			m_colorFormat,										// VkFormat						format;
			VK_SAMPLE_COUNT_1_BIT,								// deUint32						samples;
			VK_ATTACHMENT_LOAD_OP_CLEAR,						// VkAttachmentLoadOp			loadOp;
			VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp			storeOp;
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,					// VkAttachmentLoadOp			stencilLoadOp;
			VK_ATTACHMENT_STORE_OP_DONT_CARE,					// VkAttachmentStoreOp			stencilStoreOp;
			VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout				initialLayout;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,			// VkImageLayout				finalLayout;
		};

		const VkAttachmentReference						attachmentReference			=
		{
			0u,													// deUint32			attachment;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL			// VkImageLayout	layout;
		};

		const VkSubpassDescription						subpassDescription			=
		{
			0u,													// VkSubpassDescriptionFlags	flags;
			VK_PIPELINE_BIND_POINT_GRAPHICS,					// VkPipelineBindPoint			pipelineBindPoint;
			0u,													// deUint32						inputCount;
			DE_NULL,											// constVkAttachmentReference*	pInputAttachments;
			1u,													// deUint32						colorCount;
			&attachmentReference,								// constVkAttachmentReference*	pColorAttachments;
			DE_NULL,											// constVkAttachmentReference*	pResolveAttachments;
			DE_NULL,											// VkAttachmentReference		depthStencilAttachment;
			0u,													// deUint32						preserveCount;
			DE_NULL												// constVkAttachmentReference*	pPreserveAttachments;
		};

		const VkRenderPassCreateInfo					renderPassParams			=
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,			// VkStructureType					sType;
			DE_NULL,											// const void*						pNext;
			(VkRenderPassCreateFlags)0,
			1u,													// deUint32							attachmentCount;
			&attachmentDescription,								// const VkAttachmentDescription*	pAttachments;
			1u,													// deUint32							subpassCount;
			&subpassDescription,								// const VkSubpassDescription*		pSubpasses;
			0u,													// deUint32							dependencyCount;
			DE_NULL												// const VkSubpassDependency*		pDependencies;
		};

		m_renderPass = createRenderPass(vk, vkDevice, &renderPassParams);
	}

	// Create framebuffer
	{
		const VkFramebufferCreateInfo					framebufferParams			=
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,			// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			(VkFramebufferCreateFlags)0,
			*m_renderPass,										// VkRenderPass					renderPass;
			1u,													// deUint32						attachmentCount;
			&*m_colorImageView,									// const VkImageView*			pAttachments;
			(deUint32)m_renderSize.x(),							// deUint32						width;
			(deUint32)m_renderSize.y(),							// deUint32						height;
			1u													// deUint32						layers;
		};

		m_framebuffer = createFramebuffer(vk, vkDevice, &framebufferParams);
	}

	// Create descriptors
	{
		setupUniforms(quadGrid.getConstCoords());

		m_descriptorSetLayout = m_descriptorSetLayoutBuilder.build(vk, vkDevice);
		if (!m_uniformInfos.empty())
		{
			m_descriptorPool 								= m_descriptorPoolBuilder.build(vk, vkDevice, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
			const VkDescriptorSetAllocateInfo	allocInfo	=
			{
				VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				DE_NULL,
				*m_descriptorPool,
				1u,
				&m_descriptorSetLayout.get(),
			};

			m_descriptorSet = allocateDescriptorSet(vk, vkDevice, &allocInfo);
		}

		for (deUint32 i = 0; i < m_uniformInfos.size(); i++)
		{
			const UniformInfo* uniformInfo = m_uniformInfos[i].get()->get();
			deUint32 location = uniformInfo->location;

			if (uniformInfo->type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
			{
				const BufferUniform*	bufferInfo	= dynamic_cast<const BufferUniform*>(uniformInfo);

				m_descriptorSetUpdateBuilder.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(location), uniformInfo->type, &bufferInfo->descriptor);
			}
			else if (uniformInfo->type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
			{
				const SamplerUniform*	samplerInfo	= dynamic_cast<const SamplerUniform*>(uniformInfo);

				m_descriptorSetUpdateBuilder.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(location), uniformInfo->type, &samplerInfo->descriptor);
			}
			else
				DE_FATAL("Impossible");
		}

		m_descriptorSetUpdateBuilder.update(vk, vkDevice);
	}

	// Create pipeline layout
	{
		const VkPipelineLayoutCreateInfo				pipelineLayoutParams		=
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			(VkPipelineLayoutCreateFlags)0,
			1u,													// deUint32						descriptorSetCount;
			&*m_descriptorSetLayout,							// const VkDescriptorSetLayout*	pSetLayouts;
			0u,													// deUint32						pushConstantRangeCount;
			DE_NULL												// const VkPushConstantRange*	pPushConstantRanges;
		};

		m_pipelineLayout = createPipelineLayout(vk, vkDevice, &pipelineLayoutParams);
	}

	// Create shaders
	{
		m_vertexShaderModule	= createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("vert"), 0);
		m_fragmentShaderModule	= createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("frag"), 0);
	}

	// Create pipeline
	{
		const VkPipelineShaderStageCreateInfo			shaderStageParams[2]		=
		{
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,		// VkStructureType				sType;
				DE_NULL,													// const void*					pNext;
				(VkPipelineShaderStageCreateFlags)0,
				VK_SHADER_STAGE_VERTEX_BIT,									// VkShaderStage				stage;
				*m_vertexShaderModule,										// VkShader						shader;
				"main",
				DE_NULL														// const VkSpecializationInfo*	pSpecializationInfo;
			},
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,		// VkStructureType				sType;
				DE_NULL,													// const void*					pNext;
				(VkPipelineShaderStageCreateFlags)0,
				VK_SHADER_STAGE_FRAGMENT_BIT,								// VkShaderStage				stage;
				*m_fragmentShaderModule,									// VkShader						shader;
				"main",
				DE_NULL														// const VkSpecializationInfo*	pSpecializationInfo;
			}
		};

		// Add test case specific attributes
		if (m_attribFunc)
			m_attribFunc(*this, quadGrid.getNumVertices());

		// Add base attributes
		setupDefaultInputs(quadGrid);

		const VkPipelineVertexInputStateCreateInfo		vertexInputStateParams		=
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			(VkPipelineVertexInputStateCreateFlags)0,
			(deUint32)m_vertexBindingDescription.size(),					// deUint32									bindingCount;
			&m_vertexBindingDescription[0],									// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
			(deUint32)m_vertexattributeDescription.size(),					// deUint32									attributeCount;
			&m_vertexattributeDescription[0],								// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
		};

		const VkPipelineInputAssemblyStateCreateInfo	inputAssemblyStateParams	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,														// const void*			pNext;
			(VkPipelineInputAssemblyStateCreateFlags)0,
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,							// VkPrimitiveTopology	topology;
			false															// VkBool32				primitiveRestartEnable;
		};

		const VkViewport								viewport					=
		{
			0.0f,						// float	originX;
			0.0f,						// float	originY;
			(float)m_renderSize.x(),	// float	width;
			(float)m_renderSize.y(),	// float	height;
			0.0f,						// float	minDepth;
			1.0f						// float	maxDepth;
		};

		const VkRect2D									scissor						=
		{
			{
				0u,					// deUint32	x;
				0u,					// deUint32	y;
			},							// VkOffset2D	offset;
			{
				m_renderSize.x(),	// deUint32	width;
				m_renderSize.y(),	// deUint32	height;
			},							// VkExtent2D	extent;
		};

		const VkPipelineViewportStateCreateInfo			viewportStateParams			=
		{
			VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,			// VkStructureType		sType;
			DE_NULL,														// const void*			pNext;
			(VkPipelineViewportStateCreateFlags)0,
			1u,																// deUint32				viewportCount;
			&viewport,														// const VkViewport*	pViewports;
			1u,																// deUint32				scissorsCount;
			&scissor,														// const VkRect2D*		pScissors;
		};

		const VkPipelineRasterizationStateCreateInfo	rasterStateParams			=
		{
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,		// VkStructureType	sType;
			DE_NULL,														// const void*		pNext;
			(VkPipelineRasterizationStateCreateFlags)0,
			false,															// VkBool32			depthClipEnable;
			false,															// VkBool32			rasterizerDiscardEnable;
			VK_POLYGON_MODE_FILL,											// VkFillMode		fillMode;
			VK_CULL_MODE_NONE,												// VkCullMode		cullMode;
			VK_FRONT_FACE_COUNTER_CLOCKWISE,								// VkFrontFace		frontFace;
			false,															// VkBool32			depthBiasEnable;
			0.0f,															// float			depthBias;
			0.0f,															// float			depthBiasClamp;
			0.0f,															// float			slopeScaledDepthBias;
			1.0f,															// float			lineWidth;
		};

		const VkPipelineMultisampleStateCreateInfo		multisampleStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,		// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			0u,																// VkPipelineMultisampleStateCreateFlags	flags;
			VK_SAMPLE_COUNT_1_BIT,											// VkSampleCountFlagBits					rasterizationSamples;
			VK_FALSE,														// VkBool32									sampleShadingEnable;
			0.0f,															// float									minSampleShading;
			DE_NULL,														// const VkSampleMask*						pSampleMask;
			VK_FALSE,														// VkBool32									alphaToCoverageEnable;
			VK_FALSE														// VkBool32									alphaToOneEnable;
		};

		const VkPipelineColorBlendAttachmentState		colorBlendAttachmentState	=
		{
			false,															// VkBool32			blendEnable;
			VK_BLEND_FACTOR_ONE,											// VkBlend			srcBlendColor;
			VK_BLEND_FACTOR_ZERO,											// VkBlend			destBlendColor;
			VK_BLEND_OP_ADD,												// VkBlendOp		blendOpColor;
			VK_BLEND_FACTOR_ONE,											// VkBlend			srcBlendAlpha;
			VK_BLEND_FACTOR_ZERO,											// VkBlend			destBlendAlpha;
			VK_BLEND_OP_ADD,												// VkBlendOp		blendOpAlpha;
			(VK_COLOR_COMPONENT_R_BIT |
			 VK_COLOR_COMPONENT_G_BIT |
			 VK_COLOR_COMPONENT_B_BIT |
			 VK_COLOR_COMPONENT_A_BIT),										// VkChannelFlags	channelWriteMask;
		};

		const VkPipelineColorBlendStateCreateInfo		colorBlendStateParams		=
		{
			VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType								sType;
			DE_NULL,													// const void*									pNext;
			(VkPipelineColorBlendStateCreateFlags)0,
			false,														// VkBool32										logicOpEnable;
			VK_LOGIC_OP_COPY,											// VkLogicOp									logicOp;
			1u,															// deUint32										attachmentCount;
			&colorBlendAttachmentState,									// const VkPipelineColorBlendAttachmentState*	pAttachments;
			{ 0.0f, 0.0f, 0.0f, 0.0f },									// float										blendConst[4];
		};

		const VkGraphicsPipelineCreateInfo				graphicsPipelineParams		=
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
			&rasterStateParams,									// const VkPipelineRasterStateCreateInfo*			pRasterState;
			&multisampleStateParams,							// const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
			DE_NULL,											// const VkPipelineDepthStencilStateCreateInfo*		pDepthStencilState;
			&colorBlendStateParams,								// const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
			(const VkPipelineDynamicStateCreateInfo*)DE_NULL,	// const VkPipelineDynamicStateCreateInfo*			pDynamicState;
			*m_pipelineLayout,									// VkPipelineLayout									layout;
			*m_renderPass,										// VkRenderPass										renderPass;
			0u,													// deUint32											subpass;
			0u,													// VkPipeline										basePipelineHandle;
			0u													// deInt32											basePipelineIndex;
		};

		m_graphicsPipeline = createGraphicsPipeline(vk, vkDevice, DE_NULL, &graphicsPipelineParams);
	}

	// Create vertex indices buffer
	{
		const VkDeviceSize								indiceBufferSize			= quadGrid.getNumTriangles() * 3 * sizeof(deUint16);
		const VkBufferCreateInfo						indiceBufferParams			=
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			0u,											// VkBufferCreateFlags	flags;
			indiceBufferSize,							// VkDeviceSize			size;
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT,			// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
			1u,											// deUint32				queueFamilyCount;
			&queueFamilyIndex							// const deUint32*		pQueueFamilyIndices;
		};

		m_indiceBuffer		= createBuffer(vk, vkDevice, &indiceBufferParams);
		m_indiceBufferAlloc	= m_memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_indiceBuffer), MemoryRequirement::HostVisible);

		VK_CHECK(vk.bindBufferMemory(vkDevice, *m_indiceBuffer, m_indiceBufferAlloc->getMemory(), m_indiceBufferAlloc->getOffset()));

		// Load vertice indices into buffer
		deMemcpy(m_indiceBufferAlloc->getHostPtr(), quadGrid.getIndices(), (size_t)indiceBufferSize);
		flushMappedMemoryRange(vk, vkDevice, m_indiceBufferAlloc->getMemory(), m_indiceBufferAlloc->getOffset(), indiceBufferSize);
	}

	// Create command pool
	{
		const VkCommandPoolCreateInfo					cmdPoolParams				=
		{
			VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,										// const void*			pNext;
			VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,			// VkCmdPoolCreateFlags	flags;
			queueFamilyIndex,								// deUint32				queueFamilyIndex;
		};

		m_cmdPool = createCommandPool(vk, vkDevice, &cmdPoolParams);
	}

	// Create command buffer
	{
		const VkCommandBufferAllocateInfo				cmdBufferParams				=
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,	// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			*m_cmdPool,										// VkCmdPool				cmdPool;
			VK_COMMAND_BUFFER_LEVEL_PRIMARY,				// VkCmdBufferLevel			level;
			1u												// deUint32					bufferCount;
		};

		const VkCommandBufferBeginInfo					cmdBufferBeginInfo			=
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			0u,												// VkCmdBufferOptimizeFlags	flags;
			(const VkCommandBufferInheritanceInfo*)DE_NULL,
		};

		const VkClearValue								clearValues					= makeClearValueColorF32(m_clearColor.x(),
																											 m_clearColor.y(),
																											 m_clearColor.z(),
																											 m_clearColor.w());

		const VkRenderPassBeginInfo						renderPassBeginInfo			=
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,				// VkStructureType		sType;
			DE_NULL,												// const void*			pNext;
			*m_renderPass,											// VkRenderPass			renderPass;
			*m_framebuffer,											// VkFramebuffer		framebuffer;
			{ { 0, 0 },  {m_renderSize.x(), m_renderSize.y() } },	// VkRect2D				renderArea;
			1,														// deUint32				clearValueCount;
			&clearValues,											// const VkClearValue*	pClearValues;
		};

		m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, &cmdBufferParams);

		VK_CHECK(vk.beginCommandBuffer(*m_cmdBuffer, &cmdBufferBeginInfo));

		// Add texture barriers
		std::vector<VkImageMemoryBarrier> barriers;

		for(deUint32 i = 0; i < m_uniformInfos.size(); i++)
		{
			const UniformInfo* uniformInfo = m_uniformInfos[i].get()->get();

			if (uniformInfo->type != VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
			{
				continue;
			}

			const SamplerUniform*		sampler			= static_cast<const SamplerUniform*>(uniformInfo);
			const VkImageMemoryBarrier	textureBarrier	= createImageMemoryBarrier(sampler->image->get(), 0u, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

			barriers.push_back(textureBarrier);
		}

		vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, (VkDependencyFlags)0,
							  0, (const VkMemoryBarrier*)DE_NULL,
							  0, (const VkBufferMemoryBarrier*)DE_NULL,
							  (deUint32)barriers.size(), (barriers.empty() ? (const VkImageMemoryBarrier*)DE_NULL : &barriers[0]));

		vk.cmdBeginRenderPass(*m_cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipeline);
		if (!m_uniformInfos.empty())
			vk.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayout, 0u, 1, &*m_descriptorSet, 0u, DE_NULL);
		vk.cmdBindIndexBuffer(*m_cmdBuffer, *m_indiceBuffer, 0, VK_INDEX_TYPE_UINT16);

		const deUint32 numberOfVertexAttributes = (deUint32)m_vertexBuffers.size();
		const std::vector<VkDeviceSize> offsets(numberOfVertexAttributes, 0);

		std::vector<VkBuffer> buffers(numberOfVertexAttributes);
		for (size_t i = 0; i < numberOfVertexAttributes; i++)
		{
			buffers[i] = m_vertexBuffers[i].get()->get();
		}

		vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, numberOfVertexAttributes, &buffers[0], &offsets[0]);
		vk.cmdDrawIndexed(*m_cmdBuffer, quadGrid.getNumTriangles() * 3, 1, 0, 0, 0);

		vk.cmdEndRenderPass(*m_cmdBuffer);
		VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));
	}

	// Create fence
	{
		const VkFenceCreateInfo							fenceParams					=
		{
			VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,								// const void*			pNext;
			0u										// VkFenceCreateFlags	flags;
		};
		m_fence = createFence(vk, vkDevice, &fenceParams);
	}

	// Execute Draw
	{
		const VkSubmitInfo	submitInfo	=
		{
			VK_STRUCTURE_TYPE_SUBMIT_INFO,
			DE_NULL,
			0u,
			(const VkSemaphore*)DE_NULL,
			(const VkPipelineStageFlags*)DE_NULL,
			1u,
			&m_cmdBuffer.get(),
			0u,
			(const VkSemaphore*)DE_NULL,
		};

		VK_CHECK(vk.resetFences(vkDevice, 1, &m_fence.get()));
		VK_CHECK(vk.queueSubmit(queue, 1, &submitInfo, *m_fence));
		VK_CHECK(vk.waitForFences(vkDevice, 1, &m_fence.get(), true, ~(0ull) /* infinity*/));
	}

	// Read back the result
	{
		const VkDeviceSize								imageSizeBytes				= (VkDeviceSize)(sizeof(deUint32) * m_renderSize.x() * m_renderSize.y());
		const VkBufferCreateInfo						readImageBufferParams		=
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		//  VkStructureType		sType;
			DE_NULL,									//  const void*			pNext;
			0u,											//  VkBufferCreateFlags	flags;
			imageSizeBytes,								//  VkDeviceSize		size;
			VK_BUFFER_USAGE_TRANSFER_DST_BIT,			//  VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,					//  VkSharingMode		sharingMode;
			1u,											//  deUint32			queueFamilyCount;
			&queueFamilyIndex,							//  const deUint32*		pQueueFamilyIndices;
		};
		const Unique<VkBuffer>							readImageBuffer				(createBuffer(vk, vkDevice, &readImageBufferParams));
		const de::UniquePtr<Allocation>					readImageBufferMemory		(m_memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *readImageBuffer), MemoryRequirement::HostVisible));

		VK_CHECK(vk.bindBufferMemory(vkDevice, *readImageBuffer, readImageBufferMemory->getMemory(), readImageBufferMemory->getOffset()));

		// Copy image to buffer
		const VkCommandBufferAllocateInfo				cmdBufferParams				=
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,	// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			*m_cmdPool,										// VkCmdPool				cmdPool;
			VK_COMMAND_BUFFER_LEVEL_PRIMARY,				// VkCmdBufferLevel			level;
			1u												// deUint32					bufferCount;
		};

		const VkCommandBufferBeginInfo					cmdBufferBeginInfo			=
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			0u,												// VkCmdBufferOptimizeFlags	flags;
			(const VkCommandBufferInheritanceInfo*)DE_NULL,
		};

		const Move<VkCommandBuffer>						cmdBuffer					= allocateCommandBuffer(vk, vkDevice, &cmdBufferParams);

		const VkBufferImageCopy							copyParams					=
		{
			0u,											// VkDeviceSize			bufferOffset;
			(deUint32)m_renderSize.x(),					// deUint32				bufferRowLength;
			(deUint32)m_renderSize.y(),					// deUint32				bufferImageHeight;
			{
				VK_IMAGE_ASPECT_COLOR_BIT,			// VkImageAspect		aspect;
				0u,									// deUint32				mipLevel;
				0u,									// deUint32				arraySlice;
				1u,									// deUint32				arraySize;
			},											// VkImageSubresourceCopy	imageSubresource;
			{ 0u, 0u, 0u },								// VkOffset3D			imageOffset;
			{ m_renderSize.x(), m_renderSize.y(), 1u }	// VkExtent3D			imageExtent;
		};
		const VkSubmitInfo								submitInfo					=
		{
			VK_STRUCTURE_TYPE_SUBMIT_INFO,
			DE_NULL,
			0u,
			(const VkSemaphore*)DE_NULL,
			(const VkPipelineStageFlags*)DE_NULL,
			1u,
			&cmdBuffer.get(),
			0u,
			(const VkSemaphore*)DE_NULL,
		};

		VK_CHECK(vk.beginCommandBuffer(*cmdBuffer, &cmdBufferBeginInfo));

		const VkImageMemoryBarrier imageBarrier =
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
			DE_NULL,									// const void*				pNext;
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,		// VkAccessFlags			srcAccessMask;
			VK_ACCESS_TRANSFER_READ_BIT,				// VkAccessFlags			dstAccessMask;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout			oldLayout;
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,		// VkImageLayout			newLayout;
			VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
			*m_colorImage,								// VkImage					image;
			{											// VkImageSubresourceRange	subresourceRange;
				VK_IMAGE_ASPECT_COLOR_BIT,				// VkImageAspectFlags	aspectMask;
				0u,							// deUint32				baseMipLevel;
				1u,							// deUint32				mipLevels;
				0u,							// deUint32				baseArraySlice;
				1u							// deUint32				arraySize;
			}
		};

		const VkBufferMemoryBarrier bufferBarrier =
		{
			VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType	sType;
			DE_NULL,									// const void*		pNext;
			VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags	srcAccessMask;
			VK_ACCESS_HOST_READ_BIT,					// VkAccessFlags	dstAccessMask;
			VK_QUEUE_FAMILY_IGNORED,					// deUint32			srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,					// deUint32			dstQueueFamilyIndex;
			*readImageBuffer,							// VkBuffer			buffer;
			0u,											// VkDeviceSize		offset;
			imageSizeBytes								// VkDeviceSize		size;
		};

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &imageBarrier);
		vk.cmdCopyImageToBuffer(*cmdBuffer, *m_colorImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *readImageBuffer, 1u, &copyParams);
		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &bufferBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);

		VK_CHECK(vk.endCommandBuffer(*cmdBuffer));

		VK_CHECK(vk.resetFences(vkDevice, 1, &m_fence.get()));
		VK_CHECK(vk.queueSubmit(queue, 1, &submitInfo, *m_fence));
		VK_CHECK(vk.waitForFences(vkDevice, 1, &m_fence.get(), true, ~(0ull) /* infinity */));

		invalidateMappedMemoryRange(vk, vkDevice, readImageBufferMemory->getMemory(), readImageBufferMemory->getOffset(), imageSizeBytes);

		const tcu::TextureFormat						resultFormat				(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8);
		const tcu::ConstPixelBufferAccess				resultAccess				(resultFormat, m_renderSize.x(), m_renderSize.y(), 1, readImageBufferMemory->getHostPtr());

		tcu::copy(result.getAccess(), resultAccess);
	}
}

void ShaderRenderCaseInstance::computeVertexReference (tcu::Surface& result, const QuadGrid& quadGrid)
{
	// Buffer info.
	const int				width		= result.getWidth();
	const int				height		= result.getHeight();
	const int				gridSize	= quadGrid.getGridSize();
	const int				stride		= gridSize + 1;
	const bool				hasAlpha	= true; // \todo [2015-09-07 elecro] add correct alpha check
	ShaderEvalContext		evalCtx		(quadGrid);

	// Evaluate color for each vertex.
	std::vector<tcu::Vec4>	colors		((gridSize + 1) * (gridSize + 1));
	for (int y = 0; y < gridSize+1; y++)
	for (int x = 0; x < gridSize+1; x++)
	{
		const float	sx			= (float)x / (float)gridSize;
		const float	sy			= (float)y / (float)gridSize;
		const int	vtxNdx		= ((y * (gridSize+1)) + x);

		evalCtx.reset(sx, sy);
		m_evaluator.evaluate(evalCtx);
		DE_ASSERT(!evalCtx.isDiscarded); // Discard is not available in vertex shader.
		tcu::Vec4 color = evalCtx.color;

		if (!hasAlpha)
			color.w() = 1.0f;

		colors[vtxNdx] = color;
	}

	// Render quads.
	for (int y = 0; y < gridSize; y++)
	for (int x = 0; x < gridSize; x++)
	{
		const float		x0		= (float)x       / (float)gridSize;
		const float		x1		= (float)(x + 1) / (float)gridSize;
		const float		y0		= (float)y       / (float)gridSize;
		const float		y1		= (float)(y + 1) / (float)gridSize;

		const float		sx0		= x0 * (float)width;
		const float		sx1		= x1 * (float)width;
		const float		sy0		= y0 * (float)height;
		const float		sy1		= y1 * (float)height;
		const float		oosx	= 1.0f / (sx1 - sx0);
		const float		oosy	= 1.0f / (sy1 - sy0);

		const int		ix0		= deCeilFloatToInt32(sx0 - 0.5f);
		const int		ix1		= deCeilFloatToInt32(sx1 - 0.5f);
		const int		iy0		= deCeilFloatToInt32(sy0 - 0.5f);
		const int		iy1		= deCeilFloatToInt32(sy1 - 0.5f);

		const int		v00		= (y * stride) + x;
		const int		v01		= (y * stride) + x + 1;
		const int		v10		= ((y + 1) * stride) + x;
		const int		v11		= ((y + 1) * stride) + x + 1;
		const tcu::Vec4	c00		= colors[v00];
		const tcu::Vec4	c01		= colors[v01];
		const tcu::Vec4	c10		= colors[v10];
		const tcu::Vec4	c11		= colors[v11];

		//printf("(%d,%d) -> (%f..%f, %f..%f) (%d..%d, %d..%d)\n", x, y, sx0, sx1, sy0, sy1, ix0, ix1, iy0, iy1);

		for (int iy = iy0; iy < iy1; iy++)
		for (int ix = ix0; ix < ix1; ix++)
		{
			DE_ASSERT(deInBounds32(ix, 0, width));
			DE_ASSERT(deInBounds32(iy, 0, height));

			const float			sfx		= (float)ix + 0.5f;
			const float			sfy		= (float)iy + 0.5f;
			const float			fx1		= deFloatClamp((sfx - sx0) * oosx, 0.0f, 1.0f);
			const float			fy1		= deFloatClamp((sfy - sy0) * oosy, 0.0f, 1.0f);

			// Triangle quad interpolation.
			const bool			tri		= fx1 + fy1 <= 1.0f;
			const float			tx		= tri ? fx1 : (1.0f-fx1);
			const float			ty		= tri ? fy1 : (1.0f-fy1);
			const tcu::Vec4&	t0		= tri ? c00 : c11;
			const tcu::Vec4&	t1		= tri ? c01 : c10;
			const tcu::Vec4&	t2		= tri ? c10 : c01;
			const tcu::Vec4		color	= t0 + (t1-t0)*tx + (t2-t0)*ty;

			result.setPixel(ix, iy, tcu::RGBA(color));
		}
	}
}

void ShaderRenderCaseInstance::computeFragmentReference (tcu::Surface& result, const QuadGrid& quadGrid)
{
	// Buffer info.
	const int			width		= result.getWidth();
	const int			height		= result.getHeight();
	const bool			hasAlpha	= true;  // \todo [2015-09-07 elecro] add correct alpha check
	ShaderEvalContext	evalCtx		(quadGrid);

	// Render.
	for (int y = 0; y < height; y++)
	for (int x = 0; x < width; x++)
	{
		const float sx = ((float)x + 0.5f) / (float)width;
		const float sy = ((float)y + 0.5f) / (float)height;

		evalCtx.reset(sx, sy);
		m_evaluator.evaluate(evalCtx);
		// Select either clear color or computed color based on discarded bit.
		tcu::Vec4 color = evalCtx.isDiscarded ? m_clearColor : evalCtx.color;

		if (!hasAlpha)
			color.w() = 1.0f;

		result.setPixel(x, y, tcu::RGBA(color));
	}
}

bool ShaderRenderCaseInstance::compareImages (const tcu::Surface& resImage, const tcu::Surface& refImage, float errorThreshold)
{
	return tcu::fuzzyCompare(m_context.getTestContext().getLog(), "ComparisonResult", "Image comparison result", refImage, resImage, errorThreshold, tcu::COMPARE_LOG_RESULT);
}

} // sr
} // vkt
