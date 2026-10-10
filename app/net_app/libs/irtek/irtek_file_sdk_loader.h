#pragma once

#include <string>
#include <vector>

using IrFileHandle = void*;
using IrFileByte = unsigned char;
using IrFileShort = short;
using IrFileUInt = unsigned int;
using IrFileUShort = unsigned short;
using IrFileInt = int;
using IrFileULongLong = unsigned long long;

enum : IrFileInt
{
    IR_FILE_EC_OK = 0,
    IR_FILE_EC_FAIL = -1
};

enum : IrFileInt
{
    IR_FILE_THERMAL_PICTURE = 1,
    IR_FILE_BGR = 1,
    IR_FILE_HOLLY = 1,
    IR_FILE_SPOT = 1,
    IR_FILE_RECTANGLE = 3,
    IR_FILE_COLOR_DIST_T_TWB = 1,
    IR_FILE_COLOR_DIST_TEMP_LINEAR = 2,
    IR_FILE_COLOR_DIST_AD_LINEAR = 3,
    IR_FILE_GAIN_FIXED = 1,
    IR_FILE_GAIN_AUTO = 2,
    IR_FILE_IMAGE_ENH_NONE = 0,
    IR_FILE_IMAGE_ENH_IR_EDGE = 1,
    IR_FILE_DECONV_VALID = 1
};

#pragma pack(push, 8)

struct IrFileSize
{
    IrFileShort width;
    IrFileShort height;
};

struct IrFileTempRange
{
    float max;
    float min;
};

struct IrFileThermalParams
{
    float reflTemp;
    float atmTemp;
    float distance;
    float relHumidity;
    float emissivity;
    float extOptTemp;
    float extOptTrans;
};

struct IrFileLut
{
    IrFileByte radMethod;
    IrFileUInt count;
    IrFileUShort* adArray;
    float* tempArray;
};

struct IrFileFrame
{
    IrFileULongLong timeStamp;
    IrFileUInt length;
    IrFileUShort* buffer;
};

struct IrFilePoint
{
    IrFileShort x;
    IrFileShort y;
};

struct IrFileTempPoint
{
    IrFilePoint point;
    IrFileByte localParams;
    float reflTemp;
    float distance;
    float emissivity;
    float temp;
};

struct IrFileImage
{
    IrFileUInt offset;
    IrFileByte* data;
};

struct IrFileImageParams
{
    IrFileUShort contrast;
    IrFileUShort brightness;
    IrFileByte dde_enabled;
    IrFileUShort dde_value;
    IrFileByte deconv_valid;
    float deconv_psf_sigma;
};

static_assert(sizeof(IrFileImageParams) == 16, "IrFileImageParams must match ir_file_image_params");

union IrFileVtParams
{
    void* buffer;
    IrFileImageParams* image_params;
};

struct IrFileMarker
{
    IrFileInt type;
    IrFileByte name[32];
    IrFileByte localParams;
    float reflTemp;
    float distance;
    float emissivity;
    IrFileUInt pointCount;
    IrFilePoint* points;
    float tempValues[3];
    IrFilePoint tempSpots[2];
    float measuredValue;
    struct
    {
        IrFileUInt length;
        IrFileByte* buffer;
    } exInfo;
};

struct IrFilePalette
{
    IrFileByte refNo;
    IrFileUInt colors[256];
    IrFileByte inverted;
};

struct IrFileGain
{
    IrFileInt mode;
    float max;
    float min;
    IrFileByte fixedSpan;
    float span;
};

struct IrFilePresetPalette
{
    IrFileByte refNo;
    IrFileByte name[32];
};

struct IrFilePresetPalettes
{
    IrFileUInt count;
    IrFilePresetPalette* palettes;
};

#pragma pack(pop)

class IrtekFileSdkLoader
{
public:
    using FnIrFileCreate = IrFileInt (*)(const char*, IrFileInt, IrFileHandle*);
    using FnIrFileClose = IrFileInt (*)(IrFileHandle);
    using FnIrFileSetIspType = IrFileInt (*)(IrFileHandle, IrFileInt);
    using FnIrFileSetThermalSize = IrFileInt (*)(IrFileHandle, const IrFileSize*);
    using FnIrFileSetTempRange = IrFileInt (*)(IrFileHandle, const IrFileTempRange*);
    using FnIrFileSetThermalParams = IrFileInt (*)(IrFileHandle, const IrFileThermalParams*);
    using FnIrFileSetLut = IrFileInt (*)(IrFileHandle, const IrFileLut*);
    using FnIrFileSetMarker = IrFileInt (*)(IrFileHandle, IrFileMarker*);
    using FnIrFileGetThermalImageFromFrame = IrFileInt (*)(IrFileHandle, const IrFileFrame*, IrFileInt, IrFileImage*);
    using FnIrFileGetTempSpot = IrFileInt (*)(IrFileHandle, IrFileTempPoint*);
    using FnIrFileSetVtParamsV2 = IrFileInt (*)(IrFileHandle, IrFileVtParams);
    using FnIrFileGetVtParamsV2 = IrFileInt (*)(IrFileHandle, IrFileVtParams);
    using FnIrFileSetImageEnh = IrFileInt (*)(IrFileHandle, IrFileInt);
    using FnIrFileGetImageEnh = IrFileInt (*)(IrFileHandle, IrFileInt*);
    using FnIrFileSetColorDist = IrFileInt (*)(IrFileHandle, IrFileInt);
    using FnIrFileGetColorDist = IrFileInt (*)(IrFileHandle, IrFileInt*);
    using FnIrFileSetPalette = IrFileInt (*)(IrFileHandle, const IrFilePalette*);
    using FnIrFileGetPalette = IrFileInt (*)(IrFileHandle, IrFilePalette*);
    using FnIrFileSetGain = IrFileInt (*)(IrFileHandle, IrFileGain*);
    using FnIrFileGetGain = IrFileInt (*)(IrFileHandle, IrFileGain*);
    using FnIrFileGetPresetPalettes = IrFileInt (*)(IrFilePresetPalettes*);
    using FnIrFileGetPresetPaletteImage = IrFileInt (*)(IrFileByte, IrFileImage*);

    ~IrtekFileSdkLoader();

    bool load(std::string* error_message = nullptr);
    const std::string& loaded_path() const { return _loaded_path; }

    FnIrFileCreate ir_file_create = nullptr;
    FnIrFileClose ir_file_close = nullptr;
    FnIrFileSetIspType ir_file_set_isp_type = nullptr;
    FnIrFileSetThermalSize ir_file_set_thermal_size = nullptr;
    FnIrFileSetTempRange ir_file_set_temp_range = nullptr;
    FnIrFileSetThermalParams ir_file_set_thermal_params = nullptr;
    FnIrFileSetLut ir_file_set_lut = nullptr;
    FnIrFileSetMarker ir_file_set_marker = nullptr;
    FnIrFileGetThermalImageFromFrame ir_file_get_thermal_image_from_frame = nullptr;
    FnIrFileGetTempSpot ir_file_get_temp_spot = nullptr;
    FnIrFileSetVtParamsV2 ir_file_set_vt_params_v2 = nullptr;
    FnIrFileGetVtParamsV2 ir_file_get_vt_params_v2 = nullptr;
    FnIrFileSetImageEnh ir_file_set_image_enh = nullptr;
    FnIrFileGetImageEnh ir_file_get_image_enh = nullptr;
    FnIrFileSetColorDist ir_file_set_color_dist = nullptr;
    FnIrFileGetColorDist ir_file_get_color_dist = nullptr;
    FnIrFileSetPalette ir_file_set_palette = nullptr;
    FnIrFileGetPalette ir_file_get_palette = nullptr;
    FnIrFileSetGain ir_file_set_gain = nullptr;
    FnIrFileGetGain ir_file_get_gain = nullptr;
    FnIrFileGetPresetPalettes ir_file_get_preset_palettes = nullptr;
    FnIrFileGetPresetPaletteImage ir_file_get_preset_palette_image = nullptr;

private:
    void* _handle = nullptr;
    std::string _loaded_path;

    bool bind_symbols(std::string* error_message);
    static std::vector<std::string> candidate_paths();
};
