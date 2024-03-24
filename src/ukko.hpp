#pragma once

#include "display.hpp"
#include "forecast.hpp"
#include "gpio.hpp"
#include "hwif.hpp"
#include "netatmo.hpp"
#include "screen.hpp"
#include "settings.hpp"

struct Ukko {
    Ukko(Settings &&settings)
        : log{settings.get_logger(Logger::Facility::Ukko, true)}
        , debug{settings.get_logger(Logger::Facility::Ukko)}
        , settings{settings}
	, screen{settings}
	, control_pins {
		.reset { gpio::Output(settings, gpio::Active::Low, 17, "eink-reset") },
		.control { gpio::Output(settings, gpio::Active::Low, 25, "eink-control") },
		.busy { gpio::Input(settings, 24, "eink-busy") },
	}
	,hwif{settings, control_pins}
	,display{settings, hwif}
	,weather_service{settings}
	,forecast_service{settings}
	,position {settings.position} {
    }

    /** Main program loop */
    int run();

  private:
    Logger log;
    Logger debug;
    Settings settings;

    /* Prepare screen and interfaces */
    Screen screen;
    hwif::Pins control_pins;
    hwif::Hwif hwif;
    Display display;

    /* Set up weather service handlers */
    Weather weather_service;
    Forecast forecast_service;

    /* Data points */
    std::chrono::time_point<std::chrono::system_clock> forecast_time{};
    std::chrono::time_point<std::chrono::system_clock> weather_time{};

    std::optional<Position> position;
    std::optional<Weather::MeasuredData> weather_data;
    std::optional<std::vector<Forecast::DataPoint>> forecast_data;
};
