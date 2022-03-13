#pragma once

#include <algorithm>
#include <chrono>
#include <curl/curl.h>
#include <optional>
#include <string>
#include <vector>

#include "common.hpp"
#include "nlohmann/json.hpp"

struct Forecast {
    using json = nlohmann::json;

    struct DataPoint {
        std::tm time;
        double temperature;
        double windspeed;
        double gusts;
        double rain;
    };

    Forecast(const Options &options);

    std::optional<std::vector<DataPoint>> retrieve(const Position &position);

  private:
    const Options &options;
    const Logger log = options.get_logger(Logger::Facility::Forecast);
    std::optional<std::string> load_file{};
    std::optional<std::string> store_file{};

    uint32_t load_index{};
    uint32_t store_index{};

    char errbuf[CURL_ERROR_SIZE]{};
    std::vector<DataPoint> data_points{};

    json load_forecast(const std::string &filename);

    void store_forecast(const std::string &filename, const json &j);

    json fetch_forecast(const Position &pos);
};
