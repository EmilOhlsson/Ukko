#pragma once

#include <cassert>
#include <fmt/ostream.h>
#include <fmt/std.h>
#include <functional>
#include <map>
#include <random>
#include <thread>

#include "common.hpp"
#include "settings.hpp"

extern "C" {
#include <microhttpd.h>
#include <netdb.h>
#include <sys/socket.h>
}

std::ostream &operator<<(std::ostream &ostream, MHD_Result result) {
    switch (result) {
        case MHD_Result::MHD_YES:
            return ostream << "YES";
        case MHD_Result::MHD_NO:
            return ostream << "NO";
    }
    return ostream << "(unkown result)";
}
template <> struct fmt::formatter<MHD_Result> : ostream_formatter {};

std::ostream &operator<<(std::ostream &ostream, MHD_RequestTerminationCode code) {
    switch (code) {
        case MHD_RequestTerminationCode::MHD_REQUEST_TERMINATED_CLIENT_ABORT:
            return ostream << "CLIENT_ABORT";
        case MHD_RequestTerminationCode::MHD_REQUEST_TERMINATED_COMPLETED_OK:
            return ostream << "COMPLETED_OK";
        case MHD_RequestTerminationCode::MHD_REQUEST_TERMINATED_DAEMON_SHUTDOWN:
            return ostream << "DEAMON_SHUTDOWN";
        case MHD_RequestTerminationCode::MHD_REQUEST_TERMINATED_READ_ERROR:
            return ostream << "READ_ERROR";
        case MHD_RequestTerminationCode::MHD_REQUEST_TERMINATED_WITH_ERROR:
            return ostream << "WITH_ERROR";
        case MHD_RequestTerminationCode::MHD_REQUEST_TERMINATED_TIMEOUT_REACHED:
            return ostream << "TIMEOUT_REACHED";
    }
    return ostream << "(unkown code)";
}
template <> struct fmt::formatter<MHD_RequestTerminationCode> : ostream_formatter {};

std::ostream &operator<<(std::ostream &ostream, MHD_ValueKind value_kind) {
    bool first{true};
    for (const auto vkind : {MHD_ValueKind::MHD_COOKIE_KIND, MHD_ValueKind::MHD_FOOTER_KIND,
                             MHD_ValueKind::MHD_GET_ARGUMENT_KIND, MHD_ValueKind::MHD_HEADER_KIND,
                             MHD_ValueKind::MHD_POSTDATA_KIND}) {
        if ((vkind & value_kind) != 0) {
            if (not first) {
                ostream << "|";
            }
            first = false;
            switch (vkind) {
                case MHD_ValueKind::MHD_COOKIE_KIND:
                    ostream << "COOKIE";
                    break;
                case MHD_ValueKind::MHD_FOOTER_KIND:
                    ostream << "FOOTER";
                    break;
                case MHD_ValueKind::MHD_GET_ARGUMENT_KIND:
                    ostream << "GET_ARGUMENT";
                    break;
                case MHD_ValueKind::MHD_HEADER_KIND:
                    ostream << "HEADER";
                    break;
                case MHD_ValueKind::MHD_POSTDATA_KIND:
                    ostream << "POSTDATA";
                    break;

                default:
                    abort();
            }
        }
    }
    return ostream;
}
template <> struct fmt::formatter<MHD_ValueKind> : ostream_formatter {};

struct WebServer final {
    WebServer() = delete;
    WebServer(const WebServer &) = default;
    WebServer(WebServer &&) = default;
    WebServer(const Settings &settings, std::function<void(const Auth &)> on_auth_code_cb)
        : log{settings.get_logger(Logger::Facility::WebServer)}
        , debug{settings.get_logger(Logger::Facility::WebServer)}
        , settings{settings}
        , on_auth_code{on_auth_code_cb} {
    }

    WebServer &operator=(const WebServer &) = default;
    WebServer &operator=(WebServer &&) = default;

    ~WebServer() {
        if (daemon != nullptr) {
            MHD_stop_daemon(daemon);
        }
    }

    auto start() -> void {
        debug("Starting web server from thread: {}", std::this_thread::get_id());
        daemon = MHD_start_daemon(MHD_USE_AUTO_INTERNAL_THREAD, port, accept_policy_handler, this,
                                  answer_to_connection, this, MHD_OPTION_NOTIFY_COMPLETED,
                                  request_completed, this, MHD_OPTION_END);
    }

  private:
    enum class Method {
        GET,
        POST,
    };
    friend std::ostream &operator<<(std::ostream &ostream, WebServer::Method method);

    struct Connection {
        Connection() = delete;
        Connection(const Connection &) = delete;
        Connection(Connection &&) = delete;
        Connection &operator=(const Connection &) = delete;
        Connection &operator=(Connection &&) = delete;
        Connection(Method meth, WebServer *webs, MHD_Connection *connection)
            : connection{connection}
            , method{meth}
            , webserver{webs}
            , debug{webserver->settings.get_debug_logger(Logger::Facility::WebConnection)} {
            if (method == Method::POST) {
                debug("Attempting to create a post processor");
                postprocessor =
                    MHD_create_post_processor(connection, post_buffer_size, iterate_post, this);
                if (postprocessor == nullptr) {
                    debug("There was some kind of error creating Post processor");
                } else {
                    debug("Created a post processor");
                }
            }
        }

        ~Connection() {
            debug("Deleting connection");
            if (postprocessor != nullptr) {
                MHD_destroy_post_processor(postprocessor);
            }
        }

        /** Helper class for post processing parameters */
        struct PostDataInfo {
            std::optional<std::string_view> filename{};
            std::optional<std::string_view> content_type{};
            std::optional<std::string_view> encoding{};

          private:
            friend std::ostream &operator<<(std::ostream &ostream, const PostDataInfo &info) {
                ostream << "[filename=" << (info.filename ? *info.filename : "(none)");
                ostream << ",content_type=" << (info.content_type ? *info.content_type : "(none)");
                ostream << ",encoding=" << (info.encoding ? *info.encoding : "(none)") << "]";
                return ostream;
            }
        };

        template <typename T = const char>
        auto send(std::span<T> data, uint32_t code = MHD_HTTP_OK,
                  const std::map<std::string, std::string> &headers = {}) const -> MHD_Result {
            debug("Sending data");
            auto bytes{std::as_bytes(data)};
            MHD_Response *response = MHD_create_response_from_buffer(
                bytes.size(), (void *) bytes.data(), MHD_RESPMEM_MUST_COPY);

            /* Add headers */
            std::for_each(headers.begin(), headers.end(),
                          [&](const std::pair<std::string, std::string> &header) {
                              MHD_add_response_header(response, header.first.c_str(),
                                                      header.second.c_str());
                          });
            MHD_Result ret = MHD_queue_response(connection, code, response);
            MHD_destroy_response(response);
            return ret;
        }

        auto send(std::string_view data, uint32_t code = MHD_HTTP_OK,
                  const std::map<std::string, std::string> &headers = {}) const -> MHD_Result {
            return send(std::span<const char>{data.begin(), data.end()}, code, headers);
        }

        auto post_process(std::string_view input) const -> MHD_Result {
            debug("Post processing");
            /* This will trigger calls to iterate post */
            return MHD_post_process(postprocessor, input.begin(), input.size());
        }

        // NOLINTBEGIN: C callback
        static auto iterate_post(void *cls, MHD_ValueKind kind, const char *key,
                                 const char *filename, const char *content_type,
                                 const char *transfer_encoding, const char *data, uint64_t offset,
                                 size_t size) -> MHD_Result {
            auto *conn{static_cast<Connection *>(cls)};
            auto as_optional_string_view = utils::ptr_as_optional<const char, std::string_view>;
            return conn->post_handler(kind, key,
                                      {
                                          .filename = as_optional_string_view(filename),
                                          .content_type = as_optional_string_view(content_type),
                                          .encoding = as_optional_string_view(transfer_encoding),
                                      },
                                      {data, size}, offset);
        }
        // NOLINTEND

        auto post_handler(MHD_ValueKind kind, std::string_view key, const PostDataInfo &info,
                          std::string_view data, uint64_t offset) -> MHD_Result {
            // IF something:
            debug("Post handler: kind={}, key={}, data={}, info={}, offset={}", kind, key, data,
                  info, offset);
            done = true;
            return MHD_Result::MHD_YES;
        }

        auto resolve_values() -> int {
            if (resolved_values) {
                return *resolved_values;
            }
            auto all{std::numeric_limits<std::underlying_type_t<MHD_ValueKind>>::max()};
            resolved_values = MHD_get_connection_values(connection, static_cast<MHD_ValueKind>(all),
                                                        value_iterator, this);
            return *resolved_values;
        }

        // NOLINTBEGIN: C callback
        static auto value_iterator(void *cls, MHD_ValueKind value_kind, const char *key,
                                   const char *value) -> MHD_Result {
            auto *connection{static_cast<Connection *>(cls)};
            connection->handle_value(value_kind, {key, value});
            return MHD_Result::MHD_YES;
        }
        // NOLINTEND

        auto handle_value(MHD_ValueKind value_kind, std::pair<std::string, std::string> key_value)
            -> void {
            auto [key, value] = key_value;
            /* These messages are plenty, so want to use separate facility */
            auto dump = webserver->settings.get_debug_logger(Logger::Facility::WebConnectionDump);
            dump("{} {}: {}", value_kind, key, value);
            parameters[value_kind].insert(key_value);
        }

        auto lookup_value(MHD_ValueKind kind, const std::string &key) const
            -> std::optional<std::string> {
            if (auto kv_storage = parameters.find(kind); kv_storage != parameters.end()) {
                const auto &container = kv_storage->second;
                if (auto value = container.find(key); value != container.end()) {
                    return value->second;
                }
            }
            return std::nullopt;
        }

        MHD_Connection *connection{};
        Method method{};
        MHD_PostProcessor *postprocessor{};
        WebServer *webserver{};
        bool done{false};
        std::optional<int> resolved_values{};
        Logger debug;
        std::map<MHD_ValueKind, std::map<std::string, std::string>> parameters{};

        friend std::ostream &operator<<(std::ostream &ostream, const Connection &connection) {
            return ostream << "[" << connection.method << ", done=" << connection.done
                           << ", postprocessor="
                           << (connection.postprocessor != nullptr ? "yes" : "no") << "]";
        }
    };

    static auto accept_policy_handler(void *cls, const sockaddr *addr, socklen_t addrlen)
        -> MHD_Result {
        std::array<char, INET6_ADDRSTRLEN> name{};
        std::array<char, sizeof("65536")> port{};
        getnameinfo(addr, addrlen, name.data(), sizeof(name), port.data(), sizeof(port),
                    NI_NUMERICHOST | NI_NUMERICSERV);
        auto *self{static_cast<WebServer *>(cls)};
        self->debug("Got connection from {}:{}", name.data(), port.data());
        return MHD_Result::MHD_YES;
    }

    static auto request_completed(void *cls, MHD_Connection * /* connection */, void **con_cls,
                                  MHD_RequestTerminationCode toe) -> void {
        auto *self{static_cast<WebServer *>(cls)};
        self->debug("Done with request: {}", toe);
        if (*con_cls == nullptr) {
            return;
        }
        delete static_cast<Connection *>(*con_cls); // NOLINT: Too few places to care
        *con_cls = nullptr;
    }

    static auto string2method(std::string_view method) -> std::optional<Method> {
        if (method == "GET") {
            return Method::GET;
        }
        if (method == "POST") {
            return Method::POST;
        }
        return std::nullopt;
    }

    /**
     * Handles incoming connection and passes call to C++ context
     */
    /* NOLINTBEGIN: Signature must match what libmicrohttpd expects */
    static auto answer_to_connection(void *cls, MHD_Connection *connection, const char *url_cstr,
                                     const char *method_cstr, const char * /*version_cstr */,
                                     const char *upload_data_cstr, size_t *upload_data_size,
                                     void **con_cls) -> MHD_Result {
        auto *self = static_cast<WebServer *>(cls);
        auto method = string2method(method_cstr);
        if (not method) {
            self->log("Didn't get a proper HTTP command verb: {}", method_cstr);
            return MHD_Result::MHD_NO;
        }

        if (upload_data_cstr != nullptr) {
            std::string_view data{upload_data_cstr, *upload_data_size};
        }
        if (*con_cls == nullptr) {
            *con_cls = new Connection{*method, self, connection};
        }

        return self->handle_connection(*static_cast<Connection *>(*con_cls), url_cstr, *method,
                                       upload_data_cstr, upload_data_size);
    }
    /* NOLINTEND */

    auto handle_connection(Connection &connection, std::string_view url, Method method,
                           const char *upload_data, size_t *upload_size) -> MHD_Result {
        debug("Handling incoming {} {}. size={}", method, url, *upload_size);
        connection.resolve_values();
        if (method == Method::GET) {
            if (url == "/") {
                auto hostname = connection.lookup_value(MHD_ValueKind::MHD_HEADER_KIND, "Host");
                if (not hostname) {
                    return connection.send(error);
                }
                if (auto code =
                        connection.lookup_value(MHD_ValueKind::MHD_GET_ARGUMENT_KIND, "code")) {
                    debug("Got authentication code: {}", *code);
                    on_auth_code({
                        .code = *code,
                        .redirect = std::format("http://{}", *hostname),
                    });
                }
                /* Note: We are just randomizing state, but we do not track it */
                std::string page{
                    fmt::format(index, fmt::arg("client_id", settings.netatmo.client_id),
                                fmt::arg("auth_server", settings.auth_server),
                                fmt::arg("state", fmt::format("{:x}", rand(generator))),
                                fmt::arg("redirect_addr", *hostname))};
                return connection.send(page);
            } else if (url == "/oauth2/authorize") {
                auto redirect_dest{
                    connection.lookup_value(MHD_ValueKind::MHD_GET_ARGUMENT_KIND, "redirect_uri")};
                auto state{connection.lookup_value(MHD_ValueKind::MHD_GET_ARGUMENT_KIND, "state")};
                if (not redirect_dest || not state) {
                    return connection.send<const char>(error, MHD_HTTP_BAD_REQUEST);
                }
                std::string page{fmt::format(
                    redirect, fmt::arg("redirect", *redirect_dest),
                    fmt::arg("params", fmt::format("code={}&state={}", "qwerty", *state)))};
                return connection.send(page, MHD_HTTP_MOVED_PERMANENTLY);
            } else if (url == "/api/getstationsdata") {
                return connection.send(sample_device_data, MHD_HTTP_OK);
            } else {
                auto [code, page] = lookup_page(url);
                return connection.send(page, code);
            }
        }
        if (method == Method::POST) {
            if (*upload_size != 0) {
                debug("Attempting post processing using {}", connection);
                MHD_Result result = connection.post_process({upload_data, *upload_size});
                *upload_size = 0;
                debug("Post process result: {}", result);
                return result;
            } else if (connection.done) {
                debug("Send some form of result");
                if (url == "/oauth2/token") {
                    return connection.send(R"json(
                        {
                            "access_token":"2YotnFZFEjr1zCsicMWpAA",
                            "expires_in":10800,
                            "refresh_token":"tGzv3JOkF0XG5Qx2TlKWIA"
                        }
                    )json");
                }
                return connection.send("ABC");
            }
            return MHD_Result::MHD_YES;
        }
        return connection.send(error, MHD_HTTP_BAD_REQUEST);
    }

    auto lookup_page(std::string_view url) -> std::pair<int, std::string_view> {
        if (auto page = static_pages.find(url); page != static_pages.end()) {
            return {MHD_HTTP_OK, page->second};
        }
        return {MHD_HTTP_NOT_FOUND, not_found};
    }

    MHD_Daemon *daemon = nullptr;
    static constexpr uint16_t port = 8080;
    static constexpr size_t post_buffer_size = 65536;

    static constexpr std::string_view index = R"html(<!DOCTYPE html>
<html lang = "en">
    <head>
        <link rel="stylesheet" href="style.css">
    </head>
    <body>
        <h1>Authenticate with Netatmo</h1>
        <script src="script.js"></script>
        <form action="{auth_server}" method="GET">
                <input type="hidden" name="client_id" value="{client_id}" />
                <input type="hidden" name="redirect_uri" value="http://{redirect_addr}"/>
                <input type="hidden" name="scope" value="read_station"/>
                <input type="hidden" name="state" value="{state}"/>
                <button>Get code</button>
        </form>
    </body>
</html>)html";

    static constexpr std::string_view not_found = R"html( <!DOCTYPE html>
<html lang = "en">
    <head>
        <link rel="stylesheet" href="style.css">
    </head>
    <body>
        <h1>404</h1>
        <em id="greeting">Not found</em>
    </body>
</html>)html";

    static constexpr std::string_view error = R"html(<!DOCTYPE html>
<html lang="en">
    <head>
        <link rel="stylesheet" href="style.css">
    </head>
    <body>
        <h1>Broken :(</h1>
        <em id="greeting">kaputtski</em>
    </body>
</html>)html";

    static constexpr std::string_view redirect{R"html(<!DOCTYPE html>
<html lang="en">
    <head>
        <meta http-equiv="refresh" content="3; url='{redirect}?{params}'" />
    </head>
    <body>
        Almost there...
    </body>
</html>)html"};

    static constexpr std::string_view css = R"css(body {
    background-color: lightblue;
})css";

    static constexpr std::string_view javascript = R"js(console.log('Hello, world')
)js";

    static constexpr std::string_view sample_device_data = R"json({
    "body": {
        "devices": [
            {
                "_id": "70:ee:50:aa:bb:cc",
                "date_setup": 1606487524,
                "last_setup": 1606487524,
                "type": "NAMain",
                "last_status_store": 1692726789,
                "firmware": 201,
                "wifi_status": 45,
                "reachable": true,
                "co2_calibrating": false,
                "data_type": [
                    "Temperature",
                    "CO2",
                    "Humidity",
                    "Noise",
                    "Pressure"
                ],
                "place": {
                    "altitude": 33,
                    "city": "Lund",
                    "country": "SE",
                    "timezone": "Europe/Stockholm",
                    "location": [
                        15.000000,
                        55.000000
                    ]
                },
                "station_name": "House",
                "home_id": "abcdef",
                "home_name": "House",
                "dashboard_data": {
                    "time_utc": 1692726788,
                    "Temperature": 24.6,
                    "CO2": 490,
                    "Humidity": 54,
                    "Noise": 40,
                    "Pressure": 1017.3,
                    "AbsolutePressure": 1013.4,
                    "min_temp": 22.4,
                    "max_temp": 24.8,
                    "date_max_temp": 1692715592,
                    "date_min_temp": 1692678676,
                    "temp_trend": "stable",
                    "pressure_trend": "stable"
                },
                "modules": [
                    {
                        "_id": "02:00:00:aa:bb:cc",
                        "type": "NAModule1",
                        "module_name": "Outdoor",
                        "last_setup": 1606487525,
                        "data_type": [
                            "Temperature",
                            "Humidity"
                        ],
                        "battery_percent": 37,
                        "reachable": true,
                        "firmware": 53,
                        "last_message": 1692726785,
                        "last_seen": 1692726766,
                        "rf_status": 80,
                        "battery_vp": 4868,
                        "dashboard_data": {
                            "time_utc": 1692726766,
                            "Temperature": 21.6,
                            "Humidity": 64,
                            "min_temp": 14.9,
                            "max_temp": 23,
                            "date_max_temp": 1692721331,
                            "date_min_temp": 1692675035,
                            "temp_trend": "down"
                        }
                    },
                    {
                        "_id": "05:00:00:aa:bb:cc",
                        "type": "NAModule3",
                        "module_name": "Rain",
                        "last_setup": 1634305348,
                        "data_type": [
                            "Rain"
                        ],
                        "battery_percent": 66,
                        "reachable": true,
                        "firmware": 14,
                        "last_message": 1692726785,
                        "last_seen": 1692726785,
                        "rf_status": 73,
                        "battery_vp": 5246,
                        "dashboard_data": {
                            "time_utc": 1692726785,
                            "Rain": 0,
                            "sum_rain_1": 0,
                            "sum_rain_24": 0
                        }
                    }
                ]
            }
        ],
        "user": {
            "mail": "foo@example.com",
            "administrative": {
                "lang": "en",
                "reg_locale": "en-US",
                "country": "SE",
                "unit": 0,
                "windunit": 2,
                "pressureunit": 0,
                "feel_like_algo": 0
            }
        }
    },
    "status": "ok",
    "time_exec": 0.059020042419433594,
    "time_server": 1692726890
}
)json";

    std::map<std::string_view, std::string_view> static_pages{
        {"/style.css", css},
        {"/script.js", javascript},
    };

    Logger log;
    Logger debug;
    Settings settings;

    std::function<void(const Auth &)> on_auth_code;
    std::random_device random_device;
    std::mt19937 generator{random_device()};
    std::uniform_int_distribution<uint64_t> rand{};
};

inline std::ostream &operator<<(std::ostream &ostream, WebServer::Method method) {
    switch (method) {
        case WebServer::Method::GET:
            return ostream << "GET";

        case WebServer::Method::POST:
            return ostream << "POST";
    }
    return ostream << "(unknown method)";
}

template <> struct fmt::formatter<WebServer::Method> : ostream_formatter {};
template <> struct fmt::formatter<WebServer::Connection> : fmt::ostream_formatter {};
template <> struct fmt::formatter<WebServer::Connection::PostDataInfo> : ostream_formatter {};
