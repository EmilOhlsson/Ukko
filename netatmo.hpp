#pragma once

#include <optional>
#include <string>

#include "common.hpp"
#include "nlohmann/json.hpp"
#include "settings.hpp"
#include "web.hpp"

// TODO: There is basically no error handling going on here, that should be fixed

struct Weather {
    using json = nlohmann::json;

    Weather(const Settings &settings);

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
        Position position;
    };

    /**
     * Takes a sleep time, and returns the shortest of the given time, and the time until next
     * refresh
     */
    [[nodiscard]] std::chrono::minutes check_sleep(std::chrono::minutes sleep_time);

    [[nodiscard]] std::optional<MeasuredData> retrieve();

    /**
     * Authenticate against netatmo API
     */
    [[nodiscard]] bool authenticate(const Auth& auth);

    /**
     * Refresh the authentication token
     */
    [[nodiscard]] bool refresh_authentication();

    /**
     * Check if it is even possible to authenticate
     */
    [[nodiscard]] bool can_authenticate() const {
        // TODO: Once we handle tokens change this
        return false;
    }

  private:
    [[nodiscard]] bool get_token(const PostParams &params);
    /**
     * Parse JSON authentication result
     */
    [[nodiscard]] bool process_authentication_result(json auth);

    /**
     * Fetch device data from the `getstationsdata` API and return as JSON object
     */
    std::optional<json> fetch_device_data();

    /**
     * Load json data from provided file, and increment file counter
     */
    json load_data(const std::string &title);

    /**
     * Store json data to provided file, and increment file counter
     */
    void store_data(const std::string &filename, const json &j);

    /* Operation configuration */
    const Settings &settings;
    const Logger log = settings.get_logger(Logger::Facility::Weather, true);
    const Logger debug = settings.get_logger(Logger::Facility::Weather, true);
    const Settings::Netatmo &netatmo = settings.netatmo;

    /* Used for handling dummy data */
    std::optional<std::string> load_file{};
    std::optional<std::string> store_file{};
    uint32_t load_index{};
    uint32_t store_index{};

    bool is_authenticated = false;

    /* Settings handling */

    /* Authentication data */
    std::string access_token{};
    std::string refresh_token{};
    std::chrono::time_point<std::chrono::system_clock> expiration_time{};

};
