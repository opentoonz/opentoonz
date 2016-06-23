#pragma once

#ifndef LEVELUPDATER_H
#define LEVELUPDATER_H

// TnzCore includes
#include "tlevel_io.h"

// TnzLib includes
#include "toonz/txshsimplelevel.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=====================================================

//  Forward declarations

class TContentHistory;
class TPropertyGroup;

//=====================================================

//*****************************************************************************************
//    LevelUpdater  declaration
//*****************************************************************************************

//! LevelUpdater is the preferential interface for dealing with \a conservative
//! levels I/O in Toonz.
/*!
    The Toonz Image library already provides the basic TLevelWriter interface
that can be
    used to save image levels on hard disk. This is usually sufficient to
satisfy most needs,
    with a simple enough usage:

    \code
    TLevelWriterP lw(
      levelPath,                                          // Attach to the
required level path (creates level if none).
      Tiio::makeWriterProperties(levelPath.getType()));   // NOTE: Level
Properties are not read from an existing level.

    TPropertyGroup* props(lw->getProperties());
    assert(props);                                        // Currently
guaranteed ONLY if specified at lw's construction.

    props->getProperty("Bits Per Pixel")->setValue(L"64(RGBM)");

    lw->getFrameWriter(fid1)->save(img1);                 // Save image img1 at
frame fid1
    lw->getFrameWriter(fid2)->save(img2);                 // ...
    \endcode

    Some output types, however, may not support image insertions in the middle
of an already existing
    level, but just <I> at the end <\I> (as is the case for movie formats like
\a avi).
    In these cases, a newly created temporary file must be used to have both old
and new images \a appended at
    the right time. Once the appending procedure ends, the temporary gets
renamed to the original file.
    Additional files related to the level must be renamed too, such as the
level's palette and content history.
\n
    Plus, as noted in the example, the format properties of any existing level
at specified path must be manually
    retrieved.
\n\n
    The LevelUpdater is a wrapper class to both TLevelReader and TLevelWriter
employable to deal with these subtelties
    with a simple, equivalent syntax to the one above:

    \code
    LevelUpdater lu(levelPath);                             // Attach to the
required level path (creates level if none).
                                                            // Reads any
existing level format properties.

    TPropertyGroup* props(lu.getLevelWriter()->getProperties());      //
Properties (eventually default ones) are ensured
    assert(props);                                                    // to
exist.

    props->getProperty("Bits Per Pixel")->setValue(L"64(RGBM)");

    lu.update(fid1, img1);                                  // Save image img1
at frame fid1
    lu.update(fid2, img2);                                  // ...
    \endcode

\warning The LevelUpdater requires that <B> saved images must be ordered by
insertion frame <\B>. If necessary,
         users must have them sorted before supplying them tp the updater.
*/

class DVAPI LevelUpdater {
  TLevelWriterP m_lw;  //!< The updater's level writer object
  TFilePath
      m_lwPath;  //!< Path of the file written by m_lw, could be a temporary
  TPropertyGroup *m_pg;  //!< Copy of the file format propeties used by m_lw

  TLevelReaderP m_lr;       //!< The updater's level reader object
  TLevelP m_inputLevel;     //!< The initial source level frames structure
  TImageInfo *m_imageInfo;  //!< The source level's image info

  std::vector<TFrameId>
      m_fids;     //!< Remaining frames stored in the source level
  int m_currIdx;  //!< Current m_fids index

  TXshSimpleLevelP m_sl;  //!< XSheet image level the updater may be attached to

  bool m_usingTemporaryFile;  //!< Whether a temporary file is being used to
                              //! hold additional frames
  bool m_opened;  //!< Wheter the updater is already attached to a level

public:
  LevelUpdater();
  LevelUpdater(TXshSimpleLevel *sl);
  LevelUpdater(const TFilePath &path, TPropertyGroup *lwProperties = 0);
  ~LevelUpdater();

  TLevelWriterP getLevelWriter() { return m_lw; }
  TLevelReaderP getLevelReader() { return m_lr; }

  //! Returns a description of any already existing level on the attached path
  //! (in terms of frames content and palette).
  TLevelP getInputLevel() { return m_inputLevel; }

  //! Returns the image properties of the updated level, if one already exists.
  const TImageInfo *getImageInfo() { return m_imageInfo; }

  //! Attaches the updater to the specified path, using and <I> taking ownership
  //! <\I> of the supplied write properties.
  //! This function may throw in case the specified path has an unrecognized
  //! extension, or the file could
  //! not be opened for write.
  void open(const TFilePath &src, TPropertyGroup *lwProperties);

  //! Attaches the updater to the specified simple level instance. Format
  //! properties are the default in case the level
  //! is saved anew. If the level requires the alpha channel, evenutal bpp and
  //! alpha properties are upgraded accordingly.
  //! This function may throw just like its path-based counterpart.
  void open(TXshSimpleLevel *sl);

  bool opened() const { return m_opened; }

  //! Stores the supplied image in the level, at the specified frame. Remember
  //! that frames \b must be added with
  //! increasing fid ordering.
  void update(const TFrameId &fid, const TImageP &img);

  //! Closes and flushes the level. In case temporary files were used for the
  //! level, they replace the originals now.
  //! In case the replace procedure failed, the temporaries are (silently, in
  //! current implementation) retained on disk.
  void close();

  //! Flushes the level currently opened for write, \a suspending the update
  //! procedure. Since a level cannot be both
  //! opened for read and write at the same time, users could require to flush
  //! all updates and suspend (without closing)
  //! an update procedure to read some of the data that has just been written.
  //! The update procedure will be resumed automatically at the next update() or
  //! close() invocation.

  //! \warning Not all file formats currently support this. A brief list of
  //! currently supporting formats include TLV,
  //! MOV and multi-file levels, whereas formats \b not supporting it include
  //! PLI and AVI (due to TLevelWriter not
  //! supporting \a appends to an already existing level).
  void flush();

private:
  void reset();
  void resume();

  void buildSourceInfo(const TFilePath &fp);
  void buildProperties(const TFilePath &fp);
  TFilePath getNewTemporaryFilePath(const TFilePath &path);
  void addFramesTo(int endIdx);
};

#endif  // LEVELUPDATER_H
