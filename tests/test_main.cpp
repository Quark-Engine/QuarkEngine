#include "test_harness.h"

#include <cstring>

namespace testing
{

    std::vector<STestCase>& Registry()
    {
        static std::vector<STestCase> s_vInstance;
        return s_vInstance;
    }

    int g_CurrentTestFailures = 0;

    void ReportFailure(const char* pFile, int line, const std::string& message)
    {
        ++g_CurrentTestFailures;
        std::printf("    %s:%d: %s\n", pFile, line, message.c_str());
    }

    int RunAll()
    {
        int failedTests = 0;
        const char* pCurrentSuite = nullptr;

        for (const STestCase& test : Registry())
        {
            if (pCurrentSuite == nullptr || std::strcmp(pCurrentSuite, test.m_pSuite) != 0)
            {
                pCurrentSuite = test.m_pSuite;
                std::printf("[%s]\n", test.m_pSuite);
            }

            g_CurrentTestFailures = 0;
            test.pfnTest();

            if (g_CurrentTestFailures == 0)
            {
                std::printf("  PASS %s\n", test.m_pName);
            }
            else
            {
                std::printf("  FAIL %s (%d failed assertion(s))\n", test.m_pName, g_CurrentTestFailures);
                ++failedTests;
            }
        }

        std::printf("\n%d test(s), %d failed\n",
                    static_cast<int>(Registry().size()), failedTests);
        return failedTests;
    }

} // testing

int main()
{
    return testing::RunAll() == 0 ? 0 : 1;
}
