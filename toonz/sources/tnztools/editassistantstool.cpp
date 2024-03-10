
// TnzTools includes
#include <tools/tool.h>
#include <tools/toolutils.h>
#include <tools/toolhandle.h>
#include <tools/cursors.h>
#include <tools/assistant.h>
#include <tools/replicator.h>
#include <tools/inputmanager.h>

// TnzLib includes
#include <toonz/tapplication.h>
#include <toonz/txshlevelhandle.h>
#include <toonz/txsheethandle.h>
#include <toonz/tscenehandle.h>
#include <toonz/tcolumnhandle.h>
#include <toonz/tframehandle.h>
#include <toonz/dpiscale.h>
#include <toonz/toonzscene.h>
#include <toonz/sceneproperties.h>

// TnzCore includes
#include <tgl.h>
#include <tproperty.h>
#include <tmetaimage.h>
#include <tpixelutils.h>

#include <toonzqt/selection.h>
#include <toonzqt/selectioncommandids.h>
#include <toonzqt/tselectionhandle.h>

// For Qt translation support
#include <QCoreApplication>

#include <map>


//-------------------------------------------------------------------

//=============================================================================
// Edit Assistants Swap Undo
//-----------------------------------------------------------------------------

class EditAssistantsReorderUndo final : public ToolUtils::TToolUndo {
private:
  int m_oldPos;
  int m_newPos;

public:
  EditAssistantsReorderUndo(
    TXshSimpleLevel *level,
    const TFrameId &frameId,
    int m_oldPos,
    int m_newPos
  ):
    ToolUtils::TToolUndo(level, frameId),
    m_oldPos(m_oldPos),
    m_newPos(m_newPos)
  { }

  QString getToolName() override
    { return QString("Edit Assistants Tool"); }

  int getSize() const override
    { return sizeof(*this); }

  void process(int oldPos, int newPos) const {
    if (oldPos == newPos || oldPos < 0 || newPos < 0)
      return;
    TMetaImage *metaImage = dynamic_cast<TMetaImage*>(m_level->getFrame(m_frameId, true).getPointer());
    if (!metaImage)
      return;
    
    TMetaImage::Writer writer(*metaImage);
    TMetaObjectList &list = *writer;
    if (oldPos >= list.size() || newPos >= writer->size())
      return;
    
    TMetaObjectP obj = list[oldPos];
    list.erase(list.begin() + oldPos);
    list.insert(list.begin() + newPos, obj);
  }

  void undo() const override {
    process(m_newPos, m_oldPos);
    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  void redo() const override {
    process(m_oldPos, m_newPos);
    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }
};


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
  EditAssistantsUndo(
    TXshSimpleLevel *level,
    const TFrameId &frameId,
    bool frameCreated,
    bool levelCreated,
    bool objectCreated,
    bool objectRemoved,
    int index,
    TMetaObjectP metaObject,
    TVariant oldData
  ):
    ToolUtils::TToolUndo(level, frameId, frameCreated, levelCreated),
    m_isCreated(objectCreated),
    m_isRemoved(objectRemoved),
    m_index(index),
    m_metaObject(metaObject),
    m_oldData(oldData),
    m_newData(m_metaObject->data()),
    m_size(sizeof(*this) + m_oldData.getMemSize() + m_newData.getMemSize())
  { }

  int getSize() const override
    { return m_size; }
  QString getToolName() override
    { return QString("Edit Assistants Tool"); }

  void process(bool remove, const TVariant &data) const {
    if (TMetaImage *metaImage = dynamic_cast<TMetaImage*>(m_level->getFrame(m_frameId, true).getPointer())) {
      TMetaImage::Writer writer(*metaImage);
      bool found = false;
      for(TMetaObjectList::iterator i = writer->begin(); i != writer->end(); ++i)
        if ((*i) == m_metaObject) {
          if (remove) writer->erase(i);
          found = true;
          break;
        }
      if (!remove) {
        if (!found)
          writer->insert(
            writer->begin() + std::max(0, std::min((int)writer->size(), m_index)),
            m_metaObject );
        m_metaObject->data() = data;
        if (m_metaObject->handler())
          m_metaObject->handler()->fixData();
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
    explicit Selection(EditAssistantsTool &tool):
      tool(tool) { }
    void deleteSelection() 
      { tool.removeSelected(); }

    void enableCommands() override
      { if (!isEmpty()) enableCommand(this, MI_Clear, &Selection::deleteSelection); }
    bool isEmpty() const override
      { return !tool.isSelected(); }
    void selectNone() override
      { tool.deselect(); }
  };
  
protected:
  enum Mode {
    ModeImage,
    ModeAssistant,
    ModePoint
  };

  TPropertyGroup m_allProperties;
  TPropertyGroup m_toolProperties;
  TEnumProperty m_assistantType;
  TIntProperty m_replicatorIndex;
  TStringId m_newAssisnantType;

  bool           m_dragging;
  bool           m_dragAllPoints;
  TSmartHolderT<TImage> m_currentImage;
  TMetaObjectH   m_currentAssistant;
  bool           m_currentAssistantCreated;
  bool           m_currentAssistantChanged;
  int            m_currentAssistantIndex;
  TVariant       m_currentAssistantBackup;
  TStringId      m_currentPointName;
  TPointD        m_currentPointOffset;
  TPointD        m_currentPosition;
  TGuidelineList m_currentGuidelines;
  
  TReplicator::PointList m_replicatorPoints;
  
  TMetaImage::Reader   *m_reader;
  TMetaImage           *m_readImage;
  TMetaObjectPC         m_readObject;
  const TAssistantBase *m_readAssistant;

  TMetaImage::Writer   *m_writer;
  TMetaImage           *m_writeImage;
  TMetaObjectP          m_writeObject;
  TAssistantBase       *m_writeAssistant;

  Selection *selection;
  
  // don't try to follow the pointer from history, it may be invalidated
  typedef std::pair<const void*, int> HistoryItem;
  typedef std::vector<HistoryItem> History;
  History m_history;

public:
  EditAssistantsTool():
    TTool("T_EditAssistants"),
    m_assistantType("AssistantType"),
    m_replicatorIndex("ReplicatorIndex", 1, 10, 1, false),
    m_dragging(),
    m_dragAllPoints(),
    m_currentAssistantCreated(),
    m_currentAssistantChanged(),
    m_currentAssistantIndex(-1),
    m_reader(),
    m_readImage(),
    m_readAssistant(),
    m_writer(),
    m_writeImage(),
    m_writeAssistant()
  {
    selection = new Selection(*this);
    // also assign assistants to the "brush" button in toolbar
    bind(MetaImage | EmptyTarget, "T_Brush");
    m_toolProperties.bind(m_assistantType);
    m_replicatorIndex.setSpinner();
    updateTranslation();
  }

  ~EditAssistantsTool() {
    close();
    delete selection;
  }

  ToolType getToolType() const override
    { return TTool::LevelWriteTool; }
  unsigned int getToolHints() const override
    { return TTool::getToolHints() & ~(HintAssistantsAll | HintReplicatorsAll); }
  int getCursorId() const override
    { return ToolCursor::StrokeSelectCursor; }
  void onImageChanged() override {
    if (m_currentImage != getImage(false)) loadHistory();
    getViewer()->GLInvalidateAll();
  }

  void updateAssistantTypes() {
    std::wstring value = m_assistantType.getValue();

    m_assistantType.deleteAllValues();
    m_assistantType.addValueWithUIName(L"", tr("<choose to create>"));

    const TMetaObject::Registry &registry = TMetaObject::getRegistry();
    for(TMetaObject::Registry::const_iterator i = registry.begin(); i != registry.end(); ++i)
      if (const TAssistantType *assistantType = dynamic_cast<const TAssistantType*>(i->second))
        if (assistantType->name)
          m_assistantType.addValueWithUIName(
            to_wstring(assistantType->name.str()),
            assistantType->getLocalName() );

    if (m_assistantType.indexOf(value) >= 0)
      m_assistantType.setValue(value);
  }

  TPropertyGroup* getProperties(int) override {
    m_allProperties.clear();
    for(int i = 0; i < m_toolProperties.getPropertyCount(); ++i)
      m_allProperties.bind( *m_toolProperties.getProperty(i) );
    if (Closer closer = read(ModeAssistant)) {
      m_readAssistant->updateTranslation();
      if (int i = readReplicatorIndex(*m_reader)) {
        m_replicatorIndex.setValue(i);
        m_allProperties.bind( m_replicatorIndex );
      }
      TPropertyGroup &assistantProperties = m_readAssistant->getProperties();
      for(int i = 0; i < assistantProperties.getPropertyCount(); ++i)
        m_allProperties.bind( *assistantProperties.getProperty(i) );
    }
    return &m_allProperties;
  }

  void onActivate() override {
    updateAssistantTypes();
    loadHistory();
  }

  void onDeactivate() override
    { resetCurrentPoint(true, false); }

  void updateTranslation() override {
    m_assistantType.setQStringName( tr("Assistant Type") );
    m_replicatorIndex.setQStringName( tr("Order") );
    updateAssistantTypes();
    if (Closer closer = read(ModeAssistant))
      m_readAssistant->updateTranslation();
  }

  bool onPropertyChanged(std::string name, bool addToUndo) override {
    if (m_replicatorIndex.getName() == name) {
      writeReplicatorIndex(m_replicatorIndex.getValue());
    } else
    if (name == m_assistantType.getName()) {
      m_newAssisnantType = TStringId::find( to_string(m_assistantType.getValue()) );
    } else {
      if (Closer closer = write(ModeAssistant, true))
        m_writeAssistant->propertyChanged(TStringId::find(name));
      if (addToUndo) apply();
      getViewer()->GLInvalidateAll();
    }
    return true;
  }
  
  TSelection* getSelection() override
    { return isSelected() ? selection : 0; }
  
protected:
  void putHistory(const void *img, int assistantIndex) {
    if (!img) return;
    for(History::iterator i = m_history.begin(); i != m_history.end(); )
      if (i->first == img) i = m_history.erase(i); else ++i;
    if (m_history.size() >= 10) m_history.pop_back();
    m_history.push_back(HistoryItem(img, assistantIndex));
  }
  
  void loadHistory() {
    int index = -1;
    if (Closer closer = read(ModeImage))
      for(History::iterator i = m_history.begin(); i != m_history.end(); ++i)
        if (i->first == m_readImage) index = i->second;
    if (index < 0) resetCurrentPoint(true, false); else chooseAssistant(index);
  }
  
  void close() {
    m_readAssistant = 0;
    m_readObject.reset();
    m_readImage = 0;
    if (m_reader) delete(m_reader);
    m_reader = 0;

    m_writeAssistant = 0;
    m_writeObject.reset();
    m_writeImage = 0;
    if (m_writer) delete(m_writer);
    m_writer = 0;
  }

  bool openRead(Mode mode) {
    close();

    if ( (mode >= ModeAssistant && !m_currentAssistant)
      || (mode >= ModeAssistant && m_currentAssistantIndex < 0)
      || (mode >= ModePoint && !m_currentPointName) ) return false;

    m_readImage = dynamic_cast<TMetaImage*>(getImage(false));
    if (m_readImage) {
      m_reader = new TMetaImage::Reader(*m_readImage);
      if (mode == ModeImage) return true;

      if ( m_currentAssistantIndex < (int)(*m_reader)->size()
        && (**m_reader)[m_currentAssistantIndex] == m_currentAssistant )
      {
        m_readObject = (**m_reader)[m_currentAssistantIndex];
        m_readAssistant = m_readObject->getHandler<TAssistantBase>();
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
      m_currentAssistantBackup = m_writeAssistant->data();
      m_currentAssistantChanged = true;
    }
  }

  bool openWrite(Mode mode, bool touch = false) {
    close();

    if ( (mode >= ModeAssistant && !m_currentAssistant)
      || (mode >= ModeAssistant && m_currentAssistantIndex < 0)
      || (mode >= ModePoint && !m_currentPointName) ) return false;

    m_writeImage = dynamic_cast<TMetaImage*>(getImage(true));
    if (m_writeImage) {
      m_writer = new TMetaImage::Writer(*m_writeImage);
      if (mode == ModeImage) return true;

      if ( m_currentAssistantIndex < (int)(*m_writer)->size()
        && (**m_writer)[m_currentAssistantIndex] == m_currentAssistant )
      {
        m_writeObject = (**m_writer)[m_currentAssistantIndex];
        m_writeAssistant = m_writeObject->getHandler<TAssistantBase>();
        if ( (mode == ModeAssistant)
          || (mode == ModePoint && m_writeAssistant->findPoint(m_currentPointName)) )
        {
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
      Args(EditAssistantsTool &owner): owner(&owner) { }
      operator bool() const //!< declare bool-convertor here to prevent convertion path: Args->Closer->bool
        { return owner && (owner->m_reader || owner->m_writer); }
      void close()
        { if (owner) owner->close(); }
    };
    Closer(const Args &args): args(args) { }
    ~Closer() { args.close(); }
    operator bool() const { return args; }
  private:
    Args args;
  };

  Closer::Args read(Mode mode)
    { openRead(mode); return Closer::Args(*this); }
  Closer::Args write(Mode mode, bool touch = false)
    { openWrite(mode, touch); return Closer::Args(*this); }

  void updateOptionsBox()
    { getApplication()->getCurrentTool()->notifyToolOptionsBoxChanged(); }

  void resetCurrentPoint(bool updateOptionsBox = true, bool updateHistory = true) {
    close();
    m_currentImage.reset();
    m_currentAssistant.reset();
    m_currentAssistantCreated = false;
    m_currentAssistantChanged = false;
    m_currentAssistantIndex = -1;
    m_currentPointName.reset();
    m_currentPointOffset = TPointD();
    m_currentAssistantBackup.reset();

    // deselect points
    if (Closer closer = read(ModeImage)) {
      for(TMetaObjectListCW::iterator i = (*m_reader)->begin(); i != (*m_reader)->end(); ++i)
        if (*i)
          if (const TAssistantBase *assistant = (*i)->getHandler<TAssistantBase>())
            assistant->deselectAll();
      if (updateHistory) putHistory(m_readImage, -1);
    }

    if (updateOptionsBox) this->updateOptionsBox();
  }

  bool chooseAssistant(int index) {
    resetCurrentPoint(false);
    if (index >= 0)
    if (Closer closer = read(ModeImage)) {
      m_currentImage.set(m_readImage);
      if (index < (*m_reader)->size())
      if (const TMetaObjectPC &obj = (**m_reader)[index])
      if (const TAssistantBase *assistant = obj->getHandler<TAssistantBase>()) {
        assistant->deselectAll();
        m_currentAssistant.set(obj);
        m_currentAssistantIndex = index;
        m_currentPointName = assistant->getBasePoint().name;
        assistant->selectAll();
      }
      putHistory(m_readImage, m_currentAssistantIndex);
    }
    this->updateOptionsBox();
    return m_currentAssistantIndex >= 0;
  }
  
  bool findCurrentPoint(const TPointD &position, double pixelSize = 1, bool updateOptionsBox = true) {
    resetCurrentPoint(false);
    if (Closer closer = read(ModeImage)) {
      m_currentImage.set(m_readImage);
      for(TMetaObjectListCW::iterator i = (*m_reader)->begin(); i != (*m_reader)->end(); ++i) {
        if (!*i) continue;

        const TAssistantBase *assistant = (*i)->getHandler<TAssistantBase>();
        if (!assistant) continue;

        assistant->deselectAll();

        // last points is less significant and don't affecting the first points
        // so iterate points in reverse order to avoid unsolvable points overlapping
        const TAssistantPointOrder &points = assistant->pointsOrder();
        for( TAssistantPointOrder::const_reverse_iterator j = points.rbegin();
             j != points.rend() && m_currentAssistantIndex < 0;
             ++j )
        {
          const TAssistantPoint &p = **j;
          TPointD offset = p.position - position;
          if (p.visible && norm2(offset) <= p.radius*p.radius*pixelSize*pixelSize) {
            m_currentAssistant.set(*i);
            m_currentAssistantIndex = i - (*m_reader)->begin();
            m_currentPointName = p.name;
            m_currentPointOffset = offset;
            assistant->selectAll();
          }
        }
      }
      putHistory(m_readImage, m_currentAssistantIndex);
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
          getCurrentFid(),
          m_isFrameCreated,
          m_isLevelCreated,
          m_currentAssistantCreated,
          false,
          m_currentAssistantIndex,
          m_writeObject,
          m_currentAssistantBackup ));
        m_currentAssistantCreated = false;
        m_currentAssistantChanged = false;
        m_isFrameCreated = false;
        m_isLevelCreated = false;
        success = true;
      }
    }

    if (success) {
      notifyImageChanged();
      getApplication()->getCurrentTool()->notifyToolChanged();
      TTool::getApplication()->getCurrentSelection()->setSelection( getSelection() );
      getViewer()->GLInvalidateAll();
    }

    return success;
  }
  
  int readReplicatorIndex(TMetaImage::Reader &reader) {
    int cnt = (int)reader->size();
    int index = 0;
    for(int i = 0; i < cnt; ++i) {
      if (const TMetaObjectPC &obj = (*reader)[i])
      if (obj->getHandler<TReplicator>()) {
        ++index;
        if (m_currentAssistantIndex == i)
          return index;
      }
    }
    return 0;
  }

  void writeReplicatorIndex(int index) {
    apply();
    
    int wantIndex = index;
    int oldPos = -1;
    int newPos = -1;
    bool changed = false;
    
    if (Closer closer = write(ModeAssistant)) {
      if (index < 1)
        index = 1;
      
      int idx = 0;
      int lastPos = -1;
      
      TMetaObjectList &list = **m_writer;
      
      int cnt = (int)list.size();
      for(int i = 0; i < cnt; ++i) {
        if (list[i] && list[i]->getHandler<TReplicator>()) {
          ++idx;
          if (m_currentAssistantIndex == i) oldPos = i;
          if (idx == index) newPos = i;
          lastPos = i;
        }
      }
      
      if (oldPos >= 0 && lastPos >= 0) {
        assert(idx);
        if (newPos < 0)
          { index = idx; newPos = lastPos; }
        if (oldPos != newPos) {
          TMetaObjectP obj = list[oldPos];
          list.erase(list.begin() + oldPos);
          list.insert(list.begin() + newPos, obj);
          
          TUndoManager::manager()->add(new EditAssistantsReorderUndo(
            getApplication()->getCurrentLevel()->getLevel()->getSimpleLevel(),
            getCurrentFid(),
            oldPos,
            newPos ));
          
          changed = true;
        }
      }
    }
    
    if (changed) {
      m_currentAssistantIndex = newPos;
      invalidate();
    }
    
    if (wantIndex != index)
      this->updateOptionsBox();
  }
  
public:
  void deselect()
    { resetCurrentPoint(true, false); }
  
  bool isSelected()
    { return read(ModeAssistant); }
  
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
    TTool::getApplication()->getCurrentSelection()->setSelection( getSelection() );
    getViewer()->GLInvalidateAll();
    return success;
  }
  
  bool preLeftButtonDown() override {
    if (m_assistantType.getIndex() != 0) touchImage();
    TTool::getApplication()->getCurrentSelection()->setSelection( getSelection() );
    return true;
  }

  void leftButtonDown(const TPointD &position, const TMouseEvent &event) override {
    apply();
    m_dragging = true;
    m_dragAllPoints = false;
    if (m_newAssisnantType) {
      // create assistant
      resetCurrentPoint(false);
      if (Closer closer = write(ModeImage)) {
        TMetaObjectP object(new TMetaObject(m_newAssisnantType));
        if (TAssistantBase *assistant = object->getHandler<TAssistantBase>()) {
          assistant->setDefaults();
          assistant->move(position);
          assistant->selectAll();
          m_currentImage.set(m_writeImage);
          m_dragAllPoints = true;
          m_currentAssistantCreated = true;
          m_currentAssistantChanged = true;
          m_currentAssistantIndex = (int)(*m_writer)->size();
          m_currentAssistant = object;
          m_currentPointName = assistant->getBasePoint().name;
          m_currentPointOffset = TPointD();
          m_currentAssistantBackup = assistant->data();
          (*m_writer)->push_back(object);
        }
      }
      updateOptionsBox();
      m_newAssisnantType.reset();
    } else {
      findCurrentPoint(position, getViewer()->getPixelSize());
      if (event.isShiftPressed())
        if (Closer closer = read(ModePoint)) {
          m_currentPointName = m_readAssistant->getBasePoint().name;
          m_currentPointOffset = m_readAssistant->getBasePoint().position - position;
          m_dragAllPoints = true;
        }
    }

    m_currentPosition = position;
    getViewer()->GLInvalidateAll();
  }

  void leftButtonDrag(const TPointD &position, const TMouseEvent&) override {
    if (m_dragAllPoints) {
      if (Closer closer = write(ModeAssistant))
        if (m_writeAssistant->move(position + m_currentPointOffset))
          touch();
    } else {
      if (Closer closer = write(ModePoint))
        if (m_writeAssistant->movePoint(m_currentPointName, position + m_currentPointOffset))
          touch();
    }
    m_currentPosition = position;
    getViewer()->GLInvalidateAll();
  }

  void leftButtonUp(const TPointD &position, const TMouseEvent&) override {
    if (m_dragAllPoints) {
      if (Closer closer = write(ModeAssistant))
        if (m_writeAssistant->move(position + m_currentPointOffset))
          touch();
    } else {
      if (Closer closer = write(ModePoint))
        if (m_writeAssistant->movePoint(m_currentPointName, position + m_currentPointOffset))
          touch();
    }

    apply();
    m_assistantType.setIndex(0);
    getApplication()->getCurrentTool()->notifyToolChanged();
    TTool::getApplication()->getCurrentSelection()->setSelection( getSelection() );
    m_currentPosition = position;
    getViewer()->GLInvalidateAll();
    m_dragAllPoints = false;
    m_dragging = false;
  }

  void mouseMove(const TPointD &position, const TMouseEvent&) override {
    m_currentPosition = position;
    m_currentPointOffset = TPointD();
    getViewer()->GLInvalidateAll();
  }

  void draw() override {
    m_currentGuidelines.clear();
    TPointD position = m_currentPosition + m_currentPointOffset;
  
    // get current column color
    TPixelD color = TAssistantBase::colorBase;
    if (TApplication *app = getApplication())
    if (TXsheetHandle *XsheetHandle = app->getCurrentXsheet())
    if (TXsheet *Xsheet = XsheetHandle->getXsheet())
    if (TColumnHandle *columnHandle = app->getCurrentColumn())
    if (TXshColumn *column = Xsheet->getColumn( columnHandle->getColumnIndex() ))
    {
      if (int filterId = column->getColorFilterId())
      if (TSceneHandle *sceneHandle = app->getCurrentScene())
      if (ToonzScene *scene = sceneHandle->getScene())
      if (TSceneProperties *props = scene->getProperties())
        color = toPixelD(props->getColorFilterColor( filterId ));
      color.m *= column->getOpacity()/255.0;
    }

    // draw assistants
    int index = 0;
    if (Closer closer = read(ModeImage)) {
      TPixelD origColor = TAssistantBase::colorBase;
      TAssistantBase::colorBase = color;
      for(TMetaObjectListCW::iterator i = (*m_reader)->begin(); i != (*m_reader)->end(); ++i) {
        if (*i)
        if (const TAssistantBase *base = (*i)->getHandler<TAssistantBase>())
        {
          if (dynamic_cast<const TReplicator*>(base)) {
            ++index;
            base->drawEdit(getViewer(), index);
          } else {
            base->drawEdit(getViewer());
          }
          
          if (const TAssistant *assistant = dynamic_cast<const TAssistant*>(base))
            assistant->getGuidelines(
              position,
              TAffine(),
              color,
              m_currentGuidelines );
        }
      }
      TAssistantBase::colorBase = origColor;
    }
    
    TImage *img = getImage(false);
    
    // draw assistans and guidelines from other layers
    TAssistant::scanAssistants(
      this,          // tool
      &position, 1,  // pointer positions
      &m_currentGuidelines, // out guidelines
      true,          // draw
      false,         // enabled only
      false,         // mark enabled
      true,          // draw guidelines
      img );         // skip image
    
    // draw replicators from other layers
    TReplicator::scanReplicators(
      this,          // tool
      nullptr,       // in/out points
      nullptr,       // out modifiers
      true,          // draw
      false,         // enabled only
      false,         // mark enabled
      false,         // draw points
      img );         // skip image
    
    // draw replicator points
    m_replicatorPoints.clear();
    m_replicatorPoints.push_back(position);
    TReplicator::scanReplicators(
      this,          // tool
      &m_replicatorPoints, // in/out points
      nullptr,       // out modifiers
      false,         // draw
      false,         // enabled only
      false,         // mark enabled
      true,          // draw points
      nullptr );     // skip image
  }
};

//-------------------------------------------------------------------

EditAssistantsTool editAssistantsTool;
