#ifndef TEST_MACROS_H
#define TEST_MACROS_H

//----------------------------
#define TEST(Name) \
	class Test##Name : public CppTestHarness::Test \
	{ \
	public: \
		Test##Name() : Test(#Name, __FILE__, __LINE__) {} \
	private: \
		virtual void RunImpl(CppTestHarness::TestResults& testResults_); \
	}; \
	CppTestHarness::TypedTestLauncher< Test##Name > \
		staticInitTest##Name##Creator(#Name); \
	\
	void Test##Name::RunImpl(CppTestHarness::TestResults& testResults_)

#define ABORT_TEST_IF(condition) \
	if(const bool _condidtion_ = (condition)) \
	{	CHECK(false); return;	}

//----------------------------
#define TEST_FIXTURE(Fixture, Name) \
	class Test##Fixture##Name : public CppTestHarness::Test, public Fixture \
	{ \
	public: \
		Test##Fixture##Name() : Test(#Fixture "::" #Name, __FILE__, __LINE__) {} \
	private: \
		virtual void RunImpl(CppTestHarness::TestResults& testResults_); \
	}; \
	CppTestHarness::TypedTestLauncher< Test##Fixture##Name > \
	staticInitTest##Fixture##Name##Creator(#Fixture "::" #Name); \
	\
	void Test##Fixture##Name::RunImpl(CppTestHarness::TestResults& testResults_)

//----------------------------
#define TEST_FIXTURE_CTOR(Fixture, CtorParams, Name) \
	class Test##Fixture##Name : public CppTestHarness::Test, public Fixture \
	{ \
	public: \
		Test##Fixture##Name() : Test(#Fixture "::" #Name, __FILE__, __LINE__), Fixture CtorParams {} \
	private: \
		virtual void RunImpl(CppTestHarness::TestResults& testResults_); \
	}; \
	CppTestHarness::TypedTestLauncher< Test##Fixture##Name > \
		staticInitTest##Fixture##Name##Creator(#Fixture "::" #Name); \
	\
	void Test##Fixture##Name::RunImpl(CppTestHarness::TestResults& testResults_)

//----------------------------
#define TEST_FIXTURE_EX(Fixture, Name) \
	class Test##Fixture##Name : public CppTestHarness::Test, public Fixture \
	{ \
	public: \
		Test##Fixture##Name() : Test(#Fixture "::" #Name, __FILE__, __LINE__) {} \
	private: \
		virtual void RunImpl(CppTestHarness::TestResults& testResults_); \
	}; \
	CppTestHarness::TypedTestLauncherEx< Test##Fixture##Name > \
		staticInitTest##Fixture##Name##Creator(#Fixture "::" #Name); \
	\
	void Test##Fixture##Name::RunImpl(CppTestHarness::TestResults& testResults_)

#endif

