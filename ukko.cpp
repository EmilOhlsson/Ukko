#include <fmt/core.h>

#include "display.hpp"
#include "gpio.hpp"
#include "hwif.hpp"

int main() {
    using namespace std::literals::chrono_literals;
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
    display.clear();
    std::vector<uint8_t> buffer(800 * 480);
    std::ranges::fill(buffer, 0x99);
    display.set_fb(buffer.data(), buffer.size());
    display.draw_fb();
    std::this_thread::sleep_for(10s);

    display.clear();
    std::this_thread::sleep_for(10s);
    display.sleep();
    std::this_thread::sleep_for(2s);
    // Teardown should clean up the rest

    // TODO hourly fetch weather data and push to display
    return 0;
}
