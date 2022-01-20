#pragma once

#include <optional>
#include <string>

#include "utils.hpp"

enum class RunMode {
    Normal,
    Dry,
};

struct Options {
    std::optional<std::string> forecast_load{};
    std::optional<std::string> forecast_store{};
    std::optional<std::string> screen_store{};
    bool verbose = false;
    RunMode run_mode = DUMMY ? RunMode::Dry : RunMode::Normal;
    std::string spi_device = "/dev/spidev0.0";

    bool is_dry() const {
        return run_mode == RunMode::Dry;
    }
};

/* We're assuming A1 format, monochrome */
static constexpr uint32_t WIDTH = 800;
static constexpr uint32_t STRIDE = utils::div_ceil<uint32_t>(WIDTH, 8);
static constexpr uint32_t HEIGHT = 480;
static constexpr uint32_t IMG_SIZE = WIDTH * STRIDE;
