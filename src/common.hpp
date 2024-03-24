#pragma once

#include <chrono>
#include <fmt/color.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <fmt/ranges.h>
#include <optional>
#include <string>

#include "utils.hpp"

#ifndef DUMMY
#define DUMMY true
#endif

static_assert(DUMMY, "GPIO code is in an unknown state");

enum class RunMode {
    Normal,
    Dry,
};

struct Logger {
    enum class Facility {
        Curl,
        CurlList,
        CurlMime,
        Display,
        Forecast,
        Gpio,
        Hwif,
        MessageQueue,
        Screen,
        Ukko,
        Weather,
        WebConnection,
        WebConnectionDump,
        WebServer,
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

  private:
    Facility facility;
    bool enabled{};

    fmt::text_style get_style() const {
        switch (facility) {
            case Facility::Curl:
                return fmt::fg(fmt::color::coral);

            case Facility::CurlMime:
                return fmt::fg(fmt::color::pink);

            case Facility::CurlList:
                return fmt::fg(fmt::color::blanched_almond);

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

            case Facility::MessageQueue:
                return fmt::fg(fmt::color::crimson);

            case Facility::Weather:
                return fmt::fg(fmt::color::light_green);

            case Facility::WebServer:
                return fmt::fg(fmt::color::white);

            case Facility::WebConnection:
                return fmt::fg(fmt::color::green);

            case Facility::WebConnectionDump:
                return fmt::fg(fmt::color::gold);
        }
        return {};
    }

    std::string_view get_name() const {
        switch (facility) {
            case Facility::Curl:
                return "Curl";

            case Facility::CurlMime:
                return "Curl::Mime";

            case Facility::CurlList:
                return "Curl::List";

            case Facility::Ukko:
                return "Ukko";

            case Facility::Gpio:
                return "GPIO";

            case Facility::Forecast:
                return "Forecast";

            case Facility::Screen:
                return "Screen";

            case Facility::Display:
                return "Display";

            case Facility::Hwif:
                return "Hwif";

            case Facility::MessageQueue:
                return "MessageQueue";

            case Facility::Weather:
                return "Weather";

            case Facility::WebServer:
                return "Web";

            case Facility::WebConnection:
                return "Web::Connection";

            case Facility::WebConnectionDump:
                return "Web::Connection::Dump";
        }
        return "Broken";
    }
};

struct Position {
    std::string longitude;
    std::string latitude;

  private:
    friend std::ostream &operator<<(std::ostream &ostream, const Position &self) {
        ostream << "[longitude=" << self.longitude << ", latitude=" << self.latitude << "]";
        return ostream;
    }
};
template <> struct fmt::formatter<Position> : ostream_formatter {};

struct Auth {
    std::string code;
    std::string redirect;

  private:
    friend std::ostream &operator<<(std::ostream &ostream, const Auth &self) {
        ostream << "[code=" << self.code << ", redirect=" << self.redirect << "]";
        return ostream;
    }
};
template <> struct fmt::formatter<Auth> : ostream_formatter {};

/**
 * Container for handling program parameters
 *
 * These are the parameters set when starting the program
 */
struct Options {
    uint32_t cycles{};                           /**< number of cycles to run program */
    std::optional<std::string> forecast_load{};  /**< Load forecast from file */
    std::optional<std::string> forecast_store{}; /**< Store forecast to file */
    std::optional<std::string> screen_store{};   /**< Store screen to file */
    std::optional<std::string> render_store{};   /**< Store rendering to file */
    std::optional<std::string> netatmo_store{};  /**< Store measurements to file */
    std::optional<std::string> netatmo_load{};   /**< Load measurements to file */

    // TODO: This should probably be a path instead
    std::string settings_file = "/etc/ukko.lua";

    // TODO: Is `sleep` needed? can't this be deduced from frequencies?
    std::chrono::minutes sleep{60};               /**< How often to refresh values */
    std::chrono::minutes retry_sleep{5};          /**< How fast to retry on failure */
    std::chrono::minutes forecast_frequency{120}; /**< How often to refresh forecast */
    std::chrono::minutes weather_frequency{30};   /**< How often to refresh measurements */

    bool dump_traffic = true;
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
