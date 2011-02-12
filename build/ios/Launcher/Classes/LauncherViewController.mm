//
//  LauncherViewController.m
//  Launcher
//
//  Created by Apple on 2/3/11.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#import <QuartzCore/QuartzCore.h>

#import "LauncherViewController.h"
#import "EAGLView.h"

#import "../../../../src/rhinoca.h"

// Uniform index.
enum {
	UNIFORM_TRANSLATE,
	NUM_UNIFORMS
};
GLint uniforms[NUM_UNIFORMS];

// Attribute index.
enum {
	ATTRIB_VERTEX,
	ATTRIB_COLOR,
	NUM_ATTRIBUTES
};

struct RhinocaRenderContext
{
	void* platformSpecificContext;
	unsigned fbo;
	unsigned depth;
	unsigned texture;
};

static unsigned fboWidth, fboHeight;

static RhinocaRenderContext renderContext = { 0, 0, 0 };

static Rhinoca* rh = NULL;

static void printOglExtensions()
{
	char* extensions = strdup((const char*)glGetString(GL_EXTENSIONS));
	char** extension = &extensions;
	while(*extension)
		printf("%s\n", strsep(extension, " "));
}

static unsigned nextPOT(unsigned x)
{
	x = x - 1;
	x = x | (x >> 1);
	x = x | (x >> 2);
	x = x | (x >> 4);
	x = x | (x >> 8);
	x = x | (x >>16);
	return x + 1;
}

static void setupFbo(unsigned width, unsigned height)
{
	printOglExtensions();

	const char* extensions = (char*)glGetString(GL_EXTENSIONS);
	bool npot = strstr(extensions, "GL_APPLE_texture_2D_limited_npot") != 0;

	fboWidth = width;
	fboHeight = height;

	if(!npot) {
		fboWidth = nextPOT(fboWidth);
		fboHeight = nextPOT(fboHeight);
	}

	assert(GL_NO_ERROR == glGetError());

	// Generate texture
	// I've got white texture without the GL_CLAMP_TO_EDGE option,
	// see https://devforums.apple.com/message/377748#377748
	if(!renderContext.texture) glGenTextures(1, &renderContext.texture);
	glBindTexture(GL_TEXTURE_2D, renderContext.texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	if(!npot) {
		int area[] = { 0, 0, width, height };
		glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_CROP_RECT_OES, area);
	}

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, fboWidth, fboHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glBindTexture(GL_TEXTURE_2D, 0);
	assert(GL_NO_ERROR == glGetError());

	// Generate frame buffer for depth
//	if(!renderContext.depth) glGenRenderbuffers(1, &renderContext.depth);
//	glBindRenderbuffer(GL_RENDERBUFFER, renderContext.depth);
//	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
//	assert(GL_NO_ERROR == glGetError());

	// Create render target for Rhinoca to draw to
	if(!renderContext.fbo) glGenFramebuffers(1, &renderContext.fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, renderContext.fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderContext.texture, 0);
//	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, renderContext.depth);
	assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
	assert(GL_NO_ERROR == glGetError());
}

static void* ioOpen(Rhinoca* rh, const char* uri, int threadId)
{
	NSString* fullPath = [[NSBundle mainBundle] pathForResource:[NSString stringWithUTF8String:uri] ofType:nil];
	if(!fullPath) return NULL;

	FILE* f = fopen([fullPath UTF8String], "rb");
	return f;
}

static rhuint64 ioRead(void* file, void* buffer, rhuint64 size, int threadId)
{
	FILE* f = reinterpret_cast<FILE*>(file);
	return (rhuint64)fread(buffer, 1, (size_t)size, f);
}

static void ioClose(void* file, int threadId)
{
	FILE* f = reinterpret_cast<FILE*>(file);
	fclose(f);
}

@interface LauncherViewController ()
@property (nonatomic, retain) EAGLContext *context;
@property (nonatomic, assign) CADisplayLink *displayLink;
- (BOOL)loadShaders;
- (BOOL)compileShader:(GLuint *)shader type:(GLenum)type file:(NSString *)file;
- (BOOL)linkProgram:(GLuint)prog;
- (BOOL)validateProgram:(GLuint)prog;
@end

@implementation LauncherViewController

@synthesize animating, context, displayLink;

- (void)awakeFromNib
{
	EAGLContext *aContext = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES1];

	if (!aContext)
	{
		aContext = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES1];
	}

	if (!aContext)
		NSLog(@"Failed to create ES context");
	else if (![EAGLContext setCurrentContext:aContext])
		NSLog(@"Failed to set ES context current");

	self.context = aContext;
	[aContext release];

	[(EAGLView *)self.view setContext:context];
	[(EAGLView *)self.view setFramebuffer];

	if ([context API] == kEAGLRenderingAPIOpenGLES2)
		[self loadShaders];

	animating = FALSE;
	animationFrameInterval = 1;
	self.displayLink = nil;

	unsigned width = self.view.bounds.size.width;
	unsigned height = self.view.bounds.size.height;

	// Fbo for Rhinoca to render
	renderContext.platformSpecificContext = aContext;
	setupFbo(width, height);

	// Init Rhinoca
	rhinoca_init();

	rh = rhinoca_create(&renderContext);
	rhinoca_setSize(rh, width, height);
	rhinoca_io_setcallback(ioOpen, ioRead, ioClose);

	rhinoca_openDocument(rh, "test.html");
	assert(GL_NO_ERROR == glGetError());
}

- (void)dealloc
{
	rhinoca_closeDocument(rh);
	rhinoca_destroy(rh);
	rhinoca_close();

	if (program)
	{
		glDeleteProgram(program);
		program = 0;
	}

	// Tear down context.
	if ([EAGLContext currentContext] == context)
		[EAGLContext setCurrentContext:nil];

	[context release];

	[super dealloc];
}

- (void)viewWillAppear:(BOOL)animated
{
	[self startAnimation];

	[super viewWillAppear:animated];
}

- (void)viewWillDisappear:(BOOL)animated
{
	[self stopAnimation];

	[super viewWillDisappear:animated];
}

- (void)viewDidUnload
{
	[super viewDidUnload];

	if (program)
	{
		glDeleteProgram(program);
		program = 0;
	}

	// Tear down context.
	if ([EAGLContext currentContext] == context)
		[EAGLContext setCurrentContext:nil];
	self.context = nil;
}

- (NSInteger)animationFrameInterval
{
	return animationFrameInterval;
}

- (void)setAnimationFrameInterval:(NSInteger)frameInterval
{
	/*
	 Frame interval defines how many display frames must pass between each time the display link fires.
	 The display link will only fire 30 times a second when the frame internal is two on a display that refreshes 60 times a second. The default frame interval setting of one will fire 60 times a second when the display refreshes at 60 times a second. A frame interval setting of less than one results in undefined behavior.
	 */
	if (frameInterval >= 1)
	{
		animationFrameInterval = frameInterval;

		if (animating)
		{
			[self stopAnimation];
			[self startAnimation];
		}
	}
}

- (void)startAnimation
{
	if (!animating)
	{
		CADisplayLink *aDisplayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(drawFrame)];
		[aDisplayLink setFrameInterval:animationFrameInterval];
		[aDisplayLink addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
		self.displayLink = aDisplayLink;

		animating = TRUE;
	}
}

- (void)stopAnimation
{
	if (animating)
	{
		[self.displayLink invalidate];
		self.displayLink = nil;
		animating = FALSE;
	}
}

- (void)drawFrame
{
	[(EAGLView *)self.view setFramebuffer];

	rhinoca_update(rh);
	assert(GL_NO_ERROR == glGetError());

	unsigned width = self.view.bounds.size.width;
	unsigned height = self.view.bounds.size.height;

	glBindFramebuffer(GL_FRAMEBUFFER, ((EAGLView*)self.view)->defaultFramebuffer);

	glViewport(0, 0, width, height);
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, renderContext.texture);
	assert(GL_NO_ERROR == glGetError());

	static const GLubyte color[] = {
		255, 255, 255, 255,
		255, 255, 255, 255,
		255, 255, 255, 255,
		255, 255, 255, 255
	};

	float uvx = float(width)/fboWidth;
	float uvy = float(height)/fboHeight;
	static const GLfloat uv[] = {
		0, 0,
		uvx, 0,
		uvx, uvy,
		0, uvy,
	};

	static const GLfloat vertices[] = {
		-1,  1,
		 1,  1,
		 1, -1,
		-1, -1
	};

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glEnableClientState(GL_COLOR_ARRAY);
	glColorPointer(4, GL_UNSIGNED_BYTE, 0, color);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, 0, uv);
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(2, GL_FLOAT, 0, vertices);

	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	assert(GL_NO_ERROR == glGetError());

//	glDrawTexfOES(0, 0, 0, width, height);

/*	if ([context API] == kEAGLRenderingAPIOpenGLES2)
	{
		// Use shader program.
		glUseProgram(program);

		// Update uniform value.
		glUniform1f(uniforms[UNIFORM_TRANSLATE], (GLfloat)transY);
		transY += 0.075f;

		// Update attribute values.
		glVertexAttribPointer(ATTRIB_VERTEX, 2, GL_FLOAT, 0, 0, squareVertices);
		glEnableVertexAttribArray(ATTRIB_VERTEX);
		glVertexAttribPointer(ATTRIB_COLOR, 4, GL_UNSIGNED_BYTE, 1, 0, squareColors);
		glEnableVertexAttribArray(ATTRIB_COLOR);

		// Validate program before drawing. This is a good check, but only really necessary in a debug build.
		// DEBUG macro must be defined in your debug configurations if that's not already the case.
#if defined(DEBUG)
		if (![self validateProgram:program])
		{
			NSLog(@"Failed to validate program: %d", program);
			return;
		}
#endif
	}
	else
	{
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glTranslatef(0.0f, (GLfloat)(sinf(transY)/2.0f), 0.0f);
		transY += 0.075f;

		glVertexPointer(2, GL_FLOAT, 0, squareVertices);
		glEnableClientState(GL_VERTEX_ARRAY);
		glColorPointer(4, GL_UNSIGNED_BYTE, 0, squareColors);
		glEnableClientState(GL_COLOR_ARRAY);
	}

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
*/

	[(EAGLView *)self.view presentFramebuffer];
}

- (void)didReceiveMemoryWarning
{
	// Releases the view if it doesn't have a superview.
	[super didReceiveMemoryWarning];

	// Release any cached data, images, etc. that aren't in use.
}

- (BOOL)compileShader:(GLuint *)shader type:(GLenum)type file:(NSString *)file
{
	GLint status;
	const GLchar *source;

	source = (GLchar *)[[NSString stringWithContentsOfFile:file encoding:NSUTF8StringEncoding error:nil] UTF8String];
	if (!source)
	{
		NSLog(@"Failed to load vertex shader");
		return FALSE;
	}

	*shader = glCreateShader(type);
	glShaderSource(*shader, 1, &source, NULL);
	glCompileShader(*shader);

#if defined(DEBUG)
	GLint logLength;
	glGetShaderiv(*shader, GL_INFO_LOG_LENGTH, &logLength);
	if (logLength > 0)
	{
		GLchar *log = (GLchar *)malloc(logLength);
		glGetShaderInfoLog(*shader, logLength, &logLength, log);
		NSLog(@"Shader compile log:\n%s", log);
		free(log);
	}
#endif

	glGetShaderiv(*shader, GL_COMPILE_STATUS, &status);
	if (status == 0)
	{
		glDeleteShader(*shader);
		return FALSE;
	}

	return TRUE;
}

- (BOOL)linkProgram:(GLuint)prog
{
	GLint status;

	glLinkProgram(prog);

#if defined(DEBUG)
	GLint logLength;
	glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &logLength);
	if (logLength > 0)
	{
		GLchar *log = (GLchar *)malloc(logLength);
		glGetProgramInfoLog(prog, logLength, &logLength, log);
		NSLog(@"Program link log:\n%s", log);
		free(log);
	}
#endif

	glGetProgramiv(prog, GL_LINK_STATUS, &status);
	if (status == 0)
		return FALSE;

	return TRUE;
}

- (BOOL)validateProgram:(GLuint)prog
{
	GLint logLength, status;

	glValidateProgram(prog);
	glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &logLength);
	if (logLength > 0)
	{
		GLchar *log = (GLchar *)malloc(logLength);
		glGetProgramInfoLog(prog, logLength, &logLength, log);
		NSLog(@"Program validate log:\n%s", log);
		free(log);
	}

	glGetProgramiv(prog, GL_VALIDATE_STATUS, &status);
	if (status == 0)
		return FALSE;

	return TRUE;
}

- (BOOL)loadShaders
{
	GLuint vertShader, fragShader;
	NSString *vertShaderPathname, *fragShaderPathname;

	// Create shader program.
	program = glCreateProgram();

	// Create and compile vertex shader.
	vertShaderPathname = [[NSBundle mainBundle] pathForResource:@"Shader" ofType:@"vsh"];
	if (![self compileShader:&vertShader type:GL_VERTEX_SHADER file:vertShaderPathname])
	{
		NSLog(@"Failed to compile vertex shader");
		return FALSE;
	}

	// Create and compile fragment shader.
	fragShaderPathname = [[NSBundle mainBundle] pathForResource:@"Shader" ofType:@"fsh"];
	if (![self compileShader:&fragShader type:GL_FRAGMENT_SHADER file:fragShaderPathname])
	{
		NSLog(@"Failed to compile fragment shader");
		return FALSE;
	}

	// Attach vertex shader to program.
	glAttachShader(program, vertShader);

	// Attach fragment shader to program.
	glAttachShader(program, fragShader);

	// Bind attribute locations.
	// This needs to be done prior to linking.
	glBindAttribLocation(program, ATTRIB_VERTEX, "position");
	glBindAttribLocation(program, ATTRIB_COLOR, "color");

	// Link program.
	if (![self linkProgram:program])
	{
		NSLog(@"Failed to link program: %d", program);

		if (vertShader)
		{
			glDeleteShader(vertShader);
			vertShader = 0;
		}
		if (fragShader)
		{
			glDeleteShader(fragShader);
			fragShader = 0;
		}
		if (program)
		{
			glDeleteProgram(program);
			program = 0;
		}

		return FALSE;
	}

	// Get uniform locations.
	uniforms[UNIFORM_TRANSLATE] = glGetUniformLocation(program, "translate");

	// Release vertex and fragment shaders.
	if (vertShader)
		glDeleteShader(vertShader);
	if (fragShader)
		glDeleteShader(fragShader);

	return TRUE;
}

@end
