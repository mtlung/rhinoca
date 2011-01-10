nmake /f js.VS2005.mak CFG="js - Win32 Debug"
nmake /f js.VS2005.mak CFG="js - Win32 Release"

move .\Debug\js32d.dll ..\..\bin\js32d.dll
move .\Release\js32.dll ..\..\bin\js32.dll

move .\Debug\js32d.lib ..\..\lib\js32d.lib
move .\Debug\js32d.exp ..\..\lib\js32d.exp

move .\Release\js32.lib ..\..\lib\js32.lib
move .\Release\js32.exp ..\..\lib\js32.exp
