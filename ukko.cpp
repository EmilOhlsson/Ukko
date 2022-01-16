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
    std::optional<std::string> screen_store{};
    bool verbose;
    bool dry_run = DUMMY;
};

int run(const Options &);

int main(int argc, char **argv) {
    int c;

    const static option options_available[] = {{"dry-run", no_argument, nullptr, 'd'},
                                               {"verbose", no_argument, nullptr, 'v'},
                                               {"help", no_argument, nullptr, 'h'},
                                               {"store-forecast", required_argument, nullptr, 's'},
                                               {"load-forecast", required_argument, nullptr, 'l'},
                                               {"store-screen", required_argument, nullptr, 'p'},
                                               {}};

    Options options_used{};

    while (true) {
        int option_index = 0;
        c = getopt_long(argc, argv, "dhvs:l:p:", &options_available[0], &option_index);
        if (c == -1) { break; }

        switch (c) {
            case 'd':
                options_used.dry_run = true;
                break;

            case 'v':
                fmt::print("Using verbose mode\n");
                break;

            default:
            case 'h':
                fmt::print("Usage: ukko [flags]\n");
                fmt::print(" -d | --dry-run                Do now write to HW interfaces\n");
                fmt::print(" -h | --help                   Print this message and exit\n");
                fmt::print(" -v | --verbose                Run in verbose mode\n");
                fmt::print(
                    " -l | --load-forecast <file>   Load forecast data from json formatted file\n");
                fmt::print(
                    " -s | --store-forecast <file>  Store forecast data to json formatted file\n");
                fmt::print(" -p | --store-scrren <file>    Store screen as image file\n");
                exit(c != 'h');

            case 's':
                fmt::print("Store forecast data to {}\n", optarg);
                options_used.forecast_store = optarg;
                break;

            case 'l':
                fmt::print("Load forecast data from {}\n", optarg);
                options_used.forecast_load = optarg;
                break;

            case 'p':
                fmt::print("Store screen image to {}\n", optarg);
                options_used.screen_store = optarg;
        }
    }

    return run(options_used);
}

int run(const Options &options) {
    // TODO: Read parameters
    //  - Draw forecast in screen

    fmt::print("Using dry_run={}\n", options.dry_run);

    weather weather{options.forecast_load, options.forecast_store};

    std::vector<weather::Hour> dps = weather.retrieve();

    Screen screen{options.screen_store};
    screen.draw(dps);

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
