# C++ DX12 Game Engine
3D DirectX 12 engine made from scratch in C++. After familiarizing myself with Direct3D and rendering in my [DX11 Project](https://github.com/JKHYuen/DX11Engine), I started over and ported all the features to DX12 to get some experience with a modern rendering API. New features were added and old ones were completely rewritten and generally improved with modern C++. It was a challenging and time consuming process, but [this tutorial](https://www.3dgep.com/learning-directx-12-1/) series provided a good start. This is an ongoing project for learning rendering techniques, but I plan to eventually transition to a much more robust abstraction layer like [NVRHI](https://github.com/NVIDIA-RTX/NVRHI) for a more serious project. 

***Windows 64-bit required** 

## Feature Highlights
- HDR physically based rendering (PBR) with image-based lighting (IBL)
	- Using Epic Game's version of the Cook-Torrance BRDF developed for Unreal 4 ([link](https://cdn2.unrealengine.com/Resources/files/2013SiggraphPresentationsNotes-26915738.pdf) p. 1 - 8)
	- Linear space calculations with gamma correction
	- Reinhard-Jodie tonemapping ([link](https://64.github.io/tonemapping/#reinhard-jodie))
- IBL environment maps (Irradiance and Pre-Filtered (specular) cubemap) generation from equirectangular .hdr files at runtime
	- Skybox cubemap generation and rendering
	- Skyboxes/environment maps can be loaded/switched during run time
- Bloom
	- Progressive down and up sampling with box sampling
- Parallax occlusion mapping with optional self shadowing
- Directional light with shadow mapping (other types of lights delayed until switching to deferred rendering)
	- Simple 3x3 multisample PCF using HLSL compare sampling for efficient soft shadows
- Object frustum culling
- Tessellation with DX hull and domain shaders with two modes:
	- Basic uniform tessellation
	- Distance based edge tessellation
- UI for real time scene/material editing and debugging
	- Made with [Dear ImGui](https://github.com/ocornut/imgui)
- AABB generation / visualization
	- rotations supported, displacement maps not yet supported
- Scene object picking with mouse click
	- Robust object outlining (geometry agnostic) using post processing
- MSAA (will be replaced with TAA when switching to deferred rendering)

## Controls
- Click and drag to change numeric values, double click to type in values
- Cursor and movement is disabled when menus are open
- Hold **RIGHT CLICK** when menus are open to reenable camera look and movement
- **LEFT CLICK** objects with menus open to enable object inspector
- Hold **LALT** to enable cursor to click on objects without menus

		  F1: Toggle UI
	     ESC: (First press) Unlock cursor from window / (Second press) Quit app
	     F11: Toggle fullscreen
		   V: Toggle Vsync
        WASD: Camera movement
          QE: Move camera up/down
      LSHIFT: Move fast
       LCTRL: Move slow

## Caveats
This was a first attempt at a modern renderer, some important base systems like resource state tracking/transitioning and memory management are adapted from the [tutorial](https://www.3dgep.com/learning-directx-12-1/) and are not well tested. There are major features that will not be added until switching to something more robust like NVRHI:
- Render graph system
- Smarter PSO management/generation
- Bindless resources 
	
## High Priority TODOs / Research Topics
- Switch from forward rendering to deferred rendering with forward pass for transparency
- TAA
- Cascaded shadow mapping
- Light/reflection probes
- Water rendering
- Occlusion culling (HZB)
- Procedural sky shading
- Volumetrics

## Asset Sources
- https://polyhaven.com/hdris
- https://freepbr.com/

## Additional Resources Used
 - https://www.3dgep.com/learning-directx-12-1/ (Lessons 1 - 4)
 - https://learnopengl.com/PBR/IBL/Diffuse-irradiance
 - https://learnopengl.com/Advanced-Lighting/Parallax-Mapping
 - https://chanhaeng.blogspot.com/2019/01/normalparllax-mapping-with-self.html
 - https://catlikecoding.com/unity/tutorials/advanced-rendering/bloom/
 - https://catlikecoding.com/unity/tutorials/advanced-rendering/tessellation/
