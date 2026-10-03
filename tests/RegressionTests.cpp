#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include <IUtilHTTPClient.h>
#include <curl/curl.h>

#include <array>
#include <chrono>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
namespace fs = std::filesystem;

static void Require(bool condition, std::string_view message)
{
    if (!condition)
        throw std::runtime_error(std::string(message));
}

template<class Expected, class Actual>
static void Equal(const Expected& expected, const Actual& actual, std::string_view message)
{
    Require(expected == actual, message);
}

class Winsock
{
public:
    Winsock()
    {
        WSADATA data{};
        Require(WSAStartup(MAKEWORD(2, 2), &data) == 0, "WSAStartup failed");
    }
    ~Winsock() { WSACleanup(); }
};

class Socket
{
public:
    explicit Socket(SOCKET value = INVALID_SOCKET) : value(value) {}
    ~Socket() { if (value != INVALID_SOCKET) closesocket(value); }
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    SOCKET value;
};

static unsigned short BindLoopback(Socket& socket)
{
    Require(socket.value != INVALID_SOCKET, "socket failed");
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    Require(bind(socket.value, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0, "bind failed");
    int length = sizeof(address);
    Require(getsockname(socket.value, reinterpret_cast<sockaddr*>(&address), &length) == 0, "getsockname failed");
    return ntohs(address.sin_port);
}

class HttpServer
{
public:
    explicit HttpServer(int requestCount = 1) : listener(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))
    {
        port = BindLoopback(listener);
        Require(listen(listener.value, SOMAXCONN) == 0, "listen failed");
        worker = std::thread([this, requestCount] {
            try
            {
                for (int i = 0; i < requestCount; ++i)
                {
                    Socket connection(accept(listener.value, nullptr, nullptr));
                    Require(connection.value != INVALID_SOCKET, "accept failed");
                    constexpr DWORD receiveTimeoutMs = 3000;
                    setsockopt(connection.value, SOL_SOCKET, SO_RCVTIMEO,
                        reinterpret_cast<const char*>(&receiveTimeoutMs), sizeof(receiveTimeoutMs));
                    setsockopt(connection.value, SOL_SOCKET, SO_SNDTIMEO,
                        reinterpret_cast<const char*>(&receiveTimeoutMs), sizeof(receiveTimeoutMs));
                    Respond(connection.value);
                }
            }
            catch (...) { error = std::current_exception(); }
        });
    }

    ~HttpServer()
    {
        // Closing our listener also unblocks accept when a test fails early.
        closesocket(listener.value);
        if (worker.joinable()) worker.join();
        listener.value = INVALID_SOCKET;
    }

    std::string Url(std::string_view path) const
    {
        return "http://127.0.0.1:" + std::to_string(port) + std::string(path);
    }

    void Finish()
    {
        if (worker.joinable()) worker.join();
        if (error) std::rethrow_exception(error);
    }

    std::string lastRequest;

private:
    static void SendAll(SOCKET connection, std::string_view bytes)
    {
        while (!bytes.empty())
        {
            int sent = send(connection, bytes.data(), static_cast<int>(bytes.size()), 0);
            Require(sent > 0, "send failed");
            bytes.remove_prefix(sent);
        }
    }

    void Respond(SOCKET connection)
    {
        std::array<char, 4096> buffer{};
        std::string request;
        size_t headerEnd = std::string::npos;
        size_t bodySize = 0;
        constexpr size_t maxRequestSize = 64 * 1024;
        while (headerEnd == std::string::npos || request.size() < headerEnd + 4 + bodySize)
        {
            int received = recv(connection, buffer.data(), static_cast<int>(buffer.size()), 0);
            Require(received > 0, "recv failed");
            request.append(buffer.data(), received);
            Require(request.size() <= maxRequestSize, "test request too large");
            headerEnd = request.find("\r\n\r\n");
            if (headerEnd != std::string::npos)
            {
                const auto lengthPosition = request.find("\r\nContent-Length:");
                if (lengthPosition < headerEnd)
                    bodySize = std::stoul(request.substr(lengthPosition + std::string_view("\r\nContent-Length:").size()));
            }
        }
        lastRequest = request;
        if (request.starts_with("GET /stream "))
        {
            SendAll(connection, "HTTP/1.1 200 OK\r\nX-Port-Test: present\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n");
            SendAll(connection, "5\r\nhello\r\n");
            std::this_thread::sleep_for(10ms);
            SendAll(connection, "6\r\n world\r\n0\r\n\r\n");
        }
        else
        {
            const bool missing = request.starts_with("GET /missing ");
            const std::string body = request.starts_with("POST /echo ")
                ? request.substr(headerEnd + 4, bodySize) : (missing ? "missing" : "hello world");
            const std::string status = missing ? "404 Not Found" : "200 OK";
            SendAll(connection, "HTTP/1.1 " + status + "\r\nX-Port-Test: present\r\nContent-Length: "
                + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
        }
    }

    Socket listener;
    unsigned short port{};
    std::thread worker;
    std::exception_ptr error;
};

class Module
{
public:
    Module(const fs::path& dllPath, const fs::path& curlPath, bool installed)
    {
        if (!installed)
        {
            curl = LoadLibraryW(curlPath.c_str());
            Require(curl != nullptr, "cannot load built libcurl");
            ownCurl = true;
        }
        dll = LoadLibraryW(dllPath.c_str());
        if (!dll)
        {
            const auto error = GetLastError();
            if (ownCurl) FreeLibrary(curl);
            throw std::runtime_error("cannot load client DLL; Windows error " + std::to_string(error));
        }
        if (installed)
        {
            curl = GetModuleHandleW(curlPath.filename().c_str());
            Require(curl != nullptr, "installed client did not load its libcurl");
            std::array<wchar_t, 32768> loadedPath{};
            Require(GetModuleFileNameW(curl, loadedPath.data(), static_cast<DWORD>(loadedPath.size())) != 0,
                "cannot locate loaded libcurl");
            Require(fs::equivalent(curlPath, loadedPath.data()), "client loaded libcurl from outside the install root");
        }
        factory = reinterpret_cast<CreateInterfaceFn>(GetProcAddress(dll, CREATEINTERFACE_PROCNAME));
        Require(factory != nullptr, "missing CreateInterface export");
    }
    ~Module()
    {
        if (dll) FreeLibrary(dll);
        if (ownCurl && curl) FreeLibrary(curl);
    }
    Module(const Module&) = delete;
    Module& operator=(const Module&) = delete;

    IUtilHTTPClientFactory* GetFactory() const
    {
        int result = IFACE_FAILED;
        auto value = static_cast<IUtilHTTPClientFactory*>(factory(UTIL_HTTPCLIENT_FACTORY_LIBCURL_INTERFACE_VERSION, &result));
        Require(value != nullptr, "missing _007 factory interface");
        Equal(IFACE_OK, result, "factory return code");
        return value;
    }

    HMODULE curl{};
    CreateInterfaceFn factory{};
private:
    HMODULE dll{};
    bool ownCurl{};
};

class Client
{
public:
    explicit Client(Module& module)
    {
        value = module.GetFactory()->CreateUtilHTTPClient();
        Require(value != nullptr, "client creation failed");
        CUtilHTTPClientCreationContext context;
        value->Init(&context);
    }
    ~Client() { value->Shutdown(); value->Destroy(); }
    IUtilHTTPClient* value{};
};

struct Results
{
    int completions{};
    int destroyed{};
    int status{};
    bool completed{};
    bool error{};
    std::string errorMessage;
    std::string body;
    std::string streamed;
    std::string header;
    bool streamHeaderPresent{true};
    std::vector<UtilHTTPRequestState> states;
};

class Callbacks : public IUtilHTTPCallbacks
{
public:
    explicit Callbacks(Results& results) : results(results) {}
    void Destroy() override { ++results.destroyed; delete this; }
    void OnResponseComplete(IUtilHTTPRequest*, IUtilHTTPResponse* response) override
    {
        ++results.completions;
        results.status = response->GetStatusCode();
        results.completed = response->IsResponseCompleted();
        results.error = response->IsResponseError();
        results.errorMessage = response->GetResponseErrorMessage();
        auto payload = response->GetPayload();
        results.body.assign(payload->GetBytes(), payload->GetLength());
        if (const auto header = response->GetHeaderValue("x-port-test")) results.header = header;
    }
    void OnUpdateState(IUtilHTTPRequest*, IUtilHTTPResponse*, UtilHTTPRequestState state) override
    {
        results.states.push_back(state);
    }
    void OnReceiveData(IUtilHTTPRequest*, IUtilHTTPResponse* response, const void* bytes, size_t size) override
    {
        results.streamed.append(static_cast<const char*>(bytes), size);
        const auto header = response->GetHeaderValue("X-Port-Test");
        results.streamHeaderPresent &= header && std::string_view(header) == "present";
    }
private:
    Results& results;
};

struct DestroyRequest { void operator()(IUtilHTTPRequest* value) const { if (value) value->Destroy(); } };
using Request = std::unique_ptr<IUtilHTTPRequest, DestroyRequest>;

static void Pump(IUtilHTTPClient* client, const Results& results)
{
    const auto deadline = std::chrono::steady_clock::now() + 8s;
    while (!results.completions && std::chrono::steady_clock::now() < deadline)
    {
        client->RunFrame();
        std::this_thread::sleep_for(1ms);
    }
    Equal(1, results.completions, "expected one completion within deadline");
}

static void SuccessfulResponse(const Results& results)
{
    Equal(200, results.status, "HTTP status");
    Require(results.completed && !results.error, "response should complete successfully");
    Equal(std::string("present"), results.header, "response header");
    Equal(std::vector{UtilHTTPRequestState::Requesting, UtilHTTPRequestState::Responding, UtilHTTPRequestState::Finished},
        results.states, "request state callbacks");
}

static void FactoryTest(Module& module)
{
    int result = IFACE_OK;
    Require(module.factory("unknown-interface", &result) == nullptr, "unknown interface should fail");
    Equal(IFACE_FAILED, result, "unknown interface return code");
    auto direct = static_cast<IUtilHTTPClient*>(module.factory(UTIL_HTTPCLIENT_LIBCURL_INTERFACE_VERSION, &result));
    Require(direct != nullptr, "missing _007 client interface");
    Equal(IFACE_OK, result, "client interface return code");
    CUtilHTTPClientCreationContext context;
    direct->Init(&context);
    direct->Shutdown();
    direct->Destroy();
    Client client(module);
    Require(client.value->GetRequestById(UTILHTTP_REQUEST_INVALID_ID) == nullptr, "new pool must be empty");
}

static void UrlTest(Module& module)
{
    auto factory = module.GetFactory();
    auto parsed = factory->ParseUrl("https://example.test/resource?q=1");
    Require(parsed != nullptr, "valid URL rejected");
    Equal(std::string_view("example.test"), std::string_view(parsed->GetHost()), "URL host");
    Equal(443, parsed->GetPort(), "default HTTPS port");
    Equal(std::string_view("/resource?q=1"), std::string_view(parsed->GetTarget()), "URL target");
    Require(parsed->IsSecure(), "default HTTPS must be secure");
    parsed->Destroy();
    Require(factory->ParseUrl("ftp://example.test/file") == nullptr, "unsupported URL accepted");
    Require(factory->ParseUrl("http://example.test:65536/file") == nullptr, "invalid port accepted");
}

static void SyncTest(Module& module, bool post, bool missing)
{
    HttpServer server;
    Results results;
    Client client(module);
    const auto url = server.Url(post ? "/echo" : (missing ? "/missing" : "/get"));
    Request request(client.value->CreateSyncRequest(url.c_str(), post ? UtilHTTPMethod::Post : UtilHTTPMethod::Get,
        new Callbacks(results)));
    Require(request != nullptr, "sync request creation failed");
    Require(!request->WaitForCompleteTimeout(0), "unsent request must not be complete");
    constexpr std::string_view posted = "port=standalone&value=123";
    if (post)
    {
        request->SetPostBody("application/x-www-form-urlencoded", posted.data(), posted.size());
        request->SetField("X-Request-Test", "custom");
    }
    request->SetTimeout(2);
    request->Send();
    Pump(client.value, results);
    Require(request->WaitForCompleteTimeout(0) && request->IsFinished(), "sync request must finish after pumping");
    if (missing)
    {
        Equal(404, results.status, "HTTP error status");
        Require(results.completed && results.error && !request->IsRequestSuccessful(), "HTTP error must be reported");
        Require(!results.errorMessage.empty(), "HTTP error message missing");
    }
    else
    {
        SuccessfulResponse(results);
        Equal(std::string(post ? posted : "hello world"), results.body, "response body");
        auto response = request->GetResponse();
        size_t size{};
        Require(response->GetHeaderSize("X-PORT-TEST", &size), "header size lookup failed");
        Equal(size_t{8}, size, "header size including terminator");
        std::vector<char> header(size);
        Require(response->GetHeader("X-Port-Test", header.data(), header.size()), "header copy failed");
        Equal(std::string("present"), std::string(header.data()), "header copy value");
        Require(response->GetHeaderValue("absent") == nullptr, "missing header must return null");
    }
    server.Finish();
    if (post)
        Require(server.lastRequest.find("X-Request-Test: custom\r\n") != std::string::npos, "custom request header not sent");
    request.reset();
    Equal(1, results.destroyed, "sync callback ownership");
}

static void AsyncTest(Module& module, bool stream)
{
    HttpServer server(stream ? 1 : 2);
    Results results;
    Results retainedResults;
    Client client(module);
    const auto url = server.Url(stream ? "/stream" : "/get");
    auto request = stream ? client.value->CreateAsyncStreamRequest(url.c_str(), UtilHTTPMethod::Get, new Callbacks(results))
        : client.value->CreateAsyncRequest(url.c_str(), UtilHTTPMethod::Get, new Callbacks(results));
    Require(request != nullptr, "async request creation failed");
    Require(request->IsAsync() && request->IsStream() == stream, "request type");
    client.value->AddToRequestPool(request);
    const auto id = request->GetRequestId();
    Require(id != UTILHTTP_REQUEST_INVALID_ID, "pool must assign a request ID");
    Equal(request, client.value->GetRequestById(id), "pool lookup");
    request->SetTimeout(2);
    request->Send();
    Pump(client.value, results);
    SuccessfulResponse(results);
    Require(client.value->GetRequestById(id) == nullptr, "auto-destroyed request must leave pool");
    Equal(1, results.destroyed, "async callback must be destroyed exactly once");
    if (stream)
    {
        Equal(std::string("hello world"), results.streamed, "streamed body");
        Require(results.streamHeaderPresent, "headers must be available during stream callbacks");
        Equal(std::string{}, results.body, "stream should not buffer response body");
    }
    else
    {
        Equal(std::string("hello world"), results.body, "async body");
        auto retained = client.value->CreateAsyncRequest(url.c_str(), UtilHTTPMethod::Get, new Callbacks(retainedResults));
        Require(retained != nullptr, "retained request creation failed");
        retained->SetAutoDestroyOnFinish(false);
        client.value->AddToRequestPool(retained);
        const auto retainedId = retained->GetRequestId();
        retained->SetTimeout(2);
        retained->Send();
        Pump(client.value, retainedResults);
        SuccessfulResponse(retainedResults);
        Require(client.value->GetRequestById(retainedId) != nullptr, "retained request must stay in pool");
        Require(client.value->DestroyRequestById(retainedId), "manual pool destruction failed");
        Require(!client.value->DestroyRequestById(retainedId), "destroyed ID must not be found");
        Equal(1, retainedResults.destroyed, "retained callback ownership");
    }
    server.Finish();
}

static void TransportErrorTest(Module& module)
{
    // Reserve a port without listening so connection refusal is deterministic.
    Socket reserved(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    auto port = BindLoopback(reserved);
    Results results;
    Client client(module);
    const auto url = "http://127.0.0.1:" + std::to_string(port) + "/refused";
    Request request(client.value->CreateSyncRequest(url.c_str(), UtilHTTPMethod::Get, new Callbacks(results)));
    Require(request != nullptr, "error request creation failed");
    request->SetTimeout(2);
    request->Send();
    Pump(client.value, results);
    Require(results.completed && results.error && !request->IsRequestSuccessful(), "transport error must be reported");
    Require(!results.errorMessage.empty(), "transport error message missing");
    Require(request->WaitForCompleteTimeout(0), "sync error must signal completion");
    request.reset();
    Equal(1, results.destroyed, "transport error callback ownership");
}

static void TlsTest(Module& module)
{
    using VersionInfo = curl_version_info_data* (*)(CURLversion);
    auto versionInfo = reinterpret_cast<VersionInfo>(GetProcAddress(module.curl, "curl_version_info"));
    Require(versionInfo != nullptr, "curl_version_info export missing");
    const auto info = versionInfo(CURLVERSION_NOW);
    Require(info != nullptr && (info->features & CURL_VERSION_SSL), "libcurl SSL feature missing");
    Require(info->ssl_version && std::string_view(info->ssl_version).find("Schannel") != std::string_view::npos,
        "libcurl must use Schannel");
    bool https = false;
    for (auto protocol = info->protocols; protocol && *protocol; ++protocol)
        https |= std::string_view(*protocol) == "https";
    Require(https, "libcurl HTTPS protocol missing");
}

int wmain(int argc, wchar_t** argv)
{
    try
    {
        Require(argc == 4, "usage: UtilHTTPClientTests <client.dll> <libcurl.dll> <scenario>");
        Require(SetEnvironmentVariableW(L"NO_PROXY", L"127.0.0.1") != 0, "cannot bypass proxies for loopback tests");
        Winsock winsock;
        const std::wstring scenario = argv[3];
        Module module(fs::absolute(argv[1]), fs::absolute(argv[2]), scenario == L"Installed");
        if (scenario == L"Factory") FactoryTest(module);
        else if (scenario == L"Url") UrlTest(module);
        else if (scenario == L"SyncGet") SyncTest(module, false, false);
        else if (scenario == L"Post") SyncTest(module, true, false);
        else if (scenario == L"AsyncPool") AsyncTest(module, false);
        else if (scenario == L"Stream") AsyncTest(module, true);
        else if (scenario == L"HttpError") SyncTest(module, false, true);
        else if (scenario == L"TransportError") TransportErrorTest(module);
        else if (scenario == L"TLS") TlsTest(module);
        else if (scenario == L"Installed")
        {
            FactoryTest(module);
            TlsTest(module);
            SyncTest(module, false, false);
        }
        else throw std::runtime_error("unknown scenario");
        std::wcout << scenario << L" passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
