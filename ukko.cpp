#include <fmt/core.h>
#include <getopt.h>
#include <optional>

#include "display.hpp"
#include "gpio.hpp"
#include "hwif.hpp"
#include "net-stuff.hpp"
#include "screen.hpp"
#include "something.hpp"
#include "weather.hpp"

static constexpr bool DRY_RUN = DUMMY;

struct Options {
    std::optional<std::string> forecast_load{};
    std::optional<std::string> forecast_store{};
    bool verbose;
};

int run(const Options &);

int main(int argc, char **argv) {
    fmt::print("Using dry_run={}\n", DRY_RUN);

    int c;

    const static option options_available[] = {
        {"verbose", no_argument, nullptr, 'v'},
        {"help", no_argument, nullptr, 'h'},
        {"store-forecast", required_argument, nullptr, 's'},
        {"load-forecast", required_argument, nullptr, 'l'},
    };

    Options options_used{};

    while (true) {
        int option_index = 0;
        c = getopt_long(argc, argv, "hvs:l:", options_available, &option_index);
        if (c == -1) { break; }

        switch (c) {
            case 'v':
                fmt::print("Using verbose mode\n");
                break;

            case 'h':
                // TODO
                fmt::print("Print usage instructions, and exit\n");
                exit(0);
                break;

            case 's':
                fmt::print("Store forecast data to {}\n", optarg);
                options_used.forecast_store = optarg;
                break;

            case 'l':
                fmt::print("Load forecast data from {}\n", optarg);
                options_used.forecast_load = optarg;
                break;
        }
    }

    return run(options_used);
}

int run(const Options &options) {
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
