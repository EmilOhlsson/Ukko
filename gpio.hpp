#pragma once

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>

//#include <gpiod.h>
#include <gpiod.hpp>

#include "file.hpp"

namespace gpio {

enum class Active {
    Low,
    High,
};

struct Output {
    // struct ActiveScope {
    //     ActiveScope(Output &gpio) : gpio(gpio) { gpio.activate(); }
    //     ~ActiveScope() { gpio.deactive(); }
    //   private:
    //     Output &gpio;
    // };

    Output(Active level, uint32_t pin) : level(level), pin(pin) {
        // TODO: use GPIOD
        using namespace std::literals::chrono_literals;
        /* If GPIO already is exported, check if it's configured as output. If it's not already
         * exported, then export and configure as output */
        fmt::print("   Exporting pin {}\n", pin);
        if (std::filesystem::exists(fmt::format("{}/gpio{}", gpio_dir, pin))) {
            std::ifstream direction_stream(fmt::format("{}/gpio{}/direction", gpio_dir, pin));
            std::string direction;
            std::getline(direction_stream, direction);
            if (direction != "out") {
                throw std::runtime_error(fmt::format("gpio {} is already as input", pin));
            }
        } else {
            File("/sys/class/gpio/export", O_WRONLY).write(fmt::format("{}", pin));
            std::this_thread::sleep_for(1s);
            File(fmt::format("{}/gpio{}/direction", gpio_dir, pin), O_WRONLY).write("out");
        }
        output = std::make_unique<File>(fmt::format("{}/gpio{}/value", gpio_dir, pin),
                                        O_WRONLY | O_SYNC);
    }

    ~Output() {
        /* Ground output before removing */
        write(*output, "0", 1);
        output.reset(nullptr);
        File("/sys/class/gpio/unexport", O_WRONLY).write(fmt::format("{}", pin));
    }

    // ActiveScope keep_active() { return ActiveScope(*this); }

    void activate() {
        // if (activation_count == 0) {
        fmt::print("   Activating {}\n", pin);
        output->write(level == Active::Low ? "0" : "1");
        // write(*output, level == Active::Low ? "0" : "1", 1);
        //} else {
        //	fmt::print("   Skipping activation. count is {} for pin {}\n", activation_count, pin);
        //}
        // activation_count += 1;
    }

    void deactive() {
        // activation_count = std::min<uint32_t>(activation_count - 1, 0);
        // if (--activation_count == 0) {
        fmt::print("   Deactivating {}\n", pin);
        // write(*output, level == Active::Low ? "1" : "0", 1);
        output->write(level == Active::Low ? "1" : "0");
        //} else {
        //	fmt::print("   Skipping deactivation of pin {}, count is now {}\n", pin,
        // activation_count);
        //}
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
        //    using namespace std::literals::chrono_literals;
        // fmt::print("   Exporting input pin {}\n", pin);
        // if (std::filesystem::exists(fmt::format("{}/gpio{}", gpio_dir, pin))) {
        //    std::ifstream direction_stream(fmt::format("{}/gpio{}/direction", gpio_dir, pin));
        //    std::string direction;
        //    std::getline(direction_stream, direction);
        //    if (direction != "in") {
        //        throw std::runtime_error(
        //            fmt::format("gpio {} is already configured as output", pin));
        //    }
        //} else {
        //    File("/sys/class/gpio/export", O_WRONLY).write(fmt::format("{}", pin));
        //    std::this_thread::sleep_for(1s);
        //    File(fmt::format("{}/gpio{}/direction", gpio_dir, pin), O_WRONLY).write("in");
        //    File(fmt::format("{}/gpio{}/edge", gpio_dir, pin), O_WRONLY).write("rising");
        //}

        // input = std::make_unique<File>(fmt::format("{}/gpio{}/value", gpio_dir, pin), O_RDONLY |
        // O_SYNC);
        // chip = gpiod_chip_open_by_name("gpiochip0");
        // if (!chip) throw std::runtime_error("Unable to open gpio chip");

        // line = gpiod_chip_get_line(chip, 24);
        // if (!line) throw std::runtime_error("Unable to open gpio line");

        // int ret = gpiod_line_request_input(line, "Consumer");
        // if (ret < 0) throw std::runtime_error("Unable to get input");
    }

    ~Input() {
        // gpiod_line_release(line);
        // gpiod_chip_close(chip);
    }

    void wfi() {
        using namespace std::literals::chrono_literals;
        chip = gpiod_chip_open_by_name("gpiochip0");
        if (!chip) throw std::runtime_error("Unable to open gpio chip");

        line = gpiod_chip_get_line(chip, 24);
        if (!line) throw std::runtime_error("Unable to open gpio line");

        int ret = gpiod_line_request_rising_edge_events(line, "Consumer");
        if (ret < 0) throw std::runtime_error("Unable to expect event");

        auto await = [&]() -> int {
            fmt::print("Awaiting event\n");
            int ret = gpiod_line_event_wait(line, nullptr);
            if (ret < 0) throw std::runtime_error("Couldn't await :-(");

            gpiod_line_event event;
            ret = gpiod_line_event_read(line, &event);
            if (ret < 0) throw std::runtime_error("couldn't read event");
            return 0;
        };

        do {
        } while (0);
        // std::this_thread::sleep_for(5ms);
        // TODO: this should have a timeout, and check if busy is already high
        await();

        gpiod_line_release(line);
        gpiod_chip_close(chip);
    }

    // void wfi() {
    //     fmt::print("   Waiting for interrupt\n");
    //     std::array<char, 8> buffer{};
    //     using namespace std::literals::chrono_literals;

    //    //auto read_level = [&]() -> int {
    //    //    lseek(*input, 0, SEEK_SET);
    //    //    ssize_t size;
    //    //    if ((size = read(*input, buffer.data(), buffer.size() - 1)) < 0) {
    //    //        throw std::runtime_error(fmt::format("Unable to read interrupt pin: {}",
    //    strerror(errno)));
    //    //    }
    //    //    fmt::print(" -- Read size={}:  {}\n", size, &buffer[0]);
    //    //    return atoi(&buffer[0]);
    //    //};
    //    //
    //    chip = gpiod_chip_open_by_name("gpiochip0");
    //    if (!chip) throw std::runtime_error("Unable to open gpio chip");

    //    line = gpiod_chip_get_line(chip, 24);
    //    if (!line) throw std::runtime_error("Unable to open gpio line");

    //    int ret = gpiod_line_request_input(line, "Consumer");
    //    if (ret < 0) throw std::runtime_error("Unable to get input");
    //    auto read_level = [&]() -> int {

    //    	int val = gpiod_line_get_value(line);
    //    	fmt::print("   Read {} from gpio...\n", val);
    //    	if (val < 0) throw std::runtime_error("Error reading gpio");

    //    	return val;
    //    };
    //    do {
    //        std::this_thread::sleep_for(1s);
    //    } while (!read_level());
    //    gpiod_line_release(line);
    //    gpiod_chip_close(chip);
    //    //// TODO this is not really done
    //    //pollfd pfd{
    //    //    .fd = *input,
    //    //    .events = POLLPRI,
    //    //    .revents = {},
    //    //};
    //    //std::array<uint8_t, 8> buffer;
    //    //lseek(*input, 0, SEEK_SET);
    //    //::read(*input, buffer.data(), buffer.size());
    //    //poll(&pfd, 1, -1);
    //    //lseek(*input, 0, SEEK_SET); /* consume interrupt */
    //    //read(*input, buffer.data(), buffer.size());
    //}

  private:
    gpiod_chip *chip;
    gpiod_line *line;
    std::unique_ptr<File> input;
    static constexpr std::string_view gpio_dir = "/sys/class/gpio";
};

} // namespace gpio
