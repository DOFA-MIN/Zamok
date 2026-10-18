#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN

#include "http_server.h"
#include "config.h"
#include "locker.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <map>

#pragma comment(lib, "ws2_32.lib")

static std::string wstring_to_utf8(const std::wstring& wstr) {
    std::string result;
    result.reserve(wstr.size());
    for (wchar_t wc : wstr) {
        if (wc < 0x80) {
            result += (char)wc;
        } else if (wc < 0x800) {
            result += (char)(0xC0 | (wc >> 6));
            result += (char)(0x80 | (wc & 0x3F));
        } else {
            result += (char)(0xE0 | (wc >> 12));
            result += (char)(0x80 | ((wc >> 6) & 0x3F));
            result += (char)(0x80 | (wc & 0x3F));
        }
    }
    return result;
}

static std::wstring utf8_to_wstring(const std::string& str) {
    std::wstring result;
    result.reserve(str.size());
    for (size_t i = 0; i < str.size(); i++) {
        unsigned char c = (unsigned char)str[i];
        if (c < 0x80) {
            result += (wchar_t)c;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < str.size()) {
            result += (wchar_t)(((c & 0x1F) << 6) | (str[i + 1] & 0x3F));
            i++;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < str.size()) {
            result += (wchar_t)(((c & 0x0F) << 12) | ((str[i + 1] & 0x3F) << 6) | (str[i + 2] & 0x3F));
            i += 2;
        } else {
            result += (wchar_t)c;
        }
    }
    return result;
}

static std::string json_escape(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out += c; break;
        }
    }
    return out;
}

static std::string url_decode(const std::string& str) {
    std::string out;
    for (size_t i = 0; i < str.size(); i++) {
        if (str[i] == '%' && i + 2 < str.size()) {
            int hex = 0;
            sscanf_s(str.substr(i + 1, 2).c_str(), "%2x", &hex);
            out += (char)hex;
            i += 2;
        } else if (str[i] == '+') {
            out += ' ';
        } else {
            out += str[i];
        }
    }
    return out;
}

static std::string http_status_line(int code) {
    switch (code) {
    case 200: return "HTTP/1.1 200 OK\r\n";
    case 400: return "HTTP/1.1 400 Bad Request\r\n";
    case 404: return "HTTP/1.1 404 Not Found\r\n";
    case 500: return "HTTP/1.1 500 Internal Server Error\r\n";
    default: return "HTTP/1.1 200 OK\r\n";
    }
}

static void send_response(SOCKET client, int code, const std::string& body) {
    std::string headers = http_status_line(code);
    headers += "Content-Type: application/json\r\n";
    headers += "Access-Control-Allow-Origin: *\r\n";
    headers += "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    headers += "Access-Control-Allow-Headers: Content-Type\r\n";
    headers += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    headers += "Connection: close\r\n";
    headers += "\r\n";
    std::string response = headers + body;
    send(client, response.c_str(), (int)response.size(), 0);
}

// Minimal JSON value extraction for our use cases
static std::string json_get_string(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    size_t colon = json.find(':', pos + search.size());
    if (colon == std::string::npos) return "";
    size_t quote = json.find('"', colon + 1);
    if (quote == std::string::npos) return "";
    size_t end_quote = json.find('"', quote + 1);
    if (end_quote == std::string::npos) return "";
    return json.substr(quote + 1, end_quote - quote - 1);
}

static std::string make_json_response(bool success, const std::string& error_or_data, bool is_data_array) {
    if (is_data_array) {
        return "{\"success\":" + std::string(success ? "true" : "false") + ",\"items\":" + error_or_data + "}";
    }
    if (!error_or_data.empty()) {
        return "{\"success\":" + std::string(success ? "true" : "false") + ",\"" + (success ? "data" : "error") + "\":\"" + json_escape(error_or_data) + "\"}";
    }
    return "{\"success\":" + std::string(success ? "true" : "false") + "}";
}

static void handle_request(SOCKET client, const std::string& method, const std::string& path, const std::string& body) {
    locker_init_db();

    if (method == "OPTIONS") {
        send_response(client, 200, "");
        return;
    }

    if (path == "/api/status" && method == "GET") {
        send_response(client, 200, make_json_response(true, "running", false));
        return;
    }

    if (path == "/api/locked" && method == "GET") {
        auto items = locker_list_locked();
        std::string json = "[";
        for (size_t i = 0; i < items.size(); i++) {
            if (i > 0) json += ",";
            json += "{";
            json += "\"id\":\"" + json_escape(wstring_to_utf8(items[i].id)) + "\",";
            json += "\"original_path\":\"" + json_escape(wstring_to_utf8(items[i].original_path)) + "\",";
            json += "\"vault_path\":\"" + json_escape(wstring_to_utf8(items[i].vault_path)) + "\",";
            json += "\"created_at\":\"" + json_escape(wstring_to_utf8(items[i].created_at)) + "\"";
            json += "}";
        }
        json += "]";
        send_response(client, 200, make_json_response(true, json, true));
        return;
    }

    if (path == "/api/lock" && method == "POST") {
        std::string folder_path = json_get_string(body, "path");
        std::string password = json_get_string(body, "password");
        std::wstring wpath = utf8_to_wstring(folder_path);
        std::wstring wpass = utf8_to_wstring(password);

        std::wstring id;
        if (locker_lock_folder(wpath, wpass, id)) {
            send_response(client, 200, make_json_response(true, wstring_to_utf8(id), false));
        } else {
            send_response(client, 400, make_json_response(false, "Failed to lock folder", false));
        }
        return;
    }

    if (path == "/api/unlock" && method == "POST") {
        std::string id = json_get_string(body, "id");
        std::string path_param = json_get_string(body, "path");
        std::string password = json_get_string(body, "password");
        std::wstring wid = utf8_to_wstring(id);
        std::wstring wpath = utf8_to_wstring(path_param);
        std::wstring wpass = utf8_to_wstring(password);

        std::wstring target = wid.empty() ? wpath : wid;
        if (locker_unlock_folder(target, wpass)) {
            send_response(client, 200, make_json_response(true, "", false));
        } else {
            send_response(client, 400, make_json_response(false, "Failed to unlock folder", false));
        }
        return;
    }

    send_response(client, 404, make_json_response(false, "Not found", false));
}

static std::string read_request_body(SOCKET client, int content_length) {
    std::string body;
    body.resize(content_length);
    int total = 0;
    while (total < content_length) {
        int n = recv(client, &body[total], content_length - total, 0);
        if (n <= 0) break;
        total += n;
    }
    body.resize(total);
    return body;
}

void http_server_run(int port, HANDLE stop_event) {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    if (bind(listen_sock, (sockaddr*)&addr, sizeof(addr)) != 0) {
        closesocket(listen_sock);
        WSACleanup();
        return;
    }

    listen(listen_sock, SOMAXCONN);
    u_long mode = 1;
    ioctlsocket(listen_sock, FIONBIO, &mode);

    while (WaitForSingleObject(stop_event, 100) == WAIT_TIMEOUT) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(listen_sock, &readfds);
        timeval tv = {0, 100000}; // 100ms
        int ret = select(0, &readfds, NULL, NULL, &tv);
        if (ret <= 0) continue;

        SOCKET client = accept(listen_sock, NULL, NULL);
        if (client == INVALID_SOCKET) continue;

        u_long client_mode = 1;
        ioctlsocket(client, FIONBIO, &client_mode);

        // Read request
        std::string request;
        char buf[4096];
        bool got_headers = false;
        int content_length = 0;

        while (!got_headers) {
            int n = recv(client, buf, sizeof(buf) - 1, 0);
            if (n <= 0) break;
            buf[n] = 0;
            request += buf;
            size_t header_end = request.find("\r\n\r\n");
            if (header_end != std::string::npos) {
                got_headers = true;
                std::string headers = request.substr(0, header_end);
                size_t cl_pos = headers.find("Content-Length: ");
                if (cl_pos != std::string::npos) {
                    std::string cl_str = headers.substr(cl_pos + 16);
                    cl_str = cl_str.substr(0, cl_str.find("\r\n"));
                    content_length = std::stoi(cl_str);
                }
            }
        }

        if (!got_headers) { closesocket(client); continue; }

        // Parse request line
        size_t first_space = request.find(' ');
        size_t second_space = request.find(' ', first_space + 1);
        std::string method = request.substr(0, first_space);
        std::string full_path = request.substr(first_space + 1, second_space - first_space - 1);

        // Strip query string
        std::string path = full_path;
        size_t qmark = path.find('?');
        if (qmark != std::string::npos) path = path.substr(0, qmark);

        // Read body if needed
        std::string body;
        if (content_length > 0) {
            // Already read some body data after headers
            size_t header_end = request.find("\r\n\r\n") + 4;
            if (header_end < request.size()) {
                body = request.substr(header_end);
            }
            if ((int)body.size() < content_length) {
                body += read_request_body(client, content_length - (int)body.size());
            }
        }

        handle_request(client, method, path, body);
        closesocket(client);
    }

    closesocket(listen_sock);
    WSACleanup();
}
