# CHENG Flower

一键提升整体音质的插件（VST3, Windows x64）。可以挂在 Master、Group 或单轨上。

信号流程：De-Harsh / Open 动态去堆积 → Open ↔ Dense 三段瞬态 / 密度处理 → Low End（M/S）→ Focus → Width → Shine → Sweet 饱和（过采样）→ Mix → 自动音量匹配 → Output → Limiter。

## 安装 (Cubase 14, Windows)
1. 到 GitHub 的 **Actions** 页面，打开最新一次成功的 "Build Windows VST3"。
2. 在页面底部 Artifacts 下载 `ChengFlower-VST3-Windows-x64`，解压。
3. 把 `CHENG Flower.vst3` 整个文件夹复制到 `C:\Program Files\Common Files\VST3\`。
4. 打开 Cubase → Studio → VST Plug-in Manager → Rescan，插件会出现在 "CHENG" 下。

## 控件
| 控件 | 作用 |
|---|---|
| **花朵（Open ↔ Dense）** | 上下拖动花朵。往 **Open**：动态切掉 180 Hz–4 kHz 的堆积和共振，起音更清楚、尾音更短，稍微变宽，歌会"松开"。往 **Dense**：三段上 / 下压缩（围绕歌曲本身的平均音量），压住尖峰、拉长延音，更饱满紧致。双击回到 0，按住 Shift 拖动可微调。 |
| **SHINE** | 旋律亮度：只在 2.2 kHz 以上生成偶次谐波，再加一点 10 kHz Air。内置 FFT 分析：有音高的内容才亮，镲片 / 齿音多的时候会自动收回。 |
| **LOW END** | 120 Hz 以下变单声道，低频段自适应压缩让它更紧，60 Hz 加一点力度，150–600 Hz 补谐波（小喇叭也听得到贝斯）。 |
| **SWEET** + TUBE / TAPE / SILK | 甜美饱和，Track 模式 2x、Master 模式 4x 过采样。小声的地方多加一点、大声的地方自动收敛。Tube 温暖（偶次谐波），Tape 柔和（高频压缩 + 低频 bump），Silk 只作用在 1.6 kHz 以上。 |
| **FOCUS** | 中间 2.6 kHz 有人声时，自动把两边伴奏的同一频段让出来最多 6 dB，中间再加一点 presence。 |
| **DE-HARSH** | 动态去刺耳：只在 1.8–10 kHz 某个频段突出时才切。 |
| **WIDTH** | 只加宽 300 Hz 以上的 Side，单声道兼容不受影响。 |
| **TRACK / MASTER** | Track：约 4 个采样的延迟，适合单轨和 Group。Master：4x FIR 线性相位过采样 + 1.5 ms 预读限幅器 + 近似 True Peak 检测，延迟约 2.8 ms。 |
| **AUTO GAIN** | 默认开启，把输出响度对齐输入（K-weighting，约 2 秒），这样你听到的是音质变化，不是音量变大的错觉。 |
| **OUTPUT / CEILING / LIMIT / MIX** | 输出增益、限幅上限、限幅开关、干湿比（干声有延迟补偿）。 |
| **BYPASS** | 延迟补偿后的旁通，和 Cubase 的旁通按钮是同一个参数。 |

下方频谱：▼ 表示检测到的堆积频段，金色条表示正在动态清理的频段。

所有旋钮在 0 的时候对应模块完全不动（连分频器的相位都不会改变）。

## Build locally
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target ChengFlower_VST3
```
Tests: `cmake -B build -DFLOWER_BUILD_TESTS=ON && cmake --build build --target FlowerTests && ./build/FlowerTests_artefacts/Release/FlowerTests`
UI snapshot: `FlowerTests --snapshot out.png [bloom]`
