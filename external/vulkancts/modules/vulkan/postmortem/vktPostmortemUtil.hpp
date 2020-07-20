#ifndef _VKTPOSTMORTEMUTIL_HPP
#define _VKTPOSTMORTEMUTIL_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 The Khronos Group Inc.
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
 */
/*!
 * \file
 * \brief Utilities for experimental crash postmortem tests
 */
/*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"

namespace vkt
{
namespace postmortem
{

class PostmortemTestInstance : public vkt::TestInstance
{
public:
	PostmortemTestInstance(Context& context);
	virtual ~PostmortemTestInstance();

protected:
	vk::Unique<vk::VkDevice>	m_logicalDevice;
	vk::DeviceDriver			m_deviceDriver;
	deUint32					m_queueFamilyIndex;
	vk::VkQueue					m_queue;
	vk::SimpleAllocator			m_allocator;
};

} // namespace postmortem
} // namespace vkt

#endif // _VKTPOSTMORTEMUTIL_HPP
