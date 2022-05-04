
#include <string>
#include <optional>
#include <variant>
#include <unordered_map>

#include "geo.h"
#include "svg.h"
#include "domain.h"
#include "transport_router.h"
#include "transport_catalogue.h"

using namespace transport;
using namespace graph;
using namespace std;

namespace router
{
    BusRouter::BusRouter(const RouterContext& rc, const transport::TransportCatalogue& tc) :
                         router_context_(rc), tc_(tc), catalogue_graph_(ConstructGraph()), router_(catalogue_graph_)
    {}

    size_t BusRouter::RegisterVertexes()
    {
        // Вычисляем необходимое нам количество вершин строящегося графа,
        // одновременно регистрируем их в словарях stop_name_to_enter_vertex_
        // и stop_name_to_exit_vertex для дальнейшего использования при проведении
        // рёбер и построении маршрутов.
        // Для этого перечисляем все имеющиеся остановки и заводим для каждой
        // "перрон отправления" (вершину для исходящих рёбер) и "перрон прибытия"
        // (вершину для входящих ребёр).
        size_t vertex_counter = 0;
        for (auto stop_it = tc_.stop_begin(); stop_it != tc_.stop_end(); ++stop_it)
        {
            StopDescriptor stop_desc = *stop_it;
            stop_name_to_enter_vertex_[stop_desc.stop_name] = vertex_counter;
            ++vertex_counter;
            stop_name_to_exit_vertex_[stop_desc.stop_name] = vertex_counter;
            ++vertex_counter;
        }
        return vertex_counter;
    }

    void BusRouter::BuildTranferEdges(GraphT& result)
    { // Процедура создаёт пересадочные рёбра между "перроном отправления" и "перроном прибытия".
        for (auto stop_it = tc_.stop_begin(); stop_it != tc_.stop_end(); ++stop_it)
        {
            StopDescriptor stop_desc = *stop_it;
            EdgeT edge_transfer{stop_name_to_enter_vertex_[stop_desc.stop_name],
                                stop_name_to_exit_vertex_[stop_desc.stop_name], router_context_.bus_wait_time};
            edge_to_desc_[result.AddEdge(edge_transfer)] =
                {EdgeType::EDGE_TRANSFER, stop_desc.stop_name, ""s, ""s, 1, edge_transfer.weight};
        }
    }

    void BusRouter::BuildRideEdges(GraphT& result)
    { // Функция строит "поездные" рёбра, соответствующие каждому возможному отрезку пути
      // при поездке на конкретном автобусе. Эти отрезки попарно соединяют каждые две остановки,
      // между которыми можно совершить поездку по маршруту.
        for (const BusDescriptor bds : tc_)
        {
            if (bds.bus_stops.size() < 2)
                continue;
            for (size_t i = 0; i < bds.bus_stops.size() - 1; ++i)
            {
                double i_j_distance = 0, j_i_distance = 0;
                for (size_t j = i + 1; j < bds.bus_stops.size(); ++j)
                {
                    i_j_distance += tc_.CountNeighborsDistance(bds.bus_stops[j - 1], bds.bus_stops[j]).first;
                    j_i_distance += tc_.CountNeighborsDistance(bds.bus_stops[j], bds.bus_stops[j - 1]).first;
                    // Прокладывем ребро от остановки i к остановке j
                    EdgeT edge_i_j{stop_name_to_exit_vertex_[bds.bus_stops[i]],
                                   stop_name_to_enter_vertex_[bds.bus_stops[j]],
                                   i_j_distance / (router_context_.bus_velocity * 1000.0 / 60.0)};
                    edge_to_desc_[result.AddEdge(edge_i_j)] =
                        {EdgeType::EDGE_STAGE, bds.bus_stops[i], bds.bus_stops[j], bds.bus_name,
                        static_cast<int>(j - i), edge_i_j.weight};
                    if (bds.bus_type == BusType::BUSTYPE_ORDINAR)
                    { // Для обыкновенного, некольцевого, автобуса с двусторонним движением проведём также и обратное ребро, от j до i.
                        EdgeT edge_j_i{stop_name_to_exit_vertex_[bds.bus_stops[j]],
                                       stop_name_to_enter_vertex_[bds.bus_stops[i]],
                                       j_i_distance / (router_context_.bus_velocity * 1000.0 / 60.0)};
                        edge_to_desc_[result.AddEdge(edge_j_i)] =
                            {EdgeType::EDGE_STAGE, bds.bus_stops[j], bds.bus_stops[i], bds.bus_name,
                            static_cast<int>(j - i), edge_j_i.weight};
                    }
                }
            }
        }
    }

    BusRouter::GraphT BusRouter::ConstructGraph()
    {
        stop_name_to_enter_vertex_.clear();
        stop_name_to_exit_vertex_.clear();
        // Создаём граф с нужным числом вершин и пока без рёбер.
        GraphT result(RegisterVertexes());
        edge_to_desc_.clear();
        // Теперь займёмся прокладкой рёбер. Между каждой парой вершин, принадлежащих одной остановке,
        // проведём пересадочные рёбра.
        BuildTranferEdges(result);
        // Наконец, проводим, собственно, маршрутные рёбра.
        BuildRideEdges(result);
        return result;
    }

    optional<RouteResult> BusRouter::DoRoute(const string& from, const string& to)
    {
        RouteResult result;

        if (!stop_name_to_enter_vertex_.count(from) || !stop_name_to_enter_vertex_.count(to))
            return nullopt;
        VertexId from_vertex = stop_name_to_enter_vertex_.at(from);
        VertexId to_vertex = stop_name_to_enter_vertex_.at(to);
        auto br = router_.BuildRoute(from_vertex, to_vertex);
        if (!br)
            return nullopt;

        result.total_time = br->weight;

        for (const EdgeId& edge : br->edges)
        {
            EdgeDescriptor ed = edge_to_desc_.at(edge);
            switch (ed.edge_type)
            {
                case EdgeType::EDGE_TRANSFER:
                    result.route_items.push_back(WaitEvent{ed.from_stop, ed.time_length});
                    break;
                case EdgeType::EDGE_STAGE:
                    result.route_items.push_back(RideEvent{ed.bus_name, ed.span_count, ed.time_length});
                    break;
                default:
                    break;
            }
        }

        return result;
    }
} // namespace router
