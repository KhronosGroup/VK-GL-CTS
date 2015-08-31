/*------------------------------------------------------------------------
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be included
 * in all copies or substantial portions of the Materials.
 *
 * The Materials are Confidential Information as defined by the
 * Khronos Membership Agreement until designated non-confidential by Khronos,
 * at which point this condition clause shall be removed.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
 *
 *//*!
 * \file
 * \brief Vulkan ShaderRenderCase
 *//*--------------------------------------------------------------------*/

#include "vktShaderRenderCase.hpp"

#include "tcuImageCompare.hpp"
#include "tcuSurface.hpp"
#include "tcuVector.hpp"
#include "tcuTestLog.hpp"

#include "deMath.h"
#include "deUniquePtr.hpp"

#include "vkPlatform.hpp"
#include "vkStrUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkDeviceUtil.hpp"

#include <vector>
#include <string>

namespace vkt
{
namespace shaderrendercase
{

using namespace std;
using namespace tcu;
using namespace vk;

static const int		GRID_SIZE			= 2;
static const int		MAX_RENDER_WIDTH	= 128;
static const int		MAX_RENDER_HEIGHT	= 112;
static const tcu::Vec4	DEFAULT_CLEAR_COLOR	= tcu::Vec4(0.125f, 0.25f, 0.5f, 1.0f);

// QuadGrid.

class QuadGrid
{
public:
                            QuadGrid                (int gridSize, int screenWidth, int screenHeight, const Vec4& constCoords, const vector<Mat4>& userAttribTransforms
/*, const vector<TextureBinding>& textures*/);
                            ~QuadGrid               (void);

    int                     getGridSize             (void) const { return m_gridSize; }
    int                     getNumVertices          (void) const { return m_numVertices; }
    int                     getNumTriangles         (void) const { return m_numTriangles; }
    const Vec4&             getConstCoords          (void) const { return m_constCoords; }
    const vector<Mat4>      getUserAttribTransforms (void) const { return m_userAttribTransforms; }
	// TODO:
    //const vector<TextureBinding>&   getTextures     (void) const { return m_textures; }

	const Vec4*				getPositions			(void) const { return &m_positions[0]; }
	const float*			getAttribOne			(void) const { return &m_attribOne[0]; }
	const Vec4*				getCoords				(void) const { return &m_coords[0]; }
	const Vec4*				getUnitCoords			(void) const { return &m_unitCoords[0]; }

    const Vec4*             getUserAttrib           (int attribNdx) const { return &m_userAttribs[attribNdx][0]; }
    const deUint16*         getIndices              (void) const { return &m_indices[0]; }

    Vec4                    getCoords               (float sx, float sy) const;
    Vec4                    getUnitCoords           (float sx, float sy) const;

    int                     getNumUserAttribs       (void) const { return (int)m_userAttribTransforms.size(); }
    Vec4                    getUserAttrib           (int attribNdx, float sx, float sy) const;

private:
    int                     m_gridSize;
    int                     m_numVertices;
    int                     m_numTriangles;
    Vec4                    m_constCoords;
    vector<Mat4>            m_userAttribTransforms;
	// TODO:
    // vector<TextureBinding>  m_textures;

    vector<Vec4>            m_screenPos;
	vector<Vec4>                    m_positions;
	vector<Vec4>                    m_coords;                       //!< Near-unit coordinates, roughly [-2.0 .. 2.0].
	vector<Vec4>                    m_unitCoords;           //!< Positive-only coordinates [0.0 .. 1.5].
	vector<float>                   m_attribOne;
    vector<Vec4>            m_userAttribs[ShaderEvalContext::MAX_TEXTURES];
    vector<deUint16>        m_indices;
};

QuadGrid::QuadGrid (int gridSize, int width, int height, const Vec4& constCoords, const vector<Mat4>& userAttribTransforms
/*, const vector<TextureBinding>& textures*/)
    : m_gridSize                (gridSize)
    , m_numVertices             ((gridSize + 1) * (gridSize + 1))
    , m_numTriangles            (gridSize * gridSize * 2)
    , m_constCoords             (constCoords)
    , m_userAttribTransforms    (userAttribTransforms)
//    , m_textures                (textures)
{
    Vec4 viewportScale = Vec4((float)width, (float)height, 0.0f, 0.0f);

    // Compute vertices.
    m_screenPos.resize(m_numVertices);
	m_positions.resize(m_numVertices);
	m_coords.resize(m_numVertices);
	m_unitCoords.resize(m_numVertices);
	m_attribOne.resize(m_numVertices);

    // User attributes.
    for (int i = 0; i < DE_LENGTH_OF_ARRAY(m_userAttribs); i++)
        m_userAttribs[i].resize(m_numVertices);

    for (int y = 0; y < gridSize+1; y++)
    for (int x = 0; x < gridSize+1; x++)
    {
        float               sx          = (float)x / (float)gridSize;
        float               sy          = (float)y / (float)gridSize;
        float               fx          = 2.0f * sx - 1.0f;
        float               fy          = 2.0f * sy - 1.0f;
        int                 vtxNdx      = ((y * (gridSize+1)) + x);

		m_positions[vtxNdx] 	= Vec4(fx, fy, 0.0f, 1.0f);
		m_coords[vtxNdx] 		= getCoords(sx, sy);
		m_unitCoords[vtxNdx]	= getUnitCoords(sx, sy);
		m_attribOne[vtxNdx] 	= 1.0f;

        m_screenPos[vtxNdx]     = Vec4(sx, sy, 0.0f, 1.0f) * viewportScale;

        for (int attribNdx = 0; attribNdx < getNumUserAttribs(); attribNdx++)
            m_userAttribs[attribNdx][vtxNdx] = getUserAttrib(attribNdx, sx, sy);
    }

    // Compute indices.
    m_indices.resize(3 * m_numTriangles);
    for (int y = 0; y < gridSize; y++)
    for (int x = 0; x < gridSize; x++)
    {
        int stride = gridSize + 1;
        int v00 = (y * stride) + x;
        int v01 = (y * stride) + x + 1;
        int v10 = ((y+1) * stride) + x;
        int v11 = ((y+1) * stride) + x + 1;

        int baseNdx = ((y * gridSize) + x) * 6;
        m_indices[baseNdx + 0] = (deUint16)v10;
        m_indices[baseNdx + 1] = (deUint16)v00;
        m_indices[baseNdx + 2] = (deUint16)v01;

        m_indices[baseNdx + 3] = (deUint16)v10;
        m_indices[baseNdx + 4] = (deUint16)v01;
        m_indices[baseNdx + 5] = (deUint16)v11;
    }
}

QuadGrid::~QuadGrid (void)
{
}

inline Vec4 QuadGrid::getCoords (float sx, float sy) const
{
    float fx = 2.0f * sx - 1.0f;
    float fy = 2.0f * sy - 1.0f;
    return Vec4(fx, fy, -fx + 0.33f*fy, -0.275f*fx - fy);
}

inline Vec4 QuadGrid::getUnitCoords (float sx, float sy) const
{
    return Vec4(sx, sy, 0.33f*sx + 0.5f*sy, 0.5f*sx + 0.25f*sy);
}

inline Vec4 QuadGrid::getUserAttrib (int attribNdx, float sx, float sy) const
{
    // homogeneous normalized screen-space coordinates
    return m_userAttribTransforms[attribNdx] * Vec4(sx, sy, 0.0f, 1.0f);
}



// ShaderEvalContext.

ShaderEvalContext::ShaderEvalContext (const QuadGrid& quadGrid_)
	: constCoords(quadGrid_.getConstCoords())
	, isDiscarded(false)
	, quadGrid(quadGrid_)
{
	// TODO...
/*
    const vector<TextureBinding>& bindings = quadGrid.getTextures();
    DE_ASSERT((int)bindings.size() <= MAX_TEXTURES);

    // Fill in texture array.
    for (int ndx = 0; ndx < (int)bindings.size(); ndx++)
    {
        const TextureBinding& binding = bindings[ndx];

        if (binding.getType() == TextureBinding::TYPE_NONE)
            continue;

        textures[ndx].sampler = binding.getSampler();

        switch (binding.getType())
        {
            case TextureBinding::TYPE_2D:       textures[ndx].tex2D         = &binding.get2D()->getRefTexture();        break;
            case TextureBinding::TYPE_CUBE_MAP: textures[ndx].texCube       = &binding.getCube()->getRefTexture();      break;
            case TextureBinding::TYPE_2D_ARRAY: textures[ndx].tex2DArray    = &binding.get2DArray()->getRefTexture();   break;
            case TextureBinding::TYPE_3D:       textures[ndx].tex3D         = &binding.get3D()->getRefTexture();        break;
            default:
                DE_ASSERT(DE_FALSE);
        }
    }
*/
}

ShaderEvalContext::~ShaderEvalContext (void)
{
}

void ShaderEvalContext::reset (float sx, float sy)
{
    // Clear old values
    color       = Vec4(0.0f, 0.0f, 0.0f, 1.0f);
    isDiscarded = false;

    // Compute coords
    coords      = quadGrid.getCoords(sx, sy);
    unitCoords  = quadGrid.getUnitCoords(sx, sy);

    // Compute user attributes.
    int numAttribs = quadGrid.getNumUserAttribs();
    DE_ASSERT(numAttribs <= MAX_USER_ATTRIBS);
    for (int attribNdx = 0; attribNdx < numAttribs; attribNdx++)
        in[attribNdx] = quadGrid.getUserAttrib(attribNdx, sx, sy);
}

tcu::Vec4 ShaderEvalContext::texture2D (int /* unitNdx */, const tcu::Vec2& /* texCoords */)
{
	// TODO: add texture binding
/*    if (textures[unitNdx].tex2D)
        return textures[unitNdx].tex2D->sample(textures[unitNdx].sampler, texCoords.x(), texCoords.y(), 0.0f);
    else
*/
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

void ShaderEvaluator::evaluate (ShaderEvalContext& ctx)
{
    DE_ASSERT(m_evalFunc);
    m_evalFunc(ctx);
}

// ShaderRenderCaseInstance.

ShaderRenderCaseInstance::ShaderRenderCaseInstance (Context& context, const string& name, bool isVertexCase, ShaderEvaluator& evaluator, UniformSetupFunc uniformFunc, AttributeSetupFunc attribFunc)
	: vkt::TestInstance(context)
	, m_clearColor(DEFAULT_CLEAR_COLOR)
	, memAlloc(m_context.getDeviceInterface(), m_context.getDevice(), getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()))
	, m_name(name)
	, m_isVertexCase(isVertexCase)
	, m_evaluator(evaluator)
	, m_uniformFunc(uniformFunc)
	, m_attribFunc(attribFunc)
	, m_renderSize(100, 100)
	, m_colorFormat(VK_FORMAT_R8G8B8A8_UNORM)
{
}

ShaderRenderCaseInstance::~ShaderRenderCaseInstance (void)
{
}

tcu::TestStatus ShaderRenderCaseInstance::iterate (void)
{
	// Create quad grid.
	IVec2	viewportSize	= getViewportSize();
	int		width			= viewportSize.x();
	int 	height			= viewportSize.y();

	QuadGrid quadGrid(m_isVertexCase ? GRID_SIZE : 4, width, height, Vec4(0.125f, 0.25f, 0.5f, 1.0f), m_userAttribTransforms/*, m_textures*/);

	// Render result.
	Surface resImage(width, height);
	render(resImage, quadGrid);

	// Compute reference.
	Surface refImage(width, height);
	if (m_isVertexCase)
		computeVertexReference(refImage, quadGrid);
	else
		computeFragmentReference(refImage, quadGrid);

	// Compare.
	bool compareOk = compareImages(resImage, refImage, 0.05f);

	if (compareOk)
		return tcu::TestStatus::pass("Result image matches reference");
	else
		return tcu::TestStatus::fail("Image mismatch");
}

void ShaderRenderCaseInstance::setupUniformData (deUint32 size, void* dataPtr)
{
	const VkDevice				vkDevice			= m_context.getDevice();
	const DeviceInterface&		vk					= m_context.getDeviceInterface();
	const deUint32				queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();

	const VkBufferCreateInfo uniformBufferParams =
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
		DE_NULL,									// const void*			pNext;
		size,										// VkDeviceSize			size;
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,			// VkBufferUsageFlags	usage;
		0u,											// VkBufferCreateFlags	flags;
		VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
		1u,											// deUint32				queueFamilyCount;
		&queueFamilyIndex							// const deUint32*		pQueueFamilyIndices;
	};

	Move<VkBuffer> buffer = createBuffer(vk, vkDevice, &uniformBufferParams);
	de::MovePtr<Allocation> alloc = memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *buffer), MemoryRequirement::HostVisible);
	VK_CHECK(vk.bindBufferMemory(vkDevice, *buffer, alloc->getMemory(), 0));

	void* bufferPtr;
	VK_CHECK(vk.mapMemory(vkDevice, alloc->getMemory(), 0, size, 0, &bufferPtr));
	deMemcpy(bufferPtr, dataPtr, size);
	VK_CHECK(vk.unmapMemory(vkDevice, alloc->getMemory()));

	const VkBufferViewCreateInfo viewInfo =
	{
		VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO, 	// VkStructureType	sType;
		DE_NULL,									// void* 			pNext;
		*buffer,									// VkBuffer			buffer;
		VK_BUFFER_VIEW_TYPE_FORMATTED,				// VkBufferViewType	viewType;
		VK_FORMAT_R32_SFLOAT,						// VkFormat	format;
		0u,											// VkDeviceSize	offset;
		size										// VkDeviceSize	range;
	};

	m_uniformBufferViews.push_back(createBufferView(vk, vkDevice, &viewInfo).disown());

	m_uniformBuffers.push_back(buffer.disown());
	m_uniformBufferAllocs.push_back(alloc.release());
}


void ShaderRenderCaseInstance::addAttribute (deUint32 bindingLocation, vk::VkFormat format, deUint32 sizePerElement, deUint32 count, const void* dataPtr)
{
	// Add binding specification
	const deUint32 binding = (deUint32)m_vertexBindingDescription.size();
	const VkVertexInputBindingDescription bindingDescription =
	{
		binding,
		sizePerElement,
		VK_VERTEX_INPUT_STEP_RATE_VERTEX
	};

	m_vertexBindingDescription.push_back(bindingDescription);

	// Add location and format specification
	const VkVertexInputAttributeDescription attributeDescription =
	{
		bindingLocation,			// deUint32	location;
		binding,					// deUint32	binding;
		format,						// VkFormat	format;
		0u,							// deUint32	offsetInBytes;
	};

	m_vertexattributeDescription.push_back(attributeDescription);

	// Upload data to buffer
	const VkDevice				vkDevice			= m_context.getDevice();
	const DeviceInterface&		vk					= m_context.getDeviceInterface();
	const deUint32				queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();

	const VkDeviceSize inputSize = sizePerElement * count;
	const VkBufferCreateInfo vertexBufferParams =
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
		DE_NULL,									// const void*			pNext;
		inputSize,									// VkDeviceSize			size;
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,			// VkBufferUsageFlags	usage;
		0u,											// VkBufferCreateFlags	flags;
		VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
		1u,											// deUint32				queueFamilyCount;
		&queueFamilyIndex							// const deUint32*		pQueueFamilyIndices;
	};

	Move<VkBuffer> buffer = createBuffer(vk, vkDevice, &vertexBufferParams);
	de::MovePtr<vk::Allocation> alloc = memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *buffer), MemoryRequirement::HostVisible);

	VK_CHECK(vk.bindBufferMemory(vkDevice, *buffer, alloc->getMemory(), 0));

	void* bufferPtr;
	VK_CHECK(vk.mapMemory(vkDevice, alloc->getMemory(), 0, inputSize, 0, &bufferPtr));
	deMemcpy(bufferPtr, dataPtr, inputSize);
	VK_CHECK(vk.unmapMemory(vkDevice, alloc->getMemory()));

	m_vertexBuffers.push_back(buffer.disown());
	m_vertexBufferAllocs.push_back(alloc.release());
}

void ShaderRenderCaseInstance::addUniform (deUint32 bindingLocation, VkDescriptorType descriptorType, float data)
{
	m_descriptorSetLayoutBuilder.addSingleBinding(descriptorType, VK_SHADER_STAGE_VERTEX_BIT);
	m_descriptorPoolBuilder.addType(descriptorType);

	setupUniformData(sizeof(data), &data);

	const VkDescriptorInfo view =
	{
		m_uniformBufferViews[m_uniformBufferViews.size() - 1],											// VkBufferView		bufferView;
		0,											// VkSampler		sampler;
		0,											// VkImageView		imageView;
		0,											// VkAttachmentView	attachmentView;
		(VkImageLayout)0,							// VkImageLayout	imageLayout;
	};

	m_uniformDescriptorInfos.push_back(view);

	m_uniformLocations.push_back(bindingLocation);
}

void ShaderRenderCaseInstance::addUniform (deUint32 bindingLocation, VkDescriptorType descriptorType, tcu::Vec4 data)
{
	m_descriptorSetLayoutBuilder.addSingleBinding(descriptorType, VK_SHADER_STAGE_VERTEX_BIT);
	m_descriptorPoolBuilder.addType(descriptorType);

	setupUniformData(sizeof(data), &data[0]);

	const VkDescriptorInfo view =
	{
		m_uniformBufferViews[m_uniformBufferViews.size() - 1],											// VkBufferView		bufferView;
		0,											// VkSampler		sampler;
		0,											// VkImageView		imageView;
		0,											// VkAttachmentView	attachmentView;
		(VkImageLayout)0,							// VkImageLayout	imageLayout;
	};

	m_uniformDescriptorInfos.push_back(view);

	m_uniformLocations.push_back(bindingLocation);
}


void ShaderRenderCaseInstance::setupShaderData (void)
{
	// TODO!!!
}

void ShaderRenderCaseInstance::setup (void)
{
	// TODO!!
}

void ShaderRenderCaseInstance::setupUniforms (const Vec4& constCoords)
{
	// TODO!!
	DE_UNREF(constCoords);
	m_uniformFunc(*this);
}

tcu::IVec2 ShaderRenderCaseInstance::getViewportSize (void) const
{
	return tcu::IVec2(de::min(m_renderSize.x(), MAX_RENDER_WIDTH),
					  de::min(m_renderSize.y(), MAX_RENDER_HEIGHT));
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
	addAttribute(0u, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(Vec4), quadGrid.getNumVertices(), quadGrid.getPositions());
	addAttribute(1u, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(Vec4), quadGrid.getNumVertices(), quadGrid.getCoords());
	addAttribute(2u, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(Vec4), quadGrid.getNumVertices(), quadGrid.getUnitCoords());
	addAttribute(3u, VK_FORMAT_R32_SFLOAT, sizeof(float), quadGrid.getNumVertices(), quadGrid.getAttribOne());
}

void ShaderRenderCaseInstance::render (Surface& result, const QuadGrid& quadGrid)
{
	// TODO!! Vk rendering
	const VkDevice				vkDevice			= m_context.getDevice();
	const DeviceInterface&		vk					= m_context.getDeviceInterface();
	const VkQueue				queue				= m_context.getUniversalQueue();
	const deUint32				queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	//SimpleAllocator				memAlloc			(vk, vkDevice, getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));

	// Create color image
	{
		const VkImageCreateInfo		colorImageParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,										// VkStructureType		sType;
			DE_NULL,																	// const void*			pNext;
			VK_IMAGE_TYPE_2D,															// VkImageType			imageType;
			m_colorFormat,																// VkFormat				format;
			{ m_renderSize.x(), m_renderSize.y(), 1u },									// VkExtent3D			extent;
			1u,																			// deUint32				mipLevels;
			1u,																			// deUint32				arraySize;
			1u,																			// deUint32				samples;
			VK_IMAGE_TILING_OPTIMAL,													// VkImageTiling		tiling;
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SOURCE_BIT,	// VkImageUsageFlags	usage;
			0u,																			// VkImageCreateFlags	flags;
			VK_SHARING_MODE_EXCLUSIVE,													// VkSharingMode		sharingMode;
			1u,																			// deUint32				queueFamilyCount;
			&queueFamilyIndex															// const deUint32*		pQueueFamilyIndices;
		};

		m_colorImage			= createImage(vk, vkDevice, &colorImageParams);

		// Allocate and bind color image memory
		m_colorImageAlloc		= memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_colorImage), MemoryRequirement::HostVisible);
		VK_CHECK(vk.bindImageMemory(vkDevice, *m_colorImage, m_colorImageAlloc->getMemory(), 0));
	}

	// Create color attachment view
	{
		const VkAttachmentViewCreateInfo colorAttachmentViewParams =
		{
			VK_STRUCTURE_TYPE_ATTACHMENT_VIEW_CREATE_INFO,		// VkStructureType				sType;
			DE_NULL,											// constvoid*					pNext;
			*m_colorImage,										// VkImage						image;
			m_colorFormat,										// VkFormat						format;
			0u,													// deUint32						mipLevel;
			0u,													// deUint32						baseArraySlice;
			1u,													// deUint32						arraySize;
			0u													// VkAttachmentViewCreateFlags	flags;
		};

		m_colorAttachmentView = createAttachmentView(vk, vkDevice, &colorAttachmentViewParams);
	}

	// Create render pass
	{
		const VkAttachmentDescription colorAttachmentDescription =
		{
			VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION,			// VkStructureType		sType;
			DE_NULL,											// const void*			pNext;
			m_colorFormat,										// VkFormat				format;
			1u,													// deUint32				samples;
			VK_ATTACHMENT_LOAD_OP_CLEAR,						// VkAttachmentLoadOp	loadOp;
			VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp	storeOp;
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,					// VkAttachmentLoadOp	stencilLoadOp;
			VK_ATTACHMENT_STORE_OP_DONT_CARE,					// VkAttachmentStoreOp	stencilStoreOp;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,			// VkImageLayout		initialLayout;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL			// VkImageLayout		finalLayout;
		};

		const VkAttachmentDescription attachments[1] =
		{
			colorAttachmentDescription
		};

		const VkAttachmentReference colorAttachmentReference =
		{
			0u,													// deUint32			attachment;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL			// VkImageLayout	layout;
		};

		const VkSubpassDescription subpassDescription =
		{
			VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION,				// VkStructureType				sType;
			DE_NULL,											// constvoid*					pNext;
			VK_PIPELINE_BIND_POINT_GRAPHICS,					// VkPipelineBindPoint			pipelineBindPoint;
			0u,													// VkSubpassDescriptionFlags	flags;
			0u,													// deUint32						inputCount;
			DE_NULL,											// constVkAttachmentReference*	inputAttachments;
			1u,													// deUint32						colorCount;
			&colorAttachmentReference,							// constVkAttachmentReference*	colorAttachments;
			DE_NULL,											// constVkAttachmentReference*	resolveAttachments;
			{ ~0u, VK_IMAGE_LAYOUT_GENERAL },					// VkAttachmentReference		depthStencilAttachment;
			0u,													// deUint32						preserveCount;
			DE_NULL												// constVkAttachmentReference*	preserveAttachments;
		};

		const VkRenderPassCreateInfo renderPassParams =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,			// VkStructureType					sType;
			DE_NULL,											// const void*						pNext;
			1u,													// deUint32							attachmentCount;
			attachments,										// const VkAttachmentDescription*	pAttachments;
			1u,													// deUint32							subpassCount;
			&subpassDescription,								// const VkSubpassDescription*		pSubpasses;
			0u,													// deUint32							dependencyCount;
			DE_NULL												// const VkSubpassDependency*		pDependencies;
		};

		m_renderPass = createRenderPass(vk, vkDevice, &renderPassParams);
	}

	// Create framebuffer
	{
		const VkAttachmentBindInfo attachmentBindInfos[1] =
		{
			{ *m_colorAttachmentView, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL  },
		};

		const VkFramebufferCreateInfo framebufferParams =
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,			// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			*m_renderPass,										// VkRenderPass					renderPass;
			1u,													// deUint32						attachmentCount;
			attachmentBindInfos,								// const VkAttachmentBindInfo*	pAttachments;
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
		m_descriptorPool = m_descriptorPoolBuilder.build(vk, vkDevice, VK_DESCRIPTOR_POOL_USAGE_ONE_SHOT, 1u);
		m_descriptorSet = allocDescriptorSet(vk, vkDevice, *m_descriptorPool, VK_DESCRIPTOR_SET_USAGE_STATIC, *m_descriptorSetLayout);

		for(deUint32 i = 0; i < m_uniformDescriptorInfos.size(); i++)
		{
			m_descriptorSetUpdateBuilder.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(m_uniformLocations[i]), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &m_uniformDescriptorInfos[i]);
		}

		m_descriptorSetUpdateBuilder.update(vk, vkDevice);
	}

	// Create pipeline layout
	{
		// TODO:: Connect uniforms here?
		const VkPipelineLayoutCreateInfo pipelineLayoutParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			1u,													// deUint32						descriptorSetCount;
			&*m_descriptorSetLayout,							// const VkDescriptorSetLayout*	pSetLayouts;
			0u,													// deUint32						pushConstantRangeCount;
			DE_NULL												// const VkPushConstantRange*	pPushConstantRanges;
		};

		m_pipelineLayout = createPipelineLayout(vk, vkDevice, &pipelineLayoutParams);
	}

	// Create shaders
	{
		m_vertexShaderModule	= createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get(m_name + "_vert"), 0);
		m_fragmentShaderModule	= createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get(m_name + "_frag"), 0);

		const VkShaderCreateInfo vertexShaderParams =
		{
			VK_STRUCTURE_TYPE_SHADER_CREATE_INFO,			// VkStructureType		sType;
			DE_NULL,										// const void*			pNext;
			*m_vertexShaderModule,							// VkShaderModule		module;
			"main",											// const char*			pName;
			0u												// VkShaderCreateFlags	flags;
		};

		const VkShaderCreateInfo fragmentShaderParams =
		{
			VK_STRUCTURE_TYPE_SHADER_CREATE_INFO,			// VkStructureType		sType;
			DE_NULL,										// const void*			pNext;
			*m_fragmentShaderModule,						// VkShaderModule		module;
			"main",											// const char*			pName;
			0u												// VkShaderCreateFlags	flags;
		};

		m_vertexShader		= createShader(vk, vkDevice, &vertexShaderParams);
		m_fragmentShader	= createShader(vk, vkDevice, &fragmentShaderParams);
	}

	// Create pipeline
	{
		const VkPipelineShaderStageCreateInfo shaderStageParams[2] =
		{
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,		// VkStructureType				sType;
				DE_NULL,													// const void*					pNext;
				VK_SHADER_STAGE_VERTEX,										// VkShaderStage				stage;
				*m_vertexShader,											// VkShader						shader;
				DE_NULL														// const VkSpecializationInfo*	pSpecializationInfo;
			},
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,		// VkStructureType				sType;
				DE_NULL,													// const void*					pNext;
				VK_SHADER_STAGE_FRAGMENT,									// VkShaderStage				stage;
				*m_fragmentShader,											// VkShader						shader;
				DE_NULL														// const VkSpecializationInfo*	pSpecializationInfo;
			}
		};

		// Add base attributes
		setupDefaultInputs(quadGrid);

		// Add test case specific attributes
		if (m_attribFunc)
			m_attribFunc(*this, quadGrid.getNumVertices());


		const VkPipelineVertexInputStateCreateInfo vertexInputStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			(deUint32)m_vertexBindingDescription.size(),					// deUint32									bindingCount;
			&m_vertexBindingDescription[0],									// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
			(deUint32)m_vertexattributeDescription.size(),					// deUint32									attributeCount;
			&m_vertexattributeDescription[0],								// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
		};

		const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,														// const void*			pNext;
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,							// VkPrimitiveTopology	topology;
			false															// VkBool32				primitiveRestartEnable;
		};

		const VkPipelineViewportStateCreateInfo viewportStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,			// VkStructureType	sType;
			DE_NULL,														// const void*		pNext;
			1u																// deUint32			viewportCount;
		};

		const VkPipelineRasterStateCreateInfo rasterStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_RASTER_STATE_CREATE_INFO,			// VkStructureType	sType;
			DE_NULL,														// const void*		pNext;
			false,															// VkBool32			depthClipEnable;
			false,															// VkBool32			rasterizerDiscardEnable;
			VK_FILL_MODE_SOLID,												// VkFillMode		fillMode;
			VK_CULL_MODE_NONE,												// VkCullMode		cullMode;
			VK_FRONT_FACE_CCW												// VkFrontFace		frontFace;
		};

		const VkPipelineColorBlendAttachmentState colorBlendAttachmentState =
		{
			false,																		// VkBool32			blendEnable;
			VK_BLEND_ONE,																// VkBlend			srcBlendColor;
			VK_BLEND_ZERO,																// VkBlend			destBlendColor;
			VK_BLEND_OP_ADD,															// VkBlendOp		blendOpColor;
			VK_BLEND_ONE,																// VkBlend			srcBlendAlpha;
			VK_BLEND_ZERO,																// VkBlend			destBlendAlpha;
			VK_BLEND_OP_ADD,															// VkBlendOp		blendOpAlpha;
			VK_CHANNEL_R_BIT | VK_CHANNEL_G_BIT | VK_CHANNEL_B_BIT | VK_CHANNEL_A_BIT	// VkChannelFlags	channelWriteMask;
		};

		const VkPipelineColorBlendStateCreateInfo colorBlendStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType								sType;
			DE_NULL,													// const void*									pNext;
			false,														// VkBool32										alphaToCoverageEnable;
			false,														// VkBool32										logicOpEnable;
			VK_LOGIC_OP_COPY,											// VkLogicOp									logicOp;
			1u,															// deUint32										attachmentCount;
			&colorBlendAttachmentState									// const VkPipelineColorBlendAttachmentState*	pAttachments;
		};

		const VkGraphicsPipelineCreateInfo graphicsPipelineParams =
		{
			VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,	// VkStructureType									sType;
			DE_NULL,											// const void*										pNext;
			2u,													// deUint32											stageCount;
			shaderStageParams,									// const VkPipelineShaderStageCreateInfo*			pStages;
			&vertexInputStateParams,							// const VkPipelineVertexInputStateCreateInfo*		pVertexInputState;
			&inputAssemblyStateParams,							// const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState;
			DE_NULL,											// const VkPipelineTessellationStateCreateInfo*		pTessellationState;
			&viewportStateParams,								// const VkPipelineViewportStateCreateInfo*			pViewportState;
			&rasterStateParams,									// const VkPipelineRasterStateCreateInfo*			pRasterState;
			//&multisampleStateParams,							// const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
			DE_NULL,
			DE_NULL,											// const VkPipelineDepthStencilStateCreateInfo*		pDepthStencilState;
			&colorBlendStateParams,								// const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
			0u,													// VkPipelineCreateFlags							flags;
			*m_pipelineLayout,									// VkPipelineLayout									layout;
			*m_renderPass,										// VkRenderPass										renderPass;
			0u,													// deUint32											subpass;
			0u,													// VkPipeline										basePipelineHandle;
			0u													// deInt32											basePipelineIndex;
		};

		m_graphicsPipeline = createGraphicsPipeline(vk, vkDevice, DE_NULL, &graphicsPipelineParams);
	}

	// Create dynamic states
	{
		const VkViewport viewport =
		{
			0.0f,						// float	originX;
			0.0f,						// float	originY;
			(float)m_renderSize.x(),	// float	width;
			(float)m_renderSize.y(),	// float	height;
			0.0f,						// float	minDepth;
			1.0f						// float	maxDepth;
		};

		const VkRect2D scissor =
		{
			0,
			0,
			m_renderSize.x(),
			m_renderSize.y()
		};

		const VkDynamicViewportStateCreateInfo viewportStateParams =
		{
			VK_STRUCTURE_TYPE_DYNAMIC_VIEWPORT_STATE_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,												// const void*			pNext;
			1,														// deUint32				viewportAndScissorCount;
			&viewport,												// const VkViewport*	pViewports;
			&scissor												// const VkRect2D*		pScissors;
		};

		const VkDynamicRasterStateCreateInfo rasterStateParams =
		{
			VK_STRUCTURE_TYPE_DYNAMIC_RASTER_STATE_CREATE_INFO,		// VkStructureType	sType;
			DE_NULL,												// const void*		pNext;
			0.0f,													// float			depthBias;
			0.0f,													// float			depthBiasClamp;
			0.0f,													// float			slopeScaledDepthBias;
			1.0f,													// float			lineWidth;
		};


		const VkDynamicColorBlendStateCreateInfo colorBlendStateParams =
		{
			VK_STRUCTURE_TYPE_DYNAMIC_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType	sType;
			DE_NULL,													// const void*		pNext;
			{ 0.0f, 0.0f, 0.0f, 0.0f }									// float			blendConst[4];
		};

		m_viewportState		= createDynamicViewportState(vk, vkDevice, &viewportStateParams);
		m_rasterState		= createDynamicRasterState(vk, vkDevice, &rasterStateParams);
		m_colorBlendState	= createDynamicColorBlendState(vk, vkDevice, &colorBlendStateParams);
	}

	// Create vertex indices buffer
	{
		const VkDeviceSize indiceBufferSize = quadGrid.getNumTriangles() * 3 * sizeof(deUint16);
		const VkBufferCreateInfo indiceBufferParams =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			indiceBufferSize,							// VkDeviceSize			size;
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT,			// VkBufferUsageFlags	usage;
			0u,											// VkBufferCreateFlags	flags;
			VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
			1u,											// deUint32				queueFamilyCount;
			&queueFamilyIndex							// const deUint32*		pQueueFamilyIndices;
		};

		m_indiceBuffer		= createBuffer(vk, vkDevice, &indiceBufferParams);
		m_indiceBufferAlloc	= memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_indiceBuffer), MemoryRequirement::HostVisible);

		VK_CHECK(vk.bindBufferMemory(vkDevice, *m_indiceBuffer, m_indiceBufferAlloc->getMemory(), 0));

		// Load vertice indices into buffer
		void* bufferPtr;
		VK_CHECK(vk.mapMemory(vkDevice, m_indiceBufferAlloc->getMemory(), 0, indiceBufferSize, 0, &bufferPtr));
		deMemcpy(bufferPtr, quadGrid.getIndices(), indiceBufferSize);
		VK_CHECK(vk.unmapMemory(vkDevice, m_indiceBufferAlloc->getMemory()));
	}

	// Create command pool
	{
		const VkCmdPoolCreateInfo cmdPoolParams =
		{
			VK_STRUCTURE_TYPE_CMD_POOL_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			queueFamilyIndex,							// deUint32				queueFamilyIndex;
			VK_CMD_POOL_CREATE_TRANSIENT_BIT			// VkCmdPoolCreateFlags	flags;
		};

		m_cmdPool = createCommandPool(vk, vkDevice, &cmdPoolParams);
	}

	// Create command buffer
	{
		const VkCmdBufferCreateInfo cmdBufferParams =
		{
			VK_STRUCTURE_TYPE_CMD_BUFFER_CREATE_INFO,	// VkStructureType			sType;
			DE_NULL,									// const void*				pNext;
			*m_cmdPool,									// VkCmdPool				cmdPool;
			VK_CMD_BUFFER_LEVEL_PRIMARY,				// VkCmdBufferLevel			level;
			0u											// VkCmdBufferCreateFlags	flags;
		};

		const VkCmdBufferBeginInfo cmdBufferBeginInfo =
		{
			VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO,	// VkStructureType			sType;
			DE_NULL,									// const void*				pNext;
			0u,											// VkCmdBufferOptimizeFlags	flags;
			DE_NULL,									// VkRenderPass				renderPass;
			DE_NULL										// VkFramebuffer			framebuffer;
		};

		const VkClearValue attachmentClearValues[1] =
		{
			{ m_clearColor.x(), m_clearColor.y(), m_clearColor.z(), m_clearColor.w() }
		};

		const VkRenderPassBeginInfo renderPassBeginInfo =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO	,	// VkStructureType		sType;
			DE_NULL,										// const void*			pNext;
			*m_renderPass,									// VkRenderPass			renderPass;
			*m_framebuffer,									// VkFramebuffer		framebuffer;
			{ 0, 0, m_renderSize.x(), m_renderSize.y() },	// VkRect2D				renderArea;
			1,												// deUint32				attachmentCount;
			attachmentClearValues							// const VkClearValue*	pAttachmentClearValues;
		};

		m_cmdBuffer = createCommandBuffer(vk, vkDevice, &cmdBufferParams);

		VK_CHECK(vk.beginCommandBuffer(*m_cmdBuffer, &cmdBufferBeginInfo));
		vk.cmdBeginRenderPass(*m_cmdBuffer, &renderPassBeginInfo, VK_RENDER_PASS_CONTENTS_INLINE);

		vk.cmdBindDynamicViewportState(*m_cmdBuffer, *m_viewportState);
		vk.cmdBindDynamicRasterState(*m_cmdBuffer, *m_rasterState);
		//vk.cmdBindDynamicColorBlendState(*m_cmdBuffer, *m_colorBlendState);

		vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipeline);

		vk.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayout, 0u, 1, &*m_descriptorSet, 0u, DE_NULL);

		vk.cmdBindIndexBuffer(*m_cmdBuffer, *m_indiceBuffer, 0, VK_INDEX_TYPE_UINT16);

		const deUint32 numberOfVertexAttributes = (deUint32)m_vertexBuffers.size();
		std::vector<VkDeviceSize> offsets;

		for (deUint32 i = 0; i < numberOfVertexAttributes; i++)
			offsets.push_back(0);

		vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, numberOfVertexAttributes, &m_vertexBuffers[0], &offsets[0]);
		vk.cmdDrawIndexed(*m_cmdBuffer, 0, quadGrid.getNumTriangles() * 3, 0, 0, 1);

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

	// Execute Draw
	{
		VK_CHECK(vk.resetFences(vkDevice, 1, &m_fence.get()));
		VK_CHECK(vk.queueSubmit(queue, 1, &m_cmdBuffer.get(), *m_fence));
		VK_CHECK(vk.waitForFences(vkDevice, 1, &m_fence.get(), true, ~(0ull) /* infinity*/));
	}

	// Read back the result
	{
		const VkDeviceSize imageSizeBytes = (VkDeviceSize)(sizeof(deUint32) * m_renderSize.x() * m_renderSize.y());
		const VkBufferCreateInfo readImageBufferParams   =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		//  VkStructureType		sType;
			DE_NULL,									//  const void*			pNext;
			imageSizeBytes,								//  VkDeviceSize		size;
			VK_BUFFER_USAGE_TRANSFER_DESTINATION_BIT,	//  VkBufferUsageFlags	usage;
			0u,											//  VkBufferCreateFlags	flags;
			VK_SHARING_MODE_EXCLUSIVE,					//  VkSharingMode		sharingMode;
			1u,											//  deUint32			queueFamilyCount;
			&queueFamilyIndex,							//  const deUint32*		pQueueFamilyIndices;
		};
		const Unique<VkBuffer> readImageBuffer(createBuffer(vk, vkDevice, &readImageBufferParams));
		const de::UniquePtr<Allocation>	readImageBufferMemory(memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *readImageBuffer), MemoryRequirement::HostVisible));

		VK_CHECK(vk.bindBufferMemory(vkDevice, *readImageBuffer, readImageBufferMemory->getMemory(), readImageBufferMemory->getOffset()));


		// Copy image to buffer
		{
			Move<VkCmdPool> cmdPool;
			Move<VkCmdBuffer> cmdBuffer;
			Move<VkFence> fence;

			const VkCmdPoolCreateInfo cmdPoolParams =
			{
				VK_STRUCTURE_TYPE_CMD_POOL_CREATE_INFO,		// VkStructureType		sType;
				DE_NULL,									// const void*			pNext;
				queueFamilyIndex,									// deUint32				queueFamilyIndex;
				VK_CMD_POOL_CREATE_TRANSIENT_BIT			// VkCmdPoolCreateFlags	flags;
			};

			cmdPool = createCommandPool(vk, vkDevice, &cmdPoolParams);

			const VkCmdBufferCreateInfo cmdBufferParams =
			{
				VK_STRUCTURE_TYPE_CMD_BUFFER_CREATE_INFO,	// VkStructureType			sType;
				DE_NULL,									// const void*				pNext;
				*cmdPool,									// VkCmdPool				cmdPool;
				VK_CMD_BUFFER_LEVEL_PRIMARY,				// VkCmdBufferLevel			level;
				0u											// VkCmdBufferCreateFlags	flags;
			};

			const VkCmdBufferBeginInfo cmdBufferBeginInfo =
			{
				VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO,	// VkStructureType			sType;
				DE_NULL,									// const void*				pNext;
				0u,											// VkCmdBufferOptimizeFlags	flags;
				DE_NULL,									// VkRenderPass				renderPass;
				DE_NULL										// VkFramebuffer			framebuffer;
			};

			cmdBuffer = createCommandBuffer(vk, vkDevice, &cmdBufferParams);

			const VkBufferImageCopy copyParams =
			{
				0u,											//  VkDeviceSize		bufferOffset;
				m_renderSize.x() * 4u,						//  deUint32			bufferRowLength;
				0u,											//  deUint32			bufferImageHeight;
				{
					VK_IMAGE_ASPECT_COLOR,                  //  VkImageAspect		aspect;
					0u,										//  deUint32			mipLevel;
					0u,										//  deUint32			arraySlice;
				},											//  VkImageSubresource	imageSubresource;
				{ 0u, 0u, 0u },								//  VkOffset3D			imageOffset;
				{ m_renderSize.x(), m_renderSize.y(), 1u }	//  VkExtent3D			imageExtent;
			};

			VK_CHECK(vk.beginCommandBuffer(*cmdBuffer, &cmdBufferBeginInfo));
			vk.cmdCopyImageToBuffer(*cmdBuffer, *m_colorImage, VK_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL, *readImageBuffer, 1u, &copyParams);
			VK_CHECK(vk.endCommandBuffer(*cmdBuffer));

			// Create Fence
			const VkFenceCreateInfo fenceParams =
			{
				VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,	// VkStructureType		sType;
				DE_NULL,								// const void*			pNext;
				0u										// VkFenceCreateFlags	flags;
			};

			fence = createFence(vk, vkDevice, &fenceParams);

			VK_CHECK(vk.queueSubmit(queue, 1, &cmdBuffer.get(), *fence));
			VK_CHECK(vk.waitForFences(vkDevice, 1, &fence.get(), true, ~(0ull) /* infinity */));
		}


		void *imagePtr;
		VK_CHECK(vk.mapMemory(vkDevice, readImageBufferMemory->getMemory(), readImageBufferMemory->getOffset(), imageSizeBytes, 0u, &imagePtr));

		const VkMappedMemoryRange   range   =
		{
			VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,	//  VkStructureType	sType;
			DE_NULL,								//  const void*		pNext;
			readImageBufferMemory->getMemory(),		//  VkDeviceMemory	mem;
			0,										//  VkDeviceSize	offset;
			imageSizeBytes,							//  VkDeviceSize	size;
		};

		VK_CHECK(vk.invalidateMappedMemoryRanges(vkDevice, 1u, &range));

		deMemcpy(result.getAccess().getDataPtr(), imagePtr, imageSizeBytes);

		VK_CHECK(vk.unmapMemory(vkDevice, readImageBufferMemory->getMemory()));
	}
}

void ShaderRenderCaseInstance::computeVertexReference (Surface& result, const QuadGrid& quadGrid)
{
	// TODO!!
	// Buffer info.
	int					width		= result.getWidth();
	int					height		= result.getHeight();
	int					gridSize	= quadGrid.getGridSize();
	int					stride		= gridSize + 1;
	//bool				hasAlpha	= m_context.getRenderTarget().getPixelFormat().alphaBits > 0;
	bool				hasAlpha	= false;
	ShaderEvalContext	evalCtx		(quadGrid);

	// Evaluate color for each vertex.
	vector<Vec4> colors((gridSize + 1) * (gridSize + 1));
	for (int y = 0; y < gridSize+1; y++)
	for (int x = 0; x < gridSize+1; x++)
	{
		float				sx			= (float)x / (float)gridSize;
		float				sy			= (float)y / (float)gridSize;
		int					vtxNdx		= ((y * (gridSize+1)) + x);

		evalCtx.reset(sx, sy);
		m_evaluator.evaluate(evalCtx);
		DE_ASSERT(!evalCtx.isDiscarded); // Discard is not available in vertex shader.
		Vec4 color = evalCtx.color;

		if (!hasAlpha)
			color.w() = 1.0f;

		colors[vtxNdx] = color;
	}

	// Render quads.
	for (int y = 0; y < gridSize; y++)
	for (int x = 0; x < gridSize; x++)
	{
		float x0 = (float)x       / (float)gridSize;
		float x1 = (float)(x + 1) / (float)gridSize;
		float y0 = (float)y       / (float)gridSize;
		float y1 = (float)(y + 1) / (float)gridSize;

		float sx0 = x0 * (float)width;
		float sx1 = x1 * (float)width;
		float sy0 = y0 * (float)height;
		float sy1 = y1 * (float)height;
		float oosx = 1.0f / (sx1 - sx0);
		float oosy = 1.0f / (sy1 - sy0);

		int ix0 = deCeilFloatToInt32(sx0 - 0.5f);
		int ix1 = deCeilFloatToInt32(sx1 - 0.5f);
		int iy0 = deCeilFloatToInt32(sy0 - 0.5f);
		int iy1 = deCeilFloatToInt32(sy1 - 0.5f);

		int		v00 = (y * stride) + x;
		int		v01 = (y * stride) + x + 1;
		int		v10 = ((y + 1) * stride) + x;
		int		v11 = ((y + 1) * stride) + x + 1;
		Vec4	c00 = colors[v00];
		Vec4	c01 = colors[v01];
		Vec4	c10 = colors[v10];
		Vec4	c11 = colors[v11];

		//printf("(%d,%d) -> (%f..%f, %f..%f) (%d..%d, %d..%d)\n", x, y, sx0, sx1, sy0, sy1, ix0, ix1, iy0, iy1);

		for (int iy = iy0; iy < iy1; iy++)
		for (int ix = ix0; ix < ix1; ix++)
		{
			DE_ASSERT(deInBounds32(ix, 0, width));
			DE_ASSERT(deInBounds32(iy, 0, height));

			float		sfx		= (float)ix + 0.5f;
			float		sfy		= (float)iy + 0.5f;
			float		fx1		= deFloatClamp((sfx - sx0) * oosx, 0.0f, 1.0f);
			float		fy1		= deFloatClamp((sfy - sy0) * oosy, 0.0f, 1.0f);

			// Triangle quad interpolation.
			bool		tri		= fx1 + fy1 <= 1.0f;
			float		tx		= tri ? fx1 : (1.0f-fx1);
			float		ty		= tri ? fy1 : (1.0f-fy1);
			const Vec4&	t0		= tri ? c00 : c11;
			const Vec4&	t1		= tri ? c01 : c10;
			const Vec4&	t2		= tri ? c10 : c01;
			Vec4		color	= t0 + (t1-t0)*tx + (t2-t0)*ty;

			result.setPixel(ix, iy, tcu::RGBA(color));
		}
	}
}

void ShaderRenderCaseInstance::computeFragmentReference (Surface& result, const QuadGrid& quadGrid)
{
	// Buffer info.
	int					width		= result.getWidth();
	int					height		= result.getHeight();
	//bool				hasAlpha	= m_renderCtx.getRenderTarget().getPixelFormat().alphaBits > 0;
	bool				hasAlpha	= true;
	ShaderEvalContext	evalCtx		(quadGrid);

	// Render.
	for (int y = 0; y < height; y++)
	for (int x = 0; x < width; x++)
	{
		float sx = ((float)x + 0.5f) / (float)width;
		float sy = ((float)y + 0.5f) / (float)height;

		evalCtx.reset(sx, sy);
		m_evaluator.evaluate(evalCtx);
		// Select either clear color or computed color based on discarded bit.
		Vec4 color = evalCtx.isDiscarded ? m_clearColor : evalCtx.color;

		if (!hasAlpha)
			color.w() = 1.0f;

		result.setPixel(x, y, tcu::RGBA(color));
	}
}

bool ShaderRenderCaseInstance::compareImages (const Surface& resImage, const Surface& refImage, float errorThreshold)
{
	return tcu::fuzzyCompare(m_context.getTestContext().getLog(), "ComparisonResult", "Image comparison result", refImage, resImage, errorThreshold, tcu::COMPARE_LOG_RESULT);
}


} // shaderrendercase
} // vkt
