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
    Gpio::Input busy;
};

enum class Command : uint8_t {
    PanelSettings = 0x00,
    PowerSettings = 0x01,
    PowerOff = 0x02,
    PowerOffSequenceSettings = 0x03,
    PowerOn = 0x04,
    PowerOnMeasures = 0x05,
    BoosterSoftStart = 0x06,
    DeepSleep = 0x07,
    DisplayStartTransmission1 = 0x10,
    DataStop = 0x11,
    DisplayRefresh = 0x12,
    DisplayStartTransmission2 = 0x13,
    DualSPI = 0x15,
    AutoSequence = 17,
    LutVcom = 0x20,  // Is this really on v2?
    LutBlue = 0x21,  // Is this really on v2?
    LutWhite = 0x22, // Is this really on v2?
    LutGray1 = 0x23, // Is this really on v2?
    LutGray2 = 0x23, // Is this really on v2?
    KWLUOption = 0x2B,
    PLLControl = 0x30,
    TemperatureSensorCalibration = 0x40,
    TemperatureSensorSelection = 0x41,
    TemperatureSensorWrite = 0x42,
    TemperatureSensorRead = 0x43,
    PanelBreakCheck = 0x44,
    VCOMDataIntervalSetting = 0x50,
    LowPowerDetection = 0x51,
    EndVoldateSetting = 0x52,
    TCONSetting = 0x60,
    ResolutionSetting = 0x61,
    GateSourceStartSetting = 0x65,
    Revision = 0x70,
    GetStatus = 0x71,
    AutoMeasurementVCOM = 0x80,
    ReadVCOMValue = 0x81,
    VCOMDCSetting = 0x82,
    PartialWindow = 0x90,
    PartialIn = 0x91,
    PartialOut = 0x92,
    ProgramMode = 0xA0,
    ActiveProgramming = 0xA1,
    ReadOTP = 0xA2,
    CascadeSetting = 0xE0,
    PowerSaving = 0xE3,
    LVDVoltageSelect = 0xE4,
    ForceTemperature = 0xE5,
    TemperatureBoundryPhaseC2 = 0xE7,

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

    template <typename T> void send(Command cmd, T data) { send(static_cast<uint8_t>(cmd), data); }
    void send(Command cmd, std::span<uint8_t> data) { send(static_cast<uint8_t>(cmd), data); }
    void send(Command cmd, std::initializer_list<uint8_t> data) {
        send(static_cast<uint8_t>(cmd), data);
    }
    void send(Command cmd) { send(static_cast<uint8_t>(cmd)); }

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
    void send(uint8_t cmd, std::span<uint8_t> data) {
        send(cmd);
        transfer(data.data(), data.size());
    }

    void send(uint8_t cmd, std::initializer_list<uint8_t> data) {
        send(cmd);
        transfer(data.begin(), data.size());
    }

    void send(uint8_t cmd) {
        {
            auto ctrl = pins.control.keep_active();
            transfer(&cmd, 1);
        }
    }

    void transfer(const uint8_t *data, size_t size) {
        std::vector<uint8_t> buffer(data, data + size);
        fmt::print("writing {}\n", buffer);
        if (write(*fd, data, size) < 0) {
            throw std::runtime_error(
                fmt::format("Unable to write data to SPI: {}", strerror(errno)));
        }
        // spi_ioc_transfer tr {
        //     .tx_buf = reinterpret_cast<__u64>(data),
        //     .rx_buf = 0,
        //     .len = static_cast<uint32_t>(size),
        // };
        // if (ioctl(*fd, SPI_IOC_MESSAGE(1), &tr) < 0) {
        //     throw std::runtime_error(fmt::format("Unable to write data to SPI: {}",
        //     strerror(errno)));
        // }
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
