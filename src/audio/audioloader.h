#ifndef __AUDIO_AUDIOLOADER_H__
#define __AUDIO_AUDIOLOADER_H__

#include "../taskpool.h"
#include "../vector.h"

namespace Loader {

/// 
class AudioLoader : public Task
{
public:
	virtual ~AudioLoader() {}

// Operations
	virtual void loadDataForRange(unsigned begin, unsigned end) = 0;

// Attributes
	/// A class to sit between audio device and audio loader.
	/// Audio device may request for data range, and multiple request will merge into single range,
	/// then audio loader will pick up this range and load at the requested begin position, and
	/// try it's best to load till the requested end position.
	struct RequestQueue
	{
		RequestQueue() : _seek(false), _begin(0), _end(0) {}

		// Call by the audio device when it need some data
		void request(unsigned begin, unsigned end)
		{
			ScopeLock lock(mutex);
			ASSERT(begin < end);
			ASSERT(_begin <= _end);
			// Not contiguous, set seek to true
			if((_seek = (begin < _begin || begin > _end)))
				_begin = begin;
			_end = end;
		}

		// Call by the audio loader to know the range it should load
		// Returns whether a seek is needed
		bool getRequest(unsigned& begin, unsigned& end)
		{
			ScopeLock lock(mutex);
			begin = _begin;
			end = _end;
			return _seek;
		}

		// Call by the audio loader after it has loaded some data
		void commit(unsigned begin, unsigned end)
		{
			ScopeLock lock(mutex);
			ASSERT(begin == _begin && "Audio loader is leaving a gap in the audio data!");
			ASSERT(begin < end);
			ASSERT(_begin <= _end);
			_begin = end;
			_end = _end < _begin ? _begin : _end;
		}

		bool _seek;
		unsigned _begin;/// The next loading position that MUST begin with.
		unsigned _end;	/// The next loading position that SUGGEST to end with.
		Mutex mutex;
	};	// RequestQueue

	/// loadDataForRange() may use this queue
	RequestQueue requestQueue;
};	// AudioLoader

}	// namespace Loader

#endif	// __AUDIO_AUDIOLOADER_H__
