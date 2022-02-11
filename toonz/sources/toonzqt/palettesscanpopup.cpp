

#include "palettesscanpopup.h"
#include "toonzqt/gutil.h"
#include "tsystem.h"

#include <QPushButton>
#include <QLabel>

//=============================================================================
/*! \class PalettesScanPopup
                \brief The PalettesScanPopup class provides a modal dialog to
   import
                                         palette in StudioPalette folder.

                Inherits \b Dialog.
                This object is composed of three sub-object: a \b FileField to
   choose a
                folder, a label \b QLabel, generally empty, to show palette
   folder during
                them import, and a button bar to start import or cancel command.

                The class provides a method, \b setCurrentFolder(TFilePath
   path), to set
                folder wherein import palettes.
                Protected function allows to manage palette import.
*/
PalettesScanPopup::PalettesScanPopup()
    : Dialog(0, true, true, "PalettesScan")
    , m_folderPath(TFilePath())
    , m_timerId(0) {
  setWindowTitle(tr("Search for Palettes"));
  setFixedWidth(250);

  m_field = new DVGui::FileField();
  addWidget(m_field);

  m_label = new QLabel();
  m_label->setFixedWidth(200);
  addWidget(m_label);

  QPushButton *okBtn = new QPushButton(tr("Ok"), this);
  okBtn->setDefault(true);
  QPushButton *cancelBtn = new QPushButton(tr("Cancel"), this);
  connect(okBtn, SIGNAL(clicked()), this, SLOT(onOkBtnClicked()));
  connect(cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));
  addButtonBarWidget(okBtn, cancelBtn);
}

//-----------------------------------------------------------------------------
/*! Set current folder path to \b path.
 */
void PalettesScanPopup::setCurrentFolder(TFilePath path) {
  m_folderPath = path;
}

//-----------------------------------------------------------------------------
/*! Return current folder path.
 */
TFilePath PalettesScanPopup::getCurrentFolder() { return m_folderPath; }

//-----------------------------------------------------------------------------
/*! Start palette import. Add \b FileField path to directories stack and start
                a timer.
*/
void PalettesScanPopup::onOkBtnClicked() {
  m_timerId = startTimer(3);
  TFilePathSet fps;
  std::wstring s = m_field->getPath().toStdWString();
  int i = 0, len = s.length();
  while (i < len) {
    while (i < len && (s[i] == ' ' || s[i] == '\t')) i++;
    if (i >= len) break;
    int j = i;
    while (j < len && s[j] != ',') j++;
    int k = j;
    while (k > i && (s[k - 1] == ' ' || s[k - 1] == '\t')) k--;
    std::wstring token = s.substr(i, k - i);
    fps.push_back(TFilePath(token));
    i = j + 1;
  }
  push(fps);
}

//-----------------------------------------------------------------------------
/*! Set label text to path \b fp.
 */
void PalettesScanPopup::setLabel(const TFilePath &fp) {
  QString elideStr =
      elideText(toQString(fp), m_label->font(), m_label->width());
  m_label->setText(elideStr);
}

//-----------------------------------------------------------------------------
/*! Proceed in palette import recalling \b step(); if step() return false
                stop timer, clear directories stack, and close popup sending
   accept event.
*/
void PalettesScanPopup::timerEvent(QTimerEvent *event) {
  bool ret = step();
  if (!ret) {
    killTimer(m_timerId);
    clearStack();
    QDialog::accept();
  }
}

//-----------------------------------------------------------------------------
/*! Push TFilePath \b fp to the top of directories stack. Set label text to fp.
 */
void PalettesScanPopup::push(const TFilePath &fp) {
  setLabel(fp);
  Directory *dir = new Directory();
  m_stack.push_back(dir);
  dir->m_path  = fp;
  dir->m_files = TSystem::readDirectory(fp);
  dir->m_it    = dir->m_files.begin();
}

//-----------------------------------------------------------------------------
/*! Push TFilePathSet \b fs to the top of directories stack. Set label text to
                "<files>".
*/
void PalettesScanPopup::push(const TFilePathSet &fs) {
  m_label->setText(tr("<files>"));
  Directory *dir = new Directory();
  m_stack.push_back(dir);
  dir->m_path  = TFilePath();
  dir->m_files = fs;
  dir->m_it    = dir->m_files.begin();
}

//-----------------------------------------------------------------------------
/*! Removes the top item from the stack. If stack is empty return immediately.
 */
void PalettesScanPopup::pop() {
  if (m_stack.empty()) return;
  Directory *dir = m_stack.back();
  delete dir;
  m_stack.pop_back();
  if (!m_stack.empty()) {
    dir = m_stack.back();
    setLabel(dir->m_path);
  } else
    m_label->setText(tr(""));
}

//-----------------------------------------------------------------------------
/*! Return true if directories stack is not empty; otherwise return false.
\n	If stack is not empty check current directory:
\li	if all files are checked recall \b pop();
\li	if its path is a directory recall \b push(const TFilePath &fp);
\li	if its path is a file check its extension and recall \b onPlt(const
TFilePath &fp).
*/
bool PalettesScanPopup::step() {
  if (m_stack.empty()) return false;
  Directory *dir = m_stack.back();
  if (dir->m_it == dir->m_files.end())
    pop();
  else {
    TFilePath fp = *(dir->m_it)++;
    if (TFileStatus(fp).isDirectory())
      push(fp);
    else {
      setLabel(fp);
      std::string ext = fp.getType();
      if (ext == "plt" || ext == "tpl" || ext == "pli") onPlt(fp);
    }
  }
  return true;
}

//-----------------------------------------------------------------------------
/*! Resets the content of the directories stack and set label text empty.
 */
void PalettesScanPopup::clearStack() {
  for (int i = 0; i < (int)m_stack.size(); i++) delete m_stack[i];
  m_stack.clear();
  m_label->setText(tr(""));
}

//-----------------------------------------------------------------------------
/*! Import palette, defined in path \b fp, in current \b StudioPalette folder.
 */
void PalettesScanPopup::onPlt(const TFilePath &fp) {
  TFilePath root(m_field->getPath().toStdString());
  assert(root.isAncestorOf(fp));
  TFilePath q = fp.getParentDir() - root;
  try {
    StudioPalette::instance()->importPalette(m_folderPath + q, fp);
  } catch (TSystemException &se) {
    DVGui::warning(QString::fromStdWString(se.getMessage()));
  } catch (...) {
    DVGui::warning(QString::fromStdWString(fp.getWideString() + L"\n") +
                   tr("Failed to import palette."));
  }
}
