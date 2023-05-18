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
#include "vksClient.hpp"

#include <iostream>

#include "deSocket.hpp"
#include "deCommandLine.hpp"

using namespace vksc_server;

namespace opt
{

DE_DECLARE_COMMAND_LINE_OPT(Address,		string);

const string DefaultAddress = "localhost:" + std::to_string(DefaultPort);

void registerOptions (de::cmdline::Parser& parser)
{
	using de::cmdline::Option;
	using de::cmdline::NamedValue;

	parser << Option<Address>		("a", "address",	"Address", DefaultAddress.c_str());
}

}

void RunTests (Server& server);

int main (int argc, char** argv)
{
	de::cmdline::CommandLine	cmdLine;

	// Parse command line.
	{
		de::cmdline::Parser	parser;
		opt::registerOptions(parser);

		if (!parser.parse(argc, argv, &cmdLine, std::cerr))
		{
			parser.help(std::cout);
			return EXIT_FAILURE;
		}
	}

	try
	{
		string address = cmdLine.getOption<opt::Address>();
		std::cout << "connecting to " << address << "..." << std::endl;
		Server server(address);
		RunTests(server);
	}
	catch (const std::exception& e) { std::cout << e.what() << std::endl; }

	return EXIT_SUCCESS;
}

std::ostream& operator<<(std::ostream& os, const vector<u8>& data)
{
	os << '{';
	for (msize i{}; i < data.size(); ++i) os << (int)data[i] << ((i+1) < data.size() ? ", " : "");
	os << '}';
    return os;
}

template <typename T>
void Except (const string& name, const T& value, T excepted, const string& message)
{
	std::cout << message << std::endl;
	if (value != excepted)
	{
		std::cout << name << " -> expected: " << excepted << " but got " << value << std::endl;
		throw std::runtime_error("Test failed: " + message);
	}
	std::cout << "ok" << std::endl;
}

void RunStoreContentTests (Server& server)
{
	{
		StoreContentRequest request;
		request.data = {1, 2, 3, 4};
		request.name = "@test1";
		StoreContentResponse response;
		server.SendRequest(request, response);

		Except("StoreContentResponse::status", response.status, true, "After requesting to store data on a server we should received true");
	}

	{
		StoreContentRequest request;
		request.data = {5,6,7,8,9};
		request.name = "@test1";
		StoreContentResponse response;
		server.SendRequest(request, response);

		Except("StoreContentResponse::status", response.status, true, "After requesting to store data with a name that is already in use we should received false");
	}
}

void RunGetContentTests (Server& server)
{
	{
		GetContentRequest request;
		request.path = "@test1";
		request.removeAfter = true;
		GetContentResponse response;
		server.SendRequest(request, response);

		Except("StoreContentResponse::status", response.status, true, "After requesting to get data from server store we should received true");
		Except("StoreContentResponse::data", response.data, {5,6,7,8,9}, "Received data must be correct");
	}

	{
		GetContentRequest request;
		request.path = "@test1";
		request.removeAfter = true;
		GetContentResponse response;
		server.SendRequest(request, response);

		Except("StoreContentResponse::status", response.status, false, "Requesting to get data from server memory that no longer exist should result in false");
	}
}

void RunCompileShaderTests (Server& server)
{
	{
		CompileShaderRequest request;
		request.source.active = "glsl";
		request.source.glsl = {};
		request.source.glsl.sources[glu::SHADERTYPE_VERTEX].push_back(
		R"glsl(#version 450

			vec2 positions[3] = vec2[](
				vec2(0.0, -0.5),
				vec2(0.5, 0.5),
				vec2(-0.5, 0.5)
			);

			void main() {
				gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
			}
		)glsl");
		request.commandLine = {};

		CompileShaderResponse response;
		server.SendRequest(request, response);

		Except("StoreContentResponse::status", response.status, true, "After requesting server to compile glsl shader we should get true as a result");
		Except("StoreContentResponse::binary.empty()", response.binary.empty(), false, "Received data must be not empty");
	}
}

void RunTests (Server& server)
{
	RunStoreContentTests(server);
	RunGetContentTests(server);
	RunCompileShaderTests(server);

	std::cout << "All tests passed" << std::endl;
}
