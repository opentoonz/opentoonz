

#include "tparser.h"
#include "tgrammar.h"
#include "ttokenizer.h"
#include "tutil.h"

#include <iostream>

namespace TSyntax {

//-------------------------------------------------------------------
// RunningPattern
//-------------------------------------------------------------------

class RunningPattern {
public:
  std::vector<Token> m_tokens;
  const Pattern *m_pattern;

  RunningPattern(const Pattern *pattern) : m_pattern(pattern) {}

  int getPriority() const { return m_pattern->getPriority(); }
  bool isFinished(const Token &token) const {
    return m_pattern->isFinished(m_tokens, token);
  }
  bool isComplete(const Token &token) const {
    return m_pattern->isComplete(m_tokens, token);
  }
  bool expressionExpected() const {
    return m_pattern->expressionExpected(m_tokens);
  }
  bool matchToken(const Token &token) const {
    return m_pattern->matchToken(m_tokens, token);
  }
  void advance() {
    assert(!isFinished(Token()));
    m_tokens.push_back(Token());
  }
  void advance(const Token &token) {
    assert(!isFinished(token));
    m_tokens.push_back(token);
  }
  TokenType getTokenType(const Token &token) const {
    return m_pattern->getTokenType(m_tokens, token);
  }

  void createNode(Calculator *calc, std::vector<CalculatorNode *> &stack) {
    m_pattern->createNode(calc, stack, m_tokens);
  }
};

//-------------------------------------------------------------------
// Parser::Imp
//-------------------------------------------------------------------

class Parser::Imp {
public:
  const Grammar *m_grammar;
  Tokenizer m_tokenizer;
  std::string m_errorString;
  bool m_isValid;
  Calculator *m_calculator;
  std::vector<RunningPattern> m_patternStack;
  std::vector<CalculatorNode *> m_nodeStack;
  std::vector<SyntaxToken> m_syntaxTokens;
  Grammar::Position m_position;
  // Pattern *m_lastPattern;
  bool m_hasReference;

  Imp(const Grammar *grammar)
      : m_grammar(grammar)
      , m_errorString("")
      , m_isValid(false)
      , m_calculator(0)
      , m_position(Grammar::ExpressionStart)
      , m_hasReference(false) {}
  ~Imp() {
    clearPointerContainer(m_nodeStack);
    delete m_calculator;
  }

  void pushSyntaxToken(TokenType type);
  bool parseExpression(bool checkOnly);
  void flushPatterns(int minPriority, int minIndex, bool checkOnly);
  bool checkSyntax(std::vector<SyntaxToken> &tokens);
};

//-------------------------------------------------------------------
// Parser
//-------------------------------------------------------------------

Parser::Parser(const Grammar *grammar) : m_imp(new Imp(grammar)) {}

//-------------------------------------------------------------------

Parser::~Parser() {}

//-------------------------------------------------------------------

void Parser::Imp::flushPatterns(int minPriority, int minIndex, bool checkOnly) {
  while ((int)m_patternStack.size() > minIndex &&
         m_patternStack.back().getPriority() >= minPriority) {
    if (!checkOnly) m_patternStack.back().createNode(m_calculator, m_nodeStack);
    m_patternStack.pop_back();
  }
}

//-------------------------------------------------------------------

void Parser::Imp::pushSyntaxToken(TokenType type) {
  SyntaxToken syntaxToken;
  Token token          = m_tokenizer.getToken();
  syntaxToken.m_pos    = token.getPos();
  syntaxToken.m_length = token.getPos1() - token.getPos() + 1;
  syntaxToken.m_type   = type;
  m_syntaxTokens.push_back(syntaxToken);
}

//-------------------------------------------------------------------

bool Parser::Imp::parseExpression(bool checkOnly) {
  if (!m_grammar) return false;
  int stackMinIndex          = m_patternStack.size();
  int count                  = 0;
  Grammar::Position position = Grammar::ExpressionStart;
  m_position                 = position;
  // warning: parseExpression() is called recursively. m_position is used after
  // parseExpression()
  // for diagnostic and suggestions. position is used in the actual parsing

  bool expressionExpected = true;
  while (!m_tokenizer.eos()) {
    // token, position -> pattern
    const Pattern *pattern =
        m_grammar->getPattern(position, m_tokenizer.getToken());
    if (!pattern) {
      // no pattern found for the next token
      if (position == Grammar::ExpressionEnd) {
        // already found an expression
        flushPatterns(-1, stackMinIndex, checkOnly);
        return true;
      } else {
        // syntax error : expression not found
        if (checkOnly) pushSyntaxToken(UnexpectedToken);

        m_errorString = "Unexpected token";
        return false;
      }
    }
    // pattern found: start scanning it
    RunningPattern runningPattern(pattern);
    if (position ==
        Grammar::ExpressionEnd)  // patterns of the form "<expr> ...."
      runningPattern.advance();

    // eat the first token
    if (checkOnly)
      pushSyntaxToken(runningPattern.getTokenType(m_tokenizer.getToken()));
    runningPattern.advance(m_tokenizer.getToken());
    m_tokenizer.nextToken();
    expressionExpected = false;

    while (!runningPattern.isFinished(m_tokenizer.getToken())) {
      if (runningPattern.expressionExpected()) {
        // expression expected
        runningPattern.advance();
        if (runningPattern.isFinished(m_tokenizer.getToken())) {
          // pattern ended with an expression
          expressionExpected = true;
          break;
        }
        // parse a sub expression
        if (!parseExpression(checkOnly)) {
          return false;
        }
      } else {
        // "terminal" expected
        if (m_tokenizer.eos()) {
          // EOS
          if (runningPattern.isComplete(Token())) break;
          if (checkOnly) pushSyntaxToken(Eos);

          m_errorString = "Uncompleted syntax";
          return false;
        }
        if (!runningPattern.matchToken(m_tokenizer.getToken())) {
          // token mismatch
          if (runningPattern.isComplete(m_tokenizer.getToken())) break;
          if (checkOnly) pushSyntaxToken(Mismatch);
          m_errorString = "Syntax error";
          return false;
        }
        // token matched
        if (checkOnly)
          pushSyntaxToken(runningPattern.getTokenType(m_tokenizer.getToken()));
        runningPattern.advance(m_tokenizer.getToken());
        m_tokenizer.nextToken();
      }
    }

    // pattern ended
    if (expressionExpected) {
      // pattern terminated with an expression
      if (position == Grammar::ExpressionEnd) {
        // "E op . E"
        flushPatterns(runningPattern.getPriority(), stackMinIndex, checkOnly);
        m_patternStack.push_back(runningPattern);
      } else {
        // "op . E"
        m_patternStack.push_back(runningPattern);
      }
      m_position = position = Grammar::ExpressionStart;
    } else {
      // pattern terminates with a keyword
      if (position == Grammar::ExpressionEnd) {
        // "E op ."
        flushPatterns(runningPattern.getPriority(), stackMinIndex, checkOnly);
      } else {
        // "leaf .", "f(E) .", "(E) ."
      }
      if (!checkOnly) runningPattern.createNode(m_calculator, m_nodeStack);
      count++;
      m_position = position = Grammar::ExpressionEnd;
    }
  }
  if (count == 0 || expressionExpected) {
    m_errorString = "Expression expected";
    return false;
  }
  flushPatterns(-1, stackMinIndex, checkOnly);
  return true;
}

//-------------------------------------------------------------------

Calculator *Parser::parse(std::string text) {
  m_imp->m_tokenizer.setBuffer(text);
  clearPointerContainer(m_imp->m_nodeStack);
  m_imp->m_errorString  = "";
  m_imp->m_isValid      = false;
  m_imp->m_hasReference = false;
  m_imp->m_calculator   = new Calculator();
  bool ret              = m_imp->parseExpression(false);
  if (ret && !m_imp->m_nodeStack.empty()) {
    m_imp->m_calculator->setRootNode(m_imp->m_nodeStack.back());

    for (auto node : m_imp->m_nodeStack) {
      if (node->hasReference()) {
        m_imp->m_hasReference = true;
        break;
      }
    }

    m_imp->m_nodeStack.pop_back();
    m_imp->m_isValid = true;

  } else {
    delete m_imp->m_calculator;
    m_imp->m_calculator = 0;
  }
  clearPointerContainer(m_imp->m_nodeStack);
  Calculator *calculator = m_imp->m_calculator;
  m_imp->m_calculator    = 0;
  return calculator;
}

//-------------------------------------------------------------------

Parser::SyntaxStatus Parser::checkSyntax(std::vector<SyntaxToken> &tokens,
                                         std::string text) {
  m_imp->m_tokenizer.setBuffer(text);
  if (m_imp->m_tokenizer.eos()) return Incomplete;
  bool ret = m_imp->parseExpression(true);
  tokens   = m_imp->m_syntaxTokens;
  if (ret && m_imp->m_tokenizer.eos()) return Correct;
  if (tokens.empty()) return Incomplete;
  SyntaxToken &lastToken = tokens.back();
  if (lastToken.m_type == Eos) return Incomplete;

  int k = lastToken.m_pos + lastToken.m_length;
  if (k >= (int)text.length()) {
    if (lastToken.m_type < Unknown) {
      lastToken.m_type = Unknown;
    }
    return ExtraIgnored;
  }

  SyntaxToken extraIgnored;
  extraIgnored.m_type   = Unknown;
  extraIgnored.m_pos    = k;
  extraIgnored.m_length = text.length() - k;
  tokens.push_back(extraIgnored);

  return Error;
}

//-------------------------------------------------------------------

void Parser::getSuggestions(Grammar::Suggestions &suggestions,
                            std::string text) {
  std::vector<SyntaxToken> tokens;
  Parser::SyntaxStatus status = checkSyntax(tokens, text);
  suggestions.clear();
  if (status == Correct || status == Incomplete || status == ExtraIgnored)
    m_imp->m_grammar->getSuggestions(suggestions, m_imp->m_position);
}

//-------------------------------------------------------------------

std::string Parser::getCurrentPatternString(std::string text) {
  return "ohime";
}

//-------------------------------------------------------------------

bool Parser::isValid() const { return m_imp->m_isValid; }

//-------------------------------------------------------------------

bool Parser::hasReference() const { return m_imp->m_hasReference; }

//-------------------------------------------------------------------

std::string Parser::getText() const { return m_imp->m_tokenizer.getBuffer(); }

//-------------------------------------------------------------------

std::string Parser::getError() const { return m_imp->m_errorString; }

//-------------------------------------------------------------------

std::pair<int, int> Parser::getErrorPos() const {
  if (m_imp->m_errorString == "") return std::make_pair(0, -1);
  Token token = m_imp->m_tokenizer.getToken();
  return std::make_pair(token.getPos(), token.getPos1());
}

//-------------------------------------------------------------------

}  // namespace TSyntax
