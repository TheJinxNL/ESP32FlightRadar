#pragma once

#include <stddef.h>
#include <stdbool.h>

#include "../model/flight.h"

bool flight_data_fetch_flights(FlightData *out_flights, size_t out_capacity, size_t *out_count);
bool flight_data_fetch_route(const char *icao24, const char *callsign,
                         char *dep, size_t dep_size,
                         char *arr, size_t arr_size,
                         char *callsign_iata, size_t callsign_iata_size);
