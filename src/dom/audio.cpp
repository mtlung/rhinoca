#include "pch.h"
#include "audio.h"
#include "document.h"
#include "../context.h"
#include "../path.h"
#include "../xmlparser.h"
#include "../../roar/audio/roAudioDriver.h"

using namespace ro;

namespace Dom {

JSClass HTMLAudioElement::jsClass = {
	"HTMLAudioElement", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

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
		self->setLoop(roStrToBool(jss.c_str(), false));
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

	HTMLAudioElement* audio = new HTMLAudioElement(rh);
	audio->bind(cx, NULL);

	if(argc > 0)
		JS_SetProperty(cx, audio->jsObject, "src", &JS_ARGV0);

	JS_RVAL(cx, vp) = *audio;
	return JS_TRUE;
}

static JSFunctionSpec methods[] = {
	{0}
};

HTMLAudioElement::HTMLAudioElement(Rhinoca* rh)
	: HTMLMediaElement(rh), _sound(NULL)
{
}

HTMLAudioElement::~HTMLAudioElement()
{
	roAssert(roSubSystems && roSubSystems->audioDriver);
	if(roAudioDriver* audioDriver = roSubSystems ? roSubSystems->audioDriver : NULL)
		audioDriver->deleteSoundSource(_sound, true);	// NOTE: We want to audio continue to play even the audio node is garbage collected.
}

void HTMLAudioElement::bind(JSContext* cx, JSObject* parent)
{
	roAssert(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, HTMLMediaElement::createPrototype(), parent);
	roVerify(JS_SetPrivate(cx, *this, this));
	roVerify(JS_DefineFunctions(cx, *this, methods));
	roVerify(JS_DefineProperties(cx, *this, properties));
	addReference();	// releaseReference() in JsBindable::finalize()
}

void HTMLAudioElement::registerClass(JSContext* cx, JSObject* parent)
{
	roVerify(JS_InitClass(cx, parent, NULL, &jsClass, &construct, 0, NULL, NULL, NULL, NULL));
}

static ro::ConstString _tagName = "AUDIO";

Element* HTMLAudioElement::factoryCreate(Rhinoca* rh, const char* type, XmlParser* parser)
{
	HTMLAudioElement* audio = roStrCaseCmp(type, _tagName) == 0 ?
		new HTMLAudioElement(rh) : NULL;
	if(!audio) return NULL;

	return audio;
}

const ro::ConstString& HTMLAudioElement::tagName() const
{
	return _tagName;
}

static void triggerLoadEvent(HTMLAudioElement* self, bool aborted)
{
	Dom::Event* ev = new Dom::Event;
	ev->type = aborted ? "error" : "canplaythrough";
	ev->bubbles = false;
	ev->target = self;
	ev->bind(self->jsContext, NULL);
	self->dispatchEvent(ev);
}

void HTMLAudioElement::setSrc(const char* uri)
{
	Path path;
	fixRelativePath(uri, rhinoca->documentUrl.c_str(), path);

	if(!roSubSystems) goto Abort;
	if(!roSubSystems->taskPool) goto Abort;
	if(!roSubSystems->audioDriver) goto Abort;

	roAudioDriver* audioDriver = roSubSystems->audioDriver;

	if(_sound) audioDriver->deleteSoundSource(_sound, false);
	_sound = audioDriver->newSoundSource(audioDriver, uri, "", true);
	_src = uri;

	if(!_sound) goto Abort;

	rhinoca->audioTickList.pushBack(*this);
	return;

Abort:
	triggerLoadEvent(this, true);
}

const char* HTMLAudioElement::src() const
{
	return _src.c_str();
}

void HTMLAudioElement::play()
{
	if(!roSubSystems || !roSubSystems->audioDriver) return;
	roSubSystems->audioDriver->playSoundSource(_sound);
	roSubSystems->audioDriver->soundSourceSetPause(_sound, false);
}

void HTMLAudioElement::pause()
{
	if(!roSubSystems || !roSubSystems->audioDriver) return;
	roSubSystems->audioDriver->soundSourceSetPause(_sound, true);
}

Node* HTMLAudioElement::cloneNode(bool recursive)
{
	HTMLAudioElement* ret = new HTMLAudioElement(rhinoca);
	ret->bind(jsContext, NULL);
	ret->setSrc(src());
	ret->setLoop(loop());
	return ret;
}

void HTMLAudioElement::tick(float dt)
{
	if(!roSubSystems) return;
	if(!roSubSystems->taskPool) return;
	if(!roSubSystems->audioDriver) return;

	roAudioDriver* audioDriver = roSubSystems->audioDriver;

	if(audioDriver->soundSourceReady(_sound)) {
		bool aborted = audioDriver->soundSourceAborted(_sound);
		Dom::Event* ev = new Dom::Event;
		ev->type = aborted ? "error" : "canplaythrough";
		ev->bubbles = false;
		ev->target = this;
		ev->bind(jsContext, NULL);
		dispatchEvent(ev);

		removeThis();	// No longer need to tick
	}
}

double HTMLAudioElement::currentTime() const
{
	if(!roSubSystems || !roSubSystems->audioDriver) return 0;
	return roSubSystems->audioDriver->soundSourceTellPos(_sound);
}

void HTMLAudioElement::setCurrentTime(double time)
{
	if(!roSubSystems || !roSubSystems->audioDriver) return;
	roSubSystems->audioDriver->soundSourceSeekPos(_sound, (float)time);
}

bool HTMLAudioElement::loop() const
{
	if(!roSubSystems || !roSubSystems->audioDriver) return 0;
	return roSubSystems->audioDriver->soundSourceGetLoop(_sound);
}

void HTMLAudioElement::setLoop(bool loop)
{
	if(!roSubSystems || !roSubSystems->audioDriver) return;
	return roSubSystems->audioDriver->soundSourceSetLoop(_sound, loop);
}

}	// namespace Dom
