#pragma once

#include <algorithm>

#include "file.hpp"

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

    Output(Active level, uint32_t pin) : level(level), pin(pin) {
        File("/sys/class/gpio/export", O_WRONLY).write(fmt::format("{}", pin));
        std::string gpio_dir = fmt::format("/sys/class/gpio/gpio{}/", pin);
        File(gpio_dir + "direction", O_WRONLY).write("out");
        output = std::make_unique<File>(gpio_dir + "value", O_WRONLY);
    }

    ~Output() {
        // Ground output before removing
        write(*output, "0", 1);
        output.reset(nullptr);
        File("/sys/class/gpio/unexport", O_WRONLY).write(fmt::format("{}", pin));
    }

    ActiveScope keep_active() { return ActiveScope(*this); }

    void activate() {
        if (activation_count == 0) {
            fmt::print("Activating {}\n", pin);
            write(*output, level == Active::Low ? "0" : "1", 1);
        }
        activation_count += 1;
    }

    void deactive() {
        activation_count = std::min<uint32_t>(activation_count - 1, 0);
        if (activation_count-- == 0) {
            fmt::print("Deactivating {}\n", pin);
            write(*output, level == Active::Low ? "1" : "0", 1);
        }
    }

  private:
    Active level;
    uint32_t pin;
    uint32_t activation_count = 0;
    std::unique_ptr<File> output;
};

struct Input {
    // TODO: Create something that would be usable by future
    Input(uint32_t pin) : pin(pin) {
        File("/sys/class/gpio/export", O_WRONLY).write(fmt::format("{}", pin));
        std::string gpio_dir = fmt::format("/sys/class/gpio/gpio{}/", pin);
        File(gpio_dir + "direction", O_WRONLY).write("in");
        File(gpio_dir + "edge", O_WRONLY).write("rising");

        input = std::make_unique<File>(gpio_dir + "value", O_RDONLY);
        // Open as above and export pin, set it as "in"
        // Open value file and keep open
        // TODO check how to support C++ futures
    }

    void wfi() {
        // TODO this is not really done
        pollfd pfd{
            .fd = *input,
            .events = POLLPRI,
            .revents = {},
        };
        std::array<uint8_t, 8> buffer;
        lseek(*input, 0, SEEK_SET);
        ::read(*input, buffer.data(), buffer.size());
        poll(&pfd, 1, -1);
        lseek(*input, 0, SEEK_SET); /* consume interrupt */
        read(*input, buffer.data(), buffer.size());
    }

  private:
    uint32_t pin;
    std::unique_ptr<File> input;
};

} // namespace Gpio
