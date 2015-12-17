#ifndef _VKTSHADEREXECUTOR_HPP
#define _VKTSHADEREXECUTOR_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
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
 * \brief Vulkan ShaderExecutor
 *//*--------------------------------------------------------------------*/

#include "deSharedPtr.hpp"

#include "vktTestCase.hpp"
#include "vkMemUtil.hpp"
#include "vkBuilderUtil.hpp"

#include "gluVarType.hpp"

#include "tcuTexture.hpp"

#include <vector>

namespace vkt
{
namespace shaderexecutor
{

using namespace vk;

//! Shader input / output variable declaration.
struct Symbol
{
	std::string				name;		//!< Symbol name.
	glu::VarType			varType;	//!< Symbol type.

	Symbol (void) {}
	Symbol (const std::string& name_, const glu::VarType& varType_) : name(name_), varType(varType_) {}
};

//! Complete shader specification.
struct ShaderSpec
{
	std::vector<Symbol>		inputs;
	std::vector<Symbol>		outputs;
	std::string				globalDeclarations;	//!< These are placed into global scope. Can contain uniform declarations for example.
	std::string				source;				//!< Source snippet to be executed.

	ShaderSpec (void) {}
};

// UniformSetup

class UniformDataBase;
class ShaderExecutor;

typedef de::SharedPtr<de::UniquePtr<UniformDataBase> > UniformDataSp;

class UniformSetup
{
public:
										UniformSetup		(void) {}
	virtual								~UniformSetup		(void) {}

	void								addData				(UniformDataBase* uniformData)
										{
											m_uniforms.push_back(UniformDataSp(new de::UniquePtr<UniformDataBase>(uniformData)));
										}

	const std::vector<UniformDataSp>&	uniforms			(void) const
										{
											return m_uniforms;
										}

private:
										UniformSetup		(const UniformSetup&);	// not allowed!
	UniformSetup&						operator=			(const UniformSetup&);	// not allowed!

	std::vector<UniformDataSp>			m_uniforms;
};

//! Base class for shader executor.
class ShaderExecutor
{
public:
	virtual					~ShaderExecutor		(void);

	//! Log executor details (program etc.).
	virtual void			log					(tcu::TestLog& log) const = 0;

	//! Execute
	virtual void			execute				(const Context& ctx, int numValues, const void* const* inputs, void* const* outputs) = 0;

	virtual void 			setShaderSources	(SourceCollections& programCollection) const = 0;

	void					setUniforms			(const UniformSetup* uniformSetup)
												{
													m_uniformSetup = de::MovePtr<const UniformSetup>(uniformSetup);
												};

	void					setupUniformData	(const VkDevice&			vkDevice,
												 const DeviceInterface&		vk,
												 const deUint32				queueFamilyIndex,
												 Allocator&					memAlloc,
												 deUint32					bindingLocation,
												 VkDescriptorType			descriptorType,
												 deUint32					size,
												 const void*				dataPtr);

	void					setupSamplerData	(const VkDevice&			vkDevice,
												 const DeviceInterface&		vk,
												 const deUint32				queueFamilyIndex,
												 Allocator&					memAlloc,
												 deUint32					bindingLocation,
												 deUint32					numSamplers,
												 const tcu::Sampler&		refSampler,
												 const tcu::TextureFormat&	texFormat,
												 const tcu::IVec3&			texSize,
												 VkImageType				imageType,
												 VkImageViewType			imageViewType,
												 const void*				data);

protected:
							ShaderExecutor		(const ShaderSpec& shaderSpec, glu::ShaderType shaderType);

	void					addUniforms			(const VkDevice& vkDevice, const DeviceInterface& vk, const deUint32 queueFamilyIndex, Allocator& memAlloc);

	void					uploadUniforms		(DescriptorSetUpdateBuilder& descriptorSetUpdateBuilder, VkDescriptorSet descriptorSet);

	class UniformInfo;
	typedef de::SharedPtr<de::UniquePtr<UniformInfo> >			UniformInfoSp;

	class SamplerUniform;
	typedef de::SharedPtr<de::UniquePtr<SamplerUniform> >		SamplerUniformSp;

	typedef de::SharedPtr<Unique<VkBuffer> >			VkBufferSp;
	typedef de::SharedPtr<Unique<VkImage> >				VkImageSp;
	typedef de::SharedPtr<Unique<VkImageView> >			VkImageViewSp;
	typedef de::SharedPtr<Unique<VkSampler> >			VkSamplerSp;
	typedef de::SharedPtr<Allocation>					AllocationSp;

	class UniformInfo
	{
	public:
									UniformInfo			(void) {}
		virtual						~UniformInfo		(void) {}
		virtual bool				isSamplerArray		(void) const { return false; }
		virtual bool				isBufferUniform		(void) const { return false; }
		virtual bool				isSamplerUniform	(void) const { return false; }

		VkDescriptorType			type;
		deUint32					location;
	};

	class BufferUniform : public UniformInfo
	{
	public:
									BufferUniform		(void) {}
		virtual						~BufferUniform		(void) {}
		virtual bool				isBufferUniform		(void) const { return true; }

		VkBufferSp					buffer;
		AllocationSp				alloc;
		VkDescriptorBufferInfo		descriptor;
	};

	class SamplerUniform : public UniformInfo
	{
	public:
									SamplerUniform		(void) {}
		virtual						~SamplerUniform		(void) {}
		virtual bool				isSamplerUniform	(void) const { return true; }
		VkImageSp					image;
		VkImageViewSp				imageView;
		VkSamplerSp					sampler;
		AllocationSp				alloc;
		VkDescriptorImageInfo		descriptor;
	};

	class SamplerArrayUniform : public UniformInfo
	{
	public:
											SamplerArrayUniform		(void) {}
		virtual								~SamplerArrayUniform	(void) {}
		virtual bool						isSamplerArray			(void) const { return true; }

		std::vector<SamplerUniformSp>		uniforms;
	};

	Move<VkImage>							createCombinedImage			(const VkDevice&				vkDevice,
																		 const DeviceInterface&			vk,
																		 const deUint32					queueFamilyIndex,
																		 const tcu::IVec3&				texSize,
																		 const VkFormat					format,
																		 const VkImageType				imageType,
																		 const VkImageViewType			imageViewType,
																		 const VkImageUsageFlags		usage,
																		 const VkImageTiling			tiling);

	de::MovePtr<Allocation>					uploadImage					(const VkDevice&				vkDevice,
																		 const DeviceInterface&			vk,
																		 Allocator&						memAlloc,
																		 const tcu::TextureFormat&		texFormat,
																		 const tcu::IVec3&				texSize,
																		 const void*					data,
																		 const VkImage&					vkTexture,
																		 const VkImageAspectFlags		aspectMask);

	de::MovePtr<SamplerUniform>				createSamplerUniform		(const VkDevice&				vkDevice,
																		 const DeviceInterface&			vk,
																		 const deUint32					queueFamilyIndex,
																		 Allocator&						memAlloc,
																		 deUint32						bindingLocation,
																		 const tcu::Sampler&			refSampler,
																		 const tcu::TextureFormat&		texFormat,
																		 const tcu::IVec3&				texSize,
																		 VkImageType					imageType,
																		 VkImageViewType				imageViewType,
																		 const void*					data);

	const ShaderSpec									m_shaderSpec;
	const glu::ShaderType								m_shaderType;

	std::vector<UniformInfoSp>							m_uniformInfos;
	de::MovePtr<const UniformSetup>						m_uniformSetup;
	DescriptorSetLayoutBuilder							m_descriptorSetLayoutBuilder;
	DescriptorPoolBuilder								m_descriptorPoolBuilder;

};

inline tcu::TestLog& operator<< (tcu::TestLog& log, const ShaderExecutor* executor) { executor->log(log); return log; }
inline tcu::TestLog& operator<< (tcu::TestLog& log, const ShaderExecutor& executor) { executor.log(log); return log; }

ShaderExecutor* createExecutor(glu::ShaderType shaderType, const ShaderSpec& shaderSpec);

class UniformDataBase
{
public:
							UniformDataBase		(deUint32 bindingLocation)
													: m_bindingLocation		(bindingLocation)
												{
												}
	virtual					~UniformDataBase	(void) {}
	virtual void			setup				(ShaderExecutor&, const VkDevice&, const DeviceInterface&, const deUint32, Allocator&) const = 0;

protected:
	const deUint32			m_bindingLocation;
};

template<typename T>
class UniformData : public UniformDataBase
{
public:
							UniformData			(deUint32 bindingLocation, VkDescriptorType descriptorType, const T data);
	virtual					~UniformData		(void);
	virtual void			setup				(ShaderExecutor& executor, const VkDevice& vkDevice, const DeviceInterface& vk, const deUint32 queueFamilyIndex, Allocator& memAlloc) const;

private:
	VkDescriptorType		m_descriptorType;
	T 						m_data;
};

template<typename T>
UniformData<T>::UniformData (deUint32 bindingLocation, VkDescriptorType descriptorType, const T data)
	: UniformDataBase		(bindingLocation)
	, m_descriptorType		(descriptorType)
	, m_data				(data)
{
}

template<typename T>
UniformData<T>::~UniformData (void)
{
}

template<typename T>
void UniformData<T>::setup (ShaderExecutor& executor, const VkDevice& vkDevice, const DeviceInterface& vk, const deUint32 queueFamilyIndex, Allocator& memAlloc) const
{
	executor.setupUniformData(vkDevice, vk, queueFamilyIndex, memAlloc, m_bindingLocation, m_descriptorType, sizeof(T), &m_data);
}

class SamplerUniformData : public UniformDataBase
{
public:
							SamplerUniformData	(deUint32						bindingLocation,
												 deUint32						numSamplers,
												 const tcu::Sampler&			refSampler,
												 const tcu::TextureFormat&		texFormat,
												 const tcu::IVec3&				texSize,
												 VkImageType					imageType,
												 VkImageViewType				imageViewType,
												 const void*					data);
	virtual					~SamplerUniformData	(void);
	virtual void			setup				(ShaderExecutor& executor, const VkDevice& vkDevice, const DeviceInterface& vk, const deUint32 queueFamilyIndex, Allocator& memAlloc) const;

private:
	deUint32					m_numSamplers;
	const tcu::Sampler			m_refSampler;
	const tcu::TextureFormat	m_texFormat;
	const tcu::IVec3			m_texSize;
	VkImageType					m_imageType;
	VkImageViewType				m_imageViewType;
	const void*					m_data;
};

} // shaderexecutor
} // vkt

#endif // _VKTSHADEREXECUTOR_HPP
