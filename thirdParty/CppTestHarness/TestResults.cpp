#include "TestResults.h"
#include "TestReporter.h"

namespace CppTestHarness {

TestResults::TestResults(TestReporter* testReporter)
	: m_failure(false)
	, m_testReporter(testReporter)
{
}

void TestResults::ReportFailure(char const* file, int const line, std::string const failure)
{
	m_failure = true;

	if (m_testReporter)
		m_testReporter->ReportFailure(file, line, failure);
}

void TestResults::ReportDone(const std::string& testName)
{
	if (m_testReporter)
		m_testReporter->ReportSingleResult(testName, m_failure);
}

bool TestResults::Failed() const
{
	return m_failure;
}

}
