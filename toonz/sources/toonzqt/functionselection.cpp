

#include "toonzqt/functionselection.h"

#include "toonzqt/dvdialog.h"

// TnzLib includes
#include "toonz/doubleparamcmd.h"
#include "toonz/tframehandle.h"
#include "toonz/txsheetexpr.h"

// TnzBase includes
#include "tdoubleparam.h"
#include "tdoublekeyframe.h"
#include "texpression.h"

// TnzCore includes
#include "tcommon.h"
#include "tundo.h"

// Qt includes
#include <QApplication>
#include <QMimeData>
#include <QClipboard>

//=============================================================================
//
// Function clipboard operations undo
//
//-----------------------------------------------------------------------------

class KeyframesCopyUndo final : public TUndo {
  QMimeData *m_oldData, *m_newData;

public:
  KeyframesCopyUndo(const QMimeData *oldData, const QMimeData *newData)
      : m_oldData(cloneData(oldData)), m_newData(cloneData(newData)) {}
  ~KeyframesCopyUndo() {
    delete m_oldData;
    delete m_newData;
  }

  void undo() const override {
    QApplication::clipboard()->setMimeData(cloneData(m_oldData));
  }
  void redo() const override {
    QApplication::clipboard()->setMimeData(cloneData(m_newData));
  }
  int getSize() const override {
    return sizeof(*this) + sizeof(QMimeData) * 2;  // approx
  }
  QString getHistoryString() override { return QObject::tr("Copy Keyframe"); }
};

//-----------------------------------------------------------------------------

class KeyframesPasteUndo final : public TUndo {
  struct Column {
    TDoubleParam *m_param;
    std::map<int, TDoubleKeyframe> m_oldKeyframes;
    std::set<double> m_created;
  };
  std::vector<Column> m_columns;
  FunctionKeyframesData *m_data;
  double m_frame;

public:
  KeyframesPasteUndo(std::vector<TDoubleParam *> &params,
                     const FunctionKeyframesData *data, double frame)
      : m_data(dynamic_cast<FunctionKeyframesData *>(data->clone()))
      , m_frame(frame) {
    assert((int)params.size() <= data->getColumnCount());
    int columnCount = std::min((int)(params.size()), data->getColumnCount());
    m_columns.resize(columnCount);
    for (int col = 0; col < columnCount; col++) {
      TDoubleParam *param    = params[col];
      m_columns[col].m_param = param;
      param->addRef();

      const FunctionKeyframesData::Keyframes &keyframes =
          data->getKeyframes(col);
      FunctionKeyframesData::Keyframes::const_iterator it;
      for (it = keyframes.begin(); it != keyframes.end(); ++it) {
        double f = it->m_frame + frame;
        int k    = param->getClosestKeyframe(f);
        if (0 <= k && k < param->getKeyframeCount() &&
            param->keyframeIndexToFrame(k) == f)
          m_columns[col].m_oldKeyframes[k] = param->getKeyframe(k);
        else
          m_columns[col].m_created.insert(f);
      }
    }
  }
  ~KeyframesPasteUndo() {
    for (int i = 0; i < (int)m_columns.size(); i++)
      m_columns[i].m_param->release();
    delete m_data;
  }

  void undo() const override {
    int columnCount = (int)m_columns.size();
    for (int col = 0; col < columnCount; col++) {
      TDoubleParam *param = m_columns[col].m_param;
      for (std::set<double>::const_iterator it =
               m_columns[col].m_created.begin();
           it != m_columns[col].m_created.end(); ++it)
        param->deleteKeyframe(*it);
      param->setKeyframes(m_columns[col].m_oldKeyframes);
    }
  }
  void redo() const override {
    for (int col = 0; col < (int)m_columns.size(); col++) {
      m_data->setData(col, m_columns[col].m_param, m_frame);
    }
  }
  int getSize() const override {
    return sizeof(*this) + 100;  // approx
  }
  QString getHistoryString() override {
    return QObject::tr("Paste Keyframe  at Frame : %1")
        .arg(QString::number((int)m_frame + 1));
  }
};

//-----------------------------------------------------------------------------

class KeyframesDeleteUndo final : public TUndo {
public:
  struct ColumnKeyframes {
    TDoubleParam *m_param;
    std::vector<TDoubleKeyframe> m_keyframes;
  };
  struct Column {
    TDoubleParam *m_param;
    QSet<int> m_keyframes;
  };
  KeyframesDeleteUndo(const std::vector<Column> &columns) {
    m_columns.resize(columns.size());
    for (int col = 0; col < (int)m_columns.size(); col++) {
      TDoubleParam *param    = columns[col].m_param;
      m_columns[col].m_param = param;
      if (!param) continue;
      param->addRef();
      const QSet<int> &keyframes = columns[col].m_keyframes;
      for (QSet<int>::const_iterator it = keyframes.begin();
           it != keyframes.end(); ++it)
        m_columns[col].m_keyframes.push_back(param->getKeyframe(*it));
    }
  }
  ~KeyframesDeleteUndo() {
    for (int col = 0; col < (int)m_columns.size(); col++)
      m_columns[col].m_param->release();
  }

  void undo() const override {
    for (int col = 0; col < (int)m_columns.size(); col++)
      for (int i = 0; i < (int)m_columns[col].m_keyframes.size(); i++)
        m_columns[col].m_param->setKeyframe(m_columns[col].m_keyframes[i]);
  }
  void redo() const override {
    for (int col = 0; col < (int)m_columns.size(); col++)
      for (int i = 0; i < (int)m_columns[col].m_keyframes.size(); i++)
        m_columns[col].m_param->deleteKeyframe(
            m_columns[col].m_keyframes[i].m_frame);
  }
  int getSize() const override {
    return sizeof(*this) +
           sizeof(TDoubleKeyframe) * m_columns.size();  // sbagliato!
  }
  QString getHistoryString() override { return QObject::tr("Delete Keyframe"); }

private:
  std::vector<ColumnKeyframes> m_columns;
};

//-----------------------------------------------------------------------------

class KeyframesMoveUndo final : public TUndo {
public:
  KeyframesMoveUndo() {}
  ~KeyframesMoveUndo() {
    for (int i = 0; i < (int)m_movements.size(); i++)
      m_movements[i].m_param->release();
    m_movements.clear();
  }

  void addMovement(TDoubleParam *param, int kIndex, double frameDelta) {
    m_movements.push_back(KeyframeMovement(param, kIndex, frameDelta));
    param->addRef();
  }

  void undo() const override {
    for (int i = (int)m_movements.size() - 1; i >= 0; i--) {
      TDoubleKeyframe kf =
          m_movements[i].m_param->getKeyframe(m_movements[i].m_kIndex);
      kf.m_frame -= m_movements[i].m_frameDelta;
      m_movements[i].m_param->setKeyframe(m_movements[i].m_kIndex, kf);
    }
  }
  void redo() const override {
    for (int i = 0; i < (int)m_movements.size(); i++) {
      TDoubleKeyframe kf =
          m_movements[i].m_param->getKeyframe(m_movements[i].m_kIndex);
      kf.m_frame += m_movements[i].m_frameDelta;
      m_movements[i].m_param->setKeyframe(m_movements[i].m_kIndex, kf);
    }
  }
  int getSize() const override {
    return sizeof(*this) + sizeof(m_movements[0]) * m_movements.size();
  }
  int getCount() const { return (int)m_movements.size(); }
  QString getHistoryString() override { return QObject::tr("Move Keyframe"); }

private:
  struct KeyframeMovement {
    TDoubleParam *m_param;
    int m_kIndex;
    double m_frameDelta;
    KeyframeMovement(TDoubleParam *param, int kIndex, double frameDelta)
        : m_param(param), m_kIndex(kIndex), m_frameDelta(frameDelta) {}
  };
  std::vector<KeyframeMovement> m_movements;
};

//=============================================================================
//
// FunctionSelection
//
//-----------------------------------------------------------------------------

FunctionSelection::FunctionSelection()
    : m_selectedCells()
    , m_selectedKeyframes()
    , m_selectedSegment(-1)
    , m_frameHandle(0)
    , m_columnToCurveMapper(0) {}

FunctionSelection::~FunctionSelection() {
  for (int i = 0; i < m_selectedKeyframes.size(); i++)
    if (m_selectedKeyframes[i].first) m_selectedKeyframes[i].first->release();
  m_selectedKeyframes.clear();
  delete m_columnToCurveMapper;
}

void FunctionSelection::setColumnToCurveMapper(ColumnToCurveMapper *mapper) {
  if (mapper != m_columnToCurveMapper) {
    delete m_columnToCurveMapper;
    m_columnToCurveMapper = mapper;
  }
}

void FunctionSelection::selectCurve(TDoubleParam *curve) {
  if (m_selectedKeyframes.size() == 1 && m_selectedKeyframes[0].first == curve)
    return;
  curve->addRef();
  deselectAllKeyframes();
  m_selectedKeyframes.push_back(qMakePair(curve, QSet<int>()));
  m_selectedCells = QRect();
}

void FunctionSelection::deselectAllKeyframes() {
  if (getSelectedKeyframeCount() == 0) return;
  for (int i = 0; i < m_selectedKeyframes.size(); i++)
    m_selectedKeyframes[i].second.clear();
  emit selectionChanged();
}

int FunctionSelection::getCurveIndex(TDoubleParam *curve) const {
  for (int i = 0; i < m_selectedKeyframes.size(); i++)
    if (m_selectedKeyframes[i].first == curve) return i;
  return -1;
}

int FunctionSelection::touchCurveIndex(TDoubleParam *curve) {
  int i = getCurveIndex(curve);
  if (i < 0) {
    i = m_selectedKeyframes.size();
    m_selectedKeyframes.push_back(qMakePair(curve, QSet<int>()));
    curve->addRef();
  }
  return i;
}

void FunctionSelection::selectNone() {
  for (int i = 0; i < m_selectedKeyframes.size(); i++)
    if (m_selectedKeyframes[i].first) m_selectedKeyframes[i].first->release();
  m_selectedKeyframes.clear();
  m_selectedSegment = -1;
  m_selectedCells   = QRect();
  emit selectionChanged();
}

// called from FunctionSheet::selectCells
void FunctionSelection::selectCells(const QRect &selectedCells,
                                    const QList<TDoubleParam *> &curves) {
  assert(selectedCells.width() == curves.size());
  for (int i = 0; i < curves.size(); i++)
    if (curves[i]) curves[i]->addRef();
  for (int i = 0; i < m_selectedKeyframes.size(); i++)
    if (m_selectedKeyframes[i].first) m_selectedKeyframes[i].first->release();
  m_selectedKeyframes.clear();
  double r0 = selectedCells.top();
  double r1 = selectedCells.bottom();

  // Update selected keyframes
  for (int i = 0; i < curves.size(); i++) {
    TDoubleParam *curve = curves[i];
    m_selectedKeyframes.push_back(qMakePair(curve, QSet<int>()));
    if (curve)
      for (int j = 0; j < curve->getKeyframeCount(); j++) {
        double f = curve->keyframeIndexToFrame(j);
        if (r0 <= f && f <= r1) m_selectedKeyframes[i].second.insert(j);
      }
  }

  // Update selected segment
  if (curves.size() != 1)
    m_selectedSegment = -1;
  else if (!curves[0])  // curves[0] may be zero
    m_selectedSegment = -1;
  else {
    int r0 = selectedCells.top();
    int r1 = selectedCells.bottom();
    int k0 = curves[0]->getPrevKeyframe(r0);
    int k1 = curves[0]->getPrevKeyframe(r1);
    // PrevKeyFrame does NOT include itself if the specified row is keyframe

    if (k0 ==
        curves[0]->getKeyframeCount() - 1)  // select bottom of the segments
      m_selectedSegment = -1;
    else if (k0 != k1)  // select over a keyframe
    {
      // then select the segment on top in the selection
      if (curves[0]->isKeyframe(r0))
        m_selectedSegment = k0 + 1;
      else
        m_selectedSegment = k0;
    } else  // select single segment
      m_selectedSegment = k0;
  }

  m_selectedCells = selectedCells;
  makeCurrent();
  emit selectionChanged();
}

void FunctionSelection::selectCells(const QRect &selectedCells) {
  QList<TDoubleParam *> curves;
  for (int c = selectedCells.left(); c <= selectedCells.right(); c++)
    curves.append(getCurveFromColumn(c));
  selectCells(selectedCells, curves);
}

void FunctionSelection::select(TDoubleParam *curve, int k) {
  int i = touchCurveIndex(curve);
  m_selectedKeyframes[i].second.insert(k);
  double row = curve->keyframeIndexToFrame(k);
  if (row < (double)m_selectedCells.top()) m_selectedCells.setTop(floor(row));
  if (row > (double)m_selectedCells.bottom())
    m_selectedCells.setBottom(ceil(row));

  if (m_selectedSegment >= 0  // if a segment is selected
      && (m_selectedKeyframes.size() !=
              1  // and there is not a single curve selected
          || m_selectedSegment != k ||
          m_selectedSegment + 1 != k))  // or the new selected keyframe
                                        // is not a selected segment end
    m_selectedSegment = -1;             // then clear the segment selection
  makeCurrent();
  emit selectionChanged();
  m_selectedCells = QRect();
}

bool FunctionSelection::isSelected(TDoubleParam *curve, int k) const {
  int i = getCurveIndex(curve);
  if (i < 0)
    return false;
  else
    return m_selectedKeyframes[i].second.contains(k);
}

int FunctionSelection::getSelectedKeyframeCount() const {
  int count = 0;
  for (int i = 0; i < m_selectedKeyframes.size(); i++)
    count += m_selectedKeyframes[i].second.size();
  return count;
}

QPair<TDoubleParam *, int> FunctionSelection::getSelectedKeyframe(
    int index) const {
  if (index < 0) return QPair<TDoubleParam *, int>(0, -1);
  for (int i = 0; i < m_selectedKeyframes.size(); i++) {
    int count = m_selectedKeyframes[i].second.size();
    if (index < count) {
      TDoubleParam *curve          = m_selectedKeyframes[i].first;
      QSet<int>::const_iterator it = m_selectedKeyframes[i].second.begin();
      it += index;
      return QPair<TDoubleParam *, int>(curve, *it);
    }
    index -= count;
  }
  return QPair<TDoubleParam *, int>(0, -1);
}

// called from FunctionCurve::MousePressEvent() and
// NumericalColumns::SelectCells()
void FunctionSelection::selectSegment(TDoubleParam *curve, int k,
                                      QRect selectedCells) {
  if (curve == 0) return;

  // if a different curve is selected the clear the old selection
  if (m_selectedKeyframes.size() != 1 ||
      m_selectedKeyframes[0].first != curve) {
    curve->addRef();
    for (int i = 0; i < m_selectedKeyframes.size(); i++)
      if (m_selectedKeyframes[i].first) m_selectedKeyframes[i].first->release();
    m_selectedKeyframes.clear();
    m_selectedKeyframes.push_back(qMakePair(curve, QSet<int>()));
  }
  m_selectedKeyframes[0].second.clear();
  m_selectedKeyframes[0].second.insert(k);  // k is keyframe id
  m_selectedKeyframes[0].second.insert(k + 1);
  m_selectedSegment = k;
  m_selectedCells   = selectedCells;
  makeCurrent();
  emit selectionChanged();
}

bool FunctionSelection::isSegmentSelected(TDoubleParam *curve, int k) const {
  return m_selectedKeyframes.size() == 1 &&
         m_selectedKeyframes[0].first == curve && m_selectedSegment == k;
}

QPair<TDoubleParam *, int> FunctionSelection::getSelectedSegment() const {
  if (m_selectedKeyframes.size() == 1 && m_selectedSegment >= 0)
    return qMakePair(m_selectedKeyframes[0].first, m_selectedSegment);
  else
    return qMakePair<TDoubleParam *, int>(0, -1);
}

void FunctionSelection::enableCommands() {
  enableCommand(this, "MI_Copy", &FunctionSelection::doCopy);
  enableCommand(this, "MI_Paste", &FunctionSelection::doPaste);
  enableCommand(this, "MI_Cut", &FunctionSelection::doCut);
  enableCommand(this, "MI_Clear", &FunctionSelection::doDelete);
  enableCommand(this, "MI_Insert", &FunctionSelection::insertCells);

  enableCommand(this, "MI_ResetStep", &FunctionSelection::setStep1);
  enableCommand(this, "MI_Step2", &FunctionSelection::setStep2);
  enableCommand(this, "MI_Step3", &FunctionSelection::setStep3);
  enableCommand(this, "MI_Step4", &FunctionSelection::setStep4);
}
void FunctionSelection::doCopy() {
  if (isEmpty()) return;
  FunctionKeyframesData *data = new FunctionKeyframesData();
  int columnCount             = m_selectedKeyframes.size();
  data->setColumnCount(columnCount);
  for (int col = 0; col < columnCount; col++)
    data->getData(col, m_selectedKeyframes[col].first, m_selectedCells.top(),
                  m_selectedKeyframes[col].second);
  const QMimeData *oldData = QApplication::clipboard()->mimeData();
  TUndoManager::manager()->add(new KeyframesCopyUndo(oldData, data));
  QApplication::clipboard()->setMimeData(data);
}

void FunctionSelection::doPaste() {
  const FunctionKeyframesData *data =
      dynamic_cast<const FunctionKeyframesData *>(
          QApplication::clipboard()->mimeData());
  if (!data) return;
  int rowCount = data->getRowCount();
  if (rowCount <= 0) return;
  int columnCount = data->getColumnCount();
  double frame    = 0;
  int col         = 0;
  std::vector<TDoubleParam *> params;
  if (!m_selectedCells.isEmpty()) {
    col = m_selectedCells.left();
    // numeric columns
    for (int c = 0; c < columnCount; c++) {
      TDoubleParam *curve = getCurveFromColumn(col + c);
      if (curve) params.push_back(curve);
    }
    columnCount = (int)params.size();
    if (columnCount <= 0) return;
    frame = m_selectedCells.top();
    col   = m_selectedCells.left();
    selectCells(QRect(col, frame, columnCount, rowCount));
  } else {
    // curves
    // only single curve selection supported
    columnCount = 1;
    if (m_selectedKeyframes.empty()) return;
    TDoubleParam *curve = m_selectedKeyframes[0].first;
    if (curve == 0) return;
    int kIndex = *(m_selectedKeyframes[0].second.begin());
    frame      = curve->keyframeIndexToFrame(kIndex);
    params.push_back(curve);
  }

  /*--- カーブの貼り付け時に循環参照をチェックして、駄目ならアラートを返す ---*/
  for (int c = 0; c < columnCount; c++) {
    if (!data->isCircularReferenceFree(c, params[c])) {
      DVGui::warning(
          tr("There is a circular reference in the definition of the "
             "interpolation."));
      return;
    }
  }

  TUndoManager::manager()->add(new KeyframesPasteUndo(params, data, frame));
  for (int c = 0; c < columnCount; c++) data->setData(c, params[c], frame);
}

void FunctionSelection::doCut() {
  TUndoManager::manager()->beginBlock();
  doCopy();

  bool cellsSelection = !m_selectedCells.isEmpty();
  int bottomRow       = m_selectedCells.bottom();

  KeyframesMoveUndo *moveUndo = new KeyframesMoveUndo();
  for (int i = 0; i < m_selectedKeyframes.size(); i++) {
    TDoubleParam *curve = m_selectedKeyframes[i].first;
    QSet<int> &kk       = m_selectedKeyframes[i].second;
    double delta        = 0;
    if (cellsSelection) delta = -m_selectedCells.height();
    int n = curve ? curve->getKeyframeCount() : 0;
    int j = 0;
    for (int i = 0; i < n; i++) {
      if (kk.contains(i)) {
        if (i + 1 < n && kk.contains(i + 1) && !cellsSelection)
          delta += curve->keyframeIndexToFrame(i) -
                   curve->keyframeIndexToFrame(i + 1);
      } else {
        if ((cellsSelection && bottomRow <= curve->keyframeIndexToFrame(i)) ||
            (!cellsSelection && delta != 0))
          moveUndo->addMovement(curve, j, delta);
        j++;
      }
    }
  }

  doDelete();
  if (moveUndo->getCount()) {
    TUndoManager::manager()->add(moveUndo);
    moveUndo->redo();
  } else
    delete moveUndo;

  TUndoManager::manager()->endBlock();
  selectNone();
}

void FunctionSelection::doDelete() {
  if (isEmpty()) return;
  std::vector<KeyframesDeleteUndo::Column> columns;
  for (int col = 0; col < (int)m_selectedKeyframes.size(); col++) {
    KeyframesDeleteUndo::Column column;
    TDoubleParam *param = m_selectedKeyframes[col].first;
    if (!param || !param->hasKeyframes()) continue;
    column.m_param     = param;
    column.m_keyframes = m_selectedKeyframes[col].second;
    columns.push_back(column);
  }
  if (columns.empty()) return;
  KeyframesDeleteUndo *undo = new KeyframesDeleteUndo(columns);
  undo->redo();
  TUndoManager::manager()->add(undo);
  selectNone();
}

void FunctionSelection::insertCells() {
  if (isEmpty()) return;
  QRect selectedCells     = getSelectedCells();
  int frameDelta          = selectedCells.height();
  int row                 = selectedCells.top();
  KeyframesMoveUndo *undo = new KeyframesMoveUndo();
  for (int c = selectedCells.left(); c <= selectedCells.right(); c++) {
    TDoubleParam *param = getCurveFromColumn(c);
    if (param && param->hasKeyframes()) {
      // Move keyframes in reverse, so their ordering remains consistent at each
      // step
      int k = param->getKeyframeCount() - 1;
      for (; k >= 0 && param->keyframeIndexToFrame(k) >= row; --k)
        undo->addMovement(param, k, frameDelta);
    }
  }
  undo->redo();
  TUndoManager::manager()->add(undo);
}

void FunctionSelection::setStep(int step, bool inclusive) {
  if (isEmpty()) return;
  TUndoManager::manager()->beginBlock();

  int row = getSelectedCells().top();
  for (const auto &col : m_selectedKeyframes) {
    TDoubleParam *curve = col.first;
    // need to have at least one segment
    if (!curve || curve->getKeyframeCount() <= 1) continue;

    // consider the keyframe just before the top row of the selected cells
    if (inclusive) {
      int topIndex = curve->getPrevKeyframe(row);
      if (topIndex != -1 && topIndex != curve->getKeyframeCount() - 1 &&
          !col.second.contains(topIndex))
        KeyframeSetter(curve, topIndex).setStep(step);
    }

    for (const int &kIndex : col.second) {
      // ignore the last key
      if (kIndex == curve->getKeyframeCount() - 1) continue;
      KeyframeSetter(curve, kIndex).setStep(step);
    }
  }

  TUndoManager::manager()->endBlock();
}

int FunctionSelection::getCommonStep(bool inclusive) {
  if (isEmpty()) return -1;

  int step = -1;
  int row  = getSelectedCells().top();
  for (const auto &col : m_selectedKeyframes) {
    TDoubleParam *curve = col.first;
    // need to have at least one segment
    if (!curve || curve->getKeyframeCount() <= 1) continue;

    // consider the keyframe just before the top row of the selected cells
    if (inclusive) {
      int topIndex = curve->getPrevKeyframe(row);
      if (topIndex != -1 && topIndex != curve->getKeyframeCount() - 1 &&
          !col.second.contains(topIndex))
        step = curve->getKeyframe(topIndex).m_step;
    }

    for (const int &kIndex : col.second) {
      // ignore the last key
      if (kIndex == curve->getKeyframeCount() - 1) continue;
      int tmpStep = curve->getKeyframe(kIndex).m_step;
      if (step == -1)
        step = tmpStep;
      else if (step != tmpStep)
        return 0;
    }
  }
  return step;
}

void FunctionSelection::setSegmentType(TDoubleKeyframe::Type type,
                                       bool inclusive) {
  if (isEmpty()) return;
  TUndoManager::manager()->beginBlock();

  int row = getSelectedCells().top();
  for (const auto &col : m_selectedKeyframes) {
    TDoubleParam *curve = col.first;
    // need to have at least one segment
    if (!curve || curve->getKeyframeCount() <= 1) continue;

    // consider the keyframe just before the top row of the selected cells
    if (inclusive) {
      int topIndex = curve->getPrevKeyframe(row);
      if (topIndex != -1 && topIndex != curve->getKeyframeCount() - 1 &&
          !col.second.contains(topIndex))
        KeyframeSetter(curve, topIndex).setType(type);
    }

    for (const int &kIndex : col.second) {
      // ignore the last key
      if (kIndex == curve->getKeyframeCount() - 1) continue;
      KeyframeSetter(curve, kIndex).setType(type);
    }
  }

  TUndoManager::manager()->endBlock();
}

int FunctionSelection::getCommonSegmentType(bool inclusive) {
  if (isEmpty()) return -1;

  int type = -1;
  int row  = getSelectedCells().top();
  for (const auto &col : m_selectedKeyframes) {
    TDoubleParam *curve = col.first;
    // need to have at least one segment
    if (!curve || curve->getKeyframeCount() <= 1) continue;

    // consider the keyframe just before the top row of the selected cells
    if (inclusive) {
      int topIndex = curve->getPrevKeyframe(row);
      if (topIndex != -1 && topIndex != curve->getKeyframeCount() - 1 &&
          !col.second.contains(topIndex))
        type = (int)(curve->getKeyframe(topIndex).m_type);
    }

    for (const int &kIndex : col.second) {
      // ignore the last key
      if (kIndex == curve->getKeyframeCount() - 1) continue;
      int tmpType = (int)(curve->getKeyframe(kIndex).m_type);
      if (type == -1)
        type = tmpType;
      else if (type != tmpType)
        return 0;
    }
  }
  return type;
}

//=============================================================================
//
// FunctionKeyframesData
//
//-----------------------------------------------------------------------------

FunctionKeyframesData::FunctionKeyframesData() {}

FunctionKeyframesData::~FunctionKeyframesData() {}

void FunctionKeyframesData::setColumnCount(int columnCount) {
  m_keyframes.resize(columnCount);
}

void FunctionKeyframesData::getData(int columnIndex, TDoubleParam *curve,
                                    double frame, const QSet<int> &kIndices) {
  assert(0 <= columnIndex && columnIndex < (int)m_keyframes.size());
  Keyframes &keyframes = m_keyframes[columnIndex];
  keyframes.clear();
  if (!kIndices.empty()) {
    double dFrame = -frame;
    QSet<int>::const_iterator it;
    for (it = kIndices.begin(); it != kIndices.end(); ++it) {
      TDoubleKeyframe keyframe = curve->getKeyframe(*it);
      keyframe.m_frame += dFrame;
      keyframes.push_back(keyframe);
    }
  }
}

void FunctionKeyframesData::setData(int columnIndex, TDoubleParam *curve,
                                    double frame) const {
  assert(0 <= columnIndex && columnIndex < (int)m_keyframes.size());
  const Keyframes &keyframes = m_keyframes[columnIndex];
  int n                      = (int)keyframes.size();
  for (int i = 0; i < n; i++) {
    TDoubleKeyframe keyframe(keyframes[i]);
    keyframe.m_frame += frame;
    if (i == 0 || i == n - 1) keyframe.m_linkedHandles = false;
    curve->setKeyframe(keyframe);
  }
}

const FunctionKeyframesData::Keyframes &FunctionKeyframesData::getKeyframes(
    int columnIndex) const {
  assert(0 <= columnIndex && columnIndex < (int)m_keyframes.size());
  return m_keyframes[columnIndex];
}

DvMimeData *FunctionKeyframesData::clone() const {
  FunctionKeyframesData *data = new FunctionKeyframesData();
  data->m_keyframes           = m_keyframes;
  return data;
}

int FunctionKeyframesData::getRowCount() const {
  int rowCount = 0;
  for (int c = 0; c < (int)m_keyframes.size(); c++) {
    const Keyframes &keyframes = m_keyframes[c];
    if (!keyframes.empty()) {
      int row = (int)(keyframes.rbegin()->m_frame);
      if (row + 1 > rowCount) rowCount = row + 1;
    }
  }
  return rowCount;
}

/*---- カーブの貼り付け時に循環参照をチェックして、駄目ならfalseを返す ----*/
bool FunctionKeyframesData::isCircularReferenceFree(int columnIndex,
                                                    TDoubleParam *curve) const {
  const Keyframes &keyframes = m_keyframes[columnIndex];
  int n                      = (int)keyframes.size();
  // for each key frame
  for (int i = 0; i < n; i++) {
    TDoubleKeyframe keyframe(keyframes[i]);
    // only check Expression type
    if (keyframe.m_type != TDoubleKeyframe::Expression) continue;
    // check circular reference
    TExpression expr;
    expr.setGrammar(curve->getGrammar());
    expr.setText(keyframe.m_expressionText);

    if (dependsOn(expr, curve)) return false;
  }
  return true;
}
