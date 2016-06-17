#pragma once

#ifdef MACOSX

#include "tipcmsg.h"

//------------------------------------------------------------------

//  Forward declarations
namespace tipc {
class Server;
}

//------------------------------------------------------------------

using namespace tipc;

namespace font_io {

void addParsers(tipc::Server *srv);

//************************************************************************************
//    Initialization messages
//************************************************************************************

class LoadFontNamesParser : public tipc::MessageParser {
  // Syntax : $FNTloadFontNames
  // Reply: ok | err

public:
  QString header() const { return "$FNTloadFontNames"; }
  void operator()(Message &stream);
};

//------------------------------------------------------------------------------

class GetAllFamiliesParser : public tipc::MessageParser {
  // Syntax : $FNTgetAllFamilies
  // Reply: ok <families vector> | err

public:
  QString header() const { return "$FNTgetAllFamilies"; }
  void operator()(Message &stream);
};

//------------------------------------------------------------------------------

class GetAllTypefacesParser : public tipc::MessageParser {
  // Syntax : $FNTgetAllTypefaces
  // Reply: ok <typefaces vector> | err

public:
  QString header() const { return "$FNTgetAllTypefaces"; }
  void operator()(Message &stream);
};

//************************************************************************************
//    Setter messages
//************************************************************************************

class SetFamilyParser : public tipc::MessageParser {
  // Syntax: $FNTsetFamily <family>
  // Reply: ok | err

public:
  QString header() const { return "$FNTsetFamily"; }
  void operator()(Message &stream);
};

//------------------------------------------------------------------------------

class SetTypefaceParser : public tipc::MessageParser {
  // Syntax: $FNTsetTypeface <typeface>
  // Reply: ok <ascender> <descender> | err

public:
  QString header() const { return "$FNTsetTypeface"; }
  void operator()(Message &stream);
};

//------------------------------------------------------------------------------

class SetSizeParser : public tipc::MessageParser {
  // Syntax: $FNTsetSize <size>
  // Reply: ok <ascender> <descender> | err

public:
  QString header() const { return "$FNTsetSize"; }
  void operator()(Message &stream);
};

//************************************************************************************
//    Getter messages
//************************************************************************************

class GetCurrentFamilyParser : public tipc::MessageParser {
  // Syntax: $FNTgetCurrentFamily
  // Reply: ok <family name> | err

public:
  QString header() const { return "$FNTgetCurrentFamily"; }
  void operator()(Message &stream);
};

//------------------------------------------------------------------------------

class GetCurrentTypefaceParser : public tipc::MessageParser {
  // Syntax: $FNTgetCurrentTypeface
  // Reply: ok <typeface name> | err

public:
  QString header() const { return "$FNTgetCurrentTypeface"; }
  void operator()(Message &stream);
};

//------------------------------------------------------------------------------

class GetDistanceParser : public tipc::MessageParser {
  // Syntax: $FNTgetDistance <firstChar> <secondChar>
  // Reply: ok <x> <y> | err

public:
  QString header() const { return "$FNTgetDistance"; }
  void operator()(Message &stream);
};

//************************************************************************************
//    Draw messages
//************************************************************************************

class DrawCharVIParser : public tipc::MessageParser {
  // Syntax: $FNTdrawCharVI <char> <nextChar>
  // Reply: ok <x> <y> <strokes vector> | err

public:
  QString header() const { return "$FNTdrawCharVI"; }
  void operator()(Message &stream);
};

//------------------------------------------------------------------------------

class DrawCharGRParser : public tipc::MessageParser {
  // Syntax: $FNTdrawCharGR <shmem-id> <char> <nextChar>
  // Reply: ok <lx> <ly> <x> <y> | err

public:
  QString header() const { return "$FNTdrawCharGR"; }
  void operator()(Message &stream);
};

//------------------------------------------------------------------------------

class DrawCharCMParser : public tipc::MessageParser {
  // Syntax: $FNTdrawCharCM <ink> <shmem-id> <char> <nextChar>
  // Reply: ok <lx> <ly> <x> <y> | err

public:
  QString header() const { return "$FNTdrawCharCM"; }
  void operator()(Message &stream);
};

}  // namespace tlevelwriter_mov

#endif
