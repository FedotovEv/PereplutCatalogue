#pragma once

#include <string>
#include <vector>
#include <set>
#include <unordered_map>

#include "geo.h"

namespace transport
{

    enum class StopType
    {
        STOPTYPE_UNDEFINED = 0,
        STOPTYPE_ORDINAR
    };

    struct StopDescriptor
    {
        StopType stop_type;
        std::string stop_name;
        detail::Coordinates stop_coords;
        std::set<std::string> buses_at_stop;
        std::unordered_map<std::string, double> distance_to_stop;
    };

    enum class BusType
    {
        BUSTYPE_UNDEFINED = 0,
        BUSTYPE_ORDINAR,
        BUSTYPE_CIRCULAR
    };

    struct BusDescriptor
    {
        BusType bus_type;
        std::string bus_name;
        std::vector<std::string> bus_stops;
    };

    struct TCCommonMetric
    {
        size_t stops_count;
        size_t buses_count;
    };
} //namespace transport
