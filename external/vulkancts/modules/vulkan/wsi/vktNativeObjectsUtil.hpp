#ifndef _VKTNATIVEOBJECTSUTIL_HPP
#define _VKTNATIVEOBJECTSUTIL_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
 * Copyright (c) 2019 Valve Corporation.
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
 * \brief WSI Native Objects utility class.
 *//*--------------------------------------------------------------------*/
#include "vktTestCase.hpp"

#include "vkDefs.hpp"
#include "vkWsiPlatform.hpp"

#include "tcuMaybe.hpp"
#include "tcuVectorType.hpp"


namespace vkt
{
namespace wsi
{

class NativeObjects
{
public:
	using Extensions = std::vector<vk::VkExtensionProperties>;

												NativeObjects	(Context&						context,
																 const Extensions&				supportedExtensions,
																 vk::wsi::Type					wsiType,
																 size_t							windowCount = 1u,
																 const tcu::Maybe<tcu::UVec2>&	initialWindowSize = tcu::nothing<tcu::UVec2>());

												NativeObjects	(NativeObjects&& other);

	vk::wsi::Display&							getDisplay		() const;

	vk::wsi::Window&							getWindow		(size_t index = 0u) const;

	static de::MovePtr<vk::wsi::Window>			createWindow	(const vk::wsi::Display& display, const tcu::Maybe<tcu::UVec2>& initialSize);

	static de::MovePtr<vk::wsi::Display>		createDisplay	(const vk::Platform&	platform,
																 const Extensions&		supportedExtensions,
																 vk::wsi::Type			wsiType);
private:
	de::UniquePtr<vk::wsi::Display>				display;
	std::vector<de::MovePtr<vk::wsi::Window>>	windows;

};

} // wsi
} // vkt

#endif // _VKTNATIVEOBJECTSUTIL_HPP
