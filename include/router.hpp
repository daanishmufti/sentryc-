#pragma once

#include <iostream>
#include <functional>
#include <string>
#include <unordered_map>
#include "request.hpp"
#include "response.hpp"

using namespace std;

using Handler = std::function<HttpResponse(const HttpRequest&)>;
// ─── Router ───────────────────────────────────────────────────────────────────
// Maps (METHOD, path) pairs to handler functions.
class Router {
public:
    Router& get(const string& path, Handler handler);
    Router& post(const string& path, Handler handler);

    // Dispatch a parsed request to the matching handler.
    // Returns 404 if no route matches.
    HttpResponse dispatch(const HttpRequest& req) const;

private:
    unordered_map<string, Handler> routes_;

    static string make_key(const string& method, const string& path);
};
