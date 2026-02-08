# A DirectX 12 renderer featuring a GGX microfacet BRDF.
This project implements a GGX (Trowbridge-Reitz) microfacet BRDF using DirectX 12.
It demonstrates physically based rendering techniques on modern graphics hardware.（In Progress: DXR Ray Tracing integration）

## Features
- GGX (Trowbridge-Reitz) microfacet BRDF implementation
- Physically based rendering using DirectX 12
## 🛠 Current Progress & Features

### 1. PBR Texture Mapping Implementation
In addition to constant material parameters, the renderer now supports full texture-based PBR workflows. This allows for high-fidelity surface detail using maps loaded via Assimp:
* **Albedo (Base Color) Map**: Defines the raw surface color.
* **Normal Map**: High-frequency surface detail using Tangent Space normals.
* **Metallic / Roughness Map**: Pixel-level control of the GGX specular response.

### 2. GGX Microfacet BRDF
A physically based shading model implemented in HLSL, ensuring realistic light interaction:
* **D (NDF)**: Trowbridge-Reitz GGX for specular distribution.
* **G (Geometry)**: Smith Joint GGX for shadowing and masking.
* **F (Fresnel)**: Schlick's Approximation for view-dependent reflectivity.

$$f(l, v) = \frac{D(h)F(v, h)G(l, v, h)}{4(n \cdot l)(n \cdot v)}$$

## 🚧 Roadmap (In Progress)

### DXR (DirectX Ray Tracing) Integration
I am currently transitioning the project from a pure rasterization pipeline to a hybrid/ray-traced approach using DXR 1.1:
- **Acceleration Structures**: Building BLAS (Bottom Level) and TLAS (Top Level) for complex mesh hierarchies.
- **Path Tracing**: Implementing importance sampling based on the GGX BRDF.
- **Shader Binding Table (SBT)**: Managing Ray Generation, Miss, and Closest Hit shaders.
- **Hybrid Rendering**: Combining rasterized G-buffers with ray-traced reflections and shadows.
      
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

1. Open [./Sample/CMakeLists.txt](./Sample/CMakeLists.txt).
2. Locate the `DXC_DLL_PATH` variable.
3. Update the path to match your Windows SDK or Visual Studio installation.

```cmake
# Example: Windows 11 SDK
set(DXC_DLL_PATH "C:/Program Files (x86)/Windows Kits/10/bin/10.0.22621.0/x64")
```
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
After generating the project, open build/Direct3D12_GGX_Renderer.sln in Visual Studio and run the Direct3D12Renderer project.

## License
This project is licensed under the MIT License. See the [LICENSE](./LICENSE) file for details.













