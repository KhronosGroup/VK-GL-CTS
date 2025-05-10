#ifndef _GLCAPICOVERAGETESTS_HPP
#define _GLCAPICOVERAGETESTS_HPP
/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2024 The Khronos Group Inc.
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
 */ /*!
 * \file
 * \brief
 */ /*-------------------------------------------------------------------*/

/**
 */ /*!
 * \file  glcApiCoverageTests.hpp
 * \brief Conformance tests for OpenGL and OpenGL ES API coverage.
 */ /*-------------------------------------------------------------------*/

#include "glcTestCase.hpp"
#include "glwDefs.hpp"
#include "tcuDefs.hpp"

#include <map>

namespace glcts
{

struct enumTestRec
{
    const char *name;
    glw::GLint value;
};

/** class to handle parsing configuration xml and run all coverage tests */
class ApiCoverageTestCase : public deqp::TestCase
{
public:
    /* Public methods */
    ApiCoverageTestCase(deqp::Context &context);

    void deinit();
    void init();
    tcu::TestNode::IterateResult iterate();

    bool verifyEnum(const std::string &name, const std::string &value);
    bool verifyFunc(const std::string &name);

    glw::GLenum TestCoverageGLGuessColorBufferFormat(void);
    glw::GLsizei TestCoverageGLCalcTargetFormats(glw::GLenum colorBufferFormat, glw::GLenum *textureFormats);

    glw::GLint createDefaultProgram(int mode);

    template <typename... Args>
    void tcu_fail_msg(const std::string &format, Args... args);
    void tcu_msg(const std::string &msg0, const std::string &msg1);

    typedef bool (ApiCoverageTestCase::*test_func_ptr)(void);

    bool TestCoverageGLCallReadBuffer(void);
    bool TestCoverageGLCallDrawRangeElements(void);
    bool TestCoverageGLCallTexImage3D(void);
    bool TestCoverageGLCallTexSubImage3D(void);
    bool TestCoverageGLCallCopyTexSubImage3D(void);
    bool TestCoverageGLCallCompressedTexImage3D(void);
    bool TestCoverageGLCallCompressedTexSubImage3D(void);
    bool TestCoverageGLCallGenQueries(void);
    bool TestCoverageGLCallDeleteQueries(void);
    bool TestCoverageGLCallIsQuery(void);
    bool TestCoverageGLCallBeginQuery(void);
    bool TestCoverageGLCallEndQuery(void);
    bool TestCoverageGLCallGetQueryiv(void);
    bool TestCoverageGLCallGetQueryObjectuiv(void);
    bool TestCoverageGLCallMapBufferRange(void);
    bool TestCoverageGLCallUnmapBuffer(void);
    bool TestCoverageGLCallGetBufferPointerv(void);
    bool TestCoverageGLCallFlushMappedBufferRange(void);
    bool TestCoverageGLCallDrawBuffers(void);
    bool TestCoverageGLCallUniformMatrix2x4fv(void);
    bool TestCoverageGLCallBeginTransformFeedback(void);
    bool TestCoverageGLCallEndTransformFeedback(void);
    bool TestCoverageGLCallBindBufferRange(void);
    bool TestCoverageGLCallBindBufferBase(void);
    bool TestCoverageGLCallTransformFeedbackVaryings(void);
    bool TestCoverageGLCallGetTransformFeedbackVarying(void);
    bool TestCoverageGLCallVertexAttribIPointer(void);
    bool TestCoverageGLCallGetVertexAttribIiv(void);
    bool TestCoverageGLCallGetVertexAttribIuiv(void);
    bool TestCoverageGLCallVertexAttribI4i(void);
    bool TestCoverageGLCallVertexAttribI4ui(void);
    bool TestCoverageGLCallVertexAttribI4iv(void);
    bool TestCoverageGLCallVertexAttribI4uiv(void);
    bool TestCoverageGLCallGetUniformuiv(void);
    bool TestCoverageGLCallGetFragDataLocation(void);
    bool TestCoverageGLCallUniform2ui(void);
    bool TestCoverageGLCallUniform2uiv(void);
    bool TestCoverageGLCallClearBufferiv(void);
    bool TestCoverageGLCallClearBufferuiv(void);
    bool TestCoverageGLCallClearBufferfv(void);
    bool TestCoverageGLCallClearBufferfi(void);
    bool TestCoverageGLCallGetStringi(void);
    bool TestCoverageGLCallBlitFramebuffer(void);
    bool TestCoverageGLCallRenderbufferStorageMultisample(void);
    bool TestCoverageGLCallBindVertexArray(void);
    bool TestCoverageGLCallDeleteVertexArrays(void);
    bool TestCoverageGLCallGenVertexArrays(void);
    bool TestCoverageGLCallIsVertexArray(void);
    bool TestCoverageGLCallDrawArraysInstanced(void);
    bool TestCoverageGLCallDrawElementsInstanced(void);
    bool TestCoverageGLCallCopyBufferSubData(void);
    bool TestCoverageGLCallGetUniformIndices(void);
    bool TestCoverageGLCallGetActiveUniformsiv(void);
    bool TestCoverageGLCallGetUniformBlockIndex(void);
    bool TestCoverageGLCallGetActiveUniformBlockiv(void);
    bool TestCoverageGLCallGetActiveUniformBlockName(void);
    bool TestCoverageGLCallUniformBlockBinding(void);
    bool TestCoverageGLCallGetBufferParameteri64v(void);
    bool TestCoverageGLCallProgramParameteri(void);
    bool TestCoverageGLCallFenceSync(void);
    bool TestCoverageGLCallIsSync(void);
    bool TestCoverageGLCallDeleteSync(void);
    bool TestCoverageGLCallClientWaitSync(void);
    bool TestCoverageGLCallWaitSync(void);
    bool TestCoverageGLCallGetInteger64v(void);
    bool TestCoverageGLCallGetSynciv(void);
    bool TestCoverageGLCallGenSamplers(void);
    bool TestCoverageGLCallDeleteSamplers(void);
    bool TestCoverageGLCallIsSampler(void);
    bool TestCoverageGLCallBindSampler(void);
    bool TestCoverageGLCallSamplerParameteri(void);
    bool TestCoverageGLCallSamplerParameteriv(void);
    bool TestCoverageGLCallSamplerParameterf(void);
    bool TestCoverageGLCallSamplerParameterfv(void);
    bool TestCoverageGLCallGetSamplerParameteriv(void);
    bool TestCoverageGLCallGetSamplerParameterfv(void);
    bool TestCoverageGLCallBindTransformFeedback(void);
    bool TestCoverageGLCallDeleteTransformFeedbacks(void);
    bool TestCoverageGLCallGenTransformFeedbacks(void);
    bool TestCoverageGLCallIsTransformFeedback(void);
    bool TestCoverageGLCallPauseTransformFeedback(void);
    bool TestCoverageGLCallResumeTransformFeedback(void);
    bool TestCoverageGLCallInvalidateFramebuffer(void);
    bool TestCoverageGLCallInvalidateSubFramebuffer(void);
    bool TestCoverageGLCallActiveTexture(void);
    bool TestCoverageGLCallAttachShader(void);
    bool TestCoverageGLCallBindAttribLocation(void);
    bool TestCoverageGLCallBindBuffer(void);
    bool TestCoverageGLCallBindTexture(void);
    bool TestCoverageGLCallBlendColor(void);
    bool TestCoverageGLCallBlendEquation(void);
    bool TestCoverageGLCallBlendEquationSeparate(void);
    bool TestCoverageGLCallBlendFunc(void);
    bool TestCoverageGLCallBlendFuncSeparate(void);
    bool TestCoverageGLCallBufferData(void);
    bool TestCoverageGLCallBufferSubData(void);
    bool TestCoverageGLCallClear(void);
    bool TestCoverageGLCallClearColor(void);
    bool TestCoverageGLCallClearStencil(void);
    bool TestCoverageGLCallColorMask(void);
    bool TestCoverageGLCallCompressedTexImage2D(void);
    bool TestCoverageGLCallCompressedTexSubImage2D(void);
    bool TestCoverageGLCallCopyTexImage2D(void);
    bool TestCoverageGLCallCopyTexSubImage2D(void);
    bool TestCoverageGLCallCreateProgram(void);
    bool TestCoverageGLCallCreateShader(void);
    bool TestCoverageGLCallCullFace(void);
    bool TestCoverageGLCallDeleteBuffers(void);
    bool TestCoverageGLCallDeleteTextures(void);
    bool TestCoverageGLCallDeleteProgram(void);
    bool TestCoverageGLCallDeleteShader(void);
    bool TestCoverageGLCallDetachShader(void);
    bool TestCoverageGLCallDepthFunc(void);
    bool TestCoverageGLCallDepthMask(void);
    bool TestCoverageGLCallDisable(void);
    bool TestCoverageGLCallDisableVertexAttribArray(void);
    bool TestCoverageGLCallDrawArrays(void);
    bool TestCoverageGLCallDrawElements(void);
    bool TestCoverageGLCallEnable(void);
    bool TestCoverageGLCallEnableVertexAttribArray(void);
    bool TestCoverageGLCallFinish(void);
    bool TestCoverageGLCallFlush(void);
    bool TestCoverageGLCallFrontFace(void);
    bool TestCoverageGLCallGetActiveAttrib(void);
    bool TestCoverageGLCallGetActiveUniform(void);
    bool TestCoverageGLCallGetAttachedShaders(void);
    bool TestCoverageGLCallGetAttribLocation(void);
    bool TestCoverageGLCallGetBooleanv(void);
    bool TestCoverageGLCallGetBufferParameteriv(void);
    bool TestCoverageGLCallGenBuffers(void);
    bool TestCoverageGLCallGenTextures(void);
    bool TestCoverageGLCallGetError(void);
    bool TestCoverageGLCallGetFloatv(void);
    bool TestCoverageGLCallGetIntegerv(void);
    bool TestCoverageGLCallGetProgramiv(void);
    bool TestCoverageGLCallGetProgramInfoLog(void);
    bool TestCoverageGLCallGetString(void);
    bool TestCoverageGLCallGetTexParameteriv(void);
    bool TestCoverageGLCallGetTexParameterfv(void);
    bool TestCoverageGLCallGetUniformfv(void);
    bool TestCoverageGLCallGetUniformiv(void);
    bool TestCoverageGLCallGetUniformLocation(void);
    bool TestCoverageGLCallGetVertexAttribfv(void);
    bool TestCoverageGLCallGetVertexAttribiv(void);
    bool TestCoverageGLCallGetVertexAttribPointerv(void);
    bool TestCoverageGLCallHint(void);
    bool TestCoverageGLCallIsBuffer(void);
    bool TestCoverageGLCallIsEnabled(void);
    bool TestCoverageGLCallIsProgram(void);
    bool TestCoverageGLCallIsShader(void);
    bool TestCoverageGLCallIsTexture(void);
    bool TestCoverageGLCallLineWidth(void);
    bool TestCoverageGLCallLinkProgram(void);
    bool TestCoverageGLCallPixelStorei(void);
    bool TestCoverageGLCallPolygonOffset(void);
    bool TestCoverageGLCallReadPixels(void);
    bool TestCoverageGLCallSampleCoverage(void);
    bool TestCoverageGLCallScissor(void);
    bool TestCoverageGLCallStencilFunc(void);
    bool TestCoverageGLCallStencilFuncSeparate(void);
    bool TestCoverageGLCallStencilMask(void);
    bool TestCoverageGLCallStencilMaskSeparate(void);
    bool TestCoverageGLCallStencilOp(void);
    bool TestCoverageGLCallStencilOpSeparate(void);
    bool TestCoverageGLCallTexImage2D(void);
    bool TestCoverageGLCallTexParameteri(void);
    bool TestCoverageGLCallTexParameterf(void);
    bool TestCoverageGLCallTexParameteriv(void);
    bool TestCoverageGLCallTexParameterfv(void);
    bool TestCoverageGLCallTexSubImage2D(void);
    bool TestCoverageGLCallUniform1i(void);
    bool TestCoverageGLCallUniform2i(void);
    bool TestCoverageGLCallUniform3i(void);
    bool TestCoverageGLCallUniform4i(void);
    bool TestCoverageGLCallUniform1f(void);
    bool TestCoverageGLCallUniform2f(void);
    bool TestCoverageGLCallUniform3f(void);
    bool TestCoverageGLCallUniform4f(void);
    bool TestCoverageGLCallUniform1iv(void);
    bool TestCoverageGLCallUniform2iv(void);
    bool TestCoverageGLCallUniform3iv(void);
    bool TestCoverageGLCallUniform4iv(void);
    bool TestCoverageGLCallUniform1fv(void);
    bool TestCoverageGLCallUniform2fv(void);
    bool TestCoverageGLCallUniform3fv(void);
    bool TestCoverageGLCallUniform4fv(void);
    bool TestCoverageGLCallUniformMatrix2fv(void);
    bool TestCoverageGLCallUniformMatrix3fv(void);
    bool TestCoverageGLCallUniformMatrix4fv(void);
    bool TestCoverageGLCallUseProgram(void);
    bool TestCoverageGLCallValidateProgram(void);
    bool TestCoverageGLCallVertexAttrib1f(void);
    bool TestCoverageGLCallVertexAttrib2f(void);
    bool TestCoverageGLCallVertexAttrib3f(void);
    bool TestCoverageGLCallVertexAttrib4f(void);
    bool TestCoverageGLCallVertexAttrib1fv(void);
    bool TestCoverageGLCallVertexAttrib2fv(void);
    bool TestCoverageGLCallVertexAttrib3fv(void);
    bool TestCoverageGLCallVertexAttrib4fv(void);
    bool TestCoverageGLCallVertexAttribPointer(void);
    bool TestCoverageGLCallViewport(void);
    bool TestCoverageGLCallIsRenderbuffer(void);
    bool TestCoverageGLCallBindRenderbuffer(void);
    bool TestCoverageGLCallDeleteRenderbuffers(void);
    bool TestCoverageGLCallGenRenderbuffers(void);
    bool TestCoverageGLCallRenderbufferStorage(void);
    bool TestCoverageGLCallGetRenderbufferParameteriv(void);
    bool TestCoverageGLCallIsFramebuffer(void);
    bool TestCoverageGLCallBindFramebuffer(void);
    bool TestCoverageGLCallDeleteFramebuffers(void);
    bool TestCoverageGLCallGenFramebuffers(void);
    bool TestCoverageGLCallCheckFramebufferStatus(void);
    bool TestCoverageGLCallFramebufferTexture2D(void);
    bool TestCoverageGLCallFramebufferRenderbuffer(void);
    bool TestCoverageGLCallGetFramebufferAttachmentParameteriv(void);
    bool TestCoverageGLCallGenerateMipmap(void);
    bool TestCoverageGLCallCompileShader(void);
    bool TestCoverageGLCallGetShaderiv(void);
    bool TestCoverageGLCallGetShaderInfoLog(void);
    bool TestCoverageGLCallGetShaderSource(void);
    bool TestCoverageGLCallShaderSource(void);
    bool TestCoverageGLCallClearDepthf(void);
    bool TestCoverageGLCallDepthRangef(void);
    bool TestCoverageGLCallFramebufferTexture3DOES(void);
    bool TestCoverageGLCallMapBufferOES(void);
    bool TestCoverageGLCallTexImage3DOES(void);
    bool TestCoverageGLCallTexSubImage3DOES(void);
    bool TestCoverageGLCallCopyTexSubImage3DOES(void);
    bool TestCoverageGLCallCompressedTexImage3DOES(void);
    bool TestCoverageGLCallCompressedTexSubImage3DOES(void);
    bool TestCoverageGLCallShaderBinary(void);
    bool TestCoverageGLCallReleaseShaderCompiler(void);
    bool TestCoverageGLCallGetShaderPrecisionFormat(void);

    bool TestCoverageGLCallPointSize(void);
    bool TestCoverageGLCallPolygonMode(void);
    bool TestCoverageGLCallTexImage1D(void);
    bool TestCoverageGLCallDrawBuffer(void);
    bool TestCoverageGLCallClearDepth(void);
    bool TestCoverageGLCallLogicOp(void);
    bool TestCoverageGLCallPixelStoref(void);
    bool TestCoverageGLCallGetDoublev(void);
    bool TestCoverageGLCallGetTexImage(void);
    bool TestCoverageGLCallGetTexLevelParameterfv(void);
    bool TestCoverageGLCallGetTexLevelParameteriv(void);
    bool TestCoverageGLCallDepthRange(void);
    bool TestCoverageGLCallGetPointerv(void);
    bool TestCoverageGLCallCopyTexImage1D(void);
    bool TestCoverageGLCallCopyTexSubImage1D(void);
    bool TestCoverageGLCallTexSubImage1D(void);
    bool TestCoverageGLCallCompressedTexImage1D(void);
    bool TestCoverageGLCallCompressedTexSubImage1D(void);
    bool TestCoverageGLCallGetCompressedTexImage(void);
    bool TestCoverageGLCallMultiDrawArrays(void);
    bool TestCoverageGLCallMultiDrawElements(void);
    bool TestCoverageGLCallPointParameterf(void);
    bool TestCoverageGLCallPointParameterfv(void);
    bool TestCoverageGLCallPointParameteri(void);
    bool TestCoverageGLCallPointParameteriv(void);
    bool TestCoverageGLCallGetQueryObjectiv(void);
    bool TestCoverageGLCallGetBufferSubData(void); /* Shared with OpenGL ES */
    bool TestCoverageGLCallMapBuffer(void);
    bool TestCoverageGLCallGetVertexAttribdv(void);
    bool TestCoverageGLCallVertexAttrib1d(void);
    bool TestCoverageGLCallVertexAttrib1dv(void);
    bool TestCoverageGLCallVertexAttrib1s(void);
    bool TestCoverageGLCallVertexAttrib1sv(void);
    bool TestCoverageGLCallVertexAttrib2d(void);
    bool TestCoverageGLCallVertexAttrib2dv(void);
    bool TestCoverageGLCallVertexAttrib2s(void);
    bool TestCoverageGLCallVertexAttrib2sv(void);
    bool TestCoverageGLCallVertexAttrib3d(void);
    bool TestCoverageGLCallVertexAttrib3dv(void);
    bool TestCoverageGLCallVertexAttrib3s(void);
    bool TestCoverageGLCallVertexAttrib3sv(void);
    bool TestCoverageGLCallVertexAttrib4Nbv(void);
    bool TestCoverageGLCallVertexAttrib4Niv(void);
    bool TestCoverageGLCallVertexAttrib4Nsv(void);
    bool TestCoverageGLCallVertexAttrib4Nub(void);
    bool TestCoverageGLCallVertexAttrib4Nubv(void);
    bool TestCoverageGLCallVertexAttrib4Nuiv(void);
    bool TestCoverageGLCallVertexAttrib4Nusv(void);
    bool TestCoverageGLCallVertexAttrib4bv(void);
    bool TestCoverageGLCallVertexAttrib4d(void);
    bool TestCoverageGLCallVertexAttrib4dv(void);
    bool TestCoverageGLCallVertexAttrib4iv(void);
    bool TestCoverageGLCallVertexAttrib4s(void);
    bool TestCoverageGLCallVertexAttrib4sv(void);
    bool TestCoverageGLCallVertexAttrib4ubv(void);
    bool TestCoverageGLCallVertexAttrib4uiv(void);
    bool TestCoverageGLCallVertexAttrib4usv(void);
    bool TestCoverageGLCallUniformMatrix2x3fv(void);
    bool TestCoverageGLCallUniformMatrix3x2fv(void);
    bool TestCoverageGLCallUniformMatrix4x2fv(void);
    bool TestCoverageGLCallUniformMatrix3x4fv(void);
    bool TestCoverageGLCallUniformMatrix4x3fv(void);
    bool TestCoverageGLCallColorMaski(void);
    bool TestCoverageGLCallGetBooleani_v(void);
    bool TestCoverageGLCallGetIntegeri_v(void);
    bool TestCoverageGLCallEnablei(void);
    bool TestCoverageGLCallDisablei(void);
    bool TestCoverageGLCallIsEnabledi(void);
    bool TestCoverageGLCallClampColor(void);
    bool TestCoverageGLCallBeginConditionalRender(void);
    bool TestCoverageGLCallEndConditionalRender(void);
    bool TestCoverageGLCallVertexAttribI1i(void);
    bool TestCoverageGLCallVertexAttribI2i(void);
    bool TestCoverageGLCallVertexAttribI3i(void);
    bool TestCoverageGLCallVertexAttribI1ui(void);
    bool TestCoverageGLCallVertexAttribI2ui(void);
    bool TestCoverageGLCallVertexAttribI3ui(void);
    bool TestCoverageGLCallVertexAttribI1iv(void);
    bool TestCoverageGLCallVertexAttribI2iv(void);
    bool TestCoverageGLCallVertexAttribI3iv(void);
    bool TestCoverageGLCallVertexAttribI1uiv(void);
    bool TestCoverageGLCallVertexAttribI2uiv(void);
    bool TestCoverageGLCallVertexAttribI3uiv(void);
    bool TestCoverageGLCallVertexAttribI4bv(void);
    bool TestCoverageGLCallVertexAttribI4sv(void);
    bool TestCoverageGLCallVertexAttribI4ubv(void);
    bool TestCoverageGLCallVertexAttribI4usv(void);
    bool TestCoverageGLCallBindFragDataLocation(void);
    bool TestCoverageGLCallUniform1ui(void);
    bool TestCoverageGLCallUniform3ui(void);
    bool TestCoverageGLCallUniform4ui(void);
    bool TestCoverageGLCallUniform1uiv(void);
    bool TestCoverageGLCallUniform3uiv(void);
    bool TestCoverageGLCallUniform4uiv(void);
    bool TestCoverageGLCallTexParameterIiv(void);
    bool TestCoverageGLCallTexParameterIuiv(void);
    bool TestCoverageGLCallGetTexParameterIiv(void);
    bool TestCoverageGLCallGetTexParameterIuiv(void);
    bool TestCoverageGLCallFramebufferTexture1D(void);
    bool TestCoverageGLCallFramebufferTexture3D(void);
    bool TestCoverageGLCallFramebufferTextureLayer(void);
    bool TestCoverageGLCallTexBuffer(void);
    bool TestCoverageGLCallPrimitiveRestartIndex(void);
    bool TestCoverageGLCallGetActiveUniformName(void);
    bool TestCoverageGLCallGetInteger64i_v(void);
    bool TestCoverageGLCallFramebufferTexture(void);
    bool TestCoverageGLCallDrawElementsBaseVertex(void);
    bool TestCoverageGLCallDrawRangeElementsBaseVertex(void);
    bool TestCoverageGLCallDrawElementsInstancedBaseVertex(void);
    bool TestCoverageGLCallMultiDrawElementsBaseVertex(void);
    bool TestCoverageGLCallProvokingVertex(void);
    bool TestCoverageGLCallTexImage2DMultisample(void);
    bool TestCoverageGLCallTexImage3DMultisample(void);
    bool TestCoverageGLCallGetMultisamplefv(void);
    bool TestCoverageGLCallSampleMaski(void);
    bool TestCoverageGLCallBindFragDataLocationIndexed(void);
    bool TestCoverageGLCallGetFragDataIndex(void);
    bool TestCoverageGLCallSamplerParameterIiv(void);
    bool TestCoverageGLCallSamplerParameterIuiv(void);
    bool TestCoverageGLCallGetSamplerParameterIiv(void);
    bool TestCoverageGLCallGetSamplerParameterIfv(void);
    bool TestCoverageGLCallQueryCounter(void);
    bool TestCoverageGLCallGetQueryObjecti64v(void);
    bool TestCoverageGLCallGetQueryObjectui64v(void);
    bool TestCoverageGLCallVertexP2ui(void);
    bool TestCoverageGLCallVertexP2uiv(void);
    bool TestCoverageGLCallVertexP3ui(void);
    bool TestCoverageGLCallVertexP3uiv(void);
    bool TestCoverageGLCallVertexP4ui(void);
    bool TestCoverageGLCallVertexP4uiv(void);
    bool TestCoverageGLCallTexCoordP1ui(void);
    bool TestCoverageGLCallTexCoordP1uiv(void);
    bool TestCoverageGLCallTexCoordP2ui(void);
    bool TestCoverageGLCallTexCoordP2uiv(void);
    bool TestCoverageGLCallTexCoordP3ui(void);
    bool TestCoverageGLCallTexCoordP3uiv(void);
    bool TestCoverageGLCallTexCoordP4ui(void);
    bool TestCoverageGLCallTexCoordP4uiv(void);
    bool TestCoverageGLCallMultiTexCoordP1ui(void);
    bool TestCoverageGLCallMultiTexCoordP1uiv(void);
    bool TestCoverageGLCallMultiTexCoordP2ui(void);
    bool TestCoverageGLCallMultiTexCoordP2uiv(void);
    bool TestCoverageGLCallMultiTexCoordP3ui(void);
    bool TestCoverageGLCallMultiTexCoordP3uiv(void);
    bool TestCoverageGLCallMultiTexCoordP4ui(void);
    bool TestCoverageGLCallMultiTexCoordP4uiv(void);
    bool TestCoverageGLCallNormalP3ui(void);
    bool TestCoverageGLCallNormalP3uiv(void);
    bool TestCoverageGLCallColorP3ui(void);
    bool TestCoverageGLCallColorP3uiv(void);
    bool TestCoverageGLCallColorP4ui(void);
    bool TestCoverageGLCallColorP4uiv(void);
    bool TestCoverageGLCallSecondaryColorP3ui(void);
    bool TestCoverageGLCallSecondaryColorP3uiv(void);
    bool TestCoverageGLCallVertexAttribP1ui(void);
    bool TestCoverageGLCallVertexAttribP1uiv(void);
    bool TestCoverageGLCallVertexAttribP2ui(void);
    bool TestCoverageGLCallVertexAttribP2uiv(void);
    bool TestCoverageGLCallVertexAttribP3ui(void);
    bool TestCoverageGLCallVertexAttribP3uiv(void);
    bool TestCoverageGLCallVertexAttribP4ui(void);
    bool TestCoverageGLCallVertexAttribP4uiv(void);
    bool TestCoverageGLCallDrawArraysIndirect(void);
    bool TestCoverageGLCallDrawElementsIndirect(void);
    bool TestCoverageGLCallUniform1d(void);
    bool TestCoverageGLCallUniform2d(void);
    bool TestCoverageGLCallUniform3d(void);
    bool TestCoverageGLCallUniform4d(void);
    bool TestCoverageGLCallUniform1dv(void);
    bool TestCoverageGLCallUniform2dv(void);
    bool TestCoverageGLCallUniform3dv(void);
    bool TestCoverageGLCallUniform4dv(void);
    bool TestCoverageGLCallUniformMatrix2dv(void);
    bool TestCoverageGLCallUniformMatrix3dv(void);
    bool TestCoverageGLCallUniformMatrix4dv(void);
    bool TestCoverageGLCallUniformMatrix2x3dv(void);
    bool TestCoverageGLCallUniformMatrix2x4dv(void);
    bool TestCoverageGLCallUniformMatrix3x2dv(void);
    bool TestCoverageGLCallUniformMatrix3x4dv(void);
    bool TestCoverageGLCallUniformMatrix4x2dv(void);
    bool TestCoverageGLCallUniformMatrix4x3dv(void);
    bool TestCoverageGLCallGetUniformdv(void);
    bool TestCoverageGLCallProgramUniform1dEXT(void);
    bool TestCoverageGLCallProgramUniform2dEXT(void);
    bool TestCoverageGLCallProgramUniform3dEXT(void);
    bool TestCoverageGLCallProgramUniform4dEXT(void);
    bool TestCoverageGLCallProgramUniform1dvEXT(void);
    bool TestCoverageGLCallProgramUniform2dvEXT(void);
    bool TestCoverageGLCallProgramUniform3dvEXT(void);
    bool TestCoverageGLCallProgramUniform4dvEXT(void);
    bool TestCoverageGLCallProgramUniformMatrix2dvEXT(void);
    bool TestCoverageGLCallProgramUniformMatrix3dvEXT(void);
    bool TestCoverageGLCallProgramUniformMatrix4dvEXT(void);
    bool TestCoverageGLCallProgramUniformMatrix2x3dvEXT(void);
    bool TestCoverageGLCallProgramUniformMatrix2x4dvEXT(void);
    bool TestCoverageGLCallProgramUniformMatrix3x2dvEXT(void);
    bool TestCoverageGLCallProgramUniformMatrix3x4dvEXT(void);
    bool TestCoverageGLCallProgramUniformMatrix4x2dvEXT(void);
    bool TestCoverageGLCallProgramUniformMatrix4x3dvEXT(void);
    bool TestCoverageGLCallGetSubroutineUniformLocation(void);
    bool TestCoverageGLCallGetSubroutineIndex(void);
    bool TestCoverageGLCallGetActiveSubroutineUniformiv(void);
    bool TestCoverageGLCallGetActiveSubroutineUniformName(void);
    bool TestCoverageGLCallGetActiveSubroutineName(void);
    bool TestCoverageGLCallUniformSubroutinesuiv(void);
    bool TestCoverageGLCallGetUniformSubroutineuiv(void);
    bool TestCoverageGLCallGetProgramStageiv(void);
    bool TestCoverageGLCallPatchParameteri(void);
    bool TestCoverageGLCallPatchParameterfv(void);
    bool TestCoverageGLCallDrawTransformFeedback(void);
    bool TestCoverageGLCallDrawTransformFeedbackStream(void);
    bool TestCoverageGLCallBeginQueryIndexed(void);
    bool TestCoverageGLCallEndQueryIndexed(void);
    bool TestCoverageGLCallGetQueryIndexediv(void);

    glw::GLsizei TestCoverageGLGetNumPaletteEntries(glw::GLenum format);
    glw::GLsizei TestCoverageGLGetCompressedPixelsSize(glw::GLenum internalformat, glw::GLsizei width,
                                                       glw::GLsizei height, glw::GLsizei border);
    glw::GLsizei TestCoverageGLGetCompressedPaletteSize(glw::GLenum internalformat);
    glw::GLsizei TestCoverageGLGetPixelSize(glw::GLenum format);
    glw::GLsizei TestCoverageGLGetCompressedTextureSize(glw::GLenum internalformat, glw::GLsizei width,
                                                        glw::GLsizei height, glw::GLsizei border);

private:
    std::map<std::string, test_func_ptr> funcs_map;
    std::map<std::string, std::string> specialization_map;

    static const glw::GLchar *m_vert_shader;
    static const glw::GLchar *m_frag_shader;

    static const glw::GLuint m_defaultFBO = 0;

    std::string m_config_name;
    static std::vector<std::string> m_version_names;
    bool m_is_context_ES;
    bool m_is_transform_feedback_obj_supported;
    glu::ContextType m_context_type;

private:
    std::vector<enumTestRec> ea_BlendEquation;
    std::vector<enumTestRec> ea_BlendEquationSeparate1;
    std::vector<enumTestRec> ea_BlendEquationSeparate2;
    std::vector<enumTestRec> ea_BlendFunc1;
    std::vector<enumTestRec> ea_BlendFunc2;
    std::vector<enumTestRec> ea_BlendFuncSeparate1;
    std::vector<enumTestRec> ea_BlendFuncSeparate2;
    std::vector<enumTestRec> ea_BlendFuncSeparate3;
    std::vector<enumTestRec> ea_BlendFuncSeparate4;
    std::vector<enumTestRec> ea_BufferObjectTargets;
    std::vector<enumTestRec> ea_BufferObjectUsages;
    std::vector<enumTestRec> ea_ClearBufferMask;
    std::vector<enumTestRec> ea_CompressedTextureFormats;
    std::vector<enumTestRec> ea_ShaderTypes;
    std::vector<enumTestRec> ea_CullFaceMode;
    std::vector<enumTestRec> ea_DepthFunction;
    std::vector<enumTestRec> ea_Enable;
    std::vector<enumTestRec> ea_Primitives;
    std::vector<enumTestRec> ea_Face;
    std::vector<enumTestRec> ea_FrameBufferTargets;
    std::vector<enumTestRec> ea_FrameBufferAttachments;
    std::vector<enumTestRec> ea_FrontFaceDirection;
    std::vector<enumTestRec> ea_GetBoolean;
    std::vector<enumTestRec> ea_GetBufferParameter;
    std::vector<enumTestRec> ea_GetBufferParameter_OES_mapbuffer;
    std::vector<enumTestRec> ea_GetFloat;
    std::vector<enumTestRec> ea_GetFramebufferAttachmentParameter;
    std::vector<enumTestRec> ea_GetInteger;
    std::vector<enumTestRec> ea_GetInteger_OES_Texture_3D;
    std::vector<enumTestRec> ea_GetPointer;
    std::vector<enumTestRec> ea_HintTarget_OES_fragment_shader_derivative;
    std::vector<enumTestRec> ea_InvalidRenderBufferFormats;
    std::vector<enumTestRec> ea_RenderBufferFormats_OES_rgb8_rgba8;
    std::vector<enumTestRec> ea_RenderBufferFormats_OES_depth_component24;
    std::vector<enumTestRec> ea_RenderBufferFormats_OES_depth_component32;
    std::vector<enumTestRec> ea_RenderBufferFormats_OES_stencil1;
    std::vector<enumTestRec> ea_RenderBufferFormats_OES_stencil4;
    std::vector<enumTestRec> ea_ShaderPrecision;
    std::vector<enumTestRec> ea_GetIntegerES3;
    std::vector<enumTestRec> ea_GetProgram;
    std::vector<enumTestRec> ea_GetRenderBufferParameter;
    std::vector<enumTestRec> ea_GetShaderStatus;
    std::vector<enumTestRec> ea_GetString;
    std::vector<enumTestRec> ea_GetTexParameter;
    std::vector<enumTestRec> ea_GetVertexAttrib;
    std::vector<enumTestRec> ea_GetVertexAttribPointer;
    std::vector<enumTestRec> ea_HintMode;
    std::vector<enumTestRec> ea_HintTarget;
    std::vector<enumTestRec> ea_PixelStore;
    std::vector<enumTestRec> ea_RenderBufferFormats;
    std::vector<enumTestRec> ea_RenderBufferTargets;
    std::vector<enumTestRec> ea_RenderBufferInvalidTargets;
    std::vector<enumTestRec> ea_StencilFunction;
    std::vector<enumTestRec> ea_StencilOp;
    std::vector<enumTestRec> ea_TextureFormat;
    std::vector<enumTestRec> ea_TextureMagFilter;
    std::vector<enumTestRec> ea_TextureMinFilter;
    std::vector<enumTestRec> ea_TextureTarget;
    std::vector<enumTestRec> ea_TextureType;
    std::vector<enumTestRec> ea_TextureWrapMode;
    std::vector<enumTestRec> ea_GetBufferParameteri64v;
    std::vector<enumTestRec> ea_ReadBuffer;
    std::vector<enumTestRec> ea_Texture3DTarget;
    std::vector<enumTestRec> ea_CompressedTexture3DTarget;
    std::vector<enumTestRec> ea_CompressedTextureFormat;
    std::vector<glw::GLsizei> CompressedTextureSize;
    std::vector<enumTestRec> ea_DrawBuffers;
    std::vector<enumTestRec> ea_GetInteger64v;
    std::vector<enumTestRec> ea_GetSynciv;
    std::vector<enumTestRec> ea_InvalidateFramebuffer;
};

/** Test group which encapsulates all conformance tests */
class ApiCoverageTests : public deqp::TestCaseGroup
{
public:
    /* Public methods */
    ApiCoverageTests(deqp::Context &context);

    void init();

private:
    ApiCoverageTests(const ApiCoverageTests &other);
    ApiCoverageTests &operator=(const ApiCoverageTests &other);
};

} // namespace glcts

#endif // _GLCAPICOVERAGETESTS_HPP
