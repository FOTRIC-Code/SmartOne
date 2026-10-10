# 宿主机构建依赖（Ubuntu）

本文整理在 Ubuntu 22.04 上首次全量编译 Parkia.Cube（`./build.sh`）时需要的 apt 包，来源于实际构建过程中 `check-sdk` / `check-loader` / `check-kernel` / `check-buildroot` 等脚本的报错与安装记录。

适用环境：Ubuntu 22.04（Jammy）及相近 Debian/Ubuntu。其他发行版包名可能不同。

## 一键安装（推荐）

```shell
sudo apt-get update
sudo apt-get install -y \
  build-essential gcc g++ make cmake \
  flex bison bc \
  python2 python3 python3-dev python-is-python3 \
  rsync curl wget file cpio unzip \
  libncurses-dev libssl-dev libgmp-dev libmpc-dev libc6-dev \
  lz4 expect expect-dev \
  texinfo gawk chrpath diffstat git \
  u-boot-tools device-tree-compiler fakeroot
```

装好后即可按 [README](../README.md) 预置 `buildroot/dl/`（可选）并执行 `./build.sh`。

## Node.js / pnpm（编译 www 前端）

`./build.sh`（含 `--rebuild`）会构建 `app/www`，需要 **Node ≥ 16**（建议 20 LTS）以及 **pnpm 或 npm**。脚本会自动探测：

- `/opt/nodejs/current/bin`（优先）
- 或已加载的 nvm（`$NVM_DIR/nvm.sh` / `~/.nvm/nvm.sh`）
- 或当前 `PATH` 中的 `pnpm` / `npm`

无 sudo 时可用官方二进制（示例）：

```shell
# 下载并解压到 ~/.local/nodejs（Node 20 LTS）
NODE_VER=v20.18.1
curl -fL -o /tmp/node.tar.xz \
  "https://npmmirror.com/mirrors/node/${NODE_VER}/node-${NODE_VER}-linux-x64.tar.xz"
mkdir -p "$HOME/.local" && tar -xJf /tmp/node.tar.xz -C "$HOME/.local"
mv "$HOME/.local/node-${NODE_VER}-linux-x64" "$HOME/.local/nodejs"
echo 'export PATH="$HOME/.local/nodejs/bin:$PATH"' >> ~/.bashrc
export PATH="$HOME/.local/nodejs/bin:$PATH"
npm install -g pnpm
```

有 sudo 时也可把上述目录链到 `/opt/nodejs/current`，与 `build.sh` 默认探测路径对齐。

## 按阶段说明

| 阶段 | 包 | 原因 |
|------|----|------|
| SDK 入口 | `build-essential` / `gcc` / `g++` / `make` / `rsync` / `python3` | `check-sdk.sh` |
| U-Boot | `python2` | `check-loader.sh`（U-Boot 仍依赖 python2） |
| Kernel | `lz4`（≥1.9.3，需支持 `favor-decSpeed`）、`python-is-python3`、`flex`、`libssl-dev`、`libgmp-dev`、`libmpc-dev`、`libncurses-dev` | `check-kernel.sh` |
| Buildroot | `expect` + `expect-dev`（提供 `unbuffer`）、`curl`、`libc6-dev` | `check-buildroot.sh` / 网络探测 |
| 打包/常用工具 | `fakeroot`、`u-boot-tools`、`device-tree-compiler`、`cmake`、`bc`、`wget`、`file`、`cpio`、`unzip` 等 | 镜像打包与日常构建 |

### 关于 lz4

Ubuntu 22.04 的 `lz4`（1.9.3）一般可通过内核检查。若仍提示太旧，可按 SDK 提示从源码安装 v1.9.4：

```shell
git clone https://github.com/lz4/lz4.git --depth 1 -b v1.9.4
cd lz4 && sudo make install -j$(nproc)
```

### 关于 Buildroot 源码包下载

宿主机 apt 依赖装齐后，Buildroot 仍需下载大量源码包。网络不佳时请预置离线缓存（见 [README](../README.md)「首次构建：预置 Buildroot 下载依赖」），例如：

```shell
tar -xzf /path/to/buildroot-dl.tar.gz -C buildroot/
```

个别包若镜像 404/403，可从官方源手动放入 `buildroot/dl/<pkg>/`（构建会校验 hash）。

## 烧录工具（可选）

烧录不依赖上述编译包，但 Linux 下用 `upgrade_tool` 常需 USB 权限：

```shell
sudo ./tools/linux/Linux_Upgrade_Tool/Linux_Upgrade_Tool/upgrade_tool uf output/firmware/update.img
```

## 验证

```shell
which gcc g++ make python2 python3 python lz4 unbuffer curl flex rsync
lz4 -h 2>&1 | grep -q favor-decSpeed && echo "lz4 OK"
```
