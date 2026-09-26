#include "ModelImport.h"

#include "AssetServer.h"
#include "Log.h"
#include "Utils.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

namespace fs = std::filesystem;

namespace ModelImport
{
    namespace
    {
        std::string Trim(std::string text)
        {
            auto notSpace = [](unsigned char c) { return !std::isspace(c); };
            text.erase(text.begin(), std::find_if(text.begin(), text.end(), notSpace));
            text.erase(std::find_if(text.rbegin(), text.rend(), notSpace).base(), text.end());
            // Paths copied from a file browser often come quoted
            if (text.size() >= 2 && (text.front() == '"' || text.front() == '\'') && text.back() == text.front())
                text = text.substr(1, text.size() - 2);
            return text;
        }

        fs::path ExpandHome(const std::string& path)
        {
            if (path.rfind("~/", 0) == 0 || path.rfind("~\\", 0) == 0)
            {
                const char* home = std::getenv("HOME");
                if (home == nullptr)
                    home = std::getenv("USERPROFILE");
                if (home != nullptr)
                    return fs::path(home) / path.substr(2);
            }
            return fs::path(path);
        }

        // Model names are used in scene files and the editor: keep them simple
        std::string SafeName(const std::string& stem)
        {
            std::string name;
            for (char c : stem)
                name += (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-') ? c : '_';
            return name.empty() ? "model" : name;
        }

        bool ReadText(const fs::path& path, std::string& text)
        {
            std::ifstream in(path, std::ios::binary);
            if (!in)
                return false;
            std::ostringstream buffer;
            buffer << in.rdbuf();
            text = buffer.str();
            return true;
        }

        bool WriteText(const fs::path& path, const std::string& text)
        {
            std::ofstream out(path, std::ios::binary | std::ios::trunc);
            out << text;
            return static_cast<bool>(out);
        }

        bool SameContent(const fs::path& a, const std::string& content)
        {
            std::string existing;
            return ReadText(a, existing) && existing == content;
        }

        Result Fail(Result result, const std::string& error)
        {
            result.Success = false;
            result.Error = error;
            LOG_WARN("Assets", "Model import failed: %s", error.c_str());
            return result;
        }
    } // namespace

    Result Import(const std::string& sourcePath, const std::string& modelsDirectory)
    {
        Result result;
        fs::path source = ExpandHome(Trim(sourcePath));
        std::error_code ec;
        if (source.empty())
            return Fail(result, "No file given");
        if (!fs::is_regular_file(source, ec))
            return Fail(result, "File not found: " + source.string());
        std::string extension = source.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (extension != ".obj")
            return Fail(result, "Not an .obj file: " + source.string());

        // 1. It must load and have faces before anything is copied
        {
            MeshInstance mesh;
            std::vector<Material> materials;
            if (!Utils::LoadInstance(source.string(), mesh, materials))
                return Fail(result, "Could not read " + source.string());
            if (mesh.indices.empty())
                return Fail(result, source.filename().string() + " has no faces");
            result.Vertices = mesh.vertices.size();
            result.Triangles = mesh.indices.size() / 3;
        }

        std::string objText;
        if (!ReadText(source, objText))
            return Fail(result, "Could not read " + source.string());

        fs::path directory(modelsDirectory);
        fs::create_directories(directory, ec);
        if (ec)
            return Fail(result, "Could not create " + directory.string());

        // 2. Material libraries next to the .obj: copy them, renamed if a
        //    different library with the same name is already there
        std::string baseName = SafeName(source.stem().string());
        std::map<std::string, std::string> mtlRenames;
        std::vector<std::pair<fs::path, fs::path>> mtlCopies;
        {
            std::istringstream lines(objText);
            std::string line;
            while (std::getline(lines, line))
            {
                std::istringstream words(line);
                std::string keyword, library;
                words >> keyword;
                if (keyword != "mtllib")
                    continue;
                while (words >> library)
                {
                    if (mtlRenames.count(library) > 0)
                        continue;
                    fs::path mtlSource = source.parent_path() / library;
                    std::string mtlText;
                    if (!ReadText(mtlSource, mtlText))
                    {
                        result.Warnings.push_back("Material library " + library +
                                                  " not found, the default material is used");
                        mtlRenames[library] = library;
                        continue;
                    }
                    std::string target = fs::path(library).filename().string();
                    if (fs::exists(directory / target, ec) && !SameContent(directory / target, mtlText))
                        target = baseName + "_" + target;
                    mtlRenames[library] = target;
                    mtlCopies.emplace_back(mtlSource, directory / target);
                }
            }
        }

        // The .obj refers to the libraries by their new names
        std::string rewritten;
        {
            std::istringstream lines(objText);
            std::string line;
            while (std::getline(lines, line))
            {
                std::istringstream words(line);
                std::string keyword;
                words >> keyword;
                if (keyword == "mtllib")
                {
                    std::string library;
                    line = "mtllib";
                    while (words >> library)
                        line += " " + mtlRenames[library];
                }
                rewritten += line + "\n";
            }
        }

        // 3. Model name: reuse an identical import, else the first free name
        std::string name = baseName;
        for (int n = 2;; ++n)
        {
            fs::path target = directory / (name + ".obj");
            if (!fs::exists(target, ec))
                break;
            if (SameContent(target, rewritten))
            {
                result.AlreadyImported = true;
                break;
            }
            name = baseName + "_" + std::to_string(n);
        }
        result.Name = name;

        // 4. Copy
        for (const auto& copy : mtlCopies)
        {
            if (!fs::exists(copy.second, ec))
                fs::copy_file(copy.first, copy.second, ec);
            if (ec)
                return Fail(result, "Could not copy " + copy.first.string() + ": " + ec.message());
        }
        fs::path target = directory / (name + ".obj");
        if (!result.AlreadyImported && !WriteText(target, rewritten))
            return Fail(result, "Could not write " + target.string());

        // A previous attempt under this name may be cached (empty)
        AssetServer::GetInstance().ForgetModel(name);
        result.Success = true;
        LOG_INFO("Assets", "Imported model %s from %s (%zu vertices, %zu triangles)%s", name.c_str(),
                 source.string().c_str(), result.Vertices, result.Triangles,
                 result.AlreadyImported ? " (already imported)" : "");
        for (const std::string& warning : result.Warnings)
            LOG_WARN("Assets", "%s: %s", name.c_str(), warning.c_str());
        return result;
    }
} // namespace ModelImport
