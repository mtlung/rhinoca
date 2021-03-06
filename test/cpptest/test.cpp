#include "pch.h"
#include "test.h"
#include "../../roar/base/roCoroutine.h"
#include "../../roar/base/roLog.h"
#include "../../roar/base/roRegex.h"
#include "../../roar/base/roStringFormat.h"

namespace ro {
namespace cpptest {

//////////////////////////////////////////////////////////////////////////
// TestResults

TestResults::TestResults(TestReporter* testReporter)
	: _failure(false)
	, _testReporter(testReporter)
{
}

void TestResults::reportFailure(const roUtf8* file, int line, const String& failure)
{
	_failure = true;

	if(_testReporter)
		_testReporter->reportFailure(file, line, failure);
}

void TestResults::reportDone(const String& testName)
{
	if(_testReporter)
		_testReporter->reportSingleResult(testName, _failure);
}

bool TestResults::failed() const
{
	return _failure;
}

//////////////////////////////////////////////////////////////////////////
// Test

Test::Test(const String& testName, const String& filename, int lineNumber)
	: _testName(testName)
	, _filename(filename)
	, _lineNumber(lineNumber)
{
}

struct CoroutineTestRunner : public Coroutine, private NonCopyable
{
	CoroutineTestRunner(Test& test, TestResults& testResults)
		: test(test), results(testResults)
	{
		Coroutine::debugName = "TestRunner coroutine";
		initStack();
	}

	virtual void run() override;
	Test& test;
	TestResults& results;
};

void CoroutineTestRunner::run()
{
	test.runImpl(results);
	delete this;
}

void Test::run(CoroutineScheduler& scheduler, TestResults& testResults, roSize repeat)
{
#if 1
	for(roSize i=0; i<repeat; ++i)
		scheduler.add(*new CoroutineTestRunner(*this, testResults));

	scheduler.runTillAllFinish();
#else
	for(roSize i=0; i<repeat; ++i)
		runImpl(testResults);
#endif

/*	try {
#ifdef TRANSLATE_POSIX_SIGNALS
		//add any signals you want translated into system exceptions here
		SignalTranslator<SIGSEGV> sigSEGV;
		SignalTranslator<SIGFPE> sigFPE;
		SignalTranslator<SIGBUS> sigBUS;
#endif
		runImpl(testResults);
	}
	catch(const AssertException& e) {
		testResults.ReportFailure(e.Filename().c_str(), e.LineNumber(), e.what());
	}
	catch(const std::exception& e) {
		std::string const msg = std::string("Unhandled exception: ") + e.what();
		testResults.ReportFailure(m_filename.c_str(), m_lineNumber, msg);
	}
	catch(...) {
		testResults.ReportFailure(m_filename.c_str(), m_lineNumber, "Unhandled exception: crash!");
	}*/

	testResults.reportDone(_testName);
}

//////////////////////////////////////////////////////////////////////////
// PrintfTestReporter

void PrintfTestReporter::reportFailure(const roUtf8* file, roSize line, const String& failure)
{
	String str;
	roVerify(strFormat(str, "{}({}) : failure: {}\n", file, line, failure));
	roLog("", "%s", str.c_str());
}

void PrintfTestReporter::reportSingleResult(const String& testName, bool failed)
{
	// Empty
}

void PrintfTestReporter::reportSummary(roSize testCount, roSize failureCount)
{
	String str;
	roVerify(strFormat(str, "{} tests run.\n%{} failures.\n", testCount, failureCount));
	roLog("", "%s", str.c_str());
}

//////////////////////////////////////////////////////////////////////////
// TestRunner

TestRunner::TestRunner()
	: _testReporter(&_defaultTestReporter), _showTestName(false)
{
	_coScheduler.init();
}

TestRunner::~TestRunner()
{
	_coScheduler.stop();
}

void TestRunner::setTestReporter(TestReporter* testReporter)
{
	_testReporter = testReporter;
}

bool TestRunner::runTest(const char* testNameRegex)
{
	TestResults result(_testReporter);

	Regex regex;
	Regex::Compiled compiled;
	if(!regex.compile(testNameRegex, compiled))
		return false;

	bool fail = false;
	for(TestLauncher* i = TestLauncher::getTestLaunchers().findMin(); i != NULL; i = i->next()) {
		if(!regex.match(i->getName(), compiled))
			continue;

		if(_showTestName)
			roLog("", "Running test: %s\n", i->getName());

		i->launch(_coScheduler, result, 1);
		fail |= result.failed();
	}

	return !fail;
}

bool TestRunner::runTest(const char* testNameRegex, roSize repeat)
{
	TestResults result(_testReporter);

	Regex regex;
	Regex::Compiled compiled;
	if(!regex.compile(testNameRegex, compiled))
		return false;

	bool fail = false;
	for(TestLauncher* i = TestLauncher::getTestLaunchers().findMin(); i != NULL; i = i->next()) {
		if(!regex.match(i->getName(), compiled))
			continue;

		if(_showTestName)
			roLog("", "Running test: %s for %u times\n", i->getName(), repeat);

		i->launch(_coScheduler, result, repeat);
		fail |= result.failed();
	}

	return !fail;
}

int TestRunner::runAllTests()
{
	roSize failureCount = 0;
	roSize testCount = 0;

	for(TestLauncher* i=TestLauncher::getTestLaunchers().findMin(); i != NULL; i = i->next()) {
		++testCount;

		TestResults result(_testReporter);
		if(_showTestName)
			roLog("", "Running test: %s\n", i->getName());

		i->launch(_coScheduler, result, 1);

		if(result.failed())
			++failureCount;
	}

	if(_testReporter)
		_testReporter->reportSummary(testCount, failureCount);

	return int(failureCount);
}

//////////////////////////////////////////////////////////////////////////
// TestLauncher

TestLauncher::TestLauncher(const char* testName)
	: MapNode(testName)
{
	roVerify(getTestLaunchers().insertUnique(*this));
}

TestLauncher::Name2TestMap& TestLauncher::getTestLaunchers()
{
	static Name2TestMap name2TestMap;
	return name2TestMap;
}

TestLauncher* TestLauncher::getTestLauncher(const char* name)
{
	return getTestLaunchers().find(name);
}

String buildFailureString(roSize actual, roSize expected)
{
	String str;
	roVerify(strFormat(str, "Expected {} but got {}", expected, actual));
	return str;
}

}	// namespace cpptest
}	// namespace ro
