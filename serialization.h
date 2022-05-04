#pragma once

#include <string>
#include <map>

#include "json.h"
#include "svg.h"
#include "transport_catalogue.h"
#include "transport_router.h"
#include "map_renderer.h"
#include "svg.pb.h"
#include "graph.pb.h"
#include "map_renderer.pb.h"
#include "transport_catalogue.pb.h"

namespace transport::reader
{
    class JSONReader;
}

namespace serial
{
    struct SerializationContext
    {
        std::string input_file; //Имя входного файла при распоследовании
        std::string output_file; //Имя выходного файлп при упоследовании
    };

    struct ColorConvertClass
    {
        TransCatSerial::Color operator()(std::monostate)
        {
            TransCatSerial::Color result;
            result.set_encoding_type(TransCatSerial::Color_ColorEncodingType::Color_ColorEncodingType_COLORTYPE_UNDEFINED);
            return result;
        }

        TransCatSerial::Color operator()(const std::string& col_str)
        {
            TransCatSerial::Color result;
            result.set_encoding_type(TransCatSerial::Color_ColorEncodingType::Color_ColorEncodingType_COLORTYPE_STRING);
            result.set_color_name(col_str);
            return result;
        }

        TransCatSerial::Color operator()(const svg::Rgb& col_rgb)
        {
            TransCatSerial::Color result;
            result.set_encoding_type(TransCatSerial::Color_ColorEncodingType::Color_ColorEncodingType_COLORTYPE_RGB);
            result.set_red(col_rgb.red);
            result.set_green(col_rgb.green);
            result.set_blue(col_rgb.blue);
            result.set_opacity(1.0);
            return result;
        }

        TransCatSerial::Color operator()(const svg::Rgba& col_rgba)
        {
            TransCatSerial::Color result;
            result.set_encoding_type(TransCatSerial::Color_ColorEncodingType::Color_ColorEncodingType_COLORTYPE_RGBA);
            result.set_red(col_rgba.red);
            result.set_green(col_rgba.green);
            result.set_blue(col_rgba.blue);
            result.set_opacity(col_rgba.opacity);
            return result;
        }
    };

    class Serializer
    {
    public:

        Serializer(transport::reader::JSONReader& jsr);
        void Serialize();
        void Deserialize();

    private:

        transport::reader::JSONReader& jsr_;
        struct NameConvertSet
        {   // Структура, которая будет хранить набор словарей, выполняющих прямое и обратное отражение
            // имён элементов транспортного справочника в их условные порядковые номера в сериализованном виде.
            std::unordered_map<std::string, int> stop_name_to_pb_number_;
            std::unordered_map<std::string, int> bus_name_to_pb_number_;
            std::unordered_map<int, std::string> pb_number_to_stop_name_;
            std::unordered_map<int, std::string> pb_number_to_bus_name_;
            std::unordered_map<size_t, const transport::TransportCatalogue::Stop*> stop_num_to_stop_ptr;
            std::unordered_map<size_t, const transport::TransportCatalogue::Bus*> bus_num_to_bus_ptr;
            std::vector<transport::TransportCatalogue::StopsListType::iterator> stop_iter_list;
            std::vector<transport::TransportCatalogue::BusesListType::iterator> bus_iter_list;
        };
        // Приватные методы класса
        svg::Color ConvertColorRev(TransCatSerial::Color& pb_color);
        TransCatSerial::MapRendererContext SerializeRenderContext();
        TransCatSerial::RouterContext SerializeRouterContext();        
        void SerializeStopsBusesPrepare(NameConvertSet& cnv);
        TransCatSerial::StopList SerializeStops(NameConvertSet& cnv);
        TransCatSerial::BusList SerializeBuses(NameConvertSet& cnv);
        TransCatSerial::StopNameToVertexList SerializeStopNameToVertex(bool EnterOrExitList, NameConvertSet& cnv);
        TransCatSerial::EdgeToDescList SerializeEdgeToDesc(NameConvertSet& cnv);
        TransCatSerial::Edges SerializeEdges();
        TransCatSerial::IncidenceLists SerializeIncidenceLists();
        TransCatSerial::RoutesData SerializeRoutesData();
        void DeserializeStopsInit(TransCatSerial::StopList pb_stop_list, NameConvertSet& cnv);
        void DeserializeBusesInit(TransCatSerial::BusList pb_bus_list, NameConvertSet& cnv);
        void DeserializeStopsSec(TransCatSerial::StopList pb_stop_list, NameConvertSet& cnv);
        void DeserializeBusesSec(TransCatSerial::BusList pb_bus_list, NameConvertSet& cnv);
        void DeserializeRouterContext(TransCatSerial::RouterContext pb_router_context);
        void DeserializeRenderContext(TransCatSerial::MapRendererContext pb_render_context);
        void DeserializeStopNameToVertex(TransCatSerial::StopNameToVertexList pb_stop_name_to_vertex_list,
                                         bool EnterOrExitList, NameConvertSet& cnv);
        void DeserializeEdgeToDesc(TransCatSerial::EdgeToDescList pb_edge_to_desc_list, NameConvertSet& cnv);
        void DeserializeEdges(TransCatSerial::Edges pb_edges_list);
        void DeserializeIncidenceList(TransCatSerial::IncidenceLists pb_vertex_incidence_lists);
        void DeserializeRoutesData(TransCatSerial::RoutesData pb_routes_data);
    };
} // namespace serial
