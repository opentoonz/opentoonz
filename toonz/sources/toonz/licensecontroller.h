

#ifndef LICENSECONTROLLER_INCLUDED
#define LICENSECONTROLLER_INCLUDED

#if QT_VERSION >= 0x050000
#include <QNetworkAccessManager>
#else
#include <QHttp>
#endif
#include <QLabel>
#include "licensegui.h"
#include "toonzqt/dvdialog.h"
#include <string>

//---Per il controllo della sentinella
#include "tfilepath_io.h"
#include "tfilepath.h"
#include <tconvert.h>
#include "tenv.h"
#include "tsystem.h"
//----

// Anti-hacker
#define decryptLicense update
#define License VirtualEditorObject

namespace License
{

enum ActivateStatus {
	OK,					 // attivazione completata con successo
	BAD_ACTIVATION_CODE, // codice di attivazione sbagliato (checksum non ok)
	CONNECTION_ERROR,	// problemi di rete
	ACTIVATION_REFUSED   // il server ha rifiutato l'attivazione.
};

// restituisce il primo macAddress valido
std::string getFirstValidMacAddress();

//Restituisce tutti i mac address valido delle interfacce di rete della macchine corrente
void getAllValidMacAddresses(std::vector<std::string> &addresses);
// restituisce il machineCode della iesima interfaccia di rete della macchina corrente
// Il machine code e' costituito dagli ultimi 8 caratteri del macAdress
std::string getCodeFromMacAddress(std::string macAddress);

// restituisce la licenza corrente (installata sulla macchina)
std::string getInstalledLicense();

enum LicenseType {
	INVALID_LICENSE = 0,
	STUDENT_LICENSE = 1,
	STANDARD_LICENSE = 2,
	PRO_LICENSE = 3,
	MANGA_LICENSE = 4,
	STORYPLANNER_LICENSE = 5,
	STORYPLANNERPRO_LICENSE = 6,
	LINETEST_LICENSE = 7
};

// ritorna true sse code e' una licenza AND il mac e' giusto AND
// (la lic e' permanente OR non e' ancorascaduta)
//bool isValidLicense(std::string code);
LicenseType checkLicense(std::string code = "");

//ritorna il puntatore ad un intero se la licenza e' valida, altrimenti ritorna 0
//int* checkLicense();
// ritorna true sse code e' una licenza temporanea
bool isTemporaryLicense(std::string code);

// se code e' una licenza temporanea ritorna il numero di giorni rimasti (se days<=0 licenza scaduta)
// negli altri casi ritorna 0
int getDaysLeft(std::string code);

// ritorna true sse code "sembra" un codice di attivazione (checksum ok).
// n.b. NON controlla che il codice sia valido (cosa che richiede una connessione al nostr server)
bool isActivationCode(std::string code);

// prova ad attivare la licenza con il codice di attivazione 'activationCode'.
// se e' un buon codice di attivazione si connette al nostro server fornendogli
// il codice di attivazione e il machine code. riceve una licenza e la installa
// (chiamando setLicense())
ActivateStatus activate(std::string activationCode, LicenseWizard *licenseWizard);

// se isValidLicense(license) installa license e ritorna true
bool setLicense(std::string license);
// scrive su disco il macAddress della macchina corrente
bool writeMacAddress();
//std::string decrypt(std::string code);
std::string decryptLicense(std::string code);
// aggiunge i caratteri di default alla licenza necessari per il decrypt;
// Fatto per rendere la licenza digitata dall'utente piu' corta.
std::string addDefaultChars(std::string code);

// ritorna il fullpath del file di licenza
TFilePath getLicenseFilePath();
// ritorna il fullpath del file del macAddress
TFilePath getMacAddressFilePath();

//Restituisce la sentinella cryptata
std::string readSentinelDate(std::string regKey);
//Dando in input la sentinella(criptata) ritorna true se la data di
//prima installazione(memorizzata nella sentinella) e' minore della data corrente.
bool isValidSentinel(std::string license);
std::string createClsid(std::string fullMacAddress, char *licenseType);
//Controlla se la licenza e' del tipo STORYPLANNERPRO
bool isStoryPlannerPro();
// Ritorna l'ultimo messaggio di errore
// se non c'è stato alcun errore ritorna "no error"
QString getLastError();
} // namespace License

class myHttp
#if QT_VERSION < 0x050000
	: public QHttp
#endif
{
	Q_OBJECT
	bool m_httpRequestAborted;
	int m_httpGetId;
	LicenseWizard *m_licenseWizard;

public:
	myHttp(const QString &requestToServer, LicenseWizard *licenseWizard);
protected slots:
	void httpRequestStarted(int requestId) {}
#if QT_VERSION >= 0x050000
	void httpRequestFinished(QNetworkReply *);
#else
	void httpRequestFinished(int requestId, bool error);
	void readResponseHeader(const QHttpResponseHeader &responseHeader);
	void readyReadExec(const QHttpResponseHeader &head) {}
#endif
	void slotAuthenticationRequired(const QString &hostName, quint16, QAuthenticator *authenticator);
	void httpStateChanged(int state);
};

#endif
