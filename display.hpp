#pragma once

#include "spi.hpp"

struct Display {
    Display(spi::Spi &spi) : spi(spi) {}

    void init() {
        // Set CS deactive();
        // Reset
        // Reset
        //  - reset pin high
        //  - delay 20 ms
        //  - reset pin low
        //  - delay 2 ms
        //  - reset pin high
        //  - delay 20 ms
        spi.send(0x01, {0x01, 0x02, 0x03}); // Power setting

        spi.send(0x82, {0x42, 0x42, 0x42}); // VCOM
    }

  private:
    spi::Spi &spi;
};
