/*
 * IRtekFileSDK 3.0
 * Copyright 2021, fotric.cn
 *
 * library for IRTek file format
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

typedef void*               HANDLE; /**< IR_FILE handle */
typedef unsigned char       BYTE;   /**< Byte or boolean value */
typedef short               SHORT;
typedef unsigned short      USHORT;
typedef int                 INT32;
typedef unsigned int        UINT32;
typedef unsigned long long  UINT64;

typedef enum
{
    IR_FILE_EC_OK = 0,              /**< Function called successfully */
    IR_FILE_EC_FAIL = -1,           /**< Function call failed */
    IR_FILE_EC_MEMORY = -2,         /**< Memory allocation failed */
    IR_FILE_EC_NULL_POINTER = -3,   /**< Null pointer */
    IR_FILE_EC_INVALID_KEY = -4,    /**< Index does not exist */
    IR_FILE_EC_NOT_SUPPORTED = -5   /**< Function not supported */
} ir_file_enum_error_code;

/** Spot*/
typedef struct
{
    SHORT x;                        /**< x coordinate*/
    SHORT y;                        /**< y coordinate*/
} ir_file_point;

/** Normalized float point coordinates*/
typedef struct
{
    float x;                        /**< Relative x coordinate */
    float y;                        /**< Relative y coordinate */
} ir_file_point_f;

/** Rectangle*/
typedef struct
{
    SHORT x;                        /**< x coordinate of up-left corner of the 
                                     *   rectangle */
    SHORT y;                        /**< y coordinate of up-left corner of the
                                     *   rectangle */
    SHORT width;                    /**< Length of the rectangle */
    SHORT height;                   /**< Width of the rectangle */
} ir_file_rectangle;

/** Normalized float rectangle*/
typedef struct
{
    float x;                        /**< Relative x coordinate of up-left 
                                     *   corner of the rectangle */
    float y;                        /**< Relative y coordinate of up-left
                                     *   corner of the rectangle */
    float width;                    /**< Relative length of the rectangle */
    float height;                   /**< Relative width of the rectangle */
} ir_file_rectangle_f;

/** Buffer */
typedef struct
{
    UINT32 length;                  /**< Buffer size */
    BYTE *buffer;                   /**< Memory address */
} ir_file_buffer;

/** SDK version
* @param ver    [out] SDK version, length: 4 bytes, written in HEX format 
                      (Ex:0x02040100 means 2.4.1.0), could be left as null
* @param str    [out] SDK version, in string format, could be left as null
*/
IR_FILE_API INT32 ir_file_get_sdk_version(UINT32 *ver, ir_file_buffer *str);

/** IR_FILE type */
typedef enum
{
    IR_FILE_OTHER = 0,              /**< Non-IR file */ 
    IR_FILE_THERMAL_PICTURE = 1,    /**< Thermal image */ 
    IR_FILE_THERMAL_VIDEO = 2,      /**< Thermal video */ 
    IR_FILE_VISUAL_PICTURE = 3,     /**< Visible-light image*/                                    
    IR_FILE_GRID_THERMAL_PICTURE = 4/**< Electric grid thermal images */ 
} ir_file_enum_file_type;

/** Check whether the file is IR file format
* @param file_name [in] File path
* @param value     [out] IR file type
*/
IR_FILE_API INT32 ir_file_peek_file_type(const char *file_name, ir_file_enum_file_type *value);

/** Opening IR file
* If the file does not exist, the action will fail
* @param file_name [in] File path
* @param h         [out] IR_FILE handle  
*/
IR_FILE_API INT32 ir_file_open(const char *file_name, HANDLE *h);

/** Create IR_FILE
* If the file already exist, it will be overwritten
* @param file_name [in] File path
* @param file_type [in] File tpye. Possible types include:
*                       IR_FILE_THERMAL_PICTURE，
*                       IR_FILE_THERMAL_VIDEO，
*                       IR_FILE_VISUAL_PICTURE。
* @param h         [out] IR_FILE handle 
*/
IR_FILE_API INT32 ir_file_create(const char *file_name, ir_file_enum_file_type file_type, HANDLE *h);

/** save IR_file
* @param  h [in] IR_FILE handle
*/
IR_FILE_API INT32 ir_file_save(HANDLE h);

/** IR_file save as
* @param  h         [in] IR_FILE handle
* @param file_name  [in] File path
*/
IR_FILE_API INT32 ir_file_save_as(HANDLE h, const char* file_name);

/** Save version 2.01 FJPG
* @param  h         [in] IR_FILE handle
*/
IR_FILE_API INT32 ir_file_save_fjpg(HANDLE h);

/** Save as version 2.01 FJPG
* @param  h         [in] IR_FILE handle
* @param file_name  [in] File path
*/
IR_FILE_API INT32 ir_file_save_as_fjpg(HANDLE h, const char* file_name);

/** Close IR_FILE */
IR_FILE_API INT32 ir_file_close(HANDLE h);

/** IR_file version
* @param h          [in]  IR_FILE handle
* @param value      [out] IR_FILE version
*/
IR_FILE_API INT32 ir_file_get_file_version(HANDLE h, USHORT* value);

/** Get IR_FILE name
* @param h      [in] IR_FILE handle
* @param value  [out] IR_FILE name. Buffer requires users to allocate memory, 
*                     if the buffer is empty, it wont load
*                     Note: 'length' returns memory size it requires
*/
IR_FILE_API INT32 ir_file_get_file_name(HANDLE h, ir_file_buffer *value);

/** Get IR_FILE type
* @param h      [in] IR_FILE handle
* @param value  [out] IR_FILE type
*/
IR_FILE_API INT32 ir_file_get_file_type(HANDLE h, ir_file_enum_file_type *value);

/** Device info */ 
typedef struct
{
    BYTE model[32];                 /**< Device model name */
    BYTE serialNo[48];              /**< Device serial number */
    BYTE brand[32];                 /**< Brand name */
    BYTE company[64];               /**< Company name */
    UINT32 logoLength;              /**< Size of the logo */
    BYTE *logo;                     /**< Logo image, jpg file, memeory address */
} ir_file_camera_info;

/** Get device info
* @param h      [in] IR_FILE handle
* @param value  [out] Device info, 'logo' requires user to allocate memory, if 
*                     'logo' is left empty, it won't load logo image
*                     'logoLength' will return the memory it requires
*/
IR_FILE_API INT32 ir_file_get_camera_info(HANDLE h, ir_file_camera_info *value);

/** Set device info
* @param h      [in] IR_FILE handle
* @param value  [in] Device info. If 'logo' is empty, the logo image won't load. 
*                    if 'value' is empty, the existing device info will be cleared
*/
IR_FILE_API INT32 ir_file_set_camera_info(HANDLE h, const ir_file_camera_info *value);

/** Lens info */
typedef struct
{
    BYTE name[32];                  /**< Lens name */
    float focalLength;              /**< Focal length of the lens, being '0.0f' 
                                     *   by default, means invalid parameter */
    float hfov;                     /**< Field of view of the lens, being '0.0f'
                                     *   by default, means invalid parameter */
} ir_file_lens_info;

/** Get lens info
* @param h      [in] IR_FILE handle 
* @param value  [out] Lens info
*/
IR_FILE_API INT32 ir_file_get_lens_info(HANDLE h, ir_file_lens_info *value);

/** Set lens info
* @param h      [in] IR_FILE handle
* @param value  [in] Lens info. If value is empty, will clear existing Lens info
*/
IR_FILE_API INT32 ir_file_set_lens_info(HANDLE h, const ir_file_lens_info *value);

/** Measurement range info */
typedef struct
{
    float max;                      /**< Upper limit of measurement range, 
                                     *   ranging from 0 to 5750, default being
                                     *   423.15 */
    float min;                      /**< Lower limit of measurement range, 
                                     *   ranging from 0 to 5750, default being
                                     *   253.15 */
} ir_file_temp_range;

/** Get measurement range info
* @param h      [in] IR_FILE handle
* @param value  [out] Measurement range info
*/
IR_FILE_API INT32 ir_file_get_temp_range(HANDLE h, ir_file_temp_range *value);

/** Set measurement range info
* @param h      [in] IR_FILE handle
* @param value  [in] Measurement range info. if value is empty, will clear
*                    existing measurement range info
*/
IR_FILE_API INT32 ir_file_set_temp_range(HANDLE h, const ir_file_temp_range *value);

/** Temperature look-up table (LUT) */
typedef struct
{
    BYTE radMethod;                 /**< Temperature compensation method version,
                                     *   0x00 means temp compensation method 1.0
                                     *   0x01 means temperature compensation 
                                     *   method 2.0; Default being 0x01 */
    UINT32 count;                   /**< Default LUT length*/
    USHORT *adArray;                /**< Default LUT's AD value table, a USHORT
                                     *   array */
    float *tempArray;               /**< Default LUT's temperature value table,
                                     *   a float array */
} ir_file_lut;

/** Get temperature look-up table
* @param h      [in] IR_FILE handle
* @param value  [out] Temperature look-up table. 'adArray' requires users to 
*                     allocate memory (USHORT[count]) 'tempArray' requires users
*                     to allocate memory (float[count]). If 'adArray' or
*                     'tempArray' is empty, it won't read, 'count' will return
*                     length of LUT
*/
IR_FILE_API INT32 ir_file_get_lut(HANDLE h, ir_file_lut *value);

/** Set LUT
* @param h      [in] IR_FILE handle
* @param value  [in] LUT. If either 'adArray' or 'tempArray' is empty, the
*                    setting will fail. If value is empty, will clear the
*                    existing LUT
*/
IR_FILE_API INT32 ir_file_set_lut(HANDLE h, const ir_file_lut *value);

/** Palette */
typedef struct
{
    BYTE refNo;                     /**< Palette serial number, 0xFF means using
                                     *   customized palette */
    UINT32 colors[256];             /**< Palette, each 8 bytes of the first 24
                                     *   bytes in UINT32 represent R/G/B 
                                     *   respectively, the last 8 bytes is unused */
    BYTE inverted;                  /**< Indicating whether the palette is 
                                     *   inverted, 0x01 means it’s turned on
                                     *   default being 0x00 */
} ir_file_palette;

/** Get palette info
* @param h      [in] IR_FILE handle
* @param value  [out] Palette info
*/
IR_FILE_API INT32 ir_file_get_palette(HANDLE h, ir_file_palette *value);

/** Palette */
typedef struct
{
    BYTE refNo;                     /**< Palette serial number, 0xFF means using
                                     *   customized palette */
    UINT32 src_colors[256];         /**< Palette source data , each 8 bytes of the first 24
                                     *   bytes in UINT32 represent R/G/B 
                                     *   respectively, the last 8 bytes is unused */
    UINT32 dest_colors[256];        /**< Processed palette data */
    BYTE inverted;                  /**< Indicating whether the palette is 
                                     *   inverted, 0x01 means it’s turned on
                                     *   default being 0x00 */
} ir_file_palette_v2;

/** get palette info v2
* @param h      [in] IR_FILE handle。
* @param value  [out] Palette info
*/
IR_FILE_API INT32 ir_file_get_palette_v2(HANDLE h, ir_file_palette_v2* value);

/** Set palette info
* @param h      [in] IR_FILE handle
* @param value  [in] Palette info, if value is empty, clear existing palette info 
*/
IR_FILE_API INT32 ir_file_set_palette(HANDLE h, const ir_file_palette *value);

/** Image resolution */
typedef struct
{
    SHORT width;                    /**< Width */
    SHORT height;                   /**< Height */
} ir_file_size;

/** Get image resolution
* @param h      [in] IR_FILE handle
* @param size   [out] Image resolution
*/
IR_FILE_API INT32 ir_file_get_thermal_size(HANDLE h, ir_file_size *value);

/** Set image resolution
* @param h      [in] IR_FILE handle
* @param size   [in] Image resolution, if value is empty, clear existing image
*                    resolution
*/
IR_FILE_API INT32 ir_file_set_thermal_size(HANDLE h, const ir_file_size *value);

/** Measurement parameters */
typedef struct
{
    float reflTemp;                 /**< Reflected temperature, set 293.15 by
                                     *   default 293.15 */
    float atmTemp;                  /**< Ambient temperature, set 293.15 by 
                                     *   default 293.15 */
    float distance;                 /**< Distance from the measured object, 
                                     *   set 1.0 by default*/
    float relHumidity;              /**< Relative humidity of surrounding 
                                     *   environment, set 0.5 by default 0.5 */
    float emissivity;               /**< Emissivity of the measured object, 
                                     *   set 1.0 by default*/
    float extOptTemp;               /**< External optical’s temperature (IR 
                                     *   window’s temperature), set as 293.15
                                     *   by default 293.15 */
    float extOptTrans;              /**< External optical’s transmittance rate
                                     *   (IR window’s transmittance rate), set
                                     *   as 1.0 by default */
} ir_file_thermal_params;

/** Get measurement parameters
* @param h      [in] IR_FILE handle
* @param value  [out] Measurement parameters
*/
IR_FILE_API INT32 ir_file_get_thermal_params(HANDLE h, ir_file_thermal_params *value);

/** Set measurement parameters
* @param h      [in] IR_FILE handle
* @paramvalue   [in] Measurement parameter, if value is left empty, will clear
*                    existing measurement parameters
*/
IR_FILE_API INT32 ir_file_set_thermal_params(HANDLE h, const ir_file_thermal_params *value);

/** Temperature measurement type */
typedef enum
{
    IR_FILE_MAX = 1,                /**< Maximum temperature */
    IR_FILE_MIN = 2,                /**< Minimum temperature */
    IR_FILE_AVG = 3,                /**< Average temperature */
} ir_file_enum_temp_value_type;

/** Reference temperature */
typedef struct
{
    BYTE enabled;                   /**< Whether or not activate reference
                                     *   temperature, 0x01 means yes. Default 
                                     *   being 0x00. */
    BYTE name[32];                  /**< Reference name, if left empty, will
                                     *   autofill ‘temp' */
    ir_file_enum_temp_value_type valueType;/**< Reference temperature type, 
                                     *   default being the ‘max’ of the 
                                     *   selected region */
    float temp;                     /**< Reference temperature, could range
                                     *   from 0~5750 K,default being 293.15K */
} ir_file_reference;

/** Get reference temperature
* @param h      [in] IR_FILE handle
* @param value  [out] Reference temperature info
*/
IR_FILE_API INT32 ir_file_get_reference(HANDLE h, ir_file_reference *value);

/** Get reference temperature
* @param h      [in] IR_FILE句柄。
* @param value  [out] reference temperature
*/
IR_FILE_API INT32 ir_file_get_reference_temp(HANDLE h, float* value);

/** Set reference temperature
* @param h      [in] IR_FILE handle
* @param value  [in] Reference temperature info
*/
IR_FILE_API INT32 ir_file_set_reference(HANDLE h, const ir_file_reference *value);

/** Temperature scale mode */
typedef enum
{
    IR_FILE_FIXED = 1,              /**< Fixed temperature span*/
    IR_FILE_AUTO = 2,               /**< Automatic temperature span*/
    IR_FILE_TOUCH = 3,              /**< Touch screen temperature span*/
} ir_file_enum_gain_mode;

/** Temperature scale info*/
typedef struct
{
    ir_file_enum_gain_mode mode;    /**< Temperature span mode, auto-span by
                                     *   default */
    float max;                      /**< Maximum of the temperature span, 
                                     *   ranging from 0~5750 */
    float min;                      /**< Minimum of the temperature span, 
                                     *   ranging from 0~5750 */
    BYTE fixedSpan;                 /**< Whether to fix the temperature span, 
                                     *   0x01 means yes. Default being 0x00 */
    float span;                     /**< Temperature span width (ranging from 
                                     *   0.1~5750), 2°C by default 2.0 */
} ir_file_gain;

/** Get temperature scale info 
* @param h      [in] IR_FILE handle
* @param value  [out] Temperature scale info
*/
IR_FILE_API INT32 ir_file_get_gain(HANDLE h, ir_file_gain *value);

/** Set temperature scale info
* @param h      [in] IR_FILE handle
* @param value  [in|out] Temperature span info, if value is left empty, will
*                        clear the exisitng temperature span info. If the 
*                        temperature span mode is automatic temperature span, 
*                        after setting the temperature span, the ‘max’ and 
*                        ‘min’ will be automatically updated
*/
IR_FILE_API INT32 ir_file_set_gain(HANDLE h, ir_file_gain *value);

/** Alarm type*/
typedef enum
{
    IR_FILE_NORMAL = 0,             /**< Non-activate */
    IR_FILE_BELOW = 1,              /**< Trigger alarm when there are points 
                                     *   whose temperature are below certain
                                     *   threshold */
    IR_FILE_ABOVE = 2,              /**< Trigger alarm when there are points 
                                     *   whose temperature are above certain 
                                     *   threshold */
    IR_FILE_BETWEEN = 3,            /**< Trigger alarm when there are points 
                                     *   whose temperature are between certain
                                     *   thresholds */
    IR_FILE_MAGIC_THERMAL = 4,      /**< Alarm function with MagicThermal feature */
    IR_FILE_OUTSIDE = 5,            /**< Trigger alarm when there are points
                                     *   whose temperature are NOT between
                                     *   certain thresholds */
    IR_FILE_HUMIDITY = 6,           /**< Trigger alarm when there are points
                                     *   whose humidity exceed certain thresholds */
    IR_FILE_INSULATION = 7,         /**< Insulation alarm */
} ir_file_enum_alarm_type;

/** Alarm info*/
typedef struct
{
    ir_file_enum_alarm_type type;   /**<Alarm type, non-activate by default */
    BYTE custom;                    /**< Whether or not activate custom alarm, 
                                     *   non-applicable under 'non-active' or
                                     *   'Magicthermal alarm'
                                     *   0x01 means active, default being 0x00 */
    UINT32 color;                   /**< Alarm color, default ARBG value being
                                     *   (127,255,0,0) */
    float max;                      /**< For 'above' type, trigger when higher
                                     *   than this threshold
                                     *   For 'between' type, trigger when lower
                                     *   than this threshold
                                     *   For 'outside' type, trigger when higher
                                     *   than this threshold
                                     *   Non-applicable for other alarm types
                                     *   Ranging from 0 to 5750, being 313.15K 
                                     *   by default */
    float min;                      /**< For 'below' type, trigger when lower
                                     *   than this threshold
                                     *   For 'between' type, trigger when higher
                                     *   than this threshold
                                     *   For 'outside' type, trigger when lower 
                                     *   than this threshold
                                     *   Non-applicable for other alarm types
                                     *   Ranging from 0 to 5750, being 313.15K 
                                     *   by default */
    float rhLimit;                  /**< Relative humidity limit, for humidity 
                                     *   alarm type
                                     *   Non-applicable for other alarm types
                                     *   Ranging from 0.01 to 1.0, being 1.0 by
                                     *   default */
    float indoorTemp;               /**< Indoor temperature, for insulation 
                                     *   alarm type
                                     *   Non-applicable for other alarm types
                                     *   Ranging from 0 to 5750, being 293.15K
                                     *   by default */
    float outdoorTemp;              /**< Outdoor temperature, for insulation 
                                     *   alarm type
                                     *   Non-applicable for other alarm types
                                     *   Ranging from 0 to 5750, being 283.15K
                                     *   by default  */
    float heatIndex;                /**< Heat index, for insulation alarm type
                                     *   Non-applicable for other alarm types
                                     *   Ranging from 0 to 1.0, being 0.8 by 
                                     *   default*/
} ir_file_alarm;

/** Get alarm info
* @param h      [in] IR_FILE handle
* @param value  [out] Alarm info
*/
IR_FILE_API INT32 ir_file_get_alarm(HANDLE h, ir_file_alarm *value);

/** Set alarm info
* @param h      [in] IR_FILE handle
* @param value  [in] Alarm info, if value is empty, will clear existing alarm info
*/
IR_FILE_API INT32 ir_file_set_alarm(HANDLE h, const ir_file_alarm *value);

/** Geographical info*/
typedef struct
{
    BYTE isGPSValid;                /**< The marker indicating  'GPS information
                                     *   is valid'. 0x01 means activated Default
                                     *   being 0x00 */
    double longitude;               /**< longitude */
    double latitude;                /**< latitude */
    double altitude;                /**< altitude */
    BYTE isCompassValid;            /**< The marker indicating ‘Compass
                                     *   information is valid’, 0x01 means
                                     *  ‘activated. Default being 0x00 */
    SHORT direction;                /**< Direction, represented by -180~180
                                     *   degree, +/- 180 means south
                                     *   0 means north, -90 means west, +90
                                     *   means east, any value beyong -180~180
                                     *   is meaningless */
} ir_file_geo_location;

/** Get geographic location info
* @param h      [in] IR_FILE handle
* @param value  [out] Geographic location info
*/
IR_FILE_API INT32 ir_file_get_geo_location(HANDLE h, ir_file_geo_location *value);

/** Set geographic location info
* @param h      [in] IR_FILE handle
* @param value  [in] Geographic location info. If value is empty, clear existing
*                    geographic location info
*/
IR_FILE_API INT32 ir_file_set_geo_location(HANDLE h, const ir_file_geo_location *value);

/** Get text annotation info 
* @param h      [in] IR_FILE handle
* @param value  [out] Text annotation info,buffer requires users to allocate
*                     memory, if the buffer is empty, it wont load
*                     Note: 'length' returns memory size it requires
*/
IR_FILE_API INT32 ir_file_get_note(HANDLE h, ir_file_buffer *value);

/** Set text annotation info
* @param h      [in] IR_FILE handle
* @param value  [in] Text annotation info, if the buffer is empty, it wont load; 
*                    If value is empty, clear existing text annotation info
*                    
*/
IR_FILE_API INT32 ir_file_set_note(HANDLE h, const ir_file_buffer *value);

/** Voice annotation format*/
typedef enum
{
    IR_FILE_WAV = 1,                /**< WAV */
    IR_FILE_MP3 = 2,                /**< MP3 */
} ir_file_enum_audio_type;

/** Audio file*/
typedef struct
{
    ir_file_enum_audio_type type;   /**< Audio file format, being MP3 by default */
    UINT32 length;                  /**< Size of the audio file */
    BYTE *buffer;                   /**< Memory address */
} ir_file_audio;

/** Get audio file
* @param h      [in] IR_FILE handle
* @param value  [out] Audio file. Buffer require users to reallocate memories,
*                     if buffer is empty, the image would not load. Length will
*                     return size of memory it requires
*/
IR_FILE_API INT32 ir_file_get_audio(HANDLE h, ir_file_audio *value);

/** Set audio file
* @param h      [in] IR_FILE handle
* @param value  [in] Text annotation info, if the buffer is empty, it wont load; 
*                    If value is empty, clear existing audio file
*/
IR_FILE_API INT32 ir_file_set_audio(HANDLE h, const ir_file_audio *value);

/** User data item */
typedef BYTE ir_file_key[16];

/** User data item list */
typedef struct
{
    UINT32 count;                   /**< Number of user data item */
    ir_file_key *keys;              /**< Address of user data item */
} ir_file_keys;

/** Get user data item list
* @param h      [in] IR_FILE handle
* @param value  [out] User data item list
*/
IR_FILE_API INT32 ir_file_get_keys(HANDLE h, ir_file_keys *value);

/** Item and value pair */
typedef struct
{
    ir_file_key key;                /**< Data item */
    UINT32 vLength;                 /**< Buffer size */
    BYTE *vBuffer;                  /**< Memory address */
} ir_file_kvPair;

/** Get user data item list
* @param h      [in] IR_FILE handle
* @param value  [in|out] User data items, 'key' is input, others are output.
*                        'vBuffer' require users toallocate bytes of 'vLength'
*                        size, if left empty, it won't load; 'vLength' returns 
*                        length of bytes
* @retval IR_FILE_EC_INVALID_KEY The item does not exist
*/
IR_FILE_API INT32 ir_file_get_user_data(HANDLE h, ir_file_kvPair *value);

/** Set user data item list
* User data items can be set in the form of 'item and value pair'
* Here are some pre-determined pair
* 1. Favorite mark
* Key：“FAVORITE”
* Value：Favorite mark, length being 1 byte, 'Y' means yes
* 2. Tag mark
* Key：“TAG”
* Value：Tag mark, if there exist multiple tags, separate them with '\n'
* 3. Group files 
* Key：“GROUPFILES”
* Value：Bind different files in one group, if there exist multiple tags, 
* separate them with '\n'
* 4.Time lapse file name list
* Key：“TIMERFILE” 
* Value：When taking photos in time lapse mode, device will save GUID and photo
* number, GUID is recorded in the format of 24-A4C9-742A7A28EA25”，
* 'EDDCC222-97B3-4724-A4C9-742A7A28EA25', photos taken in same time lapse share 
* the same GUID. GUID and photo number is separated by'\n', photo number is in
* string format
* @param h      [in] IR_FILE handle
* @param value  [in] User data item. If 'vBuffer' is empty, delete the
*                    individual item; If 'value' is emtpy, delete the user data
*                    item list
*/
IR_FILE_API INT32 ir_file_set_user_data(HANDLE h, const ir_file_kvPair *value);

/** Get the time images/videos are taken
* @param h      [in] IR_FILE handle
* @param value  [out] Image/video taken time, in UTC format, number of 
*                     milliseconds since 1970-01-01 00:00:00.000
*/
IR_FILE_API INT32 ir_file_get_shot_time(HANDLE h, UINT64 *value);

/** Get the time images/videos are modified
* @param h      [in] IR_FILE handle
* @param value  [out] Image/video modified time, in UTC format, number of
*                     milliseconds since 1970-01-01 00:00:00.000
*/
IR_FILE_API INT32 ir_file_get_modified_time(HANDLE h, UINT64 *value);

/** Digital zoom's magnification */
typedef struct
{
    BYTE enabled;                   /**< Whether or not activate digital 
                                     *   magnification, 0x01 means yes Being 
                                     *   0x00 by default */
    ir_file_point_f location;       /**< The relative coordinates with respect
                                     *   to up-left corner, being (0.0, 0.0) by
                                     *   default. Maximum coordinate being 
                                     *   (1.0, 1.0) */
    float ratio;                    /**< Digital zoom magnification, being 1 by
                                     *   default */
} ir_file_zoom_info;

/** Get digital zoom's magnification
* @param h      [in] IR_FILE handle
* @param value  [out] Digital zoom magnification
*/
IR_FILE_API INT32 ir_file_get_zoom_info(HANDLE h, ir_file_zoom_info *value);

/** Set digital zoom's magnification
* @param h      [in] IR_FILE handle
* @param value  [out] Digital zoom magnification
*/
IR_FILE_API INT32 ir_file_set_zoom_info(HANDLE h, const ir_file_zoom_info *value);

/** Display mode */
typedef enum
{
    IR_FILE_THERMAL = 1,            /**< Thermal image mode */
    IR_FILE_PIC_IN_PIC = 2,         /**< Picture in picture mode */
    IR_FILE_BLEND = 3,              /**< Image blend mode */
    IR_FILE_FUSION = 4,             /**< Image fusion mode */
} ir_file_enum_thermal_view_type;

/** Display info */
typedef struct
{
    ir_file_enum_thermal_view_type type;/**< Display mode, being thermal image
                                     *   by default */
    float max;                      /**< Fusion upper temperature threshold, 
                                     *   meaningless under other modes, being
                                     *   313.15 by default */ 
    float min;                      /**< Fusion lower temperature threshold, 
                                     *   meaningless under other modes, being 
                                     *   293.15 by default */
    BYTE alpha;                     /**< Transparency, value between 0~100, 
                                     *   being 0 by default. Meaningless in any 
                                     *   mode besides blend mode */
    ir_file_rectangle_f area;       /**< Thermal image area in PIP, being 
                                     *   (0.25,0.25,0.5,0.5) by default
                                     *   Maximum being (0.0,0.0,1.0,1.0) */                   
} ir_file_view_info;

/** Get display info
* @param h      [in] IR_FILE handle
* @param value  [out] Display info
*/
IR_FILE_API INT32 ir_file_get_view_info(HANDLE h, ir_file_view_info *value);

/** Set display info
* @param h      [in] IR_FILE handle
* @param value  [in] Display info, if value is empty, clear existing display info
*/
IR_FILE_API INT32 ir_file_set_view_info(HANDLE h, const ir_file_view_info *value);

/** Fusion info*/
typedef struct
{
    ir_file_size visualSize;        /**< Visible light image's dimensions, being
                                     *   (800,600) by default*/
    ir_file_rectangle_f mappingArea;/**< Thermal image's projection's relative
                                     *   size with respect to visible light
                                     *   image, Maximum relative size is 
                                     *   (0.0,0.0,1.0,1.0). Default relative 
                                     *   size is (0.25,0.25,0.5,0.5) */
} ir_file_mapping_info;

/** Get fusion info
* @param h      [in] IR_FILE handle
* @param value  [out] Fusion info
*/
IR_FILE_API INT32 ir_file_get_mapping_info(HANDLE h, ir_file_mapping_info *value);

/** Set fusion info
* @param h      [in] IR_FILE handle
* @param value  [in]  Fusion info, if value is empty, clear existing fusion info
*/
IR_FILE_API INT32 ir_file_set_mapping_info(HANDLE h, const ir_file_mapping_info *value);

/** Measurement marker */
typedef BYTE ir_file_marker_name[32];

/** Measurement marker list */
typedef struct
{
    UINT32 count;                   /**< Number of markers */
    ir_file_marker_name *names;     /**< Marker's name */
} ir_file_marker_names;

/** Get measurement marker list
* @param h      [in] IR_FILE handle
* @param value  [out] Measurement marker list
*/
IR_FILE_API INT32 ir_file_get_marker_names(HANDLE h, ir_file_marker_names *value);

/** 测量标识类型 Measurement marker type*/
typedef enum
{
    IR_FILE_SPOT = 1,               /**< Spot */
    IR_FILE_RECTANGLE = 3,          /**< Rectangle */
    IR_FILE_ELLIPSE = 4,            /**< Ellipse */
    IR_FILE_POLYGON = 5,            /**< Polygon */
    IR_FILE_POLYLINE = 6,           /**< Polyline */
} ir_file_enum_marker_type;

/** Measurement marker*/
typedef struct
{
    ir_file_enum_marker_type type;  /**< Type of marker, being ‘spot’ by 
                                     *   default */
    ir_file_marker_name name;       /**< Name of the marker*/
    BYTE localParams;               /**< Whether or not to use local parameters, 
                                     *   0x01 means yes, being 0x00 by default */
    float reflTemp;                 /**< Reflected temperature, ranging from 
                                     *   0~5750, being 293.15K by default 293.15 */
    float distance;                 /**< Distance away from the measured object,
                                     *   ranging from 0~50000, being 1m by default */
    float emissivity;               /**< Emissivity, ranging from 0.01~1.0, 
                                     *   being 1 by default */
    UINT32 pointCount;              /**< Number of shape constructing points */
    ir_file_point *points;          /**< Coordinates of shape constructing points */
    float tempValues[3];            /**< [maximum temperature, minimum temperature,
                                     *   average temperature] 
                                     *   If enabled reference temperature: value = actual temperature - reference temperature
                                     *   If disabled reference temperature: value = actual temperature */
    ir_file_point tempSpots[2];     /**< [Maximum temp point, minimum temp point] */
    float measuredValue;            /**< Rectangle/ellipse/polygon’s area, or 
                                     *   polyline’s length */
} ir_file_marker;

/** Get measurement marker
* @param h      [in] IR_FILE handle
* @param value  [in|out] Measurement marker, name is input, the others are 
*                        outputs. 'Points' require users to allocate memory
*                        （ir_file_point[pointCount]）, if empty, it won't load 
*                        points pointCount returns the length of the list
* @retval IR_FILE_EC_INVALID_KEY The marker does not exist
*/
IR_FILE_API INT32 ir_file_get_marker(HANDLE h, ir_file_marker *value);

/** Set measurement marker
* @param h      [in] IR_FILE handle
* @param value  [in|out] Measurement markers,'tempValues','tempPos' and 'area'
*                        are outputs, the others are inputs. If the marker 
*                        already existed, then make modification, if not then
*                        addition. f 'points' is empty, the loading will fail. 
*                        All the output items will be updated
*/
IR_FILE_API INT32 ir_file_set_marker(HANDLE h, ir_file_marker *value);

/** Delete measurement marker
* @param h      [in] IR_FILE handle
* @param value  [in] Measurement marker name, if value is empty, clear the 
*                    measurement marker list
*/
IR_FILE_API INT32 ir_file_remove_marker(HANDLE h, const ir_file_marker_name value);

/** Rename measurement markers
* @param h      [in] IR_FILE handle
* @param from   [in] Old measurement marker name
* @param to     [in] New measurement marker name
*/
IR_FILE_API INT32 ir_file_rename_marker(HANDLE h, const ir_file_marker_name from
    , const ir_file_marker_name to);
/** Get digital image 
* @param h      [in] IR_FILE handle
* @param value  [out] Visual image, jpg file. buffer requires user to allocate
*                     memory, if buffer is empty the visual image cannot be 
*                     loaded; 'Length' returns size of the image
*/
IR_FILE_API INT32 ir_file_get_visual_image(HANDLE h, ir_file_buffer *value);

/** Set digital image 
* @param h      [in] IR_FILE handle
* @param value  [in] Visual image, jpg file. if buffer is empty, the loading 
*                    will fail. If value is empty clear the visual image
*/
IR_FILE_API INT32 ir_file_set_visual_image(HANDLE h, ir_file_buffer *value);

/** Get display cover of the image
* Cover image is the jpg part of the IR file, check the images using image
* viewing tools will display the cover image
* @param h      [in] IR_FILE handle
* @param value  [out] Cover image, in jpg format. Buffer require user to allocate
*                     memory, if buffer is empty, the cover image will not load,
*                     'length' returns size of the image
*/
IR_FILE_API INT32 ir_file_get_cover_image(HANDLE h, ir_file_buffer *value);

/** Set display cover of the image
* Cover image is the jpg part of the IR file, check the images using image
* viewing tools will display the cover image
* @param h      [in] IR_FILE handle
* @param value  [in] Cover image, in jpg format, minimum side length being 240
*                    pixels. If buffer is empty, loading will fail. If value is
*                    empty clear the cover image
*/
IR_FILE_API INT32 ir_file_set_cover_image(HANDLE h, const ir_file_buffer *value);

/** Get image params
* @param h      [in] IR_FILE handle
* @param value  [out] image params. Buffer require user to allocate
*                     memory, if buffer is empty, the image params will not get,
*                     'length' returns size of the image params
*/
IR_FILE_API INT32 ir_file_get_vt_params(HANDLE h, ir_file_buffer* value);

/** Set image params
* @param h      [in] IR_FILE handle
* @param value  [in] image params. If buffer is empty，set will fail。If value is
*                    empty clear the image params
*/
IR_FILE_API INT32 ir_file_set_vt_params(HANDLE h, const ir_file_buffer* value);

typedef BYTE ir_file_delta_name[32];

/** Tempetrature difference's list */
typedef struct
{
    UINT32 count;                   /**< Number of temp differences */
    ir_file_delta_name *names;      /**< Addresses of temp differences */
} ir_file_delta_names;

/** Get temperatures differences' list
* @param h      [in] IR_FILE handle
* @param value  [out] Temperatures differences' list
*/
IR_FILE_API INT32 ir_file_get_delta_names(HANDLE h, ir_file_delta_names *value);

/** Temperatures differences*/
typedef struct
{
    ir_file_delta_name name;        /**< Temp difference mark*/
    BYTE srcName[32];               /**< Name of reference temperature*/
    ir_file_enum_temp_value_type srcValueType;/**< Type of reference temperature,
                                     *   being ‘max’ by default */
    float srcTemp;                  /**< Value of reference temperature, being 
                                     *   293.15K by default  */
    BYTE destName[32];              /**< Target name*/
    ir_file_enum_temp_value_type destValueType;/**< Temperature type, being
                                     *   ‘max’ by default */
    float destTemp;                 /**< Target temperature, being 293.15K by 
                                     *   default  */
    float value;                    /**<  The value of temp difference*/
} ir_file_delta;

/** Get temperature difference
* @param h      [in] IR_FILE handle
* @param value  [in|out] Temperature difference mark, 'name' is input, the 
*                        others are outputs
* @retval IR_FILE_EC_INVALID_KEY The temp difference mark does not exist
*/
IR_FILE_API INT32 ir_file_get_delta(HANDLE h, ir_file_delta *value);

/** Set temperature difference
* @param h      [in] IR_FILE handle
* @param value  [in|out] Temperature difference mark, difference value is output,
*                        others are input. If temperature difference already
*                        exist, then make alteration, otherwise, addition.
*                        Once a temperature difference mark is successfully 
*                        added, the object's temperature difference structure
*                        will be updated
*/
IR_FILE_API INT32 ir_file_set_delta(HANDLE h, ir_file_delta *value);

/** Delete temperature difference
* @param h      [in] IR_FILE handle
* @param value  [in] Measurement marker's name, if value is empty, clear the 
*                    temperature difference marker list
*/
IR_FILE_API INT32 ir_file_remove_delta(HANDLE h, const ir_file_delta_name value);

/** Color distribution types */
typedef enum
{
    IR_FILE_HIST_EQUAL = 1,             /**< Normalized by histogram */
    IR_FILE_TEMP_LINEAR = 2,            /**< Temperature linear distribution */
    IR_FILE_AD_LINEAR = 3,              /**< Signal linear distribution */
} ir_file_enum_color_dist_type;

/** Get color distribution types
* @param h      [in] IR_FILE handle
* @param value  [out] Color distribution type, being linear distribution by
*                     default
*/
IR_FILE_API INT32 ir_file_get_color_dist(HANDLE h, ir_file_enum_color_dist_type *value);

/** Set color distribution types
* @param h      [in] IR_FILE handle
* @param value  [in] Color distribution type, being linear distribution by defaul
*/
IR_FILE_API INT32 ir_file_set_color_dist(HANDLE h, ir_file_enum_color_dist_type value);

/** Rotation options */
typedef enum
{
    IR_FILE_ROTATE_90 = 1,          /**< 90°clockwise */
    IR_FILE_ROTATE_180 = 2,         /**< 180°clockwise */
    IR_FILE_ROTATE_270 = 3,         /**< 270°clockwise */
} ir_file_enum_rotate_type;

/** Rotating images
* @param h      [in] IR_FILE handle
* @param value  [in] Rotation options
*/
IR_FILE_API INT32 ir_file_set_rotation(HANDLE h, ir_file_enum_rotate_type value);

/** Flipping options */
typedef enum
{
    IR_FILE_MIRROR_HORIZONTAL = 1,  /**< Flipping horizontally */
    IR_FILE_MIRROR_VERTICAL = 2,    /**< Flipping vertically */
} ir_file_enum_mirror_type;

/** Mirror Flip images
* @param h      [in] IR_FILE handle
* @param value  [in] Flipping options
*/
IR_FILE_API INT32 ir_file_set_mirror(HANDLE h, ir_file_enum_mirror_type value);

/** Get total frame count
* @param h      [in] IR_FILE handle
* @param value  [out] Total frame count
*/
IR_FILE_API INT32 ir_file_get_frame_count(HANDLE h, UINT32 *value);

/** Get frame rate
* @param h      [in] IR_FILE handle
* @param value  [out] Frame rate
*/
IR_FILE_API INT32 ir_file_get_frame_rate(HANDLE h, float *value);

/** Set frame rate
* @param h      [in] IR_FILE handle
* @param value  [in] Frame rate
*/
IR_FILE_API INT32 ir_file_set_frame_rate(HANDLE h, float value);

/** Get frame compression method
* @param h      [in] IR_FILE handle
* @param value  [out] File compression method, 0x00 means uncompressed.
*/
IR_FILE_API INT32 ir_file_get_compress_type(HANDLE h, BYTE *value);

/** Set frame compression method
* @param h      [in] IR_FILE handle
* @param value  [in] File compression method
*/
IR_FILE_API INT32 ir_file_set_compress_type(HANDLE h, BYTE value);

/** Radiometric data frames */
typedef struct
{
    UINT64 timeStamp;               /**< Number of milliseconds compared to the 
                                     *   shotTime */
    UINT32 length;                  /**< Size of AD value array */
    USHORT *buffer;                 /**< AD value address */
} ir_file_frame;

/** Get radiometric data frame
* @param h      [in] IR_FILE handle
* @param index  [in] Frame index
* @param value  [in] Radiometric data frame. Buffer require user to allocate
*                    memory, if buffer is empty, the radiometric data frame 
*                    will not load, 'length' returns size of the buffer array
*/
IR_FILE_API INT32 ir_file_get_frame(HANDLE h, UINT32 index, ir_file_frame* value);

/** Append radiometric data frame
* @param h      [in] IR_FILE handle
* @param value  [in] Radiometric data frame, if buffer is left empty, loading 
*                    will fail; clear all previously appended frame if value is
*                    left empty, clear all frame count
*/
IR_FILE_API INT32 ir_file_append_frame(HANDLE h, const ir_file_frame *value);

/** Point temperature */
typedef struct
{
    ir_file_point point;            /**< Coordinates of the point */
    BYTE localParams;               /**< Whether or not to deploy local parameters */
    float reflTemp;                 /**< Reflected temperature*/
    float distance;                 /**< Distance from the measured object */
    float emissivity;               /**< Emissivity of the measured object */
    float temp;                     /**< Temperature value  
                                     *   If enabled reference temperature: value = actual temperature - reference temperature
                                     *   If disabled reference temperature: value = actual temperature */
} ir_file_temp_point;

/** Get point temperature
* @param h      [in] IR_FILE handle
* @param value  [in|out] Point temperature, 'temp' is output, others are input
*/
IR_FILE_API INT32 ir_file_get_temp_spot(HANDLE h, ir_file_temp_point *value);

/** Area temperature -- rect*/
typedef struct
{
    ir_file_rectangle rect;         /**< Area and position of the rectangle */
    BYTE localParams;               /**< Whether or not to deploy local
                                     *   parameters */
    float reflTemp;                 /**< Reflected temperature*/
    float distance;                 /**< Distance from the measured object */
    float emissivity;               /**< Emissivity of the measured object */
    UINT32 tempCount;               /**< Count of temperature value array */
    float *temps;                   /**< Temperature array of the area, need
                                     *   users to allocate memory to store them 
                                     *   If enabled reference temperature: value = actual temperature - reference temperature
                                     *   If disabled reference temperature: value = actual temperature */
} ir_file_temp_rect;

/** Get rect temperature
* @param h      [in] IR_FILE handle
* @param value  [in|out] rect temperatures, 'tempCount','temp' is output, others are input
*                        if temps is NULL, 'tempCount' returns count of the temperature array
*/
IR_FILE_API INT32 ir_file_get_temp_rect(HANDLE h, ir_file_temp_rect *value);

/** Get ellipse temperature
* @param h      [in] IR_FILE handle
* @param value  [in|out] ellipse temperatures, 'tempCount'、'temps' is output，others are input
*                        if temps is NULL, 'tempCount' returns count of the temperature array
*/
IR_FILE_API INT32 ir_file_get_temp_ellipse(HANDLE h, ir_file_temp_rect* value);

 /** Area temperature -- polyline, polygon*/
typedef struct
{
    UINT32 pointCount;      /**< Number of point */
    ir_file_point* points;  /**< Addresses of Point array */
    BYTE localParams;       /**< Whether or not to deploy local
                             *   parameters */
    float reflTemp;         /**< Reflected temperature*/
    float distance;         /**< Distance from the measured object */
    float emissivity;       /**< Emissivity of the measured object */
    UINT32 tempCount;       /**< Count of temperature value array */
    float* temps;           /**< Temperature array of the area, need
                             *   users to allocate memory to store them 
                             *   If enabled reference temperature: value = actual temperature - reference temperature
                             *   If disabled reference temperature: value = actual temperature */
} ir_file_temp_points;

/** Get polyline temperature
* @param h      [in] IR_FILE handle
* @param value  [in|out] polyline temperatures，'tempCount'、'temps' is output，others are input
*                        if temps is NULL, 'tempCount' returns count of the temperature array
*/
IR_FILE_API INT32 ir_file_get_temp_polyline(HANDLE h, ir_file_temp_points* value);

/** Get polygon temperature
* @param h      [in] IR_FILE handle
* @param value  [in|out] polygon temperatures，'tempCount'、'temps' is output，others are input
*                        if temps is NULL, 'tempCount' returns count of the temperature array
*/
IR_FILE_API INT32 ir_file_get_temp_polygon(HANDLE h, ir_file_temp_points* value);

/** Image data */
typedef struct
{
    UINT32 offset;                  /**< Memory alignment offset */
    BYTE *data;                     /**< Address of the data */
} ir_file_image;

/** Get palette ribbon
* @param h      [in] IR_FILE handle
* @param value  [out] Palette ribbon, 256 rows and 1 column of BGR data
*/
IR_FILE_API INT32 ir_file_get_palette_image(HANDLE h, ir_file_image *value);

/** Preset palette */
typedef struct
{
    BYTE refNo;                     /**< Palette’s reference number */
    BYTE name[32];                  /**< Palette’s name */
} ir_file_preset_palette;

/** Preset palette list*/
typedef struct
{
    UINT32 count;                   /**< Number of preset palettes */
    ir_file_preset_palette *palettes;/**< Address of preset palettes */
} ir_file_preset_palettes;

/** Get preset palettes
* @param value  [out] Preset palettes
*/
IR_FILE_API INT32 ir_file_get_preset_palettes(ir_file_preset_palettes *value);

/** Get preset palette ribbon
* @param refNo  [in] Preset palette’s reference number
* @param value  [out] Preset palette ribbon, 256 rows and 1 column of BGR data
*/
IR_FILE_API INT32 ir_file_get_preset_palette_image(BYTE refNo, ir_file_image* value);

/** Get the thermal image
* @param h      [in] IR_FILE handle
* @param value  [out] Processed thermal image, a BGR array with size of image
*                     height*width
*/
IR_FILE_API INT32 ir_file_get_thermal_image(HANDLE h, ir_file_image* value);

/** get gray image
* @param h      [in] IR_FILE handle。
* @param value  [out] Processed gray image, a BGR array with size of image
*                     height*width
*/
IR_FILE_API INT32 ir_file_get_gray_image(HANDLE h, ir_file_image* value);

/** Get index of radiometric data frame
* @param h      [in] IR_FILE handle
* @param value  [out] Frame index，possible values are 0 to frameCount - 1
*/
IR_FILE_API INT32 ir_file_get_frame_index(HANDLE h, UINT32* value);

/** Set index of radiometric data frame
* @param h      [in] IR_FILE handle
* @param value  [in] Frame index，possible values are 0 to frameCount - 1
*/
IR_FILE_API INT32 ir_file_set_frame_index(HANDLE h, UINT32 value);

/** Current frame save as
* @param h              [in] IR_FILE handle
* @param file_name      [in] File path
* @param cover_image    [in] cover image，jpg file
*/
IR_FILE_API INT32 ir_file_save_frame_as(HANDLE h, const char* file_name, ir_file_buffer* cover_image);

/** creat new stream hander
* @param h              [in] IR_FILE handle。
* @param file_name      [in] file path
* @param stream_handle  [out] new stream hander
*/
IR_FILE_API INT32 ir_file_save_as_stream(HANDLE h, const char* file_name, HANDLE* h_new);

/** Get the index frame measurement marker
* @param h      [in] IR_FILE handle
* @param index	[in] Frame index
* @param value  [in|out] Measurement marker, name is input, the others are 
*                        outputs. 'Points' require users to allocate memory
*                        (ir_file_point[pointCount]), if empty, it won't load 
*                        points pointCount returns the length of the list
* @retval IR_FILE_EC_INVALID_KEY The marker does not exist
*/
IR_FILE_API INT32 ir_file_get_index_marker(HANDLE h, UINT32 index, ir_file_marker* value);

/** Get the index frame temperature difference
* @param h      [in] IR_FILE handle
* @param index	[in] Frame index
* @param value  [in|out] Temperature difference mark, 'name' is input, the 
*                        others are outputs
* @retval IR_FILE_EC_INVALID_KEY The temp difference mark does not exist
*/
IR_FILE_API INT32 ir_file_get_index_delta(HANDLE h, UINT32 index, ir_file_delta* value);

/** Get shot_time of the index frame
* @param h      [in] IR_FILE handle
* @param index	[in] Frame index
* @param value  [out] shot time, in UTC format, number of 
*                     milliseconds since 1970-01-01 00:00:00.000
*/
IR_FILE_API INT32 ir_file_get_index_shot_time(HANDLE h, UINT32 index, UINT64* value);

#pragma pack(pop)
#endif