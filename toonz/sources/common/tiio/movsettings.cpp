

#include "texception.h"
#include "tpropertytype.h"
//#include "timageinfo.h"
//#include "tlevel_io.h"
#include "tproperty.h"
#include "tiio.h"

#if !defined(x64) && !defined(__LP64__) && !(defined(LINUX) || defined(FREEBSD)) && !(defined(__GNUC__) && defined(_WIN32))

//*******************************************************************************
//    32-bit version
//*******************************************************************************

#ifdef _WIN32
#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#define list QuickTime_list
#define map QuickTime_map
#define iterator QuickTime_iterator
#define float_t QuickTime_float_t
#define GetProcessInformation QuickTime_GetProcessInformation
#define int_fast8_t QuickTime_int_fast8_t
#define int_fast16_t QuickTime_int_fast16_t
#define uint_fast16_t QuickTime_uint_fast16_t

#include "QTML.h"
#include "Movies.h"
#include "Script.h"
#include "FixMath.h"
#include "Sound.h"

#include "QuickTimeComponents.h"
#include "tquicktime.h"

#undef list
#undef map
#undef iterator
#undef float_t
#undef GetProcessInformation
#undef int_fast8_t
#undef int_fast16_t
#undef uint_fast16_t

#else

#define list List
#define map Map
#define iterator Iterator
#define float_t Float_t
#include <Carbon/Carbon.h>
#include <QuickTime/Movies.h>
#include <QuickTime/ImageCompression.h>
#include <QuickTime/QuickTimeComponents.h>

#undef list
#undef map
#undef iterator
#undef float_t

#endif
/*
questo file gestisce il salvataggio in un .tnz e il caricamento dei setting dei
mov.
viene usato il popup fornito da quicktime, con tutti i suoi setting e i sotto
settings.
i setting sono memorizzati da quicktime in un componentInstance. Da qui, possono
essere convertiti in un atomContainer,
che e' una struttura simile alla nostra propertyGroup, ma con gli atomi
strutturati ad albero.
sono state scritte due funzioni di conversione da atomContainer a propertygroup
e viceversa
ogni atom ha un type, id, e numero figli. se numero figli=0 allora l'atomo e'una
foglia,
e quindi ha un buffer di dati di valori char.

ogni atomo viene trasformato in una stringProperty. il nome della stringProperty
e'
"type id numeroFigli"
se numerofigli>0, allora la stringProperty ha un valore nullo, e le prossime
numerofigli property contengono i figli;
se numerofigli==0, allora il valore della property contiene il buffer di dati,
convertito in stringa.
ecco coem viene convertito il buffer in stringa:
se ad esempio il buffer e' composto di 3 bytes, buf[0] = 13 buf[1]=0 buf[2]=231
allora la strnga valore sara' "13 0 231"
se ci sono piu 0 consecutivi, vengono memorizzati per salvare spazio come "z
count" in cui count e' il numero di 0.
esempio:  buf[0] = 13 buf[1]=0 buf[2]=0 buf[3]=0 buf[4]=0 buf5]=231
allora str = "13 z 4 231"
*/

#include "movsettings.h"

//------------------------------------------------

void visitAtoms(const QTAtomContainer &atoms, const QTAtom &parent,
                TPropertyGroup &pg) {
  QTAtom curr = 0;

  do {
    if (QTNextChildAnyType(atoms, parent, curr, &curr) != noErr) assert(false);

    if (curr == 0) break;
    QTAtomType atomType;
    QTAtomID id;

    QTGetAtomTypeAndID(atoms, curr, &atomType, &id);
    int sonCount = QTCountChildrenOfType(atoms, curr, 0);

    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "%d %d %d",
      (int)atomType, (int)id, sonCount);
    string str(buffer);

    if (sonCount > 0) {
      pg.add(new TStringProperty(str, TString()));
      visitAtoms(atoms, curr, pg);
    }

    else {
      long size;
      UCHAR *atomData;
      if (QTGetAtomDataPtr(atoms, curr, &size, (char **)&atomData) != noErr)
        assert(false);

      string strapp;
      for (int i = 0; i < size; i++) {
        string num;
        if (atomData[i] == 0) {
          int count = 1;
          while ((i + 1) < size && atomData[i + 1] == 0) i++, count++;
          if (count > 1) {
            num    = std::to_string(count);
            strapp = strapp + "z " + num + " ";
            continue;
          }
        }
        num = std::to_string(atomData[i]);

        strapp = strapp + string(num) + " ";
      }

      // unsigned short*buffer = new unsigned short[size];
      // buffer[size]=0;
      // for (i=0; i<size; i++)
      //  buffer[i] = atomData[i]+1;

      wstring data = ::to_wstring(strapp);

      pg.add(new TStringProperty(str, data));
    }
  } while (curr != 0);
}

//------------------------------------------------
namespace {
void compareAtoms(const QTAtomContainer &atoms1, QTAtom parent1,
                  const QTAtomContainer &atoms2, QTAtom parent2) {
  QTAtom curr1 = 0, curr2 = 0;

  assert(QTCountChildrenOfType(atoms1, parent1, 0) ==
         QTCountChildrenOfType(atoms2, parent2, 0));

  do {
    if (QTNextChildAnyType(atoms1, parent1, curr1, &curr1) != noErr)
      assert(false);

    if (QTNextChildAnyType(atoms2, parent2, curr2, &curr2) != noErr)
      assert(false);
    assert((curr1 != 0 && curr2 != 0) || (curr1 == 0 && curr2 == 0));

    if (curr1 == 0 || curr2 == 0) break;

    QTAtomType atomType1, atomType2;
    QTAtomID id1, id2;

    QTGetAtomTypeAndID(atoms1, curr1, &atomType1, &id1);
    QTGetAtomTypeAndID(atoms2, curr2, &atomType2, &id2);
    assert(atomType1 == atomType2);

    int sonCount1 = QTCountChildrenOfType(atoms1, curr1, 0);
    int sonCount2 = QTCountChildrenOfType(atoms2, curr2, 0);
    assert(sonCount1 == sonCount2);
    if (sonCount1 > 0)
      compareAtoms(atoms1, curr1, atoms2, curr2);
    else {
      long size1;
      UCHAR *atomData1;
      long size2;
      UCHAR *atomData2;
      if (QTGetAtomDataPtr(atoms1, curr1, &size1, (char **)&atomData1) != noErr)
        assert(false);
      if (QTGetAtomDataPtr(atoms2, curr2, &size2, (char **)&atomData2) != noErr)
        assert(false);
      assert(size1 == size2);
      for (int i = 0; i < size1; i++) assert(atomData1[i] == atomData2[i]);
    }
  } while (curr1 != 0 && curr2 != 0);
}
}

//------------------------------------------------

void fromAtomsToProperties(const QTAtomContainer &atoms, TPropertyGroup &pg) {
  pg.clear();
  visitAtoms(atoms, kParentAtomIsContainer, pg);
}

//------------------------------------------------
void visitprops(TPropertyGroup &pg, int &index, QTAtomContainer &atoms,
                QTAtom parent) {
  int count = pg.getPropertyCount();
  while (index < count) {
    TStringProperty *p = (TStringProperty *)pg.getProperty(index++);
    string str0        = p->getName();
    const char *buf    = str0.c_str();
    int atomType, id, sonCount;
    sscanf(buf, "%d %d %d", &atomType, &id, &sonCount);
    QTAtom newAtom;
    if (sonCount == 0) {
      wstring appow   = p->getValue();
      string appo     = ::to_string(appow);
      const char *str = appo.c_str();

      vector<UCHAR> buf;
      while (strlen(str) > 0) {
        if (str[0] == 'z') {
          int count = atoi(str + 1);
          str += (count < 10) ? 4 : ((count < 100) ? 5 : 6);
          while (count--) buf.push_back(0);
        } else {
          int val = atoi(str);
          assert(val >= 0 && val < 256);

          str += (val < 10) ? 2 : ((val < 100) ? 3 : 4);
          buf.push_back(val);
        }
      }
      // const unsigned short*bufs = str1.c_str();
      // UCHAR *bufc = new UCHAR[size];
      // for (int i=0; i<size; i++)
      // {
      //	assert(bufs[i]<257);
      //	bufc[i] = (UCHAR)(bufs[i]-1);
      //	}
      void *ptr = 0;
      if (buf.size() != 0) {
        ptr = &(buf[0]);
      }
      QTInsertChild(atoms, parent, (QTAtomType)atomType, (QTAtomID)id, 0,
                    buf.size(), (void *)ptr, 0);
    } else {
      QTInsertChild(atoms, parent, (QTAtomType)atomType, (QTAtomID)id, 0, 0, 0,
                    &newAtom);
      visitprops(pg, index, atoms, newAtom);
    }
  }
}

//------------------------------------------------

void fromPropertiesToAtoms(TPropertyGroup &pg, QTAtomContainer &atoms) {
  int index = 0;
  visitprops(pg, index, atoms, kParentAtomIsContainer);
}

//------------------------------------------------
/*
#ifdef MACOSX

SCExtendedProcs gProcStruct, ptr;

static Boolean QTCmpr_FilterProc
      (DialogPtr theDialog, EventRecord *theEvent,
                                    short *theItemHit, long theRefCon)
{
#pragma unused(theItemHit, theRefCon)
   Boolean         myEventHandled = false;
   WindowRef      myEventWindow = NULL;
   WindowRef      myDialogWindow = NULL;

   myDialogWindow = GetDialogWindow(theDialog);

   switch (theEvent->what) {
      case updateEvt:
        myEventWindow = (WindowRef)theEvent->message;
                // Change the window class
        HIWindowChangeClass(myEventWindow,kUtilityWindowClass);
                // Activate the window scope
                SetWindowActivationScope(myEventWindow,kWindowActivationScopeAll);
                // Set the brushed metal theme on the window
                SetThemeWindowBackground(myEventWindow,kThemeBrushUtilityWindowBackgroundActive,true);

                break;
   }

   return(myEventHandled);
}

#endif
*/
//------------------------------------------------

void openMovSettingsPopup(TPropertyGroup *props, bool macBringToFront) {
#ifdef _WIN32
  if (InitializeQTML(0) != noErr) return;
#endif

  ComponentInstance ci =
      OpenDefaultComponent(StandardCompressionType, StandardCompressionSubType);

  QTAtomContainer atoms;
  QTNewAtomContainer(&atoms);

  fromPropertiesToAtoms(*props, atoms);

  ComponentResult err;

  if ((err = SCSetSettingsFromAtomContainer(ci, atoms)) != noErr) {
    CloseComponent(ci);
    ci = OpenDefaultComponent(StandardCompressionType,
                              StandardCompressionSubType);
    assert(false);
  }

  QTDisposeAtomContainer(atoms);

#ifdef MACOSX

// Install an external procedure to use a callback filter on the request
// settings dialog
// On MACOSX we need to change the dialog appearance in order to pop-up in front
// of the
// toonz main window.
/*
gProcStruct.filterProc = NewSCModalFilterUPP(QTCmpr_FilterProc);
// I don't install any hook
gProcStruct.hookProc = NULL;
gProcStruct.customName[0] = 0;
// I don't use refcon
gProcStruct.refcon = 0;

// set the current extended procs
SCSetInfo(ci, scExtendedProcsType, &gProcStruct);
*/
#endif

  err = SCRequestSequenceSettings(ci);
  // assert(err==noErr);
  QTAtomContainer atomsOut;

  if (SCGetSettingsAsAtomContainer(ci, &atomsOut) != noErr) assert(false);

  fromAtomsToProperties(atomsOut, *props);

  QTDisposeAtomContainer(atomsOut);
  CloseComponent(ci);

  // int dataSize=0, numChildren = 0, numLevels=0;
  // retrieveData(settings, kParentAtomIsContainer, dataSize, numChildren,
  // numLevels);
}

bool Tiio::isQuicktimeInstalled() {
#ifdef MACOSX
  return true;
#else

  static int ret     = -1;
  if (ret == -1) ret = (InitializeQTML(0) == noErr) ? 1 : 0;

  return (ret == 1);

#endif
}

#else  // x64

//*******************************************************************************
//    64-bit proxied version
//*******************************************************************************

// Toonz includes
#include "tfilepath.h"
#include "tstream.h"

// tipc includes
#include "tipc.h"
#include "t32bitsrv_wrap.h"

// MAC-Specific includes
#ifdef MACOSX
#include <ApplicationServices/ApplicationServices.h>
#endif

#include "movsettings.h"

//---------------------------------------------------------------------------

// Using 32-bit background server correspondence to achieve the same result
void openMovSettingsPopup(TPropertyGroup *props, bool unused) {
  QLocalSocket socket;
  if (!tipc::startSlaveConnection(&socket, t32bitsrv::srvName(), 3000,
                                  t32bitsrv::srvCmdline(), "_main"))
    return;

  // Send the appropriate commands to the server
  tipc::Stream stream(&socket);
  tipc::Message msg;

  // We'll communicate through temporary files.
  stream << (msg << QString("$tmpfile_request") << QString("openMovSets"));
  QString res(tipc::readMessage(stream, msg));

  QString fp;
  msg >> fp;
  assert(res == "ok" && !fp.isEmpty());

  TFilePath tfp(fp.toStdWString());
  {
    // Save the input props to the temporary file
    TOStream os(tfp);
    props->saveData(os);
  }

  // Invoke the settings popup
  stream << (msg << tipc::clr << QString("$openMovSettingsPopup") << fp);
  res =
      tipc::readMessageNB(stream, msg, -1, QEventLoop::ExcludeUserInputEvents);
  assert(res == "ok");

#ifdef MACOSX

  // Bring this application back to front
  ProcessSerialNumber psn = {0, kCurrentProcess};
  SetFrontProcess(&psn);

#endif  // MACOSX

  props->clear();
  {
    // Save the input props to the temporary file
    TIStream is(tfp);
    props->loadData(is);
  }

  // Release the temporary file
  stream << (msg << tipc::clr << QString("$tmpfile_release")
                 << QString("openMovSets"));
  res = tipc::readMessage(stream, msg);
  assert(res == "ok");
}

//---------------------------------------------------------------------------

bool Tiio::isQuicktimeInstalled() {
  // NOTE: This is *NOT* the same function as IsQuickTimeInstalled(), which is
  // implemented locally in the image lib and used there. This function here is
  // actually NEVER USED throughout Toonz, so we're placing a dummy
  // implementation here.

  assert(false);
  return false;
}

#endif  // else
