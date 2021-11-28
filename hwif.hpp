#pragma once

#include <chrono>
#include <fmt/ranges.h>
#include <linux/spi/spidev.h>
#include <linux/types.h>
#include <span>
#include <string.h>
#include <string_view>
#include <sys/ioctl.h>
#include <thread>
#include <vector>

#include "gpio.hpp"

namespace hwif {

struct Pins {
    Gpio::Output reset;
    Gpio::Output control;
    Gpio::Output select;
    Gpio::Input busy;
};

struct hwif {
    hwif(std::string_view device, Pins &pins) : pins(pins) {
        // Open File O_RDWR
        // uint8_t bits = 8;
        // ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
        // Check DEV_HARDWARE_SPI_begin()
        fd = std::make_unique<File>(device, O_RDWR);

        set_mode(SPI_MODE_0);
        set_chip_select(CS_Mode::Low);
        set_bit_order(BitOrder::LsbFirst);
        set_speed(10000000);
        set_interval(0);
    }

    void send(uint8_t cmd, std::span<uint8_t> data) {
        auto selected = pins.select.keep_active();
        send(cmd);
        transfer(data.data(), data.size());
    }

    void send(uint8_t cmd, std::initializer_list<uint8_t> data) {
        auto selected = pins.select.keep_active();
        send(cmd);
        transfer(data.begin(), data.size());
    }

    void send(uint8_t cmd) {
        auto selected = pins.select.keep_active();
        {
            auto ctrl = pins.control.keep_active();
            transfer(&cmd, 1);
        }
    }

    void reset() {
        using namespace std::literals::chrono_literals;
        pins.reset.deactive();
        std::this_thread::sleep_for(20ms);
        pins.reset.activate();
        std::this_thread::sleep_for(2ms);
        pins.reset.deactive();
        std::this_thread::sleep_for(20ms);
    }

    void wait_for_idle() { pins.busy.wfi(); }

  private:
    void transfer(const uint8_t *data, size_t size) {
        std::vector<uint8_t> buffer(data, data + size);
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

    // TODO: The functions below can be merged to pretty much one IOCTL
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
            throw std::runtime_error(fmt::format("Unable to set hwif mode: {}", strerror(errno)));
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
            throw std::runtime_error(fmt::format("Unable to set hwif mode: {}", strerror(errno)));
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
            throw std::runtime_error(fmt::format("Unable to set hwif mode: {}", strerror(errno)));
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
            throw std::runtime_error(fmt::format("Unable to set hwif mode: {}", strerror(errno)));
        }
    }

    void set_interval(uint16_t us) { tr.delay_usecs = us; }

    Pins &pins;

    std::unique_ptr<File> fd;
    spi_ioc_transfer tr;
    uint16_t m_mode = 0;
};

} // namespace hwif
