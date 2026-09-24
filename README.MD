# Contest Submission for Ubisoft 2026 (Programming category)

Top 5 contest submission for ubisoft 2026 next programming contest

[![](https://markdown-videos-api.jorgenkh.no/youtube/HWnY8v94AxY)](https://youtu.be/HWnY8v94AxY)


## Features

- Entity Component System
- 3D renderer multithreaded cpu based renderer 
- Physics System (2D)
- AI System
- Immediate mode gui library
- Save system: binary, versioned serialization of the whole game state (see [changes/](changes/README.md))
- Scene editor: in-game level authoring GUI, scenes saved to `data/scenes/`

## Dependencies

### Windows
* Visual Studio 2022 Community https://visualstudio.microsoft.com/vs/community/
    * This is a windows IDE, windows build system and c++ compiler all bundled together
* CMake (for build system) https://cmake.org/
    * This is a build system generator, which allows us to build the project on multiple platforms

### MacOS
* Homebrew (for package management) [get from homebrew website] https://brew.sh/
    * This is a package manager for macos systems
* CMake (see windows section)
* freeglut
    * This is an OpenGL utility library. Install by running [brew install freeglut] in a macos terminal
* SDL3
    * This is a cross-platform utility library. Install by running [brew install SDL3] in a macos terminal
* XCode command line tools
    * This has the MacOS build system and compiler. It should be installed automatically when installing homebrew, but can be installed through the MacOS terminal.

## To build

### Windows
* Run generate-windows.bat script in directory
* open visual studio solution in build/win64
* build and run from visual studio

### MacOS
* Run ./generate-macos.sh script in directory
* go to build/macos directory
    * run [make all] in macos terminal to build the program
    * run [make run] to run the game
        * Note that the application can't be closed in the window. use cmd + Q to quit the program
* If using VSCode, commands can be run from the VSCode terminal

## Save / Load

In the main level press **4** to quick save and **5** to quick load (`saves/quicksave.ubsave`).
From code use `GameSceneManager.SaveGame(path, error)` / `GameSceneManager.LoadGame(path, error)`.
See [changes/](changes/README.md) for the full design.

## Scene editor

From the title screen choose **Scene Editor**. Pick a prefab (palette or keys 1-7) and click the ground to place it;
select and drag objects, R rotates, X deletes, U/Y undo/redo, WASD pans, Z/C zooms.
Save / Load use the scene list (files in `data/scenes/`), **Play** tests the scene in the main level and **Tab** returns
to the editor. Scenes saved to `data/scenes/` are automatically verified by the unit tests.
See [changes/05-scene-editor.md](changes/05-scene-editor.md).

## Unit tests

The save system tests are headless (no OpenGL/GLUT/SDL needed) and run on any platform:

```
cmake -S tests -B build/tests
cmake --build build/tests
ctest --test-dir build/tests --output-on-failure
```

They can also be built from the root project with `-DBUILD_SERIALIZATION_TESTS=ON`.
Regenerate the sample scenes in `data/scenes/` with `cmake --build build/tests --target author_scenes`.
