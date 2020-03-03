#ifndef _TCUWAIVERUTIL_HPP
#define _TCUWAIVERUTIL_HPP
/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
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
 *//*!
 * \file
 * \brief Waiver mechanism implementation.
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"
#include <string>
#include <vector>

namespace tcu
{

class WaiverUtil
{
public:
			WaiverUtil		() = default;

	void	setup			(const std::string waiverFile, std::string packageName, deUint32 vendorId, deUint32 deviceId);
	void	setup			(const std::string waiverFile, std::string packageName, std::string vendor, std::string renderer);

	bool	isOnWaiverList	(const std::string& casePath) const;

public:

	struct WaiverComponent
	{
		std::string						name;
		std::vector<WaiverComponent*>	children;
	};

private:

	std::vector<WaiverComponent> m_waiverTree;
};

} // tcu

#endif // _TCUWAIVERUTIL_HPP
