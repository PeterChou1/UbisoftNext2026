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
#include "Serialization/WorldSerializer.h"
#include "World/Prefab.h"

#include <string>
#include <vector>

namespace SampleScenes
{
    struct SampleScene
    {
        std::string Name; // also the file name (without extension)
        void (*Author)(Editor::SceneEditor& editor);
        // Committed as a plain text scene file instead of binary
        bool PlainText = false;
    };

    /**
     * \brief Serialization file format of a sample scene's file
     */
    Serialization::SaveFormat FormatOf(const SampleScene& scene);

    const std::vector<SampleScene>& All();

    /**
     * \brief A sample prefab, built in an editor stage like the prefab
     *        editor does, written to data/prefabs/<name>.ubprefab
     */
    struct SamplePrefab
    {
        std::string Name;
        Prefab::Data (*Author)(Editor::SceneEditor& editor);
    };

    const std::vector<SamplePrefab>& Prefabs();
} // namespace SampleScenes
