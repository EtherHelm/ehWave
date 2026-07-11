cmake_minimum_required(VERSION 3.10)
project(ehWave LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

add_definitions(-DWM_DP -DWM_LABEL_SIZE=32 -DNoRepository)
add_compile_options(-m64 -pthread -frounding-math)

set(NNG_TESTS OFF CACHE BOOL "" FORCE)
set(NNG_ENABLE_NNGCAT OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/nng-1.11)

set(OF_PREFIX /usr/lib/openfoam/openfoam2512)
# set(OF_PREFIX /usr/lib/openfoam/openfoam2206)
include_directories(
    ./inc
    ./third_party/eigen
    ${OF_PREFIX}/src/OpenFOAM/lnInclude
    ${OF_PREFIX}/src/finiteVolume/lnInclude
    ${OF_PREFIX}/src/meshTools/lnInclude
    ${OF_PREFIX}/src/fvOptions/lnInclude
    ${OF_PREFIX}/src/surfMesh/lnInclude
    ${OF_PREFIX}/src/sampling/lnInclude
    ${OF_PREFIX}/src/OSspecific/POSIX/lnInclude
)

set(OF_LIB_DIR ${OF_PREFIX}/platforms/linux64GccDPInt32Opt/lib)

file(GLOB SOURCES src/*.cxx src/waveModels/*.cxx)
add_library(ehWave SHARED ${SOURCES})
target_link_directories(ehWave PRIVATE ${OF_LIB_DIR})
target_link_libraries(ehWave PRIVATE OpenFOAM finiteVolume fvOptions nng)
target_compile_definitions(ehWave PRIVATE NNG_STATIC_LIB NNG_ELIDE_DEPRECATED)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
add_executable(setWaveField src/setWaveField.cxx)
target_link_directories(setWaveField PRIVATE ${OF_LIB_DIR} ${OF_LIB_DIR}/sys-openmpi)
target_link_libraries(setWaveField PRIVATE OpenFOAM finiteVolume dl)