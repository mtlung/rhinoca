#ifndef TYPED_TEST_CREATOR_H_
#define TYPED_TEST_CREATOR_H_

#include "TestLauncher.h"
#include <cassert>

namespace CppTestHarness {


// Typed test launcher
template< typename TestClass >
class TypedTestLauncher : public TestLauncher {
public:
	inline TypedTestLauncher(const char* testName)
		: TestLauncher(testName)
	{
	}

	inline TypedTestLauncher() {}

	inline virtual void Launch(TestResults& testResults_) const {
		TestClass().Run(testResults_);
	}
};	// TypedTestLauncher


// Typed test launcher, which able to let user control init/destroy
template< typename TestClass >
class TypedTestLauncherEx : public TestLauncher {
public:
	inline TypedTestLauncherEx(const char* testName)
		: TestLauncher(testName), mTestClass(NULL)
	{
	}

	inline TypedTestLauncherEx() {
	}

	~TypedTestLauncherEx() {
		if(mTestClass) Destroy();
	}

	inline virtual void Launch(TestResults& testResults_) const {
		if(!mTestClass) Init();
		mTestClass->Run(testResults_);
	}

	inline virtual void Init() const {
		mTestClass = new TestClass();
	}

	inline virtual void Destroy() const {
		assert(mTestClass);
		delete mTestClass;
		mTestClass = NULL;
	}

private:
	mutable TestClass* mTestClass;
};	// TypedFixtureTestLauncher


}	// namespace CppTestHarness


#endif	// TYPED_TEST_CREATOR_H_
