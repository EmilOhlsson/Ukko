#include <fmt/core.h>

#include "display.hpp"
#include "gpio.hpp"
#include "spi.hpp"

int main() {
    fmt::print("Running\n");

    spi::Pins control_pins{
        .reset = Gpio::Output(Gpio::Active::Low, 17),
        .control = Gpio::Output(Gpio::Active::Low, 25),
        .select = Gpio::Output(Gpio::Active::Low, 8),
        .busy = Gpio::Input(24),
    };

    spi::Spi spi("/dev/spidev0.0", control_pins);
    Display display(spi);
    display.init();

    // TODO hourly fetch weather data and push to display
}
