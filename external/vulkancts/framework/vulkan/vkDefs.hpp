#ifndef _VKDEFS_HPP
#define _VKDEFS_HPP
/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2015 Google Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be
 * included in all copies or substantial portions of the Materials.
 *
 * The Materials are Confidential Information as defined by the
 * Khronos Membership Agreement until designated non-confidential by
 * Khronos, at which point this condition clause shall be removed.
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
 * \brief Vulkan utilites.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"

#if (DE_OS == DE_OS_ANDROID) && defined(__ARM_ARCH_7A__)
#	define VKAPI_ATTR __attribute__((pcs("aapcs-vfp")))
#else
#	define VKAPI_ATTR
#endif

#if (DE_OS == DE_OS_WIN32) && ((_MSC_VER >= 800) || defined(_STDCALL_SUPPORTED))
#	define VKAPI_CALL __stdcall
#else
#	define VKAPI_CALL
#endif

#define VK_DEFINE_HANDLE(NAME, TYPE)					typedef struct NAME##_s* NAME
#define VK_DEFINE_NON_DISPATCHABLE_HANDLE(NAME, TYPE)	typedef Handle<TYPE> NAME

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
typedef deUint32	VkBool32;
typedef deUint32	VkFlags;

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

enum { VK_QUEUE_FAMILY_IGNORED		= 0xffffffff	};
enum { VK_NO_ATTACHMENT				= 0xffffffff	};

enum
{
	VK_FALSE		= 0,
	VK_TRUE			= 1,
	VK_WHOLE_SIZE	= (~0ULL),
};

typedef VKAPI_ATTR void		(VKAPI_CALL* PFN_vkVoidFunction)					(void);

typedef VKAPI_ATTR void*	(VKAPI_CALL* PFN_vkAllocationFunction)				(void* pUserData, size_t size, size_t alignment, VkSystemAllocationScope allocationScope);
typedef VKAPI_ATTR void*	(VKAPI_CALL* PFN_vkReallocationFunction)			(void* pUserData, void* pOriginal, size_t size, size_t alignment, VkSystemAllocationScope allocationScope);
typedef VKAPI_ATTR void		(VKAPI_CALL* PFN_vkFreeFunction)					(void* pUserData, void* pMem);
typedef VKAPI_ATTR void		(VKAPI_CALL* PFN_vkInternalAllocationNotification)	(void* pUserData, size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope);
typedef VKAPI_ATTR void		(VKAPI_CALL* PFN_vkInternalFreeNotification)		(void* pUserData, size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope);

#include "vkStructTypes.inl"

extern "C"
{
#include "vkFunctionPointerTypes.inl"
}

class PlatformInterface
{
public:
#include "vkVirtualPlatformInterface.inl"

protected:
						PlatformInterface	(void) {}

private:
						PlatformInterface	(const PlatformInterface&);
	PlatformInterface&	operator=			(const PlatformInterface&);
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

} // vk

#endif // _VKDEFS_HPP
