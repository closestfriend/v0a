#pragma once

#include <cmath>
#include <exception>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace test
{
using TestFunction = std::function<void()>;

struct TestCase
{
    std::string name;
    TestFunction function;
};

inline std::vector<TestCase>& registry()
{
    static std::vector<TestCase> tests;
    return tests;
}

struct Registrar
{
    Registrar(std::string name, TestFunction function)
    {
        registry().push_back({ std::move(name), std::move(function) });
    }
};

inline void require(bool condition, const char* expression, const char* file, int line)
{
    if (!condition)
    {
        std::ostringstream message;
        message << file << ':' << line << ": requirement failed: " << expression;
        throw std::runtime_error(message.str());
    }
}

inline void requireNear(double actual,
                        double expected,
                        double tolerance,
                        const char* file,
                        int line)
{
    if (!std::isfinite(actual) || std::abs(actual - expected) > tolerance)
    {
        std::ostringstream message;
        message << file << ':' << line << ": expected " << expected << " +/- " << tolerance
                << ", got " << actual;
        throw std::runtime_error(message.str());
    }
}

inline int runAll()
{
    int failures = 0;
    for (const auto& testCase : registry())
    {
        try
        {
            testCase.function();
            std::cout << "PASS " << testCase.name << '\n';
        }
        catch (const std::exception& error)
        {
            ++failures;
            std::cerr << "FAIL " << testCase.name << ": " << error.what() << '\n';
        }
    }
    std::cout << registry().size() - static_cast<std::size_t>(failures) << '/'
              << registry().size() << " tests passed\n";
    return failures == 0 ? 0 : 1;
}
} // namespace test

#define TEST_JOIN_IMPL(a, b) a##b
#define TEST_JOIN(a, b) TEST_JOIN_IMPL(a, b)
#define TEST_CASE(name)                                                                          \
    static void TEST_JOIN(testFunction_, __LINE__)();                                             \
    static test::Registrar TEST_JOIN(testRegistrar_, __LINE__)(name, TEST_JOIN(testFunction_, __LINE__)); \
    static void TEST_JOIN(testFunction_, __LINE__)()
#define REQUIRE(expression) test::require((expression), #expression, __FILE__, __LINE__)
#define REQUIRE_NEAR(actual, expected, tolerance)                                                 \
    test::requireNear((actual), (expected), (tolerance), __FILE__, __LINE__)
