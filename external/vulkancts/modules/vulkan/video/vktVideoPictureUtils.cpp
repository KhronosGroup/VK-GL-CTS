/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
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
 * \brief Video Encoding and Decoding Utility Functions
 *//*--------------------------------------------------------------------*/

#include "vktVideoPictureUtils.hpp"
#include "deMemory.h"

using namespace vk;
using namespace std;

namespace vkt
{
namespace video
{
VulkanPicture::VulkanPicture()
	: m_refCount ()
{
	Clear();
}

VulkanPicture::~VulkanPicture ()
{
}

void VulkanPicture::AddRef ()
{
	m_refCount++;
}

void VulkanPicture::Release ()
{
	int32_t ref = --m_refCount;

	if (ref == 0)
		Reset();
	else
		DE_ASSERT(ref > 0);
}

void VulkanPicture::Clear ()
{
	DE_ASSERT(m_refCount == 0);

	decodeWidth = 0;
	decodeHeight = 0;
	decodeSuperResWidth = 0;
	deMemset(&reserved, 0, sizeof(reserved));
}

void VulkanPicture::Reset()
{
	DE_ASSERT(m_refCount == 0);

	delete this;
}

} // video
} // vkt
