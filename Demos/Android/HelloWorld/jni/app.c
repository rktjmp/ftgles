/* San Angeles Observation OpenGL ES version example
 * Copyright 2009 The Android Open Source Project
 * All rights reserved.
 *
 * This source is free software; you can redistribute it and/or
 * modify it under the terms of EITHER:
 *   (1) The GNU Lesser General Public License as published by the Free
 *       Software Foundation; either version 2.1 of the License, or (at
 *       your option) any later version. The text of the GNU Lesser
 *       General Public License is included with this source in the
 *       file LICENSE-LGPL.txt.
 *   (2) The BSD-style license that is included with this source in
 *       the file LICENSE-BSD.txt.
 *
 * This source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the files
 * LICENSE-LGPL.txt and LICENSE-BSD.txt for more details.
 */

#include "app.h"
#include <jni.h>
#include <sys/time.h>
#include <time.h>
#include <android/log.h>
#include <stdint.h>
#include "FTGL/ftgles.h"
#include <GLES/gl.h>

	int frames;
	long CurrentTime;
	long LastFPSUpdate;
//	FTFont *font;


int   gAppAlive   = 1;

static int  sWindowWidth  = 320;
static int  sWindowHeight = 480;
static int  sDemoStopped  = 0;
static long sTimeOffset   = 0;
static int  sTimeOffsetInit = 0;
static long sTimeStopped  = 0;
static long sStartTick = 0;
static long sTick = 0;
static long
_getTime(void)
{
    struct timeval  now;

    gettimeofday(&now, NULL);
    return (long)(now.tv_sec*1000 + now.tv_usec/1000);
}

/* Call to initialize the graphics state */
 void
Java_com_helloftgles_DemoRenderer_nativeInit( JNIEnv*  env )
{
    appInit();
    gAppAlive    = 1;
    sDemoStopped = 0;
    sTimeOffsetInit = 0;
}

 void
Java_com_helloftgles_DemoRenderer_nativeResize( JNIEnv*  env, jobject  thiz, jint w, jint h )
{
    sWindowWidth  = w;
    sWindowHeight = h;
    __android_log_print(ANDROID_LOG_INFO, "SanAngeles", "resize w=%d h=%d", w, h);
}

/* Call to finalize the graphics state */
 void
Java_com_helloftgles_DemoRenderer_nativeDone( JNIEnv*  env )
{
    appDeinit();
}

/* This is called to indicate to the render loop that it should
 * stop as soon as possible.
 */
 void
Java_com_helloftgles_DemoGLSurfaceView_nativePause( JNIEnv*  env )
{
    sDemoStopped = !sDemoStopped;
    if (sDemoStopped) {
        /* we paused the animation, so store the current
         * time in sTimeStopped for future nativeRender calls */
        sTimeStopped = _getTime();
    } else {
        /* we resumed the animation, so adjust the time offset
         * to take care of the pause interval. */
        sTimeOffset -= _getTime() - sTimeStopped;
    }
}

/* Call to render the next GL frame */
 void
Java_com_helloftgles_DemoRenderer_nativeRender( JNIEnv*  env )
{
    long   curTime;

    /* NOTE: if sDemoStopped is TRUE, then we re-render the same frame
     *       on each iteration.
     */
    if (sDemoStopped) {
        curTime = sTimeStopped + sTimeOffset;
    } else {
        curTime = _getTime() + sTimeOffset;
        if (sTimeOffsetInit == 0) {
            sTimeOffsetInit = 1;
            sTimeOffset     = -curTime;
            curTime         = 0;
        }
    }

    //__android_log_print(ANDROID_LOG_INFO, "SanAngeles", "curTime=%ld", curTime);

    appRender(curTime, sWindowWidth, sWindowHeight);
}



// Called from the app framework.
void appInit()
{
  /*
	NSString *fontpath = [NSString stringWithFormat:@"%@/Diavlo_BLACK_II_37.otf", 
						  [[NSBundle mainBundle] resourcePath]];

	font = new FTTextureFont([fontpath UTF8String]);
	
if (font->Error())
	{
	  fprintf(stderr, "Could not load font `%s'\n", fontpath);	
		delete font;
		font = NULL;
		return;
	}
	font->FaceSize(screenSize.width * 0.16f);
  */
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
}


// Called from the app framework.
void appDeinit()
{
  // delete font;
}
void appRender(long tick, int width, int height)
{
    if (sStartTick == 0)
        sStartTick = tick;
    if (!gAppAlive)
        return;

    // Actual tick value is "blurred" a little bit.
    sTick = (sTick + tick - sStartTick) >> 1;

    // Terminate application after running through the demonstration once.
    // if (sTick >= RUN_LENGTH)
    //{
    //  gAppAlive = 0;
    //  return;
    //}

	glClear(GL_COLOR_BUFFER_BIT);
	glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrthof(0.0f,  WINDOW_DEFAULT_WIDTH, 
	     0.0f, WINDOW_DEFAULT_HEIGHT, 
	     -10000.0f, 10000.0f);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	
	glTranslatef(0.0f, WINDOW_DEFAULT_HEIGHT * 0.5f, 0.0f);
	glColor4f(1.0f, 0.6f, 0.3f, 1.0f);
	//	if (font)
	//	font->Render("Hello world!");

}