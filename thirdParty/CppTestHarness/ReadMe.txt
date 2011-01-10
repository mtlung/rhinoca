-------------------------------------------------------------------------
CppTestHarness

written by Charles Nicholson (cn@cnicholson.net).
linux/gcc port and other various improvements by Dan Lind (podcat@gmail.com).

This work is based on CppUnitLite by Michael Feathers with changes inspired by Noel Llopis.

You the user have free license to do whatever you want with this source code.
No persons mentioned above accept any responsibility if files in this archive 
set your computer on fire or subject you to any number of woes. Use at your own risk!


HISTORY:
-------------------------------------------------------------------------
10 feb 2006, charles nicholson (cn@cnicholson.net)
- TestReporters are nullable for silent output, nuke all MockTestReporters in tests.
- Remove stale forward declares
    both of these thanks to Paulius Maruška (paulius.maruska@gmail.com)

2 feb 2006, rory driscoll (rorydriscoll@gmail.com)
- add CHECK_NULL and CHECK_NOT_NULL for pointer tests.

30 jan 2006, charles nicholson (cn@cnicholson.net)
- remove <cmath> from checks, do CheckClose by hand (it's not hard...)
- Expected and Actual were backwards in failure reporting.  Unify all params to be (Actual, Expected).

25 jan 2006, charles nicholson (cn@cnicholson.net)
- fix bug in BuildFailureString for array comparison, it was printing the expected array twice.

21 jan 2006, charles nicholson (cn@cnicholson.net)
- rename Assert.cpp and .h to ReportAssert.cpp and .h
    "Assert.h" is not a great name for a file, thanks to Erik Parker <martiank9@gmail.com>

19 jan 2006, charles nicholson (cn@cnicholson.net)
- resurrect VS.NET 2003 build, CppTestHarness now provides 2003 & 2005 .sln/.vcproj files

15 jan 2006, charles nicholson (cn@cnicholson.net)
- added CHECK_ASSERT macro and tests, wraps CHECK_THROW(exp, CppTestHarness::AssertException)

11 jan 2006, charles nicholson (cn@cnicholson.net)
- calls to CppTestHarness::ReportAssert will fail whatever test is currently running.
    Redirect your code's Assert functionality to call ReportAssert in test mode to fail tests.

28 dec 2005, charles nicholson (cn@cnicholson.net)
- upgraded win32 build to VS.NET 2005
- silenced all 'conditional expression is constant' warning (CHECK(true), CHECK_EQUAL(1,1), etc...)

20 dec 2005, dan lind (podcat@gmail.com)
- added signal-to-exception translator for posix systems
- more methods in TestReporter. We can now optionaly have output on each finished test
    HTMLTestReporter illustrates a fairly complex reporter doing this.

13 dec 2005, dan lind (podcat@gmail.com)
- added newlines at the end of all files (this is a warning on gcc)
- reordered initialization list of TestRunner (init order not same as order in class)
- added _MSC_VER to TestCppTestHarness.cpp to block pragmas from gcc

11 dec 2005, charles nicholson (cn@cnicholson.net)
- get rid of TestRegistry and static std::vector.
- TestRunner holds a PrintfTestReporter by value to avoid dynamic allocation at static-init
- TestCreator -> TestLauncher are now nodes in a linked list of tests.
