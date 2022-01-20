#pragma once

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>

#include <gpiod.hpp>

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
    Output(const Options &options, Active level, uint32_t pin) : level(level), options(options) {
        if (options.is_dry()) {
            return;
        }

        chip = gpiod::chip("gpiochip0");
        line = chip.get_line(pin);
        line.request(gpiod::line_request{
            .consumer = "Consumer",
            .request_type = gpiod::line_request::DIRECTION_OUTPUT,
            .flags = {},
        });
    }

    /**
     * Set pin to active state, as per `active` level given at construction
     */
    void activate() {
        if (options.is_dry()) {
            return;
        }

        line.set_value(static_cast<int>(level));
    }

    /**
     * Set pin to deactive state, as per `active` level given at construction
     */
    void deactive() {
        if (options.is_dry()) {
            return;
        }
        line.set_value(!static_cast<int>(level));
    }

  private:
    gpiod::chip chip;
    gpiod::line line;

    Active level;
    const Options &options;
};

struct Input {
    Input(const Options &options, uint32_t pin) : options(options) {
        if (options.is_dry()) {
            return;
        }

        chip = gpiod::chip("gpiochip0");
        line = chip.get_line(pin);
        // line.set_direction_input();
        line.request({
            .consumer = "Consumer?",
            .request_type = gpiod::line_request::EVENT_RISING_EDGE,
            .flags = {},
        });
    }

    void wfi() {
        fmt::print("Awaiting interrupt\n");
        if (options.is_dry()) {
            return;
        }

        using namespace std::literals::chrono_literals;

        while (true) {
            if (line.event_wait(5s)) {
                gpiod::line_event event = line.event_read();
                fmt::print("Read event: {}\n", event.event_type);
                break;
            } else {
                fmt::print("Timeout while waiting for event, retrying\n");
            }
        }
    }

  private:
    gpiod::chip chip;
    gpiod::line line;
    const Options &options;
};

} // namespace gpio
