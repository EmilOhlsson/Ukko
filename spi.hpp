#pragma once

#include <fmt/ranges.h>
#include <linux/spi/spidev.h>
#include <linux/types.h>
#include <span>
#include <string.h>
#include <string_view>
#include <sys/ioctl.h>
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

        set_mode(SPI_MODE_0);
        set_chip_select(CS_Mode::Low);
        set_bit_order(BitOrder::LsbFirst);
        set_speed(20000000);
        set_interval(0);
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

    void set_speed(uint32_t speed) {
        if (ioctl(*fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
            throw std::runtime_error(fmt::format("Unable to set write speed: {}", strerror(errno)));
        }
        if (ioctl(*fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed) < 0) {
            throw std::runtime_error(fmt::format("Unable to set read speed: {}", strerror(errno)));
        }
    }

    // TODO: The functions below can be merged to pretty much on IOCTL
    void set_bits_per_word() {
        uint8_t bits = 8;
        if (ioctl(*fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0) {
            throw std::runtime_error(
                fmt::format("Unable to set bits per word: {}", strerror(errno)));
        }
        if (ioctl(*fd, SPI_IOC_RD_BITS_PER_WORD, &bits) < 0) {
            throw std::runtime_error(
                fmt::format("Unable to set bits per word: {}", strerror(errno)));
        }
        tr.bits_per_word = bits;
    }

    void set_mode(int mode) {
        m_mode &= 0xFC;
        m_mode |= mode;
        if (ioctl(*fd, SPI_IOC_WR_MODE, &m_mode) < 0) {
            throw std::runtime_error(fmt::format("Unable to set SPI mode: {}", strerror(errno)));
        }
    }

    enum class CS_Mode {
        High,
        Low,
    };

    void set_chip_select(CS_Mode mode) {
        switch (mode) {
            case CS_Mode::High: {
                m_mode |= SPI_CS_HIGH;
                m_mode &= ~SPI_NO_CS;
                break;
            }
            case CS_Mode::Low: {
                m_mode &= ~SPI_CS_HIGH;
                m_mode &= ~SPI_NO_CS;
                break;
            }
        }
        if (ioctl(*fd, SPI_IOC_WR_MODE, &m_mode) < 0) {
            throw std::runtime_error(fmt::format("Unable to set SPI mode: {}", strerror(errno)));
        }
    }

    enum class BitOrder {
        LsbFirst,
        MsbFirst,
    };

    void set_bit_order(BitOrder order) {
        switch (order) {
            case BitOrder::LsbFirst:
                m_mode |= SPI_LSB_FIRST;
                break;
            case BitOrder::MsbFirst:
                m_mode &= ~SPI_LSB_FIRST;
                break;
        }
        if (ioctl(*fd, SPI_IOC_WR_MODE, &m_mode) < 0) {
            throw std::runtime_error(fmt::format("Unable to set SPI mode: {}", strerror(errno)));
        }
    }

    enum class BusMode {
        ThreeWire,
        FourWire,
    };

    void set_bus_mode(BusMode mode) {
        switch (mode) {
            case BusMode::ThreeWire:
                m_mode |= SPI_3WIRE;
                break;
            case BusMode::FourWire:
                m_mode &= ~SPI_3WIRE;
                break;
        }
        if (ioctl(*fd, SPI_IOC_WR_MODE, &m_mode) < 0) {
            throw std::runtime_error(fmt::format("Unable to set SPI mode: {}", strerror(errno)));
        }
    }

    void set_interval(uint16_t us) { tr.delay_usecs = us; }

    Gpio::Output &reset;
    Gpio::Output &control;
    Gpio::Output &select;
    Gpio::Input &busy;

    std::unique_ptr<File> fd;
    spi_ioc_transfer tr;
    uint16_t m_mode = 0;
};

} // namespace spi
