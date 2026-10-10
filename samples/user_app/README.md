# user_app sample

最小用户程序示例，供 Developer Kit 验证交叉编译与下载流程。

## 构建

在仓库根目录执行：

```sh
tools/developer-kit/build_user_app.sh
```

如需显式指定 SDK：

```sh
tools/developer-kit/build_user_app.sh \
  --sdk-root /path/to/arm-buildroot-linux-gnueabihf_sdk-buildroot
```

默认产物：

```text
samples/user_app/build/user_app
```

## 安装

```sh
tools/developer-kit/install_user_app.sh 192.168.123.100 \
  samples/user_app/build/user_app
```

默认会按 ELF 文件名安装到 `/usr/bin/user_app`，并把 `app_bin` 更新到该路径。

## 运行效果

程序启动后每 5 秒向 `/tmp/user_app.log` 追加一条心跳日志。
