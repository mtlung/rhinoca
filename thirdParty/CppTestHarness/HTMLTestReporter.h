#ifndef HTML_TEST_REPORTER
#define HTML_TEST_REPORTER

#include "TestReporter.h"
#include <vector>

namespace CppTestHarness {

class HTMLTestReporter : public TestReporter {
public:
	virtual void ReportFailure(char const* file, size_t line, std::string failure);
	virtual void ReportSingleResult(const std::string& testName, bool failed);
	virtual void ReportSummary(size_t testCount, size_t failureCount);

private:

	typedef std::vector<std::string> MessageList;
	MessageList m_failureMessages;

	struct ResultRecord {
		std::string testName;
		bool failed;
		MessageList failureMessages;
	};

	typedef std::vector<ResultRecord> ResultList;
	ResultList m_results;
};	// HTMLTestReporter

}	// namespace CppTestHarness

#endif	// HTML_TEST_REPORTER
