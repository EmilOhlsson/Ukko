#include <fmt/core.h>

#include "display.hpp"
#include "gpio.hpp"
#include "hwif.hpp"
#include "something.hpp"
#include "net-stuff.hpp"

int main() {
    do_stuff_with_cairo();
    do_curl_stuff();
    exit(0);


    using namespace std::literals::chrono_literals;
    fmt::print("Running\n");

    hwif::Pins control_pins{
        .reset = gpio::Output(gpio::Active::Low, 17),
        .control = gpio::Output(gpio::Active::Low, 25),
        .busy = gpio::Input(24),
    };

    // TODO make it possible to toggle running on x86, without risking
    // toggling of GPIOs
    hwif::Hwif m_hwif("/dev/spidev0.0", control_pins);
    Display display(m_hwif);

    fmt::print("Initializing display\n");
    display.init();
    fmt::print("Clearing display\n");
    display.clear();
    std::this_thread::sleep_for(500ms);

    std::vector<uint8_t> buffer(800 * 480);
    std::ranges::fill(buffer, 0x99);
    fmt::print("Setting up framebuffer\n");
    assert(buffer.size() == 800 * 480);
    fmt::print("buffer size is: {}\n", buffer.size());
    display.clear();
    std::this_thread::sleep_for(500ms);
    display.set_fb(buffer.data(), buffer.size());
    fmt::print("Drawing framebuffer\n");
    display.clear();
    std::this_thread::sleep_for(500ms);
    display.clear();
    std::this_thread::sleep_for(500ms);
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
