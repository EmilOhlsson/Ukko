#pragma once

#include <algorithm>

#include "hwif.hpp"

template <typename T> constexpr T div_ceil(T n, T d) {
    return (n + d - 1) / d;
}

struct Display {
    Display(hwif::hwif &hwif) : hwif(hwif) {}

    void init() {
        using namespace std::literals::chrono_literals;
        using namespace hwif;
        // Set CS deactive();
        // Reset
        // Reset
        //  - reset pin high
        //  - delay 20 ms
        //  - reset pin low
        //  - delay 2 ms
        //  - reset pin high
        //  - delay 20 ms

        hwif.reset();
        /* Table in data sheet is a bit odd. First line marked with blue text is address
         * the lines below marks parameters, and valid bits for the different bytes */
        hwif.send(Command::PowerSettings, {0x17, 0x17, 0x3f, 0x3f, 0x11}); // Power setting
        hwif.send(Command::VCOMDCSetting, {0x24});                         // VCOM
        hwif.send(Command::BoosterSoftStart, {0x27, 0x27, 0x2f, 0x17});    // Booster soft start
        hwif.send(Command::PLLControl, {0x06});                            // PLL control
        hwif.send(Command::PowerOn);                                       // Power on
        std::this_thread::sleep_for(100ms);
        hwif.wait_for_idle();
        hwif.send(Command::PanelSettings, {0x3f});                       // Panel settings
        hwif.send(Command::ResolutionSetting, {0x03, 0x20, 0x01, 0xe0}); // Resolution settings
        hwif.send(Command::DualSPI, {0x00});                             // Dual SPI
        hwif.send(Command::VCOMDataIntervalSetting, {0x10, 0x00});       // VCOM and data interval
        hwif.send(Command::TCONSetting, {0x22});                         // TCON
        hwif.send(Command::GateSourceStartSetting, {0x00, 0x00, 0x00, 0x00}); // Gate/source start
        {
            // LUT setup
            hwif.send(0x20,
                      {
                          // LUT VCOM
                          0x0, 0xF, 0xF, 0x0, 0x0, 0x1, 0x0, 0xF, 0x1, 0xF, 0x1, 0x2, 0x0, 0xF,
                          0xF, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                          0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                      });
            hwif.send(0x21,
                      {
                          // LUT WW
                          0x10, 0xF, 0xF, 0x0, 0x0, 0x1, 0x84, 0xF, 0x1, 0xF, 0x1, 0x2, 0x20, 0xF,
                          0xF,  0x0, 0x0, 0x1, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0,
                          0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0,
                      });
            hwif.send(0x22,
                      {
                          // LUT BW
                          0x10, 0xF, 0xF, 0x0, 0x0, 0x1, 0x84, 0xF, 0x1, 0xF, 0x1, 0x2, 0x20, 0xF,
                          0xF,  0x0, 0x0, 0x1, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0,
                          0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0,
                      });
            hwif.send(0x23,
                      {
                          // LUT WB
                          0x80, 0xF, 0xF, 0x0, 0x0, 0x1, 0x84, 0xF, 0x1, 0xF, 0x1, 0x2, 0x40, 0xF,
                          0xF,  0x0, 0x0, 0x1, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0,
                          0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0,

                      });
            hwif.send(0x24,
                      {
                          // LUT BB
                          0x80, 0xF, 0xF, 0x0, 0x0, 0x1, 0x84, 0xF, 0x1, 0xF, 0x1, 0x2, 0x40, 0xF,
                          0xF,  0x0, 0x0, 0x1, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0,
                          0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0,
                      });
        }
    }

    void clear() {
        std::ranges::fill(fb, 0xFF);
        hwif.send(0x10, fb); // Display start transmission
        std::ranges::fill(fb, 0x00);
        hwif.send(0x13, fb); // Display start transmission2
        turn_on_display();
    }

    void turn_on_display() {
        using namespace std::literals::chrono_literals;
        hwif.send(hwif::Command::DisplayRefresh); // Display refresh
        std::this_thread::sleep_for(100ms);       // Sample code claims this to be needed
        hwif.wait_for_idle();
    }

    void display_test_pattern() {
        using namespace hwif;
        // Note
        hwif.send(Command::DisplayStartTransmission2, fb);
    }

  private:
    hwif::hwif &hwif;
    static constexpr uint32_t width = 800;
    static constexpr uint32_t height = 480;

    // Frame buffer. Note that each pixel is 1 bit, so each element is 8 pixels
    std::vector<uint8_t> fb = std::vector<uint8_t>(div_ceil<uint32_t>(width, 8) * height);
};
