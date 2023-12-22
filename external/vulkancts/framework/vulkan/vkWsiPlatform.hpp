#ifndef _VKWSIPLATFORM_HPP
#define _VKWSIPLATFORM_HPP
/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2016 Google Inc.
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
 * \brief WSI Platform Abstraction.
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "tcuCommandLine.hpp"
#include "tcuVector.hpp"
#include "tcuMaybe.hpp"

namespace vk
{
namespace wsi
{

class Window
{
public:
	virtual				~Window			(void) {}

	virtual	void		setVisible		(bool visible);
	virtual void		setForeground	(void);
	virtual void		resize			(const tcu::UVec2& newSize);
	virtual	void		setMinimized	(bool minized);

protected:
						Window			(void) {}

private:
						Window			(const Window&); // Not allowed
	Window&				operator=		(const Window&); // Not allowed
};

class Display
{
public:
	virtual				~Display		(void) {}

	virtual Window*		createWindow	(const tcu::Maybe<tcu::UVec2>& initialSize = tcu::Nothing) const = 0;

protected:
						Display			(void) {}

private:
						Display			(const Display&); // Not allowed
	Display&			operator=		(const Display&); // Not allowed
};

// WSI implementation-specific APIs

template<int WsiType>
struct TypeTraits;
// {
//		typedef <NativeDisplayType>	NativeDisplayType;
//		typedef <NativeWindowType>	NativeWindowType;
// };

template<int WsiType>
struct DisplayInterface : public Display
{
public:
	typedef typename TypeTraits<WsiType>::NativeDisplayType	NativeType;

	NativeType			getNative			(void) const { return m_native; }

protected:
						DisplayInterface	(NativeType nativeDisplay)
							: m_native(nativeDisplay)
						{}

	const NativeType	m_native;
};

template<int WsiType>
struct WindowInterface : public Window
{
public:
	typedef typename TypeTraits<WsiType>::NativeWindowType	NativeType;

	NativeType			getNative			(void) const { return m_native; }

protected:
						WindowInterface	(NativeType nativeDisplay)
							: m_native(nativeDisplay)
						{}

	const NativeType	m_native;
};

// VK_KHR_xlib_surface

template<>
struct TypeTraits<TYPE_XLIB>
{
	typedef pt::XlibDisplayPtr			NativeDisplayType;
	typedef pt::XlibWindow				NativeWindowType;
};

typedef DisplayInterface<TYPE_XLIB>		XlibDisplayInterface;
typedef WindowInterface<TYPE_XLIB>		XlibWindowInterface;

// VK_KHR_xcb_surface

template<>
struct TypeTraits<TYPE_XCB>
{
	typedef pt::XcbConnectionPtr		NativeDisplayType;
	typedef pt::XcbWindow				NativeWindowType;
};

typedef DisplayInterface<TYPE_XCB>		XcbDisplayInterface;
typedef WindowInterface<TYPE_XCB>		XcbWindowInterface;

// VK_KHR_wayland_surface

template<>
struct TypeTraits<TYPE_WAYLAND>
{
	typedef pt::WaylandDisplayPtr		NativeDisplayType;
	typedef pt::WaylandSurfacePtr		NativeWindowType;
};

typedef DisplayInterface<TYPE_WAYLAND>	WaylandDisplayInterface;
typedef WindowInterface<TYPE_WAYLAND>	WaylandWindowInterface;

// VK_EXT_acquire_drm_display

template<>
struct TypeTraits<TYPE_DIRECT_DRM>
{
	typedef VkDisplayKHR NativeDisplayType;
};

struct DirectDrmDisplayInterface : public DisplayInterface<TYPE_DIRECT_DRM>
{
public:
					DirectDrmDisplayInterface	(void)
						: DisplayInterface(DE_NULL)
					{}
	virtual void	initializeDisplay			(const InstanceInterface& vki, VkInstance instance, const tcu::CommandLine& cmdLine)
	{
		DE_UNREF(vki);
		DE_UNREF(instance);
		DE_UNREF(cmdLine);
	}
};

// VK_KHR_mir_surface

// VK_KHR_android_surface

template<>
struct TypeTraits<TYPE_ANDROID>
{
	typedef pt::AndroidNativeWindowPtr	NativeWindowType;
};

typedef WindowInterface<TYPE_ANDROID>	AndroidWindowInterface;

// VK_KHR_win32_surface

template<>
struct TypeTraits<TYPE_WIN32>
{
	typedef pt::Win32InstanceHandle		NativeDisplayType;
	typedef pt::Win32WindowHandle		NativeWindowType;
};

typedef DisplayInterface<TYPE_WIN32>	Win32DisplayInterface;
typedef WindowInterface<TYPE_WIN32>		Win32WindowInterface;

// VK_MVK_macos_surface

template<>
struct TypeTraits<TYPE_MACOS>
{
	typedef void*						NativeWindowType;
};

typedef WindowInterface<TYPE_MACOS>		MacOSWindowInterface;

} // wsi
} // vk

#endif // _VKWSIPLATFORM_HPP
