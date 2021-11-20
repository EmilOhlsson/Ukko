#pragma once

#include <fmt/core.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

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
        int fd = open("/sys/class/gpio/export", O_WRONLY);
        // Write  pin number to fd
        // TODO can fmtlib create strings?
        if (fd >= 0) close(fd);

        // TODO open "/sys/class/gpio/gpio<pin>/direction
        // TODO write "out" to file
        // TODO
        // TODO open "/sys/class/gpio/gpio<pin>/value", and keep open
    }

    ~Output() {
        // close `value` fd
        // open "sys/class/unexport" and write pin number
    }

    ActiveScope keep_active() { return ActiveScope(*this); }

    void activate() { fmt::print("Activating {}\n", pin); }

    void deactive() { fmt::print("Deactivating {}\n", pin); }

  private:
    Active level;
    uint32_t pin;
};

struct Input {
    // TODO: Create something that would be usable by future
    Input(/* pin number */) {
        // Open as above and export pin, set it as "in"
        // Open value file and keep open
        // TODO check how to support C++ futures
    }
};

} // namespace Gpio
