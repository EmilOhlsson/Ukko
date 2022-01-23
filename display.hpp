#pragma once

#include <algorithm>
#include <assert.h>
#include <cairomm/surface.h>
#include <fmt/os.h>

#include "hwif.hpp"
#include "utils.hpp"

struct Display {
    Display(const Options &options, hwif::Hwif &hwif) : hwif(hwif), options(options) {
    }

    /**
     * Initialize display
     */
    void init() {
        using namespace std::literals::chrono_literals;
        using namespace hwif;

        hwif.reset();

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

        // LUT setup
        hwif.send(Command::LutVcom,
                  {
                      /* LUT VCOM */
                      0x0, 0xF, 0xF, 0x0, 0x0, 0x1, 0x0, 0xF, 0x1, 0xF, 0x1, 0x2, 0x0, 0xF,
                      0xF, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                      0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                  });
        hwif.send(Command::LutBlue,
                  {
                      /* LUT WW */
                      0x10, 0xF, 0xF, 0x0, 0x0, 0x1, 0x84, 0xF, 0x1, 0xF, 0x1, 0x2, 0x20, 0xF,
                      0xF,  0x0, 0x0, 0x1, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0,
                      0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0,
                  });
        hwif.send(Command::LutWhite,
                  {
                      /* LUT BW */
                      0x10, 0xF, 0xF, 0x0, 0x0, 0x1, 0x84, 0xF, 0x1, 0xF, 0x1, 0x2, 0x20, 0xF,
                      0xF,  0x0, 0x0, 0x1, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0,
                      0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0,
                  });
        hwif.send(Command::LutGray1,
                  {
                      /* LUT WB */
                      0x80, 0xF, 0xF, 0x0, 0x0, 0x1, 0x84, 0xF, 0x1, 0xF, 0x1, 0x2, 0x40, 0xF,
                      0xF,  0x0, 0x0, 0x1, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0,
                      0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0,

                  });
        hwif.send(Command::LutGray2,
                  {
                      /* LUT BB */
                      0x80, 0xF, 0xF, 0x0, 0x0, 0x1, 0x84, 0xF, 0x1, 0xF, 0x1, 0x2, 0x40, 0xF,
                      0xF,  0x0, 0x0, 0x1, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0,
                      0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  0x0,
                  });
    }

    void clear() {
        using namespace hwif;
        std::vector<uint8_t> buffer = std::vector<uint8_t>(IMG_SIZE);

        log("Clearing display");
        std::ranges::fill(buffer, 0xFF);
        hwif.send(Command::DisplayStartTransmission1, buffer);
        std::ranges::fill(buffer, 0x00);
        hwif.send(Command::DisplayStartTransmission2, buffer);
        refresh();
    }

    void refresh() {
        using namespace std::literals::chrono_literals;

        log("Refreshing screen");
        hwif.send(hwif::Command::DisplayRefresh);
        std::this_thread::sleep_for(100ms); // Sample code claims this to be needed
        hwif.wait_for_idle();
    }

    void enter_sleep() {
        using namespace hwif;

        log("Entering sleep");
        hwif.send(Command::PowerOff);
        hwif.wait_for_idle();
        hwif.send(Command::DeepSleep, {0x07});
    }

    /**
     * Render framebuffer to screen
     */
    void draw() {
        using namespace hwif;

        log("Drawing framebuffer to display");
        hwif.send(hwif::Command::DisplayStartTransmission2, fb);
        refresh();

        if (options.render_store) {
            auto out =
                fmt::output_file(fmt::format("{}-{}.ppm", *options.render_store, screen_number));
            out.print("P1\n");
            out.print("{} {}\n", WIDTH, HEIGHT);

            for (uint32_t row = 0; row < HEIGHT; row++) {
                for (uint32_t col = 0; col < STRIDE; col++) {
                    for (uint32_t bit = 0; bit < 8; bit++) {
                        uint8_t byte = fb[row * STRIDE + col];
                        out.print("{} ", (byte >> (7 - bit)) & 1);
                    }
                }
                out.print("\n");
            }
            screen_number += 1;
        }
    }

    /**
     * Render an image to the inernal framebuffer
     */
    void render(const std::span<uint8_t, IMG_SIZE> data) {
        assert(fb.size() >= IMG_SIZE);
        std::ranges::transform(data, begin(fb), [](uint8_t byte) {
            byte = (byte & 0xf0) >> 4 | (byte & ~0xf0) << 4;
            byte = (byte & 0xcc) >> 2 | (byte & ~0xcc) << 2;
            byte = (byte & 0xaa) >> 1 | (byte & ~0xaa) << 1;
            return byte;
        });
    }

  private:
    hwif::Hwif &hwif;

    /* Frame buffer. Note that each pixel is 1 bit, so each element is 8 pixels */
    std::vector<uint8_t> fb = std::vector<uint8_t>(IMG_SIZE);

    const Options &options;
    const Logger log = options.get_logger(Logger::Facility::Display);
    uint32_t screen_number = 0;
};
