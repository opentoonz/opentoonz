

/*max@home*/
#ifdef __cplusplus
extern "C" {
#endif

#include <Cocoa/Cocoa.h>
#include "ttwain_state.h"
#include "ttwainP.h"
//#define DEBUG
#ifndef _WIN32
#define PRINTF(args...)
#else
#define PRINTF
#endif
#if 1

extern int TTWAIN_MessageHook(void *lpmsg);

OSErr CPSSetProcessName(ProcessSerialNumber *psn, char *processname);
OSErr CPSEnableForegroundOperation(ProcessSerialNumber *psn, UInt32 _arg2,
                                   UInt32 _arg3, UInt32 _arg4, UInt32 _arg5);

int ScanDone = 0;

void setupUI(void);

/*---------------------------------------------------------------------------*/

void *TTWAIN_GetValidHwndPD(void *hwnd) {
  setupUI();
  return 0;
}

/*---------------------------------------------------------------------------*/

int TTWAIN_EnableWindowPD(void *hwnd, int flag) {
  printf("%s\n", __PRETTY_FUNCTION__);
  return 1;
}

/*---------------------------------------------------------------------------*/

static int CallbackRegistered = false;

void unregisterTwainCallback(void) {
  printf("unregistering\n");
  CallbackRegistered = 0;
}

/*---------------------------------------------------------------------------*/

TW_UINT16 twainCallback(pTW_IDENTITY pOrigin, pTW_IDENTITY pDest, TW_UINT32 DG,
                        TW_UINT16 DAT, TW_UINT16 MSG, TW_MEMREF pData) {
  PRINTF("%s msg=0x%x\n", __PRETTY_FUNCTION__, MSG);
  TTWAIN_MessageHook((void *)MSG);
  return TWRC_SUCCESS;
}

/*---------------------------------------------------------------------------*/

int exitTwainSession(void) {
/*
EventQueueRef q = GetCurrentEventQueue();
printf("flushing event queue\n");
FlushEventQueue(q);
*/
#ifdef __i386
  /*At this time the HP Scan Pro DS (OSX on i386) need at least 1 sec to process
closeUI msg
If we are too fast exiting from the application loop, the msg stay in the
queue and it will be processed the next time we open the ui !!!
Flusing the queue (see above) doesn't work, because is possible that we are
too fast purging the queue.
1 sec seems to be ok
2 sec is safe :)
*/
  sleep(2);
#endif
  printf("calling QuitApplicationEventLoop\n");
  // QuitApplicationEventLoop();

  unregisterTwainCallback();
  return 0;
}

/*---------------------------------------------------------------------------*/

static void myEventLoopTimer() {
  printf("my event loop timer ScanDone = %d\n", ScanDone);
  // if (ScanDone)
  // QuitApplicationEventLoop ();
}

/*---------------------------------------------------------------------------*/

void setupUI(void) {
  ProcessSerialNumber psn;

  GetCurrentProcess(&psn);

  /* Need to do some magic here to get the UI to work */
  CPSEnableForegroundOperation(&psn, 0x03, 0x3C, 0x2C, 0x1103);

  SetFrontProcess(&psn);
#ifndef HAVE_DOCK_TILE
/* We end up with the ugly console dock icon; let's override it */
/*char *iconfile = "/tmp/image.png";
  CFURLRef url = CFURLCreateFromFileSystemRepresentation (kCFAllocatorDefault,
                                                          (UInt8 *)iconfile,
                                                          strlen (iconfile),
                                                          FALSE);

  CGDataProviderRef png = CGDataProviderCreateWithURL (url);
  CGImageRef icon = CGImageCreateWithPNGDataProvider (png, NULL, TRUE,
                                             kCGRenderingIntentDefault);

  /* Voodoo magic fix inspired by java_swt launcher */
/* Without this the icon setting doesn't work about half the time. */
// CGrafPtr p = BeginQDContextForApplicationDockTile();
// EndQDContextForApplicationDockTile(p);

// SetApplicationDockTileImage (icon);
#else
  int numComponents       = 4;
  int bitsPerPixelChannel = 8;
  int totalBitsPerPixel   = bitsPerPixelChannel * numComponents;
  int w                   = 32;
  int h                   = 32;
  char *buffer[w * h * numComponents];
  CGContextRef context;
  CGDataProviderRef provider;
  CGColorSpaceRef colorSpace;
  CGImageRef image;
  int bytesPerRow = w * numComponents;
  context         = BeginCGContextForApplicationDockTile();
  provider   = CGDataProviderCreateWithData(0, buffer, (bytesPerRow * h), 0);
  colorSpace = CGColorSpaceCreateDeviceRGB();
  image      = CGImageCreate(w, h, bitsPerPixelChannel, totalBitsPerPixel,
                        bytesPerRow, colorSpace, kCGImageAlphaFirst, provider,
                        0, 0, kCGRenderingIntentDefault);
  CGDataProviderRelease(provider);
  CGColorSpaceRelease(colorSpace);
  SetApplicationDockTileImage(image);
  CGContextFlush(context);
  CGImageRelease(image);
  EndCGContextForApplicationDockTile(context);
#endif
}

/*---------------------------------------------------------------------------*/

void registerTwainCallback(void) {
  if (TTWAIN_GetState() < TWAIN_SOURCE_OPEN) {
    PRINTF("%s too early!, don't register\n", __FUNCTION__);
    CallbackRegistered = 0;
    return;
  }

  if (TTWAIN_GetState() != 4) {
    PRINTF("%s state != 4, don't register\n", __FUNCTION__);
    CallbackRegistered = 0;
    return;
  }

  if (!CallbackRegistered) {
    int rc = 0;
    TW_CALLBACK callback;

    PRINTF("%s registering\n", __FUNCTION__);

    /* We need to set up our callback to receive messages */
    callback.CallBackProc = (TW_MEMREF)twainCallback;
    callback.RefCon       = 0; /* user data */
    callback.Message      = 0;
    printf("registering\n");
    /*
processed = TTWAIN_DS(DG_CONTROL, DAT_CALLBACK, MSG_REGISTER_CALLBACK,
(TW_MEMREF) &callback);
*/

    rc = TTwainData.resultCode =
        (*TTwainData.DSM_Entry)(&TTwainData.appId, 0, DG_CONTROL, DAT_CALLBACK,
                                MSG_REGISTER_CALLBACK, (TW_MEMREF)&callback);

    // NSRunLoop* runLoop = [NSRunLoop currentRunLoop];

    // EventLoopTimerRef timer;
    /*OSStatus err;

// Set this up to run once the event loop is started
err = InstallEventLoopTimer (GetMainEventLoop (),
                   0, 0, // Immediately, once only
                   NewEventLoopTimerUPP (myEventLoopTimer),
                   0, &timer);*/
    CallbackRegistered = 1;
  } else {
    PRINTF("%s already registered!, don't register\n", __FUNCTION__);
  }
}

/*---------------------------------------------------------------------------*/

void TTWAIN_EmptyMessageQueuePD(void) {
  ScanDone = 0;
  registerTwainCallback();
}

/*---------------------------------------------------------------------------*/

void TTWAIN_ModalEventLoopPD(void) {
  printf("%s\n", __PRETTY_FUNCTION__);
  registerTwainCallback();
  // RunApplicationEventLoop();
  return;

  TTwainData.breakModalLoop = FALSE;
}

#endif

#ifdef __cplusplus
}
#endif
