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

#include "vksIPC.hpp"
#include "vksStore.hpp"
#include "vksClient.hpp"

#include <future>

namespace vksc_server
{

namespace ipc
{

// Bind and connect using an explicit IPv4 loopback literal rather than "localhost". "localhost" can
// resolve to either 127.0.0.1 or ::1, and if the parent's bind and the child's connect pick
// different families the child would target an address nobody is listening on. A fixed literal keeps
// both ends on the same loopback interface.
constexpr const char *LoopbackHost = "127.0.0.1";

struct ChildConnection
{
    int id;
    std::unique_ptr<de::Socket> socket;
    std::atomic<bool> &appactive;
    vector<u8> recvb;
};

struct PacketsLoop
{
    ChildConnection client;
    Store &fileStore;

    template <typename T>
    static void SendResponse(ChildConnection &c, T &data)
    {
        SendPayloadWithHeader(c.socket.get(), T::Type(), Serialize(data));
    }

    void ProcessPacketsOnServer(ChildConnection &c, u32 type, vector<u8> packet)
    {
        switch (type)
        {
        case StoreContentRequest::Type():
        {
            auto req = Deserialize<StoreContentRequest>(packet);
            bool ok  = fileStore.Set(req.name, req.data);

            StoreContentResponse res;
            res.status = ok;
            SendResponse(c, res);
        }
        break;
        case GetContentRequest::Type():
        {
            auto req = Deserialize<GetContentRequest>(packet);

            vector<u8> content;
            bool ok = fileStore.Get(req.path, content, req.removeAfter);

            GetContentResponse res;
            res.status = ok;
            res.data   = std::move(content);
            SendResponse(c, res);
        }
        break;

        default:
            throw std::runtime_error("ipc communication error");
        }
    }

    void Loop()
    {
        while (client.socket->isConnected() && client.appactive)
        {
            RecvSome(client.socket.get(), client.recvb);
            auto interpret = [this](u32 type, vector<u8> packet)
            { ProcessPacketsOnServer(client, type, std::move(packet)); };
            while (ProccessNetworkData(client.recvb, interpret))
            {
            }
        }
    }

    void operator()()
    {
        try
        {
            Loop();
        }
        catch (const std::exception &)
        {
            client.socket->close();
        }
    }
};

struct ParentImpl
{
    Store fileStore;
    de::Socket listener;
    int m_port{0};
    std::atomic<bool> appActive{true};
    std::thread listenerLoop;

    void ParentLoop()
    {
        vector<std::future<void>> clients;
        int id{};

        try
        {
            while (appActive)
            {
                remove_erase_if(clients, [](const std::future<void> &c) { return is_ready(c); });
                ChildConnection client{++id, std::unique_ptr<de::Socket>(listener.accept()), appActive, vector<u8>{}};
                clients.push_back(CreateClientThread(std::move(client)));
            }
        }
        catch (const std::exception &)
        {
            appActive = false;
        }
    }

    ParentImpl()
    {
        // Bind to an OS-assigned ephemeral port (port 0) on the loopback interface. This guarantees
        // a free port, so multiple concurrent deqp-vksc processes never collide on a shared/fixed
        // port. The listener is bound synchronously here so getPort() is valid before any subprocess
        // is spawned; the accept loop then runs on its own thread.
        de::SocketAddress addr;
        addr.setHost(LoopbackHost);
        addr.setPort(0);
        listener.listen(addr);
        m_port = listener.getBoundPort();

        listenerLoop = std::thread{[this]() { ParentLoop(); }};
    }

    ~ParentImpl()
    {
        appActive = false;

        // Dummy connection to trigger accept()
        de::SocketAddress addr;
        addr.setHost(LoopbackHost);
        addr.setPort(m_port);
        de::Socket socket;

        try
        {
            socket.connect(addr);
        }
        catch (const de::SocketError &)
        {
        }

        try
        {
            socket.close();
        }
        catch (const de::SocketError &)
        {
        }

        listenerLoop.join();
    }

    std::future<void> CreateClientThread(ChildConnection client)
    {
        return std::async(PacketsLoop{std::move(client), fileStore});
    }
};

Parent::Parent()
{
    impl.reset(new ParentImpl());
}

Parent::~Parent()
{
}

int Parent::getPort() const
{
    return impl->m_port;
}

bool Parent::SetFile(const string &name, const std::vector<u8> &content)
{
    return impl->fileStore.Set(name, content);
}

vector<u8> Parent::GetFile(const string &name)
{
    vector<u8> content;
    bool result = impl->fileStore.Get(name, content, false);
    if (result)
        return content;
    else
        return {};
}

struct ChildImpl
{
    ChildImpl(const int port) : m_port(port)
    {
        connection.reset(new Server{std::string(LoopbackHost) + ":" + std::to_string(m_port)});
    }

    int m_port;
    std::unique_ptr<Server> connection;
};

Child::Child(const int port)
{
    impl.reset(new ChildImpl(port));
}

Child::~Child()
{
}

bool Child::SetFile(const string &name, const std::vector<u8> &content)
{
    StoreContentRequest request;
    request.name = name;
    request.data = content;
    StoreContentResponse response;
    impl->connection->SendRequest(request, response);
    return response.status;
}

std::vector<u8> Child::GetFile(const string &name)
{
    GetContentRequest request;
    request.path         = name;
    request.physicalFile = false;
    request.removeAfter  = false;
    GetContentResponse response;
    impl->connection->SendRequest(request, response);
    if (response.status == true)
        return response.data;
    else
        return {};
}

} // namespace ipc

} // namespace vksc_server
