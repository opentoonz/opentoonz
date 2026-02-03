#include <iostream>
#include "mousedragfilter.h"
#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>


bool IsLeftMouseDown(void *message){
  NSEvent * event = (NSEvent*)message;
  if([event type] == NSLeftMouseDown){
    return true;
  }
  return false;
}


bool IsLeftMouseUp(void *message){
  NSEvent * event = (NSEvent*)message;
  if([event type] == NSLeftMouseUp){
    return true;
  }
  return false;
}


bool IsLeftMouseDragged(void *message){
  NSEvent * event = (NSEvent*)message;
  if([event type] == NSLeftMouseDragged){
    return true;
  }
  return false;
}


void MonitorNSMouseEvent(void *message){
  NSEvent * event = (NSEvent*)message;
  switch ([event type]) {
  case NSLeftMouseDown:
    std::cout << "Lv" << std::endl; break;
  case NSLeftMouseUp:
    std::cout << "L^" << std::endl; break;
  case NSRightMouseDown:
    std::cout << "Rv" << std::endl; break;
  case NSRightMouseUp:
    std::cout << "R^" << std::endl; break;
  case NSOtherMouseDown:
    std::cout << [event buttonNumber] << "v" << std::endl; break;
  case NSOtherMouseUp:
    std::cout << [event buttonNumber] << "^" << std::endl; break;
  default:
    break;
  }
}


void SendLeftMousePressEvent(){
  CGEventRef event = CGEventCreate(NULL);
  CGPoint location = CGEventGetLocation(event);
  CFRelease(event);
  CGEventRef mouseDown = CGEventCreateMouseEvent(NULL, kCGEventLeftMouseDown, location, kCGMouseButtonLeft);
  CGEventPost(kCGHIDEventTap, mouseDown);
}
