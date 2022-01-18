#pragma once

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>

//#include <gpiod.h>
#include <gpiod.hpp>

#include "common.hpp"
#include "file.hpp"

namespace gpio {

enum class Active : int {
    Low = 0,
    High = 1,
};

struct Output {
    Output(const Options &options, Active level, uint32_t pin) : level(level), options(options) {
        if (options.is_dry()) {
            return;
        }

        chip = gpiod::chip("gpiochip0");
        line = chip.get_line(pin);
        line.set_direction_output(static_cast<int>(level));
    }

    void activate() {
        if (options.is_dry()) {
            return;
        }

        line.set_value(static_cast<int>(level));
    }

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
        // TODO: Use C++ API instead
        // TODO: Make sure there is timeout on the interrupt awaiting

        fmt::print("Awaiting interrupt\n");
        if (options.is_dry()) {
            return;
        }

        using namespace std::literals::chrono_literals;
        chip = gpiod_chip_open_by_name("gpiochip0");
        if (!chip)
            throw std::runtime_error("Unable to open gpio chip");

        line = gpiod_chip_get_line(chip, 24);
        if (!line)
            throw std::runtime_error("Unable to open gpio line");

        int ret = gpiod_line_request_rising_edge_events(line, "Consumer");
        if (ret < 0)
            throw std::runtime_error("Unable to expect event");

        auto await = [&]() -> int {
            fmt::print("Awaiting event\n");
            int ret = gpiod_line_event_wait(line, nullptr);
            if (ret < 0)
                throw std::runtime_error("Couldn't await :-(");

            gpiod_line_event event;
            ret = gpiod_line_event_read(line, &event);
            if (ret < 0)
                throw std::runtime_error("couldn't read event");
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

    const Options &options;
};

} // namespace gpio
