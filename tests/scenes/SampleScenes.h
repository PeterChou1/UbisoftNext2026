//---------------------------------------------------------------------------------
// SampleScenes.h
//---------------------------------------------------------------------------------
//
// Sample scenes authored with the headless scene editor, using the same API
// calls the editor GUI makes. The AuthorScenes tool writes them to
// data/scenes/<name>.ubsave (committed to the repository) and SceneFileTests
// re-authors them to check the committed files still load into exactly the
// same world.
//
#pragma once

#include "SceneEditor.h"

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
} // namespace SampleScenes
