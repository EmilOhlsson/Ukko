#pragma once

#include "hwif.hpp"

struct Display {
    Display(hwif::hwif &hwif) : hwif(hwif) {}

    void init() {
        using namespace std::literals::chrono_literals;
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
        hwif.send(0x01, {0x17, 0x17, 0x3f, 0x3f, 0x11}); // Power setting
        hwif.send(0x82, {0x24});                         // VCOM
        hwif.send(0x06, {0x27, 0x27, 0x2f, 0x17});       // Booster soft start
        hwif.send(0x30, {0x06});                         // PLL control
        hwif.send(0x04, {});                             // Power on
        std::this_thread::sleep_for(100ms);
        hwif.wait_for_idle();
        hwif.send(0x00, {0x3f});                   // Panel settings
        hwif.send(0x61, {0x03, 0x20, 0x01, 0xe0}); // Resolution settings
        hwif.send(0x15, {0x00});                   // Dual SPI
        hwif.send(0x50, {0x10, 0x00});             // VCOM and data interval
        hwif.send(0x60, {0x22});                   // TCON
        hwif.send(0x65, {0x00, 0x00, 0x00, 0x00}); // Gate/source start
        // TODO Lut
    }

  private:
    hwif::hwif &hwif;
};
