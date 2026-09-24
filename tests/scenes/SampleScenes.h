//---------------------------------------------------------------------------------
// SampleScenes.h
//---------------------------------------------------------------------------------
//
// Sample Metal Invasion scenes authored with the headless scene editor, using
// the same API calls the editor GUI makes. The AuthorScenes tool writes them to
// data/scenes/<name>.ubsave (committed to the repository) and SceneFileTests
// re-authors them to verify the committed files still load into exactly the
// same world.
//
#pragma once

#include "Editor/SceneEditor.h"

#include <string>
#include <vector>

namespace SampleScenes
{
    struct SampleScene
    {
        std::string Name; // also the file name (without extension)
        std::string Description;
        void (*Author)(Editor::SceneEditor& editor);
    };

    const std::vector<SampleScene>& All();

    /**
     * \brief Register the resources the editor needs in the global ECS (once)
     */
    void EnsureEditorResources();

    constexpr const char* SCENE_EXTENSION = ".ubsave";
} // namespace SampleScenes
