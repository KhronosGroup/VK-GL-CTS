#ifndef _VKTSHADERRENDER_HPP
#define _VKTSHADERRENDER_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
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

#include "tcuTexture.hpp"
#include "tcuSurface.hpp"

#include "deMemory.h"
#include "deSharedPtr.hpp"
#include "deUniquePtr.hpp"

#include "vkDefs.hpp"
#include "vkPrograms.hpp"
#include "vkRef.hpp"
#include "vkMemUtil.hpp"
#include "vkBuilderUtil.hpp"

#include "vktTestCaseUtil.hpp"

namespace vkt
{
namespace sr
{

class LineStream
{
public:
						LineStream		(int indent = 0)	{ m_indent = indent; }
						~LineStream		(void)				{}

	const char*			str				(void) const		{ m_string = m_stream.str(); return m_string.c_str(); }
	LineStream&			operator<<		(const char* line)	{ for (int i = 0; i < m_indent; i++) { m_stream << "\t"; } m_stream << line << "\n"; return *this; }

private:
	int					m_indent;
	std::ostringstream	m_stream;
	mutable std::string	m_string;
};

class QuadGrid;
class ShaderRenderCaseInstance;

class TextureBinding
{
public:
	enum Type
	{
		TYPE_NONE = 0,
		TYPE_2D,
		TYPE_CUBE_MAP,
		TYPE_2D_ARRAY,
		TYPE_3D,

		TYPE_LAST
	};

										TextureBinding		(const tcu::Archive&	archive,
															const char*				filename,
															const Type				type,
															const tcu::Sampler&		sampler);
										~TextureBinding		(void);
	Type								getType				(void) const { return m_type;		}
	const tcu::Sampler&					getSampler			(void) const { return m_sampler;	}
	const tcu::Texture2D&				get2D				(void) const { DE_ASSERT(getType() == TYPE_2D && m_binding.tex2D !=NULL); return *m_binding.tex2D; }

private:
										TextureBinding		(const TextureBinding&);	// not allowed!
	TextureBinding&						operator=			(const TextureBinding&);	// not allowed!

	static de::MovePtr<tcu::Texture2D>	loadTexture2D		(const tcu::Archive& archive, const char* filename);

	Type								m_type;
	tcu::Sampler						m_sampler;

	union
	{
		const tcu::Texture2D*	tex2D;
	} m_binding;
};

typedef de::SharedPtr<TextureBinding> TextureBindingSp;

// ShaderEvalContext.

class ShaderEvalContext
{
public:
	// Limits.
	enum
	{
		MAX_USER_ATTRIBS	= 4,
		MAX_TEXTURES		= 4
	};

	struct ShaderSampler
	{
		tcu::Sampler				sampler;
		const tcu::Texture2D*		tex2D;
		const tcu::TextureCube*		texCube;
		const tcu::Texture2DArray*	tex2DArray;
		const tcu::Texture3D*		tex3D;

		inline ShaderSampler (void)
			: tex2D		(DE_NULL)
			, texCube	(DE_NULL)
			, tex2DArray(DE_NULL)
			, tex3D		(DE_NULL)
		{
		}
	};

							ShaderEvalContext		(const QuadGrid& quadGrid);
							~ShaderEvalContext		(void);

	void					reset					(float sx, float sy);

	// Inputs.
	tcu::Vec4				coords;
	tcu::Vec4				unitCoords;
	tcu::Vec4				constCoords;

	tcu::Vec4				in[MAX_USER_ATTRIBS];
	ShaderSampler			textures[MAX_TEXTURES];

	// Output.
	tcu::Vec4				color;
	bool					isDiscarded;

	// Functions.
	inline void				discard					(void)  { isDiscarded = true; }
	tcu::Vec4				texture2D				(int unitNdx, const tcu::Vec2& coords);

private:
	const QuadGrid&			m_quadGrid;
};

typedef void (*ShaderEvalFunc) (ShaderEvalContext& c);

inline void evalCoordsPassthroughX		(ShaderEvalContext& c) { c.color.x() = c.coords.x(); }
inline void evalCoordsPassthroughXY		(ShaderEvalContext& c) { c.color.xy() = c.coords.swizzle(0,1); }
inline void evalCoordsPassthroughXYZ	(ShaderEvalContext& c) { c.color.xyz() = c.coords.swizzle(0,1,2); }
inline void evalCoordsPassthrough		(ShaderEvalContext& c) { c.color = c.coords; }
inline void evalCoordsSwizzleWZYX		(ShaderEvalContext& c) { c.color = c.coords.swizzle(3,2,1,0); }

// ShaderEvaluator
// Either inherit a class with overridden evaluate() or just pass in an evalFunc.

class ShaderEvaluator
{
public:
							ShaderEvaluator			(void);
							ShaderEvaluator			(const ShaderEvalFunc evalFunc);
	virtual					~ShaderEvaluator		(void);

	virtual void			evaluate				(ShaderEvalContext& ctx) const;

private:
							ShaderEvaluator			(const ShaderEvaluator&);   // not allowed!
	ShaderEvaluator&		operator=				(const ShaderEvaluator&);   // not allowed!

	const ShaderEvalFunc	m_evalFunc;
};

// UniformSetup

typedef void (*UniformSetupFunc) (ShaderRenderCaseInstance& instance, const tcu::Vec4& constCoords);

class UniformSetup
{
public:
							UniformSetup			(void);
							UniformSetup			(const UniformSetupFunc setup);
	virtual					~UniformSetup			(void);
	virtual void			setup					(ShaderRenderCaseInstance& instance, const tcu::Vec4& constCoords) const;

private:
							UniformSetup			(const UniformSetup&);	// not allowed!
	UniformSetup&			operator=				(const UniformSetup&);	// not allowed!

	const UniformSetupFunc	m_setupFunc;
};

typedef void (*AttributeSetupFunc) (ShaderRenderCaseInstance& instance, deUint32 numVertices);

class ShaderRenderCase : public vkt::TestCase
{
public:
													ShaderRenderCase	(tcu::TestContext&			testCtx,
																		 const std::string&			name,
																		 const std::string&			description,
																		 const bool					isVertexCase,
																		 const ShaderEvalFunc		evalFunc,
																		 const UniformSetup*		uniformSetup,
																		 const AttributeSetupFunc	attribFunc);

													ShaderRenderCase	(tcu::TestContext&			testCtx,
																		 const std::string&			name,
																		 const std::string&			description,
																		 const bool					isVertexCase,
																		 const ShaderEvaluator*		evaluator,
																		 const UniformSetup*		uniformSetup,
																		 const AttributeSetupFunc	attribFunc);


	virtual											~ShaderRenderCase	(void);
	virtual	void									initPrograms		(vk::SourceCollections& programCollection) const;
	virtual	TestInstance*							createInstance		(Context& context) const;

protected:
	std::string										m_vertShaderSource;
	std::string										m_fragShaderSource;

	const bool										m_isVertexCase;
	const de::UniquePtr<const ShaderEvaluator>		m_evaluator;
	const de::UniquePtr<const UniformSetup>			m_uniformSetup;
	const AttributeSetupFunc						m_attribFunc;
};


enum BaseUniformType
{
// Bool
	UB_FALSE,
	UB_TRUE,

// BVec4
	UB4_FALSE,
	UB4_TRUE,

// Integers
	UI_ZERO,
	UI_ONE,
	UI_TWO,
	UI_THREE,
	UI_FOUR,
	UI_FIVE,
	UI_SIX,
	UI_SEVEN,
	UI_EIGHT,
	UI_ONEHUNDREDONE,

// IVec2
	UI2_MINUS_ONE,
	UI2_ZERO,
	UI2_ONE,
	UI2_TWO,
	UI2_THREE,
	UI2_FOUR,
	UI2_FIVE,

// IVec3
	UI3_MINUS_ONE,
	UI3_ZERO,
	UI3_ONE,
	UI3_TWO,
	UI3_THREE,
	UI3_FOUR,
	UI3_FIVE,

// IVec4
	UI4_MINUS_ONE,
	UI4_ZERO,
	UI4_ONE,
	UI4_TWO,
	UI4_THREE,
	UI4_FOUR,
	UI4_FIVE,

// Float
	UF_ZERO,
	UF_ONE,
	UF_TWO,
	UF_THREE,
	UF_FOUR,
	UF_FIVE,
	UF_SIX,
	UF_SEVEN,
	UF_EIGHT,

	UF_HALF,
	UF_THIRD,
	UF_FOURTH,
	UF_FIFTH,
	UF_SIXTH,
	UF_SEVENTH,
	UF_EIGHTH,

// Vec2
	UV2_MINUS_ONE,
	UV2_ZERO,
	UV2_ONE,
	UV2_TWO,
	UV2_THREE,

	UV2_HALF,

// Vec3
	UV3_MINUS_ONE,
	UV3_ZERO,
	UV3_ONE,
	UV3_TWO,
	UV3_THREE,

	UV3_HALF,

// Vec4
	UV4_MINUS_ONE,
	UV4_ZERO,
	UV4_ONE,
	UV4_TWO,
	UV4_THREE,

	UV4_HALF,

	UV4_BLACK,
	UV4_GRAY,
	UV4_WHITE
};

enum BaseAttributeType
{
// User attributes
	A_IN0,
	A_IN1,
	A_IN2,
	A_IN3,

// Matrices
	MAT2,
	MAT2x3,
	MAT2x4,
	MAT3x2,
	MAT3,
	MAT3x4,
	MAT4x2,
	MAT4x3,
	MAT4
};

// ShaderRenderCaseInstance.

class ShaderRenderCaseInstance : public vkt::TestInstance
{
public:
														ShaderRenderCaseInstance	(Context&					context,
																					const bool					isVertexCase,
																					const ShaderEvaluator&		evaluator,
																					const UniformSetup&			uniformSetup,
																					const AttributeSetupFunc	attribFunc);

	virtual												~ShaderRenderCaseInstance	(void);
	virtual tcu::TestStatus								iterate						(void);

	void												addAttribute				(deUint32			bindingLocation,
																					vk::VkFormat		format,
																					deUint32			sizePerElement,
																					deUint32			count,
																					const void*			data);
	void												useAttribute				(deUint32			bindingLocation,
																					BaseAttributeType	type);

	template<typename T>
	void												addUniform					(deUint32				bindingLocation,
																					vk::VkDescriptorType	descriptorType,
																					const T&				data);
	void												addUniform					(deUint32				bindingLocation,
																					vk::VkDescriptorType	descriptorType,
																					size_t					dataSize,
																					const void*				data);
	void												useUniform					(deUint32				bindingLocation,
																					BaseUniformType			type);
	void												useSampler2D				(deUint32				bindingLocation,
																					deUint32				textureId);

protected:
	virtual void										setup						(void);
	virtual void										setupUniforms				(const tcu::Vec4& constCoords);

	const tcu::UVec2									getViewportSize				(void) const;

	std::vector<tcu::Mat4>								m_userAttribTransforms;
	const tcu::Vec4										m_clearColor;
	std::vector<TextureBindingSp>						m_textures;

	vk::Allocator&										m_memAlloc;

private:

	void												setupTextures				(void);
	de::MovePtr<vk::Allocation>							uploadImage2D				(const tcu::Texture2D&			refTexture,
																					 const vk::VkImage&				vkTexture);
	vk::Move<vk::VkImage>								createImage2D				(const tcu::Texture2D&			texture,
																					 const vk::VkFormat				format,
																					 const vk::VkImageUsageFlags	usage,
																					 const vk::VkImageTiling		tiling);
	void												copyTilingImageToOptimal	(const vk::VkImage&				srcImage,
																					 const vk::VkImage&				dstImage,
																					 deUint32						width,
																					 deUint32						height);

	void												setupUniformData			(deUint32 bindingLocation, size_t size, const void* dataPtr);
	void												setupDefaultInputs			(const QuadGrid& quadGrid);

	void												render						(tcu::Surface& result, const QuadGrid& quadGrid);
	void												computeVertexReference		(tcu::Surface& result, const QuadGrid& quadGrid);
	void												computeFragmentReference	(tcu::Surface& result, const QuadGrid& quadGrid);
	bool												compareImages				(const tcu::Surface&	resImage,
																					 const tcu::Surface&	refImage,
																					 float					errorThreshold);

	const bool											m_isVertexCase;
	const ShaderEvaluator&								m_evaluator;
	const UniformSetup&									m_uniformSetup;
	const AttributeSetupFunc							m_attribFunc;

	struct EnabledBaseAttribute
	{
		deUint32			location;
		BaseAttributeType	type;
	};
	std::vector<EnabledBaseAttribute>					m_enabledBaseAttributes;

	const tcu::UVec2									m_renderSize;
	const vk::VkFormat									m_colorFormat;

	vk::Move<vk::VkImage>								m_colorImage;
	de::MovePtr<vk::Allocation>							m_colorImageAlloc;
	vk::Move<vk::VkImageView>							m_colorImageView;

	vk::Move<vk::VkRenderPass>							m_renderPass;
	vk::Move<vk::VkFramebuffer>							m_framebuffer;
	vk::Move<vk::VkPipelineLayout>						m_pipelineLayout;
	vk::Move<vk::VkPipeline>							m_graphicsPipeline;

	vk::Move<vk::VkShaderModule>						m_vertexShaderModule;
	vk::Move<vk::VkShaderModule>						m_fragmentShaderModule;

	vk::Move<vk::VkBuffer>								m_indiceBuffer;
	de::MovePtr<vk::Allocation>							m_indiceBufferAlloc;

	vk::Move<vk::VkDescriptorSetLayout>					m_descriptorSetLayout;

	vk::Move<vk::VkDescriptorPool>						m_descriptorPool;
	vk::Move<vk::VkDescriptorSet>						m_descriptorSet;

	vk::Move<vk::VkCommandPool>							m_cmdPool;
	vk::Move<vk::VkCommandBuffer>						m_cmdBuffer;

	vk::Move<vk::VkFence>								m_fence;

	vk::DescriptorSetLayoutBuilder						m_descriptorSetLayoutBuilder;
	vk::DescriptorPoolBuilder							m_descriptorPoolBuilder;
	vk::DescriptorSetUpdateBuilder						m_descriptorSetUpdateBuilder;

	typedef de::SharedPtr<vk::Unique<vk::VkBuffer> >		VkBufferSp;

	typedef de::SharedPtr<vk::Unique<vk::VkImage> >			VkImageSp;
	typedef de::SharedPtr<vk::Unique<vk::VkImageView> >		VkImageViewSp;
	typedef de::SharedPtr<vk::Unique<vk::VkSampler> >		VkSamplerSp;
	typedef de::SharedPtr<vk::Allocation>					AllocationSp;

	class UniformInfo
	{
	public:
									UniformInfo		(void) {}
		virtual						~UniformInfo	(void) {}

		vk::VkDescriptorType		type;
		deUint32					location;
	};

	class BufferUniform : public UniformInfo
	{
	public:
									BufferUniform	(void) {}
		virtual						~BufferUniform	(void) {}

		VkBufferSp					buffer;
		AllocationSp				alloc;
		vk::VkDescriptorBufferInfo	descriptor;
	};

	class SamplerUniform : public UniformInfo
	{
	public:
									SamplerUniform	(void) {}
		virtual						~SamplerUniform	(void) {}

		VkImageSp					image;
		VkImageViewSp				imageView;
		VkSamplerSp					sampler;
		AllocationSp				alloc;
		vk::VkDescriptorImageInfo	descriptor;
	};

	typedef de::SharedPtr<de::UniquePtr<UniformInfo> >	UniformInfoSp;
	std::vector<UniformInfoSp>							m_uniformInfos;

	std::vector<vk::VkVertexInputBindingDescription>	m_vertexBindingDescription;
	std::vector<vk::VkVertexInputAttributeDescription>	m_vertexattributeDescription;

	std::vector<VkBufferSp>								m_vertexBuffers;
	std::vector<AllocationSp>							m_vertexBufferAllocs;
};

template<typename T>
void ShaderRenderCaseInstance::addUniform (deUint32 bindingLocation, vk::VkDescriptorType descriptorType, const T& data)
{
	addUniform(bindingLocation, descriptorType, sizeof(T), &data);
}

} // sr
} // vkt

#endif // _VKTSHADERRENDER_HPP
