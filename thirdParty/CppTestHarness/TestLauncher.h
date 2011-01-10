#ifndef TEST_LAUNCHER_H
#define TEST_LAUNCHER_H

#include <map>
#include <string>

namespace CppTestHarness {


class TestResults;

class TestLauncher {
public:
	typedef std::map<std::string, TestLauncher*> TName2TestMap;

	virtual void Launch(TestResults& results_) const = 0;

	// Init and destroy only meaningful for TEST_FIXTURE
	virtual void Init() const;
	virtual void Destroy() const;

	static TName2TestMap& GetTestLaunchers();
	static TestLauncher& GetTestLauncher(const std::string& name);

protected:
	TestLauncher(const char* testName);
	virtual ~TestLauncher();

	const char* GetName() const {
		return m_Name;
	}

private:
	const char* m_Name;

	// revoked
	TestLauncher();
	TestLauncher(TestLauncher const&);
	TestLauncher& operator =(TestLauncher const&);
};


}	// CppTestHarness


#endif	// TEST_LAUNCHER_H
