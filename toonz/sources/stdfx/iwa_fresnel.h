#pragma once

#ifndef IWA_FRESNEL_H
#define IWA_FRESNEL_H

/* Fresnel反射率のテーブル（0°〜90° １°刻み） */
static float fresnel[91] = {
    0.020059312f, 0.020059313f, 0.020059328f, 0.020059394f, 0.020059572f,
    0.020059947f, 0.020060633f, 0.020061768f, 0.020063519f, 0.020066082f,
    0.020069685f, 0.020074587f, 0.020081084f, 0.020089508f, 0.020100231f,
    0.020113671f, 0.020130291f, 0.020150605f, 0.020175182f, 0.020204654f,
    0.020239715f, 0.020281134f, 0.020329757f, 0.020386517f, 0.020452442f,
    0.020528662f, 0.020616424f, 0.020717098f, 0.020832193f, 0.02096337f,
    0.021112458f, 0.021281471f, 0.021472628f, 0.021688372f, 0.021931397f,
    0.022204671f, 0.022511467f, 0.022855398f, 0.023240448f, 0.023671017f,
    0.024151962f, 0.024688653f, 0.025287023f, 0.025953633f, 0.026695742f,
    0.027521384f, 0.028439454f, 0.029459807f, 0.030593365f, 0.031852239f,
    0.033249863f, 0.034801146f, 0.036522642f, 0.038432743f, 0.040551883f,
    0.042902788f, 0.045510732f, 0.048403845f, 0.051613442f, 0.055174404f,
    0.059125599f, 0.063510357f, 0.068377002f, 0.073779452f, 0.079777891f,
    0.086439526f, 0.093839443f, 0.102061562f, 0.111199722f, 0.121358904f,
    0.132656604f, 0.145224399f, 0.159209711f, 0.174777806f, 0.192114072f,
    0.211426604f, 0.232949158f, 0.256944521f, 0.283708376f, 0.313573743f,
    0.346916096f, 0.384159282f, 0.425782383f, 0.472327717f, 0.524410184f,
    0.582728245f, 0.648076867f, 0.721362859f, 0.803623128f, 0.896046505f,
    1.0f};

#endif