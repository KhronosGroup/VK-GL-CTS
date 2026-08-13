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
    // Binds the IPC listener to an OS-assigned ephemeral port on localhost. The actual port
    // must be queried via getPort() and handed to the subprocess so it can connect back. This
    // makes concurrent deqp-vksc instances collision-free without any shared/fixed port.
    Parent();
    ~Parent();

    // Port the IPC listener is actually bound to (only valid after construction succeeds).
    int getPort() const;

    bool SetFile(const string &name, const std::vector<u8> &content);
    vector<u8> GetFile(const string &name);

private:
    std::unique_ptr<ParentImpl> impl;
};

struct ChildImpl;

struct Child
{
    // Connects to the parent IPC listener on localhost:port (the port reported by Parent::getPort()).
    Child(const int port);
    ~Child();

    bool SetFile(const string &name, const std::vector<u8> &content);
    vector<u8> GetFile(const string &name);

private:
    std::unique_ptr<ChildImpl> impl;
};

} // namespace ipc

} // namespace vksc_server

#endif // _VKSIPC_HPP
