#pragma once

#ifndef TEXPRESSION_INCLUDED
#define TEXPRESSION_INCLUDED

#include <memory>

// TnzCore includes
#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef TNZBASE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//========================================================

//    Forward declaration

class TUnit;
class TDoubleParam;

namespace TSyntax {
class Grammar;
class CalculatorNodeVisitor;
class Calculator;
}

//========================================================

//************************************************************************************
//    TExpression  declaration
//************************************************************************************

//! This class manages a generic expression.
/*!
  An expression is a sequence of characthers and contains:

  \li a \e CalcNode that is a basic element of the expression that must be
  evaluated;
  \li a \e Grammar that is a sequence of string patterns that define operations
  on nodes;
  \li a \e Builder that is a sequence or a tree of calcnodes and operations on
  that;
*/
class DVAPI TExpression {
  class Imp;
  std::unique_ptr<Imp> m_imp;

public:
  TExpression();
  ~TExpression();

  TExpression(const TExpression &);
  TExpression &operator=(TExpression);

  //! Sets the expression's grammar - does \b not acquire ownership.
  void setGrammar(const TSyntax::Grammar *grammar);
  const TSyntax::Grammar *getGrammar() const;

  /*!
Sets the expression's text - a mix of mathematical expressions
and grammar-recognized patterns that is able to calculate the
expression's value at a given frame.
*/
  void setText(std::string text);
  std::string getText() const;

  TSyntax::Calculator *getCalculator();

  /*!
          Returns whether the expression is valid according to actual
grammar syntax (i.e. it has at least one calculator node).
  */
  bool isValid();

  std::string getError() const;
  std::pair<int, int> getErrorPos() const;

  void accept(TSyntax::CalculatorNodeVisitor &visitor);

  void setOwnerParameter(TDoubleParam *param);
  TDoubleParam *getOwnerParameter() const;

  bool isCycling() const;

private:
  // The expression is parsed lazily - ie when a getters attempts to
  // evaluate the expression and it was not yet parsed
  void parse();
};

#endif
