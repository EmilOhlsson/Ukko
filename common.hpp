#pragma once

#include <chrono>
#include <fmt/color.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <optional>
#include <string>

#include "utils.hpp"

enum class RunMode {
    Normal,
    Dry,
};

struct Logger {
    enum class Facility {
        Ukko,
        Forecast,
        Screen,
        Display,
        Hwif,
        Gpio,
        Weather,
    };

    Logger(Facility facility, bool enabled)
        : facility(facility), enabled(enabled), style{fmt::fg(fmt::color::blue)} {
    }

    template <typename S, typename... Args> void operator()(const S &format, Args &&...args) const {
        if (enabled) {
            std::string message{fmt::vformat(format, fmt::make_format_args(args...))};
            fmt::print("{}: {}\n", get_name(), fmt::styled(message, style));
        }
    }

  private:
    Facility facility;
    bool enabled{};
    fmt::text_style style;

    std::string get_name() const {
        switch (facility) {
            case Facility::Ukko:
                return "Ukko";

            case Facility::Gpio:
                return "GPIO";

            case Facility::Forecast:
                return "Weather";

            case Facility::Screen:
                return "Screen";

            case Facility::Display:
                return "Display";

            case Facility::Hwif:
                return "Hwif";

            case Facility::Weather:
                return "Weather";
        }
        return "Broken";
    }
};

struct Options {
    uint32_t cycles{};
    std::optional<std::string> forecast_load{};
    std::optional<std::string> forecast_store{};
    std::optional<std::string> screen_store{};
    std::optional<std::string> render_store{};
    std::optional<std::string> netatmo_store{};
    std::optional<std::string> netatmo_load{};

    std::string settings_file = "/etc/ukko.lua";

    std::chrono::minutes sleep{60};
    std::chrono::minutes retry_sleep{5};
    bool verbose = false;
    bool debug_log = false;
    RunMode run_mode = DUMMY ? RunMode::Dry : RunMode::Normal;
    std::string spi_device = "/dev/spidev0.0";

    bool is_dry() const {
        return run_mode == RunMode::Dry;
    }

    Logger get_logger(Logger::Facility facility, bool force_enabled = false) const {
        return Logger(facility, verbose || force_enabled);
    }
    Logger get_debug_logger(Logger::Facility facility, bool force_enabled = false) const {
        return Logger(facility, debug_log || force_enabled);
    }
};

struct Position {
    std::string longitude;
    std::string latitude;
};

/* We're assuming A1 format, monochrome */
static constexpr uint32_t WIDTH = 800;
static constexpr uint32_t STRIDE = utils::div_ceil<uint32_t>(WIDTH, 8);
static constexpr uint32_t HEIGHT = 480;
static constexpr uint32_t IMG_SIZE = WIDTH * STRIDE;
