#include "pch.h"
#include "audio.h"
#include "document.h"
#include "../context.h"
#include "../path.h"
#include "../resource.h"
#include "../xmlparser.h"
#include <string.h>	// for strcasecmp

namespace Dom {

JSClass HTMLAudioElement::jsClass = {
	"HTMLAudioElement", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool getLoop(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	HTMLAudioElement* self = reinterpret_cast<HTMLAudioElement*>(JS_GetPrivate(cx, obj));
	*vp = BOOLEAN_TO_JSVAL(self->loop()); return JS_TRUE;
}

static JSBool setLoop(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	HTMLAudioElement* self = reinterpret_cast<HTMLAudioElement*>(JS_GetPrivate(cx, obj));
	self->setLoop(JSVAL_TO_BOOLEAN(*vp) == JS_TRUE); return JS_TRUE;
}

static JSPropertySpec properties[] = {
	{"loop", 0, 0, getLoop, setLoop},
	{0}
};

static JSBool construct(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	if(!JS_IsConstructing(cx)) return JS_FALSE;	// Not called as constructor? (called without new)

	Rhinoca* rh = reinterpret_cast<Rhinoca*>(JS_GetContextPrivate(cx));

	HTMLAudioElement* audio = new HTMLAudioElement(rh->audioDevice, &rh->resourceManager);
	audio->bind(cx, NULL);

	audio->ownerDocument = rh->domWindow->document;

	*rval = *audio;
	return JS_TRUE;
}

static JSFunctionSpec methods[] = {
	{0}
};

HTMLAudioElement::HTMLAudioElement(AudioDevice* device, ResourceManager* mgr)
	: _sound(NULL), _device(device), _resourceManager(mgr)
{
}

HTMLAudioElement::~HTMLAudioElement()
{
	audioSound_destroy(_sound);
}

void HTMLAudioElement::bind(JSContext* cx, JSObject* parent)
{
	ASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, HTMLMediaElement::createPrototype(), parent);
	VERIFY(JS_SetPrivate(cx, *this, this));
	VERIFY(JS_DefineFunctions(cx, *this, methods));
	VERIFY(JS_DefineProperties(cx, *this, properties));
	addReference();
}

void HTMLAudioElement::registerClass(JSContext* cx, JSObject* parent)
{
	JS_InitClass(cx, parent, NULL, &jsClass, &construct, 0, NULL, NULL, NULL, NULL);
}

Element* HTMLAudioElement::factoryCreate(Rhinoca* rh, const char* type, XmlParser* parser)
{
	HTMLAudioElement* audio = strcasecmp(type, "AUDIO") == 0 ? new HTMLAudioElement(rh->audioDevice, &rh->resourceManager) : NULL;
	if(!audio) return NULL;

	audio->parseMediaElementAttributes(rh, parser);

	// HTMLAudioElement specific attributes
	if(bool loop = parser->attributeValueAsBoolIgnoreCase("loop"))
		audiodevice_setSoundLoop(audio->_device, audio->_sound, loop);

	return audio;
}

static FixString _tagName = "AUDIO";

const FixString& HTMLAudioElement::tagName() const
{
	return _tagName;
}

void HTMLAudioElement::setSrc(const char* uri)
{
	if(_sound)
		audioSound_destroy(_sound);

	_sound = audiodevice_createSound(_device, uri, _resourceManager);
	_src = uri;
}

const char* HTMLAudioElement::src() const
{
	if(!_sound)
		return "";

	return audioSound_getUri(_sound);
}

void HTMLAudioElement::play()
{
	if(_sound)
		audiodevice_playSound(_device, _sound);
}

void HTMLAudioElement::pause()
{
	if(_sound)
		audiodevice_pauseSound(_device, _sound);
}

double HTMLAudioElement::currentTime() const
{
	if(_sound)
		return audiodevice_getSoundCurrentTime(_device, _sound);
	else
		return 0;
}

void HTMLAudioElement::setLoop(bool loop)
{
	if(_sound)
		audiodevice_setSoundLoop(_device, _sound, loop);
}

bool HTMLAudioElement::loop() const
{
	if(_sound)
		return audiodevice_getSoundLoop(_device, _sound);
	else
		return false;
}

}	// namespace Dom
