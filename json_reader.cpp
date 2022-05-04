
#include <string>
#include <vector>
#include <map>
#include <unordered_set>
#include <algorithm>
#include <optional>
#include <variant>
#include <fstream>

#include "json_reader.h"
#include "domain.h"
#include "transport_catalogue.h"
#include "map_renderer.h"
#include "json_builder.h"
#include "transport_router.h"
#include "serialization.h"

using namespace transport;
using namespace detail;
using namespace json;
using namespace svg;
using namespace renderer;
using namespace router;
using namespace serial;
using namespace std;

namespace transport::reader
{
    Color JSONReader::DecodeJSONColor(const Node& node)
    {
        if (node.IsString())
        {
            return node.AsString();
        }
        else if (node.IsArray())
        {
            if (node.AsArray().size() == 3)
                return Rgb(node.AsArray()[0].AsInt(), node.AsArray()[1].AsInt(), node.AsArray()[2].AsInt());
            else if (node.AsArray().size() == 4)
                return Rgba(node.AsArray()[0].AsInt(), node.AsArray()[1].AsInt(),
                            node.AsArray()[2].AsInt(), node.AsArray()[3].AsDouble());
        }

        return {};
    }

    void JSONReader::ReadSerializationContext(const Dict& rndc)
    {
        serial_context_.input_file = rndc.at("file").AsString();
        serial_context_.output_file = rndc.at("file").AsString();
    }

    void JSONReader::ReadRouterContext(const Dict& rndc)
    {
        router_context_.bus_wait_time = rndc.at("bus_wait_time").AsDouble();
        router_context_.bus_velocity = rndc.at("bus_velocity").AsDouble();
    }

    void JSONReader::ReadRenderContext(const Dict& rndc)
    {
        render_context_.width = rndc.at("width").AsDouble();
        render_context_.height = rndc.at("height").AsDouble();

        render_context_.padding = rndc.at("padding").AsDouble();

        render_context_.line_width = rndc.at("line_width").AsDouble();
        render_context_.stop_radius = rndc.at("stop_radius").AsDouble();

        render_context_.bus_label_font_size = rndc.at("bus_label_font_size").AsInt();
        render_context_.bus_label_offset = {rndc.at("bus_label_offset").AsArray()[0].AsDouble(),
                                            rndc.at("bus_label_offset").AsArray()[1].AsDouble()};

        render_context_.stop_label_font_size = rndc.at("stop_label_font_size").AsInt();
        render_context_.stop_label_offset = {rndc.at("stop_label_offset").AsArray()[0].AsDouble(),
                                             rndc.at("stop_label_offset").AsArray()[1].AsDouble()};

        render_context_.underlayer_color = DecodeJSONColor(rndc.at("underlayer_color"));
        render_context_.underlayer_width = rndc.at("underlayer_width").AsDouble();

        for (const Node& node : rndc.at("color_palette").AsArray())
            render_context_.color_palette.push_back(DecodeJSONColor(node));
    }

    JSONReader::JSONReader(std::istream& input, TransportCatalogue& trans_cat) :
                           json_document_(Load(input)), trans_cat_(trans_cat), bus_router_ptr_(nullptr)
    {
        if (json_document_.GetRoot().IsDict())
        {
            const Dict& root_map = json_document_.GetRoot().AsDict();
            if (root_map.count(BASE_REQUEST_NAME) && root_map.at(BASE_REQUEST_NAME).IsArray())
                base_requests_ = root_map.at(BASE_REQUEST_NAME).AsArray();
            if (root_map.count(STAT_REQUEST_NAME) && root_map.at(STAT_REQUEST_NAME).IsArray())
                stat_requests_ = root_map.at(STAT_REQUEST_NAME).AsArray();
            if (root_map.count(RENDER_SETTINGS_NAME) && root_map.at(RENDER_SETTINGS_NAME).IsDict())
                ReadRenderContext(root_map.at(RENDER_SETTINGS_NAME).AsDict());
            if (root_map.count(ROUTER_SETTINGS_NAME) && root_map.at(ROUTER_SETTINGS_NAME).IsDict())
                ReadRouterContext(root_map.at(ROUTER_SETTINGS_NAME).AsDict());
            if (root_map.count(SERIAL_SETTINGS_NAME) && root_map.at(SERIAL_SETTINGS_NAME).IsDict())
                ReadSerializationContext(root_map.at(SERIAL_SETTINGS_NAME).AsDict());
        }
    }

    void JSONReader::ProcessAddStopRequest(const Dict& cur_dict)
    {
        struct StopDescriptor stop;
        stop.stop_name = cur_dict.at("name").AsString();
        stop.stop_type = StopType::STOPTYPE_ORDINAR;
        stop.stop_coords.lat = cur_dict.at("latitude").AsDouble();
        stop.stop_coords.lng = cur_dict.at("longitude").AsDouble();
        //Разбираем параметры длины дорожных расстояний
        for (auto road_dist_pair : cur_dict.at("road_distances").AsDict())
            stop.distance_to_stop[road_dist_pair.first] = road_dist_pair.second.AsDouble();

        trans_cat_.AddStop(stop);
    }

    void JSONReader::ProcessAddBusRequest(const Dict& cur_dict)
    {
        struct BusDescriptor bus;
        bus.bus_name = cur_dict.at("name").AsString();
        bus.bus_type = BusType::BUSTYPE_UNDEFINED;
        if (cur_dict.at("is_roundtrip").AsBool())
            bus.bus_type = BusType::BUSTYPE_CIRCULAR;
        else
            bus.bus_type = BusType::BUSTYPE_ORDINAR;

        for (auto stop_node: cur_dict.at("stops").AsArray())
            bus.bus_stops.push_back(stop_node.AsString());
        trans_cat_.AddBus(bus);
    }

    void JSONReader::ProcessAddInfoRequests()
    {
        if (bus_router_ptr_)
        {
            delete bus_router_ptr_;
            bus_router_ptr_ = nullptr;
        }

        for (const Node& cur_request: base_requests_)
        {
            if (!cur_request.IsDict())
                continue;

            const Dict& cur_dict = cur_request.AsDict();
            if (!cur_dict.count("type") || !cur_dict.at("type").IsString())
                continue;

            string request_code = cur_dict.at("type").AsString();

            if (request_code == "Stop")
                ProcessAddStopRequest(cur_dict);
            else if (request_code == "Bus")
                ProcessAddBusRequest(cur_dict);
        }
    }

    void JSONReader::ProcessGetBusRequest(const Dict& cur_dict, Builder& result)
    {
        string bus_name = cur_dict.at("name").AsString();
        BusDescriptor bus = trans_cat_.GetBus(bus_name);

        if (bus.bus_type == BusType::BUSTYPE_UNDEFINED)
        {
            result.Key("error_message"s).Value("not found"s);
        }
        else
        {
            double sum_road_distance = 0, sum_geo_distance = 0;
            int stop_counter = 0, stops_all = bus.bus_stops.size();
            unordered_set<string> stops_name;

            if (stops_all)
                stops_name.insert(bus.bus_stops[0]);

            for (int stop_num = 0; stop_num < stops_all - 1; ++stop_num)
            {
                auto distance_pair = trans_cat_.CountNeighborsDistance(bus.bus_stops[stop_num],
                                                                       bus.bus_stops[stop_num + 1]);
                sum_road_distance += distance_pair.first;
                sum_geo_distance += distance_pair.second;
                stops_name.insert(bus.bus_stops[stop_num + 1]);
            }

            if (bus.bus_type == BusType::BUSTYPE_ORDINAR)
            {
                for (int stop_num = stops_all - 1; stop_num > 0; --stop_num)
                {
                    auto distance_pair = trans_cat_.CountNeighborsDistance(bus.bus_stops[stop_num],
                                                                           bus.bus_stops[stop_num - 1]);
                    sum_road_distance += distance_pair.first;
                    sum_geo_distance += distance_pair.second;
                }
                stop_counter = stops_all * 2 - 1;
            }
            else if (bus.bus_type == BusType::BUSTYPE_CIRCULAR)
            {
                stop_counter = stops_all;
            }

            result.Key("stop_count"s).Value(stop_counter)
                  .Key("unique_stop_count").Value(static_cast<int>(stops_name.size()));
            result.Key("route_length"s).Value(sum_road_distance)
                  .Key("curvature"s).Value(sum_road_distance / sum_geo_distance);
        }
    }

    void JSONReader::ProcessGetStopRequest(const Dict& cur_dict, Builder& result)
    {
        string stop_name = cur_dict.at("name").AsString();
        StopDescriptor stop = trans_cat_.GetStop(stop_name);
        int buses_all = stop.buses_at_stop.size();

        if (stop.stop_type == StopType::STOPTYPE_UNDEFINED)
        {
            result.Key("error_message"s).Value("not found"s);
        }
        else if (!buses_all)
        {
            result.Key("buses"s).StartArray().EndArray();
        }
        else
        {
            result.Key("buses"s).StartArray();
            for_each(stop.buses_at_stop.begin(), stop.buses_at_stop.end(),
                     [&result](const string& stop_name)
                     {result.Value(stop_name);});
            result.EndArray();
        }
    }

    void JSONReader::BuildBusRouter()
    {
        if (bus_router_ptr_)
            delete bus_router_ptr_;
        bus_router_ptr_ = new router::BusRouter(router_context_, trans_cat_);
    }

    void JSONReader::ProcessRouteRequest(const Dict& cur_dict, Builder& result)
    {
        if (!bus_router_ptr_)
            BuildBusRouter();
        auto route_result = bus_router_ptr_->DoRoute(cur_dict.at("from").AsString(), cur_dict.at("to").AsString());
        if (route_result)
        {
            result.Key("total_time"s).Value(route_result->total_time);
            result.Key("items"s).StartArray();

            for (const RouteItem& route_item: route_result->route_items)
                if (holds_alternative<WaitEvent>(route_item))
                {
                    const WaitEvent& we = get<WaitEvent>(route_item);
                    result.StartDict().Key("type"s).Value("Wait"s)
                                      .Key("stop_name"s).Value(we.stop_name)
                                      .Key("time"s).Value(we.wait_time)
                                      .EndDict();
                }
                else if (holds_alternative<RideEvent>(route_item))
                {
                    const RideEvent& re = get<RideEvent>(route_item);
                    result.StartDict().Key("type"s).Value("Bus"s)
                                      .Key("bus"s).Value(re.bus_name)
                                      .Key("span_count"s).Value(re.span_count)
                                      .Key("time"s).Value(re.ride_time)
                                      .EndDict();
                }

            result.EndArray();
        }
        else
        {
            result.Key("error_message"s).Value("not found"s);
        }
    }

    json::Document JSONReader::ProcessGetInfoRequests()
    {
        Builder result;
        result.StartArray();

        for (const Node& cur_request: stat_requests_)
        {
            if (!cur_request.IsDict())
                continue;

            const Dict& cur_dict = cur_request.AsDict();
            if (!cur_dict.count("type") || !cur_dict.at("type").IsString() ||
                !cur_dict.count("id") || !cur_dict.at("id").IsInt())
                continue;

            string request_code = cur_dict.at("type").AsString();
            int request_id = cur_dict.at("id").AsInt();
            result.StartDict();

            if (request_code == "Bus")
            {
                ProcessGetBusRequest(cur_dict, result);
            }
            else if (request_code == "Stop")
            {
                ProcessGetStopRequest(cur_dict, result);
            }
            else if (request_code == "Map")
            {
                svg::Document doc = MapRenderer(render_context_).Render(trans_cat_);
                ostringstream ostr;
                doc.Render(ostr);
                result.Key("map"s).Value(ostr.str());
            }
            else if (request_code == "Route")
            {
                ProcessRouteRequest(cur_dict, result);
            }

            result.Key("request_id"s).Value(request_id).EndDict();
        }

        return json::Document(result.EndArray().Build());
    }

    void JSONReader::ProcessSerialize()
    {
        Serializer sr(*this);
        sr.Serialize();
    }

    void JSONReader::ProcessDeserialize()
    {
        Serializer sr(*this);
        sr.Deserialize();
    }

} //namespace transport::reader
