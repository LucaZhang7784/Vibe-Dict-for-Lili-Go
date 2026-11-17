# Tactility 0.7.0 触屏补丁

本目录公开源码补丁，**不公开带上游 Logo 的修改版固件二进制**。Tactility 的上游许可授予对 Logo 使用有独立限制；个人本地构建允许的范围见 [上游许可](UPSTREAM-LICENSE-GRANT.md)。板级源代码补丁为 GPL-3.0，完整正文见 LICENSE-GPL-3.0.md。

## 修改与实测

基于 v0.7.0 / commit `cd07c649264b2cfb850b675cd0d5aa9ed5ce4110`。唯一运行时源码改动为 `Devices/lilygo-tdeck/Source/devices/Display.cpp` 的 GT911 `pinInterrupt` 参数由 `GPIO_NUM_16` 改为 `GPIO_NUM_NC`。仍使用原生驱动和坐标变换，LVGL 保留原生 TIMER 输入模式，沿用 17ms 读取周期。

项目所有者在一台 T-Vibe-Dev 上确认修正版系统运行正常、Vibe Dictionary 触屏正常。持续轮询增加总线活动，长期功耗尚未测量；这不是对所有 GT911 触摸异常的统一诊断。

## 本地构建

在克隆的官方源码中应用补丁，不要把本仓库 App 当作系统工程：

```sh
git clone --branch v0.7.0 --recurse-submodules https://github.com/TactilityProject/Tactility.git Tactility-touchfix
cd Tactility-touchfix
git rev-parse HEAD
# 应为 cd07c649264b2cfb850b675cd0d5aa9ed5ce4110
git apply /path/to/Vibe-Dict-for-Lili-Go/firmware/tactility-0.7.0-tvibe-touchfix1.patch
python device.py lilygo-tdeck
# 激活 ESP-IDF v5.5.2 后
idf.py build
```

源码较多，建议把源码、依赖和 build 放在 Linux 原生文件系统，避免 Docker bind 到 Windows 目录时的大量文件访问开销。

本机成功构建使用依赖的固定版本：LVGL 9.3.0、esp_lvgl_port 2.7.2、esp_lcd_touch 1.2.1、esp_lcd_touch_gt911 1.1.3、ESP-IDF 5.5.2；完整原始解析锁文件附在 dependencies.lock。上游组件解析器可能将已下载组件当作本地覆盖，继而重新解析传递依赖；若日志引入 LVGL 9.6，或 epdiy 未注册，应停止并核对依赖，不能把升级后的结果当作本次验证构建。本机处理方式是固定原版本组件至独立 vendor 目录，并通过 override_path 显式引用，避免与 managed_components 清理过程交叉。该构建环境处理不改变触屏运行时代码。

## 更新边界

构建生成的默认 flash_args 使用官方标准分区。通过 Launcher 安装的设备可能有完全不同的应用槽位及容量，**不可直接使用默认整机刷写命令或复制其他设备的偏移**。

先核对硬件、备份 Flash 和设置、读取实际分区，确保新应用能放入目标槽。只更新自己的 Tactility 应用槽可保留 Launcher 和原数据；必须在写后验证镜像内容与槽位外区域。本仓库不发布设备备份、个人分区表、MAC、NVS 或自动刷写脚本。

本地构建镜像属于系统固件，不能通过 Tactility 的 App 安装界面安装。VibeDictionary 的 `.app` 是另一种独立安装包，见根 README。
