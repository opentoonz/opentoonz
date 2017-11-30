

// TnzCore includes
#include "tconvert.h"

// TnzBase includes
#include "tdoubleparam.h"
#include "tparamcontainer.h"
#include "tparamset.h"
#include "texpression.h"
#include "tparser.h"
#include "tfx.h"

#include "tw/stringtable.h"
#include "tunit.h"

// TnzLib includes
#include "toonz/txsheet.h"
#include "toonz/txshcell.h"
#include "toonz/tstageobjectid.h"
#include "toonz/tstageobject.h"
#include "toonz/fxdag.h"

// Boost includes
#include "boost/noncopyable.hpp"

// Qt includes
#include <QString>

#include "toonz/txsheetexpr.h"

#include <memory>

using namespace TSyntax;

//******************************************************************************
//    Local namespace  stuff
//******************************************************************************

namespace {

//===================================================================

class ParamDependencyFinder final : public TSyntax::CalculatorNodeVisitor {
  TDoubleParam *m_possiblyDependentParam;
  bool m_found;

public:
  ParamDependencyFinder(TDoubleParam *possiblyDependentParam)
      : m_possiblyDependentParam(possiblyDependentParam), m_found(false) {}

  void check(TDoubleParam *param) {
    if (param == m_possiblyDependentParam) m_found = true;
  }

  bool found() const { return m_found; }
};

//===================================================================
//
// Calculator Nodes
//
//-------------------------------------------------------------------

class ParamCalculatorNode final : public CalculatorNode,
                                  public TParamObserver,
                                  public boost::noncopyable {
  TDoubleParamP m_param;
  std::unique_ptr<CalculatorNode> m_frame;

public:
  ParamCalculatorNode(Calculator *calculator, const TDoubleParamP &param,
                      std::unique_ptr<CalculatorNode> frame)
      : CalculatorNode(calculator), m_param(param), m_frame(std::move(frame)) {
    param->addObserver(this);
  }

  ~ParamCalculatorNode() { m_param->removeObserver(this); }

  double compute(double vars[3]) const override {
    double value      = m_param->getValue(m_frame->compute(vars) - 1);
    TMeasure *measure = m_param->getMeasure();
    if (measure) {
      const TUnit *unit = measure->getCurrentUnit();
      if (unit) value   = unit->convertTo(value);
    }
    return value;
  }

  void accept(TSyntax::CalculatorNodeVisitor &visitor) override {
    ParamDependencyFinder *pdf =
        dynamic_cast<ParamDependencyFinder *>(&visitor);
    pdf->check(m_param.getPointer());
    m_param->accept(visitor);
  }

  void onChange(const TParamChange &paramChange) override {
    // The referenced parameter changed. This means the parameter owning the
    // expression this node is part of, changes too.

    // A param change is thus propagated for this parameter, with the 'keyframe'
    // parameter turned off - since no keyframe value is actually altered.

    if (TDoubleParam *ownerParam = getCalculator()->getOwnerParameter()) {
      const std::set<TParamObserver *> &observers = ownerParam->observers();

      TParamChange propagatedChange(ownerParam, 0, 0, false,
                                    paramChange.m_dragging,
                                    paramChange.m_undoing);

      std::set<TParamObserver *>::const_iterator ot, oEnd = observers.end();
      for (ot = observers.begin(); ot != oEnd; ++ot)
        (*ot)->onChange(propagatedChange);
    }
  }
};

//-------------------------------------------------------------------

class XsheetDrawingCalculatorNode final : public CalculatorNode,
                                          public boost::noncopyable {
  TXsheet *m_xsh;
  int m_columnIndex;

  std::unique_ptr<CalculatorNode> m_frame;

public:
  XsheetDrawingCalculatorNode(Calculator *calc, TXsheet *xsh, int columnIndex,
                              std::unique_ptr<CalculatorNode> frame)
      : CalculatorNode(calc)
      , m_xsh(xsh)
      , m_columnIndex(columnIndex)
      , m_frame(std::move(frame)) {}

  double compute(double vars[3]) const override {
    double f = m_frame->compute(vars);
    int i    = tfloor(f);
    f        = f - (double)i;
    TXshCell cell;
    cell     = m_xsh->getCell(i, m_columnIndex);
    int d0   = cell.isEmpty() ? 0 : cell.m_frameId.getNumber();
    cell     = m_xsh->getCell(i + 1, m_columnIndex);
    int d1   = cell.isEmpty() ? 0 : cell.m_frameId.getNumber();
    double d = (1 - f) * d0 + f * d1;
    return d;
  }

  void accept(TSyntax::CalculatorNodeVisitor &) override {}
};

//===================================================================
//
// Patterns
//
//-------------------------------------------------------------------

class XsheetReferencePattern final : public Pattern {
  TXsheet *m_xsh;

public:
  XsheetReferencePattern(TXsheet *xsh) : m_xsh(xsh) {
    setDescription(std::string("object.action\nTransformation reference\n") +
                   "object can be: tab, table, cam<n>, camera<n>, col<n>, "
                   "peg<n>, pegbar<n>\n" +
                   "action can be: "
                   "ns,ew,rot,ang,angle,z,zdepth,sx,sy,sc,scale,scalex,scaley,"
                   "path,pos,shx,shy");
  }

  TStageObjectId matchObjectName(const Token &token) const {
    std::string s = toLower(token.getText());
    int len       = (int)s.length(), i, j;
    for (i = 0; i < len && isascii(s[i]) && isalpha(s[i]); i++) {
    }
    if (i == 0) return TStageObjectId::NoneId;
    std::string a = s.substr(0, i);
    int index     = 0;
    for (j = i; j < len && isascii(s[j]) && isdigit(s[j]); j++)
      index = index * 10 + (s[j] - '0');
    if (j < len) return TStageObjectId::NoneId;
    if (i == j) index = -1;
    if ((a == "table" || a == "tab") && index < 0)
      return TStageObjectId::TableId;
    else if (a == "col" && index >= 1)
      return TStageObjectId::ColumnId(index - 1);
    else if ((a == "cam" || a == "camera") && index >= 1)
      return TStageObjectId::CameraId(index - 1);
    else if ((a == "peg" || a == "pegbar") && index >= 1)
      return TStageObjectId::PegbarId(index - 1);
    else
      return TStageObjectId::NoneId;
  }

  TStageObject::Channel matchChannelName(const Token &token) const {
    std::string s = toLower(token.getText());
    if (s == "ns")
      return TStageObject::T_Y;
    else if (s == "ew")
      return TStageObject::T_X;
    else if (s == "rot" || s == "ang" || s == "angle")
      return TStageObject::T_Angle;
    else if (s == "z" || s == "zdepth")
      return TStageObject::T_Z;
    else if (s == "sx" || s == "scalex" || s == "xscale" || s == "xs" ||
             s == "sh" || s == "scaleh" || s == "hscale" || s == "hs")
      return TStageObject::T_ScaleX;
    else if (s == "sy" || s == "scaley" || s == "yscale" || s == "ys" ||
             s == "sv" || s == "scalev" || s == "vscale" || s == "vs")
      return TStageObject::T_ScaleY;
    else if (s == "sc" || s == "scale")
      return TStageObject::T_Scale;
    else if (s == "path" || s == "pos")
      return TStageObject::T_Path;
    else if (s == "shearx" || s == "shx" || s == "shearh" || s == "shh")
      return TStageObject::T_ShearX;
    else if (s == "sheary" || s == "shy" || s == "shearv" || s == "shv")
      return TStageObject::T_ShearY;
    else
      return TStageObject::T_ChannelCount;
  }

  bool expressionExpected(
      const std::vector<Token> &previousTokens) const override {
    return previousTokens.size() == 4;
  }
  bool matchToken(const std::vector<Token> &previousTokens,
                  const Token &token) const override {
    int i = (int)previousTokens.size();
    if (i == 0)
      return matchObjectName(token) != TStageObjectId::NoneId;
    else if (i == 1 && token.getText() == "." ||
             i == 3 && token.getText() == "(" ||
             i == 5 && token.getText() == ")")
      return true;
    else if (i == 2) {
      if (matchChannelName(token) < TStageObject::T_ChannelCount)
        return true;
      else
        return token.getText() == "cell" &&
               matchObjectName(previousTokens[0]).isColumn();
    } else
      return false;
  }
  bool isFinished(const std::vector<Token> &previousTokens,
                  const Token &token) const override {
    return previousTokens.size() >= 6;
  }
  bool isComplete(const std::vector<Token> &previousTokens,
                  const Token &token) const override {
    return previousTokens.size() >= 6 || previousTokens.size() == 3;
  }
  TSyntax::TokenType getTokenType(const std::vector<Token> &previousTokens,
                                  const Token &token) const override {
    return TSyntax::Operator;
  }

  void getAcceptableKeywords(
      std::vector<std::string> &keywords) const override {
    const std::string ks[] = {"table",  "tab", "col",   "cam",
                              "camera", "peg", "pegbar"};
    for (int i = 0; i < tArrayCount(ks); i++) keywords.push_back(ks[i]);
  }

  void createNode(Calculator *calc, std::vector<CalculatorNode *> &stack,
                  const std::vector<Token> &tokens) const override {
    assert(tokens.size() >= 3);

    std::unique_ptr<CalculatorNode> frameNode(
        (tokens.size() == 6) ? popNode(stack)
                             : new VariableNode(calc, CalculatorNode::FRAME));

    TStageObjectId objectId = matchObjectName(tokens[0]);

    std::string field = toLower(tokens[2].getText());
    if (field == "cell" || field == "cel" || field == "cels") {
      int columnIndex = objectId.getIndex();
      stack.push_back(new XsheetDrawingCalculatorNode(calc, m_xsh, columnIndex,
                                                      std::move(frameNode)));
    } else {
      TStageObject *object              = m_xsh->getStageObject(objectId);
      TStageObject::Channel channelName = matchChannelName(tokens[2]);
      TDoubleParam *channel             = object->getParam(channelName);
      if (channel)
        stack.push_back(
            new ParamCalculatorNode(calc, channel, std::move(frameNode)));
    }
  }
};

//-------------------------------------------------------------------

class FxReferencePattern final : public Pattern {
  TXsheet *m_xsh;

public:
  FxReferencePattern(TXsheet *xsh) : m_xsh(xsh) {}

  TFx *getFx(const Token &token) const {
    return m_xsh->getFxDag()->getFxById(::to_wstring(toLower(token.getText())));
  }
  TParam *getParam(const TFx *fx, const Token &token) const {
    int i;
    for (i = 0; i < fx->getParams()->getParamCount(); i++) {
      TParam *param         = fx->getParams()->getParam(i);
      std::string paramName = ::to_string(
          TStringTable::translate(fx->getFxType() + "." + param->getName()));
      int i = paramName.find_first_of(" -");
      while (i != std::string::npos) {
        paramName.erase(i, 1);
        i = paramName.find_first_of(" -");
      }
      std::string paramNameToCheck = token.getText();
      if (paramName == paramNameToCheck ||
          toLower(paramName) == toLower(paramNameToCheck))
        return param;
    }
    return 0;
  }
  TParam *getLeafParam(const TFx *fx, const TParamSet *paramSet,
                       const Token &token) const {
    int i;
    for (i = 0; i < paramSet->getParamCount(); i++) {
      TParam *param         = paramSet->getParam(i).getPointer();
      std::string paramName = param->getName();
      int i                 = paramName.find(" ");
      while (i != std::string::npos) {
        paramName.erase(i, 1);
        i = paramName.find(" ");
      }
      std::string paramNameToCheck = token.getText();
      if (paramName == paramNameToCheck ||
          toLower(paramName) == toLower(paramNameToCheck))
        return param;
    }
    return 0;
  }
  std::string getFirstKeyword() const override { return "fx"; }
  bool expressionExpected(
      const std::vector<Token> &previousTokens) const override {
    return !previousTokens.empty() && previousTokens.back().getText() == "(";
  }
  bool matchToken(const std::vector<Token> &previousTokens,
                  const Token &token) const override {
    int i         = (int)previousTokens.size();
    std::string s = toLower(token.getText());
    if (i == 0 && s == "fx")
      return true;
    else if (i == 1)
      return s == ".";
    else if (i & 1) {
      if (previousTokens[i - 2].getText() == "(")
        return s == ")";
      else
        return s == "." || s == "(";
    } else if (i == 2) {
      // nome fx
      return getFx(token) != 0;
    } else if (i == 4) {
      TFx *fx = getFx(previousTokens[2]);
      if (!fx) return false;
      TParam *param = getParam(fx, token);
      return !!param;
    } else if (i == 6) {
      TFx *fx = getFx(previousTokens[2]);
      if (!fx) return false;
      TParam *param       = getParam(fx, previousTokens[4]);
      TParamSet *paramSet = dynamic_cast<TParamSet *>(param);
      if (!paramSet) return false;
      TParam *leafParam = getLeafParam(fx, paramSet, token);
      return !!param;
    } else
      return false;
  }
  bool isFinished(const std::vector<Token> &previousTokens,
                  const Token &token) const override {
    return !previousTokens.empty() && previousTokens.back().getText() == ")";
  }
  bool isComplete(const std::vector<Token> &previousTokens,
                  const Token &token) const override {
    int n = (int)previousTokens.size();
    return n >= 2 && (n & 1) == 1 && previousTokens[n - 2].getText() != "(";
  }
  TSyntax::TokenType getTokenType(const std::vector<Token> &previousTokens,
                                  const Token &token) const override {
    return TSyntax::Operator;
  }

  void createNode(Calculator *calc, std::vector<CalculatorNode *> &stack,
                  const std::vector<Token> &tokens) const override {
    int tokenSize = tokens.size();

    std::unique_ptr<CalculatorNode> frameNode(
        (tokenSize > 0 && tokens.back().getText() == ")")
            ? popNode(stack)
            : new VariableNode(calc, CalculatorNode::FRAME));

    TFx *fx = getFx(tokens[2]);
    if (!fx || tokenSize < 4) return;

    TParamP param = getParam(fx, tokens[4]);
    if (!param.getPointer()) return;

    TDoubleParamP channel;
    TParamSet *paramSet = dynamic_cast<TParamSet *>(param.getPointer());

    if (paramSet && tokenSize > 6)
      channel =
          dynamic_cast<TDoubleParam *>(getLeafParam(fx, paramSet, tokens[6]));
    else
      channel = param;

    if (channel.getPointer())
      stack.push_back(
          new ParamCalculatorNode(calc, channel, std::move(frameNode)));
  }
};

//-------------------------------------------------------------------

class PlasticVertexPattern final : public Pattern {
  TXsheet *m_xsh;

  /*
Full pattern layout:

vertex ( columnNumber , " vertexName " ) . component (  expr )
   0   1      2       3 4      5     6 7 8     9     10  11  12
*/

  enum Positions {
    OBJECT,
    L1,
    COLUMN_NUMBER,
    COMMA,
    QUOTE1,
    VERTEX_NAME,
    QUOTE2,
    R1,
    SELECTOR,
    COMPONENT,
    L2,
    EXPR,
    R2,
    POSITIONS_COUNT
  };

public:
  PlasticVertexPattern(TXsheet *xsh) : m_xsh(xsh) {
    setDescription(
        "vertex(columnNumber, \"vertexName\").action\nVertex data\n"
        "columnNumber must be the number of the column containing the desired "
        "skeleton\n"
        "vertexName must be the name of a Plastic Skeleton vertex\n"
        "action must be one of the parameter names available for a Plastic "
        "Skeleton vertex");
  }

  std::string getFirstKeyword() const override { return "vertex"; }

  bool expressionExpected(
      const std::vector<Token> &previousTokens) const override {
    return (previousTokens.size() == EXPR);
  }

  bool matchToken(const std::vector<Token> &previousTokens,
                  const Token &token) const override {
    struct {
      const PlasticVertexPattern *m_this;
      const SkD *skdp(const Token &columnToken) {
        int colIdx = columnToken.getIntValue() -
                     1;  // The first column (1) actually starts at index 0

        if (!m_this->m_xsh->isColumnEmpty(colIdx)) {
          TStageObject *obj =
              m_this->m_xsh->getStageObject(TStageObjectId::ColumnId(colIdx));
          assert(obj);

          if (const SkDP &skdp = obj->getPlasticSkeletonDeformation())
            return skdp.getPointer();
        }

        return 0;
      }
    } locals = {this};

    const std::string &text = token.getText();
    int pos                 = previousTokens.size();

    if (!m_fixedTokens[pos].empty()) return (text == m_fixedTokens[pos]);

    switch (pos) {
    case COLUMN_NUMBER:
      return (token.getType() == Token::Number && locals.skdp(token));

    case VERTEX_NAME:
      if (const SkD *skdp = locals.skdp(previousTokens[COLUMN_NUMBER])) {
        const QString &vertexName = QString::fromStdString(text);
        return (skdp->vertexDeformation(vertexName) != 0);
      }
      break;

    case COMPONENT:
      return std::count(m_components,
                        m_components + sizeof(m_components) / sizeof(Component),
                        text) > 0;
    }

    return false;
  }

  bool isFinished(const std::vector<Token> &previousTokens,
                  const Token &token) const override {
    return (previousTokens.size() >= POSITIONS_COUNT);
  }

  bool isComplete(const std::vector<Token> &previousTokens,
                  const Token &token) const override {
    return (previousTokens.size() >= POSITIONS_COUNT ||
            previousTokens.size() == L2);
  }

  TSyntax::TokenType getTokenType(const std::vector<Token> &previousTokens,
                                  const Token &token) const override {
    return TSyntax::Operator;
  }

  void createNode(Calculator *calc, std::vector<CalculatorNode *> &stack,
                  const std::vector<Token> &tokens) const override {
    assert(tokens.size() > COMPONENT);

    std::unique_ptr<CalculatorNode> frameNode(
        (tokens.size() == POSITIONS_COUNT)
            ? popNode(stack)
            : new VariableNode(calc, CalculatorNode::FRAME));

    int colIdx = tokens[COLUMN_NUMBER].getIntValue() - 1;
    if (!m_xsh->isColumnEmpty(colIdx)) {
      TStageObject *obj =
          m_xsh->getStageObject(TStageObjectId::ColumnId(colIdx));
      assert(obj);

      if (const SkDP &skdp = obj->getPlasticSkeletonDeformation()) {
        const QString &vertexName =
            QString::fromStdString(tokens[VERTEX_NAME].getText());
        if (SkVD *skvd = skdp->vertexDeformation(vertexName)) {
          const Component *componentsEnd =
                              m_components +
                              sizeof(m_components) / sizeof(Component),
                          *component = std::find(m_components, componentsEnd,
                                                 tokens[COMPONENT].getText());

          if (component != componentsEnd) {
            const TDoubleParamP &param =
                skvd->m_params[component->m_paramId].getPointer();
            stack.push_back(
                new ParamCalculatorNode(calc, param, std::move(frameNode)));
          }
        }
      }
    }
  }

private:
  struct Component {
    std::string m_name;
    SkVD::Params m_paramId;

    bool operator==(const std::string &name) const { return (m_name == name); }
  };

private:
  static const std::string m_fixedTokens[POSITIONS_COUNT];
  static const Component m_components[5];
};

const std::string PlasticVertexPattern::m_fixedTokens[POSITIONS_COUNT] = {
    "vertex", "(", "", ",", "\"", "", "\"", ")", ".", "", "(", "", ")"};

const PlasticVertexPattern::Component PlasticVertexPattern::m_components[] = {
    {"ang", SkVD::ANGLE},
    {"angle", SkVD::ANGLE},
    {"dist", SkVD::DISTANCE},
    {"distance", SkVD::DISTANCE},
    {"so", SkVD::SO}};

}  // namespace

//******************************************************************************
//    API functions
//******************************************************************************

TSyntax::Grammar *createXsheetGrammar(TXsheet *xsh) {
  Grammar *grammar = new Grammar();
  grammar->addPattern(new XsheetReferencePattern(xsh));
  grammar->addPattern(new FxReferencePattern(xsh));
  grammar->addPattern(new PlasticVertexPattern(xsh));
  return grammar;
}

bool dependsOn(TExpression &expr, TDoubleParam *possiblyDependentParam) {
  ParamDependencyFinder pdf(possiblyDependentParam);
  expr.accept(pdf);
  return pdf.found();
}

bool dependsOn(TDoubleParam *param, TDoubleParam *possiblyDependentParam) {
  ParamDependencyFinder pdf(possiblyDependentParam);
  param->accept(pdf);
  return pdf.found();
}
