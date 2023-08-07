#include <condition_variable>
#include <fmt/core.h>
#include <getopt.h>
#include <mutex>
#include <optional>
#include <thread>

#include "common.hpp"
#include "display.hpp"
#include "forecast.hpp"
#include "gpio.hpp"
#include "hwif.hpp"
#include "message_queue.hpp"
#include "netatmo.hpp"
#include "screen.hpp"
#include "token-storage.hpp"
#include "ukko.hpp"
#include "web-server.hpp"

int main(int argc, char **argv) {
    int c;

    const static option options_available[] = {
        {"dry-run", no_argument, nullptr, 'n'},
        {"verbose", no_argument, nullptr, 'v'},
        {"debug-log", no_argument, nullptr, 'V'},
        {"help", no_argument, nullptr, 'h'},
        {"store-forecast", required_argument, nullptr, 'F'},
        {"load-forecast", required_argument, nullptr, 'f'},
        {"store-device-data", required_argument, nullptr, 'D'},
        {"load-device-data", required_argument, nullptr, 'd'},
        {"store-screen", required_argument, nullptr, 'p'},
        {"store-render", required_argument, nullptr, 'r'},
        {"sleep", required_argument, nullptr, 's'},
        {"weather-frequency", required_argument, nullptr, 'W'},
        {"forecast-frequency", required_argument, nullptr, 'Y'},
        {"cycles", required_argument, nullptr, 'c'},
        {"settings", required_argument, nullptr, 'i'},
        {},
    };

    static std::string_view help_text =
        "Usage: ukko [flags]\n"
        " -n | --dry-run                   Do now write to HW interfaces\n"
        " -v | --verbose                   Run in verbose mode\n"
        " -V | --debug-log                 Print debug messages\n"
        " -h | --help                      Print this message and exit\n"
        " -i | --settings <file>           Load settings from <file>\n"
        " -d | --load-device-data <file>   Load device data from <file>\n"
        " -D | --store-device-data <file>  Store device data to <file>\n"
        " -f | --store-forecast <file>     Store forecast data to json formatted file\n"
        " -F | --load-forecast <file>      Load forecast data from json formatted file\n"
        " -r | --store-render <file>       Store rendering to file\n"
        " -s | --sleep <minutes>           Number of minutes to sleep between refresh\n"
        " -W | --weather-frequency <mins>  Minutes between weather measurements\n"
        " -Y | --forecast-frequency <mins> Minutes between forecast\n"
        " -c | --cycles <cycles>           Number of frames to render before quitting\n"
        "                                  0 cycles means cycle forever\n"
        " -p | --store-screen <file>       Store screen as image file\n";

    Options options_used{};

    while (true) {
        int option_index = 0;
        c = getopt_long(argc, argv, "c:d:D:nvVhF:f:p:s:r:i:W:Y:", &options_available[0],
                        &option_index);
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

            case 'V':
                options_used.debug_log = true;
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

            case 'W':
                options_used.weather_frequency = std::chrono::minutes{atoi(optarg)};
                break;

            case 'Y':
                options_used.forecast_frequency = std::chrono::minutes{atoi(optarg)};
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

    Settings settings{options_used};
    Ukko ukko{std::move(settings)};

    return ukko.run();
}

int Ukko::run() {
    using namespace std::literals::chrono_literals;
    using namespace std::chrono;

    MessageQueue<Auth> queue{settings};
    WebServer wserver{settings, [&](const Auth &auth) { queue.push(auth); }};
    wserver.start();

    /* Main logic loop. This will run for a given amount of cycles, or forever. This will fetch
     * weather measurements and forecast, and update display */
    for (uint32_t i = 0; settings.cycles == 0 || i < settings.cycles; i++) {
        bool update_screen = false;
        const time_point<system_clock> now = system_clock::now();

        /* Check if it's time to fetch new weather metrics */
        if (weather_time + settings.weather_frequency < now) {
            debug("Fetching current weather metrics");
            if (std::optional<Weather::MeasuredData> mdp = weather_service.retrieve()) {
                update_screen = true;
                weather_data = mdp;
                weather_time = now;
                if (not position) {
                    debug("Using position from Netatmo");
                    position = mdp->position;
                }
            } else {
                debug("Was not able to fetch weather metrics, clearing outdated measuerments");
                weather_data = std::nullopt;
            }
        }

        /* NOTE: We can get position either from Netatmo, or from configuration files. If we don't
         * have a position, and we don't have information about how to authenticate we can never
         * learn the position. */
        if (not position && weather_service.can_authenticate()) {
            /* TODO: As this state means we cannot operate, we should notify something on the
             * display indicating the broken state */
            log("Doesn't have a proper position");
            abort();
        }

        /* Fetch forecast if it's time. If we did get a new forecast make sure the display is
         * updated with the new forecast */
        if (forecast_time + settings.forecast_frequency < now) {
            debug("Fetching forecast");
            if (std::optional<std::vector<Forecast::DataPoint>> dps =
                    forecast_service.retrieve(*position)) {
                update_screen = true;
                forecast_data = dps;
                forecast_time = now;
                debug("Will fetch next forecast {}", forecast_time + settings.forecast_frequency);
            } else {
                debug("Was not able to fetch forecast");
            }
        }

        /* We have new information to display */
        if (update_screen) {
            debug("Updating screen with new information");
            screen.draw(forecast_data, weather_data);
            display.draw(screen.get_ptr());
        }

        if (std::optional<Auth> auth = queue.pop(now + settings.sleep)) {
            debug("Have code, will attempt to authenticate: {}", *auth);
            std::ignore = weather_service.authenticate(*auth);
        } else {
            debug("Waited, refreshing authentication");
            std::ignore = weather_service.refresh_authentication();
        }
    }

    return 0;
}
