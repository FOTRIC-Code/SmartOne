# IRtek SDK (prebuilt, proprietary)

- 提供方：fotric.cn
- 许可：专有，仅以二进制形式随本 SDK 分发，见同目录 `LICENSE`
- 内容：
  - `lib/libIRtekFileSDK.so`：红外图像文件读写 / 温度解析
  - `lib/libIRtekUVCSDK.so`：红外 UVC 采集
  - `include/`：对应头文件（`IRtekFileSDK.h` 为中文注释版，`IRtekFileSDK(EN).h` 为英文版）
- 用途：net_app 红外采集与测温；`irtek_file_sdk_loader.*` 在运行时加载 `libIRtekFileSDK.so`
