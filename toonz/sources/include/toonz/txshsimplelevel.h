#pragma once

#ifndef TXSHSIMPLELEVEL_INCLUDED
#define TXSHSIMPLELEVEL_INCLUDED

#include <memory>
#include <set>
#include <string>
#include <vector>
#include <map>

// TnzLib includes
#include "toonz/txshlevel.h"
#include "toonz/txshleveltypes.h"
#include "toonz/imagemanager.h"

// TnzCore includes
#include "traster.h"
#include "trasterimage.h"

// Qt includes
#include <QObject>
#include <QStringList>

// boost includes
#include <boost/container/flat_set.hpp>
#include <boost/container/flat_map.hpp>

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//===================================================================

// Forward declarations
class ImageBuilder;
class LevelProperties;
class TPalette;
class TContentHistory;
class TFrameId;
class TFilePath;
class TImageInfo;
class TRasterImageP;

//*************************************************************************************************
// TXshSimpleLevel declaration
//*************************************************************************************************

/*!
  \brief The \p TXshLevel specialization for image levels.

  \todo Substitute m_frames with a sorted vector or a boost flat_set.
*/

class DVAPI TXshSimpleLevel final : public TXshLevel {
  Q_OBJECT

  PERSIST_DECLARATION(TXshSimpleLevel)
  DECLARE_CLASS_CODE

public:
  /*! \details Level frames may have special properties depending on
          the level type they are part of.

  \sa \p TXshSimpleLevel::getFrameStatus() and \p setFrameStatus() for further
  details. */

  //! Describes a level's frame status.
  enum FrameStatusBit {
    Normal         = 0x0,  //!< Frame has no special status.
    Scanned        = 0x1,  //!< A fullcolor frame (only tlv levels).
    Cleanupped     = 0x2,  //!< A cleanupped frame (only tlv levels).
    CleanupPreview = 0x4   //!< A cleanup preview (only fullcolor levels).
  };

  // Static members
  static bool
      m_rasterizePli;  //!< \internal Not the proper place for this data.
  static bool
      m_fillFullColorRaster;  //!< \internal Not the proper place for this data.

public:
  // Construction/Destruction
  explicit TXshSimpleLevel(const std::wstring &name = std::wstring());
  ~TXshSimpleLevel() override;

  // Disable copying and moving
  TXshSimpleLevel(const TXshSimpleLevel &)            = delete;
  TXshSimpleLevel &operator=(const TXshSimpleLevel &) = delete;

  TXshSimpleLevel(TXshSimpleLevel &&)            = delete;
  TXshSimpleLevel &operator=(TXshSimpleLevel &&) = delete;

  // Type casting
  TXshSimpleLevel *getSimpleLevel() override { return this; }

  // Subsequence status
  bool isSubsequence() const { return m_isSubsequence; }

  // Channel level type queries
  bool is16BitChannelLevel() const {
    return getType() == OVL_XSHLEVEL && m_16BitChannelLevel;
  }
  void set16BitChannelLevel(bool value) {
    m_16BitChannelLevel = (value && getType() == OVL_XSHLEVEL);
  }

  bool isFloatChannelLevel() const {
    return getType() == OVL_XSHLEVEL && m_floatChannelLevel;
  }
  void setFloatChannelLevel(bool value) {
    m_floatChannelLevel = (value && getType() == OVL_XSHLEVEL);
  }

  // Read-only status
  bool isReadOnly() const { return m_isReadOnly; }
  void setIsReadOnly(bool value) { m_isReadOnly = value; }
  void updateReadOnly();

  // Properties management
  LevelProperties *getProperties() const { return m_properties.get(); }
  void clonePropertiesFrom(const TXshSimpleLevel *oldSl);

  // Palette management
  TPalette *getPalette() const;
  void setPalette(TPalette *palette);

  // Path management
  TFilePath getPath() const override { return m_path; }
  void setPath(const TFilePath &path, bool retainCachedImages = false);

  TFilePath getScannedPath() const { return m_scannedPath; }
  void setScannedPath(const TFilePath &path);

  // Frame ID management
  std::vector<TFrameId> getFids() const;
  void getFids(std::vector<TFrameId> &fids) const override;

  TFrameId getFirstFid() const;
  TFrameId getLastFid() const;

  bool isEmpty() const override { return m_frames.empty(); }
  bool isFid(const TFrameId &fid) const;

  const TFrameId &getFrameId(int index) const;
  int getFrameCount() const override {
    return static_cast<int>(m_frames.size());
  }

  TFrameId index2fid(int index) const;
  int fid2index(const TFrameId &fid) const;

  /*!
    if the table contains 'fid' it returns fid2index(fid).
    if fid is greater than the last fid in the table, it returns a "guessed"
    index. e.g. if fids = [1,3,5,7] then fid2index(11) == 5 if fid is smaller
    than the last fid in the table (and is not contained in the table) it
    returns the proper insertion index
  */
  int guessIndex(const TFrameId &fid) const;

  //! Analyzes the level's frames table and returns the alleged entries \a step
  //! - ie the distance from each entry to the next.
  int guessStep() const;

  void formatFId(TFrameId &fid, TFrameId tmplFId);

  // Frame image operations
  void setFrame(const TFrameId &fid, const TImageP &img);

  TImageP getFrame(const TFrameId &fid, UCHAR imgManagerParamsMask,
                   int subsampling) const;
  TImageP getFrame(const TFrameId &fid, bool toBeModified) const {
    return getFrame(
        fid, toBeModified ? ImageManager::toBeModified : ImageManager::none, 0);
  }

  TImageP getFullsampledFrame(const TFrameId &fid,
                              UCHAR imgManagerParamsMask) const;
  TImageInfo *getFrameInfo(const TFrameId &fid, bool toBeModified);
  TImageP getFrameIcon(const TFrameId &fid) const;

  // Load icon (and image) data of all frames into cache
  void loadAllIconsAndPutInCache(bool cacheImagesAsWell);

  TRasterImageP getFrameToCleanup(const TFrameId &fid,
                                  bool toBeLineProcessed) const;
  TRasterImageP getFrameRasterized(const TFrameId &fid, TPointD dpi) const;

  std::string getImageId(const TFrameId &fid, int frameStatus = -1) const;
  std::string getIconId(const TFrameId &fid, int frameStatus = -1) const;
  std::string getIconId(const TFrameId &fid, const TDimension &size) const;

  void eraseFrame(const TFrameId &fid);
  void clearFrames();
  void invalidateFrames();
  void invalidateFrame(const TFrameId &fid);

  // Editable range management
  void setEditableRange(unsigned int from, unsigned int to,
                        const std::wstring &userName);
  void mergeTemporaryHookFile(unsigned int from, unsigned int to,
                              const TFilePath &hookFile);
  void clearEditableRange();
  std::wstring getEditableFileName();
  std::set<TFrameId> getEditableRange();

  // Frame status management
  int getFrameStatus(const TFrameId &fid) const;
  void setFrameStatus(const TFrameId &fid, int status);

  /*! \details This function will implicitly convert the input path
              extension to a correctly formatted \a tlv. Behavior
              is undefined in case the level is not of fullcolor type.

    \deprecated Function is obviously an implementation detail of the
                cleanup process. Should be moved there. */
  void makeTlv(const TFilePath &tlvPath);

  TImageP createEmptyFrame();
  void initializePalette();
  void initializeResolutionAndDpi(const TDimension &dim = TDimension(),
                                  double dpi            = 0.0);

  TDimension getResolution();
  TPointD getImageDpi(const TFrameId &fid = TFrameId::NO_FRAME,
                      int frameStatus     = -1);
  int getImageSubsampling(const TFrameId &fid) const;
  TPointD getDpi(const TFrameId &fid = TFrameId::NO_FRAME,
                 int frameStatus     = -1);

  /*! \brief Returns the bbox for the level's fid, specified in standard
          \a inch coordinates (which are \a different from Toonz's
          standard world coordinates, by a \p Stage::inch factor). */
  TRectD getBBox(const TFrameId &fid) const;

  void setDirtyFlag(bool on) override;
  bool getDirtyFlag() const;

  //! Updates content history (invokes setDirtyFlag(true))
  //! \warning Not what users may expect!
  void touchFrame(const TFrameId &fid);

  // Serialization
  void loadData(TIStream &is) override;
  void saveData(TOStream &os) override;

  //! Loads the level from disk, translating encoded level paths relative
  //! to the level's scene path.
  void load() override;
  void load(const std::vector<TFrameId> &fIds);

  //! Saves the level to disk, with the same path deduction from load()
  void save() override;

  /*!
    Save the level in the specified fp.
    The oldFp is used when the current scene path change...
  */
  void save(const TFilePath &fp, const TFilePath &oldFp = TFilePath(),
            bool overwritePalette = true);

  // Content history management
  const TContentHistory *getContentHistory() const {
    return m_contentHistory.get();
  }
  TContentHistory *getContentHistory() { return m_contentHistory.get(); }

  //! destroys the old contentHistory and replaces it with the new one. Gets
  //! ownership
  void setContentHistory(TContentHistory *contentHistory);

  // Frame renumbering
  void setRenumberTable();
  const std::map<TFrameId, TFrameId> &renumberTable() const {
    return m_renumberTable;
  }

  //! Renumbers the level frames to the specified fids (fids and this->fids()
  //! must have the same size).
  void renumber(const std::vector<TFrameId> &fids);

  bool isFrameReadOnly(TFrameId fid);

public:
  // Static methods for auxiliary files management: hooks, tpl, etc.
  // May throw; copy and rename perform touchparentdir

  //! Copy files from src to dst.
  static void copyFiles(const TFilePath &dst, const TFilePath &src);

  //! Rename files from src to dst.
  static void renameFiles(const TFilePath &dst, const TFilePath &src);

  //! Remove files at fp.
  static void removeFiles(const TFilePath &fp);

  //! Get the auxiliary files list: hooks, tpl, etc.
  static void getFiles(const TFilePath &fp, TFilePathSet &fpset);

  /*!
    Translates a level path into the corresponding hook path (no check for
    the existence of said hook file).
  */
  static TFilePath getHookPath(const TFilePath &levelPath);

  /*!
    \brief Returns the list of \a existing hook files associated to the
           specified level, sorted by modification date (last modified at
    front). The list stores <I>local paths<\I> relative to
    decodedLevelPath.getParentDir().

    \note A level may have multiple hook files if the hook set was edited
          with older Toonz versions. Use this function if you need to access all
          of them. Only the latest file format is considered when loading a
    level.
  */
  static QStringList getHookFiles(const TFilePath &decodedLevelPath);

  /*!
    \brief Returns the path of the newest \a existing hook file associated to
    the specified \b decoded level path - or an empty path if none was found.

    \note In case there are more than one hook file (ie files from older
          Toonz version), the latest file version is used.
  */
  static TFilePath getExistingHookFile(const TFilePath &decodedLevelPath);

  static void setCompatibilityMasks(int writeMask, int neededMask,
                                    int forbiddenMask);

public Q_SLOTS:
  void onPaletteChanged();  //!< Invoked when some colorstyle has been changed

private:
  using FramesSet = boost::container::flat_set<TFrameId>;

private:
  std::unique_ptr<LevelProperties> m_properties;
  std::unique_ptr<TContentHistory> m_contentHistory;

  TPalette *m_palette;

  FramesSet m_frames;

  std::map<TFrameId, TFrameId>
      m_renumberTable;  //!< Maps disk-frames to level-frames.
  //!  Typically, the 2 match - however, the situation changes
  //!  after an explicit frame renumbering process. In case
  //!  a level-frame is deleted, the table entry is removed;
  //!  it will be physically removed only when the level is saved.
  //!  A similar thing happens with newly inserted frames.
  std::map<TFrameId, int> m_framesStatus;

  std::set<TFrameId> m_editableRange;

  TFilePath m_path;
  TFilePath m_scannedPath;

  std::string m_idBase;
  std::wstring m_editableRangeUserInfo;

  bool m_isSubsequence;
  bool m_16BitChannelLevel;
  bool m_floatChannelLevel;
  bool m_isReadOnly;
  bool m_temporaryHookMerged;  //!< Used only during hook merge (and hence
                               //!< during saving)

private:
  //! Save simple level in scene-decoded path \p decodedFp.
  void saveSimpleLevel(const TFilePath &decodedFp,
                       bool overwritePalette = true);
};

//=====================================================================================

#ifdef _WIN32
template class DV_EXPORT_API TSmartPointerT<TXshSimpleLevel>;
#endif

typedef TSmartPointerT<TXshSimpleLevel> TXshSimpleLevelP;

//=====================================================================================

DVAPI void setLoadingLevelRange(const TFrameId &fromFid, const TFrameId &toFid);
DVAPI void getLoadingLevelRange(TFrameId &fromFid, TFrameId &toFid);

#endif  // TXSHSIMPLELEVEL_INCLUDED
