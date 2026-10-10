#ifndef NET_APP_UNIT_CONVERT_H
#define NET_APP_UNIT_CONVERT_H

#include "net_common/net_enums.h"

float temp_kelvin_to_unit(float k, net_enum_temp_unit_t unit);
float temp_unit_to_kelvin(float v, net_enum_temp_unit_t unit);
float temp_kelvin_delta_to_unit(float delta_k, net_enum_temp_unit_t unit);
float temp_unit_delta_to_kelvin(float delta_v, net_enum_temp_unit_t unit);
float dist_meter_to_unit(float m, net_enum_distance_unit_t unit);
float dist_unit_to_meter(float v, net_enum_distance_unit_t unit);

#endif
