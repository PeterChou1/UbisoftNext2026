//---------------------------------------------------------------------------------
// ArchiveTests.cpp
//---------------------------------------------------------------------------------
//
// Low level tests of the binary archives: encoding, containers and the
// protection against corrupt / truncated data
//
#include "Serialization/Archive.h"
#include "Serialization/Crc32.h"
#include "Serialization/MathSerialization.h"
#include "TestFramework.h"

#include <cmath>
#include <limits>

using namespace Serialization;

namespace
{
    enum TestEnum
    {
        First,
        Second,
        Negative = -5
    };

} // namespace

SERIALIZATION_ENUM_RANGE(TestEnum, Negative, Second)

namespace
{
    struct Nested
    {
        int A = 0;
        std::string Name;
        std::vector<float> Values;
    };

    template <typename Archive>
    void Serialize(Archive& ar, Nested& n)
    {
        ar(n.A, n.Name, n.Values);
    }

    bool BitEqual(float a, float b) { return std::memcmp(&a, &b, sizeof(float)) == 0; }

    bool BitEqual(double a, double b) { return std::memcmp(&a, &b, sizeof(double)) == 0; }
} // namespace

TEST_CASE("Archive: primitives round trip")
{
    std::int8_t i8 = -12;
    std::uint8_t u8 = 250;
    std::int16_t i16 = -31000;
    std::uint16_t u16 = 65000;
    std::int32_t i32 = std::numeric_limits<std::int32_t>::min();
    std::uint32_t u32 = 0xDEADBEEF;
    std::int64_t i64 = -1234567890123456789LL;
    std::uint64_t u64 = 0xFEDCBA9876543210ULL;
    float f = 3.14159f;
    double d = -2.718281828459045;
    bool t = true;
    bool fl = false;
    TestEnum e1 = Second;
    TestEnum e2 = Negative;

    OutputArchive out;
    out(i8, u8, i16, u16, i32, u32, i64, u64, f, d, t, fl, e1, e2);

    std::int8_t ri8{};
    std::uint8_t ru8{};
    std::int16_t ri16{};
    std::uint16_t ru16{};
    std::int32_t ri32{};
    std::uint32_t ru32{};
    std::int64_t ri64{};
    std::uint64_t ru64{};
    float rf{};
    double rd{};
    bool rt = false;
    bool rfl = true;
    TestEnum re1 = First;
    TestEnum re2 = First;

    InputArchive in(out.Buffer());
    in(ri8, ru8, ri16, ru16, ri32, ru32, ri64, ru64, rf, rd, rt, rfl, re1, re2);

    CHECK(in.AtEnd());
    CHECK_EQ(ri8, i8);
    CHECK_EQ(ru8, u8);
    CHECK_EQ(ri16, i16);
    CHECK_EQ(ru16, u16);
    CHECK_EQ(ri32, i32);
    CHECK_EQ(ru32, u32);
    CHECK_EQ(ri64, i64);
    CHECK_EQ(ru64, u64);
    CHECK(BitEqual(rf, f));
    CHECK(BitEqual(rd, d));
    CHECK_EQ(rt, true);
    CHECK_EQ(rfl, false);
    CHECK(re1 == Second);
    CHECK(re2 == Negative);
}

TEST_CASE("Archive: special float values are preserved bit exactly")
{
    std::vector<float> values = {0.0f,
                                 -0.0f,
                                 std::numeric_limits<float>::infinity(),
                                 -std::numeric_limits<float>::infinity(),
                                 std::numeric_limits<float>::quiet_NaN(),
                                 std::numeric_limits<float>::denorm_min(),
                                 std::numeric_limits<float>::max(),
                                 1.0f / 3.0f};
    std::vector<float> loaded;
    FromBytes(ToBytes(values), loaded);
    REQUIRE(loaded.size() == values.size());
    for (size_t i = 0; i < values.size(); ++i)
        CHECK(BitEqual(loaded[i], values[i]));
}

TEST_CASE("Archive: integers are encoded little-endian with a fixed width")
{
    std::uint32_t value = 0x01020304;
    std::int16_t negative = -2;
    OutputArchive out;
    out(value, negative);
    const auto& b = out.Buffer();
    REQUIRE(b.size() == 6);
    CHECK_EQ(int(b[0]), 0x04);
    CHECK_EQ(int(b[1]), 0x03);
    CHECK_EQ(int(b[2]), 0x02);
    CHECK_EQ(int(b[3]), 0x01);
    CHECK_EQ(int(b[4]), 0xFE);
    CHECK_EQ(int(b[5]), 0xFF);
}

TEST_CASE("Archive: strings and standard containers round trip")
{
    std::string empty;
    std::string text = "Metal Invasion \xE2\x9A\x94 round 4";
    std::vector<int> numbers = {1, -2, 3, 400000};
    std::vector<std::vector<std::string>> nested = {{"a", "bc"}, {}, {"def"}};
    std::vector<bool> flags = {true, false, false, true, true};
    std::array<float, 3> fixed = {1.5f, -2.5f, 3.25f};
    std::pair<int, std::string> pair = {7, "seven"};
    std::set<std::uint32_t> ids = {9, 3, 4000, 1};
    std::map<std::string, int> ordered = {{"b", 2}, {"a", 1}};
    std::unordered_map<std::uint32_t, float> hashed = {{5, 0.5f}, {1, 1.0f}, {77, -7.0f}};
    Nested custom{42, "custom", {0.1f, 0.2f}};

    OutputArchive out;
    out(empty, text, numbers, nested, flags, fixed, pair, ids, ordered, hashed, custom);

    std::string rEmpty = "not empty";
    std::string rText;
    std::vector<int> rNumbers = {99};
    std::vector<std::vector<std::string>> rNested;
    std::vector<bool> rFlags;
    std::array<float, 3> rFixed{};
    std::pair<int, std::string> rPair;
    std::set<std::uint32_t> rIds = {12345};
    std::map<std::string, int> rOrdered;
    std::unordered_map<std::uint32_t, float> rHashed;
    Nested rCustom;

    InputArchive in(out.Buffer());
    in(rEmpty, rText, rNumbers, rNested, rFlags, rFixed, rPair, rIds, rOrdered, rHashed, rCustom);

    CHECK(in.AtEnd());
    CHECK(rEmpty.empty());
    CHECK_EQ(rText, text);
    CHECK(rNumbers == numbers);
    CHECK(rNested == nested);
    CHECK(rFlags == flags);
    CHECK(rFixed == fixed);
    CHECK(rPair == pair);
    CHECK(rIds == ids);
    CHECK(rOrdered == ordered);
    CHECK(rHashed == hashed);
    CHECK_EQ(rCustom.A, 42);
    CHECK_EQ(rCustom.Name, std::string("custom"));
    CHECK(rCustom.Values == custom.Values);
}

TEST_CASE("Archive: hash maps serialize deterministically regardless of insertion order")
{
    std::unordered_map<std::uint32_t, int> a;
    std::unordered_map<std::uint32_t, int> b;
    for (std::uint32_t i = 0; i < 200; ++i)
        a[i * 37 % 1009] = static_cast<int>(i);
    for (std::uint32_t i = 200; i-- > 0;)
        b[i * 37 % 1009] = static_cast<int>(i);
    CHECK(ToBytes(a) == ToBytes(b));
}

TEST_CASE("Archive: math types round trip bit exactly")
{
    Vec2 v2(1.25f, -0.0f);
    Vec3 v3(0.1f, 0.2f, 0.3f);
    Vec4 v4(1, 2, 3, std::numeric_limits<float>::infinity());
    Quat q(Vec3(0.0f, 1.0f, 0.0f), 0.785398f);
    Mat4 m(Vec4(1, 2, 3, 4), Vec4(5, 6, 7, 8), Vec4(9, 10, 11, 12), Vec4(13, 14, 15, 16.5f));

    OutputArchive out;
    out(v2, v3, v4, q, m);

    Vec2 r2;
    Vec3 r3;
    Vec4 r4;
    Quat rq;
    Mat4 rm;
    InputArchive in(out.Buffer());
    in(r2, r3, r4, rq, rm);

    CHECK(in.AtEnd());
    CHECK(BitEqual(r2.X, v2.X) && BitEqual(r2.Y, v2.Y));
    CHECK(BitEqual(r3.X, v3.X) && BitEqual(r3.Y, v3.Y) && BitEqual(r3.Z, v3.Z));
    CHECK(BitEqual(r4.W, v4.W) && BitEqual(r4.X, v4.X));
    CHECK(BitEqual(rq.W, q.W) && BitEqual(rq.X, q.X) && BitEqual(rq.Y, q.Y) &&
          BitEqual(rq.Z, q.Z));
    for (int i = 0; i < 4; ++i)
    {
        CHECK(BitEqual(rm.Rows[i].X, m.Rows[i].X) && BitEqual(rm.Rows[i].Y, m.Rows[i].Y) &&
              BitEqual(rm.Rows[i].Z, m.Rows[i].Z) && BitEqual(rm.Rows[i].W, m.Rows[i].W));
    }
}

TEST_CASE("Archive: reading past the end throws instead of reading garbage")
{
    std::vector<int> numbers = {1, 2, 3, 4, 5};
    std::vector<std::uint8_t> bytes = ToBytes(numbers);
    for (size_t cut = 0; cut < bytes.size(); ++cut)
    {
        std::vector<std::uint8_t> truncated(bytes.begin(), bytes.begin() + cut);
        std::vector<int> out;
        CHECK_THROWS_AS(FromBytes(truncated, out), SerializationError);
    }
}

TEST_CASE("Archive: corrupt container length is rejected before allocating")
{
    // Claims 4 billion elements but only a few bytes follow
    std::vector<std::uint8_t> bytes = {0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x02};
    std::vector<std::uint64_t> out;
    CHECK_THROWS_AS(FromBytes(bytes, out), SerializationError);
    std::string text;
    CHECK_THROWS_AS(FromBytes(bytes, text), SerializationError);
}

TEST_CASE("Archive: invalid boolean byte is rejected")
{
    std::vector<std::uint8_t> bytes = {0x02};
    bool value = false;
    CHECK_THROWS_AS(FromBytes(bytes, value), SerializationError);
}

TEST_CASE("Archive: duplicate keys in a serialized map are rejected")
{
    OutputArchive out;
    out.WriteSize(2);
    std::uint32_t key = 5;
    int value = 1;
    out(key, value, key, value);
    std::unordered_map<std::uint32_t, int> map;
    CHECK_THROWS_AS(FromBytes(out.Buffer(), map), SerializationError);
}

TEST_CASE("Archive: trailing data is detected by FromBytes")
{
    int five = 5;
    std::vector<std::uint8_t> bytes = ToBytes(five);
    bytes.push_back(0);
    int value = 0;
    CHECK_THROWS_AS(FromBytes(bytes, value), SerializationError);
}

TEST_CASE("Archive: version is visible to Serialize functions")
{
    OutputArchive out;
    out.SetVersion(3);
    CHECK_EQ(out.Version(), 3u);
    InputArchive in(out.Buffer());
    in.SetVersion(2);
    CHECK_EQ(in.Version(), 2u);
    InputArchive sub = in.SubArchive(0);
    CHECK_EQ(sub.Version(), 2u);
}

TEST_CASE("Crc32: matches the standard check value")
{
    const char* check = "123456789";
    CHECK_EQ(Crc32(reinterpret_cast<const std::uint8_t*>(check), 9), 0xCBF43926u);
    CHECK_EQ(Crc32(nullptr, 0), 0u);
}

TEST_CASE("Archive: enum values outside the declared range are refused")
{
    std::int32_t raw = 7;
    std::vector<std::uint8_t> bytes = ToBytes(raw);
    TestEnum value = First;
    CHECK_THROWS_AS(FromBytes(bytes, value), SerializationError);
    raw = -5;
    bytes = ToBytes(raw);
    FromBytes(bytes, value);
    CHECK(value == Negative);
}
