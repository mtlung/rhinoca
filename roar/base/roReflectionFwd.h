#ifndef __roReflectionFwd_h__
#define __roReflectionFwd_h__

#include "roStatus.h"

namespace ro {

roStatus registerReflection();

namespace Reflection {

struct Type;
struct Field;
struct Registry;
struct Serializer;

template<class T> struct Klass;
template<class T> struct DisableResolution;

typedef roStatus (*SerializeFunc)(Serializer& se, Field& field, void* fieldParent);

}	// namespace Reflection
}	// namespace ro

#endif	// __roReflectionFwd_h__
