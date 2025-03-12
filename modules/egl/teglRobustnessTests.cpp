/*-------------------------------------------------------------------------
 * drawElements Quality Program EGL Module
 * ---------------------------------------
 *
 * Copyright 2017 The Android Open Source Project
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
 * \brief Robustness tests for KHR_robustness.
 *//*--------------------------------------------------------------------*/

#include "teglRobustnessTests.hpp"

#include "tcuTestLog.hpp"
#include "tcuStringTemplate.hpp"

#include "egluConfigFilter.hpp"
#include "egluStrUtil.hpp"
#include "egluUtil.hpp"
#include "eglwLibrary.hpp"

#include "gluStrUtil.hpp"
#include "gluShaderProgram.hpp"
#include "gluDrawUtil.hpp"

#include "glwFunctions.hpp"
#include "glwEnums.hpp"

#include "deSTLUtil.hpp"
#include "deStringUtil.hpp"
#include "deThread.hpp"
#include "deSharedPtr.hpp"

#include <set>

using std::set;
using std::string;
using std::vector;
using tcu::TestLog;

using namespace eglw;

DE_STATIC_ASSERT(GL_RESET_NOTIFICATION_STRATEGY == 0x8256);
DE_STATIC_ASSERT(GL_LOSE_CONTEXT_ON_RESET == 0x8252);
DE_STATIC_ASSERT(GL_NO_RESET_NOTIFICATION == 0x8261);

namespace deqp
{
namespace egl
{
namespace
{

enum ContextResetType
{
    CONTEXTRESETTYPE_SHADER_OOB,
    CONTEXTRESETTYPE_FIXED_FUNC_OOB,
};

enum ShaderType
{
    SHADERTYPE_VERT,
    SHADERTYPE_FRAG,
    SHADERTYPE_COMPUTE,
    SHADERTYPE_VERT_AND_FRAG,
};

enum ReadWriteType
{
    READWRITETYPE_READ,
    READWRITETYPE_WRITE,
};

enum ResourceType
{
    RESOURCETYPE_UBO,
    RESOURCETYPE_SSBO,
    RESOURCETYPE_LOCAL_ARRAY,
};

enum FixedFunctionType
{
    FIXEDFUNCTIONTYPE_INDICES,
    FIXEDFUNCTIONTYPE_VERTICES,
};

enum RobustAccessType
{
    ROBUSTACCESS_TRUE,
    ROBUSTACCESS_FALSE,
};

void requireEGLExtension(const Library &egl, EGLDisplay eglDisplay, const char *requiredExtension)
{
    if (!eglu::hasExtension(egl, eglDisplay, requiredExtension))
        TCU_THROW(NotSupportedError, (string(requiredExtension) + " not supported").c_str());
}

bool isWindow(const eglu::CandidateConfig &c)
{
    return (c.surfaceType() & EGL_WINDOW_BIT) == EGL_WINDOW_BIT;
}

template <uint32_t Type>
bool renderable(const eglu::CandidateConfig &c)
{
    return (c.renderableType() & Type) == Type;
}

eglu::ConfigFilter getRenderableFilter(uint32_t bits)
{
    switch (bits)
    {
    case EGL_OPENGL_ES2_BIT:
        return renderable<EGL_OPENGL_ES2_BIT>;
    case EGL_OPENGL_ES3_BIT:
        return renderable<EGL_OPENGL_ES3_BIT>;
    case EGL_OPENGL_BIT:
        return renderable<EGL_OPENGL_BIT>;
    default:
        DE_FATAL("Unknown EGL bitfied value");
        return renderable<0>;
    }
}

const char *eglResetNotificationStrategyToString(EGLint strategy)
{
    switch (strategy)
    {
    case EGL_NO_RESET_NOTIFICATION_KHR:
        return "EGL_NO_RESET_NOTIFICATION_KHR";
    case EGL_LOSE_CONTEXT_ON_RESET_KHR:
        return "EGL_LOSE_CONTEXT_ON_RESET_KHR";
    default:
        return "<Unknown>";
    }
}

void logAttribList(const EglTestContext &eglTestCtx, const EGLint *attribList)
{
    const EGLint *iter = &(attribList[0]);
    std::ostringstream attribListString;

    while ((*iter) != EGL_NONE)
    {
        switch (*iter)
        {
            //    case EGL_CONTEXT_CLIENT_VERSION:
        case EGL_CONTEXT_MAJOR_VERSION_KHR:
            iter++;
            attribListString << "EGL_CONTEXT_CLIENT_VERSION, " << (*iter) << ", ";
            iter++;
            break;

        case EGL_CONTEXT_MINOR_VERSION_KHR:
            iter++;
            attribListString << "EGL_CONTEXT_MINOR_VERSION_KHR, " << (*iter) << ", ";
            iter++;
            break;

        case EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT:
            iter++;
            attribListString << "EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT, "
                             << eglResetNotificationStrategyToString(*iter) << ", ";
            iter++;
            break;

        case EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR:
            iter++;
            attribListString << "EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR, "
                             << eglResetNotificationStrategyToString(*iter) << ", ";
            iter++;
            break;

        case EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT:
            iter++;
            attribListString << "EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT, ";

            if (*iter == EGL_FALSE || *iter == EGL_TRUE)
                attribListString << (*iter ? "EGL_TRUE" : "EGL_FALSE") << ", ";
            else
                attribListString << (*iter) << ", ";
            iter++;
            break;

        default:
            DE_FATAL("Unsupported attribute");
        }
    }

    attribListString << "EGL_NONE";
    eglTestCtx.getTestContext().getLog() << TestLog::Message << "EGL attrib list: { " << attribListString.str()
                                         << " }\n\n"
                                         << TestLog::EndMessage;
}

class RobustnessTestCase : public TestCase
{
public:
    class Params
    {
    public:
        Params(void)
        {
        }

        Params(const string &name, const string &description, const RobustAccessType &robustAccessType,
               const ContextResetType &contextResetType, const FixedFunctionType &fixedFunctionType);

        Params(const string &name, const string &description, const RobustAccessType &robustAccessType,
               const ContextResetType &contextResetType, const ShaderType &shaderType, const ResourceType &resourceType,
               const ReadWriteType &readWriteType);

        const string &getName(void) const
        {
            return m_name;
        }
        const string &getDescription(void) const
        {
            return m_description;
        }
        const ContextResetType &getContextResetType(void) const
        {
            return m_contextResetType;
        }
        const ShaderType &getShaderType(void) const
        {
            return m_shaderType;
        }
        const ResourceType &getResourceType(void) const
        {
            return m_resourceType;
        }
        const ReadWriteType &getReadWriteType(void) const
        {
            return m_readWriteType;
        }
        const FixedFunctionType &getFixedFunctionType(void) const
        {
            return m_fixedFunctionType;
        }
        const RobustAccessType &getRobustAccessType(void) const
        {
            return m_robustAccessType;
        }

    private:
        string m_name;
        string m_description;
        RobustAccessType m_robustAccessType;
        ContextResetType m_contextResetType;
        ShaderType m_shaderType;
        ResourceType m_resourceType;
        ReadWriteType m_readWriteType;
        FixedFunctionType m_fixedFunctionType;
    };

    RobustnessTestCase(EglTestContext &eglTestCtx, const char *name, const char *description);
    RobustnessTestCase(EglTestContext &eglTestCtx, const char *name, const char *description, Params params);
    ~RobustnessTestCase(void);

    void checkRequiredEGLExtensions(const EGLint *attribList);

protected:
    Params m_params;
    EGLDisplay m_eglDisplay;
    EGLConfig m_eglConfig;
    EGLSurface m_eglSurface;

private:
    void init(void);
    void deinit(void);
    void initEGLSurface(void);
    EGLConfig getEGLConfig(void);

    eglu::NativeWindow *m_window;
};

RobustnessTestCase::Params::Params(const string &name, const string &description,
                                   const RobustAccessType &robustAccessType, const ContextResetType &contextResetType,
                                   const FixedFunctionType &fixedFunctionType)
    : m_name(name)
    , m_description(description)
    , m_robustAccessType(robustAccessType)
    , m_contextResetType(contextResetType)
    , m_fixedFunctionType(fixedFunctionType)
{
}

RobustnessTestCase::Params::Params(const string &name, const string &description,
                                   const RobustAccessType &robustAccessType, const ContextResetType &contextResetType,
                                   const ShaderType &shaderType, const ResourceType &resourceType,
                                   const ReadWriteType &readWriteType)
    : m_name(name)
    , m_description(description)
    , m_robustAccessType(robustAccessType)
    , m_contextResetType(contextResetType)
    , m_shaderType(shaderType)
    , m_resourceType(resourceType)
    , m_readWriteType(readWriteType)
{
}

RobustnessTestCase::RobustnessTestCase(EglTestContext &eglTestCtx, const char *name, const char *description)
    : TestCase(eglTestCtx, name, description)
    , m_eglDisplay(EGL_NO_DISPLAY)
    , m_eglConfig(0)
    , m_eglSurface(EGL_NO_SURFACE)
    , m_window(nullptr)
{
}

RobustnessTestCase::RobustnessTestCase(EglTestContext &eglTestCtx, const char *name, const char *description,
                                       Params params)
    : TestCase(eglTestCtx, name, description)
    , m_params(params)
    , m_eglDisplay(EGL_NO_DISPLAY)
    , m_eglConfig(0)
    , m_eglSurface(EGL_NO_SURFACE)
    , m_window(nullptr)
{
}

RobustnessTestCase::~RobustnessTestCase(void)
{
    deinit();
}

void RobustnessTestCase::init(void)
{
    m_eglDisplay = eglu::getAndInitDisplay(m_eglTestCtx.getNativeDisplay());
    m_eglConfig  = getEGLConfig();

    initEGLSurface();
}

void RobustnessTestCase::deinit(void)
{
    const Library &egl = m_eglTestCtx.getLibrary();

    if (m_eglSurface != EGL_NO_SURFACE)
    {
        egl.destroySurface(m_eglDisplay, m_eglSurface);
        m_eglSurface = EGL_NO_SURFACE;
    }
    if (m_eglDisplay != EGL_NO_DISPLAY)
    {
        egl.terminate(m_eglDisplay);
        m_eglDisplay = EGL_NO_DISPLAY;
    }

    delete m_window;
    m_window = nullptr;
}

EGLConfig RobustnessTestCase::getEGLConfig(void)
{
    eglu::FilterList filters;
    filters << isWindow << getRenderableFilter(EGL_OPENGL_ES3_BIT);
    return eglu::chooseSingleConfig(m_eglTestCtx.getLibrary(), m_eglDisplay, filters);
}

void RobustnessTestCase::initEGLSurface(void)
{
    EGLU_CHECK_CALL(m_eglTestCtx.getLibrary(), bindAPI(EGL_OPENGL_ES_API));

    const eglu::NativeWindowFactory &factory =
        eglu::selectNativeWindowFactory(m_eglTestCtx.getNativeDisplayFactory(), m_testCtx.getCommandLine());

    const eglu::WindowParams windowParams =
        eglu::WindowParams(256, 256, eglu::parseWindowVisibility(m_testCtx.getCommandLine()));
    m_window = factory.createWindow(&m_eglTestCtx.getNativeDisplay(), m_eglDisplay, m_eglConfig, nullptr, windowParams);
    m_eglSurface =
        eglu::createWindowSurface(m_eglTestCtx.getNativeDisplay(), *m_window, m_eglDisplay, m_eglConfig, nullptr);
}

glu::ApiType paramsToApiType(const RobustnessTestCase::Params &params)
{
    EGLint minorVersion = 0;
    if (params.getShaderType() == SHADERTYPE_COMPUTE || params.getResourceType() == RESOURCETYPE_SSBO ||
        params.getContextResetType() == CONTEXTRESETTYPE_SHADER_OOB)
    {
        minorVersion = 1;
    }

    return glu::ApiType::es(3, minorVersion);
}

void RobustnessTestCase::checkRequiredEGLExtensions(const EGLint *attribList)
{
    set<string> requiredExtensions;
    vector<string> extensions = eglu::getDisplayExtensions(m_eglTestCtx.getLibrary(), m_eglDisplay);

    {
        const EGLint *iter = attribList;

        while ((*iter) != EGL_NONE)
        {
            switch (*iter)
            {
            case EGL_CONTEXT_MAJOR_VERSION_KHR:
                iter++;
                iter++;
                break;

            case EGL_CONTEXT_MINOR_VERSION_KHR:
                iter++;
                requiredExtensions.insert("EGL_KHR_create_context");
                iter++;
                break;

            case EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT:
            case EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT:
                iter++;
                requiredExtensions.insert("EGL_EXT_create_context_robustness");
                iter++;
                break;

            default:
                DE_ASSERT(false);
            }
        }
    }

    for (std::set<string>::const_iterator reqExt = requiredExtensions.begin(); reqExt != requiredExtensions.end();
         ++reqExt)
    {
        if (!de::contains(extensions.begin(), extensions.end(), *reqExt))
        {
            const char *const extension = reqExt->c_str();
            requireEGLExtension(m_eglTestCtx.getLibrary(), m_eglDisplay, extension);
        }
    }
}

void checkRequiredGLSupport(const glw::Functions &gl, glu::ApiType requiredApi)
{
    if (!glu::hasExtension(gl, requiredApi, "GL_KHR_robustness") &&
        !glu::hasExtension(gl, requiredApi, "GL_EXT_robustness"))
    {
        TCU_THROW(NotSupportedError, (string("GL_KHR_robustness and GL_EXT_robustness") + " not supported").c_str());
    }
    else
    {
        int realMinorVersion = 0;
        gl.getIntegerv(GL_MINOR_VERSION, &realMinorVersion);
        GLU_EXPECT_NO_ERROR(gl.getError(), "Get minor version failed");

        if (realMinorVersion < requiredApi.getMinorVersion())
            TCU_THROW(NotSupportedError, "Test case requires GLES 3.1");
    }
}

void checkGLSupportForParams(const glw::Functions &gl, const RobustnessTestCase::Params &params)
{
    int minorVersion = 0;
    if (params.getShaderType() == SHADERTYPE_COMPUTE || params.getResourceType() == RESOURCETYPE_SSBO ||
        params.getContextResetType() == CONTEXTRESETTYPE_SHADER_OOB)
    {
        minorVersion = 1;
    }
    checkRequiredGLSupport(gl, glu::ApiType::es(3, minorVersion));
}

class RenderingContext
{
public:
    RenderingContext(const EglTestContext &eglTestCtx, const EGLint *attribList, const EGLConfig &config,
                     const EGLDisplay &display, const EGLContext &sharedContext);
    ~RenderingContext(void);

    void initGLFunctions(glw::Functions *gl, const glu::ApiType apiType);
    void makeCurrent(const EGLSurface &surface);
    EGLContext getContext(void);

private:
    const EglTestContext &m_eglTestCtx;
    const EGLint *m_attribList;
    const EGLConfig &m_config;
    const EGLDisplay &m_display;
    const Library &m_egl;

    EGLContext m_context;

    void createContext(const EGLConfig &sharedConfig);
    void destroyContext(void);

    RenderingContext(const RenderingContext &);
    RenderingContext &operator=(const RenderingContext &);
};

RenderingContext::RenderingContext(const EglTestContext &eglTestCtx, const EGLint *attribList, const EGLConfig &config,
                                   const EGLDisplay &display, const EGLContext &sharedContext)
    : m_eglTestCtx(eglTestCtx)
    , m_attribList(attribList)
    , m_config(config)
    , m_display(display)
    , m_egl(eglTestCtx.getLibrary())
    , m_context(EGL_NO_CONTEXT)
{
    logAttribList(eglTestCtx, m_attribList);
    createContext(sharedContext);
}

RenderingContext::~RenderingContext(void)
{
    destroyContext();
}

void RenderingContext::createContext(const EGLConfig &sharedContext)
{
    m_context = m_egl.createContext(m_display, m_config, sharedContext, m_attribList);
    EGLU_CHECK_MSG(m_egl, "eglCreateContext()");
}

void RenderingContext::destroyContext(void)
{
    EGLU_CHECK_CALL(m_eglTestCtx.getLibrary(), makeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));

    if (m_context != EGL_NO_CONTEXT)
        m_egl.destroyContext(m_display, m_context);
}

void RenderingContext::makeCurrent(const EGLSurface &surface)
{
    EGLU_CHECK_CALL(m_egl, makeCurrent(m_display, surface, surface, m_context));
}

void RenderingContext::initGLFunctions(glw::Functions *gl, const glu::ApiType apiType)
{
    // \todo [2017-03-23 pyry] Current version has 2 somewhat ugly hacks:
    //
    // 1) Core functions are loaded twice. We need glGetString(i) to query supported
    //    extensions to determine if we need to load EXT or KHR-suffixed robustness
    //    functions. This could be fixed by exposing glw::FunctionLoader in EglTestContext
    //    for example.
    //
    // 2) We assume that calling code will check for KHR_robustness or EXT_robustness
    //    support after calling initGLFunctions(). We could move the check here.

    m_eglTestCtx.initGLFunctions(gl, apiType);

    {
        const char *const robustnessExt =
            glu::hasExtension(*gl, apiType, "GL_KHR_robustness") ? "GL_KHR_robustness" : "GL_EXT_robustness";
        const char *const extensions[] = {robustnessExt};

        m_eglTestCtx.initGLFunctions(gl, apiType, DE_LENGTH_OF_ARRAY(extensions), &extensions[0]);
    }
}

EGLContext RenderingContext::getContext(void)
{
    return m_context;
}

class ContextReset
{
public:
    ContextReset(glw::Functions &gl, tcu::TestLog &log, FixedFunctionType fixedFunctionType);
    ContextReset(glw::Functions &gl, tcu::TestLog &log, ShaderType shaderType, ResourceType resourceType,
                 ReadWriteType readWriteType);

    virtual ~ContextReset(void)
    {
    }

    virtual void setup(void)    = 0;
    virtual void draw(void)     = 0;
    virtual void teardown(void) = 0;

    void finish(void);

    glw::GLint getError(void);
    glw::GLint getGraphicsResetStatus(void);

    glw::GLsync getSyncObject(void) const
    {
        return m_sync;
    }
    glw::GLuint getQueryID(void) const
    {
        return m_queryID;
    }

    glw::Functions &m_gl;
    tcu::TestLog &m_log;
    ShaderType m_shaderType;
    ResourceType m_resourceType;
    ReadWriteType m_readWriteType;
    FixedFunctionType m_fixedFunctionType;

private:
    ContextReset(const ContextReset &);
    ContextReset &operator=(const ContextReset &);

    glw::GLuint m_queryID;
    glw::GLsync m_sync;
};

ContextReset::ContextReset(glw::Functions &gl, tcu::TestLog &log, FixedFunctionType fixedFunctionType)
    : m_gl(gl)
    , m_log(log)
    , m_fixedFunctionType(fixedFunctionType)
{
}

ContextReset::ContextReset(glw::Functions &gl, tcu::TestLog &log, ShaderType shaderType, ResourceType resourceType,
                           ReadWriteType readWriteType)
    : m_gl(gl)
    , m_log(log)
    , m_shaderType(shaderType)
    , m_resourceType(resourceType)
    , m_readWriteType(readWriteType)
{
}

void ContextReset::finish(void)
{
    GLU_CHECK_GLW_CALL(m_gl, finish());
}

glw::GLint ContextReset::getError(void)
{
    glw::GLint error;
    error = m_gl.getError();

    return error;
}

glw::GLint ContextReset::getGraphicsResetStatus(void)
{
    glw::GLint resetStatus;
    resetStatus = m_gl.getGraphicsResetStatus();

    return resetStatus;
}

class FixedFunctionOOB : public ContextReset
{
public:
    FixedFunctionOOB(glw::Functions &gl, tcu::TestLog &log, FixedFunctionType fixedFunctionType);
    ~FixedFunctionOOB(void);

    struct TestConfig
    {
        int textureWidth;
        int textureHeight;
    };

    virtual void setup(void);
    virtual void draw(void);
    virtual void teardown(void);

private:
    glu::ProgramSources genSources(void);
    glw::GLuint m_coordinatesBuffer;
    glw::GLint m_coordLocation;
};

FixedFunctionOOB::FixedFunctionOOB(glw::Functions &gl, tcu::TestLog &log, FixedFunctionType fixedFunctionType)
    : ContextReset(gl, log, fixedFunctionType)
    , m_coordinatesBuffer(0)
    , m_coordLocation(0)
{
}

FixedFunctionOOB::~FixedFunctionOOB(void)
{
    try
    {
        // Reset GL_CONTEXT_LOST error before destroying resources
        m_gl.getGraphicsResetStatus();
        teardown();
    }
    catch (...)
    {
        // Ignore GL errors from teardown()
    }
}

glu::ProgramSources FixedFunctionOOB::genSources(void)
{
    const char *const vert = "#version 300 es\n"
                             "in highp vec4 a_position;\n"
                             "void main (void)\n"
                             "{\n"
                             "    gl_Position = a_position;\n"
                             "}\n";

    const char *const frag = "#version 300 es\n"
                             "layout(location = 0) out highp vec4 fragColor;\n"
                             "void main (void)\n"
                             "{\n"
                             "    fragColor = vec4(1.0f);\n"
                             "}\n";

    return glu::ProgramSources() << glu::VertexSource(vert) << glu::FragmentSource(frag);
}

void FixedFunctionOOB::setup(void)
{
    glu::ShaderProgram program(m_gl, genSources());

    m_log << program;

    if (!program.isOk())
        TCU_FAIL("Failed to compile shader program");

    GLU_CHECK_GLW_CALL(m_gl, useProgram(program.getProgram()));

    const glw::GLfloat coords[] = {-1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f};

    m_coordLocation = m_gl.getAttribLocation(program.getProgram(), "a_position");
    GLU_CHECK_GLW_MSG(m_gl, "glGetAttribLocation()");
    TCU_CHECK(m_coordLocation != (glw::GLint)-1);

    // Load the vertex data
    m_coordinatesBuffer = 0;
    GLU_CHECK_GLW_CALL(m_gl, genBuffers(1, &m_coordinatesBuffer));
    GLU_CHECK_GLW_CALL(m_gl, bindBuffer(GL_ARRAY_BUFFER, m_coordinatesBuffer));
    GLU_CHECK_GLW_CALL(m_gl, bufferData(GL_ARRAY_BUFFER, (glw::GLsizeiptr)sizeof(coords), coords, GL_STATIC_DRAW));
    GLU_CHECK_GLW_CALL(m_gl, enableVertexAttribArray(m_coordLocation));
    GLU_CHECK_GLW_CALL(m_gl, vertexAttribPointer(m_coordLocation, 2, GL_FLOAT, GL_FALSE, 0, nullptr));
}

void FixedFunctionOOB::draw(void)
{
    const glw::GLint bad_indices[] = {0, 10, 100, 1000, 10000, 100000};

    if (m_fixedFunctionType == FIXEDFUNCTIONTYPE_INDICES)
        m_gl.drawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, bad_indices);
    else if (m_fixedFunctionType == FIXEDFUNCTIONTYPE_VERTICES)
        m_gl.drawArrays(GL_TRIANGLES, 0, 1000);
    else
        DE_FATAL("Unknown fixed function type");
}

void FixedFunctionOOB::teardown(void)
{
    if (m_coordLocation)
    {
        GLU_CHECK_GLW_CALL(m_gl, disableVertexAttribArray(m_coordLocation));
        m_coordLocation = 0;
    }

    if (m_coordinatesBuffer)
    {
        GLU_CHECK_GLW_CALL(m_gl, deleteBuffers(1, &m_coordinatesBuffer));
        m_coordinatesBuffer = 0;
    }

    GLU_CHECK_GLW_CALL(m_gl, useProgram(0));
}

class ShadersOOB : public ContextReset
{
public:
    ShadersOOB(glw::Functions &gl, tcu::TestLog &log, ShaderType shaderType, ResourceType resourceType,
               ReadWriteType readWriteType);
    ~ShadersOOB(void);

    virtual void setup(void);
    virtual void draw(void);
    virtual void teardown(void);

private:
    static const int s_numBindings = 3;

    glw::GLuint m_coordinatesBuffer;
    glw::GLint m_coordLocation;

    bool m_isUBO;
    bool m_isRead;
    bool m_isLocalArray;
    std::vector<glw::GLuint> m_buffers;

    std::string genVertexShader(const std::string &shaderDecl, const std::string &shaderBody);
    std::string genFragmentShader(const std::string &shaderDecl, const std::string &shaderBody);
    std::string genComputeShader(const std::string &shaderDecl, const std::string &shaderBody);

    glu::ProgramSources genNonComputeSource(void);
    glu::ProgramSources genComputeSource(void);
    glu::ProgramSources genSources(void);
};

ShadersOOB::ShadersOOB(glw::Functions &gl, tcu::TestLog &log, ShaderType shaderType, ResourceType resourceType,
                       ReadWriteType readWriteType)
    : ContextReset(gl, log, shaderType, resourceType, readWriteType)
    , m_coordinatesBuffer(0)
    , m_coordLocation(0)
    , m_buffers(s_numBindings, 0)
{
    m_isUBO        = (m_resourceType == RESOURCETYPE_UBO);
    m_isLocalArray = (m_resourceType == RESOURCETYPE_LOCAL_ARRAY);
    m_isRead       = (m_readWriteType == READWRITETYPE_READ);
}

ShadersOOB::~ShadersOOB(void)
{
    try
    {
        // Reset GL_CONTEXT_LOST error before destroying resources
        m_gl.getGraphicsResetStatus();
        teardown();
    }
    catch (...)
    {
        // Ignore GL errors from teardown()
    }
}

std::string ShadersOOB::genVertexShader(const std::string &shaderDecl, const std::string &shaderBody)
{
    static const char *const s_simpleVertexShaderSource = "#version 310 es\n"
                                                          "in highp vec4 a_position;\n"
                                                          "void main (void)\n"
                                                          "{\n"
                                                          "    gl_Position = a_position;\n"
                                                          "}\n";

    switch (m_shaderType)
    {
    case SHADERTYPE_VERT:
    case SHADERTYPE_VERT_AND_FRAG:
    {
        std::ostringstream vertexShaderSource;
        vertexShaderSource << "#version 310 es\n"
                           << "in highp vec4 a_position;\n"
                           << "out highp vec4 v_color;\n"
                           << shaderDecl << "\n"
                           << "void main (void)\n"
                           << "{\n"
                           << "    highp vec4 color = vec4(0.0f);\n"
                           << shaderBody << "\n"
                           << "    v_color = color;\n"
                           << "    gl_Position = a_position;\n"
                           << "}\n";

        return vertexShaderSource.str();
    }

    case SHADERTYPE_FRAG:
        return s_simpleVertexShaderSource;

    default:
        DE_FATAL("Unknown shader type");
        return "";
    }
}

std::string ShadersOOB::genFragmentShader(const std::string &shaderDecl, const std::string &shaderBody)
{
    static const char *const s_simpleFragmentShaderSource = "#version 310 es\n"
                                                            "in highp vec4 v_color;\n"
                                                            "layout(location = 0) out highp vec4 fragColor;\n"
                                                            "void main (void)\n"
                                                            "{\n"
                                                            "    fragColor = v_color;\n"
                                                            "}\n";

    switch (m_shaderType)
    {
    case SHADERTYPE_VERT:
        return s_simpleFragmentShaderSource;

    case SHADERTYPE_FRAG:
    {
        std::ostringstream fragmentShaderSource;
        fragmentShaderSource << "#version 310 es\n"
                             << "layout(location = 0) out highp vec4 fragColor;\n"
                             << shaderDecl << "\n"
                             << "void main (void)\n"
                             << "{\n"
                             << "    highp vec4 color = vec4(0.0f);\n"
                             << shaderBody << "\n"
                             << "    fragColor = color;\n"
                             << "}\n";

        return fragmentShaderSource.str();
    }
    case SHADERTYPE_VERT_AND_FRAG:
    {
        std::ostringstream fragmentShaderSource;
        fragmentShaderSource << "#version 310 es\n"
                             << "in highp vec4 v_color;\n"
                             << "layout(location = 0) out highp vec4 fragColor;\n"
                             << shaderDecl << "\n"
                             << "void main (void)\n"
                             << "{\n"
                             << "    highp vec4 color = vec4(0.0f);\n"
                             << shaderBody << "\n"
                             << "    fragColor = color;\n"
                             << "}\n";

        return fragmentShaderSource.str();
    }

    default:
        DE_FATAL("Unknown shader type");
        return "";
    }
}

std::string ShadersOOB::genComputeShader(const std::string &shaderDecl, const std::string &shaderBody)
{
    std::ostringstream computeShaderSource;

    computeShaderSource << "#version 310 es\n"
                        << "layout(local_size_x = 1, local_size_y = 1) in;\n"
                        << "\n"
                        << "layout(binding = 0) buffer Output {\n"
                        << "    highp vec4 values;\n"
                        << "} sb_out;\n"
                        << "\n"
                        << shaderDecl << "void main ()\n"
                        << "{\n"
                        << shaderBody << "}\n";

    return computeShaderSource.str();
}

glu::ProgramSources ShadersOOB::genNonComputeSource(void)
{
    std::ostringstream shaderDecl;
    std::ostringstream shaderBody;

    shaderDecl << "uniform highp int u_index;\n";

    if (m_isLocalArray)
    {
        const char *const readWriteStatement =
            (m_isRead) ? "    color.x = color_out[u_index];\n" : "    color[u_index] = color_out[0];\n";

        shaderBody << "    highp float color_out[4] = float[4](0.25f, 0.5f, 0.75f, 1.0f);\n" << readWriteStatement;
    }
    else
    {
        const std::string resName = (m_isUBO) ? "ub_in" : "sb_in";

        shaderDecl << "layout(std140, binding = 0) " << ((m_isUBO) ? "uniform" : "buffer") << " Block\n"
                   << "{\n"
                   << "    highp float color_out[4];\n"
                   << "} " << resName << "[" << s_numBindings << "];\n";

        const std::string readWriteStatement = (m_isRead) ? "    color.x = " + resName + "[0].color_out[u_index];\n" :
                                                            "    color[u_index] = " + resName + "[0].color_out[0];\n";

        shaderBody << readWriteStatement;
    }

    return glu::ProgramSources() << glu::VertexSource(genVertexShader(shaderDecl.str(), shaderBody.str()))
                                 << glu::FragmentSource(genFragmentShader(shaderDecl.str(), shaderBody.str()));
}

glu::ProgramSources ShadersOOB::genComputeSource(void)
{
    std::ostringstream shaderDecl;
    std::ostringstream shaderBody;

    shaderDecl << "uniform highp int u_index;\n";

    shaderBody << "    uvec3 size = gl_NumWorkGroups * gl_WorkGroupSize;\n"
               << "    uint groupNdx = size.x*gl_GlobalInvocationID.y + gl_GlobalInvocationID.x;\n";

    if (m_isLocalArray)
    {
        const char *const readWriteStatement =
            (m_isRead) ? "    sb_out.values.x = values[u_index];\n" : "    sb_out.values[u_index] = values.x;\n";

        shaderBody << "    highp vec4 values = vec4(1.0f, 0.0f, 3.0f, 2.0f) * float(groupNdx);\n" << readWriteStatement;
    }
    else
    {
        const std::string resName = (m_isUBO) ? "ub_in" : "sb_in";

        shaderDecl << "layout(std140, binding = 1) " << ((m_isUBO) ? "uniform" : "buffer") << " Input\n"
                   << "{\n"
                   << "    highp vec4 values;\n"
                   << "} " << resName << "[" << s_numBindings << "];\n";

        std::string readWriteStatement =
            (m_isRead) ? "    sb_out.values.x = " + resName + "[0].values[u_index] * float(groupNdx);\n" :
                         "    sb_out.values[u_index] = " + resName + "[0].values.x * float(groupNdx);\n";

        shaderBody << readWriteStatement;
    }

    return glu::ProgramSources() << glu::ComputeSource(genComputeShader(shaderDecl.str(), shaderBody.str()));
}

glu::ProgramSources ShadersOOB::genSources(void)
{
    if (m_shaderType == SHADERTYPE_COMPUTE)
        return genComputeSource();
    else
        return genNonComputeSource();
}

void ShadersOOB::setup(void)
{
    if (!m_isUBO && !m_isLocalArray && (m_shaderType != SHADERTYPE_COMPUTE))
    {
        // Check implementation limits for shader SSBO
        int shaderStorageBlockSupported = -1;
        const bool isVertex =
            (m_shaderType == SHADERTYPE_VERT || m_shaderType == SHADERTYPE_VERT_AND_FRAG) ? true : false;
        string shaderTypeStr =
            isVertex ? "GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS" : "GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS";

        GLU_CHECK_GLW_CALL(
            m_gl, getIntegerv(isVertex ? GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS : GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS,
                              &shaderStorageBlockSupported));

        if (shaderStorageBlockSupported < (int)m_buffers.size())
            TCU_THROW(NotSupportedError,
                      ("Test requires " + shaderTypeStr + " >= " + de::toString((int)m_buffers.size()) + ", got " +
                       de::toString(shaderStorageBlockSupported))
                          .c_str());
    }

    glu::ShaderProgram program(m_gl, genSources());

    m_log << program;

    if (!program.isOk())
        TCU_FAIL("Failed to compile shader program");

    GLU_CHECK_GLW_CALL(m_gl, useProgram(program.getProgram()));

    const glw::GLint indexLocation = m_gl.getUniformLocation(program.getProgram(), "u_index");
    GLU_CHECK_GLW_MSG(m_gl, "glGetUniformLocation()");
    TCU_CHECK(indexLocation != (glw::GLint)-1);

    const glw::GLint index = -1;
    GLU_CHECK_GLW_CALL(m_gl, uniform1i(indexLocation, index));

    if (m_shaderType != SHADERTYPE_COMPUTE)
    {
        const glw::GLfloat coords[] = {-1.0f, -1.0f, +1.0f, -1.0f, +1.0f, +1.0f, -1.0f, +1.0f};

        // Setup vertices position
        m_coordLocation = m_gl.getAttribLocation(program.getProgram(), "a_position");
        GLU_CHECK_GLW_MSG(m_gl, "glGetAttribLocation()");
        TCU_CHECK(m_coordLocation != (glw::GLint)-1);

        // Load the vertex data
        m_coordinatesBuffer = 0;
        GLU_CHECK_GLW_CALL(m_gl, genBuffers(1, &m_coordinatesBuffer));
        GLU_CHECK_GLW_CALL(m_gl, bindBuffer(GL_ARRAY_BUFFER, m_coordinatesBuffer));
        GLU_CHECK_GLW_CALL(m_gl, bufferData(GL_ARRAY_BUFFER, (glw::GLsizeiptr)sizeof(coords), coords, GL_STATIC_DRAW));
        GLU_CHECK_GLW_CALL(m_gl, enableVertexAttribArray(m_coordLocation));
        GLU_CHECK_GLW_CALL(m_gl, vertexAttribPointer(m_coordLocation, 2, GL_FLOAT, GL_FALSE, 0, nullptr));
    }

    // Create unused data for filling buffer objects
    const std::vector<tcu::Vec4> refValues(s_numBindings, tcu::Vec4(0.0f, 1.0f, 1.0f, 1.0f));

    if (m_isLocalArray && m_shaderType == SHADERTYPE_COMPUTE)
    {
        // Setup output buffer
        GLU_CHECK_GLW_CALL(m_gl, genBuffers((glw::GLsizei)1u, &m_buffers[0]));

        GLU_CHECK_GLW_CALL(m_gl, bindBuffer(GL_SHADER_STORAGE_BUFFER, m_buffers[0]));
        GLU_CHECK_GLW_CALL(m_gl,
                           bufferData(GL_SHADER_STORAGE_BUFFER, sizeof(tcu::Vec4), &(refValues[0]), GL_STATIC_DRAW));
        GLU_CHECK_GLW_CALL(m_gl, bindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_buffers[0]));
    }
    else if (!m_isLocalArray)
    {
        // Set up interface block of buffer bindings
        GLU_CHECK_GLW_CALL(m_gl, genBuffers((glw::GLsizei)m_buffers.size(), &m_buffers[0]));

        for (int bufNdx = 0; bufNdx < (int)m_buffers.size(); ++bufNdx)
        {
            const glw::GLenum resType = m_isUBO && (m_shaderType != SHADERTYPE_COMPUTE || bufNdx != 0) ?
                                            GL_UNIFORM_BUFFER :
                                            GL_SHADER_STORAGE_BUFFER;

            GLU_CHECK_GLW_CALL(m_gl, bindBuffer(resType, m_buffers[bufNdx]));
            GLU_CHECK_GLW_CALL(m_gl, bufferData(resType, sizeof(tcu::Vec4), &(refValues[bufNdx]), GL_STATIC_DRAW));
            GLU_CHECK_GLW_CALL(m_gl, bindBufferBase(resType, bufNdx, m_buffers[bufNdx]));
        }
    }
}

void ShadersOOB::draw(void)
{
    if (m_shaderType == SHADERTYPE_COMPUTE)
        m_gl.dispatchCompute(1, 1, 1);
    else
    {
        const glw::GLuint indices[] = {0, 1, 2, 2, 3, 0};
        m_gl.drawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, indices);
    }
}

void ShadersOOB::teardown(void)
{
    if (m_shaderType != SHADERTYPE_COMPUTE)
    {
        if (m_coordLocation)
        {
            GLU_CHECK_GLW_CALL(m_gl, disableVertexAttribArray(m_coordLocation));
            m_coordLocation = 0;
        }
    }

    if (m_coordinatesBuffer)
    {
        GLU_CHECK_GLW_CALL(m_gl, deleteBuffers(1, &m_coordinatesBuffer));
        m_coordinatesBuffer = 0;
    }

    if (!m_isLocalArray)
    {
        if (!m_buffers.empty())
        {
            GLU_CHECK_GLW_CALL(m_gl, deleteBuffers((glw::GLsizei)m_buffers.size(), &m_buffers[0]));
            m_buffers.clear();
        }
    }

    GLU_CHECK_GLW_CALL(m_gl, useProgram(0));
}

class QueryRobustAccessCase : public RobustnessTestCase
{
public:
    QueryRobustAccessCase(EglTestContext &eglTestCtx, const char *name, const char *description)
        : RobustnessTestCase(eglTestCtx, name, description)
    {
    }

    TestCase::IterateResult iterate(void)
    {
        TestLog &log = m_testCtx.getLog();

        log << tcu::TestLog::Message
            << "Check that after successfully creating a robust context the robust access query returned by "
               "glBooleanv() equals GL_TRUE\n\n"
            << tcu::TestLog::EndMessage;

        const EGLint attribList[] = {EGL_CONTEXT_CLIENT_VERSION,
                                     3,
                                     EGL_CONTEXT_MINOR_VERSION_KHR,
                                     0,
                                     EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT,
                                     EGL_TRUE,
                                     EGL_NONE};

        checkRequiredEGLExtensions(attribList);

        RenderingContext context(m_eglTestCtx, attribList, m_eglConfig, m_eglDisplay, EGL_NO_CONTEXT);
        context.makeCurrent(m_eglSurface);

        glw::Functions gl;
        {
            const glu::ApiType apiType(3, 0, glu::PROFILE_ES);
            context.initGLFunctions(&gl, apiType);
            checkRequiredGLSupport(gl, apiType);
        }

        uint8_t robustAccessGL;
        gl.getBooleanv(GL_CONTEXT_ROBUST_ACCESS_EXT, &robustAccessGL);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glGetBooleanv()");

        if (robustAccessGL != GL_TRUE)
        {
            log << TestLog::Message << "Invalid GL_CONTEXT_ROBUST_ACCESS returned by glGetBooleanv(). Got '"
                << robustAccessGL << "' expected GL_TRUE." << TestLog::EndMessage;

            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
            return STOP;
        }

        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
        return STOP;
    }
};

class NoResetNotificationCase : public RobustnessTestCase
{
public:
    NoResetNotificationCase(EglTestContext &eglTestCtx, const char *name, const char *description)
        : RobustnessTestCase(eglTestCtx, name, description)
    {
    }

    TestCase::IterateResult iterate(void)
    {
        TestLog &log = m_testCtx.getLog();

        log << tcu::TestLog::Message
            << "Check the reset notification strategy returned by glGetIntegerv() equals GL_NO_RESET_NOTIFICATION\n\n"
            << tcu::TestLog::EndMessage;

        const EGLint attribList[] = {EGL_CONTEXT_CLIENT_VERSION,
                                     3,
                                     EGL_CONTEXT_MINOR_VERSION_KHR,
                                     0,
                                     EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT,
                                     EGL_TRUE,
                                     EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT,
                                     EGL_NO_RESET_NOTIFICATION,
                                     EGL_NONE};

        checkRequiredEGLExtensions(attribList);

        RenderingContext context(m_eglTestCtx, attribList, m_eglConfig, m_eglDisplay, EGL_NO_CONTEXT);
        context.makeCurrent(m_eglSurface);

        glw::Functions gl;
        {
            const glu::ApiType apiType(3, 0, glu::PROFILE_ES);
            context.initGLFunctions(&gl, apiType);
            checkRequiredGLSupport(gl, apiType);
        }

        uint8_t robustAccessGL;
        gl.getBooleanv(GL_CONTEXT_ROBUST_ACCESS_EXT, &robustAccessGL);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glGetBooleanv()");

        glw::GLint reset = 0;
        gl.getIntegerv(GL_RESET_NOTIFICATION_STRATEGY, &reset);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glGetIntegerv()");

        if (reset != GL_NO_RESET_NOTIFICATION)
        {
            log << tcu::TestLog::Message << "Test failed! glGetIntegerv() returned wrong value. ["
                << glu::getErrorStr(reset) << ", expected " << glu::getErrorStr(GL_NO_RESET_NOTIFICATION) << "]"
                << tcu::TestLog::EndMessage;

            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
            return STOP;
        }

        GLU_CHECK_GLW_CALL(gl, getGraphicsResetStatus());

        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
        return STOP;
    }
};

class LoseContextOnResetCase : public RobustnessTestCase
{
public:
    LoseContextOnResetCase(EglTestContext &eglTestCtx, const char *name, const char *description)
        : RobustnessTestCase(eglTestCtx, name, description)
    {
    }

    TestCase::IterateResult iterate(void)
    {
        TestLog &log = m_testCtx.getLog();

        log << tcu::TestLog::Message
            << "Check the reset notification strategy returned by glGetIntegerv() equals GL_LOSE_CONTEXT_ON_RESET\n\n"
            << tcu::TestLog::EndMessage;

        const EGLint attribList[] = {EGL_CONTEXT_CLIENT_VERSION,
                                     3,
                                     EGL_CONTEXT_MINOR_VERSION_KHR,
                                     0,
                                     EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT,
                                     EGL_TRUE,
                                     EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT,
                                     EGL_LOSE_CONTEXT_ON_RESET,
                                     EGL_NONE};

        checkRequiredEGLExtensions(attribList);

        RenderingContext context(m_eglTestCtx, attribList, m_eglConfig, m_eglDisplay, EGL_NO_CONTEXT);
        context.makeCurrent(m_eglSurface);

        glw::Functions gl;
        {
            const glu::ApiType apiType(3, 0, glu::PROFILE_ES);
            context.initGLFunctions(&gl, apiType);
            checkRequiredGLSupport(gl, apiType);
        }
        glw::GLint reset = 0;
        gl.getIntegerv(GL_RESET_NOTIFICATION_STRATEGY, &reset);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glGetIntegerv()");

        if (reset != GL_LOSE_CONTEXT_ON_RESET)
        {
            log << tcu::TestLog::Message << "Test failed! glGetIntegerv() returned wrong value. [" << reset
                << ", expected " << glu::getErrorStr(GL_LOSE_CONTEXT_ON_RESET) << "]" << tcu::TestLog::EndMessage;

            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
            return STOP;
        }

        log << tcu::TestLog::Message << "Check the graphics reset status returned by glGetGraphicsResetStatus() "
            << "equals GL_NO_ERROR\n"
            << tcu::TestLog::EndMessage;

        GLU_CHECK_GLW_CALL(gl, getGraphicsResetStatus());

        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
        return STOP;
    }
};

de::SharedPtr<ContextReset> contextResetFactory(const RobustnessTestCase::Params params, glw::Functions &gl,
                                                tcu::TestLog &log)
{
    if (params.getContextResetType() == CONTEXTRESETTYPE_FIXED_FUNC_OOB)
        return de::SharedPtr<ContextReset>(new FixedFunctionOOB(gl, log, params.getFixedFunctionType()));

    if (params.getContextResetType() == CONTEXTRESETTYPE_SHADER_OOB)
        return de::SharedPtr<ContextReset>(
            new ShadersOOB(gl, log, params.getShaderType(), params.getResourceType(), params.getReadWriteType()));
    else
    {
        DE_FATAL("Unknown context reset type");
        return de::SharedPtr<ContextReset>(nullptr);
    }
}

class ContextResetCase : public RobustnessTestCase
{

public:
    ContextResetCase(EglTestContext &eglTestCtx, const char *name, const char *description, Params params);
    virtual ~ContextResetCase(void)
    {
    }

    virtual void provokeReset(de::SharedPtr<ContextReset> &contextReset) = 0;
    virtual void waitForReset(de::SharedPtr<ContextReset> &contextReset) = 0;
    virtual void passAndLog(de::SharedPtr<ContextReset> &contextReset)   = 0;

    TestCase::IterateResult iterate(void);
    void execute(glw::Functions &gl);

private:
    ContextResetCase(const ContextResetCase &);
    ContextResetCase &operator=(const ContextResetCase &);
};

ContextResetCase::ContextResetCase(EglTestContext &eglTestCtx, const char *name, const char *description, Params params)
    : RobustnessTestCase(eglTestCtx, name, description, params)
{
}

TestCase::IterateResult ContextResetCase::iterate(void)
{
    glw::Functions gl;

    const EGLint attribList[] = {EGL_CONTEXT_CLIENT_VERSION,
                                 3,
                                 EGL_CONTEXT_MINOR_VERSION_KHR,
                                 0,
                                 EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT,
                                 (m_params.getRobustAccessType() == ROBUSTACCESS_TRUE) ? EGL_TRUE : EGL_FALSE,
                                 EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT,
                                 EGL_LOSE_CONTEXT_ON_RESET,
                                 EGL_NONE};

    checkRequiredEGLExtensions(attribList);

    RenderingContext context(m_eglTestCtx, attribList, m_eglConfig, m_eglDisplay, EGL_NO_CONTEXT);
    context.makeCurrent(m_eglSurface);

    {
        const glu::ApiType apiType = paramsToApiType(m_params);
        context.initGLFunctions(&gl, apiType);
        checkGLSupportForParams(gl, m_params);
    }

    execute(gl);

    return STOP;
}

void ContextResetCase::execute(glw::Functions &gl)
{
    de::SharedPtr<ContextReset> contextReset = contextResetFactory(m_params, gl, m_testCtx.getLog());
    glw::GLboolean isContextRobust           = GL_FALSE;

    GLU_CHECK_GLW_CALL(gl, getBooleanv(GL_CONTEXT_ROBUST_ACCESS_EXT, &isContextRobust));
    provokeReset(contextReset);

    if (m_params.getContextResetType() == CONTEXTRESETTYPE_SHADER_OOB ||
        m_params.getContextResetType() == CONTEXTRESETTYPE_FIXED_FUNC_OOB)
    {
        try
        {
            waitForReset(contextReset);
            m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Context was NOT lost. Test skipped");
        }
        catch (const glu::Error &error)
        {
            if (error.getError() == GL_CONTEXT_LOST)
            {
                if (isContextRobust)
                    m_testCtx.setTestResult(QP_TEST_RESULT_FAIL,
                                            "No context reset should of occurred GL_CONTEXT_ROBUST_ACCESS == TRUE");
                else
                    passAndLog(contextReset);
            }
            else if (isContextRobust)
                m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got unknown error.");
            else
            {
                m_testCtx.setTestResult(QP_TEST_RESULT_QUALITY_WARNING,
                                        "Warning: glGetError() returned wrong value. Expected GL_CONTEXT_LOST");

                m_testCtx.getLog() << tcu::TestLog::Message << "Warning: glGetError() returned wrong value ["
                                   << error.what() << ", expected " << glu::getErrorStr(GL_CONTEXT_LOST) << "]"
                                   << tcu::TestLog::EndMessage;
            }
        }
    }
    else
        DE_FATAL("Unknown context reset type");
}

class BasicResetCase : public ContextResetCase
{
public:
    BasicResetCase(EglTestContext &eglTestCtx, const char *name, const char *description, Params params)
        : ContextResetCase(eglTestCtx, name, description, params)
    {
    }

    virtual void provokeReset(de::SharedPtr<ContextReset> &contextReset)
    {
        m_testCtx.getLog() << tcu::TestLog::Message
                           << "Check the graphics reset status returned by glGetGraphicsResetStatus() equals "
                           << "GL_GUILTY_CONTEXT_RESET after a context reset\n\n"
                           << tcu::TestLog::EndMessage;

        contextReset->setup();
        contextReset->draw();
    }

    virtual void waitForReset(de::SharedPtr<ContextReset> &contextReset)
    {
        contextReset->teardown();
        contextReset->finish();
    }

    virtual void passAndLog(de::SharedPtr<ContextReset> &contextReset)
    {
        const glw::GLint status = contextReset->getGraphicsResetStatus();

        if (status == GL_NO_ERROR)
        {
            m_testCtx.getLog() << tcu::TestLog::Message
                               << "Test failed! glGetGraphicsResetStatus() returned wrong value ["
                               << glu::getGraphicsResetStatusStr(status) << ", expected "
                               << glu::getGraphicsResetStatusStr(GL_GUILTY_CONTEXT_RESET) << "]"
                               << tcu::TestLog::EndMessage;

            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
        }
        else
        {
            if (contextReset->getError() != GL_NO_ERROR)
                m_testCtx.setTestResult(QP_TEST_RESULT_FAIL,
                                        "Error flag not reset after calling getGraphicsResetStatus()");
            else
                m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
        }
    }
};

class InvalidShareContextCase : public RobustnessTestCase
{
public:
    InvalidShareContextCase(EglTestContext &eglTestCtx, const char *name, const char *description)
        : RobustnessTestCase(eglTestCtx, name, description)
    {
    }

    TestCase::IterateResult iterate(void)
    {
        TestLog &log       = m_testCtx.getLog();
        const Library &egl = m_eglTestCtx.getLibrary();
        bool isOk          = true;

        log << tcu::TestLog::Message
            << "EGL_BAD_MATCH is generated if reset notification strategies do not match when creating shared "
               "contexts\n\n"
            << tcu::TestLog::EndMessage;

        const EGLint attribListA[] = {EGL_CONTEXT_CLIENT_VERSION,
                                      3,
                                      EGL_CONTEXT_MINOR_VERSION_KHR,
                                      0,
                                      EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT,
                                      EGL_TRUE,
                                      EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT,
                                      EGL_NO_RESET_NOTIFICATION,
                                      EGL_NONE};

        const EGLint attribListB[] = {EGL_CONTEXT_CLIENT_VERSION,
                                      3,
                                      EGL_CONTEXT_MINOR_VERSION_KHR,
                                      0,
                                      EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT,
                                      EGL_TRUE,
                                      EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT,
                                      EGL_LOSE_CONTEXT_ON_RESET,
                                      EGL_NONE};

        checkRequiredEGLExtensions(attribListA);

        log << tcu::TestLog::Message << "Create context A (share_context = EGL_NO_CONTEXT)" << tcu::TestLog::EndMessage;
        RenderingContext contextA(m_eglTestCtx, attribListA, m_eglConfig, m_eglDisplay, EGL_NO_CONTEXT);

        log << tcu::TestLog::Message << "Create context B (share_context = context A)" << tcu::TestLog::EndMessage;
        logAttribList(m_eglTestCtx, attribListB);

        EGLContext contextB = egl.createContext(m_eglDisplay, m_eglConfig, contextA.getContext(), attribListB);

        const EGLenum error = egl.getError();
        if (error != EGL_BAD_MATCH)
        {
            log << TestLog::Message << "Test failed! eglCreateContext() returned with error ["
                << eglu::getErrorStr(error) << ", expected " << eglu::getErrorStr(EGL_BAD_MATCH) << "]"
                << TestLog::EndMessage;

            isOk = false;
        }

        if (contextB != EGL_NO_CONTEXT)
            egl.destroyContext(m_eglDisplay, contextB);

        if (isOk)
            m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
        else
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");

        return STOP;
    }
};

class InvalidNotificationEnumCase : public RobustnessTestCase
{
public:
    InvalidNotificationEnumCase(EglTestContext &eglTestCtx, const char *name, const char *description)
        : RobustnessTestCase(eglTestCtx, name, description)
    {
    }

    TestCase::IterateResult iterate(void)
    {
        TestLog &log       = m_testCtx.getLog();
        const Library &egl = m_eglTestCtx.getLibrary();
        bool isOk          = true;

        log << tcu::TestLog::Message
            << "EGL_BAD_ATTRIBUTE is generated if EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR is used with EGL "
               "versions <= 1.4\n\n"
            << tcu::TestLog::EndMessage;

        const EGLint attribList[] = {EGL_CONTEXT_CLIENT_VERSION,
                                     3,
                                     EGL_CONTEXT_MINOR_VERSION_KHR,
                                     1,
                                     EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR,
                                     EGL_NO_RESET_NOTIFICATION,
                                     EGL_NONE};

        if (eglu::getVersion(egl, m_eglDisplay) >= eglu::Version(1, 5))
        {
            m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "Test requires EGL version to be under 1.5");
            return STOP;
        }

        logAttribList(m_eglTestCtx, attribList);
        EGLContext context = egl.createContext(m_eglDisplay, m_eglConfig, EGL_NO_CONTEXT, attribList);

        const EGLenum error = egl.getError();
        if (error != EGL_BAD_ATTRIBUTE)
        {
            log << TestLog::Message << "Test failed! eglCreateContext() returned with error ["
                << eglu::getErrorStr(error) << ", expected " << eglu::getErrorStr(EGL_BAD_ATTRIBUTE) << "]"
                << TestLog::EndMessage;

            isOk = false;
        }

        if (context != EGL_NO_CONTEXT)
            egl.destroyContext(m_eglDisplay, context);

        if (isOk)
            m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
        else
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");

        return STOP;
    }
};

class InvalidContextCase : public RobustnessTestCase
{
public:
    InvalidContextCase(EglTestContext &eglTestCtx, const char *name, const char *description)
        : RobustnessTestCase(eglTestCtx, name, description)
    {
    }

    TestCase::IterateResult iterate(void)
    {
        const Library &egl = m_eglTestCtx.getLibrary();
        TestLog &log       = m_testCtx.getLog();
        bool isOk          = true;

        log << tcu::TestLog::Message
            << "EGL_BAD_ATTRIBUTE is generated if EXT_create_context_robustness is NOT supported but "
               "EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT is specified\n\n"
            << tcu::TestLog::EndMessage;

        const EGLint attribList[] = {EGL_CONTEXT_CLIENT_VERSION,
                                     3,
                                     EGL_CONTEXT_MINOR_VERSION_KHR,
                                     0,
                                     EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT,
                                     EGL_LOSE_CONTEXT_ON_RESET,
                                     EGL_NONE};

        if (eglu::hasExtension(egl, m_eglDisplay, "EGL_EXT_create_context_robustness"))
        {
            m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED,
                                    "Test requires EGL_EXT_create_context_robustness to be unsupported");
            return STOP;
        }

        logAttribList(m_eglTestCtx, attribList);
        EGLContext context = egl.createContext(m_eglDisplay, m_eglConfig, EGL_NO_CONTEXT, attribList);

        const EGLenum error = egl.getError();
        if (error != EGL_BAD_ATTRIBUTE)
        {
            log << TestLog::Message << "Test failed! eglCreateContext() returned with error ["
                << eglu::getErrorStr(error) << ", expected " << eglu::getErrorStr(EGL_BAD_ATTRIBUTE) << "]"
                << TestLog::EndMessage;

            isOk = false;
        }

        if (context != EGL_NO_CONTEXT)
            egl.destroyContext(m_eglDisplay, context);

        if (isOk)
            m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
        else
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");

        return STOP;
    }
};

} // namespace

// Note: Tests limited to openGLES 3.1 contexts only
TestCaseGroup *createRobustnessTests(EglTestContext &eglTestCtx)
{
    de::MovePtr<TestCaseGroup> group(new TestCaseGroup(eglTestCtx, "robustness", "KHR_robustness tests"));

    tcu::TestCaseGroup *const contextCreationTestGroup =
        new TestCaseGroup(eglTestCtx, "create_context", "Test valid context_creation attributes");
    tcu::TestCaseGroup *const contextResetTestGroup =
        new TestCaseGroup(eglTestCtx, "reset_context", "Test context resets scenarios");
    tcu::TestCaseGroup *const negativeContextTestGroup =
        new TestCaseGroup(eglTestCtx, "negative_context", "Test invalid context creation attributes");

    tcu::TestCaseGroup *const shadersTestGroup =
        new TestCaseGroup(eglTestCtx, "shaders", "Shader specific context reset tests");
    tcu::TestCaseGroup *const fixedFunctionTestGroup = new TestCaseGroup(
        eglTestCtx, "fixed_function_pipeline", "Fixed function pipeline context reset tests with robust context");
    tcu::TestCaseGroup *const fixedFunctionNonRobustTestGroup =
        new TestCaseGroup(eglTestCtx, "fixed_function_pipeline_non_robust",
                          "Fixed function pipeline context reset tests with non-robust context");

    tcu::TestCaseGroup *const outOfBoundsTestGroup =
        new TestCaseGroup(eglTestCtx, "out_of_bounds", "Out of bounds access scenarios with robust context");

    tcu::TestCaseGroup *const outOfBoundsNonRobustTestGroup = new TestCaseGroup(
        eglTestCtx, "out_of_bounds_non_robust", "Out of bounds access scenarios with non-robust context");

    const string resetScenarioDescription = "query error states and reset notifications";

    // out-of-bounds test cases
    {
        // robust context
        tcu::TestCaseGroup *const uboReadArrayResetTestGroup =
            new TestCaseGroup(eglTestCtx, "uniform_block", "Uniform Block Accesses");
        tcu::TestCaseGroup *const uboWriteArrayResetTestGroup =
            new TestCaseGroup(eglTestCtx, "uniform_block", "Uniform Block Accesses");
        tcu::TestCaseGroup *const ssboWriteArrayResetTestGroup =
            new TestCaseGroup(eglTestCtx, "shader_storage_block", "Shader Storage Block accesses");
        tcu::TestCaseGroup *const ssboReadArrayResetTestGroup =
            new TestCaseGroup(eglTestCtx, "shader_storage_block", "Shader Storage Block accesses");
        tcu::TestCaseGroup *const localWriteArrayResetTestGroup =
            new TestCaseGroup(eglTestCtx, "local_array", "Local array accesses");
        tcu::TestCaseGroup *const localReadArrayResetTestGroup =
            new TestCaseGroup(eglTestCtx, "local_array", "Local array accesses");

        // non-robust context (internal use only)
        tcu::TestCaseGroup *const uboReadArrayResetNonRobustTestGroup =
            new TestCaseGroup(eglTestCtx, "uniform_block", "Uniform Block Accesses");
        tcu::TestCaseGroup *const uboWriteArrayResetNonRobustTestGroup =
            new TestCaseGroup(eglTestCtx, "uniform_block", "Uniform Block Accesses");
        tcu::TestCaseGroup *const ssboWriteArrayResetNonRobustTestGroup =
            new TestCaseGroup(eglTestCtx, "shader_storage_block", "Shader Storage Block accesses");
        tcu::TestCaseGroup *const ssboReadArrayResetNonRobustTestGroup =
            new TestCaseGroup(eglTestCtx, "shader_storage_block", "Shader Storage Block accesses");
        tcu::TestCaseGroup *const localWriteArrayResetNonRobustTestGroup =
            new TestCaseGroup(eglTestCtx, "local_array", "Local array accesses");
        tcu::TestCaseGroup *const localReadArrayResetNonRobustTestGroup =
            new TestCaseGroup(eglTestCtx, "local_array", "Local array accesses");

        static const RobustnessTestCase::Params s_outOfBoundReadCases[] = {
            // ubo read only
            RobustnessTestCase::Params("vertex", "Provoke a context reset in vertex shader and ", ROBUSTACCESS_TRUE,
                                       CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_VERT, RESOURCETYPE_UBO,
                                       READWRITETYPE_READ),
            RobustnessTestCase::Params("fragment", "Provoke a context reset in fragment shader and ", ROBUSTACCESS_TRUE,
                                       CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_FRAG, RESOURCETYPE_UBO,
                                       READWRITETYPE_READ),
            RobustnessTestCase::Params(
                "vertex_and_fragment", "Provoke a context reset in vertex and fragment shader and ", ROBUSTACCESS_TRUE,
                CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_VERT_AND_FRAG, RESOURCETYPE_UBO, READWRITETYPE_READ),
            RobustnessTestCase::Params("compute", "Provoke a context reset in compute shader and ", ROBUSTACCESS_TRUE,
                                       CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_COMPUTE, RESOURCETYPE_UBO,
                                       READWRITETYPE_READ),

            // ssbo read only
            RobustnessTestCase::Params("vertex", "Provoke a context reset in vertex shader and ", ROBUSTACCESS_TRUE,
                                       CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_VERT, RESOURCETYPE_SSBO,
                                       READWRITETYPE_READ),
            RobustnessTestCase::Params("fragment", "Provoke a context reset in fragment shader and ", ROBUSTACCESS_TRUE,
                                       CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_FRAG, RESOURCETYPE_SSBO,
                                       READWRITETYPE_READ),
            RobustnessTestCase::Params(
                "vertex_and_fragment", "Provoke a context reset in vertex and fragment shader and ", ROBUSTACCESS_TRUE,
                CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_VERT_AND_FRAG, RESOURCETYPE_SSBO, READWRITETYPE_READ),
            RobustnessTestCase::Params("compute", "Provoke a context reset in compute shader and ", ROBUSTACCESS_TRUE,
                                       CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_COMPUTE, RESOURCETYPE_SSBO,
                                       READWRITETYPE_READ),

            // local array read only
            RobustnessTestCase::Params("vertex", "Provoke a context reset in vertex shader and ", ROBUSTACCESS_TRUE,
                                       CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_VERT, RESOURCETYPE_LOCAL_ARRAY,
                                       READWRITETYPE_READ),
            RobustnessTestCase::Params("fragment", "Provoke a context reset in fragment shader and ", ROBUSTACCESS_TRUE,
                                       CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_FRAG, RESOURCETYPE_LOCAL_ARRAY,
                                       READWRITETYPE_READ),
            RobustnessTestCase::Params(
                "vertex_and_fragment", "Provoke a context reset in vertex and fragment shader and ", ROBUSTACCESS_TRUE,
                CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_VERT_AND_FRAG, RESOURCETYPE_LOCAL_ARRAY, READWRITETYPE_READ),
            RobustnessTestCase::Params("compute", "Provoke a context reset in compute shader and ", ROBUSTACCESS_TRUE,
                                       CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_COMPUTE, RESOURCETYPE_LOCAL_ARRAY,
                                       READWRITETYPE_READ),

            // ubo read only (non-robust)
            RobustnessTestCase::Params("vertex", "Provoke a context reset in vertex shader and ", ROBUSTACCESS_FALSE,
                                       CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_VERT, RESOURCETYPE_UBO,
                                       READWRITETYPE_READ),
            RobustnessTestCase::Params("fragment", "Provoke a context reset in fragment shader and ",
                                       ROBUSTACCESS_FALSE, CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_FRAG,
                                       RESOURCETYPE_UBO, READWRITETYPE_READ),
            RobustnessTestCase::Params(
                "vertex_and_fragment", "Provoke a context reset in vertex and fragment shader and ", ROBUSTACCESS_FALSE,
                CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_VERT_AND_FRAG, RESOURCETYPE_UBO, READWRITETYPE_READ),
            RobustnessTestCase::Params("compute", "Provoke a context reset in compute shader and ", ROBUSTACCESS_FALSE,
                                       CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_COMPUTE, RESOURCETYPE_UBO,
                                       READWRITETYPE_READ),

            // ssbo read only (non-robust)
            RobustnessTestCase::Params("vertex", "Provoke a context reset in vertex shader and ", ROBUSTACCESS_FALSE,
                                       CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_VERT, RESOURCETYPE_SSBO,
                                       READWRITETYPE_READ),
            RobustnessTestCase::Params("fragment", "Provoke a context reset in fragment shader and ",
                                       ROBUSTACCESS_FALSE, CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_FRAG,
                                       RESOURCETYPE_SSBO, READWRITETYPE_READ),
            RobustnessTestCase::Params(
                "vertex_and_fragment", "Provoke a context reset in vertex and fragment shader and ", ROBUSTACCESS_FALSE,
                CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_VERT_AND_FRAG, RESOURCETYPE_SSBO, READWRITETYPE_READ),
            RobustnessTestCase::Params("compute", "Provoke a context reset in compute shader and ", ROBUSTACCESS_FALSE,
                                       CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_COMPUTE, RESOURCETYPE_SSBO,
                                       READWRITETYPE_READ),

            // local array read only (non-robust)
            RobustnessTestCase::Params("vertex", "Provoke a context reset in vertex shader and ", ROBUSTACCESS_FALSE,
                                       CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_VERT, RESOURCETYPE_LOCAL_ARRAY,
                                       READWRITETYPE_READ),
            RobustnessTestCase::Params("fragment", "Provoke a context reset in fragment shader and ",
                                       ROBUSTACCESS_FALSE, CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_FRAG,
                                       RESOURCETYPE_LOCAL_ARRAY, READWRITETYPE_READ),
            RobustnessTestCase::Params(
                "vertex_and_fragment", "Provoke a context reset in vertex and fragment shader and ", ROBUSTACCESS_FALSE,
                CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_VERT_AND_FRAG, RESOURCETYPE_LOCAL_ARRAY, READWRITETYPE_READ),
            RobustnessTestCase::Params("compute", "Provoke a context reset in compute shader and ", ROBUSTACCESS_FALSE,
                                       CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_COMPUTE, RESOURCETYPE_LOCAL_ARRAY,
                                       READWRITETYPE_READ),
        };

        for (int testNdx = 0; testNdx < DE_LENGTH_OF_ARRAY(s_outOfBoundReadCases); ++testNdx)
        {
            const RobustnessTestCase::Params &test = s_outOfBoundReadCases[testNdx];

            if (test.getResourceType() == RESOURCETYPE_UBO && test.getRobustAccessType() == ROBUSTACCESS_TRUE)
                uboReadArrayResetTestGroup->addChild(
                    new BasicResetCase(eglTestCtx, test.getName().c_str(),
                                       (test.getDescription() + resetScenarioDescription).c_str(), test));

            if (test.getResourceType() == RESOURCETYPE_UBO && test.getRobustAccessType() == ROBUSTACCESS_FALSE)
                uboReadArrayResetNonRobustTestGroup->addChild(
                    new BasicResetCase(eglTestCtx, test.getName().c_str(),
                                       (test.getDescription() + resetScenarioDescription).c_str(), test));

            if (test.getResourceType() == RESOURCETYPE_SSBO && test.getRobustAccessType() == ROBUSTACCESS_TRUE)
                ssboReadArrayResetTestGroup->addChild(
                    new BasicResetCase(eglTestCtx, test.getName().c_str(),
                                       (test.getDescription() + resetScenarioDescription).c_str(), test));

            if (test.getResourceType() == RESOURCETYPE_SSBO && test.getRobustAccessType() == ROBUSTACCESS_FALSE)
                ssboReadArrayResetNonRobustTestGroup->addChild(
                    new BasicResetCase(eglTestCtx, test.getName().c_str(),
                                       (test.getDescription() + resetScenarioDescription).c_str(), test));

            if (test.getResourceType() == RESOURCETYPE_LOCAL_ARRAY && test.getRobustAccessType() == ROBUSTACCESS_TRUE)
                localReadArrayResetTestGroup->addChild(
                    new BasicResetCase(eglTestCtx, test.getName().c_str(),
                                       (test.getDescription() + resetScenarioDescription).c_str(), test));

            if (test.getResourceType() == RESOURCETYPE_LOCAL_ARRAY && test.getRobustAccessType() == ROBUSTACCESS_FALSE)
                localReadArrayResetNonRobustTestGroup->addChild(
                    new BasicResetCase(eglTestCtx, test.getName().c_str(),
                                       (test.getDescription() + resetScenarioDescription).c_str(), test));
        }

        static const RobustnessTestCase::Params s_outOfBoundWriteCases[] = {
            // ubo write only
            RobustnessTestCase::Params("vertex", "Provoke a context reset in vertex shader and ", ROBUSTACCESS_TRUE,
                                       CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_VERT, RESOURCETYPE_UBO,
                                       READWRITETYPE_WRITE),
            RobustnessTestCase::Params("fragment", "Provoke a context reset in fragment shader and ", ROBUSTACCESS_TRUE,
                                       CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_FRAG, RESOURCETYPE_UBO,
                                       READWRITETYPE_WRITE),
            RobustnessTestCase::Params(
                "vertex_and_fragment", "Provoke a context reset in vertex and fragment shader and ", ROBUSTACCESS_TRUE,
                CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_VERT_AND_FRAG, RESOURCETYPE_UBO, READWRITETYPE_WRITE),
            RobustnessTestCase::Params("compute", "Provoke a context reset in compute shader and ", ROBUSTACCESS_TRUE,
                                       CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_COMPUTE, RESOURCETYPE_UBO,
                                       READWRITETYPE_WRITE),

            // ssbo write only
            RobustnessTestCase::Params("vertex", "Provoke a context reset in vertex shader and ", ROBUSTACCESS_TRUE,
                                       CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_VERT, RESOURCETYPE_SSBO,
                                       READWRITETYPE_WRITE),
            RobustnessTestCase::Params("fragment", "Provoke a context reset in fragment shader and ", ROBUSTACCESS_TRUE,
                                       CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_FRAG, RESOURCETYPE_SSBO,
                                       READWRITETYPE_WRITE),
            RobustnessTestCase::Params(
                "vertex_and_fragment", "Provoke a context reset in vertex and fragment shader and ", ROBUSTACCESS_TRUE,
                CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_VERT_AND_FRAG, RESOURCETYPE_SSBO, READWRITETYPE_WRITE),
            RobustnessTestCase::Params("compute", "Provoke a context reset in compute shader and ", ROBUSTACCESS_TRUE,
                                       CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_COMPUTE, RESOURCETYPE_SSBO,
                                       READWRITETYPE_WRITE),

            // local array write only
            RobustnessTestCase::Params("vertex", "Provoke a context reset in vertex shader and ", ROBUSTACCESS_TRUE,
                                       CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_VERT, RESOURCETYPE_LOCAL_ARRAY,
                                       READWRITETYPE_WRITE),
            RobustnessTestCase::Params("fragment", "Provoke a context reset in fragment shader and ", ROBUSTACCESS_TRUE,
                                       CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_FRAG, RESOURCETYPE_LOCAL_ARRAY,
                                       READWRITETYPE_WRITE),
            RobustnessTestCase::Params(
                "vertex_and_fragment", "Provoke a context reset in vertex and fragment shader and ", ROBUSTACCESS_TRUE,
                CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_VERT_AND_FRAG, RESOURCETYPE_LOCAL_ARRAY, READWRITETYPE_WRITE),
            RobustnessTestCase::Params("compute", "Provoke a context reset in compute shader and ", ROBUSTACCESS_TRUE,
                                       CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_COMPUTE, RESOURCETYPE_LOCAL_ARRAY,
                                       READWRITETYPE_WRITE),

            // ubo write only (non-robust)
            RobustnessTestCase::Params("vertex", "Provoke a context reset in vertex shader and ", ROBUSTACCESS_FALSE,
                                       CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_VERT, RESOURCETYPE_UBO,
                                       READWRITETYPE_WRITE),
            RobustnessTestCase::Params("fragment", "Provoke a context reset in fragment shader and ",
                                       ROBUSTACCESS_FALSE, CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_FRAG,
                                       RESOURCETYPE_UBO, READWRITETYPE_WRITE),
            RobustnessTestCase::Params(
                "vertex_and_fragment", "Provoke a context reset in vertex and fragment shader and ", ROBUSTACCESS_FALSE,
                CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_VERT_AND_FRAG, RESOURCETYPE_UBO, READWRITETYPE_WRITE),
            RobustnessTestCase::Params("compute", "Provoke a context reset in compute shader and ", ROBUSTACCESS_FALSE,
                                       CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_COMPUTE, RESOURCETYPE_UBO,
                                       READWRITETYPE_WRITE),

            // ssbo write only (non-robust)
            RobustnessTestCase::Params("vertex", "Provoke a context reset in vertex shader and ", ROBUSTACCESS_FALSE,
                                       CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_VERT, RESOURCETYPE_SSBO,
                                       READWRITETYPE_WRITE),
            RobustnessTestCase::Params("fragment", "Provoke a context reset in fragment shader and ",
                                       ROBUSTACCESS_FALSE, CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_FRAG,
                                       RESOURCETYPE_SSBO, READWRITETYPE_WRITE),
            RobustnessTestCase::Params(
                "vertex_and_fragment", "Provoke a context reset in vertex and fragment shader and ", ROBUSTACCESS_FALSE,
                CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_VERT_AND_FRAG, RESOURCETYPE_SSBO, READWRITETYPE_WRITE),
            RobustnessTestCase::Params("compute", "Provoke a context reset in compute shader and ", ROBUSTACCESS_FALSE,
                                       CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_COMPUTE, RESOURCETYPE_SSBO,
                                       READWRITETYPE_WRITE),

            // local array write only (non-robust)
            RobustnessTestCase::Params("vertex", "Provoke a context reset in vertex shader and ", ROBUSTACCESS_FALSE,
                                       CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_VERT, RESOURCETYPE_LOCAL_ARRAY,
                                       READWRITETYPE_WRITE),
            RobustnessTestCase::Params("fragment", "Provoke a context reset in fragment shader and ",
                                       ROBUSTACCESS_FALSE, CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_FRAG,
                                       RESOURCETYPE_LOCAL_ARRAY, READWRITETYPE_WRITE),
            RobustnessTestCase::Params(
                "vertex_and_fragment", "Provoke a context reset in vertex and fragment shader and ", ROBUSTACCESS_FALSE,
                CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_VERT_AND_FRAG, RESOURCETYPE_LOCAL_ARRAY, READWRITETYPE_WRITE),
            RobustnessTestCase::Params("compute", "Provoke a context reset in compute shader and ", ROBUSTACCESS_FALSE,
                                       CONTEXTRESETTYPE_SHADER_OOB, SHADERTYPE_COMPUTE, RESOURCETYPE_LOCAL_ARRAY,
                                       READWRITETYPE_WRITE),
        };

        for (int testNdx = 0; testNdx < DE_LENGTH_OF_ARRAY(s_outOfBoundWriteCases); ++testNdx)
        {
            const RobustnessTestCase::Params &test = s_outOfBoundWriteCases[testNdx];

            if (test.getResourceType() == RESOURCETYPE_UBO && test.getRobustAccessType() == ROBUSTACCESS_TRUE)
                uboWriteArrayResetTestGroup->addChild(
                    new BasicResetCase(eglTestCtx, test.getName().c_str(),
                                       (test.getDescription() + resetScenarioDescription).c_str(), test));

            if (test.getResourceType() == RESOURCETYPE_UBO && test.getRobustAccessType() == ROBUSTACCESS_FALSE)
                uboWriteArrayResetNonRobustTestGroup->addChild(
                    new BasicResetCase(eglTestCtx, test.getName().c_str(),
                                       (test.getDescription() + resetScenarioDescription).c_str(), test));

            if (test.getResourceType() == RESOURCETYPE_SSBO && test.getRobustAccessType() == ROBUSTACCESS_TRUE)
                ssboWriteArrayResetTestGroup->addChild(
                    new BasicResetCase(eglTestCtx, test.getName().c_str(),
                                       (test.getDescription() + resetScenarioDescription).c_str(), test));

            if (test.getResourceType() == RESOURCETYPE_SSBO && test.getRobustAccessType() == ROBUSTACCESS_FALSE)
                ssboWriteArrayResetNonRobustTestGroup->addChild(
                    new BasicResetCase(eglTestCtx, test.getName().c_str(),
                                       (test.getDescription() + resetScenarioDescription).c_str(), test));

            if (test.getResourceType() == RESOURCETYPE_LOCAL_ARRAY && test.getRobustAccessType() == ROBUSTACCESS_TRUE)
                localWriteArrayResetTestGroup->addChild(
                    new BasicResetCase(eglTestCtx, test.getName().c_str(),
                                       (test.getDescription() + resetScenarioDescription).c_str(), test));

            if (test.getResourceType() == RESOURCETYPE_LOCAL_ARRAY && test.getRobustAccessType() == ROBUSTACCESS_FALSE)
                localWriteArrayResetNonRobustTestGroup->addChild(
                    new BasicResetCase(eglTestCtx, test.getName().c_str(),
                                       (test.getDescription() + resetScenarioDescription).c_str(), test));
        }

        // robust Context
        tcu::TestCaseGroup *const outOfBoundsResetReadAccessTestGroup =
            new TestCaseGroup(eglTestCtx, "reads", "Out of bounds read accesses");
        tcu::TestCaseGroup *const outOfBoundsResetWriteAccessTestGroup =
            new TestCaseGroup(eglTestCtx, "writes", "Out of bounds write accesses");

        outOfBoundsResetReadAccessTestGroup->addChild(uboReadArrayResetTestGroup);
        outOfBoundsResetReadAccessTestGroup->addChild(ssboReadArrayResetTestGroup);
        outOfBoundsResetReadAccessTestGroup->addChild(localReadArrayResetTestGroup);

        outOfBoundsResetWriteAccessTestGroup->addChild(uboWriteArrayResetTestGroup);
        outOfBoundsResetWriteAccessTestGroup->addChild(ssboWriteArrayResetTestGroup);
        outOfBoundsResetWriteAccessTestGroup->addChild(localWriteArrayResetTestGroup);

        tcu::TestCaseGroup *const outOfBoundsResetTestGroup = new TestCaseGroup(
            eglTestCtx, "reset_status", "Tests that query the reset status after a context reset has occurred");

        outOfBoundsResetTestGroup->addChild(outOfBoundsResetReadAccessTestGroup);
        outOfBoundsResetTestGroup->addChild(outOfBoundsResetWriteAccessTestGroup);

        outOfBoundsTestGroup->addChild(outOfBoundsResetTestGroup);

        // non-robust Context (internal use only)
        tcu::TestCaseGroup *const outOfBoundsResetReadAccessNonRobustTestGroup =
            new TestCaseGroup(eglTestCtx, "reads", "Out of bounds read accesses");
        tcu::TestCaseGroup *const outOfBoundsResetWriteAccessNonRobustTestGroup =
            new TestCaseGroup(eglTestCtx, "writes", "Out of bounds write accesses");

        outOfBoundsResetReadAccessNonRobustTestGroup->addChild(uboReadArrayResetNonRobustTestGroup);
        outOfBoundsResetReadAccessNonRobustTestGroup->addChild(ssboReadArrayResetNonRobustTestGroup);
        outOfBoundsResetReadAccessNonRobustTestGroup->addChild(localReadArrayResetNonRobustTestGroup);

        outOfBoundsResetWriteAccessNonRobustTestGroup->addChild(uboWriteArrayResetNonRobustTestGroup);
        outOfBoundsResetWriteAccessNonRobustTestGroup->addChild(ssboWriteArrayResetNonRobustTestGroup);
        outOfBoundsResetWriteAccessNonRobustTestGroup->addChild(localWriteArrayResetNonRobustTestGroup);

        tcu::TestCaseGroup *const outOfBoundsResetNonRobustTestGroup = new TestCaseGroup(
            eglTestCtx, "reset_status", "Tests that query the reset status after a context reset has occurred");

        outOfBoundsResetNonRobustTestGroup->addChild(outOfBoundsResetReadAccessNonRobustTestGroup);
        outOfBoundsResetNonRobustTestGroup->addChild(outOfBoundsResetWriteAccessNonRobustTestGroup);

        outOfBoundsNonRobustTestGroup->addChild(outOfBoundsResetNonRobustTestGroup);
    }

    // fixed function test cases
    {
        // robust context
        tcu::TestCaseGroup *const fixedFunctionResetStatusTestGroup = new TestCaseGroup(
            eglTestCtx, "reset_status", "Tests that query the reset status after a context reset has occurred");

        // non-robust context (internal use only)
        tcu::TestCaseGroup *const fixedFunctionResetStatusNonRobustTestGroup = new TestCaseGroup(
            eglTestCtx, "reset_status", "Tests that query the reset status after a context reset has occurred");

        static const RobustnessTestCase::Params s_fixedFunctionPipelineCases[] = {
            RobustnessTestCase::Params("index_buffer_out_of_bounds",
                                       "Provoke context reset and query error states and reset notifications",
                                       ROBUSTACCESS_TRUE, CONTEXTRESETTYPE_FIXED_FUNC_OOB, FIXEDFUNCTIONTYPE_INDICES),
            RobustnessTestCase::Params("vertex_buffer_out_of_bounds",
                                       "Provoke context reset and query error states and reset notifications",
                                       ROBUSTACCESS_TRUE, CONTEXTRESETTYPE_FIXED_FUNC_OOB, FIXEDFUNCTIONTYPE_VERTICES),

            RobustnessTestCase::Params("index_buffer_out_of_bounds",
                                       "Provoke context reset and query error states and reset notifications",
                                       ROBUSTACCESS_FALSE, CONTEXTRESETTYPE_FIXED_FUNC_OOB, FIXEDFUNCTIONTYPE_INDICES),
            RobustnessTestCase::Params("vertex_buffer_out_of_bounds",
                                       "Provoke context reset and query error states and reset notifications",
                                       ROBUSTACCESS_FALSE, CONTEXTRESETTYPE_FIXED_FUNC_OOB, FIXEDFUNCTIONTYPE_VERTICES),
        };

        for (int testNdx = 0; testNdx < DE_LENGTH_OF_ARRAY(s_fixedFunctionPipelineCases); ++testNdx)
        {
            const RobustnessTestCase::Params &test = s_fixedFunctionPipelineCases[testNdx];
            if (test.getRobustAccessType() == ROBUSTACCESS_TRUE)
                fixedFunctionResetStatusTestGroup->addChild(
                    new BasicResetCase(eglTestCtx, test.getName().c_str(), test.getDescription().c_str(), test));
            else
                fixedFunctionResetStatusNonRobustTestGroup->addChild(
                    new BasicResetCase(eglTestCtx, test.getName().c_str(), test.getDescription().c_str(), test));
        }

        fixedFunctionTestGroup->addChild(fixedFunctionResetStatusTestGroup);
        fixedFunctionNonRobustTestGroup->addChild(fixedFunctionResetStatusNonRobustTestGroup);
    }

    // context creation query cases
    {
        contextCreationTestGroup->addChild(new QueryRobustAccessCase(
            eglTestCtx, "query_robust_access", "Query robust access after successfully creating a robust context"));
        contextCreationTestGroup->addChild(
            new NoResetNotificationCase(eglTestCtx, "no_reset_notification",
                                        "Query reset notification strategy after specifying GL_NO_RESET_NOTIFICATION"));
        contextCreationTestGroup->addChild(
            new LoseContextOnResetCase(eglTestCtx, "lose_context_on_reset",
                                       "Query reset notification strategy after specifying GL_LOSE_CONTEXT_ON_RESET"));
    }

    // invalid context creation cases
    {
        negativeContextTestGroup->addChild(
            new InvalidContextCase(eglTestCtx, "invalid_robust_context_creation",
                                   "Create a non-robust context but specify a reset notification strategy"));
        negativeContextTestGroup->addChild(
            new InvalidShareContextCase(eglTestCtx, "invalid_robust_shared_context_creation",
                                        "Create a context share group with conflicting reset notification strategies"));
        negativeContextTestGroup->addChild(new InvalidNotificationEnumCase(
            eglTestCtx, "invalid_notification_strategy_enum",
            "Create a robust context using EGL 1.5 only enum with EGL versions <= 1.4"));
    }

    shadersTestGroup->addChild(outOfBoundsTestGroup);
    shadersTestGroup->addChild(outOfBoundsNonRobustTestGroup);

    contextResetTestGroup->addChild(shadersTestGroup);
    contextResetTestGroup->addChild(fixedFunctionTestGroup);
    contextResetTestGroup->addChild(fixedFunctionNonRobustTestGroup);

    group->addChild(contextCreationTestGroup);
    group->addChild(contextResetTestGroup);
    group->addChild(negativeContextTestGroup);

    return group.release();
}

} // namespace egl
} // namespace deqp
