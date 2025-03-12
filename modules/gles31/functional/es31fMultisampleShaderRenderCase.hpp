#ifndef _ES31FMULTISAMPLESHADERRENDERCASE_HPP
#define _ES31FMULTISAMPLESHADERRENDERCASE_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 3.1 Module
 * -------------------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
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
 * \brief Multisample shader render case
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tes31TestCase.hpp"

#include <map>

namespace tcu
{
class Surface;
} // namespace tcu

namespace glu
{
class ShaderProgram;
} // namespace glu

namespace deqp
{
namespace gles31
{
namespace Functional
{
namespace MultisampleShaderRenderUtil
{

class QualityWarning : public tcu::Exception
{
public:
    QualityWarning(const std::string &message);
};

class MultisampleRenderCase : public TestCase
{
public:
    enum RenderTarget
    {
        TARGET_DEFAULT = 0,
        TARGET_TEXTURE,
        TARGET_RENDERBUFFER,

        TARGET_LAST
    };
    enum Flags
    {
        FLAG_PER_ITERATION_SHADER               = 1,
        FLAG_VERIFY_MSAA_TEXTURE_SAMPLE_BUFFERS = 2, // !< flag set: each sample layer is verified by verifySampleBuffer
    };

    MultisampleRenderCase(Context &context, const char *name, const char *desc, int numSamples, RenderTarget target,
                          int renderSize, int flags = 0);
    virtual ~MultisampleRenderCase(void);

    virtual void init(void);
    virtual void deinit(void);
    IterateResult iterate(void);

private:
    virtual void preDraw(void);
    virtual void postDraw(void);
    virtual void preTest(void);
    virtual void postTest(void);
    virtual std::string getIterationDescription(int iteration) const;

    void drawOneIteration(void);
    void verifyResultImageAndSetResult(const tcu::Surface &resultImage);
    void verifyResultBuffersAndSetResult(const std::vector<tcu::Surface> &resultBuffers);
    virtual std::string genVertexSource(int numTargetSamples) const;
    virtual std::string genFragmentSource(int numTargetSamples) const = 0;
    std::string genMSSamplerSource(int numTargetSamples) const;
    std::string genMSTextureResolverSource(int numTargetSamples) const;
    std::string genMSTextureLayerFetchSource(int numTargetSamples) const;
    virtual bool verifyImage(const tcu::Surface &resultImage) = 0;
    virtual bool verifySampleBuffers(const std::vector<tcu::Surface> &resultBuffers);
    virtual void setupRenderData(void);

    glw::GLint getMaxConformantSampleCount(glw::GLenum target, glw::GLenum internalFormat);

protected:
    struct Attrib
    {
        int offset;
        int stride;
    };

    const int m_numRequestedSamples;
    const RenderTarget m_renderTarget;
    const int m_renderSize;
    const bool m_perIterationShader;
    const bool m_verifyTextureSampleBuffers;
    int32_t m_numTargetSamples;

    uint32_t m_buffer;
    uint32_t m_resolveBuffer;
    glu::ShaderProgram *m_program;
    uint32_t m_fbo;
    uint32_t m_fboTexture;
    glu::ShaderProgram *m_textureSamplerProgram;
    uint32_t m_fboRbo;
    uint32_t m_resolveFbo;
    uint32_t m_resolveFboTexture;
    int m_iteration;
    int m_numIterations;
    uint32_t m_renderMode;
    int32_t m_renderCount;
    uint32_t m_renderVao;
    uint32_t m_resolveVao;

    std::string m_renderSceneDescription;
    std::map<std::string, Attrib> m_renderAttribs;
};

} // namespace MultisampleShaderRenderUtil
} // namespace Functional
} // namespace gles31
} // namespace deqp

#endif // _ES31FMULTISAMPLESHADERRENDERCASE_HPP
