#pragma once

#ifndef FORMATSETTINGSPOPUPS_H
#define FORMATSETTINGSPOPUPS_H

// TnzQt includes
#include "toonzqt/dvdialog.h"

// TnzCore includes
#include "tfilepath.h"

// Qt includes
#include <QMap>

//==============================================================

//    Forward  declarations

class TPropertyGroup;
class QLabel;

namespace DVGui {
class PropertyWidget;
class PropertyComboBox;
}

//==============================================================

//**********************************************************************************
//    FormatSettingsPopup  definition
//**********************************************************************************

class FormatSettingsPopup final : public DVGui::Dialog {
  Q_OBJECT

public:
  FormatSettingsPopup(QWidget *parent, const std::string &format,
                      TPropertyGroup *props);

  void setLevelPath(const TFilePath &path) { m_levelPath = path; }
  void setFormatProperties(TPropertyGroup *props);

private:
  std::string m_format;
  TPropertyGroup *m_props;
  TFilePath m_levelPath;

  QMap<std::string, DVGui::PropertyWidget *>
      m_widgets;  //!< Property name -> PropertyWidget

  QGridLayout *m_mainLayout;

#ifdef _WIN32

  // AVI codec - related members
  QLabel *m_codecRestriction;
  DVGui::PropertyComboBox *m_codecComboBox;
  QPushButton *m_configureCodec;

#endif

private:
  void buildPropertyComboBox(int index, TPropertyGroup *props);
  void buildValueField(int index, TPropertyGroup *props);
  void buildPropertyCheckBox(int index, TPropertyGroup *props);
  void buildPropertyLineEdit(int index, TPropertyGroup *props);
  void showEvent(QShowEvent *se) override;

private Q_SLOTS:
  void onComboBoxIndexChanged(const QString &);
  void onAviCodecConfigure();
};

//**********************************************************************************
//    Related  global functions
//**********************************************************************************

/*!
  \details    The openFormatSettingsPopup() opens a Toonz dialog with the
  specified
              properties for a given format, <I>or alternatively opens a
  suitable
              native dialog</I>. The returned dialog relates \a specifically to
  the
              former case, while it will be \p 0 in case the opened dialog is
  native.

              The returned dialog, if any, will have the following properties
              by default:
              <UL>
                <LI><B>Automatically delete itself when closed</B>, through the
                    \p Qt::WA_DeleteOnClose attribute.</LI>
                <LI>Modal interaction.</LI>
              </UL>

  \return     The format settings popup instance opened on request, \a if the
              opened dialog is not native.

  \remark     This function \a may return \p 0 depending on the requested
  format.
*/

FormatSettingsPopup *openFormatSettingsPopup(
    QWidget *parent,            //!< Parent for the new format popup.
    const std::string &format,  //!< File extension of the displayed format.
    TPropertyGroup *props,      //!< Properties to be shown for the format.
    const TFilePath &levelPath =
        TFilePath()  //!< May be used to choose available codecs
                     //!  depending on a level's resolution.
    );               //!< Opens a suitable popup with settings
                     //!  for an input level format.

#endif  // FORMATSETTINGSPOPUPS_H
