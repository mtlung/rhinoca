#ifndef __NONCOPYABLE_H__
#define __NONCOPYABLE_H__

class Noncopyable
{
protected:
	Noncopyable() {}
	~Noncopyable() {}

private:
	// Emphasize the following members are private
	Noncopyable(const Noncopyable&);
	const Noncopyable& operator=(const Noncopyable&);
};	// Noncopyable

#endif	// __NONCOPYABLE_H__
