#pragma once

#ifndef TCG_IMAGE_ITERATOR_HPP
#define TCG_IMAGE_ITERATOR_HPP

// tcg includes
#include "../image_iterator.h"
#include "../pixel_ops.h"

namespace tcg
{

//***************************************************************************
//    image_edge_iterator  implementation
//***************************************************************************

template <typename It, _iei_adherence_policy _adherence>
template <typename Img>
image_edge_iterator<It, _adherence>::image_edge_iterator(
	const Img &img, int x, int y, int dirX, int dirY)
	: m_lx_1(image_traits<Img>::width(img) - 1), m_ly_1(image_traits<Img>::height(img) - 1), m_wrap(image_traits<Img>::wrap(img)), m_pos(x, y), m_dir(dirX, dirY), m_outsideColor(image_traits<Img>::outsideColor(img)), m_elbowColor(m_outsideColor), m_pix(image_traits<Img>::pixel(img, x, y)), m_turn(UNKNOWN)
{
	pixels(m_leftPix, m_rightPix);
	colors(m_leftColor, m_rightColor);
}

//---------------------------------------------------------------------------------------------

template <typename It, _iei_adherence_policy _adherence>
inline void image_edge_iterator<It, _adherence>::pixels(
	iter pixLeft, iter pixRight)
{
	if (m_dir.y)
		if (m_dir.y > 0)
			pixLeft = m_pix - 1, pixRight = m_pix;
		else
			pixLeft = m_pix - m_wrap, pixRight = pixLeft - 1;
	else if (m_dir.x > 0)
		pixLeft = m_pix, pixRight = m_pix - m_wrap;
	else
		pixRight = m_pix - 1, pixLeft = pixRight - m_wrap;
}

//---------------------------------------------------------------------------------------------

template <typename It, _iei_adherence_policy _adherence>
inline void image_edge_iterator<It, _adherence>::colors(
	value_type &leftColor, value_type &rightColor)
{
	if (m_dir.y)
		if (m_dir.y > 0) {
			if (m_pos.y > m_ly_1)
				leftColor = rightColor = m_outsideColor;
			else {
				leftColor = (m_pos.x > 0) ? *m_leftPix : m_outsideColor;
				rightColor = (m_pos.x <= m_lx_1) ? *m_rightPix : m_outsideColor;
			}
		} else {
			if (m_pos.y < 1)
				leftColor = rightColor = m_outsideColor;
			else {
				leftColor = (m_pos.x <= m_lx_1) ? *m_leftPix : m_outsideColor;
				rightColor = (m_pos.x > 0) ? *m_rightPix : m_outsideColor;
			}
		}
	else if (m_dir.x > 0) {
		if (m_pos.x > m_lx_1)
			leftColor = rightColor = m_outsideColor;
		else {
			leftColor = (m_pos.y <= m_ly_1) ? *m_leftPix : m_outsideColor;
			rightColor = (m_pos.y > 0) ? *m_rightPix : m_outsideColor;
		}
	} else {
		if (m_pos.x < 1)
			leftColor = rightColor = m_outsideColor;
		else {
			leftColor = (m_pos.y > 0) ? *m_leftPix : m_outsideColor;
			rightColor = (m_pos.y <= m_ly_1) ? *m_rightPix : m_outsideColor;
		}
	}
}

//---------------------------------------------------------------------------------------------

template <typename It, _iei_adherence_policy _adherence>
inline void image_edge_iterator<It, _adherence>::turn(
	const value_type &newLeftColor, const value_type &newRightColor,
	policy<RIGHT_ADHERENCE>)
{
	if (newLeftColor == m_rightColor) {
		if (newRightColor == m_leftColor)
			turnAmbiguous(newLeftColor, newRightColor);
		else
			turnLeft();
	} else {
		if (newRightColor != m_rightColor)
			turnRight();
		else
			m_turn = STRAIGHT;
	}

	m_elbowColor = newLeftColor;

	pixels(m_leftPix, m_rightPix);
}

//---------------------------------------------------------------------------------------------

template <typename It, _iei_adherence_policy _adherence>
inline void image_edge_iterator<It, _adherence>::turn(
	const value_type &newLeftColor, const value_type &newRightColor,
	policy<LEFT_ADHERENCE>)
{
	if (newRightColor == m_leftColor) {
		if (newLeftColor == m_rightColor)
			turnAmbiguous(newLeftColor, newRightColor);
		else
			turnRight();
	} else {
		if (newLeftColor != m_leftColor)
			turnLeft();
		else
			m_turn = STRAIGHT;
	}

	m_elbowColor = newRightColor;

	pixels(m_leftPix, m_rightPix);
}

//---------------------------------------------------------------------------------------------

template <typename It, _iei_adherence_policy _adherence>
inline void image_edge_iterator<It, _adherence>::turnAmbiguous(
	const value_type &newLeftColor, const value_type &newRightColor)
{
	UCHAR count1 = 0, count2 = 0;

	value_type val;

	// Check the 4x4 neighbourhood and connect the minority color
	if (m_pos.x > 2) {
		val = *(m_pix - 2);
		if (val == m_leftColor)
			++count1;
		else if (val == m_rightColor)
			++count2;

		val = *(m_pix - 2 - m_wrap);
		if (val == m_leftColor)
			++count1;
		else if (val == m_rightColor)
			++count2;
	}

	if (m_pos.x < m_lx_1) {
		val = *(m_pix + 1);
		if (val == m_leftColor)
			++count1;
		else if (val == m_rightColor)
			++count2;

		val = *(m_pix + 1 - m_wrap);
		if (val == m_leftColor)
			++count1;
		else if (val == m_rightColor)
			++count2;
	}

	if (m_pos.y > 2) {
		int wrap2 = m_wrap << 1;

		val = *(m_pix - wrap2);
		if (val == m_leftColor)
			++count1;
		else if (val == m_rightColor)
			++count2;

		val = *(m_pix - wrap2 - 1);
		if (val == m_leftColor)
			++count1;
		else if (val == m_rightColor)
			++count2;
	}

	if (m_pos.y < m_ly_1) {
		val = *(m_pix + m_wrap);
		if (val == m_leftColor)
			++count1;
		else if (val == m_rightColor)
			++count2;

		val = *(m_pix + m_wrap - 1);
		if (val == m_leftColor)
			++count1;
		else if (val == m_rightColor)
			++count2;
	}

	// Minority connection - join the one with less count
	if (count1 <= count2)
		turnRight(); // Join m_leftColor == newRightColor
	else if (count1 > count2)
		turnLeft(); // Join m_rightColor == newLeftColor

	m_turn |= AMBIGUOUS;
}

//---------------------------------------------------------------------------------------------

template <typename It, _iei_adherence_policy _adherence>
void image_edge_iterator<It, _adherence>::advance(policy<RIGHT_ADHERENCE>)
{
	value_type newLeftColor = m_leftColor, newRightColor = m_rightColor;

	int pixAdd = m_dir.y * m_wrap + m_dir.x;

	m_pos.x += m_dir.x, m_pos.y += m_dir.y;
	m_pix += pixAdd, m_leftPix += pixAdd, m_rightPix += pixAdd;
	m_leftColor = newLeftColor;

	colors(newLeftColor, newRightColor);

	turn(newLeftColor, newRightColor);
	colors(m_leftColor, m_rightColor);
}

//---------------------------------------------------------------------------------------------

template <typename It, _iei_adherence_policy _adherence>
void image_edge_iterator<It, _adherence>::advance(policy<LEFT_ADHERENCE>)
{
	value_type newLeftColor = m_leftColor, newRightColor = m_rightColor;

	int pixAdd = m_dir.y * m_wrap + m_dir.x;

	m_pos.x += m_dir.x, m_pos.y += m_dir.y;
	m_pix += pixAdd, m_leftPix += pixAdd, m_rightPix += pixAdd;
	m_rightColor = newRightColor;

	colors(newLeftColor, newRightColor);

	turn(newLeftColor, newRightColor);
	colors(m_leftColor, m_rightColor);
}

} // namespace tcg

#endif // TCG_IMAGE_ITERATOR_HPP
