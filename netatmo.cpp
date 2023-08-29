#include <assert.h>
#include <curl/curl.h>
#include <fmt/core.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>

#include "common.hpp"
#include "web.hpp"

#include "netatmo.hpp"
#include "nlohmann/json.hpp"
#include "settings.hpp"

using json = nlohmann::json;

Weather::Weather(const Settings &settings)
    : settings(settings), load_file(settings.netatmo_load), store_file(settings.netatmo_store) {
}

/**
 * Takes a sleep time, and returns the shortest of the given time, and the time until next
 * refresh
 */
std::chrono::minutes Weather::check_sleep(std::chrono::minutes sleep_time) {
    // TODO: Perhaps safeguard against missint expiration time, and have threshold configurable
    using namespace std::literals::chrono_literals;
    std::chrono::time_point<std::chrono::system_clock> expiration = expiration_time - 10min;
    std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();

    std::chrono::system_clock::duration until_expiration = expiration - now;

    return std::min<std::chrono::minutes>(
        std::chrono::duration_cast<std::chrono::minutes>(until_expiration), sleep_time);
}

std::optional<Weather::MeasuredData> Weather::retrieve() {
    debug("Attempting to retrieve Netatmo metrics. is_authenticated={}", is_authenticated);
    if (not is_authenticated) {
        return std::nullopt;
    }
    std::optional<json> maybe_device_data{fetch_device_data()};
    if (not maybe_device_data) {
        debug("Was not able to fetch device data");
        return std::nullopt;
    }
    json device_data = *maybe_device_data;
    if (device_data.contains("error")) {
        debug("Got error from netatmo:\n{}", device_data.dump(2, ' '));
        return std::nullopt;
    }
    // debug("Got: {}\n", device_data.dump(2, ' '));
    //  TODO: Check for error message from server
    json device_list = device_data["body"]["devices"];
    if (device_list.size() != 1) {
        log("Invalid amount of devices, should be 1, got {}", device_list.size());
        return std::nullopt;
    }
    try {
        json device = device_list[0];

        json device_dashboard = device["dashboard_data"];

        MeasuredData md{
            .rain{},
            .outdoor{},
            .indoor{
                .now = device_dashboard["Temperature"].get<double>(),
                .min = device_dashboard["min_temp"].get<double>(),
                .max = device_dashboard["max_temp"].get<double>(),
            },
            .position{
                .longitude = fmt::format("{}", device["place"]["location"][0].get<double>()),
                .latitude = fmt::format("{}", device["place"]["location"][1].get<double>()),
            },
        };
        log("Read temperature now {}, min {}, max {}", md.indoor.now, md.indoor.min, md.indoor.max);

        for (const json &module : device["modules"]) {
            std::string module_id = module["_id"].get<std::string>();
            log("Looking at module: {}", module_id);
            if (module_id == netatmo.modules.outdoor) {
                json dashboard = module["dashboard_data"];
                md.outdoor.now = dashboard["Temperature"].get<double>();
                md.outdoor.min = dashboard["min_temp"].get<double>();
                md.outdoor.max = dashboard["max_temp"].get<double>();
                log("Presenting temperature: {}",
                    module["dashboard_data"]["Temperature"].get<double>());
            } else if (module_id == netatmo.modules.rain) {
                json dashboard = module["dashboard_data"];
                md.rain.last_1h = dashboard["sum_rain_1"].get<double>();
                md.rain.last_24h = dashboard["sum_rain_24"].get<double>();
                log("Presenting rain: {}", module["dashboard_data"]["Rain"].get<double>());
            }
        }

        return md;
    } catch (const std::exception &e) {
        log("There was an error parsing weather data: {}", e.what());
        return std::nullopt;
    }
}

bool Weather::get_token(const PostParams &params) {
    debug("Retrieving authentication token");
    if (load_file) {
        log("Will not fetch authorization token, as data is loaded from file");
        is_authenticated = true;
        return true;
    }

    Curl curl{settings};
    Url url{std::string{settings.token_server}};
    auto [success, response] = curl.postfields(url, params);
    debug("Got response: {}", utils::as_string_view(response));
    if (not success) {
        log("Unable to POST authorization request");
        return false;
    }

    if (!json::accept(response)) {
        log("Authorization POST response is not a valid JSON");
        return false;
    }

    return process_authentication_result(json::parse(response));
}

/**
 * Authenticate against netatmo API
 */
bool Weather::authenticate(const Auth &auth) {
    debug("Authenticating against Netatmo using code=\"{}\", redirect=\"{}\"", auth.code,
          auth.redirect);
    return get_token(PostParams{
        {"grant_type", "authorization_code"},
        {"client_id", settings.netatmo.client_id},
        {"client_secret", settings.netatmo.client_secret},
        {"redirect_uri", auth.redirect},
        {"code", auth.code},
        {"scope", "read_station"},
    });
}

/**
 * Refresh the authentication token
 */
bool Weather::refresh_authentication() {
    debug("Refreshing Netatmo authentication");
    return get_token(PostParams{
        {"grant_type", "refresh_token"},
        {"client_id", settings.netatmo.client_id},
        {"client_secret", settings.netatmo.client_secret},
        {"refresh_token", refresh_token},
        {"scope", "read_station"},
    });
}

bool Weather::process_authentication_result(json auth) {
    debug("Processing authentication result from Netatmo");
    if (auth.find("error") != auth.end()) {
        is_authenticated = false;
        log("There was an error requesting auth token: {}", auth["error"]);
        debug("authentication response: {}", auth.dump(2, ' '));
        return false;
    }

    is_authenticated = true;
    access_token = auth["access_token"];
    refresh_token = auth["refresh_token"];
    std::chrono::seconds expires_in{static_cast<long>(auth["expires_in"])};

    expiration_time = std::chrono::system_clock::now() + expires_in;
    debug("We are now authenticated for netatmo");
    return true;
}

/**
 * Fetch device data from the `getstationsdata` API and return as JSON object
 */
std::optional<nlohmann::json> Weather::fetch_device_data() {
    debug("Fetching device data");
    if (load_file) {
        return load_data(*load_file);
    }

    assert(is_authenticated);

    Curl curl{settings};
    Url url{std::string{settings.station_addr}};
    url.add_param({"device_id", settings.netatmo.device_id});
    auto [success, response] =
        curl.get(url, HeaderParams{
                          {"Accept", "application/json"},
                          {"Authorization", fmt::format("Bearer {}", access_token)},
                      });

    if (!json::accept(response)) {
        log("Device data is not properly formatted JSON");
        debug("Got:\n{}", utils::dump_bytes_as_string(response));
        return std::nullopt;
    }

    json device_data = json::parse(response);
    if (store_file) {
        store_data(*store_file, device_data);
    }
    return device_data;
}

/**
 * Load json data from provided file, and increment file counter
 */
nlohmann::json Weather::load_data(const std::string &title) {
    std::string filename = fmt::format("{}-{}.json", title, load_index);
    if (access(filename.c_str(), R_OK) != 0) {
        throw std::runtime_error(
            fmt::format("Unable to open file {} for loading dummy netatmo data", filename));
    }
    std::ifstream input{filename};
    load_index += 1;
    json result;
    input >> result;
    return result;
}

/**
 * Store json data to provided file, and increment file counter
 */
void Weather::store_data(const std::string &filename, const json &data) {
    std::ofstream out{fmt::format("{}-{}.json", filename, store_index)};
    store_index += 1;
    out << std::setw(4) << data << std::endl;
}
