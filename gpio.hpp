#pragma once

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <memory>

#include <gpiod.hpp>
#include <thread>

#include "common.hpp"
#include "file.hpp"

namespace gpio {

/**
 * Enumerated desciption of active low, vs active high
 */
enum class Active : int {
    Low = 0,
    High = 1,
};

struct Output {
    /**
     * Create new GPIO output using `pin`. It will be considered active at given `level`.
     * If options describe this as dry run, then no actual GPIO toggling will be made
     */
    Output(const Options &options, Active level, uint32_t pin, const std::string &name)
        : level(level), options(options) {
        if (options.is_dry()) {
            return;
        }

        chip = gpiod_chip_open("/dev/gpiochip0");
        if (!chip) {
            throw std::runtime_error("Unable to open gpio chip for output");
        }

        line = gpiod_chip_get_line(chip, pin);
        if (!line) {
            throw std::runtime_error("Unable to get line");
        }

        gpiod_line_request_config cfg{
            .consumer = name.c_str(),
            .request_type = GPIOD_LINE_REQUEST_DIRECTION_OUTPUT,
            .flags = 0,
        };

        if (gpiod_line_request(line, &cfg, 0) != 0) {
            throw std::runtime_error("Unable to request output");
        }
    }

    ~Output() {
        if (line) {
            gpiod_line_release(line);
        }
        if (chip) {
            gpiod_chip_close(chip);
        }
    }

    /**
     * Set pin to active state, as per `active` level given at construction
     */
    void activate() {
        int value = static_cast<int>(level);
        if (options.is_dry()) {
            return;
        }

        if (gpiod_line_set_value(line, value) != 0) {
            throw std::runtime_error("Unable to activate pin");
        }
    }

    /**
     * Set pin to deactive state, as per `active` level given at construction
     */
    void deactive() {
        int value = !static_cast<int>(level);
        if (options.is_dry()) {
            return;
        }

        if (gpiod_line_set_value(line, value) != 0) {
            throw std::runtime_error("Unable to deactivate pin");
        }
    }

  private:
    gpiod_chip *chip{};
    gpiod_line *line{};

    Active level;
    const Options &options;
    Logger log = options.get_logger(Logger::Facility::Gpio);
};

struct Input {
    /**
     * Create new GPIO input using `pin`. If options describe this as dry run,
     * then no actual GPIO toggling will be made
     */
    Input(const Options &options, uint32_t pin, const std::string &name) : options(options) {
        if (options.is_dry()) {
            return;
        }

        chip = gpiod_chip_open("/dev/gpiochip0");
        if (!chip) {
            throw std::runtime_error("Error opening chip for input");
        }

        line = gpiod_chip_get_line(chip, pin);
        if (!line) {
            throw std::runtime_error("Error opening line");
        }

        gpiod_line_request_config cfg{
            .consumer = name.c_str(),
            .request_type = gpiod::line_request::DIRECTION_INPUT,
            .flags = 0,
        };

        if (gpiod_line_request(line, &cfg, 0) != 0) {
            throw std::runtime_error("Unable to request line");
        }
    }

    ~Input() {
        if (line) {
            gpiod_line_release(line);
        }
        if (chip) {
            gpiod_chip_close(chip);
        }
    }

    void wfi() {
        using namespace std::literals::chrono_literals;

        log("Awaiting interrupt");
        if (options.is_dry()) {
            return;
        }

        int sleep_count = 0;
        do {
            std::this_thread::sleep_for(5ms);
            sleep_count += 1;
        } while (!gpiod_line_get_value(line));
        log("Slept for {} iterations", sleep_count);
        std::this_thread::sleep_for(5ms);
    }

  private:
    gpiod_chip *chip{};
    gpiod_line *line{};
    const Options &options;
    const Logger log = options.get_logger(Logger::Facility::Gpio);
};

} // namespace gpio
