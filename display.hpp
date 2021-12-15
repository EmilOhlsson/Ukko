#pragma once

#include <algorithm>
#include <assert.h>
#include <cairomm/surface.h>

#include "hwif.hpp"

template <typename T> constexpr T div_ceil(T n, T d) {
    return (n + d - 1) / d;
}

struct Display {
    Display(hwif::Hwif &hwif) : hwif(hwif) {}

    // TODO, this might make more sense to have in hwif
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
            hwif.send(Command::LutVcom,
                      {
                          // LUT VCOM
                          0x0, 0xF, 0xF, 0x0, 0x0, 0x1, 0x0, 0xF, 0x1, 0xF, 0x1, 0x2, 0x0, 0xF,
                          0xF, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                          0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                      });
            hwif.send(Command::LutBlue,
                      {
                          // LUT WW
                          0x10, 0xF, 0xF, 0x0, 0x0, 0x1, 0x84, 0xF, 0x1, 0xF, 0x1, 0x2, 0x20, 0xF,
                          0xF,  0x0, 0x0, 0x1, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0,
                          0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0,
                      });
            hwif.send(Command::LutWhite,
                      {
                          // LUT BW
                          0x10, 0xF, 0xF, 0x0, 0x0, 0x1, 0x84, 0xF, 0x1, 0xF, 0x1, 0x2, 0x20, 0xF,
                          0xF,  0x0, 0x0, 0x1, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0,
                          0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0,
                      });
            hwif.send(Command::LutGray1,
                      {
                          // LUT WB
                          0x80, 0xF, 0xF, 0x0, 0x0, 0x1, 0x84, 0xF, 0x1, 0xF, 0x1, 0x2, 0x40, 0xF,
                          0xF,  0x0, 0x0, 0x1, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0,
                          0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0,

                      });
            hwif.send(Command::LutGray2,
                      {
                          // LUT BB
                          0x80, 0xF, 0xF, 0x0, 0x0, 0x1, 0x84, 0xF, 0x1, 0xF, 0x1, 0x2, 0x40, 0xF,
                          0xF,  0x0, 0x0, 0x1, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0,
                          0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0,
                      });
        }
    }

    void clear() {
        using namespace hwif;
        std::ranges::fill(fb, 0xFF);
        hwif.send(Command::DisplayStartTransmission1, fb); // Display start transmission
        std::ranges::fill(fb, 0x00);
        fmt::print(" Writing clear\n");
        hwif.send(Command::DisplayStartTransmission2, fb); // Display start transmission2
        turn_on_display();
    }

    void turn_on_display() {
        using namespace std::literals::chrono_literals;
        fmt::print(" Turning on display\n");
        hwif.send(hwif::Command::DisplayRefresh); // Display refresh
        std::this_thread::sleep_for(100ms);       // Sample code claims this to be needed
        hwif.wait_for_idle();
    }

    void sleep() {
        using namespace hwif;
        hwif.send(Command::PowerOff);
        hwif.wait_for_idle();
        hwif.send(Command::DeepSleep, {0x07});
    }

    /**
     * Render framebuffer to screen
     */
    void draw_fb() {
        using namespace hwif;
        fmt::print(" Drawing framebuffer\n");
        std::vector<uint8_t> buffer(800 * 480);
        std::ranges::fill(buffer, 0x99);
        hwif.send(hwif::Command::DisplayStartTransmission2, buffer);
        turn_on_display();
    }

    /**
     * Draw data to frame buffer
     */
    void set_fb(const uint8_t *data, size_t size) {
        // TODO THIS HAS A BUG!!!!!!
        assert(size <= width * height);
        assert(size % bpb == 0);
        for (uint32_t i = 0; i < size; i += 8) {
            uint8_t page = 0;
            for (uint32_t px = 0; px < bpb; px += 1) {
                page <<= 1;
                page |= data[i + px] ? 1 : 0;
            }
            fb[i / bpb] = page;
        }
    }

  private:
    hwif::Hwif &hwif;
    static constexpr uint32_t bpb = 8; // Bits per byte
    static constexpr uint32_t width = 800;
    static constexpr uint32_t width_bytes = div_ceil(width, bpb);
    static constexpr uint32_t height = 480;

    // Frame buffer. Note that each pixel is 1 bit, so each element is 8 pixels
    std::vector<uint8_t> fb = std::vector<uint8_t>(width_bytes * height);
};
