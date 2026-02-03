#pragma once

#ifndef CONTROL_POINT_OBSERVER_INCLUDED
#define CONTROL_POINT_OBSERVER_INCLUDED

class TControlPointObserver {
public:
  virtual void controlPointChanged() = 0;
  virtual ~TControlPointObserver() {}
};

#endif
