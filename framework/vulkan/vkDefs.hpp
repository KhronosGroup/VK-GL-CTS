#ifndef _VKDEFS_HPP
#define _VKDEFS_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Vulkan Utilities
 * -----------------------------------------------
 *
 * Copyright 2015 The Android Open Source Project
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

#if (DE_OS == DE_OS_ANDROID)
#	include <sys/cdefs.h>
#	if !defined(__NDK_FPABI__)
#		define __NDK_FPABI__
#	endif
#	define VK_APICALL __NDK_FPABI__
#else
#	define VK_APICALL
#endif

#if (DE_OS == DE_OS_WIN32) && (_MSC_VER >= 800) || defined(_STDCALL_SUPPORTED)
#	define VK_APIENTRY __stdcall
#else
#	define VK_APIENTRY
#endif

#define VK_DEFINE_HANDLE_TYPE_TRAITS(HANDLE)			\
	struct HANDLE##T { private: HANDLE##T (void); };	\
	template<>											\
	struct Traits<HANDLE##T>							\
	{													\
		typedef HANDLE		Type;						\
	}

#if (DE_PTR_SIZE == 8)
#	define VK_DEFINE_PTR_HANDLE(HANDLE)						\
		struct HANDLE##_s { private: HANDLE##_s (void); };	\
		typedef HANDLE##_s*	HANDLE

#	define VK_DEFINE_PTR_SUBCLASS_HANDLE(HANDLE, PARENT)						\
		struct HANDLE##_s : public PARENT##_s { private: HANDLE##_s (void); };	\
		typedef HANDLE##_s* HANDLE

#	define VK_DEFINE_BASE_HANDLE(HANDLE)						VK_DEFINE_PTR_HANDLE(HANDLE)
#	define VK_DEFINE_DISP_SUBCLASS_HANDLE(HANDLE, PARENT)		VK_DEFINE_PTR_SUBCLASS_HANDLE(HANDLE, PARENT)
#	define VK_DEFINE_NONDISP_SUBCLASS_HANDLE(HANDLE, PARENT)	VK_DEFINE_PTR_SUBCLASS_HANDLE(HANDLE, PARENT)
#else
#	define VK_DEFINE_BASE_HANDLE(HANDLE)						typedef deUint64 HANDLE
#	define VK_DEFINE_DISP_SUBCLASS_HANDLE(HANDLE, PARENT)		typedef deUintptr HANDLE
#	define VK_DEFINE_NONDISP_SUBCLASS_HANDLE(HANDLE, PARENT)	typedef deUint64 HANDLE
#endif

#define VK_MAKE_VERSION(MAJOR, MINOR, PATCH)	((MAJOR << 22) | (MINOR << 12) | PATCH)
#define VK_BIT(NUM)								(1<<NUM)

#define VK_CHECK(EXPR)							vk::checkResult((EXPR), #EXPR, __FILE__, __LINE__)
#define VK_CHECK_MSG(EXPR, MSG)					vk::checkResult((EXPR), MSG, __FILE__, __LINE__)

/*--------------------------------------------------------------------*//*!
 * \brief Vulkan utilities
 *//*--------------------------------------------------------------------*/
namespace vk
{

typedef deUint64	VkDeviceSize;
typedef deUint32	VkSampleMask;

typedef deUint32	VkShaderCreateFlags;		// Reserved
typedef deUint32	VkEventCreateFlags;			// Reserved
typedef deUint32	VkCmdBufferCreateFlags;		// Reserved
typedef deUint32	VkMemoryMapFlags;			// \todo [2015-05-08 pyry] Reserved? Not documented

template<typename T> struct Traits;

#include "vkBasicTypes.inl"

typedef VK_APICALL void		(VK_APIENTRY* FunctionPtr)			(void);

typedef VK_APICALL void*	(VK_APIENTRY* PFN_vkAllocFunction)	(void* pUserData, deUintptr size, deUintptr alignment, VkSystemAllocType allocType);
typedef VK_APICALL void		(VK_APIENTRY* PFN_vkFreeFunction)	(void* pUserData, void* pMem);

union VkClearColorValue
{
	float		floatColor[4];
	deUint32	rawColor[4];
};

#include "vkStructTypes.inl"

class PlatformInterface
{
public:
#include "vkVirtualPlatformInterface.inl"
};

class DeviceInterface
{
public:
#include "vkVirtualDeviceInterface.inl"
};

struct ApiVersion
{
	deUint32	major;
	deUint32	minor;
	deUint32	patch;

	ApiVersion (deUint32	major_,
				deUint32	minor_,
				deUint32	patch_)
		: major	(major_)
		, minor	(minor_)
		, patch	(patch_)
	{
	}
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

ApiVersion		unpackVersion	(deUint32 version);
deUint32		pack			(const ApiVersion& version);

void			checkResult		(VkResult result, const char* message, const char* file, int line);

//! Map Vk{Object}T to VK_OBJECT_TYPE_{OBJECT}. Defined for leaf objects only.
template<typename T>
VkObjectType	getObjectType	(void);

} // vk

#endif // _VKDEFS_HPP
