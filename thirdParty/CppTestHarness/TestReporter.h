#ifndef TEST_REPORTER_H
#define TEST_REPORTER_H

#include <string>

namespace CppTestHarness {


class TestReporter {
public:
	virtual ~TestReporter();

	virtual void ReportFailure(char const* file, size_t line, std::string failure) = 0;
	virtual void ReportSingleResult(const std::string& testName, bool failed) = 0;
	virtual void ReportSummary(size_t testCount, size_t failureCount) = 0;

protected:
	TestReporter();
};	// TestReporter


}	// namespace CppTestHarness


#endif	// TEST_REPORTER_H
