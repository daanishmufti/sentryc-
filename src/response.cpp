#include "response.hpp"

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace std;

HttpResponse& HttpResponse::set_status(int code, const string& text) {
    status_code = code;
    status_text = text;
    return *this;
}

HttpResponse& HttpResponse::set_header(const string& key, const string& value) {
    headers[key] = value;
    return *this;
}

HttpResponse& HttpResponse::set_body(const string& content, const string& content_type) {
    body = content;
    headers["Content-Type"]   = content_type;
    headers["Content-Length"] = std::to_string(content.size());
    return *this;
}

HttpResponse& HttpResponse::set_json(const string& json) {
    return set_body(json, "application/json; charset=utf-8");
}

HttpResponse& HttpResponse::set_html(const string& html) {
    return set_body(html, "text/html; charset=utf-8");
}

HttpResponse HttpResponse::ok(const string& body) {
    HttpResponse r;
    r.set_status(200, "OK");
    if (!body.empty()) r.set_body(body);
    return r;
}

HttpResponse HttpResponse::not_found() {
    HttpResponse r;
    r.set_status(404, "Not Found");
    r.set_html("<h1>404 Not Found</h1>");
    return r;
}

HttpResponse HttpResponse::internal_error() {
    HttpResponse r;
    r.set_status(500, "Internal Server Error");
    r.set_html("<h1>500 Internal Server Error</h1>");
    return r;
}

string HttpResponse::to_string() const {
    ostringstream oss;
    oss << "HTTP/1.1 " << status_code << " " << status_text << "\r\n";
    oss << "Connection: close\r\n";
    for (const auto& kv : headers) {
        oss << kv.first << ": " << kv.second << "\r\n";
    }
    oss << "\r\n";
    oss << body;
    return oss.str();
}

void HttpResponse::send(socket_t fd) const {
    string data = to_string();
    const char* ptr  = data.data();
    int remaining = static_cast<int>(data.size());
    while (remaining > 0) {
        int sent = ::send(fd, ptr, remaining, 0);
        if (sent <= 0) break;
        ptr       += sent;
        remaining -= sent;
    }
}
