#include <iostream>
#include "router.hpp"
#include "server.hpp"
#include <fstream>
#include <sstream>
#include <atomic>
#include <sentry.h>

void sentry_func(void);

using namespace std;

static atomic<bool> toggle_state{false};

static string read_file(const string& path) {
    ifstream file(path);
    if (!file.is_open()) return "";
    ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

class SentryRouter : public Router {
public:
    SentryRouter& get(const string& path, Handler handler) {
        Router::get(path, wrap(handler));
        return *this;
    }
    SentryRouter& post(const string& path, Handler handler) {
        Router::post(path, wrap(handler));
        return *this;
    }
private:
    Handler wrap(Handler handler) {
        return [handler](const HttpRequest& req) -> HttpResponse {
            try {
                return handler(req);
            } catch (const std::exception& e) {
                sentry_value_t event = sentry_value_new_message_event(
                    SENTRY_LEVEL_ERROR,
                    "http_server",
                    e.what()
                );
                sentry_value_t st1 = sentry_value_new_stacktrace(nullptr, 0);
                sentry_value_t th1 = sentry_value_new_thread(1, "main");
                sentry_value_set_by_key(th1, "stacktrace", st1);
                sentry_event_add_thread(event, th1);
                sentry_capture_event(event);
                return HttpResponse{}.set_status(500, "Internal Server Error")
                                    .set_body("Internal Server Error: " + string(e.what()));
            } catch (...) {
                sentry_value_t event = sentry_value_new_message_event(
                    SENTRY_LEVEL_ERROR,
                    "http_server",
                    "Unknown exception caught in route handler"
                );
                sentry_value_t st2 = sentry_value_new_stacktrace(nullptr, 0);
                sentry_value_t th2 = sentry_value_new_thread(2, "main");
                sentry_value_set_by_key(th2, "stacktrace", st2);
                sentry_event_add_thread(event, th2);
                sentry_capture_event(event);
                return HttpResponse{}.set_status(500, "Internal Server Error")
                                    .set_body("Internal Server Error: Unknown Exception");
            }
        };
    }
};

int main() {
    sentry_func();   // start Sentry monitoring

    SentryRouter router;

    router.get("/", [](const HttpRequest&) {
        string html = read_file("static/index.html");
        if (html.empty())
            return HttpResponse{}.set_status(500, "Internal Server Error").set_body("Could not load index.html");
        return HttpResponse{}.set_html(html);
    });

    router.get("/hello", [](const HttpRequest& req) {
        string name = "World";
        auto q = req.query;
        auto pos = q.find("name=");
        if (pos != string::npos) {
            string val = q.substr(pos + 5);
            auto amp = val.find('&');
            if (amp != string::npos) val = val.substr(0, amp);
            name = val;
        }
        return HttpResponse{}.set_body("Hello, " + name + "!\n");
    });

    router.post("/echo", [](const HttpRequest& req) {
        string escaped = req.body;
        string out;
        for (char c : escaped) {
            if (c == '"') out += "\\\"";
            else if (c == '\\') out += "\\\\";
            else if (c == '\n') out += "\\n";
            else if (c == '\r') out += "\\r";
            else out += c;
        }
        string json =
            "{\n"
            "  \"method\": \"POST\",\n"
            "  \"path\": \"" + req.path + "\",\n"
            "  \"body\": \"" + out + "\"\n"
            "}";
        return HttpResponse{}.set_json(json);
    });

    router.get("/api/toggle", [](const HttpRequest&) {
        string state = toggle_state.load() ? "on" : "off";
        return HttpResponse{}.set_json("{\"state\": \"" + state + "\"}");
    });

    router.post("/api/toggle", [](const HttpRequest& req) {
        if (req.body.find("\"state\":\"on\"") != string::npos ||
            req.body.find("\"state\": \"on\"") != string::npos) {
            toggle_state.store(true);
        } else if (req.body.find("\"state\":\"off\"") != string::npos ||
                   req.body.find("\"state\": \"off\"") != string::npos) {
            toggle_state.store(false);
        } else {
            toggle_state.store(!toggle_state.load());
        }
        string state = toggle_state.load() ? "on" : "off";
        
        
        bool is_on = toggle_state.load();
        // Capture stack BEFORE building the sentry event so it reflects
        // the actual toggle handler context, not the Sentry SDK call site.
        sentry_value_t stacktrace = sentry_value_new_stacktrace(nullptr, 0);
        sentry_value_t event = sentry_value_new_event();
        sentry_value_t exc = sentry_value_new_exception(
            is_on ? "Tog on" : "Tog off",
            is_on ? "Toggle switched ON" : "Toggle switched OFF"
        );
        sentry_value_set_by_key(exc, "stacktrace", stacktrace);  // attach pre-captured stack
        sentry_event_add_exception(event, exc);
        sentry_capture_event(event);



        return HttpResponse{}.set_json("{\"state\": \"" + state + "\"}");
    });

    router.get("/sentry-error", [](const HttpRequest&) {
        sentry_value_t event = sentry_value_new_message_event(
            SENTRY_LEVEL_ERROR,
            "custom_logger",
            "This is a custom non-fatal error triggered from the browser!"
        );
        sentry_value_t st3 = sentry_value_new_stacktrace(nullptr, 0);
        sentry_value_t th3 = sentry_value_new_thread(3, "main");
        sentry_value_set_by_key(th3, "stacktrace", st3);
        sentry_event_add_thread(event, th3);
        sentry_capture_event(event);
        return HttpResponse{}.set_body("Non-fatal error reported to Sentry database!\n");
    });
    router.get("/sentry-crash", [](const HttpRequest&) {
        volatile int* ptr = nullptr;
        *ptr = 42; 
        return HttpResponse{}.set_body("This line will never be reached.\n");
    });

    router.get("/sentry-crash2", [](const HttpRequest&) -> HttpResponse {
        volatile int a = 42;
        volatile int b = 0;
        int c = a / b; // This will trigger EXCEPTION_INT_DIVIDE_BY_ZERO
        return HttpResponse{}.set_body("Result: " + std::to_string(c));
    });

    router.get("/sentry-exception", [](const HttpRequest&) -> HttpResponse {
        throw std::runtime_error("This is an unhandled C++ runtime exception thrown from a route handler!");
    });

    HttpServer server(8080, 4);
    server.use(router);
    server.run();

    sentry_close();  // flush and shut down Sentry
    return 0;
}
