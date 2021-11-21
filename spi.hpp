#pragma once

#include <fmt/ranges.h>
#include <linux/spi/spidev.h> 
#include <linux/types.h> 
#include <span>
#include <string_view>
#include <sys/ioctl.h> 
#include <string.h>
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
        : reset(pins.reset), control(pins.control), select(pins.select), busy(pins.busy) {
            // Open File O_RDWR
            // uint8_t bits = 8;
            // ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
            // Check DEV_HARDWARE_SPI_begin()
            fd = std::make_unique<File>(device, O_RDWR);

        }

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

    void set_bits_per_word() {
        uint8_t bits = 8;
        if (ioctl(*fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0) {
            throw std::runtime_error(fmt::format("Unable to set bits per word: {}", 
                        strerror(errno)));
        }
    }

    void set_mode(int mode) {
        if (ioctl(*fd, SPI_IOC_WR_MODE, mode) < 0) {
            throw std::runtime_error(fmt::format("Unable to set SPI mode: {}",
                        strerror(errno)));
        }
    }

    Gpio::Output &reset;
    Gpio::Output &control;
    Gpio::Output &select;
    Gpio::Input &busy;

    std::unique_ptr<File> fd;
};

} // namespace spi
