#pragma once

#include <stddef.h>
#include <stdint.h>
#include <cstring>

#if defined(_WIN32) || defined(_WIN64)
#define WINDOWS
#elif defined(__ANDROID__)
#define ANDROID
#elif defined(__linux__)
#define LINUX
#else
#error "Unsupported platform: only Windows, Android and Linux are supported"
#endif

#if defined(WINDOWS)
#if defined(UVC_SDK_EXPORT) 
#define UVC_SDK_API __declspec(dllexport)
#else
#define UVC_SDK_API __declspec(dllimport)
#endif
#define UVC_CALLBACK __stdcall
#elif defined(ANDROID)
#define UVC_SDK_API __attribute__((visibility("default")))
#define UVC_CALLBACK
#elif defined(LINUX)
#define UVC_SDK_API __attribute__((visibility("default")))
#define UVC_CALLBACK
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define UVC_LEN_4   4
#define UVC_LEN_16  16
#define UVC_LEN_32  32
#define UVC_LEN_64  64
#define UVC_LEN_80  80
#define UVC_LEN_128 128

typedef void*               HANDLE;
typedef unsigned char       BYTE;
typedef short               SHORT;
typedef unsigned short      USHORT;
typedef int                 INT32;
typedef unsigned int        UINT32;
typedef unsigned long long  UINT64;

enum uvc_enum_error_code : INT32
{
    UVC_EC_OK = 0,                     // 函数调用成功
    UVC_EC_FAIL = -1,                  // 函数调用失败
    UVC_EC_MEMORY = -2,                // 内存分配失败
    UVC_EC_NULL_POINTER = -3,          // 空指针
    UVC_EC_INVALID_PARAM = -4,         // 索引不存在
    UVC_EC_NOT_SUPPORTED = -5,         // 功能不支持
    UVC_EC_NOT_INITIALIZED = -6        // SDK未初始化，请先调用uvc_sdk_init()
};

#pragma pack(push, 1)

    /// <summary>
    /// 挡片校正模式
    /// </summary>
    enum class uvc_enum_shutter_mode : BYTE
    {
        AUTOMATIC = 0,                      // 自动模式
        MANUAL = 1                          // 手动模式
    };

    /// <summary>
    /// 翻转模式
    /// </summary>
    enum class uvc_enum_image_flip_mode : BYTE
    {
        NONE = 0, ///< 不翻转
        UP_DOWN = 1, ///< 上下翻转
        LEFT_RIGHT = 2, ///< 左右翻转
        CENTER = 3  ///< 中心翻转
    };

    /// <summary>
    /// 超分模式
    /// </summary>
    enum class uvc_enum_sr_mode : BYTE
    {
        NONE = 0,                      // 不超分
        SR = 1                          // 超分
    };

    /// <summary>
    /// 固件升级状态（与设备侧 FLOW_UPDFW_STATE 一一对应）
    /// </summary>
    enum class uvc_enum_upgrade_state : BYTE
    {
        IDLE      = 0,  // 当前无升级任务
        RECEIVING = 1,  // 设备正在接收固件包
        STAGED    = 2,  // 包已落盘并完成验签，等待写入 RTOS 分区
        FLASHING  = 3,  // 正在把固件写入 RTOS 分区（可能持续十几到几十秒）
        DONE      = 4,  // 写入完成，设备即将自动复位
        FAILED    = 5   // 升级失败（验签失败 / 写入失败等）
    };

    struct uvc_sdk_version
    {
        USHORT major;
        USHORT minor;
        USHORT patch;
        USHORT build;
    };

    /// <summary>
    /// uvc设备id
    /// </summary>
    struct uvc_device
    {
        BYTE path[UVC_LEN_64];  // uvc路径
        USHORT vid;             // uvc vid
        USHORT pid;             // uvc pid
    };

    /// <summary>
    /// uvc视频流信息
    /// </summary>
    struct uvc_frame_info
    {
        USHORT interface_num;       // uvc接口描述符编号
        USHORT alt_setting;         // uvc备用接口描述符编号
        USHORT frame_index;         // uvc视频流编号
        USHORT format_index;        // uvc视频流格式编号
        USHORT width;               // uvc视频流宽
        USHORT height;              // uvc视频流高
        BYTE format[UVC_LEN_128];   // uvc视频流格式
    };

    /// <summary>
    /// uvc视频帧数据
    /// </summary>
    struct uvc_frame_data
    {
        BYTE* data;                 // uvc视频帧数据
        UINT32 length;              // uvc视频帧数据长度
        USHORT width;               // uvc视频帧宽
        USHORT height;              // uvc视频帧高
    };

    /// <summary>
    /// 设备信息
    /// </summary>
    struct uvc_device_info
    {
        BYTE model[UVC_LEN_128];            // 设备型号
        BYTE serial_no[UVC_LEN_16];         // 设备序列号
        BYTE id[UVC_LEN_32];                // 设备ID
        BYTE brand[UVC_LEN_32];             // 设备品牌
        BYTE company[UVC_LEN_64];           // 设备厂商
        BYTE firmware_name[UVC_LEN_128];     // 固件名称
        BYTE firmware_version[UVC_LEN_16];  // 固件版本号
    };

    /// <summary>
    /// 测温范围
    /// </summary>
    struct uvc_temp_range
    {
        float min;  // 最小值，默认值：253.15
        float max;  // 最大值，默认值：323.15
    };

    /// <summary>
    /// 测温量程
    /// </summary>
    struct uvc_temp_range_map
    {
        USHORT id;                         // 测温量程id
        uvc_temp_range range;              // 测温量程范围
    };

    /// <summary>
    /// 镜头信息
    /// </summary>
    struct uvc_lens_info
    {
        BYTE name[UVC_LEN_64];          // 镜头名称
        float focal;                    // 镜头焦距
        float vfov;                     // 垂直视场角
        float hfov;                     // 水平视场角
        BYTE band[UVC_LEN_32];          // 镜头波长
        uvc_temp_range_map range_map[4];// 镜头的量程信息，最大4
        USHORT ranges_count;            // 真正镜头量程的数量
    };

    /// <summary>
    /// 探测器分辨率
    /// </summary>
    struct uvc_size
    {
        USHORT width;  // 宽度，默认值：320
        USHORT height; // 高度，默认值：240
    };

    /// <summary>
    /// lut表数据
    /// </summary>
    struct uvc_lut_table
    {
        USHORT range_id;            // 测温量程id

        USHORT ad[UVC_LEN_64];      // AD值数组指针，默认值：空指针
        float t[UVC_LEN_64];        // 温度值数组指针，默认值：空指针 (单位: 开氏度)
        USHORT lut_count;           // lut表数组长度，默认值：0
    };

    /// <summary>
    /// 测温参数
    /// </summary>
    struct uvc_thermal_params
    {
        float emissivity;       // 发射率，默认值：1.0
        float distance;         // 距离，默认值：1.0米
        float refl_temp;        // 反射温度，默认值：293.15开氏度
        float atm_temp;         // 环境温度，默认值：293.15开氏度
        float rel_humidity;     // 相对湿度，默认值：0.5
    };

    /// <summary>
    /// 坏点
    /// </summary>
    struct uvc_dpc_point
    {
        USHORT y;               // 坏点的纵坐标值
        USHORT x;               // 坏点的横坐标值
    };

    /// <summary>
    /// 坏点参数
    /// </summary>
    struct uvc_dpc_params
    {
        uvc_dpc_point points[UVC_LEN_64];  // 新增坏点数据的数组
        USHORT count;           // 新增坏点的数量
    };

    /// <summary>
    /// 图像参数
    /// </summary>
    struct uvc_image_params
    {
        USHORT contrast;     // 对比度，取值范围：0~100，默认值：50
        USHORT brightness;   // 亮度，取值范围：0~100，默认值：50
        BYTE dde_enable;     // DDE使能，1：使能，0：不使能，默认值：0
        USHORT dde_value;    // DDE强度，取值范围：0~100，默认值：50
    };


    // callback
    typedef void(UVC_CALLBACK* on_uvc_found_cb)(const uvc_device* device, bool found);

    typedef void(UVC_CALLBACK* on_frame_received_cb)(const uvc_frame_data* frame);

#pragma pack(pop)

    // SDK API
    // 获取SDK版本信息
    UVC_SDK_API INT32 uvc_get_sdk_version(uvc_sdk_version* version);

    // 初始化SDK
    UVC_SDK_API INT32 uvc_sdk_init();

    // 启停SDK日志
    UVC_SDK_API INT32 uvc_enable_log(bool enable);

    // 释放SDK
    UVC_SDK_API INT32 uvc_sdk_cleanup();

    // 获取设备列表
    UVC_SDK_API INT32 uvc_get_uvc_list(uvc_device* device_list, USHORT length, USHORT* out_length);

    // 设置设备热插拔事件回调
    UVC_SDK_API INT32 uvc_set_hotplug_callback(on_uvc_found_cb cb);

    // 设备登录
    UVC_SDK_API INT32 uvc_login(USHORT vid, USHORT pid, HANDLE* h);

    // 设备路径登录
    UVC_SDK_API INT32 uvc_login_by_path(BYTE* path, USHORT length, HANDLE* h);

#ifdef ANDROID
    // Android: 通过 Java 层传入的 USB 文件描述符登录设备
    UVC_SDK_API INT32 uvc_login_by_fd(int fd, HANDLE* h);
#endif

    // 设备注销
    UVC_SDK_API INT32 uvc_logout(HANDLE* h);

    // 获取视频流列表
    UVC_SDK_API INT32 uvc_get_frame_list(HANDLE h, uvc_frame_info* frames, USHORT length, USHORT* out_length);

    // 设置视频流回调
    UVC_SDK_API INT32 uvc_set_stream_callback(HANDLE h, uvc_frame_info* frame, on_frame_received_cb cb);

    /* ================= 设备管理 ================= */
    // 获取设备信息
    UVC_SDK_API INT32 uvc_get_device_info(HANDLE h, uvc_device_info* info);

    // 获取镜头列表
    UVC_SDK_API INT32 uvc_get_lens_info(HANDLE h, uvc_lens_info* info);

    // 设备重启
    UVC_SDK_API INT32 uvc_device_restart(HANDLE h);

    // 设备重置
    UVC_SDK_API INT32 uvc_device_reset(HANDLE h);

    // 设备升级
    UVC_SDK_API INT32 uvc_device_upgrade(HANDLE h, BYTE* path, USHORT length);

    // 获取设备升级状态
    UVC_SDK_API INT32 uvc_get_upgrade_state(HANDLE h, uvc_enum_upgrade_state* state);

    /* ================= 热像设置 ================= */
    // 执行热像校正
    UVC_SDK_API INT32 uvc_thermal_calibrate(HANDLE h);

    // 获取热像挡片模式
    UVC_SDK_API INT32 uvc_get_thermal_shutter_mode(HANDLE h, uvc_enum_shutter_mode* mode);

    // 设置热像挡片模式
    UVC_SDK_API INT32 uvc_set_thermal_shutter_mode(HANDLE h, const uvc_enum_shutter_mode* mode);

    // 获取热像挡片自动校正时间
    UVC_SDK_API INT32 uvc_get_thermal_shutter_time(HANDLE h, USHORT* minutes);

    // 设置热像挡片自动校正时间
    UVC_SDK_API INT32 uvc_set_thermal_shutter_time(HANDLE h, const USHORT* minutes);

    // 获取热像分辨率
    UVC_SDK_API INT32 uvc_get_thermal_resolution(HANDLE h, uvc_size* uvc_size);

    // 获取热像当前测温量程
    UVC_SDK_API INT32 uvc_get_thermal_range(HANDLE h, USHORT* range_id);

    // 设置热像当前测温量程
    UVC_SDK_API INT32 uvc_set_thermal_range(HANDLE h, const USHORT* range_id);

    // 获取热像当前LUT表
    UVC_SDK_API INT32 uvc_get_thermal_lut(HANDLE h, uvc_lut_table* lut);

    // 获取热像测温参数
    UVC_SDK_API INT32 uvc_get_thermal_params(HANDLE h, uvc_thermal_params* param);

    // 设置热像测温参数
    UVC_SDK_API INT32 uvc_set_thermal_params(HANDLE h, const uvc_thermal_params* param);

    // 增加热像的坏点
    UVC_SDK_API INT32 uvc_set_thermal_dpc(HANDLE h, const uvc_dpc_params* param);

    // 获取热像超分模式
    UVC_SDK_API INT32 uvc_get_thermal_sr(HANDLE h, uvc_enum_sr_mode* mode);

    // 设置热像超分模式
    UVC_SDK_API INT32 uvc_set_thermal_sr(HANDLE h, const uvc_enum_sr_mode* mode);

    /* ================= 显示设置 ================= */
    // 获取图像参数
    UVC_SDK_API INT32 uvc_get_image_params(HANDLE h, uvc_image_params* param);

    // 设置图像参数
    UVC_SDK_API INT32 uvc_set_image_params(HANDLE h, const uvc_image_params* param);

    // 获取图像翻转模式
    UVC_SDK_API INT32 uvc_get_image_flip(HANDLE h, uvc_enum_image_flip_mode* mode);

    // 设置图像翻转模式
    UVC_SDK_API INT32 uvc_set_image_flip(HANDLE h, const uvc_enum_image_flip_mode* mode);

#ifdef __cplusplus
}
#endif