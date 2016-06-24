#pragma once

#ifndef MESHIFYPOPUP_H
#define MESHIFYPOPUP_H

// TnzQt includes
#include "toonzqt/dvdialog.h"

// TnzLib includes
#include "toonz/txshcell.h"

// TnzCore includes
#include "timage.h"
#include "tfilepath.h"

//============================================================

//    Forward declarations

class TXshSimpleLevel;

namespace DVGui {
class MeasuredDoubleField;
class DoubleLineEdit;
class IntLineEdit;
}

class QPushButton;

//============================================================

//********************************************************************************************
//    Meshify Popup  declaration
//********************************************************************************************

/*!
  \brief    Popup window displayed when the user wants to create meshes for the
            Plastic Tool.
*/

class MeshifyPopup final : public DVGui::Dialog {
  Q_OBJECT

public:
  MeshifyPopup();

protected:
  void showEvent(QShowEvent *se) override;
  void hideEvent(QHideEvent *se) override;

  void acquirePreview();
  void updateMeshPreview();

protected slots:

  void onCellSwitched();
  void onParamsChanged(bool dragging = false);

  void apply();

private:
  class Swatch;

private:
  // GUI memebers

  DVGui::MeasuredDoubleField *m_edgesLength;  //!< Target length of mesh edges.
  DVGui::DoubleLineEdit
      *m_rasterizationDpi;  //!< Dpi used for pli and sub-xsheet rendering.
  DVGui::IntLineEdit
      *m_margin;  //!< Mesh margin to the original shape (in pixels).

  Swatch *m_viewer;  //!< Swatch viewer used to preview meshification results.
  QPushButton *m_okBtn;  //!< "Ok" button at bottom.

  // Preview-related members

  int m_r,          //!< Current xsheet row.
      m_c;          //!< Current xsheet column.
  TXshCell m_cell;  //!< Xsheet cell to be previewed (may not correspond
                    //!  to <TT>(m_r, m_c)</TT> due to redirection!).
};

#endif  // MESHIFYPOPUP_H
