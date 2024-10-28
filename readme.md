# 100 Dungeons

![dungeon1](dungeon1.gif)
## Screen capture of the first dungeon's current engine status. 
The engine currently only renders static meshes. Skinned animation support is planned for future implementation.

## Overview

**100 Dungeons** is a personal project inspired by the legendary Zelda games and serves as a way to apply my knowledge of data-oriented design and game engine architecture. The goal is to design and create 100 unique dungeons, each inspired by the classic Zelda-style gameplay, but in a 3D environment. This project aims to not only develop interesting dungeons but also iterate on the game engine that supports these small, simple, but engaging levels.

The project is a learning journey focused on dungeon design, putting into practice what I've learned about data-oriented design and game engine development to create these experiences. The idea is to learn how to develop dungeons systematically while iteratively improving the tools and technologies that make them come to life.

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