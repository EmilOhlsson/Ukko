#include <fmt/core.h>
#include <getopt.h>
#include <optional>

#include "common.hpp"
#include "display.hpp"
#include "gpio.hpp"
#include "hwif.hpp"
#include "screen.hpp"
#include "weather.hpp"

int run(const Options &);

int main(int argc, char **argv) {
    int c;

    const static option options_available[] = {
        {"dry-run", no_argument, nullptr, 'd'},
        {"verbose", no_argument, nullptr, 'v'},
        {"help", no_argument, nullptr, 'h'},
        {"store-forecast", required_argument, nullptr, 'F'},
        {"load-forecast", required_argument, nullptr, 'f'},
        {"store-screen", required_argument, nullptr, 'p'},
        {"store-render", required_argument, nullptr, 'r'},
        {"sleep", required_argument, nullptr, 's'},
        {"cycles", required_argument, nullptr, 'c'},
        {},
    };

    Options options_used{};

    while (true) {
        int option_index = 0;
        c = getopt_long(argc, argv, "c:dvhF:f:p:s:r:", &options_available[0], &option_index);
        if (c == -1) {
            break;
        }

        switch (c) {
            case 'c':
                options_used.cycles = atoi(optarg);
                break;

            case 'd':
                options_used.run_mode = RunMode::Dry;
                break;

            case 'v':
                options_used.verbose = true;
                break;

            default:
                fmt::print("Unknown flag: {}\n", argv[optind]);
                [[fallthrough]];

            case 'h':
                fmt::print("Usage: ukko [flags]\n");
                fmt::print(" -d | --dry-run                Do now write to HW interfaces\n");
                fmt::print(" -v | --verbose                Run in verbose mode\n");
                fmt::print(" -h | --help                   Print this message and exit\n");
                fmt::print(
                    " -f | --store-forecast <file>  Store forecast data to json formatted file\n");
                fmt::print(
                    " -F | --load-forecast <file>   Load forecast data from json formatted file\n");
                fmt::print(" -r | --store-render <file>    Store rendering to file\n");
                fmt::print(
                    " -s | --sleep <minutes>        Number of minutes to sleep between refresh\n");
                fmt::print(
                    " -c | --cycles <cycles>        Number of frames to render before quitting\n");
                fmt::print("                               0 cycles means cycle forever\n");
                fmt::print(" -p | --store-screen <file>    Store screen as image file\n");
                exit(c != 'h');

            case 'F':
                options_used.forecast_store = optarg;
                break;

            case 'f':
                options_used.forecast_load = optarg;
                break;

            case 'p':
                options_used.screen_store = optarg;
                break;

            case 's':
                options_used.sleep = std::chrono::minutes{atoi(optarg)};
                break;

            case 'r':
                options_used.render_store = optarg;
                break;
        }
    }

    return run(options_used);
}

int run(const Options &options) {
    using namespace std::literals::chrono_literals;

    Logger log = options.get_logger(Logger::Facility::Ukko);

    weather weather{options};
    Screen screen{options};
    hwif::Pins control_pins{
        .reset = gpio::Output(options, gpio::Active::Low, 17, "eink-reset"),
        .control = gpio::Output(options, gpio::Active::Low, 25, "eink-control"),
        .busy = gpio::Input(options, 24, "eink-busy"),
    };
    hwif::Hwif m_hwif(options, control_pins);
    Display display(options, m_hwif);

    log("Initializing display");
    display.init();

    for (uint32_t i = 0; options.cycles == 0 || i < options.cycles; i++) {
        std::vector<weather::Hour> dps = weather.retrieve();
        screen.draw(dps);

        log("Clearing display");
        display.clear(); // <-- This seem to fail, when waiting for event after
                         //     waiting for !busy after writing {18}, DisplayRefresh
                         //     And it truly doesn't seem to be done!
                         //     But if this is run with the demo software first,
                         //     then this suddenly works! So clearly, something is
                         //     wrong/different before this point
        std::this_thread::sleep_for(500ms);

        log("Setting up framebuffer");

        display.clear();
        std::this_thread::sleep_for(500ms);

        display.render(screen.get_ptr());
        log("Drawing framebuffer");
        display.draw();
        display.enter_sleep();
        std::this_thread::sleep_for(options.sleep);
    }

    return 0;
}
