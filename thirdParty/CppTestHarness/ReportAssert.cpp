#include "ReportAssert.h"
#include "AssertException.h"

namespace CppTestHarness {

void ReportAssert(char const* description, char const* filename, int const lineNumber)
{
	std::string const strDesc = std::string("Assert: ") + description;
	throw AssertException(strDesc.c_str(), filename, lineNumber);
}

}
