
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <variant>

#include "svg.h"
#include "json.h"
#include "transport_router.h"
#include "json_builder.h"
#include "json_reader.h"
#include "map_renderer.h"
#include "transport_catalogue.h"
#include "svg.pb.h"
#include "graph.pb.h"
#include "map_renderer.pb.h"
#include "transport_catalogue.pb.h"
#include "serialization.h"

using namespace std;
using namespace transport;

namespace serial
{
    Serializer::Serializer(transport::reader::JSONReader& jsr) : jsr_(jsr)
    {}

    svg::Color Serializer::ConvertColorRev(TransCatSerial::Color& pb_color)
    {
        switch (pb_color.encoding_type())
        {
            case TransCatSerial::Color_ColorEncodingType::Color_ColorEncodingType_COLORTYPE_STRING:
                return pb_color.color_name();
            case TransCatSerial::Color_ColorEncodingType::Color_ColorEncodingType_COLORTYPE_RGB:
                return svg::Rgb{static_cast<uint8_t>(pb_color.red()), static_cast<uint8_t>(pb_color.green()),
                                static_cast<uint8_t>(pb_color.blue())};
            case TransCatSerial::Color_ColorEncodingType::Color_ColorEncodingType_COLORTYPE_RGBA:
                return svg::Rgba{static_cast<uint8_t>(pb_color.red()), static_cast<uint8_t>(pb_color.green()),
                                 static_cast<uint8_t>(pb_color.blue()), pb_color.opacity()};
            default:
                return monostate();
        }
    }

    // Перечень информационных элементов, подлежащих упоследованию/распоследованию для полного
    // сохранения/восстановления состояния системы, полученного после обработки запросов первой
    // стадии (запросов типа make_base).
    // 1. список (множество) остановок transport::TransportCatalogue::stops_ (jsr_._tc_.stops_).
    // 2. список (множество) автобусных маршрутов transport::TransportCatalogue::buses_ (jsr_.tc_.buses_).
    //----------------------------------------------------------------------------------------------------
    // Группу переменных, хронящих служебный контекст для поддержки исполнения запросов второй стадии:
    // 3. Параметры маршрутизации router::RouterContext::router_context_ (jsr_.router_context_ ).
    // 4. Параметры построения карты renderer::MapRendererContext::render_context_ (jsr_.render_context_).
    //----------------------------------------------------------------------------------------------------
    // Группа переменных, описывающих построенный граф маршрутизации:
    // Словари-преобразователи именных идентификаторов элементов проложенного маршрута в их
    // номера-идентификаторы, используемые библиотеками поддержки графов (graph.h) и
    // маршрутизации внутри них (router.h).
    // 5. Словарь-преобразователь имён остановок в номера вершин графа, соответствующие их перронам
    // отправления - router::BusRouter::stop_name_to_enter_vertex_ (jsr_.bus_router_ptr_->stop_name_to_enter_vertex_)
    // 6. Словарь-преобразователь имён остановок в номера вершин графа, соответствующие их перронам
    // прибытия - router::BusRouter::stop_name_to_exit_vertex_ (jsr_.bus_router_ptr_->stop_name_to_exit_vertex_)
    // 7. Словарь-преобразователь номеров рёбер в их структуры-описатели типа router::BusRouter::EdgeDescriptor -
    // - (jsr_.bus_router_ptr_->edge_to_desc_)
    // 8. Список (вектор) рёбер графа, каждому ребру соответствует описатель ребра типа graph::Edge - 
    // - (jsr_.bus_router_ptr_->router_.graph_.edges_)
    // 9. Список (вектор) списков (векторов) смежности вершин и рёбер этого графа. Количество элементов в списке равно
    // общему количеству вершин графа. Для каждой вершины в список входит, в свою очередь, другой список (IncidenceList = vector<EdgeId>)
    // из номеров рёбер, выходящих из данной вершины (jsr_.bus_router_ptr_->router_.graph_.incidence_lists_)
    //----------------------------------------------------------------------------------------------------------------------------------
    // 10. Наконец, разрешающая матрица routes_internal_data_, которая позволяет за линейное время строить маршрут от любой вершины к
    // любой другой вершине. Матрица хранится как вектор векторов, размер этих векторов равен количеству вершин графа, на пересечении
    // (в элементах второго вектора) хранится элемент типа RouteInternalData (jsr_.bus_router_ptr_->router_.routes_internal_data_)

    TransCatSerial::MapRendererContext Serializer::SerializeRenderContext()
    {
        // Функция заполняет структуру типа TransCatSerial::MapRendererContext, служащую
        // для упоследования контекста, применяемого для построения карт программой - jsr_.render_context_.
        TransCatSerial::MapRendererContext pb_render_context;
        pb_render_context.set_width(jsr_.render_context_.width);
        pb_render_context.set_width(jsr_.render_context_.width);
        pb_render_context.set_height(jsr_.render_context_.height);
        pb_render_context.set_padding(jsr_.render_context_.padding);
        pb_render_context.set_line_width(jsr_.render_context_.line_width);
        pb_render_context.set_stop_radius(jsr_.render_context_.stop_radius);
        pb_render_context.set_bus_label_font_size(jsr_.render_context_.bus_label_font_size);
        pb_render_context.set_stop_label_font_size(jsr_.render_context_.stop_label_font_size);
        pb_render_context.set_underlayer_width(jsr_.render_context_.underlayer_width);
        TransCatSerial::Point pb_point;
        pb_point.set_x(jsr_.render_context_.bus_label_offset.x);
        pb_point.set_y(jsr_.render_context_.bus_label_offset.y);
        *pb_render_context.mutable_bus_label_offset() = pb_point;
        pb_point.set_x(jsr_.render_context_.stop_label_offset.x);
        pb_point.set_y(jsr_.render_context_.stop_label_offset.y);
        *pb_render_context.mutable_stop_label_offset() = pb_point;
        TransCatSerial::Color pb_color(visit(ColorConvertClass{}, jsr_.render_context_.underlayer_color));
        *pb_render_context.mutable_underlayer_color() = pb_color;
        for (const svg::Color& col : jsr_.render_context_.color_palette)
        {
            pb_color = visit(ColorConvertClass{}, col);
            *pb_render_context.add_color_palette() = pb_color;
        }
        return pb_render_context;
    }

    TransCatSerial::RouterContext Serializer::SerializeRouterContext()
    { 
        // Функция заполняет структуру типа TransCatSerial::RouterContext, служащую для
        // сериализации контекста построения маршрутов - jsr_.router_context_.
        TransCatSerial::RouterContext pb_router_context;
        pb_router_context.set_bus_velocity(jsr_.router_context_.bus_velocity);
        pb_router_context.set_bus_wait_time(jsr_.router_context_.bus_wait_time);
        return pb_router_context;
    }

    void Serializer::SerializeStopsBusesPrepare(Serializer::NameConvertSet& cnv)
    {   // Подпрограмма подготавливает словари-трансляторы имён элементов базы данных транспортного каталога
        // в условные порядковые номера, которые будут использоваться для их идентификации в упоследованной базе данных.
        // Перебираем все наличные автобусные остановки, запоминая для каждой собственный порядковый номер в порядке обхода,
        // сопоставляя его имени остановки в прямом и обратном словарях stop_name_to_pb_number_ и pb_number_to_stop_name_.
        cnv.stop_name_to_pb_number_.clear();
        cnv.pb_number_to_stop_name_.clear();
        int i = 0;
        for (auto stop_it = jsr_.trans_cat_.stop_begin(); stop_it != jsr_.trans_cat_.stop_end(); ++stop_it)
        {
            cnv.stop_name_to_pb_number_[(*stop_it).stop_name] = i;
            cnv.pb_number_to_stop_name_[i++] = (*stop_it).stop_name;
        }
        // Аналогично перечислим все наличные маршруты автобусов, запоминая для каждого собственный порядковый номер в порядке обхода,
        // сопоставляя его имени(номеру) автобуса в прямом и обратном словарях bus_name_to_pb_number_ и pb_number_to_bus_name_.
        cnv.bus_name_to_pb_number_.clear();
        cnv.pb_number_to_bus_name_.clear();
        i = 0;
        for (BusDescriptor busd : jsr_.trans_cat_)
        {
            cnv.bus_name_to_pb_number_[busd.bus_name] = i;
            cnv.pb_number_to_bus_name_[i++] = busd.bus_name;
        }
    }

    TransCatSerial::StopList Serializer::SerializeStops(Serializer::NameConvertSet& cnv)
    {
        TransCatSerial::StopList pb_stop_list;
        // Сначала займёмся заполнением первого массива упоследующей структуры TransCatSerial::StopBusList - stops.
        for (auto stop_it = jsr_.trans_cat_.stop_begin(); stop_it != jsr_.trans_cat_.stop_end(); ++stop_it)
        {
            StopDescriptor stopd = *stop_it;
            TransCatSerial::Stop pb_stop;
            pb_stop.set_stop_type(static_cast<TransCatSerial::Stop_StopType>(static_cast<int>(stopd.stop_type)));
            pb_stop.set_stop_name(stopd.stop_name);
            // Заполним поле pb_stop.stop_coords
            TransCatSerial::Coordinates pb_stop_coords;
            pb_stop_coords.set_lat(stopd.stop_coords.lat);
            pb_stop_coords.set_lng(stopd.stop_coords.lng);
            *pb_stop.mutable_stop_coords() = pb_stop_coords;
            // Заполняем массив pb_stop.buses_at_stop
            for (const string& bus_name : stopd.buses_at_stop)
                pb_stop.add_buses_at_stop(cnv.bus_name_to_pb_number_[bus_name]);
            // Наконец, заполним массив pb_stop.distance_to_stop
            TransCatSerial::DistanceToStop pb_distance_to_stop;
            for (auto distance_to_stop_pair : stopd.distance_to_stop)
            {
                pb_distance_to_stop.set_stop_num(cnv.stop_name_to_pb_number_[distance_to_stop_pair.first]);
                pb_distance_to_stop.set_distance(distance_to_stop_pair.second);
                *pb_stop.add_distance_to_stop() = pb_distance_to_stop;
            }
            // Окончательно, добавляем сформированный описатель остановки к массиву pb_transport_cataloque.stops
            *pb_stop_list.add_stops() = pb_stop;
        }
        return pb_stop_list;
    }

    TransCatSerial::BusList Serializer::SerializeBuses(Serializer::NameConvertSet& cnv)
    {
        TransCatSerial::BusList pb_bus_list;
        // Здесь займёмся заполнением второго массива упоследующей структуры TransCatSerial::StopBusList - buses.
        for (BusDescriptor busd : jsr_.trans_cat_)
        {
            TransCatSerial::Bus pb_bus;
            pb_bus.set_bus_type(static_cast<TransCatSerial::Bus_BusType>(static_cast<int>(busd.bus_type)));
            pb_bus.set_bus_name(busd.bus_name);
            // Заполняем массив pb_bus.bus_stops
            for (const string& stop_name : busd.bus_stops)
                pb_bus.add_bus_stops(cnv.stop_name_to_pb_number_[stop_name]);
            // Окончательно, добавляем сформированный описатель маршрута к массиву pb_transport_cataloque.buses
            *pb_bus_list.add_buses() = pb_bus;
        }
        return pb_bus_list;
    }

    TransCatSerial::StopNameToVertexList Serializer::SerializeStopNameToVertex(bool EnterOrExitList, Serializer::NameConvertSet& cnv)
    {
        TransCatSerial::StopNameToVertexList pb_stop_name_to_vertex_list;
        router::BusRouter::StopToVertexT *stop_name_to_vertex_ptr =
            EnterOrExitList ?
            &jsr_.bus_router_ptr_->stop_name_to_enter_vertex_ :
            &jsr_.bus_router_ptr_->stop_name_to_exit_vertex_;
        for (auto& stop_to_vertex_pair : *stop_name_to_vertex_ptr)
        {
            TransCatSerial::StopNameToVertex pb_stop_name_to_vertex;
            pb_stop_name_to_vertex.set_stop_name_num(cnv.stop_name_to_pb_number_[stop_to_vertex_pair.first]);
            pb_stop_name_to_vertex.set_vertex_id(stop_to_vertex_pair.second);
            *pb_stop_name_to_vertex_list.add_stop_name_to_vertex() = pb_stop_name_to_vertex;
        }
        return pb_stop_name_to_vertex_list;
    }

    TransCatSerial::EdgeToDescList Serializer::SerializeEdgeToDesc(NameConvertSet& cnv)
    {
        TransCatSerial::EdgeToDescList pb_edge_to_desc_list;
        for (auto& edge_to_desc_pair : jsr_.bus_router_ptr_->edge_to_desc_)
        {
            TransCatSerial::EdgeToDesc pb_edge_to_desc;
            pb_edge_to_desc.set_edge_id(edge_to_desc_pair.first);
            TransCatSerial::EdgeDescriptor pb_edge_desc;
            pb_edge_desc.set_edge_type(static_cast<TransCatSerial::EdgeDescriptor_EdgeType>
                                      (static_cast<int>(edge_to_desc_pair.second.edge_type)));
            pb_edge_desc.set_from_stop_num(cnv.stop_name_to_pb_number_[edge_to_desc_pair.second.from_stop]);
            pb_edge_desc.set_to_stop_num(cnv.stop_name_to_pb_number_[edge_to_desc_pair.second.to_stop]);
            pb_edge_desc.set_bus_name_num(cnv.bus_name_to_pb_number_[edge_to_desc_pair.second.bus_name]);
            pb_edge_desc.set_span_count(edge_to_desc_pair.second.span_count);
            pb_edge_desc.set_time_length(edge_to_desc_pair.second.time_length);
            *pb_edge_to_desc.mutable_edge_descriptor() = pb_edge_desc;
            *pb_edge_to_desc_list.add_edge_to_desc() = pb_edge_to_desc;
        }
        return pb_edge_to_desc_list;
    }

    TransCatSerial::Edges Serializer::SerializeEdges()
    {
        TransCatSerial::Edges pb_edges_list;
        for (auto& edge_def : jsr_.bus_router_ptr_->router_.graph_.edges_)
        {
            TransCatSerial::Edge pb_edge_list_elem;
            pb_edge_list_elem.set_from(edge_def.from);
            pb_edge_list_elem.set_to(edge_def.to);
            pb_edge_list_elem.set_weight(edge_def.weight);
            *pb_edges_list.add_edges() = pb_edge_list_elem;
        }
        return pb_edges_list;
    }

    TransCatSerial::IncidenceLists Serializer::SerializeIncidenceLists()
    {
        TransCatSerial::IncidenceLists pb_vertex_incidence_list;
        for (auto& incidence_list : jsr_.bus_router_ptr_->router_.graph_.incidence_lists_)
        {
            TransCatSerial::IncidenceList pb_incidence_list;
            for (auto edge_id : incidence_list)
                pb_incidence_list.add_edge_id(edge_id);
            *pb_vertex_incidence_list.add_incidence_list() = pb_incidence_list;
        }
        return pb_vertex_incidence_list;
    }

    TransCatSerial::RoutesData Serializer::SerializeRoutesData()
    {
        TransCatSerial::RoutesData pb_routes_data;
        for (auto& routes_sec_index_data : jsr_.bus_router_ptr_->router_.routes_internal_data_)
        {
            TransCatSerial::RoutesSecIndexData pb_sec_index_routes_data;
            for (auto& opt_route_data : routes_sec_index_data)
            {
                TransCatSerial::RouteData pb_route_data;
                if (opt_route_data)
                {
                    pb_route_data.set_has_value(true);
                    pb_route_data.set_weight(opt_route_data->weight);
                    if (opt_route_data->prev_edge)
                    {
                        pb_route_data.set_has_prev_edge_value(true);
                        pb_route_data.set_prev_edge(opt_route_data->prev_edge.value());
                    }
                    else
                    {
                        pb_route_data.set_has_prev_edge_value(false);
                    }
                }
                else
                {
                    pb_route_data.set_has_value(false);
                }
                *pb_sec_index_routes_data.add_sec_routes_data() = pb_route_data;
            }
            *pb_routes_data.add_first_routes_data() = pb_sec_index_routes_data;
        }
        return pb_routes_data;
    }

    void Serializer::Serialize()
    {
        TransCatSerial::TransportCatalogue pb_transport_cataloque;
        NameConvertSet cnv;
        // В данной функции последовательно, по этапам заполняется данными упоследующая структура TransCatSerial::TransportCatalogue,
        // созданная по схеме transport_catalogue.proto транслятором protobuf. Начнём с автобусных остановок и маршрутов автобусов - элемент stops_buses.
        SerializeStopsBusesPrepare(cnv);
        // Сериализация списка остановок
        *pb_transport_cataloque.mutable_stops() = SerializeStops(cnv);
         // Затем сериализация списка автобусов
        *pb_transport_cataloque.mutable_buses() = SerializeBuses(cnv);
        // Выполним сериализацию маршрутного контекста,
        *pb_transport_cataloque.mutable_router_context() = SerializeRouterContext();
        // и сериализацию контекста картографии.
        *pb_transport_cataloque.mutable_render_context() = SerializeRenderContext();
        // Дальнейшее касается структур маршрутизатора и выполняется только при его наличии.
        if (jsr_.bus_router_ptr_)
        {
            pb_transport_cataloque.set_is_routes_data(true);
            // Теперь последовательным вызовом соответствующих процедур сериализуем построенный граф маршрутизации.
            // Сначала - словари-отображатели имён остановок в соответствующие им номера вершин маршрутизирующего графа.
            *pb_transport_cataloque.mutable_stop_name_to_enter_vertex() = SerializeStopNameToVertex(true, cnv);
            *pb_transport_cataloque.mutable_stop_name_to_exit_vertex() = SerializeStopNameToVertex(false, cnv);
            // Упоследование словаря-преобразователя номеров рёбер в их содержательные описатели.
            *pb_transport_cataloque.mutable_edge_to_desc() = SerializeEdgeToDesc(cnv);
            // Сериализация списка рёбер графа
            *pb_transport_cataloque.mutable_edges() = SerializeEdges();
            // Упоследование списков смежности вершин и выходящих из них рёбер.
            *pb_transport_cataloque.mutable_incidence_lists() = SerializeIncidenceLists();
            // И, наконец, сериализация разрешающей маршрутной матрицы.
            *pb_transport_cataloque.mutable_routes_data() = SerializeRoutesData();
            ofstream ofs(jsr_.serial_context_.output_file, ios_base::binary);
            pb_transport_cataloque.SerializeToOstream(&ofs);
        }
        else
        {
            pb_transport_cataloque.set_is_routes_data(false);
        }
    };

    void Serializer::DeserializeStopsInit(TransCatSerial::StopList pb_stop_list, NameConvertSet& cnv)
    {
        jsr_.trans_cat_.stops_.clear();
        // Сначала создадим список остановок stops_. Одновременно заполним там поля, не требующие знания
        // указателей на другие объекты базы данных.
        cnv.stop_iter_list.reserve(jsr_.trans_cat_.stops_.size());
        for (int cur_stop_num = 0; cur_stop_num < pb_stop_list.stops_size(); ++cur_stop_num)
        {
            TransCatSerial::Stop pb_stop = pb_stop_list.stops(cur_stop_num);
            TransportCatalogue::Stop stopd;
            stopd.stop_type = static_cast<transport::StopType>(static_cast<int>(pb_stop.stop_type()));
            stopd.stop_name = pb_stop.stop_name();
            TransCatSerial::Coordinates pb_coords = pb_stop.stop_coords();
            stopd.stop_coords.lat = pb_coords.lat();
            stopd.stop_coords.lng = pb_coords.lng();
            auto stops_it_pair = jsr_.trans_cat_.stops_.insert(stopd);
            cnv.stop_name_to_pb_number_[stopd.stop_name] = cur_stop_num;
            cnv.pb_number_to_stop_name_[cur_stop_num] = stopd.stop_name;
            cnv.stop_num_to_stop_ptr[cur_stop_num] = &(*stops_it_pair.first);
            cnv.stop_iter_list.push_back(stops_it_pair.first);
        }
    }

    void Serializer::DeserializeBusesInit(TransCatSerial::BusList pb_bus_list, NameConvertSet& cnv)
    {
        jsr_.trans_cat_.buses_.clear();
        // А здесь создаётся вторая часть базы данных - список маршрутов автобусов buses_. Аналогично, заполняем
        // в элементах списка поля, не содержащие указателей на другие объекты базы.
        cnv.bus_iter_list.reserve(jsr_.trans_cat_.buses_.size());
        for (int cur_bus_num = 0; cur_bus_num < pb_bus_list.buses_size(); ++cur_bus_num)
        {
            TransCatSerial::Bus pb_bus = pb_bus_list.buses(cur_bus_num);
            TransportCatalogue::Bus busd;
            busd.bus_type = static_cast<transport::BusType>(static_cast<int>(pb_bus.bus_type()));
            busd.bus_name = pb_bus.bus_name();
            auto buses_it_pair = jsr_.trans_cat_.buses_.insert(busd);
            cnv.bus_name_to_pb_number_[busd.bus_name] = cur_bus_num;
            cnv.pb_number_to_bus_name_[cur_bus_num] = busd.bus_name;
            cnv.bus_num_to_bus_ptr[cur_bus_num] = &(*buses_it_pair.first);
            cnv.bus_iter_list.push_back(buses_it_pair.first);
        }
    }

    void Serializer::DeserializeStopsSec(TransCatSerial::StopList pb_stop_list, NameConvertSet& cnv)
    {
        // Заполняем поля списков, которые требуют для своего заполнения указателей на другие элементы списков.
        // Сначала заполним поля buses_at_stop и distance_to_stop элементов списка (множества) stops_.
        for (auto& cur_stop_it : cnv.stop_iter_list)
        {
            int src_stop_num = cnv.stop_name_to_pb_number_[cur_stop_it->stop_name];
            TransCatSerial::Stop pb_stop = pb_stop_list.stops(src_stop_num);
            auto stop_node = jsr_.trans_cat_.stops_.extract(cur_stop_it);
            // Заполняем массив (вектор) buses_at_stop.
            for (int cur_bus_num = 0; cur_bus_num < pb_stop.buses_at_stop_size(); ++cur_bus_num)
                stop_node.value().buses_at_stop.push_back(cnv.bus_num_to_bus_ptr[pb_stop.buses_at_stop(cur_bus_num)]);
            // А теперь - словарь distance_to_stop.
            for (int cur_stop_num = 0; cur_stop_num < pb_stop.distance_to_stop_size(); ++cur_stop_num)
            {
                TransCatSerial::DistanceToStop pb_distance_to_stop = pb_stop.distance_to_stop(cur_stop_num);
                stop_node.value().distance_to_stop[cnv.stop_num_to_stop_ptr[pb_distance_to_stop.stop_num()]] =
                    pb_distance_to_stop.distance();
            }
            jsr_.trans_cat_.stops_.insert(move(stop_node));
        }
    }

    void Serializer::DeserializeBusesSec(TransCatSerial::BusList pb_bus_list, NameConvertSet& cnv)
    {
        // Наконец, последняя операция. Заполним поле bus_stops списка (множества) buses_.
        for (auto& cur_bus_it : cnv.bus_iter_list)
        {
            int src_bus_num = cnv.bus_name_to_pb_number_[cur_bus_it->bus_name];
            TransCatSerial::Bus pb_bus = pb_bus_list.buses(src_bus_num);
            auto bus_node = jsr_.trans_cat_.buses_.extract(cur_bus_it);
            for (int cur_stop_num = 0; cur_stop_num < pb_bus.bus_stops_size(); ++cur_stop_num)
                bus_node.value().bus_stops.push_back(cnv.stop_num_to_stop_ptr[pb_bus.bus_stops(cur_stop_num)]);
            jsr_.trans_cat_.buses_.insert(move(bus_node));
        }
    }

    void Serializer::DeserializeRouterContext(TransCatSerial::RouterContext pb_router_context)
    {
        // Считаем из TransCatSerial::TransportCatalogue и заполним структуру router_context_.
        jsr_.router_context_.bus_velocity = pb_router_context.bus_velocity();
        jsr_.router_context_.bus_wait_time = pb_router_context.bus_wait_time();
    }

    void Serializer::DeserializeRenderContext(TransCatSerial::MapRendererContext pb_render_context)
    {
        // Считаем из TransCatSerial::TransportCatalogue и заполним структуру render_context_.
        jsr_.render_context_.width = pb_render_context.width();
        jsr_.render_context_.height = pb_render_context.height();
        jsr_.render_context_.padding = pb_render_context.padding();
        jsr_.render_context_.line_width = pb_render_context.line_width();
        jsr_.render_context_.stop_radius = pb_render_context.stop_radius();
        jsr_.render_context_.bus_label_font_size = pb_render_context.bus_label_font_size();
        jsr_.render_context_.stop_label_font_size = pb_render_context.stop_label_font_size();
        jsr_.render_context_.underlayer_width = pb_render_context.underlayer_width();
        TransCatSerial::Point pb_point(pb_render_context.bus_label_offset());
        jsr_.render_context_.bus_label_offset.x = pb_point.x();
        jsr_.render_context_.bus_label_offset.y = pb_point.y();
        pb_point = pb_render_context.stop_label_offset();
        jsr_.render_context_.stop_label_offset.x = pb_point.x();
        jsr_.render_context_.stop_label_offset.y = pb_point.y();
        TransCatSerial::Color pb_color(pb_render_context.underlayer_color());
        jsr_.render_context_.underlayer_color = ConvertColorRev(pb_color);
        jsr_.render_context_.color_palette.clear();
        for (int color_num = 0; color_num < pb_render_context.color_palette_size(); ++color_num)
        {
            pb_color = pb_render_context.color_palette(color_num);
            jsr_.render_context_.color_palette.push_back(ConvertColorRev(pb_color));
        }
    }

    void Serializer::DeserializeStopNameToVertex(TransCatSerial::StopNameToVertexList pb_stop_name_to_vertex_list,
                                                 bool EnterOrExitList, NameConvertSet& cnv)
    {
        if (EnterOrExitList)
            jsr_.bus_router_ptr_->stop_name_to_enter_vertex_.clear();
        else
            jsr_.bus_router_ptr_->stop_name_to_exit_vertex_.clear();

        for (int stop_to_vertex_recnum = 0;
            stop_to_vertex_recnum < pb_stop_name_to_vertex_list.stop_name_to_vertex_size();
            ++stop_to_vertex_recnum)
        {
            TransCatSerial::StopNameToVertex stop_name_to_vertex =
                pb_stop_name_to_vertex_list.stop_name_to_vertex(stop_to_vertex_recnum);
            if (EnterOrExitList)
                jsr_.bus_router_ptr_->stop_name_to_enter_vertex_[cnv.pb_number_to_stop_name_[stop_name_to_vertex.stop_name_num()]] =
                    stop_name_to_vertex.vertex_id();
            else
                jsr_.bus_router_ptr_->stop_name_to_exit_vertex_[cnv.pb_number_to_stop_name_[stop_name_to_vertex.stop_name_num()]] =
                    stop_name_to_vertex.vertex_id();
        }
    }

    void Serializer::DeserializeEdgeToDesc(TransCatSerial::EdgeToDescList pb_edge_to_desc_list, NameConvertSet& cnv)
    {
        jsr_.bus_router_ptr_->edge_to_desc_.clear();
        for (int edge_to_desc_recnum = 0;
            edge_to_desc_recnum < pb_edge_to_desc_list.edge_to_desc_size();
            ++edge_to_desc_recnum)
        {
            TransCatSerial::EdgeToDesc pb_edge_to_desc = pb_edge_to_desc_list.edge_to_desc(edge_to_desc_recnum);
            TransCatSerial::EdgeDescriptor pb_edge_desc = pb_edge_to_desc.edge_descriptor();
            router::BusRouter::EdgeDescriptor edge_desc;
            edge_desc.edge_type = static_cast<router::BusRouter::EdgeType>(static_cast<int>(pb_edge_desc.edge_type()));
            edge_desc.from_stop = cnv.pb_number_to_stop_name_[pb_edge_desc.from_stop_num()];
            edge_desc.to_stop = cnv.pb_number_to_stop_name_[pb_edge_desc.to_stop_num()];
            edge_desc.bus_name = cnv.pb_number_to_bus_name_[pb_edge_desc.bus_name_num()];
            edge_desc.span_count = pb_edge_desc.span_count();
            edge_desc.time_length = pb_edge_desc.time_length();
            jsr_.bus_router_ptr_->edge_to_desc_[pb_edge_to_desc.edge_id()] = edge_desc;
        }
    }

    void Serializer::DeserializeEdges(TransCatSerial::Edges pb_edges_list)
    {
        using GraphNC = decay_t<decltype(jsr_.bus_router_ptr_->router_.graph_)>;
        GraphNC& graph_nc(const_cast<GraphNC&>(jsr_.bus_router_ptr_->router_.graph_));
        graph_nc.edges_.clear();
        for (int edges_recnum = 0; edges_recnum < pb_edges_list.edges_size(); ++edges_recnum)
        {
            TransCatSerial::Edge pb_edge = pb_edges_list.edges(edges_recnum);
            decltype(graph_nc.edges_)::value_type edge;
            edge.from = pb_edge.from();
            edge.to = pb_edge.to();
            edge.weight = pb_edge.weight();
            graph_nc.edges_.push_back(edge);
        }
    }

    void Serializer::DeserializeIncidenceList(TransCatSerial::IncidenceLists pb_vertex_incidence_lists)
    {
        using GraphNC = decay_t<decltype(jsr_.bus_router_ptr_->router_.graph_)>;
        GraphNC& graph_nc(const_cast<GraphNC&>(jsr_.bus_router_ptr_->router_.graph_));
        graph_nc.incidence_lists_.clear();
        // Определённый далее incidence_list_num совпадает, фактически, с номером вершины графа,
        // для которой составлен данный список смежности.
        for (int incidence_list_num = 0;
            incidence_list_num < pb_vertex_incidence_lists.incidence_list_size();
            ++incidence_list_num)
        {
            decltype(graph_nc.incidence_lists_)::value_type incidence_list;
            TransCatSerial::IncidenceList pb_incidence_list(pb_vertex_incidence_lists.incidence_list(incidence_list_num));
            for (int edge_id_num = 0; edge_id_num < pb_incidence_list.edge_id_size(); ++edge_id_num)
                incidence_list.push_back(pb_incidence_list.edge_id(edge_id_num));
            graph_nc.incidence_lists_.push_back(incidence_list);
        }
    }

    void Serializer::DeserializeRoutesData(TransCatSerial::RoutesData pb_routes_data)
    {
        jsr_.bus_router_ptr_->router_.routes_internal_data_.clear();
        using RoutesSecIndexData = decltype(jsr_.bus_router_ptr_->router_.routes_internal_data_)::value_type;
        using OptRouteData = RoutesSecIndexData::value_type;
        using RouteData = OptRouteData::value_type;
        for (int first_index = 0; first_index < pb_routes_data.first_routes_data_size(); ++first_index)
        {
            RoutesSecIndexData sec_routes_data;
            TransCatSerial::RoutesSecIndexData pb_sec_routes_data(pb_routes_data.first_routes_data(first_index));
            for (int second_index = 0; second_index < pb_sec_routes_data.sec_routes_data_size(); ++second_index)
            {
                TransCatSerial::RouteData pb_routes_data_elem(pb_sec_routes_data.sec_routes_data(second_index));
                OptRouteData opt_routes_data_elem;
                if (pb_routes_data_elem.has_value())
                {
                    RouteData route_data_elem;
                    route_data_elem.weight = pb_routes_data_elem.weight();
                    if (pb_routes_data_elem.has_prev_edge_value())
                        route_data_elem.prev_edge = pb_routes_data_elem.prev_edge();
                    else
                        route_data_elem.prev_edge = nullopt;
                    opt_routes_data_elem = route_data_elem;
                }
                else
                {
                    opt_routes_data_elem = nullopt;
                }
                sec_routes_data.push_back(opt_routes_data_elem);
            }
            jsr_.bus_router_ptr_->router_.routes_internal_data_.push_back(sec_routes_data);
        }
    }

    void Serializer::Deserialize()
    {
        TransCatSerial::TransportCatalogue pb_transport_cataloque;
        ifstream ifs(jsr_.serial_context_.input_file, ios_base::binary);
        pb_transport_cataloque.ParseFromIstream(&ifs);
        NameConvertSet cnv;
        // Во-первых, заполним два основных массива транспортного справочника - множество остановок jsr_.trans_cat_.stops_
        // и множество описателей автобусных маршрутов jsr_.trans_cat_.buses_.
        // Создадим предварительную версию множества остановок
        DeserializeStopsInit(pb_transport_cataloque.stops(), cnv);
        // и автобусных маршрутов.
        DeserializeBusesInit(pb_transport_cataloque.buses(), cnv);
        // Закончим заполнение списков остановок и маршрутов, связав их теперь уже известными
        // перекрёстными списками.
        DeserializeStopsSec(pb_transport_cataloque.stops(), cnv);
        DeserializeBusesSec(pb_transport_cataloque.buses(), cnv);
        // Формирование списков-множеств остановок и автобусов завершено
        // Далее считаем из TransCatSerial::TransportCatalogue и заполним структуру router_context_.
        DeserializeRouterContext(pb_transport_cataloque.router_context());
        // Считаем из TransCatSerial::TransportCatalogue и заполним структуру render_context_.
        DeserializeRenderContext(pb_transport_cataloque.render_context());
        // Все дальнейшие элементы относятся к структуре маршрутизатора (маршрутизирующего графа) и будут восстанавливаться
        // только при его наличии.
        if (pb_transport_cataloque.is_routes_data())
        {
            if (!jsr_.bus_router_ptr_)
                jsr_.BuildBusRouter();
            // Восстановим ряд словарей, служащих для установления связи между именными элементами транспортного
            // справочника и численными идентификаторами соответствующих им вершин и рёбер маршрутного графа.
            // Сначала заполним словарь-преобразователь имён остановок в номера вершин - "перронов отправления".
            DeserializeStopNameToVertex(pb_transport_cataloque.stop_name_to_enter_vertex(), true, cnv);
            // Во-вторых, обрабатываем словарь-преобразователь имён остановок в номера вершин - "перронов прибытия".
            DeserializeStopNameToVertex(pb_transport_cataloque.stop_name_to_exit_vertex(), false, cnv);
            // Наконец, словарь-преобразователь номеров рёбер в их содержательное описание.
            DeserializeEdgeToDesc(pb_transport_cataloque.edge_to_desc(), cnv);
            // Заполняем список дескрипторов вершин маршрутного графа.
            DeserializeEdges(pb_transport_cataloque.edges());
            // Восстанавливаем списки смежности маршрутного графа.
            DeserializeIncidenceList(pb_transport_cataloque.incidence_lists());
            // Последняя операция - заполнение разрешающей (маршрутизирующей) матрицы.
            DeserializeRoutesData(pb_transport_cataloque.routes_data());
        }
        else
        {
            if (jsr_.bus_router_ptr_)
                delete jsr_.bus_router_ptr_;        
        }
        // Собственно, всё. Состояние программы полностью восстановлено в соответствии со входным
        // сериализующим файлом.
    }
} // namespace serial
