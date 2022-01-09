#pragma once

#include <algorithm>
#include <assert.h>
#include <chrono>
#include <curl/curl.h>
#include <exception>
#include <fmt/chrono.h>
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <optional>
#include <sstream>
#include <vector>

#include "nlohmann/json.hpp"

struct weather {
    using json = nlohmann::json;

    struct Hour {
        std::tm time;
        double temperature;
        double windspeed;
        double gusts;
    };

    weather(std::optional<std::string> store_forecast = std::nullopt,
            std::optional<std::string> load_forecast = std::nullopt)
        : load_file(load_forecast), store_file(store_forecast) {}

    ~weather() {}

    std::vector<Hour> retrieve() {
        json weather;
        if (load_file) {
            weather = load_forecast(*load_file);
        } else {
            weather = fetch_forecast();
        }

        if (store_file) { store_forecast(*store_file); }

        assert(weather.is_object());
        for (auto &[key, value] : weather.items()) { fmt::print("Key: {}\n", key); }

        fmt::print("Got time: {}\n", weather["referenceTime"]);
        fmt::print("Valid time: {}\n", to_string(weather["timeSeries"][0]["validTime"]));
        fmt::print("Temperature: {}\n",
                   to_string(weather["timeSeries"][0]["parameters"][1]["values"][0]));

        std::tm ref_time{};
        char *res =
            strptime(weather["referenceTime"].get<std::string>().c_str(), "%FT%TZ", &ref_time);
        if (res == nullptr) {
            fmt::print("Unable to parse date from {}\n",
                       to_string(weather["referenceTime"]).c_str());
        } else if (*res != '\0') {
            fmt::print("Stopped parsing at: {}\n", res);
        }
        // std::istringstream is {to_string(weather["referenceTime"])};

        // is >> std::get_time(&ref_time, "%FT%TZ");
        fmt::print("Date: {:%Y-%m-%d %H:%M:%S}\n", ref_time);

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
            fmt::print("at {:%H:%M}: {:2}°C\n", dp.time, dp.temperature);
        }
        this->hours = hours;
        return hours;
    }

  private:
    std::optional<std::string> load_file{};
    std::optional<std::string> store_file{};

    char errbuf[CURL_ERROR_SIZE]{};
    std::vector<Hour> hours{};

    json load_forecast(std::string_view filename) {
        // TODO load from file
        return {};
    }

    void store_forecast(std::string_view filename) {}

    json fetch_forecast() {
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
