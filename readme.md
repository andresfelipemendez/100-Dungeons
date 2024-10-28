# 100 Dungeons

![dungeon1](dungeon1.gif)
## Screen capture of the first dungeon's current engine status. 
The engine currently only renders static meshes. Skinned animation support is planned for future implementation.

## Overview
100 Dungeons is a personal project inspired by the legendary Zelda games and serves as a way to apply my knowledge of data-oriented design and game engine architecture. The goal is to design and create 100 unique dungeons, each inspired by the classic Zelda-style gameplay but in a 3D environment. This project is not only about developing engaging dungeons but also about iterating on the game engine that supports these small, simple, but immersive levels.

The project integrates elements from previous engines I've developed, including systems I built when [reimplementing](https://github.com/andresfelipemendez/GP1) examples from "Game Programming in C++" by Sanjay Madhav, as well as hot-reloading features from [Anitra](https://github.com/andresfelipemendez/anitra). In Anitra, I implemented directory watching to recompile when source files are saved and added support for ImGui reloading. These experiences will help enhance 100 Dungeons by making development efficient and modular.

Additionally, I’ll be replacing the scene description format from JSON to YAML for this engine, favoring YAML for its compactness and readability and for the ability to specify data types, such as !!float. The engine will also replace the ENTT ECS architecture from GP1 with Flecs, allowing for a more flexible and performance-oriented approach to entity management.


## Features
- **Manually Designed Dungeons:** Each of the 100 dungeons will vary in size, layout, and complexity, providing a variety of challenges.
- **Iterative Engine Development:** The engine is being continuously developed and improved as the project progresses, focusing on features that support dungeon creation and gameplay.
- **Flecs-based ECS Architecture:** The engine utilizes the Flecs ECS (Entity Component System) to manage game entities, making the code modular and efficient.
- **OpenGL 4.5 Rendering:** The project uses OpenGL 4.5 for rendering the dungeons, allowing for flexibility and control over the graphical output.
- **GLTF for Mesh and Animation Loading:** Assets are loaded using the glTF format, which allows for efficient loading of 3D models and animations.

## Libraries and Tools
- **[Flecs](https://github.com/SanderMertens/flecs):** Used for managing entities and components.
- **[GLFW](https://github.com/glfw/glfw):** For window management and input handling.
- **[GLAD](https://glad.dav1d.de/):** For loading OpenGL 4.5 extensions.
- **[GLM](https://github.com/g-truc/glm):** Mathematics library for OpenGL to handle transformations.
- **Assimp or tinygltf:** For loading 3D models and animations in glTF format.

## How to Run
1. **Install Dependencies:** Ensure you have the required libraries installed (Flecs, GLFW, GLAD, GLM, etc.).
2. **Clone the Repository:**
   ```sh
   git clone https://github.com/andresfelipemendez/100-Dungeons.git
   cd 100-Dungeons
   ```
3. **Build the Project:**
   Use the provided build script to compile the code withing each dungeon:
   ```sh
   .\build.bat
   ```
4. **Run the Game:**
   Execute the compiled `game.exe` to start exploring the dungeons.

## Roadmap
- **Combat and Puzzle Systems:** Implement basic enemy AI, combat mechanics, and puzzles that players can solve to progress.
- **Iteration on Engine Features:** Improve the underlying game engine to better support real-time rendering, collision detection, and other essential features.
- **Player Controls and Camera:** Implement smooth player movement and a dynamic camera to provide the best perspective for each dungeon.


## Acknowledgements
- **Nintendo's Zelda Series:** For endless inspiration in dungeon design and world-building.
- **Open Source Community:** Thanks to all contributors of the open source libraries used in this project.