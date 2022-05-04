cmake_minimum_required(VERSION 3.10)

project(TransportCatalogue CXX)
set(CMAKE_CXX_STANDARD 17)

find_package(Protobuf REQUIRED)
find_package(Threads REQUIRED)

protobuf_generate_cpp(PROTO_SRCS_GR PROTO_HDRS_GR graph.proto)
protobuf_generate_cpp(PROTO_SRCS_SVG PROTO_HDRS_SVG svg.proto)
protobuf_generate_cpp(PROTO_SRCS_MR PROTO_HDRS_MR map_renderer.proto)
protobuf_generate_cpp(PROTO_SRCS_TC PROTO_HDRS_TC transport_catalogue.proto)

set(PROTO_SRCS ${PROTO_SRCS_GR} ${PROTO_SRCS_SVG} ${PROTO_SRCS_MR} ${PROTO_SRCS_TC})
set(PROTO_HDRS ${PROTO_HDRS_GR} ${PROTO_HDRS_SVG} ${PROTO_HDRS_MR} ${PROTO_HDRS_TC})

set(TRANSPORT_CATALOGUE_FILES domain.h geo.h graph.h json.cpp json.h json_builder.cpp json_builder.h
                              json_reader.cpp json_reader.h main.cpp map_renderer.cpp map_renderer.h
                              ranges.h router.h serialization.cpp serialization.h svg.cpp svg.h
                              transport_catalogue.cpp transport_catalogue.h transport_catalogue.pb.cc transport_catalogue.pb.h
                              transport_router.cpp transport_router.h
                              graph.proto svg.proto map_renderer.proto transport_catalogue.proto)

add_executable(transport_catalogue ${PROTO_SRCS} ${PROTO_HDRS} ${TRANSPORT_CATALOGUE_FILES})
target_include_directories(transport_catalogue PUBLIC ${Protobuf_INCLUDE_DIRS})
target_include_directories(transport_catalogue PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

string(REPLACE "protobuf.lib" "protobufd.lib" "Protobuf_LIBRARY_DEBUG" "${Protobuf_LIBRARY_DEBUG}")
string(REPLACE "protobuf.a" "protobufd.a" "Protobuf_LIBRARY_DEBUG" "${Protobuf_LIBRARY_DEBUG}")

target_link_libraries(transport_catalogue "$<IF:$<CONFIG:Debug>,${Protobuf_LIBRARY_DEBUG},${Protobuf_LIBRARY}>" Threads::Threads)
