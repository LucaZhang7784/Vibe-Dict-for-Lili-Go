# 第三方许可与数据来源

根目录 Apache-2.0 适用于本项目原创 App 代码和文档。它不覆盖以下第三方内容。

| 内容 | 来源与条款 | 本仓库处理 |
|---|---|---|
| Tactility 板级触屏补丁 | [Tactility v0.7.0](https://github.com/TactilityProject/Tactility/tree/v0.7.0)，Devices 目录为 GPL-3.0 | 保存补丁、GPL 正文、上游许可授予与第三方声明；不发布带上游 Logo 的修改版固件二进制 |
| 外部 App 接口 | [上游许可授予与 FAQ](../firmware/UPSTREAM-LICENSE-GRANT.md)区分外部 SDK App 与主固件 | App 保持其原有 Apache-2.0，主固件及补丁遵循上游许可 |
| font18.bin | Google Fonts Noto Sans SC，SIL OFL 1.1 | 分发派生位图字体并保留 assets/OFL-NotoSansSC.txt |
| roots.pem | certifi / Mozilla CA 集合，MPL-2.0 | 保留 assets/LICENSE-certifi.txt；筛选的公开信任根不包含私钥 |
| cJSON | DaveGamble/cJSON，MIT | App 静态使用 IDF cJSON 实现；保留 assets/LICENSE-cJSON.txt |
| 英汉数据 | ECDICT 快照，MIT | 本仓库只保存来源清单，二进制数据来自原公开项目 |
| 汉英数据及索引 | CC-CEDICT / MDBG，CC BY-SA 4.0 | 本仓库只保存来源清单；数据及改编遵循其条款 |
| 发音词表 | wordfreq，代码 Apache-2.0、数据 CC BY-SA 4.0 | 由原项目提供，非本仓库原创数据 |
| WAV 及语音模型 | Piper / en_US-lessac-medium；源数据存在非商业条款问题 | 本仓库不重复分发音频，沿用原项目详细来源与未决边界；不宣称商业授权 |
| 在线内容 | 外研在线官方接入试用 | API 使用与返回内容不被本仓库重新许可；长期使用需确认授权 |

详细音频与原项目依赖说明见 [CARDPUTER_DATA_NOTICES.md](CARDPUTER_DATA_NOTICES.md)。其中“本仓库包含音频”等叙述指原 Cardputer 仓库，不能理解为本仓库另行打包音频。完整仓库引用清单见 [ACKNOWLEDGEMENTS](../ACKNOWLEDGEMENTS.md)。

固件 Logo 使用要求见上游许可授予的 Logo 小节。本仓库没有申请额外 Logo 授权，因此仅公开触屏补丁与个人本地构建说明。
