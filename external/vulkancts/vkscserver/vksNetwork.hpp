#ifndef _VKSNETWORK_HPP
#define _VKSNETWORK_HPP

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

namespace de { class Socket; };

namespace vksc_server
{

constexpr auto DefaultPort = 59333;

// Conver string (for example "192.168.0.1:59333") to host and port
void		StringToAddress			(const string& str,		string& host,				int& port							);

// Scan buffer, looking for a packet and call packetInterpreter, returns true if there is possibitly for another packet
bool		ProccessNetworkData		(vector<u8>& buffer,	const std::function<void(u32, vector<u8>)>& packetInterpreter	);

// Sends whole bufer to socket
void		Send					(de::Socket* socket,	const vector<u8>& buffer										);

// Send whole payload and insert [type, size] header before it
void		SendPayloadWithHeader	(de::Socket* socket,	u32 type,					const vector<u8>& payload			);

// Recv some bytes frome socket and insert it at the back of recvb
void		RecvSome				(de::Socket* socket,	vector<u8>& recvb);

// Recv single packet from socket (it will block until this function get it)
vector<u8>	RecvPacket				(de::Socket* socket,	vector<u8>&	recvb,			u32 type							);

}

#endif // _VKSNETWORK_HPP
