#include <fmt/core.h>
#include <cairommconfig.h>
#include <cairomm/context.h>
#include <cairomm/surface.h>
//#include <cairomm/xlib_surface.h>
#include <cairomm/enums.h>

void do_stuff_with_cairo() {
    /* It is possible to draw to Xlib sruface */
    auto surface = Cairo::ImageSurface::create(Cairo::Format::FORMAT_A1,800,480);
    auto cr = Cairo::Context::create(surface);

    cr->save();
    cr->set_line_width(20.0);
    cr->set_source_rgba(1.0, 1.0, 1.0, 1.0);

    cr->move_to(surface->get_width() / 4.0, surface->get_height() / 4.0);
    cr->line_to(surface->get_width() * 3.0 / 4.0, surface->get_height() * 3.0 / 4.0);
    cr->stroke();

    fmt::print("Surface is {} bytes per row, with {} rows\n", surface->get_stride(), surface->get_height());
    
    //std::string filename = "image.png";
    //surface->write_to_png(filename);
}
