

#include "ttokenizer.h"
#include <QString>

namespace TSyntax {

int Token::getIntValue() const {
  return QString::fromStdString(getText()).toInt();
}

double Token::getDoubleValue() const {
  return QString::fromStdString(getText()).toDouble();
}

//===================================================================

Tokenizer::Tokenizer() : m_buffer(), m_index(0) {}

//-------------------------------------------------------------------

Tokenizer::Tokenizer(std::string buffer) : m_buffer(), m_index(0) {
  setBuffer(buffer);
}

//===================================================================

Tokenizer::~Tokenizer() {}

//-------------------------------------------------------------------

void Tokenizer::setBuffer(std::string buffer) {
  m_buffer = buffer + '\0';
  m_index  = 0;
  m_tokens.clear();

  bool stringBlock = false;

  int i         = 0;
  const char *s = &m_buffer[0];
  for (;;) {
    while (isascii(s[i]) && isspace(s[i])) i++;

    int j = i;

    if (s[i] == '\0') {
      m_tokens.push_back(Token("", Token::Eos, j));
      break;
    }

    if (s[i] == '"') {
      stringBlock = !stringBlock;
      m_tokens.push_back(Token("\"", Token::Punct, j));

      ++i;
      continue;
    }

    std::string token;

    if (stringBlock) {
      // string block - read mercilessly until either another '"' or EOS
      token = std::string(1, s[i++]);

      while (s[i] != '"' && s[i] != '\0') token.append(1, s[i++]);

      m_tokens.push_back(Token(token, Token::Ident, j));
    } else if ((isascii(s[i]) && isalpha(s[i])) || s[i] == '_') {
      // ident
      token = std::string(1, s[i++]);

      while (isascii(s[i]) && (isalpha(s[i]) || s[i] == '_' || isdigit(s[i])))
        token.append(1, s[i++]);

      m_tokens.push_back(Token(token, Token::Ident, j));
    } else if ((isascii(s[i]) && isdigit(s[i])) || s[i] == '.') {
      // number
      while (isascii(s[i]) && isdigit(s[i])) token.append(1, s[i++]);

      if (s[i] == '.') {
        token.append(1, s[i++]);

        while (isascii(s[i]) && isdigit(s[i])) token.append(1, s[i++]);

        if ((s[i] == 'e' || s[i] == 'E') &&
            (((isascii(s[i + 1]) && isdigit(s[i + 1])) ||
             s[i + 1] == '-' || s[i + 1] == '+') && isascii(s[i + 2]) &&
                 isdigit(s[i + 2]))) {
          token.append(1, s[i++]);

          if (s[i] == '-' || s[i] == '+') token.append(1, s[i++]);

          while (isascii(s[i]) && isdigit(s[i])) token.append(1, s[i++]);
        }
      }
      m_tokens.push_back(Token(token, Token::Number, j));
    } else {
      // punct.
      if (s[i + 1] != '\0') {
        token = std::string(s + i, 2);

        const std::string ss[] = {"==", "!=", ">=", "<=", "||", "&&"};

        if (std::find(std::begin(ss), std::end(ss), token) != std::end(ss))
          i += 2;
        else
          token = std::string(1, s[i++]);
      } else
        token = std::string(1, s[i++]);

      m_tokens.push_back(Token(token, Token::Punct, j));
    }
  }
}

//-------------------------------------------------------------------

int Tokenizer::getTokenCount() const { return m_tokens.size(); }

//-------------------------------------------------------------------

const Token &Tokenizer::getToken(int index) const {
  assert(0 <= index && index < getTokenCount());
  return m_tokens[index];
}

//-------------------------------------------------------------------

void Tokenizer::reset() { m_index = 0; }

//-------------------------------------------------------------------

const Token &Tokenizer::getToken() { return getToken(m_index); }

//-------------------------------------------------------------------

Token Tokenizer::nextToken() {
  Token token = getToken();
  if (m_index + 1 < getTokenCount()) m_index++;
  return token;
}

//-------------------------------------------------------------------

bool Tokenizer::eos() const { return m_index + 1 == getTokenCount(); }

//-------------------------------------------------------------------

Token Tokenizer::getTokenFromPos(int pos) const {
  int len = m_buffer.length();
  if (pos < 0 || pos >= len) return Token(pos);
  int x = 0;
  for (int i = 0; i < getTokenCount(); i++) {
    const Token &token = getToken(i);
    int y              = token.getPos();
    if (pos < y) {
      assert(x < y);
      return Token(x, y - 1);
    }
    x = y + (int)token.getText().length();
    if (pos < x) return token;
  }
  assert(x < len);
  return Token(x, len - 1);
}

//===================================================================

}  // namespace TSyntax
