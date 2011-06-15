//
//  EAGLView.h
//  Launcher
//
//  Created by Apple on 2/3/11.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#import <UIKit/UIKit.h>

#import <OpenGLES/ES1/gl.h>
#import <OpenGLES/ES1/glext.h>
#import <OpenGLES/ES2/gl.h>
#import <OpenGLES/ES2/glext.h>

#import "../../../../src/rhinoca.h"

// This class wraps the CAEAGLLayer from CoreAnimation into a convenient UIView subclass.
// The view content is basically an EAGL surface you render your OpenGL scene into.
// Note that setting the view non-opaque will only work if the EAGL surface has an alpha channel.
@interface EAGLView : UIView
{
@public
	EAGLContext *context;
	
	// The pixel dimensions of the CAEAGLLayer.
	GLint framebufferWidth;
	GLint framebufferHeight;
	
	// The OpenGL ES names for the framebuffer and renderbuffer used to render to this view.
	GLuint defaultFramebuffer, colorRenderbuffer;
	
	Rhinoca* rh;
}

@property (nonatomic, retain) EAGLContext *context;

- (void)setFramebuffer;
- (BOOL)presentFramebuffer;

@end
