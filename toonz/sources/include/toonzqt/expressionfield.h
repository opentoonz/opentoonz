#pragma once

#ifndef EXPRESSIONFIELD_H
#define EXPRESSIONFIELD_H

#include "tgrammar.h"
#include <QWidget>
#include <QTextEdit>

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

// forward declaration
class QPushButton;
class QCompleter;
class QListView;
class QModelIndex;

//=============================================================================

namespace DVGui {

//=============================================================================
/*! \class DVGui::ExpressionField
                \brief The ExpressionField allows to edit expressions (i.e.
   formulas as '34*(5-2)')

                Inherits \b QWidget.

*/

class DVAPI ExpressionField final : public QTextEdit {
  Q_OBJECT
  bool m_editing;
  const TSyntax::Grammar *m_grammar;

  class SyntaxHighlighter;
  SyntaxHighlighter *m_syntaxHighlighter;

  QListView *m_completerPopup;
  int m_completerStartPos;
  TSyntax::Grammar::Suggestions m_suggestions;

public:
  ExpressionField(QWidget *parent = 0);
  ~ExpressionField();

  void setExpression(std::string expression);
  std::string getExpression() const;

  void setGrammar(const TSyntax::Grammar *grammar);
  const TSyntax::Grammar *getGrammar() const { return m_grammar; }

private:
  void openCompleterPopup();
  bool updateCompleterPopup();
  void insertCompletion();
  int computeSuggestions();

protected:
  bool event(QEvent *e) override;
  void keyPressEvent(QKeyEvent *e) override;
  void focusInEvent(QFocusEvent *e) override;
  void focusOutEvent(QFocusEvent *e) override;
  void showEvent(QShowEvent *) override;
  void hideEvent(QHideEvent *) override;
  bool eventFilter(QObject *obj, QEvent *e) override;

private slots:
  void onTextChanged();
  void onCursorPositionChanged();
  void insertCompletion(const QModelIndex &index);

signals:
  void expressionChanged();
};

//-----------------------------------------------------------------------------
}  // namespace DVGui
//-----------------------------------------------------------------------------

#endif  // FILEFIELD_H
