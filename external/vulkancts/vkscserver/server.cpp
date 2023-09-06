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
#include "vksServices.hpp"

#include <iostream>
#include <fstream>
#include <future>
#include <atomic>
#include <initializer_list>

#include "deSocket.hpp"
#include "deCommandLine.hpp"

using namespace vksc_server;

namespace opt
{

DE_DECLARE_COMMAND_LINE_OPT(Port,						int);
DE_DECLARE_COMMAND_LINE_OPT(LogFile,					std::string);
DE_DECLARE_COMMAND_LINE_OPT(PipelineCompilerPath,		std::string);
DE_DECLARE_COMMAND_LINE_OPT(PipelineCompilerDataDir,	std::string);
DE_DECLARE_COMMAND_LINE_OPT(PipelineCompilerOutputFile,	std::string);
DE_DECLARE_COMMAND_LINE_OPT(PipelineCompilerLogFile,	std::string);
DE_DECLARE_COMMAND_LINE_OPT(PipelineCompilerArgs,		std::string);

const auto DefaultPortStr = std::to_string(DefaultPort);

void registerOptions (de::cmdline::Parser& parser)
{
	using de::cmdline::Option;
	using de::cmdline::NamedValue;

	parser << Option<Port>							(DE_NULL, "port",				"Port",									DefaultPortStr.c_str());
	parser << Option<LogFile>						(DE_NULL, "log",				"Log filename",							"dummy.log");
	parser << Option<PipelineCompilerPath>			(DE_NULL, "pipeline-compiler",	"Path to offline pipeline compiler",	"");
	parser << Option<PipelineCompilerDataDir>		(DE_NULL, "pipeline-dir",		"Offline pipeline data directory",		"");
	parser << Option<PipelineCompilerOutputFile>	(DE_NULL, "pipeline-file",		"Output file with pipeline cache",		"");
	parser << Option<PipelineCompilerLogFile>		(DE_NULL, "pipeline-log",		"Compiler log file",					"compiler.log");
	parser << Option<PipelineCompilerArgs>			(DE_NULL, "pipeline-args",		"Additional compiler parameters",		"");
}

}

void Log () { std::cout << std::endl; }
template <typename ARG>
void Log (ARG&& arg) { std::cout << arg << ' ' << std::endl;  }
template <typename ARG, typename... ARGs>
void Log (ARG&& first, ARGs&&... args)	{ std::cout << first << ' '; Log(args...); }

#ifdef _DEBUG
template <typename... ARGs>
void Debug (ARGs&&... args)
{
	Log("[DEBUG]", std::forward<ARGs>(args)...);
}
#else
template <typename... ARGs>
void Debug (ARGs&&...) { }
#endif

struct Client
{
	int							id;
	std::unique_ptr<de::Socket>	socket;
	std::atomic<bool>&			appactive;
	vector<u8>					recvb;
	CmdLineParams				cmdLineParams;
	std::string					logFile;
};

std::future<void> CreateClientThread (Client client);

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

	std::atomic<bool> appActive{true};

	try
	{
		de::SocketAddress addr;
		addr.setHost("0.0.0.0");
		addr.setPort(cmdLine.getOption<opt::Port>());
		de::Socket listener;
		int id{};

		vector<std::future<void>> clients;

		listener.listen(addr);
		Log("Listening on port", addr.getPort());

		while (appActive)
		{
			remove_erase_if(clients, [](const std::future<void>& c) { return is_ready(c); });
			Client client{ ++id, std::unique_ptr<de::Socket>(listener.accept()), appActive, vector<u8>{},
			{
				cmdLine.getOption<opt::PipelineCompilerPath>(),
				cmdLine.getOption<opt::PipelineCompilerDataDir>(),
				cmdLine.getOption<opt::PipelineCompilerOutputFile>(),
				cmdLine.getOption<opt::PipelineCompilerLogFile>(),
				cmdLine.getOption<opt::PipelineCompilerArgs>()
			},
			cmdLine.getOption<opt::LogFile>() };
			Debug("New client with id", id - 1, "connected");
			clients.push_back(CreateClientThread(std::move(client)));
		}
	}
	catch (const std::exception& e) { Log(e.what()); appActive = false; }

	return EXIT_SUCCESS;
}

template <typename T>
void SendResponse (Client& c, T& data)
{
	SendPayloadWithHeader(c.socket.get(), T::Type(), Serialize(data));
}

void ProcessPacketsOnServer (Client& client, u32 type, vector<u8> packet)
{
	switch (type)
	{
		case LogRequest::Type():
		{
			auto req = Deserialize<LogRequest>(packet);
			std::cout << req.message;
		}
		break;
		case CompileShaderRequest::Type():
		{
			auto req = Deserialize<CompileShaderRequest>(packet);

			vector<u8> result;
			bool ok = CompileShader(req.source, req.commandLine, result);

			CompileShaderResponse res;
			res.status = ok;
			res.binary = std::move(result);
			SendResponse(client, res);
		}
		break;
		case StoreContentRequest::Type():
		{
			auto req = Deserialize<StoreContentRequest>(packet);
			bool ok = StoreFile(req.name, req.data);

			StoreContentResponse res;
			res.status = ok;
			SendResponse(client, res);
		}
		break;
		case GetContentRequest::Type():
		{
			auto req = Deserialize<GetContentRequest>(packet);

			vector<u8> content;
			bool ok = GetFile(req.path, content, req.removeAfter);

			GetContentResponse res;
			res.status = ok;
			res.data = std::move(content);
			SendResponse(client, res);
		}
		break;
		case AppendRequest::Type():
		{
			auto req = Deserialize<AppendRequest>(packet);

			bool result = AppendFile(req.fileName, req.data, req.clear);
			if (!result) Log("[WARNING] Can't append file", req.fileName);
		}
		break;
		case CreateCacheRequest::Type():
		{
			auto req = Deserialize<CreateCacheRequest>(packet);

			vector<u8>							binary;
			bool ok = false;
			try
			{
				CreateVulkanSCCache(req.input, req.caseFraction, binary, client.cmdLineParams, client.logFile);
				ok = true;
			}
			catch (const std::exception& e)
			{
				Log("[ERROR] Can't create cache:", e.what());
				binary = {};
			}

			CreateCacheResponse res;
			res.status			= ok;
			res.binary			= std::move(binary);
			SendResponse(client, res);
		}
		break;

		default:
			throw std::runtime_error("communication error");
	}
}

struct PacketsLoop
{
	Client client;

	void Loop ()
	{
		while (client.socket->isConnected() && client.appactive)
		{
			RecvSome(client.socket.get(), client.recvb);
			auto interpret = [this](u32 type, vector<u8> packet) { ProcessPacketsOnServer(client, type, std::move(packet)); };
			while (ProccessNetworkData(client.recvb, interpret)) {}
		}
	}

	void operator() ()
	{
		try { Loop(); }
		catch (const std::exception& e)
		{
			client.socket->close();
			Debug(e.what(), "from client with id", client.id);
		}
		Debug("Client with id", client.id, "disconnected.");
	}
};

std::future<void> CreateClientThread (Client client)
{
	return std::async( PacketsLoop{ std::move(client) } );
}
