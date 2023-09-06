/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2022 NVIDIA CORPORATION, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Vulkan SC utilities
 *//*--------------------------------------------------------------------*/

#include "vkAppParamsUtil.hpp"

#include <fstream>
#include <string>
#include <sstream>

#ifdef CTS_USES_VULKANSC

namespace vk
{

std::string trim (const std::string& original)
{
	static const std::string whiteSigns = " \t";
	const auto beg = original.find_first_not_of(whiteSigns);
	if (beg == std::string::npos)
		return std::string();
	const auto end = original.find_last_not_of(whiteSigns);
	return original.substr(beg, end - beg + 1);
}

bool readApplicationParameters (std::vector<VkApplicationParametersEXT>& appParams, const tcu::CommandLine& cmdLine, const bool readInstanceAppParams)
{
	const char* appParamsInputFilePath = cmdLine.getAppParamsInputFilePath();

	if (appParamsInputFilePath == DE_NULL)
		return false;

	std::ifstream							file(appParamsInputFilePath);
	std::vector<std::string>				lines;
	std::vector<VkApplicationParametersEXT>	tmpAppParams;

	if (file.is_open())
	{
		std::string line;

		while (std::getline(file, line))
			lines.push_back(line);

		file.close();
	}
	else
	{
		TCU_THROW(InternalError, "Application parameters input file not found from --deqp-app-params-input-file");
		return false;
	}

	for (const std::string& line : lines)
	{
		if (line.empty())
			continue;

		std::stringstream			sstream(line);
		std::string					token;
		std::vector<std::string>	tokens;

		while (std::getline(sstream, token, ','))
			tokens.push_back(trim(token));

		if (tokens[0] != "instance" && tokens[0] != "device")
		{
			TCU_THROW(InternalError, "Invalid create type from --deqp-app-params-input-file");
			return false;
		}

		if ((tokens[0] == "instance" && readInstanceAppParams) || (tokens[0] == "device" && !readInstanceAppParams))
		{
			if (tokens.size() == 5)
			{
				const VkApplicationParametersEXT appParam =
				{
					VK_STRUCTURE_TYPE_APPLICATION_PARAMETERS_EXT,				// sType
					DE_NULL,													// pNext
					static_cast<deUint32>(std::stoul(tokens[1], nullptr, 16)),	// vendorID
					static_cast<deUint32>(std::stoul(tokens[2], nullptr, 16)),	// deviceID
					static_cast<deUint32>(std::stoul(tokens[3], nullptr, 16)),	// key
					static_cast<deUint64>(std::stoul(tokens[4], nullptr, 16))	// value
				};

				tmpAppParams.push_back(appParam);
			}
			else
			{
				TCU_THROW(InternalError, "Invalid input format from --deqp-app-params-input-file");
				return false;
			}
		}
	}

	if (tmpAppParams.empty())
		return false;

	appParams = tmpAppParams;

	for (size_t ndx = 0; ndx < appParams.size(); ++ndx)
	{
		if (ndx != appParams.size() - 1)
			appParams[ndx].pNext = &appParams[ndx + 1];
	}

	return true;
}

} // vk

#endif // CTS_USES_VULKANSC