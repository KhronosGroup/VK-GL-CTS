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
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include <vector>
#include <string>

namespace vkt
{
namespace sr
{

using namespace vk;

VkImageViewType textureTypeToImageViewType (TextureBinding::Type type)
{
	switch (type)
	{
		case TextureBinding::TYPE_1D:			return VK_IMAGE_VIEW_TYPE_1D;
		case TextureBinding::TYPE_2D:			return VK_IMAGE_VIEW_TYPE_2D;
		case TextureBinding::TYPE_3D:			return VK_IMAGE_VIEW_TYPE_3D;
		case TextureBinding::TYPE_CUBE_MAP:		return VK_IMAGE_VIEW_TYPE_CUBE;
		case TextureBinding::TYPE_1D_ARRAY:		return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
		case TextureBinding::TYPE_2D_ARRAY:		return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		case TextureBinding::TYPE_CUBE_ARRAY:	return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;

		default:
			DE_FATAL("Impossible");
			return (VkImageViewType)0;
	}
}

VkImageType viewTypeToImageType (VkImageViewType type)
{
	switch (type)
	{
		case VK_IMAGE_VIEW_TYPE_1D:
		case VK_IMAGE_VIEW_TYPE_1D_ARRAY:		return VK_IMAGE_TYPE_1D;
		case VK_IMAGE_VIEW_TYPE_2D:
		case VK_IMAGE_VIEW_TYPE_2D_ARRAY:		return VK_IMAGE_TYPE_2D;
		case VK_IMAGE_VIEW_TYPE_3D:				return VK_IMAGE_TYPE_3D;
		case VK_IMAGE_VIEW_TYPE_CUBE:
		case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:		return VK_IMAGE_TYPE_2D;

		default:
			DE_FATAL("Impossible");
			return (VkImageType)0;
	}
}

vk::VkImageUsageFlags textureUsageFlags (void)
{
	return (VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
}

vk::VkImageCreateFlags textureCreateFlags (vk::VkImageViewType viewType, ShaderRenderCaseInstance::ImageBackingMode backingMode)
{
	const bool			isCube				= (viewType == VK_IMAGE_VIEW_TYPE_CUBE || viewType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY);
	VkImageCreateFlags	imageCreateFlags	= (isCube ? static_cast<VkImageCreateFlags>(VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) : 0u);

	if (backingMode == ShaderRenderCaseInstance::IMAGE_BACKING_MODE_SPARSE)
		imageCreateFlags |= (VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT);

	return imageCreateFlags;
}

namespace
{

static const deUint32	MAX_RENDER_WIDTH	= 128;
static const deUint32	MAX_RENDER_HEIGHT	= 128;
static const tcu::Vec4	DEFAULT_CLEAR_COLOR	= tcu::Vec4(0.125f, 0.25f, 0.5f, 1.0f);

/*! Gets the next multiple of a given divisor */
static deUint32 getNextMultiple (deUint32 divisor, deUint32 value)
{
	if (value % divisor == 0)
	{
		return value;
	}
	return value + divisor - (value % divisor);
}

/*! Gets the next value that is multiple of all given divisors */
static deUint32 getNextMultiple (const std::vector<deUint32>& divisors, deUint32 value)
{
	deUint32	nextMultiple		= value;
	bool		nextMultipleFound	= false;

	while (true)
	{
		nextMultipleFound = true;

		for (size_t divNdx = 0; divNdx < divisors.size(); divNdx++)
			nextMultipleFound = nextMultipleFound && (nextMultiple % divisors[divNdx] == 0);

		if (nextMultipleFound)
			break;

		DE_ASSERT(nextMultiple < ~((deUint32)0u));
		nextMultiple = getNextMultiple(divisors[0], nextMultiple + 1);
	}

	return nextMultiple;
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

TextureBinding::TextureBinding (const tcu::Texture1D* tex1D, const tcu::Sampler& sampler)
	: m_type	(TYPE_1D)
	, m_sampler	(sampler)
{
	m_binding.tex1D = tex1D;
}

TextureBinding::TextureBinding (const tcu::Texture2D* tex2D, const tcu::Sampler& sampler)
	: m_type	(TYPE_2D)
	, m_sampler	(sampler)
{
	m_binding.tex2D = tex2D;
}

TextureBinding::TextureBinding (const tcu::Texture3D* tex3D, const tcu::Sampler& sampler)
	: m_type	(TYPE_3D)
	, m_sampler	(sampler)
{
	m_binding.tex3D = tex3D;
}

TextureBinding::TextureBinding (const tcu::TextureCube* texCube, const tcu::Sampler& sampler)
	: m_type	(TYPE_CUBE_MAP)
	, m_sampler	(sampler)
{
	m_binding.texCube = texCube;
}

TextureBinding::TextureBinding (const tcu::Texture1DArray* tex1DArray, const tcu::Sampler& sampler)
	: m_type	(TYPE_1D_ARRAY)
	, m_sampler	(sampler)
{
	m_binding.tex1DArray = tex1DArray;
}

TextureBinding::TextureBinding (const tcu::Texture2DArray* tex2DArray, const tcu::Sampler& sampler)
	: m_type	(TYPE_2D_ARRAY)
	, m_sampler	(sampler)
{
	m_binding.tex2DArray = tex2DArray;
}

TextureBinding::TextureBinding (const tcu::TextureCubeArray* texCubeArray, const tcu::Sampler& sampler)
	: m_type	(TYPE_CUBE_ARRAY)
	, m_sampler	(sampler)
{
	m_binding.texCubeArray = texCubeArray;
}

TextureBinding::~TextureBinding (void)
{
	switch(m_type)
	{
		case TYPE_1D:			delete m_binding.tex1D;			break;
		case TYPE_2D:			delete m_binding.tex2D;			break;
		case TYPE_3D:			delete m_binding.tex3D;			break;
		case TYPE_CUBE_MAP:		delete m_binding.texCube;		break;
		case TYPE_1D_ARRAY:		delete m_binding.tex1DArray;	break;
		case TYPE_2D_ARRAY:		delete m_binding.tex2DArray;	break;
		case TYPE_CUBE_ARRAY:	delete m_binding.texCubeArray;	break;
		default:												break;
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
			case TextureBinding::TYPE_1D:			textures[ndx].tex1D			= &binding.get1D();			break;
			case TextureBinding::TYPE_2D:			textures[ndx].tex2D			= &binding.get2D();			break;
			case TextureBinding::TYPE_3D:			textures[ndx].tex3D			= &binding.get3D();			break;
			case TextureBinding::TYPE_CUBE_MAP:		textures[ndx].texCube		= &binding.getCube();		break;
			case TextureBinding::TYPE_1D_ARRAY:		textures[ndx].tex1DArray	= &binding.get1DArray();	break;
			case TextureBinding::TYPE_2D_ARRAY:		textures[ndx].tex2DArray	= &binding.get2DArray();	break;
			case TextureBinding::TYPE_CUBE_ARRAY:	textures[ndx].texCubeArray	= &binding.getCubeArray();	break;
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

ShaderRenderCaseInstance::ShaderRenderCaseInstance (Context& context)
	: vkt::TestInstance		(context)
	, m_imageBackingMode	(IMAGE_BACKING_MODE_REGULAR)
	, m_quadGridSize		(static_cast<deUint32>(GRID_SIZE_DEFAULT_FRAGMENT))
	, m_memAlloc			(getAllocator())
	, m_clearColor			(DEFAULT_CLEAR_COLOR)
	, m_isVertexCase		(false)
	, m_vertexShaderName	("vert")
	, m_fragmentShaderName	("frag")
	, m_renderSize			(MAX_RENDER_WIDTH, MAX_RENDER_HEIGHT)
	, m_colorFormat			(VK_FORMAT_R8G8B8A8_UNORM)
	, m_evaluator			(DE_NULL)
	, m_uniformSetup		(DE_NULL)
	, m_attribFunc			(DE_NULL)
	, m_sampleCount			(VK_SAMPLE_COUNT_1_BIT)
	, m_fuzzyCompare		(true)
{
}


ShaderRenderCaseInstance::ShaderRenderCaseInstance (Context&					context,
													const bool					isVertexCase,
													const ShaderEvaluator&		evaluator,
													const UniformSetup&			uniformSetup,
													const AttributeSetupFunc	attribFunc,
													const ImageBackingMode		imageBackingMode,
													const deUint32				gridSize,
													const bool					fuzzyCompare)
	: vkt::TestInstance		(context)
	, m_imageBackingMode	(imageBackingMode)
	, m_quadGridSize		(gridSize == static_cast<deUint32>(GRID_SIZE_DEFAULTS)
							 ? (isVertexCase
								? static_cast<deUint32>(GRID_SIZE_DEFAULT_VERTEX)
								: static_cast<deUint32>(GRID_SIZE_DEFAULT_FRAGMENT))
							 : gridSize)
	, m_memAlloc			(getAllocator())
	, m_clearColor			(DEFAULT_CLEAR_COLOR)
	, m_isVertexCase		(isVertexCase)
	, m_vertexShaderName	("vert")
	, m_fragmentShaderName	("frag")
	, m_renderSize			(MAX_RENDER_WIDTH, MAX_RENDER_HEIGHT)
	, m_colorFormat			(VK_FORMAT_R8G8B8A8_UNORM)
	, m_evaluator			(&evaluator)
	, m_uniformSetup		(&uniformSetup)
	, m_attribFunc			(attribFunc)
	, m_sampleCount			(VK_SAMPLE_COUNT_1_BIT)
	, m_fuzzyCompare		(fuzzyCompare)
{
}

ShaderRenderCaseInstance::ShaderRenderCaseInstance (Context&					context,
													const bool					isVertexCase,
													const ShaderEvaluator*		evaluator,
													const UniformSetup*			uniformSetup,
													const AttributeSetupFunc	attribFunc,
													const ImageBackingMode		imageBackingMode,
													const deUint32				gridSize)
	: vkt::TestInstance		(context)
	, m_imageBackingMode	(imageBackingMode)
	, m_quadGridSize		(gridSize == static_cast<deUint32>(GRID_SIZE_DEFAULTS)
							 ? (isVertexCase
								? static_cast<deUint32>(GRID_SIZE_DEFAULT_VERTEX)
								: static_cast<deUint32>(GRID_SIZE_DEFAULT_FRAGMENT))
							 : gridSize)
	, m_memAlloc			(getAllocator())
	, m_clearColor			(DEFAULT_CLEAR_COLOR)
	, m_isVertexCase		(isVertexCase)
	, m_vertexShaderName	("vert")
	, m_fragmentShaderName	("frag")
	, m_renderSize			(MAX_RENDER_WIDTH, MAX_RENDER_HEIGHT)
	, m_colorFormat			(VK_FORMAT_R8G8B8A8_UNORM)
	, m_evaluator			(evaluator)
	, m_uniformSetup		(uniformSetup)
	, m_attribFunc			(attribFunc)
	, m_sampleCount			(VK_SAMPLE_COUNT_1_BIT)
	, m_fuzzyCompare		(false)
{
}

vk::Allocator& ShaderRenderCaseInstance::getAllocator (void) const
{
	return m_context.getDefaultAllocator();
}

ShaderRenderCaseInstance::~ShaderRenderCaseInstance (void)
{
}

VkDevice ShaderRenderCaseInstance::getDevice (void) const
{
	return m_context.getDevice();
}

deUint32 ShaderRenderCaseInstance::getUniversalQueueFamilyIndex	(void) const
{
	return m_context.getUniversalQueueFamilyIndex();
}

deUint32 ShaderRenderCaseInstance::getSparseQueueFamilyIndex (void) const
{
	return m_context.getSparseQueueFamilyIndex();
}

const DeviceInterface& ShaderRenderCaseInstance::getDeviceInterface (void) const
{
	return m_context.getDeviceInterface();
}

VkQueue ShaderRenderCaseInstance::getUniversalQueue (void) const
{
	return m_context.getUniversalQueue();
}

VkQueue ShaderRenderCaseInstance::getSparseQueue (void) const
{
	return m_context.getSparseQueue();
}

VkPhysicalDevice ShaderRenderCaseInstance::getPhysicalDevice (void) const
{
	return m_context.getPhysicalDevice();
}

const InstanceInterface& ShaderRenderCaseInstance::getInstanceInterface (void) const
{
	return m_context.getInstanceInterface();
}

tcu::TestStatus ShaderRenderCaseInstance::iterate (void)
{
	setup();

	// Create quad grid.
	const tcu::UVec2	viewportSize	= getViewportSize();
	const int			width			= viewportSize.x();
	const int			height			= viewportSize.y();

	m_quadGrid							= de::MovePtr<QuadGrid>(new QuadGrid(m_quadGridSize, width, height, getDefaultConstCoords(), m_userAttribTransforms, m_textures));

	// Render result.
	tcu::Surface		resImage		(width, height);

	render(m_quadGrid->getNumVertices(), m_quadGrid->getNumTriangles(), m_quadGrid->getIndices(), m_quadGrid->getConstCoords());
	tcu::copy(resImage.getAccess(), m_resultImage.getAccess());

	// Compute reference.
	tcu::Surface		refImage		(width, height);
	if (m_isVertexCase)
		computeVertexReference(refImage, *m_quadGrid);
	else
		computeFragmentReference(refImage, *m_quadGrid);

	// Compare.
	const bool			compareOk		= compareImages(resImage, refImage, 0.2f);

	if (compareOk)
		return tcu::TestStatus::pass("Result image matches reference");
	else
		return tcu::TestStatus::fail("Image mismatch");
}

void ShaderRenderCaseInstance::setup (void)
{
	m_resultImage					= tcu::TextureLevel();
	m_descriptorSetLayoutBuilder	= de::MovePtr<DescriptorSetLayoutBuilder>	(new DescriptorSetLayoutBuilder());
	m_descriptorPoolBuilder			= de::MovePtr<DescriptorPoolBuilder>		(new DescriptorPoolBuilder());
	m_descriptorSetUpdateBuilder	= de::MovePtr<DescriptorSetUpdateBuilder>	(new DescriptorSetUpdateBuilder());

	m_uniformInfos.clear();
	m_vertexBindingDescription.clear();
	m_vertexAttributeDescription.clear();
	m_vertexBuffers.clear();
	m_vertexBufferAllocs.clear();
	m_pushConstantRanges.clear();
}

void ShaderRenderCaseInstance::setupUniformData (deUint32 bindingLocation, size_t size, const void* dataPtr)
{
	const VkDevice					vkDevice			= getDevice();
	const DeviceInterface&			vk					= getDeviceInterface();
	const deUint32					queueFamilyIndex	= getUniversalQueueFamilyIndex();

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
	flushAlloc(vk, vkDevice, *alloc);

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
	m_descriptorSetLayoutBuilder->addSingleBinding(descriptorType, vk::VK_SHADER_STAGE_ALL);
	m_descriptorPoolBuilder->addType(descriptorType);

	setupUniformData(bindingLocation, dataSize, data);
}

void ShaderRenderCaseInstance::addAttribute (deUint32		bindingLocation,
											 vk::VkFormat	format,
											 deUint32		sizePerElement,
											 deUint32		count,
											 const void*	dataPtr)
{
	// Portability requires stride to be multiply of minVertexInputBindingStrideAlignment
	// this value is usually 4 and current tests meet this requirement but
	// if this changes in future then this limit should be verified in checkSupport
#ifndef CTS_USES_VULKANSC
	if (m_context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") &&
		((sizePerElement % m_context.getPortabilitySubsetProperties().minVertexInputBindingStrideAlignment) != 0))
	{
		DE_FATAL("stride is not multiply of minVertexInputBindingStrideAlignment");
	}
#endif // CTS_USES_VULKANSC

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

	m_vertexAttributeDescription.push_back(attributeDescription);

	// Upload data to buffer
	const VkDevice							vkDevice				= getDevice();
	const DeviceInterface&					vk						= getDeviceInterface();
	const deUint32							queueFamilyIndex		= getUniversalQueueFamilyIndex();

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
	flushAlloc(vk, vkDevice, *alloc);

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

void ShaderRenderCaseInstance::setupUniforms (const tcu::Vec4& constCoords)
{
	if (m_uniformSetup)
		m_uniformSetup->setup(*this, constCoords);
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

void ShaderRenderCaseInstance::setSampleCount (VkSampleCountFlagBits sampleCount)
{
	m_sampleCount	= sampleCount;
}

bool ShaderRenderCaseInstance::isMultiSampling (void) const
{
	return m_sampleCount != VK_SAMPLE_COUNT_1_BIT;
}

void ShaderRenderCaseInstance::uploadImage (const tcu::TextureFormat&			texFormat,
											const TextureData&					textureData,
											const tcu::Sampler&					refSampler,
											deUint32							mipLevels,
											deUint32							arrayLayers,
											VkImage								destImage)
{
	const VkDevice					vkDevice				= getDevice();
	const DeviceInterface&			vk						= getDeviceInterface();
	const VkQueue					queue					= getUniversalQueue();
	const deUint32					queueFamilyIndex		= getUniversalQueueFamilyIndex();

	const bool						isShadowSampler			= refSampler.compare != tcu::Sampler::COMPAREMODE_NONE;
	const VkImageAspectFlags		aspectMask				= isShadowSampler ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	deUint32						bufferSize				= 0u;
	Move<VkBuffer>					buffer;
	de::MovePtr<Allocation>			bufferAlloc;
	std::vector<VkBufferImageCopy>	copyRegions;
	std::vector<deUint32>			offsetMultiples;

	offsetMultiples.push_back(4u);
	offsetMultiples.push_back(texFormat.getPixelSize());

	// Calculate buffer size
	for (TextureData::const_iterator mit = textureData.begin(); mit != textureData.end(); ++mit)
	{
		for (TextureLayerData::const_iterator lit = mit->begin(); lit != mit->end(); ++lit)
		{
			const tcu::ConstPixelBufferAccess&	access	= *lit;

			bufferSize = getNextMultiple(offsetMultiples, bufferSize);
			bufferSize += access.getWidth() * access.getHeight() * access.getDepth() * access.getFormat().getPixelSize();
		}
	}

	// Create source buffer
	{
		const VkBufferCreateInfo bufferParams =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			0u,											// VkBufferCreateFlags	flags;
			bufferSize,									// VkDeviceSize			size;
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,			// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
			0u,											// deUint32				queueFamilyIndexCount;
			DE_NULL,									// const deUint32*		pQueueFamilyIndices;
		};

		buffer		= createBuffer(vk, vkDevice, &bufferParams);
		bufferAlloc = m_memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *buffer), MemoryRequirement::HostVisible);
		VK_CHECK(vk.bindBufferMemory(vkDevice, *buffer, bufferAlloc->getMemory(), bufferAlloc->getOffset()));
	}

	// Get copy regions and write buffer data
	{
		deUint32	layerDataOffset		= 0;
		deUint8*	destPtr				= (deUint8*)bufferAlloc->getHostPtr();

		for (size_t levelNdx = 0; levelNdx < textureData.size(); levelNdx++)
		{
			const TextureLayerData&		layerData	= textureData[levelNdx];

			for (size_t layerNdx = 0; layerNdx < layerData.size(); layerNdx++)
			{
				layerDataOffset = getNextMultiple(offsetMultiples, layerDataOffset);

				const tcu::ConstPixelBufferAccess&	access		= layerData[layerNdx];
				const tcu::PixelBufferAccess		destAccess	(access.getFormat(), access.getSize(), destPtr + layerDataOffset);

				const VkBufferImageCopy				layerRegion =
				{
					layerDataOffset,						// VkDeviceSize				bufferOffset;
					(deUint32)access.getWidth(),			// deUint32					bufferRowLength;
					(deUint32)access.getHeight(),			// deUint32					bufferImageHeight;
					{										// VkImageSubresourceLayers	imageSubresource;
						aspectMask,								// VkImageAspectFlags		aspectMask;
						(deUint32)levelNdx,						// uint32_t					mipLevel;
						(deUint32)layerNdx,						// uint32_t					baseArrayLayer;
						1u										// uint32_t					layerCount;
					},
					{ 0u, 0u, 0u },							// VkOffset3D			imageOffset;
					{										// VkExtent3D			imageExtent;
						(deUint32)access.getWidth(),
						(deUint32)access.getHeight(),
						(deUint32)access.getDepth()
					}
				};

				copyRegions.push_back(layerRegion);
				tcu::copy(destAccess, access);

				layerDataOffset += access.getWidth() * access.getHeight() * access.getDepth() * access.getFormat().getPixelSize();
			}
		}
	}

	flushAlloc(vk, vkDevice, *bufferAlloc);

	if(m_externalCommandPool.get() != DE_NULL)
		copyBufferToImage(vk, vkDevice, queue, queueFamilyIndex, *buffer, bufferSize, copyRegions, DE_NULL, aspectMask, mipLevels, arrayLayers, destImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, &(m_externalCommandPool.get()->get()));
	else
		copyBufferToImage(vk, vkDevice, queue, queueFamilyIndex, *buffer, bufferSize, copyRegions, DE_NULL, aspectMask, mipLevels, arrayLayers, destImage);
}

void ShaderRenderCaseInstance::clearImage (const tcu::Sampler&					refSampler,
										   deUint32								mipLevels,
										   deUint32								arrayLayers,
										   VkImage								destImage)
{
	const VkDevice					vkDevice				= m_context.getDevice();
	const DeviceInterface&			vk						= m_context.getDeviceInterface();
	const VkQueue					queue					= m_context.getUniversalQueue();
	const deUint32					queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();

	const bool						isShadowSampler			= refSampler.compare != tcu::Sampler::COMPAREMODE_NONE;
	const VkImageAspectFlags		aspectMask				= isShadowSampler ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	Move<VkCommandPool>				cmdPool;
	Move<VkCommandBuffer>			cmdBuffer;

	VkClearValue					clearValue;
	deMemset(&clearValue, 0, sizeof(clearValue));


	// Create command pool
	VkCommandPool activeCmdPool;
	if (m_externalCommandPool.get() == DE_NULL)
	{
		// Create local command pool
		cmdPool = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
		activeCmdPool = *cmdPool;
	}
	else
	{
		// Use external command pool if available
		activeCmdPool = m_externalCommandPool.get()->get();
	}
	// Create command buffer
	cmdBuffer	= allocateCommandBuffer(vk, vkDevice, activeCmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	const VkImageMemoryBarrier preImageBarrier =
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// VkStructureType			sType;
		DE_NULL,										// const void*				pNext;
		0u,												// VkAccessFlags			srcAccessMask;
		VK_ACCESS_TRANSFER_WRITE_BIT,					// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,			// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,						// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,						// deUint32					dstQueueFamilyIndex;
		destImage,										// VkImage					image;
		{												// VkImageSubresourceRange	subresourceRange;
			aspectMask,								// VkImageAspect	aspect;
			0u,										// deUint32			baseMipLevel;
			mipLevels,								// deUint32			mipLevels;
			0u,										// deUint32			baseArraySlice;
			arrayLayers								// deUint32			arraySize;
		}
	};

	const VkImageMemoryBarrier postImageBarrier =
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// VkStructureType			sType;
		DE_NULL,										// const void*				pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,					// VkAccessFlags			srcAccessMask;
		VK_ACCESS_SHADER_READ_BIT,						// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,			// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,		// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,						// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,						// deUint32					dstQueueFamilyIndex;
		destImage,										// VkImage					image;
		{												// VkImageSubresourceRange	subresourceRange;
			aspectMask,								// VkImageAspect	aspect;
			0u,										// deUint32			baseMipLevel;
			mipLevels,								// deUint32			mipLevels;
			0u,										// deUint32			baseArraySlice;
			arrayLayers								// deUint32			arraySize;
		}
	};

	const VkImageSubresourceRange clearRange		=
	{
		aspectMask,										// VkImageAspectFlags	aspectMask;
		0u,												// deUint32				baseMipLevel;
		mipLevels,										// deUint32				levelCount;
		0u,												// deUint32				baseArrayLayer;
		arrayLayers										// deUint32				layerCount;
	};

	// Copy buffer to image
	beginCommandBuffer(vk, *cmdBuffer);
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &preImageBarrier);
	if (aspectMask == VK_IMAGE_ASPECT_COLOR_BIT)
	{
		vk.cmdClearColorImage(*cmdBuffer, destImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue.color, 1, &clearRange);
	}
	else
	{
		vk.cmdClearDepthStencilImage(*cmdBuffer, destImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue.depthStencil, 1, &clearRange);
	}
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &postImageBarrier);
	endCommandBuffer(vk, *cmdBuffer);

	submitCommandsAndWait(vk, vkDevice, queue, cmdBuffer.get());
}

VkExtent3D mipLevelExtents (const VkExtent3D& baseExtents, const deUint32 mipLevel)
{
	VkExtent3D result;

	result.width	= std::max(baseExtents.width  >> mipLevel, 1u);
	result.height	= std::max(baseExtents.height >> mipLevel, 1u);
	result.depth	= std::max(baseExtents.depth  >> mipLevel, 1u);

	return result;
}

tcu::UVec3 alignedDivide (const VkExtent3D& extent, const VkExtent3D& divisor)
{
	tcu::UVec3 result;

	result.x() = extent.width  / divisor.width  + ((extent.width  % divisor.width != 0)  ? 1u : 0u);
	result.y() = extent.height / divisor.height + ((extent.height % divisor.height != 0) ? 1u : 0u);
	result.z() = extent.depth  / divisor.depth  + ((extent.depth  % divisor.depth != 0)  ? 1u : 0u);

	return result;
}

bool isImageSizeSupported (const VkImageType imageType, const tcu::UVec3& imageSize, const vk::VkPhysicalDeviceLimits& limits)
{
	switch (imageType)
	{
		case VK_IMAGE_TYPE_1D:
			return (imageSize.x() <= limits.maxImageDimension1D
				 && imageSize.y() == 1
				 && imageSize.z() == 1);
		case VK_IMAGE_TYPE_2D:
			return (imageSize.x() <= limits.maxImageDimension2D
				 && imageSize.y() <= limits.maxImageDimension2D
				 && imageSize.z() == 1);
		case VK_IMAGE_TYPE_3D:
			return (imageSize.x() <= limits.maxImageDimension3D
				 && imageSize.y() <= limits.maxImageDimension3D
				 && imageSize.z() <= limits.maxImageDimension3D);
		default:
			DE_FATAL("Unknown image type");
			return false;
	}
}

void ShaderRenderCaseInstance::checkSparseSupport (const VkImageCreateInfo& imageInfo) const
{
#ifdef CTS_USES_VULKANSC
	TCU_THROW(NotSupportedError, "Vulkan SC does not support sparse operations");
#endif // CTS_USES_VULKANSC
	const InstanceInterface&		instance		= getInstanceInterface();
	const VkPhysicalDevice			physicalDevice	= getPhysicalDevice();
	const VkPhysicalDeviceFeatures	deviceFeatures	= getPhysicalDeviceFeatures(instance, physicalDevice);
#ifndef CTS_USES_VULKANSC
	const std::vector<VkSparseImageFormatProperties> sparseImageFormatPropVec = getPhysicalDeviceSparseImageFormatProperties(
		instance, physicalDevice, imageInfo.format, imageInfo.imageType, imageInfo.samples, imageInfo.usage, imageInfo.tiling);
#endif // CTS_USES_VULKANSC

	if (!deviceFeatures.shaderResourceResidency)
		TCU_THROW(NotSupportedError, "Required feature: shaderResourceResidency.");

	if (!deviceFeatures.sparseBinding)
		TCU_THROW(NotSupportedError, "Required feature: sparseBinding.");

	if (imageInfo.imageType == VK_IMAGE_TYPE_2D && !deviceFeatures.sparseResidencyImage2D)
		TCU_THROW(NotSupportedError, "Required feature: sparseResidencyImage2D.");

	if (imageInfo.imageType == VK_IMAGE_TYPE_3D && !deviceFeatures.sparseResidencyImage3D)
		TCU_THROW(NotSupportedError, "Required feature: sparseResidencyImage3D.");
#ifndef CTS_USES_VULKANSC
	if (sparseImageFormatPropVec.size() == 0)
		TCU_THROW(NotSupportedError, "The image format does not support sparse operations");
#endif // CTS_USES_VULKANSC
}

#ifndef CTS_USES_VULKANSC
void ShaderRenderCaseInstance::uploadSparseImage (const tcu::TextureFormat&		texFormat,
												  const TextureData&			textureData,
												  const tcu::Sampler&			refSampler,
												  const deUint32				mipLevels,
												  const deUint32				arrayLayers,
												  const VkImage					sparseImage,
												  const VkImageCreateInfo&		imageCreateInfo,
												  const tcu::UVec3				texSize)
{
	const VkDevice							vkDevice				= getDevice();
	const DeviceInterface&					vk						= getDeviceInterface();
	const VkPhysicalDevice					physicalDevice			= getPhysicalDevice();
	const VkQueue							queue					= getUniversalQueue();
	const VkQueue							sparseQueue				= getSparseQueue();
	const deUint32							queueFamilyIndex		= getUniversalQueueFamilyIndex();
	const InstanceInterface&				instance				= getInstanceInterface();
	const VkPhysicalDeviceProperties		deviceProperties		= getPhysicalDeviceProperties(instance, physicalDevice);
	const bool								isShadowSampler			= refSampler.compare != tcu::Sampler::COMPAREMODE_NONE;
	const VkImageAspectFlags				aspectMask				= isShadowSampler ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	const Unique<VkSemaphore>				imageMemoryBindSemaphore(createSemaphore(vk, vkDevice));
	Move<VkBuffer>							buffer;
	deUint32								bufferSize				= 0u;
	de::MovePtr<Allocation>					bufferAlloc;
	std::vector<VkBufferImageCopy>			copyRegions;
	std::vector<deUint32>					offsetMultiples;

	offsetMultiples.push_back(4u);
	offsetMultiples.push_back(texFormat.getPixelSize());

	if (isImageSizeSupported(imageCreateInfo.imageType, texSize, deviceProperties.limits) == false)
		TCU_THROW(NotSupportedError, "Image size not supported for device.");

	allocateAndBindSparseImage(vk, vkDevice, physicalDevice, instance, imageCreateInfo, *imageMemoryBindSemaphore, sparseQueue, m_memAlloc, m_allocations, texFormat, sparseImage);

	// Calculate buffer size
	for (TextureData::const_iterator mit = textureData.begin(); mit != textureData.end(); ++mit)
	{
		for (TextureLayerData::const_iterator lit = mit->begin(); lit != mit->end(); ++lit)
		{
			const tcu::ConstPixelBufferAccess&	access	= *lit;

			bufferSize = getNextMultiple(offsetMultiples, bufferSize);
			bufferSize += access.getWidth() * access.getHeight() * access.getDepth() * access.getFormat().getPixelSize();
		}
	}

	{
		// Create source buffer
		const VkBufferCreateInfo bufferParams =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,								// const void*			pNext;
			0u,										// VkBufferCreateFlags	flags;
			bufferSize,								// VkDeviceSize			size;
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,		// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode;
			0u,										// deUint32				queueFamilyIndexCount;
			DE_NULL,								// const deUint32*		pQueueFamilyIndices;
		};

		buffer		= createBuffer(vk, vkDevice, &bufferParams);
		bufferAlloc = m_memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *buffer), MemoryRequirement::HostVisible);

		VK_CHECK(vk.bindBufferMemory(vkDevice, *buffer, bufferAlloc->getMemory(), bufferAlloc->getOffset()));
	}

	// Get copy regions and write buffer data
	{
		deUint32	layerDataOffset		= 0;
		deUint8*	destPtr				= (deUint8*)bufferAlloc->getHostPtr();

		for (size_t levelNdx = 0; levelNdx < textureData.size(); levelNdx++)
		{
			const TextureLayerData&		layerData	= textureData[levelNdx];

			for (size_t layerNdx = 0; layerNdx < layerData.size(); layerNdx++)
			{
				layerDataOffset = getNextMultiple(offsetMultiples, layerDataOffset);

				const tcu::ConstPixelBufferAccess&	access		= layerData[layerNdx];
				const tcu::PixelBufferAccess		destAccess	(access.getFormat(), access.getSize(), destPtr + layerDataOffset);

				const VkBufferImageCopy				layerRegion =
				{
					layerDataOffset,						// VkDeviceSize				bufferOffset;
					(deUint32)access.getWidth(),			// deUint32					bufferRowLength;
					(deUint32)access.getHeight(),			// deUint32					bufferImageHeight;
					{										// VkImageSubresourceLayers	imageSubresource;
						aspectMask,								// VkImageAspectFlags		aspectMask;
						(deUint32)levelNdx,						// uint32_t					mipLevel;
						(deUint32)layerNdx,						// uint32_t					baseArrayLayer;
						1u										// uint32_t					layerCount;
					},
					{ 0u, 0u, 0u },							// VkOffset3D			imageOffset;
					{										// VkExtent3D			imageExtent;
						(deUint32)access.getWidth(),
						(deUint32)access.getHeight(),
						(deUint32)access.getDepth()
					}
				};

				copyRegions.push_back(layerRegion);
				tcu::copy(destAccess, access);

				layerDataOffset += access.getWidth() * access.getHeight() * access.getDepth() * access.getFormat().getPixelSize();
			}
		}
	}
	copyBufferToImage(vk, vkDevice, queue, queueFamilyIndex, *buffer, bufferSize, copyRegions, &(*imageMemoryBindSemaphore), aspectMask, mipLevels, arrayLayers, sparseImage);
}
#endif // CTS_USES_VULKANSC

void ShaderRenderCaseInstance::useSampler (deUint32 bindingLocation, deUint32 textureId)
{
	DE_ASSERT(textureId < m_textures.size());

	const TextureBinding&				textureBinding		= *m_textures[textureId];
	const TextureBinding::Type			textureType			= textureBinding.getType();
	const tcu::Sampler&					refSampler			= textureBinding.getSampler();
	const TextureBinding::Parameters&	textureParams		= textureBinding.getParameters();
	const bool							isMSTexture			= textureParams.samples != vk::VK_SAMPLE_COUNT_1_BIT;
	deUint32							mipLevels			= 1u;
	deUint32							arrayLayers			= 1u;
	tcu::TextureFormat					texFormat;
	tcu::UVec3							texSize;
	TextureData							textureData;

	if (textureType == TextureBinding::TYPE_2D)
	{
		const tcu::Texture2D&			texture		= textureBinding.get2D();

		texFormat									= texture.getFormat();
		texSize										= tcu::UVec3(texture.getWidth(), texture.getHeight(), 1u);
		mipLevels									= isMSTexture ? 1u : (deUint32)texture.getNumLevels();
		arrayLayers									= 1u;

		textureData.resize(mipLevels);

		for (deUint32 level = 0; level < mipLevels; ++level)
		{
			if (texture.isLevelEmpty(level))
				continue;

			textureData[level].push_back(texture.getLevel(level));
		}
	}
	else if (textureType == TextureBinding::TYPE_CUBE_MAP)
	{
		const tcu::TextureCube&			texture		= textureBinding.getCube();

		texFormat									= texture.getFormat();
		texSize										= tcu::UVec3(texture.getSize(), texture.getSize(), 1u);
		mipLevels									= isMSTexture ? 1u : (deUint32)texture.getNumLevels();
		arrayLayers									= 6u;

		static const tcu::CubeFace		cubeFaceMapping[tcu::CUBEFACE_LAST] =
		{
			tcu::CUBEFACE_POSITIVE_X,
			tcu::CUBEFACE_NEGATIVE_X,
			tcu::CUBEFACE_POSITIVE_Y,
			tcu::CUBEFACE_NEGATIVE_Y,
			tcu::CUBEFACE_POSITIVE_Z,
			tcu::CUBEFACE_NEGATIVE_Z
		};

		textureData.resize(mipLevels);

		for (deUint32 level = 0; level < mipLevels; ++level)
		{
			for (int faceNdx = 0; faceNdx < tcu::CUBEFACE_LAST; ++faceNdx)
			{
				tcu::CubeFace face = cubeFaceMapping[faceNdx];

				if (texture.isLevelEmpty(face, level))
					continue;

				textureData[level].push_back(texture.getLevelFace(level, face));
			}
		}
	}
	else if (textureType == TextureBinding::TYPE_2D_ARRAY)
	{
		const tcu::Texture2DArray&		texture		= textureBinding.get2DArray();

		texFormat									= texture.getFormat();
		texSize										= tcu::UVec3(texture.getWidth(), texture.getHeight(), 1u);
		mipLevels									= isMSTexture ? 1u : (deUint32)texture.getNumLevels();
		arrayLayers									= (deUint32)texture.getNumLayers();

		textureData.resize(mipLevels);

		for (deUint32 level = 0; level < mipLevels; ++level)
		{
			if (texture.isLevelEmpty(level))
				continue;

			const tcu::ConstPixelBufferAccess&	levelLayers		= texture.getLevel(level);
			const deUint32						layerSize		= levelLayers.getWidth() * levelLayers.getHeight() * levelLayers.getFormat().getPixelSize();

			for (deUint32 layer = 0; layer < arrayLayers; ++layer)
			{
				const deUint32					layerOffset		= layerSize * layer;
				tcu::ConstPixelBufferAccess		layerData		(levelLayers.getFormat(), levelLayers.getWidth(), levelLayers.getHeight(), 1, (deUint8*)levelLayers.getDataPtr() + layerOffset);
				textureData[level].push_back(layerData);
			}
		}
	}
	else if (textureType == TextureBinding::TYPE_3D)
	{
		const tcu::Texture3D&			texture		= textureBinding.get3D();

		texFormat									= texture.getFormat();
		texSize										= tcu::UVec3(texture.getWidth(), texture.getHeight(), texture.getDepth());
		mipLevels									= isMSTexture ? 1u : (deUint32)texture.getNumLevels();
		arrayLayers									= 1u;

		textureData.resize(mipLevels);

		for (deUint32 level = 0; level < mipLevels; ++level)
		{
			if (texture.isLevelEmpty(level))
				continue;

			textureData[level].push_back(texture.getLevel(level));
		}
	}
	else if (textureType == TextureBinding::TYPE_1D)
	{
		const tcu::Texture1D&			texture		= textureBinding.get1D();

		texFormat									= texture.getFormat();
		texSize										= tcu::UVec3(texture.getWidth(), 1, 1);
		mipLevels									= isMSTexture ? 1u : (deUint32)texture.getNumLevels();
		arrayLayers									= 1u;

		textureData.resize(mipLevels);

		for (deUint32 level = 0; level < mipLevels; ++level)
		{
			if (texture.isLevelEmpty(level))
				continue;

			textureData[level].push_back(texture.getLevel(level));
		}
	}
	else if (textureType == TextureBinding::TYPE_1D_ARRAY)
	{
		const tcu::Texture1DArray&		texture		= textureBinding.get1DArray();

		texFormat									= texture.getFormat();
		texSize										= tcu::UVec3(texture.getWidth(), 1, 1);
		mipLevels									= isMSTexture ? 1u : (deUint32)texture.getNumLevels();
		arrayLayers									= (deUint32)texture.getNumLayers();

		textureData.resize(mipLevels);

		for (deUint32 level = 0; level < mipLevels; ++level)
		{
			if (texture.isLevelEmpty(level))
				continue;

			const tcu::ConstPixelBufferAccess&	levelLayers		= texture.getLevel(level);
			const deUint32						layerSize		= levelLayers.getWidth() * levelLayers.getFormat().getPixelSize();

			for (deUint32 layer = 0; layer < arrayLayers; ++layer)
			{
				const deUint32					layerOffset		= layerSize * layer;
				tcu::ConstPixelBufferAccess		layerData		(levelLayers.getFormat(), levelLayers.getWidth(), 1, 1, (deUint8*)levelLayers.getDataPtr() + layerOffset);
				textureData[level].push_back(layerData);
			}
		}
	}
	else if (textureType == TextureBinding::TYPE_CUBE_ARRAY)
	{
		const tcu::TextureCubeArray&	texture		= textureBinding.getCubeArray();
		texFormat									= texture.getFormat();
		texSize										= tcu::UVec3(texture.getSize(), texture.getSize(), 1);
		mipLevels									= isMSTexture ? 1u : (deUint32)texture.getNumLevels();
		arrayLayers									= texture.getDepth();

		textureData.resize(mipLevels);

		for (deUint32 level = 0; level < mipLevels; ++level)
		{
			if (texture.isLevelEmpty(level))
				continue;

			const tcu::ConstPixelBufferAccess&	levelLayers		= texture.getLevel(level);
			const deUint32						layerSize		= levelLayers.getWidth() * levelLayers.getHeight() * levelLayers.getFormat().getPixelSize();

			for (deUint32 layer = 0; layer < arrayLayers; ++layer)
			{
				const deUint32					layerOffset		= layerSize * layer;
				tcu::ConstPixelBufferAccess		layerData		(levelLayers.getFormat(), levelLayers.getWidth(), levelLayers.getHeight(), 1, (deUint8*)levelLayers.getDataPtr() + layerOffset);
				textureData[level].push_back(layerData);
			}
		}
	}
	else
	{
		TCU_THROW(InternalError, "Invalid texture type");
	}

	createSamplerUniform(bindingLocation, textureType, textureBinding.getParameters().initialization, texFormat, texSize, textureData, refSampler, mipLevels, arrayLayers, textureParams);
}

void ShaderRenderCaseInstance::setPushConstantRanges (const deUint32 rangeCount, const vk::VkPushConstantRange* const pcRanges)
{
	m_pushConstantRanges.clear();
	for (deUint32 i = 0; i < rangeCount; ++i)
	{
		m_pushConstantRanges.push_back(pcRanges[i]);
	}
}

void ShaderRenderCaseInstance::updatePushConstants (vk::VkCommandBuffer, vk::VkPipelineLayout)
{
}

void ShaderRenderCaseInstance::createSamplerUniform (deUint32						bindingLocation,
													 TextureBinding::Type			textureType,
													 TextureBinding::Init			textureInit,
													 const tcu::TextureFormat&		texFormat,
													 const tcu::UVec3				texSize,
													 const TextureData&				textureData,
													 const tcu::Sampler&			refSampler,
													 deUint32						mipLevels,
													 deUint32						arrayLayers,
													 TextureBinding::Parameters		textureParams)
{
	const VkDevice					vkDevice			= getDevice();
	const DeviceInterface&			vk					= getDeviceInterface();
	const deUint32					queueFamilyIndex	= getUniversalQueueFamilyIndex();
	const deUint32					sparseFamilyIndex	= (m_imageBackingMode == IMAGE_BACKING_MODE_SPARSE) ? getSparseQueueFamilyIndex() : queueFamilyIndex;

	const bool						isShadowSampler		= refSampler.compare != tcu::Sampler::COMPAREMODE_NONE;

	// when isShadowSampler is true mapSampler utill will set compareEnabled in
	// VkSamplerCreateInfo to true and in portability this functionality is under
	// feature flag - note that this is safety check as this is known at the
	// TestCase level and NotSupportedError should be thrown from checkSupport
#ifndef CTS_USES_VULKANSC
	if (isShadowSampler &&
		m_context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") &&
		!m_context.getPortabilitySubsetFeatures().mutableComparisonSamplers)
	{
		DE_FATAL("mutableComparisonSamplers support should be checked in checkSupport");
	}
#endif // CTS_USES_VULKANSC

	const VkImageAspectFlags		aspectMask			= isShadowSampler ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	const VkImageViewType			imageViewType		= textureTypeToImageViewType(textureType);
	const VkImageType				imageType			= viewTypeToImageType(imageViewType);
	const VkSharingMode				sharingMode			= (queueFamilyIndex != sparseFamilyIndex) ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
	const VkFormat					format				= mapTextureFormat(texFormat);
	const VkImageUsageFlags			imageUsageFlags		= textureUsageFlags();
	const VkImageCreateFlags		imageCreateFlags	= textureCreateFlags(imageViewType, m_imageBackingMode);

	const deUint32					queueIndexCount		= (queueFamilyIndex != sparseFamilyIndex) ? 2 : 1;
	const deUint32					queueIndices[]		=
	{
		queueFamilyIndex,
		sparseFamilyIndex
	};

	Move<VkImage>					vkTexture;
	de::MovePtr<Allocation>			allocation;

	// Create image
	const VkImageCreateInfo			imageParams =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,							// VkStructureType			sType;
		DE_NULL,														// const void*				pNext;
		imageCreateFlags,												// VkImageCreateFlags		flags;
		imageType,														// VkImageType				imageType;
		format,															// VkFormat					format;
		{																// VkExtent3D				extent;
			texSize.x(),
			texSize.y(),
			texSize.z()
		},
		mipLevels,														// deUint32					mipLevels;
		arrayLayers,													// deUint32					arrayLayers;
		textureParams.samples,											// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,										// VkImageTiling			tiling;
		imageUsageFlags,												// VkImageUsageFlags		usage;
		sharingMode,													// VkSharingMode			sharingMode;
		queueIndexCount,												// deUint32					queueFamilyIndexCount;
		queueIndices,													// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED										// VkImageLayout			initialLayout;
	};

	if (m_imageBackingMode == IMAGE_BACKING_MODE_SPARSE)
	{
		checkSparseSupport(imageParams);
	}

	vkTexture		= createImage(vk, vkDevice, &imageParams);
	allocation		= m_memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *vkTexture), MemoryRequirement::Any);

	if (m_imageBackingMode != IMAGE_BACKING_MODE_SPARSE)
	{
		VK_CHECK(vk.bindImageMemory(vkDevice, *vkTexture, allocation->getMemory(), allocation->getOffset()));
	}

	switch (textureInit)
	{
		case TextureBinding::INIT_UPLOAD_DATA:
		{
			// upload*Image functions use cmdCopyBufferToImage, which is invalid for multisample images
			DE_ASSERT(textureParams.samples == VK_SAMPLE_COUNT_1_BIT);

			if (m_imageBackingMode == IMAGE_BACKING_MODE_SPARSE)
			{
#ifndef CTS_USES_VULKANSC
				uploadSparseImage(texFormat, textureData, refSampler, mipLevels, arrayLayers, *vkTexture, imageParams, texSize);
#endif // CTS_USES_VULKANSC
			}
			else
			{
				// Upload texture data
				uploadImage(texFormat, textureData, refSampler, mipLevels, arrayLayers, *vkTexture);
			}
			break;
		}
		case TextureBinding::INIT_CLEAR:
			clearImage(refSampler, mipLevels, arrayLayers, *vkTexture);
			break;
		default:
			DE_FATAL("Impossible");
	}

	// Create sampler
	const auto&						minMaxLod		= textureParams.minMaxLod;
	const VkSamplerCreateInfo		samplerParams	= (minMaxLod
														? mapSampler(refSampler, texFormat, minMaxLod.get().minLod, minMaxLod.get().maxLod)
														: mapSampler(refSampler, texFormat));
	Move<VkSampler>					sampler			= createSampler(vk, vkDevice, &samplerParams);
	const deUint32					baseMipLevel	= textureParams.baseMipLevel;
	const vk::VkComponentMapping	components		= textureParams.componentMapping;
	const VkImageViewCreateInfo		viewParams		=
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// VkStructureType			sType;
		NULL,										// const voide*				pNext;
		0u,											// VkImageViewCreateFlags	flags;
		*vkTexture,									// VkImage					image;
		imageViewType,								// VkImageViewType			viewType;
		format,										// VkFormat					format;
		components,									// VkChannelMapping			channels;
		{
			aspectMask,						// VkImageAspectFlags	aspectMask;
			baseMipLevel,					// deUint32				baseMipLevel;
			mipLevels - baseMipLevel,		// deUint32				mipLevels;
			0,								// deUint32				baseArraySlice;
			arrayLayers						// deUint32				arraySize;
		},											// VkImageSubresourceRange	subresourceRange;
	};

	Move<VkImageView>				imageView		= createImageView(vk, vkDevice, &viewParams);

	const vk::VkDescriptorImageInfo	descriptor		=
	{
		sampler.get(),								// VkSampler				sampler;
		imageView.get(),							// VkImageView				imageView;
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,	// VkImageLayout			imageLayout;
	};

	de::MovePtr<SamplerUniform> uniform(new SamplerUniform());
	uniform->type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	uniform->descriptor = descriptor;
	uniform->location = bindingLocation;
	uniform->image = VkImageSp(new vk::Unique<VkImage>(vkTexture));
	uniform->imageView = VkImageViewSp(new vk::Unique<VkImageView>(imageView));
	uniform->sampler = VkSamplerSp(new vk::Unique<VkSampler>(sampler));
	uniform->alloc = AllocationSp(allocation.release());

	m_descriptorSetLayoutBuilder->addSingleSamplerBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, vk::VK_SHADER_STAGE_ALL, DE_NULL);
	m_descriptorPoolBuilder->addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

	m_uniformInfos.push_back(UniformInfoSp(new de::UniquePtr<UniformInfo>(uniform)));
}

void ShaderRenderCaseInstance::setupDefaultInputs (void)
{
	/* Configuration of the vertex input attributes:
		a_position   is at location 0
		a_coords     is at location 1
		a_unitCoords is at location 2
		a_one        is at location 3

	  User attributes starts from at the location 4.
	*/

	DE_ASSERT(m_quadGrid);
	const QuadGrid&		quadGrid	= *m_quadGrid;

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

void ShaderRenderCaseInstance::render (deUint32				numVertices,
									   deUint32				numTriangles,
									   const deUint16*		indices,
									   const tcu::Vec4&		constCoords)
{
	render(numVertices, numTriangles * 3, indices, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, constCoords);
}

void ShaderRenderCaseInstance::render (deUint32				numVertices,
									   deUint32				numIndices,
									   const deUint16*		indices,
									   VkPrimitiveTopology	topology,
									   const tcu::Vec4&		constCoords)
{
	const VkDevice										vkDevice					= getDevice();
	const DeviceInterface&								vk							= getDeviceInterface();
	const VkQueue										queue						= getUniversalQueue();
	const deUint32										queueFamilyIndex			= getUniversalQueueFamilyIndex();

	vk::Move<vk::VkImage>								colorImage;
	de::MovePtr<vk::Allocation>							colorImageAlloc;
	vk::Move<vk::VkImageView>							colorImageView;
	vk::Move<vk::VkImage>								resolvedImage;
	de::MovePtr<vk::Allocation>							resolvedImageAlloc;
	vk::Move<vk::VkImageView>							resolvedImageView;
	vk::Move<vk::VkRenderPass>							renderPass;
	vk::Move<vk::VkFramebuffer>							framebuffer;
	vk::Move<vk::VkPipelineLayout>						pipelineLayout;
	vk::Move<vk::VkPipeline>							graphicsPipeline;
	vk::Move<vk::VkShaderModule>						vertexShaderModule;
	vk::Move<vk::VkShaderModule>						fragmentShaderModule;
	vk::Move<vk::VkBuffer>								indexBuffer;
	de::MovePtr<vk::Allocation>							indexBufferAlloc;
	vk::Move<vk::VkDescriptorSetLayout>					descriptorSetLayout;
	vk::Move<vk::VkDescriptorPool>						descriptorPool;
	vk::Move<vk::VkDescriptorSet>						descriptorSet;
	vk::Move<vk::VkCommandPool>							cmdPool;
	vk::Move<vk::VkCommandBuffer>						cmdBuffer;

	// Create color image
	{
		const VkImageUsageFlags	imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		VkImageFormatProperties	properties;

		if ((getInstanceInterface().getPhysicalDeviceImageFormatProperties(getPhysicalDevice(),
																		   m_colorFormat,
																		   VK_IMAGE_TYPE_2D,
																		   VK_IMAGE_TILING_OPTIMAL,
																		   imageUsage,
																		   0u,
																		   &properties) == VK_ERROR_FORMAT_NOT_SUPPORTED))
		{
			TCU_THROW(NotSupportedError, "Format not supported");
		}

		if ((properties.sampleCounts & m_sampleCount) != m_sampleCount)
		{
			TCU_THROW(NotSupportedError, "Format not supported");
		}

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
			m_sampleCount,																// deUint32				samples;
			VK_IMAGE_TILING_OPTIMAL,													// VkImageTiling		tiling;
			imageUsage,																	// VkImageUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,													// VkSharingMode		sharingMode;
			1u,																			// deUint32				queueFamilyCount;
			&queueFamilyIndex,															// const deUint32*		pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED,													// VkImageLayout		initialLayout;
		};

		colorImage = createImage(vk, vkDevice, &colorImageParams);

		// Allocate and bind color image memory
		colorImageAlloc = m_memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *colorImage), MemoryRequirement::Any);
		VK_CHECK(vk.bindImageMemory(vkDevice, *colorImage, colorImageAlloc->getMemory(), colorImageAlloc->getOffset()));
	}

	// Create color attachment view
	{
		const VkImageViewCreateInfo						colorImageViewParams		=
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,			// VkStructureType			sType;
			DE_NULL,											// const void*				pNext;
			0u,													// VkImageViewCreateFlags	flags;
			*colorImage,										// VkImage					image;
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

		colorImageView = createImageView(vk, vkDevice, &colorImageViewParams);
	}

	if (isMultiSampling())
	{
		// Resolved Image
		{
			const VkImageUsageFlags	imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
			VkImageFormatProperties	properties;

			if ((getInstanceInterface().getPhysicalDeviceImageFormatProperties(getPhysicalDevice(),
																			   m_colorFormat,
																			   VK_IMAGE_TYPE_2D,
																			   VK_IMAGE_TILING_OPTIMAL,
																			   imageUsage,
																			   0,
																			   &properties) == VK_ERROR_FORMAT_NOT_SUPPORTED))
			{
				TCU_THROW(NotSupportedError, "Format not supported");
			}

			const VkImageCreateInfo					imageCreateInfo			=
			{
				VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,		// VkStructureType			sType;
				DE_NULL,									// const void*				pNext;
				0u,											// VkImageCreateFlags		flags;
				VK_IMAGE_TYPE_2D,							// VkImageType				imageType;
				m_colorFormat,								// VkFormat					format;
				{ m_renderSize.x(),	m_renderSize.y(), 1u },	// VkExtent3D				extent;
				1u,											// deUint32					mipLevels;
				1u,											// deUint32					arrayLayers;
				VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits	samples;
				VK_IMAGE_TILING_OPTIMAL,					// VkImageTiling			tiling;
				imageUsage,									// VkImageUsageFlags		usage;
				VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode			sharingMode;
				1u,											// deUint32					queueFamilyIndexCount;
				&queueFamilyIndex,							// const deUint32*			pQueueFamilyIndices;
				VK_IMAGE_LAYOUT_UNDEFINED					// VkImageLayout			initialLayout;
			};

			resolvedImage		= vk::createImage(vk, vkDevice, &imageCreateInfo, DE_NULL);
			resolvedImageAlloc	= m_memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *resolvedImage), MemoryRequirement::Any);
			VK_CHECK(vk.bindImageMemory(vkDevice, *resolvedImage, resolvedImageAlloc->getMemory(), resolvedImageAlloc->getOffset()));
		}

		// Resolved Image View
		{
			const VkImageViewCreateInfo				imageViewCreateInfo		=
			{
				VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// VkStructureType				sType;
				DE_NULL,									// const void*					pNext;
				0u,											// VkImageViewCreateFlags		flags;
				*resolvedImage,								// VkImage						image;
				VK_IMAGE_VIEW_TYPE_2D,						// VkImageViewType				viewType;
				m_colorFormat,								// VkFormat						format;
				{
					VK_COMPONENT_SWIZZLE_R,					// VkChannelSwizzle		r;
					VK_COMPONENT_SWIZZLE_G,					// VkChannelSwizzle		g;
					VK_COMPONENT_SWIZZLE_B,					// VkChannelSwizzle		b;
					VK_COMPONENT_SWIZZLE_A					// VkChannelSwizzle		a;
				},
				{
					VK_IMAGE_ASPECT_COLOR_BIT,					// VkImageAspectFlags			aspectMask;
					0u,											// deUint32						baseMipLevel;
					1u,											// deUint32						mipLevels;
					0u,											// deUint32						baseArrayLayer;
					1u,											// deUint32						arraySize;
				},											// VkImageSubresourceRange		subresourceRange;
			};

			resolvedImageView = vk::createImageView(vk, vkDevice, &imageViewCreateInfo, DE_NULL);
		}
	}

	// Create render pass
	{
		const VkAttachmentDescription					attachmentDescription[]		=
		{
			{
				(VkAttachmentDescriptionFlags)0,					// VkAttachmentDescriptionFlags		flags;
				m_colorFormat,										// VkFormat							format;
				m_sampleCount,										// deUint32							samples;
				VK_ATTACHMENT_LOAD_OP_CLEAR,						// VkAttachmentLoadOp				loadOp;
				VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp				storeOp;
				VK_ATTACHMENT_LOAD_OP_DONT_CARE,					// VkAttachmentLoadOp				stencilLoadOp;
				VK_ATTACHMENT_STORE_OP_DONT_CARE,					// VkAttachmentStoreOp				stencilStoreOp;
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,			// VkImageLayout					initialLayout;
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,			// VkImageLayout					finalLayout;
			},
			{
				(VkAttachmentDescriptionFlags)0,					// VkAttachmentDescriptionFlags		flags;
				m_colorFormat,										// VkFormat							format;
				VK_SAMPLE_COUNT_1_BIT,								// VkSampleCountFlagBits			samples;
				VK_ATTACHMENT_LOAD_OP_DONT_CARE,					// VkAttachmentLoadOp				loadOp;
				VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp				storeOp;
				VK_ATTACHMENT_LOAD_OP_DONT_CARE,					// VkAttachmentLoadOp				stencilLoadOp;
				VK_ATTACHMENT_STORE_OP_DONT_CARE,					// VkAttachmentStoreOp				stencilStoreOp;
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,			// VkImageLayout					initialLayout;
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,			// VkImageLayout					finalLayout;
			}
		};

		const VkAttachmentReference						attachmentReference			=
		{
			0u,													// deUint32			attachment;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL			// VkImageLayout	layout;
		};

		const VkAttachmentReference						resolveAttachmentRef		=
		{
			1u,													// deUint32			attachment;
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
			isMultiSampling() ? &resolveAttachmentRef : DE_NULL,// constVkAttachmentReference*	pResolveAttachments;
			DE_NULL,											// VkAttachmentReference		depthStencilAttachment;
			0u,													// deUint32						preserveCount;
			DE_NULL												// constVkAttachmentReference*	pPreserveAttachments;
		};

		const VkRenderPassCreateInfo					renderPassParams			=
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,			// VkStructureType					sType;
			DE_NULL,											// const void*						pNext;
			0u,													// VkRenderPassCreateFlags			flags;
			isMultiSampling() ? 2u : 1u,						// deUint32							attachmentCount;
			attachmentDescription,								// const VkAttachmentDescription*	pAttachments;
			1u,													// deUint32							subpassCount;
			&subpassDescription,								// const VkSubpassDescription*		pSubpasses;
			0u,													// deUint32							dependencyCount;
			DE_NULL												// const VkSubpassDependency*		pDependencies;
		};

		renderPass = createRenderPass(vk, vkDevice, &renderPassParams);
	}

	// Create framebuffer
	{
		const VkImageView								attachments[]				=
		{
			*colorImageView,
			*resolvedImageView
		};

		const VkFramebufferCreateInfo					framebufferParams			=
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,			// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			(VkFramebufferCreateFlags)0,
			*renderPass,										// VkRenderPass					renderPass;
			isMultiSampling() ? 2u : 1u,						// deUint32						attachmentCount;
			attachments,										// const VkImageView*			pAttachments;
			(deUint32)m_renderSize.x(),							// deUint32						width;
			(deUint32)m_renderSize.y(),							// deUint32						height;
			1u													// deUint32						layers;
		};

		framebuffer = createFramebuffer(vk, vkDevice, &framebufferParams);
	}

	// Create descriptors
	{
		setupUniforms(constCoords);

		descriptorSetLayout = m_descriptorSetLayoutBuilder->build(vk, vkDevice);
		if (!m_uniformInfos.empty())
		{
			descriptorPool									= m_descriptorPoolBuilder->build(vk, vkDevice, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
			const VkDescriptorSetAllocateInfo	allocInfo	=
			{
				VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				DE_NULL,
				*descriptorPool,
				1u,
				&descriptorSetLayout.get(),
			};

			descriptorSet = allocateDescriptorSet(vk, vkDevice, &allocInfo);
		}

		for (deUint32 i = 0; i < m_uniformInfos.size(); i++)
		{
			const UniformInfo* uniformInfo = m_uniformInfos[i].get()->get();
			deUint32 location = uniformInfo->location;

			if (uniformInfo->type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
			{
				const BufferUniform*	bufferInfo	= dynamic_cast<const BufferUniform*>(uniformInfo);

				m_descriptorSetUpdateBuilder->writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(location), uniformInfo->type, &bufferInfo->descriptor);
			}
			else if (uniformInfo->type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
			{
				const SamplerUniform*	samplerInfo	= dynamic_cast<const SamplerUniform*>(uniformInfo);

				m_descriptorSetUpdateBuilder->writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(location), uniformInfo->type, &samplerInfo->descriptor);
			}
			else
				DE_FATAL("Impossible");
		}

		m_descriptorSetUpdateBuilder->update(vk, vkDevice);
	}

	// Create pipeline layout
	{
		const VkPushConstantRange* const				pcRanges					= m_pushConstantRanges.empty() ? DE_NULL : &m_pushConstantRanges[0];
		const VkPipelineLayoutCreateInfo				pipelineLayoutParams		=
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			(VkPipelineLayoutCreateFlags)0,
			1u,													// deUint32						descriptorSetCount;
			&*descriptorSetLayout,								// const VkDescriptorSetLayout*	pSetLayouts;
			deUint32(m_pushConstantRanges.size()),				// deUint32						pushConstantRangeCount;
			pcRanges											// const VkPushConstantRange*	pPushConstantRanges;
		};

		pipelineLayout = createPipelineLayout(vk, vkDevice, &pipelineLayoutParams);
	}

	// Create shaders
	{
		vertexShaderModule		= createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get(m_vertexShaderName), 0);
		fragmentShaderModule	= createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get(m_fragmentShaderName), 0);
	}

	// Create pipeline
	{
		// Add test case specific attributes
		if (m_attribFunc)
			m_attribFunc(*this, numVertices);

		// Add base attributes
		setupDefaultInputs();

		const VkPipelineVertexInputStateCreateInfo		vertexInputStateParams		=
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			(VkPipelineVertexInputStateCreateFlags)0,
			(deUint32)m_vertexBindingDescription.size(),					// deUint32									bindingCount;
			&m_vertexBindingDescription[0],									// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
			(deUint32)m_vertexAttributeDescription.size(),					// deUint32									attributeCount;
			&m_vertexAttributeDescription[0],								// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
		};

		const std::vector<VkViewport>					viewports					(1, makeViewport(m_renderSize));
		const std::vector<VkRect2D>						scissors					(1, makeRect2D(m_renderSize));

		const VkPipelineMultisampleStateCreateInfo		multisampleStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,		// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			0u,																// VkPipelineMultisampleStateCreateFlags	flags;
			m_sampleCount,													// VkSampleCountFlagBits					rasterizationSamples;
			VK_FALSE,														// VkBool32									sampleShadingEnable;
			0.0f,															// float									minSampleShading;
			DE_NULL,														// const VkSampleMask*						pSampleMask;
			VK_FALSE,														// VkBool32									alphaToCoverageEnable;
			VK_FALSE														// VkBool32									alphaToOneEnable;
		};

		graphicsPipeline = makeGraphicsPipeline(vk,							// const DeviceInterface&                        vk
												vkDevice,					// const VkDevice                                device
												*pipelineLayout,			// const VkPipelineLayout                        pipelineLayout
												*vertexShaderModule,		// const VkShaderModule                          vertexShaderModule
												DE_NULL,					// const VkShaderModule                          tessellationControlShaderModule
												DE_NULL,					// const VkShaderModule                          tessellationEvalShaderModule
												DE_NULL,					// const VkShaderModule                          geometryShaderModule
												*fragmentShaderModule,		// const VkShaderModule                          fragmentShaderModule
												*renderPass,				// const VkRenderPass                            renderPass
												viewports,					// const std::vector<VkViewport>&                viewports
												scissors,					// const std::vector<VkRect2D>&                  scissors
												topology,					// const VkPrimitiveTopology                     topology
												0u,							// const deUint32                                subpass
												0u,							// const deUint32                                patchControlPoints
												&vertexInputStateParams,	// const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
												DE_NULL,					// const VkPipelineRasterizationStateCreateInfo* rasterizationStateCreateInfo
												&multisampleStateParams);	// const VkPipelineMultisampleStateCreateInfo*   multisampleStateCreateInfo
	}

	// Create vertex indices buffer
	if (numIndices != 0)
	{
		const VkDeviceSize								indexBufferSize			= numIndices * sizeof(deUint16);
		const VkBufferCreateInfo						indexBufferParams		=
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			0u,											// VkBufferCreateFlags	flags;
			indexBufferSize,							// VkDeviceSize			size;
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT,			// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
			1u,											// deUint32				queueFamilyCount;
			&queueFamilyIndex							// const deUint32*		pQueueFamilyIndices;
		};

		indexBuffer			= createBuffer(vk, vkDevice, &indexBufferParams);
		indexBufferAlloc	= m_memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *indexBuffer), MemoryRequirement::HostVisible);

		VK_CHECK(vk.bindBufferMemory(vkDevice, *indexBuffer, indexBufferAlloc->getMemory(), indexBufferAlloc->getOffset()));

		// Load vertice indices into buffer
		deMemcpy(indexBufferAlloc->getHostPtr(), indices, (size_t)indexBufferSize);
		flushAlloc(vk, vkDevice, *indexBufferAlloc);
	}

	VkCommandPool activeCmdPool;
	if (m_externalCommandPool.get() == DE_NULL)
	{
		// Create local command pool
		cmdPool = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
		activeCmdPool = *cmdPool;
	}
	else
	{
		// Use external command pool if available
		activeCmdPool = m_externalCommandPool.get()->get();
	}

	// Create command buffer
	{
		cmdBuffer = allocateCommandBuffer(vk, vkDevice, activeCmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

		beginCommandBuffer(vk, *cmdBuffer);

		{
			const VkImageMemoryBarrier					imageBarrier				=
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,										// VkStructureType			sType;
				DE_NULL,																	// const void*				pNext;
				0u,																			// VkAccessFlags			srcAccessMask;
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,										// VkAccessFlags			dstAccessMask;
				VK_IMAGE_LAYOUT_UNDEFINED,													// VkImageLayout			oldLayout;
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,									// VkImageLayout			newLayout;
				VK_QUEUE_FAMILY_IGNORED,													// deUint32					srcQueueFamilyIndex;
				VK_QUEUE_FAMILY_IGNORED,													// deUint32					dstQueueFamilyIndex;
				*colorImage,																// VkImage					image;
				{																			// VkImageSubresourceRange	subresourceRange;
					VK_IMAGE_ASPECT_COLOR_BIT,												// VkImageAspectFlags		aspectMask;
					0u,																		// deUint32					baseMipLevel;
					1u,																		// deUint32					mipLevels;
					0u,																		// deUint32					baseArrayLayer;
					1u,																		// deUint32					arraySize;
				}
			};

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, DE_NULL, 1, &imageBarrier);

			if (isMultiSampling()) {
				// add multisample barrier
				const VkImageMemoryBarrier				multiSampleImageBarrier		=
				{
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,										// VkStructureType			sType;
					DE_NULL,																	// const void*				pNext;
					0u,																			// VkAccessFlags			srcAccessMask;
					VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,										// VkAccessFlags			dstAccessMask;
					VK_IMAGE_LAYOUT_UNDEFINED,													// VkImageLayout			oldLayout;
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,									// VkImageLayout			newLayout;
					VK_QUEUE_FAMILY_IGNORED,													// deUint32					srcQueueFamilyIndex;
					VK_QUEUE_FAMILY_IGNORED,													// deUint32					dstQueueFamilyIndex;
					*resolvedImage,																// VkImage					image;
					{																			// VkImageSubresourceRange	subresourceRange;
						VK_IMAGE_ASPECT_COLOR_BIT,												// VkImageAspectFlags		aspectMask;
						0u,																		// deUint32					baseMipLevel;
						1u,																		// deUint32					mipLevels;
						0u,																		// deUint32					baseArrayLayer;
						1u,																		// deUint32					arraySize;
					}
				};

				vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, DE_NULL, 1, &multiSampleImageBarrier);
			}
		}

		beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(0, 0, m_renderSize.x(), m_renderSize.y()), m_clearColor);

		updatePushConstants(*cmdBuffer, *pipelineLayout);
		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline);
		if (!m_uniformInfos.empty())
			vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1, &*descriptorSet, 0u, DE_NULL);

		const deUint32 numberOfVertexAttributes = (deUint32)m_vertexBuffers.size();
		const std::vector<VkDeviceSize> offsets(numberOfVertexAttributes, 0);

		std::vector<VkBuffer> buffers(numberOfVertexAttributes);
		for (size_t i = 0; i < numberOfVertexAttributes; i++)
		{
			buffers[i] = m_vertexBuffers[i].get()->get();
		}

		vk.cmdBindVertexBuffers(*cmdBuffer, 0, numberOfVertexAttributes, &buffers[0], &offsets[0]);
		if (numIndices != 0)
		{
			vk.cmdBindIndexBuffer(*cmdBuffer, *indexBuffer, 0, VK_INDEX_TYPE_UINT16);
			vk.cmdDrawIndexed(*cmdBuffer, numIndices, 1, 0, 0, 0);
		}
		else
			vk.cmdDraw(*cmdBuffer, numVertices,  1, 0, 0);

		endRenderPass(vk, *cmdBuffer);
		endCommandBuffer(vk, *cmdBuffer);
	}

	// Execute Draw
	submitCommandsAndWait(vk, vkDevice, queue, cmdBuffer.get());

	// Read back the result
	{
		const tcu::TextureFormat						resultFormat				= mapVkFormat(m_colorFormat);
		const VkDeviceSize								imageSizeBytes				= (VkDeviceSize)(resultFormat.getPixelSize() * m_renderSize.x() * m_renderSize.y());
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
		const Move<VkCommandBuffer>						resultCmdBuffer				= allocateCommandBuffer(vk, vkDevice, activeCmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

		beginCommandBuffer(vk, *resultCmdBuffer);

		copyImageToBuffer(vk, *resultCmdBuffer, isMultiSampling() ? *resolvedImage : *colorImage, *readImageBuffer, tcu::IVec2(m_renderSize.x(), m_renderSize.y()));

		endCommandBuffer(vk, *resultCmdBuffer);

		submitCommandsAndWait(vk, vkDevice, queue, resultCmdBuffer.get());

		invalidateAlloc(vk, vkDevice, *readImageBufferMemory);

		const tcu::ConstPixelBufferAccess				resultAccess				(resultFormat, m_renderSize.x(), m_renderSize.y(), 1, readImageBufferMemory->getHostPtr());

		m_resultImage.setStorage(resultFormat, m_renderSize.x(), m_renderSize.y());
		tcu::copy(m_resultImage.getAccess(), resultAccess);
	}
}

void ShaderRenderCaseInstance::computeVertexReference (tcu::Surface& result, const QuadGrid& quadGrid)
{
	DE_ASSERT(m_evaluator);

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
		m_evaluator->evaluate(evalCtx);
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
	DE_ASSERT(m_evaluator);

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
		m_evaluator->evaluate(evalCtx);
		// Select either clear color or computed color based on discarded bit.
		tcu::Vec4 color = evalCtx.isDiscarded ? m_clearColor : evalCtx.color;

		if (!hasAlpha)
			color.w() = 1.0f;

		result.setPixel(x, y, tcu::RGBA(color));
	}
}

bool ShaderRenderCaseInstance::compareImages (const tcu::Surface& resImage, const tcu::Surface& refImage, float errorThreshold)
{
	if (m_fuzzyCompare)
		return tcu::fuzzyCompare(m_context.getTestContext().getLog(), "ComparisonResult", "Image comparison result", refImage, resImage, errorThreshold, tcu::COMPARE_LOG_EVERYTHING);
	else
		return tcu::pixelThresholdCompare(m_context.getTestContext().getLog(), "ComparisonResult", "Image comparison result", refImage, resImage, tcu::RGBA(1, 1, 1, 1), tcu::COMPARE_LOG_EVERYTHING);
}

} // sr
} // vkt
