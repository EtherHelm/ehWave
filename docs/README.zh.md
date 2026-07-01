# ehWave

<p align="center">
  <b>OpenFOAM 非侵入式波浪模拟插件</b><br>
  <em>造波 · 消波 · 粘势流耦合</em>
</p>

<p align="center">
  <a href="./README.md"><strong>🌐 English</strong></a> ·
  <a href="./README.zh.md"><strong>📖 简体中文</strong></a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/OpenFOAM-2512-blue?logo=openfoam" alt="OpenFOAM">
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
- [许可证](#-许可证)

---

## 🌊 简介

**ehWave** 是一个面向 **OpenFOAM v2512** 的非侵入式波浪模拟插件，提供造波、消波及粘势流耦合功能。其设计理念是：

- **非侵入** —— 无需修改求解器源码，通过 `fvOptions` 和边界条件即可使用
- **模块化** —— 基于运行时间选择表（RTS）的波浪模型架构，易于扩展
- **高性能** —— 基于 Eigen 的向量化计算，适用于大规模并行模拟

> ⚠ **开发阶段**  
> 当前处于 **alpha** 阶段，API 和接口定义可能发生变化。

---

## 🚀 功能特性

### ✅ 已实现

| 特性 | 描述 |
|------|------|
| **造波边界条件** | `ehWaveVel` / `ehWaveAlpha` 边界条件，支持波浪速度场与相分数场 |
| **EOM 消波技术** | 基于外区域消波模型（External Origin Matching）的松弛区消波，通过 `fvOptions` 实现 |
| **Stokes 深水波浪** | 五阶 Stokes 深水波浪理论（Fenton, 1985） |
| **静水模型** | `CalmWater` 零速度/零波高基准模型 |
| **场初始化工具** | `setWaveField` 命令行工具，基于波形初始化 `U`、`alpha.water` 等场 |
| **模块化架构** | 基于 `waveModel` 基类的运行时选择机制，可轻松添加新波浪模型 |

### 🔄 开发中

- 任意水深非线性规则波
- 高阶谱波浪（HOS）
- 双向粘势耦合
- 更完善的教程和文档

---

## 🔧 快速开始

### 环境依赖

| 依赖 | 版本 |
|------|------|
| OpenFOAM | v2512 |
| C++ 编译器 | GCC 10+ (支持 C++17) |
| CMake | 3.20+ |
| Eigen | 3.4+ (已作为 third_party 子模块包含) |
| MPI | OpenMPI (可选，用于并行计算) |

### 安装步骤

```bash
# 1. 克隆仓库（需 --recursive 以获取 third_party/eigen 子模块）
git clone --recursive https://github.com/EtherHelm/ehWave.git
cd ehWave

# 若已普通克隆，可后续拉取子模块：
# git submodule update --init --recursive

# 2. 配置 OpenFOAM 环境（根据实际安装路径调整）
source /usr/lib/openfoam/openfoam2512/etc/bashrc

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
    waveType        StokesWave;      // 波浪模型类型
    waveHeight      0.05;            // 波浪参数
    wavePeriod      0.8;             // 波浪参数
}
```

当前支持的波浪模型：

| 模型 | 类型名称 | 说明 |
|------|----------|------|
| `StokesWave` | StokesWave | 五阶 Stokes 深水波浪 |
| `CalmWater` | CalmWater | 静水（零速度、零波高） |

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

### 5️⃣ 初始化场

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
- [ ] 任意水深、浪向非线性规则波
- [ ] 高阶谱波浪 (HOS)
- [ ] 双向粘势耦合
- [ ] 完善教程与文档
