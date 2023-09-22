/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 The Khronos Group Inc.
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
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
 * \brief Protected memory image access tests
 *//*--------------------------------------------------------------------*/

#include "vktProtectedMemShaderImageAccessTests.hpp"

#include "vktProtectedMemContext.hpp"
#include "vktProtectedMemUtils.hpp"
#include "vktProtectedMemImageValidator.hpp"
#include "vktTestCase.hpp"
#include "vktTestGroupUtil.hpp"

#include "vkPrograms.hpp"
#include "vkTypeUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuVector.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuStringTemplate.hpp"

#include "gluTextureTestUtil.hpp"

#include "deRandom.hpp"

namespace vkt
{
namespace ProtectedMem
{

namespace
{

enum
{
	RENDER_WIDTH	= 128,
	RENDER_HEIGHT	= 128,
	IMAGE_WIDTH		= 128,
	IMAGE_HEIGHT	= 128,
};

enum AccessType
{
	ACCESS_TYPE_SAMPLING = 0,
	ACCESS_TYPE_TEXEL_FETCH,
	ACCESS_TYPE_IMAGE_LOAD,
	ACCESS_TYPE_IMAGE_STORE,
	ACCESS_TYPE_IMAGE_ATOMICS,

	ACCESS_TYPE_LAST
};

enum AtomicOperation
{
	ATOMIC_OPERATION_ADD = 0,
	ATOMIC_OPERATION_MIN,
	ATOMIC_OPERATION_MAX,
	ATOMIC_OPERATION_AND,
	ATOMIC_OPERATION_OR,
	ATOMIC_OPERATION_XOR,
	ATOMIC_OPERATION_EXCHANGE,

	ATOMIC_OPERATION_LAST
};

struct Params
{
	glu::ShaderType				shaderType;
	AccessType					accessType;
	vk::VkFormat				imageFormat;
	AtomicOperation				atomicOperation;
	bool						pipelineProtectedAccess;
	bool						useMaintenance5;
	vk::VkPipelineCreateFlags	flags;
	ProtectionMode				protectionMode;

	Params (void)
		: shaderType				(glu::SHADERTYPE_LAST)
		, accessType				(ACCESS_TYPE_LAST)
		, imageFormat				(vk::VK_FORMAT_UNDEFINED)
		, atomicOperation			(ATOMIC_OPERATION_LAST)
		, pipelineProtectedAccess	(false)
		, useMaintenance5			(false)
		, flags						((vk::VkPipelineCreateFlags)0u)
		, protectionMode			(PROTECTION_ENABLED)
	{}

	Params (const glu::ShaderType			shaderType_,
			const AccessType				accessType_,
			const vk::VkFormat				imageFormat_,
			const AtomicOperation			atomicOperation_,
			const bool						pipelineProtectedAccess_,
			const vk::VkPipelineCreateFlags flags_)
		: shaderType				(shaderType_)
		, accessType				(accessType_)
		, imageFormat				(imageFormat_)
		, atomicOperation			(atomicOperation_)
		, pipelineProtectedAccess	(pipelineProtectedAccess_)
		, flags						(flags_)
		, protectionMode			(PROTECTION_ENABLED)
	{
#ifndef CTS_USES_VULKANSC
		if ((flags_ & vk::VK_PIPELINE_CREATE_NO_PROTECTED_ACCESS_BIT_EXT) != 0) {
			protectionMode = PROTECTION_DISABLED;
		}
#endif
	}
};

static deUint32 getSeedValue (const Params& params)
{
	return deInt32Hash(params.shaderType) ^ deInt32Hash(params.accessType) ^ deInt32Hash(params.imageFormat) ^ deInt32Hash(params.atomicOperation);
}

static std::string getAtomicOperationCaseName (const AtomicOperation op)
{
	switch (op)
	{
		case ATOMIC_OPERATION_ADD:			return "add";
		case ATOMIC_OPERATION_MIN:			return "min";
		case ATOMIC_OPERATION_MAX:			return "max";
		case ATOMIC_OPERATION_AND:			return "and";
		case ATOMIC_OPERATION_OR:			return "or";
		case ATOMIC_OPERATION_XOR:			return "xor";
		case ATOMIC_OPERATION_EXCHANGE:		return "exchange";
		default:
			DE_FATAL("Impossible");
			return "";
	}
}

static std::string getAtomicOperationShaderFuncName (const AtomicOperation op)
{
	switch (op)
	{
		case ATOMIC_OPERATION_ADD:			return "imageAtomicAdd";
		case ATOMIC_OPERATION_MIN:			return "imageAtomicMin";
		case ATOMIC_OPERATION_MAX:			return "imageAtomicMax";
		case ATOMIC_OPERATION_AND:			return "imageAtomicAnd";
		case ATOMIC_OPERATION_OR:			return "imageAtomicOr";
		case ATOMIC_OPERATION_XOR:			return "imageAtomicXor";
		case ATOMIC_OPERATION_EXCHANGE:		return "imageAtomicExchange";
		default:
			DE_FATAL("Impossible");
			return "";
	}
}

//! Computes the result of an atomic operation where "a" is the data operated on and "b" is the parameter to the atomic function.
static deInt32 computeBinaryAtomicOperationResult (const AtomicOperation op, const deInt32 a, const deInt32 b)
{
	switch (op)
	{
		case ATOMIC_OPERATION_ADD:			return a + b;
		case ATOMIC_OPERATION_MIN:			return de::min(a, b);
		case ATOMIC_OPERATION_MAX:			return de::max(a, b);
		case ATOMIC_OPERATION_AND:			return a & b;
		case ATOMIC_OPERATION_OR:			return a | b;
		case ATOMIC_OPERATION_XOR:			return a ^ b;
		case ATOMIC_OPERATION_EXCHANGE:		return b;
		default:
			DE_FATAL("Impossible");
			return -1;
	}
}

static std::string getShaderImageFormatQualifier (const tcu::TextureFormat& format)
{
	const char* orderPart;
	const char* typePart;

	switch (format.order)
	{
		case tcu::TextureFormat::R:		orderPart = "r";	break;
		case tcu::TextureFormat::RG:	orderPart = "rg";	break;
		case tcu::TextureFormat::RGB:	orderPart = "rgb";	break;
		case tcu::TextureFormat::RGBA:	orderPart = "rgba";	break;

		default:
			DE_FATAL("Impossible");
			orderPart = DE_NULL;
	}

	switch (format.type)
	{
		case tcu::TextureFormat::FLOAT:				typePart = "32f";		break;
		case tcu::TextureFormat::HALF_FLOAT:		typePart = "16f";		break;

		case tcu::TextureFormat::UNSIGNED_INT32:	typePart = "32ui";		break;
		case tcu::TextureFormat::UNSIGNED_INT16:	typePart = "16ui";		break;
		case tcu::TextureFormat::UNSIGNED_INT8:		typePart = "8ui";		break;

		case tcu::TextureFormat::SIGNED_INT32:		typePart = "32i";		break;
		case tcu::TextureFormat::SIGNED_INT16:		typePart = "16i";		break;
		case tcu::TextureFormat::SIGNED_INT8:		typePart = "8i";		break;

		case tcu::TextureFormat::UNORM_INT16:		typePart = "16";		break;
		case tcu::TextureFormat::UNORM_INT8:		typePart = "8";			break;

		case tcu::TextureFormat::SNORM_INT16:		typePart = "16_snorm";	break;
		case tcu::TextureFormat::SNORM_INT8:		typePart = "8_snorm";	break;

		default:
			DE_FATAL("Impossible");
			typePart = DE_NULL;
	}

	return std::string() + orderPart + typePart;
}

static std::string getShaderSamplerOrImageType (const tcu::TextureFormat& format, bool isSampler)
{
	const std::string formatPart = tcu::getTextureChannelClass(format.type) == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER ? "u" :
								   tcu::getTextureChannelClass(format.type) == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER   ? "i" : "";

	return formatPart + (isSampler ? "sampler2D" : "image2D");
}

class ImageAccessTestInstance : public ProtectedTestInstance
{
public:
								ImageAccessTestInstance	(Context&				ctx,
														 const ImageValidator&	validator,
														 const Params&			params);
	virtual tcu::TestStatus		iterate					(void);

private:
	de::MovePtr<tcu::Texture2D>	createTestTexture2D		(void);
	void						calculateAtomicRef		(tcu::Texture2D&		texture2D);
	tcu::TestStatus				validateResult			(vk::VkImage			image,
														 vk::VkImageLayout		imageLayout,
														 const tcu::Texture2D&	texture2D,
														 const tcu::Sampler&	refSampler);

	tcu::TestStatus				executeFragmentTest		(void);
	tcu::TestStatus				executeComputeTest		(void);

	const ImageValidator&		m_validator;
	const Params				m_params;
};

class ImageAccessTestCase : public TestCase
{
public:
								ImageAccessTestCase		(tcu::TestContext&		testCtx,
														 const std::string&		name,
														 const std::string&		description,
														 const Params&			params)
									: TestCase		(testCtx, name, description)
									, m_validator	(params.imageFormat)
									, m_params		(params)
								{
								}

	virtual						~ImageAccessTestCase	(void) {}
	virtual TestInstance*		createInstance			(Context& ctx) const
								{
									return new ImageAccessTestInstance(ctx, m_validator, m_params);
								}
	virtual void				initPrograms			(vk::SourceCollections& programCollection) const;
	virtual void				checkSupport			(Context& context) const
								{
									checkProtectedQueueSupport(context);
									if (m_params.useMaintenance5)
										context.requireDeviceFunctionality("VK_KHR_maintenance5");
								}

private:
	ImageValidator				m_validator;
	Params						m_params;
};

void ImageAccessTestCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const tcu::TextureFormat&	texFormat		= mapVkFormat(m_params.imageFormat);
	const std::string			imageFormat		= getShaderImageFormatQualifier(texFormat);
	const std::string			imageType		= getShaderSamplerOrImageType(texFormat, false);
	const std::string			samplerType		= getShaderSamplerOrImageType(texFormat, true);
	const std::string			colorVecType	= isIntFormat(m_params.imageFormat)		? "ivec4" :
												  isUintFormat(m_params.imageFormat)	? "uvec4" : "vec4";

	m_validator.initPrograms(programCollection);

	if (m_params.shaderType == glu::SHADERTYPE_FRAGMENT)
	{
		{
			// Vertex shader
			const char* vert = "#version 450\n"
							   "layout(location = 0) in mediump vec2 a_position;\n"
							   "layout(location = 1) in mediump vec2 a_texCoord;\n"
							   "layout(location = 0) out mediump vec2 v_texCoord;\n"
							   "\n"
							   "void main() {\n"
							   "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
							   "    v_texCoord = a_texCoord;\n"
							   "}\n";

			programCollection.glslSources.add("vert") << glu::VertexSource(vert);
		}

		{
			// Fragment shader
			std::ostringstream frag;
			frag << "#version 450\n"
					"layout(location = 0) in mediump vec2 v_texCoord;\n"
					"layout(location = 0) out highp ${COLOR_VEC_TYPE} o_color;\n";

			switch (m_params.accessType)
			{
				case ACCESS_TYPE_SAMPLING:
				case ACCESS_TYPE_TEXEL_FETCH:
					frag << "layout(set = 0, binding = 0) uniform highp ${SAMPLER_TYPE} u_sampler;\n";
					break;
				case ACCESS_TYPE_IMAGE_LOAD:
					frag << "layout(set = 0, binding = 0, ${IMAGE_FORMAT}) readonly uniform highp ${IMAGE_TYPE} u_image;\n";
					break;
				case ACCESS_TYPE_IMAGE_STORE:
					frag << "layout(set = 0, binding = 0, ${IMAGE_FORMAT}) readonly uniform highp ${IMAGE_TYPE} u_imageA;\n";
					frag << "layout(set = 0, binding = 1, ${IMAGE_FORMAT}) writeonly uniform highp ${IMAGE_TYPE} u_imageB;\n";
					break;
				case ACCESS_TYPE_IMAGE_ATOMICS:
					frag << "layout(set = 0, binding = 0, ${IMAGE_FORMAT}) coherent uniform highp ${IMAGE_TYPE} u_image;\n";
					break;
				default:
					DE_FATAL("Impossible");
					break;
			}

			frag << "\n"
					"void main() {\n";

			switch (m_params.accessType)
			{
				case ACCESS_TYPE_SAMPLING:
					frag << "    o_color = texture(u_sampler, v_texCoord);\n";
					break;
				case ACCESS_TYPE_TEXEL_FETCH:
					frag << "    const highp int lod = 0;\n";
					frag << "    o_color = texelFetch(u_sampler, ivec2(v_texCoord), lod);\n";
					break;
				case ACCESS_TYPE_IMAGE_LOAD:
					frag << "    o_color = imageLoad(u_image, ivec2(v_texCoord));\n";
					break;
				case ACCESS_TYPE_IMAGE_STORE:
					frag << "    o_color = imageLoad(u_imageA, ivec2(v_texCoord));\n";
					frag << "    imageStore(u_imageB, ivec2(v_texCoord), o_color);\n";
					break;
				case ACCESS_TYPE_IMAGE_ATOMICS:
					frag << "    int gx = int(v_texCoord.x);\n";
					frag << "    int gy = int(v_texCoord.y);\n";
					frag << "    "
						 << getAtomicOperationShaderFuncName(m_params.atomicOperation)
						 << "(u_image, ivec2(v_texCoord), "
						 << (isUintFormat(m_params.imageFormat) ? "uint" : "int")
						 << "(gx*gx + gy*gy));\n";
					frag << "    o_color = imageLoad(u_image, ivec2(v_texCoord));\n";
					break;
				default:
					DE_FATAL("Impossible");
					break;
			}

			frag << "}\n";

			std::map<std::string, std::string> fragParams;

			fragParams["IMAGE_FORMAT"]		= imageFormat;
			fragParams["IMAGE_TYPE"]		= imageType;
			fragParams["SAMPLER_TYPE"]		= samplerType;
			fragParams["COLOR_VEC_TYPE"]	= colorVecType;

			programCollection.glslSources.add("frag") << glu::FragmentSource(tcu::StringTemplate(frag.str()).specialize(fragParams));
		}
	}
	else if (m_params.shaderType == glu::SHADERTYPE_COMPUTE)
	{
		// Compute shader
		std::ostringstream comp;
		comp << "#version 450\n"
				"layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
				"layout(set = 0, binding = 0, ${IMAGE_FORMAT}) ${RES_MEM_QUALIFIER} uniform highp ${IMAGE_TYPE} u_resultImage;\n";

		switch (m_params.accessType)
		{
			case ACCESS_TYPE_SAMPLING:
			case ACCESS_TYPE_TEXEL_FETCH:
				comp << "layout(set = 0, binding = 1) uniform highp ${SAMPLER_TYPE} u_sampler;\n";
				break;
			case ACCESS_TYPE_IMAGE_LOAD:
			case ACCESS_TYPE_IMAGE_STORE:
				comp << "layout(set = 0, binding = 1, ${IMAGE_FORMAT}) readonly uniform highp ${IMAGE_TYPE} u_srcImage;\n";
				break;
			case ACCESS_TYPE_IMAGE_ATOMICS:
				break;
			default:
				DE_FATAL("Impossible");
				break;
		}

		comp << "\n"
				"void main() {\n"
				"    int gx = int(gl_GlobalInvocationID.x);\n"
				"    int gy = int(gl_GlobalInvocationID.y);\n";

		switch (m_params.accessType)
		{
			case ACCESS_TYPE_SAMPLING:
				comp << "    ${COLOR_VEC_TYPE} color = texture(u_sampler, vec2(float(gx)/" << de::toString((int)IMAGE_WIDTH) << ", float(gy)/" << de::toString((int)IMAGE_HEIGHT) << "));\n";
				comp << "    imageStore(u_resultImage, ivec2(gx, gy), color);\n";
				break;
			case ACCESS_TYPE_TEXEL_FETCH:
				comp << "    const highp int lod = 0;\n";
				comp << "    ${COLOR_VEC_TYPE} color = texelFetch(u_sampler, ivec2(gx, gy), lod);\n";
				comp << "    imageStore(u_resultImage, ivec2(gx, gy), color);\n";
				break;
			case ACCESS_TYPE_IMAGE_LOAD:
			case ACCESS_TYPE_IMAGE_STORE:
				comp << "    ${COLOR_VEC_TYPE} color = imageLoad(u_srcImage, ivec2(gx, gy));\n";
				comp << "    imageStore(u_resultImage, ivec2(gx, gy), color);\n";
				break;
			case ACCESS_TYPE_IMAGE_ATOMICS:
				comp << "    "
					 << getAtomicOperationShaderFuncName(m_params.atomicOperation)
					 << "(u_resultImage, ivec2(gx, gy), "
					 << (isUintFormat(m_params.imageFormat) ? "uint" : "int")
					 << "(gx*gx + gy*gy));\n";
				break;
			default:
				DE_FATAL("Impossible");
				break;
		}

		comp << "}\n";

		std::map<std::string, std::string> compParams;

		compParams["IMAGE_FORMAT"]		= imageFormat;
		compParams["IMAGE_TYPE"]		= imageType;
		compParams["SAMPLER_TYPE"]		= samplerType;
		compParams["COLOR_VEC_TYPE"]	= colorVecType;
		compParams["RES_MEM_QUALIFIER"]	= m_params.accessType == ACCESS_TYPE_IMAGE_ATOMICS ? "coherent" : "writeonly";

		programCollection.glslSources.add("comp") << glu::ComputeSource(tcu::StringTemplate(comp.str()).specialize(compParams));
	}
	else
		DE_FATAL("Impossible");
}

ImageAccessTestInstance::ImageAccessTestInstance (Context&					ctx,
												  const ImageValidator&		validator,
												  const Params&				params)
	: ProtectedTestInstance(ctx, params.pipelineProtectedAccess ? std::vector<std::string>({ "VK_EXT_pipeline_protected_access" }) : std::vector<std::string>())
	, m_validator			(validator)
	, m_params				(params)
{
}

de::MovePtr<tcu::Texture2D> ImageAccessTestInstance::createTestTexture2D (void)
{
	const tcu::TextureFormat		texFmt		= mapVkFormat(m_params.imageFormat);
	const tcu::TextureFormatInfo	fmtInfo		= tcu::getTextureFormatInfo(texFmt);
	de::MovePtr<tcu::Texture2D>		texture2D	(new tcu::Texture2D(texFmt, IMAGE_WIDTH, IMAGE_HEIGHT));

	// \note generate only the base level
	texture2D->allocLevel(0);

	const tcu::PixelBufferAccess&	level		= texture2D->getLevel(0);

	if (m_params.accessType == ACCESS_TYPE_IMAGE_ATOMICS)
	{
		// use a smaller range than the format would allow
		const float		cMin	= isIntFormat(m_params.imageFormat) ? -1000.0f : 0.0f;
		const float		cMax	= +1000.0f;

		fillWithRandomColorTiles(level, tcu::Vec4(cMin, 0, 0, 0), tcu::Vec4(cMax, 0, 0, 0), getSeedValue(m_params));
	}
	else
		fillWithRandomColorTiles(level, fmtInfo.valueMin, fmtInfo.valueMax, getSeedValue(m_params));

	return texture2D;
}

tcu::TestStatus ImageAccessTestInstance::iterate (void)
{
	switch (m_params.shaderType)
	{
		case glu::SHADERTYPE_FRAGMENT:	return executeFragmentTest();
		case glu::SHADERTYPE_COMPUTE:	return executeComputeTest();
		default:
			DE_FATAL("Impossible");
			return tcu::TestStatus::fail("");
	}
}

tcu::TestStatus ImageAccessTestInstance::executeComputeTest (void)
{
	ProtectedContext&					ctx					(m_protectedContext);
	const vk::DeviceInterface&			vk					= ctx.getDeviceInterface();
	const vk::VkDevice					device				= ctx.getDevice();
	const vk::VkQueue					queue				= ctx.getQueue();
	const deUint32						queueFamilyIndex	= ctx.getQueueFamilyIndex();

	vk::Unique<vk::VkCommandPool>		cmdPool				(makeCommandPool(vk, device, m_params.protectionMode, queueFamilyIndex));

	de::MovePtr<tcu::Texture2D>			texture2D			= createTestTexture2D();
	const tcu::Sampler					refSampler			= tcu::Sampler(tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE,
																		   tcu::Sampler::NEAREST, tcu::Sampler::NEAREST,
																		   00.0f /* LOD threshold */, true /* normalized coords */, tcu::Sampler::COMPAREMODE_NONE,
																		   0 /* cmp channel */, tcu::Vec4(0.0f) /* border color */, true /* seamless cube map */);

	vk::Unique<vk::VkShaderModule>		computeShader		(vk::createShaderModule(vk, device, ctx.getBinaryCollection().get("comp"), 0));

	de::MovePtr<vk::ImageWithMemory>	imageSrc;
	de::MovePtr<vk::ImageWithMemory>	imageDst;
	vk::Move<vk::VkSampler>				sampler;
	vk::Move<vk::VkImageView>			imageViewSrc;
	vk::Move<vk::VkImageView>			imageViewDst;

	vk::Move<vk::VkDescriptorSetLayout>	descriptorSetLayout;
	vk::Move<vk::VkDescriptorPool>		descriptorPool;
	vk::Move<vk::VkDescriptorSet>		descriptorSet;

	// Create src and dst images
	{
		vk::VkImageUsageFlags imageUsageFlags = vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT	|
												vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT	|
												vk::VK_IMAGE_USAGE_SAMPLED_BIT		|
												vk::VK_IMAGE_USAGE_STORAGE_BIT;

		imageSrc = createImage2D(ctx, m_params.protectionMode, queueFamilyIndex,
								 IMAGE_WIDTH, IMAGE_HEIGHT,
								 m_params.imageFormat,
								 imageUsageFlags);

		if (m_params.accessType != ACCESS_TYPE_IMAGE_ATOMICS)
		{
			imageDst = createImage2D(ctx, m_params.protectionMode, queueFamilyIndex,
									 IMAGE_WIDTH, IMAGE_HEIGHT,
									 m_params.imageFormat,
									 imageUsageFlags);
		}
	}

	// Upload source image
	{
		de::MovePtr<vk::ImageWithMemory>	unprotectedImage	= createImage2D(ctx, PROTECTION_DISABLED, queueFamilyIndex,
																				IMAGE_WIDTH, IMAGE_HEIGHT,
																				m_params.imageFormat,
																				vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT | vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT);

		// Upload data to an unprotected image
		uploadImage(m_protectedContext, **unprotectedImage, *texture2D);

		// Select vkImageLayout based upon accessType
		vk::VkImageLayout imageSrcLayout = vk::VK_IMAGE_LAYOUT_UNDEFINED;

		switch (m_params.accessType)
		{
			case ACCESS_TYPE_SAMPLING:
			case ACCESS_TYPE_TEXEL_FETCH:
			{
				imageSrcLayout = vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				break;
			}
			case ACCESS_TYPE_IMAGE_LOAD:
			case ACCESS_TYPE_IMAGE_STORE:
			case ACCESS_TYPE_IMAGE_ATOMICS:
			{
				imageSrcLayout = vk::VK_IMAGE_LAYOUT_GENERAL;
				break;
			}
			default:
				DE_FATAL("Impossible");
				break;
		}

		// Copy unprotected image to protected image
		copyToProtectedImage(m_protectedContext, **unprotectedImage, **imageSrc, imageSrcLayout, IMAGE_WIDTH, IMAGE_HEIGHT, m_params.protectionMode);
	}

	// Clear dst image
	if (m_params.accessType != ACCESS_TYPE_IMAGE_ATOMICS && m_params.protectionMode == PROTECTION_ENABLED)
		clearImage(m_protectedContext, **imageDst);

	// Create descriptors
	{
		vk::DescriptorSetLayoutBuilder	layoutBuilder;
		vk::DescriptorPoolBuilder		poolBuilder;

		switch (m_params.accessType)
		{
			case ACCESS_TYPE_SAMPLING:
			case ACCESS_TYPE_TEXEL_FETCH:
				layoutBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, vk::VK_SHADER_STAGE_COMPUTE_BIT);
				layoutBuilder.addSingleSamplerBinding(vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, vk::VK_SHADER_STAGE_COMPUTE_BIT, DE_NULL);
				poolBuilder.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1u);
				poolBuilder.addType(vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1u);
				break;
			case ACCESS_TYPE_IMAGE_LOAD:
			case ACCESS_TYPE_IMAGE_STORE:
				layoutBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, vk::VK_SHADER_STAGE_COMPUTE_BIT);
				layoutBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, vk::VK_SHADER_STAGE_COMPUTE_BIT);
				poolBuilder.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2u);
				break;
			case ACCESS_TYPE_IMAGE_ATOMICS:
				layoutBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, vk::VK_SHADER_STAGE_COMPUTE_BIT);
				poolBuilder.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1u);
				break;
			default:
				DE_FATAL("Impossible");
				break;
		}

		descriptorSetLayout		= layoutBuilder.build(vk, device);
		descriptorPool			= poolBuilder.build(vk, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
		descriptorSet			= makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout);
	}

	// Create pipeline layout
	vk::Unique<vk::VkPipelineLayout>	pipelineLayout		(makePipelineLayout(vk, device, *descriptorSetLayout));

	// Create sampler and image views
	{
		if (m_params.accessType == ACCESS_TYPE_SAMPLING || m_params.accessType == ACCESS_TYPE_TEXEL_FETCH)
		{
			const tcu::TextureFormat		texFormat		= mapVkFormat(m_params.imageFormat);
			const vk::VkSamplerCreateInfo	samplerParams	= vk::mapSampler(refSampler, texFormat);

			sampler = createSampler(vk, device, &samplerParams);
		}

		imageViewSrc = createImageView(ctx, **imageSrc, m_params.imageFormat);

		if (m_params.accessType != ACCESS_TYPE_IMAGE_ATOMICS)
			imageViewDst = createImageView(ctx, **imageDst, m_params.imageFormat);
	}

	// Update descriptor set information
	{
		vk::DescriptorSetUpdateBuilder		updateBuilder;

		switch (m_params.accessType)
		{
			case ACCESS_TYPE_SAMPLING:
			case ACCESS_TYPE_TEXEL_FETCH:
			{
				vk::VkDescriptorImageInfo	descStorageImgDst	= makeDescriptorImageInfo((vk::VkSampler)0, *imageViewDst, vk::VK_IMAGE_LAYOUT_GENERAL);
				vk::VkDescriptorImageInfo	descSampledImgSrc	= makeDescriptorImageInfo(*sampler, *imageViewSrc, vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

				updateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descStorageImgDst);
				updateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(1u), vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &descSampledImgSrc);
				break;
			}
			case ACCESS_TYPE_IMAGE_LOAD:
			case ACCESS_TYPE_IMAGE_STORE:
			{
				vk::VkDescriptorImageInfo	descStorageImgDst	= makeDescriptorImageInfo((vk::VkSampler)0, *imageViewDst, vk::VK_IMAGE_LAYOUT_GENERAL);
				vk::VkDescriptorImageInfo	descStorageImgSrc	= makeDescriptorImageInfo((vk::VkSampler)0, *imageViewSrc, vk::VK_IMAGE_LAYOUT_GENERAL);

				updateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descStorageImgDst);
				updateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(1u), vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descStorageImgSrc);
				break;
			}
			case ACCESS_TYPE_IMAGE_ATOMICS:
			{
				vk::VkDescriptorImageInfo	descStorageImg		= makeDescriptorImageInfo((vk::VkSampler)0, *imageViewSrc, vk::VK_IMAGE_LAYOUT_GENERAL);

				updateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descStorageImg);
				break;
			}
			default:
				DE_FATAL("Impossible");
				break;
		}

		updateBuilder.update(vk, device);
	}

	// Create validation compute commands & submit
	{
		const vk::VkPipelineShaderStageCreateInfo pipelineShaderStageParams
		{
			vk::VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType						sType;
			nullptr,													// const void*							pNext;
			0u,															// VkPipelineShaderStageCreateFlags		flags;
			vk::VK_SHADER_STAGE_COMPUTE_BIT,							// VkShaderStageFlagBits				stage;
			*computeShader,												// VkShaderModule						module;
			"main",														// const char*							pName;
			DE_NULL,													// const VkSpecializationInfo*			pSpecializationInfo;
		};

		vk::VkComputePipelineCreateInfo pipelineCreateInfo
		{
			vk::VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,			// VkStructureType					sType;
			nullptr,													// const void*						pNext;
			m_params.flags,												// VkPipelineCreateFlags			flags;
			pipelineShaderStageParams,									// VkPipelineShaderStageCreateInfo	stage;
			*pipelineLayout,											// VkPipelineLayout					layout;
			DE_NULL,													// VkPipeline						basePipelineHandle;
			0,															// deInt32							basePipelineIndex;
		};

#ifndef CTS_USES_VULKANSC
		vk::VkPipelineCreateFlags2CreateInfoKHR pipelineFlags2CreateInfo = vk::initVulkanStructure();
		if (m_params.useMaintenance5)
		{
			pipelineFlags2CreateInfo.flags = (vk::VkPipelineCreateFlagBits2KHR)m_params.flags;
			pipelineCreateInfo.pNext = &pipelineFlags2CreateInfo;
			pipelineCreateInfo.flags = 0;
		}
#endif // CTS_USES_VULKANSC

		vk::Unique<vk::VkPipeline>			pipeline(createComputePipeline(vk, device, DE_NULL, &pipelineCreateInfo));

		const vk::Unique<vk::VkFence>		fence		(vk::createFence(vk, device));
		vk::Unique<vk::VkCommandBuffer>		cmdBuffer	(vk::allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

		beginCommandBuffer(vk, *cmdBuffer);

		vk.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
		vk.cmdBindDescriptorSets(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &*descriptorSet, 0u, DE_NULL);
		vk.cmdDispatch(*cmdBuffer, (deUint32)IMAGE_WIDTH, (deUint32)IMAGE_HEIGHT, 1u);
		endCommandBuffer(vk, *cmdBuffer);

		VK_CHECK(queueSubmit(ctx, m_params.protectionMode, queue, *cmdBuffer, *fence, ~0ull));
	}

	// Calculate reference image
	if (m_params.accessType == ACCESS_TYPE_IMAGE_ATOMICS)
		calculateAtomicRef(*texture2D);

	// Validate result
	{
		const vk::VkImage	resultImage		= m_params.accessType == ACCESS_TYPE_IMAGE_ATOMICS ? **imageSrc : **imageDst;

		return validateResult(resultImage, vk::VK_IMAGE_LAYOUT_GENERAL, *texture2D, refSampler);
	}
}

tcu::TestStatus ImageAccessTestInstance::executeFragmentTest (void)
{
	ProtectedContext&					ctx					(m_protectedContext);
	const vk::DeviceInterface&			vk					= ctx.getDeviceInterface();
	const vk::VkDevice					device				= ctx.getDevice();
	const vk::VkQueue					queue				= ctx.getQueue();
	const deUint32						queueFamilyIndex	= ctx.getQueueFamilyIndex();

	// Create output image
	de::MovePtr<vk::ImageWithMemory>	colorImage			(createImage2D(ctx, m_params.protectionMode, queueFamilyIndex,
																		   RENDER_WIDTH, RENDER_HEIGHT,
																		   m_params.imageFormat,
																		   vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|vk::VK_IMAGE_USAGE_SAMPLED_BIT));
	vk::Unique<vk::VkImageView>			colorImageView		(createImageView(ctx, **colorImage, m_params.imageFormat));

	vk::Unique<vk::VkRenderPass>		renderPass			(createRenderPass(ctx, m_params.imageFormat));
	vk::Unique<vk::VkFramebuffer>		framebuffer			(createFramebuffer(ctx, RENDER_WIDTH, RENDER_HEIGHT, *renderPass, *colorImageView));

	vk::Unique<vk::VkCommandPool>		cmdPool				(makeCommandPool(vk, device, m_params.protectionMode, queueFamilyIndex));
	vk::Unique<vk::VkCommandBuffer>		cmdBuffer			(vk::allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	de::MovePtr<tcu::Texture2D>			texture2D			= createTestTexture2D();
	const tcu::Sampler					refSampler			= tcu::Sampler(tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE,
																		   tcu::Sampler::NEAREST, tcu::Sampler::NEAREST,
																		   00.0f /* LOD threshold */, true /* normalized coords */, tcu::Sampler::COMPAREMODE_NONE,
																		   0 /* cmp channel */, tcu::Vec4(0.0f) /* border color */, true /* seamless cube map */);

	vk::Move<vk::VkShaderModule>		vertexShader		= createShaderModule(vk, device, ctx.getBinaryCollection().get("vert"), 0);
	vk::Move<vk::VkShaderModule>		fragmentShader		= createShaderModule(vk, device, ctx.getBinaryCollection().get("frag"), 0);

	de::MovePtr<vk::ImageWithMemory>	imageSrc;
	de::MovePtr<vk::ImageWithMemory>	imageDst;
	vk::Move<vk::VkSampler>				sampler;
	vk::Move<vk::VkImageView>			imageViewSrc;
	vk::Move<vk::VkImageView>			imageViewDst;

	vk::Move<vk::VkPipeline>			graphicsPipeline;
	vk::Move<vk::VkDescriptorSetLayout>	descriptorSetLayout;
	vk::Move<vk::VkDescriptorPool>		descriptorPool;
	vk::Move<vk::VkDescriptorSet>		descriptorSet;

	// Create src and dst images
	{
		vk::VkImageUsageFlags imageUsageFlags = vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT	|
												vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT	|
												vk::VK_IMAGE_USAGE_SAMPLED_BIT;

		switch (m_params.accessType)
		{
			case ACCESS_TYPE_IMAGE_LOAD:
			case ACCESS_TYPE_IMAGE_STORE:
			case ACCESS_TYPE_IMAGE_ATOMICS:
				imageUsageFlags |= vk::VK_IMAGE_USAGE_STORAGE_BIT;
				break;
			default:
				break;
		}

		imageSrc = createImage2D(ctx, m_params.protectionMode, queueFamilyIndex,
								 IMAGE_WIDTH, IMAGE_HEIGHT,
								 m_params.imageFormat,
								 imageUsageFlags);

		if (m_params.accessType == ACCESS_TYPE_IMAGE_STORE)
		{
			imageDst = createImage2D(ctx, m_params.protectionMode, queueFamilyIndex,
									 IMAGE_WIDTH, IMAGE_HEIGHT,
									 m_params.imageFormat,
									 imageUsageFlags);
		}
	}

	// Select vkImageLayout based upon accessType
	vk::VkImageLayout imageLayout = vk::VK_IMAGE_LAYOUT_UNDEFINED;

	switch (m_params.accessType)
	{
		case ACCESS_TYPE_SAMPLING:
		case ACCESS_TYPE_TEXEL_FETCH:
		{
			imageLayout = vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			break;
		}
		case ACCESS_TYPE_IMAGE_LOAD:
		case ACCESS_TYPE_IMAGE_STORE:
		case ACCESS_TYPE_IMAGE_ATOMICS:
		{
			imageLayout = vk::VK_IMAGE_LAYOUT_GENERAL;
			break;
		}
		default:
			DE_FATAL("Impossible");
			break;
	}

	// Upload source image
	{
		de::MovePtr<vk::ImageWithMemory>	unprotectedImage	= createImage2D(ctx, PROTECTION_DISABLED, queueFamilyIndex,
																				IMAGE_WIDTH, IMAGE_HEIGHT,
																				m_params.imageFormat,
																				vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT | vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT);

		// Upload data to an unprotected image
		uploadImage(m_protectedContext, **unprotectedImage, *texture2D);

		// Copy unprotected image to protected image
		copyToProtectedImage(m_protectedContext, **unprotectedImage, **imageSrc, imageLayout, IMAGE_WIDTH, IMAGE_HEIGHT, m_params.protectionMode);
	}

	// Clear dst image
	if (m_params.accessType == ACCESS_TYPE_IMAGE_STORE && m_params.protectionMode == PROTECTION_ENABLED)
		clearImage(m_protectedContext, **imageDst);

	// Create descriptors
	{
		vk::DescriptorSetLayoutBuilder	layoutBuilder;
		vk::DescriptorPoolBuilder		poolBuilder;

		switch (m_params.accessType)
		{
			case ACCESS_TYPE_SAMPLING:
			case ACCESS_TYPE_TEXEL_FETCH:
				layoutBuilder.addSingleSamplerBinding(vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, vk::VK_SHADER_STAGE_FRAGMENT_BIT, DE_NULL);
				poolBuilder.addType(vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1u);
				break;
			case ACCESS_TYPE_IMAGE_LOAD:
				layoutBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, vk::VK_SHADER_STAGE_FRAGMENT_BIT);
				poolBuilder.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1u);
				break;
			case ACCESS_TYPE_IMAGE_STORE:
				layoutBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, vk::VK_SHADER_STAGE_FRAGMENT_BIT);
				layoutBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, vk::VK_SHADER_STAGE_FRAGMENT_BIT);
				poolBuilder.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2u);
				break;
			case ACCESS_TYPE_IMAGE_ATOMICS:
				layoutBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, vk::VK_SHADER_STAGE_FRAGMENT_BIT);
				poolBuilder.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1u);
				break;
			default:
				DE_FATAL("Impossible");
				break;
		}

		descriptorSetLayout		= layoutBuilder.build(vk, device);
		descriptorPool			= poolBuilder.build(vk, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
		descriptorSet			= makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout);
	}

	// Create pipeline layout
	vk::Unique<vk::VkPipelineLayout>	pipelineLayout		(makePipelineLayout(vk, device, *descriptorSetLayout));

	// Create sampler and image views
	{
		if (m_params.accessType == ACCESS_TYPE_SAMPLING || m_params.accessType == ACCESS_TYPE_TEXEL_FETCH)
		{
			const tcu::TextureFormat		texFormat		= mapVkFormat(m_params.imageFormat);
			const vk::VkSamplerCreateInfo	samplerParams	= vk::mapSampler(refSampler, texFormat);

			sampler = createSampler(vk, device, &samplerParams);
		}

		imageViewSrc = createImageView(ctx, **imageSrc, m_params.imageFormat);

		if (m_params.accessType == ACCESS_TYPE_IMAGE_STORE)
			imageViewDst = createImageView(ctx, **imageDst, m_params.imageFormat);
	}

	// Update descriptor set information
	{
		vk::DescriptorSetUpdateBuilder		updateBuilder;

		switch (m_params.accessType)
		{
			case ACCESS_TYPE_SAMPLING:
			case ACCESS_TYPE_TEXEL_FETCH:
			{
				vk::VkDescriptorImageInfo	descSampledImg		= makeDescriptorImageInfo(*sampler, *imageViewSrc, vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

				updateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &descSampledImg);
				break;
			}
			case ACCESS_TYPE_IMAGE_LOAD:
			{
				vk::VkDescriptorImageInfo	descStorageImg		= makeDescriptorImageInfo((vk::VkSampler)0, *imageViewSrc, vk::VK_IMAGE_LAYOUT_GENERAL);

				updateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descStorageImg);
				break;
			}
			case ACCESS_TYPE_IMAGE_STORE:
			{
				vk::VkDescriptorImageInfo	descStorageImgSrc	= makeDescriptorImageInfo((vk::VkSampler)0, *imageViewSrc, vk::VK_IMAGE_LAYOUT_GENERAL);
				vk::VkDescriptorImageInfo	descStorageImgDst	= makeDescriptorImageInfo((vk::VkSampler)0, *imageViewDst, vk::VK_IMAGE_LAYOUT_GENERAL);

				updateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descStorageImgSrc);
				updateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(1u), vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descStorageImgDst);
				break;
			}
			case ACCESS_TYPE_IMAGE_ATOMICS:
			{
				vk::VkDescriptorImageInfo	descStorageImg		= makeDescriptorImageInfo((vk::VkSampler)0, *imageViewSrc, vk::VK_IMAGE_LAYOUT_GENERAL);

				updateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descStorageImg);
				break;
			}
			default:
				DE_FATAL("Impossible");
				break;
		}

		updateBuilder.update(vk, device);
	}

	// Create vertex buffer and vertex input descriptors
	VertexBindings						vertexBindings;
	VertexAttribs						vertexAttribs;
	de::MovePtr<vk::BufferWithMemory>	vertexBuffer;
	{
		const float			positions[]		=
		{
			-1.0f,	-1.0f,
			-1.0f,	+1.0f,
			+1.0f,	-1.0f,
			+1.0f,	+1.0f,
		};

		std::vector<float>	texCoord;

		{
			const tcu::Vec2		minCoords		(0.0f, 0.0f);
			const tcu::Vec2		maxCoords		= m_params.accessType == ACCESS_TYPE_SAMPLING ?
												  tcu::Vec2(1.0f, 1.0f) :
												  tcu::Vec2((float)IMAGE_WIDTH - 0.1f, (float)IMAGE_HEIGHT - 0.1f);

			glu::TextureTestUtil::computeQuadTexCoord2D(texCoord, minCoords, maxCoords);
		}

		const deUint32		vertexPositionStrideSize	= (deUint32)sizeof(tcu::Vec2);
		const deUint32		vertexTextureStrideSize		= (deUint32)sizeof(tcu::Vec2);
		const deUint32		positionDataSize			= 4 * vertexPositionStrideSize;
		const deUint32		textureCoordDataSize		= 4 * vertexTextureStrideSize;
		const deUint32		vertexBufferSize			= positionDataSize + textureCoordDataSize;

		{
			const vk::VkVertexInputBindingDescription	vertexInputBindingDescriptions[2]	=
			{
				{
					0u,									// deUint32					binding;
					vertexPositionStrideSize,			// deUint32					strideInBytes;
					vk::VK_VERTEX_INPUT_RATE_VERTEX		// VkVertexInputStepRate	inputRate;
				},
				{
					1u,									// deUint32					binding;
					vertexTextureStrideSize,			// deUint32					strideInBytes;
					vk::VK_VERTEX_INPUT_RATE_VERTEX		// VkVertexInputStepRate	inputRate;
				}
			};
			vertexBindings.push_back(vertexInputBindingDescriptions[0]);
			vertexBindings.push_back(vertexInputBindingDescriptions[1]);

			const vk::VkVertexInputAttributeDescription	vertexInputAttributeDescriptions[2]	=
			{
				{
					0u,									// deUint32	location;
					0u,									// deUint32	binding;
					vk::VK_FORMAT_R32G32_SFLOAT,		// VkFormat	format;
					0u									// deUint32	offsetInBytes;
				},
				{
					1u,									// deUint32	location;
					1u,									// deUint32	binding;
					vk::VK_FORMAT_R32G32_SFLOAT,		// VkFormat	format;
					positionDataSize					// deUint32	offsetInBytes;
				}
			};
			vertexAttribs.push_back(vertexInputAttributeDescriptions[0]);
			vertexAttribs.push_back(vertexInputAttributeDescriptions[1]);
		}

		vertexBuffer = makeBuffer(ctx,
								  PROTECTION_DISABLED,
								  queueFamilyIndex,
								  vertexBufferSize,
								  vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
								  vk::MemoryRequirement::HostVisible);

		deMemcpy(vertexBuffer->getAllocation().getHostPtr(), positions, positionDataSize);
		deMemcpy(reinterpret_cast<deUint8*>(vertexBuffer->getAllocation().getHostPtr()) +  positionDataSize, texCoord.data(), textureCoordDataSize);
		vk::flushAlloc(vk, device, vertexBuffer->getAllocation());
	}

	// Create pipeline
	graphicsPipeline = makeGraphicsPipeline(vk,
											device,
											*pipelineLayout,
											*renderPass,
											*vertexShader,
											*fragmentShader,
											vertexBindings,
											vertexAttribs,
											tcu::UVec2(RENDER_WIDTH, RENDER_HEIGHT),
											vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
											m_params.flags);

	// Begin cmd buffer
	beginCommandBuffer(vk, *cmdBuffer);

	// Start image barrier
	{
		const vk::VkImageMemoryBarrier	startImgBarrier		=
		{
			vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// sType
			DE_NULL,											// pNext
			0,													// srcAccessMask
			vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,			// dstAccessMask
			vk::VK_IMAGE_LAYOUT_UNDEFINED,						// oldLayout
			vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,		// newLayout
			queueFamilyIndex,									// srcQueueFamilyIndex
			queueFamilyIndex,									// dstQueueFamilyIndex
			**colorImage,										// image
			{
				vk::VK_IMAGE_ASPECT_COLOR_BIT,					// aspectMask
				0u,												// baseMipLevel
				1u,												// mipLevels
				0u,												// baseArraySlice
				1u,												// subresourceRange
			}
		};

		vk.cmdPipelineBarrier(*cmdBuffer,
							  vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,				// srcStageMask
							  vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,	// dstStageMask
							  (vk::VkDependencyFlags)0,
							  0, (const vk::VkMemoryBarrier*)DE_NULL,
							  0, (const vk::VkBufferMemoryBarrier*)DE_NULL,
							  1, &startImgBarrier);
	}

	beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, vk::makeRect2D(0, 0, RENDER_WIDTH, RENDER_HEIGHT), tcu::Vec4(0.0f));

	vk.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline);
	vk.cmdBindDescriptorSets(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u, &*descriptorSet, 0u, DE_NULL);

	{
		const vk::VkDeviceSize vertexBufferOffset = 0;

		vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &vertexBuffer->get(), &vertexBufferOffset);
		vk.cmdBindVertexBuffers(*cmdBuffer, 1u, 1u, &vertexBuffer->get(), &vertexBufferOffset);
	}

	vk.cmdDraw(*cmdBuffer, /*vertexCount*/ 4u, 1u, 0u, 1u);

	endRenderPass(vk, *cmdBuffer);

	{
		const vk::VkImageMemoryBarrier	endImgBarrier		=
		{
			vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// sType
			DE_NULL,											// pNext
			vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,			// srcAccessMask
			vk::VK_ACCESS_SHADER_READ_BIT,						// dstAccessMask
			vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,		// oldLayout
			imageLayout,										// newLayout
			queueFamilyIndex,									// srcQueueFamilyIndex
			queueFamilyIndex,									// dstQueueFamilyIndex
			**colorImage,										// image
			{
				vk::VK_IMAGE_ASPECT_COLOR_BIT,					// aspectMask
				0u,												// baseMipLevel
				1u,												// mipLevels
				0u,												// baseArraySlice
				1u,												// subresourceRange
			}
		};
		vk.cmdPipelineBarrier(*cmdBuffer,
							  vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,	// srcStageMask
							  vk::VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,				// dstStageMask
							  (vk::VkDependencyFlags)0,
							  0, (const vk::VkMemoryBarrier*)DE_NULL,
							  0, (const vk::VkBufferMemoryBarrier*)DE_NULL,
							  1, &endImgBarrier);
	}

	endCommandBuffer(vk, *cmdBuffer);

	// Submit command buffer
	{
		const vk::Unique<vk::VkFence>	fence		(vk::createFence(vk, device));
		VK_CHECK(queueSubmit(ctx, m_params.protectionMode, queue, *cmdBuffer, *fence, ~0ull));
	}

	// Calculate reference image
	if (m_params.accessType == ACCESS_TYPE_IMAGE_ATOMICS)
		calculateAtomicRef(*texture2D);

	// Validate result
	{
		const vk::VkImage	resultImage		= m_params.accessType == ACCESS_TYPE_IMAGE_ATOMICS	?	**imageSrc	:
											  m_params.accessType == ACCESS_TYPE_IMAGE_STORE	?	**imageDst	: **colorImage;

		return validateResult(resultImage, imageLayout, *texture2D, refSampler);
	}
}

void ImageAccessTestInstance::calculateAtomicRef (tcu::Texture2D& texture2D)
{
	DE_ASSERT(m_params.accessType == ACCESS_TYPE_IMAGE_ATOMICS);

	const tcu::PixelBufferAccess&	reference	= texture2D.getLevel(0);

	for (int x = 0; x < reference.getWidth(); ++x)
	for (int y = 0; y < reference.getHeight(); ++y)
	{
		const deInt32	oldX		= reference.getPixelInt(x, y).x();
		const deInt32	atomicArg	= x*x + y*y;
		const deInt32	newX		= computeBinaryAtomicOperationResult(m_params.atomicOperation, oldX, atomicArg);

		reference.setPixel(tcu::IVec4(newX, 0, 0, 0), x, y);
	}
}

tcu::TestStatus ImageAccessTestInstance::validateResult (vk::VkImage image, vk::VkImageLayout imageLayout, const tcu::Texture2D& texture2D, const tcu::Sampler& refSampler)
{
	de::Random			rnd			(getSeedValue(m_params));
	ValidationData		refData;

	for (int ndx = 0; ndx < 4; ++ndx)
	{
		const float		lod		= 0.0f;
		const float		cx		= rnd.getFloat(0.0f, 1.0f);
		const float		cy		= rnd.getFloat(0.0f, 1.0f);

		refData.coords[ndx] = tcu::Vec4(cx, cy, 0.0f, 0.0f);
		refData.values[ndx] = texture2D.sample(refSampler, cx, cy, lod);
	}

	if (!m_validator.validateImage(m_protectedContext, refData, image, m_params.imageFormat, imageLayout))
		return tcu::TestStatus::fail("Something went really wrong");
	else
		return tcu::TestStatus::pass("Everything went OK");
}

} // anonymous

tcu::TestCaseGroup*	createShaderImageAccessTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> accessGroup (new tcu::TestCaseGroup(testCtx, "access", "Shader Image Access Tests"));

	static const struct
	{
		glu::ShaderType	type;
		const char*		name;
		const char*		desc;
	} shaderTypes[] =
	{
		{ glu::SHADERTYPE_FRAGMENT,		"fragment",			"Image access from fragment shader"		},
		{ glu::SHADERTYPE_COMPUTE,		"compute",			"Image access from compute shader"		},
	};

	static const struct
	{
		AccessType		type;
		const char*		name;
		const char*		desc;
	} accessTypes[] =
	{
		{ ACCESS_TYPE_SAMPLING,			"sampling",			"Sampling test"			},
		{ ACCESS_TYPE_TEXEL_FETCH,		"texelfetch",		"Texel fetch test"		},
		{ ACCESS_TYPE_IMAGE_LOAD,		"imageload",		"Image load test"		},
		{ ACCESS_TYPE_IMAGE_STORE,		"imagestore",		"Image store test"		},
		{ ACCESS_TYPE_IMAGE_ATOMICS,	"imageatomics",		"Image atomics test"	},
	};

	static const struct
	{
		vk::VkFormat	format;
		const char*		name;
	} formats[] =
	{
		{ vk::VK_FORMAT_R8G8B8A8_UNORM,	"rgba8"	},
		{ vk::VK_FORMAT_R32_SINT,		"r32i"	},
		{ vk::VK_FORMAT_R32_UINT,		"r32ui"	},
	};

	static const struct
	{
		bool			pipelineProtectedAccess;
		const char*		name;
	} protectedAccess[] =
	{
		{ false, "default"},
#ifndef CTS_USES_VULKANSC
		{ true, "protected_access"},
#endif
	};
	static const struct
	{
		vk::VkPipelineCreateFlags	flags;
		const char*					name;
	} flags[] =
	{
		{ (vk::VkPipelineCreateFlagBits)0u,						"none"},
#ifndef CTS_USES_VULKANSC
		{ vk::VK_PIPELINE_CREATE_PROTECTED_ACCESS_ONLY_BIT_EXT, "protected_access_only"},
		{ vk::VK_PIPELINE_CREATE_NO_PROTECTED_ACCESS_BIT_EXT,	"no_protected_access"},
#endif
	};

	for (int shaderTypeNdx = 0; shaderTypeNdx < DE_LENGTH_OF_ARRAY(shaderTypes); ++shaderTypeNdx)
	{
		const glu::ShaderType				shaderType = shaderTypes[shaderTypeNdx].type;
		de::MovePtr<tcu::TestCaseGroup>		shaderGroup(new tcu::TestCaseGroup(testCtx, shaderTypes[shaderTypeNdx].name, shaderTypes[shaderTypeNdx].desc));

		for (int protectedAccessNdx = 0; protectedAccessNdx < DE_LENGTH_OF_ARRAY(protectedAccess); ++protectedAccessNdx) {
			de::MovePtr<tcu::TestCaseGroup>		protectedAccessGroup(new tcu::TestCaseGroup(testCtx, protectedAccess[protectedAccessNdx].name, ""));
			for (int flagsNdx = 0; flagsNdx < DE_LENGTH_OF_ARRAY(flags); ++flagsNdx) {
				de::MovePtr<tcu::TestCaseGroup>		flagsGroup(new tcu::TestCaseGroup(testCtx, flags[flagsNdx].name, ""));
				if (!protectedAccess[protectedAccessNdx].pipelineProtectedAccess && flags[flagsNdx].flags != 0u) continue;
				for (int accessNdx = 0; accessNdx < DE_LENGTH_OF_ARRAY(accessTypes); ++accessNdx)
				{
					const AccessType					accessType = accessTypes[accessNdx].type;

					if (shaderType == glu::SHADERTYPE_COMPUTE && accessType == ACCESS_TYPE_IMAGE_STORE) // \note already tested in other tests
						continue;

					de::MovePtr<tcu::TestCaseGroup>		accessTypeGroup(new tcu::TestCaseGroup(testCtx, accessTypes[accessNdx].name, accessTypes[accessNdx].desc));

					if (accessType == ACCESS_TYPE_IMAGE_ATOMICS)
					{
						for (deUint32 atomicOpI = 0; atomicOpI < ATOMIC_OPERATION_LAST; ++atomicOpI)
						{
							const AtomicOperation				atomicOp = (AtomicOperation)atomicOpI;
							de::MovePtr<tcu::TestCaseGroup>		operationGroup(new tcu::TestCaseGroup(testCtx, getAtomicOperationCaseName(atomicOp).c_str(), ""));

							for (deUint32 formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(formats); formatNdx++)
							{
								const vk::VkFormat		format = formats[formatNdx].format;

								if (format != vk::VK_FORMAT_R32_UINT && format != vk::VK_FORMAT_R32_SINT)
									continue;

								operationGroup->addChild(new ImageAccessTestCase(testCtx, formats[formatNdx].name, "", Params(shaderType, accessType, format, atomicOp, protectedAccess[protectedAccessNdx].pipelineProtectedAccess, flags[flagsNdx].flags)));
							}

							accessTypeGroup->addChild(operationGroup.release());
						}
					}
					else
					{
						for (deUint32 formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(formats); formatNdx++)
						{
							const vk::VkFormat		format = formats[formatNdx].format;

							accessTypeGroup->addChild(new ImageAccessTestCase(testCtx, formats[formatNdx].name, "", Params(shaderType, accessType, format, ATOMIC_OPERATION_LAST, protectedAccess[protectedAccessNdx].pipelineProtectedAccess, flags[flagsNdx].flags)));
						}
					}

					flagsGroup->addChild(accessTypeGroup.release());
				}
				protectedAccessGroup->addChild(flagsGroup.release());
			}
			shaderGroup->addChild(protectedAccessGroup.release());
		}

		accessGroup->addChild(shaderGroup.release());
	}

#ifndef CTS_USES_VULKANSC
	{
		Params params(glu::SHADERTYPE_COMPUTE, ACCESS_TYPE_IMAGE_LOAD, vk::VK_FORMAT_R8G8B8A8_UNORM, ATOMIC_OPERATION_LAST, false, vk::VK_PIPELINE_CREATE_PROTECTED_ACCESS_ONLY_BIT_EXT);
		params.useMaintenance5 = true;
		de::MovePtr<tcu::TestCaseGroup> miscGroup(new tcu::TestCaseGroup(testCtx, "misc", ""));
		miscGroup->addChild(new ImageAccessTestCase(testCtx, "maintenance5_protected_access", "", params));
		params.flags = vk::VK_PIPELINE_CREATE_NO_PROTECTED_ACCESS_BIT_EXT;
		miscGroup->addChild(new ImageAccessTestCase(testCtx, "maintenance5_no_protected_access", "", params));
		accessGroup->addChild(miscGroup.release());
	}
#endif // CTS_USES_VULKANSC

	return accessGroup.release();
}

} // ProtectedMem
} // vkt
