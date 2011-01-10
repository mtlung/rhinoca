#ifndef __PATH_H__
#define __PATH_H__

#include <string>

/*!	A path class that is similar the one provided in boost.
	\note Comparison of Path in windows is case in-sensitive.
 */
class Path
{
public:
	typedef char char_type;
	typedef std::basic_string<char_type> string_type;

	Path() {}

	Path(const char_type* path) : mStr(path) {}

	Path(const string_type& path) : mStr(path) {}

	const char* c_str() const {
		return mStr.c_str();
	}

	/*! Get the root name.
		"C:"	->	"C:"	\n
		"C:\"	->	"C:"	\n
		"C:\B"	->	"C:"	\n
		"/"		->	""		\n
		"/home"	->	""		\n
		"./"	->	""		\n
		""		->	""		\n
	 */
	string_type getRootName() const;

	/*! Get the root directory, it will only be empty or with the value '/'.
		"C:"	->	"/"		\n
		"C:\"	->	"/"		\n
		"C:\B"	->	"/"		\n
		"/"		->	"/"		\n
		"/home"	->	"/"		\n
		"./"	->	""		\n
		""		->	""		\n
	 */
	string_type getRootDirectory() const;

	bool hasRootDirectory() const {
		return !getRootDirectory().empty();
	}

	/*!	Get the leaf path.
		""				-> ""		\n
		"/"				-> "/"		\n
		"a.txt"			-> "a.txt"	\n
		"/home/a.b.txt"	-> "a.b.txt"\n
	 */
	string_type getLeaf() const;

	/*!	Get the branch path.
		""				-> ""		\n
		"/"				-> ""		\n
		"a.txt"			-> ""		\n
		"/home/a.b.txt"	-> "/home"	\n
	 */
	Path getBranchPath() const;

	/*!	Get the file extension.
		""				-> ""		\n
		"a.txt"			-> "txt"	\n
		"a.txt/"		-> ""		\n
		"a.b.txt"		-> "txt"	\n
	 */
	string_type getExtension() const;

	/*!	Remove the file extension in the path.
		"/home/a.b.txt" -> "/home/a.b"
	 */
	void removeExtension();

	/*!	Normalize the path to a standard form.
		"./././"		-> ""		\n
		"C:\"			-> "C:/"	\n
		"/home/./"		-> "/home"	\n
		"/home/../bar"	-> "/bar"	\n
	 */
	Path& normalize();

	/*! Append.
		"/a/b" / "./c"	->	"/a/b/c"	\n
		"/a/b" / "c"	->	"/a/b/c"	\n
		"/a/b" / "./c"	->	"/a/b/c"	\n
		"/a/b" / "../c"	->	"/a/c"		\n
		"/a/b" / "/c"	->	Error: cannot append with root	\n
	 */
	Path& operator/=(const Path& rhs);

	Path operator/(const Path& rhs) const {
		return Path(*this) /= rhs;
	}

	/*!	Beware that comparison may not be accurate if the path are not normalized.
		Also, comparsion on Windows platforms are case in-sensitive.
	 */
	bool operator==(const Path& rhs) const {
		return compare(rhs) == 0;
	}

	bool operator!=(const Path& rhs) const {
		return compare(rhs) != 0;
	}

	bool operator<(const Path& rhs) const {
		return compare(rhs) < 0;
	}

	//!	A comparison function that gives int as the result just like what strCaseCmp() does.
	int compare(const Path& rhs) const;

protected:
	string_type mStr;
};	// Path

#endif	// __PATH_H__
