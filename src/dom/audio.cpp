#include "pch.h"
#include "audio.h"
#include "document.h"
#include "../audio/audiodevice.h"
#include "../context.h"
#include "../path.h"
#include "../resource.h"
#include "../xmlparser.h"
#include <string.h>	// for strcasecmp

using namespace ro;
using namespace Audio;

namespace Dom {

JSClass HTMLAudioElement::jsClass = {
	"HTMLAudioElement", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static void triggerLoadEvent(HTMLAudioElement* self, AudioBuffer::State state)
{
	Dom::Event* ev = new Dom::Event;
	ev->type = (state == AudioBuffer::Aborted) ? "error" : "canplaythrough";
	ev->bubbles = false;
	ev->target = self;
	ev->bind(self->jsContext, NULL);
	self->dispatchEvent(ev);
}

static void onLoadCallback(TaskPool* taskPool, void* userData)
{
	HTMLAudioElement* self = reinterpret_cast<HTMLAudioElement*>(userData);
	AudioBuffer* b = audiodevice_getSoundBuffer(self->_device, self->_sound);
	triggerLoadEvent(self, b ? b->state : AudioBuffer::Aborted);
	self->releaseGcRoot();
}

static JSBool getLoop(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	HTMLAudioElement* self = getJsBindable<HTMLAudioElement>(cx, obj);
	*vp = BOOLEAN_TO_JSVAL(self->loop()); return JS_TRUE;
}

static JSBool setLoop(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	HTMLAudioElement* self = getJsBindable<HTMLAudioElement>(cx, obj);
	JSBool ret;

	if(JSVAL_IS_STRING(*vp)) {
		JsString jss(cx, *vp);
		self->setLoop(strToBool(jss.c_str(), false));
		return JS_TRUE;
	}

	if(!JS_ValueToBoolean(cx, *vp, &ret)) return JS_FALSE;
	self->setLoop(ret == JS_TRUE);
	return JS_TRUE;
}

static JSPropertySpec properties[] = {
	{"loop", 0, JsBindable::jsPropFlags, getLoop, setLoop},
	{0}
};

static JSBool construct(JSContext* cx, uintN argc, jsval* vp)
{
	if(!JS_IsConstructing(cx, vp)) return JS_FALSE;	// Not called as constructor? (called without new)

	Rhinoca* rh = reinterpret_cast<Rhinoca*>(JS_GetContextPrivate(cx));

	HTMLAudioElement* audio = new HTMLAudioElement(rh, rh->audioDevice, &rh->resourceManager);
	audio->bind(cx, NULL);

	JS_RVAL(cx, vp) = *audio;
	return JS_TRUE;
}

static JSFunctionSpec methods[] = {
	{0}
};

HTMLAudioElement::HTMLAudioElement(Rhinoca* rh, AudioDevice* device, ResourceManager* mgr)
	: HTMLMediaElement(rh), _sound(NULL), _device(device), _resourceManager(mgr)
{
}

HTMLAudioElement::~HTMLAudioElement()
{
	audioSound_destroy(_sound);
}

void HTMLAudioElement::bind(JSContext* cx, JSObject* parent)
{
	RHASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, HTMLMediaElement::createPrototype(), parent);
	RHVERIFY(JS_SetPrivate(cx, *this, this));
	RHVERIFY(JS_DefineFunctions(cx, *this, methods));
	RHVERIFY(JS_DefineProperties(cx, *this, properties));
	addReference();	// releaseReference() in JsBindable::finalize()
}

void HTMLAudioElement::registerClass(JSContext* cx, JSObject* parent)
{
	RHVERIFY(JS_InitClass(cx, parent, NULL, &jsClass, &construct, 0, NULL, NULL, NULL, NULL));
}

static FixString _tagName = "AUDIO";

Element* HTMLAudioElement::factoryCreate(Rhinoca* rh, const char* type, XmlParser* parser)
{
	HTMLAudioElement* audio = strcasecmp(type, _tagName) == 0 ?
		new HTMLAudioElement(rh, rh->audioDevice, &rh->resourceManager) : NULL;
	if(!audio) return NULL;

	return audio;
}

const FixString& HTMLAudioElement::tagName() const
{
	return _tagName;
}

void HTMLAudioElement::setSrc(const char* uri)
{
	Path path;
	fixRelativePath(uri, rhinoca->documentUrl.c_str(), path);

	if(_sound) audioSound_destroy(_sound);
	_sound = audiodevice_createSound(_device, uri, _resourceManager);
	_src = uri;

	if(!_sound) goto Abort;

	if(AudioBuffer* b = audiodevice_getSoundBuffer(_device, _sound)) {
		// Prevent HTMLImageElement begging GC before the callback finished.
		addGcRoot();
		// Register callbacks
		_resourceManager->taskPool->addCallback(b->taskReady, onLoadCallback, this, TaskPool::threadId());
	}
	else
		goto Abort;

	return;

Abort:
	triggerLoadEvent(this, AudioBuffer::Aborted);
}

const char* HTMLAudioElement::src() const
{
	return _src.c_str();
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

Node* HTMLAudioElement::cloneNode(bool recursive)
{
	HTMLAudioElement* ret = new HTMLAudioElement(rhinoca, _device, _resourceManager);
	ret->bind(jsContext, NULL);
	ret->setSrc(src());
	ret->setLoop(loop());
	return ret;
}

double HTMLAudioElement::currentTime() const
{
	if(_sound)
		return audiodevice_getSoundCurrentTime(_device, _sound);
	else
		return 0;
}

void HTMLAudioElement::setCurrentTime(double time)
{
	if(_sound)
		audiodevice_setSoundCurrentTime(_device, _sound, (float)time);
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
