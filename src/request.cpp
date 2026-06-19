#include "request.hpp"

#include <iostream>

#include <algorithm>
#include <sstream>
#include <string>

using namespace std;

// Trim trailing whitespace / carriage returns from a line.
static string trim(string s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == ' ' || s.back() == '\t'))
        s.pop_back();
    return s;
}

// Trim leading whitespace.
static string ltrim(string s) {
    auto it = s.begin();
    while(it != s.end() && (*it == ' ' || *it == '\t'))
    {
        ++it;
    }
    return string(it, s.end());
}

HttpRequest parse_request(const string& raw) {
    HttpRequest req;

    // Locate the header/body boundary.
    const string separator = "\r\n\r\n";
    auto sep_pos = raw.find(separator);
    string header_section = (sep_pos != string::npos)
                                     ? raw.substr(0, sep_pos)
                                     : raw;

    istringstream stream(header_section);
    string line;

    // ── Request line ──────────────────────────────────────────────────────────
    if (getline(stream, line)) {
        line = trim(line);
        istringstream rl(line);
        string path_query, version;
        rl >> req.method >> path_query >> version;

        auto q = path_query.find('?');
        if (q != string::npos) {
            req.path  = path_query.substr(0, q);
            req.query = path_query.substr(q + 1);
        } else {
            req.path = path_query;
        }
    }

    // ── Headers ───────────────────────────────────────────────────────────────
    while (getline(stream, line)) {
        line = trim(line);
        if (line.empty()) break;

        auto colon = line.find(':');
        if (colon != string::npos) {
            string key = line.substr(0, colon);
            string val = ltrim(line.substr(colon + 1));
            req.headers[key] = val;
        }
    }

    // ── Body ──────────────────────────────────────────────────────────────────
    if (sep_pos != string::npos) {
        string body_raw = raw.substr(sep_pos + separator.size());

        auto it = req.headers.find("Content-Length");
        if (it != req.headers.end()) {
            try {
                size_t len = static_cast<size_t>(stoul(it->second));
                req.body = body_raw.substr(0, len);
            } catch (...) {
                req.body = body_raw;
            }
        } else {
            req.body = body_raw;
        }
    }

    return req;
}
