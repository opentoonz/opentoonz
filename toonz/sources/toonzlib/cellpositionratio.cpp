#include "toonz/cellpositionratio.h"

#include <stdexcept>
#include <algorithm>

// Euclid's algorithm
int greatestCommonDivisor(int a, int b) {
  a     = std::abs(a);
  b     = std::abs(b);
  int c = std::max(a, b);
  int d = std::min(a, b);
  while (d) {
    int q = c / d;
    int r = c % d;
    c     = d;
    d     = r;
  }
  return c;
}

int leastCommonMultiple(int a, int b) {
  return a * b / greatestCommonDivisor(a, b);
}

void Ratio::normalize() {
  int gcd              = greatestCommonDivisor(m_num, m_denom);
  if (m_denom < 0) gcd = -gcd;
  m_num /= gcd;
  m_denom /= gcd;
}

Ratio Ratio::normalized() const {
  Ratio copy(*this);
  copy.normalize();
  return copy;
}

Ratio::Ratio(int num, int denom) : m_num(num), m_denom(denom) {
  if (!denom) throw std::runtime_error("ratio with denominator == 0");
  normalize();
}

Ratio operator*(const Ratio &a, const Ratio &b) {
  return Ratio(a.m_num * b.m_num, a.m_denom * b.m_denom);
}
Ratio operator/(const Ratio &a, const Ratio &b) {
  return Ratio(a.m_num * b.m_denom, a.m_denom * b.m_num);
}

Ratio operator+(const Ratio &a, const Ratio &b) {
  int gcd   = greatestCommonDivisor(a.m_denom, b.m_denom);
  int denom = a.m_denom * b.m_denom / gcd;
  int aMult = b.m_denom * gcd;
  int bMult = a.m_denom * gcd;
  return Ratio(a.m_num * aMult + b.m_num * bMult, denom);
}

Ratio operator-(const Ratio &a, const Ratio &b) {
  int gcd   = greatestCommonDivisor(a.m_denom, b.m_denom);
  int denom = a.m_denom * b.m_denom / gcd;
  int aMult = b.m_denom * gcd;
  int bMult = a.m_denom * gcd;
  return Ratio(a.m_num * aMult - b.m_num * bMult, denom);
}

int operator*(const Ratio &a, int b) { return a.m_num * b / a.m_denom; }
