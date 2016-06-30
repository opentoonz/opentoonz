#pragma once

// SError.h: interface for the SError class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SERROR_H__25953AF0_0B0B_11D6_B96F_0040F674BE6A__INCLUDED_)
#define AFX_SERROR_H__25953AF0_0B0B_11D6_B96F_0040F674BE6A__INCLUDED_

#include <string>
#include <vector>

#include "SDef.h"

class SError {
protected:
  std::string m_msg;

public:
  SError() : m_msg(""){};
  SError(const char *s) : m_msg(s){};
  virtual ~SError(){};
  virtual void debug_print() const {
    /*if ( !m_msg.empty() )
            smsg_error("Error %s!\n",m_msg.c_str());
    else
            smsg_error("\n"); */
  }
};

class SMemAllocError final : public SError {
public:
  SMemAllocError() : SError(""){};
  SMemAllocError(const char *s) : SError(s){};
  virtual ~SMemAllocError(){};
  void debug_print() const override {
    /*	if ( !m_msg.empty() )
            smsg_error("Error in Memory Allocation %s!\n",m_msg.c_str());
    else
            smsg_error("Error in Memory Allocation\n"); */
  }
};

class SWriteRasterError final : public SError {
public:
  SWriteRasterError() : SError(""){};
  SWriteRasterError(const char *s) : SError(s){};
  virtual ~SWriteRasterError(){};
  void debug_print() const override {
    /*if ( !m_msg.empty() )
            smsg_error("Error in Writing Raster %s!\n",m_msg.c_str());
    else
            smsg_error("Error in Writing Raster\n"); */
  }
};

class SBlurMatrixError final : public SError {
public:
  SBlurMatrixError() : SError(""){};
  SBlurMatrixError(const char *s) : SError(s){};
  virtual ~SBlurMatrixError(){};
  void debug_print() const override {
    /*	if ( !m_msg.empty() ) {
            smsg_error("Error in Generating BlurMatrix %s!\n",m_msg.c_str());
    } else
            smsg_error("Error in Generating BlurMatrix!\n"); */
  }
};

class SFileReadError final : public SError {
public:
  SFileReadError() : SError(""){};
  SFileReadError(const char *s) : SError(s){};
  virtual ~SFileReadError(){};
  void debug_print() const override {
    /*if ( !m_msg.empty() ) {
            smsg_error("Error in Reading File %s!\n",m_msg.c_str());
    } else
            smsg_error("Error in Reading File!\n"); */
  }
};

#endif  // !defined(AFX_SERROR_H__25953AF0_0B0B_11D6_B96F_0040F674BE6A__INCLUDED_)
