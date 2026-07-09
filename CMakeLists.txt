cmake_minimum_required(VERSION 3.20)
project(ehWave LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

add_definitions(
    -DOPENFOAM=2512
    -DWM_DP
    -DWM_LABEL_SIZE=32
    -DNoRepository
)

add_compile_options(
    -m64
    -pthread
    -frounding-math
    $<$<COMPILE_LANGUAGE:CXX>:-ftemplate-depth=100>
)

set(NNG_TESTS OFF CACHE BOOL "Disable nng tests" FORCE)
set(NNG_ENABLE_NNGCAT OFF CACHE BOOL "Disable nngcat" FORCE)
add_subdirectory(third_party/nng-1.11)

include_directories(
    ./inc
    ./third_party/eigen
    /usr/lib/openfoam/openfoam2512/src/OpenFOAM/lnInclude
    /usr/lib/openfoam/openfoam2512/src/finiteVolume/lnInclude
    /usr/lib/openfoam/openfoam2512/src/meshTools/lnInclude
    /usr/lib/openfoam/openfoam2512/src/fvOptions/lnInclude
    /usr/lib/openfoam/openfoam2512/src/ODE/lnInclude
    /usr/lib/openfoam/openfoam2512/src/dynamicMesh/lnInclude
    /usr/lib/openfoam/openfoam2512/src/surfMesh/lnInclude
    /usr/lib/openfoam/openfoam2512/src/fileFormats/lnInclude
    /usr/lib/openfoam/openfoam2512/src/sampling/lnInclude
    /usr/lib/openfoam/openfoam2512/src/conversion/lnInclude
    /usr/lib/openfoam/openfoam2512/src/OSspecific/POSIX/lnInclude
)

set(OF_LIB_DIR /usr/lib/openfoam/openfoam2512/platforms/linux64GccDPInt32Opt/lib)

add_library(ehWave SHARED
    src/ehWaveVel.cxx
    src/ehWaveAlpha.cxx
    src/ehEOM.cxx
    src/waveModel.cxx
    src/waveModels/StokesWave.cxx
    src/waveModels/GPLWave.cxx
    src/waveModels/CalmWater.cxx
    src/waveModels/RegularWave.cxx
    src/setWaveFieldFunc.cxx
    src/ehFreeSurfaceCoupling.cxx
)

target_link_options(ehWave PRIVATE
    -Wl,--add-needed
    -Wl,--no-as-needed
)

target_link_directories(ehWave PRIVATE ${OF_LIB_DIR})

target_link_libraries(ehWave PRIVATE
    OpenFOAM
    finiteVolume
    meshTools
    fvOptions
    sampling
)

target_link_libraries(ehWave PRIVATE nng)
target_compile_definitions(ehWave PRIVATE NNG_STATIC_LIB)
target_compile_definitions(ehWave PRIVATE NNG_ELIDE_DEPRECATED)

# --- setWaveField utility ---
# Thin wrapper that loads libehWave.so via dlopen at runtime.
add_executable(setWaveField
    src/setWaveField.cxx
)
target_link_directories(setWaveField PRIVATE
    ${OF_LIB_DIR}
    ${OF_LIB_DIR}/sys-openmpi
)
target_link_options(setWaveField PRIVATE
    -Wl,--add-needed
    -Wl,--no-as-needed
)
target_link_libraries(setWaveField PRIVATE
    OpenFOAM
    finiteVolume
    meshTools
    Pstream
    dl
)