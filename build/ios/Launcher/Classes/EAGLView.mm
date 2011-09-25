//
//  EAGLView.m
//  Launcher
//
//  Created by Apple on 2/3/11.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#import <QuartzCore/QuartzCore.h>

#import "EAGLView.h"
#import "../../../../src/rhinoca.h"

@interface EAGLView (PrivateMethods)
- (void)createFramebuffer;
- (void)deleteFramebuffer;
@end

@implementation EAGLView

@dynamic context;

// You must implement this method
+ (Class)layerClass
{
	return [CAEAGLLayer class];
}

//The EAGL view is stored in the nib file. When it's unarchived it's sent -initWithCoder:.
- (id)initWithCoder:(NSCoder*)coder
{
	self = [super initWithCoder:coder];
	if (self)
	{
		CAEAGLLayer *eaglLayer = (CAEAGLLayer *)self.layer;
		
		eaglLayer.opaque = TRUE;
		eaglLayer.drawableProperties = [NSDictionary dictionaryWithObjectsAndKeys:
										[NSNumber numberWithBool:FALSE], kEAGLDrawablePropertyRetainedBacking,
										kEAGLColorFormatRGBA8, kEAGLDrawablePropertyColorFormat,
										nil];

		[self setMultipleTouchEnabled:true];
	}
	
	return self;
}

- (void)dealloc
{
	[self deleteFramebuffer];	
	[context release];
	
	[super dealloc];
}

- (EAGLContext *)context
{
	return context;
}

- (void)setContext:(EAGLContext *)newContext
{
	if (context != newContext)
	{
		[self deleteFramebuffer];
		
		[context release];
		context = [newContext retain];
		
		[EAGLContext setCurrentContext:nil];
	}
}

- (void)createFramebuffer
{
	if (context && !defaultFramebuffer)
	{
		[EAGLContext setCurrentContext:context];
		
		// Create default framebuffer object.
		glGenFramebuffers(1, &defaultFramebuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebuffer);
		
		// Create color render buffer and allocate backing store.
		glGenRenderbuffers(1, &colorRenderbuffer);
		glBindRenderbuffer(GL_RENDERBUFFER, colorRenderbuffer);
		[context renderbufferStorage:GL_RENDERBUFFER fromDrawable:(CAEAGLLayer *)self.layer];
		glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &framebufferWidth);
		glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &framebufferHeight);
		
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorRenderbuffer);
		
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			NSLog(@"Failed to make complete framebuffer object %x", glCheckFramebufferStatus(GL_FRAMEBUFFER));
	}
}

- (void)deleteFramebuffer
{
	if (context)
	{
		[EAGLContext setCurrentContext:context];
		
		if (defaultFramebuffer)
		{
			glDeleteFramebuffers(1, &defaultFramebuffer);
			defaultFramebuffer = 0;
		}
		
		if (colorRenderbuffer)
		{
			glDeleteRenderbuffers(1, &colorRenderbuffer);
			colorRenderbuffer = 0;
		}
	}
}

- (void)setFramebuffer
{
	if (context)
	{
		[EAGLContext setCurrentContext:context];
		
		if (!defaultFramebuffer)
			[self createFramebuffer];
		
		glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebuffer);
		
		glViewport(0, 0, framebufferWidth, framebufferHeight);
	}
}

- (BOOL)presentFramebuffer
{
	BOOL success = FALSE;
	
	if (context)
	{
		[EAGLContext setCurrentContext:context];
		
		glBindRenderbuffer(GL_RENDERBUFFER, colorRenderbuffer);
		
		success = [context presentRenderbuffer:GL_RENDERBUFFER];
	}
	
	return success;
}

- (void)layoutSubviews
{
	// The framebuffer will be re-created at the beginning of the next setFramebuffer method call.
	[self deleteFramebuffer];
}

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event
{
	static const char* msg = "touchstart";
	for(UITouch* touch in touches) {
		CGPoint loc = [touch locationInView:self];
		loc.x *= self.contentScaleFactor;
		loc.y *= self.contentScaleFactor;
		// NOTE: The value of 'touch' pointer will persit until touchesEnded
		// or touchesCancelled, so it can be used as identifier
		RhinocaEvent ev = { (void*)msg, (int)touch, loc.x, loc.y, 0 };
		rhinoca_processEvent(rh, ev);
	}
}

-(void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event
{
	static const char* msg = "touchmove";
	for(UITouch* touch in touches) {
		CGPoint loc = [touch locationInView:self];
		loc.x *= self.contentScaleFactor;
		loc.y *= self.contentScaleFactor;
		RhinocaEvent ev = { (void*)msg, (int)touch, loc.x, loc.y, 0 };
		rhinoca_processEvent(rh, ev);
	}
}

-(void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event
{
	static const char* msg = "touchend";
	for(UITouch* touch in touches) {
		CGPoint loc = [touch locationInView:self];
		loc.x *= self.contentScaleFactor;
		loc.y *= self.contentScaleFactor;
		RhinocaEvent ev = { (void*)msg, (int)touch, loc.x, loc.y, 0 };
		rhinoca_processEvent(rh, ev);
	}
}

-(void)touchesCancelled:(NSSet *)touches withEvent:(UIEvent *)event
{
	static const char* msg = "touchcancel";
	for(UITouch* touch in touches) {
		CGPoint loc = [touch locationInView:self];
		loc.x *= self.contentScaleFactor;
		loc.y *= self.contentScaleFactor;
		RhinocaEvent ev = { (void*)msg, (int)touch, loc.x, loc.y, 0 };
		rhinoca_processEvent(rh, ev);
	}
}

@end
