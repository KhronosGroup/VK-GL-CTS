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
#include "vksSerializer.hpp"

#include <sstream>

#include "deSocket.hpp"

namespace vksc_server
{

void StringToAddress (const string& str, string& host, int& port)
{
	auto pos = str.find_last_of(':');
	if (pos == string::npos)
	{
		host = str.c_str();
		port = DefaultPort;
	}
	else
	{
		host = str.substr(0, pos);
		std::stringstream{str.substr(pos+1)} >> port;
	}
}

bool ProccessNetworkData (vector<u8>& buffer, const std::function<void(u32, vector<u8>)>& packetInterpreter)
{
	constexpr msize headerSize = 8;

	if (buffer.size() >= headerSize)
	{
		u32 classHash;
		u32 packetSize;

		Serializer<ToRead>{buffer}.Serialize(classHash, packetSize);

		if (buffer.size() >= packetSize + headerSize)
		{
			auto itbeging	= buffer.begin() + headerSize;
			auto itend		= itbeging + packetSize;
			packetInterpreter(classHash, vector<u8>(itbeging, itend));
			buffer.erase(buffer.begin(), itend);
			return buffer.size() >= headerSize; // Try again?
		}
	}

	return false;
}

void Send (de::Socket* socket, const vector<u8>& buffer)
{
	msize sent_total{};
	do
	{
		msize sent{};
		auto result = socket->send(buffer.data() + sent_total, buffer.size() - sent_total, &sent);
		if (result != DE_SOCKETRESULT_SUCCESS)
			throw std::runtime_error("Can't send data to socket");
		sent_total += sent;
	} while (sent_total < buffer.size());
}

void RecvSome (de::Socket* socket, vector<u8>& recvb)
{
	msize received;
	u8 data[8 * 1024];
	auto result = socket->receive(data, sizeof(data), &received);
	if (result != DE_SOCKETRESULT_SUCCESS)
		throw std::runtime_error("Can't receive data from socket");
	recvb.insert(recvb.end(), data, data + received);
}

void SendPayloadWithHeader (de::Socket* socket, u32 type, const std::vector<u8>& payload)
{
	u32 size = static_cast<u32>(payload.size());

	vector<u8> header;
	Serializer<ToWrite> header_serializer(header);
	header_serializer.Serialize(type, size);

	Send(socket, header);
	Send(socket, payload);
}

vector<u8> RecvPacket (de::Socket* socket, vector<u8>& recvb, u32 type)
{
	bool result = false;
	vector<u8> packet;

	while (socket->isConnected() && !result)
	{
		RecvSome(socket, recvb);

		auto interpret = [&](u32 classHash, vector<u8> bufferData)
		{
			if (classHash != type) throw std::runtime_error("Unexpected packet type received");
			packet = std::move(bufferData);
			result = true;
		};

		ProccessNetworkData(recvb, interpret);
	}

	if (!result) throw std::runtime_error("connection lost before we could get data");

	return packet;
}

};
