#pragma once

#ifndef OPTIMIZE_FOR_LP64_INCLUDED
#define OPTIMIZE_FOR_LP64_INCLUDED

/* ========================================================================= */

/*

  *****************************************************************************
  *  OSSERVAZIONI                                                             *
  *****************************************************************************

  ____________OSS 1:___________________________________________________________


  se devo fare DUE MOLTIPLICAZIONI 13 bit * 8 bit posso farle in un
  colpo solo, ad esempio:

  siano X = xxxxxxxxxxxxx
  S = ssssssss
  Y = yyyyyyyyyyyyy
  T = tttttttt

  e devo calcolare
  U = X * S
  V = Y * T
  posso farlo in un colpo solo impacchettando i bit cosi':

  A = X       0 00000000 Y       = xxxxxxxxxxxxx 0 00000000 yyyyyyyyyyyyy
  B = 00000 S 0 00000000 00000 T = 00000ssssssss 0 00000000 00000tttttttt

  ora se faccio C = A * B si ha

  C = U ?????????????????????? V =
  = uuuuuuuuuuuuuuuuuuuuu ?????????????????????? vvvvvvvvvvvvvvvvvvvvv

  dove C e' di 64 bit; cioe' i primi 21 bit sono X * S = U
  e gli ultimi 21 sono Y * T = V

  ____________OSS 2:___________________________________________________________


  se devo fare DUE MOLTIPLICAZIONI 16 bit * 16 bit del tipo
  X * S = U
  Y * S = V

  con

  #X = 16,
  #Y = 16,
  #S = 16

  (dove l'operatore '#' da' come risultato il numero di bit di cui e' composto
  un numero intero)

  posso farle tutte e due in un solo colpo impacchettando i bit cosi':

  O = 0000000000000000,   #O = 16
  A = X O Y           ,   #A = 48
  B = S               ,   #B = 16
  C = A * B           ,   #C = 64

  dove i primi 32 bit sono X * S e i secondi 32 bit sono Y * S

  ____________OSS 3:___________________________________________________________


  se devo fare QUATTRO MOLTIPLICAZIONI 8 bit * 8 bit del tipo
  X * S = I           #X = 8, #S = 8, #I = 16
  Y * S = J           #Y = 8, #S = 8, #J = 16
  Z * S = K           #Z = 8, #S = 8, #K = 16
  W * S = L           #W = 8, #S = 8, #L = 16


  posso farle tutte e due in un solo colpo impacchettando i bit cosi':

  O = 00000000             #O = 8
  C = XOYOZOW * OOOOOOS    #C = 64

  dove
  I sono i primi 16 bit,
  J sono i secondi 16 bit,
  K sono i terzi 16 bit,
  L i quarti 16 bit
  _____________________________________________________________________________

  *****************************************************************************
  */

/* ========================================================================= */

#define OPTIMIZE_FOR_LP64

/* ========================================================================= */

#define MASK_FIRST_OF_3_X_16BIT 0x7FFFC00000000
#define MASK_SECOND_OF_3_X_16BIT 0x3FFFE0000
#define MASK_THIRD_OF_3_X_16BIT 0x1FFFF

#define FIRST_OF_3_X_16BIT(x) (x) >> 34
#define SECOND_OF_3_X_16BIT(x) ((x)&MASK_SECOND_OF_3_X_16BIT) >> 17;
#define THIRD_OF_3_X_16BIT(x) (x) & MASK_THIRD_OF_3_X_16BIT;

/* ========================================================================= */

#define MASK_FIRST_OF_2_X_24BIT 0x3FFFFFE000000
#define MASK_SECOND_OF_2_X_24BIT 0x1FFFFFF

#define FIRST_OF_2_X_24BIT(x) (x) >> 25
#define SECOND_OF_2_X_24BIT(x) (x) & MASK_SECOND_OF_2_X_24BIT

/* ========================================================================= */

#define MASK_FIRST_OF_2_X_32BIT 0xFFFFFFFF00000000
#define MASK_SECOND_OF_2_X_32BIT 0xFFFFFFFF

#define FIRST_OF_2_X_32BIT(x) (x) >> 32
#define SECOND_OF_2_X_32BIT(x) (x) & MASK_SECOND_OF_2_X_32BIT

/* ========================================================================= */

typedef unsigned char UINT8;
typedef unsigned short UINT16;
typedef unsigned int UINT24;
typedef unsigned int UINT32;
typedef unsigned long UINT50;
typedef unsigned long UINT51;
typedef unsigned long UINT64;

/* ========================================================================= */

#if 0

/* esegue a1+b1, a2+c2, a3+c3 in un'unica operazione */
UINT64  add_3_x_16bit ( UINT16 a1, UINT16 a2, UINT16 a3,
			UINT16 b1, UINT16 b2, UINT16 b3 );

/* esegue a1+b1, a2+b2 in un'unica operazione */
UINT50 add_2_x_24bit ( UINT24 a1, UINT24 a2,
		       UINT24 b1, UINT24 b2 );

/* esegue a1*b, a2*b in un'unica operazione */
UINT64 mult_2_x_16bit ( UINT16 a1, UINT16 a2,
			UINT16 b );
#endif

/* ========================================================================= */

/* ------------------------------------------------------------------------- */

#define ADD_3_X_16BIT(a1, a2, a3, b1, b2, b3)                                  \
  (0L | (UINT64)(a1) << 34 | (UINT64)(a2) << 17 | (a3)) +                      \
      (0L | (UINT64)(b1) << 34 | (UINT64)(b2) << 17 | (b3))

inline UINT64 add_3_x_16bit(UINT16 a1, UINT16 a2, UINT16 a3, UINT16 b1,
                            UINT16 b2, UINT16 b3) {
  return (0L | (UINT64)a1 << 34 | (UINT64)a2 << 17 | a3) +
         (0L | (UINT64)b1 << 34 | (UINT64)b2 << 17 | b3);
}

/* ------------------------------------------------------------------------- */

#define ADD_2_X_24BIT(a1, a2, b1, b2)                                          \
  (0L | (UINT64)(a1) << 25 | (a2)) + (0L | (UINT64)(b1) << 25 | (b2))

inline UINT50 add_2_x_24bit(UINT24 a1, UINT24 a2, UINT24 b1, UINT24 b2) {
  return (0L | (UINT64)a1 << 25 | a2) + (0L | (UINT64)b1 << 25 | b2);
}

/* ------------------------------------------------------------------------- */

#define MULT_2_X_16BIT(a1, a2, b)                                              \
  ((UINT64)b) * (((UINT64)(a1) << 32) | (UINT64)a2)

inline UINT64 mult_2_x_16bit(UINT16 a1, UINT16 a2, UINT16 b) {
  return (0L | (UINT64)a1 << 32 | a2) * b;
}

#endif
