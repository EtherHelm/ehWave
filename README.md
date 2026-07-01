# ehWave

<p align="center">
  <em>OpenFOAM plugin for wave generation, wave absorption, and viscous–potential flow coupling</em>
</p>

<p align="center">
  <strong>Non-intrusive wave simulation toolkit for OpenFOAM</strong>
</p>

<p align="center">
  <a href="./README.md"><strong>📖 English</strong></a> ·
  <a href="./docs/README.zh.md"><strong>🌐 简体中文</strong></a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/OpenFOAM-2512-blue?logo=openfoam" alt="OpenFOAM">
  <img src="https://img.shields.io/badge/C++-17-blue?logo=cplusplus" alt="C++17">
  <img src="https://img.shields.io/badge/license-GPLv3-green" alt="License">
  <img src="https://img.shields.io/badge/status-alpha-yellow" alt="Status">
</p>

---

## 📋 Table of Contents

- [Introduction](#-introduction)
- [Features](#-features)
- [Quick Start](#-quick-start)
- [Usage Guide](#-usage-guide)
- [Examples](#-examples)
- [Roadmap](#-roadmap)
- [License](#-license)

---

## 🌊 Introduction

**ehWave** is a non-intrusive wave simulation plugin for **OpenFOAM v2512**, providing wave generation, wave absorption via **EOM** (External Origin Matching), and viscous–potential flow coupling capabilities. It is designed around three core principles:

- **Non-intrusive** — Works with existing solvers out of the box via `fvOptions` and boundary conditions — no solver source modification required.
- **Modular** — Built on a runtime selection table (RTS) architecture centered around the `waveModel` base class, making it straightforward to add new wave theories.
- **High-performance** — Leverages Eigen for vectorized computations, suitable for large-scale parallel simulations.

> ⚠ **Development Status**  
> Currently in **alpha** stage — interfaces and APIs are subject to change.

---

## 🚀 Features

### ✅ Implemented

| Feature | Description |
|---------|-------------|
| **Wave BCs** | `ehWaveVel` / `ehWaveAlpha` boundary conditions for velocity and phase fraction fields |
| **EOM Absorption** | Relaxation-zone wave absorption via `fvOptions` using the External Origin Matching technique |
| **Stokes Deep-Water Wave** | 5th-order Stokes deep-water wave theory (Fenton, 1985) |
| **Calm Water Model** | `CalmWater` — zero-velocity, zero-elevation baseline model |
| **Field Initialization** | `setWaveField` CLI tool to initialize `U` and `alpha.water` fields from wave theory |
| **Modular Architecture** | RTS-based `waveModel` hierarchy — easily extendable with new wave types |

### 🔄 In Progress

- Finite-depth nonlinear regular waves
- High-Order Spectral (HOS) waves
- Two-way viscous–potential coupling
- Tutorials and comprehensive documentation

---

## 🔧 Quick Start

### Prerequisites

| Dependency | Version |
|------------|---------|
| OpenFOAM | v2512 |
| C++ Compiler | GCC 10+ (C++17 support) |
| CMake | 3.20+ |
| Eigen | 3.4+ (included as a third_party submodule) |
| MPI | OpenMPI (optional, for parallel runs) |

### Installation

```bash
# 1. Clone the repository (--recursive is required for the third_party/eigen submodule)
git clone --recursive https://github.com/EtherHelm/ehWave.git
cd ehWave

# If you already cloned without --recursive, run:
# git submodule update --init --recursive

# 2. Source the OpenFOAM environment (adjust path as needed)
source /usr/lib/openfoam/openfoam2512/etc/bashrc

# 3. Build
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

After a successful build, the following artifacts are placed in `bin/`:

| Artifact | Description |
|----------|-------------|
| `libehWave.so` | Core shared library (wave models, BCs, absorption module) |
| `setWaveField` | Field initialization tool (loads `libehWave.so` via `dlopen`) |

---

## 📖 Usage Guide

### 1️⃣ Load the Library

Add to `system/controlDict`:

```cpp
libs            ("./libehWave.so");
```

### 2️⃣ Define a Wave Model

Configure the wave in `constant/ehWaveProperties`:

```cpp
wave1
{
    waveType        StokesWave;      // Wave model type
    waveHeight      0.05;            // Wave parameters
    wavePeriod      0.8;             // Wave parameters
}
```

Supported wave models:

| Model | Type Name | Description |
|-------|-----------|-------------|
| `StokesWave` | StokesWave | 5th-order Stokes deep-water wave |
| `CalmWater` | CalmWater | Calm water (zero velocity, zero elevation) |

### 3️⃣ Set Boundary Conditions

In `0/U`, define the wave velocity at inlet/outlet:

```cpp
inlet
{
    type            ehWaveVel;
    waveName        wave1;
}

outlet
{
    type            ehWaveVel;
    waveName        wave1;
}
```

In `0/alpha.water`, define the phase fraction boundary:

```cpp
inlet
{
    type            ehWaveAlpha;
    waveName        wave1;
}

outlet
{
    type            ehWaveAlpha;
    waveName        wave1;
}
```

### 4️⃣ Configure Wave Absorption (EOM)

Add the EOM relaxation zone in `constant/fvOptions`:

```cpp
relax
{
    type            ehEOM;
    active          on;
    zones
    (
        { direction (1 0 0);   width 0.5; origin (0 0 0); }
        { direction (-1 0 0);  width 0.5; origin (6 0 0); }
    );
    waveName        wave1;
    coeffVel        0.5;
    characteristicFreq  7.8540;
    alphaRelax      false;
    coeffAlpha      0.1;
    selectionMode   all;
}
```

### 5️⃣ Initialize Fields

Run the `setWaveField` tool from your case directory:

```bash
./setWaveField
```

The tool reads `system/setWaveFieldDict` to determine which wave to use, then computes and sets the initial fields for `U` and `alpha.water`.

`system/setWaveFieldDict` example:

```cpp
waveName        wave1;
```

---

## 📁 Examples

| Example | Path | Description |
|---------|------|-------------|
| 2D Stokes Wave | `examples/stokes_wave_2D/` | 2D Stokes deep-water wave generation & absorption |

Run an example:

```bash
cd examples/stokes_wave_2D
bash run.sh
```

---

## 🗺️ Roadmap

- [x] Core framework
- [x] Wave boundary conditions (`ehWaveVel`, `ehWaveAlpha`)
- [x] EOM wave absorption
- [x] Deep-water Stokes wave (5th order)
- [ ] Finite-depth nonlinear regular waves
- [ ] High-Order Spectral (HOS) waves
- [ ] Two-way viscous–potential coupling
- [ ] Tutorials and comprehensive documentation
- [ ] CI/CD automated testing

---

## 📄 License

ehWave is released under the **GNU General Public License v3 (GPLv3)**. See [LICENSE](./LICENSE) for details.

Third-party dependencies:

- **Eigen** — licensed under MPL2 (see `third_party/eigen/`)

