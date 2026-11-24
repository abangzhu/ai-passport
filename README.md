**简体中文** · [English](README.en.md)

# Marvy 套羊

为 FoloToy AI Passport 制作的羊年离线套圈小游戏。看准羊群经过落点的时机，按任意功能键扔圈；每局 10 个圈，普通羊得 1 分，带粉蓝渐变 Marvy Logo 的羊得 5 分，跑得也更快。

当前 `dev` 分支迭代只开发「Marvy 套羊」，不包含数羊、单键飞行、官网二维码或 NFC 标签配置。欢迎页、游戏中的 Marvy 羊、捕获动画和结算页角色使用用户提供的 Marvy Logo。羊角色为原创程序绘制；短音效为原创合成，尚未替换为正式的 Marvy 声音 logo。

## 怎么玩

1. 开机显示约 2 秒欢迎页，展示 Marvy Logo 和「Marvy 祝你新年快乐」，然后自动进入游戏；按任意功能键也可跳过欢迎页。
2. 羊群沿同一草地通道左右跑动。圈落地时，根据羊的当前位置判断是否命中；每圈最多套中一只羊。
3. 上、下、确定三个功能键的按下动作都是投圈。长按不会连发，飞行和命中反馈期间重复按键不会多扣圈。
4. 普通羊得 1 分，Marvy 羊得 5 分。普通羊速度为 30–42 px/s，Marvy 羊为 48–56 px/s。套中 Marvy 羊时，显示「+5 分／新年快乐」约 1.2 秒；普通命中与落空反馈为 0.4 秒。
5. 第 10 个圈落地并播放完整反馈后结算，显示本局分数和最高分；按任意功能键立即再来一局。
6. 最高分在完整结算后写入 NVS，正常断电后保留。存储不可用时继续游戏，仅保留本次运行纪录。

游戏中循环播放轻快的芯片音乐风格配乐，音效出现时降低背景乐音量。欢迎页、结算页和闲置暂停时不播放背景乐。闲置 45 秒后背光变暗并暂停游戏；第一下按键恢复亮度，不消耗圈数。右上角显示电量，读数不可用时显示 `--%`。

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

游戏逻辑与硬件界面分离，便于主机测试。正式交付应分别记录构建、主机测试、真机测试与未验证项；构建成功不能代替真机验收。

## 字体与资源

中文界面使用 Noto Sans SC Regular 的 16 / 20 px 固定字符子集，按 OFL 1.1 分发。完整源字体仅用于重建，固件只编译生成的 C 字库。修改显示文字后运行：

```bash
bash tools/generate_marvy_fonts.sh
```

字体来源、许可证、字符清单和 SHA-256 见 [资源目录](assets/README.zh_CN.md)。编译后仍需检查真实屏幕上的字形、排版和动画。

Logo 原图保存在 `assets/images/marvy-logo.png`。执行 `python3 tools/generate_marvy_logo.py` 可生成欢迎页与羊身使用的透明位图，执行 `python3 tools/generate_marvy_logo.py --check` 可检查生成一致性。

背景乐为 ADnova 的 [beepbox #11](https://freesound.org/s/381316/)，按 [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/) 使用；固件内嵌 16 kHz、16 位单声道 PCM，并由音频任务分块播放。来源、转换方法和品牌素材使用范围见资源目录。

## 当前范围与验收

- 原创草地套圈界面、Logo 欢迎页、10 圈结算和快速重开。
- 普通羊 / 更快的 Marvy 羊计分、命中祝福、背景乐与音效、最高分持久化。
- 本轮待真机检查：Logo 清晰度、中文与祝福排版、音乐循环与音效音量、三键手感、刷新流畅度、掉电后纪录和电池读数。
- 如需品牌声音 logo，可替换当前原创合成提示音。

本分支基于 [FoloToy 官方开源开发基础](https://github.com/FoloToy/ai-passport)，复用 `components/bsp` 硬件驱动；应用代码在 `main/`，没有复用原硬件测试菜单作为产品界面。

开发约定见 [AGENTS.md](AGENTS.md)，工程资料索引见 [docs/README.md](docs/README.md)。
