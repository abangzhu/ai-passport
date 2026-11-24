<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 资源目录（Assets）

本目录集中存放可复用的资源（字库、图片、音乐、电池参数等），按资源类型分子目录管理。每个资源放在其类型对应的子目录，并记录放置路径、命名方式、集成方式与来源/许可。二进制资源（字体、图片、音频）不属于纯 markdown 文档，请勿与文档混放。涉及版权/授权的资源需注明来源与许可。

## 字库（fonts）

可复用的字库文件与生成的字库源码放在 `fonts/`。

- 命名要能反映字族、字重、字级与格式。
- 记录来源、许可、字符范围、转换命令与目标放置路径。
- 添加字库前评估 Flash 与内部 RAM 影响；ESP32-C3 无 PSRAM。
- 不提交许可不允许分发的字库。

### Marvy 界面字库

| 文件 | 用途 |
| --- | --- |
| [`fonts/NotoSansSC-Regular.otf`](fonts/NotoSansSC-Regular.otf) | 未修改的 Noto Sans SC Regular 源字体，用于可复现子集生成；不编译进固件。 |
| [`fonts/OFL.txt`](fonts/OFL.txt) | 源字体的 SIL Open Font License 1.1 与版权声明；再分发字体资源时应保留。 |
| [`fonts/marvy_characters.txt`](fonts/marvy_characters.txt) | 去重、排序后的 53 个固定中文界面字符清单。四份字库都包含 95 个可打印 ASCII 字符 U+0020–U+007E，每份共 148 个字形。 |
| [`fonts/marvy_font_16.c`](fonts/marvy_font_16.c) | 16 px、4 bpp、未压缩的平滑 LVGL 位图字库，用于欢迎页文字；导出符号 `marvy_font_16`。 |
| [`fonts/marvy_font_20.c`](fonts/marvy_font_20.c) | 20 px、4 bpp、未压缩的平滑 LVGL 位图字库，用于欢迎页标题；导出符号 `marvy_font_20`。 |
| [`fonts/marvy_pixel_16.c`](fonts/marvy_pixel_16.c) | 16 px、1 bpp、未压缩的硬边 LVGL 位图字库，用于游戏 HUD 与提示；导出符号 `marvy_pixel_16`。 |
| [`fonts/marvy_pixel_20.c`](fonts/marvy_pixel_20.c) | 20 px、1 bpp、未压缩的硬边 LVGL 位图字库，用于游戏与结算页标题、祝福文字；导出符号 `marvy_pixel_20`。 |

来源：[Noto CJK 官方仓库](https://github.com/notofonts/noto-cjk)，
[`Sans/SubsetOTF/SC/NotoSansSC-Regular.otf`](https://raw.githubusercontent.com/notofonts/noto-cjk/main/Sans/SubsetOTF/SC/NotoSansSC-Regular.otf)，
及其[许可证](https://raw.githubusercontent.com/notofonts/noto-cjk/main/Sans/LICENSE)。
保留的源字体以 SHA-256 固定：
`faa6c9df652116dde789d351359f3d7e5d2285a2b2a1f04a2d7244df706d5ea9`。

在仓库根目录执行 `bash tools/generate_marvy_fonts.sh` 可重新生成。
脚本校验源字体摘要，固定使用 `lv_font_conv` **1.5.3**，分别生成 16 和 20 px
的两种风格：平滑欢迎页字库使用 `--bpp 4`，硬边游戏字库使用 `--bpp 1`；两者
都采用 `--format lvgl --no-compress --lv-include lvgl.h`。生成的 C 文件加入应用
CMake 源码列表，界面标签显式选择对应字体。修改字符清单后应重建四份字库并
运行字符覆盖 gate。固定子集不支持任意输入；
真机字形与排版检查仍是独立验收项。

## 图片（images）

可复用的源图与生成的显示资产放在 `images/`。

| 文件 | 尺寸与格式 | 用途与来源 |
| --- | --- | --- |
| [`images/home.jpg`](images/home.jpg) | 3840 × 2160，JPEG | 保留在上游硬件文档中的产品主图。 |
| [`images/readme-hardware-specs.png`](images/readme-hardware-specs.png) | 2172 × 724，PNG RGBA | 保留为可选技术参考图，不再用于首页主视觉。于 2026-09-17 使用内置图像生成工具为本仓库生成；已根据文档中的硬件能力契约核对图中的六项标签与参数。 |
| [`images/logo-wordmark.png`](images/logo-wordmark.png) | 1648 × 336，PNG RGBA | 从仓库原始 `images/logo.png` 中精确裁切并去除背景的黑色字标；保留用于上游硬件文档的浅色主题。 |
| [`images/logo-wordmark-dark.png`](images/logo-wordmark-dark.png) | 1648 × 336，PNG RGBA | 提取字标的白色版本，保留用于上游硬件文档的深色主题。 |
| [`images/marvy-logo.png`](images/marvy-logo.png) | 600 × 600，PNG RGBA | 用户于 2026-11-24 提供用于本产品的 Marvy Logo 原图，未修改；保留透明背景与粉蓝渐变。 |
| [`images/marvy-logo-welcome.png`](images/marvy-logo-welcome.png) | 112 × 112，PNG RGBA | 从用户提供的 Logo 生成的欢迎页预览。 |
| [`images/marvy-logo-badge.png`](images/marvy-logo-badge.png) | 20 × 20，PNG RGBA | 用于标记游戏中、捕获动画及结算页 Marvy 羊的徽标预览。 |

用户提供的 Marvy Logo 是本产品的品牌素材，不是公共领域或开源 Logo。
原图 SHA-256：
`4ab5057babe4ee8a9f07c99fd5cd7b105e2751d5d7dd36c035ce935ba56efa99`。
在仓库根目录执行 `python3 tools/generate_marvy_logo.py`，可在不裁切、不变形的
前提下缩放完整图片，生成两个 PNG 预览及 `main/marvy_logo.h` / `main/marvy_logo.c`。
转换器依赖 Pillow；执行 `python3 tools/generate_marvy_logo.py --check` 可只检查
生成文件而不写入。固件使用 LVGL RGB565A8 描述符：小端 RGB565 色彩平面后接
独立 A8 平面。两份常驻 Flash 的像素数组共 38,832 字节，无需运行时 PNG 解码器
或整图堆内存分配。

- 使用描述性命名，并记录尺寸、像素格式、转换步骤与目标路径。
- 优先采用适合 240 × 320 RGB565 显示的格式，并纳入 Flash 与内部 RAM 考量。
- 许可允许时保留可编辑源文件，并记录来源与许可。
- 图片中不得包含设备二维码秘密、凭证或个人数据。

## 音乐与音效（music）

可复用的音乐与音效源码放在 `music/`。

- 记录来源、许可、采样率、位深、声道、转换命令与目标路径。
- 与当前 BSP 音频路径匹配时优先采用 16 kHz、16 位单声道 PCM。
- 嵌入音频前评估 Flash 与内部 RAM 成本；长录音应流式或分块。
- 无再分发许可不提交媒体文件。

### Marvy 配乐与采样命中音效

通过 sound 技能的 Freesound 搜索挑选三段素材，均按
[CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/) 使用，可用于本商业礼品
应用。权威元数据保留各自的来源页、作者、获取时快照、原文件及 PCM SHA-256、
转换方法：[`music/marvy_audio.source.json`](music/marvy_audio.source.json)。

| 用途与来源 | 转换源文件 | 内嵌 PCM |
| --- | --- | --- |
| 背景乐：**solarpsychedelic** 的 [adventure](https://freesound.org/s/749008/) | [`fs_749008_adventure.mp3`](music/fs_749008_adventure.mp3)，下载的高质量预览 | [`marvy_bgm.pcm`](music/marvy_bgm.pcm)：177,824 个采样 / 355,648 字节 / 11.114 秒，轻快芯片音乐循环。 |
| 普通命中：**timgormly** 的 [8-bit coin1.1.wav](https://freesound.org/s/162805/) | [`fs_162805_8-bit-coin1-1-wav.mp3`](music/fs_162805_8-bit-coin1-1-wav.mp3)，下载的高质量预览 | [`marvy_hit.pcm`](music/marvy_hit.pcm)：5,917 个采样 / 11,834 字节 / 0.3698125 秒，硬币音效。 |
| Marvy 命中：**SomeGuy22** 的 [8-Bit Powerup](https://freesound.org/s/431329/) | [`fs_431329_8-Bit-Powerup.mp3`](music/fs_431329_8-Bit-Powerup.mp3)，下载的高质量预览 | [`marvy_bonus.pcm`](music/marvy_bonus.pcm)：8,029 个采样 / 16,058 字节 / 0.5018125 秒，不同的升级音效。 |

三份运行文件均为 16 kHz、16 位小端有符号单声道 PCM，合计 **383,540 字节**。
每段解码音频在内嵌前独立归一化至峰值 **27,852**（16 位满幅的 85%）。
MP3 预览只作为制备输入，不编译进固件。

使用 [`music/prepare_audio.py`](music/prepare_audio.py) 重建：

```bash
python3 assets/music/prepare_audio.py
# FFmpeg 不在 PATH 时：
python3 assets/music/prepare_audio.py --ffmpeg /path/to/ffmpeg
```

脚本相对自身定位输入，校验固定的源文件摘要，通过 FFmpeg 解码并归一化 PCM，
更新处理元数据和文件摘要。需要已安装 FFmpeg，或通过 `--ffmpeg` 指定；实际
工具版本会写入元数据。设备端不执行归一化。

应用内嵌 `marvy_bgm_pcm`、`marvy_hit_pcm` 和 `marvy_bonus_pcm`，由一个
**256 采样**的音频任务缓冲区从 Flash 分块播放。运行时背景乐振幅为归一化 PCM
的 1/4，采样命中音效为 1/3；循环接缝使用 4 ms 淡化，音乐开停和 ducking 使用
16 ms 渐变。任意提示音播放时，背景乐再降到 1/4。最终采样限制在 **±16,384**，
独占 codec 的音频任务将主音量设为 **65%**。其他短提示仍为原创合成反馈，不是
正式的 Marvy 声音 logo。背景乐在欢迎页、结算页和闲置暂停时停止。

这些软件设置提高了相较上一轮的数字输出，但不是实际扬声器响度测量。
音乐音量、循环接缝、两种命中音的区分，以及保存分数时的播放效果仍需真机试听。

## 电池参数（battery）

[`battery/wanli-500mah-profile2-xil40x-20261020.txt`](battery/wanli-500mah-profile2-xil40x-20261020.txt)
保存万锂新能源 500 mAh 电芯的 CW2017 参数（profile2、XIL40X、2026-10-20）。
该文件是项目提供的 `喜梨_智能工卡_万锂新能源500mAh_profile2_XIL40X_20261020.txt`
的原样副本，按维护者要求保留用于本次固件更新；原文件没有附带单独的许可说明。

格式为 80 行两位十六进制数字，每行一个字节，按寄存器顺序排列。
BSP 在 `components/bsp/src/bsp_battery.c` 中嵌入这 80 字节，主机检查会验证
源文件及数组内容完全一致。文本文件 SHA-256：
`7ea96eba4634f4495a37c25ffacd54c1deaa0693b162c892d671a2ba68460919`。
解码后 80 字节 SHA-256：
`fa7365671093ef449c5bdcfb4adb279e3d275f888109f50b6b2f21e9b96220d5`。
这是电量计参数，不是充电配置，也不能通用于所有 500 mAh 电芯；详见
[硬件指南](../docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.zh_CN.md#9-cw2017-电池计)。
