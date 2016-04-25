// Only enable the reflection section if you include roReflection.h first
#ifdef __roReflection_h__
#	ifndef __roLinkList_inl_reflection__
#	define __roLinkList_inl_reflection__

#include "roReflectionFwd.h"
#include "roStringFormat.h"

namespace ro {
namespace Reflection {

template<class T>
roStatus serialize_LinkList(Serializer& se, Field& field, void* fieldParent)
{
	const LinkList<T>* a = field.getConstPtr<LinkList<T> >(fieldParent);
	if(!a) return roStatus::pointer_is_null;

	roStatus st = se.beginArray(field, a->size());
	if(!st) return st;

	if(field.type->templateArgTypes.isEmpty())
		return roStatus::index_out_of_range;

	Type* innerType = field.type->templateArgTypes.front();
	if(!innerType) return roStatus::pointer_is_null;

	Field f = {
		"",
		innerType,
		0,
		field.isConst,
		false
	};

	if(se.isReading) {
		LinkList<T>* l = field.getPtr<LinkList<T> >(fieldParent);
		l->destroyAll();

		while(!se.isArrayEnded()) {
			T* node = new T();	// TODO: Allow customized allocation
			st = innerType->serializeFunc(se, f, node);
			if(!st) {
				delete node;	// TODO: Use auto pointer
				return st;
			}
			l->pushBack(*node);
		}
	}
	else {
		const LinkList<T>* l = field.getConstPtr<LinkList<T> >(fieldParent);
		for(const T* i = l->begin(); i != l->end(); i = i->next()) {
			st = innerType->serializeFunc(se, f, (void*)i);
			if(!st) return st;
		}
	}

	return se.endArray();
}

template<class T> struct DisableResolution<LinkList<T> > {};

template<class T>
Type* RegisterTemplateArgument(Registry& registry, LinkList<T>* dummy=NULL)
{
	Type* type = registry.getType<LinkList<T> >();
	if(type)
		return type;

	Type* innerType = RegisterTemplateArgument(registry, (T*)NULL);
	roAssert(innerType);

	String name;
	strFormat(name, "LinkList<{}>", innerType->name);

	Klass<LinkList<T> > klass = registry.Class<LinkList<T> >(name.c_str());
	klass.type->serializeFunc = serialize_LinkList<T>;
	klass.type->templateArgTypes.pushBack(innerType);

	return klass.type;
}

template<class T, class F, class C>
void extractField(const Klass<C>& klass, Field& outField, const char* name, F f, LinkList<T> C::*m)
{
	Registry* registry = klass.registry;
	roAssert(registry);

	// Register the LinkList type if not yet done so
	Type* type = RegisterTemplateArgument(*registry, (LinkList<T>*)NULL);

	unsigned offset = num_cast<unsigned>((roPtrInt)&((C*)(NULL)->*f));
	Field tmp = {
		name,
		type,
		offset,
		isMemberConst(f),
		isMemberPointerType(f)
	};

	roAssert("Type not found when registering LinkList member field" && !type->templateArgTypes.isEmpty());
	outField = tmp;
}

}	// namespace Reflection
}	// namespace ro

#	endif
#endif	// __roReflection_h__
