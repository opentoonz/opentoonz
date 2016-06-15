

#include "filebrowserpopup.h"
#include "filebrowser.h"
#include "tlevel_io.h"
#include "tw/button.h"
#include "tw/textfield.h"
#include "tw/label.h"
#include "tw/colors.h"
#include "tw/mainshell.h"

#include "tw/message.h"

using namespace TwConsts;

//==============================================================================

class FileBrowserPopup::Data {
public:
  Data() : m_okAction(0) {}

  FileBrowserPopup *m_popup;

  FileBrowser *m_fileBrowser;
  TButton *m_okButton;
  TButton *m_cancelButton;
  TLabel *m_fileLabel;
  TTextField *m_file;

  TFilePath m_currentFile;

  TGenericFileBrowserPopupAction *m_okAction;

  void initPopup(const vector<string> &fileTypes);

  void onOk();

  void onFileSelChange(const TFilePath &fp) {
    m_currentFile = fp;
    m_file->setText(fp.withoutParentDir().getWideString());
  }
};

//                , m_importTrayH(120)

//-------------------------------------------------------------------

void FileBrowserPopup::Data::initPopup(const vector<string> &fileTypes) {
  m_fileBrowser = new FileBrowser(m_popup, "fileBrowser", fileTypes);
  m_fileBrowser->setBackgroundColor(Gray240);

  m_fileBrowser->setFileSelChangeAction(
      new FileBrowserAction<FileBrowserPopup::Data>(
          this, &FileBrowserPopup::Data::onFileSelChange));

  m_okButton     = new TButton(m_popup, "Ok");
  m_cancelButton = new TButton(m_popup, "Cancel");

  tconnect<TPopup>(*(m_cancelButton), m_popup, &TPopup::close);
  tconnect<FileBrowserPopup::Data>(*(m_okButton), this,
                                   &FileBrowserPopup::Data::onOk);

  m_fileLabel = new TLabel(m_popup, "File");
  m_file      = new TTextField(m_popup);
}

//-------------------------------------------------------------------

void FileBrowserPopup::Data::onOk() {
  assert(m_okAction);
  string file = toString(m_file->getText());
  if (file != "") {
    try {
      m_okAction->sendCommand(m_fileBrowser->getCurrentDir() + file);
      m_popup->close();
    } catch (TException &e) {
      TMessage::error(toString(e.getMessage()));
    }
  }
}

//-------------------------------------------------------------------

//-------------------------------------------------------------------

FileBrowserPopup::FileBrowserPopup(TWidget *parent)
    : TPopup(parent, "fileBrowserPopup"), m_data(new Data()) {
  m_data->m_popup = this;

  m_isMenu = false;
  setSize(640, 400);

  vector<string> fileTypes;
  TImageReader::getSupportedFormats(fileTypes);
  TLevelReader::getSupportedFormats(fileTypes);

  m_data->initPopup(fileTypes);
}

//-------------------------------------------------------------------

FileBrowserPopup::FileBrowserPopup(TWidget *parent,
                                   const vector<string> &fileTypes)
    : TPopup(parent, "fileBrowserPopup"), m_data(new Data()) {
  m_data->m_popup = this;
  m_isMenu        = false;
  setSize(640, 400);
  m_data->initPopup(fileTypes);
}

//-------------------------------------------------------------------

FileBrowserPopup::~FileBrowserPopup() { delete m_data; }

//-------------------------------------------------------------------

TDimension FileBrowserPopup::getPreferredSize() const {
  return TDimension(640, 400);
}

//-------------------------------------------------------------------

void FileBrowserPopup::configureNotify(const TDimension &d) {
  int y = 4;

  m_data->m_fileLabel->setGeometry(10 /*100*/, y, 60 /*150*/, y + 20);
  m_data->m_file->setGeometry(65 /*155*/, y, d.lx - 220, y + 20);

  m_data->m_okButton->setGeometry(d.lx - 200, y, d.lx - 150, y + 20);
  m_data->m_cancelButton->setGeometry(d.lx - 130, y, d.lx - 80, y + 20);

  y += 20 + 4;

  int x = 200;
  // m_importTray->setGeometry(x,y,d.lx-1,y+m_importTrayH-4);

  // y = y + (m_importTrayH + 8);
  m_data->m_fileBrowser->setGeometry(0, y, d.lx - 1, d.ly - 10);
  invalidate();
}

//-------------------------------------------------------------------

void FileBrowserPopup::popup(const TPoint &p) {
  m_data->m_currentFile = TFilePath();
  TPopup::popup(p);
  setCaption("File Browser");
}

//-------------------------------------------------------------------

void FileBrowserPopup::draw() {
  int y = 10 + 20 + 4 /*+ m_importTrayH*/;
  setColor(Gray150);
  draw3DRect(TRect(TPoint(0, y), TDimension(getSize().lx - 1, 6)));
}

//-------------------------------------------------------------------

void FileBrowserPopup::setOkAction(TGenericFileBrowserPopupAction *action) {
  m_data->m_okAction = action;
}

//-------------------------------------------------------------------

void FileBrowserPopup::setCurrentDir(const TFilePath &dirPath) {
  m_data->m_fileBrowser->setCurrentDir(dirPath);
}
//-------------------------------------------------------------------

void FileBrowserPopup::setFilter(const vector<string> &fileTypes) {
  m_data->m_fileBrowser->setFilter(fileTypes);
}
