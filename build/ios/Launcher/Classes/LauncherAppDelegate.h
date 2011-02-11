//
//  LauncherAppDelegate.h
//  Launcher
//
//  Created by Apple on 2/3/11.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#import <UIKit/UIKit.h>

@class LauncherViewController;

@interface LauncherAppDelegate : NSObject <UIApplicationDelegate> {
	UIWindow *window;
	LauncherViewController *viewController;
}

@property (nonatomic, retain) IBOutlet UIWindow *window;
@property (nonatomic, retain) IBOutlet LauncherViewController *viewController;

@end

