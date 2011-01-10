#include "PrintfTestReporter.h"


#if defined(__ANDROID__)
#	include <android/log.h>
#	define my_printf(...) ((void)__android_log_print(ANDROID_LOG_INFO, "CppTestHarness", __VA_ARGS__))
#else
#	include <cstdio>
#	define my_printf(...) ((void)printf(__VA_ARGS__))
#endif

namespace CppTestHarness {

void PrintfTestReporter::ReportFailure(char const* file, size_t const line, std::string const failure) {
	my_printf("%s(%lu) : failure: %s\n", file, line, failure.c_str());
}

void PrintfTestReporter::ReportSingleResult(const std::string& /*testName*/, bool /*failed*/) {
	//empty
}

void PrintfTestReporter::ReportSummary(size_t const testCount, size_t const failureCount) {
	my_printf("%lu tests run.\n", testCount);
	my_printf("%lu failures.\n", failureCount);
}

}
