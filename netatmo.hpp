#pragma once

#include <curl/curl.h>
#include <string>

#include "nlohmann/json.hpp"
#include "settings.hpp"

// TODO: There is basically no error handling going on here, that should be fixed

struct Weather {
    using json = nlohmann::json;

    Weather(const Options &options);

    /**
     * Return position reported by Netatmo
     */
    Position get_position() const;

    /**
     * Container for weather measurements from
     */
    struct MeasuredData {
        struct Rain {
            double last_1h;
            double last_24h;
        } rain;
        struct Temperature {
            double now;
            double min;
            double max;
        };
        Temperature outdoor;
        Temperature indoor;
    };

    /**
     * Takes a sleep time, and returns the shortest of the given time, and the time until next
     * refresh
     */
    std::chrono::minutes check_sleep(std::chrono::minutes sleep_time);

    MeasuredData retrieve();

    /**
     * Authenticate against netatmo API
     */
    void authenticate();

    /**
     * Refresh the authentication token
     */
    void refresh_authentication();

  private:
    /**
     * Parse JSON authentication result
     */
    void process_authentication_result(json auth);

    /**
     * Fetch device data from the `getstationsdata` API and return as JSON object
     */
    json fetch_device_data();

    /**
     * Load json data from provided file, and increment file counter
     */
    json load_data(const std::string &title);

    /**
     * Store json data to provided file, and increment file counter
     */
    void store_data(const std::string &filename, const json &j);

    /* Position reported by Netatmo */
    std::string longitude{};
    std::string latitude{};

    /* Used for curl error messages */
    char errbuf[CURL_ERROR_SIZE]{};

    /* Operation configuration */
    const Options &options;
    const Logger log = options.get_logger(Logger::Facility::Weather);

    /* Used for handling dummy data */
    std::optional<std::string> load_file{};
    std::optional<std::string> store_file{};
    uint32_t load_index{};
    uint32_t store_index{};

    bool is_authenticated = false;

    /* Settings handling */
    const Settings settings{options.settings_file};
    const Settings::Netatmo &netatmo = settings.netatmo;

    /* Authentication data */
    std::string access_token{};
    std::string refresh_token{};
    std::chrono::time_point<std::chrono::system_clock> expiration_time{};
};
