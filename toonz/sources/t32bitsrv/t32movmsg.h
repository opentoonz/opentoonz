#pragma once

#ifndef T32MOVMSG_H
#define T32MOVMSG_H

#include "tipcmsg.h"

//------------------------------------------------------------------

//  Forward declarations
namespace tipc {
class Server;
}

//------------------------------------------------------------------

using namespace tipc;

namespace mov_io {

void addParsers(tipc::Server *srv);

//************************************************************************************
//    Generic messages
//************************************************************************************

class IsQTInstalledParser : public tipc::MessageParser {
  // Syntax : $isQTInstalled
  // Reply: yes | no

public:
  QString header() const { return "$isQTInstalled"; }
  void operator()(Message &stream);
};

//------------------------------------------------------------------------------

class DefaultMovPropsParser : public tipc::MessageParser {
  // Syntax : $defaultMovProps <props fp>\n
  // Reply: ok | err

public:
  QString header() const { return "$defaultMovProps"; }
  void operator()(Message &stream);
};

//------------------------------------------------------------------------------

class OpenMovSettingsPopupParser : public tipc::MessageParser {
  // Syntax : $openMovPopup <props fp>\n
  // Reply: ok

public:
  QString header() const { return "$openMovSettingsPopup"; }
  void operator()(Message &stream);
};

//************************************************************************************
//    Write messages
//************************************************************************************

class InitLWMovParser : public tipc::MessageParser {
  // Syntax: $initLWMov <id> <fp> <props fp>
  // Reply: ok | err

public:
  QString header() const { return "$initLWMov"; }
  void operator()(Message &stream);
};

//------------------------------------------------------------------------------

class LWSetFrameRateParser : public tipc::MessageParser {
  // Syntax: $LWMovSetFrameRate <id> <fps>
  // Reply: ok | err

public:
  QString header() const { return "$LWMovSetFrameRate"; }
  void operator()(Message &stream);
};

//------------------------------------------------------------------------------

class LWImageWriteParser : public tipc::MessageParser {
  // Syntax: [$LWMovImageWrite <id> <frameIdx> <lx> <ly>] [data writer]
  // Reply: ok | err

public:
  QString header() const { return "$LWMovImageWrite"; }
  void operator()(Message &stream);
};

//------------------------------------------------------------------------------

class LWSaveSoundTrackParser : public tipc::MessageParser {
  // Syntax: [$LWMovSaveSoundTrack <id> <sampleRate> <bps> <chanCount> <sCount>
  // <signedSample>] [data writer]
  // Reply: ok | err

public:
  QString header() const { return "$LWMovSaveSoundTrack"; }
  void operator()(Message &stream);
};

//------------------------------------------------------------------------------

class CloseLWMovParser : public tipc::MessageParser {
  // Syntax: $closeLWMov <id>
  // Reply: ok | err

public:
  QString header() const { return "$closeLWMov"; }
  void operator()(Message &stream);
};

//************************************************************************************
//    Read messages
//************************************************************************************

class InitLRMovParser : public tipc::MessageParser {
  // Syntax: $initLRMov <id> <fp>
  // Reply: ok <lx> <ly> <framerate> | err

public:
  QString header() const { return "$initLRMov"; }
  void operator()(Message &stream);
};

//------------------------------------------------------------------------------

class LRLoadInfoParser : public tipc::MessageParser {
  // Syntax: $LRMovLoadInfo <id> <shmem id>
  // Reply: ok <frameCount> | err

  // NOTE: Expects an external call to $shmem_release <shmem_id> after data is
  // dealt with.
  //      If the shmem_id is empty, the level infos are not returned.

public:
  QString header() const { return "$LRMovLoadInfo"; }
  void operator()(Message &stream);
};

//------------------------------------------------------------------------------

class LREnableRandomAccessReadParser : public tipc::MessageParser {
  // Syntax: $LRMovEnableRandomAccessRead <id> <"true" | "false">
  // Reply: ok | err

public:
  QString header() const { return "$LRMovEnableRandomAccessRead"; }
  void operator()(Message &stream);
};

//------------------------------------------------------------------------------

#ifdef WIN32  // The following commands are used only from Digital Dailies Lab -
              // Windows only

class LRSetYMirrorParser : public tipc::MessageParser {
  // Syntax: $LRMovSetYMirror <id> <"true" | "false">
  // Reply: ok | err

public:
  QString header() const { return "$LRMovSetYMirror"; }
  void operator()(Message &stream);
};

//------------------------------------------------------------------------------

class LRSetLoadTimecodeParser : public tipc::MessageParser {
  // Syntax: $LRMovSetLoadTimecode <id> <"true" | "false">
  // Reply: ok | err

public:
  QString header() const { return "$LRMovSetLoadTimecode"; }
  void operator()(Message &stream);
};

//------------------------------------------------------------------------------

class LRTimecodeParser : public tipc::MessageParser {
  // Syntax: $LRMovTimecode <id> <frame>
  // Reply: ok <hh> <mm> <ss> <ff> | err

public:
  QString header() const { return "$LRMovTimecode"; }
  void operator()(Message &stream);
};

#endif

//------------------------------------------------------------------------------

class LRImageReadParser : public tipc::MessageParser {
  // Syntax: $LRMovImageRead <id> <lx> <ly> <bypp> <frameIdx> <x> <y> <shrinkX>
  // <shrinkY>
  // Reply: [data reader]

public:
  QString header() const { return "$LRMovImageRead"; }
  void operator()(Message &stream);
};

//------------------------------------------------------------------------------

class LRImageReadSHMParser : public tipc::MessageParser {
  // Syntax: $LRMovImageReadSHM <id> <lx> <ly> <frameIdx> <shmem id>
  // Reply: ok <hh> <mm> <ss> <ff> | err

public:
  QString header() const { return "$LRMovImageReadSHM"; }
  void operator()(Message &stream);
};

//------------------------------------------------------------------------------

class CloseLRMovParser : public tipc::MessageParser {
  // Syntax: $closeLRMov <id>
  // Reply: ok | err

public:
  QString header() const { return "$closeLRMov"; }
  void operator()(Message &stream);
};

}  // namespace tlevelwriter_mov

#endif  // T32MOVMSG_H
