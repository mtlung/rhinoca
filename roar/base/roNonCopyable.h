#ifndef __roNonCopyable_h__
#define __roNonCopyable_h__

namespace ro {

struct NonCopyable
{
    NonCopyable() = default;
    ~NonCopyable() = default;

	// Emphasize the following members are private
	NonCopyable(const NonCopyable&) = delete;
	const NonCopyable& operator=(const NonCopyable&) = delete;
};	// NonCopyable

}	// namespace ro

#endif	// __roNonCopyable_h__
