#ifndef __input_roInputDriver_h__
#define __input_roInputDriver_h__

#include "../platform/roCompiler.h"
#include "../base/roNonCopyable.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef roUint32 roStringHash;

typedef struct roInputDriver : private ro::NonCopyable
{
// Keyboard state query
	bool			(*buttonUp)				(roInputDriver* self, roStringHash buttonName, bool lastFrame);
	bool			(*buttonDown)			(roInputDriver* self, roStringHash buttonName, bool lastFrame);
	bool			(*buttonPressed)		(roInputDriver* self, roStringHash buttonName, bool lastFrame);
	bool			(*anyButtonUp)			(roInputDriver* self, bool lastFrame);
	bool			(*anyButtonDown)		(roInputDriver* self, bool lastFrame);

// Immediate input state query
	float			(*mouseAxis)			(roInputDriver* self, roStringHash axisName);
	float			(*mouseAxisRaw)			(roInputDriver* self, roStringHash axisName);
	float			(*mouseAxisDelta)		(roInputDriver* self, roStringHash axisName);
	float			(*mouseAxisDeltaRaw)	(roInputDriver* self, roStringHash axisName);
	bool			(*mouseButtonUp)		(roInputDriver* self, int buttonId, bool lastFrame);
	bool			(*mouseButtonDown)		(roInputDriver* self, int buttonId, bool lastFrame);
	bool			(*mouseButtonPressed)	(roInputDriver* self, int buttonId, bool lastFrame);

	const roUtf8*	(*inputText)			(roInputDriver* self);

// Others
	void			(*tick)					(roInputDriver* self);
	void			(*processEvents)		(roInputDriver* self, roUserData* data, roSize numData);
} roInputDriver;

roInputDriver*		roNewInputDriver		();
void				roInitInputDriver		(roInputDriver* self, const char* options);
void				roDeleteInputDriver		(roInputDriver* self);

#ifdef __cplusplus
}
#endif

#endif	// __input_roInputDriver_h__
