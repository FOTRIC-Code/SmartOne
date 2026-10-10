/*
 * IRtekFileSDK 3.0
 * Copyright 2021, fotric.cn
 *
 * Library for IRTek file format
*/
#ifndef IRTEKFILESDK_H_INCLUDED
#define IRTEKFILESDK_H_INCLUDED

#ifdef _WIN32
#ifdef IRTEKFILESDK_EXPORTS
#ifdef __cplusplus
#define IR_FILE_API extern "C" __declspec(dllexport)
#else
#define IR_FILE_API __declspec(dllexport)
#endif
#else
#ifdef __cplusplus
#define IR_FILE_API extern "C" __declspec(dllimport)
#else
#define IR_FILE_API __declspec(dllexport)
#endif
#endif
#else
#ifdef __cplusplus
#define IR_FILE_API extern "C"
#else
#define IR_FILE_API
#endif
#endif

#pragma pack(push, 8)

typedef void*               HANDLE; /**< IR_FILE句柄 */
typedef unsigned char       BYTE;   /**< 单字节或布尔值 */
typedef short               SHORT;
typedef unsigned short      USHORT;
typedef int                 INT32;
typedef unsigned int        UINT32;
typedef unsigned long long  UINT64;

/** 所有函数的返回值 */
typedef enum 
{
    IR_FILE_EC_OK = 0,              /**< 函数调用成功 */
    IR_FILE_EC_FAIL = -1,           /**< 函数调用失败 */
    IR_FILE_EC_MEMORY = -2,         /**< 内存分配失败 */
    IR_FILE_EC_NULL_POINTER = -3,   /**< 空指针 */
    IR_FILE_EC_INVALID_KEY = -4,    /**< 索引不存在 */
    IR_FILE_EC_NOT_SUPPORTED = -5   /**< 功能不支持 */
} ir_file_enum_error_code;

/** 点 */
typedef struct
{
    SHORT x;                        /**< 点的X轴坐标 */
    SHORT y;                        /**< 点的Y轴坐标 */
} ir_file_point;

/** 温度点 */
typedef struct
{
    ir_file_point point;            /**< 点的坐标 */
    float temp;                     /**< 点的温度值 */
} ir_file_point_value;

/** 归一化浮点坐标 */
typedef struct
{
    float x;                        /**< 点的X轴相对坐标 */
    float y;                        /**< 点的Y轴相对坐标 */
} ir_file_point_f;

/** 矩形 */
typedef struct
{
    SHORT x;                        /**< 矩形左上角点的X轴坐标 */
    SHORT y;                        /**< 矩形左上角点的Y轴坐标 */
    SHORT width;                    /**< 矩形的长度 */
    SHORT height;                   /**< 矩形的宽度 */
} ir_file_rectangle;

/** 归一化浮点矩形 */
typedef struct
{
    float x;                        /**< 矩形左上角点的X轴相对坐标 */
    float y;                        /**< 矩形左上角点的Y轴相对坐标 */
    float width;                    /**< 矩形的相对长度 */
    float height;                   /**< 矩形的相对宽度 */
} ir_file_rectangle_f;

/** 缓存区 */
typedef struct
{
    UINT32 length;                  /**< 缓存区长度 */
    BYTE* buffer;                   /**< 字节数组首地址 */
} ir_file_buffer;

/** 获取SDK版本信息
* @param ver    [out] SDK版本号，四字节形式，如0x02040100为2.4.1.0。若为空，则
*                     不读取。
* @param str    [out] SDK版本号，字符串形式。若为空，则不读取。
*/
IR_FILE_API INT32 ir_file_get_sdk_version(UINT32* ver, ir_file_buffer* str);

/** 热像文件类型 */
typedef enum 
{
    IR_FILE_OTHER = 0,              /**< 非热像文件 */
    IR_FILE_THERMAL_PICTURE = 1,    /**< 热像图片 */
    IR_FILE_THERMAL_VIDEO = 2,      /**< 热像视频 */
    IR_FILE_VISUAL_PICTURE = 3,     /**< 可见光图片 */
    IR_FILE_GRID_THERMAL_PICTURE = 4/**< 电网热像图片 */
} ir_file_enum_file_type;

/** 检查文件是否为热像格式
* @param file_name [in] 文件路径。
* @param value     [out] 热像文件类型。
*/
IR_FILE_API INT32 ir_file_peek_file_type(const char* file_name, ir_file_enum_file_type* value);

/** 打开热像文件
* 若文件不存在则失败。
* @param file_name [in] 文件路径。
* @param h         [out] IR_FILE句柄。
*/
IR_FILE_API INT32 ir_file_open(const char* file_name, HANDLE* h);

/** 创建热像文件
* 若文件已存在将被覆盖。
* @param file_name [in] 文件路径。
* @param file_type [in] 文件类型。可创建类型为：
*                       IR_FILE_THERMAL_PICTURE，
*                       IR_FILE_THERMAL_VIDEO，
*                       IR_FILE_VISUAL_PICTURE。
* @param h         [out] IR_FILE句柄。
*/
IR_FILE_API INT32 ir_file_create(const char* file_name, ir_file_enum_file_type file_type, HANDLE* h);

/** 保存热像文件
* @param  h [in] IR_FILE句柄。
*/
IR_FILE_API INT32 ir_file_save(HANDLE h);

/** 热像文件另存为
* @param  h         [in] IR_FILE句柄。
* @param file_name  [in] 文件路径。
*/
IR_FILE_API INT32 ir_file_save_as(HANDLE h, const char* file_name);

/** 保存2.01版本FJPG图片
* @param  h [in] IR_FILE句柄。
*/
IR_FILE_API INT32 ir_file_save_fjpg(HANDLE h);

/** 另存为2.01版本FJPG图片
* @param  h         [in] IR_FILE句柄。
* @param file_name  [in] 文件路径。
*/
IR_FILE_API INT32 ir_file_save_as_fjpg(HANDLE h, const char* file_name);

/** 关闭热像文件 */
IR_FILE_API INT32 ir_file_close(HANDLE h);

/** 获取热像文件版本号
* @param h      [in]  IR_FILE句柄
* @param ver    [out] 文件版本号
*/
IR_FILE_API INT32 ir_file_get_file_version(HANDLE h, USHORT* value);

/** 读热像文件名
* @param h      [in] IR_FILE句柄。
* @param value  [out] 热像文件名。buffer需要用户分配内存，若buffer为空，不读取
*                     备注信息，length返回所需内存大小。
*/
IR_FILE_API INT32 ir_file_get_file_name(HANDLE h, ir_file_buffer* value);

/** 读热像文件类型
* @param h      [in] IR_FILE句柄。
* @param value  [out] 热像文件类型。
*/
IR_FILE_API INT32 ir_file_get_file_type(HANDLE h, ir_file_enum_file_type* value);

/** 设备信息 */
typedef struct
{
    BYTE model[32];                 /**< 设备型号 */
    BYTE serialNo[48];              /**< 设备序列号 */
    BYTE brand[32];                 /**< 品牌 */
    BYTE company[64];               /**< 厂商 */
    UINT32 logoLength;              /**< 图片大小 */
    BYTE* logo;                     /**< logo图片，jpg文件，字节数组首地址 */
} ir_file_camera_info;

/** 读设备信息
* @param h      [in] IR_FILE句柄。
* @param value  [out] 设备信息。logo需要用户分配内存，若logo为空，不 读取logo图
*                     片，logoLength返回所需内存大小。
*/
IR_FILE_API INT32 ir_file_get_camera_info(HANDLE h, ir_file_camera_info* value);

/** 写设备信息
* @param h      [in] IR_FILE句柄。
* @param value  [in] 设备信息。若logo为空，不写入logo图片。若value为空，清空设
*                    备信息。
*/
IR_FILE_API INT32 ir_file_set_camera_info(HANDLE h, const ir_file_camera_info* value);

/** 镜头信息 */
typedef struct
{
    BYTE name[32];                  /**< 镜头名 */
    float focalLength;              /**< 镜头焦距，默认为0.0f，表示无效参数 */
    float hfov;                     /**< 镜头度数，默认为0.0f，表示无效参数 */
} ir_file_lens_info;

/** 读镜头信息
* @param h      [in] IR_FILE句柄。
* @param value  [out] 镜头信息。
*/
IR_FILE_API INT32 ir_file_get_lens_info(HANDLE h, ir_file_lens_info* value);

/** 写镜头信息
* @param h      [in] IR_FILE句柄。
* @param value  [in] 镜头信息。若value为空，清空镜头信息。
*/
IR_FILE_API INT32 ir_file_set_lens_info(HANDLE h, const ir_file_lens_info* value);

/** 镜头信息 V2 */
typedef struct
{
    BYTE name[32];                  /**< 镜头名 */
    float focalLength;              /**< 镜头焦距，默认为0.0f，表示无效参数 */
    float hfov;                     /**< 镜头度数，默认为0.0f，表示无效参数 */
    BYTE band[32];                  /**< 镜头响应波段，默认为空，表示无响应波段信息 */
} ir_file_lens_info_v2;

/** 读镜头信息 V2
 * @param h      [in] IR_FILE句柄。
 * @param value  [out] 镜头信息。
 */
IR_FILE_API INT32 ir_file_get_lens_info_v2(HANDLE h, ir_file_lens_info_v2* value);

/** 写镜头信息 V2
 * @param h      [in] IR_FILE句柄。
 * @param value  [in] 镜头信息。若value为空，清空镜头信息。
 */
IR_FILE_API INT32 ir_file_set_lens_info_v2(HANDLE h, const ir_file_lens_info_v2* value);

/** 量程信息 */
typedef struct
{
    float max;                      /**< 量程上限，取值范围0~5750，默认值为
                                     *   423.15 */
    float min;                      /**< 量程下限，取值范围0~5750，默认值为
                                     *   253.15 */
} ir_file_temp_range;

/** 读量程信息
* @param h      [in] IR_FILE句柄。
* @param value  [out] 量程信息。
*/
IR_FILE_API INT32 ir_file_get_temp_range(HANDLE h, ir_file_temp_range* value);

/** 写量程信息
* @param h      [in] IR_FILE句柄。
* @param value  [in] 量程信息。若value为空，清空量程信息。
*/
IR_FILE_API INT32 ir_file_set_temp_range(HANDLE h, const ir_file_temp_range* value);

/** 温度查找表 */
typedef struct
{
    BYTE radMethod;                 /**< 温度补偿算法版本。0x00表示温度补偿算法
                                     *   1.0。0x01表示温度补偿算法2.0，默认值为
                                     *   0x01。 */
    UINT32 count;                   /**< 出厂LUT长度 */
    USHORT* adArray;                /**< 出厂LUT表的AD值表，USHORT数组 */
    float* tempArray;               /**< 出厂LUT表的温度值表，float数组 */
} ir_file_lut;

/** 读温度查找表
* @param h      [in] IR_FILE句柄。
* @param value  [out] 温度查找表。adArray需要用户分配内存（USHORT[count]），
*                     且tempArray需要用户分配内存（float[count]）。若adArray或
*                     tempArray为空，不读取该项，count返回温度查找表长度。
*/
IR_FILE_API INT32 ir_file_get_lut(HANDLE h, ir_file_lut* value);

/** 写温度查找表
* @param h      [in] IR_FILE句柄。
* @param value  [in] 温度查找表。若adArray或tempArray为空，写入失败。若value为
*                    空，清空温度查找表。
*/
IR_FILE_API INT32 ir_file_set_lut(HANDLE h, const ir_file_lut* value);

/** 读补偿后的温度查找表
* @param h      [in] IR_FILE句柄。
* @param value  [out] 温度查找表。adArray需要用户分配内存（USHORT[count]），
*                     且tempArray需要用户分配内存（float[count]）。若adArray或
*                     tempArray为空，不读取该项，count返回温度查找表长度。
*/
IR_FILE_API INT32 ir_file_get_calculated_lut(HANDLE h, ir_file_lut* value);

/** 调色板 */
typedef struct
{
    BYTE refNo;                     /**< 调色板编号，0xFF表示使用自定义调色
                                     *   板 */
    UINT32 colors[256];             /**< 调色板，每个UINT32前24位分别为RGB，最
                                     *   后8位不使用 */
    BYTE inverted;                  /**< 调色板是否反转显示，0x01表示启用，其它
                                     *   表示不启用，默认值为0x00 */
} ir_file_palette;

/** 读调色板
* @param h      [in] IR_FILE句柄。
* @param value  [out] 调色板信息。
*/
IR_FILE_API INT32 ir_file_get_palette(HANDLE h, ir_file_palette* value);

/** 调色板 V2 */
typedef struct
{
    BYTE refNo;                     /**< 调色板编号，0xFF表示使用自定义调色
                                     *   板 */
    UINT32 src_colors[256];         /**< 调色板原始数据，每个UINT32前24位分别为RGB，最
                                     *   后8位不使用 */
    UINT32 dest_colors[256];        /**< 调色板叠加信息后的数据，每个UINT32前24位分别为RGB，最
                                     *   后8位不使用 */
    BYTE inverted;                  /**< 调色板是否反转显示，0x01表示启用，其它
                                     *   表示不启用，默认值为0x00 */
} ir_file_palette_v2;

/** 读调色板 V2
* @param h      [in] IR_FILE句柄。
* @param value  [out] 调色板信息。
*/
IR_FILE_API INT32 ir_file_get_palette_v2(HANDLE h, ir_file_palette_v2* value);

/** 写调色板
* @param h      [in] IR_FILE句柄。
* @param value  [in] 调色板信息。若value为空，清空调色板信息。
*/
IR_FILE_API INT32 ir_file_set_palette(HANDLE h, const ir_file_palette* value);

/** 图像分辨率 */
typedef struct
{
    SHORT width;                    /**< 宽度 */
    SHORT height;                   /**< 高度 */
} ir_file_size;

/** 读图像分辨率
* @param h      [in] IR_FILE句柄。
* @param size   [out] 图像分辨率。
*/
IR_FILE_API INT32 ir_file_get_thermal_size(HANDLE h, ir_file_size* value);

/** 写图像分辨率
* @param h      [in] IR_FILE句柄。
* @param size   [in] 图像分辨率。若value为空，清空图像分辨率。
*/
IR_FILE_API INT32 ir_file_set_thermal_size(HANDLE h, const ir_file_size* value);

/** 测温参数 */
typedef struct
{
    float reflTemp;                 /**< 反射温度，取值范围0~5750，默认值为
                                     *   293.15 */
    float atmTemp;                  /**< 大气温度，取值范围0~5750，默认值为
                                     *   293.15 */
    float distance;                 /**< 目标距离，取值范围0~50000，默认值为1.0 */
    float relHumidity;              /**< 相对湿度，取值范围0.01~1.0，默认值为
                                     *   0.5 */
    float emissivity;               /**< 发射率，取值范围0.01~1.0，默认值为1.0 */
    float extOptTemp;               /**< 外部光学温度，取值范围0~5750，默认值为
                                     *   293.15 */
    float extOptTrans;              /**< 外部光学透过率，取值范围0.01~1.0，默认
                                     *   值为1.0 */
} ir_file_thermal_params;

/** 读测温参数
* @param h      [in] IR_FILE句柄。
* @param value  [out] 测温参数。
*/
IR_FILE_API INT32 ir_file_get_thermal_params(HANDLE h, ir_file_thermal_params* value);

/** 写测温参数
* @param h      [in] IR_FILE句柄。
* @paramvalue   [in] 测温参数。若value为空，清空测温参数。
*/
IR_FILE_API INT32 ir_file_set_thermal_params(HANDLE h, const ir_file_thermal_params* value);

/** 测量标识值类型 */
typedef enum 
{
    IR_FILE_MAX = 1,                /**< 最高温 */
    IR_FILE_MIN = 2,                /**< 最低温 */
    IR_FILE_AVG = 3,                /**< 平均温 */
} ir_file_enum_temp_value_type;

/** 参考温度 */
typedef struct
{
    BYTE enabled;                             /**< 是否启用基准，0x01表示启用，
                                               *   其它表示不启用，默认值为0x00 */
    BYTE name[32];                            /**< 参考源名称，空表示使用temp，
                                               *   其他表示测量标识名称 */
    ir_file_enum_temp_value_type valueType;   /**< 测量标识值类型，默认为最高温 */
    float temp;                               /**< 参考温度值，取值范围0~5750，
                                               *   默认值为293.15 */
} ir_file_reference;

/** 读参考温度信息
* @param h      [in] IR_FILE句柄。
* @param value  [out] 参考温度信息。
*/
IR_FILE_API INT32 ir_file_get_reference(HANDLE h, ir_file_reference* value);

/** 读参考源温度
* @param h      [in] IR_FILE句柄。
* @param value  [out] 参考源温度。
*/
IR_FILE_API INT32 ir_file_get_reference_temp(HANDLE h, float* value);

/** 写参考温度信息
* @param h      [in] IR_FILE句柄。
* @param value  [in] 参考温度信息。
*/
IR_FILE_API INT32 ir_file_set_reference(HANDLE h, const ir_file_reference* value);

/** 温宽模式 */
typedef enum 
{
    IR_FILE_FIXED = 1,              /**< 固定模式 */
    IR_FILE_AUTO = 2,               /**< 自动模式 */
    IR_FILE_TOUCH = 3,              /**< 触控模式 */
} ir_file_enum_gain_mode;

/** 温宽信息 */
typedef struct
{
    ir_file_enum_gain_mode mode;    /**< 温宽模式，默认为自动模式 */
    float max;                      /**< 温宽上限值，取值范围0~5750 */
    float min;                      /**< 温宽下限值，取值范围0~5750 */
    BYTE fixedSpan;                 /**< 是否使用固定温宽跨度，0x01表示启用，其
                                     *   它表示不启用，默认值为0x00 */
    float span;                     /**< 温宽跨度，取值范围0.1~5750，默认值为
                                     *   2.0 */
} ir_file_gain;

/** 读温宽信息
* @param h      [in] IR_FILE句柄。
* @param value  [out]温宽信息。
*/
IR_FILE_API INT32 ir_file_get_gain(HANDLE h, ir_file_gain* value);

/** 写温宽信息
* @param h      [in] IR_FILE句柄。
* @param value  [in|out] 温宽信息。若value为空，清空温宽信息。若为自动模式，写
*                        温宽信息成功后min和max将会被更新。
*/
IR_FILE_API INT32 ir_file_set_gain(HANDLE h, ir_file_gain* value);

/** 报警类型 */
typedef enum 
{
    IR_FILE_NORMAL = 0,             /**< 不启用报警 */
    IR_FILE_BELOW = 1,              /**< 低于报警 */
    IR_FILE_ABOVE = 2,              /**< 高于报警 */
    IR_FILE_BETWEEN = 3,            /**< 之间报警 */
    IR_FILE_MIX_PALETTE = 4,        /**< 复合调色板报警 */
    IR_FILE_OUTSIDE = 5,            /**< 之外报警 */
    IR_FILE_HUMIDITY = 6,           /**< 湿度报警 */
    IR_FILE_INSULATION = 7,         /**< 保温报警 */
} ir_file_enum_alarm_type;

/** 报警信息 */
typedef struct
{
    ir_file_enum_alarm_type type;   /**< 报警类型，默认为不启用 */
    BYTE custom;                    /**< 是否启用自定义报警，不启用报警和复合调色板报警类型，此参数无效。
                                     *   0x01表示启用，其它表示不启用，默认值为
                                     *   0x00，*/
    UINT32 color;                   /**< 报警颜色，ARGB，默认值为
                                     *   （127,255,0,0） */
    float max;                      /**< 高于报警(Above)类型，高于该阈值；
                                     *   之间报警(Between)，低于该阈值；
                                     *   复合调色板报警(MixPalette)，高于该阈值；
                                     *   之外报警(Outsize)，高于该阈值；
                                     *   其他报警类型无效。
                                     *   取值范围0~5750，默认值为313.15 */
    float min;                      /**< 低于报警(Below)类型，低于该阈值；
                                     *   之间报警(Between)，高于该阈值；
                                     *   复合调色板报警(MixPalette)，低于该阈值；
                                     *   之外报警(Outside)，低于该阈值；
                                     *   其他模报警类型无效。
                                     *   取值范围0~5750，默认值为293.15 */
    float rhLimit;                  /**< 相对湿度限制，除湿度报警(Humidity)类型，
                                     *   其他类型此参数无效。
                                     *   取值范围0.01~1.0，默认值为1.0 */
    float indoorTemp;               /**< 室内温度，除保温报警(Insulation)类型，
                                     *   其他类型此参数无效。
                                     *   取值范围0~5750，默认值为293.15 */
    float outdoorTemp;              /**< 室外温度，除保温报警(Insulation)类型，
                                     *   其他类型此参数无效。
                                     *   取值范围0~5750，默认值为283.15 */
    float heatIndex;                /**< 热量指数，除保温报警(Insulation)类型，
                                     *   其他类型此参数无效。
                                     *   取值范围0.0~1.0，默认值为0.8 */
} ir_file_alarm;

/** 报警信息 V2*/
typedef struct
{
    ir_file_enum_alarm_type type; /**< 报警类型，默认为不启用 */
    BYTE custom; /**< 是否启用自定义报警，不启用报警和复合调色板报警类型，此参数无效。
                                     *   0x01表示启用，其它表示不启用，默认值为
                                     *   0x00，*/
    UINT32 color; /**< 报警颜色，ARGB，默认值为
                                     *   （127,255,0,0） */
    float max; /**< 高于报警(Above)类型，高于该阈值；
                                     *   之间报警(Between)，低于该阈值；
                                     *   复合调色板报警(MixPalette)，高于该阈值；
                                     *   之外报警(Outsize)，高于该阈值；
                                     *   其他报警类型无效。
                                     *   取值范围0~5750，默认值为313.15 */
    float min; /**< 低于报警(Below)类型，低于该阈值；
                                     *   之间报警(Between)，高于该阈值；
                                     *   复合调色板报警(MixPalette)，低于该阈值；
                                     *   之外报警(Outside)，低于该阈值；
                                     *   其他模报警类型无效。
                                     *   取值范围0~5750，默认值为293.15 */
    float rhLimit; /**< 相对湿度限制，除湿度报警(Humidity)类型，
                                     *   其他类型此参数无效。
                                     *   取值范围0.01~1.0，默认值为1.0 */
    float indoorTemp; /**< 室内温度，除保温报警(Insulation)类型，
                                     *   其他类型此参数无效。
                                     *   取值范围0~5750，默认值为293.15 */
    float outdoorTemp; /**< 室外温度，除保温报警(Insulation)类型，
                                     *   其他类型此参数无效。
                                     *   取值范围0~5750，默认值为283.15 */
    float heatIndex; /**< 热量指数，除保温报警(Insulation)类型，
                                     *   其他类型此参数无效。
                                     *   取值范围0.0~1.0，默认值为0.8 */
    float threshold; /**< 湿度报警、保温报警类型的温度阈值，
                                     *   其他类型此参数无效。
                                     *   取值范围0~5750，默认值为293.15 */
} ir_file_alarm_v2;

/** 读报警信息
* @param h      [in] IR_FILE句柄。
* @param value  [out] 报警信息。
*/
IR_FILE_API INT32 ir_file_get_alarm(HANDLE h, ir_file_alarm* value);

/** 读报警信息 V2
* @param h      [in] IR_FILE句柄。
* @param value  [out] 报警信息。
*/
IR_FILE_API INT32 ir_file_get_alarm_v2(HANDLE h, ir_file_alarm_v2* value);

/** 写报警信息
* @param h      [in] IR_FILE句柄。
* @param value  [in] 报警信息。若value为空，清空报警信息。
*/
IR_FILE_API INT32 ir_file_set_alarm(HANDLE h, const ir_file_alarm* value);

/** 地理位置 */
typedef struct
{
    BYTE isGPSValid;                /**< GPS信息有效标识。0x01表示启用，其它表
                                     *   示不启用，默认值为0x00 */
    double longitude;               /**< 经度 */
    double latitude;                /**< 纬度 */
    double altitude;                /**< 海拔 */
    BYTE isCompassValid;            /**< 指南针有效标识。0x01表示启用，其它表示
                                     *   不启用，默认值为0x00 */
    SHORT direction;                /**< 方向，为-180~180的角度值，+-180表示正
                                     *   南方向，0度表示正北，-90表示正西，+90
                                     *   表示正东。-180~180范围外的值表示无意义 */
} ir_file_geo_location;

/** 读地理位置信息
* @param h      [in] IR_FILE句柄。
* @param value  [out] 地理位置信息。
*/
IR_FILE_API INT32 ir_file_get_geo_location(HANDLE h, ir_file_geo_location* value);

/** 写地理位置信息
* @param h      [in] IR_FILE句柄。
* @param value  [in] 地理位置信息。若value为空，清空地理位置信息。
*/
IR_FILE_API INT32 ir_file_set_geo_location(HANDLE h, const ir_file_geo_location* value);

/** 读文本备注信息
* @param h      [in] IR_FILE句柄。
* @param value  [out] 文本备注信息。buffer需要用户分配内存，若buffer为空，不读
*                     取备注信息，length返回所需内存大小。
  */
IR_FILE_API INT32 ir_file_get_note(HANDLE h, ir_file_buffer* value);

/** 写文本备注信息
* @param h      [in] IR_FILE句柄。
* @param value  [in] 文本备注信息。若buffer为空，写入失败。若value为空，清空备
*                    注信息。
*/
IR_FILE_API INT32 ir_file_set_note(HANDLE h, const ir_file_buffer* value);

/** 语音文件格式 */
typedef enum 
{
    IR_FILE_WAV = 1,                /**< WAV */
    IR_FILE_MP3 = 2,                /**< MP3 */
} ir_file_enum_audio_type;

/** 音频文件 */
typedef struct
{
    ir_file_enum_audio_type type;   /**< 语音文件格式，默认为MP3 */
    UINT32 length;                  /**< 音频大小 */
    BYTE* buffer;                   /**< 字节数组首地址 */
} ir_file_audio;

/** 读音频文件
* @param h      [in] IR_FILE句柄。
* @param value  [out] 音频文件。buffer需要用户分配内存，若buffer为空，不读取音
*                     频文件，length返回所需内存大小。
*/
IR_FILE_API INT32 ir_file_get_audio(HANDLE h, ir_file_audio* value);

/** 写音频文件
* @param h      [in] IR_FILE句柄。
* @param value  [in] 音频文件。若buffer为空，写入失败。若value为空，清空音频文
*                    件。
*/
IR_FILE_API INT32 ir_file_set_audio(HANDLE h, const ir_file_audio* value);

/** 用户数据键 */
typedef BYTE ir_file_key[16];

/** 用户数据键列表 */
typedef struct
{
    UINT32 count;                   /**< 用户数据键数量 */
    ir_file_key* keys;              /**< 用户数据键数组首地址 */
} ir_file_keys;

/** 获取用户数据键列表
* @param h      [in] IR_FILE句柄。
* @param value  [out] 用户数据键列表。
*/
IR_FILE_API INT32 ir_file_get_keys(HANDLE h, ir_file_keys* value);

/** 键值对 */
typedef struct 
{
    ir_file_key key;                /**< 键 */
    UINT32 vLength;                 /**< 值长度 */
    BYTE* vBuffer;                  /**< 值 */
}ir_file_kvPair;

/** 读用户数据列表
* @param h      [in] IR_FILE句柄。
* @param value  [in|out] 用户数据项，key为输入项，其它为输出项。vBuffer需要用户
*                        分配内存vLength字节，若为空，不读取值，vLength返回值长
*                        度。
* @retval IR_FILE_EC_INVALID_KEY 键不存在。
*/
IR_FILE_API INT32 ir_file_get_user_data(HANDLE h, ir_file_kvPair* value);

/** 写用户数据列表
* 用户数据以键值对方式存取，其中已定义：
* 其中已定义：
* 1.收藏标签：
* Key：“FAVORITE”
* Value：收藏标签，长度为1个字节，“Y”表示收藏，其它表示正常
* 2.注释标签：
* Key：“TAG”
* Value：注释标签，若存在多个标签采用字符‘\n’分隔
* 3.分组文件名列表：
* Key：“GROUPFILES”
* Value：关联分组文件名列表，若存在多个文件采用字符‘\n’分隔
* 4.定时拍照文件名列表：
* Key：“TIMERFILE”
* Value：采用定时拍摄方式时，保存GUID和编号，GUID采用编码格式“EDDCC222-97B3-47
* 24-A4C9-742A7A28EA25”，属于同一定时拍摄的文件保持GUID不变，GUID和编号之间采
* 用‘\n’分隔符，编号采用字符串编码格式。
* 5.耐材厚度监测参数：
* Key：“THICKNESSPARAMS”
* Value：采用如下JSON STRING格式存储
* {
*   "enabled": //是否启用耐材厚度监测,1表示启用,其他表示未启用
*   "initInnerTemp": //初始流体温度,取值范围0~5750,默认值为1873.15
*   "initOuterTemp": //初始外壳温度,取值范围0~5750,默认值为553.15
*   "initAtmTemp": //初始环境温度,取值范围0~5750,默认值为303.15
*   "correctionFactor": //修正系统,取值范围0.00-1.00,默认值0.25
*   "alarmLimit": //报警阈值,取值范围0~100,默认值50
* }
* @param h      [in] IR_FILE句柄。
* @param value  [in] 用户数据项。若vBuffer为空，删除该项用户数据。若value为空，
*                    清空用户数据列表。
*/
IR_FILE_API INT32 ir_file_set_user_data(HANDLE h, const ir_file_kvPair* value);

/** 读拍摄时间
* @param h      [in] IR_FILE句柄。
* @param value  [out] 拍摄时间，采用UTC时间，从1970-01-01 00:00:00.000起的毫秒
*                     数。
*/
IR_FILE_API INT32 ir_file_get_shot_time(HANDLE h, UINT64* value);

/** 读修改时间
* @param h      [in] IR_FILE句柄。
* @param value  [out] 修改时间，采用UTC时间，从1970-01-01 00:00:00.000起的毫秒
*                     数。
*/
IR_FILE_API INT32 ir_file_get_modified_time(HANDLE h, UINT64* value);

/** 数码变倍信息 */
typedef struct
{
    BYTE enabled;                   /**< 是否启用数码变倍，0x01表示启用，其它表
                                     *   示不启用，默认值为0x00 */
    ir_file_point_f location;       /**< 左上角相对坐标，最小相对坐标为(0.0,0.0)，
                                     *   最大相对坐标为(1.0,1.0)，默认值为
                                     *   (0.0,0.0) */
    float ratio;                    /**< 数码变倍数，变焦倍数需大于1.0，默认值
                                     *   为1 */
} ir_file_zoom_info;

/** 读数码变焦倍数
* @param h      [in] IR_FILE句柄。
* @param value  [out] 数码变焦倍数。
*/
IR_FILE_API INT32 ir_file_get_zoom_info(HANDLE h, ir_file_zoom_info* value);

/** 写数码变焦倍数
* @param h      [in] IR_FILE句柄。
* @param value  [out] 数码变焦倍数。
*/
IR_FILE_API INT32 ir_file_set_zoom_info(HANDLE h, const ir_file_zoom_info* value);

/** 显示模式 */
typedef enum 
{
    IR_FILE_THERMAL = 1,            /**< 红外图像模式 */
    IR_FILE_PIC_IN_PIC = 2,         /**< 画中画模式 */
    IR_FILE_T_DEF = 3,              /**< 可见光测温模式 */
    IR_FILE_FUSION = 4,             /**< 融合模式 */
} ir_file_enum_thermal_view_type;

/** 显示信息 */
typedef struct
{
    ir_file_enum_thermal_view_type type; /**< 显示模式，默认为红外图像模式 */
    float max;                      /**< 融合阈值上限，除融合模式(Fusion)，其他
                                     *   模式此参数无效，默认值为313.15 */
    float min;                      /**< 融合阈值下限，除融合模式(Fusion)，其他
                                     *   模式此参数无效，默认值为293.15 */
    BYTE alpha;                     /**< 透明度，取值范围0~100，表示透明度百分
                                     *   比，除可见光测温模式(T_DEF)，其他模式此参数
                                     *   无效,默认值为0 */
    ir_file_rectangle_f area;       /**< 热像可视区域，使用相对区域大小，除画中
                                     *   画模式(Picture In Picture)，其他模式此
                                     *   参数无效，最大区域相对大小为
                                     *   (0.0,0.0,1.0,1.0)，默认值为
                                     *   (0.25,0.25,0.5,0.5)，最小宽高限制为0.1 */
} ir_file_view_info;

/** 读显示信息
* @param h      [in] IR_FILE句柄。
* @param value  [out] 显示信息。
*/
IR_FILE_API INT32 ir_file_get_view_info(HANDLE h, ir_file_view_info* value);

/** 写显示信息
* @param h      [in] IR_FILE句柄。
* @param value  [in] 显示信息。若value为空，清空显示信息。
*/
IR_FILE_API INT32 ir_file_set_view_info(HANDLE h, const ir_file_view_info* value);

/** 双光融合信息 */
typedef struct
{
    ir_file_size visualSize;        /**< 可见光图像的长宽，默认值为(800,600) */
    ir_file_rectangle_f mappingArea;/**< 热像在可见光图像中的映射区域相对大小，
                                     *   最大区域相对大小为(0.0,0.0,1.0,1.0)，
                                     *   默认值为(0.25,0.25,0.5,0.5)，
                                     *   最小宽高限制为0.1 */
} ir_file_mapping_info;

/** 读双光融合信息
* @param h      [in] IR_FILE句柄。
* @param value  [out] 双光融合信息。
*/
IR_FILE_API INT32 ir_file_get_mapping_info(HANDLE h, ir_file_mapping_info* value);

/** 写双光融合信息
* @param h      [in] IR_FILE句柄。
* @param value  [in]  双光融合信息。若value为空，清空双光融合信息。
*/
IR_FILE_API INT32 ir_file_set_mapping_info(HANDLE h, const ir_file_mapping_info* value);

/** 测量标识名 */
typedef BYTE ir_file_marker_name[32];

/** 测量标识名列表 */
typedef struct
{
    UINT32 count;                   /**< 测量标识名数量 */
    ir_file_marker_name* names;     /**< 测量标识名数组首地址 */
} ir_file_marker_names;

/** 获取测量标识名列表
* @param h      [in] IR_FILE句柄。
* @param value  [out] 测量标识名列表。
*/
IR_FILE_API INT32 ir_file_get_marker_names(HANDLE h, ir_file_marker_names* value);

/** 测量标识类型 */
typedef enum 
{
    IR_FILE_SPOT = 1,               /**< 点 */
    IR_FILE_RECTANGLE = 3,          /**< 矩形 */
    IR_FILE_ELLIPSE = 4,            /**< 椭圆 */
    IR_FILE_POLYGON = 5,            /**< 多边形 */
    IR_FILE_POLYLINE = 6,           /**< 折线 */
} ir_file_enum_marker_type;

/** 测量标识 */
typedef struct
{
    ir_file_enum_marker_type type;  /**< 测量标识类型，默认值为点 */
    ir_file_marker_name name;       /**< 名称 */
    BYTE localParams;               /**< 是否启用局部参数，0x01表示启用，其它表
                                     *   示不启用，默认值为0x00 */
    float reflTemp;                 /**< 反射温度，取值范围0~5750，默认值为
                                     *   293.15 */
    float distance;                 /**< 目标距离，取值范围0~50000，默认值为1.0 */
    float emissivity;               /**< 发射率，取值范围0.01~1.0，默认值为1.0 */
    UINT32 pointCount;		        /**< 点数量 */
    ir_file_point* points;          /**< 点数组首地址 */
    float tempValues[3];            /**< 启用参考温度时表示最高温、最低温、平均温与参考温度的差值 
                                     *   不启用参考温度时表示实际最高温、最低温、平均温*/
    ir_file_point tempSpots[2];     /**< 最高温点、最低温点 */
    float measuredValue;            /**< 点、矩形、椭圆、多边形的面积(平方米)或
                                     *   折线的长度(米) */
    ir_file_buffer exInfo;          /**< 扩展信息 */
} ir_file_marker;

/** 读测量标识
* @param h      [in] IR_FILE句柄。
* @param value  [in|out] 测量标识，name为输入项，其它为输出项。points需要用户分
*                        配内存（ir_file_point[pointCount]），若为空，不读取点
*                        列表，pointCount返回点列表长度。
* @retval IR_FILE_EC_INVALID_KEY 该测量标识不存在。
*/
IR_FILE_API INT32 ir_file_get_marker(HANDLE h, ir_file_marker* value);

/** 写测量标识
* @param h      [in] IR_FILE句柄。
* @param value  [in|out] 测量标识，tempValues、tempPos、area为输出项，其它为输
*                        入项。若测量标识存在则修改，不存在则添加。若points为空，
*                        写入失败。写测量标识成功后输出项将会被更新。
*/
IR_FILE_API INT32 ir_file_set_marker(HANDLE h, ir_file_marker* value);

/** 删除测量标识,会同时清掉对应的reference
* @param h      [in] IR_FILE句柄。
* @param value  [in] 测量标识名。若value为空，清空测量标识列表。
*/
IR_FILE_API INT32 ir_file_remove_marker(HANDLE h, const ir_file_marker_name value);

/** 重命名测量标识
* @param h      [in] IR_FILE句柄。
* @param from   [in] 旧测量标识名。
* @param to     [in] 新测量标识名。
*/
IR_FILE_API INT32 ir_file_rename_marker(HANDLE h, const ir_file_marker_name from
    , const ir_file_marker_name to);

/** 读可见光图片
* @param h      [in] IR_FILE句柄。
* @param value  [out] 可见光图片，jpg文件。buffer需要用户分配内存，若buffer为空，
*                     不读取可见光图片，length返回图片大小。
*/
IR_FILE_API INT32 ir_file_get_visual_image(HANDLE h, ir_file_buffer* value);

/** 写可见光图片
* @param h      [in] IR_FILE句柄。
* @param value  [in] 可见光图片，jpg文件。若buffer为空，写入失败。若value为空，
*                    清空可见光图片。
*/
IR_FILE_API INT32 ir_file_set_visual_image(HANDLE h, ir_file_buffer* value);

/** 读封面图
* 封面图即热像文件中的普通jpg部分，使用看图工具查看图片将显示封面图。
* @param h      [in] IR_FILE句柄。
* @param value  [out] 封面图，jpg文件。buffer需要用户分配内存，若buffer为空，不
*                     读取封面图，length返回图片大小。
*/
IR_FILE_API INT32 ir_file_get_cover_image(HANDLE h, ir_file_buffer* value);

/** 写封面图
* 封面图即热像文件中的普通jpg部分，使用看图工具查看图片将显示封面图。
* @param h      [in] IR_FILE句柄。
* @param value  [in] 封面图，jpg文件，单边最小尺寸240像素。若buffer为空，写入失
*                    败。若value为空，清空封面图。
*/
IR_FILE_API INT32 ir_file_set_cover_image(HANDLE h, const ir_file_buffer* value);

/** 读图像显示参数信息
* @param h      [in] IR_FILE句柄。
* @param value  [out] 图像显示参数信息。buffer需要用户分配内存，若buffer为空，
*                     不读取图像显示参数信息，length返回图像显示参数信息大小。
*/
IR_FILE_API INT32 ir_file_get_vt_params(HANDLE h, ir_file_buffer* value);

/** 写图像显示参数信息
* @param h      [in] IR_FILE句柄。
* @param value  [in] 图像显示参数信息。若buffer为空，写入失败。若value为空，清
*                    空图像显示参数信息。
*/
IR_FILE_API INT32 ir_file_set_vt_params(HANDLE h, const ir_file_buffer* value);

/** 图像ISP类型 */
typedef enum {
    IR_FILE_NEMO = 0, /**< NEMO平台 */
    IR_FILE_HOLLY = 1, /**< HOLLY平台 */
} ir_file_enum_isp_type;

/** HOLLY显示参数 */
typedef struct
{
    USHORT contrast; // 对比度，取值范围：0~100，默认值：50
    USHORT brightness; // 亮度，取值范围：0~100，默认值：50
    BYTE dde_enabled; // DDE使能，1：使能，0：不使能，默认值：0
    USHORT dde_value; // DDE强度，取值范围：0~100，默认值：50
    BYTE deconv_valid; // deconv_psf_sigma是否生效，1：生效，0：不生效（使用默认值）
    float deconv_psf_sigma; // 去卷积PSF sigma，取值范围：0~5，0：关闭去卷积，默认值：0.71
} ir_file_image_params;

#define IR_FILE_DECONV_VALID 1

typedef union {
    ir_file_buffer* buffer; 
    ir_file_image_params* image_params;
} ir_file_vt_params;

/** 读图像显示参数信息
* @param h      [in] IR_FILE句柄。
* @param value  [out] 图像显示参数信息。若ispType为NEMO时输出ir_file_buffer,
*                     若ispType为HOLLY时输出ir_file_image_params。
*/
IR_FILE_API INT32 ir_file_get_vt_params_v2(HANDLE h, ir_file_vt_params value);

/** 写图像显示参数信息
* @param h      [in] IR_FILE句柄。
* @param value  [in] 图像显示参数信息。若ispType为NEMO时,输入ir_file_buffer，buffer为空，写入失败。若value为空，清
*                    空图像显示参数信息。若ispType为HOLLY时,输入ir_file_image_params。
*/
IR_FILE_API INT32 ir_file_set_vt_params_v2(HANDLE h, const ir_file_vt_params value);

    /** 温差标识名 */
typedef BYTE ir_file_delta_name[32];

/** 温差标识名列表 */
typedef struct
{
    UINT32 count;                   /**< 温差标识名数量 */
    ir_file_delta_name* names;      /**< 温差标识名数组首地址 */
} ir_file_delta_names;

/** 获取温差标识名列表
* @param h      [in] IR_FILE句柄。
* @param value  [out] 温差标识名列表。
*/
IR_FILE_API INT32 ir_file_get_delta_names(HANDLE h, ir_file_delta_names* value);

/** 温差标识 */
typedef struct
{
    ir_file_delta_name name;        /**< 温差标识名 */
    BYTE srcName[32];               /**< 源名称 */
    ir_file_enum_temp_value_type srcValueType; /**< 源标识值类型，默认为最高温 */
    float srcTemp;                  /**< 源参考温度值，取值范围0~5750，默认值为
                                     *   293.15 */
    BYTE destName[32];              /**< 目标名称 */
    ir_file_enum_temp_value_type destValueType; /**< 目标标识值类型，默认为最高
                                                 *   温 */
    float destTemp;                 /**< 目标参考温度值，取值范围0~5750，默认值
                                     *   为293.15  */
    float value;                    /**< 温差值 */
} ir_file_delta;

/** 读温差标识
* @param h      [in] IR_FILE句柄。
* @param value  [in|out] 温差标识，name为输入项，其它为输出项。
* @retval IR_FILE_EC_INVALID_KEY 该温差标识不存在。
*/
IR_FILE_API INT32 ir_file_get_delta(HANDLE h, ir_file_delta* value);

/** 写温差标识
* @param h      [in] IR_FILE句柄。
* @param value  [in|out] 温差标识，温差值为输出项，其它为输入项。若温差标识存在
*                        则修改，不存在则添加。写温差标识成功后结构体内温差值将
*                        会被更新。
*/
IR_FILE_API INT32 ir_file_set_delta(HANDLE h, ir_file_delta* value);

/** 删除温差标识
* @param h      [in] IR_FILE句柄。
* @param value  [in] 测量标识名。若value为空，清空温差标识列表。
*/
IR_FILE_API INT32 ir_file_remove_delta(HANDLE h, const ir_file_delta_name value);

/** 色彩分布类型 */
typedef enum 
{
    IR_FILE_T_TWB = 1,              /**< 高温差均衡成像分布 */
    IR_FILE_TEMP_LINEAR = 2,        /**< 温度线性分布 */
    IR_FILE_AD_LINEAR = 3,          /**< 信号线性分布 */
} ir_file_enum_color_dist_type;

/** 读色彩分布类型
* @param h      [in] IR_FILE句柄。
* @param value  [out] 色彩分布类型，默认为线性分布。
*/
IR_FILE_API INT32 ir_file_get_color_dist(HANDLE h, ir_file_enum_color_dist_type* value);

/** 写色彩分布类型
* @param h      [in] IR_FILE句柄。
* @param value  [in] 色彩分布类型。
*/
IR_FILE_API INT32 ir_file_set_color_dist(HANDLE h, ir_file_enum_color_dist_type value);

/** 图像增强类型 */
typedef enum 
{
    IR_FILE_NONE = 0,               /**< 不启用 */
    IR_FILE_IR_EDGE = 1,            /**< 图像细节增强(IREdge) */
} ir_file_enum_image_enh_type;

/** 读图像增强类型
 * @param h      [in] IR_FILE句柄。
 * @param value  [out] 图像增强类型，默认为不启用。
 */
IR_FILE_API INT32 ir_file_get_image_enh(HANDLE h, ir_file_enum_image_enh_type* value);

/** 写图像增强类型
 * @param h      [in] IR_FILE句柄。
 * @param value  [in] 图像增强类型。
 */
IR_FILE_API INT32 ir_file_set_image_enh(HANDLE h, ir_file_enum_image_enh_type value);

/** 旋转方式 */
typedef enum 
{
    IR_FILE_ROTATE_90 = 1,          /**< 顺时针旋转90° */
    IR_FILE_ROTATE_180 = 2,         /**< 顺时针旋转180° */
    IR_FILE_ROTATE_270 = 3,         /**< 顺时针旋转270° */
} ir_file_enum_rotate_type;

/** 旋转图像
* @param h      [in] IR_FILE句柄。
* @param value  [in] 旋转方式。
*/
IR_FILE_API INT32 ir_file_set_rotation(HANDLE h, ir_file_enum_rotate_type value);

/** 镜像方式 */
typedef enum 
{
    IR_FILE_MIRROR_HORIZONTAL = 1,  /**< 左右镜像 */
    IR_FILE_MIRROR_VERTICAL = 2,    /**< 上下镜像 */
} ir_file_enum_mirror_type;

/** 镜像翻转图像
* @param h      [in] IR_FILE句柄。
* @param value  [in] 镜像方式。
*/
IR_FILE_API INT32 ir_file_set_mirror(HANDLE h, ir_file_enum_mirror_type value);


/** 读图像ISP类型
 * @param h      [in] IR_FILE句柄。
 * @param value  [out] 图像ISP类型，默认为NEMO。
 */
IR_FILE_API INT32 ir_file_get_isp_type(HANDLE h, ir_file_enum_isp_type* value);

/** 写图像ISP类型
 * @param h      [in] IR_FILE句柄。
 * @param value  [in] 图像ISP类型。
 */
IR_FILE_API INT32 ir_file_set_isp_type(HANDLE h, ir_file_enum_isp_type value);


/** 读总帧数
* @param h      [in] IR_FILE句柄。
* @param value  [out] 总帧数。
*/
IR_FILE_API INT32 ir_file_get_frame_count(HANDLE h, UINT32* value);

/** 读帧频
* @param h      [in] IR_FILE句柄。
* @param value  [out] 帧频。
*/
IR_FILE_API INT32 ir_file_get_frame_rate(HANDLE h, float* value);

/** 写帧频
* @param h      [in] IR_FILE句柄。
* @param value  [in] 帧频。
*/
IR_FILE_API INT32 ir_file_set_frame_rate(HANDLE h, float value);

/** 读帧内压缩方式
* @param h      [in] IR_FILE句柄。
* @param value  [out] 帧内压缩方式。0x00表示未使用压缩算法，默认值为0x00。
*/
IR_FILE_API INT32 ir_file_get_compress_type(HANDLE h, BYTE* value);

/** 写帧内压缩方式
* @param h      [in] IR_FILE句柄。
* @param value  [in] 帧内压缩方式。
*/
IR_FILE_API INT32 ir_file_set_compress_type(HANDLE h, BYTE value);

/** 全辐射数据帧 */
typedef struct 
{
    UINT64 timeStamp;               /**< 拍摄时间，采用UTC时间，从1970-01-01 00:00:00.000起的毫秒数 */
    UINT32 length;                  /**< AD值数组大小，通常为图像宽度*图像高度 */
    USHORT* buffer;                 /**< AD值数组首地址 */
} ir_file_frame;

/** 获取全辐射数据帧
* @param h      [in] IR_FILE句柄。
* @param index  [in] 帧索引。
* @param value  [in] 全辐射数据帧。buffer需要用户分配内存，若buffer为空，不读
*                    取全辐射数据帧，length返回buffer数组所需大小。
*/
IR_FILE_API INT32 ir_file_get_frame(HANDLE h, UINT32 index, ir_file_frame* value);

/** 追加全辐射数据帧
* @param h      [in] IR_FILE句柄。
* @param value  [in] 全辐射数据帧。若buffer为空，写入失败。若value为空，清空帧
*                    数据区。
*/
IR_FILE_API INT32 ir_file_append_frame(HANDLE h, const ir_file_frame* value);

/** 温度点 */
typedef struct
{
    ir_file_point point;            /**< 点坐标 */
    BYTE localParams;               /**< 是否启用局部参数 */
    float reflTemp;                 /**< 反射温度 */
    float distance;                 /**< 目标距离 */
    float emissivity;               /**< 发射率 */
    float temp;                     /**< 温度值
                                     *   不启用参考温度时表示实际温度
                                     *   启用参考温度时表示实际温度与参考温度的差值 */
} ir_file_temp_point;

/** 读温度点
* @param h      [in] IR_FILE句柄。
* @param value  [in|out] 温度点，temp为输出项，其它为输入项。
*/
IR_FILE_API INT32 ir_file_get_temp_spot(HANDLE h, ir_file_temp_point* value);

/** 温度区域-矩形 */
typedef struct
{
    ir_file_rectangle rect;         /**< 矩形范围 */
    BYTE localParams;               /**< 是否启用局部参数 */
    float reflTemp;                 /**< 反射温度 */
    float distance;                 /**< 目标距离 */
    float emissivity;               /**< 发射率 */
    UINT32 tempCount;               /**< 区域温度数组大小 */
    ir_file_point_value* temps; /**< 区域温度点数组首地址，需要用户分配内存
                                     *   不启用参考温度时表示实际温度
                                     *   启用参考温度时表示实际温度与参考温度的差值 */
} ir_file_temp_rect;

/** 读矩形区域温度
* @param h      [in] IR_FILE句柄。
* @param value  [in|out] 温度区域，tempCount、temps为输出项，其它为输入项
*                        temps为NULL时获取tempCount。
*/
IR_FILE_API INT32 ir_file_get_temp_rect(HANDLE h, ir_file_temp_rect* value);

/** 读椭圆区域温度
* @param h      [in] IR_FILE句柄。
* @param value  [in|out] 温度区域，tempCount、temps为输出项，其它为输入项
*                        temps为NULL时获取tempCount。
*/
IR_FILE_API INT32 ir_file_get_temp_ellipse(HANDLE h, ir_file_temp_rect* value);

 /** 温度区域-折线,多边形 */
typedef struct
{
    UINT32 pointCount;      /**< 点数量 */
    ir_file_point* points;  /**< 点列表 */
    BYTE localParams;       /**< 是否启用局部参数 */
    float reflTemp;         /**< 反射温度 */
    float distance;         /**< 目标距离 */
    float emissivity;       /**< 发射率 */
    UINT32 tempCount;       /**< 温度数组大小 */
    ir_file_point_value* temps; /**< 温度点数组首地址，需要用户分配内存
                             *   不启用参考温度时表示实际温度
                             *   启用参考温度时表示实际温度与参考温度的差值 */
} ir_file_temp_points;

/** 读折线温度
* @param h      [in] IR_FILE句柄。
* @param value  [in|out] 温度线，tempCount、temps为输出项，其它为输入项
*                        temps为NULL时获取tempCount。
*/
IR_FILE_API INT32 ir_file_get_temp_polyline(HANDLE h, ir_file_temp_points* value);

/** 读多边形温度
* @param h      [in] IR_FILE句柄。
* @param value  [in|out] 温度区域，tempCount、temps为输出项，其它为输入项
                         temps为NULL时获取tempCount。
*/
IR_FILE_API INT32 ir_file_get_temp_polygon(HANDLE h, ir_file_temp_points* value);

/** 图片 */
typedef struct
{
    UINT32 offset;                  /**< 行字节对齐偏移 */
    BYTE* data;                     /**< 数据区首地址 */
}ir_file_image;

/** 读调色板色带
* @param h      [in] IR_FILE句柄。
* @param value  [out] 调色板色带，256行1列BGR。
*/
IR_FILE_API INT32 ir_file_get_palette_image(HANDLE h, ir_file_image* value);

/** 预置调色板 */
typedef struct
{
    BYTE refNo;                     /**< 调色板编号 */
    BYTE name[32];                  /**< 调色板名 */
} ir_file_preset_palette;

/** 预置调色板列表 */
typedef struct
{
    UINT32 count;                    /**< 预置调色板数量 */
    ir_file_preset_palette* palettes;/**< 预置调色板数组首地址 */
} ir_file_preset_palettes;

/** 读预置调色板
* @param value  [out] 预置调色板。
*/
IR_FILE_API INT32 ir_file_get_preset_palettes(ir_file_preset_palettes* value);

/** 读预置调色板色带
* @param refNo  [in] 调色板编号。
* @param value  [out] 预置调色板色带，256行1列BGR。
*/
IR_FILE_API INT32 ir_file_get_preset_palette_image(BYTE refNo, ir_file_image* value);

/** 读热像图
* @param h      [in] IR_FILE句柄。
* @param value  [out] 处理后的热像图，图像高度*图像宽度大小的BGR。
*/
IR_FILE_API INT32 ir_file_get_thermal_image(HANDLE h, ir_file_image* value);

/** 颜色空间 */
typedef enum {
    IR_FILE_BGR = 1, /**< BGR */
    IR_FILE_RGBA = 2, /**< RGBA */
} ir_file_enum_color_space_type;

/** 读热像图 V2
* @param h              [in] IR_FILE句柄。
* @param type           [in] 颜色空间。
* @param value          [out] 处理后的热像图。
*/
IR_FILE_API INT32 ir_file_get_thermal_image_v2(HANDLE h, ir_file_enum_color_space_type type, ir_file_image* value);

/** 从全辐射数据帧生产热像图
 * @param h             [in] IR_FILE句柄。
 * @param frame         [in] 全辐射数据帧。
 * @param type          [in] 颜色空间。
 * @param value         [out] 处理后的热像图。
 */
IR_FILE_API INT32 ir_file_get_thermal_image_from_frame(HANDLE h, const ir_file_frame* frame, ir_file_enum_color_space_type type, ir_file_image* value);

/** 读包含alpha通道的热像图
 * @param h      [in] IR_FILE句柄。
 * @param value  [out] 处理后的热像图，图像高度*图像宽度大小的BGRA。
 */
IR_FILE_API INT32 ir_file_get_thermal_image_bgra(HANDLE h, ir_file_image* value);

/** 读灰度图
* @param h      [in] IR_FILE句柄。
* @param value  [out] 处理后的灰度图，图像高度*图像宽度大小的灰度图。
*/
IR_FILE_API INT32 ir_file_get_gray_image(HANDLE h, ir_file_image* value);

/** 获取当前全辐射数据帧索引
* @param h      [in] IR_FILE句柄。
* @param value  [out] 帧索引，取值范围0到总帧数-1。
*/
IR_FILE_API INT32 ir_file_get_frame_index(HANDLE h, UINT32* value);

/** 设置当前全辐射数据帧索引
* @param h      [in] IR_FILE句柄。
* @param value  [in] 帧索引，取值范围0到总帧数-1。
*/
IR_FILE_API INT32 ir_file_set_frame_index(HANDLE h, UINT32 value);

/** 热像文件当前帧另存为
* @param h              [in] IR_FILE句柄。
* @param file_name      [in] 文件路径。
* @param cover_image    [in] 封面图，jpg文件，单边最小尺寸240像素。
*                            cover_image为空时使用原始文件封面图，如果原始文件封面图不存在，则生成黑色封面图
*/
IR_FILE_API INT32 ir_file_save_frame_as(HANDLE h, const char* file_name, ir_file_buffer* cover_image);

/** 创建视频裁剪句柄
* @param h              [in] IR_FILE句柄。
* @param file_name      [in] 文件路径。
* @param stream_handle  [out] 视频裁剪句柄。
*/
IR_FILE_API INT32 ir_file_save_as_stream(HANDLE h, const char* file_name, HANDLE* h_new);

/** 读指定帧的测量标识
* @param h      [in] IR_FILE句柄。
* @param index	[in] 帧索引。
* @param value  [in|out] 测量标识，name为输入项，其它为输出项。points需要用户分
*                        配内存（ir_file_point[pointCount]），若为空，不读取点
*                        列表，pointCount返回点列表长度。
* @retval IR_FILE_EC_INVALID_KEY 该测量标识不存在。
*/
IR_FILE_API INT32 ir_file_get_index_marker(HANDLE h, UINT32 index, ir_file_marker* value);

/** 读指定帧的温差标识
* @param h      [in] IR_FILE句柄。
* @param index	[in] 帧索引。
* @param value  [in|out] 温差标识，name为输入项，其它为输出项。
* @retval IR_FILE_EC_INVALID_KEY 该测量标识不存在。
*/
IR_FILE_API INT32 ir_file_get_index_delta(HANDLE h, UINT32 index, ir_file_delta* value);

/** 读指定帧的拍摄时间
* @param h      [in] IR_FILE句柄。
* @param index	[in] 帧索引。
* @param value  [out] 拍摄时间，采用UTC时间，从1970-01-01 00:00:00.000起的毫秒数。
*/
IR_FILE_API INT32 ir_file_get_index_shot_time(HANDLE h, UINT32 index, UINT64* value);

#pragma pack(pop)
#endif