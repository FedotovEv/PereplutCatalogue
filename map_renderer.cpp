
#include <string>
#include <map>
#include <vector>
#include <cmath>
#include <algorithm>

#include "svg.h"
#include "json.h"
#include "geo.h"
#include "domain.h"
#include "transport_catalogue.h"
#include "map_renderer.h"

using namespace svg;
using namespace json;
using namespace detail;
using namespace transport;
using namespace std;

namespace renderer
{

    Point MapRenderer::ConvertToRenderCoords(const Coordinates& point,
                                             const MiniMaxParams& mp) const
    {
        return {(point.lng - mp.min_lon) * mp.zoom_coef + render_context_.padding,
                (mp.max_lat - point.lat) * mp.zoom_coef + render_context_.padding};

    }

    Text MapRenderer::FormBusCaption(Point stop_point, Color use_color,
                                     bool is_undertext, const string& data) const
    {
        Text text;
        text.SetPosition(stop_point);
        text.SetData(data).SetOffset(render_context_.bus_label_offset)
            .SetFontFamily("Verdana").SetFontWeight("bold")
            .SetFontSize(render_context_.bus_label_font_size);

        if (is_undertext)
            text.SetFillColor(render_context_.underlayer_color)
                .SetStrokeColor(render_context_.underlayer_color)
                .SetStrokeWidth(render_context_.underlayer_width)
                .SetStrokeLineCap(StrokeLineCap::ROUND).SetStrokeLineJoin(StrokeLineJoin::ROUND);
        else
            text.SetFillColor(use_color);

        return text;
    }

    Text MapRenderer::FormStopCaption(Point stop_point, bool is_undertext, const string& data) const
    {
        Text text;
        text.SetPosition(stop_point);
        text.SetData(data).SetOffset(render_context_.stop_label_offset)
            .SetFontFamily("Verdana")
            .SetFontSize(render_context_.stop_label_font_size);

        if (is_undertext)
            text.SetFillColor(render_context_.underlayer_color)
                .SetStrokeColor(render_context_.underlayer_color)
                .SetStrokeWidth(render_context_.underlayer_width)
                .SetStrokeLineCap(StrokeLineCap::ROUND).SetStrokeLineJoin(StrokeLineJoin::ROUND);
        else
            text.SetFillColor("black");

        return text;
    }

    svg::Document MapRenderer::Render(const TransportCatalogue& tc)
    {
        svg::Document result;
        struct BusDescriptorComparator
        {
            bool operator()(const BusDescriptor& lhs, const BusDescriptor& rhs) const
            {
                return lhs.bus_name < rhs.bus_name;
            }
        };
        set<BusDescriptor, BusDescriptorComparator> bus_desc;
        map<string, Coordinates> stop_coords;
        MiniMaxParams mp;

        mp.max_lon = numeric_limits<double>::min();
        mp.min_lon = numeric_limits<double>::max();
        mp.max_lat = numeric_limits<double>::min();
        mp.min_lat = numeric_limits<double>::max();

        for (const BusDescriptor& bds : tc)
            if (bds.bus_stops.size())
            {
                bus_desc.insert(bds);
                for (const string& stop_name : bds.bus_stops)
                {
                    StopDescriptor stop = tc.GetStop(stop_name);
                    stop_coords[stop_name] = stop.stop_coords;
                    if (mp.min_lon > stop.stop_coords.lng)
                        mp.min_lon = stop.stop_coords.lng;
                    if (mp.min_lat > stop.stop_coords.lat)
                        mp.min_lat = stop.stop_coords.lat;
                    if (mp.max_lon < stop.stop_coords.lng)
                        mp.max_lon = stop.stop_coords.lng;
                    if (mp.max_lat < stop.stop_coords.lat)
                        mp.max_lat = stop.stop_coords.lat;
                }
            }

        double width_zoom_coef = abs(mp.max_lon - mp.min_lon) >= ZERO_TOLERANCE ?
                                 (render_context_.width - 2 * render_context_.padding) / (mp.max_lon - mp.min_lon) : 0;
        double height_zoom_coef = abs(mp.max_lat - mp.min_lat) >= ZERO_TOLERANCE ?
                                  (render_context_.height - 2 * render_context_.padding) / (mp.max_lat - mp.min_lat) : 0;
        mp.zoom_coef = min(width_zoom_coef, height_zoom_coef);

        //Рисуем ломаные линии маршрутов
        size_t color_number = 0;
        for (const BusDescriptor& bds : bus_desc) //Перебираем маршруты автобусов
        {
            Color use_color = render_context_.color_palette[color_number];
            Polyline polyline;
            //Прямой ход по маршруту
            for (int stop_num = 0; stop_num < static_cast<int>(bds.bus_stops.size()); ++stop_num)
                polyline.AddPoint(ConvertToRenderCoords(stop_coords[bds.bus_stops[stop_num]], mp));
            // Обратный ход для некольцевых марщрутов
            if (bds.bus_type == BusType::BUSTYPE_ORDINAR)
                for (int stop_num = bds.bus_stops.size() - 2; stop_num >= 0; --stop_num)
                    polyline.AddPoint(ConvertToRenderCoords(stop_coords[bds.bus_stops[stop_num]], mp));
            //Добавляем в итоговый svg-документ сформированную ломаную линию маршрута
            result.Add(polyline.SetFillColor(NoneColor).SetStrokeColor(use_color)
                               .SetStrokeWidth(render_context_.line_width)
                               .SetStrokeLineCap(StrokeLineCap::ROUND)
                               .SetStrokeLineJoin(StrokeLineJoin::ROUND));
            //Опеределим цвет следующего маршрута
            ++color_number;
            if (color_number >= render_context_.color_palette.size())
                color_number = 0;
        }
        //Выводим названия маршрутов
        color_number = 0;
        for (const BusDescriptor& bds : bus_desc) //Перебираем маршруты автобусов
        {
            Color use_color = render_context_.color_palette[color_number];
            //Сначала подложка
            result.Add(FormBusCaption(ConvertToRenderCoords(stop_coords[bds.bus_stops[0]], mp),
                                      use_color, true, bds.bus_name));
            //Теперь само название
            result.Add(FormBusCaption(ConvertToRenderCoords(stop_coords[bds.bus_stops[0]], mp),
                                      use_color, false, bds.bus_name));

            //Для некольцевого маршрута с несовпадающими конечными выводим метку также и у второй конечной
            if (bds.bus_type == BusType::BUSTYPE_ORDINAR &&
                bds.bus_stops[0] != bds.bus_stops[bds.bus_stops.size() - 1])
            {
                //Сначала подложка
                result.Add(FormBusCaption(ConvertToRenderCoords(stop_coords[bds.bus_stops[bds.bus_stops.size() - 1]], mp),
                                          use_color, true, bds.bus_name));
                //Теперь само название
                result.Add(FormBusCaption(ConvertToRenderCoords(stop_coords[bds.bus_stops[bds.bus_stops.size() - 1]], mp),
                                          use_color, false, bds.bus_name));
            }

            //Опеределим цвет следующего маршрута
            ++color_number;
            if (color_number >= render_context_.color_palette.size())
                color_number = 0;
        }
        //Рисуем круги остановок
        for (auto stop_coords_pair : stop_coords)
        {
            Circle circle;
            circle.SetCenter(ConvertToRenderCoords(stop_coords_pair.second, mp))
                  .SetRadius(render_context_.stop_radius).SetFillColor("white");

            result.Add(circle);
        }
        //И, наконец, названия остановок
        for (auto stop_coords_pair : stop_coords)
        {
            result.Add(FormStopCaption(ConvertToRenderCoords(stop_coords_pair.second, mp),
                                       true, stop_coords_pair.first)); //Подложка
            result.Add(FormStopCaption(ConvertToRenderCoords(stop_coords_pair.second, mp),
                                       false, stop_coords_pair.first)); //Собственно, название
        }

        return result;
    }
} //namespace renderer
