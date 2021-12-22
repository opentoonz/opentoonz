#pragma once

#ifndef TPARSER_INCLUDED
#define TPARSER_INCLUDED

#include <memory>

#include "tcommon.h"
#include "tgrammar.h"

#undef DVAPI
#undef DVVAR
#ifdef TNZBASE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

namespace TSyntax {

struct DVAPI SyntaxToken {
  int m_pos, m_length;
  TokenType m_type;
};

class DVAPI Parser {
  class Imp;
  std::unique_ptr<Imp> m_imp;

public:
  Parser(const Grammar *grammar);
  ~Parser();

  //! parse the input string and create the corresponding calculator
  //! (returns 0 if the text contains mistakes)
  Calculator *parse(std::string text);

  //! return true if the last parsed string was correct
  bool isValid() const;

  //! return true if the last parsed string was correct
  bool hasReference() const;

  //! return the last parsed string
  std::string getText() const;

  //! return the last error code (if the last parsed string was correct then
  //! returns "")
  std::string getError() const;

  //! if getError() != "" returns the position of the last parsed token
  //! the pair contains the indices of the first and the last characters of the
  //! token
  std::pair<int, int> getErrorPos() const;

  enum SyntaxStatus { Correct, Incomplete, Error, ExtraIgnored };
  SyntaxStatus checkSyntax(std::vector<SyntaxToken> &tokens, std::string text);

  void getSuggestions(Grammar::Suggestions &suggestions, std::string text);
  std::string getCurrentPatternString(std::string text);

private:
  // not implemented
  Parser(const Parser &);
  Parser &operator=(const Parser &);
};

}  // namespace TSyntax

#endif
