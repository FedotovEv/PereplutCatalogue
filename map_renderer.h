#pragma once

#include <vector>

#include "geo.h"
#include "svg.h"
#include "transport_catalogue.h"

namespace renderer
{
    struct MapRendererContext
    {
      double width;
      double height;

      double padding;

      double line_width;
      double stop_radius;

      int bus_label_font_size;
      svg::Point bus_label_offset;

      int stop_label_font_size;
      svg::Point stop_label_offset;

      svg::Color underlayer_color;
      double underlayer_width;

      std::vector<svg::Color> color_palette;
    };

    class MapRenderer
    {
    public:

        MapRenderer(const MapRendererContext& rc) : render_context_(rc)
        {}
        svg::Document Render(const transport::TransportCatalogue& tc);

    private:

        struct MiniMaxParams
        {
            double max_lon;
            double min_lon;
            double max_lat;
            double min_lat;
            double zoom_coef;
        };

        static constexpr double ZERO_TOLERANCE = 1E-6;

        const MapRendererContext& render_context_;
        //Приватные методы класса
        svg::Point ConvertToRenderCoords(const detail::Coordinates& point,
                                         const MiniMaxParams& mp) const;
        svg::Text FormBusCaption(svg::Point stop_point, svg::Color use_color,
                                 bool is_undertext, const std::string& data) const;
        svg::Text FormStopCaption(svg::Point stop_point,
                                  bool is_undertext, const std::string& data) const;
    };
}
