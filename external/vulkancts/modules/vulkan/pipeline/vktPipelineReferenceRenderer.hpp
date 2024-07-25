#ifndef _VKTPIPELINEREFERENCERENDERER_HPP
#define _VKTPIPELINEREFERENCERENDERER_HPP
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
 * \brief Reference renderer.
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "vktPipelineVertexUtil.hpp"
#include "tcuVector.hpp"
#include "tcuVectorType.hpp"
#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "rrRenderState.hpp"
#include "rrRenderer.hpp"
#include <cstring>

namespace vkt
{

namespace pipeline
{

tcu::Vec4 swizzle(const tcu::Vec4 &color, const tcu::UVec4 &swizzle);

class ColorVertexShader : public rr::VertexShader
{
public:
    ColorVertexShader(void) : rr::VertexShader(2, 2)
    {
        m_inputs[0].type = rr::GENERICVECTYPE_FLOAT;
        m_inputs[1].type = rr::GENERICVECTYPE_FLOAT;

        m_outputs[0].type = rr::GENERICVECTYPE_FLOAT;
        m_outputs[1].type = rr::GENERICVECTYPE_FLOAT;
    }

    virtual ~ColorVertexShader(void)
    {
    }

    virtual void shadeVertices(const rr::VertexAttrib *inputs, rr::VertexPacket *const *packets,
                               const int numPackets) const
    {
        tcu::Vec4 position;
        tcu::Vec4 color;

        for (int packetNdx = 0; packetNdx < numPackets; packetNdx++)
        {
            rr::VertexPacket *const packet = packets[packetNdx];

            readVertexAttrib(position, inputs[0], packet->instanceNdx, packet->vertexNdx);
            readVertexAttrib(color, inputs[1], packet->instanceNdx, packet->vertexNdx);

            packet->outputs[0] = position;
            packet->outputs[1] = color;
            packet->position   = position;
        }
    }
};

class ColorVertexShaderDualSource : public rr::VertexShader
{
public:
    ColorVertexShaderDualSource(void) : rr::VertexShader(3, 3)
    {
        m_inputs[0].type = rr::GENERICVECTYPE_FLOAT;
        m_inputs[1].type = rr::GENERICVECTYPE_FLOAT;
        m_inputs[2].type = rr::GENERICVECTYPE_FLOAT;

        m_outputs[0].type = rr::GENERICVECTYPE_FLOAT;
        m_outputs[1].type = rr::GENERICVECTYPE_FLOAT;
        m_outputs[2].type = rr::GENERICVECTYPE_FLOAT;
    }

    virtual ~ColorVertexShaderDualSource(void)
    {
    }

    virtual void shadeVertices(const rr::VertexAttrib *inputs, rr::VertexPacket *const *packets,
                               const int numPackets) const
    {
        tcu::Vec4 position;
        tcu::Vec4 color0;
        tcu::Vec4 color1;

        for (int packetNdx = 0; packetNdx < numPackets; packetNdx++)
        {
            rr::VertexPacket *const packet = packets[packetNdx];

            readVertexAttrib(position, inputs[0], packet->instanceNdx, packet->vertexNdx);
            readVertexAttrib(color0, inputs[1], packet->instanceNdx, packet->vertexNdx);
            readVertexAttrib(color1, inputs[2], packet->instanceNdx, packet->vertexNdx);

            packet->outputs[0] = position;
            packet->outputs[1] = color0;
            packet->outputs[2] = color1;
            packet->position   = position;
        }
    }
};

class TexCoordVertexShader : public rr::VertexShader
{
public:
    TexCoordVertexShader(void) : rr::VertexShader(2, 2)
    {
        m_inputs[0].type = rr::GENERICVECTYPE_FLOAT;
        m_inputs[1].type = rr::GENERICVECTYPE_FLOAT;

        m_outputs[0].type = rr::GENERICVECTYPE_FLOAT;
        m_outputs[1].type = rr::GENERICVECTYPE_FLOAT;
    }

    virtual ~TexCoordVertexShader(void)
    {
    }

    virtual void shadeVertices(const rr::VertexAttrib *inputs, rr::VertexPacket *const *packets,
                               const int numPackets) const
    {
        tcu::Vec4 position;
        tcu::Vec4 texCoord;

        for (int packetNdx = 0; packetNdx < numPackets; packetNdx++)
        {
            rr::VertexPacket *const packet = packets[packetNdx];

            readVertexAttrib(position, inputs[0], packet->instanceNdx, packet->vertexNdx);
            readVertexAttrib(texCoord, inputs[1], packet->instanceNdx, packet->vertexNdx);

            packet->outputs[0] = position;
            packet->outputs[1] = texCoord;
            packet->position   = position;
        }
    }
};

class ColorFragmentShader : public rr::FragmentShader
{
private:
    const tcu::TextureFormat m_colorFormat;
    const tcu::TextureFormat m_depthStencilFormat;
    const bool m_disableVulkanDepthRange;

public:
    ColorFragmentShader(const tcu::TextureFormat &colorFormat, const tcu::TextureFormat &depthStencilFormat,
                        const bool disableVulkanDepthRange = false)
        : rr::FragmentShader(2, 1)
        , m_colorFormat(colorFormat)
        , m_depthStencilFormat(depthStencilFormat)
        , m_disableVulkanDepthRange(disableVulkanDepthRange)
    {
        const tcu::TextureChannelClass channelClass = tcu::getTextureChannelClass(m_colorFormat.type);

        m_inputs[0].type  = rr::GENERICVECTYPE_FLOAT;
        m_inputs[1].type  = rr::GENERICVECTYPE_FLOAT;
        m_outputs[0].type = (channelClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER)   ? rr::GENERICVECTYPE_INT32 :
                            (channelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER) ? rr::GENERICVECTYPE_UINT32 :
                                                                                          rr::GENERICVECTYPE_FLOAT;
    }

    virtual ~ColorFragmentShader(void)
    {
    }

    virtual void shadeFragments(rr::FragmentPacket *packets, const int numPackets,
                                const rr::FragmentShadingContext &context) const
    {
        for (int packetNdx = 0; packetNdx < numPackets; packetNdx++)
        {
            const rr::FragmentPacket &packet = packets[packetNdx];

            // Reference renderer uses OpenGL depth range of -1..1, and does the viewport depth transform using
            // formula (position.z+1)/2. For Vulkan the depth range is 0..1 and the vertex depth is mapped as is, so
            // the values gets overridden here, unless depth clip control extension changes the depth clipping to use
            // the OpenGL depth range.
            if (!m_disableVulkanDepthRange && (m_depthStencilFormat.order == tcu::TextureFormat::D ||
                                               m_depthStencilFormat.order == tcu::TextureFormat::DS))
            {
                for (int fragNdx = 0; fragNdx < 4; fragNdx++)
                {
                    const tcu::Vec4 vtxPosition = rr::readVarying<float>(packet, context, 0, fragNdx);
                    rr::writeFragmentDepth(context, packetNdx, fragNdx, 0, vtxPosition.z());
                }
            }

            for (int fragNdx = 0; fragNdx < 4; fragNdx++)
            {
                const tcu::Vec4 vtxColor = rr::readVarying<float>(packet, context, 1, fragNdx);
                rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, vtxColor);
            }
        }
    }
};

class ColorFragmentShaderDualSource : public rr::FragmentShader
{
private:
    const tcu::TextureFormat m_colorFormat;
    const tcu::TextureFormat m_depthStencilFormat;

public:
    ColorFragmentShaderDualSource(const tcu::TextureFormat &colorFormat, const tcu::TextureFormat &depthStencilFormat)
        : rr::FragmentShader(3, 1)
        , m_colorFormat(colorFormat)
        , m_depthStencilFormat(depthStencilFormat)
    {
        const tcu::TextureChannelClass channelClass = tcu::getTextureChannelClass(m_colorFormat.type);

        m_inputs[0].type = rr::GENERICVECTYPE_FLOAT;
        m_inputs[1].type = rr::GENERICVECTYPE_FLOAT;
        m_inputs[2].type = rr::GENERICVECTYPE_FLOAT;

        m_outputs[0].type = (channelClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER)   ? rr::GENERICVECTYPE_INT32 :
                            (channelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER) ? rr::GENERICVECTYPE_UINT32 :
                                                                                          rr::GENERICVECTYPE_FLOAT;
    }

    virtual ~ColorFragmentShaderDualSource(void)
    {
    }

    virtual void shadeFragments(rr::FragmentPacket *packets, const int numPackets,
                                const rr::FragmentShadingContext &context) const
    {
        for (int packetNdx = 0; packetNdx < numPackets; packetNdx++)
        {
            const rr::FragmentPacket &packet = packets[packetNdx];

            if (m_depthStencilFormat.order == tcu::TextureFormat::D ||
                m_depthStencilFormat.order == tcu::TextureFormat::DS)
            {
                for (int fragNdx = 0; fragNdx < 4; fragNdx++)
                {
                    const tcu::Vec4 vtxPosition = rr::readVarying<float>(packet, context, 0, fragNdx);
                    rr::writeFragmentDepth(context, packetNdx, fragNdx, 0, vtxPosition.z());
                }
            }

            for (int fragNdx = 0; fragNdx < 4; fragNdx++)
            {
                const tcu::Vec4 vtxColor0 = rr::readVarying<float>(packet, context, 1, fragNdx);
                const tcu::Vec4 vtxColor1 = rr::readVarying<float>(packet, context, 2, fragNdx);
                rr::writeFragmentOutputDualSource(context, packetNdx, fragNdx, 0, vtxColor0, vtxColor1);
            }
        }
    }
};

class CoordinateCaptureFragmentShader : public rr::FragmentShader
{
public:
    CoordinateCaptureFragmentShader(void) : rr::FragmentShader(2, 1)
    {
        m_inputs[0].type  = rr::GENERICVECTYPE_FLOAT;
        m_inputs[1].type  = rr::GENERICVECTYPE_FLOAT;
        m_outputs[0].type = rr::GENERICVECTYPE_FLOAT;
    }

    virtual ~CoordinateCaptureFragmentShader(void)
    {
    }

    virtual void shadeFragments(rr::FragmentPacket *packets, const int numPackets,
                                const rr::FragmentShadingContext &context) const
    {
        for (int packetNdx = 0; packetNdx < numPackets; packetNdx++)
        {
            const rr::FragmentPacket &packet = packets[packetNdx];

            for (int fragNdx = 0; fragNdx < 4; fragNdx++)
            {
                const tcu::Vec4 vtxTexCoord = rr::readVarying<float>(packet, context, 1, fragNdx);
                rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, vtxTexCoord);
            }
        }
    }
};

class Program
{
public:
    virtual ~Program(void)
    {
    }

    virtual rr::Program getReferenceProgram(void) const = 0;
};

class CoordinateCaptureProgram : public Program
{
private:
    TexCoordVertexShader m_vertexShader;
    CoordinateCaptureFragmentShader m_fragmentShader;

public:
    CoordinateCaptureProgram(void)
    {
    }

    virtual ~CoordinateCaptureProgram(void)
    {
    }

    virtual rr::Program getReferenceProgram(void) const
    {
        return rr::Program(&m_vertexShader, &m_fragmentShader);
    }
};

class ReferenceRenderer
{
public:
    ReferenceRenderer(int surfaceWidth, int surfaceHeight, int numSamples, const tcu::TextureFormat &colorFormat,
                      const tcu::TextureFormat &depthStencilFormat, const rr::Program *const program);

    virtual ~ReferenceRenderer(void);

    void colorClear(const tcu::Vec4 &color);

    void draw(const rr::RenderState &renderState, const rr::PrimitiveType primitive,
              const std::vector<Vertex4RGBA> &vertexBuffer);

    void draw(const rr::RenderState &renderState, const rr::PrimitiveType primitive,
              const std::vector<Vertex4RGBARGBA> &vertexBuffer);

    void draw(const rr::RenderState &renderState, const rr::PrimitiveType primitive,
              const std::vector<Vertex4Tex4> &vertexBuffer);

    tcu::PixelBufferAccess getAccess(void);
    tcu::PixelBufferAccess getDepthStencilAccess(void);
    const rr::ViewportState getViewportState(void) const;

private:
    rr::Renderer m_renderer;

    const int m_surfaceWidth;
    const int m_surfaceHeight;
    const int m_numSamples;

    const tcu::TextureFormat m_colorFormat;
    const tcu::TextureFormat m_depthStencilFormat;

    tcu::TextureLevel m_colorBuffer;
    tcu::TextureLevel m_resolveColorBuffer;
    tcu::TextureLevel m_depthStencilBuffer;
    tcu::TextureLevel m_resolveDepthStencilBuffer;

    rr::RenderTarget *m_renderTarget;
    const rr::Program *m_program;
};

rr::TestFunc mapVkCompareOp(vk::VkCompareOp compareFunc);
rr::PrimitiveType mapVkPrimitiveTopology(vk::VkPrimitiveTopology primitiveTopology);
rr::BlendFunc mapVkBlendFactor(vk::VkBlendFactor blendFactor);
rr::BlendEquation mapVkBlendOp(vk::VkBlendOp blendOp);
tcu::BVec4 mapVkColorComponentFlags(vk::VkColorComponentFlags flags);
rr::StencilOp mapVkStencilOp(vk::VkStencilOp stencilOp);

} // namespace pipeline
} // namespace vkt

#endif // _VKTPIPELINEREFERENCERENDERER_HPP
