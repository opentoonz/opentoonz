#include <iostream>
#include "mousedragfilter.h"
#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>


bool IsLeftMouseDown(void *message){
  NSEvent * event = (NSEvent*)message;
  if([event type] == NSEventTypeLeftMouseDown){
    return true;
  }
  return false;
}


bool IsLeftMouseUp(void *message){
  NSEvent * event = (NSEvent*)message;
  if([event type] == NSEventTypeLeftMouseUp){
    return true;
  }
  return false;
}


bool IsLeftMouseDragged(void *message){
  NSEvent * event = (NSEvent*)message;
  if([event type] == NSEventTypeLeftMouseDragged){
    return true;
  }
  return false;
}


void MonitorNSMouseEvent(void *message){
  NSEvent * event = (NSEvent*)message;
  switch ([event type]) {
  case NSEventTypeLeftMouseDown:
    std::cout << "Lv" << std::endl; break;
  case NSEventTypeLeftMouseUp:
    std::cout << "L^" << std::endl; break;
  case NSEventTypeRightMouseDown:
    std::cout << "Rv" << std::endl; break;
  case NSEventTypeRightMouseUp:
    std::cout << "R^" << std::endl; break;
  case NSEventTypeOtherMouseDown:
    std::cout << [event buttonNumber] << "v" << std::endl; break;
  case NSEventTypeOtherMouseUp:
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
