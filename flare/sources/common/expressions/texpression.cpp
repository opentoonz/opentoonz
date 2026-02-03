

// TnzBase includes
#include "tgrammar.h"
#include "tparser.h"
#include "tunit.h"

#include "texpression.h"

using namespace TSyntax;

//**********************************************************************************
//    TExpression::Imp  definition
//**********************************************************************************

class TExpression::Imp {
public:
  const Grammar *m_grammar;  //!< (not owned) The expression's grammar
  TDoubleParam *m_param;     //!< (not owned) The expression's owner
  Calculator *m_calculator;  //!< (owned) Expression calculator object

  std::string m_text;  //!< Text content

  std::string m_error;  //!< Description of an error in the expression's text
  std::pair<int, int>
      m_errorPos;  //!< Position of the error in the expression's text

  bool m_isValid,       //!< Whether the expression is valid
      m_hasBeenParsed,  //!< Whether the expression has already been parsed
      m_hasReference;

public:
  Imp()
      : m_grammar(0)
      , m_param(0)
      , m_calculator(0)
      , m_errorPos(0, -1)
      , m_isValid(false)
      , m_hasBeenParsed(true)
      , m_hasReference(false) {}
  ~Imp() { delete m_calculator; }
};

//**********************************************************************************
//    TExpression  implementation
//**********************************************************************************

TExpression::TExpression() : m_imp(new Imp()) {}

//--------------------------------------------------------------------------

TExpression::~TExpression() {}

//--------------------------------------------------------------------------

TExpression::TExpression(const TExpression &src) : m_imp(new Imp()) {
  m_imp->m_grammar       = src.m_imp->m_grammar;
  m_imp->m_param         = src.m_imp->m_param;
  m_imp->m_text          = src.m_imp->m_text;
  m_imp->m_calculator    = 0;
  m_imp->m_isValid       = src.m_imp->m_isValid;
  m_imp->m_error         = src.m_imp->m_error;
  m_imp->m_errorPos      = src.m_imp->m_errorPos;
  m_imp->m_hasBeenParsed = false;
  m_imp->m_hasReference  = src.m_imp->m_hasReference;
}

//--------------------------------------------------------------------------

TExpression &TExpression::operator=(TExpression copy) {
  std::swap(copy.m_imp, m_imp);
  return *this;
}

//--------------------------------------------------------------------------

void TExpression::setGrammar(const TSyntax::Grammar *grammar) {
  m_imp->m_grammar       = grammar;
  m_imp->m_hasBeenParsed = false;
}

//--------------------------------------------------------------------------

const TSyntax::Grammar *TExpression::getGrammar() const {
  return m_imp->m_grammar;
}

//--------------------------------------------------------------------------

void TExpression::setText(std::string text) {
  if (m_imp->m_text != text || m_imp->m_hasReference) {
    m_imp->m_text = text;
    delete m_imp->m_calculator;
    m_imp->m_calculator    = 0;
    m_imp->m_isValid       = false;
    m_imp->m_hasReference  = false;
    m_imp->m_hasBeenParsed = false;
    m_imp->m_error         = "";
    m_imp->m_errorPos      = std::make_pair(0, -1);
  }
}

//--------------------------------------------------------------------------

std::string TExpression::getText() const { return m_imp->m_text; }

//--------------------------------------------------------------------------

// note: unit is used by the CycleNode node only
TSyntax::Calculator *TExpression::getCalculator() {
  if (!m_imp->m_hasBeenParsed) parse();
  if (m_imp->m_isValid && m_imp->m_calculator)
    return m_imp->m_calculator;
  else
    return 0;
}

//--------------------------------------------------------------------------

bool TExpression::isValid() {
  if (!m_imp->m_hasBeenParsed) parse();
  return m_imp->m_isValid;
}

//--------------------------------------------------------------------------

std::string TExpression::getError() const { return m_imp->m_error; }

//--------------------------------------------------------------------------

std::pair<int, int> TExpression::getErrorPos() const {
  return m_imp->m_errorPos;
}

//--------------------------------------------------------------------------

void TExpression::accept(TSyntax::CalculatorNodeVisitor &visitor) {
  if (!m_imp->m_hasBeenParsed) parse();
  if (m_imp->m_isValid && m_imp->m_calculator)
    m_imp->m_calculator->accept(visitor);
}

//--------------------------------------------------------------------------

void TExpression::parse() {
  delete m_imp->m_calculator;
  m_imp->m_calculator = 0;

  m_imp->m_errorPos     = std::make_pair(0, -1);
  m_imp->m_error        = std::string();
  m_imp->m_hasReference = false;

  if (!m_imp->m_grammar) {
    m_imp->m_error   = "No grammar defined";
    m_imp->m_isValid = false;
  } else {
    Parser parser(m_imp->m_grammar);

    m_imp->m_calculator = parser.parse(m_imp->m_text);

    if (m_imp->m_calculator)
      m_imp->m_calculator->setOwnerParameter(m_imp->m_param);

    m_imp->m_isValid      = parser.isValid();
    m_imp->m_hasReference = parser.hasReference();

    if (!m_imp->m_isValid) {
      m_imp->m_error    = parser.getError();
      m_imp->m_errorPos = parser.getErrorPos();
    }
  }

  m_imp->m_hasBeenParsed = true;
}

//--------------------------------------------------------------------------

void TExpression::setOwnerParameter(TDoubleParam *param) {
  m_imp->m_param = param;

  if (m_imp->m_calculator) m_imp->m_calculator->setOwnerParameter(param);
}

//--------------------------------------------------------------------------

TDoubleParam *TExpression::getOwnerParameter() const { return m_imp->m_param; }

//--------------------------------------------------------------------------

bool TExpression::isCycling() const {
  // TODO: this is a quick&dirty implementation to be replaced with a more
  // "semantic" one
  return getText().find("cycle") != std::string::npos;
}
