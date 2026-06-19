#pragma once

#include <iostream>
#include <string>
#include <unordered_map>
#include "platform.hpp"

using namespace std;

// ─── HTTP Response ────────────────────────────────────────────────────────────
class HttpResponse {
public:
    int status_code = 200;
    string status_text = "OK";
    unordered_map<string, string> headers;
    string body;

    // Fluent setters
    HttpResponse& set_status(int code, const string& text);
    HttpResponse& set_header(const string& key, const string& value);
    HttpResponse& set_body(const string& content,
                           const string& content_type = "text/plain; charset=utf-8");
    HttpResponse& set_json(const string& json);
    HttpResponse& set_html(const string& html);

    // Serialize to wire format and send over socket.
    void send(socket_t fd) const;
    string to_string() const;

    // Convenience factory methods
    static HttpResponse ok(const string& body = "");
    static HttpResponse not_found();
    static HttpResponse internal_error();
};
