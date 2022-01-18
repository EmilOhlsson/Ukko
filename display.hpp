#pragma once

#include <algorithm>
#include <assert.h>
#include <cairomm/surface.h>
#include <fmt/os.h>

#include "hwif.hpp"
#include "utils.hpp"

struct Display {
    Display(hwif::Hwif &hwif) : hwif(hwif) {
    }

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
        std::vector<uint8_t> buffer = std::vector<uint8_t>(IMG_SIZE);
        std::ranges::fill(buffer, 0xFF);
        hwif.send(Command::DisplayStartTransmission1, buffer); // Display start transmission
        std::ranges::fill(buffer, 0x00);
        fmt::print(" Writing clear\n");
        hwif.send(Command::DisplayStartTransmission2, buffer); // Display start transmission2
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
        // std::vector<uint8_t> buffer(800 * 480);
        // std::ranges::fill(buffer, 0x99);
        //  TODO: This sends more data than should probably be needed.
        // hwif.send(hwif::Command::DisplayStartTransmission2, buffer);
        hwif.send(hwif::Command::DisplayStartTransmission2, fb);
        turn_on_display();

        for (uint32_t i = 0; i < IMG_SIZE; i++) {
            if (fb[i] != 0) {
                fmt::print("Drawing: set bit at {}\n", i);
            }
        }
        // TODO: Render to ppm file for viewing
        auto out = fmt::output_file("fb.ppm");
        out.print("P1\n");
        out.print("{} {}\n", 800, 480);

        for (uint32_t row = 0; row < HEIGHT; row++) {
            for (uint32_t col = 0; col < STRIDE; col++) {
                for (uint32_t bit = 0; bit < 8; bit++) {
                    uint8_t byte = fb[row * STRIDE + col];
                    out.print("{} ", (byte >> bit) & 1);
                }
            }
            out.print("\n");
        }
    }

    ///**
    // * Draw data to frame buffer
    // */
    // void set_fb(const uint8_t *data, size_t size) {
    //    // TODO THIS HAS A BUG!!!!!!
    //    assert(size <= width * height);
    //    assert(size % bpb == 0);
    //    for (uint32_t i = 0; i < size; i += 8) {
    //        uint8_t page = 0;
    //        for (uint32_t px = 0; px < bpb; px += 1) {
    //            page <<= 1;
    //            page |= data[i + px] ? 1 : 0;
    //        }
    //        fb[i / bpb] = page;
    //    }
    //}

    // TODO temporary
    void render(const uint8_t *data) {
        assert(fb.size() >= IMG_SIZE);
        memcpy(&fb[0], data, IMG_SIZE);
    }

  private:
    hwif::Hwif &hwif;

    // Frame buffer. Note that each pixel is 1 bit, so each element is 8 pixels
    std::vector<uint8_t> fb = std::vector<uint8_t>(IMG_SIZE);
};
