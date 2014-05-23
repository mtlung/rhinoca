#ifndef __cpptest_macros_h__
#define __cpptest_macros_h__

//////////////////////////////////////////////////////////////////////////
// Test macros

#define TEST(Name) \
	struct Test##Name : public ::ro::cpptest::Test \
	{ \
		Test##Name() : Test(#Name, __FILE__, __LINE__) {} \
		virtual void runImpl(::ro::cpptest::TestResults& testResults_); \
	}; \
	::ro::cpptest::TypedTestLauncher<Test##Name> staticInitTest##Name##Creator(#Name); \
	\
	void Test##Name::runImpl(::ro::cpptest::TestResults& testResults_)

#define ABORT_TEST_IF(condition) \
	if(const bool _condidtion_ = (condition)) \
	{	CHECK(false); return;	}

#define TEST_FIXTURE(Fixture, Name) \
	struct Test##Fixture##Name : public ::ro::cpptest::Test, public Fixture \
	{ \
		Test##Fixture##Name() : Test(#Fixture "::" #Name, __FILE__, __LINE__) {} \
		virtual void runImpl(::ro::cpptest::TestResults& testResults_); \
	}; \
	::ro::cpptest::TypedTestLauncher<Test##Fixture##Name> staticInitTest##Fixture##Name##Creator(#Fixture "::" #Name); \
	\
	void Test##Fixture##Name::runImpl(::ro::cpptest::TestResults& testResults_)

#define TEST_FIXTURE_CTOR(Fixture, CtorParams, Name) \
	struct Test##Fixture##Name : public ::ro::cpptest::Test, public Fixture \
	{ \
		Test##Fixture##Name() : Test(#Fixture "::" #Name, __FILE__, __LINE__), Fixture CtorParams {} \
		virtual void runImpl(::ro::cpptest::TestResults& testResults_); \
	}; \
	::ro::cpptest::TypedTestLauncher<Test##Fixture##Name> staticInitTest##Fixture##Name##Creator(#Fixture "::" #Name); \
	\
	void Test##Fixture##Name::runImpl(::ro::cpptest::TestResults& testResults_)

#define TEST_FIXTURE_EX(Fixture, Name) \
	struct Test##Fixture##Name : public ::ro::cpptest::Test, public Fixture \
	{ \
		Test##Fixture##Name() : Test(#Fixture "::" #Name, __FILE__, __LINE__) {} \
		virtual void runImpl(::ro::cpptest::TestResults& testResults_); \
	}; \
	::ro::cpptest::TypedTestLauncherEx<Test##Fixture##Name> staticInitTest##Fixture##Name##Creator(#Fixture "::" #Name); \
	\
	void Test##Fixture##Name::runImpl(::ro::cpptest::TestResults& testResults_)

//////////////////////////////////////////////////////////////////////////
// Check macros

#define CHECK(value) \
	if (!::ro::cpptest::check(value)) \
		testResults_.reportFailure(__FILE__, __LINE__, #value);

#define CHECK_NULL(ptr) \
	if (!::ro::cpptest::checkNull(ptr)) \
		testResults_.reportFailure(__FILE__, __LINE__, #ptr " is not NULL.");

#define CHECK_NOT_NULL(ptr) \
	if (::ro::cpptest::checkNull(ptr)) \
		testResults_.reportFailure(__FILE__, __LINE__, #ptr " is NULL.");

#define CHECK_EQUAL(expected, actual) \
	if (!::ro::cpptest::checkEqual(actual, expected)) \
		testResults_.reportFailure(__FILE__, __LINE__, ::ro::cpptest::buildFailureString(actual, expected));

#define CHECK_CLOSE(expected, actual, tolerance) \
	if (!::ro::cpptest::checkClose(actual, expected, tolerance)) \
		testResults_.reportFailure(__FILE__, __LINE__, ::ro::cpptest::buildFailureString(actual, expected));

#define CHECK_ARRAY_EQUAL(expected, actual, count) \
	if (!::ro::cpptest::checkArrayEqual(actual, expected, count)) \
		testResults_.reportFailure(__FILE__, __LINE__, ::ro::cpptest::buildFailureString(actual, expected, count));

#define CHECK_ARRAY_CLOSE(expected, actual, count, tolerance) \
	if (!::ro::cpptest::checkArrayClose(actual, expected, count, tolerance)) \
		testResults_.reportFailure(__FILE__, __LINE__, ::ro::cpptest::buildFailureString(actual, expected, count));

#define CHECK_THROW(expression, ExpectedExceptionType) \
	{ \
		bool caught_ = false; \
		\
		try { expression; } \
		catch (ExpectedExceptionType const&) { caught_ = true; } \
		\
		if(!caught_) \
			testResults_.ReportFailure(__FILE__, __LINE__, "Expected exception: \"" #ExpectedExceptionType "\" not thrown"); \
	}

#define CHECK_ASSERT(expression) \
	CHECK_THROW(expression, ::ro::cpptest::AssertException);

#endif	// __cpptest_macros_h__
