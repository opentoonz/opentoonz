#pragma once

#ifndef TTOKENIZER_INCLUDED
#define TTOKENIZER_INCLUDED

#include "tcommon.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

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

//-------------------------------------------------------------------

class DVAPI Token {
public:
  enum Type { None, Space, Ident, Number, Punct, Eos };

  Token(int pos = 0) : m_text(""), m_type(None), m_pos(pos) {}
  Token(int p0, int p1) : m_text(p1 - p0 + 1, ' '), m_type(Space), m_pos(p0) {}

  Token(std::string text, Type type, int pos)
      : m_text(text), m_type(type), m_pos(pos) {}

  Type getType() const { return m_type; }

  std::string getText() const { return m_text; }
  int getIntValue() const;
  double getDoubleValue() const;

  int getPos() const { return m_pos; }
  int getPos1() const { return m_pos + m_text.length() - 1; }

private:
  std::string m_text;
  int m_pos;
  Type m_type;
};

//-------------------------------------------------------------------

class DVAPI Tokenizer {
  std::string m_buffer;
  std::vector<Token> m_tokens;
  int m_index;

public:
  Tokenizer();
  Tokenizer(std::string buffer);
  ~Tokenizer();

  void setBuffer(std::string buffer);
  std::string getBuffer() const { return m_buffer; }

  int getTokenCount() const;
  const Token &getToken(int index) const;

  //! get the token containing the pos-th character in the input string
  Token getTokenFromPos(int pos) const;

  //! reset the token index. (set it to 0)
  void reset();

  //! return the current token (possibly Eos)
  const Token &getToken();

  //! same as getToken(), but post-increment the token index (if !eos())
  Token nextToken();

  //! return true if the current token is the last one
  bool eos() const;

  //! to read all the sequence:
  //! while(!tokenizer.eos()) {token = tokenizer.nextToken();...}
};

//-------------------------------------------------------------------

}  // namespace TSyntax

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
