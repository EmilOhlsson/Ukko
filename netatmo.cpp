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

#include "netatmo.hpp"
#include "nlohmann/json.hpp"
#include "settings.hpp"

namespace {
/**
 * Helper function for cURL. Append read data to a provided std::string
 */
size_t write_cb(void *data, size_t size, size_t nmemb, void *userp) {
    assert(size == 1);
    const char *bytes = static_cast<char *>(data);
    auto &storage = *static_cast<std::string *>(userp);
    std::copy_n(bytes, nmemb, std::back_inserter(storage));
    return nmemb;
}

int trace_function(CURL * /*handle*/, curl_infotype type, char *data, size_t size,
                   void * /*clientp*/) {
    switch (type) {
        case CURLINFO_TEXT:
            fmt::print("** TEXT\n");
            break;
        case CURLINFO_DATA_OUT:
            fmt::print("** DATA_OUT\n");
            fmt::print("{}\n\n", data);
            break;
        case CURLINFO_SSL_DATA_OUT:
            fmt::print("** SSL_DATA_OUT\n");
            break;
        case CURLINFO_HEADER_IN:
            fmt::print("** HEADER_IN\n");
            break;
        case CURLINFO_HEADER_OUT:
            fmt::print("** HEADER_OUT\n");
            fmt::print("{}\n\n", data);
            break;
        case CURLINFO_DATA_IN:
            fmt::print("** DATA_IN\n");
            break;
        case CURLINFO_SSL_DATA_IN:
            fmt::print("** SSL_DATA_IN\n");
            break;
        default:
            fmt::print("** unknown: {}\n", type);
            break;
    }
    for (size_t i = 0; i < size; i++) {
        if (i % 80 == 0 && i != 0)
            fprintf(stderr, "\n");
        fprintf(stderr, "%c", isgraph(data[i]) ? data[i] : '.');
    }
    fprintf(stderr, "\n");
    return 0;
}
} // namespace

Weather::Weather(const Options &options)
    : options(options), load_file(options.netatmo_load), store_file(options.netatmo_store) {
    std::ignore = authenticate();
}

/**
 * Return position reported by Netatmo
 */
std::optional<Position> Weather::get_position() const {
    if (has_position) {
        return Position{
            .longitude = longitude,
            .latitude = latitude,
        };
    } else {
        return std::nullopt;
    }
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
    json device_data = fetch_device_data();
    json device_list = device_data["body"]["devices"];
    if (device_list.size() != 1) {
        log("Invalid amount of devices, should be 1, got {}", device_list.size());
        return std::nullopt;
    }
    try {
        json device = device_list[0];

        longitude = fmt::format("{}", device["place"]["location"][0].get<double>());
        latitude = fmt::format("{}", device["place"]["location"][1].get<double>());
        has_position = true;

        json device_dashboard = device["dashboard_data"];

        MeasuredData md{};
        md.indoor.now = device_dashboard["Temperature"].get<double>();
        md.indoor.max = device_dashboard["max_temp"].get<double>();
        md.indoor.min = device_dashboard["min_temp"].get<double>();
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

/**
 * Authenticate against netatmo API
 */
bool Weather::authenticate() {
    debug("Authenticating against Netatmo");
    if (load_file) {
        log("Skipping authentication, as data is loaded from file");
        is_authenticated = true;
        return true;
    }

    CURL *curl = curl_easy_init();

    curl_mime *mime = curl_mime_init(curl);
    curl_mimepart *part;

    part = curl_mime_addpart(mime);
    curl_mime_data(part, netatmo.client_id.c_str(), CURL_ZERO_TERMINATED);
    curl_mime_name(part, "client_id");

    part = curl_mime_addpart(mime);
    curl_mime_data(part, netatmo.client_secret.c_str(), CURL_ZERO_TERMINATED);
    curl_mime_name(part, "client_secret");

    part = curl_mime_addpart(mime);
    curl_mime_data(part, netatmo.username.c_str(), CURL_ZERO_TERMINATED);
    curl_mime_name(part, "username");

    part = curl_mime_addpart(mime);
    curl_mime_data(part, netatmo.password.c_str(), CURL_ZERO_TERMINATED);
    curl_mime_name(part, "password");

    part = curl_mime_addpart(mime);
    curl_mime_data(part, "password", CURL_ZERO_TERMINATED);
    curl_mime_name(part, "grant_type");

    part = curl_mime_addpart(mime);
    curl_mime_data(part, "read_station", CURL_ZERO_TERMINATED);
    curl_mime_name(part, "scope");

    char errbuf[CURL_ERROR_SIZE]{};
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0l);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1l);

    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
    curl_easy_setopt(curl, CURLOPT_URL, "https://api.netatmo.com/oauth2/token");

    std::string response{};
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode result = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    curl_mime_free(mime);

    if (result != CURLE_OK) {
        log("Was not able to retrieve auth token. {}", response);
        return false;
    }

    if (!json::accept(response)) {
        log("Response is not a valid JSON");
        return false;
    }

    return process_authentication_result(json::parse(response));
}

/**
 * Refresh the authentication token
 */
bool Weather::refresh_authentication() {
    if (load_file) {
        log("Skipping authentication refresh, as data is loaded from file");
        is_authenticated = true;
        return true;
    }

    CURL *curl = curl_easy_init();

    curl_mime *mime = curl_mime_init(curl);
    curl_mimepart *part;

    part = curl_mime_addpart(mime);
    curl_mime_data(part, netatmo.client_id.c_str(), CURL_ZERO_TERMINATED);
    curl_mime_name(part, "client_id");

    part = curl_mime_addpart(mime);
    curl_mime_data(part, netatmo.client_secret.c_str(), CURL_ZERO_TERMINATED);
    curl_mime_name(part, "client_secret");

    part = curl_mime_addpart(mime);
    curl_mime_data(part, refresh_token.c_str(), CURL_ZERO_TERMINATED);
    curl_mime_name(part, "refresh_token");

    part = curl_mime_addpart(mime);
    curl_mime_data(part, "refresh_token", CURL_ZERO_TERMINATED);
    curl_mime_name(part, "grant_type");

    char errbuf[CURL_ERROR_SIZE]{};
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0l);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1l);

    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
    curl_easy_setopt(curl, CURLOPT_URL, "https://api.netatmo.com/oauth2/token");

    std::string response{};
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode result = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    curl_mime_free(mime);

    if (result != CURLE_OK) {
        log("Was not able to retrieve auth token. {}", response);
        return false;
    }

    if (!json::accept(response)) {
        log("Response is not a valid JSON");
        return false;
    }

    return process_authentication_result(json::parse(response));
}

bool Weather::process_authentication_result(json auth) {
    debug("Processing authentication result from Netatmo");
    if (auth.find("error") != auth.end()) {
        is_authenticated = false;
        log("There was an error requesting auth token: {}", auth["error"]);
        debug("authentication response: {}", auth.dump(2, ' '));
        return false;
    }

    access_token = auth["access_token"];
    refresh_token = auth["refresh_token"];
    std::chrono::seconds expires_in{static_cast<long>(auth["expires_in"])};

    expiration_time = std::chrono::system_clock::now() + expires_in;
    return true;
}

/**
 * Fetch device data from the `getstationsdata` API and return as JSON object
 */
nlohmann::json Weather::fetch_device_data() {
    debug("Fetching device data");
    if (load_file) {
        return load_data(*load_file);
    }

    assert(is_authenticated);

    CURL *curl = curl_easy_init();
    curl_slist *list = nullptr;

    char errbuf[CURL_ERROR_SIZE]{};
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0l);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1l);

    char *device_id = curl_easy_escape(curl, settings.netatmo.device_id.c_str(), 0);
    const std::string url = fmt::format(
        "https://api.netatmo.com/api/getstationsdata?"
        "device_id={}",
        device_id);
    curl_free(device_id);

    list = curl_slist_append(list, "Accept: application/json");
    std::string auth = fmt::format("Authorization: Bearer {}", access_token);
    list = curl_slist_append(list, auth.c_str());

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1l);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    std::string response{};
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_perform(curl);

    curl_slist_free_all(list);
    curl_easy_cleanup(curl);

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
void Weather::store_data(const std::string &filename, const json &j) {
    std::ofstream out{fmt::format("{}-{}.json", filename, store_index)};
    store_index += 1;
    out << std::setw(4) << j << std::endl;
}
