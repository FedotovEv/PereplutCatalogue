#pragma once

#include <cmath>

namespace detail
{
    struct Coordinates
    {
        double lat;
        double lng;
    };

    inline double ComputeDistance(Coordinates from, Coordinates to)
    {
        using namespace std;
        static const double radians_in_degree = 3.1415926535 / 180.;
        static const double earth_radius = 6371000;

        return acos(sin(from.lat * radians_in_degree) * sin(to.lat * radians_in_degree)
                    + cos(from.lat * radians_in_degree) * cos(to.lat * radians_in_degree)
                    * cos(abs(from.lng - to.lng) * radians_in_degree)) * earth_radius;
    }
} //namespace detail
