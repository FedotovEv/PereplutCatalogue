#pragma once

#include <string>
#include <optional>
#include <variant>

#include "geo.h"
#include "svg.h"
#include "graph.h"
#include "router.h"
#include "transport_catalogue.h"
#include "serialization.h"

namespace router
{
    struct RouterContext
    {
        double bus_wait_time;
        double bus_velocity;
    };

    struct WaitEvent
    {
        std::string stop_name;
        double wait_time;
    };

    struct RideEvent
    {
        std::string bus_name;
        int span_count;
        double ride_time;
    };

    using RouteItem = std::variant<std::monostate, WaitEvent, RideEvent>;

    struct RouteResult
    {
        double total_time;
        std::vector<RouteItem> route_items;
    };

    class BusRouter
    {
    public:
        friend class serial::Serializer;
        BusRouter(const RouterContext& rc, const transport::TransportCatalogue& tc);
        std::optional<RouteResult> DoRoute(const std::string& from, const std::string& to);

    private:

        static constexpr double ZERO_TOLERANCE = 1E-6;
        using WeightT = double;
        using EdgeT = graph::Edge<WeightT>;
        using GraphT = graph::DirectedWeightedGraph<WeightT>;
        using RouterT = graph::Router<WeightT>;
        using StopToVertexT = std::unordered_map<std::string, graph::VertexId>;

        enum class EdgeType
        {
            EDGE_UNKNOWN = 0,
            EDGE_STAGE,
            EDGE_TRANSFER
        };

        struct EdgeDescriptor
        {
            EdgeType edge_type;
            std::string from_stop;
            std::string to_stop;
            std::string bus_name;
            int span_count;
            double time_length;
        };

        using EdgeToDescT = std::unordered_map<graph::EdgeId, EdgeDescriptor>;

        // Переменные, которые уже должны быть проинициализированы к моменты вызова конструктора BusRouter
        StopToVertexT stop_name_to_enter_vertex_;
        StopToVertexT stop_name_to_exit_vertex_;
        EdgeToDescT edge_to_desc_;

        // Переменные, входящие в список инициализации конструктора BusRouter
        // Должны инициализироваться и следовать в объявлении класса именно в таком порядке
        const RouterContext& router_context_;
        const transport::TransportCatalogue& tc_;
        GraphT catalogue_graph_;
        RouterT router_;

        //Приватные методы класса
        GraphT ConstructGraph();
        size_t RegisterVertexes();
        void BuildTranferEdges(GraphT& result);
        void BuildRideEdges(GraphT& result);
    };
} // namespace router
