#ifndef _VKTPROTECTEDMEMIMAGEVALIDATOR_HPP
#define _VKTPROTECTEDMEMIMAGEVALIDATOR_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 The Khronos Group Inc.
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
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
 * \brief Protected Memory image validator helper
 *//*--------------------------------------------------------------------*/

#include "tcuVector.hpp"
#include "vkDefs.hpp"
#include "vktTestCase.hpp"

namespace vkt
{
namespace ProtectedMem
{

class ProtectedContext;

struct ValidationData {
	tcu::Vec4	coords[4];
	tcu::Vec4	values[4];
};

class ImageValidator
{
public:
							ImageValidator	(vk::VkFormat imageFormat = vk::VK_FORMAT_R8G8B8A8_UNORM)
								: m_imageFormat	(imageFormat)
							{}
							~ImageValidator	() {}
	void					initPrograms	(vk::SourceCollections&	programCollection) const;

	bool					validateImage	(ProtectedContext&		ctx,
											 const ValidationData&	refData,
											 const vk::VkImage		image,
											 const vk::VkFormat		imageFormat,
											 const vk::VkImageLayout imageLayout) const;

private:
	const vk::VkFormat		m_imageFormat;
};

} // ProtectedMem
} // vkt

#endif // _VKTPROTECTEDMEMIMAGEVALIDATOR_HPP
