#pragma once

#include <cairomm/context.h>
#include <cairomm/enums.h>
#include <cairomm/surface.h>
#include <cairommconfig.h>
#include <fmt/core.h>

#include "weather.hpp"

class Screen {
    Cairo::RefPtr<Cairo::ImageSurface> surface =
        Cairo::ImageSurface::create(Cairo::Format::FORMAT_A1, 800, 480);
    Cairo::RefPtr<Cairo::Context> context = Cairo::Context::create(surface);

  public:
    void draw() {
        context->save();
        // context->set_source_rgba(1.0, 1.0, 1.0, 1.0);
        // context->paint();
        context->restore();
        context->set_line_width(5.0);
        context->set_source_rgba(0.0, 0.0, 0.0, 1.0);

        context->move_to(surface->get_width() / 4.0, surface->get_height() / 4.0);
        context->line_to(surface->get_width() * 3.0 / 4.0, surface->get_height() * 3.0 / 4.0);
        context->stroke();

        // fmt::print("Surface is {} bytes per row, with {} rows\n", surface->get_stride(),
        //         surface->get_height());
    }

    void store() { surface->write_to_png("image.png"); }

    void update(const std::vector<weather::Hour> &dps) {
        double min = 0;
        double max = 0;
        for (const auto &dp : dps) {
            max = std::max<double>(dp.temperature, max);
            min = std::min<double>(dp.temperature, min);
        }

        double norm_div = max - min;
        double steps = dps.size() - 1;

        context->move_to(0, dps[0].temperature / norm_div * surface->get_height());
        for (size_t i = 1; i < dps.size(); i++) {
            context->line_to((surface->get_width() / steps) * i,
                             dps[i].temperature / norm_div * surface->get_height());
        }
    }
};
