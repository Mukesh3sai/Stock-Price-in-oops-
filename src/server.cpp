// Small LOCAL-ONLY HTTP server. All stock calculations happen in stock.cpp.
// No frameworks, cloud API, API keys, database, or third-party C++ dependencies.
#include "stock.hpp"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
using Socket = SOCKET;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using Socket = int;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#endif

namespace {
constexpr std::size_t MAX_BODY = 2 * 1024 * 1024;  // 2 MiB CSV limit
constexpr std::size_t MAX_HEADERS = 16 * 1024;

void closeSocket(Socket sock) {
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
}

struct HttpRequest {
    std::string method;
    std::string target;
    std::string body;
};

int parseIntStrict(const std::string& raw, int lower, int upper) {
    if (raw.empty() || raw.size() > 4) throw std::runtime_error("Invalid numeric parameter");
    for (char ch : raw) {
        if (!std::isdigit(static_cast<unsigned char>(ch))) throw std::runtime_error("Invalid numeric parameter");
    }
    const int number = std::stoi(raw);
    if (number < lower || number > upper) {
        throw std::runtime_error("Parameter outside permitted range");
    }
    return number;
}

std::map<std::string, std::string> parseQuery(const std::string& target) {
    std::map<std::string, std::string> result;
    const auto q = target.find('?');
    if (q == std::string::npos) return result;
    std::istringstream pairs(target.substr(q + 1));
    std::string pair;
    while (std::getline(pairs, pair, '&')) {
        const auto equals = pair.find('=');
        if (equals != std::string::npos) result[pair.substr(0, equals)] = pair.substr(equals + 1);
    }
    return result;
}

std::string readFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Could not read: " + path.string());
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

HttpRequest receiveRequest(Socket client) {
    std::string bytes;
    char buffer[8192];
    std::size_t delim = std::string::npos;
    while ((delim = bytes.find("\r\n\r\n")) == std::string::npos) {
        const int got = recv(client, buffer, sizeof(buffer), 0);
        if (got <= 0) throw std::runtime_error("Connection interrupted reading headers");
        bytes.append(buffer, static_cast<std::size_t>(got));
        if (bytes.size() > MAX_BODY + MAX_HEADERS) throw std::runtime_error("HTTP request too large");
        if (bytes.size() > MAX_HEADERS && bytes.find("\r\n\r\n") == std::string::npos) {
            throw std::runtime_error("HTTP headers too large");
        }
    }
    if (delim > MAX_HEADERS) throw std::runtime_error("HTTP headers too large");
    std::istringstream headers(bytes.substr(0, delim));
    std::string firstLine;
    std::getline(headers, firstLine);
    std::istringstream first(firstLine);
    HttpRequest request;
    first >> request.method >> request.target;
    if ((request.method != "GET" && request.method != "POST") ||
        request.target.empty() || request.target[0] != '/') {
        throw std::runtime_error("Unsupported HTTP request");
    }
    std::size_t contentLength = 0;
    bool lengthSeen = false;
    std::string line;
    while (std::getline(headers, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = line.substr(0, colon);
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return std::tolower(c); });
        if (key == "transfer-encoding") throw std::runtime_error("Chunked HTTP not supported");
        if (key == "content-length") {
            if (lengthSeen) throw std::runtime_error("Duplicate Content-Length");
            lengthSeen = true;
            std::string value = line.substr(colon + 1);
            const auto begin = value.find_first_not_of(" \t");
            if (begin == std::string::npos) throw std::runtime_error("Invalid Content-Length");
            value = value.substr(begin);
            if (value.empty() || !std::all_of(value.begin(), value.end(), [](unsigned char c){ return std::isdigit(c); })) {
                throw std::runtime_error("Invalid Content-Length");
            }
            const unsigned long long parsed = std::stoull(value);
            if (parsed > MAX_BODY) throw std::runtime_error("Upload must be smaller than 2 MiB");
            contentLength = static_cast<std::size_t>(parsed);
        }
    }
    if (request.method == "POST" && !lengthSeen) {
        throw std::runtime_error("POST needs Content-Length");
    }
    const std::size_t bodyStart = delim + 4;
    if (bytes.size() - bodyStart > MAX_BODY) throw std::runtime_error("Upload too large");
    request.body = bytes.substr(bodyStart);
    while (request.body.size() < contentLength) {
        const int got = recv(client, buffer, static_cast<int>(std::min(sizeof(buffer), contentLength - request.body.size())), 0);
        if (got <= 0) throw std::runtime_error("Connection interrupted reading CSV");
        request.body.append(buffer, static_cast<std::size_t>(got));
    }
    request.body.resize(contentLength);
    return request;
}

void sendResponse(Socket client, int status, const std::string& type, const std::string& body) {
    std::string statusText = status == 200 ? "OK" : status == 404 ? "Not Found" :
                             status == 405 ? "Method Not Allowed" :
                             status == 413 ? "Payload Too Large" : "Bad Request";
    std::ostringstream header;
    header << "HTTP/1.1 " << status << " " << statusText << "\r\n"
           << "Content-Type: " << type << "\r\n"
           << "Content-Length: " << body.size() << "\r\n"
           << "Connection: close\r\n"
           << "Cache-Control: no-store\r\n"
           << "X-Content-Type-Options: nosniff\r\n"
           << "X-Frame-Options: DENY\r\n\r\n";
    const std::string response = header.str() + body;
    std::size_t sent = 0;
    while (sent < response.size()) {
        const int wrote = send(client, response.data() + sent,
                               static_cast<int>(std::min<std::size_t>(8192, response.size() - sent)), 0);
        if (wrote <= 0) break;
        sent += static_cast<std::size_t>(wrote);
    }
}

void route(Socket client, const HttpRequest& request, const std::filesystem::path& root) {
    const std::string path = request.target.substr(0, request.target.find('?'));
    if (request.method == "GET") {
        if (path == "/health") {
            sendResponse(client, 200, "application/json; charset=utf-8", "{\"status\":\"ok\"}");
            return;
        }
        const std::map<std::string, std::pair<std::string, std::filesystem::path>> routes = {
            {"/", {"text/html; charset=utf-8", root / "web/index.html"}},
            {"/index.html", {"text/html; charset=utf-8", root / "web/index.html"}},
            {"/styles.css", {"text/css; charset=utf-8", root / "web/styles.css"}},
            {"/app.js", {"text/javascript; charset=utf-8", root / "web/app.js"}},
            {"/sample.csv", {"text/csv; charset=utf-8", root / "data/sample_stock.csv"}}
        };
        const auto match = routes.find(path);
        if (match == routes.end()) {
            sendResponse(client, 404, "text/plain; charset=utf-8", "Not found");
            return;
        }
        sendResponse(client, 200, match->second.first, readFile(match->second.second));
        return;
    }
    if (request.method == "POST" && path == "/api/analyze") {
        const auto query = parseQuery(request.target);
        const int window = query.count("window") ? parseIntStrict(query.at("window"), 2, 60) : 7;
        const int days = query.count("days") ? parseIntStrict(query.at("days"), 1, 30) : 5;
        if (request.body.empty()) throw std::runtime_error("Please upload a CSV file first");
        std::istringstream csvStream(request.body);
        stock::CsvStockRepository repo;
        stock::MovingAverageModel model;
        const auto result = model.analyze(repo.load(csvStream), static_cast<std::size_t>(window), days);
        sendResponse(client, 200, "application/json; charset=utf-8", stock::toJson(result));
        return;
    }
    sendResponse(client, 405, "text/plain; charset=utf-8", "Method not allowed");
}
} // namespace

int main(int argc, char* argv[]) {
    const std::filesystem::path root = argc >= 2 ? argv[1] : ".";
    const int port = argc >= 3 ? std::atoi(argv[2]) : 8080;
    if (port < 1024 || port > 65535) {
        std::cerr << "Choose a port between 1024 and 65535\n";
        return 1;
    }
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "Winsock initialization failed\n";
        return 1;
    }
#endif
    Socket listening = socket(AF_INET, SOCK_STREAM, 0);
    if (listening == INVALID_SOCKET) {
        std::cerr << "Could not create server socket\n";
        return 1;
    }
    int reuse = 1;
    setsockopt(listening, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&reuse), sizeof(reuse));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<unsigned short>(port));
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // Only this PC can connect.
    if (bind(listening, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR ||
        listen(listening, 10) == SOCKET_ERROR) {
        std::cerr << "Cannot listen on 127.0.0.1:" << port << " (port already in use?)\n";
        closeSocket(listening);
        return 1;
    }
    std::cout << "C++ stock prediction website running at http://127.0.0.1:"
              << port << "\nOpen it in your browser. Press Ctrl+C to stop.\n" << std::flush;
    while (true) {
        Socket client = accept(listening, nullptr, nullptr);
        if (client == INVALID_SOCKET) continue;
        try {
            const auto request = receiveRequest(client);
            try {
                route(client, request, root);
            } catch (const std::exception& error) {
                sendResponse(client, 400, "application/json; charset=utf-8",
                             "{\"error\":\"" + stock::jsonEscape(error.what()) + "\"}");
            }
        } catch (const std::exception& error) {
            const std::string message = error.what();
            const int code = message.find("2 MiB") != std::string::npos ||
                             message.find("too large") != std::string::npos ? 413 : 400;
            sendResponse(client, code, "application/json; charset=utf-8",
                         "{\"error\":\"" + stock::jsonEscape(message) + "\"}");
        }
        closeSocket(client);
    }
}
