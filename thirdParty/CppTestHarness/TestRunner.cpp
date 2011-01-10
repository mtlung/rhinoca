#include "TestRunner.h"
#include "TestLauncher.h"
#include "TestResults.h"
#include "Test.h"
#include "PrintfTestReporter.h"
#include <iostream>
#include <map>

namespace CppTestHarness {

TestRunner::TestRunner()
	: mTestReporter(&mDefaultTestReporter), mShowTestName(false)
{
}

TestRunner::~TestRunner()
{
}

void TestRunner::SetTestReporter(TestReporter* testReporter) {
	mTestReporter = testReporter;
}

bool TestRunner::RunTest(const std::string& name) {
	TestResults result(mTestReporter);
	TestLauncher::TName2TestMap::iterator i = TestLauncher::GetTestLaunchers().find(name.c_str());
	if(i != TestLauncher::GetTestLaunchers().end()) {
		if(mShowTestName)
			std::cout << "Running test: " << i->first << std::endl;

		i->second->Launch(result);
		return !result.Failed();
	}
	return false;
}

int TestRunner::RunAllTests() {
	size_t failureCount = 0;
	size_t testCount = 0;

	for(TestLauncher::TName2TestMap::iterator i=TestLauncher::GetTestLaunchers().begin();
		i != TestLauncher::GetTestLaunchers().end(); ++i)
	{
		++testCount;

		TestResults result(mTestReporter);

		if(mShowTestName)
			std::cout << "Running test: " << i->first << std::endl;

		i->second->Launch(result);

		if (result.Failed())
			++failureCount;
	}

	if (mTestReporter)
		mTestReporter->ReportSummary(testCount, failureCount);

	return int(failureCount);
}

}
