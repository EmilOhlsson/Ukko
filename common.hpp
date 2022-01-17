#pragma once

#include <optional>
#include <string>

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

    bool is_dry() const {
        return run_mode == RunMode::Dry;
    }
};
