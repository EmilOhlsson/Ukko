#include <fmt/core.h>

#include "display.hpp"
#include "gpio.hpp"
#include "hwif.hpp"

int main() {
    fmt::print("Running\n");

    hwif::Pins control_pins{
        .reset = Gpio::Output(Gpio::Active::Low, 17),
        .control = Gpio::Output(Gpio::Active::Low, 25),
        .select = Gpio::Output(Gpio::Active::Low, 8),
        .busy = Gpio::Input(24),
    };

    hwif::hwif hwif("/dev/spidev0.0", control_pins);
    Display display(hwif);
    display.init();

    // TODO hourly fetch weather data and push to display
}
