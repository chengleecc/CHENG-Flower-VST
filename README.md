# CHENG Flower v3

> v3、v2、v1 是互相独立的插件（名称 "CHENG Flower v3"，不同的插件 ID），可以同时装在 Cubase 里对比。

## v3 有什么不同（v2 太 aggressive）
- **COLOR 默认 15%，并且跟着歌曲音量自动校准**：不再因为母带接近 0 dBFS 就被推爆。默认设置下，-1 dBFS 的信号谐波约 0.3%。
- **Dense / Thick 的饱和约减半**：Dense 50% ≈ 2% 谐波，100% ≈ 6%（v2 是 19%）。Thick 的 fat 区要到 85% 以上才出现。
- **不刺耳**：Color / Dense / Thick 饱和前先把 3.5 kHz 以上压低 6 dB，饱和后再补回去，谐波主要来自中低频。Dense 的 air 降到 +1.2 dB，Shine 谐波降低约 30%。
- **旋钮加速变温和**：推到底是原来的 1.4 倍（v2 是 2.2 倍）。
- **Track 模式限幅器加入 0.5 ms 预读**：不再直接削波（延迟约 0.6 ms）。
- Sweet 不变（Pentode / Tape / Hot Tube 还是可以很脏）。

一键提升整体音质的插件（VST3, Windows x64）。可以挂在 Master、Group 或单轨上。

## v2 加入的东西
- **CHENG Color（常驻）**：铁芯变压器低频饱和 + class-A 放大器，左右声道有不同的元件误差（模拟设备的立体感）。（v3 默认 15%），开到 0 就是干净的。
- **Open = 3D**：除了去堆积，还会把中间的高频做去相关后放进两边（单声道兼容），再加一点 pentode 电子管的饱满感，方向参考 THE OVEN。
- **Dense = 推亮推满**：并联 pentode + triode 饱和、enhance 式软削波、14 kHz air，方向参考 HG-2 + 限幅器 enhance。
- **THICK（新）**：Inflator 曲线（小声的部分最多抬 3.5 dB，峰值不变）+ Neve 式低频铁芯；超过 60% 进入 fat 区，方向参考 Dada Life。
- **SWEET**：Pentode / Tape / Hot Tube，可以推到很脏（100% 约 19–36% 谐波）。
- **旋钮后半段加速**：前半段细腻，后半段约是 v1 的 2.2 倍。
- **MATCH 默认关闭**：需要公平比较时再打开。

信号流程：De-Harsh / Open 动态去堆积 → Open ↔ Dense 三段瞬态 / 密度处理 → Low End（M/S）→ Focus → Width → Shine → Color / Dense 驱动 / Thick / Sweet（过采样）→ Mix → 自动音量匹配 → Output → Limiter。

## 安装 (Cubase 14, Windows)
1. 到 GitHub 的 **Actions** 页面，打开最新一次成功的 "Build Windows VST3"。
2. 在页面底部 Artifacts 下载 `ChengFlower-v3-VST3-Windows-x64`，解压。
3. 把 `CHENG Flower v3.vst3` 整个文件夹复制到 `C:\Program Files\Common Files\VST3\`。
4. 打开 Cubase → Studio → VST Plug-in Manager → Rescan，插件会出现在 "CHENG" 下。

## 控件
| 控件 | 作用 |
|---|---|
| **花朵（Open ↔ Dense）** | 上下拖动花朵。往 **Open**：动态切掉 180 Hz–4 kHz 的堆积和共振，起音更清楚、尾音更短，稍微变宽，歌会"松开"。往 **Dense**：三段上 / 下压缩（围绕歌曲本身的平均音量），压住尖峰、拉长延音，更饱满紧致。双击回到 0，按住 Shift 拖动可微调。 |
| **SHINE** | 旋律亮度：只在 2.2 kHz 以上生成偶次谐波，再加一点 10 kHz Air。内置 FFT 分析：有音高的内容才亮，镲片 / 齿音多的时候会自动收回。 |
| **LOW END** | 120 Hz 以下变单声道，低频段自适应压缩让它更紧，60 Hz 加一点力度，150–600 Hz 补谐波（小喇叭也听得到贝斯）。 |
| **SWEET** + PENTODE / TAPE / HOT TUBE | 饱和，Track 模式 2x、Master 模式 4x 过采样。Pentode 厚实向前；Tape 有高频压缩、60 Hz head bump 和磁滞感；Hot Tube 会随音量改变偏压，越推越脏、会"下沉"。 |
| **THICK** | Inflator 曲线 + 低频铁芯，60% 以上加入 fat 驱动。 |
| **COLOR** | CHENG 的常驻模拟颜色，0 = 干净。 |
| **FOCUS** | 中间 2.6 kHz 有人声时，自动把两边伴奏的同一频段让出来最多 6 dB，中间再加一点 presence。 |
| **DE-HARSH** | 动态去刺耳：只在 1.8–10 kHz 某个频段突出时才切。 |
| **WIDTH** | 只加宽 300 Hz 以上的 Side，单声道兼容不受影响。 |
| **TRACK / MASTER** | Track：约 0.6 ms 延迟，适合单轨和 Group。Master：4x FIR 线性相位过采样 + 1.5 ms 预读限幅器 + 近似 True Peak 检测，延迟约 2.8 ms。 |
| **MATCH** | 默认关闭。打开后把输出响度对齐输入（K-weighting，约 2 秒），用来公平比较。 |
| **OUTPUT / CEILING / LIMIT / MIX** | 输出增益、限幅上限、限幅开关、干湿比（干声有延迟补偿）。 |
| **BYPASS** | 延迟补偿后的旁通，和 Cubase 的旁通按钮是同一个参数。 |

下方频谱：▼ 表示检测到的堆积频段，金色条表示正在动态清理的频段。

除了 COLOR，所有旋钮在 0 的时候对应模块完全不动。

## Build locally
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target ChengFlower_VST3
```
Tests: `cmake -B build -DFLOWER_BUILD_TESTS=ON && cmake --build build --target FlowerTests && ./build/FlowerTests_artefacts/Release/FlowerTests`
UI snapshot: `FlowerTests --snapshot out.png [bloom]`
