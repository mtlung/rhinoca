#include "pch.h"
#include "path.h"
#include "../roar/base/roArray.h"
#include "../roar/platform/roPlatformHeaders.h"

#ifndef _MSC_VER
#	include <unistd.h>	// for getcwd()
#endif

using namespace ro;

namespace {

typedef Path::string_type string_type;
typedef roSize size_type;

static bool IsSlash(Path::char_type c) {
	return c == '/' || c == '\\';
}

//! Find the offset of the first non-slash character in the path, starting from $initialOffset
static size_type FindNonSlash(const string_type& path, size_type initialOffset=0)
{
	for(size_type i=initialOffset, count=path.size(); i<count; ++i) {
		if(!IsSlash(path[i]))
			return i;
	}
	return string_type::npos;
}

//! Find the offset of the relative path
static size_type FindRelativePathPos(const string_type& path)
{
	if(path.isEmpty())
		return string_type::npos;

	size_type pos = path.find(':');

	if(pos != string_type::npos)
		return FindNonSlash(path, pos+1);

	return path[0] == '/' ? 1 : 0;
}

}	// namespace

Path::string_type Path::getRootName() const
{
	size_type pos(mStr.find(':'));
	if(pos != string_type::npos)
		return mStr.substr(0, pos + 1);

	return string_type();
}

Path::string_type Path::getRootDirectory() const
{
	if(mStr.isEmpty())
		return string_type();
	return (mStr[0] == '/' || !getRootName().isEmpty()) ?	"/" : "";
}

// POSIX & Windows cases:	"", "/", "/foo", "foo", "foo/bar"
// Windows only cases:		"c:", "c:/", "c:foo", "c:/foo",
//							"prn:", "//share", "//share/", "//share/foo"
static size_t leafPos(const Path::string_type& str, size_t endPos) // endPos is past-the-end position
{
	// return 0 if str itself is leaf (or isEmpty)
	if(endPos && IsSlash(str[endPos - 1]))
		return endPos - 1;

	size_t pos = Path::string_type::npos;
	for(size_t i=str.size(); i--;)
	{
		if(IsSlash(str[i])) {
			pos = i;
			break;
		}
	}

#ifdef _WIN32
	if(pos == Path::string_type::npos)
		pos = str.rfind(':', endPos - 2);
#endif	// _WIN32

	return (pos == Path::string_type::npos	// path itself must be a leaf (or isEmpty)
#ifdef _WIN32
		|| (pos == 1 && IsSlash(str[0]))	// or share
#endif	// _WIN32
		) ? 0							// so leaf is entire string
		: pos + 1;						// or starts after delimiter
}

Path::string_type Path::getLeaf() const
{
	return mStr.substr(leafPos(mStr, mStr.size()));
}

Path Path::getBranchPath() const
{
	size_t endPos(leafPos(mStr, mStr.size()));

	// Skip a '/' unless it is a root directory
	if(endPos && IsSlash(mStr[endPos - 1]) /*&& !is_absolute_root(mStr, endPos)*/)
		--endPos;
	return Path(mStr.substr(0, endPos));
}

Path::string_type Path::getExtension() const
{
	// Scan from the end for the "." character
	// Return if '\' or '/' encountered
	size_type i = mStr.size();

	while(i--) {
		const char_type c = mStr[i];
		if(c == '.') {
			++i;	// Skip the dot character
			return mStr.substr(i, mStr.size() - i);
		}
		else if(c == '\\' || c == '/')
			break;
	}

	return "";
}

void Path::removeExtension()
{
	// Scan from the end for the "." character
	// Return if '\' or '/' encountered
	size_type i = mStr.size();

	while(i--) {
		const char_type c = mStr[i];
		if(c == '.') {
			mStr.resize(i);
			return;
		}
		else if(c == '\\' || c == '/')
			break;
	}
}

Path& Path::normalize()
{
	if(mStr.isEmpty())
		return *this;

	// Unify all '\' into '/'
	for(size_type i=0, count=mStr.size(); i<count; ++i)
		if(mStr[i] == '\\')
			mStr[i] = '/';

	size_type end, beg(0), start = FindRelativePathPos(mStr);

	while( (beg = mStr.find("/..", beg)) != string_type::npos ) {
		end = beg + 3;
		if( (beg == 1 && mStr[0] == '.')
			|| (beg == 2 && mStr[0] == '.' && mStr[1] == '.')
			// NOTE: The following 2 lines of code handles 3 or more level of "../../../"
			|| (beg > 2 && mStr[beg-3] == '/'
			&& mStr[beg-2] == '.' && mStr[beg-1] == '.') )
		{
			beg = end;
			continue;
		}
		if(end < mStr.size()) {
			if(mStr[end] == '/')
				++end;
			else {	// name starts with ..
				beg = end;
				continue;
			}
		}

		// end is one past end of substr to be erased; now set beg
		while(beg > start && mStr[--beg] != '/') {}
		if(mStr[beg] == '/')
			++beg;
		mStr.erase(beg, end - beg);
		if(beg)
			--beg;
	}

	// Remove all "./" (note that remove from the back is more efficient)
	end = string_type::npos;
	while( (beg = mStr.rfind("./", end)) != string_type::npos ) {
		if(beg == 0 || mStr[beg-1] != '.')	// Not confuse with "../"
			mStr.erase(beg, 2);
		else
			end = beg - 1;
	}

	// Remove trailing '.'
	if(!mStr.isEmpty() && mStr[mStr.size()-1] == '.') {
		if(mStr.size() == 1 || mStr[mStr.size()-2] != '.')	// If not ".."
			mStr.resize(mStr.size() - 1);
	}

	// Remove trailing '/' but not the root path "/"
	beg = mStr.rfind("/");
	if(beg != string_type::npos && beg > start && beg == mStr.size() -1)
		mStr.resize(mStr.size() - 1);

	return *this;
}

Path& Path::operator/=(const Path& rhs)
{
	normalize();

	roAssert(mStr.isEmpty() || !rhs.hasRootDirectory());

	if(!mStr.isEmpty() && mStr[mStr.size()-1] != '/')
		mStr += "/";
	mStr += rhs.mStr;
	normalize();

	return *this;
}

int Path::compare(const Path& rhs) const
{
	return roStrCaseCmp(c_str(), rhs.c_str());
}

Path Path::getCurrentPath()
{
#ifdef _WIN32
	DWORD sz;
	wchar_t dummy[1];	// Use a dummy to avoid a warning in Intel Thread Checker: passing NULL to GetCurrentDirectoryW

	// Query the required buffer size
	if((sz = ::GetCurrentDirectoryW(0, dummy)) == 0)
		return Path();

	TinyArray<wchar_t, 256> wstr(sz);
	if(::GetCurrentDirectoryW(sz, &wstr[0]) == 0)
		return Path();

	String astr;
	if(!astr.fromUtf16((rhuint16*)&wstr[0], wstr.size()))
		return false;

	Path ret(astr.c_str());
	return ret.normalize();
#else
	// For other system we assume it have a UTF-8 locale

	char* buffer = getcwd(NULL, 0);

	if(!buffer)
		return Path();

	Path ret(buffer);
	free(buffer);
	return ret.normalize();
#endif
}
