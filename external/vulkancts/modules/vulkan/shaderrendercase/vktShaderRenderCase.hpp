#ifndef _VKTSHADERRENDERCASE_HPP
#define _VKTSHADERRENDERCASE_HPP
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

#include "tcuTexture.hpp"
#include "tcuSurface.hpp"

#include "deMemory.h"
#include "deUniquePtr.hpp"

#include "vktTestCaseUtil.hpp"

#include "vkDefs.hpp"
#include "vkPrograms.hpp"
#include "vkRef.hpp"
#include "vkMemUtil.hpp"

namespace vkt
{
namespace shaderrendercase
{

class QuadGrid;

class TextureBinding
{
};

// ShaderEvalContext.

class ShaderEvalContext
{
public:
    // Limits.
    enum
    {
        MAX_USER_ATTRIBS    = 4,
        MAX_TEXTURES        = 4,
    };

    struct ShaderSampler
    {
        tcu::Sampler                sampler;
        const tcu::Texture2D*       tex2D;
        const tcu::TextureCube*     texCube;
        const tcu::Texture2DArray*  tex2DArray;
        const tcu::Texture3D*       tex3D;

        inline ShaderSampler (void)
            : tex2D     (DE_NULL)
            , texCube   (DE_NULL)
            , tex2DArray(DE_NULL)
            , tex3D     (DE_NULL)
        {
        }
    };

                            ShaderEvalContext       (const QuadGrid& quadGrid);
                            ~ShaderEvalContext      (void);

    void                    reset                   (float sx, float sy);

    // Inputs.
    tcu::Vec4               coords;
    tcu::Vec4               unitCoords;
    tcu::Vec4               constCoords;

    tcu::Vec4               in[MAX_USER_ATTRIBS];
    ShaderSampler           textures[MAX_TEXTURES];

    // Output.
    tcu::Vec4               color;
    bool                    isDiscarded;

    // Functions.
    inline void             discard                 (void)  { isDiscarded = true; }
    tcu::Vec4               texture2D               (int unitNdx, const tcu::Vec2& coords);

private:
    const QuadGrid&         quadGrid;
};


typedef void (*ShaderEvalFunc) (ShaderEvalContext& c);

inline void evalCoordsPassthroughX      (ShaderEvalContext& c) { c.color.x() = c.coords.x(); }
inline void evalCoordsPassthroughXY     (ShaderEvalContext& c) { c.color.xy() = c.coords.swizzle(0,1); }
inline void evalCoordsPassthroughXYZ    (ShaderEvalContext& c) { c.color.xyz() = c.coords.swizzle(0,1,2); }
inline void evalCoordsPassthrough       (ShaderEvalContext& c) { c.color = c.coords; }
inline void evalCoordsSwizzleWZYX       (ShaderEvalContext& c) { c.color = c.coords.swizzle(3,2,1,0); }

// ShaderEvaluator
// Either inherit a class with overridden evaluate() or just pass in an evalFunc.

class ShaderEvaluator
{
public:
                        ShaderEvaluator         (void);
                        ShaderEvaluator         (ShaderEvalFunc evalFunc);
    virtual             ~ShaderEvaluator        (void);

    virtual void        evaluate                (ShaderEvalContext& ctx);

private:
                        ShaderEvaluator         (const ShaderEvaluator&);   // not allowed!
    ShaderEvaluator&    operator=               (const ShaderEvaluator&);   // not allowed!

    ShaderEvalFunc      m_evalFunc;
};


template<typename Instance>
class ShaderRenderCase : public vkt::TestCase
{
public:
							ShaderRenderCase	(tcu::TestContext& testCtx,
												const std::string& name,
												const std::string& description,
												bool isVertexCase,
												ShaderEvalFunc evalFunc)
								: vkt::TestCase(testCtx, name, description)
								, m_isVertexCase(isVertexCase)
								, m_evaluator(new ShaderEvaluator(evalFunc))
							{}

							ShaderRenderCase	(tcu::TestContext& testCtx,
												const std::string& name,
												const std::string& description,
												bool isVertexCase,
												ShaderEvaluator* evaluator)
								: vkt::TestCase(testCtx, name, description)
								, m_isVertexCase(isVertexCase)
								, m_evaluator(evaluator)
							{}


	virtual					~ShaderRenderCase	(void) {}
	virtual	void			initPrograms		(vk::ProgramCollection<glu::ProgramSources>& programCollection) const
							{
								programCollection.add(m_name + "_vert") << glu::VertexSource(m_vertShaderSource);
								programCollection.add(m_name + "_frag") << glu::FragmentSource(m_fragShaderSource);
							}

	virtual	TestInstance*	createInstance		(Context& context) const { return new Instance(context, m_name, m_isVertexCase, *m_evaluator); }

protected:
    std::string				m_vertShaderSource;
    std::string				m_fragShaderSource;

private:
	bool 					m_isVertexCase;
	ShaderEvaluator*		m_evaluator;

};

// ShaderRenderCaseInstance.

class ShaderRenderCaseInstance : public vkt::TestInstance
{
public:
							ShaderRenderCaseInstance	(Context& context, const std::string& name, bool isVertexCase, ShaderEvaluator& evaluator);
	virtual					~ShaderRenderCaseInstance	(void);
	virtual tcu::TestStatus	iterate						(void);

protected:
	virtual void			setupShaderData				(void);
	virtual void			setup						(void);
	virtual void			setupUniforms				(const tcu::Vec4& constCoords);

	tcu::IVec2				getViewportSize				(void) const;

	std::vector<tcu::Mat4>	m_userAttribTransforms;
	tcu::Vec4				m_clearColor;

private:

	void					setupDefaultInputs			(void);

	void					render						(tcu::Surface& result, const QuadGrid& quadGrid);
	void					computeVertexReference		(tcu::Surface& result, const QuadGrid& quadGrid);
	void					computeFragmentReference	(tcu::Surface& result, const QuadGrid& quadGrid);
	bool					compareImages				(const tcu::Surface& resImage, const tcu::Surface& refImage, float errorThreshold);

	std::string				m_name;
	bool					m_isVertexCase;
	ShaderEvaluator&		m_evaluator;

	const tcu::IVec2		m_renderSize;
	const vk::VkFormat		m_colorFormat;

	vk::Move<vk::VkImage>				m_colorImage;
	de::MovePtr<vk::Allocation>			m_colorImageAlloc;
	vk::Move<vk::VkAttachmentView> 		m_colorAttachmentView;

	vk::Move<vk::VkRenderPass>			m_renderPass;
	vk::Move<vk::VkFramebuffer>			m_framebuffer;
	vk::Move<vk::VkPipelineLayout>		m_pipelineLayout;
	vk::Move<vk::VkPipeline>			m_graphicsPipeline;

	vk::Move<vk::VkShaderModule>		m_vertexShaderModule;
	vk::Move<vk::VkShaderModule>		m_fragmentShaderModule;
	vk::Move<vk::VkShader>				m_vertexShader;
	vk::Move<vk::VkShader>				m_fragmentShader;

	vk::Move<vk::VkDynamicViewportState>		m_viewportState;
	vk::Move<vk::VkDynamicRasterState>			m_rasterState;
	vk::Move<vk::VkDynamicColorBlendState>		m_colorBlendState;

	vk::Move<vk::VkBuffer>						m_vertexBuffer;
	de::MovePtr<vk::Allocation>				m_vertexBufferAlloc;

	vk::Move<vk::VkBuffer>						m_indiceBuffer;
	de::MovePtr<vk::Allocation>				m_indiceBufferAlloc;

	vk::Move<vk::VkDescriptorSetLayout>		m_descriptorSetLayout;
	vk::Move<vk::VkBuffer>						m_uniformBuffer;
	de::MovePtr<vk::Allocation>				m_uniformBufferAlloc;
	vk::Move<vk::VkBufferView>				m_uniformBufferView;

	vk::Move<vk::VkDescriptorPool>		m_descriptorPool;
	vk::Move<vk::VkDescriptorSet>		m_descriptorSet;

	vk::Move<vk::VkCmdPool>						m_cmdPool;
	vk::Move<vk::VkCmdBuffer>					m_cmdBuffer;

	vk::Move<vk::VkFence>				m_fence;
};


} // shaderrendercase
} // vkt

#endif // _VKTSHADERRENDERCASE_HPP
