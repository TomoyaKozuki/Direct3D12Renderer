# A DirectX 12 renderer featuring a GGX microfacet BRDF.
This project implements a GGX (Trowbridge-Reitz) microfacet BRDF using DirectX 12.
It demonstrates physically based rendering techniques on modern graphics hardware.

## Features
- GGX (Trowbridge-Reitz) microfacet BRDF implementation
- Physically based rendering using DirectX 12

## Environment
- **OS**: Windows 11 (64-bit)
- **API**: DirectX 12
- **GPU**: NVIDIA GeForce RTX 4060 Ti
- **CPU**: AMD Ryzen 7 5700X (8-Core)
- **Memory**: 32 GB RAM
- **Windows SDK**: 10.0.22621.0

### Build
- Visual Studio 2022
- CMake 3.2x

### Libraries
- ImGui
- Assimp (v5.2.5)
- DirectXTK12 (oct2025)

## Pre-build Configuration
Before running CMake, you must configure the path to the Shader Compiler DLLs (`dxcompiler.dll` and `dxil.dll`) which are required for DXR shader signing.

1. Open `./Sample/CMakeLists.txt`.
2. Locate the `DXC_DLL_PATH` variable.
3. Update the path to match your Windows SDK or Visual Studio installation.

```cmake
# Example: Windows 11 SDK
set(DXC_DLL_PATH "C:/Program Files (x86)/Windows Kits/10/bin/10.0.22621.0/x64")

## Cloning & Building
This code is primarily tested with Windows 11 and Visual Studio 2022.  
To clone this repo, run the following (note the --recursive flag):
```
git clone --recursive https://github.com/TomoyaKozuki/DirectX12Renderer_GGX.git
```
Next, create a build directory and generate the project files using CMake:
```
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```
Alternatively:
```
cd (PATH_TO_CLONED_REPOSITORY)
mkdir build
cd build
cmake ..
```

## License
This project is licensed under the MIT License. See the [LICENSE](./LICENSE) file for details.













