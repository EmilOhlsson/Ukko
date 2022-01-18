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
        Weather,
        Screen,
        Display,
        Hwif,
        Gpio,
    };

    Logger(Facility facility, bool verbose) : facility(facility), verbose(verbose) {
    }

    template <typename... Ts> void operator()(const std::string &fmt, Ts &&...args) const {
        if (verbose) {
            fmt::vprint(get_name() + ": " + fmt + "\n",
                        fmt::make_args_checked<Ts...>(fmt, args...));
        }
    }

  private:
    Facility facility;
    bool verbose{};

    std::string get_name() const {
        switch (facility) {
            case Facility::Ukko:
                return "Ukko";

            case Facility::Gpio:
                return "GPIO";

            case Facility::Weather:
                return "Weather";

            case Facility::Screen:
                return "Screen";

            case Facility::Display:
                return "Display";

            case Facility::Hwif:
                return "Hwif";
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
    std::chrono::minutes sleep{120};
    bool verbose = false;
    RunMode run_mode = DUMMY ? RunMode::Dry : RunMode::Normal;
    std::string spi_device = "/dev/spidev0.0";

    bool is_dry() const {
        return run_mode == RunMode::Dry;
    }

    Logger get_logger(Logger::Facility facility) const {
        return Logger(facility, verbose);
    }
};

/* We're assuming A1 format, monochrome */
static constexpr uint32_t WIDTH = 800;
static constexpr uint32_t STRIDE = utils::div_ceil<uint32_t>(WIDTH, 8);
static constexpr uint32_t HEIGHT = 480;
static constexpr uint32_t IMG_SIZE = WIDTH * STRIDE;
