**简体中文** · [English](README.en.md)

# Marvy 套羊

为 FoloToy AI Passport 制作的羊年离线套圈小游戏。按住任意功能键蓄力，松手投圈，看准距离和羊群经过的时机。每局 10 个圈，普通羊得 1 分，带粉蓝渐变 Marvy Logo 的羊得 5 分，跑得也更快。

当前 `0.3.0-dev` 只开发「Marvy 套羊」，不包含数羊、单键飞行、官网二维码或 NFC 标签配置。欢迎页保留平滑的 Marvy Logo 和「Marvy 祝你新年快乐」；游戏与结算页采用硬边字库、方块羊和草地的像素风格。游戏中的 Marvy 羊、捕获动画和结算页角色都使用用户提供的 Logo 标记。

## 怎么玩

1. 开机后停留在欢迎页，直到按下任意功能键才进入游戏，不再自动开始。
2. 上、下、确定三个功能键都可以投圈：按下后蓄力，松手才投出并扣一个圈。短按落得近，长按落得远；蓄力条和落点标记会随按住时间变化。
3. 蓄力 0–1400 ms 将落点从左侧 32 px 移到右侧 208 px；超过 1400 ms 保持最远距离，不会自动投圈。羊群沿同一草地通道左右跑动，圈落地时按落点和羊的当前位置判定命中；每圈最多套中一只羊，当前命中容差为 16 px。
4. 用于开始游戏、重开或唤醒的那次松手不会投圈。飞行和命中反馈期间的按键不会额外扣圈；丢失或取消输入时，未投出的蓄力会安全取消。
5. 普通羊得 1 分，速度为 30–42 px/s；Marvy 羊得 5 分，速度为 48–56 px/s。套中 Marvy 羊时，透明文字显示「+5 分／新年快乐」约 1.2 秒，伴随简单像素烟花，没有遮挡场景的实底卡片。普通命中与落空反馈为 0.4 秒。
6. 第 10 个圈落地并播放完整反馈后结算，显示本局分数和最高分；按任意功能键重新开始，再次按住并松手才投出下一局第一个圈。
7. 最高分在完整结算后写入 NVS，正常断电后保留。存储不可用时继续游戏，仅保留本次运行纪录。

游戏中循环播放轻快的芯片音乐；普通命中使用硬币音，Marvy 命中使用不同的升级音，音效出现时降低背景乐音量。欢迎页、结算页和闲置暂停时不播放背景乐。闲置 45 秒后背光变暗并暂停游戏，未释放的蓄力会取消；第一下按键只恢复亮度，其松手不消耗圈数。右上角显示电量，读数不可用时显示 `--%`。

## 开发与验证

目标硬件为 ESP32-C3、8 MB Flash、无 PSRAM、240 × 320 彩屏；使用 ESP-IDF **5.5.3**。应用不需要 Wi-Fi、蓝牙、账号或手机软件。

先按 [环境安装指南](docs/development/engineering/environment-setup.md) 准备并激活 ESP-IDF，再从仓库根目录执行：

```bash
python -m pip install -r tools/requirements-dev.txt
./tools/validate.sh --static
./tools/validate.sh --firmware
./tools/validate.sh
```

完整 gate 会保留可从 `0x0` 写入的合并固件 `build/FoloToy-AI-Passport-full.bin`，以及 `build/firmware/` 下与其匹配的调试归档。不要把仅应用镜像当成 `0x0` 合并固件使用。刷入合并固件可能重置 NVS 中的已有记录；刷机需要另行确认，不自动执行整片擦除。

游戏逻辑、带时间戳的输入配对和硬件界面分离，便于主机测试。正式交付应分别记录构建、主机测试、真机测试与未验证项；构建成功不能代替真机验收。

## 字体与资源

中文界面使用 Noto Sans SC Regular 的固定字符子集，按 OFL 1.1 分发。欢迎页使用 16 / 20 px、4 bpp 的平滑字库，游戏和结算页使用同尺寸、1 bpp 的硬边字库。四份字库各覆盖 95 个可打印 ASCII 与 53 个固定中文字符。完整源字体仅用于重建，固件只编译生成的 C 字库。修改显示文字后运行：

```bash
bash tools/generate_marvy_fonts.sh
```

字体来源、许可证、字符清单和 SHA-256 见 [资源目录](assets/README.zh_CN.md)。编译后仍需检查真实屏幕上的字形、排版和动画。

Logo 原图保存在 `assets/images/marvy-logo.png`。执行 `python3 tools/generate_marvy_logo.py` 可生成欢迎页与羊身使用的透明位图，执行 `python3 tools/generate_marvy_logo.py --check` 可检查生成一致性。

背景乐为 solarpsychedelic 的 [adventure](https://freesound.org/s/749008/)，普通命中使用 timgormly 的 [8-bit coin1.1.wav](https://freesound.org/s/162805/)，Marvy 命中使用 SomeGuy22 的 [8-Bit Powerup](https://freesound.org/s/431329/)，三项均按 [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/) 使用。固件内嵌 16 kHz、16 位单声道 PCM，由音频任务分块播放；输出音量和数字混音增益已提高。来源、归一化与重建方法、品牌素材使用范围见资源目录。

## 当前范围与验收

- 等待按键的 Logo 欢迎页，以及像素风游戏与结算页。
- 蓄力控制距离、10 圈结算、普通羊／更快的 Marvy 羊计分、透明祝福和像素烟花。
- 区分普通／Marvy 命中的采样音效、背景乐、最高分持久化。
- 本轮待真机检查：欢迎页等待与按键释放、蓄力条和实际落点、Logo 与中文清晰度、祝福和烟花是否遮挡、音乐循环与音量、刷新流畅度、掉电后纪录和电池读数。
- 其他短提示音仍为原创合成；如需正式品牌声音 logo，可另行提供替换素材。

本分支基于 [FoloToy 官方开源开发基础](https://github.com/FoloToy/ai-passport)，复用 `components/bsp` 硬件驱动；应用代码在 `main/`，没有复用原硬件测试菜单作为产品界面。

开发约定见 [AGENTS.md](AGENTS.md)，工程资料索引见 [docs/README.md](docs/README.md)。
