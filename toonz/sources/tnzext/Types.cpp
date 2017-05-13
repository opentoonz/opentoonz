

#ifdef _DEBUG
#define _STLP_DEBUG 1
#endif
#include "ext/Types.h"

#if defined(_MSC_VER) && (_MSC_VER <= 1200)
// to avoid annoying warning
#pragma warning(push)
#pragma warning(disable : 4290)
#endif

//-----------------------------------------------------------------------------

ToonzExt::OddInt::OddInt(int v) : val_(v) {}

//-----------------------------------------------------------------------------

ToonzExt::OddInt::operator int() const
// throw(std::range_error)
{
  if (!isOdd()) throw std::range_error("Value is Even!!!");
  return val_;
}

//-----------------------------------------------------------------------------

ToonzExt::OddInt::operator int()
// throw(std::range_error)
{
  if (!isOdd()) throw std::range_error("Value is Even!!!");
  return val_;
}

//-----------------------------------------------------------------------------

bool ToonzExt::OddInt::isOdd() const { return (bool)(val_ & 1); }

//-----------------------------------------------------------------------------

// -4 -2 0 2 4..

ToonzExt::EvenInt::EvenInt(int v) : val_(v) {}

//-----------------------------------------------------------------------------

ToonzExt::EvenInt::operator int() const
// throw(std::range_error)
{
  if (!isEven()) throw std::range_error("Value is Odd!!!");

  return val_;
}

//-----------------------------------------------------------------------------

ToonzExt::EvenInt::operator int()
// throw(std::range_error)
{
  if (!isEven()) throw std::range_error("Value is Odd!!!");

  return val_;
}

//-----------------------------------------------------------------------------

bool ToonzExt::EvenInt::isEven() const { return (bool)(!(val_ & 1)); }

//-----------------------------------------------------------------------------

#if defined(_MSC_VER) && (_MSC_VER <= 1200)
#pragma warning(pop)
#endif

//-----------------------------------------------------------------------------
//  End Of File
//-----------------------------------------------------------------------------
