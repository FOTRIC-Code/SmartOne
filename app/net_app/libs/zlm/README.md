# ZLMediaKit (prebuilt)

- 上游：<https://github.com/ZLMediaKit/ZLMediaKit>
- 许可：MIT，见同目录 `LICENSE`
- 内容：`libmk_api.so`（ZLMediaKit C API，已为 RK3506 armhf 交叉编译）+ `include/` 头文件
- 用途：net_app 的 RTSP / WebRTC 推流；由 `app/net_app/CMakeLists.txt` 离线链接（`ZLM_INCLUDE_DIR` / `ZLM_LIBRARY`）

`./build.sh --rebuild-third-party` 会强制重建 net_app 第三方库。
