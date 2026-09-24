//---------------------------------------------------------------------------------
// GameScripts.h
//---------------------------------------------------------------------------------
//
// The project's C++ scripts. Both the Game and the SceneEditor call
// RegisterGameScripts() at startup: the editor to offer the scripts in its
// inspector (and run them in play mode), the game to run authored scenes.
//
// Registration is explicit (no static registrars) because the scripts live in
// a static library, where unreferenced self-registering objects would be
// dropped by the linker.
//
#pragma once

void RegisterGameScripts();
