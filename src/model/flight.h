#pragma once

#include <stddef.h>

constexpr size_t MAX_FLIGHTS = 64;

struct FlightData {
    char icao24[9];
    char callsign[9];
    char callsign_iata[9];
    char origin_country[33];
    char type_code[9];
    char kind[9];
    char departure[5];
    char arrival[5];
    float lat;
    float lon;
    float altitude_m;
    float speed_mps;
    float heading_deg;
};
