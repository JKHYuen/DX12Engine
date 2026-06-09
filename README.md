# C++ DX12 Game Engine

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
- Directional light with shadow mapping
	- Simple 5x5 multisample PCF
- Object frustum culling
- Tessellation with hull and domain shaders with two modes:
	- Basic uniform tessellation
	- Distance based edge tessellation
- UI for real time scene/material editing and debugging (with options to tweak all features above)	
	- Made with [Dear ImGui](https://github.com/ocornut/imgui)

## Controls
F1: Open UI  

Controls below are listed in app UI:     

    - Click and drag to change numeric values, double click to type in values
    - Cursor and movement is disabled when menus are open
    - Hold RIGHT CLICK when menus are open to reenable camera look and movement
    - LEFT CLICK objects with menus open to enable object inspector
      Note: click raycasts/AABBs currently do not account for height maps
    - Hold LALT to enable cursor to click on objects without menus
       ESC: (First press) Unlock cursor
            (Second press) Quit app
        F1: Toggle this menu
       F11: Toggle fullscreen
         V: Toggle Vsync
      WASD: Camera movement
        QE: Move camera up/down
    LSHIFT: Move fast
     LCTRL: Move slow
    
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
