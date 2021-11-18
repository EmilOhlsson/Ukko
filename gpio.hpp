#pragma once

#include <fmt/core.h>

namespace Gpio {

enum class Active {
    Low,
    High,
};

struct Output {
    struct ActiveScope {
        ActiveScope(Output &gpio) : gpio(gpio) { gpio.activate(); }
        ~ActiveScope() { gpio.deactive(); }

      private:
        Output &gpio;
    };

    Output(Active level, uint32_t pin) : level(level), pin(pin) {}

    ActiveScope keep_active() { return ActiveScope(*this); }

    void activate() { fmt::print("Activating {}\n", pin); }

    void deactive() { fmt::print("Deactivating {}\n", pin); }

  private:
    Active level;
    uint32_t pin;
};

struct Input {
    // TODO: Create something that would be usable by future
};

} // namespace Gpio
