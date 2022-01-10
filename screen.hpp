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

        double min = std::numeric_limits<double>::max();
        double max = std::numeric_limits<double>::min();

        for (const auto &dp : dps) {
            max = std::max<double>(dp.temperature, max);
            min = std::min<double>(dp.temperature, min);
        }

        double norm_div = max - min;
        double vertical_span = 480 - 2 * 48;
        double scale = vertical_span / norm_div;
        double offset = 48 - min * scale;
        double steps = dps.size() - 1;
        double step_size = WIDTH / steps;

        fmt::print("min={}, max={}, scale={} offset={}\n", min, max, scale, offset);

        context->set_line_width(2.0);
        // context->move_to(0, dps[0].temperature / norm_div * surface->get_height());
        context->move_to(0, dps[0].temperature * scale + offset);

        for (size_t i = 1; i < dps.size(); i++) {
            fmt::print(" Drawing at {}\n", dps[i].temperature * scale + offset);
            context->line_to(step_size * i, dps[i].temperature * scale + offset);
        }
        context->stroke();

        if (filename) { surface->write_to_png(*filename); }
    }
};
