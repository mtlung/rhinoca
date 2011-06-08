#ifndef __STACK_H__
#define __STACK_H__

#include "vector.h"

template<typename T> class Stack
{
public:
	Stack() {}

// Operations
	void push(const T& val) { _vector.push_back(val); }

	void pop() { _vector.pop_back(); }

	void swap(Stack& rhs) { _vector.swap(rhs); }

// Attributes
	rhuint size() const { return _vector.size(); }

	bool empty() const { return _vector.empty(); }

	T& top() { return _vector.back(); }
	const T& top() const { return _vector.back(); }

	T& back() const { return _vector.back(); }

protected:
	Vector<T> _vector;
};	// Stack

#endif	// __STACK_H__
