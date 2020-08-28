
// TnzTools includes
#include <tools/tool.h>
#include <tools/toolutils.h>
#include <tools/toolhandle.h>
#include <tools/cursors.h>
#include <tools/assistant.h>
#include <tools/inputmanager.h>

// TnzLib includes
#include <toonz/tapplication.h>
#include <toonz/txshlevelhandle.h>
#include <toonz/txsheethandle.h>

// TnzCore includes
#include <tgl.h>
#include <tproperty.h>
#include <tmetaimage.h>

#include <toonzqt/selection.h>
#include <toonzqt/selectioncommandids.h>
#include <toonzqt/tselectionhandle.h>

// For Qt translation support
#include <QCoreApplication>

#include <map>

//-------------------------------------------------------------------

//=============================================================================
// Edit Assistants Undo
//-----------------------------------------------------------------------------

class EditAssistantsUndo final : public ToolUtils::TToolUndo {
private:
  bool m_isCreated;
  bool m_isRemoved;
  int m_index;
  TMetaObjectP m_metaObject;
  TVariant m_oldData;
  TVariant m_newData;
  size_t m_size;

public:
  EditAssistantsUndo(TXshSimpleLevel *level, const TFrameId &frameId,
                     bool frameCreated, bool levelCreated, bool objectCreated,
                     bool objectRemoved, int index, TMetaObjectP metaObject,
                     TVariant oldData)
      : ToolUtils::TToolUndo(level, frameId, frameCreated, levelCreated)
      , m_isCreated(objectCreated)
      , m_isRemoved(objectRemoved)
      , m_index(index)
      , m_metaObject(metaObject)
      , m_oldData(oldData)
      , m_newData(m_metaObject->data())
      , m_size(m_oldData.getMemSize() + m_newData.getMemSize()) {}

  int getSize() const override { return m_size; }
  QString getToolName() override { return QString("Edit Assistants Tool"); }

  void process(bool remove, const TVariant &data) const {
    if (TMetaImage *metaImage = dynamic_cast<TMetaImage *>(
            m_level->getFrame(m_frameId, true).getPointer())) {
      TMetaImage::Writer writer(*metaImage);
      bool found = false;
      for (TMetaObjectList::iterator i = writer->begin(); i != writer->end();
           ++i)
        if ((*i) == m_metaObject) {
          if (remove) writer->erase(i);
          found = true;
          break;
        }
      if (!remove) {
        if (!found)
          writer->insert(
              writer->begin() +
                  std::max(0, std::min((int)writer->size(), m_index)),
              m_metaObject);
        m_metaObject->data() = data;
        if (m_metaObject->handler()) m_metaObject->handler()->fixData();
      }
    }
  }

  void undo() const override {
    removeLevelAndFrameIfNeeded();
    process(m_isCreated, m_oldData);
    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  void redo() const override {
    insertLevelAndFrameIfNeeded();
    process(m_isRemoved, m_newData);
    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }
};

//=============================================================================
// Edit Assistants Tool
//-----------------------------------------------------------------------------

class EditAssistantsTool final : public TTool {
  Q_DECLARE_TR_FUNCTIONS(EditAssistantsTool)
public:
  class Selection final : public TSelection {
  private:
    EditAssistantsTool &tool;

  public:
    explicit Selection(EditAssistantsTool &tool) : tool(tool) {}
    void deleteSelection() { tool.removeSelected(); }

    void enableCommands() override {
      if (!isEmpty())
        enableCommand(this, MI_Clear, &Selection::deleteSelection);
    }
    bool isEmpty() const override { return !tool.isSelected(); }
    void selectNone() override { tool.deselect(); }
  };

protected:
  enum Mode { ModeImage, ModeAssistant, ModePoint };

  TPropertyGroup m_allProperties;
  TPropertyGroup m_toolProperties;
  TEnumProperty m_assistantType;
  TStringId m_newAssisnantType;

  bool m_dragging;
  bool m_dragAllPoints;
  TSmartHolderT<TImage> m_currentImage;
  TMetaObjectH m_currentAssistant;
  bool m_currentAssistantCreated;
  bool m_currentAssistantChanged;
  int m_currentAssistantIndex;
  TVariant m_currentAssistantBackup;
  TStringId m_currentPointName;
  TPointD m_currentPointOffset;
  TPointD m_currentPosition;

  TMetaImage::Reader *m_reader;
  TMetaImage *m_readImage;
  TMetaObjectPC m_readObject;
  const TAssistant *m_readAssistant;

  TMetaImage::Writer *m_writer;
  TMetaImage *m_writeImage;
  TMetaObjectP m_writeObject;
  TAssistant *m_writeAssistant;

  Selection *selection;

public:
  EditAssistantsTool()
      : TTool("T_EditAssistants")
      , m_assistantType("AssistantType")
      , m_dragging()
      , m_dragAllPoints()
      , m_currentAssistantCreated()
      , m_currentAssistantChanged()
      , m_currentAssistantIndex(-1)
      , m_reader()
      , m_readImage()
      , m_readAssistant()
      , m_writer()
      , m_writeImage()
      , m_writeAssistant() {
    selection = new Selection(*this);
    bind(MetaImage | EmptyTarget);
    m_toolProperties.bind(m_assistantType);
    updateTranslation();
  }

  ~EditAssistantsTool() {
    close();
    delete selection;
  }

  ToolType getToolType() const override { return TTool::LevelWriteTool; }
  int getCursorId() const override { return ToolCursor::StrokeSelectCursor; }
  void onImageChanged() override {
    if (m_currentImage != getImage(false)) resetCurrentPoint();
    getViewer()->GLInvalidateAll();
  }
  ToolModifiers getToolModifiers() const override { return ModifierAssistants; }

  void updateAssistantTypes() {
    std::wstring value = m_assistantType.getValue();

    m_assistantType.deleteAllValues();
    m_assistantType.addValueWithUIName(L"", tr("--"));

    const TMetaObject::Registry &registry = TMetaObject::getRegistry();
    for (TMetaObject::Registry::const_iterator i = registry.begin();
         i != registry.end(); ++i)
      if (const TAssistantType *assistantType =
              dynamic_cast<const TAssistantType *>(i->second))
        if (assistantType->name)
          m_assistantType.addValueWithUIName(
              to_wstring(assistantType->name.str()),
              assistantType->getLocalName());

    if (m_assistantType.indexOf(value) >= 0) m_assistantType.setValue(value);
  }

  TPropertyGroup *getProperties(int) override {
    m_allProperties.clear();
    for (int i = 0; i < m_toolProperties.getPropertyCount(); ++i)
      m_allProperties.bind(*m_toolProperties.getProperty(i));
    if (Closer closer = read(ModeAssistant)) {
      m_readAssistant->updateTranslation();
      TPropertyGroup &assistantProperties = m_readAssistant->getProperties();
      for (int i = 0; i < assistantProperties.getPropertyCount(); ++i)
        m_allProperties.bind(*assistantProperties.getProperty(i));
    }
    return &m_allProperties;
  }

  void onActivate() override {
    updateAssistantTypes();
    resetCurrentPoint();
  }

  void onDeactivate() override { resetCurrentPoint(); }

  void updateTranslation() override {
    m_assistantType.setQStringName(tr("Assistant Type"));
    updateAssistantTypes();
    if (Closer closer = read(ModeAssistant))
      m_readAssistant->updateTranslation();
  }

  bool onPropertyChanged(std::string name, bool addToUndo) override {
    if (TProperty *property = m_toolProperties.getProperty(name)) {
      if (name == m_assistantType.getName())
        m_newAssisnantType =
            TStringId::find(to_string(m_assistantType.getValue()));
    } else {
      if (Closer closer = write(ModeAssistant, true))
        m_writeAssistant->propertyChanged(TStringId::find(name));
      if (addToUndo) apply();
      getViewer()->GLInvalidateAll();
    }
    return true;
  }

  TSelection *getSelection() override { return isSelected() ? selection : 0; }

protected:
  void close() {
    m_readAssistant = 0;
    m_readObject.reset();
    m_readImage = 0;
    if (m_reader) delete (m_reader);
    m_reader = 0;

    m_writeAssistant = 0;
    m_writeObject.reset();
    m_writeImage = 0;
    if (m_writer) delete (m_writer);
    m_writer = 0;
  }

  bool openRead(Mode mode) {
    close();

    if ((mode >= ModeAssistant && !m_currentAssistant) ||
        (mode >= ModeAssistant && m_currentAssistantIndex < 0) ||
        (mode >= ModePoint && !m_currentPointName))
      return false;

    m_readImage = dynamic_cast<TMetaImage *>(getImage(false));
    if (m_readImage) {
      m_reader = new TMetaImage::Reader(*m_readImage);
      if (mode == ModeImage) return true;

      if (m_currentAssistantIndex < (int)(*m_reader)->size() &&
          (**m_reader)[m_currentAssistantIndex] == m_currentAssistant) {
        m_readObject    = (**m_reader)[m_currentAssistantIndex];
        m_readAssistant = m_readObject->getHandler<TAssistant>();
        if (mode == ModeAssistant) return true;

        if (m_readAssistant->findPoint(m_currentPointName)) {
          if (mode == ModePoint) return true;
        }
      }
    }

    close();
    return false;
  }

  void touch() {
    if (m_writeAssistant && !m_currentAssistantChanged) {
      m_currentAssistantBackup  = m_writeAssistant->data();
      m_currentAssistantChanged = true;
    }
  }

  bool openWrite(Mode mode, bool touch = false) {
    close();

    if ((mode >= ModeAssistant && !m_currentAssistant) ||
        (mode >= ModeAssistant && m_currentAssistantIndex < 0) ||
        (mode >= ModePoint && !m_currentPointName))
      return false;

    m_writeImage = dynamic_cast<TMetaImage *>(getImage(true));
    if (m_writeImage) {
      m_writer = new TMetaImage::Writer(*m_writeImage);
      if (mode == ModeImage) return true;

      if (m_currentAssistantIndex < (int)(*m_writer)->size() &&
          (**m_writer)[m_currentAssistantIndex] == m_currentAssistant) {
        m_writeObject    = (**m_writer)[m_currentAssistantIndex];
        m_writeAssistant = m_writeObject->getHandler<TAssistant>();
        if ((mode == ModeAssistant) ||
            (mode == ModePoint &&
             m_writeAssistant->findPoint(m_currentPointName))) {
          if (touch) this->touch();
          return true;
        }
      }
    }

    close();
    return false;
  }

  //! helper functions for construction like this:
  //!   if (Closer closer = read(ModeAssistant)) { do-something... }
  struct Closer {
    struct Args {
      EditAssistantsTool *owner;
      Args(EditAssistantsTool &owner) : owner(&owner) {}
      operator bool() const  //!< declare bool-convertor here to prevent
                             //! convertion path: Args->Closer->bool
      {
        return owner && (owner->m_reader || owner->m_writer);
      }
      void close() {
        if (owner) owner->close();
      }
    };
    Closer(const Args &args) : args(args) {}
    ~Closer() { args.close(); }
    operator bool() const { return args; }

  private:
    Args args;
  };

  Closer::Args read(Mode mode) {
    openRead(mode);
    return Closer::Args(*this);
  }
  Closer::Args write(Mode mode, bool touch = false) {
    openWrite(mode, touch);
    return Closer::Args(*this);
  }

  void updateOptionsBox() {
    getApplication()->getCurrentTool()->notifyToolOptionsBoxChanged();
  }

  void resetCurrentPoint(bool updateOptionsBox = true) {
    close();
    m_currentImage.reset();
    m_currentAssistant.reset();
    m_currentAssistantCreated = false;
    m_currentAssistantChanged = false;
    m_currentAssistantIndex   = -1;
    m_currentPointName.reset();
    m_currentPointOffset = TPointD();
    m_currentAssistantBackup.reset();

    // deselect points
    if (Closer closer = read(ModeImage))
      for (TMetaObjectListCW::iterator i = (*m_reader)->begin();
           i != (*m_reader)->end(); ++i)
        if (*i)
          if (const TAssistant *assistant = (*i)->getHandler<TAssistant>())
            assistant->deselectAll();

    if (updateOptionsBox) this->updateOptionsBox();
  }

  bool findCurrentPoint(const TPointD &position, double pixelSize,
                        bool updateOptionsBox = true) {
    resetCurrentPoint(false);
    if (Closer closer = read(ModeImage)) {
      m_currentImage.set(m_readImage);
      for (TMetaObjectListCW::iterator i = (*m_reader)->begin();
           i != (*m_reader)->end(); ++i) {
        if (!*i) continue;

        const TAssistant *assistant = (*i)->getHandler<TAssistant>();
        if (!assistant) continue;

        assistant->deselectAll();

        // last points is less significant and don't affecting the first points
        // so iterate points in reverse order to avoid unsolvable points
        // overlapping
        const TAssistantPointOrder &points = assistant->pointsOrder();
        for (TAssistantPointOrder::const_reverse_iterator j = points.rbegin();
             j != points.rend() && m_currentAssistantIndex < 0; ++j) {
          const TAssistantPoint &p = **j;
          TPointD offset           = p.position - position;
          if (p.visible &&
              norm2(offset) <= p.radius * p.radius * pixelSize * pixelSize) {
            m_currentAssistant.set(*i);
            m_currentAssistantIndex = i - (*m_reader)->begin();
            m_currentPointName      = p.name;
            m_currentPointOffset    = offset;
            assistant->selectAll();
          }
        }
      }
    }

    if (updateOptionsBox) this->updateOptionsBox();
    return m_currentAssistantIndex >= 0;
  }

  bool apply() {
    bool success = false;
    if (m_currentAssistantChanged || m_currentAssistantCreated) {
      if (Closer closer = write(ModeAssistant)) {
        m_writeAssistant->fixData();
        TUndoManager::manager()->add(new EditAssistantsUndo(
            getApplication()->getCurrentLevel()->getLevel()->getSimpleLevel(),
            getCurrentFid(), m_isFrameCreated, m_isLevelCreated,
            m_currentAssistantCreated, false, m_currentAssistantIndex,
            m_writeObject, m_currentAssistantBackup));
        m_currentAssistantCreated = false;
        m_currentAssistantChanged = false;
        m_isFrameCreated          = false;
        m_isLevelCreated          = false;
        success                   = true;
      }
    }

    if (success) {
      notifyImageChanged();
      getApplication()->getCurrentTool()->notifyToolChanged();
      TTool::getApplication()->getCurrentSelection()->setSelection(
          getSelection());
      getViewer()->GLInvalidateAll();
    }

    return success;
  }

public:
  void deselect() { resetCurrentPoint(); }

  bool isSelected() { return read(ModeAssistant); }

  bool removeSelected() {
    apply();
    bool success = false;
    if (Closer closer = write(ModeAssistant, true)) {
      (*m_writer)->erase((*m_writer)->begin() + m_currentAssistantIndex);
      TUndoManager::manager()->add(new EditAssistantsUndo(
          getApplication()->getCurrentLevel()->getLevel()->getSimpleLevel(),
          getCurrentFid(),
          false,  // frameCreated
          false,  // levelCreated
          false,  // objectCreated
          true,   // objectRemoved
          m_currentAssistantIndex, m_writeObject, m_writeObject->data()));
      success = true;
    }

    if (success) notifyImageChanged();

    resetCurrentPoint();
    getApplication()->getCurrentTool()->notifyToolChanged();
    TTool::getApplication()->getCurrentSelection()->setSelection(
        getSelection());
    getViewer()->GLInvalidateAll();
    return success;
  }

  bool preLeftButtonDown() override {
    if (m_assistantType.getIndex() != 0) touchImage();
    TTool::getApplication()->getCurrentSelection()->setSelection(
        getSelection());
    return true;
  }

  void paintTrackBegin(const TTrackPoint &point, const TTrack &track,
                       bool firstTrack) override {
    if (!firstTrack) return;
    apply();
    m_dragging      = true;
    m_dragAllPoints = false;
    if (m_newAssisnantType) {
      // create assistant
      resetCurrentPoint(false);
      if (Closer closer = write(ModeImage)) {
        TMetaObjectP object(new TMetaObject(m_newAssisnantType));
        if (TAssistant *assistant = object->getHandler<TAssistant>()) {
          assistant->setDefaults();
          assistant->move(point.position);
          assistant->selectAll();
          m_currentImage.set(m_writeImage);
          m_dragAllPoints           = true;
          m_currentAssistantCreated = true;
          m_currentAssistantChanged = true;
          m_currentAssistantIndex   = (int)(*m_writer)->size();
          m_currentAssistant        = object;
          m_currentPointName        = assistant->getBasePoint().name;
          m_currentPointOffset      = TPointD();
          m_currentAssistantBackup  = assistant->data();
          (*m_writer)->push_back(object);
        }
      }
      updateOptionsBox();
      m_newAssisnantType.reset();
    } else {
      TAffine matrix = getViewer()->getInputManager()->screenToTool();
      double pixelSize =
          0.5 * (sqrt(matrix.a11 * matrix.a11 + matrix.a21 * matrix.a21) +
                 sqrt(matrix.a12 * matrix.a12 + matrix.a22 * matrix.a22));
      findCurrentPoint(point.position, pixelSize);
      if (track.getKeyState(point.time).isPressed(TKey::shift))
        if (Closer closer = read(ModePoint)) {
          m_currentPointName = m_readAssistant->getBasePoint().name;
          m_currentPointOffset =
              m_readAssistant->getBasePoint().position - point.position;
          m_dragAllPoints = true;
        }
    }

    m_currentPosition = point.position;
    getViewer()->GLInvalidateAll();
  }

  void paintTrackMotion(const TTrackPoint &point, const TTrack &,
                        bool firstTrack) override {
    if (!firstTrack) return;
    if (m_dragAllPoints) {
      if (Closer closer = write(ModeAssistant))
        if (m_writeAssistant->move(point.position + m_currentPointOffset))
          touch();
    } else {
      if (Closer closer = write(ModePoint))
        if (m_writeAssistant->movePoint(m_currentPointName,
                                        point.position + m_currentPointOffset))
          touch();
    }
    m_currentPosition = point.position;
    getViewer()->GLInvalidateAll();
  }

  void paintTrackEnd(const TTrackPoint &point, const TTrack &,
                     bool firstTrack) override {
    if (!firstTrack) return;
    if (m_dragAllPoints) {
      if (Closer closer = write(ModeAssistant))
        if (m_writeAssistant->move(point.position + m_currentPointOffset))
          touch();
    } else {
      if (Closer closer = write(ModePoint))
        if (m_writeAssistant->movePoint(m_currentPointName,
                                        point.position + m_currentPointOffset))
          touch();
    }

    apply();
    m_assistantType.setIndex(0);
    getApplication()->getCurrentTool()->notifyToolChanged();
    TTool::getApplication()->getCurrentSelection()->setSelection(
        getSelection());
    m_currentPosition = point.position;
    getViewer()->GLInvalidateAll();
    m_dragAllPoints = false;
    m_dragging      = false;
  }

  void draw() override {
    if (Closer closer = read(ModeImage))
      for (TMetaObjectListCW::iterator i = (*m_reader)->begin();
           i != (*m_reader)->end(); ++i)
        if (*i)
          if (const TAssistant *assistant = (*i)->getHandler<TAssistant>())
            assistant->drawEdit(getViewer());
  }
};

//-------------------------------------------------------------------

EditAssistantsTool editAssistantsTool;
