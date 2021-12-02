#pragma once

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>

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
        /* If GPIO already is exported, check if it's configured as output. If it's not already
         * exported, then export and configure as output */
        fmt::print("Exporting pin {}\n", pin);
        if (std::filesystem::exists(fmt::format("{}/gpio{}", gpio_dir, pin))) {
            std::ifstream direction_stream(fmt::format("{}/gpio{}/direction", gpio_dir, pin));
            std::string direction;
            std::getline(direction_stream, direction);
            if (direction != "out") {
                throw std::runtime_error(fmt::format("gpio {} is already as input", pin));
            }
        } else {
            File("/sys/class/gpio/export", O_WRONLY).write(fmt::format("{}", pin));
            File(fmt::format("{}/gpio{}/direction", gpio_dir, pin), O_WRONLY).write("out");
        }
        output = std::make_unique<File>(fmt::format("{}/gpio{}/value", gpio_dir, pin), O_WRONLY);
    }

    ~Output() {
        /* Ground output before removing */
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
    static constexpr std::string_view gpio_dir = "/sys/class/gpio";
};

struct Input {
    Input(uint32_t pin) {
        fmt::print("Exporting input pin {}\n", pin);
        if (std::filesystem::exists(fmt::format("{}/gpio{}", gpio_dir, pin))) {
            std::ifstream direction_stream(fmt::format("{}/gpio{}/direction", gpio_dir, pin));
            std::string direction;
            std::getline(direction_stream, direction);
            if (direction != "in") {
                throw std::runtime_error(
                    fmt::format("gpio {} is already configured as output", pin));
            }
        } else {
            File("/sys/class/gpio/export", O_WRONLY).write(fmt::format("{}", pin));
            File(fmt::format("{}/gpio{}/direction", gpio_dir, pin), O_WRONLY).write("in");
            File(fmt::format("{}/gpio{}/edge", gpio_dir, pin), O_WRONLY).write("rising");
        }

        input = std::make_unique<File>(fmt::format("{}/gpio{}/value", gpio_dir, pin), O_RDONLY);
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
    std::unique_ptr<File> input;
    static constexpr std::string_view gpio_dir = "/sys/class/gpio";
};

} // namespace Gpio
