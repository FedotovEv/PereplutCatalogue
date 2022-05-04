#pragma once
#include <iostream>
#include <string>

#include "json.h"
#include "json_builder.h"
#include "svg.h"
#include "map_renderer.h"
#include "transport_catalogue.h"
#include "transport_router.h"
#include "serialization.h"

namespace transport::reader
{
    class JSONReader
    {
    public:
        friend class serial::Serializer;
        JSONReader(std::istream& input, TransportCatalogue& trans_cat);
        // Публичный метод для исполнения запросов на ввод информации в базу данных
        void ProcessAddInfoRequests();
        // Публичный метод для исполнения запросов для получения информации из базы данных
        json::Document ProcessGetInfoRequests();
        // Метод для принудительной постройки или перестройки маршрутизирующего графа на основе
        // текущего состояния базы данных транспортного справочника.
        void BuildBusRouter();
        // Исполнители запросов на упоследование/распоследование
        void ProcessSerialize();
        void ProcessDeserialize();

        inline size_t GetRequestsSize(bool is_get_info_reqs)
        {
            return is_get_info_reqs ? stat_requests_.size() : base_requests_.size();
        }

        ~JSONReader()
        {
            delete bus_router_ptr_;
            bus_router_ptr_ = nullptr;
        }

    private:
        static constexpr char BASE_REQUEST_NAME[] = "base_requests";
        static constexpr char STAT_REQUEST_NAME[] = "stat_requests";

        static constexpr char RENDER_SETTINGS_NAME[] = "render_settings";
        static constexpr char ROUTER_SETTINGS_NAME[] = "routing_settings";
        static constexpr char SERIAL_SETTINGS_NAME[] = "serialization_settings";

        json::Document json_document_;
        transport::TransportCatalogue& trans_cat_;
        json::Array base_requests_;
        json::Array stat_requests_;
        renderer::MapRendererContext render_context_;
        serial::SerializationContext serial_context_;
        router::RouterContext router_context_;
        router::BusRouter *bus_router_ptr_;

        // Считыватели контекстной информации
        void ReadRouterContext(const json::Dict& rndc);
        void ReadRenderContext(const json::Dict& rndc);
        void ReadSerializationContext(const json::Dict& rndc);
        // Исполнители запросов на ввод
        void ProcessAddBusRequest(const json::Dict& cur_dict);
        void ProcessAddStopRequest(const json::Dict& cur_dict);
        // Исполнители запросов на вывод
        void ProcessGetBusRequest(const json::Dict& cur_dict, json::Builder& result);
        void ProcessGetStopRequest(const json::Dict& cur_dict, json::Builder& result);
        void ProcessRouteRequest(const json::Dict& cur_dict, json::Builder& result);
        // Вспомогательные методы класса
        svg::Color DecodeJSONColor(const json::Node& node);
    };
}
