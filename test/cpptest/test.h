#ifndef __cpptest_test_h__
#define __cpptest_test_h__

#include "../../roar/base/roCoroutine.h"
#include "../../roar/base/roNonCopyable.h"
#include "../../roar/base/roMap.h"
#include "../../roar/base/roString.h"

namespace ro {
namespace cpptest {

struct TestReporter
{
	virtual ~TestReporter() {}

	virtual void reportFailure		(const roUtf8* file, roSize line, const String& failure) = 0;
	virtual void reportSingleResult	(const String& testName, bool failed) = 0;
	virtual void reportSummary		(roSize testCount, roSize failureCount) = 0;
};	// TestReporter

struct TestResults : private NonCopyable
{
	explicit TestResults(TestReporter* reporter);

	void reportFailure	(const roUtf8* file, int line, const String& failure);
	void reportDone		(const String& testName);

	bool failed			() const;

// Private:
	bool _failure;
	TestReporter* _testReporter;
};	// TestResults

struct Test : private NonCopyable
{
	void run(CoroutineScheduler& scheduler, TestResults& testResults, roSize repeat);

protected:
	Test(const String& testName, const String& filename, int lineNumber = 0);

private:
	friend struct CoroutineTestRunner;
	virtual void runImpl(TestResults& testResults_) = 0;

	const String _testName;
	const String _filename;
	int const _lineNumber;
};	// Test

struct PrintfTestReporter : public TestReporter
{
	virtual void reportFailure		(const roUtf8* file, roSize line, const String& failure) override;
	virtual void reportSingleResult	(const String& testName, bool failed) override;
	virtual void reportSummary		(roSize testCount, roSize failureCount) override;
};	// PrintfTestReporter

struct TestRunner
{
	TestRunner();
	~TestRunner();

	void setTestReporter(TestReporter* testReporter);

	//! Run a single test
	bool runTest(const char* name);

	//! Run a single test multiple times
	bool runTest(const char* name, roSize repeat);

	//! Run all tests and make a report
	int runAllTests();

	void showTestName(bool show) {
		_showTestName = show;
	}

private:
	TestReporter*		_testReporter;
	PrintfTestReporter	_defaultTestReporter;
	bool				_showTestName;
	CoroutineScheduler	_coScheduler;
};	// TestRunner

struct TestLauncher : public MapNode<const char*, TestLauncher>, private NonCopyable
{
	typedef Map<TestLauncher> Name2TestMap;

	virtual void launch	(CoroutineScheduler& scheduler, TestResults& results_, roSize repeat) const = 0;

	// Init and destroy only meaningful for TEST_FIXTURE
	virtual void init	() const {}
	virtual void destroy() const {}

	const char* getName() const {
		return key();
	}

	static Name2TestMap& getTestLaunchers();
	static TestLauncher* getTestLauncher(const char* name);

protected:
	TestLauncher(const char* testName);
};	// TestLauncher

// Typed test launcher
template<class TestClass>
struct TypedTestLauncher : public TestLauncher
{
	TypedTestLauncher(const char* testName)
		: TestLauncher(testName)
	{
	}

	virtual void launch(CoroutineScheduler& scheduler, TestResults& testResults, roSize repeat) const override
	{
		TestClass().run(scheduler, testResults, repeat);
	}
};	// TypedTestLauncher

// Typed test launcher, which able to let user control init/destroy
template<class TestClass>
struct TypedTestLauncherEx : public TestLauncher
{
	TypedTestLauncherEx(const char* testName)
		: TestLauncher(testName), _testClass(NULL)
	{
	}

	~TypedTestLauncherEx() {
		destroy();
	}

	virtual void launch(CoroutineScheduler& scheduler, TestResults& testResults, roSize repeat) const override
	{
		if(!_testClass)
			init();
		_testClass->Run(scheduler, testResults, repeat);
	}

	virtual void init() const override
	{
		_testClass = new TestClass();
	}

	virtual void destroy() const override
	{
		delete _testClass;
		_testClass = NULL;
	}

private:
	mutable TestClass* _testClass;
};	// TypedTestLauncherEx

}	// namespace cpptest
}	// namespace ro

#endif	// __cpptest_test_h__
