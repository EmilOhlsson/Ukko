#pragma once

#include <cairomm/context.h>
#include <cairomm/enums.h>
#include <cairomm/surface.h>
#include <cairommconfig.h>
#include <fmt/core.h>

#include <algorithm>
#include <iterator>
#include <ranges>
#include <span>

#include "common.hpp"
#include "utils.hpp"
#include "weather.hpp"

class Screen {
    static constexpr Cairo::Format FORMAT = Cairo::Format::FORMAT_A1;

    /* It's worth pointing out that using the A1 format then only alpha channel
     * will be used to draw pixels. As alpha is additive there is no way to
     * draw black on white, so just mentally invert the image */
    Cairo::RefPtr<Cairo::ImageSurface> surface = Cairo::ImageSurface::create(FORMAT, WIDTH, HEIGHT);
    Cairo::RefPtr<Cairo::Context> context = Cairo::Context::create(surface);

    const Options &options;
    const Logger log = options.get_logger(Logger::Facility::Screen);

    /* Filename to store image in */
    const std::optional<std::string> &filename;

    /**
     * Representation of range from low to high value
     * */
    struct Range {
        Range(double lo, double hi) : lo(lo), hi(hi) {
        }
        double lo;
        double hi;
    };

    /**
     * Conversion object from one linear range to another.
     */
    class Conv {
        double scale;
        double offset;

      public:
        Conv(Range src, Range dst)
            : scale((dst.hi - dst.lo) / (src.hi - src.lo)), offset(dst.lo - src.lo * scale) {
        }

        double operator()(double v) const {
            return v * scale + offset;
        }
    };

  public:
    Screen(const Options &options) : options(options), filename(options.render_store) {
        context->set_source_rgba(0.0, 0.0, 0.0, 1.0);
    }

    void draw(const std::vector<weather::Hour> &dps) {
        log("Drawing data points to screen");
        surface = Cairo::ImageSurface::create(FORMAT, WIDTH, HEIGHT);
        context = Cairo::Context::create(surface);

        /* number of samples, limit to coming 12 h */
        const size_t samples = std::min<size_t>(dps.size(), 12);

        /* Relavant temperature range */
        const auto &get_temp = [](const weather::Hour &dp) -> double { return dp.temperature; };
        const auto temperatures = dps | std::views::take(samples) | std::views::transform(get_temp);
        const auto [minp, maxp] = std::ranges::minmax_element(temperatures);
        const Range temperature_range(*minp, *maxp);

        /* Limit graph area */
        const double graph_x_offset = 50;
        const double graph_width = WIDTH - graph_x_offset;
        const double graph_y_offset = 30;
        const Range graph_y_range(HEIGHT - graph_y_offset, graph_y_offset);

        const double steps = samples - 1;
        const double step_size = graph_width / steps;

        /* Grading: there should never be more than 7-9 lines. If range is including, or
         * "close" to zero, there should be a zero line. (Close meaning wihin 1°C of zero)
         *
         * lines should never be closer than 1°C, and not more than 5°C
         *
         * left of line there are values written */

        /* Calcluate reasonable resolution for graph. If resolution gives too many temperature
         * lines, then reduce temperature until scale works. If temperature resolution need to be
         * higher than 100°C we have a problem... */
        int block_size = 0;
        int blocks = 0;
        const int span = std::ceil(temperature_range.hi - temperature_range.lo);
        constexpr int max_blocks = 7;
        for (const int bsize : {1, 2, 5, 10, 20, 50, 100}) {
            blocks = utils::div_ceil(span, bsize);
            if (blocks < max_blocks) {
                block_size = bsize;
                break;
            }
        }
        assert(block_size > 0);

        /* Calculate a lower line, i.e, min value rounded down to multiple of block size */
        const Range input_range(
            utils::round_down<int>(std::floor(temperature_range.lo), block_size),
            utils::round_up<int>(std::ceil(temperature_range.hi), block_size));

        /* Conversion object that maps input temperature range, to screen pixels */
        const Conv conv{input_range, graph_y_range};

        /* Draw the levels, and annotate them */
        for (int l = input_range.lo; l <= input_range.hi; l++) {
            if (l == 0) {
                /* Make zero line stand out */
                context->set_line_width(2);
                context->unset_dash();
            } else {
                context->set_line_width(1);
                context->set_dash(std::vector{1.0, 5.0}, 0.0);
            }
            context->move_to(graph_x_offset, conv(l));
            context->rel_line_to(graph_width, 0);
            context->stroke();

            /* Print temperature */
            context->move_to(10, conv(l));
            context->show_text(fmt::format("{}°C", l));
            context->stroke();
        }

        /* Draw timestamps below graph */
        double clock_x = graph_x_offset;
        const auto get_time = [](const auto &dp) { return dp.time; };
        const auto timestamps =
            dps | std::views::take(samples - 1) | std::views::transform(get_time);
        constexpr double clock_y = HEIGHT - 10;
        for (const auto &timestamp : timestamps) {
            context->move_to(clock_x, clock_y);
            context->show_text(fmt::format("{:%H:%M}", timestamp));
            context->stroke();
            clock_x += step_size;
        }

        /* Draw temperature curve */
        context->set_line_width(4.0);
        context->unset_dash();
        double graph_x = graph_x_offset;
        context->move_to(graph_x, conv(dps[0].temperature));
        for (const auto temperature : temperatures | std::views::drop(1)) {
            context->line_to(graph_x += step_size, conv(temperature));
        }
        context->stroke();

        if (filename) {
            surface->write_to_png(fmt::format("{}-{}.png", *filename, render_number));
            render_number += 1;
        }
    }

    std::span<uint8_t, IMG_SIZE> get_ptr() {
        return std::span<uint8_t, IMG_SIZE>{surface->get_data(), IMG_SIZE};
    }
    uint32_t render_number = 0;
};
