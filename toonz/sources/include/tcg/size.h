#pragma once

#ifndef TCG_SIZE_H
#define TCG_SIZE_H

namespace tcg
{

//**********************************************************************************
//    Bidimensional size  class
//**********************************************************************************

template <typename T>
struct SizeT {
	typedef T value_type;

public:
	T w, h;

public:
	SizeT() : w(), h() {}
	SizeT(T w_, T h_) : w(w_), h(h_) {}

	bool empty() const { return (w <= 0) || (h <= 0); }

	SizeT &operator+=(const SizeT &other)
	{
		w += other.w, h += other.h;
		return *this;
	}
	SizeT &operator-=(const SizeT &other)
	{
		w -= other.w, h -= other.h;
		return *this;
	}

	friend SizeT<T> operator+(const SizeT<T> &a, const SizeT<T> &b)
	{
		return SizeT<T>(a.w + b.w, a.h + b.h);
	}

	friend SizeT<T> operator-(const SizeT<T> &a, const SizeT<T> &b)
	{
		return SizeT<T>(a.w - b.w, a.h - b.h);
	}

	template <typename K>
	SizeT &operator*=(K k)
	{
		w *= k, h *= k;
		return *this;
	}

	template <typename K>
	SizeT &operator/=(K k)
	{
		w /= k, h /= k;
		return *this;
	}

	template <typename K>
	friend SizeT<T> operator*(K k, const SizeT<T> &a)
	{
		return SizeT<T>(k * a.w, k * a.h);
	}

	template <typename K>
	friend SizeT<T> operator*(const SizeT<T> &a, K k)
	{
		return SizeT<T>(a.w * k, a.h * k);
	}

	template <typename K>
	friend SizeT<T> operator/(const SizeT<T> &a, K k)
	{
		return SizeT<T>(a.w / k, a.h / k);
	}

	SizeT &operator&=(const SizeT &other)
	{
		if (other.w < w)
			w = other.w;
		if (other.h < h)
			h = other.h;

		return *this;
	}

	SizeT &operator|=(const SizeT &other)
	{
		if (other.w > w)
			w = other.w;
		if (other.h > h)
			h = other.h;

		return *this;
	}

	friend SizeT<T> operator&(const SizeT<T> &a, const SizeT<T> &b)
	{
		SizeT<T> tmp(a);
		return tmp &= b;
	}
	friend SizeT<T> operator|(const SizeT<T> &a, const SizeT<T> &b)
	{
		SizeT<T> tmp(a);
		return tmp |= b;
	}
};

//------------------------------------------------------------------------

typedef SizeT<int> SizeI;
typedef SizeI Size;
typedef SizeT<double> SizeD;

//**********************************************************************************
//    Tridimensional size  class
//**********************************************************************************

template <typename T>
struct Size3T {
	typedef T value_type;

public:
	T w, h, d;

public:
	Size3T() : w(), h(), d() {}
	Size3T(T w_, T h_, T d_) : w(w_), h(h_), d(d_) {}

	bool empty() const { return (w <= 0) || (h <= 0) || (d <= 0); }

	Size3T &operator+=(const Size3T &other)
	{
		w += other.w, h += other.h, d += other.d;
		return *this;
	}
	Size3T &operator-=(const Size3T &other)
	{
		w -= other.w, h -= other.h, d -= other.d;
		return *this;
	}

	friend Size3T<T> operator+(const Size3T<T> &a, const Size3T<T> &b)
	{
		return SizeT<T>(a.w + b.w, a.h + b.h, a.d + b.d);
	}

	friend Size3T<T> operator-(const Size3T<T> &a, const Size3T<T> &b)
	{
		return SizeT<T>(a.w - b.w, a.h - b.h, a.d + b.d);
	}

	template <typename K>
	Size3T &operator*=(K k)
	{
		w *= k, h *= k, d *= k;
		return *this;
	}

	template <typename K>
	Size3T &operator/=(K k)
	{
		w /= k, h /= k, d /= k;
		return *this;
	}

	template <typename K>
	friend Size3T<T> operator*(K k, const Size3T<T> &a)
	{
		return Size3T<T>(k * a.w, k * a.h, k * a.d);
	}

	template <typename K>
	friend Size3T<T> operator*(const Size3T<T> &a, K k)
	{
		return Size3T<T>(a.w * k, a.h * k, a.d * k);
	}

	template <typename K>
	friend Size3T<T> operator/(const Size3T<T> &a, K k)
	{
		return Size3T<T>(a.w / k, a.h / k, a.d / k);
	}

	Size3T &operator&=(const Size3T &other)
	{
		if (other.w < w)
			w = other.w;
		if (other.h < h)
			h = other.h;
		if (other.d < d)
			d = other.d;

		return *this;
	}

	Size3T &operator|=(const Size3T &other)
	{
		if (other.w > w)
			w = other.w;
		if (other.h > h)
			h = other.h;
		if (other.d > d)
			d = other.d;

		return *this;
	}

	friend Size3T<T> operator&(const Size3T<T> &a, const Size3T<T> &b)
	{
		Size3T<T> tmp(a);
		return tmp &= b;
	}
	friend Size3T<T> operator|(const Size3T<T> &a, const Size3T<T> &b)
	{
		Size3T<T> tmp(a);
		return tmp |= b;
	}
};

//------------------------------------------------------------------------

typedef Size3T<int> Size3I;
typedef Size3I Size3;
typedef Size3T<double> Size3D;

//**********************************************************************************
//    N-dimensional size  class
//**********************************************************************************

template <int N, typename T>
struct SizeN {
	static const int size = N;
	typedef T value_type;

public:
	T span[N];

public:
	SizeN() {}
	SizeN(const T &t) { std::fill(span, span + N, t); }

	template <typename It>
	SizeN(It begin, It end)
	{
		std::copy(begin, end, span);
	}

	// To be completed...
};

} // namespace tcg

#endif // TCG_SIZE_H
