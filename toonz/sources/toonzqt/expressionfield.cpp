

#include "toonzqt/expressionfield.h"

#include "texpression.h"
#include "tparser.h"
#include "tconvert.h"
#include <QSyntaxHighlighter>
#include <QCompleter>
#include <QKeyEvent>
#include <QAbstractItemView>
#include <QScrollBar>
#include <QStringListModel>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QToolTip>
#include <QListView>
#include <QLabel>

using namespace DVGui;
using namespace TSyntax;

//=============================================================================

class ExpressionField::SyntaxHighlighter final : public QSyntaxHighlighter {
  const Grammar *m_grammar;

public:
  bool m_open;
  SyntaxHighlighter(QTextDocument *parent)
      : QSyntaxHighlighter(parent), m_grammar(0), m_open(true) {}
  ~SyntaxHighlighter() {}

  void setGrammar(const Grammar *grammar) { m_grammar = grammar; }

  void highlightBlock(const QString &text) override {
    Parser parser(m_grammar);
    std::vector<SyntaxToken> tokens;
    Parser::SyntaxStatus status =
        parser.checkSyntax(tokens, text.toStdString());

    int nextPos = 0;
    for (int i = 0; i < (int)tokens.size(); i++) {
      QTextCharFormat fmt;
      int pos    = tokens[i].m_pos;
      int length = tokens[i].m_length;
      int type   = tokens[i].m_type;
      nextPos    = pos + length;
      switch (type) {
      case TSyntax::Unknown:
        fmt.setForeground(Qt::black);
        break;
      case TSyntax::Number:
        fmt.setForeground(QColor(0x50, 0x7d, 0x0));
        break;  // number
      case TSyntax::Constant:
        fmt.setForeground(QColor(0x50, 0x7d, 0x0));
        break;  // constant
      case TSyntax::Variable:
        fmt.setForeground(QColor(0x0, 0x88, 0xc8));
        break;  // var
      case TSyntax::Operator:
        fmt.setForeground(QColor(50, 0, 255));
        break;  // infix
      case TSyntax::Parenthesis:
        fmt.setForeground(QColor(50, 50, 255));
        break;  // braket
      case TSyntax::Function:
        fmt.setForeground(QColor(0x0, 0x50, 0x7d));
        break;  // fname
      case TSyntax::Comma:
        fmt.setForeground(QColor(50, 20, 255));
        break;  // f ;

      case TSyntax::UnexpectedToken:
        fmt.setForeground(QColor(0xdc, 0x0, 0x0));
        break;  // expression not found
      case TSyntax::Eos:
        fmt.setForeground(QColor(255, 127, 0));
        break;  //  EOS
      case TSyntax::Mismatch:
        fmt.setForeground(QColor(255, 0, 0));
        break;  // token mismatch

      default:
        fmt.setForeground(QColor(127, 127, 255));
        break;
      }
      if (type == 4) fmt.setToolTip("Infix");
      if (length == 0) length = 1;
      setFormat(pos, length, fmt);
    }
  }
};

//=============================================================================

class MyListView final : public QListView {
  QLabel *m_tooltip;

public:
  MyListView() : QListView() {
    setObjectName("SuggestionPopup");
    setStyleSheet(
        "#SuggestionPopup {background-color:#FFFFFF; border:1px solid black; "
        "color: black;}");
    setWindowFlags(Qt::Popup);
    setMouseTracking(true);
    m_tooltip = new QLabel(0, Qt::ToolTip);
    // Stesso stile del popuop che lo contiene.
    m_tooltip->hide();

    m_tooltip->setObjectName("helpTooltip");
    m_tooltip->setAlignment(Qt::AlignLeft);
    m_tooltip->setIndent(1);
    m_tooltip->setWordWrap(false);
  }
  void showEvent(QShowEvent *) override { showToolTip(currentIndex()); }
  void hideEvent(QHideEvent *) override { m_tooltip->hide(); }
  void currentChanged(const QModelIndex &current,
                      const QModelIndex &previous) override {
    showToolTip(current);
    QListView::currentChanged(current, previous);
  }
  void showToolTip(const QModelIndex &index) {
    if (!index.isValid()) {
      m_tooltip->hide();
      return;
    }
    QVariant data = model()->data(index, Qt::ToolTipRole);
    if (!data.isValid()) {
      m_tooltip->hide();
      return;
    }
    QRect rect = visualRect(index);
    m_tooltip->setText(data.toString());
    QPoint pos = viewport()->mapToGlobal(
        QPoint(-m_tooltip->sizeHint().width(), rect.top()));
    m_tooltip->setGeometry(QRect(pos, m_tooltip->sizeHint()));
    m_tooltip->show();
  }

protected:
  void resizeEvent(QResizeEvent *e) override {
    QListView::resizeEvent(e);
    if (m_tooltip->isVisible()) showToolTip(currentIndex());
  }
};

//=============================================================================

ExpressionField::ExpressionField(QWidget *parent)
    : QTextEdit(parent)
    , m_editing(false)
    , m_grammar(0)
    , m_syntaxHighlighter(0)
    , m_completerPopup(0)
    , m_completerStartPos(0) {
  setFrameStyle(QFrame::StyledPanel);
  setObjectName("ExpressionField");
  setLineWrapMode(NoWrap);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setTabChangesFocus(true);
  // setSizePolicy(QSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed));
  connect(this, SIGNAL(textChanged()), this, SLOT(onTextChanged()));

#ifdef MACOSX
  setFixedHeight(23);
#else
  setFixedHeight(20);  // +40);
#endif

  m_completerPopup          = new MyListView();
  QStandardItemModel *model = new QStandardItemModel();
  m_completerPopup->setModel(model);
  m_completerPopup->setFocusPolicy(Qt::NoFocus);
  m_completerPopup->setFocusProxy(this);
  m_completerPopup->installEventFilter(this);
  connect(m_completerPopup, SIGNAL(clicked(const QModelIndex &)), this,
          SLOT(insertCompletion(const QModelIndex &)));

  m_syntaxHighlighter = new SyntaxHighlighter(document());
}

ExpressionField::~ExpressionField() { delete m_syntaxHighlighter; }

void ExpressionField::showEvent(QShowEvent *e) { QTextEdit::showEvent(e); }

void ExpressionField::hideEvent(QHideEvent *e) { QTextEdit::hideEvent(e); }

void ExpressionField::setExpression(std::string expression) {
  setPlainText(QString::fromStdString(expression));
}

std::string ExpressionField::getExpression() const {
  return toPlainText().toStdString();
}

bool ExpressionField::event(QEvent *e) {
  if (e->type() == QEvent::ToolTip) {
    QHelpEvent *helpEvent = static_cast<QHelpEvent *>(e);
    // openCompletionPopup();

  } else if (e->type() == QEvent::ShortcutOverride) {
    e->accept();
    return true;
  }
  // else
  return QTextEdit::event(e);
}

void ExpressionField::keyPressEvent(QKeyEvent *e) {
  // setStyleSheet("background-color: cyan");
  if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
    m_editing = false;
    emit expressionChanged();
  } else if (e->key() == Qt::Key_F10) {
    setAutoFillBackground(true);
    QPalette p = palette();
    p.setColor(QPalette::Base, Qt::cyan);
    p.setColor(QPalette::Background, Qt::cyan);
    setPalette(p);
    update();
    setStyleSheet("#ExpressionField {background-color:cyan;}");
  } else if (e->key() == Qt::Key_F11) {
    m_completerPopup->installEventFilter(this);
    QRect cr = cursorRect();
    QSize size(100, 200);
    QPoint pos = mapToGlobal(QPoint(cr.left(), cr.top() - size.height()));
    m_completerPopup->setGeometry(QRect(pos, size));
    m_completerPopup->show();
    QTextEdit::keyPressEvent(e);
  } else {
    QTextEdit::keyPressEvent(e);
    if (m_completerPopup->isVisible()) {
      updateCompleterPopup();
    } else if (Qt::Key_A <= e->key() && e->key() <= Qt::Key_Z ||
               std::string("+&|!*/=?,:-").find(e->key()) != std::string::npos) {
      openCompleterPopup();
    }
    setFocus();
  }
}

bool ExpressionField::eventFilter(QObject *obj, QEvent *e) {
  if (e->type() == QEvent::KeyPress) {
    QKeyEvent *keyEvent = static_cast<QKeyEvent *>(e);
    switch (keyEvent->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
      insertCompletion(m_completerPopup->currentIndex());
      return true;

    case Qt::Key_Left:
    case Qt::Key_Right:
      event(keyEvent);
      m_completerPopup->hide();
      return true;

    case Qt::Key_Escape:
      m_completerPopup->hide();
      return true;

    case Qt::Key_Down:
    case Qt::Key_Up:
      return false;

    default:
      event(e);
      return true;
    }
  } else if (e->type() == QEvent::MouseButtonPress) {
    m_completerPopup->hide();
    event(e);
    return true;
  } else if (e->type() == QEvent::ShortcutOverride) {
    e->accept();
    return true;
  }
  return QObject::eventFilter(obj, e);
}

void ExpressionField::onTextChanged() {
  if (!m_editing) {
    m_editing = true;

    // setStyleSheet("background: rgb(250,200,200)");
  }
}

void ExpressionField::focusInEvent(QFocusEvent *e) {
  m_syntaxHighlighter->m_open = true;
  m_syntaxHighlighter->rehighlight();
  QTextEdit::focusInEvent(e);
}

void ExpressionField::focusOutEvent(QFocusEvent *e) {
  m_syntaxHighlighter->m_open = false;
  m_syntaxHighlighter->rehighlight();
  QTextEdit::focusOutEvent(e);
}

void ExpressionField::onCursorPositionChanged() {}

void ExpressionField::openCompleterPopup() {
  int n = computeSuggestions();
  if (n < 2) return;
  if (updateCompleterPopup()) m_completerPopup->show();
}

bool ExpressionField::updateCompleterPopup() {
  int start        = m_completerStartPos;
  int pos          = textCursor().position();
  std::string text = getExpression();
  if (m_suggestions.empty() || start < 0 || start > pos ||
      pos > (int)text.length()) {
    if (m_completerPopup->isVisible()) m_completerPopup->hide();
    return false;
  }

  QStandardItemModel *model = new QStandardItemModel();
  std::string prefix        = toLower(text.substr(start, pos - start));
  int prefixLength          = prefix.length();
  int count                 = 0;
  for (int i = 0; i < (int)m_suggestions.size(); i++) {
    std::string item = m_suggestions[i].first;
    if ((int)item.length() >= prefixLength &&
        toLower(item.substr(0, prefixLength)) == prefix) {
      QStandardItem *item = new QStandardItem();
      item->setData(QString::fromStdString(m_suggestions[i].first),
                    Qt::EditRole);
      if (m_suggestions[i].second != "")
        item->setData(QString::fromStdString(m_suggestions[i].second),
                      Qt::ToolTipRole);
      model->appendRow(item);
      count++;
    }
  }
  if (count == 0) {
    if (m_completerPopup->isVisible()) m_completerPopup->hide();
    return false;
  }
  m_completerPopup->setModel(model);
  m_completerPopup->setCurrentIndex(model->index(0, 0));

  QTextCursor cursor(textCursor());
  cursor.setPosition(m_completerStartPos);
  QRect cr = cursorRect(cursor);

  int w = m_completerPopup->sizeHintForColumn(0) +
          m_completerPopup->verticalScrollBar()->sizeHint().width() + 5;
  int h =
      (m_completerPopup->sizeHintForRow(0) * qMin(7, model->rowCount()) + 3) +
      3;

  QSize size(w, h);
  QPoint popupPos = mapToGlobal(QPoint(cr.left(), cr.bottom() + 3));
  m_completerPopup->setGeometry(QRect(popupPos, size));
  return true;
}

int ExpressionField::computeSuggestions() {
  m_completerStartPos = -1;
  m_suggestions.clear();

  std::string text = getExpression();
  int pos          = textCursor().position();
  int start        = pos;
  if (start > 0) {
    start--;
    while (start > 0) {
      char c = text[start - 1];
      if (isascii(c) && isalpha(c) || c == '_' ||
          c == '.' && (start - 2 < 0 ||
                       isascii(text[start - 2]) && isalpha(text[start - 2]))) {
      } else
        break;
      start--;
    }
  }
  if (start >= (int)text.length()) return 0;
  m_completerStartPos = start;
  text                = text.substr(0, start);
  TSyntax::Parser parser(m_grammar);
  parser.getSuggestions(m_suggestions, text);
  return (int)m_suggestions.size();
}

void ExpressionField::insertCompletion() {
  if (!m_completerPopup->isVisible()) return;
  QModelIndex index = m_completerPopup->currentIndex();
  if (!index.isValid()) return;
  QString item =
      m_completerPopup->model()->data(index, Qt::EditRole).toString();
  QTextCursor tc = textCursor();
  int pos        = tc.position();
  // tc.movePosition(m_completionStartPos, QTextCursor::KeepAnchor);
  tc.insertText(item);
  m_completerPopup->hide();
}

void ExpressionField::insertCompletion(const QModelIndex &index) {
  if (index.isValid()) {
    QString item =
        m_completerPopup->model()->data(index, Qt::EditRole).toString();

    QTextCursor tc = textCursor();
    int pos        = tc.position();
    if (pos - m_completerStartPos >= 1)
      tc.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor,
                      pos - m_completerStartPos);
    tc.insertText(item);
  }
  m_completerPopup->hide();
}

void ExpressionField::setGrammar(const Grammar *grammar) {
  m_grammar = grammar;
  m_syntaxHighlighter->setGrammar(grammar);
};
