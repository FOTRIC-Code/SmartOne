#include "libs/unit/unit_convert.h"

#define KELVIN_OFFSET 273.15f

float temp_kelvin_to_unit(float k, net_enum_temp_unit_t unit)
{
	switch (unit) {
	case NET_TEMP_UNIT_CELSIUS:
		return k - KELVIN_OFFSET;
	case NET_TEMP_UNIT_FAHRENHEIT:
		return (k - KELVIN_OFFSET) * 9.0f / 5.0f + 32.0f;
	case NET_TEMP_UNIT_KELVIN:
	default:
		return k;
	}
}

float temp_unit_to_kelvin(float v, net_enum_temp_unit_t unit)
{
	switch (unit) {
	case NET_TEMP_UNIT_CELSIUS:
		return v + KELVIN_OFFSET;
	case NET_TEMP_UNIT_FAHRENHEIT:
		return (v - 32.0f) * 5.0f / 9.0f + KELVIN_OFFSET;
	case NET_TEMP_UNIT_KELVIN:
	default:
		return v;
	}
}

float temp_kelvin_delta_to_unit(float delta_k, net_enum_temp_unit_t unit)
{
	if (unit == NET_TEMP_UNIT_FAHRENHEIT)
		return delta_k * 9.0f / 5.0f;
	return delta_k;
}

float temp_unit_delta_to_kelvin(float delta_v, net_enum_temp_unit_t unit)
{
	if (unit == NET_TEMP_UNIT_FAHRENHEIT)
		return delta_v * 5.0f / 9.0f;
	return delta_v;
}

float dist_meter_to_unit(float m, net_enum_distance_unit_t unit)
{
	return unit == NET_DISTANCE_UNIT_FEET ? m / 0.3048f : m;
}

float dist_unit_to_meter(float v, net_enum_distance_unit_t unit)
{
	return unit == NET_DISTANCE_UNIT_FEET ? v * 0.3048f : v;
}
