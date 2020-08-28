

#include "tgeometry.h"


using namespace std;

const T3DPointD TConsts::nap3d((numeric_limits<double>::max)(),
                               (numeric_limits<double>::max)(),
                               (numeric_limits<double>::max)());

const TThickPoint TConsts::natp((numeric_limits<double>::max)(),
                                (numeric_limits<double>::max)(),
                                (numeric_limits<double>::max)());

const TPointD TConsts::napd((numeric_limits<double>::max)(),
                            (numeric_limits<double>::max)());

const TPointI TConsts::nap((numeric_limits<int>::max)(),
                           (numeric_limits<int>::max)());

const TRectD TConsts::infiniteRectD(-(numeric_limits<double>::max)(),
                                    -(numeric_limits<double>::max)(),
                                    (numeric_limits<double>::max)(),
                                    (numeric_limits<double>::max)());

const TRectI TConsts::infiniteRectI(-(numeric_limits<int>::max)(),
                                    -(numeric_limits<int>::max)(),
                                    (numeric_limits<int>::max)(),
                                    (numeric_limits<int>::max)());

//==================================================================================================

TAffine TAffine::operator*(const TAffine &b) const {
  return TAffine(transformDirection(b.rowX()), transformDirection(b.rowY()),
                 *this * b.rowW());
}

//--------------------------------------------------------------------------------------------------

double TAffine::det() const { return m00 * m11 - m10 * m01; }

//--------------------------------------------------------------------------------------------------

TAffine TAffine::inv() const {
  if (m01 == 0.0 && m10 == 0.0) {
    if (m00 == 0.0 || m11 == 0.0) return zero();
    double inv_m00 = 1.0 / m00;
    double inv_m11 = 1.0 / m11;
    return TAffine(TPointD(inv_m00, 0.0), TPointD(0.0, inv_m11),
                   TPointD(-m20 * inv_m00, -m21 * inv_m11));
  } else if (m00 == 0.0 && m11 == 0.0) {
    if (m01 == 0.0 || m10 == 0.0) return zero();
    double inv_m01 = 1.0 / m01;
    double inv_m10 = 1.0 / m10;
    return TAffine(TPointD(0.0, inv_m10), TPointD(inv_m01, 0.0),
                   TPointD(-m21 * inv_m01, -m20 * inv_m10));
  } else {
    double d = det();
    if (d == 0.0) return zero();
    d = 1.0 / d;
    return TAffine(
        TPointD(m11 * d, -m01 * d), TPointD(-m10 * d, m00 * d),
        TPointD((m10 * m21 - m11 * m20) * d, (m01 * m20 - m00 * m21) * d));
  }
}

//--------------------------------------------------------------------------------------------------

bool TAffine::operator==(const TAffine &a) const {
  return m00 == a.m00 && m01 == a.m01 && m10 == a.m10 && m11 == a.m11 &&
         m20 == a.m20 && m21 == a.m21;
}

//--------------------------------------------------------------------------------------------------

bool TAffine::operator!=(const TAffine &a) const {
  return m00 != a.m00 || m01 != a.m01 || m10 != a.m10 || m11 != a.m11 ||
         m20 != a.m20 || m21 != a.m21;
}

//--------------------------------------------------------------------------------------------------

bool TAffine::isZero(double err) const {
  return fabs(m00) <= err && fabs(m01) <= err && fabs(m10) <= err &&
         fabs(m11) <= err && fabs(m20) <= err && fabs(m21) <= err;
}

bool TAffine::isIdentity(double err) const {
  return ((m00 - 1.0) * (m00 - 1.0) + (m11 - 1.0) * (m11 - 1.0) + m01 * m01 +
          m10 * m10 + m20 * m20 + m21 * m21) < err;
}

//--------------------------------------------------------------------------------------------------

bool TAffine::isTranslation(double err) const {
  return ((m00 - 1.0) * (m00 - 1.0) + (m11 - 1.0) * (m11 - 1.0) + m01 * m01 +
          m10 * m10) < err;
}

//--------------------------------------------------------------------------------------------------

bool TAffine::isIsotropic(double err) const {
  return areAlmostEqual(m00, m11, err) && areAlmostEqual(m01, -m10, err);
}

//--------------------------------------------------------------------------------------------------

TPointD TAffine::operator*(const TPointD &p) const {
  return TPointD(p.x * m00 + p.y * m10 + m20, p.x * m01 + p.y * m11 + m21);
}

//--------------------------------------------------------------------------------------------------

TPointD TAffine::transformDirection(const TPointD &p) const {
  return TPointD(p.x * m00 + p.y * m10, p.x * m01 + p.y * m11);
}

//--------------------------------------------------------------------------------------------------

TRectD TAffine::operator*(const TRectD &rect) const {
  if (rect == TConsts::infiniteRectD) return TConsts::infiniteRectD;
  TPointD p1 = *this * rect.getP00(), p2 = *this * rect.getP01(),
          p3 = *this * rect.getP10(), p4 = *this * rect.getP11();
  return TRectD(
      std::min({p1.x, p2.x, p3.x, p4.x}), std::min({p1.y, p2.y, p3.y, p4.y}),
      std::max({p1.x, p2.x, p3.x, p4.x}), std::max({p1.y, p2.y, p3.y, p4.y}));
}

//--------------------------------------------------------------------------------------------------

TAffine TAffine::place(double u, double v, double x, double y) const {
  return TAffine(rowX(), rowY(),
                 TPointD(x - (m00 * u + m10 * v), y - (m01 * u + m11 * v)));
}

TAffine TAffine::rotation(double angle) {
  double s = sin(angle);
  double c = cos(angle);
  return TAffine(TPointD(c, s), TPointD(-s, c));
}

//==================================================================================================

TPoint4D TAffine4::operator*(const TPoint4D &b) const {
  return TPoint4D(b.x * m00 + b.y * m10 + b.z * m20 + b.w * m30,
                  b.x * m01 + b.y * m11 + b.z * m21 + b.w * m31,
                  b.x * m02 + b.y * m12 + b.z * m22 + b.w * m32,
                  b.x * m03 + b.y * m13 + b.z * m23 + b.w * m33);
}

TAffine4 TAffine4::operator*(const TAffine4 &b) const {
  return TAffine4(*this * b.rowX(), *this * b.rowY(), *this * b.rowZ(),
                  *this * b.rowW());
}

TAffine4 TAffine4::operator*=(const TAffine4 &b) { return *this = *this * b; }

TAffine4 TAffine4::inv() const {
  TAffine4 r;
  r.m00 = m11 * (m22 * m33 - m23 * m32) + m12 * (m23 * m31 - m21 * m33) +
          m13 * (m21 * m32 - m22 * m31);
  r.m01 = m10 * (m23 * m32 - m22 * m33) + m12 * (m20 * m33 - m23 * m30) +
          m13 * (m22 * m30 - m20 * m32);
  r.m02 = m10 * (m21 * m33 - m23 * m31) + m11 * (m23 * m30 - m20 * m33) +
          m13 * (m20 * m31 - m21 * m30);
  r.m03 = m10 * (m22 * m31 - m21 * m32) + m11 * (m20 * m32 - m22 * m30) +
          m12 * (m21 * m30 - m20 * m31);

  double det = m00 * r.m00 + m01 * r.m10 + m02 * r.m20 + m03 * r.m30;
  if (fabs(det) <= TConsts::epsilon) return zero();
  det = 1.0 / det;
  r.m00 *= det;
  r.m01 *= det;
  r.m02 *= det;
  r.m03 *= det;

  r.m10 = det * (m01 * (m23 * m32 - m22 * m33) + m02 * (m21 * m33 - m23 * m31) +
                 m03 * (m22 * m31 - m21 * m32));
  r.m11 = det * (m00 * (m22 * m33 - m23 * m32) + m02 * (m23 * m30 - m20 * m33) +
                 m03 * (m20 * m32 - m22 * m30));
  r.m12 = det * (m00 * (m23 * m31 - m21 * m33) + m01 * (m20 * m33 - m23 * m30) +
                 m03 * (m21 * m30 - m20 * m31));
  r.m13 = det * (m00 * (m21 * m32 - m22 * m31) + m01 * (m22 * m30 - m20 * m32) +
                 m02 * (m20 * m31 - m21 * m30));
  r.m20 = det * (m01 * (m12 * m33 - m13 * m32) + m02 * (m13 * m31 - m11 * m33) +
                 m03 * (m11 * m32 - m12 * m31));
  r.m21 = det * (m00 * (m13 * m32 - m12 * m33) + m02 * (m10 * m33 - m13 * m30) +
                 m03 * (m12 * m30 - m10 * m32));
  r.m22 = det * (m00 * (m11 * m33 - m13 * m31) + m01 * (m13 * m30 - m10 * m33) +
                 m03 * (m10 * m31 - m11 * m30));
  r.m23 = det * (m00 * (m12 * m31 - m11 * m32) + m01 * (m10 * m32 - m12 * m30) +
                 m02 * (m11 * m30 - m10 * m31));
  r.m30 = det * (m01 * (m13 * m22 - m12 * m23) + m02 * (m11 * m23 - m13 * m21) +
                 m03 * (m12 * m21 - m11 * m22));
  r.m31 = det * (m00 * (m12 * m23 - m13 * m22) + m02 * (m13 * m20 - m10 * m23) +
                 m03 * (m10 * m22 - m12 * m20));
  r.m32 = det * (m00 * (m13 * m21 - m11 * m23) + m01 * (m10 * m23 - m13 * m20) +
                 m03 * (m11 * m20 - m10 * m21));
  r.m33 = det * (m00 * (m11 * m22 - m12 * m21) + m01 * (m12 * m20 - m10 * m22) +
                 m02 * (m10 * m21 - m11 * m20));

  return r;
}

TAffine TAffine4::get2d(double z) const {
  return TAffine(TPointD(m00, m01), TPointD(m10, m11),
                 TPointD(z * m20 + m30, z * m21 + m31));
}

TAffine4 TAffine4::translation(double x, double y, double z) {
  TAffine4 r;
  r.rowW().x = x;
  r.rowW().y = y;
  r.rowW().z = z;
  return r;
}

TAffine4 TAffine4::scale(double x, double y, double z) {
  TAffine4 r;
  r.m00 = x;
  r.m11 = y;
  r.m22 = z;
  return r;
}

TAffine4 TAffine4::rotation(double x, double y, double z, double angle) {
  TAffine4 r;
  double k = x * x + y * y + z * z;
  if (k > TConsts::epsilon * TConsts::epsilon) {
    k         = 1.0 / sqrt(k);
    double s  = sin(angle);
    double c  = cos(angle);
    double ic = 1.0 - c;
    x *= k;
    y *= k;
    z *= k;

    r.m00 = ic * x * x + c;
    r.m01 = ic * x * y + s * z;
    r.m02 = ic * z * x - s * y;

    r.m10 = ic * x * y - s * z;
    r.m11 = ic * y * y + c;
    r.m12 = ic * y * z + s * x;

    r.m20 = ic * z * x + s * y;
    r.m21 = ic * y * z - s * x;
    r.m22 = ic * z * z + c;
  }
  return r;
}

TAffine4 TAffine4::rotationX(double angle) {
  TAffine4 r;
  double s = sin(angle);
  double c = cos(angle);
  r.m11    = c;
  r.m12    = s;
  r.m21    = -s;
  r.m22    = c;
  return r;
}

TAffine4 TAffine4::rotationY(double angle) {
  TAffine4 r;
  double s = sin(angle);
  double c = cos(angle);
  r.m00    = c;
  r.m02    = -s;
  r.m20    = s;
  r.m22    = c;
  return r;
}

TAffine4 TAffine4::rotationZ(double angle) {
  TAffine4 r;
  double s = sin(angle);
  double c = cos(angle);
  r.m00    = c;
  r.m01    = s;
  r.m10    = -s;
  r.m11    = c;
  return r;
}

//==================================================================================================

int TAngleRangeSet::find(Type a) const {
  assert(!m_angles.empty());
  int i0 = 0, i1 = m_angles.size() - 1;
  if (a < m_angles[0]) return i1;
  if (m_angles[i1] <= a) return i1;
  while (true) {
    int i = (i1 + i0) / 2;
    if (i == i0) break;
    if (m_angles[i] <= a)
      i0 = i;
    else
      i1 = i;
  }
  return i0;
}

void TAngleRangeSet::insert(Type a) {
  if (m_angles.empty()) {
    m_angles.push_back(a);
  } else {
    int i = find(a);
    if (m_angles[i] == a)
      m_angles.erase(m_angles.begin() + i);
    else if (a < m_angles[0])
      m_angles.insert(m_angles.begin(), a);
    else
      m_angles.insert(m_angles.begin() + i + 1, a);
  }
}

void TAngleRangeSet::doAdd(Type a0, Type a1) {
  if (m_angles.empty()) {
    if (!m_flip) set(a0, a1);
    return;
  }

  int i0 = find(a0);
  int i1 = find(a1);
  if (i0 == i1) {
    bool visible = (i0 % 2 != 0) == m_flip;
    if (m_angles[i0] != a0 && m_angles[i0] - a0 <= a1 - a0) {
      if (visible) fill(); else set(a0, a1);
    } else
    if (!visible) {
      if (a1 < a0) m_flip = true;
      insert(a0);
      insert(a1);
    }
    return;
  }

  bool visible0 = (i0 % 2 != 0) == m_flip;
  bool visible1 = (i1 % 2 != 0) == m_flip;

  // remove range (i0, i1]
  i0 = (i0 + 1) % m_angles.size();
  if (i1 < i0) {
    m_angles.erase(m_angles.begin() + i0, m_angles.end());
    m_angles.erase(m_angles.begin(), m_angles.begin() + i1 + 1);
  } else {
    m_angles.erase(m_angles.begin() + i0, m_angles.begin() + i1 + 1);
  }

  // insert new angles if need
  if (!visible0) insert(a0);
  if (!visible1) insert(a1);

  if (m_angles.empty() || a1 < a0)
    m_flip = true;
}

bool TAngleRangeSet::contains(Type a) const {
  if (isEmpty()) return false;
  if (isFull()) return true;
  return (find(a) % 2 != 0) == m_flip;
}

bool TAngleRangeSet::check() const {
  if (m_angles.size() % 2 != 0) return false;
  for (int i = 1; i < (int)m_angles.size(); ++i)
    if (m_angles[i - 1] >= m_angles[i]) return false;
  return true;
}

void TAngleRangeSet::set(Type a0, Type a1) {
  m_angles.clear();
  if (a0 < a1) {
    m_flip = false;
    m_angles.push_back(a0);
    m_angles.push_back(a1);
  } else if (a0 > a1) {
    m_flip = true;
    m_angles.push_back(a1);
    m_angles.push_back(a0);
  } else {
    m_flip = true;
  }
}

void TAngleRangeSet::set(const TAngleRangeSet &x, bool flip) {
  if (&x == this) return;
  m_flip   = (x.isFlipped() != flip);
  m_angles = x.angles();
}

void TAngleRangeSet::invert(Type a0, Type a1) {
  if (a0 == a1) return;
  if (isEmpty()) {
    set(a0, a1);
    return;
  }
  if (isFull()) {
    set(a1, a0);
    return;
  }
  if (a1 < a0) m_flip = !m_flip;
  insert(a0);
  insert(a1);
}

void TAngleRangeSet::invert(const TAngleRangeSet &x) {
  if (x.isEmpty()) {
    return;
  }
  if (x.isFull()) {
    invert();
    return;
  }
  if (isEmpty()) {
    set(x);
    return;
  }
  if (isFull()) {
    set(x, true);
    return;
  }
  m_flip = m_flip != x.isFlipped();
  for (List::const_iterator i = x.angles().begin(); i != x.angles().end(); ++i)
    insert(*i);
}

void TAngleRangeSet::add(Type a0, Type a1) {
  if (!isFull() && a0 != a1) {
    if (isEmpty())
      set(a0, a1);
    else
      doAdd(a0, a1);
  }
}

void TAngleRangeSet::add(const TAngleRangeSet &x) {
  if (&x == this || isFull() || x.isEmpty()) return;
  if (isEmpty()) {
    set(x);
    return;
  }
  if (x.isFull()) {
    fill();
    return;
  }
  for (Iterator i(x); i && !isFull(); ++i)
    doAdd(i.a0(), i.a1());
}

void TAngleRangeSet::subtract(Type a0, Type a1) {
  if (!isEmpty() && a0 != a1) {
    if (isFull())
      set(a1, a0);
    else {
      invert();
      doAdd(a0, a1);
      invert();
    }
  }
}

void TAngleRangeSet::subtract(const TAngleRangeSet &x) {
  if (isEmpty() || x.isEmpty()) return;
  if (&x == this || x.isFull()) {
    clear();
    return;
  }
  if (isFull()) {
    set(x);
    invert();
    return;
  }

  // a - b = !(!a + b)
  invert();
  for (Iterator i(x); i && !isFull(); ++i)
    doAdd(i.a0(), i.a1());
  invert();
}

void TAngleRangeSet::intersect(Type a0, Type a1) {
  if (!isEmpty()) {
    if (a0 == a1)
      clear();
    else if (isFull())
      set(a0, a1);
    else {
      invert();
      doAdd(a1, a0);
      invert();
    }
  }
}

void TAngleRangeSet::intersect(const TAngleRangeSet &x) {
  if (&x == this || isEmpty() || x.isFull()) return;
  if (x.isEmpty()) {
    clear();
    return;
  }
  if (isFull()) {
    set(x);
    return;
  }

  // a & b = !(!a + !b)
  invert();
  for (Iterator i(x, true); i && !isFull(); ++i)
    doAdd(i.a0(), i.a1());
  invert();
}
