#include "ShaderLibrary.h"

namespace ShaderLibrary
{
    namespace
    {
        // Every fragment shader, the listed ones first
        const std::vector<Entry<FragShaderTypeID>>& AllFragmentShaders()
        {
            static const std::vector<Entry<FragShaderTypeID>> shaders = {
                    {ShapeShaderID, "Shape", "Lit, vertex colour (shapes)"},
                    {BlinnPhongID, "Lit", "Blinn-Phong, material colour (models)"},
                    {UnlitShaderID, "Unlit", "Flat material colour"},
                    {PulseShaderID, "Pulse", "Lit, brightness pulses"},
                    {RimShaderID, "Rim", "Lit, glowing edges"},
                    {StripesShaderID, "Stripes", "Lit, bands scrolling up"},
                    {NormalShaderID, "Normals", "Colour from the surface normal"},
                    {RedShaderID, "Red", "Solid red (highlight)"},
                    {DefaultFragShaderID, "Default", "Blinn-Phong"},
                    {OutlineShaderID, "Outline", "Internal"},
                    {ParticleShaderID, "Particle", "Internal"},
                    {ToonShaderID, "Toon", "Internal"},
            };
            return shaders;
        }

        constexpr std::size_t LISTED_FRAGMENT_SHADERS = 8;
    } // namespace

    const std::vector<Entry<FragShaderTypeID>>& FragmentShaders()
    {
        static const std::vector<Entry<FragShaderTypeID>> listed(
                AllFragmentShaders().begin(), AllFragmentShaders().begin() + LISTED_FRAGMENT_SHADERS);
        return listed;
    }

    const std::vector<Entry<VertShaderTypeID>>& VertexShaders()
    {
        static const std::vector<Entry<VertShaderTypeID>> shaders = {
                {DefaultVertShaderID, "Default", "No animation"},
                {WaveVertShaderID, "Wave", "Bobs up and down in a travelling wave"},
                {SwayVertShaderID, "Sway", "Sways sideways, more at the top"},
        };
        return shaders;
    }

    std::string Name(FragShaderTypeID id)
    {
        for (const auto& shader : AllFragmentShaders())
            if (shader.Value == id)
                return shader.Name;
        return "#" + std::to_string(static_cast<int>(id));
    }

    std::string Name(VertShaderTypeID id)
    {
        for (const auto& shader : VertexShaders())
            if (shader.Value == id)
                return shader.Name;
        return "#" + std::to_string(static_cast<int>(id));
    }

    bool Find(const std::string& name, FragShaderTypeID& id)
    {
        for (const auto& shader : AllFragmentShaders())
            if (name == shader.Name)
            {
                id = shader.Value;
                return true;
            }
        return false;
    }

    bool Find(const std::string& name, VertShaderTypeID& id)
    {
        for (const auto& shader : VertexShaders())
            if (name == shader.Name)
            {
                id = shader.Value;
                return true;
            }
        return false;
    }
} // namespace ShaderLibrary
