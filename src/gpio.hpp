#pragma once

#include <gpiod.hpp>
#include <thread>

#include "common.hpp"

namespace gpio {

/**
 * Enumerated desciption of active low, vs active high
 */
enum class Active : int {
    Low = 0,
    High = 1,
};

/* TODO: I suspect we might want to merge this with hwif, considering how gpiod has
 * been updated */
struct Output {
    /**
     * Create new GPIO output using `pin`. It will be considered active at given `level`.
     * If options describe this as dry run, then no actual GPIO toggling will be made
     */
    Output(const Options &options, Active level, uint32_t pin,
           const std::string &name [[maybe_unused]])
        : options(options) {
        if (options.is_dry()) {
            return;
        }

        chip = gpiod::chip{"/dev/gpiochip0"};
        if (!chip) {
            throw std::runtime_error("Unable to open gpio chip for output");
        }

        // This would probably be the prepare request
        gpiod::request_builder request = chip->prepare_request();
        gpiod::line_settings settings{};
        settings.set_direction(gpiod::line::direction::OUTPUT)
            .set_output_value(level == Active::Low ? gpiod::line::value::INACTIVE: gpiod::line::value::ACTIVE)
            .set_drive(gpiod::line::drive::OPEN_DRAIN);

        line = request.add_line_settings(pin, settings).do_request();
        this->pin = pin;
    }

    ~Output() {
        if (line) {
            line->release();
        }
        if (chip) {
            chip->close();
        }
    }

    /**
     * Set pin to active state, as per `active` level given at construction
     */
    void activate() {
        if (options.is_dry()) {
            return;
        }

        line->set_value(*pin, gpiod::line::value::ACTIVE);
    }

    /**
     * Set pin to deactive state, as per `active` level given at construction
     */
    void deactive() {
        if (options.is_dry()) {
            return;
        }

        line->set_value(*pin, gpiod::line::value::INACTIVE);
    }

  private:
    std::optional<gpiod::line::offset> pin;
    std::optional<gpiod::chip> chip;
    std::optional<gpiod::line_request> line{};

    const Options &options;
    Logger log = options.get_logger(Logger::Facility::Gpio);
};

struct Input {
    /**
     * Create new GPIO input using `pin`. If options describe this as dry run,
     * then no actual GPIO toggling will be made
     */
    Input(const Options &options, uint32_t pin, const std::string &name [[maybe_unused]])
        : options(options) {
        if (options.is_dry()) {
            return;
        }

        chip = gpiod::chip{"/dev/gpiochip0"};
        if (!chip) {
            throw std::runtime_error("Unable to open gpio chip for output");
        }

        // This would probably be the prepare request
        gpiod::request_builder request = chip->prepare_request();
        gpiod::line_settings settings{};
        settings.set_direction(gpiod::line::direction::INPUT)
            .set_output_value(gpiod::line::value::INACTIVE)
            .set_drive(gpiod::line::drive::OPEN_DRAIN);

        line = request.add_line_settings(pin, settings).do_request();
        this->pin = pin;
    }

    ~Input() {
        if (line) {
            line->release();
        }
        if (chip) {
            chip->close();
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
        } while (line->get_value(*pin) != gpiod::line::value::ACTIVE);
        log("Slept for {} iterations", sleep_count);
        std::this_thread::sleep_for(5ms);
    }

  private:
    std::optional<gpiod::line::offset> pin;
    std::optional<gpiod::chip> chip;
    std::optional<gpiod::line_request> line{};
    const Options &options;
    const Logger log = options.get_logger(Logger::Facility::Gpio);
};

} // namespace gpio
