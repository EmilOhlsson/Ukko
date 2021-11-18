#pragma once

#include <fmt/ranges.h>
#include <span>
#include <string_view>
#include <vector>

#include "gpio.hpp"

namespace spi {

struct Pins {
    Gpio::Output reset;
    Gpio::Output control;
    Gpio::Output select;
    Gpio::Input busy;
};

struct Spi {
    Spi(std::string_view device, Pins &pins)
        : reset(pins.reset), control(pins.control), select(pins.select), busy(pins.busy) {}

    void send(uint8_t cmd, std::initializer_list<uint8_t> data) {
        auto selected = select.keep_active();
        {
            auto ctrl = control.keep_active();
            transfer({cmd});
        }
        transfer(data);
    }

  private:
    void transfer(std::initializer_list<uint8_t> data) {
        std::vector<uint8_t> buffer(std::begin(data), std::end(data));
        fmt::print("writing {}\n", buffer);
    }

    Gpio::Output &reset;
    Gpio::Output &control;
    Gpio::Output &select;
    Gpio::Input &busy;
};

} // namespace spi
