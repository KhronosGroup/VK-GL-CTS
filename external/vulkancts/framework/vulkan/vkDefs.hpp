#ifndef _VKDEFS_HPP
#define _VKDEFS_HPP
/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2015 Google Inc.
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
 * \brief Vulkan utilites.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"

#if (DE_OS == DE_OS_ANDROID) && defined(__ARM_ARCH) && defined(__ARM_32BIT_STATE)
#	define VKAPI_ATTR __attribute__((pcs("aapcs-vfp")))
#else
#	define VKAPI_ATTR
#endif

#if (DE_OS == DE_OS_WIN32) && ((defined(_MSC_VER) && _MSC_VER >= 800) || defined(__MINGW32__) || defined(_STDCALL_SUPPORTED))
#	define VKAPI_CALL __stdcall
#else
#	define VKAPI_CALL
#endif

#define VK_DEFINE_HANDLE(NAME, TYPE)					typedef struct NAME##_s* NAME
#define VK_DEFINE_NON_DISPATCHABLE_HANDLE(NAME, TYPE)	typedef Handle<TYPE> NAME

#define VK_DEFINE_PLATFORM_TYPE(NAME, COMPATIBLE)		\
namespace pt {											\
struct NAME {											\
	COMPATIBLE internal;								\
	explicit NAME (COMPATIBLE internal_)				\
		: internal(internal_) {}						\
};														\
} // pt

#define VK_MAKE_API_VERSION(VARIANT, MAJOR, MINOR, PATCH)	\
												((((deUint32)(VARIANT)) << 29) | (((deUint32)(MAJOR)) << 22) | (((deUint32)(MINOR)) << 12) | ((deUint32)(PATCH)))
#define VK_MAKE_VERSION(MAJOR, MINOR, PATCH)	VK_MAKE_API_VERSION(0, MAJOR, MINOR, PATCH)
#define VK_BIT(NUM)								(1u<<(deUint32)(NUM))

#define VK_API_VERSION_VARIANT(version)			((deUint32)(version) >> 29)
#define VK_API_VERSION_MAJOR(version)			(((deUint32)(version) >> 22) & 0x7FU)
#define VK_API_VERSION_MINOR(version)			(((deUint32)(version) >> 12) & 0x3FFU)
#define VK_API_VERSION_PATCH(version)			((deUint32)(version) & 0xFFFU)

#define VK_CHECK(EXPR)							vk::checkResult((EXPR), #EXPR, __FILE__, __LINE__)
#define VK_CHECK_MSG(EXPR, MSG)					vk::checkResult((EXPR), MSG, __FILE__, __LINE__)
#define VK_CHECK_WSI(EXPR)						vk::checkWsiResult((EXPR), #EXPR, __FILE__, __LINE__)

/*--------------------------------------------------------------------*//*!
 * \brief Vulkan utilities
 *//*--------------------------------------------------------------------*/
namespace vk
{

typedef deUint64	VkDeviceSize;
typedef deUint32	VkSampleMask;
typedef deUint32	VkBool32;
typedef deUint32	VkFlags;
typedef deUint64	VkFlags64;
typedef deUint64	VkDeviceAddress;

// enum HandleType { HANDLE_TYPE_INSTANCE, ... };
#include "vkHandleType.inl"

template<HandleType Type>
class Handle
{
public:
				Handle		(void) {} // \note Left uninitialized on purpose
				Handle		(deUint64 internal) : m_internal(internal) {}

	Handle&		operator=	(deUint64 internal)					{ m_internal = internal; return *this;			}

	bool		operator==	(const Handle<Type>& other) const	{ return this->m_internal == other.m_internal;	}
	bool		operator!=	(const Handle<Type>& other) const	{ return this->m_internal != other.m_internal;	}

	bool		operator!	(void) const						{ return !m_internal;							}

	deUint64	getInternal	(void) const						{ return m_internal;							}

	enum { HANDLE_TYPE = Type };

private:
	deUint64	m_internal;
};

#include "vkBasicTypes.inl"

#define VK_CORE_FORMAT_LAST			((vk::VkFormat)(vk::VK_FORMAT_ASTC_12x12_SRGB_BLOCK+1))
#define VK_CORE_IMAGE_TILING_LAST	((vk::VkImageTiling)(vk::VK_IMAGE_TILING_LINEAR+1))
#define VK_CORE_IMAGE_TYPE_LAST		((vk::VkImageType)(vk::VK_IMAGE_TYPE_3D+1))

enum SpirvVersion
{
	SPIRV_VERSION_1_0	= 0,	//!< SPIR-V 1.0
	SPIRV_VERSION_1_1	= 1,	//!< SPIR-V 1.1
	SPIRV_VERSION_1_2	= 2,	//!< SPIR-V 1.2
	SPIRV_VERSION_1_3	= 3,	//!< SPIR-V 1.3
	SPIRV_VERSION_1_4	= 4,	//!< SPIR-V 1.4
	SPIRV_VERSION_1_5	= 5,	//!< SPIR-V 1.5
	SPIRV_VERSION_1_6	= 6,	//!< SPIR-V 1.6

	SPIRV_VERSION_LAST
};

typedef struct
{
	deUint32	magic;
	deUint32	version;
	deUint32	generator;
	deUint32	bound;
} SpirvBinaryHeader;

namespace wsi
{

enum Type
{
	TYPE_XLIB = 0,
	TYPE_XCB,
	TYPE_WAYLAND,
	TYPE_ANDROID,
	TYPE_WIN32,
	TYPE_MACOS,
	TYPE_HEADLESS,

	TYPE_LAST
};

} // wsi

typedef VKAPI_ATTR void		(VKAPI_CALL* PFN_vkVoidFunction)					(void);

typedef VKAPI_ATTR void*	(VKAPI_CALL* PFN_vkAllocationFunction)				(void*						pUserData,
																				 size_t						size,
																				 size_t						alignment,
																				 VkSystemAllocationScope	allocationScope);
typedef VKAPI_ATTR void*	(VKAPI_CALL* PFN_vkReallocationFunction)			(void*						pUserData,
																				 void*						pOriginal,
																				 size_t						size,
																				 size_t						alignment,
																				 VkSystemAllocationScope	allocationScope);
typedef VKAPI_ATTR void		(VKAPI_CALL* PFN_vkFreeFunction)					(void*						pUserData,
																				 void*						pMem);
typedef VKAPI_ATTR void		(VKAPI_CALL* PFN_vkInternalAllocationNotification)	(void*						pUserData,
																				 size_t						size,
																				 VkInternalAllocationType	allocationType,
																				 VkSystemAllocationScope	allocationScope);
typedef VKAPI_ATTR void		(VKAPI_CALL* PFN_vkInternalFreeNotification)		(void*						pUserData,
																				 size_t						size,
																				 VkInternalAllocationType	allocationType,
																				 VkSystemAllocationScope	allocationScope);

typedef VKAPI_ATTR VkBool32	(VKAPI_CALL* PFN_vkDebugReportCallbackEXT)			(VkDebugReportFlagsEXT		flags,
																				 VkDebugReportObjectTypeEXT	objectType,
																				 deUint64					object,
																				 size_t						location,
																				 deInt32					messageCode,
																				 const char*				pLayerPrefix,
																				 const char*				pMessage,
																				 void*						pUserData);

typedef VKAPI_ATTR VkBool32 (VKAPI_CALL *PFN_vkDebugUtilsMessengerCallbackEXT)	(VkDebugUtilsMessageSeverityFlagBitsEXT				messageSeverity,
																				 VkDebugUtilsMessageTypeFlagsEXT					messageTypes,
																				 const struct VkDebugUtilsMessengerCallbackDataEXT*	pCallbackData,
																				 void*												pUserData);
typedef VKAPI_ATTR void		(VKAPI_CALL* PFN_vkDeviceMemoryReportCallbackEXT)	(const struct VkDeviceMemoryReportCallbackDataEXT*	pCallbackData,
																				 void*												pUserData);

#include "vkStructTypes.inl"

typedef void* VkRemoteAddressNV;

extern "C"
{
#include "vkFunctionPointerTypes.inl"
}

class PlatformInterface
{
public:
#include "vkVirtualPlatformInterface.inl"

	virtual	GetInstanceProcAddrFunc	getGetInstanceProcAddr	() const = 0;

protected:
									PlatformInterface		(void) {}

private:
									PlatformInterface		(const PlatformInterface&);
	PlatformInterface&				operator=				(const PlatformInterface&);
};

class InstanceInterface
{
public:
#include "vkVirtualInstanceInterface.inl"

protected:
						InstanceInterface	(void) {}

private:
						InstanceInterface	(const InstanceInterface&);
	InstanceInterface&	operator=			(const InstanceInterface&);
};

class DeviceInterface
{
public:
#include "vkVirtualDeviceInterface.inl"

protected:
						DeviceInterface		(void) {}

private:
						DeviceInterface		(const DeviceInterface&);
	DeviceInterface&	operator=			(const DeviceInterface&);
};

class Error : public tcu::TestError
{
public:
					Error				(VkResult error, const char* message, const char* expr, const char* file, int line);
					Error				(VkResult error, const std::string& message);
	virtual			~Error				(void) throw();

	VkResult		getError			(void) const { return m_error; }

private:
	const VkResult	m_error;
};

class OutOfMemoryError : public tcu::ResourceError
{
public:
					OutOfMemoryError	(VkResult error, const char* message, const char* expr, const char* file, int line);
					OutOfMemoryError	(VkResult error, const std::string& message);
	virtual			~OutOfMemoryError	(void) throw();

	VkResult		getError			(void) const { return m_error; }

private:
	const VkResult	m_error;
};

void			checkResult			(VkResult result, const char* message, const char* file, int line);
void			checkWsiResult		(VkResult result, const char* message, const char* file, int line);

} // vk

#endif // _VKDEFS_HPP
