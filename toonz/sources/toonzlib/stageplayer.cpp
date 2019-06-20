

// TnzLib includes
#include "toonz/stage.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshchildlevel.h"
#include "toonz/txshleveltypes.h"
#include "toonz/txsheet.h"
#include "toonz/txshcell.h"
#include "toonz/textureutils.h"
#include "toonz/imagemanager.h"
#include "imagebuilders.h"

#include "toonz/stageplayer.h"

using namespace Stage;

//*****************************************************************************************
//    Stage::Player  implementation
//*****************************************************************************************

double Player::m_onionSkinFrontSize   = 0;
double Player::m_onionSkinBackSize    = 0;
double Player::m_firstBackOnionSkin   = 0;
double Player::m_lastBackVisibleSkin  = 0;
bool Player::m_isShiftAndTraceEnabled = false;

//-----------------------------------------------------------------------------

Stage::Player::Player()
    : m_placement()
    , m_dpiAff()
    , m_z(0)
    , m_onionSkinDistance(c_noOnionSkin)
    , m_ancestorColumnIndex(-1)
    , m_isCurrentColumn(false)
    , m_isCurrentXsheetLevel(false)
    , m_isEditingLevel(false)
    , m_isVisibleinOSM(false)
    , m_sl()
    , m_xsh()
    , m_column(-1)
    , m_frame(0)
    , m_isPlaying(false)
    , m_opacity(255)
    , m_bingoOrder(0) {}

//-----------------------------------------------------------------------------

TImageP Stage::Player::image() const {
  if (!m_sl) return TImageP();

  std::string id = m_sl->getImageId(m_fid);
  int slType     = m_sl->getType();

  if (slType == PLI_XSHLEVEL && TXshSimpleLevel::m_rasterizePli) {
    if (!(m_isCurrentColumn && m_isCurrentXsheetLevel)) id = id + "_rasterized";
  }

  if (TXshSimpleLevel::m_fillFullColorRaster &&
      (slType == OVL_XSHLEVEL || slType == TZI_XSHLEVEL))
    id = id + "_filled";

  ImageLoader::BuildExtData extData(m_sl, m_fid);
  return ImageManager::instance()->getImage(id, ImageManager::none, &extData);
}

//-----------------------------------------------------------------------------

DrawableTextureDataP Stage::Player::texture() const {
  if (m_sl) {
    // Ask the sLevel directly
    return texture_utils::getTextureData(
        m_sl, m_fid, -1);  // -1 stands for 'current subsampling'
  }

  // The level is supposedly a sub-xsheet one. It means we have to build the
  // texture
  // by rendering the sub-xsheet.
  const TXshCell &cell = m_xsh->getCell(m_frame, m_column);

  TXshChildLevel *cl = cell.getChildLevel();
  if (!cl) return DrawableTextureDataP();  // Should never happen, though

  // Fetch the xsheet data
  TXsheet *xsh = cl->getXsheet();
  int frame =
      cell.getFrameId().getNumber() - 1;  // frame 1 internally stands for 0

  return texture_utils::getTextureData(xsh, frame);
}
