#ifndef _VKSIPC_HPP
#define _VKSIPC_HPP

/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
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
 *-------------------------------------------------------------------------*/

#include "vksCommon.hpp"

#include <memory>

namespace vksc_server
{

namespace ipc
{

struct ParentImpl;

struct Parent
{
				Parent	(const int portOffset);
				~Parent	();

	bool		SetFile	(const string& name, const std::vector<u8>& content);
	vector<u8>	GetFile	(const string& name);

private:
	std::unique_ptr<ParentImpl> impl;
};

struct ChildImpl;

struct Child
{
				Child	(const int portOffset);
				~Child	();

	bool		SetFile	(const string& name, const std::vector<u8>& content);
	vector<u8>	GetFile	(const string& name);

private:
	std::unique_ptr<ChildImpl> impl;
};

} // ipc

} // vksc_server

#endif // _VKSIPC_HPP
