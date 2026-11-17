# 致谢与引用仓库

感谢所有上游维护者。以下按本次项目中的实际角色分类；研究参考不代表代码已被集成，也不代表推荐这些固件适用于当前硬件。许可范围见 [第三方声明](docs/THIRD_PARTY_NOTICES.md)。

## 直接使用、构建与硬件参考

| 仓库 | 用途 |
|---|---|
| [LucaZhang7784/cardputer_adv_dict](https://github.com/LucaZhang7784/cardputer_adv_dict) | 原始词典项目、离线数据格式及发音数据来源。 |
| [TactilityProject/Tactility](https://github.com/TactilityProject/Tactility) | App 宿主、0.7.0 SDK、系统触屏补丁基线。 |
| [Xinyuan-LilyGO/LilyGoLib](https://github.com/Xinyuan-LilyGO/LilyGoLib) | T-Vibe-Dev 硬件资料、键盘与原厂实现参考。 |
| [Xinyuan-LilyGO/T-Deck](https://github.com/Xinyuan-LilyGO/T-Deck) | 同类板型硬件及原理图参考。 |
| [bmorcelli/Launcher](https://github.com/bmorcelli/Launcher) | 设备启动与固件安装环境；App 不要求重新刷写 Launcher。 |
| [espressif/esp-idf](https://github.com/espressif/esp-idf) | 5.5.2 构建、I2S、HTTPS、FreeRTOS 和底层设备 API。 |
| [espressif/esp-bsp](https://github.com/espressif/esp-bsp) | esp_lvgl_port 与 GT911 触屏组件、事件读取路径排查。 |
| [lvgl/lvgl](https://github.com/lvgl/lvgl) | 图形界面、输入设备与定时器 API。 |
| [espressif/mbedtls](https://github.com/espressif/mbedtls) | ESP-IDF 使用的 TLS/AES 实现参考。 |
| [Mbed-TLS/mbedtls](https://github.com/Mbed-TLS/mbedtls) | TLS/AES 上游。 |
| [DaveGamble/cJSON](https://github.com/DaveGamble/cJSON) | App 内 JSON 解析；MIT 许可随资源保留。 |
| [certifi/python-certifi](https://github.com/certifi/python-certifi) | Mozilla CA 集合的构建来源；MPL-2.0 声明保留。 |
| [google/fonts](https://github.com/google/fonts) | Noto Sans SC 字形来源；SIL OFL 1.1 随资源保留。 |
| [fonttools/fonttools](https://github.com/fonttools/fonttools) | 字体码点表读取。 |
| [python-pillow/Pillow](https://github.com/python-pillow/Pillow) | SD 位图字体生成。 |
| [skywind3000/ECDICT](https://github.com/skywind3000/ECDICT) | 离线英汉词库来源，MIT。 |
| [rspeer/wordfreq](https://github.com/rspeer/wordfreq) | 原项目 30,000 个发音词的词频筛选来源。 |
| [rhasspy/piper](https://github.com/rhasspy/piper) | 原项目离线 TTS 生成工具。 |
| [ByteWelder/minitar](https://github.com/ByteWelder/minitar) | Tactility .app 的 USTAR 安装器依赖与包验证参考。 |
| [FreeRTOS/FreeRTOS-Kernel](https://github.com/FreeRTOS/FreeRTOS-Kernel) | 系统调度及线程基础（通过 ESP-IDF / Tactility）。 |

## 原 Cardputer 项目沿革

| 仓库 | 用途 |
|---|---|
| [m5stack/uiflow-micropython](https://github.com/m5stack/uiflow-micropython) | Cardputer 原项目运行环境。 |
| [micropython/micropython](https://github.com/micropython/micropython) | Cardputer 原项目运行环境。 |
| [dejavu-fonts/dejavu-fonts](https://github.com/dejavu-fonts/dejavu-fonts) | 原项目 IPA 位图来源；本 App 改用 Noto 字体。 |
| [numpy/numpy](https://github.com/numpy/numpy) | 原项目音频批量生成依赖。 |
| [eshaz/wasm-audio-decoders](https://github.com/eshaz/wasm-audio-decoders) | 原项目旧后端解码实验，本 App 不集成。 |
| [libsdl-org/mpg123](https://github.com/libsdl-org/mpg123) | 上述解码器的上游，本 App 不集成。 |

## 前期比较及已替换的方案

| 仓库 | 用途 |
|---|---|
| [PhilMo6/meshpunk](https://github.com/PhilMo6/meshpunk) | 前期候选固件试用，未集成进本 App。 |
| [meetDeveloper/freeDictionaryAPI](https://github.com/meetDeveloper/freeDictionaryAPI) | 前期在线释义接口尝试，现已替换。 |
| [Tatoeba/tatoeba2](https://github.com/Tatoeba/tatoeba2) | 前期例句服务尝试，现已替换。 |
| [0015/PocketSSH](https://github.com/0015/PocketSSH) | 前期设备固件/应用方案调研。 |
| [8bitmcu/vtOS](https://github.com/8bitmcu/vtOS) | 前期设备系统方案调研。 |
| [BruceDevices/firmware](https://github.com/BruceDevices/firmware) | 前期设备固件方案调研。 |
| [GhostESP-Revival/GhostESP](https://github.com/GhostESP-Revival/GhostESP) | 前期设备固件方案调研。 |
| [Haris16-code/KryonOS](https://github.com/Haris16-code/KryonOS) | 前期设备系统方案调研。 |
| [acidvegas/acid-drop](https://github.com/acidvegas/acid-drop) | 前期设备固件方案调研。 |
| [echo-lalia/MicroHydra](https://github.com/echo-lalia/MicroHydra) | 前期设备系统方案调研。 |

## Tactility 上游声明涉及的其他项目

以下沿用上游致谢，具体代码归属及许可见 [原声明](firmware/UPSTREAM-THIRD-PARTY-NOTICES.md)。

- [UsefulElectronics/esp32s3-gc9a01-lvgl](https://github.com/UsefulElectronics/esp32s3-gc9a01-lvgl)
- [Xinyuan-LilyGO/T-Echo](https://github.com/Xinyuan-LilyGO/T-Echo)
- [flipperdevices/flipperzero-firmware](https://github.com/flipperdevices/flipperzero-firmware)
- [google/material-design-icons](https://github.com/google/material-design-icons)
- [kosma/minmea](https://github.com/kosma/minmea)
- [meshtastic/firmware](https://github.com/meshtastic/firmware)
- [nayarsystems/posix_tz_db](https://github.com/nayarsystems/posix_tz_db)

## 不以 GitHub 仓库提供的来源

- [CC-CEDICT / MDBG](https://cc-cedict.org/editor/editor.php?handler=Download)：汉英词库，CC BY-SA 4.0。
- [外研在线开放平台](https://open.unipus.cn/docs/api/dict/read/)：官方试用查询服务，非本项目所有；未找到或未引用其公开服务端源码仓库。
- [Piper voice package](https://huggingface.co/rhasspy/piper-voices/tree/main/en/en_US/lessac/medium)：原音频语音模型；模型与训练数据条款详见数据声明。

这些署名不改变各项目的许可证，也不表示上游为本项目背书。
