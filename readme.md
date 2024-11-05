# 100 Dungeons

![dungeon1](dungeon1.gif)
## Screen capture of the first dungeon's current engine status. 
The engine currently only renders static meshes. Skinned animation support is planned for future implementation.

## Overview
**100 Dungeons** is a personal project inspired by the legendary Zelda games, aimed at designing 100 unique, immersive dungeons with classic Zelda-style gameplay in a 3D environment. This project is both a creative and technical challenge, allowing me to iterate on my game engine architecture while creating simple yet engaging dungeon experiences.

The current focus is on dungeon1, which is a continuation of my Anitra project. I plan to gradually migrate code from 1dungeon (based on my earlier GP1 project) to dungeon1, combining Anitra with GP1 along with new features to enhance functionality. To build dungeon1, you need to first run `generate.bat` and then `build.bat`. The `anitra.exe` will subsequently call `build_engine.bat` to generate the engine DLL for hot reloading.

The engine builds upon systems from previous projects, incorporating modular design and real-time updates via hot-reloading and directory-watching features from [Anitra](https://github.com/andresfelipemendez/anitra). Additionally, the project draws on my experience [reimplementing](https://github.com/andresfelipemendez/GP1) examples from *Game Programming in C++* by Sanjay Madhav to create a streamlined development workflow.

For the scene description format, I’ve replaced JSON with TOML, which provides straightforward handling of data types without YAML's added complexity. I’m also developing a custom ECS tailored to this engine's needs, reusing my knowledge of manual memory layout from the "direct x pong engine" project ([DirectX Pong Engine](https://github.com/andresfelipemendez/C-D3D11-Engine)). This ECS gives me precise memory control from within the hot-reloaded DLL, enhancing flexibility and performance.

## Features
- **Manually Designed Dungeons:** Each of the 100 dungeons will be unique, offering varied layouts and challenges.
- **Iterative Engine Development:** The engine undergoes continuous improvements, adding features tailored to dungeon creation and gameplay as development progresses.
- **Custom ECS Architecture:** A streamlined, custom-built ECS system allows direct memory management within the hot-reloaded DLL, drawing on manual memory layout techniques from prior engine projects.
- **OpenGL 4.5 Rendering:** Rendering is handled via OpenGL 4.5, providing extensive control over graphical output and flexibility in visual presentation.
- **GLTF for Mesh and Animation Loading:** The engine uses *fastgltf* to handle glTF assets, ensuring efficient loading of 3D models and animations.

## Libraries and Tools
- **[GLFW](https://github.com/glfw/glfw):** Manages windowing and input handling.
- **[GLAD](https://glad.dav1d.de/):** Loads OpenGL 4.5 extensions.
- **[GLM](https://github.com/g-truc/glm):** A mathematics library used for OpenGL transformations.
- **[tomlc99](https://github.com/cktan/tomlc99):** Parses TOML files for managing scene descriptions and configurations.
- **[fastgltf](https://github.com/spnda/fastgltf):** Supports efficient loading of 3D models and animations in the glTF format.

## How to Run
1. **Install Dependencies:** Ensure you have the required libraries installed (Flecs, GLFW, GLAD, GLM, etc.).
2. **Clone the Repository:**
   ```sh
   git clone https://github.com/andresfelipemendez/100-Dungeons.git
   cd 100-Dungeons
   ```
3. **Build the Project:**
   Use the provided build script to compile the code withing each dungeon:
   ```batch
   cd dungeon#
   .\generate.bat
   .\build.bat
   ```
   Replace `#` with the specific dungeon number you want to build. This will generate the engine DLL with hot reloading capability.
4. **Run the Game:**
   Execute the compiled `game.exe` to start exploring the dungeons.

## Roadmap
- **Combat and Puzzle Systems:** Implement basic enemy AI, combat mechanics, and puzzles that players can solve to progress.
- **Iteration on Engine Features:** Improve the underlying game engine to better support real-time rendering, collision detection, and other essential features.
- **Player Controls and Camera:** Implement smooth player movement and a dynamic camera to provide the best perspective for each dungeon.


## Acknowledgements
- **Nintendo's Zelda Series:** For endless inspiration in dungeon design and world-building.
- **Open Source Community:** Thanks to all contributors of the open source libraries used in this project.
