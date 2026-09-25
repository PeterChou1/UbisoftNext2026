#include "Utils.h"

#include "ECSManager.h"
#include "Log.h"
#include "RigidBody.h"
#include "stdafx.h"

#include <fstream>
#include <random>
#include <sstream>
#include <unordered_map>
#include <vector>

extern ECSManager ECS;

Vec2 Utils::PointToLineSegment(Vec2 point, Vec2 a, Vec2 b)
{
    Vec2 a2b = b - a;
    Vec2 a2point = point - a;

    float proj = a2point.Dot(a2b);
    float lengthSquared = a2b.GetMagnitudeSquared();
    float d = proj / lengthSquared;

    if (d <= 0)
    {
        return a;
    }
    if (d >= 1)
    {
        return b;
    }

    return a + a2b * d;
}

std::vector<Vec2>
Utils::TranslatePoints(const std::vector<Vec2>& points, const float angle, const Vec2& position)
{
    std::vector<Vec2> translatedPolygon;
    int polySize = static_cast<int>(points.size());
    Mat2 matrix =
            Mat2(Vec2(std::cos(angle), -std::sin(angle)), Vec2(std::sin(angle), std::cos(angle)));

    for (int i = 0; i < polySize; i++)
    {
        Vec2 point = points[i];
        Vec2 translated = matrix * point + position;
        translatedPolygon.push_back(translated);
    }
    return translatedPolygon;
}

Mat2 Utils::RotationMatrix(const float angle)
{
    return Mat2(Vec2(std::cos(angle), -std::sin(angle)), Vec2(std::sin(angle), std::cos(angle)));
}

void Utils::TranslatePointsInto(const std::vector<Vec2>& points,
                                const float angle,
                                const Vec2& position,
                                std::vector<Vec2>& out)
{
    TranslatePointsInto(points, RotationMatrix(angle), position, out);
}

void Utils::TranslatePointsInto(const std::vector<Vec2>& points,
                                Mat2 matrix,
                                const Vec2& position,
                                std::vector<Vec2>& out)
{
    const std::size_t polySize = points.size();
    out.resize(polySize);
    for (std::size_t i = 0; i < polySize; i++)
    {
        Vec2 point = points[i];
        out[i] = matrix * point + position;
    }
}

/// Not the cleanest code but it gets the job done
bool Utils::LoadInstance(std::string filename,
                         MeshInstance& mesh,
                         std::vector<Material>& textureList)
{
    // Reads the common subset of Wavefront OBJ: v / vt / vn, faces with any
    // number of corners (triangulated as a fan), 1-based or negative
    // (relative) indices, mtllib / usemtl. Faces referring to missing data are
    // skipped instead of reading out of bounds, a missing .mtl or unknown
    // material falls back to the default material
    std::ifstream file(filename);
    size_t lastSlash = filename.find_last_of("/\\");
    std::string directory = filename.substr(0, lastSlash + 1);

    if (!file.is_open())
    {
        return false;
    }
    std::vector<Vec3> temp_vertices;
    std::vector<Vec2> temp_uvs;
    std::vector<Vec3> temp_normals;
    std::unordered_map<std::string, uint32_t> vertexMap;
    int cur_texID = -1;
    std::string line;
    std::unordered_map<std::string, size_t> textureIDs;
    size_t skippedFaces = 0;

    // "7", "-1": index into a list of `count` elements, -1 if invalid
    auto resolveIndex = [](const std::string& token, size_t count) -> long {
        if (token.empty())
            return -1;
        char* end = nullptr;
        long index = std::strtol(token.c_str(), &end, 10);
        if (end == token.c_str() || *end != '\0' || index == 0)
            return -1;
        long resolved = index > 0 ? index - 1 : static_cast<long>(count) + index;
        return resolved >= 0 && resolved < static_cast<long>(count) ? resolved : -1;
    };

    while (std::getline(file, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        std::stringstream ss(line);
        std::string prefix;
        ss >> prefix;
        if (prefix == "mtllib")
        {
            std::string mtlfilename;
            ss >> mtlfilename;
            std::string mtlfilepath = directory + mtlfilename;
            if (!LoadMTLFile(directory, mtlfilepath, textureList, textureIDs))
                LOG_WARN("Assets", "%s: material library %s not found, using the default material",
                         filename.c_str(), mtlfilepath.c_str());
        }
        else if (prefix == "usemtl")
        {
            std::string matname;
            ss >> matname;
            auto found = textureIDs.find(matname);
            cur_texID = found == textureIDs.end() ? -1 : static_cast<int>(found->second);
        }
        else if (prefix == "v")
        {
            Vec3 vertex;
            ss >> vertex.X >> vertex.Y >> vertex.Z;
            temp_vertices.push_back(vertex);
        }
        else if (prefix == "vt")
        {
            Vec2 uv;
            ss >> uv.X >> uv.Y;
            temp_uvs.push_back(uv);
        }
        else if (prefix == "vn")
        {
            Vec3 normal;
            ss >> normal.X >> normal.Y >> normal.Z;
            temp_normals.push_back(normal);
        }
        else if (prefix == "f")
        {
            // Corners "v", "v/vt", "v//vn" or "v/vt/vn"
            std::vector<Vertex> corners;
            bool allNormals = true;
            bool valid = true;
            std::string corner;
            while (ss >> corner && valid)
            {
                std::string parts[3];
                size_t part = 0;
                for (char c : corner)
                {
                    if (c == '/')
                    {
                        if (++part > 2)
                            break;
                    }
                    else
                        parts[part] += c;
                }
                Vertex v;
                long vIndex = resolveIndex(parts[0], temp_vertices.size());
                if (vIndex < 0)
                {
                    valid = false;
                    break;
                }
                v.LocalPosition = temp_vertices[vIndex];
                if (!parts[1].empty())
                {
                    long uvIdx = resolveIndex(parts[1], temp_uvs.size());
                    if (uvIdx < 0)
                    {
                        valid = false;
                        break;
                    }
                    v.UV.X = temp_uvs[uvIdx].X;
                    v.UV.Y = std::abs(temp_uvs[uvIdx].Y - 1);
                }
                if (!parts[2].empty())
                {
                    long nIdx = resolveIndex(parts[2], temp_normals.size());
                    if (nIdx < 0)
                    {
                        valid = false;
                        break;
                    }
                    v.LocalNormal = temp_normals[nIdx];
                }
                else
                    allNormals = false;
                corners.push_back(v);
            }
            if (!valid || corners.size() < 3)
            {
                ++skippedFaces;
                continue;
            }

            // Fan: (0, i, i + 1)
            for (size_t i = 1; i + 1 < corners.size(); ++i)
            {
                Vertex vertarray[3] = {corners[0], corners[i], corners[i + 1]};
                if (!allNormals)
                {
                    Vec3 lineA = vertarray[0].LocalPosition - vertarray[1].LocalPosition;
                    Vec3 lineB = vertarray[0].LocalPosition - vertarray[2].LocalPosition;
                    Vec3 normal = lineA.Cross(lineB);
                    normal.Normalize();
                    vertarray[0].LocalNormal = normal;
                    vertarray[1].LocalNormal = normal;
                    vertarray[2].LocalNormal = normal;
                }

                for (Vertex& v : vertarray)
                {
                    std::string VString = v.ToString();
                    v.TextureID = cur_texID;
                    if (vertexMap.find(VString) == vertexMap.end())
                    {
                        vertexMap[VString] = static_cast<uint32_t>(mesh.vertices.size());
                        mesh.vertices.push_back(v);
                    }
                    mesh.indices.push_back(vertexMap[VString]);
                }
            }
        }
    }
    file.close();
    if (skippedFaces > 0)
        LOG_WARN("Assets", "%s: skipped %zu faces with missing or invalid indices", filename.c_str(), skippedFaces);
    return true;
}

bool Utils::LoadMTLFile(const std::string& directory,
                        const std::string& filename,
                        std::vector<Material>& textureList,
                        std::unordered_map<std::string, size_t>& textureIDs)
{
    std::ifstream file(filename);
    if (!file.is_open())
    {
        return false;
    }

    std::string line;
    Vec3 ambient, diffuse, specular;
    float highlight;
    std::string textureFilename;
    std::string textureName;
    while (getline(file, line))
    {
        std::istringstream iss(line);
        std::string keyword;
        iss >> keyword;

        if (keyword == "newmtl")
        {
            // Handle new material - save the previous material if it exists
            if (!textureFilename.empty() && !textureName.empty())
            {
                textureIDs[textureName] = textureList.size();
                textureList.emplace_back(ambient, diffuse, specular, highlight);
                textureFilename.clear();
            }
            // material does not have a texture use alternate constructor
            else if (!textureName.empty())
            {
                textureIDs[textureName] = textureList.size();
                textureList.emplace_back(ambient, diffuse, specular, highlight);
            }
            iss >> textureName;
        }
        else if (keyword == "Ka")
        {
            // Ambient color
            iss >> ambient.X >> ambient.Y >> ambient.Z;
        }
        else if (keyword == "Kd")
        {
            // Diffuse color
            iss >> diffuse.X >> diffuse.Y >> diffuse.Z;
        }
        else if (keyword == "Ks")
        {
            // Specular color
            iss >> specular.X >> specular.Y >> specular.Z;
        }
        else if (keyword == "Ns")
        {
            // Specular highlight, exponent
            iss >> highlight;
        }
        else if (keyword == "map_Kd")
        {
            // Diffuse texture map
            iss >> textureFilename;
            textureFilename = directory + textureFilename;
        }
    }

    // Handle the last material
    if (!textureFilename.empty())
    {
        textureIDs[textureName] = textureList.size();
        textureList.emplace_back(ambient, diffuse, specular, highlight);
    }
    else if (!textureName.empty())
    {
        textureIDs[textureName] = textureList.size();
        textureList.emplace_back(ambient, diffuse, specular, highlight);
    }

    file.close();
    return true;
}
