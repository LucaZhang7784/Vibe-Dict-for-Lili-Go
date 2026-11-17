# Vibe-Dict for Lili Go

**面向 LILYGO T-Vibe-Dev 的原生 Tactility 电子词典：离线英汉 / 汉英、拼音输入、本地发音，以及按需在线释义与双语例句。**

从 [Cardputer ADV 词典](https://github.com/LucaZhang7784/cardputer_adv_dict) 迁移而来，围绕 320×240 屏幕、小键盘、轨迹球和触屏重新设计。仓库名沿用项目发起者指定的 “Lili Go”，设备官方品牌为 **LILYGO**。

当前版本：**App 0.2.0 + Tactility 0.7.0 touchfix1**。2026-11-17，项目所有者在一台 T-Vibe-Dev 上确认查词、发音、在线查询及触屏可用。此记录不代表其他板型或长期稳定性均已验证。

[下载可安装 App](https://github.com/LucaZhang7784/Vibe-Dict-for-Lili-Go/releases) · [安装与操作](docs/USAGE.md) · [技术细节](docs/TECHNICAL.md) · [构建与检查](docs/BUILD.md) · [完整致谢](ACKNOWLEDGEMENTS.md)

## 功能

- **离线双向查词**：ECDICT 衍生英汉 768,739 条，CC-CEDICT 衍生汉英 125,264 条；计数对应本项目所用数据快照。
- **中文输入**：App 内全拼、横向五候选、数字 / 轨迹球选词、左右 Shift 候选翻页。
- **阅读**：轨迹球逐行滚动、正文翻页、多义项切换、历史记录与收藏。
- **本地发音**：复用原项目的 30,000 个美音 WAV；P 键或屏幕按钮触发。
- **在线扩展**：在离线查询之后主动选择在线英汉释义、双语例句，使用外研在线官方接入试用服务。
- **系统触屏修正**：单独提供 Tactility 板级补丁，将 GT911 读取切为原生定时模式；不是 App 内的触摸模拟。

## 项目小记

### 给 T-Vibe 做了一个电子词典 📚

最近把原来 Cardputer ADV 上的词典项目，迁到了 LILYGO T-Vibe-Dev，做成了 Tactility 里的一个 App，叫 Vibe Dictionary。

这台小设备有键盘、轨迹球和触屏，适合把查词做成一个比较专注的小工具。于是也顺着它的操作方式，重新调整了输入和阅读界面。

📖 平时先查离线词库。想再了解一点，可以从结果页继续查在线释义和中英例句。

⌨️ 中文输入用了 App 内的拼音候选栏，一次横向显示五个，用数字或轨迹球选词，左右 Shift 翻页。读长释义时，也可以用轨迹球慢慢往下滚。

🔊 词库和本地发音放在 SD 卡里。这次核验了三万个发音文件，也修好了语速和文件读取的问题，现在可以按键或点按钮听发音。

🎨 界面用了暖白底和一点橙色，把输入框、候选词和正文分开，尽量让小屏幕上的内容清楚一些。

期间还处理了触屏进入页面几秒后就不再响应的问题。最后修改了 Tactility 的触屏读取方式，系统和词典里的触摸操作都已经实机验证正常。

现在可以输入一个词，读释义、听发音，再按需要看例句。这个使用过程慢慢变得顺手了。🌿

在线部分目前接的是外研在线官方试用接口，后续还会继续确认长期使用方式，也观察日常稳定性和续航。

#LILYGO #TVibe #电子词典 #Tactility #嵌入式开发 #个人项目


## 安装速览

1. 设备为 **T-Vibe-Dev / T-Deck 类硬件、ESP32-S3**，运行 Tactility **0.7.0**。T-Vibe-Pager、T-Deck Pro 等不在本次验证范围。
2. 从 [Releases](https://github.com/LucaZhang7784/Vibe-Dict-for-Lili-Go/releases) 下载 `VibeDictionary-0.2.0-esp32s3.app`，核对同页 `SHA256SUMS.txt`。
3. 首选在设备 Web Server 的 **Apps** 页上传 `.app`，由 Tactility 安装；也可按 [使用说明](docs/USAGE.md) 将 App 完整目录放入 `SD:/app/local.tvibe.dictionary/`；词库及 WAV 放入 `SD:/cardputer/dictionary/`，然后重启系统扫描 App。
4. 词库和音频沿用原有 [cardputer_adv_dict](https://github.com/LucaZhang7784/cardputer_adv_dict) 数据，避免在两个仓库重复储存约 2.33GB 音频。完整下载音频需要 Git LFS，详见原仓库的许可说明。
5. 如果遇到“进入页面几秒后触屏停止响应”，阅读 [固件补丁说明](firmware/README.md)。固件应用镜像不能替代 `.app` 包；不要套用本项目开发机的串口或 Flash 偏移。

## 操作

| 场景 | 操作 |
|---|---|
| 搜索页 | 字母输入；`$` 切换英文 / 拼音 |
| 拼音组合 | `1`–`5` 或轨迹球选候选；左 Shift 上一组，右 Shift 下一组 |
| Enter | 组合中确认候选；组合结束后查词 |
| 释义阅读 | 轨迹球滚动，`U` / `D` 正文上 / 下页 |
| 结果页 | `P` 发音、`O` 在线释义、`E` 双语例句、`S` 收藏、`X` 退出 |
| 输入页执行菜单 | 先 `#` 再按菜单标注字母，例如 `#X` 退出、`#T` 触屏测试 |

麦克风物理键的独立事件透传尚未完成验收；数字 `0` 不用作发音快捷键。中文输入限本 App，并非全系统中文输入法。完整菜单及状态差异见 [USAGE](docs/USAGE.md)。

## 技术概览

App 使用 C、Tactility 0.7.0 ESP32-S3 SDK、ESP-IDF 5.5.2 和 LVGL。离线数据按索引随机读取，18px Unicode 字体按需从 SD 读取。网络与本地任务分离；网络有超时、取消、返回体上限、完整 JSON 验证及有界重试。

本地单声道 48kHz PCM 发音适配系统双声道 I2S；文件名使用明确 ASCII 规则，避免宿主字符分类表 ABI 差异。GT911 固件补丁只改变该板型的中断参数，沿用系统 17ms 定时读取，长期功耗影响仍待测试。

## 当前边界

- 在线服务使用 [外研在线官方公开试用账号](https://open.unipus.cn/docs/api/dict/read/)。源码中的试用标识和密钥来自该公开文档，**不是项目所有者的私有凭据，也不代表长期生产授权**。该文档仅将统一账号用于前期接入测试。试用接口可能限流、失效或不返回部分词条。
- 已在项目所有者的中国大陆网络环境下通过实机测试；不能保证所有网络和运营商均可访问。当前在线路径为英汉查询，离线中文查词仍可用。
- 系统时间必须准确；时区可显示为 UTC+8。鉴权使用 Unix 时间戳，单纯调整显示时区不能纠正错误的系统时间。
- 30,000 个 WAV 完成了文件名映射与源文件头、长度检查；apple、hello 等做过实机发音确认，未逐个人工试听全部文件。
- 触屏长期稳定性、续航、其他硬件修订、系统升级兼容性尚需继续验证。

## 致谢

感谢 [Tactility](https://github.com/TactilityProject/Tactility)、[LilyGoLib](https://github.com/Xinyuan-LilyGO/LilyGoLib)、[Launcher](https://github.com/bmorcelli/Launcher)、[ESP-IDF](https://github.com/espressif/esp-idf)、[LVGL](https://github.com/lvgl/lvgl) 及 [esp-bsp](https://github.com/espressif/esp-bsp) 提供系统、硬件接口、启动环境和输入组件。

感谢 [原 Cardputer 词典](https://github.com/LucaZhang7784/cardputer_adv_dict)、[ECDICT](https://github.com/skywind3000/ECDICT)、[CC-CEDICT / MDBG](https://cc-cedict.org/editor/editor.php?handler=Download)、[wordfreq](https://github.com/rspeer/wordfreq)、[Piper](https://github.com/rhasspy/piper)、[Google Fonts](https://github.com/google/fonts)、[cJSON](https://github.com/DaveGamble/cJSON) 等提供数据来源、工具与基础组件。

[ACKNOWLEDGEMENTS.md](ACKNOWLEDGEMENTS.md) 列出开发中引用的完整仓库清单，并区分实际使用、上游传递依赖与前期研究参考；列名不表示所有项目都被集成。

## 许可与公开范围

本项目原创 App 代码与文档采用 [Apache-2.0](LICENSE)。Tactility 固件、补丁和对应源码遵循其 [GPL-3.0](firmware/LICENSE-GPL-3.0.md) 及上游第三方声明。字体、CA、cJSON、词库和音频各自保留原有许可，不能统一改标 Apache-2.0。参见 [第三方声明](docs/THIRD_PARTY_NOTICES.md)。

公开仓库及 Release 不包含设备 Flash 备份、NVS、个人 Wi-Fi 配置、设备 MAC、查询历史、收藏或原始串口日志。Release 提供 Tactility 可安装 `.app` 包。触屏修正版提供源码补丁与本地构建说明；上游 Logo 条款限制带 Logo 的修改版固件公开分发，因此不公开当前固件二进制。Flash 更新前请先核对自己的设备和分区并完成备份。
