#ifndef __TESTS_TEST_HARNESS_H__
#define __TESTS_TEST_HARNESS_H__
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace testing
{

struct STestCase
{
    const char* pSuite;
    const char* pName;
    void (*pfnTest)();
};

std::vector<STestCase>& Registry();
void ReportFailure(const char* pFile, int line, const std::string& message);
int RunAll();

struct SRegistrar
{
    SRegistrar(const char* pSuite, const char* pName, void (*pfnTest)())
    {
        Registry().push_back(STestCase{ pSuite, pName, pfnTest });
    }
};

extern int g_CurrentTestFailures;

} // testing

#define TEST(suite, name) \
    static void suite##_##name##_body(); \
    static ::testing::SRegistrar suite##_##name##_registrar( \
        #suite, #name, &suite##_##name##_body); \
    static void suite##_##name##_body()

#define CHECK(cond) \
    do \
    { \
        if (!(cond)) \
        { \
            ::testing::ReportFailure(__FILE__, __LINE__, \
                std::string("CHECK(") + #cond + ")"); \
        } \
    } while (0)

#define CHECK_MSG(cond, msg) \
    do \
    { \
        if (!(cond)) \
        { \
            ::testing::ReportFailure(__FILE__, __LINE__, \
                std::string("CHECK(") + #cond + ") -- " + (msg)); \
        } \
    } while (0)

#define CHECK_NEAR(actual, expected, eps) \
    do \
    { \
        const double CheckActual = static_cast<double>(actual); \
        const double CheckExpected = static_cast<double>(expected); \
        const double CheckTolerance = static_cast<double>(eps); \
        if (!(std::fabs(CheckActual - CheckExpected) <= CheckTolerance)) \
        { \
            ::testing::ReportFailure(__FILE__, __LINE__, \
                std::string("CHECK_NEAR(") + #actual + ", " + #expected + ", " + \
                #eps + ") -> got " + std::to_string(CheckActual) + \
                ", want " + std::to_string(CheckExpected)); \
        } \
    } while (0)

#endif // __TESTS_TEST_HARNESS_H__
