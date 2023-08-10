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

    Logger(Facility facility, bool enabled) : facility(facility), enabled(enabled) {
    }

    template <typename S, typename... Args> void operator()(const S &format, Args &&...args) const {
        if (enabled) {
            std::string message{fmt::vformat(format, fmt::make_format_args(args...))};
#if FMT_VERSION < 90100
            fmt::print("{}: {}\n", get_name(), message);
#else
            fmt::print("{}: {}\n", get_name(), fmt::styled(message, get_style()));
#endif
        }
    }
    // void operator()(const fmt::format_string<Ts...> &fmt, Ts &&...args) const {
    //     if (verbose) {
    //         //fmt::v7::vprint(get_name() + ": " + fmt + "\n",
    //         //        fmt::v7::make_args_checked<Ts...>(fmt, args...));
    //         fmt::print("{}: ", get_name());
    //         fmt::vprint(fmt, fmt::make_format_args(fmt, args...));
    //         fmt::print("\n");
    //     fmt::v9::print("DINK DINK DINK!\n");
    //     }
    // }

  private:
    Facility facility;
    bool enabled{};

    fmt::text_style get_style() const {
        switch (facility) {
            case Facility::Ukko:
                return fmt::fg(fmt::color::light_blue);

            case Facility::Gpio:
                return fmt::fg(fmt::color::red);

            case Facility::Forecast:
                return fmt::fg(fmt::color::light_cyan);

            case Facility::Screen:
                return fmt::fg(fmt::color::light_blue);

            case Facility::Display:
                return fmt::fg(fmt::color::purple);

            case Facility::Hwif:
                return fmt::fg(fmt::color::light_pink);

            case Facility::Weather:
                return fmt::fg(fmt::color::light_green);
        }
        return {};
    }

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

struct Position {
    std::string longitude;
    std::string latitude;
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
    std::chrono::minutes forecast_frequency{120};
    std::chrono::minutes weather_frequency{30};

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

/* We're assuming A1 format, monochrome */
static constexpr uint32_t WIDTH = 800;
static constexpr uint32_t STRIDE = utils::div_ceil<uint32_t>(WIDTH, 8);
static constexpr uint32_t HEIGHT = 480;
static constexpr uint32_t IMG_SIZE = WIDTH * STRIDE;
