#pragma once

#ifndef TNZ_EXPRESSIONFIELD_INCLUDED
#define TNZ_EXPRESSIONFIELD_INCLUDED

#include "tw/textfield.h"

#undef DVAPI
#undef DVVAR
#ifdef TWIN_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

#ifdef WIN32
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

// forward declaration
namespace TSyntax {
class Parser;
class Grammar;
}

class DVAPI TExpressionField : public TTextField {
public:
  class Listener {
  public:
    virtual void onExpressionChange(string s, bool isValid) = 0;
    virtual ~Listener() {}
  };

  TSyntax::Parser *m_parser;
  Listener *m_listener;
  void apply();

public:
  TExpressionField(TWidget *parent, string name = "expressionField");
  ~TExpressionField();

  void setGrammar(TSyntax::Grammar *grammar);
  TSyntax::Grammar *getGrammar() const;

  void setListener(Listener *listener) { m_listener = listener; }

  void keyDown(int key, TUINT32 status, const TPoint &p);
  void drawFieldText(const TPoint &origin, wstring text);

  string getError() const;
};

#ifdef WIN32
#pragma warning(pop)
#endif

#endif
