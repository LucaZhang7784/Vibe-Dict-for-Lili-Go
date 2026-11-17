# 安装与使用

## 硬件与系统

验证对象为 LILYGO T-Vibe-Dev（T-Deck 类）、ESP32-S3、320×240 ST7789 显示及 GT911 触屏，Tactility 0.7.0。T-Vibe-Pager / T-Deck Pro 等需另行适配。microSD 实测采用 FAT32；请先备份，不提供自动格式化程序。

## 数据与 App

数据目录需要约 2.5GB；建议预留至少 3GB。词库快照来自原项目，五个二进制数据/索引文件的哈希见 [data-manifest.json](data-manifest.json)。

```sh
# 仅先下载代码与词库，不自动拉取大体积音频
GIT_LFS_SKIP_SMUDGE=1 git clone https://github.com/LucaZhang7784/cardputer_adv_dict.git
cd cardputer_adv_dict
git checkout 30e37654aadd5b0496943b7712888e929660dc4f
git lfs pull --include="data/audio/*.wav"
```

这一步依赖原仓库 Git LFS 的可用性与流量额度。音频条款见 [原数据声明](CARDPUTER_DATA_NOTICES.md)；本仓库不另行重复打包这些音频。

将 `data/local/` 中的文件复制到 SD 的 `cardputer/dictionary/`，将 `data/audio/` 的 WAV 复制到其 `audio/` 子目录，保持名称不变：

```text
SD:/cardputer/dictionary/
  manifest.json
  en-zh.idx
  en-zh.dat
  zh-en.idx
  zh-en.dat
  zh-en.pinyin.idx
  audio/*.wav
```

## 方式 A：由 Tactility 安装 .app

从 Release 下载 `.app.zip`，解压得到 `.app`，并核对 SHA256。ZIP 只是 GitHub 下载封装，不要将 ZIP 本身上传到设备。在设备启用 Tactility Web Server，在电脑浏览器打开设备显示的地址，进入 **Apps** 页，选择安装/上传 App，将 `.app` 上传。服务端调用原生 App 安装器完成解包及 manifest 解析。电脑与设备需要互相可达，不使用本文作者的本地 IP。

本包约 15MB，小于 0.7.0 Web Server 的 20MiB App 上传上限。需在 SD 上预留安装包临时文件和解包资源的空间，建议在词库音频之外再留至少 50MB。安装成功后刷新 App 列表；若仍未显示，重启系统。不要把系统固件 `.bin` 上传到 App 安装入口。

## 方式 B：USB 离线复制

若网络安装不可达，`.app` 是普通 USTAR 归档、不是 gzip。可以在电脑先解包，再通过 Tactility 的 USB storage（SD card）复制完整内容：

```sh
mkdir -p unpacked-vibe
tar -xf VibeDictionary-0.2.0-esp32s3.app -C unpacked-vibe
```

把 `unpacked-vibe` 内的 `manifest.properties`、`elf/`、`assets/` 一起放进 `SD:/app/local.tvibe.dictionary/`：

```text
SD:/app/local.tvibe.dictionary/
  manifest.properties
  elf/esp32s3.elf
  assets/font18.bin
  assets/roots.pem
  assets/其余许可文件
```

请复制至 SD 卡，不是容量很小的内部 Flash USB 盘。不改 manifest 分节或字段大小写。安全退出电脑存储后退出 USB 模式并重启设备，让系统重新扫描。App 列表中选择 **Vibe Dictionary**。

## 操作

搜索页 `$` 切换英文 / 全拼。英文输入后 Enter 查词；拼音组合中 Enter 先确认候选，再次 Enter 查词。候选一屏五个，以数字或轨迹球选择；左 / 右 Shift 翻组。U/D 不用于拼音候选翻页。

轨迹球滚动正文，结果页 U/D 翻页。按钮与菜单按界面标注使用：P 发音、O 在线释义、E 例句、S 收藏、H 历史、F 收藏列表、B 返回、X 退出。输入页先 # 再输入菜单字母，避免把正常输入误判为菜单；结果页可直接按快捷字母。#T 打开九宫格触屏测试，T 关闭。

App 支持 $ 切换及屏幕 EN/拼音按钮。麦克风键的宿主事件透传未验收，请用 P 或发音按钮；数字 0 保持数字输入。

## 联网、设置与记录

先在 Tactility 系统中连接自己的 2.4GHz Wi-Fi，并校准系统时间。Launcher 的联网状态不等于 Tactility 已联网。外研试用鉴权依赖当前 Unix 秒数，HTTPS 校验证书开启。在线查询只在主动执行 O/E 时发送词头，不自动发送逐键输入。

系统分配的用户目录下保存历史与收藏，最近 20 条在界面显示；收藏文件保留完整记录。根目录 `/vibe-dictionary.log` 用于诊断，可能含查询词及网络诊断信息，分享前自行检查。

## 常见问题

| 现象 | 处理 |
|---|---|
| 列表没有 App | 确认复制完整目录及 INI 格式 manifest；重启系统扫描 |
| 中文方框或资源缺失 | 核对 assets/font18.bin 及完整许可/资源文件 |
| 缺少发音 | 确认 WAV 在 dictionary/audio 下，且未只下载 LFS 指针；并非所有词都有音频 |
| 在线超时或授权错误 | 校准时间，确认 Tactility Wi-Fi，查看错误与试用服务状态；离线可继续使用 |
| 只有最初几秒触屏可用 | 参阅 [固件补丁](../firmware/README.md)，不要以修改 App 触点阈值代替系统排查 |
| 升级后异常 | 本版本锁定 0.7.0 SDK；升级宿主后需重新评估 ABI 与导出符号 |

不要通过本项目修改键盘 ESP32-C3 固件，也不要向未知 Flash 偏移写入固件文件。
