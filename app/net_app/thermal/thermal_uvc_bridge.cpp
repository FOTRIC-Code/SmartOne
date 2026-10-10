#include "thermal_uvc_bridge.h"

#include "libs/irtek/irtek_file_sdk_loader.h"
#include "libs/log/log.h"
#include "video_process/image_convert.h"

#include "IRtekUVCSDK/IRtekUVCSDK.h"
#include "uvc_frame_utils.h"

extern "C" {
#include "net_common/net_common.h"
}

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <limits>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace
{
constexpr const char* kFileSdkScratchPath = "/tmp/net_app_thermal_preview.irs";
/* Hotplug can surface the USB device before Novatek vendor controls answer.
 * Starting the stream in that window crashes inside libIRtekUVCSDK
 * (PROBE/COMMIT / bulk worker). Wait for at least one config GET to succeed. */
constexpr int kDeviceReadyAttempts = 8;
constexpr int kDeviceReadyDelayMs = 250;

void copy_status_message(char* dest, size_t length, const std::string& message)
{
    if (!dest || length == 0)
    {
        return;
    }

    dest[0] = '\0';
    if (message.empty())
    {
        return;
    }

    std::strncpy(dest, message.c_str(), length - 1);
    dest[length - 1] = '\0';
}

bool marker_requests_center(const char* marker_name)
{
    return marker_name && std::strstr(marker_name, "center") != nullptr;
}

class ThermalUvcProvider
{
public:
    static ThermalUvcProvider& instance()
    {
        static ThermalUvcProvider provider;
        return provider;
    }

    int start()
    {
        std::lock_guard<std::mutex> api(_api_mutex);
        std::lock_guard<std::mutex> life(_lifecycle_mutex);
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_streaming)
            {
                return 0;
            }
            clear_last_error_locked();
        }

        if (!ensure_sdk_locked())
        {
            return -1;
        }

        ensure_worker_locked();
        try_open_from_list();
        return 0;
    }

    void stop()
    {
        std::lock_guard<std::mutex> api(_api_mutex);
        {
            std::lock_guard<std::mutex> life(_lifecycle_mutex);
            unregister_hotplug();
        }

        {
            std::lock_guard<std::mutex> lock(_hotplug_mutex);
            _worker_running = false;
            _hotplug_pending = false;
            _hotplug_cv.notify_all();
        }
        if (_worker.joinable())
        {
            _worker.join();
        }

        std::lock_guard<std::mutex> life(_lifecycle_mutex);
        close_session();
        cleanup_sdk();
    }

    int get_marker_max_temp(const char* marker_name, float* temp_celsius)
    {
        if (!temp_celsius)
        {
            return -1;
        }

        std::lock_guard<std::mutex> lock(_mutex);
        if (!_has_raw)
        {
            return -1;
        }

        float value_kelvin = _max_temp_kelvin;
        if (marker_requests_center(marker_name))
        {
            value_kelvin = _center_temp_kelvin;
        }

        if (!std::isfinite(value_kelvin))
        {
            return -1;
        }

        *temp_celsius = value_kelvin - 273.15f;
        return 0;
    }

    int copy_latest_raw(uint16_t* pixels, size_t capacity, size_t* count,
                        int* width, int* height, unsigned int* frame_no)
    {
        if (!count)
        {
            return -1;
        }

        std::lock_guard<std::mutex> lock(_mutex);
        if (!_has_raw || _latest_raw.raw_pixels.empty())
        {
            *count = 0;
            return -1;
        }

        const size_t needed = _latest_raw.raw_pixels.size();
        *count = needed;
        if (width)
        {
            *width = static_cast<int>(_latest_raw.width);
        }
        if (height)
        {
            *height = static_cast<int>(_latest_raw.image_height);
        }
        if (frame_no)
        {
            *frame_no = _latest_raw.frame_no;
        }
        if (!pixels || capacity < needed)
        {
            return -1;
        }

        std::memcpy(pixels, _latest_raw.raw_pixels.data(),
                    needed * sizeof(uint16_t));
        return 0;
    }

    int get_status(thermal_provider_status_t* status)
    {
        if (!status)
        {
            return -1;
        }

        std::lock_guard<std::mutex> lock(_mutex);
        std::memset(status, 0, sizeof(*status));
        status->connected = _logged_in ? 1 : 0;
        status->streaming = _streaming ? 1 : 0;
        status->file_sdk_ready = _file_sdk_loaded ? 1 : 0;
        status->has_frame = _has_raw ? 1 : 0;
        status->width = _frame_width;
        status->height = _frame_height;
        status->frame_no = _frame_no;
        status->center_temp_celsius = std::isfinite(_center_temp_kelvin) ? (_center_temp_kelvin - 273.15f) : std::numeric_limits<float>::quiet_NaN();
        status->max_temp_celsius = std::isfinite(_max_temp_kelvin) ? (_max_temp_kelvin - 273.15f) : std::numeric_limits<float>::quiet_NaN();
        copy_status_message(status->last_error, sizeof(status->last_error), _last_error);
        return 0;
    }

    int copy_preview_bgr(thermal_preview_frame_t* frame)
    {
        UvcParsedFrame snapshot;
        uint32_t snapshot_gen = 0;
        int src_w = 0;
        int src_h = 0;
        int out_w = 0;
        int out_h = 0;
        int flip_mode = 0;
        int rotation = 0;
            uint32_t transform_gen = 0;
            uint32_t isp_gen = 0;
            unsigned int src_frame_no = 0;

        if (!frame)
        {
            return -1;
        }
        frame->err = THERMAL_PREVIEW_OK;

        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (!_has_raw || _latest_raw.image_height == 0 || _latest_raw.width == 0)
            {
                frame->err = THERMAL_PREVIEW_ERR_NO_RAW;
                return -1;
            }

            src_w = _latest_raw.width;
            src_h = _latest_raw.image_height;
            flip_mode = _flip_mode;
            rotation = _rotation;
            transform_gen = _transform_gen;
            isp_gen = _isp_gen;
            out_w = (rotation == 90 || rotation == 270) ? src_h : src_w;
            out_h = (rotation == 90 || rotation == 270) ? src_w : src_h;
            src_frame_no = _latest_raw.frame_no;
            frame->width = out_w;
            frame->height = out_h;
            frame->frame_no = src_frame_no;
            if (!frame->data)
            {
                return 0;
            }

            const size_t needed = static_cast<size_t>(out_w) * out_h * 3U;
            if (frame->capacity < needed)
            {
                frame->err = THERMAL_PREVIEW_ERR_BUF;
                return -1;
            }

            if (_preview_gen == _raw_gen
                && _preview_transform_gen == transform_gen
                && _preview_isp_gen == isp_gen
                && !_preview_bgr.empty()
                && _preview_bgr.size() >= needed)
            {
                std::memcpy(frame->data, _preview_bgr.data(), needed);
                return 0;
            }

            snapshot = _latest_raw;
            snapshot_gen = _raw_gen;
        }

        std::vector<uint8_t> rendered;
        float next_max = std::numeric_limits<float>::quiet_NaN();
        bool rendered_ok = false;
        {
            std::lock_guard<std::mutex> render_lock(_render_mutex);
            rendered_ok = render_with_file_sdk(snapshot, nullptr, &next_max, rendered);
        }
        if (!rendered_ok)
        {
            render_gray_preview(snapshot, rendered);
        }

        const size_t raw_needed = static_cast<size_t>(src_w) * src_h * 3U;
        if (rendered.size() < raw_needed)
        {
            frame->err = THERMAL_PREVIEW_ERR_RENDER;
            return -1;
        }

        std::vector<uint8_t> output;
        if (flip_mode == 0 && rotation == 0)
        {
            output = std::move(rendered);
        }
        else
        {
            output.resize(raw_needed);
            image_transform_bgr(rendered.data(), src_w, src_h, flip_mode, rotation,
                                output.data(), &out_w, &out_h);
        }

        {
            std::lock_guard<std::mutex> lock(_mutex);
            _preview_bgr.swap(output);
            _preview_gen = snapshot_gen;
            _preview_transform_gen = transform_gen;
            _preview_isp_gen = isp_gen;
            if (std::isfinite(next_max))
            {
                _max_temp_kelvin = next_max;
            }
            const size_t needed = static_cast<size_t>(out_w) * out_h * 3U;
            std::memcpy(frame->data, _preview_bgr.data(), needed);
        }
        return 0;
    }

    int get_frame_stats(thermal_frame_stats_t* stats)
    {
        if (!stats)
        {
            return -1;
        }

        stats->callbacks = _stat_callbacks.load();
        stats->drop_empty = _stat_drop_empty.load();
        stats->drop_parse = _stat_drop_parse.load();
        stats->drop_shutter = _stat_drop_shutter.load();
        stats->accepted = _stat_accepted.load();
        stats->last_length = _stat_last_length.load();
        return 0;
    }

    int get_native_size(int* width, int* height)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (!_streaming)
        {
            return -1;
        }
        const int w = _frame_width > 0 ? _frame_width : _selected_frame.width;
        const int h = _frame_height > 0 ? _frame_height : _selected_frame.height;
        if (w <= 0 || h <= 0)
        {
            return -1;
        }
        if (width)
        {
            *width = w;
        }
        if (height)
        {
            *height = h;
        }
        return 0;
    }

private:
    struct HotplugEvent
    {
        uvc_device device {};
        bool found = false;
    };

    std::mutex _lifecycle_mutex;
    std::mutex _api_mutex;
    std::mutex _mutex;
    std::mutex _render_mutex;
    std::mutex _hotplug_mutex;
    std::condition_variable _hotplug_cv;
    std::thread _worker;
    bool _worker_running = false;
    bool _hotplug_pending = false;
    bool _hotplug_registered = false;
    HotplugEvent _hotplug_event {};
    uvc_device _current_device {};
    HANDLE _handle = nullptr;
    IrtekFileSdkLoader _file_sdk;
    IrFileHandle _file_handle = nullptr;
    bool _sdk_initialized = false;
    bool _logged_in = false;
    bool _streaming = false;
    bool _has_raw = false;
    bool _file_sdk_loaded = false;
    bool _file_sdk_size_ready = false;
    int _file_sdk_cfg_w = 0;
    int _file_sdk_cfg_h = 0;
    bool _file_sdk_lut_ready = false;
    std::vector<uint16_t> _file_sdk_lut_ad;
    std::vector<float> _file_sdk_lut_temp;
    bool _lens_info_loaded = false;
    bool _thermal_params_loaded = false;
    bool _range_loaded = false;
    bool _image_params_loaded = false;
    uvc_frame_info _selected_frame {};
    UvcParsedFrame _incoming;
    UvcParsedFrame _latest_raw;
    uint32_t _raw_gen = 0;
    uint32_t _preview_gen = 0;
    uvc_lens_info _lens_info {};
    uvc_thermal_params _thermal_params {};
    uvc_image_params _image_params {};
    USHORT _range_id = 0;
    /* Display transform from g_config.image; _transform_gen invalidates the preview cache. */
    int _flip_mode = 0;
    int _rotation = 0;
    uint32_t _transform_gen = 0;
    uint32_t _preview_transform_gen = 0;
    uint32_t _isp_gen = 0;
    uint32_t _preview_isp_gen = 0;
    bool _file_sdk_params_dirty = false;
    bool _device_push_pending = false;
    std::vector<uint8_t> _preview_bgr;
    float _max_temp_kelvin = std::numeric_limits<float>::quiet_NaN();
    float _center_temp_kelvin = std::numeric_limits<float>::quiet_NaN();
    uint16_t _frame_width = 0;
    uint16_t _frame_height = 0;
    uint32_t _frame_no = 0;
    std::string _last_error;
    /* Written from the UVC callback thread, read from the capture thread. */
    std::atomic<unsigned int> _stat_callbacks { 0 };
    std::atomic<unsigned int> _stat_drop_empty { 0 };
    std::atomic<unsigned int> _stat_drop_parse { 0 };
    std::atomic<unsigned int> _stat_drop_shutter { 0 };
    std::atomic<unsigned int> _stat_accepted { 0 };
    std::atomic<unsigned int> _stat_last_length { 0 };

    static uvc_frame_info select_best_frame(const uvc_frame_info* frames, USHORT count)
    {
        uvc_frame_info best = frames[0];
        for (USHORT i = 1; i < count; ++i)
        {
            const int best_score = static_cast<int>(best.width) * static_cast<int>(best.height);
            const int candidate_score = static_cast<int>(frames[i].width) * static_cast<int>(frames[i].height);
            if (candidate_score > best_score)
            {
                best = frames[i];
            }
        }
        return best;
    }

    static float sample_max_temperature_kelvin(const UvcParsedFrame& parsed)
    {
        if (!parsed.has_lut || parsed.raw_pixels.empty())
        {
            return std::numeric_limits<float>::quiet_NaN();
        }

        uint16_t max_ad = 0;
        for (size_t i = 0; i < parsed.raw_pixels.size(); i += 8)
        {
            if (parsed.raw_pixels[i] > max_ad)
            {
                max_ad = parsed.raw_pixels[i];
            }
        }
        return interpolate_temperature_kelvin(max_ad, parsed.lut_ad, parsed.lut_temp);
    }

    static void render_gray_preview(const UvcParsedFrame& parsed, std::vector<uint8_t>& bgr)
    {
        const int width = parsed.width;
        const int height = parsed.image_height;
        if (width <= 0 || height <= 0 || parsed.raw_pixels.size() < static_cast<size_t>(width) * height)
        {
            bgr.clear();
            return;
        }

        bgr.resize(static_cast<size_t>(width) * height * 3);
        for (int i = 0; i < width * height; ++i)
        {
            const uint8_t gray = static_cast<uint8_t>(parsed.raw_pixels[static_cast<size_t>(i)] >> 8);
            bgr[static_cast<size_t>(i) * 3] = gray;
            bgr[static_cast<size_t>(i) * 3 + 1] = gray;
            bgr[static_cast<size_t>(i) * 3 + 2] = gray;
        }
    }

    static void UVC_CALLBACK on_frame_received(const uvc_frame_data* frame)
    {
        instance().handle_frame(frame);
    }

    static void UVC_CALLBACK on_uvc_found(const uvc_device* device, bool found)
    {
        instance().queue_hotplug(device, found);
    }

    /* SDK path is a binary bus/port id (see build_device_id), not a C string. */
    static std::string format_device_path(const uvc_device& device)
    {
        char buf[3 * UVC_LEN_64 + 1];
        size_t pos = 0;
        for (size_t i = 0; i < sizeof(device.path) && pos + 3 < sizeof(buf); ++i)
        {
            if (device.path[i] == 0 && i > 0)
            {
                break;
            }
            pos += static_cast<size_t>(std::snprintf(
                buf + pos, sizeof(buf) - pos, "%s%02x", i ? ":" : "", device.path[i]));
        }
        buf[pos] = '\0';
        return buf;
    }

    static bool same_device(const uvc_device& lhs, const uvc_device& rhs)
    {
        if (lhs.path[0] != 0 || rhs.path[0] != 0)
        {
            return std::memcmp(lhs.path, rhs.path, sizeof(lhs.path)) == 0;
        }
        return lhs.vid == rhs.vid && lhs.pid == rhs.pid;
    }

    void handle_frame(const uvc_frame_data* frame)
    {
        ++_stat_callbacks;
        if (!frame || !frame->data || frame->length == 0)
        {
            ++_stat_drop_empty;
            return;
        }
        _stat_last_length = frame->length;

        if (!parse_uvc_frame_payload(frame->data, frame->length, frame->width, frame->height, _incoming))
        {
            ++_stat_drop_parse;
            return;
        }

        if (_incoming.has_metadata && _incoming.shutter_state == 1)
        {
            ++_stat_drop_shutter;
            return;
        }

        ++_stat_accepted;
        std::lock_guard<std::mutex> lock(_mutex);
        if (!_streaming)
        {
            return;
        }
        std::swap(_incoming, _latest_raw);
        ++_raw_gen;
        _has_raw = true;
        _frame_width = _latest_raw.width;
        _frame_height = _latest_raw.image_height;
        _frame_no = _latest_raw.frame_no;
        _center_temp_kelvin = get_center_temperature_kelvin(_latest_raw);
        _max_temp_kelvin = sample_max_temperature_kelvin(_latest_raw);
    }

    bool ensure_sdk_locked()
    {
        if (_sdk_initialized)
        {
            return true;
        }

        /* Do not hold _mutex across IRtek USB calls: set_stream_callback may
         * deliver the first frame on another thread, and handle_frame needs it. */
        if (uvc_sdk_init() != UVC_EC_OK)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            set_last_error_locked("uvc_sdk_init failed");
            LOGE("thermal_uvc: %s", _last_error.c_str());
            return false;
        }

        uvc_enable_log(false);
        if (uvc_set_hotplug_callback(&ThermalUvcProvider::on_uvc_found) != UVC_EC_OK)
        {
            LOGW("thermal_uvc: uvc_set_hotplug_callback failed, falling back to enumerate");
            _hotplug_registered = false;
        }
        else
        {
            _hotplug_registered = true;
        }

        std::lock_guard<std::mutex> lock(_mutex);
        _sdk_initialized = true;
        return true;
    }

    void ensure_worker_locked()
    {
        if (_worker.joinable())
        {
            return;
        }
        {
            std::lock_guard<std::mutex> lock(_hotplug_mutex);
            _worker_running = true;
        }
        _worker = std::thread(&ThermalUvcProvider::worker_loop, this);
    }

    void unregister_hotplug()
    {
        if (_hotplug_registered)
        {
            uvc_set_hotplug_callback(nullptr);
            _hotplug_registered = false;
        }
    }

    void cleanup_sdk()
    {
        unregister_hotplug();
        if (_sdk_initialized)
        {
            uvc_sdk_cleanup();
            _sdk_initialized = false;
        }
    }

    void queue_hotplug(const uvc_device* device, bool found)
    {
        std::lock_guard<std::mutex> lock(_hotplug_mutex);
        if (!_worker_running)
        {
            return;
        }
        _hotplug_event = {};
        _hotplug_event.found = found;
        if (device)
        {
            _hotplug_event.device = *device;
        }
        _hotplug_pending = true;
        _hotplug_cv.notify_one();
        LOGI("thermal_uvc: hotplug %s vid=0x%04x pid=0x%04x path=%s",
             found ? "arrive" : "remove",
             _hotplug_event.device.vid, _hotplug_event.device.pid,
             format_device_path(_hotplug_event.device).c_str());
    }

    void worker_loop()
    {
        while (true)
        {
            HotplugEvent event;
            {
                std::unique_lock<std::mutex> lock(_hotplug_mutex);
                _hotplug_cv.wait(lock, [this] {
                    return !_worker_running || _hotplug_pending;
                });
                if (!_worker_running && !_hotplug_pending)
                {
                    break;
                }
                event = _hotplug_event;
                _hotplug_pending = false;
            }

            std::lock_guard<std::mutex> life(_lifecycle_mutex);
            if (!_sdk_initialized)
            {
                continue;
            }
            handle_hotplug_event(event);
        }
    }

    void handle_hotplug_event(const HotplugEvent& event)
    {
        bool logged_in = false;
        uvc_device current {};
        {
            std::lock_guard<std::mutex> lock(_mutex);
            logged_in = _logged_in;
            current = _current_device;
        }

        if (event.found)
        {
            if (logged_in && same_device(current, event.device))
            {
                return;
            }
            if (logged_in)
            {
                LOGI("thermal_uvc: switching UVC session");
                close_session();
            }
            if (!open_session(event.device))
            {
                LOGW("thermal_uvc: hotplug open failed");
            }
            return;
        }

        if (!logged_in)
        {
            return;
        }
        if (event.device.path[0] != 0 && !same_device(current, event.device))
        {
            return;
        }
        LOGI("thermal_uvc: closing UVC session after unplug");
        close_session();
    }

    void try_open_from_list()
    {
        bool streaming = false;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            streaming = _streaming;
        }
        if (streaming)
        {
            return;
        }

        uvc_device devices[8] {};
        USHORT device_count = 0;
        if (uvc_get_uvc_list(devices, static_cast<USHORT>(sizeof(devices) / sizeof(devices[0])), &device_count) != UVC_EC_OK
            || device_count == 0)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            set_last_error_locked("waiting for UVC device");
            LOGI("thermal_uvc: no UVC devices found, waiting for hotplug");
            return;
        }

        LOGI("thermal_uvc: found %u device(s), first vid=0x%04x pid=0x%04x path=%s",
             device_count, devices[0].vid, devices[0].pid,
             format_device_path(devices[0]).c_str());
        if (!open_session(devices[0]))
        {
            LOGW("thermal_uvc: enumerate open failed, waiting for hotplug");
        }
    }

    bool wait_for_device_ready(std::unique_lock<std::mutex>& lock)
    {
        for (int attempt = 1; attempt <= kDeviceReadyAttempts; ++attempt)
        {
            load_device_configuration_locked();
            if (_lens_info_loaded || _thermal_params_loaded
                || _range_loaded || _image_params_loaded)
            {
                if (attempt > 1)
                {
                    LOGI("thermal_uvc: device controls ready after %d attempt(s)", attempt);
                }
                return true;
            }
            if (attempt < kDeviceReadyAttempts)
            {
                LOGW("thermal_uvc: device controls not ready (attempt %d/%d), waiting %dms",
                     attempt, kDeviceReadyAttempts, kDeviceReadyDelayMs);
                lock.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(kDeviceReadyDelayMs));
                lock.lock();
                if (!_logged_in || !_handle)
                {
                    return false;
                }
            }
        }
        return false;
    }

    bool open_session(const uvc_device& device)
    {
        close_session();

        uvc_device target = device;
        HANDLE handle = nullptr;
        if (uvc_login(target.vid, target.pid, &handle) != UVC_EC_OK
            && uvc_login_by_path(target.path, sizeof(target.path), &handle) != UVC_EC_OK)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            set_last_error_locked("UVC login failed");
            LOGE("thermal_uvc: %s", _last_error.c_str());
            return false;
        }
        bool device_ready = false;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _handle = handle;
            _logged_in = true;
            _current_device = device;
            device_ready = wait_for_device_ready(lock);
            if (device_ready)
            {
                load_file_sdk_locked();
                /* net_app config is the SSOT: push it onto the fresh session. */
                apply_thermal_settings_locked();
                apply_image_settings_locked();
            }
            else
            {
                set_last_error_locked("UVC device controls not ready after hotplug");
                LOGW("thermal_uvc: %s — deferring stream start", _last_error.c_str());
            }
        }
        if (!device_ready)
        {
            /* Drop the half-ready login; video_process retries start() every 1s. */
            close_session();
            return false;
        }

        uvc_frame_info frames[16] {};
        USHORT frame_count = 0;
        if (uvc_get_frame_list(handle, frames, static_cast<USHORT>(sizeof(frames) / sizeof(frames[0])), &frame_count) != UVC_EC_OK
            || frame_count == 0)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            set_last_error_locked("UVC frame list is empty");
            LOGE("thermal_uvc: %s", _last_error.c_str());
            close_session();
            return false;
        }

        for (USHORT i = 0; i < frame_count; ++i)
        {
            LOGI("thermal_uvc: frame[%u] %ux%u iface=%u alt=%u idx=%u fmt=%u %s",
                 i, frames[i].width, frames[i].height, frames[i].interface_num,
                 frames[i].alt_setting, frames[i].frame_index, frames[i].format_index,
                 reinterpret_cast<const char*>(frames[i].format));
        }

        uvc_frame_info selected = select_best_frame(frames, frame_count);
        LOGI("thermal_uvc: starting stream %ux%u frame_index=%u format_index=%u",
             selected.width, selected.height, selected.frame_index, selected.format_index);
        INT32 stream_rc = uvc_set_stream_callback(handle, &selected, &ThermalUvcProvider::on_frame_received);
        if (stream_rc != UVC_EC_OK)
        {
            for (USHORT i = 0; i < frame_count && stream_rc != UVC_EC_OK; ++i)
            {
                if (frames[i].frame_index == selected.frame_index
                    && frames[i].format_index == selected.format_index
                    && frames[i].interface_num == selected.interface_num)
                {
                    continue;
                }
                LOGW("thermal_uvc: retry stream on frame[%u] %ux%u",
                     i, frames[i].width, frames[i].height);
                selected = frames[i];
                stream_rc = uvc_set_stream_callback(handle, &selected, &ThermalUvcProvider::on_frame_received);
            }
        }
        if (stream_rc != UVC_EC_OK)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            set_last_error_locked("failed to start UVC stream");
            LOGE("thermal_uvc: %s (rc=%d)", _last_error.c_str(), static_cast<int>(stream_rc));
            close_session();
            return false;
        }

        {
            std::lock_guard<std::mutex> lock(_mutex);
            _handle = handle;
            _selected_frame = selected;
            _streaming = true;
            clear_last_error_locked();
        }
        LOGI("thermal_uvc: streaming started %ux%u frame_index=%u",
             selected.width, selected.height, selected.frame_index);
        return true;
    }

    void close_session()
    {
        HANDLE handle = nullptr;
        bool streaming = false;
        bool logged_in = false;
        uvc_frame_info selected {};
        {
            std::lock_guard<std::mutex> lock(_mutex);
            handle = _handle;
            streaming = _streaming;
            logged_in = _logged_in;
            selected = _selected_frame;
            _streaming = false;
        }

        if (streaming && handle)
        {
            uvc_set_stream_callback(handle, &selected, nullptr);
        }
        if (logged_in)
        {
            uvc_logout(&handle);
        }

        std::lock_guard<std::mutex> lock(_mutex);
        reset_session_state_locked();
    }

    void reset_session_state_locked()
    {
        close_file_sdk_session_locked();
        _handle = nullptr;
        _logged_in = false;
        _streaming = false;
        _has_raw = false;
        _lens_info_loaded = false;
        _thermal_params_loaded = false;
        _range_loaded = false;
        _image_params_loaded = false;
        _frame_width = 0;
        _frame_height = 0;
        _frame_no = 0;
        _range_id = 0;
        _current_device = {};
        _selected_frame = {};
        std::memset(&_lens_info, 0, sizeof(_lens_info));
        std::memset(&_thermal_params, 0, sizeof(_thermal_params));
        std::memset(&_image_params, 0, sizeof(_image_params));
        _preview_bgr.clear();
        _incoming = {};
        _latest_raw = {};
        _raw_gen = 0;
        _preview_gen = 0;
        _max_temp_kelvin = std::numeric_limits<float>::quiet_NaN();
        _center_temp_kelvin = std::numeric_limits<float>::quiet_NaN();
    }

    void clear_last_error_locked()
    {
        _last_error.clear();
    }

    void set_last_error_locked(const std::string& message)
    {
        _last_error = message;
    }

    void load_device_configuration_locked()
    {
        std::memset(&_lens_info, 0, sizeof(_lens_info));
        _lens_info_loaded = uvc_get_lens_info(_handle, &_lens_info) == UVC_EC_OK;
        if (_lens_info_loaded)
        {
            /* This UVC module reports lens ranges in Celsius; the Volga wire
             * contract and the FileSDK LUT both run in Kelvin. */
            for (USHORT i = 0; i < _lens_info.ranges_count && i < THERMAL_LENS_MAX_RANGES; ++i)
            {
                _lens_info.range_map[i].range.min += 273.15f;
                _lens_info.range_map[i].range.max += 273.15f;
            }
        }
        if (!_lens_info_loaded)
        {
            LOGW("thermal_uvc: uvc_get_lens_info failed, FileSDK temperature range fallback will be limited");
        }

        std::memset(&_thermal_params, 0, sizeof(_thermal_params));
        _thermal_params_loaded = uvc_get_thermal_params(_handle, &_thermal_params) == UVC_EC_OK;
        if (!_thermal_params_loaded)
        {
            LOGW("thermal_uvc: uvc_get_thermal_params failed, FileSDK local measurement will be skipped");
        }

        _range_loaded = uvc_get_thermal_range(_handle, &_range_id) == UVC_EC_OK;
        if (!_range_loaded)
        {
            LOGW("thermal_uvc: uvc_get_thermal_range failed, FileSDK range mapping will be skipped");
        }

        std::memset(&_image_params, 0, sizeof(_image_params));
        _image_params_loaded = uvc_get_image_params(_handle, &_image_params) == UVC_EC_OK;
        if (!_image_params_loaded)
        {
            LOGW("thermal_uvc: uvc_get_image_params failed, FileSDK will use HOLLY defaults then force DDE off");
        }
    }

    void load_file_sdk_locked()
    {
        if (_file_sdk_loaded)
        {
            return;
        }

        std::string error_message;
        if (!_file_sdk.load(&error_message))
        {
            set_last_error_locked(std::string("FileSDK unavailable: ") + error_message);
            LOGW("thermal_uvc: %s", _last_error.c_str());
            return;
        }

        _file_sdk_loaded = true;
        LOGI("thermal_uvc: FileSDK runtime loaded from %s", _file_sdk.loaded_path().c_str());
    }

    void close_file_sdk_session_locked()
    {
        if (_file_handle)
        {
            _file_sdk.ir_file_close(_file_handle);
            _file_handle = nullptr;
        }
        _file_sdk_size_ready = false;
        _file_sdk_cfg_w = 0;
        _file_sdk_cfg_h = 0;
        _file_sdk_lut_ready = false;
        _file_sdk_lut_ad.clear();
        _file_sdk_lut_temp.clear();
        std::remove(kFileSdkScratchPath);
    }

    bool ensure_file_sdk_session_locked()
    {
        if (!_file_sdk_loaded)
        {
            return false;
        }

        if (_file_handle)
        {
            return true;
        }

        if (_file_sdk.ir_file_create(kFileSdkScratchPath, IR_FILE_THERMAL_PICTURE, &_file_handle) != IR_FILE_EC_OK || !_file_handle)
        {
            LOGW("thermal_uvc: FileSDK ir_file_create failed");
            _file_handle = nullptr;
            return false;
        }

        if (_file_sdk.ir_file_set_isp_type(_file_handle, IR_FILE_HOLLY) != IR_FILE_EC_OK)
        {
            LOGW("thermal_uvc: FileSDK ir_file_set_isp_type failed");
            close_file_sdk_session_locked();
            return false;
        }

        apply_preview_isp_params_locked();
        return true;
    }

    void apply_vt_image_params_locked()
    {
        IrFileImageParams img {};
        img.contrast = _image_params_loaded ? _image_params.contrast : 50;
        img.brightness = _image_params_loaded ? _image_params.brightness : 50;
        img.dde_enabled = _image_params_loaded ? _image_params.dde_enable : 0;
        img.dde_value = _image_params_loaded ? _image_params.dde_value : 50;
        img.deconv_valid = IR_FILE_DECONV_VALID;
        img.deconv_psf_sigma = 0.0f;

        if (_file_sdk.ir_file_set_vt_params_v2)
        {
            IrFileVtParams vt {};
            vt.image_params = &img;
            if (_file_sdk.ir_file_set_vt_params_v2(_file_handle, vt) != IR_FILE_EC_OK)
            {
                LOGW("thermal_uvc: FileSDK ir_file_set_vt_params_v2 failed");
            }
        }
        else
        {
            LOGW("thermal_uvc: ir_file_set_vt_params_v2 not exported, cannot apply DDE");
        }
    }

    void apply_display_isp_locked()
    {
        const thermal_settings_t& cfg = g_config.thermal;
        const IrFileInt dist = cfg.color_dist > 0 ? cfg.color_dist
                                                   : IR_FILE_COLOR_DIST_AD_LINEAR;

        if (_file_sdk.ir_file_set_color_dist
            && _file_sdk.ir_file_set_color_dist(_file_handle, dist) != IR_FILE_EC_OK)
        {
            LOGW("thermal_uvc: FileSDK ir_file_set_color_dist failed");
        }

        if (_file_sdk.ir_file_set_palette)
        {
            IrFilePalette pal {};
            pal.refNo = static_cast<IrFileByte>(cfg.palette_ref_no);
            pal.inverted = cfg.palette_inverted ? 1 : 0;
            if (cfg.palette_ref_no == NET_CUSTOM_PALETTE && cfg.palette_color_count > 0)
            {
                const int count = std::min(cfg.palette_color_count, NET_MAX_PALETTE_COLORS);
                for (int i = 0; i < count; ++i)
                {
                    pal.colors[i] = static_cast<IrFileUInt>(cfg.palette_colors[i]);
                }
            }
            if (_file_sdk.ir_file_set_palette(_file_handle, &pal) != IR_FILE_EC_OK)
            {
                LOGW("thermal_uvc: FileSDK ir_file_set_palette failed");
            }
        }

        if (_file_sdk.ir_file_set_gain)
        {
            IrFileGain gain {};
            gain.mode = cfg.gain_mode > 0 ? cfg.gain_mode : IR_FILE_GAIN_AUTO;
            gain.max = cfg.gain_max;
            gain.min = cfg.gain_min;
            gain.fixedSpan = cfg.gain_fixed_span ? 1 : 0;
            gain.span = cfg.gain_span;
            if (_file_sdk.ir_file_set_gain(_file_handle, &gain) != IR_FILE_EC_OK)
            {
                LOGW("thermal_uvc: FileSDK ir_file_set_gain failed");
            }
        }
    }

    void apply_preview_isp_params_locked()
    {
        apply_vt_image_params_locked();

        if (_file_sdk.ir_file_set_image_enh
            && _file_sdk.ir_file_set_image_enh(_file_handle, IR_FILE_IMAGE_ENH_NONE) != IR_FILE_EC_OK)
        {
            LOGW("thermal_uvc: FileSDK ir_file_set_image_enh(NONE) failed");
        }

        apply_display_isp_locked();
    }

    bool apply_file_sdk_configuration_locked(const UvcParsedFrame& parsed)
    {
        if (!ensure_file_sdk_session_locked())
        {
            return false;
        }
        if (!_thermal_params_loaded || !parsed.has_lut || parsed.raw_pixels.empty())
        {
            return false;
        }

        const bool size_changed = !_file_sdk_size_ready
            || _file_sdk_cfg_w != parsed.width
            || _file_sdk_cfg_h != parsed.image_height;
        const bool params_dirty = _file_sdk_params_dirty;

        if (size_changed || params_dirty) {
            IrFileSize size {
                static_cast<IrFileShort>(parsed.width),
                static_cast<IrFileShort>(parsed.image_height)
            };
            if (_file_sdk.ir_file_set_thermal_size(_file_handle, &size) != IR_FILE_EC_OK)
            {
                LOGW("thermal_uvc: FileSDK ir_file_set_thermal_size failed");
                return false;
            }

            if (_lens_info_loaded && _range_loaded)
            {
                for (USHORT index = 0; index < _lens_info.ranges_count && index < 4; ++index)
                {
                    if (_lens_info.range_map[index].id == _range_id)
                    {
                        IrFileTempRange temp_range {
                            _lens_info.range_map[index].range.max,
                            _lens_info.range_map[index].range.min
                        };
                        _file_sdk.ir_file_set_temp_range(_file_handle, &temp_range);
                        break;
                    }
                }
            }

            IrFileThermalParams file_params {
                _thermal_params.refl_temp,
                _thermal_params.atm_temp,
                _thermal_params.distance,
                _thermal_params.rel_humidity,
                _thermal_params.emissivity,
                293.15f,
                1.0f
            };
            if (_file_sdk.ir_file_set_thermal_params(_file_handle, &file_params) != IR_FILE_EC_OK)
            {
                LOGW("thermal_uvc: FileSDK ir_file_set_thermal_params failed");
                return false;
            }
            apply_preview_isp_params_locked();
            _file_sdk_cfg_w = parsed.width;
            _file_sdk_cfg_h = parsed.image_height;
            _file_sdk_size_ready = true;
            _file_sdk_params_dirty = false;
        }

        const bool lut_changed = !_file_sdk_lut_ready
            || _file_sdk_lut_ad.size() != parsed.lut_ad.size()
            || _file_sdk_lut_temp.size() != parsed.lut_temp.size()
            || std::memcmp(_file_sdk_lut_ad.data(), parsed.lut_ad.data(),
                           parsed.lut_ad.size() * sizeof(uint16_t)) != 0
            || std::memcmp(_file_sdk_lut_temp.data(), parsed.lut_temp.data(),
                           parsed.lut_temp.size() * sizeof(float)) != 0;

        if (lut_changed)
        {
            IrFileLut lut {};
            lut.radMethod = 0x01;
            lut.count = static_cast<IrFileUInt>(parsed.lut_ad.size());
            lut.adArray = const_cast<IrFileUShort*>(parsed.lut_ad.data());
            lut.tempArray = const_cast<float*>(parsed.lut_temp.data());
            if (_file_sdk.ir_file_set_lut(_file_handle, &lut) != IR_FILE_EC_OK)
            {
                LOGW("thermal_uvc: FileSDK ir_file_set_lut failed");
                return false;
            }
            _file_sdk_lut_ad = parsed.lut_ad;
            _file_sdk_lut_temp = parsed.lut_temp;
            _file_sdk_lut_ready = true;
        }

        return true;
    }

    /* g_config.thermal -> runtime state + device. Hardware consumption is best-effort. */
    void apply_thermal_settings_locked()
    {
        const thermal_settings_t& cfg = g_config.thermal;

        _thermal_params.emissivity = cfg.emissivity;
        _thermal_params.distance = cfg.distance;
        _thermal_params.refl_temp = cfg.refl_temp;
        _thermal_params.atm_temp = cfg.atm_temp;
        _thermal_params.rel_humidity = cfg.rel_humidity;
        _thermal_params_loaded = true;
        _range_id = static_cast<USHORT>(cfg.range_id > 0 ? cfg.range_id : 1);
        _range_loaded = true;
        _file_sdk_params_dirty = true;
        _device_push_pending = true;
        ++_isp_gen;

        /* Normalize against the lens id space (1-based): a stale/unknown id
         * falls back to the first range so device and FileSDK stay usable. */
        if (_lens_info_loaded)
        {
            bool found = false;
            for (USHORT i = 0; i < _lens_info.ranges_count && i < THERMAL_LENS_MAX_RANGES; ++i)
            {
                if (_lens_info.range_map[i].id == _range_id)
                {
                    found = true;
                    break;
                }
            }
            if (!found && _lens_info.ranges_count > 0)
            {
                LOGW("thermal_uvc: range_id %u not in lens range_map, fallback to %u",
                     _range_id, _lens_info.range_map[0].id);
                _range_id = _lens_info.range_map[0].id;
            }
        }

    }

    /*
     * Holly video thread: USB calls run on the capture path, never on the
     * HTTP thread, and never while _mutex is held.
     */
    void push_device_calls_outside_lock()
    {
        HANDLE handle = nullptr;
        uvc_thermal_params thermal {};
        uvc_image_params image {};
        bool image_loaded = false;
        USHORT range_id = 1;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (!_device_push_pending || !_logged_in || !_handle)
            {
                return;
            }
            _device_push_pending = false;
            handle = _handle;
            thermal = _thermal_params;
            image = _image_params;
            image_loaded = _image_params_loaded;
            range_id = _range_id;
        }

        const thermal_settings_t& cfg = g_config.thermal;
        const uvc_enum_shutter_mode mode = cfg.shutter_mode == NET_SHUTTER_MANUAL
            ? uvc_enum_shutter_mode::MANUAL
            : uvc_enum_shutter_mode::AUTOMATIC;
        const USHORT minutes = static_cast<USHORT>(
            cfg.shutter_interval_min > 0 ? cfg.shutter_interval_min : 1);
        if (uvc_set_thermal_shutter_mode(handle, &mode) != UVC_EC_OK)
        {
            LOGW("thermal_uvc: uvc_set_thermal_shutter_mode failed");
        }
        if (uvc_set_thermal_shutter_time(handle, &minutes) != UVC_EC_OK)
        {
            LOGW("thermal_uvc: uvc_set_thermal_shutter_time failed");
        }
        if (uvc_set_thermal_range(handle, &range_id) != UVC_EC_OK)
        {
            LOGW("thermal_uvc: uvc_set_thermal_range failed");
        }
        if (uvc_set_thermal_params(handle, &thermal) != UVC_EC_OK)
        {
            LOGW("thermal_uvc: uvc_set_thermal_params failed (kept in software path)");
        }
        if (image_loaded && uvc_set_image_params(handle, &image) != UVC_EC_OK)
        {
            LOGW("thermal_uvc: uvc_set_image_params failed");
        }
    }

    /* g_config.image -> runtime state + device; flip/rotation stay in software. */
    void apply_image_settings_locked()
    {
        const image_settings_t& cfg = g_config.image;

        _image_params.contrast = static_cast<USHORT>(cfg.contrast);
        _image_params.brightness = static_cast<USHORT>(cfg.brightness);
        _image_params.dde_enable = cfg.dde_enable ? 1 : 0;
        _image_params.dde_value = static_cast<USHORT>(cfg.dde_value);
        _image_params_loaded = true;
        _file_sdk_params_dirty = true;
        _device_push_pending = true;

        if (_flip_mode != cfg.flip_mode || _rotation != cfg.rotation)
        {
            _flip_mode = cfg.flip_mode;
            _rotation = cfg.rotation;
            ++_transform_gen;
        }

    }

public:
    int apply_thermal_settings()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        apply_thermal_settings_locked();
        return 0;
    }

    int apply_image_settings()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        apply_image_settings_locked();
        return 0;
    }

    int calibrate()
    {
        std::lock_guard<std::mutex> life(_lifecycle_mutex);
        HANDLE handle = nullptr;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (!_logged_in || !_handle)
            {
                return -1;
            }
            handle = _handle;
        }
        return uvc_thermal_calibrate(handle) == UVC_EC_OK ? 0 : -1;
    }

    int get_lens_info(thermal_lens_info_t* info)
    {
        if (!info)
        {
            return -1;
        }

        std::lock_guard<std::mutex> lock(_mutex);
        if (!_logged_in || !_lens_info_loaded)
        {
            return -1;
        }

        std::memset(info, 0, sizeof(*info));
        std::memcpy(info->name, _lens_info.name, sizeof(info->name) - 1);
        std::memcpy(info->band, _lens_info.band, sizeof(info->band) - 1);
        info->focal = _lens_info.focal;
        info->vfov = _lens_info.vfov;
        info->hfov = _lens_info.hfov;
        const int count = _lens_info.ranges_count < THERMAL_LENS_MAX_RANGES
            ? _lens_info.ranges_count
            : THERMAL_LENS_MAX_RANGES;
        for (int i = 0; i < count; ++i)
        {
            info->range_map[i].id = _lens_info.range_map[i].id;
            info->range_map[i].min = _lens_info.range_map[i].range.min;
            info->range_map[i].max = _lens_info.range_map[i].range.max;
        }
        info->ranges_count = count;
        return 0;
    }

    bool render_with_file_sdk(
        const UvcParsedFrame& parsed,
        float* center_kelvin,
        float* max_kelvin,
        std::vector<uint8_t>& bgr)
    {
        push_device_calls_outside_lock();
        if (!apply_file_sdk_configuration_locked(parsed))
        {
            return false;
        }

        bgr.resize(static_cast<size_t>(parsed.width) * parsed.image_height * 3);
        IrFileFrame input_frame {};
        input_frame.timeStamp = parsed.pts / 1000ULL;
        input_frame.length = static_cast<IrFileUInt>(parsed.raw_pixels.size());
        input_frame.buffer = const_cast<IrFileUShort*>(parsed.raw_pixels.data());

        IrFileImage image {};
        image.offset = 0;
        image.data = bgr.data();
        if (_file_sdk.ir_file_get_thermal_image_from_frame(_file_handle, &input_frame, IR_FILE_BGR, &image) != IR_FILE_EC_OK)
        {
            LOGW("thermal_uvc: FileSDK ir_file_get_thermal_image_from_frame failed");
            return false;
        }

        if (center_kelvin)
        {
            IrFileTempPoint center {};
            center.point.x = static_cast<IrFileShort>(parsed.width / 2);
            center.point.y = static_cast<IrFileShort>(parsed.image_height / 2);
            center.localParams = 0;
            center.reflTemp = _thermal_params.refl_temp;
            center.distance = _thermal_params.distance;
            center.emissivity = _thermal_params.emissivity;
            if (_file_sdk.ir_file_get_temp_spot(_file_handle, &center) == IR_FILE_EC_OK)
            {
                *center_kelvin = center.temp;
            }
        }

        if (max_kelvin)
        {
            IrFilePoint points[2] = {
                { 0, 0 },
                {
                    static_cast<IrFileShort>(parsed.width - 1),
                    static_cast<IrFileShort>(parsed.image_height - 1)
                }
            };
            IrFileMarker global_marker {};
            global_marker.type = IR_FILE_RECTANGLE;
            std::memcpy(global_marker.name, "global", sizeof("global"));
            global_marker.pointCount = 2;
            global_marker.points = points;
            global_marker.localParams = 0;
            global_marker.reflTemp = _thermal_params.refl_temp;
            global_marker.distance = _thermal_params.distance;
            global_marker.emissivity = _thermal_params.emissivity;
            if (_file_sdk.ir_file_set_marker(_file_handle, &global_marker) == IR_FILE_EC_OK
                && std::isfinite(global_marker.tempValues[0]))
            {
                *max_kelvin = global_marker.tempValues[0];
            }
        }

        return true;
    }

    int get_preset_palettes(thermal_preset_palette_t* out, int max_count, int* out_count)
    {
        if (!out || max_count <= 0 || !out_count)
        {
            return -1;
        }

        std::lock_guard<std::mutex> life(_lifecycle_mutex);
        std::lock_guard<std::mutex> render(_render_mutex);
        if (!_file_sdk_loaded)
        {
            std::string error;
            _file_sdk_loaded = _file_sdk.load(&error);
            if (!_file_sdk_loaded)
            {
                return -1;
            }
        }
        if (!_file_sdk.ir_file_get_preset_palettes)
        {
            return -1;
        }

        std::vector<IrFilePresetPalette> buf(static_cast<size_t>(max_count));
        IrFilePresetPalettes list {};
        list.count = static_cast<IrFileUInt>(max_count);
        list.palettes = buf.data();
        if (_file_sdk.ir_file_get_preset_palettes(&list) != IR_FILE_EC_OK || list.count == 0)
        {
            return -1;
        }

        int count = static_cast<int>(list.count);
        if (count > max_count)
        {
            count = max_count;
        }
        for (int i = 0; i < count; ++i)
        {
            out[i].ref_no = buf[static_cast<size_t>(i)].refNo;
            std::memset(out[i].name, 0, sizeof(out[i].name));
            std::memcpy(out[i].name, buf[static_cast<size_t>(i)].name,
                        sizeof(out[i].name) - 1);
        }
        *out_count = count;
        return 0;
    }

    int get_preset_palette_colors(int ref_no, int* colors, int max_colors, int* color_count)
    {
        if (!colors || !color_count || max_colors < NET_MAX_PALETTE_COLORS)
        {
            return -1;
        }
        if (ref_no < 0 || ref_no > 255)
        {
            return -1;
        }

        std::lock_guard<std::mutex> life(_lifecycle_mutex);
        std::lock_guard<std::mutex> render(_render_mutex);
        if (!_file_sdk_loaded)
        {
            std::string error;
            _file_sdk_loaded = _file_sdk.load(&error);
            if (!_file_sdk_loaded)
            {
                return -1;
            }
        }
        if (!_file_sdk.ir_file_get_preset_palette_image)
        {
            return -1;
        }

        std::vector<IrFileByte> bgr(static_cast<size_t>(NET_MAX_PALETTE_COLORS) * 3);
        IrFileImage image {};
        image.offset = 0;
        image.data = bgr.data();
        if (_file_sdk.ir_file_get_preset_palette_image(static_cast<IrFileByte>(ref_no), &image)
            != IR_FILE_EC_OK)
        {
            return -1;
        }

        bool any = false;
        for (int i = 0; i < NET_MAX_PALETTE_COLORS; ++i)
        {
            const IrFileByte b = bgr[static_cast<size_t>(i) * 3];
            const IrFileByte g = bgr[static_cast<size_t>(i) * 3 + 1];
            const IrFileByte r = bgr[static_cast<size_t>(i) * 3 + 2];
            colors[i] = (static_cast<int>(r) << 16) | (static_cast<int>(g) << 8) | static_cast<int>(b);
            if (colors[i] != 0)
            {
                any = true;
            }
        }
        if (!any)
        {
            return -1;
        }
        *color_count = NET_MAX_PALETTE_COLORS;
        return 0;
    }

    int get_live_gain(float* min_k, float* max_k)
    {
        if (!min_k || !max_k)
        {
            return -1;
        }

        std::lock_guard<std::mutex> life(_lifecycle_mutex);
        std::lock_guard<std::mutex> render(_render_mutex);
        if (!_file_handle || !_file_sdk.ir_file_get_gain)
        {
            return -1;
        }

        IrFileGain gain {};
        if (_file_sdk.ir_file_get_gain(_file_handle, &gain) != IR_FILE_EC_OK
            || !std::isfinite(gain.min) || !std::isfinite(gain.max)
            || !(gain.min < gain.max))
        {
            return -1;
        }
        *min_k = gain.min;
        *max_k = gain.max;
        return 0;
    }

    int get_live_palette(int* colors, int max_colors, int* color_count)
    {
        if (!colors || !color_count || max_colors < NET_MAX_PALETTE_COLORS)
        {
            return -1;
        }

        std::lock_guard<std::mutex> life(_lifecycle_mutex);
        std::lock_guard<std::mutex> render(_render_mutex);
        if (!_file_handle || !_file_sdk.ir_file_get_palette)
        {
            return -1;
        }

        IrFilePalette pal {};
        if (_file_sdk.ir_file_get_palette(_file_handle, &pal) != IR_FILE_EC_OK)
        {
            return -1;
        }

        bool any = false;
        for (int i = 0; i < NET_MAX_PALETTE_COLORS; ++i)
        {
            colors[i] = static_cast<int>(pal.colors[i]);
            if (colors[i] != 0)
            {
                any = true;
            }
        }
        if (!any)
        {
            return -1;
        }
        *color_count = NET_MAX_PALETTE_COLORS;
        return 0;
    }
};
}

extern "C" int thermal_uvc_provider_start(void)
{
    return ThermalUvcProvider::instance().start();
}

extern "C" void thermal_uvc_provider_stop(void)
{
    ThermalUvcProvider::instance().stop();
}

extern "C" int thermal_uvc_provider_get_marker_max_temp(const char* marker_name, float* temp_celsius)
{
    return ThermalUvcProvider::instance().get_marker_max_temp(marker_name, temp_celsius);
}

extern "C" int thermal_uvc_provider_copy_latest_raw(uint16_t* pixels, size_t capacity,
                                                   size_t* count, int* width,
                                                   int* height, unsigned int* frame_no)
{
    return ThermalUvcProvider::instance().copy_latest_raw(pixels, capacity, count,
                                                          width, height, frame_no);
}

extern "C" int thermal_uvc_provider_get_status(thermal_provider_status_t* status)
{
    return ThermalUvcProvider::instance().get_status(status);
}

extern "C" int thermal_uvc_provider_copy_preview_bgr(thermal_preview_frame_t* frame)
{
    return ThermalUvcProvider::instance().copy_preview_bgr(frame);
}

extern "C" int thermal_uvc_provider_get_native_size(int* width, int* height)
{
    return ThermalUvcProvider::instance().get_native_size(width, height);
}

extern "C" int thermal_uvc_provider_get_frame_stats(thermal_frame_stats_t* stats)
{
    return ThermalUvcProvider::instance().get_frame_stats(stats);
}

extern "C" int thermal_uvc_provider_apply_thermal_settings(void)
{
    return ThermalUvcProvider::instance().apply_thermal_settings();
}

extern "C" int thermal_uvc_provider_apply_image_settings(void)
{
    return ThermalUvcProvider::instance().apply_image_settings();
}

extern "C" int thermal_uvc_provider_calibrate(void)
{
    return ThermalUvcProvider::instance().calibrate();
}

extern "C" int thermal_uvc_provider_get_lens_info(thermal_lens_info_t* info)
{
    return ThermalUvcProvider::instance().get_lens_info(info);
}

extern "C" int thermal_uvc_provider_get_preset_palettes(thermal_preset_palette_t* out,
                                                       int max_count, int* out_count)
{
    return ThermalUvcProvider::instance().get_preset_palettes(out, max_count, out_count);
}

extern "C" int thermal_uvc_provider_get_preset_palette_colors(int ref_no, int* colors,
                                                             int max_colors, int* color_count)
{
    return ThermalUvcProvider::instance().get_preset_palette_colors(ref_no, colors, max_colors,
                                                                    color_count);
}

extern "C" int thermal_uvc_provider_get_live_gain(float* min_k, float* max_k)
{
    return ThermalUvcProvider::instance().get_live_gain(min_k, max_k);
}

extern "C" int thermal_uvc_provider_get_live_palette(int* colors, int max_colors,
                                                    int* color_count)
{
    return ThermalUvcProvider::instance().get_live_palette(colors, max_colors, color_count);
}
