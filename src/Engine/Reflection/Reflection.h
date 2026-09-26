//---------------------------------------------------------------------------------
// Reflection.h
//---------------------------------------------------------------------------------
//
// Describe a component's fields once, in code, and get for free:
//
//   - an editor inspector: the SceneEditor draws a widget per field (number
//     boxes, check boxes, text, colours, enum pickers, object references);
//   - saving and loading "like Unity": fields are stored by name, so adding,
//     removing, reordering or retyping fields never breaks existing scenes.
//
//     struct Health
//     {
//         float Current = 100.0f;
//         float Max = 100.0f;
//         bool Invulnerable = false;
//     };
//
//     REFLECT(Health)
//     {
//         Field("Current", &Health::Current).Range(0, 1000).Step(5);
//         Field("Max", &Health::Max).Min(1).Step(5);
//         Field("Invulnerable", &Health::Invulnerable);
//     }
//
// then register it once at startup (ComponentCatalog.h):
//
//     ComponentCatalog::Get().Register<Health>("Health", "Hit points");
//
// Supported field types: bool, 8 / 16 / 32 bit integers, float, double,
// std::string, Vec2, Vec3, enums and Entity references (a uint32 marked
// AsEntity). Nested structs and containers are not supported (static_assert).
//
// Editor hints, chained after Field(...):
//
//   .Range(min, max) .Min(v) .Max(v)   values are kept inside (typed, stepped
//                                      and loaded values)
//   .Step(v)                           increment of the - / + buttons
//   .Label("Text")                     shown instead of the field name
//   .Hidden()                          saved but not shown in the inspector
//   .ReadOnly()                        shown but not editable
//   .Options({"A", "B"})               names of an enum's values
//   .AsColor()                         Vec3 edited with colour swatches
//   .AsEntity()                        uint32 holding another object
//   .Tooltip("Text")                   help line shown under the field
//
// Saved form (text save files, one component record):
//
//     [3] Current:f 100 Max:f 100 Invulnerable:b false
//
// A field is written as its name, its type tag and its value. Loading
// matches fields by name: unknown fields (removed from the struct) are
// skipped, missing fields (added to the struct) keep their default value,
// numbers convert between integer and floating point fields, and a value
// whose type no longer fits the field is ignored.
//
#pragma once

#include "../Entity.h"
#include "../Serialization/Archive.h"
#include "../Serialization/MathSerialization.h"
#include "../Serialization/TextArchive.h"
#include "../Vec2.h"
#include "../Vec3.h"

#include <cfloat>
#include <cmath>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace Reflection
{
    /**
     * \brief How the editor shows a field
     */
    enum class FieldType
    {
        Bool,
        Int,
        Float,
        String,
        Vec2,
        Vec3,
        Color,
        Enum,
        Entity
    };

    /**
     * \brief How a field is stored in save files (its C++ type)
     */
    enum class ValueTag : std::uint8_t
    {
        Bool = 1,
        Int = 2,    // signed integer, up to 32 bits
        UInt = 3,   // unsigned integer, up to 32 bits (also Entity)
        Float = 4,  // float
        Double = 5, // double
        String = 6,
        Vec2 = 7,
        Vec3 = 8,
        Enum = 9
    };

    /**
     * \brief Short name written in text save files: b i u f d s v2 v3 e
     */
    const char* TagName(ValueTag tag);

    bool TagFromName(const std::string& name, ValueTag& tag);

    bool IsKnownTag(std::uint8_t tag);

    /**
     * \brief A field value on its way between a component, the editor and
     *        save files. Integers and enums travel as int64, float and double
     *        as double
     */
    using FieldValue = std::variant<bool, std::int64_t, double, std::string, Vec2, Vec3>;

    /**
     * \brief One reflected field
     */
    struct FieldInfo
    {
        std::string Name;
        // Text shown in the inspector (the name unless .Label was given)
        std::string Label;
        std::string Tooltip;
        FieldType Type = FieldType::Float;
        ValueTag Tag = ValueTag::Float;
        double Min = -DBL_MAX;
        double Max = DBL_MAX;
        // 0 = default step (1 for integers, 0.1 for floating point)
        double Step = 0.0;
        bool Hidden = false;
        bool ReadOnly = false;
        // Names of an enum's values, first name = lowest value
        std::vector<std::string> Options;
        // Valid values of an enum field
        std::int64_t EnumMin = 0;
        std::int64_t EnumMax = -1;

        std::function<FieldValue(const void*)> GetValue;
        // Converts, clamps and stores a value, false when it does not fit
        std::function<bool(void*, const FieldValue&)> SetValue;

        bool HasRange() const { return Min > -DBL_MAX || Max < DBL_MAX; }

        double StepOrDefault() const
        {
            if (Step > 0.0)
                return Step;
            return Type == FieldType::Int || Type == FieldType::Enum ? 1.0 : 0.1;
        }

        /**
         * \brief Name of an enum value (its number when it has no name)
         */
        std::string OptionName(std::int64_t value) const;
    };

    /**
     * \brief Every reflected field of a type, in declaration order
     */
    struct TypeInfo
    {
        std::string Name;
        std::vector<FieldInfo> Fields;

        const FieldInfo* Find(const std::string& name) const
        {
            for (const FieldInfo& field : Fields)
            {
                if (field.Name == name)
                    return &field;
            }
            return nullptr;
        }

        FieldValue Get(const void* object, const FieldInfo& field) const
        {
            return field.GetValue(object);
        }

        bool Set(void* object, const FieldInfo& field, const FieldValue& value) const
        {
            return !field.ReadOnly && field.SetValue(object, value);
        }
    };

    /**
     * \brief Human readable value (inspector labels, logs, tests)
     */
    std::string ToString(const FieldValue& value);

    namespace Detail
    {
        template <typename>
        inline constexpr bool AlwaysFalse = false;

        // Numbers convert between integers and floating point, other values
        // only to the same type
        inline bool ToDouble(const FieldValue& value, double& out)
        {
            if (const auto* i = std::get_if<std::int64_t>(&value))
                out = static_cast<double>(*i);
            else if (const auto* d = std::get_if<double>(&value))
                out = *d;
            else
                return false;
            return true;
        }

        inline double Clamp(double value, double min, double max)
        {
            return value < min ? min : (value > max ? max : value);
        }

        template <typename M>
        constexpr ValueTag TagOf()
        {
            if constexpr (std::is_same_v<M, bool>)
                return ValueTag::Bool;
            else if constexpr (std::is_enum_v<M>)
            {
                static_assert(sizeof(M) <= 4, "Reflected enums must fit in 32 bits");
                return ValueTag::Enum;
            }
            else if constexpr (std::is_integral_v<M>)
            {
                static_assert(sizeof(M) <= 4, "Reflected integers must fit in 32 bits");
                return std::is_signed_v<M> ? ValueTag::Int : ValueTag::UInt;
            }
            else if constexpr (std::is_same_v<M, float>)
                return ValueTag::Float;
            else if constexpr (std::is_same_v<M, double>)
                return ValueTag::Double;
            else if constexpr (std::is_same_v<M, std::string>)
                return ValueTag::String;
            else if constexpr (std::is_same_v<M, Vec2>)
                return ValueTag::Vec2;
            else if constexpr (std::is_same_v<M, Vec3>)
                return ValueTag::Vec3;
            else
            {
                static_assert(
                        AlwaysFalse<M>,
                        "Unsupported reflected field type: use bool, an integer, float, double, "
                        "std::string, Vec2, Vec3 or an enum");
                return ValueTag::Bool;
            }
        }

        inline FieldType TypeOf(ValueTag tag)
        {
            switch (tag)
            {
            case ValueTag::Bool:
                return FieldType::Bool;
            case ValueTag::Int:
            case ValueTag::UInt:
                return FieldType::Int;
            case ValueTag::Float:
            case ValueTag::Double:
                return FieldType::Float;
            case ValueTag::String:
                return FieldType::String;
            case ValueTag::Vec2:
                return FieldType::Vec2;
            case ValueTag::Vec3:
                return FieldType::Vec3;
            case ValueTag::Enum:
                return FieldType::Enum;
            }
            return FieldType::Float;
        }

        template <typename M>
        FieldValue Read(const M& member)
        {
            if constexpr (std::is_same_v<M, bool> || std::is_same_v<M, std::string> ||
                          std::is_same_v<M, Vec2> || std::is_same_v<M, Vec3>)
                return member;
            else if constexpr (std::is_enum_v<M>)
                return static_cast<std::int64_t>(static_cast<std::int32_t>(member));
            else if constexpr (std::is_integral_v<M>)
                return static_cast<std::int64_t>(member);
            else
                return static_cast<double>(member);
        }

        inline float ClampComponent(float value, const FieldInfo& field)
        {
            if (!field.HasRange())
                return value;
            return static_cast<float>(Clamp(value, field.Min, field.Max));
        }

        template <typename M>
        bool Write(M& member, const FieldValue& value, const FieldInfo& field)
        {
            if constexpr (std::is_same_v<M, bool> || std::is_same_v<M, std::string>)
            {
                const M* v = std::get_if<M>(&value);
                if (v == nullptr)
                    return false;
                member = *v;
                return true;
            }
            else if constexpr (std::is_same_v<M, Vec2>)
            {
                const Vec2* v = std::get_if<Vec2>(&value);
                if (v == nullptr)
                    return false;
                member = Vec2(ClampComponent(v->X, field), ClampComponent(v->Y, field));
                return true;
            }
            else if constexpr (std::is_same_v<M, Vec3>)
            {
                const Vec3* v = std::get_if<Vec3>(&value);
                if (v == nullptr)
                    return false;
                member = Vec3(ClampComponent(v->X, field),
                              ClampComponent(v->Y, field),
                              ClampComponent(v->Z, field));
                return true;
            }
            else
            {
                double number = 0.0;
                if (!ToDouble(value, number) || std::isnan(number))
                    return false;
                if constexpr (std::is_enum_v<M>)
                {
                    number = std::round(number);
                    // An enum value that does not exist is refused (turning it
                    // into the enum would be undefined behaviour)
                    if (number < static_cast<double>(field.EnumMin) ||
                        number > static_cast<double>(field.EnumMax))
                        return false;
                    member = static_cast<M>(static_cast<std::int32_t>(number));
                }
                else if constexpr (std::is_integral_v<M>)
                {
                    number = Clamp(std::round(number), field.Min, field.Max);
                    number = Clamp(number,
                                   static_cast<double>(std::numeric_limits<M>::lowest()),
                                   static_cast<double>(std::numeric_limits<M>::max()));
                    member = static_cast<M>(number);
                }
                else
                {
                    if (std::isinf(number) && field.HasRange())
                        return false;
                    number = Clamp(number, field.Min, field.Max);
                    if constexpr (std::is_same_v<M, float>)
                    {
                        // Keep finite doubles inside the float range
                        if (std::isfinite(number))
                            number = Clamp(number, -FLT_MAX, FLT_MAX);
                    }
                    member = static_cast<M>(number);
                }
                return true;
            }
        }
    } // namespace Detail

    /**
     * \brief Chained editor hints of one field (see the top of this file)
     */
    class FieldBuilder
    {
      public:
        explicit FieldBuilder(FieldInfo& field)
            : m_Field(field)
        {
        }

        FieldBuilder& Range(double min, double max)
        {
            m_Field.Min = min;
            m_Field.Max = max;
            return *this;
        }
        FieldBuilder& Min(double min)
        {
            m_Field.Min = min;
            return *this;
        }
        FieldBuilder& Max(double max)
        {
            m_Field.Max = max;
            return *this;
        }
        FieldBuilder& Step(double step)
        {
            m_Field.Step = step;
            return *this;
        }
        FieldBuilder& Label(const std::string& label)
        {
            m_Field.Label = label;
            return *this;
        }
        FieldBuilder& Tooltip(const std::string& tooltip)
        {
            m_Field.Tooltip = tooltip;
            return *this;
        }
        FieldBuilder& Hidden()
        {
            m_Field.Hidden = true;
            return *this;
        }
        FieldBuilder& ReadOnly()
        {
            m_Field.ReadOnly = true;
            return *this;
        }
        FieldBuilder& Options(std::initializer_list<const char*> names)
        {
            if (m_Field.Tag != ValueTag::Enum)
                throw std::logic_error("Options(...) is only for enum fields: " + m_Field.Name);
            m_Field.Options.assign(names.begin(), names.end());
            return *this;
        }
        FieldBuilder& AsColor()
        {
            if (m_Field.Tag != ValueTag::Vec3)
                throw std::logic_error("AsColor() needs a Vec3 field: " + m_Field.Name);
            m_Field.Type = FieldType::Color;
            m_Field.Min = 0.0;
            m_Field.Max = 1.0;
            return *this;
        }
        FieldBuilder& AsEntity()
        {
            if (m_Field.Tag != ValueTag::UInt)
                throw std::logic_error("AsEntity() needs an Entity (uint32) field: " +
                                       m_Field.Name);
            m_Field.Type = FieldType::Entity;
            return *this;
        }

      private:
        FieldInfo& m_Field;
    };

    /**
     * \brief Base of every REFLECT(Type) block: Field(...) adds a field
     */
    template <typename T>
    class Builder
    {
      public:
        TypeInfo Info;

      protected:
        template <typename M>
        FieldBuilder Field(const char* name, M T::*member)
        {
            FieldInfo field;
            field.Name = name;
            field.Label = name;
            field.Tag = Detail::TagOf<M>();
            field.Type = Detail::TypeOf(field.Tag);
            if constexpr (std::is_enum_v<M>)
            {
                if constexpr (Serialization::EnumRange<M>::Defined)
                {
                    field.EnumMin = Serialization::EnumRange<M>::Min;
                    field.EnumMax = Serialization::EnumRange<M>::Max;
                }
            }
            field.GetValue = [member](const void* object) {
                return Detail::Read(static_cast<const T*>(object)->*member);
            };
            // The FieldInfo is captured by index: Info.Fields grows while
            // fields are declared
            std::size_t index = Info.Fields.size();
            const TypeInfo* info = &Info;
            field.SetValue = [member, index, info](void* object, const FieldValue& value) {
                return Detail::Write(static_cast<T*>(object)->*member, value, info->Fields[index]);
            };
            Info.Fields.push_back(std::move(field));
            return FieldBuilder(Info.Fields.back());
        }
    };

    /**
     * \brief Specialised by REFLECT(Type). The primary template marks a type
     *        without reflection
     */
    template <typename T>
    struct Describe
    {
        using NotReflected = void;
    };

    template <typename T, typename = void>
    struct IsReflected : std::true_type
    {
    };

    template <typename T>
    struct IsReflected<T, std::void_t<typename Describe<T>::NotReflected>> : std::false_type
    {
    };

    template <typename T>
    inline constexpr bool IsReflectedV = IsReflected<T>::value;

    /**
     * \brief Checks a finished description (names, enum ranges), throws
     *        std::logic_error with the problem
     */
    void Validate(TypeInfo& info);

    /**
     * \brief The reflected fields of T, built on first use
     */
    template <typename T>
    const TypeInfo& TypeInfoOf()
    {
        static_assert(IsReflectedV<T>,
                      "Describe the type's fields with REFLECT(Type) { Field(...); }");
        static_assert(std::is_default_constructible_v<T>,
                      "Reflected types must be default constructible");
        // The setters point at the TypeInfo they were built in: it lives
        // inside this static and is never moved or copied afterwards
        static Describe<T> description;
        static const bool built = [] {
            description.Run();
            Validate(description.Info);
            return true;
        }();
        (void)built;
        return description.Info;
    }

    //-----------------------------------------------------------------------------
    // Generic serialization: fields stored by name
    //-----------------------------------------------------------------------------

    namespace Detail
    {
        template <typename Archive>
        constexpr bool IsText = std::is_same_v<Archive, Serialization::TextOutputArchive> ||
                                std::is_same_v<Archive, Serialization::TextInputArchive>;

        template <typename Archive>
        void WriteHeader(Archive& ar, const std::string& name, ValueTag tag)
        {
            if constexpr (IsText<Archive>)
                ar.WriteWord(name + ":" + TagName(tag));
            else
            {
                std::string copy = name;
                auto code = static_cast<std::uint8_t>(tag);
                ar(copy, code);
            }
        }

        template <typename Archive>
        void ReadHeader(Archive& ar, std::string& name, ValueTag& tag)
        {
            if constexpr (IsText<Archive>)
            {
                std::string token = ar.NextToken("a Name:type field");
                std::size_t colon = token.rfind(':');
                if (colon == std::string::npos || colon == 0 ||
                    !TagFromName(token.substr(colon + 1), tag))
                    ar.Fail("expected a Name:type field, got '" + token + "'");
                name = token.substr(0, colon);
            }
            else
            {
                std::uint8_t code = 0;
                ar(name, code);
                if (!IsKnownTag(code))
                    throw Serialization::SerializationError(
                            "Unknown field type " + std::to_string(code) + " for field " + name);
                tag = static_cast<ValueTag>(code);
            }
        }

        // A value stored in files as its field's C++ type (Stored), carried in
        // the FieldValue as Carried
        template <typename Stored, typename Carried, typename Archive>
        void SerializeAs(Archive& ar, FieldValue& value)
        {
            if constexpr (Archive::IsSaving)
            {
                auto stored = static_cast<Stored>(std::get<Carried>(value));
                ar(stored);
            }
            else
            {
                Stored stored{};
                ar(stored);
                value = static_cast<Carried>(stored);
            }
        }

        template <typename Archive>
        void SerializeValue(Archive& ar, ValueTag tag, FieldValue& value)
        {
            switch (tag)
            {
            case ValueTag::Bool:
                return SerializeAs<bool, bool>(ar, value);
            case ValueTag::Int:
            case ValueTag::Enum:
                return SerializeAs<std::int32_t, std::int64_t>(ar, value);
            case ValueTag::UInt:
                return SerializeAs<std::uint32_t, std::int64_t>(ar, value);
            case ValueTag::Float:
                return SerializeAs<float, double>(ar, value);
            case ValueTag::Double:
                return SerializeAs<double, double>(ar, value);
            case ValueTag::String:
                return SerializeAs<std::string, std::string>(ar, value);
            case ValueTag::Vec2:
                return SerializeAs<Vec2, Vec2>(ar, value);
            case ValueTag::Vec3:
                return SerializeAs<Vec3, Vec3>(ar, value);
            }
            throw Serialization::SerializationError("Unknown field type");
        }
    } // namespace Detail

    /**
     * \brief Save / load every reflected field of `object` by name
     */
    template <typename Archive, typename T>
    void SerializeFields(Archive& ar, T& object)
    {
        const TypeInfo& info = TypeInfoOf<T>();
        if constexpr (Archive::IsSaving)
        {
            ar.WriteSize(info.Fields.size());
            for (const FieldInfo& field : info.Fields)
            {
                Detail::WriteHeader(ar, field.Name, field.Tag);
                FieldValue value = field.GetValue(&object);
                Detail::SerializeValue(ar, field.Tag, value);
            }
        }
        else
        {
            std::size_t count = ar.ReadSize();
            for (std::size_t i = 0; i < count; ++i)
            {
                std::string name;
                ValueTag tag = ValueTag::Bool;
                Detail::ReadHeader(ar, name, tag);
                FieldValue value;
                Detail::SerializeValue(ar, tag, value);
                // Unknown fields are skipped, values that no longer fit the
                // field are ignored (the field keeps its default)
                if (const FieldInfo* field = info.Find(name))
                    field->SetValue(&object, value);
            }
        }
    }
} // namespace Reflection

namespace Serialization
{
    /**
     * \brief Every reflected type is serializable, without a Serialize
     *        function of its own (found through the archive's namespace)
     */
    template <typename Archive, typename T>
    std::enable_if_t<Reflection::IsReflectedV<T>> Serialize(Archive& ar, T& value)
    {
        Reflection::SerializeFields(ar, value);
    }
} // namespace Serialization

/**
 * \brief Describe the fields of Type (at global scope, after the type and
 *        after SERIALIZATION_ENUM_RANGE of its enums):
 *
 *            REFLECT(Health) { Field("Current", &Health::Current).Min(0); }
 */
#define REFLECT(Type)                                                                              \
    namespace Reflection                                                                           \
    {                                                                                              \
        template <>                                                                                \
        struct Describe<Type> : Builder<Type>                                                      \
        {                                                                                          \
            Describe()                                                                             \
            {                                                                                      \
                Info.Name = #Type;                                                                 \
            }                                                                                      \
            void Run();                                                                            \
        };                                                                                         \
    }                                                                                              \
    inline void Reflection::Describe<Type>::Run()
