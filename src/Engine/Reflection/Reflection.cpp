#include "Reflection.h"

#include <cstdio>
#include <set>

namespace Reflection
{
    namespace
    {
        struct TagEntry
        {
            ValueTag Tag;
            const char* Name;
        };

        const TagEntry TAGS[] = {{ValueTag::Bool, "b"},
                                 {ValueTag::Int, "i"},
                                 {ValueTag::UInt, "u"},
                                 {ValueTag::Float, "f"},
                                 {ValueTag::Double, "d"},
                                 {ValueTag::String, "s"},
                                 {ValueTag::Vec2, "v2"},
                                 {ValueTag::Vec3, "v3"},
                                 {ValueTag::Enum, "e"}};

        // Field names are written as bare words in text save files
        bool IsValidName(const std::string& name)
        {
            if (name.empty())
                return false;
            for (char c : name)
            {
                bool letter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
                bool digit = c >= '0' && c <= '9';
                if (!letter && !digit && c != '_')
                    return false;
            }
            return !(name[0] >= '0' && name[0] <= '9');
        }

        std::string Number(double value)
        {
            return Serialization::TextOutputArchive::FormatFloat(value);
        }
    } // namespace

    const char* FieldTypeName(FieldType type)
    {
        switch (type)
        {
        case FieldType::Bool:
            return "Bool";
        case FieldType::Int:
            return "Int";
        case FieldType::Float:
            return "Float";
        case FieldType::String:
            return "String";
        case FieldType::Vec2:
            return "Vec2";
        case FieldType::Vec3:
            return "Vec3";
        case FieldType::Color:
            return "Color";
        case FieldType::Enum:
            return "Enum";
        case FieldType::Entity:
            return "Entity";
        }
        return "?";
    }

    const char* TagName(ValueTag tag)
    {
        for (const TagEntry& entry : TAGS)
        {
            if (entry.Tag == tag)
                return entry.Name;
        }
        return "?";
    }

    bool TagFromName(const std::string& name, ValueTag& tag)
    {
        for (const TagEntry& entry : TAGS)
        {
            if (name == entry.Name)
            {
                tag = entry.Tag;
                return true;
            }
        }
        return false;
    }

    bool IsKnownTag(std::uint8_t tag)
    {
        for (const TagEntry& entry : TAGS)
        {
            if (static_cast<std::uint8_t>(entry.Tag) == tag)
                return true;
        }
        return false;
    }

    std::string FieldInfo::OptionName(std::int64_t value) const
    {
        std::int64_t index = value - EnumMin;
        if (index >= 0 && index < static_cast<std::int64_t>(Options.size()))
            return Options[static_cast<std::size_t>(index)];
        return std::to_string(value);
    }

    std::string ToString(const FieldValue& value)
    {
        if (const auto* b = std::get_if<bool>(&value))
            return *b ? "true" : "false";
        if (const auto* i = std::get_if<std::int64_t>(&value))
            return std::to_string(*i);
        if (const auto* d = std::get_if<double>(&value))
            return Number(*d);
        if (const auto* s = std::get_if<std::string>(&value))
            return *s;
        if (const auto* v2 = std::get_if<Vec2>(&value))
            return "(" + Number(v2->X) + ", " + Number(v2->Y) + ")";
        const Vec3& v3 = std::get<Vec3>(value);
        return "(" + Number(v3.X) + ", " + Number(v3.Y) + ", " + Number(v3.Z) + ")";
    }

    void Validate(TypeInfo& info)
    {
        std::set<std::string> names;
        for (FieldInfo& field : info.Fields)
        {
            std::string where = info.Name + "::" + field.Name;
            if (!IsValidName(field.Name))
                throw std::logic_error(where + ": field names are letters, digits and _ (they are saved by name)");
            if (!names.insert(field.Name).second)
                throw std::logic_error(where + ": two fields with the same name");
            if (field.Min > field.Max)
                throw std::logic_error(where + ": Min is larger than Max");
            if (field.Tag == ValueTag::Enum && field.EnumMax < field.EnumMin)
            {
                // Without SERIALIZATION_ENUM_RANGE the options give the values
                // 0 .. n-1
                if (field.Options.empty())
                    throw std::logic_error(where + ": give the enum's names with .Options({...}) or declare "
                                                   "SERIALIZATION_ENUM_RANGE before REFLECT");
                field.EnumMin = 0;
                field.EnumMax = static_cast<std::int64_t>(field.Options.size()) - 1;
            }
        }
    }
} // namespace Reflection
