#ifndef __roReflectionFwd_h__
#define __roReflectionFwd_h__

#include "roStatus.h"

namespace ro {

roStatus registerReflection();

namespace Reflection {

struct Type;
struct Field;
struct Serializer;
struct Field;

typedef roStatus (*SerializeFunc)(Serializer& se, Field& field, void* fieldParent);

}	// namespace Reflection
}	// namespace ro

#endif	// __roReflectionFwd_h__
