
#include <limits>

#include "transport_catalogue.h"
#include "geo.h"

using namespace std;
using namespace transport;
using namespace detail;

StopDescriptor TransportCatalogue::ConvertStopToExternalFormat(const Stop& stopi) const
{
    StopDescriptor result;

    result.stop_name = stopi.stop_name;
    result.stop_type = stopi.stop_type;
    result.stop_coords = stopi.stop_coords;
    for (const Bus* current_bus: stopi.buses_at_stop)
        result.buses_at_stop.insert(current_bus->bus_name);

    for (auto current_stop_pair : stopi.distance_to_stop)
        result.distance_to_stop[current_stop_pair.first->stop_name] = current_stop_pair.second;

    return result;
}

BusDescriptor TransportCatalogue::ConvertBusToExternalFormat(const Bus& busi) const
{
    BusDescriptor result;

    result.bus_name = busi.bus_name;
    result.bus_type = busi.bus_type;
    for (const Stop* current_stop: busi.bus_stops)
        result.bus_stops.push_back(current_stop->stop_name);

    return result;
}

Coordinates TransportCatalogue::GetStopCoordinates(const string& stop_name) const
{
    Stop stopi;
    stopi.stop_name = stop_name;
    auto stop_it = stops_.find(stopi);
    if (stop_it == stops_.end())
        return {numeric_limits<double>::min(), numeric_limits<double>::min()};
    else
        return stop_it->stop_coords;
}

decltype(TransportCatalogue::stops_)::iterator TransportCatalogue::MakeDumbStop(Stop& dumb_stop)
{
    dumb_stop.stop_type = StopType::STOPTYPE_UNDEFINED;
    dumb_stop.stop_coords.lat = numeric_limits<double>::min();
    dumb_stop.stop_coords.lng = numeric_limits<double>::min();
    return stops_.insert(dumb_stop).first;
}

// В возвращаемой паре первый член - расстояние по дорогам,
// второй член - географическое расстояние по кратчайшей.
pair<double, double> TransportCatalogue::CountNeighborsDistance(const string& from_stop_name,
                                                                const string& to_stop_name) const
{
    double road_distance = 0, geo_distance = 0;

    Stop from_stop, to_stop;
    from_stop.stop_name = from_stop_name;
    auto from_stop_it = stops_.find(from_stop);
    to_stop.stop_name = to_stop_name;
    auto to_stop_it = stops_.find(to_stop);
    if (from_stop_it == stops_.end() || to_stop_it == stops_.end())
        return {0, 0};

    if (from_stop_name != to_stop_name)
        geo_distance = ComputeDistance(from_stop_it->stop_coords, to_stop_it->stop_coords);

    if (from_stop_it->distance_to_stop.count(&(*to_stop_it)))
        road_distance = from_stop_it->distance_to_stop.at(&(*to_stop_it));
    else if (to_stop_it->distance_to_stop.count(&(*from_stop_it)))
        road_distance = to_stop_it->distance_to_stop.at(&(*from_stop_it));
    else
        road_distance = geo_distance;

    return {road_distance, geo_distance};
}

void TransportCatalogue::AddStop(const StopDescriptor& stop)
{
    Stop stopi;

    stopi.stop_type = stop.stop_type;
    stopi.stop_name = stop.stop_name;
    stopi.stop_coords = stop.stop_coords;
    //Заполняем массив distance_to_stop - список актуальных расстояний до других остановок по дорогам
    for (auto to_stop_pair : stop.distance_to_stop)
    {
        Stop to_stop;
        to_stop.stop_name = to_stop_pair.first;
        auto to_stop_it = stops_.find(to_stop);
        if (to_stop_it == stops_.end())
            to_stop_it = MakeDumbStop(to_stop); //Такой остановки ещё нет, добавим вместо неё болванку
        stopi.distance_to_stop[&(*to_stop_it)] = to_stop_pair.second;
    }

    auto [stop_it, is_added] = stops_.insert(stopi);
    if (!is_added)
    {
        if (stop_it->stop_coords.lat == numeric_limits<double>::min() &&
            stop_it->stop_coords.lng == numeric_limits<double>::min() &&
            stop_it->stop_type == StopType::STOPTYPE_UNDEFINED)
        {
            auto node_ex = stops_.extract(stop_it);
            Stop& old_stop = node_ex.value();
            old_stop.stop_type = stopi.stop_type;
            old_stop.stop_coords = stopi.stop_coords;
            old_stop.distance_to_stop.merge(stopi.distance_to_stop);
            stops_.insert(move(node_ex));
        }
    }
}

void TransportCatalogue::AddBus(const BusDescriptor& bus)
{
    Bus busi;
    busi.bus_type = bus.bus_type;
    busi.bus_name = bus.bus_name;
    if (buses_.count(busi))
        return;
    for (const string& current_stop_name : bus.bus_stops)
    {
        Stop current_stop;
        current_stop.stop_name = current_stop_name;
        auto current_stop_it = stops_.find(current_stop);
        if (current_stop_it == stops_.end())
            current_stop_it = MakeDumbStop(current_stop); //Такой остановки ещё нет, добавим вместо неё болванку
        busi.bus_stops.push_back(&(*current_stop_it));
    }

    const Bus* bus_ptr = &(*buses_.insert(busi).first);

    //Добавляем создаваемый маршрут в список останавливающихся автобусов для каждой его остановки
    for (const Stop* stop_ptr : busi.bus_stops)
        const_cast<Stop*>(stop_ptr)->buses_at_stop.push_back(bus_ptr);
}

BusDescriptor TransportCatalogue::GetBus(const string& bus_name) const
{
    BusDescriptor result;
    Bus busi;

    busi.bus_name = bus_name;
    auto bus_it = buses_.find(busi);
    if (bus_it == buses_.end())
    {
        result.bus_type = BusType::BUSTYPE_UNDEFINED;
        result.bus_name = bus_name;
    }
    else
    {
        result = ConvertBusToExternalFormat(*bus_it);
    }

    return result;
}

StopDescriptor TransportCatalogue::GetStop(const string& stop_name) const
{
    StopDescriptor result;
    Stop stopi;

    stopi.stop_name = stop_name;
    auto stop_it = stops_.find(stopi);
    if (stop_it == stops_.end())
    {
        result.stop_type = StopType::STOPTYPE_UNDEFINED;
        result.stop_name = stop_name;
        result.stop_coords = {numeric_limits<double>::min(), numeric_limits<double>::min()};
    }
    else
    {
        result = ConvertStopToExternalFormat(*stop_it);
    }

    return result;
}

TCCommonMetric TransportCatalogue::GetCommonMetric() const
{
    return {stops_.size(), buses_.size()};
}
