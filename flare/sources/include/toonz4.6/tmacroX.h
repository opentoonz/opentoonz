#pragma once

#ifndef _TMACROX_H_
#define _TMACROX_H_

#ifndef FUNCPROTO
#define FUNCPROTO 0xFF
#endif
#include <Xm/Xm.h>
#include <Xm/BulletinB.h>
#include <Xm/Frame.h>

#include "tmacro.h"

/*---------------------------------------------------------------------------*/

#if XtSpecificationRelease >= 5
typedef int xargc_t;
#else
typedef unsigned int xargc_t;
#endif

/*---------------------------------------------------------------------------*/
/* macro per la gestione delle liste di argomenti */

#define TX_ARG_VARS(num)                                                       \
  Cardinal TxArgN = 0;                                                         \
  Arg TxArgs[num]

#define TX_ADD_ARG(resource, value)                                            \
  {                                                                            \
    if (TxArgN >= sizeof(TxArgs) / sizeof(Arg)) {                              \
      fprintf(stderr, "TX_ADD_ARG error in file %s at line %d\n", __FILE__,    \
              __LINE__);                                                       \
      abort();                                                                 \
    }                                                                          \
    XtSetArg(TxArgs[TxArgN], resource, value);                                 \
    TxArgN++;                                                                  \
  }

#define TX_RESET_ARGS TxArgN = 0;
#define TX_ARGS TxArgs, TxArgN
#define TX_N_ARGS TxArgN

/*---------------------------------------------------------------------------*/
/* macro per la gestione dei form */
/* nota: l'attachment che e' widget o form a seconda che il widget ci sia o no
   e' una correzione veloce per lesstif (linux) che scrive un warning,
   ma andrebbe lasciato solo di tipo XmATTACH_WIDGET e corretto il codice
   che usa la macro sbagliata
*/
#define TX_ADD_LEFT_FORM(offset)                                               \
  {                                                                            \
    TX_ADD_ARG(XmNleftAttachment, XmATTACH_FORM);                              \
    TX_ADD_ARG(XmNleftOffset, offset)                                          \
  }

#define TX_ADD_LEFT_WIDGET(widget, offset)                                     \
  {                                                                            \
    TX_ADD_ARG(XmNleftAttachment, widget ? XmATTACH_WIDGET : XmATTACH_FORM);   \
    TX_ADD_ARG(XmNleftWidget, widget);                                         \
    TX_ADD_ARG(XmNleftOffset, offset)                                          \
  }

#define TX_ADD_LEFT_OPPOSITE(widget, offset)                                   \
  {                                                                            \
    TX_ADD_ARG(XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET);                   \
    TX_ADD_ARG(XmNleftWidget, widget);                                         \
    TX_ADD_ARG(XmNleftOffset, offset)                                          \
  }

#define TX_ADD_RIGHT_FORM(offset)                                              \
  {                                                                            \
    TX_ADD_ARG(XmNrightAttachment, XmATTACH_FORM);                             \
    TX_ADD_ARG(XmNrightOffset, offset)                                         \
  }

#define TX_ADD_RIGHT_WIDGET(widget, offset)                                    \
  {                                                                            \
    TX_ADD_ARG(XmNrightAttachment, widget ? XmATTACH_WIDGET : XmATTACH_FORM);  \
    TX_ADD_ARG(XmNrightWidget, widget);                                        \
    TX_ADD_ARG(XmNrightOffset, offset)                                         \
  }

#define TX_ADD_RIGHT_OPPOSITE(widget, offset)                                  \
  {                                                                            \
    TX_ADD_ARG(XmNrightAttachment, XmATTACH_OPPOSITE_WIDGET);                  \
    TX_ADD_ARG(XmNrightWidget, widget);                                        \
    TX_ADD_ARG(XmNrightOffset, offset)                                         \
  }

#define TX_ADD_TOP_FORM(offset)                                                \
  {                                                                            \
    TX_ADD_ARG(XmNtopAttachment, XmATTACH_FORM);                               \
    TX_ADD_ARG(XmNtopOffset, offset)                                           \
  }

#define TX_ADD_TOP_WIDGET(widget, offset)                                      \
  {                                                                            \
    TX_ADD_ARG(XmNtopAttachment, widget ? XmATTACH_WIDGET : XmATTACH_FORM);    \
    TX_ADD_ARG(XmNtopWidget, widget);                                          \
    TX_ADD_ARG(XmNtopOffset, offset)                                           \
  }

#define TX_ADD_TOP_OPPOSITE(widget, offset)                                    \
  {                                                                            \
    TX_ADD_ARG(XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET);                    \
    TX_ADD_ARG(XmNtopWidget, widget);                                          \
    TX_ADD_ARG(XmNtopOffset, offset)                                           \
  }

#define TX_ADD_BOTTOM_FORM(offset)                                             \
  {                                                                            \
    TX_ADD_ARG(XmNbottomAttachment, XmATTACH_FORM);                            \
    TX_ADD_ARG(XmNbottomOffset, offset)                                        \
  }

#define TX_ADD_BOTTOM_WIDGET(widget, offset)                                   \
  {                                                                            \
    TX_ADD_ARG(XmNbottomAttachment, widget ? XmATTACH_WIDGET : XmATTACH_FORM); \
    TX_ADD_ARG(XmNbottomWidget, widget);                                       \
    TX_ADD_ARG(XmNbottomOffset, offset)                                        \
  }

#define TX_ADD_BOTTOM_OPPOSITE(widget, offset)                                 \
  {                                                                            \
    TX_ADD_ARG(XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET);                 \
    TX_ADD_ARG(XmNbottomWidget, widget);                                       \
    TX_ADD_ARG(XmNbottomOffset, offset)                                        \
  }

/*---------------------------------------------------------------------------*/
/* macro per actions e translations */

#define TX_ADD_ACTIONS(w, action_table)                                        \
  XtAppAddActions(XtWidgetToApplicationContext(w), (action_table),             \
                  XtNumber(action_table));

#define TX_OVERRIDE_TRANSLATIONS(w, translation_table)                         \
  XtOverrideTranslations((w), XtParseTranslationTable(translation_table));

#define TX_ADD_ACTIONS_AND_TRANSLATIONS(w, action_table, translation_table)    \
  {                                                                            \
    TX_ADD_ACTIONS((w), (action_table))                                        \
    TX_OVERRIDE_TRANSLATIONS((w), (translation_table))                         \
  }

#endif
