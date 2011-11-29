#ifndef __STACK_H__
#define __STACK_H__

#include "../roar/base/roArray.h"

template<typename T> class Stack
{
public:
	Stack() {}

// Operations
	void push(const T& val) { _vector.pushBack(val); }

	void pop() { _vector.popBack(); }

	void swap(Stack& rhs) { roSwap(_vector, rhs); }

// Attributes
	rhuint size() const { return _vector.size(); }

	bool empty() const { return _vector.isEmpty(); }

	T& top() { return _vector.back(); }
	const T& top() const { return _vector.back(); }

	T& back() const { return _vector.back(); }

protected:
	ro::Array<T> _vector;
};	// Stack

#endif	// __STACK_H__
