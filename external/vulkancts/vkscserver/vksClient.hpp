#ifndef _VKSCLIENT_HPP
#define _VKSCLIENT_HPP

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

#include "vksNetwork.hpp"
#include "vksProtocol.hpp"

#include <mutex>

#include "deSocket.hpp"

namespace vksc_server
{

class Server
{
	de::SocketAddress addr;
	de::Socket socket;
	vector<u8> recvb;
	std::mutex mutex;

public:
	Server(const string& address)
	{
		string host;
		int port;
		StringToAddress(address, host, port);
		addr.setHost(host.c_str());
		addr.setPort(port);
		socket.connect(addr);
	}

	template <typename REQUEST, typename RESPONSE>
	void SendRequest(REQUEST& request, RESPONSE& response)
	{
		std::lock_guard<std::mutex> lock(mutex);
		SendPayloadWithHeader(&socket, REQUEST::Type(), Serialize(request));

		vector<u8> packet = RecvPacket(&socket, recvb, RESPONSE::Type());
		response = Deserialize<RESPONSE>(packet);
	}

	template <typename REQUEST>
	void SendRequest(REQUEST& request)
	{
		std::lock_guard<std::mutex> lock(mutex);
		SendPayloadWithHeader(&socket, REQUEST::Type(), Serialize(request));
	}
};

inline std::unique_ptr<Server>& StandardOutputServerSingleton()
{
	static std::unique_ptr<Server> server;
	return server;
}

inline void OpenRemoteStandardOutput (const string& address)
{
	StandardOutputServerSingleton() = std::unique_ptr<Server>( new Server(address) );
}

inline bool RemoteWrite (int type, const char* message)
{
	auto&& ss = StandardOutputServerSingleton();
	if (ss)
	{
		LogRequest request;
		request.type = type;
		request.message = message;
		ss->SendRequest(request);
		return false;
	}
	return true;
}

inline bool RemoteWriteFtm (int type, const char* format, va_list args)
{
	auto&& ss = StandardOutputServerSingleton();
	if (ss)
	{
		char message[4086];
		vsprintf(message, format, args);
		LogRequest request;
		request.type = type;
		request.message = message;
		ss->SendRequest(request);
		return false;
	}
	return true;
}

}

#endif // _VKSCLIENT_HPP
