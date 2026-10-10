#include "irtek_file_sdk_loader.h"

#include <cstdlib>
#include <dlfcn.h>

namespace
{
template <typename T>
bool bind_symbol(void* handle, const char* name, T& target, std::string* error_message)
{
    dlerror();
    target = reinterpret_cast<T>(dlsym(handle, name));
    const char* error = dlerror();
    if (error != nullptr || !target)
    {
        if (error_message)
        {
            *error_message = std::string("Missing symbol: ") + name;
        }
        return false;
    }
    return true;
}
} // namespace

IrtekFileSdkLoader::~IrtekFileSdkLoader()
{
    if (_handle)
    {
        dlclose(_handle);
        _handle = nullptr;
    }
}

bool IrtekFileSdkLoader::load(std::string* error_message)
{
    if (_handle)
    {
        return true;
    }

    std::string last_error;
    for (const std::string& candidate : candidate_paths())
    {
        void* handle = dlopen(candidate.c_str(), RTLD_LAZY | RTLD_LOCAL);
        if (!handle)
        {
            const char* dl_error = dlerror();
            if (dl_error)
            {
                last_error = dl_error;
            }
            continue;
        }

        _handle = handle;
        _loaded_path = candidate;
        if (bind_symbols(error_message))
        {
            return true;
        }

        dlclose(_handle);
        _handle = nullptr;
        _loaded_path.clear();
    }

    if (error_message)
    {
        *error_message = last_error.empty() ? "Unable to locate libIRtekFileSDK.so" : last_error;
    }
    return false;
}

bool IrtekFileSdkLoader::bind_symbols(std::string* error_message)
{
    if (!(bind_symbol(_handle, "ir_file_create", ir_file_create, error_message)
          && bind_symbol(_handle, "ir_file_close", ir_file_close, error_message)
          && bind_symbol(_handle, "ir_file_set_isp_type", ir_file_set_isp_type, error_message)
          && bind_symbol(_handle, "ir_file_set_thermal_size", ir_file_set_thermal_size, error_message)
          && bind_symbol(_handle, "ir_file_set_temp_range", ir_file_set_temp_range, error_message)
          && bind_symbol(_handle, "ir_file_set_thermal_params", ir_file_set_thermal_params, error_message)
          && bind_symbol(_handle, "ir_file_set_lut", ir_file_set_lut, error_message)
          && bind_symbol(_handle, "ir_file_set_marker", ir_file_set_marker, error_message)
          && bind_symbol(_handle, "ir_file_get_thermal_image_from_frame", ir_file_get_thermal_image_from_frame, error_message)
          && bind_symbol(_handle, "ir_file_get_temp_spot", ir_file_get_temp_spot, error_message)))
    {
        return false;
    }

    bind_symbol(_handle, "ir_file_set_vt_params_v2", ir_file_set_vt_params_v2, nullptr);
    bind_symbol(_handle, "ir_file_get_vt_params_v2", ir_file_get_vt_params_v2, nullptr);
    bind_symbol(_handle, "ir_file_set_image_enh", ir_file_set_image_enh, nullptr);
    bind_symbol(_handle, "ir_file_get_image_enh", ir_file_get_image_enh, nullptr);
    bind_symbol(_handle, "ir_file_set_color_dist", ir_file_set_color_dist, nullptr);
    bind_symbol(_handle, "ir_file_get_color_dist", ir_file_get_color_dist, nullptr);
    bind_symbol(_handle, "ir_file_set_palette", ir_file_set_palette, nullptr);
    bind_symbol(_handle, "ir_file_get_palette", ir_file_get_palette, nullptr);
    bind_symbol(_handle, "ir_file_set_gain", ir_file_set_gain, nullptr);
    bind_symbol(_handle, "ir_file_get_gain", ir_file_get_gain, nullptr);
    bind_symbol(_handle, "ir_file_get_preset_palettes", ir_file_get_preset_palettes, nullptr);
    bind_symbol(_handle, "ir_file_get_preset_palette_image", ir_file_get_preset_palette_image, nullptr);
    return true;
}

std::vector<std::string> IrtekFileSdkLoader::candidate_paths()
{
    std::vector<std::string> paths;
    if (const char* env_path = std::getenv("IRTEKFILESDK_LIBRARY"))
    {
        if (*env_path)
        {
            paths.emplace_back(env_path);
        }
    }

    // App-private install paths (not system /usr/lib).
    paths.emplace_back("/usr/lib/net_app/libIRtekFileSDK.so");
    paths.emplace_back("/userdata/parkia/app/lib/libIRtekFileSDK.so");
    paths.emplace_back("libIRtekFileSDK.so");
    paths.emplace_back("./libs/irtek/lib/libIRtekFileSDK.so");
    paths.emplace_back("../libs/irtek/lib/libIRtekFileSDK.so");
    return paths;
}
