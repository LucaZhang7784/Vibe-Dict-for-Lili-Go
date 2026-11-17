# App 构建与检查

## 工具链

使用 Tactility v0.7.0 的 **TactilitySDK-esp32s3.zip**，不要换成最新开发版 SDK。下载自：

https://github.com/TactilityProject/Tactility/releases/tag/v0.7.0

SDK 解包目录应直接包含 `TactilitySDK.cmake`。ESP-IDF 使用 `espressif/idf:v5.5.2`。仓库已有可分发的字体和证书资源；仅重建资源时才需要 `requirements-build.txt` 中的 Python 依赖。

以下在已激活 ESP-IDF 5.5.2 的 Linux 环境执行，路径按自己机器调整：

```sh
export TACTILITY_SDK_PATH=/absolute/path/to/TactilitySDK
idf.py set-target esp32s3
cmake --build build --target elf
python scripts/package_app.py
```

输出 `release/VibeDictionary-0.2.0-esp32s3.app`、完整安装目录和逐文件 SHA-256 清单。目标是外部 App ELF；无需构建或刷写整机固件。

Windows PowerShell 可用 Docker，例如将仓库路径和 SDK 路径分别挂到 `/project` 与 `/sdk/TactilitySDK`，工作目录 `/project`，设置容器变量 `TACTILITY_SDK_PATH=/sdk/TactilitySDK` 后执行上述命令。路径有空格时务必正确引用。

## 自动检查

在同一 IDF 环境、仓库根目录执行：

```sh
bash scripts/check_all.sh
```

覆盖 Shift、Enter、重试、接收、JSON 完整性、TCP 诊断、鉴权及响应格式化。`release/unipus-*-trial.json` 为**合成测试夹具**，不是在线词典内容分发。

真实词库检查需另有原 Cardputer 项目（包括 apps/lib/local_dict.py 及 data/local），例如挂载为 `/source`：

```sh
python scripts/check_core.py /source/data/local --skip-audio-sweep
# 完整音频检查：先下载原项目全部 Git LFS 音频，再不带 skip 参数运行
python scripts/check_core.py /source/data/local
```

需要重新生成资源时：

```sh
python -m pip install -r requirements-build.txt
python scripts/build_font.py
python scripts/build_roots.py
```

生成字体会下载 Google Fonts 字体及其 OFL 文件；重建证书会连接提供者核验证书链。字体上游、证书和构建工具变化可能导致资源哈希改变；发布时以对应 Release 的哈希为准。

## 安装包验收

包为普通 USTAR，无 gzip、无符号链接、无设备文件及路径穿越。根目录必须有 manifest.properties；manifest schema 0.1，sdk 0.7.0，platforms esp32s3，appId local.tvibe.dictionary。ELF 为 ESP32-S3 Xtensa，所有资源和许可一起打包。Release 另提供包级 SHA256SUMS.txt 与相对路径文件清单。

公开版重建的功能代码与实机验收版本相同，仅将本地 Wi-Fi 名称提示改为通用提示并更新资源 NOTICE。没有在公开代码中加入自动格式化、自动识别并刷写串口或自动改系统分区的脚本。


### 使用宿主同款 minitar 验证安装包

测试程序位于 tests/minitar_package.c，只向指定的临时目录解包。下列测试在主机执行，不在设备上执行：

```sh
git clone https://github.com/ByteWelder/minitar.git /tmp/vibe-minitar
git -C /tmp/vibe-minitar checkout 274f9566e51b81117036dc95155f943084086f53
mkdir -p /tmp/vibe-unpack/elf /tmp/vibe-unpack/assets
cc -I/tmp/vibe-minitar tests/minitar_package.c /tmp/vibe-minitar/src/tar.c /tmp/vibe-minitar/src/util.c -o /tmp/vibe-package-test
/tmp/vibe-package-test release/VibeDictionary-0.2.0-esp32s3.app /tmp/vibe-unpack
python scripts/check_package.py /tmp/vibe-unpack
```
