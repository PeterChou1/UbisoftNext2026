//---------------------------------------------------------------------------------
// TestFramework.h
//---------------------------------------------------------------------------------
//
// Minimal self contained unit test framework (no external dependencies so the
// tests build anywhere the game builds).
//
//   TEST_CASE("Name") { CHECK(a == b); REQUIRE(ptr != nullptr); }
//
// CHECK records a failure and continues, REQUIRE aborts the current test.
// Run the executable with a substring argument to only run matching tests.
//
#pragma once

#include <cmath>
#include <cstring>
#include <exception>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace TestFramework
{
    struct TestCase
    {
        std::string Name;
        std::function<void()> Body;
        const char* File;
        int Line;
    };

    struct RequireFailed
    {
    };

    inline std::vector<TestCase>& Registry()
    {
        static std::vector<TestCase> tests;
        return tests;
    }

    inline int& CurrentFailures()
    {
        static int failures = 0;
        return failures;
    }

    inline int& TotalChecks()
    {
        static int checks = 0;
        return checks;
    }

    struct Registrar
    {
        Registrar(const char* name, std::function<void()> body, const char* file, int line)
        {
            Registry().push_back({name, std::move(body), file, line});
        }
    };

    inline void ReportFailure(const char* file, int line, const std::string& message)
    {
        ++CurrentFailures();
        std::cerr << "    " << file << ":" << line << ": FAILED: " << message << "\n";
    }

    inline int RunAll(int argc, char** argv)
    {
        std::string filter = argc > 1 ? argv[1] : "";
        int passed = 0;
        int failed = 0;
        for (const TestCase& test : Registry())
        {
            if (!filter.empty() && test.Name.find(filter) == std::string::npos)
                continue;
            CurrentFailures() = 0;
            std::cout << "[ RUN  ] " << test.Name << std::endl;
            try
            {
                test.Body();
            }
            catch (const RequireFailed&)
            {
            }
            catch (const std::exception& e)
            {
                ReportFailure(test.File, test.Line, std::string("unexpected exception: ") + e.what());
            }
            catch (...)
            {
                ReportFailure(test.File, test.Line, "unexpected unknown exception");
            }
            if (CurrentFailures() == 0)
            {
                ++passed;
                std::cout << "[  OK  ] " << test.Name << std::endl;
            }
            else
            {
                ++failed;
                std::cout << "[ FAIL ] " << test.Name << std::endl;
            }
        }
        std::cout << "\n" << passed << " passed, " << failed << " failed, " << TotalChecks()
                  << " checks" << std::endl;
        if (passed + failed == 0)
        {
            std::cout << "No test matched the filter '" << filter << "'" << std::endl;
            return 1;
        }
        return failed == 0 ? 0 : 1;
    }
} // namespace TestFramework

#define TF_CONCAT_INNER(a, b) a##b
#define TF_CONCAT(a, b) TF_CONCAT_INNER(a, b)

#define TEST_CASE(name)                                                                        \
    static void TF_CONCAT(TestBody_, __LINE__)();                                              \
    static TestFramework::Registrar TF_CONCAT(TestRegistrar_, __LINE__)(                       \
            name, &TF_CONCAT(TestBody_, __LINE__), __FILE__, __LINE__);                        \
    static void TF_CONCAT(TestBody_, __LINE__)()

#define CHECK(...)                                                                            \
    do                                                                                         \
    {                                                                                          \
        ++TestFramework::TotalChecks();                                                        \
        if (!(__VA_ARGS__))                                                                    \
            TestFramework::ReportFailure(__FILE__, __LINE__, #__VA_ARGS__);                    \
    } while (0)

#define REQUIRE(...)                                                                          \
    do                                                                                         \
    {                                                                                          \
        ++TestFramework::TotalChecks();                                                        \
        if (!(__VA_ARGS__))                                                                    \
        {                                                                                      \
            TestFramework::ReportFailure(__FILE__, __LINE__, #__VA_ARGS__);                    \
            throw TestFramework::RequireFailed{};                                              \
        }                                                                                      \
    } while (0)

#define CHECK_EQ(a, b)                                                                         \
    do                                                                                         \
    {                                                                                          \
        ++TestFramework::TotalChecks();                                                        \
        auto&& tfA = (a);                                                                      \
        auto&& tfB = (b);                                                                      \
        if (!(tfA == tfB))                                                                     \
        {                                                                                      \
            std::ostringstream tfMsg;                                                          \
            tfMsg << #a << " == " << #b << "  (" << tfA << " vs " << tfB << ")";               \
            TestFramework::ReportFailure(__FILE__, __LINE__, tfMsg.str());                     \
        }                                                                                      \
    } while (0)

#define CHECK_THROWS_AS(expr, ExceptionType)                                                   \
    do                                                                                         \
    {                                                                                          \
        ++TestFramework::TotalChecks();                                                        \
        bool tfThrew = false;                                                                  \
        try                                                                                    \
        {                                                                                      \
            expr;                                                                              \
        }                                                                                      \
        catch (const ExceptionType&)                                                           \
        {                                                                                      \
            tfThrew = true;                                                                    \
        }                                                                                      \
        if (!tfThrew)                                                                          \
            TestFramework::ReportFailure(__FILE__, __LINE__,                                   \
                                         #expr " did not throw " #ExceptionType);              \
    } while (0)
