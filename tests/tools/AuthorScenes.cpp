//---------------------------------------------------------------------------------
// AuthorScenes.cpp
//---------------------------------------------------------------------------------
//
// Writes every sample scene (tests/scenes/SampleScenes.cpp) to a directory:
//
//     AuthorScenes <output directory>
//
// Run through the `author_scenes` CMake target to refresh data/scenes. The
// scenes use models from data/models, so run it from the repository root (the
// CMake target does)
//
#include "SampleScenes.h"
#include "TestEnvironment.h"

#include <iostream>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: AuthorScenes <output directory>\n";
        return 2;
    }
    std::string directory = argv[1];
    TestEnvironment::Init();
    Editor::SceneEditor editor;

    int failures = 0;
    for (const auto& scene : SampleScenes::All())
    {
        scene.Author(editor);
        std::string path = directory + "/" + scene.Name + ".ubsave";
        Serialization::SaveResult result = editor.SaveScene(path, scene.Name);
        if (result)
        {
            std::cout << "wrote " << path << " (" << result.BytesWritten << " bytes, "
                      << editor.Objects().size() << " objects)\n";
        }
        else
        {
            std::cerr << "failed to write " << path << ": " << result.Error << "\n";
            ++failures;
        }
    }
    return failures == 0 ? 0 : 1;
}
