# ehWave

<p align="center">
  <b>Non-intrusive Wave Simulation Toolkit for OpenFOAM</b><br>
  <em>Wave Generation · Wave Absorption · Viscous–Potential Coupling</em>
</p>

<p align="center">
  <a href="./docs/README.md"><strong>📖 English</strong></a> ·
  <a href="./docs/README.zh.md"><strong>🌐 简体中文</strong></a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/OpenFOAM-2206%2B-blue?logo=openfoam" alt="OpenFOAM">
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
- [Contributions & Feedback](#-contributions--feedback)
- [License](#-license)

---

## 🌊 Introduction

**ehWave** is a non-intrusive wave simulation toolkit for **OpenFOAM v2206 and later**, providing wave generation, wave absorption, and viscous–potential flow coupling. It is designed around three core principles:

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
| **Wave Boundary Conditions** | `ehWaveVel` / `ehWaveAlpha` boundary conditions for wave velocity and phase fraction fields |
| **EOM Wave Absorption** | Euler Overlay Method (EOM) based wave absorption via `fvOptions`, effectively dampening waves at the outlet |
| **Viscous–Potential Bidirectional Coupling** | Far-field nonlinear potential flow solver coupled with near-field viscous solver, greatly extending the effective computational domain and significantly reducing computational cost |
| **Calm Water Model** | `CalmWater` — zero-velocity, zero-elevation baseline model |
| **Nonlinear Regular Waves** | Nonlinear periodic waves in deep and shallow water, covering Stokes waves, cnoidal waves, etc., computed using the method of Clamond & Dutykh (2017) |
| **High-Order Spectral (HOS) Waves** | Nonlinear wave evolution based on the High-Order Spectral (HOS) method and Periodic Fundamental Solution (PFS) method, supporting deep-water, shallow-water, long-crested, and short-crested irregular waves at arbitrary directions |
| **Field Initialization Tool** | `setWaveField` CLI tool for initializing `U`, `alpha.water`, and other fields from wave theory |
| **Modular Architecture** | RTS-based `waveModel` hierarchy — easily extendable with new wave types |

### 🔄 In Progress

- More comprehensive documentation and comments
- Additional tutorial cases
- Performance optimization

---

## 🔧 Quick Start

### Prerequisites

| Dependency | Version |
|------------|---------|
| OpenFOAM | v2206+ |
| C++ Compiler | GCC 10+ (C++17 support) |
| CMake | 3.20+ |
| Eigen | 3.4+ (included as a `third_party` submodule) |

### Installation

```bash
# 1. Clone the repository (--recursive is required for the third_party/eigen submodule)
git clone --recursive https://github.com/EtherHelm/ehWave.git
cd ehWave

# If you already cloned without --recursive, run:
# git submodule update --init --recursive

# 2. Source the OpenFOAM environment (adjust path to match your version)
source /usr/lib/openfoam/openfoamXXXX/etc/bashrc   # e.g., openfoam2512, openfoam2406, etc.

# 3. Build
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

After a successful build, the following artifacts are placed in `bin/`:

| Artifact | Description |
|----------|-------------|
| `libehWave.so` | Core shared library (wave models, boundary conditions, and absorption module) |
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
    waveType        RegularWave;      // Wave model type
    waveLength      1.0;              // Wave parameters
    waterDepth      2.0;              // Wave parameters
    waveHeight      0.05;             // Wave parameters
    waveCrestIni    0.333;            // Wave parameters
    waveDirection   (1 1 0);          // Wave parameters
}
```

Supported wave models:

| Type Name | Description |
|-----------|-------------|
| `CalmWater` | Calm water (zero velocity, zero elevation) |
| `RegularWave` | Nonlinear periodic waves at arbitrary water depths, covering Stokes waves, cnoidal waves, etc. |
| `StokesWave` | 5th-order Stokes deep-water wave — a tutorial example demonstrating how to implement custom wave models. For steep waves, the more accurate `RegularWave` model is recommended. |
| `GPLWave` | Viscous–potential coupled wave model based on the GPL solver, supporting both one-way and two-way coupling |

### 3️⃣ Set Boundary Conditions

In `0/U`, define the wave velocity at inlet/outlet:

```cpp
inlet
{
    type            ehWaveVel;       // Wave velocity boundary condition
    waveName        wave1;           // Referenced wave name
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
    type            ehWaveAlpha;     // Wave phase fraction boundary condition
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
    type            ehEOM;           // EOM wave absorption model
    active          on;
    zones                              // Relaxation zone definitions
    (
        { direction (1 0 0);   width 0.5; origin (0 0 0); }
        { direction (-1 0 0);  width 0.5; origin (6 0 0); }
    );
    waveName        wave1;           // Target wave
    coeffVel        0.5;             // Velocity relaxation coefficient
    characteristicFreq  7.8540;      // Characteristic frequency [rad/s]
    alphaRelax      false;           // Whether to relax the phase fraction
    coeffAlpha      0.1;             // Phase fraction relaxation coefficient
    selectionMode   all;             // Selection mode
}
```

### 5️⃣ Configure Viscous–Potential Bidirectional Coupling (Optional)

Enable two-way coupling in `constant/ehWaveProperties` (required only for the `GPLWave` model):

```cpp
wave1
{
    waveType        GPLWave;
    // ...wave parameters...
    ehTwoWayCoupling
    {
        type            ehTwoWayCoupling;
        enabled         true;
        waveName        wave1;
    }
}
```

### 6️⃣ Initialize Fields

Run the `setWaveField` tool from your case directory:

```bash
# Run from the case directory
./setWaveField
```

The tool reads `system/setWaveFieldDict` to determine which wave to use, then computes and sets the initial fields for `U` and `alpha.water`.

`system/setWaveFieldDict` example:

```cpp
waveName        wave1;               // Use the wave defined in ehWaveProperties
```

---

## 📁 Examples

| Example | Path | Description |
|---------|------|-------------|
| 2D Stokes Wave | `examples/stokes_wave_2D/` | 2D Stokes deep-water wave generation and absorption |
| One-Way Viscous–Potential Coupling | `examples/GPL_one_way_coupling/` | Nonlinear potential flow solver generates waves, one-way coupled into the viscous solver for propagation |

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
- [x] Stokes deep-water wave (5th order)
- [x] High-Order Spectral (HOS) waves
- [x] Two-way viscous–potential coupling
- [x] Nonlinear regular waves at arbitrary water depths and directions
- [ ] Improved tutorials and documentation
- [ ] Open-source GPLServer code

---

## 🤝 Contributions & Feedback

We welcome your contributions and feedback!

- **Report Bugs** — If you encounter any issues, please file a bug report on [GitHub Issues](https://github.com/EtherHelm/ehWave/issues) with reproduction steps and environment details.
- **Porting Requests** — If you need support for a different OpenFOAM version or build environment, feel free to contact the authors or open an issue describing your requirements.
- **Feature Suggestions** — Ideas for new features or improvements are always welcome — start a discussion!

---

## 📄 License

ehWave is released under the **GNU General Public License v3.0 (GPLv3)**. See [LICENSE](../LICENSE) for details.

