#include <fmt/core.h>

#include "display.hpp"
#include "gpio.hpp"
#include "hwif.hpp"
#include "net-stuff.hpp"
#include "screen.hpp"
#include "something.hpp"
#include "weather.hpp"

static constexpr bool DRY_RUN = DUMMY;

int main() {
    fmt::print("Using dry_run={}\n", DRY_RUN);

    // TODO: Read parameters
    //  - Store forecast to file
    //    - Update weather class, and change its name to forecast
    //    - Come up with a better name than "Hours" for forecast data points
    //  - Load forecast to file (to reduce network traffic)
    //  - Draw forecast in screen

    weather weather{};

    std::vector<weather::Hour> dps = weather.retrieve();

    Screen screen{};
    screen.update(dps);
    screen.draw();
    screen.store();

    // do_stuff_with_cairo();
    // do_curl_stuff();

    /* For now, simply bail if we're in dry run mode */
    if (DRY_RUN) { exit(0); }

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
