#pragma once

#ifndef VECTORIZERPOPUP_H
#define VECTORIZERPOPUP_H

#include <memory>

// TnzCore includes
#include "tvectorimage.h"

// TnzLib includes
#include "toonz/txshsimplelevel.h"
#include "toonz/tcenterlinevectorizer.h"

// TnzQt includes
#include "toonzqt/dvdialog.h"

// Qt includes
#include <QWidget>
#include <QThread>

//====================================================

// Forward declarations

class QComboBox;
class QPushButton;
class QLabel;
class QSlider;
class QShowEvent;
class QHideEvent;
class ProgressDialog;
class TXshSimpleLevel;
class Vectorizer;
class TSceneHandle;
class OverwriteDialog;
class VectorizerSwatchArea;

namespace DVGui {
class IntField;
class MeasuredDoubleLineEdit;
class ColorField;
class CheckBox;
}  // namespace DVGui

//====================================================

//*****************************************************************************
//    VectorizerPopup declaration
//*****************************************************************************

/*!
  \brief    The "Convert-To-Vector Settings" popup window.

  \details  Vectorization interface is segmented among 3 main classes: \p
  VectorizerPopup,
            \p Vectorizer, and \p VectorizerCore. \p VectorizerPopup represents
  the
            outermost layer, providing a suitable user interface to choose
  between
            available vectorization modes and options, and displays a progress
  dialog
            to inform about the status of the underlying process.

            In order to launch a suitable vectorization process, an Xsheet or
  scene
            cast selection must be active; then the \p show() method should be
  called to
            let the user choose the preferred vectorization options. Each
  parameter
            set is passed down to a \p VectorizerConfiguration class, so see
  there for
            more detailed explanations. The \p apply() method can be
  alternatively
            called for default options, avoiding user interaction.

            During the process, the progress bar dialog is set on modal
  behaviour, in
            order to prevent the user from modifying or removing the frames to
  be
            vectorized.

  \sa       The \p VectorizerConfiguration, \p Vectorizer and \p VectorizerCore
  classes.
*/

class VectorizerPopup final : public DVGui::Dialog {
  Q_OBJECT

public:
#if QT_VERSION >= 0x050500
  VectorizerPopup(QWidget *parent = 0, Qt::WindowFlags flags = Qt::Tool);
#else
  VectorizerPopup(QWidget *parent = 0, Qt::WFlags flags = Qt::Tool);
#endif

  bool apply();  //!< The main vectorization method - prepares input
                 //!  configuration and runs the vectorization thread.
  bool isLevelToConvert(TXshSimpleLevel *level);  //!< Verifies if specified
                                                  //! level supports
  //! vectorization.

signals:

  void valuesChanged();

public slots:

  void onOk();  //!< Invokes \p apply(), and closes the popup once done.

private:
  QComboBox *m_typeMenu;
  QPushButton *m_okBtn;
  QFrame *m_paramsWidget;
  QGridLayout *m_paramsLayout;

  // Centerline Options

  QLabel *m_cThresholdLabel;
  DVGui::IntField *m_cThreshold;

  QLabel *m_cAccuracyLabel;
  DVGui::IntField *m_cAccuracy;

  QLabel *m_cDespecklingLabel;
  DVGui::IntField *m_cDespeckling;

  QLabel *m_cMaxThicknessLabel;
  DVGui::IntField *m_cMaxThickness;

  QLabel *m_cThicknessRatioLabel;
  // DVGui::IntField*    m_cThicknessRatio;

  QLabel *m_cThicknessRatioFirstLabel;
  DVGui::MeasuredDoubleLineEdit *m_cThicknessRatioFirst;

  QLabel *m_cThicknessRatioLastLabel;
  DVGui::MeasuredDoubleLineEdit *m_cThicknessRatioLast;

  DVGui::CheckBox *m_cMakeFrame;

  DVGui::CheckBox *m_cPaintFill;

  DVGui::CheckBox *m_cAlignBoundaryStrokesDirection;

  DVGui::Separator *m_cNaaSourceSeparator;

  DVGui::CheckBox *m_cNaaSource;

  // Outline Options

  QLabel *m_oDespecklingLabel;
  DVGui::IntField *m_oDespeckling;

  QLabel *m_oAccuracyLabel;
  DVGui::IntField *m_oAccuracy;

  QLabel *m_oPaintFillLabel;
  DVGui::CheckBox *m_oPaintFill;

  DVGui::CheckBox *m_oAlignBoundaryStrokesDirection;

  DVGui::Separator *m_oCornersSeparator;

  QLabel *m_oAdherenceLabel;
  DVGui::IntField *m_oAdherence;

  QLabel *m_oAngleLabel;
  DVGui::IntField *m_oAngle;

  QLabel *m_oRelativeLabel;
  DVGui::IntField *m_oRelative;

  DVGui::Separator *m_oFullColorSeparator;

  QLabel *m_oMaxColorsLabel;
  DVGui::IntField *m_oMaxColors;

  QLabel *m_oTransparentColorLabel;
  DVGui::ColorField *m_oTransparentColor;

  DVGui::Separator *m_oTlvSeparator;

  QLabel *m_oToneThresholdLabel;
  DVGui::IntField *m_oToneThreshold;

  // Other stuff

  Vectorizer *m_vectorizer;

  VectorizerSwatchArea *m_swatchArea;
  DVGui::ProgressDialog *m_progressDialog;

  int m_currFrame;

  TSceneHandle *m_sceneHandle;

private:
  VectorizerParameters *getParameters() const;

  void setType(
      bool outline);  //!< Changes the window appearance depending on \p m_type.

  void loadConfiguration(
      bool isOutline);  //!< Loads VectorizerConfiguration into the popup.
  void loadRanges(int outline);

  void updateVisibility();

  void showEvent(QShowEvent *) override;
  void hideEvent(QHideEvent *) override;

private slots:

  void onTypeChange(int type);  //!< Stores the active vectorization \p type
                                //! name, and calls \p setType().

  void onFrameName(QString frameName);
  void onFrameDone(int frameCount);

  void onPartialDone(int partial, int total);  //!< Partial progress \p partial
                                               //! of \p total was done for
  //! current frame.
  void onFinished();

  void updateSceneSettings();
  void refreshPopup();

  void onValueEdited(bool isDrag = false);
  void onValueEdited(const TPixel32 &, bool isDrag) { onValueEdited(isDrag); }

  void populateVisibilityMenu();
  void visibilityToggled();

  void saveParameters();
  void loadParameters();
  void resetParameters();
};

//*****************************************************************************
//    Vectorizer declaration
//*****************************************************************************

/*!
  \brief    The thread encapsulating a core vectorization process.

  \details  The \p Vectorizer class serves as a medium between Xsheets and the
  low-level
            vectorization methods: each frame of the input level is processed by
            \p VectorizerCore methods and the output is then stored on free
  xsheet cells.

            Signals are also excanghed through this class, ensuring basic
  communications
            between the invoker (typically \p VectorizerPopup) and the \p
  VectorizerCore
            class.

            Since vectorization is typically a complex and resource-consuming
  task,
            \p Vectorizer also inherits a \p QThread in order to avoid system
  freezes.

  \sa       The \p VectorizerPopup, \p VectorizerConfiguration and \p
  VectorizerCore
            classes.
*/

class Vectorizer final : public QThread {
  Q_OBJECT

public:
  Vectorizer();
  ~Vectorizer();

  TXshSimpleLevel *getVectorizedLevel() const { return m_vLevel.getPointer(); }

  void setLevel(const TXshSimpleLevelP &level);  //!< Stores current level and
                                                 //! creates a new vector level.

  void setParameters(
      const VectorizerParameters &params)  //!  Stores vectorizer parameters.
  {
    m_params = params;
  }

  void setFids(const std::vector<TFrameId>
                   &fids)  //!  Stores the frame ids to be processed.
  {
    m_fids = fids, std::sort(m_fids.begin(), m_fids.end());
  }

  bool isCanceled() {
    return m_isCanceled;
  }  //!< Tells whether vectorization was canceled.

signals:

  //! Frame name is emitted at beginning of a frame vectorization
  //! to be displayed above the progress bar
  void frameName(QString);
  void frameDone(int);

  //! Forwards the VectorizerCore::partialDone(int,int) signal.
  void partialDone(int, int);

  //! Transmits a user cancel downward to VectorizerCore.
  void transmitCancel();

  void configurationChanged();

public slots:

  void cancel() { m_isCanceled = true; }

protected:
  /*! \details  Remember that \b each of the above \a set methods \b must
          be called before invoking run().                                    */

  void run() override;  //!< Starts the vectorization thread (see QThread
                        //! documentation).

private:
  TXshSimpleLevelP m_level;  //!< Input level to vectorize (only one level at a
                             //! time is dealt here).
  VectorizerParameters m_params;  //!< Vectorizer options to be applied

  TXshSimpleLevelP m_vLevel;     //!< Output vectorized level
  std::vector<TFrameId> m_fids;  //!< Frame ids of the input \b m_level

  std::unique_ptr<OverwriteDialog>
      m_dialog;  //!< Dialog to be shown for overwrite resolution.

  bool m_isCanceled,  //!< User cancels set this flag to true
      m_dialogShown;  //!< Whether \p m_dialog was shown for current
                      //! vectorization.

private:
  int doVectorize();  //!< Start vectorization of input frames.

  //! Makes connections to low-level partial progress signals and cancel slots,
  //! and invokes the low-level vectorization of \b img.
  TVectorImageP doVectorize(TImageP img, TPalette *palette,
                            const VectorizerConfiguration &conf);
};

#endif  // VECTORIZERPOPUP_H
