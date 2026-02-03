#pragma once

#ifndef XSCOPEDLOCK_H
#define XSCOPEDLOCK_H

class XScopedLock {
private:
  class Imp;
  Imp *m_imp;

public:
  XScopedLock();
  ~XScopedLock();
};

#endif
