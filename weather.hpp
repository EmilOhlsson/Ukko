#pragma once

#include <algorithm>
#include <assert.h>
#include <chrono>
#include <curl/curl.h>
#include <exception>
#include <fmt/chrono.h>
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "common.hpp"
#include "nlohmann/json.hpp"

struct weather {
    using json = nlohmann::json;

    struct Hour {
        std::tm time;
        double temperature;
        double windspeed;
        double gusts;
    };

    weather(const Options &options)
        : options(options), load_file(options.forecast_load), store_file(options.forecast_store) {
    }

    ~weather() {
    }

    std::vector<Hour> retrieve() {
        json weather;
        if (load_file) {
            weather = load_forecast(*load_file);
        } else {
            weather = fetch_forecast();
        }

        if (store_file) {
            store_forecast(*store_file, weather);
        }

        assert(weather.is_object());

        std::vector<Hour> hours{};
        for (auto &dp : weather["timeSeries"]) {
            double temperature{};
            double windspeed{};
            double gusts{};

            for (auto &p : dp["parameters"]) {
                std::string_view name = p["name"].get<std::string_view>();
                if (name == "t") {
                    temperature = p["values"][0].get<double>();
                } else if (name == "ws") {
                    windspeed = p["values"][0].get<double>();
                } else if (name == "gust") {
                    gusts = p["values"][0].get<double>();
                }
            }

            hours.push_back(Hour{
                .time = from_is8061(dp["validTime"].get<std::string>()),
                .temperature = temperature,
                .windspeed = windspeed,
                .gusts = gusts,
            });
        }

        for (const auto &dp : hours) {
            log("at {:%H:%M}: {:2}°C", dp.time, dp.temperature);
        }
        this->hours = hours;
        return hours;
    }

  private:
    static constexpr std::string_view log_name = "Weather";
    const Options &options;
    const Logger log = options.get_logger(Logger::Facility::Weather);
    std::optional<std::string> load_file{};
    std::optional<std::string> store_file{};

    uint32_t load_index{};
    uint32_t store_index{};

    char errbuf[CURL_ERROR_SIZE]{};
    std::vector<Hour> hours{};

    json load_forecast(const std::string &filename) {
        std::ifstream input{fmt::format("{}-{}.json", filename, load_index)};
        load_index += 1;
        json result;
        input >> result;
        return result;
    }

    void store_forecast(const std::string &filename, const json &j) {
        std::ofstream out{fmt::format("{}-{}.json", filename, store_index)};
        store_index += 1;
        out << std::setw(4) << j << std::endl;
    }

    json fetch_forecast() {
        // TODO: Read longtidue and latitude from some kind of settings file
        const std::string longitude = "13.18120";
        const std::string latitude = "55.70487";
        const std::string url = fmt::format(
            "https://opendata-download-metfcst.smhi.se"
            "/api/category/pmp3g/version/2/geotype/point/lon/{}/lat/{}/data.json",
            longitude, latitude);

        CURL *curl = curl_easy_init();
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
        std::string weather_data{};
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &weather_data);
        curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        // TODO Check if this keeps connection open.

        return json::parse(weather_data);
    }

    static size_t write_cb(void *data, size_t size, size_t nmemb, void *userp) {
        assert(size == 1);
        const char *bytes = static_cast<char *>(data);
        auto &storage = *static_cast<std::string *>(userp);
        std::copy_n(bytes, nmemb, std::back_inserter(storage));
        return nmemb;
    }

    /**
     * Convert a string to a std::tm. String should be the ISO8061 format,
     * so something like 2020-01-01T10:00:00Z
     */
    std::tm from_is8061(const std::string &str) {
        std::tm time;
        char *result = strptime(str.c_str(), "%FT%TZ", &time);
        if (!result) {
            throw std::runtime_error("Unable to parse date");
        } else if (*result != '\0') {
            throw std::runtime_error("Unable to darse date, trailing characters");
        }
        return time;
    }
};
