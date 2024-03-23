#pragma once

#include <chrono>
#include <fmt/ranges.h>
#include <linux/spi/spidev.h>
#include <linux/types.h>
#include <ranges>
#include <span>
#include <string.h>
#include <string_view>
#include <sys/ioctl.h>
#include <thread>
#include <vector>

#include "common.hpp"
#include "gpio.hpp"
#include "file.hpp"

namespace hwif {

struct Pins {
    gpio::Output reset;
    gpio::Output control;
    gpio::Input busy;
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
    LutVcom = 0x20,
    LutBlue = 0x21,
    LutWhite = 0x22,
    LutGray1 = 0x23,
    LutGray2 = 0x24,
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

struct Hwif {
    Hwif(const Options &options, Pins &pins) : pins(pins), options(options) {
        fd = options.is_dry() ? nullptr
                              : std::make_unique<File>(options.spi_device, O_RDWR | O_SYNC);
        if (options.is_dry()) {
            return;
        }

        uint8_t bits = 8;
        ioctl(*fd, SPI_IOC_WR_BITS_PER_WORD, &bits);

        set_mode(SPI_MODE_0);
        set_chip_select(ChipSelect::Low);
        set_speed(10'000'000);
    }

    template <typename T> void send(Command cmd, T data) {
        send(static_cast<uint8_t>(cmd), data);
    }

    void send(Command cmd, std::span<uint8_t> data) {
        send(static_cast<uint8_t>(cmd), data);
    }

    void send(Command cmd, std::initializer_list<uint8_t> data) {
        send(static_cast<uint8_t>(cmd), data);
    }

    void send(Command cmd) {
        send(static_cast<uint8_t>(cmd));
    }

    void reset() {
        log("Resetting screen");

        using namespace std::literals::chrono_literals;
        pins.reset.deactive();
        std::this_thread::sleep_for(20ms);
        pins.reset.activate();
        std::this_thread::sleep_for(2ms);
        pins.reset.deactive();
        std::this_thread::sleep_for(20ms);
    }

    void wait_for_idle() {
        if (options.is_dry()) {
            return;
        }
        pins.busy.wfi();
    }

  private:
    enum class ChipSelect {
        High,
        Low,
    };

    enum class BitOrder {
        LsbFirst,
        MsbFirst,
    };

    enum class BusMode {
        ThreeWire,
        FourWire,
    };

    /**
     * Send command followed by data
     */
    void send(uint8_t cmd, std::span<uint8_t> data) {
        send(cmd);
        transfer(data.data(), data.size());
    }

    /**
     * Send command followed by data
     */
    void send(uint8_t cmd, std::initializer_list<uint8_t> data) {
        send(cmd);
        transfer(data.begin(), data.size());
    }

    /**
     * Send command
     */
    void send(uint8_t cmd) {
        pins.control.activate();
        transfer(&cmd, 1);
        pins.control.deactive();
    }

    /**
     * Transfer data over SPI by sending it as chunks
     */
    void transfer(const uint8_t *data, size_t size) {
        std::vector<uint8_t> buffer(data, data + size);
        log("writing {} bytes: {}", size, buffer | std::views::take(16));

        if (options.is_dry()) {
            return;
        }

        static constexpr size_t chunk_size = 64;
        size_t written = 0;
        while (written < size) {
            size_t win = std::min(size - written, chunk_size);
            if (write(*fd, buffer.data() + written, win) < 0) {
                throw std::runtime_error(
                    fmt::format("Unable to write data to SPI: {}", strerror(errno)));
            }
            written += win;
        }
    }

    /**
     * Set transfer speed
     */
    void set_speed(uint32_t speed) {
        if (options.is_dry()) {
            return;
        }

        if (ioctl(*fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
            throw std::runtime_error(fmt::format("Unable to set write speed: {}", strerror(errno)));
        }
        if (ioctl(*fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed) < 0) {
            throw std::runtime_error(fmt::format("Unable to set read speed: {}", strerror(errno)));
        }
    }

    /**
     * Set bits per word for reading and writing over SPI
     */
    void set_bits_per_word() {
        if (options.is_dry()) {
            return;
        }

        uint8_t bits = 8;
        if (ioctl(*fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0) {
            throw std::runtime_error(
                fmt::format("Unable to set bits per word: {}", strerror(errno)));
        }
        if (ioctl(*fd, SPI_IOC_RD_BITS_PER_WORD, &bits) < 0) {
            throw std::runtime_error(
                fmt::format("Unable to set bits per word: {}", strerror(errno)));
        }
    }

    void set_mode(int mode) {
        if (options.is_dry()) {
            return;
        }

        m_mode &= 0xFC;
        m_mode |= mode;
        if (ioctl(*fd, SPI_IOC_WR_MODE, &m_mode) < 0) {
            throw std::runtime_error(fmt::format("Unable to set hwif mode: {}", strerror(errno)));
        }
    }

    void set_chip_select(ChipSelect mode) {
        if (options.is_dry()) {
            return;
        }

        switch (mode) {
            case ChipSelect::High: {
                m_mode |= SPI_CS_HIGH;
                m_mode &= ~SPI_NO_CS;
                break;
            }
            case ChipSelect::Low: {
                m_mode &= ~SPI_CS_HIGH;
                m_mode &= ~SPI_NO_CS;
                break;
            }
        }
        if (ioctl(*fd, SPI_IOC_WR_MODE, &m_mode) < 0) {
            throw std::runtime_error(fmt::format("Unable to set hwif cs: {}", strerror(errno)));
        }
    }

    void set_bus_mode(BusMode mode) {
        if (options.is_dry()) {
            return;
        }

        switch (mode) {
            case BusMode::ThreeWire:
                m_mode |= SPI_3WIRE;
                break;
            case BusMode::FourWire:
                m_mode &= ~SPI_3WIRE;
                break;
        }
        if (ioctl(*fd, SPI_IOC_WR_MODE, &m_mode) < 0) {
            throw std::runtime_error(
                fmt::format("Unable to set hwif bus mode: {}", strerror(errno)));
        }
    }

    Pins &pins;

    std::unique_ptr<File> fd;
    uint16_t m_mode = 0;

    const Options &options;
    const Logger log = options.get_logger(Logger::Facility::Hwif);
};

} // namespace hwif
