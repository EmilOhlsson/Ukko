#pragma once

#include <cairomm/context.h>
#include <cairomm/enums.h>
#include <cairomm/surface.h>
#include <cairommconfig.h>
#include <fmt/core.h>

#include "weather.hpp"
#include "utils.hpp"

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

    template<typename T>
    constexpr T diff(T v1, T v2) {
        if (v1 > v2) return v1 - v2;
        else return v2 - v1;
    }

  public:
    Screen(const std::optional<std::string> &filename) : filename(filename) {}

    void draw(const std::vector<weather::Hour> &dps) {
        context->set_source_rgba(0.0, 0.0, 0.0, 1.0);

        /* number of samples */
        const size_t samples = std::min<size_t>(dps.size(), 12);

        // TODO Encapsulate this block here to allow const max and min
        double min = std::numeric_limits<double>::max();
        double max = std::numeric_limits<double>::min();
        for (size_t i = 0; i < samples; i++) {
            max = std::max<double>(dps[i].temperature, max);
            min = std::min<double>(dps[i].temperature, min);
        }

        /* Limit graph area */
        const double temp_width = 50;
        const double graph_width = WIDTH - temp_width;
        const double graph_height = HEIGHT * 0.9;
        const double graph_x_offset = temp_width;
        const double graph_y_offset = HEIGHT / 20.0;

        const double steps = samples - 1;
        const double step_size = graph_width / steps;

        const auto &temperature_to_y = [&](double temperature) -> double {
            const double norm_div = max - min;
            const double vertical_span = graph_height - 2 * 48;
            const double scale = vertical_span / norm_div;
            const double offset = 48 - min * scale;

            return graph_height - (temperature * scale + offset) - 1;
        };
        
        /* TODO: For now, just ignore the netatmo data, we only want the forecast */

        /* TODO: Grading, there should never be more than 7-9 lines. If range is including, or
         * "close" to zero, there should be a zero line. (Close meaning wihin 1°C of zero)
         *
         * lines should never be closer than 1°C, and not more than 5°C
         *
         * left of line there are values written
         */

        bool use_zero_line = (max > 0 && min < 0) || diff(0.0, min) < 1.0 || diff(0.0, max) < 1.0;

        /* If temperature resolution is higher than 100°C we have a problem... */
        double block_size = 0;
        double blocks = 0;
        const double span = max - min;
        constexpr double max_blocks = 7;
        for (const double bsize : {1,2,5,10,20,50,100}) {
            blocks = utils::div_ceil(span, bsize);
            if (blocks < max_blocks) {
                block_size = bsize;
                break;
            }
        }
        assert(block_size > 0);

        /* Calculate a lower line, i.e, min value rounded down to multiple of block size */
        double lower = utils::round_down(min, block_size);

        /* Calculate a upper line, i.e, max value rounded up to multiple of block size */
        double upper = utils::round_up(max, block_size);

        double drawn_span = upper - lower;
        
        /* TODO: Use these to draw graph lines, and write scale to the left */
        const auto &val_to_y = [&](double val, double norm) -> double {
            return HEIGHT - 
        };



        
        /* TODO: We want to have some space available above and below to print labels and extra
         * information */

        /* Zero line */
        context->set_line_width(2.0);
        context->move_to(0, temperature_to_y(0));
        context->line_to(graph_width - 1, temperature_to_y(0));
        context->stroke();

        /* -1 */
        context->set_line_width(1.0);
        context->set_dash(std::vector{1.0,5.0},0.0);
        context->move_to(0, temperature_to_y(-1));
        context->line_to(graph_width - 1, temperature_to_y(-1));
        context->stroke();

        /* Temperature curve */
        context->set_line_width(4.0);
        context->unset_dash();
        context->move_to(0, temperature_to_y(dps[0].temperature));
        for (size_t i = 1; i < samples; i++) {
            fmt::print(" Drawing {} as at {}\n", dps[i].temperature, temperature_to_y(dps[i].temperature));
            context->line_to(step_size * i, temperature_to_y(dps[i].temperature));
        }
        context->stroke();

        /* Some text */
        context->set_line_width(1.0);
        context->set_font_size(10.0); /* Default size is 10.0 */
        context->move_to(WIDTH / 2.0, HEIGHT / 2.0);
        context->show_text("Hello, world");
        context->stroke();
        context->move_to(WIDTH / 2.0, HEIGHT / 2.0);
        context->line_to(0,0);
        context->stroke();

        if (filename) {
            surface->write_to_png(*filename);
        }
    }
};
