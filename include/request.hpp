#pragma once

#include <iostream>
#include <string>
#include <unordered_map>
using namespace std;

// ─── HTTP Request ─────────────────────────────────────────────────────────────
struct HttpRequest {
    string method;                                  // "GET", "POST", …
    string path;                                    // "/hello"
    string query;                                   // "foo=bar&baz=1"
    unordered_map<string, string> headers;
    string body;
};

// Parse a raw HTTP/1.1 request string into an HttpRequest.
HttpRequest parse_request(const string& raw);
