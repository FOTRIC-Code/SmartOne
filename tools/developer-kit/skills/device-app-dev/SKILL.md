---
name: device-app-dev
description: Build and deploy a user ELF for Parkia.Cube-class devices with the Developer Kit scripts. Use when the user asks to create a device app, compile a user process with the Buildroot SDK, deploy an ELF to `/usr/bin`, update `app_bin`, or install a custom app onto the board.
disable-model-invocation: true
---

# Device App Dev

Use this skill when developing a user-space device app that will be started by `main_app`.

## Workflow

Copy this checklist and keep it updated:

```text
Task Progress:
- [ ] 1. Confirm the board IP and target ELF name
- [ ] 2. Build the app with the Buildroot SDK
- [ ] 3. Install it with the Developer Kit script
- [ ] 4. Reboot and verify the process/logs
```

## Rules

- Build with the Buildroot SDK, not the host compiler.
- Use `bash tools/developer-kit/build_user_app.sh` to compile.
- Use `bash tools/developer-kit/install_user_app.sh` to deploy.
- Do not edit `/etc/cfg/main_app/config.json` by hand unless the user explicitly asks.
- Default install mode is a custom path under `/usr/bin/<elf-name>`, then update `app_bin`.
- Only use `/usr/bin/net_app` when the user explicitly wants the compatibility path.

## Build

If the user did not provide a source directory, default to `samples/user_app/`.

```sh
bash tools/developer-kit/build_user_app.sh \
  --sdk-root /path/to/arm-buildroot-linux-gnueabihf_sdk-buildroot
```

If `BUILDROOT_SDK_ROOT` is already exported:

```sh
bash tools/developer-kit/build_user_app.sh
```

## Install

Custom path mode:

```sh
bash tools/developer-kit/install_user_app.sh <ip> ./path/to/user_app
```

Compatibility mode:

```sh
bash tools/developer-kit/install_user_app.sh <ip> ./path/to/user_app net_app
```

## Verify

After reboot, verify with SSH:

```sh
ssh root@<ip> ps | grep <process_name>
ssh root@<ip> cat /tmp/user_app.log
```

## Find the board

Prefer:

- `discovery_app` protocol
- `ipconfig.exe`

Use `adb` / `ssh` as connection and debugging tools after the IP is known.

## References

- Deployment guide: [`docs/user-app.md`](../../../../docs/user-app.md)
- Developer Kit overview: [`tools/developer-kit/README.md`](../../README.md)
- Sample app: [`samples/user_app/README.md`](../../../../samples/user_app/README.md)
