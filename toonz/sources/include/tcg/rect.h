#pragma once

#ifndef TCG_RECT_H
#define TCG_RECT_H

#include "point.h"
#include "size.h"

// STD includes
#include <limits>

namespace tcg
{

//**********************************************************************************
//    Bidimensional Rect  class
//**********************************************************************************

template <typename T>
struct RectT {
	T x0, y0,
		x1, y1;

public:
	RectT() : x0((std::numeric_limits<T>::max)()), y0(x0), x1(-x0), y1(x1) {}
	RectT(T x0_, T y0_, T x1_, T y1_)
		: x0(x0_), y0(y0_), x1(x1_), y1(y1_) {}
	RectT(const PointT<T> &p0, const PointT<T> &p1)
		: x0(p0.x), y0(p0.y), x1(p1.x), y1(p1.y) {}
	RectT(const PointT<T> &p0, const SizeT<T> &size)
		: x0(p0.x), y0(p0.y), x1(p0.x + size.w), y1(p0.y + size.h) {}

	bool empty() const { return (x1 <= x0) || (y1 <= y0); }

	PointT<T> p0() const { return PointT<T>(x0, y0); }
	PointT<T> p1() const { return PointT<T>(x1, y1); }
	PointT<T> center() const
	{
		return PointT<T>((x0 + x1) / 2, (y0 + y1) / 2);
	}

	T width() const { return x1 - x0; }
	T height() const { return y1 - y0; }
	SizeT<T> size() const { return SizeT<T>(width(), height()); }

	bool operator==(const RectT &other) const
	{
		return x0 == other.x0 && y0 == other.y0 && x1 == other.x1 && y1 == other.y1;
	}
	bool operator!=(const RectT &other) const { return !operator==(other); }

	RectT &operator+=(const PointT<T> &p)
	{
		x0 += p.x, y0 += p.y, x1 += p.x, y1 += p.y;
		return *this;
	}
	RectT &operator-=(const PointT<T> &p)
	{
		x0 -= p.x, y0 -= p.y, x1 -= p.x, y1 -= p.y;
		return *this;
	}

	friend RectT<T> operator+(const RectT<T> &r, const tcg::PointT<T> &p)
	{
		return RectT<T>(r.x0 + p.x, r.y0 + p.y, r.x1 + p.x, r.y1 + p.y);
	}
	friend RectT<T> operator-(const RectT<T> &r, const tcg::PointT<T> &p)
	{
		return RectT<T>(r.x0 - p.x, r.y0 - p.y, r.x1 - p.x, r.y1 - p.y);
	}
	friend RectT<T> operator+(const tcg::PointT<T> &p, const RectT<T> &r)
	{
		return RectT<T>(p.x + r.x0, p.y + r.y0, p.x + r.x1, p.y + r.y1);
	}
	friend RectT<T> operator-(const tcg::PointT<T> &p, const RectT<T> &r)
	{
		return RectT<T>(p.x - r.x0, p.y - r.y0, p.x - r.x1, p.y - r.y1);
	}

	template <typename K>
	RectT &operator*=(K k)
	{
		x0 *= k, y0 *= k, x1 *= k, y1 *= k;
		return *this;
	}

	template <typename K>
	friend RectT<T> operator*(const RectT<T> &r, K k)
	{
		return RectT<T>(r.x0 * k, r.y0 * k, r.x1 * k, r.y1 * k);
	}
	template <typename K>
	friend RectT<T> operator*(K k, const RectT<T> &r)
	{
		return RectT<T>(k * r.x0, k * r.y0, k * r.x1, k * r.y1);
	}

	RectT &operator|=(const RectT &other)
	{
		if (x0 > other.x0)
			x0 = other.x0;
		if (y0 > other.y0)
			y0 = other.y0;
		if (x1 < other.x1)
			x1 = other.x1;
		if (y1 < other.y1)
			y1 = other.y1;
		return *this;
	}

	RectT &operator&=(const RectT &other)
	{
		if (x0 < other.x0)
			x0 = other.x0;
		if (y0 < other.y0)
			y0 = other.y0;
		if (x1 > other.x1)
			x1 = other.x1;
		if (y1 > other.y1)
			y1 = other.y1;
		return *this;
	}

	RectT &operator|=(const PointT<T> &p)
	{
		return operator|=(RectT(p.x, p.y, p.x, p.y));
	}
	RectT &operator&=(const PointT<T> &p)
	{
		return operator&=(RectT(p.x, p.y, p.x, p.y));
	}

	friend RectT<T> operator|(const RectT<T> &a, const RectT<T> &b)
	{
		return RectT<T>(a) |= b;
	}
	friend RectT<T> operator&(const RectT<T> &a, const RectT<T> &b)
	{
		return RectT<T>(a) &= b;
	}

	friend RectT<T> operator|(const RectT<T> &r, const PointT<T> &p)
	{
		return RectT<T>(r) |= p;
	}
	friend RectT<T> operator&(const RectT<T> &r, const PointT<T> &p)
	{
		return RectT<T>(r) &= p;
	}
	friend RectT<T> operator|(const PointT<T> &p, const RectT<T> &r)
	{
		return RectT<T>(r) |= p;
	}
	friend RectT<T> operator&(const PointT<T> &p, const RectT<T> &r)
	{
		return RectT<T>(r) &= p;
	}
};

//------------------------------------------------------------------------

typedef RectT<double> RectD;
typedef RectT<int> RectI;
typedef RectI Rect;

//**********************************************************************************
//    Tridimensional Rect  class
//**********************************************************************************

template <typename T>
struct Rect3T {
	T x0, y0, z0,
		x1, y1, z1;

public:
	Rect3T() : x0((std::numeric_limits<T>::max)()), y0(x0), z0(x0), x1(-x0), y1(x1), z1(x1) {}
	Rect3T(T x0_, T y0_, T z0_, T x1_, T y1_, T z1_)
		: x0(x0_), y0(y0_), z0(z0_), x1(x1_), y1(y1_), z1(z1_) {}
	Rect3T(const Point3T<T> &p0, const Point3T<T> &p1)
		: x0(p0.x), y0(p0.y), z0(p0.z) x1(p1.x), y1(p1.y), z1(p1.z) {}
	Rect3T(const Point3T<T> &p0, const Size3T<T> &size)
		: x0(p0.x), y0(p0.y), z0(p0.z), x1(p0.x + size.w), y1(p0.y + size.h), z1(p0.z + size.d) {}

	bool empty() const { return (x1 <= x0) || (y1 <= y0) || (z1 <= z0); }

	Point3T<T> p0() const { return Point3T<T>(x0, y0, z0); }
	Point3T<T> p1() const { return Point3T<T>(x1, y1, z1); }
	Point3T<T> center() const
	{
		return Point3T<T>((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2);
	}

	T width() const { return x1 - x0; }
	T height() const { return y1 - y0; }
	T depth() const { return z1 - z0; }
	Size3T<T> size() const { return Size3T<T>(width(), height(), depth()); }

	bool operator==(const Rect3T &other) const
	{
		return x0 == other.x0 && y0 == other.y0 && z0 == other.z0 && x1 == other.x1 && y1 == other.y1 && z1 == other.z1;
	}
	bool operator!=(const Rect3T &other) const { return !operator==(other); }

	Rect3T &operator+=(const Point3T<T> &p)
	{
		x0 += p.x, y0 += p.y, z0 += p.z, x1 += p.x, y1 += p.y, z1 += p.z;
		return *this;
	}
	Rect3T &operator-=(const Point3T<T> &p)
	{
		x0 -= p.x, y0 -= p.y, z0 -= p.z, x1 -= p.x, y1 -= p.y, z1 -= p.z;
		return *this;
	}

	friend Rect3T<T> operator+(const Rect3T<T> &r, const tcg::Point3T<T> &p)
	{
		return Rect3T<T>(r.x0 + p.x, r.y0 + p.y, r.z0 + p.z,
						 r.x1 + p.x, r.y1 + p.y, r.z1 + p.z);
	}
	friend Rect3T<T> operator-(const Rect3T<T> &r, const tcg::Point3T<T> &p)
	{
		return Rect3T<T>(r.x0 - p.x, r.y0 - p.y, r.z0 - p.z,
						 r.x1 - p.x, r.y1 - p.y, r.z1 - p.z);
	}
	friend Rect3T<T> operator+(const tcg::Point3T<T> &p, const Rect3T<T> &r)
	{
		return Rect3T<T>(p.x + r.x0, p.x + r.y0, p.x + r.z0,
						 p.x + r.x1 + p.x, r.y1, p.x + r.z1);
	}
	friend Rect3T<T> operator-(const tcg::Point3T<T> &p, const Rect3T<T> &r)
	{
		return Rect3T<T>(p.x - r.x0, p.y - r.y0, p.z - r.z0,
						 p.x - r.x1, p.y - r.y1, p.z - r.z1);
	}

	template <typename K>
	Rect3T &operator*=(K k)
	{
		x0 *= k, y0 *= k, z0 *= k, x1 *= k, y1 *= k, z1 *= k;
		return *this;
	}

	template <typename K>
	friend Rect3T<T> operator*(const Rect3T<T> &r, K k)
	{
		return RectT<T>(r.x0 * k, r.y0 * k, r.z0 * k,
						r.x1 * k, r.y1 * k, r.z1 * k);
	}
	template <typename K>
	friend Rect3T<T> operator*(K k, const Rect3T<T> &r)
	{
		return RectT<T>(k * r.x0, k * r.y0, k * r.z0,
						k * r.x1, k * r.y1, k * r.z1);
	}

	Rect3T &operator|=(const Rect3T &other)
	{
		if (x0 > other.x0)
			x0 = other.x0;
		if (y0 > other.y0)
			y0 = other.y0;
		if (z0 > other.z0)
			z0 = other.z0;
		if (x1 < other.x1)
			x1 = other.x1;
		if (y1 < other.y1)
			y1 = other.y1;
		if (z1 < other.z1)
			z1 = other.z1;
		return *this;
	}

	Rect3T &operator&=(const Rect3T &other)
	{
		if (x0 < other.x0)
			x0 = other.x0;
		if (y0 < other.y0)
			y0 = other.y0;
		if (z0 < other.z0)
			z0 = other.z0;
		if (x1 > other.x1)
			x1 = other.x1;
		if (y1 > other.y1)
			y1 = other.y1;
		if (z1 > other.z1)
			z1 = other.z1;
		return *this;
	}

	Rect3T &operator|=(const Point3T<T> &p)
	{
		return operator|=(RectT(p.x, p.y, p.z, p.x, p.y, p.z));
	}
	Rect3T &operator&=(const Point3T<T> &p)
	{
		return operator&=(RectT(p.x, p.y, p.z, p.x, p.y, p.z));
	}

	friend Rect3T<T> operator|(const Rect3T<T> &a, const Rect3T<T> &b)
	{
		return Rect3T<T>(a) |= b;
	}
	friend Rect3T<T> operator&(const Rect3T<T> &a, const Rect3T<T> &b)
	{
		return Rect3T<T>(a) &= b;
	}

	friend Rect3T<T> operator|(const Rect3T<T> &r, const Point3T<T> &p)
	{
		return Rect3T<T>(r) |= p;
	}
	friend Rect3T<T> operator&(const Rect3T<T> &r, const Point3T<T> &p)
	{
		return Rect3T<T>(r) &= p;
	}
	friend Rect3T<T> operator|(const Point3T<T> &p, const Rect3T<T> &r)
	{
		return Rect3T<T>(r) |= p;
	}
	friend Rect3T<T> operator&(const Point3T<T> &p, const Rect3T<T> &r)
	{
		return Rect3T<T>(r) &= p;
	}
};

//------------------------------------------------------------------------

typedef Rect3T<double> Rect3D;
typedef Rect3T<int> Rect3I;
typedef RectI Rect;

} // namespace tcg

#endif // TCG_RECT_H
