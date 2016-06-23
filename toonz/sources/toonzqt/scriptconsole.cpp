

#include "toonzqt/scriptconsole.h"
#include "toonz/scriptengine.h"
#include <QKeyEvent>
#include <QTextBlock>
#include <QUrl>
#include <QMimeData>

ScriptConsole::ScriptConsole(QWidget *parent)
    : QTextEdit(parent), m_commandIndex(0) {
  setObjectName("ScriptConsole");

  m_prompt = ">> ";

  append(m_prompt);
  moveCursor(QTextCursor::EndOfLine);

  m_engine = new ScriptEngine();
  connect(m_engine, SIGNAL(evaluationDone()), this, SLOT(onEvaluationDone()));
  connect(m_engine, SIGNAL(output(int, const QString &)), this,
          SLOT(output(int, const QString &)));
  connect(this, SIGNAL(cursorPositionChanged()), this,
          SLOT(onCursorPositionChanged()));
}

ScriptConsole::~ScriptConsole() { delete m_engine; }

void ScriptConsole::keyPressEvent(QKeyEvent *e) {
  if (e->modifiers() == Qt::ControlModifier && e->key() == Qt::Key_Y) {
    if (m_engine->isEvaluating()) {
      m_engine->interrupt();
      setTextColor(QColor(255, 127, 0));
      append("Interrupt");
      moveCursor(QTextCursor::EndOfLine);
      setTextColor(Qt::black);
    }
    return;
  }

  switch (e->key()) {
  case Qt::Key_Return:
    onReturnKeyPress();
    break;
  case Qt::Key_Up:
    if (m_commandIndex > 0) {
      moveCursor(QTextCursor::End);
      moveCursor(QTextCursor::StartOfBlock);
      moveCursor(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
      // the first KeyUp save the current command, possibly not completed yet
      // note: m_currentCommand is the whole line, containing the prompt
      if (m_commandIndex == m_commands.count())
        m_currentCommand = textCursor().selectedText();
      textCursor().insertText(m_prompt + m_commands[--m_commandIndex]);
    }
    break;
  case Qt::Key_Down:
    if (m_commandIndex < m_commands.count()) {
      moveCursor(QTextCursor::End);
      moveCursor(QTextCursor::StartOfBlock);
      moveCursor(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
      if (m_commandIndex == m_commands.count() - 1) {
        // the last KeyDown insert back the current command, possibly not
        // completed yet
        textCursor().insertText(m_currentCommand);
        ++m_commandIndex;
      } else
        textCursor().insertText(m_prompt + m_commands[++m_commandIndex]);
    }
    break;
  case Qt::Key_Backspace:
  case Qt::Key_Left:
    if (textCursor().positionInBlock() > 3)
      QTextEdit::keyPressEvent(e);
    else
      e->ignore();
    break;
  default:
    QTextEdit::keyPressEvent(e);
  }
}

void ScriptConsole::onCursorPositionChanged() {
  // only the last block (i.e. the current command) is editable
  setReadOnly(textCursor().block().next().isValid());
}

bool ScriptConsole::canInsertFromMimeData(const QMimeData *source) const {
  if (source->hasText()) {
    QString text = source->text();
    if (text.contains("\n"))
      return false;
    else
      return true;
  } else if (source->hasUrls() && source->urls().length() == 1) {
    return true;
  } else
    return false;
}

void ScriptConsole::insertFromMimeData(const QMimeData *source) {
  if (canInsertFromMimeData(source)) {
    if (source->hasText())
      QTextEdit::insertFromMimeData(source);
    else if (source->hasUrls() && source->urls().length() == 1) {
      QUrl url                    = source->urls()[0];
      QString text                = url.toString();
      if (url.isLocalFile()) text = url.toLocalFile();
      text = "\"" + text.replace("\\", "\\\\").replace("\"", "\\\"") + "\"";
      textCursor().insertText(text);
    }
  }
}

void ScriptConsole::onReturnKeyPress() {
  int promptLength   = m_prompt.length();
  QTextCursor cursor = textCursor();
  cursor.movePosition(QTextCursor::StartOfLine);
  cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor,
                      promptLength);
  cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
  QString command = cursor.selectedText();

  QTextCharFormat fmt;
  fmt.setForeground(QColor(120, 120, 120));
  cursor.mergeCharFormat(fmt);
  cursor.clearSelection();

  if (command.trimmed() != "") {
    int k = m_commands.indexOf(command);
    if (k < 0)
      m_commands.append(command);
    else if (k <= m_commands.length() - 1) {
      m_commands.takeAt(k);
      m_commands.append(command);
    }
    m_commandIndex = m_commands.count();
    // m_currentCommand = m_prompt;
  }

  moveCursor(QTextCursor::EndOfLine);

  if (command.trimmed() != "") {
    append("");
    cursor.movePosition(QTextCursor::StartOfBlock);
    m_engine->evaluate(command);
  } else {
    append("");
    onEvaluationDone();
  }
}

void ScriptConsole::onEvaluationDone() {
  moveCursor(QTextCursor::End);
  setTextColor(Qt::black);
  textCursor().insertText(m_prompt);
  moveCursor(QTextCursor::EndOfLine);
}

void ScriptConsole::output(int type, const QString &value) {
  moveCursor(QTextCursor::End);
  if (type == ScriptEngine::ExecutionError ||
      type == ScriptEngine::SyntaxError) {
    setTextColor(Qt::red);
  } else if (type == ScriptEngine::UndefinedEvaluationResult ||
             type == ScriptEngine::Warning) {
    setTextColor(QColor(250, 120, 40));
  } else {
    setTextColor(QColor(10, 150, 240));
  }
  textCursor().insertText(value + "\n");
  moveCursor(QTextCursor::EndOfLine);
}

void ScriptConsole::executeCommand(const QString &cmd) {
  moveCursor(QTextCursor::End);
  setTextColor(Qt::black);
  append(m_prompt);
  moveCursor(QTextCursor::EndOfLine);
  textCursor().insertText(cmd);
  moveCursor(QTextCursor::EndOfLine);
  onReturnKeyPress();
}
