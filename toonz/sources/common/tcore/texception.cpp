

#include "texception.h"
#include "tconvert.h"

TException::TException(const std::string &msg) { m_msg = ::to_wstring(msg); }
/*
ostream& operator<<(ostream &out, const TException &e)
{
  return out<<e.getMessage().c_str();
}
*/
