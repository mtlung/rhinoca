#ifndef __roNonCopyable_h__
#define __roNonCopyable_h__

namespace ro {

class NonCopyable
{
protected:
	NonCopyable() {}

private:
	// Emphasize the following members are private
	NonCopyable(const NonCopyable&);
	const NonCopyable& operator=(const NonCopyable&);
};	// NonCopyable

}	// namespace ro

#endif	// __roNonCopyable_h__
