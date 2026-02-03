

// TnzCore includes
#include "trandom.h"
#include "tutil.h"
#include "tconvert.h"

// TnzBase includes
#include "ttokenizer.h"
#include "tunit.h"
#include "tparser.h"
#include "tdoubleparam.h"
#include "tdoublekeyframe.h"

// STD includes
#include <map>
#include <math.h>
#include <functional>
#include <memory>

// Qt includes
#include <QString>

#include "tgrammar.h"

static const double PI = 4 * atan(1.0);
static const double toDeg(double rad) { return rad * 180.0 / PI; }
static const double toRad(double deg) { return deg / 180.0 * PI; }

namespace TSyntax {

//===================================================================
//
// RandomManager
//
//   es. x = RandomManager::instance()->getValue(seed, frame);
//
//-------------------------------------------------------------------

class RandomSequence {
  TRandom m_rnd;
  std::vector<double> m_values;

public:
  RandomSequence(UINT seed) : m_rnd(seed) {}
  double getValue(int i) {
    assert(i >= 0);
    if (i >= (int)m_values.size()) {
      m_values.reserve(i + 1);
      while (i >= (int)m_values.size()) m_values.push_back(m_rnd.getDouble());
    }
    return m_values[i];
  }
};

//-------------------------------------------------------------------

class RandomManager {  // singleton
  typedef std::map<UINT, RandomSequence *> Table;
  Table m_table;
  RandomManager() {}

public:
  ~RandomManager() {
    for (Table::iterator i = m_table.begin(); i != m_table.end(); ++i)
      delete i->second;
  }
  static RandomManager *instance() {
    static RandomManager _instance;
    return &_instance;
  }

  RandomSequence *getSequence(UINT seed) {
    Table::iterator i = m_table.find(seed);
    if (i == m_table.end()) {
      RandomSequence *seq = new RandomSequence(seed);
      m_table[seed]       = seq;
      return seq;
    } else
      return i->second;
  }

  double getValue(UINT seed, double t) {
    RandomSequence *seq = getSequence(seed);
    return seq->getValue(t > 0 ? tfloor(t) : 0);
  }
};

//===================================================================
// Calculator
//-------------------------------------------------------------------

Calculator::Calculator() : m_rootNode(0), m_param(0), m_unit(0) {}

//-------------------------------------------------------------------

Calculator::~Calculator() { delete m_rootNode; }

//-------------------------------------------------------------------

void Calculator::setRootNode(CalculatorNode *node) {
  if (node != m_rootNode) {
    delete m_rootNode;
    m_rootNode = node;
  }
}

//===================================================================
// Nodes
//-------------------------------------------------------------------

template <class Op>
class Op0Node final : public CalculatorNode {
public:
  Op0Node(Calculator *calc) : CalculatorNode(calc) {}

  double compute(double vars[3]) const override {
    Op op;
    return op();
  }
};

//-------------------------------------------------------------------

template <class Op>
class Op1Node final : public CalculatorNode {
protected:
  std::unique_ptr<CalculatorNode> m_a;

public:
  Op1Node(Calculator *calc, CalculatorNode *a) : CalculatorNode(calc), m_a(a) {}

  double compute(double vars[3]) const override {
    Op op;
    return op(m_a->compute(vars));
  }

  void accept(CalculatorNodeVisitor &visitor) override { m_a->accept(visitor); }
};

//-------------------------------------------------------------------

template <class Op>
class Op2Node final : public CalculatorNode {
protected:
  std::unique_ptr<CalculatorNode> m_a, m_b;

public:
  Op2Node(Calculator *calc, CalculatorNode *a, CalculatorNode *b)
      : CalculatorNode(calc), m_a(a), m_b(b) {}

  double compute(double vars[3]) const override {
    Op op;
    return op(m_a->compute(vars), m_b->compute(vars));
  }

  void accept(CalculatorNodeVisitor &visitor) override {
    m_a->accept(visitor), m_b->accept(visitor);
  }
};

//-------------------------------------------------------------------

template <class Op>
class Op3Node final : public CalculatorNode {
protected:
  std::unique_ptr<CalculatorNode> m_a, m_b, m_c;

public:
  Op3Node(Calculator *calc, CalculatorNode *a, CalculatorNode *b,
          CalculatorNode *c)
      : CalculatorNode(calc), m_a(a), m_b(b), m_c(c) {}

  double compute(double vars[3]) const override {
    Op op;
    return op(m_a->compute(vars), m_b->compute(vars), m_c->compute(vars));
  }

  void accept(CalculatorNodeVisitor &visitor) override {
    m_a->accept(visitor), m_b->accept(visitor), m_c->accept(visitor);
  }
};

//-------------------------------------------------------------------

class ChsNode final : public CalculatorNode {
  std::unique_ptr<CalculatorNode> m_a;

public:
  ChsNode(Calculator *calc, CalculatorNode *a) : CalculatorNode(calc), m_a(a) {}

  double compute(double vars[3]) const override { return -m_a->compute(vars); }
  void accept(CalculatorNodeVisitor &visitor) override { m_a->accept(visitor); }
};

//-------------------------------------------------------------------

class QuestionNode final : public CalculatorNode {
  std::unique_ptr<CalculatorNode> m_a, m_b, m_c;

public:
  QuestionNode(Calculator *calc, CalculatorNode *a, CalculatorNode *b,
               CalculatorNode *c)
      : CalculatorNode(calc), m_a(a), m_b(b), m_c(c) {}

  double compute(double vars[3]) const override {
    return (m_a->compute(vars) != 0) ? m_b->compute(vars) : m_c->compute(vars);
  }

  void accept(CalculatorNodeVisitor &visitor) override {
    m_a->accept(visitor), m_b->accept(visitor), m_c->accept(visitor);
  }
};

//-------------------------------------------------------------------

class NotNode final : public CalculatorNode {
  std::unique_ptr<CalculatorNode> m_a;

public:
  NotNode(Calculator *calc, CalculatorNode *a) : CalculatorNode(calc), m_a(a) {}

  double compute(double vars[3]) const override {
    return m_a->compute(vars) == 0;
  }
  void accept(CalculatorNodeVisitor &visitor) override { m_a->accept(visitor); }
};
//-------------------------------------------------------------------

class CycleNode final : public CalculatorNode {
  std::unique_ptr<CalculatorNode> m_a;

public:
  CycleNode(Calculator *calc, CalculatorNode *a)
      : CalculatorNode(calc), m_a(a) {}

  double compute(double vars[3]) const override {
    struct locals {
      static inline double compute(const TDoubleParam &param, double f) {
        if (param.getKeyframeCount() >= 2 &&
            f < param.keyframeIndexToFrame(0)) {
          TDoubleKeyframe kf = param.getKeyframe(0);
          if (kf.m_type == TDoubleKeyframe::Expression ||
              kf.m_type == TDoubleKeyframe::SimilarShape)
            return param.getDefaultValue();
        }

        double value = param.getValue(f);
        return value;
      }
    };

    double delta = std::max(1.0, m_a->compute(vars));

    const TDoubleParam *ownerParam = getCalculator()->getOwnerParameter();
    if (!ownerParam) return 0;

    double value = locals::compute(*ownerParam, vars[FRAME] - 1 - delta);
    if (getCalculator()->getUnit())
      value = getCalculator()->getUnit()->convertTo(value);

    return value;
  }

  void accept(CalculatorNodeVisitor &visitor) override { m_a->accept(visitor); }
};

//-------------------------------------------------------------------

class RandomNode : public CalculatorNode {
protected:
  std::unique_ptr<CalculatorNode> m_seed, m_min, m_max, m_arg;

public:
  RandomNode(Calculator *calc)
      : CalculatorNode(calc)
      , m_seed()
      , m_min()
      , m_max()
      , m_arg(new VariableNode(calc, CalculatorNode::FRAME)) {}

  void setSeed(CalculatorNode *arg) {
    assert(m_seed.get() == 0);
    m_seed.reset(arg);
  }
  void setMax(CalculatorNode *arg) {
    assert(m_max.get() == 0);
    m_max.reset(arg);
  }
  void setMin(CalculatorNode *arg) {
    assert(m_min.get() == 0);
    m_min.reset(arg);
  }

  double compute(double vars[3]) const override {
    double s = (m_seed.get() != 0) ? m_seed->compute(vars) : 0;
    double r =
        RandomManager::instance()->getValue(s, fabs(m_arg->compute(vars)));
    if (m_min.get() == 0)
      if (m_max.get() == 0)
        return r;
      else
        return m_max->compute(vars) * r;
    else
      return (1 - r) * m_min->compute(vars) + r * m_max->compute(vars);
  }

  void accept(CalculatorNodeVisitor &visitor) override {
    m_arg->accept(visitor);
    if (m_seed.get()) m_seed->accept(visitor);
    if (m_min.get()) m_min->accept(visitor);
    if (m_max.get()) m_max->accept(visitor);
  }
};

//-------------------------------------------------------------------

class PeriodicRandomNode final : public RandomNode {
  std::unique_ptr<CalculatorNode> m_period;

public:
  PeriodicRandomNode(Calculator *calc) : RandomNode(calc), m_period() {}

  void setPeriod(CalculatorNode *arg) {
    assert(m_period.get() == 0);
    m_period.reset(arg);
  }

  double compute(double vars[3]) const override {
    double s      = (m_seed.get() != 0) ? m_seed->compute(vars) : 0;
    double period = m_period->compute(vars);
    if (period == 0) period = 1;
    double f = m_arg->compute(vars);

    double f0    = period * std::floor(f / period);
    double f1    = period * (std::floor(f / period) + 1);
    double ratio = (f - f0) / (f1 - f0);

    double r0 = RandomManager::instance()->getValue(s, fabs(f0));
    double r1 = RandomManager::instance()->getValue(s, fabs(f1));
    double r  = (1 - ratio) * r0 + ratio * r1;

    if (m_min.get() == 0)
      if (m_max.get() == 0)
        return r;
      else
        return m_max->compute(vars) * r;
    else
      return (1 - r) * m_min->compute(vars) + r * m_max->compute(vars);
  }

  void accept(CalculatorNodeVisitor &visitor) override {
    RandomNode::accept(visitor);
    if (m_period.get()) m_period->accept(visitor);
  }
};

//===================================================================
// Patterns
//-------------------------------------------------------------------

CalculatorNode *Pattern::popNode(std::vector<CalculatorNode *> &stack) const {
  CalculatorNode *node = stack.back();
  stack.pop_back();
  return node;
}

//===================================================================

class NumberPattern final : public Pattern {
public:
  bool matchToken(const std::vector<Token> &previousTokens,
                  const Token &token) const override {
    return previousTokens.empty() && token.getType() == Token::Number;
  }
  bool isFinished(const std::vector<Token> &previousTokens,
                  const Token &token) const override {
    return previousTokens.size() == 1;
  }
  TokenType getTokenType(const std::vector<Token> &previousTokens,
                         const Token &token) const override {
    return Number;
  }
  void createNode(Calculator *calc, std::vector<CalculatorNode *> &stack,
                  const std::vector<Token> &tokens) const override {
    assert(tokens.size() == 1);
    assert(tokens[0].getType() == Token::Number);
    stack.push_back(new NumberNode(calc, tokens[0].getDoubleValue()));
  }
};

//-------------------------------------------------------------------

class ConstantPattern final : public Pattern {
  std::string m_constantName;
  double m_value;

public:
  ConstantPattern(std::string constantName, double value,
                  std::string description = "")
      : m_constantName(constantName), m_value(value) {
    setDescription(description);
  }

  std::string getFirstKeyword() const override { return m_constantName; }
  bool matchToken(const std::vector<Token> &previousTokens,
                  const Token &token) const override {
    return previousTokens.empty() && token.getText() == m_constantName;
  }
  bool isFinished(const std::vector<Token> &previousTokens,
                  const Token &token) const override {
    return previousTokens.size() == 1;
  }
  TokenType getTokenType(const std::vector<Token> &previousTokens,
                         const Token &token) const override {
    return Constant;
  }

  void createNode(Calculator *calc, std::vector<CalculatorNode *> &stack,
                  const std::vector<Token> &tokens) const override {
    assert(tokens.size() == 1);
    stack.push_back(new NumberNode(calc, m_value));
  }
};

//-------------------------------------------------------------------

class VariablePattern final : public Pattern {
  std::string m_variableName;
  int m_varIdx;

public:
  VariablePattern(std::string variableName, int varIdx,
                  std::string description = "")
      : m_variableName(variableName), m_varIdx(varIdx) {
    setDescription(description);
  }

  std::string getFirstKeyword() const override { return m_variableName; }
  bool matchToken(const std::vector<Token> &previousTokens,
                  const Token &token) const override {
    return previousTokens.empty() && token.getText() == m_variableName;
  }
  bool isFinished(const std::vector<Token> &previousTokens,
                  const Token &token) const override {
    return previousTokens.size() == 1;
  }
  TokenType getTokenType(const std::vector<Token> &previousTokens,
                         const Token &token) const override {
    return Variable;
  }

  void createNode(Calculator *calc, std::vector<CalculatorNode *> &stack,
                  const std::vector<Token> &tokens) const override {
    assert(tokens.size() == 1);
    assert(tokens[0].getText() == m_variableName);
    stack.push_back(new VariableNode(calc, m_varIdx));
  }
};

//-------------------------------------------------------------------

template <class Op>
class Op2Pattern final : public Pattern {
  std::string m_opName;
  int m_priority;

public:
  Op2Pattern(std::string opName, int priority)
      : m_opName(opName), m_priority(priority) {}
  int getPriority() const override { return m_priority; }
  std::string getFirstKeyword() const override { return m_opName; }
  bool expressionExpected(
      const std::vector<Token> &previousTokens) const override {
    return previousTokens.empty() || previousTokens.size() == 2;
  }
  bool matchToken(const std::vector<Token> &previousTokens,
                  const Token &token) const override {
    return previousTokens.size() == 1 && token.getText() == m_opName;
  }
  bool isFinished(const std::vector<Token> &previousTokens,
                  const Token &token) const override {
    return previousTokens.size() == 3;
  }
  TokenType getTokenType(const std::vector<Token> &previousTokens,
                         const Token &token) const override {
    return previousTokens.size() == 1 ? Operator : InternalError;
  }
  void createNode(Calculator *calc, std::vector<CalculatorNode *> &stack,
                  const std::vector<Token> &tokens) const override {
    assert(tokens.size() == 3);
    assert(tokens[1].getText() == m_opName);
    CalculatorNode *b = popNode(stack);
    CalculatorNode *a = popNode(stack);
    stack.push_back(new Op2Node<Op>(calc, a, b));
  }
};

//-------------------------------------------------------------------

class UnaryMinusPattern final : public Pattern {
public:
  UnaryMinusPattern() {}
  int getPriority() const override { return 50; }
  std::string getFirstKeyword() const override { return "-"; }

  bool expressionExpected(
      const std::vector<Token> &previousTokens) const override {
    return previousTokens.size() == 1;
  }
  bool matchToken(const std::vector<Token> &previousTokens,
                  const Token &token) const override {
    return previousTokens.empty() && token.getText() == "-";
  }
  bool isFinished(const std::vector<Token> &previousTokens,
                  const Token &token) const override {
    return previousTokens.size() == 2;
  }
  TokenType getTokenType(const std::vector<Token> &previousTokens,
                         const Token &token) const override {
    return Operator;
  }

  void createNode(Calculator *calc, std::vector<CalculatorNode *> &stack,
                  const std::vector<Token> &tokens) const override {
    assert(tokens.size() == 2);
    assert(tokens[0].getText() == "-");
    stack.push_back(new ChsNode(calc, popNode(stack)));
  }
};

//-------------------------------------------------------------------

class NotPattern final : public Pattern {
  std::string m_prefix;

public:
  NotPattern(std::string prefix, std::string description) : m_prefix(prefix) {
    setDescription(description);
  }
  int getPriority() const override { return 5; }
  std::string getFirstKeyword() const override { return m_prefix; }

  bool expressionExpected(
      const std::vector<Token> &previousTokens) const override {
    return previousTokens.size() == 1;
  }
  bool matchToken(const std::vector<Token> &previousTokens,
                  const Token &token) const override {
    return previousTokens.empty() && token.getText() == m_prefix;
  }
  bool isFinished(const std::vector<Token> &previousTokens,
                  const Token &token) const override {
    return previousTokens.size() == 2;
  }
  TokenType getTokenType(const std::vector<Token> &previousTokens,
                         const Token &token) const override {
    return Operator;
  }
  void createNode(Calculator *calc, std::vector<CalculatorNode *> &stack,
                  const std::vector<Token> &tokens) const override {
    assert(tokens.size() == 2);
    assert(tokens[0].getText() == m_prefix);
    stack.push_back(new NotNode(calc, popNode(stack)));
  }
};

//-------------------------------------------------------------------

class QuestionTernaryPattern final : public Pattern {
public:
  QuestionTernaryPattern() {}
  int getPriority() const override { return 5; }
  std::string getFirstKeyword() const override { return "?"; }

  bool expressionExpected(
      const std::vector<Token> &previousTokens) const override {
    int i = (int)previousTokens.size();
    return i == 0 || i == 2 || i == 4;
  }
  bool matchToken(const std::vector<Token> &previousTokens,
                  const Token &token) const override {
    int i = (int)previousTokens.size();
    return ((i == 1 && token.getText() == "?") ||
            (i == 3 && token.getText() == ":"));
  }
  bool isFinished(const std::vector<Token> &previousTokens,
                  const Token &token) const override {
    return previousTokens.size() == 5;
  }
  TokenType getTokenType(const std::vector<Token> &previousTokens,
                         const Token &token) const override {
    int i = (int)previousTokens.size();
    return (i == 1 || i == 3) ? Operator : InternalError;
  }
  void createNode(Calculator *calc, std::vector<CalculatorNode *> &stack,
                  const std::vector<Token> &tokens) const override {
    CalculatorNode *node1 = popNode(stack);
    CalculatorNode *node2 = popNode(stack);
    CalculatorNode *node3 = popNode(stack);
    stack.push_back(new QuestionNode(calc, node3, node2, node1));
  }
};
//-------------------------------------------------------------------

class BraketPattern final : public Pattern {
public:
  BraketPattern() {}
  int getPriority() const override { return 5; }
  std::string getFirstKeyword() const override { return "("; }

  bool expressionExpected(
      const std::vector<Token> &previousTokens) const override {
    return previousTokens.size() == 1;
  }
  bool matchToken(const std::vector<Token> &previousTokens,
                  const Token &token) const override {
    return ((previousTokens.empty() && token.getText() == "(") ||
            (previousTokens.size() == 2 && token.getText() == ")"));
  }
  bool isFinished(const std::vector<Token> &previousTokens,
                  const Token &token) const override {
    return previousTokens.size() == 3;
  }
  TokenType getTokenType(const std::vector<Token> &previousTokens,
                         const Token &token) const override {
    return previousTokens.size() != 1 ? Parenthesis : InternalError;
  }
  void createNode(Calculator *calc, std::vector<CalculatorNode *> &stack,
                  const std::vector<Token> &tokens) const override {
    assert(tokens.size() == 3);
    assert(tokens[0].getText() == "(");
    assert(tokens[2].getText() == ")");
  }
};

//-------------------------------------------------------------------

class FunctionPattern : public Pattern {
protected:
  std::string m_functionName;
  bool m_implicitArgAllowed;
  // if m_implicitArgAllowed == true then the first argument is the frame number
  // e.g. f(5) means f(frame,5)
  // to use a different first argument (e.g. t*10) you have to write f(t*10;5)

  int m_minArgCount;
  std::vector<double> m_optionalArgDefaults;

public:
  FunctionPattern(std::string functionName, int minArgCount)
      : m_functionName(functionName)
      , m_implicitArgAllowed(false)
      , m_minArgCount(minArgCount) {}

  void allowImplicitArg(bool allowed) { m_implicitArgAllowed = allowed; }
  void addOptionalArg(double value) { m_optionalArgDefaults.push_back(value); }

  std::string getFirstKeyword() const override { return m_functionName; }
  bool expressionExpected(
      const std::vector<Token> &previousTokens) const override {
    int n = (int)previousTokens.size();
    return 2 <= n && (n & 1) == 0;
  }

  bool matchToken(const std::vector<Token> &previousTokens,
                  const Token &token) const override {
    int i         = (int)previousTokens.size();
    std::string s = toLower(token.getText());
    if (i == 0)
      return s == toLower(m_functionName);
    else if (i == 1)
      return s == "(";
    else if ((i & 1) == 0)
      return true;
    else if (s == ",")
      return true;
    else if (s == ";")
      return i == 3 && m_implicitArgAllowed;
    else if (s == ")") {
      int n = (i - 1) / 2;
      if (previousTokens.size() > 3 && previousTokens[3].getText() == ";") n--;
      if (n < m_minArgCount ||
          n > m_minArgCount + (int)m_optionalArgDefaults.size())
        return false;
      else
        return true;
    } else
      return false;
  }
  bool isFinished(const std::vector<Token> &previousTokens,
                  const Token &token) const override {
    if (previousTokens.empty()) return false;
    return ((m_minArgCount == 0 && previousTokens.size() == 1 &&
             token.getText() != "(") ||
            (previousTokens.back().getText() == ")"));
  }
  TokenType getTokenType(const std::vector<Token> &previousTokens,
                         const Token &token) const override {
    int i = (int)previousTokens.size();
    if (i == 0)
      return Function;
    else if (i == 1 || token.getText() == ")")
      return Function;
    else if (i == 3)
      return token.getText() == ";" ? Comma : Comma;
    else if (i & 1)
      return Comma;
    else
      return InternalError;
  }

  void getArgs(std::vector<CalculatorNode *> &nodes, Calculator *calc,
               std::vector<CalculatorNode *> &stack,
               const std::vector<Token> &tokens) const {
    bool implicitArgUsed =
        m_implicitArgAllowed && tokens.size() > 3 && tokens[3].getText() == ";";

    // n = number of arguments to provide (mandatory + optional + implicit)
    int n = m_minArgCount + (int)m_optionalArgDefaults.size() +
            (m_implicitArgAllowed ? 1 : 0);

    // m = number of default arguments to assign (with their default value)
    int m = n - (tokens.size() - 2) / 2;
    if (m_implicitArgAllowed && !implicitArgUsed) m--;

    assert(m <= (int)m_optionalArgDefaults.size());
    if (m > (int)m_optionalArgDefaults.size())
      m = (int)m_optionalArgDefaults.size();

    nodes.resize(n);

    // fetch arguments from the stack
    int k = n - m;
    if (implicitArgUsed) {
      while (k > 0) nodes[--k] = popNode(stack);
    } else {
      while (k > 1) nodes[--k] = popNode(stack);
      nodes[0] = new VariableNode(calc, CalculatorNode::FRAME);
    }

    // add default values
    for (int i = 0; i < m; i++)
      nodes[n - m + i] = new NumberNode(calc, m_optionalArgDefaults[i]);
  }
};

//-------------------------------------------------------------------

template <class Function>
class F0Pattern final : public FunctionPattern {
public:
  F0Pattern(std::string functionName) : FunctionPattern(functionName, 0) {}
  void createNode(Calculator *calc, std::vector<CalculatorNode *> &stack,
                  const std::vector<Token> &tokens) const override {
    stack.push_back(new Op0Node<Function>(calc, m_functionName));
  }
};

//-------------------------------------------------------------------

template <class Function>
class F1Pattern final : public FunctionPattern {
public:
  F1Pattern(std::string functionName, std::string descr = "")
      : FunctionPattern(functionName, 1) {
    setDescription(descr);
  }
  void createNode(Calculator *calc, std::vector<CalculatorNode *> &stack,
                  const std::vector<Token> &tokens) const override {
    stack.push_back(new Op1Node<Function>(calc, popNode(stack)));
  }
};

//-------------------------------------------------------------------

template <class Function>
class F2Pattern final : public FunctionPattern {
public:
  F2Pattern(std::string functionName, std::string descr = "")
      : FunctionPattern(functionName, 2) {
    setDescription(descr);
  }
  void createNode(Calculator *calc, std::vector<CalculatorNode *> &stack,
                  const std::vector<Token> &tokens) const override {
    CalculatorNode *b = popNode(stack);
    CalculatorNode *a = popNode(stack);
    stack.push_back(new Op2Node<Function>(calc, a, b));
  }
};

//-------------------------------------------------------------------

template <class Function>
class F3Pattern final : public FunctionPattern {
public:
  F3Pattern(std::string functionName, std::string descr = "")
      : FunctionPattern(functionName, 3) {
    setDescription(descr);
  }
  void createNode(Calculator *calc, std::vector<CalculatorNode *> &stack,
                  const std::vector<Token> &tokens) const override {
    CalculatorNode *c = popNode(stack);
    CalculatorNode *b = popNode(stack);
    CalculatorNode *a = popNode(stack);
    stack.push_back(new Op3Node<Function>(calc, a, b, c));
  }
};

//-------------------------------------------------------------------

template <class Function>
class Fs2Pattern final : public FunctionPattern {
public:
  Fs2Pattern(std::string functionName, std::string description)
      : FunctionPattern(functionName, 1) {
    allowImplicitArg(true);
    setDescription(description);
  }
  void createNode(Calculator *calc, std::vector<CalculatorNode *> &stack,
                  const std::vector<Token> &tokens) const override {
    std::vector<CalculatorNode *> nodes;
    getArgs(nodes, calc, stack, tokens);
    stack.push_back(new Op2Node<Function>(calc, nodes[0], nodes[1]));
  }
};

//-------------------------------------------------------------------

template <class Function>
class Fs3Pattern final : public FunctionPattern {
public:
  Fs3Pattern(std::string functionName, double defVal, std::string descr)
      : FunctionPattern(functionName, 1) {
    allowImplicitArg(true);
    addOptionalArg(defVal);
    setDescription(descr);
  }
  void createNode(Calculator *calc, std::vector<CalculatorNode *> &stack,
                  const std::vector<Token> &tokens) const override {
    std::vector<CalculatorNode *> nodes;
    getArgs(nodes, calc, stack, tokens);
    stack.push_back(new Op3Node<Function>(calc, nodes[0], nodes[1], nodes[2]));
  }
};

//-------------------------------------------------------------------

class CyclePattern final : public FunctionPattern {
public:
  CyclePattern(std::string functionName) : FunctionPattern(functionName, 1) {
    setDescription(
        "cycle(period)\nCycles the transitions of the period previous frames "
        "to the selected range");
  }
  void createNode(Calculator *calc, std::vector<CalculatorNode *> &stack,
                  const std::vector<Token> &tokens) const override {
    CalculatorNode *a = popNode(stack);
    stack.push_back(new CycleNode(calc, a));
  }
};

//-------------------------------------------------------------------

class RandomPattern final : public FunctionPattern {
  bool m_seed;

public:
  RandomPattern(std::string functionName, bool seed, std::string description)
      : FunctionPattern(functionName, seed ? 1 : 0), m_seed(seed) {
    allowImplicitArg(true);
    addOptionalArg(0);
    addOptionalArg(0);
    setDescription(description);
  }
  void createNode(Calculator *calc, std::vector<CalculatorNode *> &stack,
                  const std::vector<Token> &tokens) const override {
    int n = ((int)tokens.size() - 1) / 2;
    if (m_seed) n--;
    RandomNode *randomNode = new RandomNode(calc);
    if (n > 0) {
      randomNode->setMax(popNode(stack));
      if (n > 1) randomNode->setMin(popNode(stack));
    }
    if (m_seed) randomNode->setSeed(popNode(stack));
    stack.push_back(randomNode);
  }
};

//-------------------------------------------------------------------

class PeriodicRandomPattern final : public FunctionPattern {
  bool m_seed;

public:
  PeriodicRandomPattern(std::string functionName, bool seed,
                        std::string description)
      : FunctionPattern(functionName, seed ? 2 : 1), m_seed(seed) {
    allowImplicitArg(true);
    addOptionalArg(0);
    addOptionalArg(0);
    setDescription(description);
  }
  void createNode(Calculator *calc, std::vector<CalculatorNode *> &stack,
                  const std::vector<Token> &tokens) const override {
    int n = ((int)tokens.size() - 1) / 2;
    n--;
    if (m_seed) n--;
    PeriodicRandomNode *randomNode = new PeriodicRandomNode(calc);
    if (n > 0) {
      randomNode->setMax(popNode(stack));
      if (n > 1) randomNode->setMin(popNode(stack));
    }
    if (m_seed) randomNode->setSeed(popNode(stack));
    randomNode->setPeriod(popNode(stack));
    stack.push_back(randomNode);
  }
};

//===================================================================

class PatternTable {
  std::map<std::string, Pattern *> m_kTable;
  std::vector<Pattern *> m_uTable;
  Grammar::Position m_position;

public:
  PatternTable(Grammar::Position position) : m_position(position) {}

  ~PatternTable() {
    for (std::map<std::string, Pattern *>::iterator it = m_kTable.begin();
         it != m_kTable.end(); ++it)
      delete it->second;
    for (std::vector<Pattern *>::iterator it = m_uTable.begin();
         it != m_uTable.end(); ++it)
      delete *it;
  }

  void addPattern(Pattern *pattern) {
    std::string keyword = pattern->getFirstKeyword();
    if (keyword != "") {
      // first keyword should be unique
      assert(m_kTable.count(keyword) == 0);
      m_kTable[keyword] = pattern;
    } else
      m_uTable.push_back(pattern);
  }

  const Pattern *getPattern(const Token &token) const {
    std::vector<Token> tokens;
    if (m_position == Grammar::ExpressionEnd) tokens.push_back(Token());
    if (token.getType() == Token::Punct || token.getType() == Token::Ident) {
      std::string keyword = token.getText();
      std::map<std::string, Pattern *>::const_iterator it =
          m_kTable.find(keyword);
      if (it != m_kTable.end()) {
        Pattern *pattern = it->second;
        if (pattern->matchToken(tokens, token)) {
          return pattern;
        }
      }
    }
    for (int i = 0; i < (int)m_uTable.size(); i++) {
      Pattern *pattern = m_uTable[i];
      if (pattern->matchToken(tokens, token)) {
        return pattern;
      }
    }
    return 0;
  }

  void getSuggestions(Grammar::Suggestions &suggestions) const {
    std::map<std::string, Pattern *>::const_iterator it;
    for (it = m_kTable.begin(); it != m_kTable.end(); ++it) {
      suggestions.push_back(
          std::make_pair(it->first, it->second->getDescription()));
    }
    for (int i = 0; i < (int)m_uTable.size(); i++) {
      std::vector<std::string> keywords;
      m_uTable[i]->getAcceptableKeywords(keywords);
      for (int j = 0; j < (int)keywords.size(); j++)
        suggestions.push_back(
            std::make_pair(keywords[j], m_uTable[i]->getDescription()));
    }
  }
};

//===================================================================
//
// funzioni trigonometriche, log, exp, ecc.
//
//-------------------------------------------------------------------

class Pow {
public:
  double operator()(double x, double y) const { return pow(x, y); }
};
// class Mod   {public: double operator()(double x, double y) const {return
// (int)x % (int)y;} };
class Sin {
public:
  double operator()(double x) const { return sin(toRad(x)); }
};
class Cos {
public:
  double operator()(double x) const { return cos(toRad(x)); }
};
class Tan {
public:
  double operator()(double x) const { return tan(toRad(x)); }
};
class Sinh {
public:
  double operator()(double x) const { return sinh(toRad(x)); }
};
class Cosh {
public:
  double operator()(double x) const { return cosh(toRad(x)); }
};
class Tanh {
public:
  double operator()(double x) const { return tanh(toRad(x)); }
};
class Atan {
public:
  double operator()(double x) const { return toDeg(atan(x)); }
};
class Atan2 {
public:
  double operator()(double x, double y) const { return toDeg(atan2(x, y)); }
};
class Log {
public:
  double operator()(double x) const { return log(x); }
};
class Exp {
public:
  double operator()(double x) const { return exp(x); }
};
class Floor {
public:
  double operator()(double x) const { return tfloor(x); }
};
class Ceil {
public:
  double operator()(double x) const { return tceil(x); }
};
class Round {
public:
  double operator()(double x) const { return tround(x); }
};
class Abs {
public:
  double operator()(double x) const { return fabs(x); }
};
class Sign {
public:
  double operator()(double x) const { return x > 0 ? 1 : x < 0 ? -1 : 0; }
};
class Sqrt {
public:
  double operator()(double x) const { return x >= 0.0 ? sqrt(x) : 0; }
};
class Sqr {
public:
  double operator()(double x) const { return x * x; }
};
class Crop {
public:
  double operator()(double x, double a, double b) const {
    return tcrop(x, a, b);
  }
};
class Step {
public:
  double operator()(double x, double y) const { return x < y ? 0 : 1; }
};

class Mod {
public:
  double operator()(double x, double y) const {
    if (y == 0.0) return 0;
    return x - y * floor(x / y);
  }
};

class Min {
public:
  double operator()(double x, double y) const { return x < y ? x : y; }
};
class Max {
public:
  double operator()(double x, double y) const { return x > y ? x : y; }
};

class Gt {
public:
  double operator()(double x, double y) const { return x > y ? 1 : 0; }
};
class Ge {
public:
  double operator()(double x, double y) const { return x >= y ? 1 : 0; }
};
class Lt {
public:
  double operator()(double x, double y) const { return x < y ? 1 : 0; }
};
class Le {
public:
  double operator()(double x, double y) const { return x <= y ? 1 : 0; }
};
class Ne {
public:
  double operator()(double x, double y) const { return x != y ? 1 : 0; }
};
class Eq {
public:
  double operator()(double x, double y) const { return x == y ? 1 : 0; }
};
class And {
public:
  double operator()(double x, double y) const {
    return (x != 0) && (y != 0) ? 1 : 0;
  }
};
class Or {
public:
  double operator()(double x, double y) const {
    return (x != 0) || (y != 0) ? 1 : 0;
  }
};
class Not {
public:
  double operator()(double x) const { return x == 0; }
};

class Smoothstep {
public:
  double operator()(double v, double min, double max) const {
    if (v <= min)
      return 0;
    else if (v >= max)
      return 1;
    double t = (v - min) / (max - min);
    return -2 * t * t * t + 3 * t * t;
  }
};
class Pulse {
public:
  double operator()(double x, double x0, double length) const {
    // double length=5.0;
    double b = (.69315 * 4.0) / (length * length);
    x -= x0;
    return exp(-x * x * b);
  }
};
class Saw {
public:
  double operator()(double x, double length, double height) const {
    if (length <= 0.0) return 0.0;
    if (height <= 0.0) height = length;
    double q = x / length;
    return height * (q - floor(q));
  }
};
class Wave {
public:
  double operator()(double x, double length) const {
    if (length <= 0.0) return 0.0;
    return sin(x * 2 * PI / length);
  }
};

//===================================================================

class Grammar::Imp {
public:
  PatternTable m_prePatterns, m_postPatterns;

  Imp()
      : m_prePatterns(Grammar::ExpressionStart)
      , m_postPatterns(Grammar::ExpressionEnd) {}
  ~Imp() {}
};

Grammar::Grammar() : m_imp(new Imp()) {
  addPattern(new NumberPattern());
  addPattern(new ConstantPattern("pi", PI, "3.14159265..."));
  addPattern(new VariablePattern(
      "t", CalculatorNode::T, "ranges from 0.0 to 1.0 along the transition"));
  const std::string f_desc = "the current frame number";
  addPattern(new VariablePattern("f", CalculatorNode::FRAME, f_desc));
  addPattern(new VariablePattern("frame", CalculatorNode::FRAME, f_desc));
  const std::string r_desc =
      "the current frame number, relative to the transition";
  addPattern(new VariablePattern("r", CalculatorNode::RFRAME, r_desc));
  addPattern(new VariablePattern("rframe", CalculatorNode::RFRAME, r_desc));
  addPattern(new Op2Pattern<std::plus<double>>("+", 10));
  addPattern(new Op2Pattern<std::multiplies<double>>("*", 20));
  addPattern(new Op2Pattern<Pow>("^", 30));
  addPattern(new Op2Pattern<std::minus<double>>("-", 10));
  addPattern(new Op2Pattern<std::divides<double>>("/", 20));
  addPattern(new Op2Pattern<Mod>("%", 8));
  addPattern(new Op2Pattern<Gt>(">", 6));
  addPattern(new Op2Pattern<Ge>(">=", 6));
  addPattern(new Op2Pattern<Lt>("<", 6));
  addPattern(new Op2Pattern<Le>("<=", 6));
  addPattern(new Op2Pattern<Eq>("==", 6));
  addPattern(new Op2Pattern<Ne>("!=", 6));
  addPattern(new Op2Pattern<And>("&&", 3));
  addPattern(new Op2Pattern<Or>("||", 2));
  addPattern(new Op2Pattern<And>("and", 3));
  addPattern(new Op2Pattern<Or>("or", 2));
  addPattern(new NotPattern("!", "not"));
  addPattern(new NotPattern("not", ""));
  addPattern(new UnaryMinusPattern());
  addPattern(new BraketPattern());
  addPattern(new QuestionTernaryPattern());
  addPattern(new F1Pattern<Sin>("sin", "sin(degree)"));
  addPattern(new F1Pattern<Cos>("cos", "cos(degree)"));
  addPattern(new F1Pattern<Tan>("tan", "tan(degree)"));
  addPattern(new F1Pattern<Sinh>("sinh", "sinh(degree)\nHyperbolic sine"));
  addPattern(new F1Pattern<Cosh>("cosh", "cosh(degree)\nHyperbolic cosine"));
  addPattern(new F1Pattern<Tanh>("tanh", "tanh(degree)\nHyperbolic tangent"));
  addPattern(new F1Pattern<Atan>("atan",
                                 "atan(x)\nArctangent : the inverse of tan()"));
  addPattern(new F2Pattern<Atan2>("atan2",
                                  "atan2(y,x)\nThe counter-clockwise angle in "
                                  "degree between the x-axis and the "
                                  "point(x,y)"));
  addPattern(
      new F1Pattern<Log>("log", "log(x)\nThe natural logarithm of x (base e)"));
  addPattern(new F1Pattern<Exp>("exp", "exp(x)\nThe base-e exponential of x"));
  addPattern(
      new F1Pattern<Floor>("floor", "floor(x)\nThe greatest integer <= x"));
  const std::string ceil_desc = "The smallest integer >= x";
  addPattern(new F1Pattern<Ceil>("ceil", "ceil(x)\n" + ceil_desc));
  addPattern(new F1Pattern<Ceil>("ceiling", "ceiling(x)\n" + ceil_desc));
  addPattern(
      new F1Pattern<Round>("round", "round(x)\nThe integer nearest to x"));
  addPattern(new F1Pattern<Abs>("abs", "abs(x)\nThe absolute value of x"));
  addPattern(
      new F1Pattern<Sign>("sign", "sign(x)\n-1 if x<0, 1 if x>0 and 0 if x=0"));
  const std::string sqrt_desc = "Square root of x";
  addPattern(new F1Pattern<Sqrt>("sqrt", "sqrt(x)\n" + sqrt_desc));
  addPattern(new F1Pattern<Sqr>("sqr", "sqr(x)\n" + sqrt_desc));
  addPattern(new F3Pattern<Crop>(
      "crop", "crop(x,a,b)\na if x<a, b if x>b, x if x in [a,b]"));
  addPattern(new F3Pattern<Crop>(
      "clamp", "clamp(x,a,b)\na if x<a, b if x>b, x if x in [a,b]"));
  addPattern(new F2Pattern<Min>("min", "min(a,b)"));
  addPattern(new F2Pattern<Max>("max", "max(a,b)"));
  addPattern(new F2Pattern<Step>("step", "min(x,x0)\n0 if x<x0, 1 if x>=x0"));
  addPattern(new F3Pattern<Smoothstep>("smoothstep",
                                       "smoothstep(x,x0)\n0 if x<x0, 1 if "
                                       "x>=x0\nas step, but with smooth "
                                       "transition"));
  const std::string pulse_desc =
      "Generates a bump ranging from 0.0 to 1.0 set at position pos";
  addPattern(new Fs3Pattern<Pulse>("pulse", 0.5,
                                   "pulse(pos)\npulse(pos,length)\npulse(arg; "
                                   "pos)\npulse(arg;pos,length)\n" +
                                       pulse_desc));
  addPattern(new Fs3Pattern<Pulse>(
      "bump", 0.5,
      "bump(pos)\nbump(pos,length)\nbump(arg; pos)\nbump(arg;pos,length)\n" +
          pulse_desc));
  const std::string saw_desc = "Generates a periodic sawtooth shaped curve";
  addPattern(new Fs3Pattern<Saw>("sawtooth", 0.0,
                                 "sawtooth(length)\nsawtooth(length, "
                                 "height)\nsawtooth(arg; "
                                 "length)\nsawtooth(arg; length, height)\n" +
                                     saw_desc));
  addPattern(new Fs3Pattern<Saw>("saw", 0.0,
                                 "saw(length)\nsaw(length, height)\nsaw(arg; "
                                 "length)\nsaw(arg; length, height)\n" +
                                     saw_desc));
  addPattern(new Fs2Pattern<Wave>(
      "wave", "wave(_length)\nwave(_arg;_length)\nsame as sin(f*180/length)"));
  const std::string rnd_desc = "Generates random number between min and max";
  addPattern(new RandomPattern(
      "random", false,
      "random = random(0,1)\nrandom(max) = random(0,max)\nrandom(min,max)\n" +
          rnd_desc));
  addPattern(new RandomPattern(
      "rnd", false,
      "rnd = rnd(0,1)\nrnd(max) = rnd(0,max)\nrnd(min,max)\n" + rnd_desc));
  const std::string rnd_s_desc =
      rnd_desc + "; seed select different random sequences";
  addPattern(new RandomPattern("random_s", true,
                               "random_s(seed) = random_s(seed, "
                               "0,1)\nrandom_s(seed,max) = random_s(seed, "
                               "0,max)\nrandom_s(seed,min,max)\n" +
                                   rnd_s_desc));
  addPattern(new RandomPattern("rnd_s", true,
                               "rnd_s(seed) = rnd_s(seed, "
                               "0,1)\nrnd_s(seed,max) = rnd_s(seed, "
                               "0,max)\nrnd_s(seed,min,max)\n" +
                                   rnd_s_desc));

  const std::string rnd_p_desc =
      rnd_desc + "; values are interpolated in periodic intervals";
  addPattern(new PeriodicRandomPattern(
      "random_p", false,
      "random_p(period) = random_p(period,0,1)\nrandom_p(period,max) = "
      "random_p(period,0,max)\nrandom_p(period,min,max)\n" +
          rnd_p_desc));
  addPattern(new PeriodicRandomPattern(
      "rnd_p", false,
      "rnd_p(period) = rnd_p(period,0,1)\nrnd_p(period,max) = "
      "rnd_p(period,0,max)\nrnd_p(period,min,max)\n" +
          rnd_p_desc));
  const std::string rnd_ps_desc =
      rnd_s_desc + "; values are interpolated in periodic intervals";
  addPattern(new PeriodicRandomPattern(
      "random_ps", true,
      "random_ps(period,seed) = random_ps(period,seed, "
      "0,1)\nrandom_ps(period,seed,max) = random_ps(period,seed, "
      "0,max)\nrandom_ps(period,seed,min,max)\n" +
          rnd_ps_desc));
  addPattern(new PeriodicRandomPattern(
      "rnd_ps", true,
      "rnd_ps(period,seed) = rnd_ps(period,seed, "
      "0,1)\nrnd_ps(period,seed,max) = rnd_ps(period,seed, "
      "0,max)\nrnd_ps(period,seed,min,max)\n" +
          rnd_ps_desc));

  addPattern(new CyclePattern("cycle"));
}

Grammar::~Grammar() {}

void Grammar::addPattern(Pattern *pattern) {
  std::vector<Token> noTokens;
  if (pattern->expressionExpected(noTokens))
    m_imp->m_postPatterns.addPattern(pattern);
  else
    m_imp->m_prePatterns.addPattern(pattern);
}

const Pattern *Grammar::getPattern(Position position,
                                   const Token &token) const {
  Pattern *pattern = 0;
  if (position == ExpressionStart)
    return m_imp->m_prePatterns.getPattern(token);
  else
    return m_imp->m_postPatterns.getPattern(token);
}

void Grammar::getSuggestions(Grammar::Suggestions &suggestions,
                             Position position) const {
  if (position == ExpressionStart)
    return m_imp->m_prePatterns.getSuggestions(suggestions);
  else
    return m_imp->m_postPatterns.getSuggestions(suggestions);
}

//===================================================================

}  // namespace TSyntax
