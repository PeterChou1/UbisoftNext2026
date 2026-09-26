//---------------------------------------------------------------------------------
// TextArchive.h
//---------------------------------------------------------------------------------
//
// Plain text archives with the same interface as the binary archives
// (Archive.h), so every Serialize(ar, value) function also reads and writes
// text without any change:
//
//   numbers   shortest decimal that reads back to the exact same bits
//             (floats: 0.1, 3.5, 100, -0, inf, nan)
//   bool      true / false
//   enums     their integer value
//   strings   "quoted" with \" \\ \n \t \r escapes
//   sizes     [n] before the n elements of a container
//
// A TextInputArchive reads one record (one line of a text save file); every
// problem throws a SerializationError naming the line and the token.
//
#pragma once

#include "Archive.h"

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace Serialization
{
    class TextOutputArchive
    {
      public:
        static constexpr bool IsLoading = false;
        static constexpr bool IsSaving = true;

        template <typename... Ts>
        TextOutputArchive& operator()(Ts&... values)
        {
            (Dispatch(*this, values), ...);
            return *this;
        }

        std::uint32_t Version() const { return m_Version; }
        void SetVersion(std::uint32_t version) { m_Version = version; }

        template <typename T>
        void WritePrimitive(T value)
        {
            if constexpr (std::is_same_v<T, bool>)
                Token(value ? "true" : "false");
            else if constexpr (std::is_enum_v<T>)
                Token(std::to_string(static_cast<std::int32_t>(value)));
            else if constexpr (std::is_floating_point_v<T>)
                Token(FormatFloat(value));
            else if constexpr (std::is_same_v<T, char> || std::is_same_v<T, signed char>)
                Token(std::to_string(static_cast<int>(value)));
            else if constexpr (std::is_same_v<T, unsigned char>)
                Token(std::to_string(static_cast<unsigned>(value)));
            else
            {
                static_assert(std::is_integral_v<T>, "WritePrimitive requires a primitive type");
                Token(std::to_string(value));
            }
        }

        void WriteSize(std::size_t size) { Token("[" + std::to_string(size) + "]"); }

        /**
         * \brief Raw bytes as one hex token (x:0a1b...)
         */
        void WriteBytes(const void* data, std::size_t size)
        {
            static const char* digits = "0123456789abcdef";
            const auto* bytes = static_cast<const std::uint8_t*>(data);
            std::string token = "x:";
            for (std::size_t i = 0; i < size; ++i)
            {
                token += digits[bytes[i] >> 4];
                token += digits[bytes[i] & 15];
            }
            Token(token);
        }

        void WriteString(const std::string& value)
        {
            std::string token = "\"";
            for (char c : value)
            {
                switch (c)
                {
                case '"':
                    token += "\\\"";
                    break;
                case '\\':
                    token += "\\\\";
                    break;
                case '\n':
                    token += "\\n";
                    break;
                case '\r':
                    token += "\\r";
                    break;
                case '\t':
                    token += "\\t";
                    break;
                default:
                    if (static_cast<unsigned char>(c) < 32 || c == 127)
                    {
                        char escaped[8];
                        std::snprintf(
                                escaped, sizeof(escaped), "\\x%02x", static_cast<unsigned char>(c));
                        token += escaped;
                    }
                    else
                        token += c;
                }
            }
            token += '"';
            Token(token);
        }

        /**
         * \brief One bare word, separated from the previous token (the field
         *        headers of reflected types: Name:type)
         */
        void WriteWord(const std::string& word) { Token(word); }

        /**
         * \brief Write text as is (keywords, separators, comments)
         */
        void Raw(const std::string& text) { m_Text += text; }

        void NewLine() { m_Text += '\n'; }

        const std::string& Text() const { return m_Text; }

        template <typename T>
        static std::string FormatFloat(T value)
        {
            if (std::isnan(value))
                return "nan";
            if (std::isinf(value))
                return value < 0 ? "-inf" : "inf";
            // Shortest precision that reads back to the same bits
            constexpr int maxDigits = sizeof(T) == 4 ? 9 : 17;
            char buffer[64];
            for (int digits = 1; digits <= maxDigits; ++digits)
            {
                std::snprintf(buffer, sizeof(buffer), "%.*g", digits, static_cast<double>(value));
                T back = sizeof(T) == 4 ? static_cast<T>(std::strtof(buffer, nullptr))
                                        : static_cast<T>(std::strtod(buffer, nullptr));
                if (std::memcmp(&back, &value, sizeof(T)) == 0)
                    break;
            }
            // Whole numbers without an exponent (100, not 1e+02)
            if (std::strchr(buffer, 'e') != nullptr && std::fabs(value) < static_cast<T>(1e15) &&
                value == std::floor(value))
                std::snprintf(buffer, sizeof(buffer), "%.0f", static_cast<double>(value));
            return buffer;
        }

      private:
        void Token(const std::string& token)
        {
            if (!m_Text.empty() && m_Text.back() != '\n' && m_Text.back() != ' ')
                m_Text += ' ';
            m_Text += token;
        }

        std::string m_Text;
        std::uint32_t m_Version = 1;
    };

    class TextInputArchive
    {
      public:
        static constexpr bool IsLoading = true;
        static constexpr bool IsSaving = false;

        /**
         * \param text one record
         * \param line line number used in error messages
         */
        TextInputArchive(std::string text, std::size_t line = 0)
            : m_Text(std::move(text))
            , m_Line(line)
        {
        }

        template <typename... Ts>
        TextInputArchive& operator()(Ts&... values)
        {
            (Dispatch(*this, values), ...);
            return *this;
        }

        std::uint32_t Version() const { return m_Version; }
        void SetVersion(std::uint32_t version) { m_Version = version; }

        template <typename T>
        void ReadPrimitive(T& value)
        {
            std::string token = NextToken("a value");
            if constexpr (std::is_same_v<T, bool>)
            {
                if (token == "true")
                    value = true;
                else if (token == "false")
                    value = false;
                else
                    Fail("expected true or false, got '" + token + "'");
            }
            else if constexpr (std::is_floating_point_v<T>)
            {
                char* end = nullptr;
                double parsed = std::strtod(token.c_str(), &end);
                if (end == token.c_str() || *end != '\0')
                    Fail("expected a number, got '" + token + "'");
                // strtof keeps the exact float value written by FormatFloat
                if constexpr (sizeof(T) == 4)
                    value = std::strtof(token.c_str(), nullptr);
                else
                    value = static_cast<T>(parsed);
            }
            else if constexpr (std::is_enum_v<T>)
            {
                auto raw = static_cast<std::int32_t>(ParseInteger(token, INT32_MIN, INT32_MAX));
                try
                {
                    CheckEnumValue<T>(raw);
                }
                catch (const SerializationError& e)
                {
                    Fail(e.what());
                }
                value = static_cast<T>(raw);
            }
            else
            {
                static_assert(std::is_integral_v<T>, "ReadPrimitive requires a primitive type");
                if constexpr (std::is_signed_v<T>)
                {
                    value = static_cast<T>(
                            ParseInteger(token,
                                         static_cast<long long>(std::numeric_limits<T>::min()),
                                         static_cast<long long>(std::numeric_limits<T>::max())));
                }
                else
                {
                    value = static_cast<T>(ParseUnsigned(token, std::numeric_limits<T>::max()));
                }
            }
        }

        std::size_t ReadSize()
        {
            std::string token = NextToken("[count]");
            if (token.size() < 3 || token.front() != '[' || token.back() != ']')
                Fail("expected a [count], got '" + token + "'");
            std::size_t size = static_cast<std::size_t>(ParseUnsigned(
                    token.substr(1, token.size() - 2), std::numeric_limits<std::uint32_t>::max()));
            // Every element takes at least one character: refuse impossible
            // counts before allocating anything
            if (size > m_Text.size() - m_Position)
                Fail("count " + token + " is larger than the rest of the line");
            return size;
        }

        void ReadBytes(void* out, std::size_t size)
        {
            std::string token = NextToken("bytes");
            if (token.size() != 2 + size * 2 || token.compare(0, 2, "x:") != 0)
                Fail("expected " + std::to_string(size) + " hex bytes, got '" + token + "'");
            auto* bytes = static_cast<std::uint8_t*>(out);
            for (std::size_t i = 0; i < size; ++i)
            {
                int high = HexValue(token[2 + 2 * i]);
                int low = HexValue(token[3 + 2 * i]);
                if (high < 0 || low < 0)
                    Fail("bad hex byte in '" + token + "'");
                bytes[i] = static_cast<std::uint8_t>(high * 16 + low);
            }
        }

        std::string ReadString()
        {
            SkipSpaces();
            if (m_Position >= m_Text.size() || m_Text[m_Position] != '"')
                Fail("expected a \"string\"");
            ++m_Position;
            std::string value;
            while (true)
            {
                if (m_Position >= m_Text.size())
                    Fail("unterminated string");
                char c = m_Text[m_Position++];
                if (c == '"')
                    break;
                if (c != '\\')
                {
                    value += c;
                    continue;
                }
                if (m_Position >= m_Text.size())
                    Fail("unterminated escape in string");
                char e = m_Text[m_Position++];
                switch (e)
                {
                case '"':
                    value += '"';
                    break;
                case '\\':
                    value += '\\';
                    break;
                case 'n':
                    value += '\n';
                    break;
                case 'r':
                    value += '\r';
                    break;
                case 't':
                    value += '\t';
                    break;
                case 'x': {
                    if (m_Position + 2 > m_Text.size())
                        Fail("bad \\x escape in string");
                    int high = HexValue(m_Text[m_Position]);
                    int low = HexValue(m_Text[m_Position + 1]);
                    if (high < 0 || low < 0)
                        Fail("bad \\x escape in string");
                    value += static_cast<char>(high * 16 + low);
                    m_Position += 2;
                    break;
                }
                default:
                    Fail(std::string("unknown escape \\") + e + " in string");
                }
            }
            return value;
        }

        /**
         * \brief Only spaces left
         */
        bool AtEnd()
        {
            SkipSpaces();
            return m_Position >= m_Text.size();
        }

        std::string Rest()
        {
            SkipSpaces();
            return m_Text.substr(m_Position);
        }

        [[noreturn]] void Fail(const std::string& message) const
        {
            throw SerializationError("line " + std::to_string(m_Line) + ": " + message);
        }

        std::string NextToken(const char* what)
        {
            SkipSpaces();
            if (m_Position >= m_Text.size())
                Fail(std::string("expected ") + what + ", reached the end of the line");
            std::size_t start = m_Position;
            while (m_Position < m_Text.size() && m_Text[m_Position] != ' ' &&
                   m_Text[m_Position] != '\t')
                ++m_Position;
            return m_Text.substr(start, m_Position - start);
        }

      private:
        void SkipSpaces()
        {
            while (m_Position < m_Text.size() &&
                   (m_Text[m_Position] == ' ' || m_Text[m_Position] == '\t'))
                ++m_Position;
        }

        static int HexValue(char c)
        {
            if (c >= '0' && c <= '9')
                return c - '0';
            if (c >= 'a' && c <= 'f')
                return c - 'a' + 10;
            if (c >= 'A' && c <= 'F')
                return c - 'A' + 10;
            return -1;
        }

        long long ParseInteger(const std::string& token, long long min, long long max) const
        {
            errno = 0;
            char* end = nullptr;
            long long value = std::strtoll(token.c_str(), &end, 10);
            if (end == token.c_str() || *end != '\0' || errno == ERANGE || value < min ||
                value > max)
                Fail("expected an integer, got '" + token + "'");
            return value;
        }

        unsigned long long ParseUnsigned(const std::string& token, unsigned long long max) const
        {
            errno = 0;
            char* end = nullptr;
            if (token.empty() || token[0] == '-')
                Fail("expected a positive integer, got '" + token + "'");
            unsigned long long value = std::strtoull(token.c_str(), &end, 10);
            if (end == token.c_str() || *end != '\0' || errno == ERANGE || value > max)
                Fail("expected a positive integer, got '" + token + "'");
            return value;
        }

        std::string m_Text;
        std::size_t m_Position = 0;
        std::size_t m_Line = 0;
        std::uint32_t m_Version = 1;
    };

    // Strings are quoted tokens in text (the generic overload writes a size
    // and raw bytes, which binary archives want)
    inline void Serialize(TextOutputArchive& ar, std::string& value)
    {
        ar.WriteString(value);
    }

    inline void Serialize(TextInputArchive& ar, std::string& value)
    {
        value = ar.ReadString();
    }
} // namespace Serialization
