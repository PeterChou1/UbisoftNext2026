//---------------------------------------------------------------------------------
// GameScripts.h
//---------------------------------------------------------------------------------
//
// The project's C++ scripts and components. Both the Game and the SceneEditor call
// RegisterGameScripts() at startup: the editor to offer the scripts in its
// inspector (and run them in play mode), the game to run authored scenes.
// It also registers the project's components (Scripts/Components), which the
// editor can add to objects and scene files store.
//
// Registration is explicit (no static registrars) because the scripts live in
// a static library, where unreferenced self-registering objects would be
// dropped by the linker.
//
#pragma once

void RegisterGameScripts();
