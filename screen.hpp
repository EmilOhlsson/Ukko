#pragma once

#include <cairomm/context.h>
#include <cairomm/enums.h>
#include <cairomm/surface.h>
#include <cairommconfig.h>
#include <fmt/core.h>

#include "weather.hpp"

class Screen {
    static constexpr int WIDTH = 800;
    static constexpr int HEIGHT = 480;
    static constexpr Cairo::Format FORMAT = Cairo::Format::FORMAT_A1;

    /* It's worth pointing out that using the A1 format then only alpha channel
     * will be used to draw pixels. As alpha is additive there is no way to
     * draw black on white, so just mentally invert the image */
    Cairo::RefPtr<Cairo::ImageSurface> surface = Cairo::ImageSurface::create(FORMAT, WIDTH, HEIGHT);
    Cairo::RefPtr<Cairo::Context> context = Cairo::Context::create(surface);

    /* Filename to store image in */
    const std::optional<std::string> &filename;

  public:
    Screen(const std::optional<std::string> &filename) : filename(filename) {}

    void draw(const std::vector<weather::Hour> &dps) {
        context->set_source_rgba(0.0, 0.0, 0.0, 1.0);

        const size_t samples = std::min<size_t>(dps.size(), 12);

        // TODO Encapsulate this block here to allow const max and min
        double min = std::numeric_limits<double>::max();
        double max = std::numeric_limits<double>::min();
        for (size_t i = 0; i < samples; i++) {
            max = std::max<double>(dps[i].temperature, max);
            min = std::min<double>(dps[i].temperature, min);
        }

        //fmt::print("min={}, max={}, scale={} offset={}\n", min, max, scale, offset);

        // TODO draw zero line, and level lines, and limit to coming 12hoursjj

        const double steps = samples - 1;
        const double step_size = WIDTH / steps;

        const auto &temperature_to_y = [&](double temperature) -> double {
            const double norm_div = max - min;
            const double vertical_span = HEIGHT - 2 * 48;
            const double scale = vertical_span / norm_div;
            const double offset = 48 - min * scale;

            return HEIGHT - (temperature * scale + offset) - 1;
        };

        /* TODO: Grading, there should never be more than 7-9 lines. If range is including, or
         * "close" to zero, there should be a zero line. (Close meaning wihin 1°C of zero)
         *
         * lines should never be closer than 1°C, and not more than 5°C
         */

        /* Zero line */
        context->set_line_width(2.0);
        context->move_to(0, temperature_to_y(0));
        context->line_to(WIDTH - 1, temperature_to_y(0));
        context->stroke();

        /* -1 */
        context->set_line_width(1.0);
        context->move_to(0, temperature_to_y(-1));
        context->line_to(WIDTH - 1, temperature_to_y(-1));
        context->stroke();

        // TODO: make line dashed
        // TODO: This is upside down
        context->set_line_width(4.0);
        context->move_to(0, temperature_to_y(dps[0].temperature));
        for (size_t i = 1; i < samples; i++) {
            fmt::print(" Drawing {} as at {}\n", dps[i].temperature, temperature_to_y(dps[i].temperature));
            context->line_to(step_size * i, temperature_to_y(dps[i].temperature));
        }
        context->stroke();

        if (filename) {
            surface->write_to_png(*filename);
        }
    }
};
