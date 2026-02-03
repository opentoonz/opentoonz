#pragma once

#ifndef TGRAMMAR_INCLUDED
#define TGRAMMAR_INCLUDED

#include <memory>

// TnzCore includes
#include "tcommon.h"

// TnzBase includes
#include "ttokenizer.h"

#undef DVAPI
#undef DVVAR
#ifdef TNZBASE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//==============================================

//    Forward declarations

class TDoubleParam;
class TUnit;

namespace TSyntax {
class Token;
class Calculator;
}  // namespace TSyntax

//==============================================

//-------------------------------------------------------------------
// Calculator & calculator nodes
//-------------------------------------------------------------------

namespace TSyntax {

class DVAPI CalculatorNodeVisitor {
public:
  CalculatorNodeVisitor() {}
  virtual ~CalculatorNodeVisitor() {}
};

//-------------------------------------------------------------------

class DVAPI CalculatorNode {
  Calculator *m_calculator;

public:
  CalculatorNode(Calculator *calculator) : m_calculator(calculator) {}
  virtual ~CalculatorNode() {}

  Calculator *getCalculator() const { return m_calculator; }

  enum { T, FRAME, RFRAME };
  virtual double compute(double vars[3]) const = 0;

  virtual void accept(CalculatorNodeVisitor &visitor) = 0;

  virtual bool hasReference() const { return false; }

private:
  // Non-copyable
  CalculatorNode(const CalculatorNode &);
  CalculatorNode &operator=(const CalculatorNode &);
};

//-------------------------------------------------------------------

class DVAPI Calculator {
  CalculatorNode *m_rootNode;  //!< (owned) Root calculator node

  TDoubleParam *m_param;  //!< (not owned) Owner of the calculator object
  const TUnit *m_unit;    //!< (not owned)

public:
  Calculator();
  virtual ~Calculator();

  void setRootNode(CalculatorNode *node);

  double compute(double t, double frame, double rframe) {
    double vars[3];
    vars[0] = t, vars[1] = frame, vars[2] = rframe;
    return m_rootNode->compute(vars);
  }

  void accept(CalculatorNodeVisitor &visitor) { m_rootNode->accept(visitor); }

  typedef double Calculator::*Variable;

  void setOwnerParameter(TDoubleParam *param) { m_param = param; }
  TDoubleParam *getOwnerParameter() const { return m_param; }

  const TUnit *getUnit() const { return m_unit; }
  void setUnit(const TUnit *unit) { m_unit = unit; }

private:
  // not copyable
  Calculator(const Calculator &);
  Calculator &operator=(const Calculator &);
};

//-------------------------------------------------------------------

class DVAPI NumberNode final : public CalculatorNode {
  double m_value;

public:
  NumberNode(Calculator *calc, double value)
      : CalculatorNode(calc), m_value(value) {}

  double compute(double vars[3]) const override { return m_value; }

  void accept(CalculatorNodeVisitor &visitor) override {}
};

//-------------------------------------------------------------------

class DVAPI VariableNode final : public CalculatorNode {
  int m_varIdx;

public:
  VariableNode(Calculator *calc, int varIdx)
      : CalculatorNode(calc), m_varIdx(varIdx) {}

  double compute(double vars[3]) const override { return vars[m_varIdx]; }

  void accept(CalculatorNodeVisitor &visitor) override {}
};

//-------------------------------------------------------------------
// Pattern
//-------------------------------------------------------------------

enum TokenType {
  Unknown = 0,

  Number,
  Constant,
  Variable,
  Operator,
  Parenthesis,
  Function,
  Comma,

  UnexpectedToken = -100,
  Eos,
  Mismatch,

  InternalError = -200
};

//-------------------------------------------------------------------

class DVAPI Pattern {
  std::string m_description;

public:
  Pattern() {}
  virtual ~Pattern() {}

  virtual std::string getFirstKeyword() const { return ""; }
  virtual void getAcceptableKeywords(std::vector<std::string> &keywords) const {
  }
  virtual int getPriority() const { return 0; }
  virtual bool expressionExpected(
      const std::vector<Token> &previousTokens) const {
    return false;
  }
  virtual bool matchToken(const std::vector<Token> &previousTokens,
                          const Token &token) const = 0;
  virtual bool isFinished(const std::vector<Token> &previousTokens,
                          const Token &token) const = 0;
  virtual bool isComplete(const std::vector<Token> &previousTokens,
                          const Token &token) const {
    return isFinished(previousTokens, token);
  }
  virtual TokenType getTokenType(
      const std::vector<Token> &previousTokens,
      const Token &token) const = 0;  // see also SyntaxToken in tparser.h

  virtual void createNode(Calculator *calc,
                          std::vector<CalculatorNode *> &stack,
                          const std::vector<Token> &tokens) const = 0;

  std::string getDescription() const { return m_description; }
  void setDescription(std::string description) { m_description = description; }

  // helper methods
  CalculatorNode *popNode(std::vector<CalculatorNode *> &stack) const;
};

//-------------------------------------------------------------------

class DVAPI Grammar {
  class Imp;
  std::unique_ptr<Imp> m_imp;

public:
  Grammar();
  ~Grammar();

  void addPattern(Pattern *pattern);  // take ownership

  enum Position { ExpressionStart, ExpressionEnd };

  // note: returns a matching pattern (or 0 if no pattern matches)
  const Pattern *getPattern(Position position, const Token &token) const;

  // returns matching <keywords, comment>
  typedef std::vector<std::pair<std::string, std::string>> Suggestions;
  void getSuggestions(Suggestions &suggetsions, Position position) const;

private:
  // not implemented
  Grammar(const Grammar &);
  Grammar &operator=(const Grammar &);
};

}  // namespace TSyntax

#endif  // TGRAMMAR_INCLUDED
