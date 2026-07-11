# ehWave

<p align="center">
  <b>OpenFOAM 非侵入式波浪模拟插件</b><br>
  <em>造波 · 消波 · 黏-势流耦合</em>
</p>

<p align="center">
  <a href="./README.md"><strong>🌐 English</strong></a> ·
  <a href="./README.zh.md"><strong>📖 简体中文</strong></a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/OpenFOAM-2206%2B-blue?logo=openfoam" alt="OpenFOAM">
  <img src="https://img.shields.io/badge/C++-17-blue?logo=cplusplus" alt="C++17">
  <img src="https://img.shields.io/badge/license-GPLv3-green" alt="License">
  <img src="https://img.shields.io/badge/status-alpha-yellow" alt="Status">
</p>

---

## 📋 目录

- [简介](#-简介)
- [功能特性](#-功能特性)
- [快速开始](#-快速开始)
- [使用指南](#-使用指南)
- [示例](#-示例)
- [开发进度](#-开发进度)
- [贡献与反馈](#-贡献与反馈)
- [许可证](#-许可证)

---

## 🌊 简介

**ehWave** 是一个面向 **OpenFOAM v2206 及之后版本** 的非侵入式波浪模拟插件，提供造波、消波及黏-势流耦合功能。其设计理念是：

- **非侵入** —— 无需修改求解器源码，通过 `fvOptions` 和边界条件即可使用
- **模块化** —— 基于运行时选择表（RTS）的波浪模型架构，易于扩展
- **高性能** —— 基于 Eigen 的向量化计算，适用于大规模并行模拟

> ⚠ **开发阶段**  
> 当前处于 **alpha** 阶段，API 和接口定义可能发生变化。

---

## 🚀 功能特性

### ✅ 已实现

| 特性 | 描述 |
|------|------|
| **造波边界条件** | `ehWaveVel` / `ehWaveAlpha` 边界条件，分别控制波浪速度场与相分数场 |
| **EOM 消波技术** | 基于 Euler Overlay Method 的消波模型，通过 `fvOptions` 实现，有效吸收出口波浪 |
| **黏-势流双向耦合** | 远场使用非线性势流求解器，近场使用黏流求解器，大幅扩展有效计算域，显著节省计算资源 |
| **静水模型** | `CalmWater` 零速度/零波高基准模型 |
| **非线性规则波** | 深水和浅水条件下的非线性周期性波浪，涵盖 Stokes 波、椭圆余弦波等，采用 Clamond & Dutykh (2017) 方法计算 |
| **高阶谱波浪（HOS）** | 基于高阶谱方法（HOS）与周期基本解（PFS）方法的非线性波浪演化，支持任意浪向的深水波、浅水波、长峰波及短峰不规则波 |
| **场初始化工具** | `setWaveField` 命令行工具，基于波形初始化 `U`、`alpha.water` 等场 |
| **模块化架构** | 基于 `waveModel` 基类的运行时选择机制，可轻松添加新波浪模型 |

### 🔄 待完成

- 更完善的文档和注释
- 更多教程算例
- 性能优化

---

## 🔧 快速开始

### 环境依赖

| 依赖 | 版本 |
|------|------|
| OpenFOAM | v2206+ |
| C++ 编译器 | GCC 10+ (支持 C++17) |
| CMake | 3.10+ |
| Eigen | 3.4+ (已作为 third_party 子模块包含) |

### 安装步骤

```bash
# 1. 克隆仓库（需 --recursive 以获取 third_party/eigen 子模块）
git clone --recursive https://github.com/EtherHelm/ehWave.git
cd ehWave

# 若已普通克隆，可后续拉取子模块：
# git submodule update --init --recursive

# 2. 配置 OpenFOAM 环境（根据实际安装路径调整）
source /usr/lib/openfoam/openfoamXXXX/etc/bashrc   # 例如 openfoam2512、openfoam2406 等

# 3. 构建
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

构建完成后，`bin/` 目录下会生成以下产出物：

| 产出物 | 说明 |
|--------|------|
| `libehWave.so` | 核心动态库（包含波浪模型、边界条件和消波模块） |
| `setWaveField` | 场初始化工具（通过 `dlopen` 加载 `libehWave.so`） |

---

## 📖 使用指南

### 1️⃣ 加载库

在 `system/controlDict` 中添加：

```cpp
libs            ("./libehWave.so");
```

### 2️⃣ 定义波浪

在 `constant/ehWaveProperties` 中配置波浪参数：

```cpp
wave1
{
    waveType        RegularWave;      // 波浪模型类型
    waveLength      1.0;              // 波浪参数
    waterDepth      2.0;              // 波浪参数
    waveHeight      0.05;             // 波浪参数
    waveCrestIni     0.333;           // 波浪参数
    waveDirection   (1 1 0);          // 波浪参数
}
```

当前支持的波浪模型：

| 类型名称 | 说明 |
|----------|------|
| CalmWater | 静水（零速度、零波高） |
| RegularWave | 任意水深非线性周期性波浪，涵盖 Stokes 波、椭圆余弦波等 |
| StokesWave | 五阶 Stokes 深水波浪教程示例，展示用户自定义波浪模型的实现方法。对于大波陡波浪的模拟，建议使用精度更高的 RegularWave 模型 |
| GPLWave | 基于 GPL 求解器的黏-势流耦合波浪模型，支持双向及单向耦合模式 |

### 3️⃣ 配置边界条件

在 `0/U` 文件中设置波浪入口/出口速度边界：

```cpp
inlet
{
    type            ehWaveVel;       // 波浪速度边界
    waveName        wave1;           // 引用的波浪名称
}

outlet
{
    type            ehWaveVel;
    waveName        wave1;
}
```

在 `0/alpha.water` 文件中设置相分数边界：

```cpp
inlet
{
    type            ehWaveAlpha;     // 波浪相分数边界
    waveName        wave1;
}

outlet
{
    type            ehWaveAlpha;
    waveName        wave1;
}
```

### 4️⃣ 配置消波区 (EOM)

在 `constant/fvOptions` 中配置 EOM 松弛区：

```cpp
relax
{
    type            ehEOM;           // EOM 消波模型
    active          on;
    zones                       // 松弛区定义
    (
        { direction (1 0 0);   width 0.5; origin (0 0 0); }
        { direction (-1 0 0);  width 0.5; origin (6 0 0); }
    );
    waveName        wave1;           // 目标波浪
    coeffVel        0.5;             // 速度松弛系数
    characteristicFreq  7.8540;      // 特征频率 [rad/s]
    alphaRelax      false;           // 是否松弛相分数
    coeffAlpha      0.1;             // 相分数松弛系数
    selectionMode   all;             // 选择模式
}
```

### 5️⃣ 配置黏-势流双向耦合（可选）

在 `constant/ehWaveProperties` 中启用双向耦合（仅 `GPLWave` 模型需要）：

```cpp
wave1
{
    waveType        GPLWave;
    // ...波浪参数...
    ehTwoWayCoupling
    {
        type            ehTwoWayCoupling;
        enabled         true;
        waveName        wave1;
    }
}
```

### 6️⃣ 初始化波浪场

使用 `setWaveField` 工具初始化波浪场：

```bash
# 在算例目录下
./setWaveField
```

该工具会根据 `system/setWaveFieldDict` 中的配置自动计算并设置 `U` 和 `alpha.water` 的初始场。

`system/setWaveFieldDict` 配置示例：

```cpp
waveName        wave1;               // 使用 ehWaveProperties 中定义的波浪
```

---

## 📁 示例

| 示例 | 路径 | 说明 |
|------|------|------|
| 二维 Stokes 波浪 | `examples/stokes_wave_2D/` | 二维 Stokes 深水波浪造波-消波案例 |
| 黏-势流单向耦合 | `examples/GPL_one_way_coupling/` | 非线性势流求解器生成波浪，单向耦合至黏流求解器进行传播模拟 |

运行示例：

```bash
cd examples/stokes_wave_2D
bash run.sh
```

---

## 🗺️ 开发进度

- [x] 整体框架建立
- [x] 造波边界条件 (`ehWaveVel`, `ehWaveAlpha`)
- [x] EOM 消波技术
- [x] Stokes 深水波浪 (五阶)
- [x] 高阶谱波浪 (HOS)
- [x] 双向黏-势流耦合
- [x] 任意水深、浪向非线性规则波
- [ ] 完善教程与文档
- [ ] GPLServer 代码开源

---

## 🤝 贡献与反馈

欢迎通过以下方式参与项目：

- **报告 Bug** —— 若在使用过程中遇到任何问题，请在 [GitHub Issues](https://github.com/EtherHelm/ehWave/issues) 提交错误报告，并提供复现步骤及运行环境信息。
- **适配需求** —— 若您需要其他 OpenFOAM 版本或编译环境的适配支持，欢迎联系作者或提交 Issue 说明需求。
- **功能建议** —— 如有新功能构想或改进建议，欢迎发起讨论。

---

## 📄 许可证

本项目基于 **GNU General Public License v3.0 (GPLv3)** 发布。详见 [LICENSE](../LICENSE) 文件。