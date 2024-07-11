#ifndef _SGLRCONTEXT_HPP
#define _SGLRCONTEXT_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES Utilities
 * ------------------------------------------------
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
 * \brief Simplified GLES reference context.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuSurface.hpp"
#include "gluRenderContext.hpp"
#include "sglrShaderProgram.hpp"

/*--------------------------------------------------------------------*//*!
 * \brief Reference OpenGL API implementation
 *//*--------------------------------------------------------------------*/
namespace sglr
{

// Abstract drawing context with GL-style API

class Context
{
public:
    Context(glu::ContextType type) : m_type(type)
    {
    }
    virtual ~Context(void)
    {
    }

    virtual int getWidth(void) const  = DE_NULL;
    virtual int getHeight(void) const = DE_NULL;

    virtual void activeTexture(uint32_t texture)               = DE_NULL;
    virtual void viewport(int x, int y, int width, int height) = DE_NULL;

    virtual void bindTexture(uint32_t target, uint32_t texture)            = DE_NULL;
    virtual void genTextures(int numTextures, uint32_t *textures)          = DE_NULL;
    virtual void deleteTextures(int numTextures, const uint32_t *textures) = DE_NULL;

    virtual void bindFramebuffer(uint32_t target, uint32_t framebuffer)                = DE_NULL;
    virtual void genFramebuffers(int numFramebuffers, uint32_t *framebuffers)          = DE_NULL;
    virtual void deleteFramebuffers(int numFramebuffers, const uint32_t *framebuffers) = DE_NULL;

    virtual void bindRenderbuffer(uint32_t target, uint32_t renderbuffer)                 = DE_NULL;
    virtual void genRenderbuffers(int numRenderbuffers, uint32_t *renderbuffers)          = DE_NULL;
    virtual void deleteRenderbuffers(int numRenderbuffers, const uint32_t *renderbuffers) = DE_NULL;

    virtual void pixelStorei(uint32_t pname, int param)                                              = DE_NULL;
    virtual void texImage1D(uint32_t target, int level, uint32_t internalFormat, int width, int border, uint32_t format,
                            uint32_t type, const void *data)                                         = DE_NULL;
    virtual void texImage2D(uint32_t target, int level, uint32_t internalFormat, int width, int height, int border,
                            uint32_t format, uint32_t type, const void *data)                        = DE_NULL;
    virtual void texImage3D(uint32_t target, int level, uint32_t internalFormat, int width, int height, int depth,
                            int border, uint32_t format, uint32_t type, const void *data)            = DE_NULL;
    virtual void texSubImage1D(uint32_t target, int level, int xoffset, int width, uint32_t format, uint32_t type,
                               const void *data)                                                     = DE_NULL;
    virtual void texSubImage2D(uint32_t target, int level, int xoffset, int yoffset, int width, int height,
                               uint32_t format, uint32_t type, const void *data)                     = DE_NULL;
    virtual void texSubImage3D(uint32_t target, int level, int xoffset, int yoffset, int zoffset, int width, int height,
                               int depth, uint32_t format, uint32_t type, const void *data)          = DE_NULL;
    virtual void copyTexImage1D(uint32_t target, int level, uint32_t internalFormat, int x, int y, int width,
                                int border)                                                          = DE_NULL;
    virtual void copyTexImage2D(uint32_t target, int level, uint32_t internalFormat, int x, int y, int width,
                                int height, int border)                                              = DE_NULL;
    virtual void copyTexSubImage1D(uint32_t target, int level, int xoffset, int x, int y, int width) = DE_NULL;
    virtual void copyTexSubImage2D(uint32_t target, int level, int xoffset, int yoffset, int x, int y, int width,
                                   int height)                                                       = DE_NULL;
    virtual void copyTexSubImage3D(uint32_t target, int level, int xoffset, int yoffset, int zoffset, int x, int y,
                                   int width, int height)                                            = DE_NULL;

    virtual void texStorage2D(uint32_t target, int levels, uint32_t internalFormat, int width, int height) = DE_NULL;
    virtual void texStorage3D(uint32_t target, int levels, uint32_t internalFormat, int width, int height,
                              int depth)                                                                   = DE_NULL;

    virtual void texParameteri(uint32_t target, uint32_t pname, int value) = DE_NULL;

    virtual void framebufferTexture2D(uint32_t target, uint32_t attachment, uint32_t textarget, uint32_t texture,
                                      int level)                = DE_NULL;
    virtual void framebufferTextureLayer(uint32_t target, uint32_t attachment, uint32_t texture, int level,
                                         int layer)             = DE_NULL;
    virtual void framebufferRenderbuffer(uint32_t target, uint32_t attachment, uint32_t renderbuffertarget,
                                         uint32_t renderbuffer) = DE_NULL;
    virtual uint32_t checkFramebufferStatus(uint32_t target)    = DE_NULL;

    virtual void getFramebufferAttachmentParameteriv(uint32_t target, uint32_t attachment, uint32_t pname,
                                                     int *params) = DE_NULL;

    virtual void renderbufferStorage(uint32_t target, uint32_t internalformat, int width, int height) = DE_NULL;
    virtual void renderbufferStorageMultisample(uint32_t target, int samples, uint32_t internalFormat, int width,
                                                int height)                                           = DE_NULL;

    virtual void bindBuffer(uint32_t target, uint32_t buffer)           = DE_NULL;
    virtual void genBuffers(int numBuffers, uint32_t *buffers)          = DE_NULL;
    virtual void deleteBuffers(int numBuffers, const uint32_t *buffers) = DE_NULL;

    virtual void bufferData(uint32_t target, intptr_t size, const void *data, uint32_t usage)     = DE_NULL;
    virtual void bufferSubData(uint32_t target, intptr_t offset, intptr_t size, const void *data) = DE_NULL;

    virtual void clearColor(float red, float green, float blue, float alpha) = DE_NULL;
    virtual void clearDepthf(float depth)                                    = DE_NULL;
    virtual void clearStencil(int stencil)                                   = DE_NULL;

    virtual void clear(uint32_t buffers)                                                  = DE_NULL;
    virtual void clearBufferiv(uint32_t buffer, int drawbuffer, const int *value)         = DE_NULL;
    virtual void clearBufferfv(uint32_t buffer, int drawbuffer, const float *value)       = DE_NULL;
    virtual void clearBufferuiv(uint32_t buffer, int drawbuffer, const uint32_t *value)   = DE_NULL;
    virtual void clearBufferfi(uint32_t buffer, int drawbuffer, float depth, int stencil) = DE_NULL;
    virtual void scissor(int x, int y, int width, int height)                             = DE_NULL;

    virtual void enable(uint32_t cap)  = DE_NULL;
    virtual void disable(uint32_t cap) = DE_NULL;

    virtual void stencilFunc(uint32_t func, int ref, uint32_t mask)                                 = DE_NULL;
    virtual void stencilOp(uint32_t sfail, uint32_t dpfail, uint32_t dppass)                        = DE_NULL;
    virtual void stencilFuncSeparate(uint32_t face, uint32_t func, int ref, uint32_t mask)          = DE_NULL;
    virtual void stencilOpSeparate(uint32_t face, uint32_t sfail, uint32_t dpfail, uint32_t dppass) = DE_NULL;

    virtual void depthFunc(uint32_t func)       = DE_NULL;
    virtual void depthRangef(float n, float f)  = DE_NULL;
    virtual void depthRange(double n, double f) = DE_NULL;

    virtual void polygonOffset(float factor, float units) = DE_NULL;
    virtual void provokingVertex(uint32_t convention)     = DE_NULL;
    virtual void primitiveRestartIndex(uint32_t index)    = DE_NULL;

    virtual void blendEquation(uint32_t mode)                                                              = DE_NULL;
    virtual void blendEquationSeparate(uint32_t modeRGB, uint32_t modeAlpha)                               = DE_NULL;
    virtual void blendFunc(uint32_t src, uint32_t dst)                                                     = DE_NULL;
    virtual void blendFuncSeparate(uint32_t srcRGB, uint32_t dstRGB, uint32_t srcAlpha, uint32_t dstAlpha) = DE_NULL;
    virtual void blendColor(float red, float green, float blue, float alpha)                               = DE_NULL;

    virtual void colorMask(bool r, bool g, bool b, bool a)         = DE_NULL;
    virtual void depthMask(bool mask)                              = DE_NULL;
    virtual void stencilMask(uint32_t mask)                        = DE_NULL;
    virtual void stencilMaskSeparate(uint32_t face, uint32_t mask) = DE_NULL;

    virtual void blitFramebuffer(int srcX0, int srcY0, int srcX1, int srcY1, int dstX0, int dstY0, int dstX1, int dstY1,
                                 uint32_t mask, uint32_t filter) = DE_NULL;

    virtual void invalidateSubFramebuffer(uint32_t target, int numAttachments, const uint32_t *attachments, int x,
                                          int y, int width, int height)                                  = DE_NULL;
    virtual void invalidateFramebuffer(uint32_t target, int numAttachments, const uint32_t *attachments) = DE_NULL;

    virtual void bindVertexArray(uint32_t array)                                 = DE_NULL;
    virtual void genVertexArrays(int numArrays, uint32_t *vertexArrays)          = DE_NULL;
    virtual void deleteVertexArrays(int numArrays, const uint32_t *vertexArrays) = DE_NULL;

    virtual void vertexAttribPointer(uint32_t index, int size, uint32_t type, bool normalized, int stride,
                                     const void *pointer)              = DE_NULL;
    virtual void vertexAttribIPointer(uint32_t index, int size, uint32_t type, int stride,
                                      const void *pointer)             = DE_NULL;
    virtual void enableVertexAttribArray(uint32_t index)               = DE_NULL;
    virtual void disableVertexAttribArray(uint32_t index)              = DE_NULL;
    virtual void vertexAttribDivisor(uint32_t index, uint32_t divisor) = DE_NULL;

    virtual void vertexAttrib1f(uint32_t index, float)                                    = DE_NULL;
    virtual void vertexAttrib2f(uint32_t index, float, float)                             = DE_NULL;
    virtual void vertexAttrib3f(uint32_t index, float, float, float)                      = DE_NULL;
    virtual void vertexAttrib4f(uint32_t index, float, float, float, float)               = DE_NULL;
    virtual void vertexAttribI4i(uint32_t index, int32_t, int32_t, int32_t, int32_t)      = DE_NULL;
    virtual void vertexAttribI4ui(uint32_t index, uint32_t, uint32_t, uint32_t, uint32_t) = DE_NULL;

    virtual int32_t getAttribLocation(uint32_t program, const char *name) = DE_NULL;

    virtual void uniform1f(int32_t index, float)                                                       = DE_NULL;
    virtual void uniform1i(int32_t index, int32_t)                                                     = DE_NULL;
    virtual void uniform1fv(int32_t index, int32_t count, const float *)                               = DE_NULL;
    virtual void uniform2fv(int32_t index, int32_t count, const float *)                               = DE_NULL;
    virtual void uniform3fv(int32_t index, int32_t count, const float *)                               = DE_NULL;
    virtual void uniform4fv(int32_t index, int32_t count, const float *)                               = DE_NULL;
    virtual void uniform1iv(int32_t index, int32_t count, const int32_t *)                             = DE_NULL;
    virtual void uniform2iv(int32_t index, int32_t count, const int32_t *)                             = DE_NULL;
    virtual void uniform3iv(int32_t index, int32_t count, const int32_t *)                             = DE_NULL;
    virtual void uniform4iv(int32_t index, int32_t count, const int32_t *)                             = DE_NULL;
    virtual void uniformMatrix3fv(int32_t location, int32_t count, bool transpose, const float *value) = DE_NULL;
    virtual void uniformMatrix4fv(int32_t location, int32_t count, bool transpose, const float *value) = DE_NULL;
    virtual int32_t getUniformLocation(uint32_t program, const char *name)                             = DE_NULL;

    virtual void lineWidth(float) = DE_NULL;

    virtual void drawArrays(uint32_t mode, int first, int count)                             = DE_NULL;
    virtual void drawArraysInstanced(uint32_t mode, int first, int count, int instanceCount) = DE_NULL;
    virtual void drawElements(uint32_t mode, int count, uint32_t type, const void *indices)  = DE_NULL;
    virtual void drawElementsInstanced(uint32_t mode, int count, uint32_t type, const void *indices,
                                       int instanceCount)                                    = DE_NULL;
    virtual void drawElementsBaseVertex(uint32_t mode, int count, uint32_t type, const void *indices,
                                        int baseVertex)                                      = DE_NULL;
    virtual void drawElementsInstancedBaseVertex(uint32_t mode, int count, uint32_t type, const void *indices,
                                                 int instanceCount, int baseVertex)          = DE_NULL;
    virtual void drawRangeElements(uint32_t mode, uint32_t start, uint32_t end, int count, uint32_t type,
                                   const void *indices)                                      = DE_NULL;
    virtual void drawRangeElementsBaseVertex(uint32_t mode, uint32_t start, uint32_t end, int count, uint32_t type,
                                             const void *indices, int baseVertex)            = DE_NULL;
    virtual void drawArraysIndirect(uint32_t mode, const void *indirect)                     = DE_NULL;
    virtual void drawElementsIndirect(uint32_t mode, uint32_t type, const void *indirect)    = DE_NULL;

    virtual void multiDrawArrays(uint32_t mode, const int *first, const int *count, int primCount) = DE_NULL;
    virtual void multiDrawElements(uint32_t mode, const int *count, uint32_t type, const void **indices,
                                   int primCount)                                                  = DE_NULL;
    virtual void multiDrawElementsBaseVertex(uint32_t mode, const int *count, uint32_t type, const void **indices,
                                             int primCount, const int *baseVertex)                 = DE_NULL;

    virtual uint32_t createProgram(ShaderProgram *program) = DE_NULL;
    virtual void useProgram(uint32_t program)              = DE_NULL;
    virtual void deleteProgram(uint32_t program)           = DE_NULL;

    virtual void readPixels(int x, int y, int width, int height, uint32_t format, uint32_t type, void *data) = DE_NULL;
    virtual uint32_t getError(void)                                                                          = DE_NULL;
    virtual void finish(void)                                                                                = DE_NULL;

    virtual void getIntegerv(uint32_t pname, int *params) = DE_NULL;
    virtual const char *getString(uint32_t pname)         = DE_NULL;

    // Helpers implemented by Context.
    virtual void texImage2D(uint32_t target, int level, uint32_t internalFormat, const tcu::Surface &src);
    virtual void texImage2D(uint32_t target, int level, uint32_t internalFormat, int width, int height);
    virtual void texSubImage2D(uint32_t target, int level, int xoffset, int yoffset, const tcu::Surface &src);
    virtual void readPixels(tcu::Surface &dst, int x, int y, int width, int height);

    glu::ContextType getType(void)
    {
        return m_type;
    }

private:
    const glu::ContextType m_type;
} DE_WARN_UNUSED_TYPE;

} // namespace sglr

#endif // _SGLRCONTEXT_HPP
