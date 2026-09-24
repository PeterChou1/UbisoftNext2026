//---------------------------------------------------------------------------------
// AuthorScenes.cpp
//---------------------------------------------------------------------------------
//
// Writes every sample scene (tests/scenes/SampleScenes.cpp) to a directory:
//
//     AuthorScenes <output directory>
//
// Run through the `author_scenes` CMake target to refresh data/scenes.
//
#include "ECSManager.h"
#include "SampleScenes.h"

#include <iostream>

ECSManager ECS;

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: AuthorScenes <output directory>\n";
        return 2;
    }
    std::string directory = argv[1];

    ECS.Init();
    SampleScenes::EnsureEditorResources();
    Editor::SceneEditor editor;

    int failures = 0;
    for (const auto& scene : SampleScenes::All())
    {
        scene.Author(editor);
        std::string path = directory + "/" + scene.Name + SampleScenes::SCENE_EXTENSION;
        Serialization::SaveResult result = editor.SaveScene(path, scene.Name);
        if (result)
        {
            std::cout << "wrote " << path << " (" << result.BytesWritten << " bytes, "
                      << editor.EditableEntities().size() << " objects)\n";
        }
        else
        {
            std::cerr << "failed to write " << path << ": " << result.Error << "\n";
            ++failures;
        }
    }
    return failures == 0 ? 0 : 1;
}
