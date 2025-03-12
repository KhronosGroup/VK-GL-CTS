/*-------------------------------------------------------------------------
 * drawElements Quality Program EGL Module
 * ---------------------------------------
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
 * \brief Android-specific operations.
 *//*--------------------------------------------------------------------*/

#include "teglAndroidUtil.hpp"

#include "deStringUtil.hpp"
#include "tcuTextureUtil.hpp"
#include "gluTextureUtil.hpp"
#include "glwEnums.hpp"
#include "eglwLibrary.hpp"
#include "eglwEnums.hpp"

namespace deqp
{
namespace egl
{
namespace Image
{

using de::MovePtr;
using eglu::AttribMap;
using std::string;
using tcu::PixelBufferAccess;
using tcu::Texture2D;
using tcu::TextureFormat;
using namespace glw;
using namespace eglw;

#if (DE_OS != DE_OS_ANDROID)

MovePtr<ImageSource> createAndroidNativeImageSource(GLenum format, uint32_t numLayers, bool isYUV)
{
    DE_UNREF(numLayers);
    return createUnsupportedImageSource("Not Android platform", format, isYUV);
}

#else // DE_OS == DE_OS_ANDROID

#if defined(__ANDROID_API_O__) && (DE_ANDROID_API >= __ANDROID_API_O__)
#define BUILT_WITH_ANDROID_HARDWARE_BUFFER 1
#endif

#if defined(__ANDROID_API_P__) && (DE_ANDROID_API >= __ANDROID_API_P__)
#define BUILT_WITH_ANDROID_P_HARDWARE_BUFFER 1
#endif

#if !defined(BUILT_WITH_ANDROID_HARDWARE_BUFFER)

MovePtr<ImageSource> createAndroidNativeImageSource(GLenum format, uint32_t numLayers, bool isYUV)
{
    DE_UNREF(numLayers);
    return createUnsupportedImageSource("AHB API not supported", format, isYUV);
}

#else // defined(BUILT_WITH_ANDROID_HARDWARE_BUFFER)

namespace
{

#include <sys/system_properties.h>
#include <android/hardware_buffer.h>
#include "deDynamicLibrary.hpp"

const uint32_t AHB_FORMAT_Y8Cb8Cr8_420 = 0x23;

int32_t androidGetSdkVersion(void)
{
    static int32_t sdkVersion = -1;
    if (sdkVersion < 0)
    {
        char value[128] = {0};
        __system_property_get("ro.build.version.sdk", value);
        sdkVersion = static_cast<int32_t>(strtol(value, nullptr, 10));
        printf("SDK Version is %d\n", sdkVersion);
    }
    return sdkVersion;
}

typedef int (*pfnAHardwareBuffer_allocate)(const AHardwareBuffer_Desc *desc, AHardwareBuffer **outBuffer);
typedef void (*pfnAHardwareBuffer_describe)(const AHardwareBuffer *buffer, AHardwareBuffer_Desc *outDesc);
typedef void (*pfnAHardwareBuffer_acquire)(AHardwareBuffer *buffer);
typedef void (*pfnAHardwareBuffer_release)(AHardwareBuffer *buffer);
typedef int (*pfnAHardwareBuffer_isSupported)(const AHardwareBuffer_Desc *desc);

struct AhbFunctions
{
    pfnAHardwareBuffer_allocate allocate;
    pfnAHardwareBuffer_describe describe;
    pfnAHardwareBuffer_acquire acquire;
    pfnAHardwareBuffer_release release;
    pfnAHardwareBuffer_isSupported isSupported;
};

AhbFunctions ahbFunctions;

bool ahbFunctionsLoaded(AhbFunctions *pAhbFunctions, int32_t sdkVersion)
{
    static bool ahbApiLoaded = false;
    if (ahbApiLoaded || ((pAhbFunctions->allocate != nullptr) && (pAhbFunctions->describe != nullptr) &&
                         (pAhbFunctions->acquire != nullptr) && (pAhbFunctions->release != nullptr) &&
                         (pAhbFunctions->isSupported != nullptr || sdkVersion < 29)))
    {
        ahbApiLoaded = true;
        return true;
    }
    return false;
}

bool loadAhbDynamicApis(int32_t sdkVersion)
{
    if (!ahbFunctionsLoaded(&ahbFunctions, sdkVersion))
    {
        static de::DynamicLibrary libnativewindow("libnativewindow.so");
        ahbFunctions.allocate =
            reinterpret_cast<pfnAHardwareBuffer_allocate>(libnativewindow.getFunction("AHardwareBuffer_allocate"));
        ahbFunctions.describe =
            reinterpret_cast<pfnAHardwareBuffer_describe>(libnativewindow.getFunction("AHardwareBuffer_describe"));
        ahbFunctions.acquire =
            reinterpret_cast<pfnAHardwareBuffer_acquire>(libnativewindow.getFunction("AHardwareBuffer_acquire"));
        ahbFunctions.release =
            reinterpret_cast<pfnAHardwareBuffer_release>(libnativewindow.getFunction("AHardwareBuffer_release"));
        if (sdkVersion >= 29)
            ahbFunctions.isSupported = reinterpret_cast<pfnAHardwareBuffer_isSupported>(
                libnativewindow.getFunction("AHardwareBuffer_isSupported"));
        else
            ahbFunctions.isSupported = nullptr;

        return ahbFunctionsLoaded(&ahbFunctions, sdkVersion);
    }

    return true;
}

uint32_t getPixelFormat(GLenum format)
{
    switch (format)
    {
    case GL_RGB565:
        return AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM;
    case GL_RGB8:
        return AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM;
    case GL_RGBA8:
        return AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
    case GL_DEPTH_COMPONENT16:
        return AHARDWAREBUFFER_FORMAT_D16_UNORM;
    case GL_DEPTH_COMPONENT24:
        return AHARDWAREBUFFER_FORMAT_D24_UNORM;
    case GL_DEPTH24_STENCIL8:
        return AHARDWAREBUFFER_FORMAT_D24_UNORM_S8_UINT;
    case GL_DEPTH_COMPONENT32F:
        return AHARDWAREBUFFER_FORMAT_D32_FLOAT;
    case GL_DEPTH32F_STENCIL8:
        return AHARDWAREBUFFER_FORMAT_D32_FLOAT_S8_UINT;
    case GL_RGB10_A2:
        return AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM;
    case GL_RGBA16F:
        return AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT;
    case GL_STENCIL_INDEX8:
        return AHARDWAREBUFFER_FORMAT_S8_UINT;

    default:
        TCU_THROW(NotSupportedError, "Texture format unsupported by Android");
    }
}

class AndroidNativeClientBuffer : public ClientBuffer
{
public:
    AndroidNativeClientBuffer(const Library &egl, GLenum format, uint32_t numLayers, bool isYUV);
    ~AndroidNativeClientBuffer(void);
    EGLClientBuffer get(void) const;
    void lock(void **data);
    void unlock(void);
    AHardwareBuffer_Desc describe(void);

private:
    const Library &m_egl;
    AHardwareBuffer *m_hardwareBuffer;
};

AndroidNativeClientBuffer::AndroidNativeClientBuffer(const Library &egl, GLenum format, uint32_t numLayers, bool isYUV)
    : m_egl(egl)
{
    int32_t sdkVersion = androidGetSdkVersion();

#if defined(BUILT_WITH_ANDROID_P_HARDWARE_BUFFER)
    // When testing AHB on Android-P and newer the CTS must be compiled against API28 or newer.
    DE_TEST_ASSERT(sdkVersion >= 28); /*__ANDROID_API_P__ */
#else
    // When testing AHB on Android-O and newer the CTS must be compiled against API26 or newer.
    DE_TEST_ASSERT(sdkVersion >= 26); /* __ANDROID_API_O__ */
#endif // !defined(BUILT_WITH_ANDROID_P_HARDWARE_BUFFER)

    if (!loadAhbDynamicApis(sdkVersion))
    {
        // Couldn't load Android AHB system APIs.
        DE_TEST_ASSERT(false);
    }

    AHardwareBuffer_Desc hbufferdesc = {
        64u,
        64u,
        numLayers,
        isYUV ? AHB_FORMAT_Y8Cb8Cr8_420 : getPixelFormat(format),
        AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN | AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY |
            AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE | AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT,
        0u, // Stride in pixels, ignored for AHardwareBuffer_allocate()
        0u, // Initialize to zero, reserved for future use
        0u  // Initialize to zero, reserved for future use
    };

    // If we have AHardwareBuffer_isSupported use that before trying the allocation.
    if (ahbFunctions.isSupported != nullptr)
    {
        if (!ahbFunctions.isSupported(&hbufferdesc))
            TCU_THROW(NotSupportedError, "Texture format unsupported");
    }

    if (ahbFunctions.allocate(&hbufferdesc, &m_hardwareBuffer) != 0)
    {
        // Throw unsupported instead of failing the test as the texture format or the number
        // of layers might be unsupported.
        TCU_THROW(NotSupportedError, "AHB allocation failed");
    }
}

AndroidNativeClientBuffer::~AndroidNativeClientBuffer(void)
{
    ahbFunctions.release(m_hardwareBuffer);
}

EGLClientBuffer AndroidNativeClientBuffer::get(void) const
{
    typedef EGLW_APICALL EGLClientBuffer(EGLW_APIENTRY *
                                         eglGetNativeClientBufferANDROIDFunc)(const struct AHardwareBuffer *buffer);
    return ((eglGetNativeClientBufferANDROIDFunc)m_egl.getProcAddress("eglGetNativeClientBufferANDROID"))(
        m_hardwareBuffer);
}

void AndroidNativeClientBuffer::lock(void **data)
{
    const int status =
        AHardwareBuffer_lock(m_hardwareBuffer, AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY, -1, nullptr, data);

    if (status != 0)
        TCU_FAIL(("AHardwareBuffer_lock failed with error: " + de::toString(status)).c_str());
}

void AndroidNativeClientBuffer::unlock(void)
{
    const int status = AHardwareBuffer_unlock(m_hardwareBuffer, nullptr);

    if (status != 0)
        TCU_FAIL(("AHardwareBuffer_unlock failed with error: " + de::toString(status)).c_str());
}

AHardwareBuffer_Desc AndroidNativeClientBuffer::describe(void)
{
    AHardwareBuffer_Desc ret;
    ahbFunctions.describe(m_hardwareBuffer, &ret);
    return ret;
}

class AndroidNativeImageSource : public ImageSource
{
public:
    AndroidNativeImageSource(GLenum format, uint32_t numLayers, bool isYUV)
        : m_format(format)
        , m_numLayers(numLayers)
        , m_isY8Cb8Cr8_420(isYUV)
    {
    }
    ~AndroidNativeImageSource(void);
    MovePtr<ClientBuffer> createBuffer(const Library &egl, const glw::Functions &, Texture2D *) const;
    string getRequiredExtension(void) const
    {
        return "EGL_ANDROID_get_native_client_buffer";
    }
    EGLImageKHR createImage(const Library &egl, EGLDisplay dpy, EGLContext ctx, EGLClientBuffer clientBuffer) const;
    GLenum getEffectiveFormat(void) const
    {
        return m_format;
    }
    bool isYUVFormatImage(void) const
    {
        return m_isY8Cb8Cr8_420;
    }

protected:
    GLenum m_format;
    uint32_t m_numLayers;
    bool m_isY8Cb8Cr8_420;
};

AndroidNativeImageSource::~AndroidNativeImageSource(void)
{
}

MovePtr<ClientBuffer> AndroidNativeImageSource::createBuffer(const Library &egl, const glw::Functions &,
                                                             Texture2D *ref) const
{
    MovePtr<AndroidNativeClientBuffer> buffer(
        new AndroidNativeClientBuffer(egl, m_format, m_numLayers, m_isY8Cb8Cr8_420));

    if (ref != nullptr)
    {
        const TextureFormat texFormat = glu::mapGLInternalFormat(m_format);
        void *bufferData              = nullptr;

        *ref                  = Texture2D(texFormat, 64, 64);
        ref->m_yuvTextureUsed = m_isY8Cb8Cr8_420;
        ref->allocLevel(0);
        tcu::fillWithComponentGradients(ref->getLevel(0), tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f),
                                        tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));

        // AHB doesn't allow locking a layered image. In that case the data
        // will be initialized later using OpenGL API.
        // YUV format texture will be initialized by glClear.

        if (m_numLayers == 1u && !m_isY8Cb8Cr8_420)
        {
            buffer->lock(&bufferData);
            {
                AHardwareBuffer_Desc desc = buffer->describe();
                const int rowPitch        = texFormat.getPixelSize() * desc.stride;
                const int slicePitch      = rowPitch * desc.height;
                PixelBufferAccess nativeBuffer(texFormat, desc.width, desc.height, 1, rowPitch, slicePitch, bufferData);

                tcu::copy(nativeBuffer, ref->getLevel(0));
            }
            buffer->unlock();
        }
    }
    return MovePtr<ClientBuffer>(buffer);
}

EGLImageKHR AndroidNativeImageSource::createImage(const Library &egl, EGLDisplay dpy, EGLContext,
                                                  EGLClientBuffer clientBuffer) const
{
    static const EGLint attribs[] = {EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE};
    const EGLImageKHR image = egl.createImageKHR(dpy, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, clientBuffer, attribs);

    EGLU_CHECK_MSG(egl, "eglCreateImageKHR()");
    return image;
}

} // namespace

MovePtr<ImageSource> createAndroidNativeImageSource(GLenum format, uint32_t numLayers, bool isYUV)
{
    try
    {
        return MovePtr<ImageSource>(new AndroidNativeImageSource(format, numLayers, isYUV));
    }
    catch (const std::runtime_error &exc)
    {
        return createUnsupportedImageSource(string("Android native buffers unsupported: ") + exc.what(), format, isYUV);
    }
}

#endif // defined(BUILT_WITH_ANDROID_HARDWARE_BUFFER)

#endif // DE_OS == DE_OS_ANDROID

} // namespace Image
} // namespace egl
} // namespace deqp
