#ifndef TEST_RUNNER_H
#define TEST_RUNNER_H

#include "PrintfTestReporter.h"
#include <string>

namespace CppTestHarness {


class TestLauncher;
class TestReporter;

class TestRunner {
public:
	TestRunner();
	~TestRunner();

	void SetTestReporter(TestReporter* testReporter);

	//! Run a single test
	bool RunTest(const std::string& name);

	//! Run all tests and make a report
	int RunAllTests();

	void ShowTestName(bool show) {
		mShowTestName = show;
	}

private:
	TestReporter* mTestReporter;
	PrintfTestReporter mDefaultTestReporter;
	bool mShowTestName;
};


}	// namespace CppTestHarness


#endif	// TEST_RUNNER_H
