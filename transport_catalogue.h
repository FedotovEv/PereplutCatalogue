#pragma once

#include <string>
#include <vector>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <iterator>

#include "domain.h"
#include "geo.h"

namespace serial
{
    class Serializer;
}

namespace transport
{

    class TransportCatalogue
    {
    public:

        void AddStop(const StopDescriptor& stop);
        void AddBus(const BusDescriptor& bus);
        BusDescriptor GetBus(const std::string& bus_name) const;
        StopDescriptor GetStop(const std::string& stop_name) const;
        TCCommonMetric GetCommonMetric() const;
        detail::Coordinates GetStopCoordinates(const std::string& stop_name) const;
        // В возвращаемой CountNeighborsDistance паре первый член - расстояние по дорогам,
        // второй член - географическое расстояние по кратчайшей.
        std::pair<double, double> CountNeighborsDistance(const std::string& from_stop_name,
                                                         const std::string& to_stop_name) const;

    private:

        friend class serial::Serializer;
        struct Bus;
        struct Stop
        {
            StopType stop_type;
            std::string stop_name;
            detail::Coordinates stop_coords;
            std::vector<const Bus*> buses_at_stop;
            std::unordered_map<const Stop*, double> distance_to_stop;
            bool operator==(const Stop& second_stop) const
            {
                return stop_name == second_stop.stop_name;
            }
        };

        struct Bus
        {
            BusType bus_type;
            std::string bus_name;
            std::vector<const Stop*> bus_stops;
            bool operator==(const Bus& second_bus) const
            {
                return bus_name == second_bus.bus_name;
            }
        };

        class StopHasher
        {
        public:

            size_t operator()(const Stop& stop) const
            {
                return std::hash<std::string>()(stop.stop_name);
            }
        };

        class BusHasher
        {
        public:

            size_t operator()(const Bus& bus) const
            {
                return std::hash<std::string>()(bus.bus_name);
            }
        };

        using StopsListType = std::unordered_set<Stop, StopHasher>;
        using BusesListType = std::unordered_set<Bus, BusHasher>;

        StopsListType stops_; //Массив зарегистрированных остановок
        BusesListType buses_;  //Массив зарегистрированных маршрутов автобусов

        //Приватные методы класса
        StopDescriptor ConvertStopToExternalFormat(const Stop& stopi) const;
        BusDescriptor ConvertBusToExternalFormat(const Bus& busi) const;
        decltype(stops_)::iterator MakeDumbStop(Stop& dumb_stop);

    public:

        class iterator : public std::iterator<std::forward_iterator_tag, BusDescriptor>
        {
        public:
            friend class TransportCatalogue;
            explicit iterator(const TransportCatalogue& tc, bool is_end_it) :
                              tc_(tc), buses_it_(is_end_it ? tc.buses_.end() : tc.buses_.begin())
            {}

            iterator& operator++()
            {
                ++buses_it_;
                return *this;
            }

            iterator operator++(int)
            {
                iterator retval = *this;
                ++(*this);
                return retval;
            }

            bool operator==(iterator other) const
            {
                return buses_it_ == other.buses_it_;
            }

            bool operator!=(iterator other) const
            {
                return !(*this == other);
            }

            value_type operator*() const
            {
                return tc_.ConvertBusToExternalFormat(*buses_it_);
            }

        private:
            const TransportCatalogue& tc_;
            decltype(TransportCatalogue::buses_)::const_iterator buses_it_;
            mutable BusDescriptor temp_bus_desc_;
        };

        class stop_iterator : public std::iterator<std::forward_iterator_tag, StopDescriptor>
        {
        public:
            friend class TransportCatalogue;
            explicit stop_iterator(const TransportCatalogue& tc, bool is_end_it) :
                                   tc_(tc), stops_it_(is_end_it ? tc.stops_.end() : tc.stops_.begin())
            {}

            stop_iterator& operator++()
            {
                ++stops_it_;
                return *this;
            }

            stop_iterator operator++(int)
            {
                stop_iterator retval = *this;
                ++(*this);
                return retval;
            }

            bool operator==(stop_iterator other) const
            {
                return stops_it_ == other.stops_it_;
            }

            bool operator!=(stop_iterator other) const
            {
                return !(*this == other);
            }

            value_type operator*() const
            {
                return tc_.ConvertStopToExternalFormat(*stops_it_);
            }

        private:
            const TransportCatalogue& tc_;
            decltype(TransportCatalogue::stops_)::const_iterator stops_it_;
            mutable StopDescriptor temp_stop_desc_;
        };

        friend class iterator;
        friend class stop_iterator;
        iterator begin() const
        {
            return iterator(*this, false);
        }

        iterator end() const
        {
            return iterator(*this, true);
        }

        stop_iterator stop_begin() const
        {
            return stop_iterator(*this, false);
        }

        stop_iterator stop_end() const
        {
            return stop_iterator(*this, true);
        }
    };

} //namespace transport
