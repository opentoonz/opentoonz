

#include "iocommand.h"

// Tnz6 includes
#include "tapp.h"
#include "menubarcommandids.h"
#include "loadfolderpopup.h"

// TnzQt includes
#include "toonzqt/dvdialog.h"
#include "toonzqt/gutil.h"
#include "toonzqt/validatedchoicedialog.h"
#include "toonzqt/menubarcommand.h"

// TnzLib includes
#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"
#include "toonz/preferences.h"

// TnzCore includes
#include "tsystem.h"
#include "tfiletype.h"

// Qt includes
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QRegExp>
#include <QButtonGroup>
#include <QRadioButton>

// boost includes
#include <boost/optional.hpp>
#include <boost/operators.hpp>
#include <boost/range.hpp>

#include <boost/bind.hpp>

#include <boost/iterator/transform_iterator.hpp>

#include <boost/range/algorithm/for_each.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/adaptor/adjacent_filtered.hpp>
#include <boost/range/adaptor/transformed.hpp>

#include <boost/utility/in_place_factory.hpp>

// STD includes
#include <set>
#include <algorithm>

//************************************************************************
//    Local namespace  structures
//************************************************************************

namespace {

typedef IoCmd::LoadResourceArguments LRArgs;

typedef TFilePath (*PathFunc)(const TFilePath &);

//--------------------------------------------------------------

struct FormatData {
  QRegExp m_regExp;
  PathFunc m_resourcePathFunc, m_componentPathFunc;
};

//************************************************************************
//    ResourceData  definition
//************************************************************************

struct Resource  //!  A single resource to be loaded.
{
  struct Path : private boost::less_than_comparable<Path>  //!  Path locating a
                                                           //!  resource.
  {
    TFilePath m_rootFp,  //!< Selected root folder hosting the resource.
        m_relFp;         //!< Path representing the resource, \a relative
                         //!  to \p m_rootFolder.
  public:
    Path(const TFilePath &rootPath, const TFilePath &relativeFilePath)
        : m_rootFp(rootPath), m_relFp(relativeFilePath) {
      assert(m_rootFp.isAbsolute());
      assert(!m_relFp.isAbsolute());
    }

    TFilePath absoluteResourcePath() const { return m_rootFp + m_relFp; }

    bool operator<(const Path &other) const {
      return (m_rootFp < other.m_rootFp ||
              (!(other.m_relFp < m_relFp) && m_relFp < other.m_relFp));
    }
  };

  struct Component {
    TFilePath m_srcFp;    //!< Source path, <I>relative to parent folder</I>.
    PathFunc m_pathFunc;  //!< Possible callback function transforming source
                          //!  paths into their imported counterparts.
  public:
    Component(const TFilePath &srcPath, PathFunc pathFunc)
        : m_srcFp(srcPath), m_pathFunc(pathFunc) {
      assert(m_srcFp.getParentDir() == TFilePath());
    }
  };

  typedef std::vector<Component> CompSeq;

public:
  Path m_path;           //!< The level path.
  CompSeq m_components;  //!< File Paths for level components. The first path
                         //!  is intended as the resource's file representant.
  boost::optional<LevelOptions>
      m_levelOptions;  //!< Level Options to be loaded for a level resource.

public:
  Resource(const Path &path) : m_path(path) {}
  Resource(const TFilePath &rootPath, const TFilePath &relativeFilePath)
      : m_path(rootPath, relativeFilePath) {}
  Resource(const Path &path, const CompSeq &components)
      : m_path(path), m_components(components) {}
};

//************************************************************************
//    OverwriteDialog  definition
//************************************************************************

class OverwriteDialog final : public DVGui::ValidatedChoiceDialog {
public:
  enum Resolution {
    NO_RESOLUTION,
    CANCEL,
    OVERWRITE,
    SKIP,
  };

  struct Obj {
    const TFilePath &m_dstDir;
    Resource &m_rsrc;
  };

public:
  OverwriteDialog(QWidget *parent);

  QString acceptResolution(void *obj, int resolution, bool applyToAll) override;
};

//************************************************************************
//    Local namespace  functions
//************************************************************************

TFilePath relativePath(const TFilePath &from, const TFilePath &to) {
  return TFilePath(QDir(from.getQString()).relativeFilePath(to.getQString()));
}

//--------------------------------------------------------------

bool isLoadable(const TFilePath &resourcePath) {
  TFileType::Type type = TFileType::getInfo(resourcePath);

  return (type & TFileType::IMAGE || type & TFileType::LEVEL);
}

//--------------------------------------------------------------

template <typename RegStruct>
bool exactMatch(const RegStruct &regStruct, const TFilePath &fp) {
  return regStruct.m_regExp.exactMatch(fp.getQString());
}

//==============================================================

TFilePath multiframeResourcePath(const TFilePath &fp) {
  return fp.withFrame(TFrameId::EMPTY_FRAME);
}

//--------------------------------------------------------------

TFilePath retasComponentPath(const TFilePath &fp) {
  std::wstring name = fp.getWideName();

  // Assumes the name is Axxxx.tga and needs to be changed to A.xxxx.tga
  if (name.size() > 4 && fp.getDots() != "..")
    name.insert(name.size() - 4, 1, L'.');

  return fp.withName(name);
}

//--------------------------------------------------------------

TFilePath retasResourcePath(const TFilePath &fp) {
  return retasComponentPath(fp).withFrame(TFrameId::EMPTY_FRAME);
}

//--------------------------------------------------------------

static const FormatData l_formatDatas[] = {
    {QRegExp(".+\\.[0-9]{4,4}.*\\..*"), &multiframeResourcePath, 0},
    {QRegExp(".+[0-9]{4,4}\\.tga", Qt::CaseInsensitive), &retasResourcePath,
     &retasComponentPath}};

//==============================================================

typedef std::pair<Resource::Path, int> RsrcKey;
typedef std::map<RsrcKey, Resource::CompSeq> RsrcMap;

auto const differentPath = [](const RsrcMap::value_type &a,
                              const RsrcMap::value_type &b) -> bool {
  return (a.first.first < b.first.first) || (b.first.first < a.first.first);
};

struct buildResources_locals {
  static bool isValid(const RsrcMap::value_type &rsrcVal) {
    return (isLoadable(rsrcVal.first.first.m_relFp) && !rsrcVal.second.empty());
  }

  static Resource toResource(const RsrcMap::value_type &rsrcVal) {
    return Resource(rsrcVal.first.first, rsrcVal.second);
  }

  struct MergeData {
    QRegExp m_regExp;    //!< Path regexp for file pattern recognition.
    int m_componentIdx;  //!< Starting index for components merging.
  };

  static void mergeInto(RsrcMap::iterator &rt, RsrcMap &rsrcMap) {
    // NOTE: This algorithm works for 1-level-deep inclusions. I guess this is
    // sufficient,
    //       for now.

    static const std::string componentsTable[] = {"cln", "tpl", "hst"};

    static const MergeData mergeTable[] = {
        {QRegExp(".*\\.\\..*"), 0}, {QRegExp(".*\\.tlv"), 1}, {QRegExp(), 3}};

    // Lookup rd's path in the mergeTable
    const MergeData *mdt,
        *mdEnd = mergeTable + boost::size(mergeTable) - 1;  // Last item is fake

    mdt = std::find_if(mergeTable, mdEnd,
                       boost::bind(exactMatch<MergeData>, _1,
                                   boost::cref(rt->first.first.m_relFp)));

    if (mdt != mdEnd) {
      // Lookup every possible resource component to merge
      const std::string *cBegin = componentsTable + mdt->m_componentIdx,
                        *cEnd   = componentsTable + (mdt + 1)->m_componentIdx;

      for (const std::string *ct = cBegin; ct != cEnd; ++ct) {
        RsrcKey childKey(
            Resource::Path(rt->first.first.m_rootFp,
                           rt->first.first.m_relFp.withNoFrame().withType(*ct)),
            rt->first.second);

        RsrcMap::iterator chrt = rsrcMap.find(childKey);

        if (chrt != rsrcMap.end()) {
          // Move every component into rsrc
          rt->second.insert(rt->second.end(), chrt->second.begin(),
                            chrt->second.end());
          chrt->second.clear();
        }
      }
    }
  }

  static void assignLevelOptions(Resource &rsrc) {
    assert(!rsrc.m_components.empty());

    const Preferences &prefs = *Preferences::instance();

    // Match level format against the level's *file representant*
    int formatIdx = prefs.matchLevelFormat(rsrc.m_components.front().m_srcFp);

    if (formatIdx >= 0)
      rsrc.m_levelOptions = prefs.levelFormat(formatIdx).m_options;
  }

};  // buildResources_locals

void buildResources(std::vector<Resource> &resources, const TFilePath &rootPath,
                    const TFilePath &subPath = TFilePath()) {
  typedef buildResources_locals locals;

  const TFilePath &folderPath = rootPath + subPath;
  assert(QFileInfo(folderPath.getQString()).isDir());

  // Extract loadable levels in the folder
  QDir folderDir(folderPath.getQString());
  {
    const QStringList &files =
        folderDir.entryList(QDir::Files);  // Files only first

    // Store every possible resource path
    RsrcMap rsrcMap;

    QStringList::const_iterator ft, fEnd = files.end();
    for (ft = files.begin(); ft != fEnd; ++ft) {
      const TFilePath &compPath = TFilePath(*ft);  // Relative to folderPath
      PathFunc componentFunc    = 0;

      TFilePath relPath = relativePath(rootPath, folderPath + compPath);

      const FormatData *fdt,
          *fdEnd = l_formatDatas + boost::size(l_formatDatas);
      fdt        = std::find_if(
          l_formatDatas, fdEnd,
          boost::bind(exactMatch<FormatData>, _1, boost::cref(relPath)));

      if (fdt != fdEnd) {
        relPath       = fdt->m_resourcePathFunc(relPath);
        componentFunc = fdt->m_componentPathFunc;
      }

      assert(!relPath.isEmpty());

      RsrcKey rsrcKey(Resource::Path(rootPath, relPath),
                      int(fdt - l_formatDatas));
      rsrcMap[rsrcKey].push_back(Resource::Component(compPath, componentFunc));
    }

    // Further let found resources merge with others they recognize as
    // 'children'
    RsrcMap::iterator rt, rEnd = rsrcMap.end();
    for (rt = rsrcMap.begin(); rt != rEnd; ++rt) locals::mergeInto(rt, rsrcMap);

    // Export valid data into the output resources collection
    boost::copy(rsrcMap | boost::adaptors::filtered(locals::isValid) |
                    boost::adaptors::adjacent_filtered(
                        differentPath)  // E.g. A.xxxx.tga and Axxxx.tga
                    | boost::adaptors::transformed(locals::toResource),
                std::back_inserter(resources));
  }

  // Look for level options associated to each level
  std::for_each(resources.begin(), resources.end(), locals::assignLevelOptions);

  // Recursive on sub-folders
  const QStringList &dirs =
      folderDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

  QStringList::const_iterator dt, dEnd = dirs.end();
  for (dt = dirs.begin(); dt != dEnd; ++dt)
    buildResources(resources, rootPath, subPath + TFilePath(*dt));
}

//--------------------------------------------------------------

TFilePath dstPath(const TFilePath &dstDir, const Resource::Component &comp) {
  return dstDir +
         (comp.m_pathFunc ? comp.m_pathFunc(comp.m_srcFp) : comp.m_srcFp);
}

//--------------------------------------------------------------

struct import_Locals {
  const ToonzScene &m_scene;
  std::unique_ptr<OverwriteDialog> m_overwriteDialog;

  void switchToDst(Resource::Path &path) {
    path.m_rootFp = m_scene.decodeFilePath(
        m_scene.getImportedLevelPath(path.absoluteResourcePath())
            .getParentDir()            // E.g. +drawings/
        + path.m_rootFp.getWideName()  // Root dir name
        );
  }

  static void copy(const TFilePath &srcDir, const TFilePath &dstDir,
                   const Resource::Component &comp, bool overwrite) {
    TSystem::copyFile(dstPath(dstDir, comp), srcDir + comp.m_srcFp, overwrite);
  }

  void import(Resource &rsrc) {
    if (!m_scene.isExternPath(rsrc.m_path.m_rootFp)) return;

    try {
      // Build parent folder paths
      const TFilePath &srcDir =
          rsrc.m_path.absoluteResourcePath().getParentDir();

      switchToDst(rsrc.m_path);
      const TFilePath &dstDir =
          rsrc.m_path.absoluteResourcePath().getParentDir();

      // Make sure destination folder exists
      TSystem::mkDir(dstDir);

      // Find out whether a destination component file
      // already exists
      bool overwrite = false;

      OverwriteDialog::Obj obj = {dstDir, rsrc};
      switch (m_overwriteDialog->execute(&obj)) {
      case OverwriteDialog::OVERWRITE:
        overwrite = true;
        break;
      case OverwriteDialog::SKIP:
        return;
      }

      // Perform resource copy
      std::for_each(rsrc.m_components.begin(), rsrc.m_components.end(),
                    boost::bind(copy, boost::cref(srcDir), boost::cref(dstDir),
                                _1, overwrite));
    } catch (const TException &e) {
      DVGui::error(QString::fromStdWString(e.getMessage()));
    } catch (...) {
    }
  }

};  // import_Locals

void import(const ToonzScene &scene, std::vector<Resource> &resources,
            IoCmd::LoadResourceArguments::ScopedBlock &sb) {
  import_Locals locals = {scene, std::unique_ptr<OverwriteDialog>()};

  // Setup import GUI
  int r, rCount = resources.size();

  DVGui::ProgressDialog *progressDialog = 0;
  if (rCount > 1) {
    progressDialog = &sb.progressDialog();

    progressDialog->setModal(true);
    progressDialog->setMinimum(0);
    progressDialog->setMaximum(rCount);
    progressDialog->show();
  }

  // Perform import
  locals.m_overwriteDialog.reset(new OverwriteDialog(
      progressDialog ? (QWidget *)progressDialog
                     : (QWidget *)TApp::instance()->getMainWindow()));

  for (r = 0; r != rCount; ++r) {
    Resource &rsrc = resources[r];

    if (progressDialog) {
      progressDialog->setLabelText(
          DVGui::ProgressDialog::tr("Importing \"%1\"...")
              .arg(rsrc.m_path.m_relFp.getQString()));
      progressDialog->setValue(r);

      QCoreApplication::processEvents();
    }

    locals.import(rsrc);
  }
}

//************************************************************************
//    OverwriteDialog  implementation
//************************************************************************

OverwriteDialog::OverwriteDialog(QWidget *parent)
    : ValidatedChoiceDialog(parent, ValidatedChoiceDialog::APPLY_TO_ALL) {
  setWindowTitle(QObject::tr("Warning!", "OverwriteDialog"));

  // Option 1: OVERWRITE
  QRadioButton *radioButton = new QRadioButton;
  radioButton->setText(QObject::tr("Overwrite", "OverwriteDialog"));
  radioButton->setFixedHeight(20);
  radioButton->setChecked(true);  // initial option: OVERWRITE

  m_buttonGroup->addButton(radioButton, OVERWRITE);
  addWidget(radioButton);

  // Option 2: SKIP
  radioButton = new QRadioButton;
  radioButton->setText(QObject::tr("Skip", "OverwriteDialog"));
  radioButton->setFixedHeight(20);

  m_buttonGroup->addButton(radioButton, SKIP);
  addWidget(radioButton);

  endVLayout();

  layout()->setSizeConstraint(QLayout::SetFixedSize);
}

//--------------------------------------------------------------

QString OverwriteDialog::acceptResolution(void *obj_, int resolution,
                                          bool applyToAll) {
  struct locals {
    static bool existsComponent(const TFilePath &dstDir,
                                const Resource::Component &comp) {
      return QFile::exists(dstPath(dstDir, comp).getQString());
    }

    static bool existsResource(const TFilePath &dstDir, const Resource &rsrc) {
      return std::any_of(rsrc.m_components.begin(), rsrc.m_components.end(),
                         boost::bind(existsComponent, boost::cref(dstDir), _1));
    }
  };  // locals

  const Obj &obj = *static_cast<Obj *>(obj_);

  QString error;

  if (resolution == NO_RESOLUTION) {
    // Test existence of any resource components
    if (locals::existsResource(obj.m_dstDir, obj.m_rsrc))
      error = QObject::tr(
                  "File \"%1\" already exists.\nDo you want to overwrite it?",
                  "OverwriteDialog")
                  .arg(obj.m_rsrc.m_path.m_relFp.getQString());
  }

  return error;
}

}  // namespace

//************************************************************************
//    API functions
//************************************************************************

int IoCmd::loadResourceFolders(LoadResourceArguments &args,
                               LoadResourceArguments::ScopedBlock *sb) {
  struct locals {
    static LRArgs::ResourceData toResourceData(const Resource &rsrc) {
      LRArgs::ResourceData rd(rsrc.m_path.absoluteResourcePath());
      rd.m_options = rsrc.m_levelOptions;

      return rd;
    }

    static bool isExternPath(const ToonzScene &scene,
                             const LRArgs::ResourceData &rd) {
      return scene.isExternPath(rd.m_path);
    }
  };  // locals

  boost::optional<LRArgs::ScopedBlock> sb_;
  if (!sb) sb = (sb_ = boost::in_place()).get_ptr();

  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  assert(scene);

  // Loading options popup

  // Deal with import decision
  bool import = false;
  {
    if (std::any_of(
            args.resourceDatas.begin(), args.resourceDatas.end(),
            boost::bind(locals::isExternPath, boost::cref(*scene), _1))) {
      // Ask for data import in this case
      int resolutionButton = DVGui::MsgBox(
          QObject::tr("Selected folders don't belong to the current project.\n"
                      "Do you want to import them or load from their original "
                      "location?"),
          QObject::tr("Load"), QObject::tr("Import"), QObject::tr("Cancel"));

      enum { CLOSED, LOAD, IMPORT, CANCELED };
      switch (resolutionButton) {
      case CLOSED:
      case CANCELED:
        return 0;

      case IMPORT:
        import = true;
      }
    }
  }

  // Select resources to be loaded
  std::vector<Resource> resources;

  boost::for_each(
      args.resourceDatas |
          boost::adaptors::transformed(boost::bind<const TFilePath &>(
              &LRArgs::ResourceData::m_path, _1)),
      boost::bind(::buildResources, boost::ref(resources), _1, TFilePath()));

  // Import them if required
  if (import) ::import(*scene, resources, *sb);

  // Temporarily substitute args' import policy
  struct SubstImportPolicy {
    LRArgs &m_args;
    LRArgs::ImportPolicy m_policy;

    SubstImportPolicy(LRArgs &args, LRArgs::ImportPolicy policy)
        : m_args(args), m_policy(args.importPolicy) {
      args.importPolicy = policy;
    }

    ~SubstImportPolicy() { m_args.importPolicy = m_policy; }

  } substImportPolicy(args, import ? LRArgs::IMPORT : LRArgs::LOAD);

  // Perform loading via loadResources()
  args.resourceDatas.assign(
      boost::make_transform_iterator(resources.begin(), locals::toResourceData),
      boost::make_transform_iterator(resources.end(), locals::toResourceData));

  return IoCmd::loadResources(args, false, sb);
}

//************************************************************************
//    Command instantiation
//************************************************************************

OpenPopupCommandHandler<LoadFolderPopup> loadFolderCommand(MI_LoadFolder);
