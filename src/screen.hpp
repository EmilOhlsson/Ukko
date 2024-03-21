#pragma once

#include <cairomm/context.h>
#include <cairomm/enums.h>
#include <cairomm/surface.h>
#include <cairommconfig.h>
#include <fmt/chrono.h>
#include <fmt/core.h>
#include <fmt/ranges.h>

#include <algorithm>
#include <iterator>
#include <ranges>
#include <span>

#include "common.hpp"
#include "forecast.hpp"
#include "netatmo.hpp"
#include "utils.hpp"

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
     * Representation of range from low to high value.
     *
     * The low-value does not need to smaller than the high value.
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

    /**
     * Represent an area
     */
    class Area {
        Range x;
        Range y;

      public:
        Area(Range x, Range y) : x(x), y(y) {
        }

        double left() const {
            return x.lo;
        }
        double right() const {
            return x.hi;
        }
        double width() const {
            return abs(x.hi - x.lo);
        }

        double top() const {
            return y.lo;
        }
        double bottom() const {
            return y.hi;
        }
        double height() const {
            return abs(y.hi - y.lo);
        }
    };

    /**
     * Draw forecast on given area
     */
    void draw_forecast(const std::vector<Forecast::DataPoint> &dps, const Area &area) {
        /* number of samples, limit to coming 12 h */
        const size_t samples = std::min<size_t>(dps.size(), 12);

        /* Relavant temperature range */
        const auto &get_temp = [](const Forecast::DataPoint &dp) -> double {
            return dp.temperature;
        };
        const auto temperatures = dps | std::views::take(samples) | std::views::transform(get_temp);
        const auto [minp, maxp] = std::ranges::minmax_element(temperatures);
        const Range temperature_range(*minp, *maxp);

        /* Limit graph area */
        const double graph_x_offset = area.left() + 50;
        const double graph_width = area.width() - 50;
        const double graph_y_offset = area.top() + 30;
        const Range graph_y_range(area.bottom() - 30 - 30 - 30 - 30, graph_y_offset);

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
        static constexpr int max_blocks = 5;
        for (const int bsize : {1, 2, 5, 10, 20, 50, 100}) {
            blocks = utils::div_ceil(span, bsize);
            if (blocks < max_blocks) {
                block_size = bsize;
                break;
            }
        }
        assert(block_size > 0);

        /* Calculate a lower line, i.e, min value rounded down to multiple of block size */
        int lo_line = utils::round_down<int>(std::floor(temperature_range.lo), block_size);
        int hi_line = utils::round_up<int>(std::ceil(temperature_range.hi), block_size);

        /* Make sure there are at least four lines */
        if (hi_line - lo_line <= 1) {
            lo_line -= 1;
            hi_line += 1;
        }
        const Range input_range(lo_line, hi_line);

        /* Conversion object that maps input temperature range, to screen pixels */
        const Conv conv{input_range, graph_y_range};

        context->set_font_size(20.0);
        context->select_font_face("cairo:sans-serif", Cairo::FONT_SLANT_NORMAL,
                                  Cairo::FONT_WEIGHT_NORMAL);
        /* Draw the levels, and annotate them */
        for (int l = input_range.lo; l <= input_range.hi; l += block_size) {
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
            context->move_to(area.left(), conv(l) + 5);
            context->show_text(fmt::format("{}°", l));
        }

        /* Draw timestamps below graph */
        {
            context->set_font_size(22.0);
            double clock_x = graph_x_offset;
            const auto get_time = [](const auto &dp) { return dp.time; };
            const auto timestamps =
                dps | std::views::take(samples - 1) | std::views::transform(get_time);
            const double clock_y = area.bottom() - 10 - 30 - 30 - 30;
            for (const auto &timestamp : timestamps) {
                context->move_to(clock_x, clock_y);
                context->show_text(fmt::format("{:%H}", timestamp));
                clock_x += step_size;
            }
        }

        /* Show windspeed */
        {
            double x = graph_x_offset;
            const auto get_windspeed = [](const auto &dp) { return dp.windspeed; };
            const auto windspeeds =
                dps | std::views::take(samples - 1) | std::views::transform(get_windspeed);
            const double y = area.bottom() - 10 - 30 - 30;
            for (const auto &windspeed : windspeeds) {
                context->move_to(x, y);
                context->show_text(fmt::format("{:.0f}", std::round(windspeed)));
                x += step_size;
            }
        }

        /* Show gusts */
        {
            double x = graph_x_offset;
            const auto get_gust = [](const auto &dp) { return dp.gusts; };
            const auto gusts =
                dps | std::views::take(samples - 1) | std::views::transform(get_gust);
            const double y = area.bottom() - 10 - 30;
            for (const auto &gust : gusts) {
                context->move_to(x, y);
                context->show_text(fmt::format("{:.0f}", std::round(gust)));
                x += step_size;
            }
        }

        /* Show rain */
        {
            double x = graph_x_offset;
            const auto get_rain = [](const auto &dp) { return dp.rain; };
            const auto rains =
                dps | std::views::take(samples - 1) | std::views::transform(get_rain);
            const double y = area.bottom() - 10;
            for (const auto &rain : rains) {
                if (rain > 0) {
                    context->move_to(x, y);
                    context->show_text(fmt::format("{:.1f}", rain));
                }
                x += step_size;
            }
        }

        {
            context->set_font_size(14.0);
            const double x = graph_x_offset + 5;
            double y = area.bottom() - 10 - 3 * 30;
            context->move_to(x - 80.0, y += 30);
            context->show_text("Vind, m/s");
            context->move_to(x - 80.0, y += 30);
            context->show_text("Byar, m/s");
            context->move_to(x - 80.0, y += 30);
            context->show_text("Regn, mm");
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
    }

    /**
     * Draw current measured values
     */
    void draw_values(const Weather::MeasuredData &mdp) {
        const double font_large = 56.0;
        const double font_small = 32.0;
        const double font_tiny = 14.0;
        const double spacing = 5.0;
        const double indent_small = 10.0;
        const double indent_large = 40.0;
        const double indoor_y = spacing + font_small + spacing + font_large;
        const double outdoor_y = HEIGHT - spacing - font_small - spacing;
        const double rain_y = HEIGHT / 2.0 + font_small / 2;
        const double above_large = font_large + spacing;
        const double above_small = font_small + spacing;
        const double below = font_small + spacing;

        /* Set up font */
        context->set_font_size(font_large);
        context->select_font_face("cairo:sans-serif", Cairo::FONT_SLANT_NORMAL,
                                  Cairo::FONT_WEIGHT_NORMAL);

        /* Draw current temperatures */
        context->move_to(indent_small, outdoor_y);
        context->show_text(fmt::format("{}°", mdp.outdoor.now));

        context->move_to(indent_small, indoor_y);
        context->show_text(fmt::format("{}°", mdp.indoor.now));

        context->set_font_size(font_small);
        context->move_to(indent_small, rain_y);
        context->show_text(fmt::format("{:.1f} / {:.1f}", mdp.rain.last_1h, mdp.rain.last_24h));

        /* Draw min/max as smaller text next to current values */
        context->move_to(indent_large, indoor_y - above_large);
        context->show_text(fmt::format("{}°", mdp.indoor.max));
        context->move_to(indent_large, indoor_y + below);
        context->show_text(fmt::format("{}°", mdp.indoor.min));

        context->move_to(indent_large, outdoor_y - above_large);
        context->show_text(fmt::format("{}°", mdp.outdoor.max));
        context->move_to(indent_large, outdoor_y + below);
        context->show_text(fmt::format("{}°", mdp.outdoor.min));

        /* Annotation */
        context->set_font_size(font_tiny);
        context->move_to(indent_small, indoor_y - above_large);
        context->show_text("Inne");
        context->move_to(indent_small, outdoor_y - above_large);
        context->show_text("Ute");
        context->move_to(indent_small, rain_y - above_small);
        context->show_text("Regn (mm), 1h/24h");
    }

    uint32_t render_number = 0;

  public:
    Screen(const Options &options) : options(options), filename(options.render_store) {
        context->set_source_rgba(0.0, 0.0, 0.0, 1.0);
    }

    /**
     * Draw forecast and measured values on screen
     */
    void draw(const std::optional<std::vector<Forecast::DataPoint>> &dps,
              const std::optional<Weather::MeasuredData> &mdp) {
        log("Drawing data points to screen");
        surface = Cairo::ImageSurface::create(FORMAT, WIDTH, HEIGHT);
        context = Cairo::Context::create(surface);

        if (mdp) {
            draw_values(*mdp);
        }
        if (dps) {
            draw_forecast(*dps, Area(Range(mdp ? 192 : 40, WIDTH - 10), Range(0, HEIGHT)));
        }

        if (filename) {
            surface->write_to_png(fmt::format("{}-{}.png", *filename, render_number));
            render_number += 1;
        }
    }

    std::span<uint8_t, IMG_SIZE> get_ptr() {
        return std::span<uint8_t, IMG_SIZE>{surface->get_data(), IMG_SIZE};
    }
};
