#include <fmt/core.h>
#include <getopt.h>
#include <optional>

#include "common.hpp"
#include "display.hpp"
#include "forecast.hpp"
#include "gpio.hpp"
#include "hwif.hpp"
#include "netatmo.hpp"
#include "screen.hpp"

int run(const Options &);

int main(int argc, char **argv) {
    int c;

    const static option options_available[] = {
        {"dry-run", no_argument, nullptr, 'n'},
        {"verbose", no_argument, nullptr, 'v'},
        {"help", no_argument, nullptr, 'h'},
        {"store-forecast", required_argument, nullptr, 'F'},
        {"load-forecast", required_argument, nullptr, 'f'},
        {"store-device-data", required_argument, nullptr, 'D'},
        {"load-device-data", required_argument, nullptr, 'd'},
        {"store-screen", required_argument, nullptr, 'p'},
        {"store-render", required_argument, nullptr, 'r'},
        {"sleep", required_argument, nullptr, 's'},
        {"cycles", required_argument, nullptr, 'c'},
        {"settings", required_argument, nullptr, 'i'},
        {},
    };

    static std::string_view help_text =
        "Usage: ukko [flags]\n"
        " -n | --dry-run                   Do now write to HW interfaces\n"
        " -v | --verbose                   Run in verbose mode\n"
        " -h | --help                      Print this message and exit\n"
        " -i | --settings <file>           Load settings from <file>\n"
        " -d | --load-device-data <file>   Load device data from <file>\n"
        " -D | --store-device-data <file>  Store device data to <file>\n"
        " -f | --store-forecast <file>     Store forecast data to json formatted file\n"
        " -F | --load-forecast <file>      Load forecast data from json formatted file\n"
        " -r | --store-render <file>       Store rendering to file\n"
        " -s | --sleep <minutes>           Number of minutes to sleep between refresh\n"
        " -c | --cycles <cycles>           Number of frames to render before quitting\n"
        "                                  0 cycles means cycle forever\n"
        " -p | --store-screen <file>       Store screen as image file\n";

    Options options_used{};

    while (true) {
        int option_index = 0;
        c = getopt_long(argc, argv, "c:d:D:nvhF:f:p:s:r:i:", &options_available[0], &option_index);
        if (c == -1) {
            break;
        }

        switch (c) {
            case 'c':
                options_used.cycles = atoi(optarg);
                break;

            case 'n':
                options_used.run_mode = RunMode::Dry;
                break;

            case 'v':
                options_used.verbose = true;
                break;

            default:
                fmt::print("Unknown flag: {}\n", argv[optind]);
                [[fallthrough]];

            case 'h':
                fmt::print("{}", help_text);
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

            case 'i':
                options_used.settings_file = optarg;
                break;

            case 'd':
                options_used.netatmo_load = optarg;
                break;

            case 'D':
                options_used.netatmo_store = optarg;
                break;
        }
    }

    if (access(options_used.settings_file.c_str(), R_OK) != 0) {
        fmt::print("Missing settings file {}\n", options_used.settings_file);
        return -1;
    }

    return run(options_used);
}

int run(const Options &options) {
    using namespace std::literals::chrono_literals;

    Logger log = options.get_logger(Logger::Facility::Ukko);

    Screen screen{options};
    hwif::Pins control_pins{
        .reset = gpio::Output(options, gpio::Active::Low, 17, "eink-reset"),
        .control = gpio::Output(options, gpio::Active::Low, 25, "eink-control"),
        .busy = gpio::Input(options, 24, "eink-busy"),
    };
    hwif::Hwif m_hwif(options, control_pins);
    Display display(options, m_hwif);

    Weather weather{options};
    Forecast forecast{options};

    bool refresh = false;
    for (uint32_t i = 0; options.cycles == 0 || i < options.cycles; i++) {
        log("Waking display");
        display.wake_up();

        log("Clearing display");
        display.clear();
        std::this_thread::sleep_for(500ms);

        if (refresh) {
            weather.refresh_authentication();
        }
        Weather::MeasuredData mdp = weather.retrieve();
        const Position pos = weather.get_position();
        // TODO: Handle optional forecast here
        std::vector<Forecast::DataPoint> dps = *forecast.retrieve(pos);
        screen.draw(dps, mdp);

        display.render(screen.get_ptr());
        log("Drawing framebuffer");
        display.draw();

        log("Sleeping for {} minute(s)", options.sleep.count());
        display.enter_sleep();

        const std::chrono::minutes sleep = weather.check_sleep(options.sleep);
        refresh = sleep != options.sleep;
        std::this_thread::sleep_for(sleep);
    }

    return 0;
}
