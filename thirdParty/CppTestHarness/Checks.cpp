#include "Checks.h"
#include <stdlib.h> // For wcstombs
#include <limits.h> // For INT_MAX

namespace CppTestHarness {

std::string ToNarrowString(const wchar_t* wstr)
{
	std::string ret;

	// Get the required character count of the destination string (\0 not included)
	unsigned count = ::wcstombs(NULL, wstr, INT_MAX);

	// Check for error
	if(count == unsigned(-1))
		return "";
	if(count != 0)
	{
		ret.resize(count);
		// Warning: The underlying pointer of wstr is being used
		::wcstombs(&(ret.at(0)), wstr, count+1);
	}

	return ret;
}

std::string BuildFailureString(const size_t actual, const size_t expected)
{
	std::stringstream failureStr;
	failureStr << "Expected " << (unsigned int)(expected) << " but got " << (unsigned int)(actual) << std::endl;
	return failureStr.str();
}

}
