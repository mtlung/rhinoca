#ifndef __SORT_H__
#define __SORT_H__

#include "taskpool.h"

/*!	A parallel merge sort algorithm
	The type T should be bitwise movable

	Example:
	@code
	struct Comparator {
		static bool isCorrectOrder(int a, int b) { return a < b; }
	};
	TaskPool taskPool;
	int data[] = { 3, 6, 1, 2 };
	Sorter<int, Comparator, 1024> sorter(&data[0], 4, &taskPool);
	sorter.sort();
	@endcode
 */
template<class T, class Comparator, int MinItemCountPerTask>
class Sorter
{
public:
	Sorter(T* data, unsigned count, TaskPool* taskPool)
	{
		_data = data;
		_count = count;
		_tempToFree = rhnew<T>(count);
		_temp = (T*)_tempToFree;
		_taskPool = taskPool;
		_taskToWait = 0;
	}

	Sorter(T* data, unsigned count, T* temp, TaskPool* taskPool)
	{
		_data = data;
		_count = count;
		_tempToFree = NULL;
		_temp = temp;
		_taskPool = taskPool;
		_taskToWait = 0;
	}

	~Sorter()
	{
		if(_taskPool)
			_taskPool->wait(_taskToWait);
		rhdelete(_tempToFree);
	}

	void sort();

public:
	T* _data;
	T* _temp;
	void* _tempToFree;
	unsigned _count;
	TaskPool* _taskPool;
	TaskId _taskToWait;
};

template<class T, class Comparator, int MinItemCountPerTask>
class SorterTask : public Task
{
public:
	void swap(T& a, T& b)
	{
		T c = a;
		a = b;
		b = c;
	}

	void merge(int iA, int nA, int iB, int nB);

	void sort(int start, int count);

	void run(TaskPool* taskPool);

	void runTask(TaskPool* taskPool, int start, int count, int splitCount);

	Sorter<T, Comparator, MinItemCountPerTask>* _sorter;

	int _start, _count;
	int _splitCount;
};

template<class T, class Comparator, int MinItemCountPerTask>
void SorterTask<T, Comparator ,MinItemCountPerTask>::merge(int iA, int nA, int iB, int nB)
{
	T* p;
	T* data = _sorter->_data;
	T* temp = _sorter->_temp;
	int iC, nC;
	
	// Find first item not already at its place
	while(nA)
	{
		if(!Comparator::isCorrectOrder(data[iA], data[iB]))
			break;
		
		++iA;
		--nA;
	}
	
	if(!nA) return; // Already sorted
	
	// Merge what needs to be merged in temp
	iC = iA;
	p = &temp[iA];
	
	*p++ = data[iB++];	// First one is from B
	--nB;

	while(nA && nB)
	{
		if(Comparator::isCorrectOrder(data[iA], data[iB])) {
			*p++ = data[iA++];
			--nA;
		}
		else {
			*p++ = data[iB++];
			--nB;
		}
	}
	
	// If anything remains in B, it already is in correct order inside data
	// if anything is left in A, it needs to be moved
	memcpy(p, &data[iA], nA * sizeof(T)); 
	p += nA;
	
	// Copy what has changed position back from temp to data
	nC = int(p - &temp[iC]);
	memcpy(&data[iC], &temp[iC], nC * sizeof(T));
}

template<class T, class Comparator, int MinItemCountPerTask>
void SorterTask<T, Comparator, MinItemCountPerTask>::sort(int start, int count)
{
	int half;

	switch(count)
	{
	case 0:
	case 1:
		ASSERT(false);
		return;
	case 2:
	{	
		T* data = _sorter->_data + start;
		
		if(!Comparator::isCorrectOrder(data[0], data[1]))
			swap(data[0], data[1]); 

	}	break;
	
	case 3:
	{
		T* data = _sorter->_data + start;
		T tmp;

		int k	= (Comparator::isCorrectOrder(data[0], data[1]) ? 1:0)
				+ (Comparator::isCorrectOrder(data[0], data[2]) ? 2:0)
				+ (Comparator::isCorrectOrder(data[1], data[2]) ? 4:0);

		switch(k)
		{
			case 7:	// ABC
				break;
			case 3:	// ACB
				swap(data[1], data[2]);
				break;
			case 6:	// BAC
				swap(data[0], data[1]);
				break;
			case 1:	// BCA
				tmp = data[0]; data[0] = data[2]; data[2] = data[1]; data[1] = tmp;
				break;
			case 4:	// CAB
				tmp = data[0]; data[0] = data[1]; data[1] = data[2]; data[2] = tmp;
				break;
			case 0:	// CBA
				swap(data[0], data[2]);
				break;
			default:
				ASSERT(false);
		}

		if(!Comparator::isCorrectOrder(data[0], data[1]))
		{
			if(!Comparator::isCorrectOrder(data[1], data[2]))
				swap(data[0], data[2]); 
			else
				swap(data[0], data[1]); 
		}

		if(!Comparator::isCorrectOrder(data[1], data[2]))
			swap(data[1], data[2]); 

	}	break;
	
	default:
		half = count / 2;

		sort(start, half);
		sort(start + half, count - half);

		merge(start, half, start + half, count - half);
	}
}

template<class T, class Comparator, int MinItemCountPerTask>
void SorterTask<T, Comparator, MinItemCountPerTask>::run(TaskPool* taskPool)
{
	runTask(taskPool, _start, _count, _splitCount);
}

template<class T, class Comparator, int MinItemCountPerTask>
void SorterTask<T, Comparator, MinItemCountPerTask>::runTask(TaskPool* taskPool, int start, int count, int splitCount)
{
	if(count < MinItemCountPerTask || splitCount > 4) {
		sort(start, count);
		return;
	}

	int half = count / 2;

	// Sort each half
	SorterTask newTask;
	newTask._sorter = _sorter;
	newTask._start = start;
	newTask._count = half;
	newTask._splitCount = splitCount + 1;

	TaskId taskId = taskPool->addFinalized(&newTask);
	
	runTask(taskPool, start + half, count - half, splitCount + 1);

	taskPool->wait(taskId);
	
	// Merge result
	merge(start, half, start+half, count-half);
}

template<class T, class Comparator, int MinItemCountPerTask>
void Sorter<T, Comparator, MinItemCountPerTask>::sort()
{
	SorterTask<T, Comparator, MinItemCountPerTask> sorterTask;
	sorterTask._sorter = this;
	sorterTask._splitCount = 0;

	if(!_taskPool)
		sorterTask.sort(0, _count);
	else
		sorterTask.runTask(_taskPool, 0, _count, 0);
}

#endif	// __SORT_H__
