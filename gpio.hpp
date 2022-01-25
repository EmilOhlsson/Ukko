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

        chip = gpiod::chip("gpiochip0");
        line = chip.get_line(pin);
        line.request(gpiod::line_request{
            .consumer = name,
            .request_type = gpiod::line_request::DIRECTION_OUTPUT,
            .flags = {},
        });
        line.set_direction_output(0);
        assert(line.is_requested());
    }

    /**
     * Set pin to active state, as per `active` level given at construction
     */
    void activate() {
        int value = static_cast<int>(level);
        log("Setting pin {} to {}", line.name(), value);
        if (options.is_dry()) {
            return;
        }

        line.set_value(value);
    }

    /**
     * Set pin to deactive state, as per `active` level given at construction
     */
    void deactive() {
        int value = !static_cast<int>(level);
        log("Setting pin {} to {}", line.name(), value);
        if (options.is_dry()) {
            return;
        }
        line.set_value(value);
    }

  private:
    gpiod::chip chip;
    gpiod::line line;

    Active level;
    const Options &options;
    Logger log = options.get_logger(Logger::Facility::Gpio);
};

struct Input {
    Input(const Options &options, uint32_t pin, const std::string &name) : options(options) {
        if (options.is_dry()) {
            return;
        }

        chip = gpiod::chip("gpiochip0");
        line = chip.get_line(pin);
        line.request({
            .consumer = name,
            .request_type = gpiod::line_request::DIRECTION_INPUT,
            .flags = {},
        });
        line.set_direction_input();
        assert(line.is_requested());
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
        } while (!line.get_value());
        log("Slept for {} iterations", sleep_count);
        // while (true) {
        //     if (line.event_wait(1s)) {
        //         gpiod::line_event event = line.event_read();
        //         log("Read event: {}", event.event_type);
        //         break;
        //     } else {
        //         log("Timeout while waiting for event, retrying {}", line.get_value());
        //         if (line.get_value() == 0) {
        //             break;
        //         }
        //     }
        // }
        std::this_thread::sleep_for(5ms);
    }

  private:
    gpiod::chip chip;
    gpiod::line line;
    const Options &options;
    const Logger log = options.get_logger(Logger::Facility::Gpio);
};

} // namespace gpio
