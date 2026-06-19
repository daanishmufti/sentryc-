#include "router.hpp"

#include <algorithm>
#include <stdexcept>
#include <typeinfo>

#include <sentry.h>

namespace {
// Report a handler exception to Sentry, capturing a stack trace at the catch
// site. The frames resolve to source in Sentry once debug-information files are
// uploaded (see the sentry-cli step in CMakeLists.txt).
void report_to_sentry(const char *type, const char *value) {
    sentry_value_t event = sentry_value_new_event();
    sentry_value_t exc   = sentry_value_new_exception(type, value);
    sentry_value_set_stacktrace(exc, nullptr, 0);  // nullptr => current stack
    sentry_event_add_exception(event, exc);
    sentry_capture_event(event);
}
}  // namespace

// ─── Route registration ───────────────────────────────────────────────────────

Router& Router::get(const std::string& path, Handler handler) {
    routes_[make_key("GET", path)] = std::move(handler);
    return *this;
}

Router& Router::post(const std::string& path, Handler handler) {
    routes_[make_key("POST", path)] = std::move(handler);
    return *this;
}

// ─── Dispatch ─────────────────────────────────────────────────────────────────

HttpResponse Router::dispatch(const HttpRequest& req) const {
    auto it = routes_.find(make_key(req.method, req.path));
    if (it == routes_.end()) {
        return HttpResponse::not_found();
    }

    try {
        return it->second(req);
    } catch (const std::exception& e) {
        report_to_sentry(typeid(e).name(), e.what());
        HttpResponse err = HttpResponse::internal_error();
        err.set_json(std::string("{\"error\":\"") + e.what() + "\"}");
        return err;
    } catch (...) {
        report_to_sentry("unknown_exception",
                         "non-std::exception thrown by handler");
        return HttpResponse::internal_error();
    }
}

// ─── Helpers ──────────────────────────────────────────────────────────────────

std::string Router::make_key(const std::string& method,
                              const std::string& path) {
    // Normalize: upper-case method + canonical path.
    std::string m = method;
    for (char& c : m) c = static_cast<char>(::toupper(static_cast<unsigned char>(c)));
    return m + " " + path;
}
