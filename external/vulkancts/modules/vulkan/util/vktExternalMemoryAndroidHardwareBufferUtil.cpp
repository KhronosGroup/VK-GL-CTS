/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2023 The Khronos Group Inc.
 * Copyright (c) 2023 Google Inc.
 * Copyright (c) 2023 LunarG Inc.
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
 * \brief Vulkan external memory utilities for Android Hardware Buffer
 *//*--------------------------------------------------------------------*/

#include "vktExternalMemoryAndroidHardwareBufferUtil.hpp"

#ifndef CTS_USES_VULKANSC

#if (DE_OS == DE_OS_ANDROID)
#include <sys/system_properties.h>

#if defined(__ANDROID_API_O__) && (DE_ANDROID_API >= __ANDROID_API_O__)
#include <android/hardware_buffer.h>
#include "deDynamicLibrary.hpp"
#define BUILT_WITH_ANDROID_HARDWARE_BUFFER 1
#endif // defined(__ANDROID_API_O__) && (DE_ANDROID_API >= __ANDROID_API_O__)

#if defined(__ANDROID_API_P__) && (DE_ANDROID_API >= __ANDROID_API_P__)
#define BUILT_WITH_ANDROID_P_HARDWARE_BUFFER 1
#endif // defined(__ANDROID_API_P__) && (DE_ANDROID_API >= __ANDROID_API_P__)

#if defined(__ANDROID_API_T__) && (DE_ANDROID_API >= __ANDROID_API_T__)
#define BUILT_WITH_ANDROID_T_HARDWARE_BUFFER 1
#endif // defined(__ANDROID_API_T__) && (DE_ANDROID_API >= __ANDROID_API_T__)

#if defined(__ANDROID_API_U__) && (DE_ANDROID_API >= __ANDROID_API_U__)
#define BUILT_WITH_ANDROID_U_HARDWARE_BUFFER 1
#endif // defined(__ANDROID_API_U__) && (DE_ANDROID_API >= __ANDROID_API_U__)

#endif // (DE_OS == DE_OS_ANDROID)

namespace vkt
{

namespace ExternalMemoryUtil
{

#if (DE_OS == DE_OS_ANDROID)

static deInt32 androidGetSdkVersion()
{
	static deInt32 sdkVersion = -1;
	if (sdkVersion < 0)
	{
		char value[128] = {0};
		__system_property_get("ro.build.version.sdk", value);
		sdkVersion = static_cast<deInt32>(strtol(value, DE_NULL, 10));
		printf("SDK Version is %d\n", sdkVersion);
	}
	return sdkVersion;
}

static deInt32 checkAnbApiBuild()
{
	deInt32 sdkVersion = androidGetSdkVersion();
#if !defined(BUILT_WITH_ANDROID_HARDWARE_BUFFER)
	// When testing AHB on Android-O and newer the CTS must be compiled against API26 or newer.
	DE_TEST_ASSERT(!(sdkVersion >= 26)); /* __ANDROID_API_O__ */
#endif // !defined(BUILT_WITH_ANDROID_HARDWARE_BUFFER)
#if !defined(BUILT_WITH_ANDROID_P_HARDWARE_BUFFER)
	// When testing AHB on Android-P and newer the CTS must be compiled against API28 or newer.
	DE_TEST_ASSERT(!(sdkVersion >= 28)); /*__ANDROID_API_P__ */
#endif // !defined(BUILT_WITH_ANDROID_P_HARDWARE_BUFFER)
#if !defined(BUILT_WITH_ANDROID_T_HARDWARE_BUFFER)
	// When testing AHB on Android-T and newer the CTS must be compiled against API33 or newer.
	DE_TEST_ASSERT(!(sdkVersion >= 33)); /*__ANDROID_API_T__ */
#endif // !defined(BUILT_WITH_ANDROID_T_HARDWARE_BUFFER)
#if !defined(BUILT_WITH_ANDROID_U_HARDWARE_BUFFER)
	// When testing AHB on Android-U and newer the CTS must be compiled against API34 or newer.
	DE_TEST_ASSERT(!(sdkVersion >= 34)); /*__ANDROID_API_U__ */
#endif // !defined(BUILT_WITH_ANDROID_U_HARDWARE_BUFFER)
	return sdkVersion;
}

bool AndroidHardwareBufferExternalApi::supportsAhb()
{
	return (checkAnbApiBuild() >= __ANDROID_API_O__);
}

bool AndroidHardwareBufferExternalApi::supportsCubeMap()
{
	return (checkAnbApiBuild() >= 28);
}

AndroidHardwareBufferExternalApi::AndroidHardwareBufferExternalApi()
{
	deInt32 sdkVersion = checkAnbApiBuild();
	if(sdkVersion >= __ANDROID_API_O__)
	{
#if defined(BUILT_WITH_ANDROID_HARDWARE_BUFFER)
		if (!loadAhbDynamicApis(sdkVersion))
		{
			// Couldn't load  Android AHB system APIs.
			DE_TEST_ASSERT(false);
		}
#else
		// Invalid Android AHB APIs configuration. Please check the instructions on how to build NDK for Android.
		DE_TEST_ASSERT(false);
#endif // defined(BUILT_WITH_ANDROID_HARDWARE_BUFFER)
	}
}

AndroidHardwareBufferExternalApi::~AndroidHardwareBufferExternalApi()
{
}

#if defined(BUILT_WITH_ANDROID_HARDWARE_BUFFER)
typedef int  (*pfn_system_property_get)(const char *, char *);
typedef int  (*pfnAHardwareBuffer_allocate)(const AHardwareBuffer_Desc* desc, AHardwareBuffer** outBuffer);
typedef void (*pfnAHardwareBuffer_describe)(const AHardwareBuffer* buffer, AHardwareBuffer_Desc* outDesc);
typedef void (*pfnAHardwareBuffer_acquire)(AHardwareBuffer* buffer);
typedef void (*pfnAHardwareBuffer_release)(AHardwareBuffer* buffer);
typedef int  (*pfnAHardwareBuffer_lock)(AHardwareBuffer* buffer, uint64_t usage, int32_t fence, const ARect* rect, void** outVirtualAddress);
typedef int  (*pfnAHardwareBuffer_unlock)(AHardwareBuffer* buffer, int32_t* fence);

#if defined(BUILT_WITH_ANDROID_T_HARDWARE_BUFFER)
typedef int (*pfnAHardwareBuffer_lockPlanes)(AHardwareBuffer* buffer, uint64_t usage, int32_t fence, const ARect* rect, AHardwareBuffer_Planes* outPlanes);
#endif // defined(BUILT_WITH_ANDROID_T_HARDWARE_BUFFER)

struct AhbFunctions
{
	pfnAHardwareBuffer_allocate		allocate;
	pfnAHardwareBuffer_describe		describe;
	pfnAHardwareBuffer_acquire		acquire;
	pfnAHardwareBuffer_release		release;
	pfnAHardwareBuffer_lock			lock;
	pfnAHardwareBuffer_unlock		unlock;
#if defined(BUILT_WITH_ANDROID_T_HARDWARE_BUFFER)
	pfnAHardwareBuffer_lockPlanes	lockPlanes; // Introduced in SDK 29
#endif // defined(BUILT_WITH_ANDROID_T_HARDWARE_BUFFER)
};

static AhbFunctions ahbFunctions;

static bool ahbFunctionsLoaded(AhbFunctions* pAhbFunctions)
{
	static bool ahbApiLoaded = false;
	if (ahbApiLoaded ||
		((pAhbFunctions->allocate		!= DE_NULL) &&
		 (pAhbFunctions->describe		!= DE_NULL) &&
		 (pAhbFunctions->acquire		!= DE_NULL) &&
		 (pAhbFunctions->release		!= DE_NULL) &&
		 (pAhbFunctions->lock			!= DE_NULL) &&
		 (pAhbFunctions->unlock			!= DE_NULL)
#if defined(BUILT_WITH_ANDROID_T_HARDWARE_BUFFER)
		&& (pAhbFunctions->lockPlanes	!= DE_NULL)
#endif // defined(BUILT_WITH_ANDROID_T_HARDWARE_BUFFER)
		))
	{
		ahbApiLoaded = true;
		return true;
	}
	return false;
}

bool AndroidHardwareBufferExternalApi::loadAhbDynamicApis(deInt32 sdkVersion)
{
	if(sdkVersion >= __ANDROID_API_O__)
	{
		if (!ahbFunctionsLoaded(&ahbFunctions))
		{
			static de::DynamicLibrary libnativewindow("libnativewindow.so");
			ahbFunctions.allocate	= reinterpret_cast<pfnAHardwareBuffer_allocate>(libnativewindow.getFunction("AHardwareBuffer_allocate"));
			ahbFunctions.describe	= reinterpret_cast<pfnAHardwareBuffer_describe>(libnativewindow.getFunction("AHardwareBuffer_describe"));
			ahbFunctions.acquire	= reinterpret_cast<pfnAHardwareBuffer_acquire>(libnativewindow.getFunction("AHardwareBuffer_acquire"));
			ahbFunctions.release	= reinterpret_cast<pfnAHardwareBuffer_release>(libnativewindow.getFunction("AHardwareBuffer_release"));
			ahbFunctions.lock		= reinterpret_cast<pfnAHardwareBuffer_lock>(libnativewindow.getFunction("AHardwareBuffer_lock"));
			ahbFunctions.unlock		= reinterpret_cast<pfnAHardwareBuffer_unlock>(libnativewindow.getFunction("AHardwareBuffer_unlock"));
#if defined(BUILT_WITH_ANDROID_T_HARDWARE_BUFFER)
			ahbFunctions.lockPlanes	= reinterpret_cast<pfnAHardwareBuffer_lockPlanes>(libnativewindow.getFunction("AHardwareBuffer_lockPlanes"));
#endif // defined(BUILT_WITH_ANDROID_T_HARDWARE_BUFFER)

			return ahbFunctionsLoaded(&ahbFunctions);
		}
		else
		{
			return true;
		}
	}

	return false;
}

class AndroidHardwareBufferExternalApi26 : public  AndroidHardwareBufferExternalApi
{
public:

	vk::pt::AndroidHardwareBufferPtr allocate(deUint32 width, deUint32  height, deUint32 layers, deUint32  format, deUint64 usage) override;
	void acquire(vk::pt::AndroidHardwareBufferPtr buffer) override;
	void release(vk::pt::AndroidHardwareBufferPtr buffer) override;
	void describe(const vk::pt::AndroidHardwareBufferPtr buffer,
				  deUint32* width,
				  deUint32* height,
				  deUint32* layers,
				  deUint32* format,
				  deUint64* usage,
				  deUint32* stride) override;
	void*	lock(vk::pt::AndroidHardwareBufferPtr buffer, deUint64 usage) override;
	bool	lockPlanes(vk::pt::AndroidHardwareBufferPtr	buffer,
					   deUint64							usage,
					   deUint32&						planeCount,
					   void*							planeData[4],
					   deUint32							planeStride[4],
					   deUint32							planeRowStride[4]) override;
	bool	unlock(vk::pt::AndroidHardwareBufferPtr buffer) override;
	deUint64 vkUsageToAhbUsage(vk::VkImageUsageFlagBits vkFlag) override;
	deUint64 vkCreateToAhbUsage(vk::VkImageCreateFlagBits vkFlag) override;
	deUint32 vkFormatToAhbFormat(vk::VkFormat vkFormat) override;
	deUint64 mustSupportAhbUsageFlags() override;
	bool     ahbFormatIsBlob(deUint32 ahbFormat) override { return (ahbFormat == AHARDWAREBUFFER_FORMAT_BLOB); };
	bool     ahbFormatIsYuv(deUint32) override { return false; };
	std::vector<deUint32>	getAllSupportedFormats() override;
	const char*				getFormatAsString(deUint32 format) override;

	AndroidHardwareBufferExternalApi26() : AndroidHardwareBufferExternalApi() {};
	virtual ~AndroidHardwareBufferExternalApi26() {};

private:
	// Stop the compiler generating methods of copy the object
	AndroidHardwareBufferExternalApi26(AndroidHardwareBufferExternalApi26 const& copy);            // Not Implemented
	AndroidHardwareBufferExternalApi26& operator=(AndroidHardwareBufferExternalApi26 const& copy); // Not Implemented
};

vk::pt::AndroidHardwareBufferPtr AndroidHardwareBufferExternalApi26::allocate(	deUint32 width,
																				deUint32 height,
																				deUint32 layers,
																				deUint32 format,
																				deUint64 usage)
{
	AHardwareBuffer_Desc hbufferdesc = {
		width,
		height,
		layers,   // number of images
		format,
		usage,
		0u,       // Stride in pixels, ignored for AHardwareBuffer_allocate()
		0u,       // Initialize to zero, reserved for future use
		0u        // Initialize to zero, reserved for future use
	};

	AHardwareBuffer* hbuffer  = DE_NULL;
	ahbFunctions.allocate(&hbufferdesc, &hbuffer);

	return vk::pt::AndroidHardwareBufferPtr(hbuffer);
}

void AndroidHardwareBufferExternalApi26::acquire(vk::pt::AndroidHardwareBufferPtr buffer)
{
	ahbFunctions.acquire(static_cast<AHardwareBuffer*>(buffer.internal));
}

void AndroidHardwareBufferExternalApi26::release(vk::pt::AndroidHardwareBufferPtr buffer)
{
	ahbFunctions.release(static_cast<AHardwareBuffer*>(buffer.internal));
}

void AndroidHardwareBufferExternalApi26::describe( const vk::pt::AndroidHardwareBufferPtr buffer,
													deUint32* width,
													deUint32* height,
													deUint32* layers,
													deUint32* format,
													deUint64* usage,
													deUint32* stride)
{
	AHardwareBuffer_Desc desc;
	ahbFunctions.describe(static_cast<const AHardwareBuffer*>(buffer.internal), &desc);
	if (width)  *width  = desc.width;
	if (height) *height = desc.height;
	if (layers) *layers = desc.layers;
	if (format) *format = desc.format;
	if (usage)  *usage  = desc.usage;
	if (stride) *stride = desc.stride;
}

void* AndroidHardwareBufferExternalApi26::lock(vk::pt::AndroidHardwareBufferPtr buffer, deUint64 usage)
{
	void*			data	= nullptr;
	int32_t			fence	= -1; // No fence
	const ARect*	rect	= nullptr; // NULL rect means all buffer access
	int				result	= ahbFunctions.lock(static_cast<AHardwareBuffer*>(buffer.internal), usage, fence, rect, &data);
	return result == 0 ? data : nullptr;
}

bool AndroidHardwareBufferExternalApi26::lockPlanes(vk::pt::AndroidHardwareBufferPtr, deUint64, deUint32&, void*[4], deUint32[4], deUint32[4])
{
	// SDK 26 does not support locking planes
	return false;
}

bool AndroidHardwareBufferExternalApi26::unlock(vk::pt::AndroidHardwareBufferPtr buffer)
{
	int32_t*	fence	= nullptr; // No fence

	// 0 is the success code for the unlock function
	return (ahbFunctions.unlock(static_cast<AHardwareBuffer*>(buffer.internal), fence) == 0u);
}

deUint64 AndroidHardwareBufferExternalApi26::vkUsageToAhbUsage(vk::VkImageUsageFlagBits vkFlags)
{
	switch(vkFlags)
	{
	  case vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT:
	  case vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT:
		// No AHB equivalent.
		return 0u;
	  case vk::VK_IMAGE_USAGE_SAMPLED_BIT:
		return AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;
	  case vk::VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT:
		return AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;
	  case vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT:
	  case vk::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT:
		// Alias of AHARDWAREBUFFER_USAGE_GPU_FRAMEBUFFER which is defined in later Android API versions.
		return AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT;
	  default:
		  return 0u;
	}
}

deUint64 AndroidHardwareBufferExternalApi26::vkCreateToAhbUsage(vk::VkImageCreateFlagBits vkFlags)
{
	switch(vkFlags)
	{
	  case vk::VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT:
	  case vk::VK_IMAGE_CREATE_EXTENDED_USAGE_BIT:
		// No AHB equivalent.
		return 0u;
	  case vk::VK_IMAGE_CREATE_PROTECTED_BIT:
		return AHARDWAREBUFFER_USAGE_PROTECTED_CONTENT;
	  default:
		return 0u;
	}
}

deUint32 AndroidHardwareBufferExternalApi26::vkFormatToAhbFormat(vk::VkFormat vkFormat)
{
	 switch(vkFormat)
	 {
	   case vk::VK_FORMAT_R8G8B8A8_UNORM:
		 return AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
	   case vk::VK_FORMAT_R8G8B8_UNORM:
		 return AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM;
	   case vk::VK_FORMAT_R5G6B5_UNORM_PACK16:
		 return AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM;
	   case vk::VK_FORMAT_R16G16B16A16_SFLOAT:
		 return AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT;
	   case vk::VK_FORMAT_A2B10G10R10_UNORM_PACK32:
		 return AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM;
	   default:
		 return 0u;
	 }
}

deUint64 AndroidHardwareBufferExternalApi26::mustSupportAhbUsageFlags()
{
	return (AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE | AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT);
}

std::vector<deUint32>	AndroidHardwareBufferExternalApi26::getAllSupportedFormats()
{
	std::vector<deUint32>	formats =
	{
		AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM,
		AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM,
		AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM,
		AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM,
		AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT,
		AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM,
		AHARDWAREBUFFER_FORMAT_BLOB,
	};

	return formats;
}

const char*				AndroidHardwareBufferExternalApi26::getFormatAsString (deUint32 format)
{
	switch(format)
	{
	case AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM:
		return "AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM";
	case AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM:
		return "AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM";
	case AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM:
		return "AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM";
	case AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM:
		return "AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM";
	case AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT:
		return "AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT";
	case AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM:
		return "AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM";
	case AHARDWAREBUFFER_FORMAT_BLOB:
		return "AHARDWAREBUFFER_FORMAT_BLOB";
	default:
	return "Unknown";
	}
}

#if defined(BUILT_WITH_ANDROID_P_HARDWARE_BUFFER)
class AndroidHardwareBufferExternalApi28 : public  AndroidHardwareBufferExternalApi26
{
public:

	deUint64 vkCreateToAhbUsage(vk::VkImageCreateFlagBits vkFlag) override;
	deUint32 vkFormatToAhbFormat(vk::VkFormat vkFormat) override;
	deUint64 mustSupportAhbUsageFlags() override;
	std::vector<deUint32>	getAllSupportedFormats() override;
	const char*				getFormatAsString(deUint32 format) override;

	AndroidHardwareBufferExternalApi28() : AndroidHardwareBufferExternalApi26() {};
	virtual ~AndroidHardwareBufferExternalApi28() {};

private:
	// Stop the compiler generating methods of copy the object
	AndroidHardwareBufferExternalApi28(AndroidHardwareBufferExternalApi28 const& copy);            // Not Implemented
	AndroidHardwareBufferExternalApi28& operator=(AndroidHardwareBufferExternalApi28 const& copy); // Not Implemented
};

deUint64 AndroidHardwareBufferExternalApi28::vkCreateToAhbUsage(vk::VkImageCreateFlagBits vkFlags)
{
	switch(vkFlags)
	{
	  case vk::VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT:
		return AHARDWAREBUFFER_USAGE_GPU_CUBE_MAP;
	  default:
		return AndroidHardwareBufferExternalApi26::vkCreateToAhbUsage(vkFlags);
	}
}

deUint32 AndroidHardwareBufferExternalApi28::vkFormatToAhbFormat(vk::VkFormat vkFormat)
{
	switch(vkFormat)
	{
	  case vk::VK_FORMAT_D16_UNORM:
		return AHARDWAREBUFFER_FORMAT_D16_UNORM;
	  case vk::VK_FORMAT_X8_D24_UNORM_PACK32:
		return AHARDWAREBUFFER_FORMAT_D24_UNORM;
	  case vk::VK_FORMAT_D24_UNORM_S8_UINT:
		return AHARDWAREBUFFER_FORMAT_D24_UNORM_S8_UINT;
	  case vk::VK_FORMAT_D32_SFLOAT:
		return AHARDWAREBUFFER_FORMAT_D32_FLOAT;
	  case vk::VK_FORMAT_D32_SFLOAT_S8_UINT:
		return AHARDWAREBUFFER_FORMAT_D32_FLOAT_S8_UINT;
	  case vk::VK_FORMAT_S8_UINT:
		return AHARDWAREBUFFER_FORMAT_S8_UINT;
	  default:
		return AndroidHardwareBufferExternalApi26::vkFormatToAhbFormat(vkFormat);
	}
}

deUint64 AndroidHardwareBufferExternalApi28::mustSupportAhbUsageFlags()
{
	return AndroidHardwareBufferExternalApi26::mustSupportAhbUsageFlags() | AHARDWAREBUFFER_USAGE_GPU_CUBE_MAP | AHARDWAREBUFFER_USAGE_GPU_MIPMAP_COMPLETE;
}

std::vector<deUint32>	AndroidHardwareBufferExternalApi28::getAllSupportedFormats()
{
	std::vector<deUint32>	formats	= AndroidHardwareBufferExternalApi26::getAllSupportedFormats();

	formats.emplace_back(AHARDWAREBUFFER_FORMAT_D16_UNORM);
	formats.emplace_back(AHARDWAREBUFFER_FORMAT_D24_UNORM);
	formats.emplace_back(AHARDWAREBUFFER_FORMAT_D24_UNORM_S8_UINT);
	formats.emplace_back(AHARDWAREBUFFER_FORMAT_D32_FLOAT);
	formats.emplace_back(AHARDWAREBUFFER_FORMAT_D32_FLOAT_S8_UINT);
	formats.emplace_back(AHARDWAREBUFFER_FORMAT_S8_UINT);

	return formats;
}

const char*				AndroidHardwareBufferExternalApi28::getFormatAsString (deUint32 format)
{
	switch(format)
	{
	case AHARDWAREBUFFER_FORMAT_D16_UNORM:
		return "AHARDWAREBUFFER_FORMAT_D16_UNORM";
	case AHARDWAREBUFFER_FORMAT_D24_UNORM:
		return "AHARDWAREBUFFER_FORMAT_D24_UNORM";
	case AHARDWAREBUFFER_FORMAT_D24_UNORM_S8_UINT:
		return "AHARDWAREBUFFER_FORMAT_D24_UNORM_S8_UINT";
	case AHARDWAREBUFFER_FORMAT_D32_FLOAT:
		return "AHARDWAREBUFFER_FORMAT_D32_FLOAT";
	case AHARDWAREBUFFER_FORMAT_D32_FLOAT_S8_UINT:
		return "AHARDWAREBUFFER_FORMAT_D32_FLOAT_S8_UINT";
	case AHARDWAREBUFFER_FORMAT_S8_UINT:
		return "AHARDWAREBUFFER_FORMAT_S8_UINT";
	default:
		return AndroidHardwareBufferExternalApi26::getFormatAsString(format);
	}
}

#endif // defined(BUILT_WITH_ANDROID_P_HARDWARE_BUFFER)

#if defined(BUILT_WITH_ANDROID_T_HARDWARE_BUFFER)
class AndroidHardwareBufferExternalApi33 : public  AndroidHardwareBufferExternalApi28
{
public:
	bool	lockPlanes(vk::pt::AndroidHardwareBufferPtr	buffer,
					   deUint64							usage,
					   deUint32&						planeCount,
					   void*							planeData[4],
					   deUint32							planeStride[4],
					   deUint32							planeRowStride[4]) override;

	deUint32				vkFormatToAhbFormat(vk::VkFormat vkFormat) override;
	std::vector<deUint32>	getAllSupportedFormats() override;
	const char*				getFormatAsString(deUint32 format) override;
	bool					ahbFormatIsYuv(deUint32 format) override { return (format == AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420) || (format == AHARDWAREBUFFER_FORMAT_YCbCr_P010); }

	AndroidHardwareBufferExternalApi33() : AndroidHardwareBufferExternalApi28() {};
	virtual ~AndroidHardwareBufferExternalApi33() {};

private:
	// Stop the compiler generating methods of copy the object
	AndroidHardwareBufferExternalApi33(AndroidHardwareBufferExternalApi33 const& copy);            // Not Implemented
	AndroidHardwareBufferExternalApi33& operator=(AndroidHardwareBufferExternalApi33 const& copy); // Not Implemented
};

bool AndroidHardwareBufferExternalApi33::lockPlanes(vk::pt::AndroidHardwareBufferPtr	buffer,
													deUint64							usage,
													deUint32&							planeCountOut,
													void*								planeDataOut[4],
													deUint32							planePixelStrideOut[4],
													deUint32							planeRowStrideOut[4])
{
	AHardwareBuffer_Planes	planes;
	int32_t					fence		= -1; // No fence
	const ARect*			rect		= nullptr; // NULL rect means all buffer access
	const int				lockResult	= ahbFunctions.lockPlanes(static_cast<AHardwareBuffer*>(buffer.internal), usage, fence, rect, &planes);
	const bool				succeeded	= (lockResult == 0);

	if (succeeded)
	{
		planeCountOut = planes.planeCount;
		for (deUint32 i = 0; i < planeCountOut; ++i)
		{
			planeDataOut[i] = planes.planes[i].data;
			planePixelStrideOut[i] = planes.planes[i].pixelStride;
			planeRowStrideOut[i] = planes.planes[i].rowStride;
		}
	}

	return succeeded;
}

deUint32 AndroidHardwareBufferExternalApi33::vkFormatToAhbFormat(vk::VkFormat vkFormat)
{
	switch(vkFormat)
	{
	  case vk::VK_FORMAT_R8_UNORM:
		return AHARDWAREBUFFER_FORMAT_R8_UNORM;
	  default:
		return AndroidHardwareBufferExternalApi28::vkFormatToAhbFormat(vkFormat);
	}
}

std::vector<deUint32>	AndroidHardwareBufferExternalApi33::getAllSupportedFormats()
{
	std::vector<deUint32>	formats	= AndroidHardwareBufferExternalApi28::getAllSupportedFormats();

	formats.emplace_back(AHARDWAREBUFFER_FORMAT_R8_UNORM);
	formats.emplace_back(AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420); // Unsure if this was added in SDK 30
	formats.emplace_back(AHARDWAREBUFFER_FORMAT_YCbCr_P010);

	return formats;
}

const char*				AndroidHardwareBufferExternalApi33::getFormatAsString (deUint32 format)
{
	switch(format)
	{
	case AHARDWAREBUFFER_FORMAT_R8_UNORM:
		return "AHARDWAREBUFFER_FORMAT_R8_UNORM";
	case AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420:
		return "AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420";
	case AHARDWAREBUFFER_FORMAT_YCbCr_P010:
		return "AHARDWAREBUFFER_FORMAT_YCbCr_P010";
	default:
		return AndroidHardwareBufferExternalApi28::getFormatAsString(format);
	}
}

#endif // defined(BUILT_WITH_ANDROID_T_HARDWARE_BUFFER)

#if defined(BUILT_WITH_ANDROID_U_HARDWARE_BUFFER)
class AndroidHardwareBufferExternalApi34 : public  AndroidHardwareBufferExternalApi33
{
public:

	deUint32 vkFormatToAhbFormat(vk::VkFormat vkFormat) override;
	std::vector<deUint32>	getAllSupportedFormats() override;
	const char*				getFormatAsString(deUint32 format) override;

	AndroidHardwareBufferExternalApi34() : AndroidHardwareBufferExternalApi33() {};
	virtual ~AndroidHardwareBufferExternalApi34() {};

private:
	// Stop the compiler generating methods of copy the object
	AndroidHardwareBufferExternalApi34(AndroidHardwareBufferExternalApi34 const& copy);            // Not Implemented
	AndroidHardwareBufferExternalApi34& operator=(AndroidHardwareBufferExternalApi34 const& copy); // Not Implemented
};

deUint32 AndroidHardwareBufferExternalApi34::vkFormatToAhbFormat(vk::VkFormat vkFormat)
{
	switch(vkFormat)
	{
	  case vk::VK_FORMAT_R16_UINT:
		return AHARDWAREBUFFER_FORMAT_R16_UINT;
	  case vk::VK_FORMAT_R16G16_UINT:
		return AHARDWAREBUFFER_FORMAT_R16G16_UINT;
	  case vk::VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16:
		return AHARDWAREBUFFER_FORMAT_R10G10B10A10_UNORM;
	  default:
		return AndroidHardwareBufferExternalApi33::vkFormatToAhbFormat(vkFormat);
	}
}

std::vector<deUint32>	AndroidHardwareBufferExternalApi34::getAllSupportedFormats()
{
	std::vector<deUint32>	formats	= AndroidHardwareBufferExternalApi33::getAllSupportedFormats();

	formats.emplace_back(AHARDWAREBUFFER_FORMAT_R16_UINT);
	formats.emplace_back(AHARDWAREBUFFER_FORMAT_R16G16_UINT);
	formats.emplace_back(AHARDWAREBUFFER_FORMAT_R10G10B10A10_UNORM);

	return formats;
}

const char*				AndroidHardwareBufferExternalApi34::getFormatAsString (deUint32 format)
{
	switch(format)
	{
	case AHARDWAREBUFFER_FORMAT_R16_UINT:
		return "AHARDWAREBUFFER_FORMAT_R16_UINT";
	case AHARDWAREBUFFER_FORMAT_R16G16_UINT:
		return "AHARDWAREBUFFER_FORMAT_R16G16_UINT";
	case AHARDWAREBUFFER_FORMAT_R10G10B10A10_UNORM:
		return "AHARDWAREBUFFER_FORMAT_R10G10B10A10_UNORM";
	default:
		return AndroidHardwareBufferExternalApi33::getFormatAsString(format);
	}
}

#endif // defined(BUILT_WITH_ANDROID_U_HARDWARE_BUFFER)
#endif // defined(BUILT_WITH_ANDROID_HARDWARE_BUFFER)
#endif // (DE_OS == DE_OS_ANDROID)

AndroidHardwareBufferExternalApi* AndroidHardwareBufferExternalApi::getInstance()
{
#if (DE_OS == DE_OS_ANDROID)
	deInt32 sdkVersion = checkAnbApiBuild();
#if defined(BUILT_WITH_ANDROID_HARDWARE_BUFFER)
#if defined(__ANDROID_API_U__) && (DE_ANDROID_API >= __ANDROID_API_U__)
	if (sdkVersion >= __ANDROID_API_U__ )
	{
		static AndroidHardwareBufferExternalApi34 api34Instance;
		return &api34Instance;
	}
#endif // defined(__ANDROID_API_U__) && (DE_ANDROID_API >= __ANDROID_API_U__)

#if defined(__ANDROID_API_T__) && (DE_ANDROID_API >= __ANDROID_API_T__)
	if (sdkVersion >= __ANDROID_API_T__ )
	{
		static AndroidHardwareBufferExternalApi33 api33Instance;
		return &api33Instance;
	}
#endif // defined(__ANDROID_API_T__) && (DE_ANDROID_API >= __ANDROID_API_T__)

#if defined(__ANDROID_API_P__) && (DE_ANDROID_API >= __ANDROID_API_P__)
	if (sdkVersion >= __ANDROID_API_P__ )
	{
		static AndroidHardwareBufferExternalApi28 api28Instance;
		return &api28Instance;
	}
#endif // defined(__ANDROID_API_P__) && (DE_ANDROID_API >= __ANDROID_API_P__)

#if defined(__ANDROID_API_O__) && (DE_ANDROID_API >= __ANDROID_API_O__)
	if (sdkVersion >= __ANDROID_API_O__ )
	{
		static AndroidHardwareBufferExternalApi26 api26Instance;
		return &api26Instance;
	}
#endif // defined(__ANDROID_API_O__) && (DE_ANDROID_API >= __ANDROID_API_O__)

#endif // defined(BUILT_WITH_ANDROID_HARDWARE_BUFFER)
	DE_UNREF(sdkVersion);
#endif // DE_OS == DE_OS_ANDROID
	return DE_NULL;
}

deInt32 AndroidHardwareBufferInstance::getSdkVersion()
{
#if (DE_OS == DE_OS_ANDROID)
	return androidGetSdkVersion();
#endif // (DE_OS == DE_OS_ANDROID)
	return 0u;
}

bool AndroidHardwareBufferInstance::isFormatSupported (Format format)
{
	switch (format)
	{
#if defined(BUILT_WITH_ANDROID_HARDWARE_BUFFER)
	case Format::R8G8B8A8_UNORM:
	case Format::R8G8B8X8_UNORM:
	case Format::R8G8B8_UNORM:
	case Format::R5G6B5_UNORM:
	case Format::R16G16B16A16_FLOAT:
	case Format::R10G10B10A2_UNORM:
	case Format::BLOB:

#if defined(BUILT_WITH_ANDROID_P_HARDWARE_BUFFER)
	case Format::D16_UNORM:
	case Format::D24_UNORM:
	case Format::D24_UNORM_S8_UINT:
	case Format::D32_FLOAT:
	case Format::D32_FLOAT_S8_UINT:
	case Format::S8_UINT:
#endif // defined(BUILT_WITH_ANDROID_P_HARDWARE_BUFFER)

#if defined(BUILT_WITH_ANDROID_T_HARDWARE_BUFFER)
	case Format::Y8Cb8Cr8_420:
	case Format::YCbCr_P010:
	case Format::R8_UNORM:
#endif // defined(BUILT_WITH_ANDROID_T_HARDWARE_BUFFER)

#if defined(BUILT_WITH_ANDROID_U_HARDWARE_BUFFER)
	case Format::R16_UINT:
	case Format::R16G16_UINT:
	case Format::R10G10B10A10_UNORM:
#endif // defined(BUILT_WITH_ANDROID_U_HARDWARE_BUFFER)

	case Format::B8G8R8A8_UNORM:
	case Format::YV12:
	case Format::Y8:
	case Format::Y16:
	case Format::RAW16:
	case Format::RAW10:
	case Format::RAW12:
	case Format::RAW_OPAQUE:
	case Format::IMPLEMENTATION_DEFINED:
	case Format::NV16:
	case Format::NV21:
	case Format::YUY2:
		return true;
#endif // defined(BUILT_WITH_ANDROID_HARDWARE_BUFFER)

	default:
		return false;
	}
}

bool AndroidHardwareBufferInstance::isFormatYuv (Format format)
{
	switch (format)
	{
	case Format::Y8Cb8Cr8_420:
	case Format::YCbCr_P010:
	case Format::YV12:
	case Format::Y8:
	case Format::Y16:
	case Format::NV16:
	case Format::NV21:
	case Format::YUY2:
		return true;
	default:
		return false;
	}
}

bool AndroidHardwareBufferInstance::isFormatRaw (Format format)
{
	switch (format)
	{
	case Format::RAW10:
	case Format::RAW12:
	case Format::RAW16:
	case Format::RAW_OPAQUE:
		return true;
	default:
		return false;
	}
}

bool AndroidHardwareBufferInstance::isFormatColor (Format format)
{
	switch (format)
	{
	case Format::R8G8B8A8_UNORM:
	case Format::R8G8B8X8_UNORM:
	case Format::R8G8B8_UNORM:
	case Format::R5G6B5_UNORM:
	case Format::R16G16B16A16_FLOAT:
	case Format::R10G10B10A2_UNORM:
	case Format::Y8Cb8Cr8_420:
	case Format::YCbCr_P010:
	case Format::R8_UNORM:
	case Format::R16_UINT:
	case Format::R16G16_UINT:
	case Format::R10G10B10A10_UNORM:
	case Format::B8G8R8A8_UNORM:
	case Format::YV12:
	case Format::Y8:
	case Format::Y16:
	case Format::IMPLEMENTATION_DEFINED:
	case Format::NV16:
	case Format::NV21:
	case Format::YUY2:
		return true;
	default:
		return false;
	}
}

bool AndroidHardwareBufferInstance::isFormatDepth (Format format)
{
	switch (format)
	{
	case Format::D16_UNORM:
	case Format::D24_UNORM:
	case Format::D24_UNORM_S8_UINT:
	case Format::D32_FLOAT:
	case Format::D32_FLOAT_S8_UINT:
		return true;
	default:
		return false;
	}
}

bool AndroidHardwareBufferInstance::isFormatStencil (Format format)
{
	switch (format)
	{
	case Format::D24_UNORM_S8_UINT:
	case Format::D32_FLOAT_S8_UINT:
	case Format::S8_UINT:
		return true;
	default:
		return false;
	}
}

const char* AndroidHardwareBufferInstance::getFormatName (Format format)
{
	switch (format)
	{
	case Format::R8G8B8A8_UNORM:
		return "AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM";
	case Format::R8G8B8X8_UNORM:
		return "AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM";
	case Format::R8G8B8_UNORM:
		return "AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM";
	case Format::R5G6B5_UNORM:
		return "AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM";
	case Format::R16G16B16A16_FLOAT:
		return "AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT";
	case Format::R10G10B10A2_UNORM:
		return "AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM";
	case Format::BLOB:
		return "AHARDWAREBUFFER_FORMAT_BLOB";
	case Format::D16_UNORM:
		return "AHARDWAREBUFFER_FORMAT_D16_UNORM";
	case Format::D24_UNORM:
		return "AHARDWAREBUFFER_FORMAT_D24_UNORM";
	case Format::D24_UNORM_S8_UINT:
		return "AHARDWAREBUFFER_FORMAT_D24_UNORM_S8_UINT";
	case Format::D32_FLOAT:
		return "AHARDWAREBUFFER_FORMAT_D32_FLOAT";
	case Format::D32_FLOAT_S8_UINT:
		return "AHARDWAREBUFFER_FORMAT_D32_FLOAT_S8_UINT";
	case Format::S8_UINT:
		return "AHARDWAREBUFFER_FORMAT_S8_UINT";
	case Format::Y8Cb8Cr8_420:
		return "AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420";
	case Format::YCbCr_P010:
		return "AHARDWAREBUFFER_FORMAT_YCbCr_P010";
	case Format::R8_UNORM:
		return "AHARDWAREBUFFER_FORMAT_R8_UNORM";
	case Format::R16_UINT:
		return "AHARDWAREBUFFER_FORMAT_R16_UINT";
	case Format::R16G16_UINT:
		return "AHARDWAREBUFFER_FORMAT_R16G16_UINT";
	case Format::R10G10B10A10_UNORM:
		return "AHARDWAREBUFFER_FORMAT_R10G10B10A10_UNORM";
	case Format::B8G8R8A8_UNORM:
		return "AHARDWAREBUFFER_FORMAT_B8G8R8A8_UNORM";
	case Format::YV12:
		return "AHARDWAREBUFFER_FORMAT_YV12";
	case Format::Y8:
		return "AHARDWAREBUFFER_FORMAT_Y8";
	case Format::Y16:
		return "AHARDWAREBUFFER_FORMAT_Y16";
	case Format::RAW16:
		return "AHARDWAREBUFFER_FORMAT_RAW16";
	case Format::RAW10:
		return "AHARDWAREBUFFER_FORMAT_RAW10";
	case Format::RAW12:
		return "AHARDWAREBUFFER_FORMAT_RAW12";
	case Format::RAW_OPAQUE:
		return "AHARDWAREBUFFER_FORMAT_RAW_OPAQUE";
	case Format::IMPLEMENTATION_DEFINED:
		return "AHARDWAREBUFFER_FORMAT_IMPLEMENTATION_DEFINED";
	case Format::NV16:
		return "AHARDWAREBUFFER_FORMAT_YCbCr_422_SP";
	case Format::NV21:
		return "AHARDWAREBUFFER_FORMAT_YCrCb_420_SP";
	case Format::YUY2:
		return "AHARDWAREBUFFER_FORMAT_YCbCr_422_I";

	default:
		return "Unknown";
	}
}

deUint32 AndroidHardwareBufferInstance::formatToInternalFormat (Format format)
{
	switch (format)
	{
#if defined(BUILT_WITH_ANDROID_HARDWARE_BUFFER)
	case Format::R8G8B8A8_UNORM:
		return AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
	case Format::R8G8B8X8_UNORM:
		return AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM;
	case Format::R8G8B8_UNORM:
		return AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM;
	case Format::R5G6B5_UNORM:
		return AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM;
	case Format::R16G16B16A16_FLOAT:
		return AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT;
	case Format::R10G10B10A2_UNORM:
		return AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM;
	case Format::BLOB:
		return AHARDWAREBUFFER_FORMAT_BLOB;

#if defined(BUILT_WITH_ANDROID_P_HARDWARE_BUFFER)
	case Format::D16_UNORM:
		return AHARDWAREBUFFER_FORMAT_D16_UNORM;
	case Format::D24_UNORM:
		return AHARDWAREBUFFER_FORMAT_D24_UNORM;
	case Format::D24_UNORM_S8_UINT:
		return AHARDWAREBUFFER_FORMAT_D24_UNORM_S8_UINT;
	case Format::D32_FLOAT:
		return AHARDWAREBUFFER_FORMAT_D32_FLOAT;
	case Format::D32_FLOAT_S8_UINT:
		return AHARDWAREBUFFER_FORMAT_D32_FLOAT_S8_UINT;
	case Format::S8_UINT:
		return AHARDWAREBUFFER_FORMAT_S8_UINT;
#endif // defined(BUILT_WITH_ANDROID_P_HARDWARE_BUFFER)

#if defined(BUILT_WITH_ANDROID_T_HARDWARE_BUFFER)
	case Format::Y8Cb8Cr8_420:
		return AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420;
	case Format::YCbCr_P010:
		return AHARDWAREBUFFER_FORMAT_YCbCr_P010;
	case Format::R8_UNORM:
		return AHARDWAREBUFFER_FORMAT_R8_UNORM;
#endif // defined(BUILT_WITH_ANDROID_T_HARDWARE_BUFFER)

#if defined(BUILT_WITH_ANDROID_U_HARDWARE_BUFFER)
	case Format::R16_UINT:
		return AHARDWAREBUFFER_FORMAT_R16_UINT;
	case Format::R16G16_UINT:
		return AHARDWAREBUFFER_FORMAT_R16G16_UINT;
	case Format::R10G10B10A10_UNORM:
		return AHARDWAREBUFFER_FORMAT_R10G10B10A10_UNORM;
#endif // defined(BUILT_WITH_ANDROID_U_HARDWARE_BUFFER)

	// Values obtained AOSP header (nativewindow/include/vndk/hardware_buffer.h)
	case B8G8R8A8_UNORM:
		return 5;
	case YV12:
		return 0x32315659;
	case Y8:
		return 0x20203859;
	case Y16:
		return 0x20363159;
	case RAW16:
		return 0x20;
	case RAW10:
		return 0x25;
	case RAW12:
		return 0x26;
	case RAW_OPAQUE:
		return 0x24;
	case IMPLEMENTATION_DEFINED:
		return 0x22;
	case NV16:
		return 0x10;
	case NV21:
		return 0x11;
	case YUY2:
		return 0x14;
#endif // defined(BUILT_WITH_ANDROID_HARDWARE_BUFFER)

	default:
		return 0u;
	}
}

tcu::TextureFormat AndroidHardwareBufferInstance::formatToTextureFormat(Format format)
{
	switch (format)
	{
	case Format::R8G8B8A8_UNORM:
		return tcu::TextureFormat(tcu::TextureFormat::ChannelOrder::RGBA, tcu::TextureFormat::ChannelType::UNORM_INT8);
	case Format::R8G8B8X8_UNORM:
		return tcu::TextureFormat(tcu::TextureFormat::ChannelOrder::RGBA, tcu::TextureFormat::ChannelType::UNORM_INT8);
	case Format::R8G8B8_UNORM:
		return tcu::TextureFormat(tcu::TextureFormat::ChannelOrder::RGB, tcu::TextureFormat::ChannelType::UNORM_INT8);
	case Format::R5G6B5_UNORM:
		return tcu::TextureFormat(tcu::TextureFormat::ChannelOrder::RGB, tcu::TextureFormat::ChannelType::UNORM_SHORT_565);
	case Format::R16G16B16A16_FLOAT:
		return tcu::TextureFormat(tcu::TextureFormat::ChannelOrder::RGBA, tcu::TextureFormat::ChannelType::HALF_FLOAT);
	case Format::R10G10B10A2_UNORM:
		return tcu::TextureFormat(tcu::TextureFormat::ChannelOrder::RGBA, tcu::TextureFormat::ChannelType::UNORM_INT_1010102_REV);
	case Format::D16_UNORM:
		return tcu::TextureFormat(tcu::TextureFormat::ChannelOrder::D, tcu::TextureFormat::ChannelType::UNORM_INT16);
	case Format::D24_UNORM:
		return tcu::TextureFormat(tcu::TextureFormat::ChannelOrder::D, tcu::TextureFormat::ChannelType::UNORM_INT24);
	case Format::D24_UNORM_S8_UINT:
		return tcu::TextureFormat(tcu::TextureFormat::ChannelOrder::DS, tcu::TextureFormat::ChannelType::UNSIGNED_INT_24_8_REV);
	case Format::D32_FLOAT:
		return tcu::TextureFormat(tcu::TextureFormat::ChannelOrder::D, tcu::TextureFormat::ChannelType::FLOAT);
	case Format::S8_UINT:
		return tcu::TextureFormat(tcu::TextureFormat::ChannelOrder::S, tcu::TextureFormat::ChannelType::UNSIGNED_INT8);
	case Format::Y8Cb8Cr8_420:
	case Format::YV12:
	case Format::NV16:
	case Format::NV21:
	case Format::YUY2:
		return tcu::TextureFormat(tcu::TextureFormat::ChannelOrder::RGB, tcu::TextureFormat::ChannelType::UNORM_INT8);
	case Format::YCbCr_P010:
		return tcu::TextureFormat(tcu::TextureFormat::ChannelOrder::RGB, tcu::TextureFormat::ChannelType::UNORM_INT_101010);
	case Format::R8_UNORM:
	case Format::Y8:
		return tcu::TextureFormat(tcu::TextureFormat::ChannelOrder::R, tcu::TextureFormat::ChannelType::UNORM_INT8);
	case Format::Y16:
		return tcu::TextureFormat(tcu::TextureFormat::ChannelOrder::R, tcu::TextureFormat::ChannelType::UNORM_INT16);
	case Format::RAW16:
	case Format::R16_UINT:
		return tcu::TextureFormat(tcu::TextureFormat::ChannelOrder::R, tcu::TextureFormat::ChannelType::UNSIGNED_INT16);
	case Format::R16G16_UINT:
		return tcu::TextureFormat(tcu::TextureFormat::ChannelOrder::RG, tcu::TextureFormat::ChannelType::UNSIGNED_INT16);
	case Format::R10G10B10A10_UNORM:
		return tcu::TextureFormat(tcu::TextureFormat::ChannelOrder::RGBA, tcu::TextureFormat::ChannelType::UNORM_SHORT_10);

	case Format::B8G8R8A8_UNORM:
		return tcu::TextureFormat(tcu::TextureFormat::ChannelOrder::BGRA, tcu::TextureFormat::ChannelType::UNORM_INT8);
	case Format::RAW10:
		return tcu::getUncompressedFormat(tcu::COMPRESSEDTEXFORMAT_AHB_RAW10);
	case Format::RAW12:
		return tcu::getUncompressedFormat(tcu::COMPRESSEDTEXFORMAT_AHB_RAW12);

	// Format::IMPLEMENTATION_DEFINED
	// Format::RAW_OPAQUE
	// Format::D32_FLOAT_S8_UINT
	// Format::BLOB
	default:
		return tcu::TextureFormat(); // Unassigned
	}
}

deUint64 AndroidHardwareBufferInstance::usageToInternalUsage (Usage usage)
{
	deUint64	internalUsage	= 0u;

#if defined(BUILT_WITH_ANDROID_HARDWARE_BUFFER)

	// AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT used instead of AHARDWAREBUFFER_USAGE_GPU_FRAMEBUFFER
	// due to the latter requiring higher SDK versions than the former
	if (usage & Usage::GPU_FRAMEBUFFER)
		internalUsage |= AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT;

	if (usage & Usage::GPU_SAMPLED)
		internalUsage |= AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;

	if (usage & Usage::CPU_READ)
		internalUsage |= AHARDWAREBUFFER_USAGE_CPU_READ_RARELY;

	if (usage & Usage::CPU_WRITE)
		internalUsage |= AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY;
#else
	DE_UNREF(usage);
#endif // defined(BUILT_WITH_ANDROID_HARDWARE_BUFFER)

	return internalUsage;
}

deUint32 AndroidHardwareBufferInstance::pixelStride (Format format)
{
	return tcu::getPixelSize(formatToTextureFormat(format));
}

AndroidHardwareBufferInstance::~AndroidHardwareBufferInstance (void)
{
	release();
}

bool AndroidHardwareBufferInstance::allocate (Format format, deUint32 width, deUint32 height, deUint32 layers, Usage usage)
{
	// Should never be called when there's no actual api to call with
	DE_ASSERT(m_ahbApi);

	// Don't allocate another buffer before releasing previous
	DE_ASSERT(m_handle.internal == nullptr);

	m_format			= format;
	m_internalFormat	= formatToInternalFormat(m_format);
	m_usage				= usage;
	m_internalUsage		= usageToInternalUsage(m_usage);
	m_width				= width;
	m_height			= height;
	m_layers			= layers;

	m_handle	= m_ahbApi->allocate(width, height, layers, m_internalFormat, m_internalUsage);

	return m_handle.internal != nullptr;
}

void AndroidHardwareBufferInstance::release (void)
{
	if (m_handle.internal != nullptr)
	{
		// Should never be called when there's no actual api to call with
		DE_ASSERT(m_ahbApi);

		m_ahbApi->release(m_handle);
		m_handle.internal = nullptr;
	}
}

bool AndroidHardwareBufferInstance::lock (Usage usage)
{
	// Should never be called when there's no actual api to call with
	DE_ASSERT(m_ahbApi);

	// Only allowed to lock once
	DE_ASSERT(m_accessData.m_planeCount == 0);

	// Cannot lock what we don't have
	if (m_handle.internal == nullptr)
		return false;

	// Validate buffer was allocated with wanted usage
	if ((usage & m_usage) != usage)
		return false;

	bool		lockSuccessful	= false;
	deUint64	internalUsage	= usageToInternalUsage(usage);

	if (isYuv())
		lockSuccessful	= m_ahbApi->lockPlanes(m_handle, internalUsage, m_accessData.m_planeCount, m_accessData.m_planeDataVoid, m_accessData.m_planePixelStride, m_accessData.m_planeRowStride);
	else
	{
		m_accessData.m_planeDataVoid[0]	= m_ahbApi->lock(m_handle, internalUsage);

		// null data means locking failed
		if (m_accessData.m_planeDataVoid[0])
		{
			lockSuccessful	= true;

			m_accessData.m_planePixelStride[0]	= pixelStride(m_format);
			// Need to retrieve row stride from description (will only return how many pixels). Need to multiply by pixel size
			m_ahbApi->describe(m_handle, nullptr, nullptr, nullptr, nullptr, nullptr, &m_accessData.m_planeRowStride[0]);
			m_accessData.m_planeRowStride[0] *= m_accessData.m_planePixelStride[0];
			m_accessData.m_planeCount = 1u; // Non planar formats will be treated as a single plane
		}
	}

	return lockSuccessful;
}

bool AndroidHardwareBufferInstance::unlock (void)
{
	// Should never be called when there's no actual api to call with
	DE_ASSERT(m_ahbApi);

	// Force locking again if we want to read/write again
	m_accessData.m_planeCount	= 0u;

	return m_ahbApi->unlock(m_handle);
}

void AndroidHardwareBufferInstance::copyCpuBufferToAndroidBuffer(const tcu::TextureLevel& cpuBuffer)
{
	// Running into this assertion means no locking was performed
	DE_ASSERT(m_accessData.m_planeCount != 0);

	tcu::ConstPixelBufferAccess	access	= cpuBuffer.getAccess();

	for (deUint32 y = 0u; y < m_height; ++y)
	{
		for (deUint32 x = 0u; x < m_width; ++x)
		{
			const deUint8*	cpuBufferPixel		= static_cast<const deUint8*>(access.getPixelPtr(x, y));
			deUint32		offset				= (y * m_accessData.m_planeRowStride[0]) + (x * m_accessData.m_planePixelStride[0]);
			deUint8*		androidBufferPixel	= m_accessData.m_planeData[0] + offset;

			switch (m_format)
			{
			// YUV 4:2:0 formats
			case Y8Cb8Cr8_420:
			case YV12:
			case NV21:
				DE_ASSERT(m_accessData.m_planeCount == 3u);

				// Component size is UNSIGNED_INT8. We can copy values directly
				// YUV need to map RGB the following way according to Vulkan spec:
				// G is Y
				// B is Cb
				// R is Cr

				// Plane 0 contains Y information
				*(androidBufferPixel)	= *(cpuBufferPixel + 1);

				// Plane 1 contains Cb information
				// 4:2:0 has half size for Cb
				offset					= ((y / 2) * m_accessData.m_planeRowStride[1]) + ((x / 2) * m_accessData.m_planePixelStride[1]);
				androidBufferPixel		= m_accessData.m_planeData[1] + offset;
				*(androidBufferPixel)	= *(cpuBufferPixel + 2);

				// Plane 2 contains Cr information
				// 4:2:0 has half size for Cr
				offset					= ((y / 2) * m_accessData.m_planeRowStride[2]) + ((x / 2) * m_accessData.m_planePixelStride[2]);
				androidBufferPixel		= m_accessData.m_planeData[2] + offset;
				*(androidBufferPixel)	= *(cpuBufferPixel);

				break;

			// YUV 4:2:2 formats
			case NV16:
			case YUY2:
				DE_ASSERT(m_accessData.m_planeCount == 3u);

				// Component size is UNSIGNED_INT8. We can copy values directly
				// YUV need to map RGB the following way according to Vulkan spec:
				// G is Y
				// B is Cb
				// R is Cr

				// Plane 0 contains Y information
				*(androidBufferPixel)	= *(cpuBufferPixel + 1);

				// Plane 1 contains Cb information
				// 4:2:2 has half width for Cb
				offset					= (y * m_accessData.m_planeRowStride[1]) + ((x / 2) * m_accessData.m_planePixelStride[1]);
				androidBufferPixel		= m_accessData.m_planeData[1] + offset;
				*(androidBufferPixel)	= *(cpuBufferPixel + 2);

				// Plane 2 contains Cr information
				// 4:2:2 has half width for Cr
				offset					= (y * m_accessData.m_planeRowStride[2]) + ((x / 2) * m_accessData.m_planePixelStride[2]);
				androidBufferPixel		= m_accessData.m_planeData[2] + offset;
				*(androidBufferPixel)	= *(cpuBufferPixel);

				break;

			// YUV 4:2:0 format
			case YCbCr_P010:
			{
				DE_ASSERT(m_accessData.m_planeCount == 3u);

				// YCbCr_P010 does not have same comoponent size as m_cpuBuffer type (UNORM_INT_101010)
				// We need to transform from UNORM_INT_101010 to YCbCr_P010 where data is stored in the higher bits of 16 bit values

				deUint32	redOffset	= 22u;
				deUint32	greenOffset	= 12u;
				deUint32	blueOffset	= 2u;
				deUint32	bitOffset	= 6u;
				deUint32	bitCount	= 10u;
				deUint32	mask		= (1u << bitCount) - 1u;
				deUint32	cpuValue	= *(reinterpret_cast<const deUint32*>(cpuBufferPixel));
				deUint16	red			= static_cast<deUint16>(((cpuValue >> redOffset) & mask) << bitOffset);
				deUint16	green		= static_cast<deUint16>(((cpuValue >> greenOffset) & mask) << bitOffset);
				deUint16	blue		= static_cast<deUint16>(((cpuValue >> blueOffset) & mask) << bitOffset);

				// m_cpuBuffer for YCbCr_P010 has order RGB, so we need to swizzle values
				// Following Vulkan spec for ordering where:
				// G is Y
				// B is Cb
				// R is Cr

				// Plane 0 contains Y information
				*(reinterpret_cast<deUint16*>(androidBufferPixel))	= green;

				// Plane 1 contains Cb information
				// YCbCr_P010 is half size for Cb
				offset												= ((y / 2) * m_accessData.m_planeRowStride[1]) + ((x / 2) * m_accessData.m_planePixelStride[1]);
				androidBufferPixel									= m_accessData.m_planeData[1] + offset;
				*(reinterpret_cast<deUint16*>(androidBufferPixel))	= blue;

				// Plane 2 contains Cr information
				// YCbCr_P010 is half size for Cr
				offset												= ((y / 2) * m_accessData.m_planeRowStride[2]) + ((x / 2) * m_accessData.m_planePixelStride[2]);
				androidBufferPixel									= m_accessData.m_planeData[2] + offset;
				*(reinterpret_cast<deUint16*>(androidBufferPixel))	= red;

				break;
			}

			case RAW10:
			case RAW12:
				DE_ASSERT(false); // Use compressed variation
				break;

			default:
				memcpy(androidBufferPixel, cpuBufferPixel, m_accessData.m_planePixelStride[0]);
			}
		}
	}
}

void AndroidHardwareBufferInstance::copyCpuBufferToAndroidBufferCompressed(const tcu::CompressedTexture& cpuBuffer)
{
	// Running into this assertion means no locking was performed
	DE_ASSERT(m_accessData.m_planeCount != 0);

	const deUint8* cpuBufferPixel	= static_cast<const deUint8*>(cpuBuffer.getData());

	for (deUint32 y = 0u; y < m_height; ++y)
	{
		for (deUint32 x = 0u; x < m_width; ++x)
		{
			deUint32		offset				= (y * m_accessData.m_planeRowStride[0]) + (x * m_accessData.m_planePixelStride[0]);
			deUint8*		androidBufferPixel	= m_accessData.m_planeData[0] + offset;

			switch (m_format)
			{
			case RAW10:
			{
				DE_ASSERT(m_accessData.m_planeCount == 1u);

				// Packed format with 4 pixels in 5 bytes
				// Layout: https://developer.android.com/reference/android/graphics/ImageFormat#RAW10
				memcpy(androidBufferPixel, cpuBufferPixel, 5u); // Copy 4 pixels packed in 5 bytes

				// Advance 3 pixels so in total we advance 4
				x	+= 3u;
				cpuBufferPixel += 5u;

				break;
			}

			case RAW12:
			{
				DE_ASSERT(m_accessData.m_planeCount == 1u);

				// Packed format with 2 pixels in 3 bytes
				// Layout: https://developer.android.com/reference/android/graphics/ImageFormat#RAW12
				memcpy(androidBufferPixel, cpuBufferPixel, 3u); // Copy 2 pixels packed in 3 bytes

				// Advance 1 pixels so in total we advance 2
				x	+= 1u;
				cpuBufferPixel += 3u;

				break;
			}

			default:
				DE_ASSERT(false); // Use non-compressed variant
				break;
			}
		}
	}
}

void AndroidHardwareBufferInstance::copyAndroidBufferToCpuBuffer(tcu::TextureLevel& cpuBuffer) const
{
	// Running into this assertion means no locking was performed
	DE_ASSERT(m_accessData.m_planeCount != 0);

	tcu::PixelBufferAccess	access	= cpuBuffer.getAccess();

	for (deUint32 y = 0u; y < m_height; ++y)
	{
		for (deUint32 x = 0u; x < m_width; ++x)
		{
			deUint32		offset				= (y * m_accessData.m_planeRowStride[0]) + (x * m_accessData.m_planePixelStride[0]);
			deUint8*		cpuBufferPixel		= static_cast<deUint8*>(access.getPixelPtr(x, y));
			const deUint8*	androidBufferPixel	= m_accessData.m_planeData[0] + offset;

			switch (m_format)
			{
			// YUV 4:2:0 formats
			case Y8Cb8Cr8_420:
			case YV12:
			case NV21:
				DE_ASSERT(m_accessData.m_planeCount == 3u);

				// Component size is UNSIGNED_INT8. We can copy values directly
				// YUV need to map RGB the following way according to Vulkan spec:
				// Y is G
				// Cb is B
				// Cr is R

				// Plane 0 contains Y information
				*(cpuBufferPixel + 1)	= *(androidBufferPixel);

				// Plane 1 contains Cb information
				// 4:2:0 has half size for Cb
				offset					= ((y / 2) * m_accessData.m_planeRowStride[1]) + ((x / 2) * m_accessData.m_planePixelStride[1]);
				androidBufferPixel		= m_accessData.m_planeData[1] + offset;
				*(cpuBufferPixel + 2)	= *(androidBufferPixel);

				// Plane 2 contains Cr information
				// 4:2:0 has half size for Cr
				offset					= ((y / 2) * m_accessData.m_planeRowStride[2]) + ((x / 2) * m_accessData.m_planePixelStride[2]);
				androidBufferPixel		= m_accessData.m_planeData[2] + offset;
				*(cpuBufferPixel)		= *(androidBufferPixel);

				break;

			// YUV 4:2:2 formats
			case NV16:
			case YUY2:
				DE_ASSERT(m_accessData.m_planeCount == 3u);

				// Component size is UNSIGNED_INT8. We can copy values directly
				// YUV need to map RGB the following way according to Vulkan spec:
				// Y is G
				// Cb is B
				// Cr is R

				// Plane 0 contains Y information
				*(cpuBufferPixel + 1)	= *(androidBufferPixel);

				// Plane 1 contains Cb information
				// 4:2:2 has half width for Cb
				offset					= (y * m_accessData.m_planeRowStride[1]) + ((x / 2) * m_accessData.m_planePixelStride[1]);
				androidBufferPixel		= m_accessData.m_planeData[1] + offset;
				*(cpuBufferPixel + 2)	= *(androidBufferPixel);

				// Plane 2 contains Cr information
				// 4:2:2 has half width for Cr
				offset					= (y * m_accessData.m_planeRowStride[2]) + ((x / 2) * m_accessData.m_planePixelStride[2]);
				androidBufferPixel		= m_accessData.m_planeData[2] + offset;
				*(cpuBufferPixel)		= *(androidBufferPixel);

				break;

			// YUV 4:2:0 format
			case YCbCr_P010:
			{
				DE_ASSERT(m_accessData.m_planeCount == 3u);

				// m_cpuBuffer for YCbCr_P010 has order RGB, so we need to swizzle values
				// Following Vulkan spec for ordering where:
				// Y is G
				// Cb is B
				// Cr is R

				// Plane 0 contains Y information
				deUint16	green		= *(reinterpret_cast<const deUint16*>(androidBufferPixel));

				// Plane 1 contains Cb information
				// YCbCr_P010 is half size for Cb
				offset					= ((y / 2) * m_accessData.m_planeRowStride[1]) + ((x / 2) * m_accessData.m_planePixelStride[1]);
				androidBufferPixel		= m_accessData.m_planeData[1] + offset;
				deUint16	blue		= *(reinterpret_cast<const deUint16*>(androidBufferPixel));

				// Plane 2 contains Cr information
				// YCbCr_P010 is half size for Cr
				offset					= ((y / 2) * m_accessData.m_planeRowStride[2]) + ((x / 2) * m_accessData.m_planePixelStride[2]);
				androidBufferPixel		= m_accessData.m_planeData[2] + offset;
				deUint16	red			= *(reinterpret_cast<const deUint16*>(androidBufferPixel));

				// YCbCr_P010 does not have same comoponent size as m_cpuBuffer type (UNORM_INT_101010)
				// We need to transform from YCbCr_P010 where data is stored in the higher bits of 16 bit values to UNORM_INT_101010

				deUint32	redOffset	= 22u;
				deUint32	greenOffset	= 12u;
				deUint32	blueOffset	= 2u;
				deUint32	bitOffset	= 6u;
				deUint32	finalValue	= 0u;

				finalValue	|= static_cast<deUint32>(blue >> bitOffset) << blueOffset;
				finalValue	|= static_cast<deUint32>(green >> bitOffset) << greenOffset;
				finalValue	|= static_cast<deUint32>(red >> bitOffset) << redOffset;

				*(reinterpret_cast<deUint32*>(cpuBufferPixel))	= finalValue;


				break;
			}

			case RAW10:
			case RAW12:
				DE_ASSERT(false); // Use compressed variation
				break;

			default:
				memcpy(cpuBufferPixel, androidBufferPixel, m_accessData.m_planePixelStride[0]);
			}
		}
	}
}

void AndroidHardwareBufferInstance::copyAndroidBufferToCpuBufferCompressed(tcu::CompressedTexture& cpuBuffer) const
{
	// Running into this assertion means no locking was performed
	DE_ASSERT(m_accessData.m_planeCount != 0);

	deUint8*		cpuBufferPixel	= static_cast<deUint8*>(cpuBuffer.getData());

	for (deUint32 y = 0u; y < m_height; ++y)
	{
		for (deUint32 x = 0u; x < m_width; ++x)
		{
			deUint32		offset				= (y * m_accessData.m_planeRowStride[0]) + (x * m_accessData.m_planePixelStride[0]);
			const deUint8*	androidBufferPixel	= m_accessData.m_planeData[0] + offset;

			switch (m_format)
			{
			case RAW10:
			{
				DE_ASSERT(m_accessData.m_planeCount == 1u);

				// Packed format with 4 pixels in 5 bytes
				// Layout: https://developer.android.com/reference/android/graphics/ImageFormat#RAW10
				memcpy(cpuBufferPixel, androidBufferPixel, 5u); // Copy 4 pixels packed in 5 bytes

				// Advance 3 pixels so in total we advance 4
				x	+= 3u;
				cpuBufferPixel += 5u;

				break;
			}

			case RAW12:
			{
				DE_ASSERT(m_accessData.m_planeCount == 1u);

				// Packed format with 2 pixels in 3 bytes
				// Layout: https://developer.android.com/reference/android/graphics/ImageFormat#RAW12
				memcpy(cpuBufferPixel, androidBufferPixel, 3u); // Copy 2 pixels packed in 3 bytes

				// Advance 1 pixels so in total we advance 2
				x	+= 1u;
				cpuBufferPixel += 3u;

				break;
			}

			default:
				DE_ASSERT(false); // Use non-compressed variant
				break;
			}
		}
	}
}

bool AndroidHardwareBufferInstance::isYuv (void) const
{
	return isFormatYuv(m_format);
}

bool AndroidHardwareBufferInstance::isRaw (void) const
{
	return isFormatRaw(m_format);
}

bool AndroidHardwareBufferInstance::hasDepth (void) const
{
	switch (m_format)
	{
	case D16_UNORM:
	case D24_UNORM:
	case D24_UNORM_S8_UINT:
	case D32_FLOAT:
	case D32_FLOAT_S8_UINT:
		return true;
	default:
		return false;
	}
}

bool AndroidHardwareBufferInstance::hasStencil (void) const
{
	switch (m_format)
	{
	case D24_UNORM_S8_UINT:
	case D32_FLOAT_S8_UINT:
	case S8_UINT:
		return true;
	default:
		return false;
	}
}


} // ExternalMemoryUtil

} // vkt

#endif // CTS_USES_VULKANSC
