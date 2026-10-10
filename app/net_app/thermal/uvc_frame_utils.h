#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

struct UvcParsedFrame
{
    uint16_t width = 0;
    uint16_t height = 0;
    uint16_t image_height = 0;
    uint32_t lut_length = 0;
    uint32_t shutter_state = 0;
    uint32_t frame_no = 0;
    uint32_t sensor_temp = 0;
    uint32_t raw_type = 0;
    uint64_t pts = 0;
    bool has_metadata = false;
    bool has_lut = false;
    std::vector<uint16_t> raw_pixels;
    std::vector<uint16_t> lut_ad;
    std::vector<float> lut_temp;
};

inline bool parse_uvc_frame_payload(
    const uint8_t* data,
    size_t length,
    uint16_t width,
    uint16_t height,
    UvcParsedFrame& out)
{
    out.width = width;
    out.height = height;
    out.lut_length = 0;
    out.shutter_state = 0;
    out.frame_no = 0;
    out.sensor_temp = 0;
    out.raw_type = 0;
    out.pts = 0;
    out.has_metadata = false;
    out.has_lut = false;

    if (!data || width == 0 || height == 0)
    {
        return false;
    }

    const size_t total_bytes = static_cast<size_t>(width) * height * sizeof(uint16_t);
    if (length < total_bytes)
    {
        return false;
    }

    out.image_height = height;
    out.has_metadata = height > 4;
    if (out.has_metadata)
    {
        out.image_height = static_cast<uint16_t>(height - 4);
    }

    const size_t raw_pixel_count = static_cast<size_t>(width) * out.image_height;
    out.raw_pixels.resize(raw_pixel_count);
    std::memcpy(out.raw_pixels.data(), data, raw_pixel_count * sizeof(uint16_t));

    if (!out.has_metadata)
    {
        return true;
    }

    const size_t meta_offset = raw_pixel_count * sizeof(uint16_t);
    auto read_u32 = [&](size_t relative_offset) -> uint32_t {
        uint32_t value = 0;
        std::memcpy(&value, data + meta_offset + relative_offset, sizeof(value));
        return value;
    };
    auto read_u64 = [&](size_t relative_offset) -> uint64_t {
        uint64_t value = 0;
        std::memcpy(&value, data + meta_offset + relative_offset, sizeof(value));
        return value;
    };

    out.lut_length = read_u32(0);
    out.sensor_temp = read_u32(8);
    out.shutter_state = read_u32(12);
    out.frame_no = read_u32(16);
    out.pts = read_u64(20);
    out.raw_type = read_u32(28);

    if (out.lut_length == 0 || out.lut_length > 4096)
    {
        return true;
    }

    const size_t temp_offset = meta_offset + 32;
    const size_t ad_offset = temp_offset + static_cast<size_t>(out.lut_length) * sizeof(float);
    const size_t end_offset = ad_offset + static_cast<size_t>(out.lut_length) * sizeof(uint16_t);
    if (end_offset > total_bytes)
    {
        return true;
    }

    out.lut_temp.resize(out.lut_length);
    out.lut_ad.resize(out.lut_length);
    std::memcpy(out.lut_temp.data(), data + temp_offset, static_cast<size_t>(out.lut_length) * sizeof(float));
    std::memcpy(out.lut_ad.data(), data + ad_offset, static_cast<size_t>(out.lut_length) * sizeof(uint16_t));
    out.has_lut = true;
    return true;
}

inline float interpolate_temperature_kelvin(
    uint16_t ad_value,
    const std::vector<uint16_t>& lut_ad,
    const std::vector<float>& lut_temp)
{
    if (lut_ad.empty() || lut_temp.empty() || lut_ad.size() != lut_temp.size())
    {
        return std::numeric_limits<float>::quiet_NaN();
    }

    if (lut_ad.size() == 1)
    {
        return lut_temp.front();
    }

    if (ad_value <= lut_ad.front())
    {
        return lut_temp.front();
    }
    if (ad_value >= lut_ad.back())
    {
        return lut_temp.back();
    }

    auto upper = std::lower_bound(lut_ad.begin(), lut_ad.end(), ad_value);
    if (upper == lut_ad.end())
    {
        return lut_temp.back();
    }
    if (*upper == ad_value)
    {
        return lut_temp[static_cast<size_t>(upper - lut_ad.begin())];
    }

    const size_t hi = static_cast<size_t>(upper - lut_ad.begin());
    const size_t lo = hi - 1;
    const uint16_t x0 = lut_ad[lo];
    const uint16_t x1 = lut_ad[hi];
    const float y0 = lut_temp[lo];
    const float y1 = lut_temp[hi];
    if (x1 == x0)
    {
        return y0;
    }

    const float ratio = static_cast<float>(ad_value - x0) / static_cast<float>(x1 - x0);
    return y0 + ratio * (y1 - y0);
}

inline float get_center_temperature_kelvin(const UvcParsedFrame& frame)
{
    if (!frame.has_lut || frame.raw_pixels.empty() || frame.width == 0 || frame.image_height == 0)
    {
        return std::numeric_limits<float>::quiet_NaN();
    }

    const size_t center_x = frame.width / 2;
    const size_t center_y = frame.image_height / 2;
    const size_t index = center_y * frame.width + center_x;
    if (index >= frame.raw_pixels.size())
    {
        return std::numeric_limits<float>::quiet_NaN();
    }

    return interpolate_temperature_kelvin(frame.raw_pixels[index], frame.lut_ad, frame.lut_temp);
}

inline float get_max_temperature_kelvin(const UvcParsedFrame& frame)
{
    if (!frame.has_lut || frame.raw_pixels.empty())
    {
        return std::numeric_limits<float>::quiet_NaN();
    }

    const auto it = std::max_element(frame.raw_pixels.begin(), frame.raw_pixels.end());
    if (it == frame.raw_pixels.end())
    {
        return std::numeric_limits<float>::quiet_NaN();
    }

    return interpolate_temperature_kelvin(*it, frame.lut_ad, frame.lut_temp);
}
