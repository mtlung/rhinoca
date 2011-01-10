#include "TestLauncher.h"
#include <cassert>
#include <stdexcept>
#include <utility>

namespace CppTestHarness {

TestLauncher::TName2TestMap& TestLauncher::GetTestLaunchers() {
	static TestLauncher::TName2TestMap Name2TestMap;
	return Name2TestMap;
}

TestLauncher& TestLauncher::GetTestLauncher(const std::string& name) {
	TestLauncher::TName2TestMap::iterator i = GetTestLaunchers().find(name);
	if(i == GetTestLaunchers().end())
		throw "GetTestLauncher failed: unknown test name";

	return *(i->second);
}

TestLauncher::TestLauncher(const char* testName)
	: m_Name(testName)
{
	bool success = GetTestLaunchers().insert(std::make_pair(std::string(testName), this)).second;
	assert(success);
	(void)success;	// Hide warning
}

void TestLauncher::Init() const {
}

void TestLauncher::Destroy() const {
}

TestLauncher::~TestLauncher() {
}

}
