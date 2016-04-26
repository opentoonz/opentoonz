

#if _MSC_VER >= 1400
#define _CRT_SECURE_NO_DEPRECATE 1
#endif
#include <QSettings>
#include <QCoreApplication>
#include <QNetworkInterface>
#include <QDate>
#include <QUrl>
#include <QStringList>
#include <assert.h>
#include "toonzqt/dvdialog.h"
#include "licensecontroller.h"
#include "licensegui.h"

// Anti-hacker
#define decrypt update0

#ifdef WIN32
#include <windows.h>
#include <Iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#endif

// Cryptopp
#include "pubkey.h"
#include "dh.h"
#include "sha.h"
#include "cryptlib.h"
#include "osrng.h"	// Random Numbeer Generator
#include "eccrypto.h" // Elliptic Curve
#include "ecp.h"	  // F(p) EC
#include "integer.h"  // Integer Operations
#include "base32.h"   // Encodeing-Decoding
#include "nbtheory.h" // ModularSquareRoot(...)

// Formattazione licenza TYPE 1:
// Licenze che contano 4 separatori (2prodotto e 2versione):
//	Licenza non codificata:	mmmmmmPPVVyyMMdd
//	MacAddress(6 caratteri) Prodotto(2 caratteri) Versione(2 caratteri) Data (6 caratteri)
// NOTA: Per questo tipo di licenza	la data è crittata in formato esadecimale,
//   quindi nella decrittazione deve essere riconvertita in formato decimale.

// Formattazione licenza TYPE 2:
// Licenze che contano 2 separatori:
//  Licenza non codificata:	mmmmmmmSSSyyMMdd
//  MACAddress(7 caratteri) Separatore(2 caratteri) Data (6 caratteri)

// Formattazione licenza TYPE 3:
// Licenze che contano 1 solo separatore:
//	Licenza non codificata:	mmmmmmmmSyyMMdd
//	MACAddress(8 caratteri) Separatore(1 carattere) Data (6 caratteri)

#define LICENSE_FORMAT_TYPE 1 // vedi sopra

// Il separatore sta ad indicare il tipo di licenza: standard, pro, student, ecc.. e viene indicato
// nella licenza con lettere tipo "N", "S", "P" ecc. oppure con tre lettere...

// La licenza codificata è lunga 40 caratteri.

const std::string INFINITY_LICENSE = "999999"; // licenze a scadenza infinita

// Definisco la lunghezza del machincode a seconda del tipo di licenza.
// Per i prodotti che hanno 3 separatori nella licenza la lunghezza del machine code è 7.
// Per i prodotti che hanno un solo separatore nella licenza la lunghezza del machine code è 8

#if LICENSE_FORMAT_TYPE == 1
const int MACHINE_CODE_LENGTH =
	6; // prendo solo gli ultimi 6 caratteri del macAddress(che ha 12 caratteri)
const int SEPARATOR_LENGTH = 4; // Numero di caratteri che indicano il numero di separatori.
// Per questo tipo di licenza i primi due sono il license type e gli altri due indicano la versione
const int DATE_LENGTH = 6; // Numero di caratteri che indicano la data nella licenza decrittata
const int DECRYPTED_TOTAL_LENGTH = 16; // lunghezza della licenza decrittata
const char *CODECHAR = "D"; // Ultimo carattere della licenza che identifica il formato
#elif LICENSE_FORMAT_TYPE == 2
const int MACHINE_CODE_LENGTH =
	7; // prendo solo gli ultimi 6 caratteri del macAddress(che ha 12 caratteri)
const int SEPARATOR_LENGTH =
	2;					   // Numero di caratteri che indicano il prodotto nella licenza decrittata
const int DATE_LENGTH = 6; // Numero di caratteri che indicano la data nella licenza decrittata
const int DECRYPTED_TOTAL_LENGTH = 15; // lunghezza della licenza decrittata
const char *CODECHAR = "A"; // Ultimo carattere della licenza che identifica il formato
#elif LICENSE_FORMAT_TYPE == 3
const int MACHINE_CODE_LENGTH =
	8; // prendo solo gli ultimi 6 caratteri del macAddress(che ha 12 caratteri)
const int SEPARATOR_LENGTH =
	1;					   // Numero di caratteri che indicano il prodotto nella licenza decrittata
const int DATE_LENGTH = 6; // Numero di caratteri che indicano la data nella licenza decrittata
const int DECRYPTED_TOTAL_LENGTH = 15; // lunghezza della licenza decrittata
const char *CODECHAR = "A"; // Ultimo carattere della licenza che identifica il formato
#endif

const int ACTIVATION_CODE_LENGTH = 25;
const int LICENSE_CODE_LENGTH = 40;

extern const char *applicationVersion;
const char *wincls = "CLSID";
const char *sentinelfileRootPathMac = "/Library/Receipts/";
using namespace License;

QString m_lastErrorMessage = "No error";

std::string decrypt(std::string code);

// le seguenti due classi sono valide solo per la crittografia
class TRUEHash
	: public CryptoPP::IteratedHashWithStaticTransform<CryptoPP::word32, CryptoPP::BigEndian, 32, 4,
													   TRUEHash>
{
  public:
	static void InitState(HashWordType *state)
	{
		state[0] = 0x01;
		return;
	}
	static void Transform(CryptoPP::word32 *digest, const CryptoPP::word32 *data) { return; }
	static const char *StaticAlgorithmName() { return "TRUE HASH"; }
};

template <class EC, class COFACTOR_OPTION = CryptoPP::NoCofactorMultiplication,
		  bool DHAES_MODE = false>
struct ECIESNullT
	: public CryptoPP::DL_ES<
		  CryptoPP::DL_Keys_EC<EC>,
		  CryptoPP::DL_KeyAgreementAlgorithm_DH<typename EC::Point, COFACTOR_OPTION>,
		  CryptoPP::DL_KeyDerivationAlgorithm_P1363<typename EC::Point, DHAES_MODE,
													CryptoPP::P1363_KDF2<CryptoPP::SHA1>>,
		  CryptoPP::DL_EncryptionAlgorithm_Xor<CryptoPP::HMAC<TRUEHash>, DHAES_MODE>,
		  CryptoPP::ECIES<EC>> {
	static std::string StaticAlgorithmName() { return "ECIES with NULL T"; }
};

// Nella versione attuale (debug) i codici di attivazione sono lunghi 6 lettere, cominciano
// per 'P' e finiscono per 'E'. L'unico codice attivo e' PLEASE
// il codice POVERE provoca la simulazione di un errore di connessione
// Le licenze valide sono Thorin, Balin, Dwalin (permanenti), Gilgamesh (a tempo, scaduta) e
// Spiderman (a tempo, ancora valida). Topolino e' una licenza valida, ma su un'altra macchina

//-----------------------------------------------------------------------------
namespace
{
// e' il nome con cui viene salvata la licenza nei registry (oppure nei file di configurazione)
const char *licenseKeyName = "LicenseCode";
} // namespace
//-----------------------------------------------------------------------------

std::string License::addDefaultChars(std::string code)
{
	return "ASA" + code + "AAAAB";
}

#ifdef WIN32
// Restituisce i MacAddress di tutte le interfacce di rete
std::vector<string> getAllMacAddressWin()
{
	std::vector<string> allMacAddress;
	allMacAddress.clear();
	IP_ADAPTER_INFO AdapterInfo[16];	  // Allocate information for up to 16 NICs
	DWORD dwBufLen = sizeof(AdapterInfo); // Save the memory size of buffer

	DWORD dwStatus = GetAdaptersInfo(  // Call GetAdapterInfo
		AdapterInfo,				   // [out] buffer to receive data
		&dwBufLen);					   // [in] size of receive data buffer
	assert(dwStatus == ERROR_SUCCESS); // Verify return value is valid, no buffer overflow
	if (dwStatus != ERROR_SUCCESS) {
		MsgBox(CRITICAL, QObject::tr("Can't find Adapter Info"));
		return allMacAddress;
	}
	PIP_ADAPTER_INFO pAdapterInfo = AdapterInfo; // Contains pointer to current adapter info
	string MacAddresStr = "";
	do {
		char acMAC[18];
		int type = (int)pAdapterInfo->Type;
		sprintf(acMAC, "%02X%02X%02X%02X%02X%02X", int(pAdapterInfo->Address[0]),
				int(pAdapterInfo->Address[1]), int(pAdapterInfo->Address[2]),
				int(pAdapterInfo->Address[3]), int(pAdapterInfo->Address[4]),
				int(pAdapterInfo->Address[5]));
		string MacAddresStr = toUpper(acMAC);
		pAdapterInfo = pAdapterInfo->Next; // Progress through linked list
		// se type è == 6 allora si tratta di un Ethernet adapter
		if (type == 6)
			allMacAddress.push_back(MacAddresStr);
	} while (pAdapterInfo); // Terminate if last adapter
	return allMacAddress;
}
// Restituisce il Mac Address della prima interfaccia di rete
std::string getMacAddressWin()
{
	std::vector<string> allMacAddress = getAllMacAddressWin();
	std::string MacAddressStr = "";
	for (int i = 0; i < (int)allMacAddress.size(); i++) {
		MacAddressStr = allMacAddress[i];
		if ((int)MacAddressStr.size() == 12)
			break;
	}
	return MacAddressStr;
}

// Restituisce i Machine Code (ultimi MACHINE_CODE_LENGTH caratteri del MAc Address della prima
// interfaccia di Rete.) di
// tutt le interfacce di rete
// Funzione per Windows.
std::vector<string> getAllMachineCodeWin()
{
	std::vector<string> allMachineCode;
	allMachineCode.clear();
	allMachineCode = getAllMacAddressWin();
	for (int i = 0; i < (int)allMachineCode.size(); i++)
		allMachineCode[i] = allMachineCode[i].substr(12 - MACHINE_CODE_LENGTH, MACHINE_CODE_LENGTH);
	return allMachineCode;
}
// ritorna il machine code per Windows. Se nn c'è nessun machine code valido ritorna "";
std::string getMachineCodeWin(int index)
{
	std::vector<string> allMachineCode = getAllMachineCodeWin();
	assert(index < (int)allMachineCode.size() && index >= -1);
	std::string MacAddressStr = "";
	if ((int)allMachineCode.size() > 0) {
		if (index == -1)
			MacAddressStr = allMachineCode[0];
		else
			MacAddressStr = allMachineCode[index];
	}
	return MacAddressStr;
}
#endif

// Resitituisce tutti gli indici delle interfacce di rete (vale per macOS)
std::vector<int> getValidMachineCodesIndex()
{
	std::vector<int> machineCodesIndex;
	QList<QNetworkInterface> allInterface = QNetworkInterface::allInterfaces();
	bool valid = false;
	int i = 0;

	for (i = 0; i < allInterface.size(); i++) {
		QNetworkInterface netInterface;
		netInterface = QNetworkInterface::interfaceFromIndex(i);
		if (!netInterface.isValid())
			continue;
		QString mac = netInterface.hardwareAddress();

		// Se la lunghezza dell'indirizzo e' 17 (caratteri piu' i : separatori)
		// allora e' considerato valido
		if (mac.size() == 17)
			machineCodesIndex.push_back(i);
	}
	if (machineCodesIndex.size() == 0) {
		MsgBox(CRITICAL, QObject::tr("No Network Interface found."));
	}
	return machineCodesIndex;
}

// restituisce il MacAddress per intero
std::string License::getFirstValidMacAddress()
{
	std::string macAddressString = "";
#ifdef WIN32
	macAddressString = getMacAddressWin();
#else
	std::vector<int> validMachineCodesIndex = getValidMachineCodesIndex();
	if ((int)validMachineCodesIndex.size() > 0) {
		// prendo la prima interfaccia di rete valida
		QNetworkInterface netInterface =
			QNetworkInterface::interfaceFromIndex(validMachineCodesIndex[0]);
		QString macAddress = netInterface.hardwareAddress();
		macAddress.remove(QChar(':'), Qt::CaseInsensitive);
		macAddress = macAddress.toUpper();
		macAddressString = macAddress.toStdString();
	}
#endif
	if (macAddressString == "" || macAddressString.size() != 12)
		MsgBox(CRITICAL, QObject::tr("Cannot find a valid computer code."));
	return macAddressString;
}
std::string License::getCodeFromMacAddress(string macAddress)
{
	macAddress = macAddress.substr(12 - MACHINE_CODE_LENGTH, MACHINE_CODE_LENGTH);
	return macAddress;
}

//-----------------------------------------------------------------------------

std::string License::getInstalledLicense()
{
	TFilePath fp(getLicenseFilePath());
	if (TFileStatus(fp).doesExist() == false) {
		m_lastErrorMessage = "Can't find License!";
		return "";
	}
	Tifstream is(fp);
	char buffer[1024];
	is.getline(buffer, sizeof buffer);
	std::string license = buffer;
	return license;
}

void License::getAllValidMacAddresses(std::vector<string> &addresses)
{
#ifdef WIN32
	addresses = getAllMacAddressWin();
#else
	std::vector<int> macAddressIndex;
	macAddressIndex = getValidMachineCodesIndex();

	for (int i = 0; i < (int)macAddressIndex.size(); i++) {
		QNetworkInterface netInterface = QNetworkInterface::interfaceFromIndex(macAddressIndex[i]);
		QString macAddress = netInterface.hardwareAddress();
		macAddress.remove(QChar(':'), Qt::CaseInsensitive);
		macAddress = macAddress.toUpper();
		std::string macAddressString = macAddress.toStdString();
		addresses.push_back(macAddressString);
	}
#endif
}

bool getMachineCodeFromDecryptedCode(string decryptedCode, string &macAddress)
{
	if (decryptedCode.size() != DECRYPTED_TOTAL_LENGTH) {
		m_lastErrorMessage = "Invalid License length";
		return false;
	}
	macAddress = decryptedCode.substr(0, MACHINE_CODE_LENGTH);
	return true;
}

bool getDateFromDecryptedCode(string decryptedCode, string &date)
{
	if (decryptedCode.size() != DECRYPTED_TOTAL_LENGTH) {
		m_lastErrorMessage = "Invalid License length";
		return false;
	}
	date = decryptedCode.substr(MACHINE_CODE_LENGTH + SEPARATOR_LENGTH, DATE_LENGTH);
	return true;
}

bool getSeparatorFromDecryptedCode(string decryptedCode, string &separator)
{
	if (decryptedCode.size() != DECRYPTED_TOTAL_LENGTH) {
		m_lastErrorMessage = "Invalid License length";
		return false;
	}
	separator = decryptedCode.substr(MACHINE_CODE_LENGTH, SEPARATOR_LENGTH);
	return true;
}

bool checkSeparatorFromDecryptedCode(string decryptedCode, LicenseType &licenseType)
{
	string separator = "";
	if (!getSeparatorFromDecryptedCode(decryptedCode, separator))
		return false;

	if (separator.compare("LT64") == 0) {
		licenseType = LINETEST_LICENSE;
	} else
		licenseType = INVALID_LICENSE;
	return true;
}
/*
// data la licenza decriptata resituisce il tipo di licenza
void checkSeparatorFromDecryptedCode(string decryptedCode, string &separator, int &licenseType)
{
QString QdecryptedLicense = QString::fromStdString(decryptedCode);
	if(QdecryptedLicense.contains("LT"))
	{
		licenseType = LINETEST_LICENSE;
		separator = "LT";
	}
	else
		  licenseType = INVALID_LICENSE;
}

// ritorna il tipo di licenza, il machine code e la data di scadenza della licenza
// se non c'è un separatre valido ritorna INVALID_LICENSE
int getMachineCodeAndExpDateFromDecryptedCode(std::string decryptedCode, string &machineCode, string
&expDate)
{
	if(decryptedCode.length()!=LICENSE_CODE_LENGTH) return INVALID_LICENSE;
	machineCode = decryptedCode.substr(0,MACHINE_CODE_LENGTH);
	expDate = decryptedCode.substr(decryptedCode.size()-6,6);

	QStringList QdecryptedLicenseList;
	int licenseType = 0;
	string separator ;
	checkSeparatorFromDecryptedCode(decryptedCode,separator,licenseType);
	QdecryptedLicenseList = codeQString.split(QString::fromStdString(separator));
	//se manca il separatore ritorna "".
	if(licenseType == INVALID_LICENSE) return licenseType;
	if(QdecryptedLicenseList.size()<2) return licenseType;
	machineCode = QdecryptedLicenseList.at(0).toStdString();
	expDate = QdecryptedLicenseList.at(1).toStdString();
	return licenseType;
}*/

LicenseType License::checkLicense(std::string code)
{
	if (code == "")
		code = getInstalledLicense();
	// decodifca code; controlla il MAC; se c'e' una data di scadenza
	// la confronta con oggi.
	QString codeQString = QString::fromStdString(code);
	// Elimino i trattini dalla licenza
	codeQString = codeQString.remove(QChar('-'), Qt::CaseInsensitive);
	code = codeQString.toStdString();

	if (code.size() != LICENSE_CODE_LENGTH)
		return INVALID_LICENSE;

	// decodifca code; controlla il MAC; se c'e' una data di scadenza
	// la confronta con oggi.
	std::string decryptedLicense = decryptLicense(code);

	if (decryptedLicense == "")
		return INVALID_LICENSE; // throw std::string("CryptoPP::DecodingResult: Invalid Coding"); }

	string machineCodeFromDecryptedLecense = "";
	bool ret = getMachineCodeFromDecryptedCode(decryptedLicense, machineCodeFromDecryptedLecense);
	if (!ret)
		return INVALID_LICENSE;
	string expDate = "";
	ret = getDateFromDecryptedCode(decryptedLicense, expDate);
	if (!ret)
		return INVALID_LICENSE;

	LicenseType licenseType = INVALID_LICENSE;
	ret = checkSeparatorFromDecryptedCode(decryptedLicense, licenseType);
	if (!ret)
		return INVALID_LICENSE;
	if (licenseType == INVALID_LICENSE)
		return INVALID_LICENSE;

	std::vector<string> allMacAddresses;
	// Recupero tutti i mac address validi presenti sulla macchina corrente.
	getAllValidMacAddresses(allMacAddresses);

	bool validMac = false;
	int t = 0;
	for (t = 0; t < (int)allMacAddresses.size(); t++) {
		std::string machineCode = getCodeFromMacAddress(allMacAddresses[t]);
		if (machineCodeFromDecryptedLecense == machineCode) {
			validMac = true;
			break;
		}
	}
	if (validMac == false)
		return INVALID_LICENSE;

	if (expDate != INFINITY_LICENSE) {
		QString expirationDateString = "20" + QString::fromStdString(expDate);
		QDate expirationDate = expirationDate.fromString(expirationDateString, "yyyyMMdd");
		QDate currentDate = QDate::currentDate();

		if (!expirationDate.isValid() || expirationDate <= currentDate)
			licenseType = INVALID_LICENSE;
	}
	return (licenseType);
}
/*
 int *License::checkLicense()
 {
 static int globalSave;
 std::string license = getInstalledLicense();
 if(isValidLicense(license))
 return &globalSave;
 else
 return 0;
 }
 */
//-----------------------------------------------------------------------------

bool License::isTemporaryLicense(std::string code)
{
	// if(checkLicense(code)==INVALID_LICENSE) return false;
	QString codeQString = QString::fromStdString(code);
	codeQString = codeQString.remove(QChar('-'), Qt::CaseInsensitive);
	code = codeQString.toStdString();
	// decodificazione di code
	std::string decryptedLicense = decryptLicense(code);
	QString QdecryptedLicense = QString::fromStdString(decryptedLicense);
	LicenseType licenseType = INVALID_LICENSE;
	bool ret = checkSeparatorFromDecryptedCode(decryptedLicense, licenseType);
	if (licenseType == INVALID_LICENSE || !ret)
		return false;

	std::vector<string> allMacAddresses;
	getAllValidMacAddresses(allMacAddresses);
	std::string machineCodeFromDecryptedLecense = "";
	ret = getMachineCodeFromDecryptedCode(decryptedLicense, machineCodeFromDecryptedLecense);
	if (!ret)
		return false;
	int t = 0;
	bool validMac = false;
	for (t = 0; t < (int)allMacAddresses.size(); t++) {
		std::string machineCode = getCodeFromMacAddress(allMacAddresses[t]);
		if (machineCodeFromDecryptedLecense == machineCode) {
			validMac = true;
			break;
		}
	}
	if (validMac == false)
		return false;
	string expDate = "";
	ret = getDateFromDecryptedCode(decryptedLicense, expDate);
	if (!ret)
		return false;
	if (expDate != INFINITY_LICENSE) {
		QString expirationDateString = "20" + QString::fromStdString(expDate);
		QDate expirationDate = expirationDate.fromString(expirationDateString, "yyyyMMdd");
		return (expirationDate.isValid());
	} else
		return false;
	return false;
}

//-----------------------------------------------------------------------------

int License::getDaysLeft(std::string code)
{
	// Elimino i segni "-" dal code
	QString codeQString = QString::fromStdString(code);
	codeQString = codeQString.remove(QChar('-'), Qt::CaseInsensitive);
	code = codeQString.toStdString();
	// decodificazione di code
	std::string decryptedLicense = decryptLicense(code);
	QString QdecryptedLicense = QString::fromStdString(decryptedLicense);
	QStringList QdecryptedLicenseList; // = QdecryptedLicense.split("%");
	string separator;
	LicenseType licenseType = INVALID_LICENSE;
	bool ret = checkSeparatorFromDecryptedCode(decryptedLicense, licenseType);
	if (licenseType == INVALID_LICENSE || !ret)
		return 0;

	string expDate = "";
	ret = getDateFromDecryptedCode(decryptedLicense, expDate);
	if (!ret)
		return 0;

	QString expirationDateString = "20" + QString::fromStdString(expDate);

	QDate expirationDate = expirationDate.fromString(expirationDateString, "yyyyMMdd");

	bool validExpirationDate = expirationDate.isValid();
	assert(validExpirationDate == true);
	QDate currentDate = QDate::currentDate();
	int day = currentDate.daysTo(expirationDate);
	return currentDate.daysTo(expirationDate);
}

//-----------------------------------------------------------------------------

bool License::isActivationCode(std::string code)
{
	// Elimino i segni "-" dal code
	QString codeQString = QString::fromStdString(code);
	codeQString = codeQString.remove(QChar('-'), Qt::CaseInsensitive);
	code = codeQString.toStdString();
	int length = code.length();
	if (length != ACTIVATION_CODE_LENGTH)
		return 0;
	int tot = 0;
	for (int i = 0; i < length - 1; i++) {
		int chrAscii = code[i];
		tot += chrAscii; // std::cout << chrAscii;
	}
	tot = tot % 26;
	return ((tot + 65) == (int)code[length - 1]);
	// return code.length() == 6 && code[0]=='P' && code[5]=='E';
}
//---------------------------------------------------------------------
// Controllo della sentinella. Lo facciamo per controllare che la data della
// prima installazione sia minore della data corrente.
//---------------------------------------------------------------------
#ifdef MACOSX

void strreverse(char *begin, char *end)
{
	char aux;
	while (end > begin)
		aux = *end, *end-- = *begin, *begin++ = aux;
}

void itoa(int value, char *str, int base)
{

	static char num[] = "0123456789abcdefghijklmnopqrstuvwxyz";
	char *wstr = str;
	int sign;

	// Validate base
	if (base < 2 || base > 35) {
		*wstr = '\0';
		return;
	}

	// Take care of sign
	if ((sign = value) < 0)
		value = -value;

	// Conversion. Number is reversed.
	do
		*wstr++ = num[value % base];
	while (value /= base);
	if (sign < 0)
		*wstr++ = '-';
	*wstr = '\0';

	// Reverse string
	strreverse(str, wstr - 1);
}

TFilePath getSentileFilePath(string regKey)
{
	string filenameString = "." + regKey.substr(1, regKey.size() - 2);
	return TFilePath(sentinelfileRootPathMac) + TFilePath(filenameString);
}
#endif

char *getC(char *c)
{
	int num = (int)c[0];
	num = (num % 16);
	char *ch = new char;
	*ch = '0';
#ifdef MACOSX
	itoa(num, ch, 16);
#else
	_itoa(num, ch, 16);
#endif
	return ch;
}
std::string License::createClsid(string fullMacAddress, char *licenseType)
{
	string RFullMacAddress = fullMacAddress;
	reverse(RFullMacAddress.begin(), RFullMacAddress.end());
	int i;

	// std::cout << "applicationVersion=" << applicationVersion << std::endl;
	string applicationVersionString = string(applicationVersion);
	// std::cout << "applicationVersionString=" << applicationVersionString << std::endl;

	string fullString = "";
	if (LICENSE_FORMAT_TYPE == 3)
		fullString = licenseType + RFullMacAddress + applicationVersionString.substr(0, 1) +
					 applicationVersionString.substr(2, 1) + fullMacAddress + "0" +
					 applicationVersionString.substr(2, 1) + applicationVersionString.substr(0, 1) +
					 "00";
	else
		fullString = licenseType + RFullMacAddress + applicationVersionString.substr(0, 1) +
					 applicationVersionString.substr(2, 1) + fullMacAddress +
					 applicationVersionString.substr(2, 1) + applicationVersionString.substr(0, 1) +
					 "00";
	assert(fullString.length() == 32);
	string clsid = "{";
	for (i = 0; i < 32; i++) {
		if (i == 8 || i == 12 || i == 16 || i == 20)
			clsid += "-";
		char c0 = fullString[i];
		clsid += getC(&c0);
	}
	// sostituiamo due elementi affinche cambino al cambiare di applicationVersion per
	// garantire nuove licenze demo al cambio di versione

	// std::cout << "applicationVersion[0]=" << applicationVersion[0] << std::endl;
	// std::cout << "applicationVersion[2]=" << applicationVersion[2] << std::endl;

	// clsid[8]=applicationVersion[0];
	// clsid[13]=applicationVersion[2];
	clsid[1] = toupper(clsid[1]);
	clsid += "}";
	return clsid;
}
// Se la sentinella non esiste ritorna la stringa "noExists"
std::string License::readSentinelDate(string regKey)
{
#ifdef WIN32
	// string wincls="CLSID";
	TCHAR buffer[1024];
	unsigned long bufferSize = sizeof(buffer);
	wstring value;
	string regKeyF = string(wincls) + "\\" + regKey + "\\InProcServer32";
	HKEY hcplF;
	if (RegOpenKeyEx(HKEY_CLASSES_ROOT, toWideString(regKeyF).c_str(), 0, KEY_READ, &hcplF) ==
		ERROR_SUCCESS) {
		RegQueryValueEx(hcplF, L"Version", 0, 0, (BYTE *)buffer, &bufferSize);
	} else
		return "noExists";
	RegCloseKey(hcplF);
	return toString(wstring(buffer));
#else
	TFilePath fp = getSentileFilePath(regKey);
	if (TFileStatus(fp).doesExist() == false)
		return "noExists";
	Tifstream is(fp);
	char buffer[1024];
	is.getline(buffer, sizeof buffer);
	std::string cdate = buffer;
	return cdate;
#endif
}

bool License::isValidSentinel(string license)
{
	string decryptedLicense = decryptLicense(license);
	LicenseType licenseType = INVALID_LICENSE;
	bool ret = checkSeparatorFromDecryptedCode(decryptedLicense, licenseType);
	if (!ret)
		return false;
	string separator = "";
	ret = getSeparatorFromDecryptedCode(decryptedLicense, separator);
	if (!ret)
		return false;
	std::vector<string> allMacAddress;
	getAllValidMacAddresses(allMacAddress);
	string cryptedSentinelDateString;
	for (int i = 0; i < (int)allMacAddress.size(); i++) {
		std::string fullMacAddress = allMacAddress[i];
		string licenseType = "";
		if (LICENSE_FORMAT_TYPE == 1)
			licenseType = separator.substr(0, 2);
		else
			licenseType = separator;
		char *licType = new char;
		strcpy(licType, licenseType.c_str());
		string regKey = createClsid(fullMacAddress, licType);
		cryptedSentinelDateString = readSentinelDate(regKey);
		if (cryptedSentinelDateString != "noExists")
			break;
	}
	// Se la sentinella non esiste vuol dire che il software non
	//è stato istallato e quindi non viene fatto alcun controllo sulla sentinella.
	if (cryptedSentinelDateString == "noExists")
		return true;
	string sentinelDateString = decrypt(cryptedSentinelDateString).substr(0, 8);
	QDate sentinelDate =
		sentinelDate.fromString(QString::fromStdString(sentinelDateString), "yyyyMMdd");
	if (!sentinelDate.isValid())
		return false;
	QDate currentDate = QDate::currentDate();
	return (sentinelDate <= currentDate);
}

//-----------------------------------------------------------------------------
myHttp::myHttp(const QString &requestToServer, LicenseWizard *licenseWizard)
#if QT_VERSION < 0x050000
	: QHttp()
#endif
{
	m_licenseWizard = licenseWizard;
	// m_licenseWizard = new LicenseWizard(((QWidget*)TApp::instance()->getMainWindow()));

	m_licenseWizard->setPage(2);

	QUrl url(requestToServer);

#if QT_VERSION >= 0x050000

	QString urlTemp = requestToServer;
	QStringList urlList = urlTemp.split('?');
	QStringList paramList = urlList.at(1).split('&');
	if (!url.userName().isEmpty())
		setUser(url.userName(), url.password());

	m_httpRequestAborted = false;

	QString param;
	param = QString("?") + paramList.at(0) + QString("&") + paramList.at(1) + QString("&") +
			paramList.at(2) + QString("&") + paramList.at(3);

	QNetworkAccessManager manager;
	QNetworkReply *reply = manager.get(QNetworkRequest(url.path() + param));

	QEventLoop loop;

	// connect(reply, SIGNAL(readyRead(const QHttpResponseHeader &)), &loop,
	// SLOT(readyReadexec(const QHttpResponseHeader &)));
	// connect(reply, SIGNAL(requestFinished(int, bool)), &loop, SLOT(httpRequestFinished(int,
	// bool)));
	connect(reply, SIGNAL(requestStarted(int)), &loop, SLOT(httpRequestStarted(int)));
	// connect(reply, SIGNAL(responseHeaderReceived(const QHttpResponseHeader &)), &loop,
	// SLOT(readResponseHeader(const QHttpResponseHeader &)));
	connect(reply, SIGNAL(authenticationRequired(const QString &, quint16, QAuthenticator *)),
			&loop, SLOT(slotAuthenticationRequired(const QString &, quint16, QAuthenticator *)));
	connect(reply, SIGNAL(stateChanged(int)), &loop, SLOT(httpStateChanged(int)));

	connect(&manager, SIGNAL(finished(QNetworkReply *)), this,
			SLOT(httpRequestFinished(QNetworkReply *)));

	loop.exec();
#else

	connect(this, SIGNAL(readyRead(const QHttpResponseHeader &)), this,
			SLOT(readyReadexec(const QHttpResponseHeader &)));
	connect(this, SIGNAL(requestFinished(int, bool)), this, SLOT(httpRequestFinished(int, bool)));
	connect(this, SIGNAL(requestStarted(int)), this, SLOT(httpRequestStarted(int)));
	connect(this, SIGNAL(responseHeaderReceived(const QHttpResponseHeader &)), this,
			SLOT(readResponseHeader(const QHttpResponseHeader &)));
	connect(this, SIGNAL(authenticationRequired(const QString &, quint16, QAuthenticator *)), this,
			SLOT(slotAuthenticationRequired(const QString &, quint16, QAuthenticator *)));
	connect(this, SIGNAL(stateChanged(int)), this, SLOT(httpStateChanged(int)));
	QHttp::ConnectionMode mode =
		url.scheme().toLower() == "https" ? QHttp::ConnectionModeHttps : QHttp::ConnectionModeHttp;

	setHost(url.host(), mode, url.port() == -1 ? 0 : url.port());
	QString urlTemp = requestToServer;
	QStringList urlList = urlTemp.split('?');
	QStringList paramList = urlList.at(1).split('&');
	if (!url.userName().isEmpty())
		setUser(url.userName(), url.password());

	m_httpRequestAborted = false;

	QString param;
	param = QString("?") + paramList.at(0) + QString("&") + paramList.at(1) + QString("&") +
			paramList.at(2) + QString("&") + paramList.at(3);
	m_httpGetId = get(url.path() + param); //, file);
#endif
}

#if QT_VERSION < 0x050000
void myHttp::readResponseHeader(const QHttpResponseHeader &responseHeader)
{
	int err = responseHeader.statusCode();
	if (err != 200 && err != 502) {
		// MsgBox(INFORMATION,tr("Download failed: %1.").arg(responseHeader.reasonPhrase()));
		/*QMessageBox::information(0, tr("HTTP"),
		 tr("Download failed: %1.")
		 .arg(responseHeader.reasonPhrase()));*/
		m_httpRequestAborted = true;
		abort();
	}
}
#endif

//-----------------------------------------------------------------------------
void myHttp::httpStateChanged(int status)
{
	QString stateStr;
	switch (status) {
	case 1:
		stateStr = "A host name lookup is in progress...";
		break;
	case 2:
		stateStr = "Connecting...";
		break;
	case 3:
		stateStr = "Sending informations...";
		break;
	case 4:
		stateStr = "Reading informations...";
		break;
	case 5:
		stateStr = "Connected.";
		break;
	case 6:
		stateStr = "The connection is closing down, but is not yet closed. (The state will be "
				   "Unconnected when the connection is closed.)";
	default:
		stateStr = "There is no connection to the host.";
	}
	status = state();
	qDebug("Status: %d", status);
	m_licenseWizard->m_stateConnectionLbl->setText(stateStr);
}

#if QT_VERSION >= 0x050000
void myHttp::httpRequestFinished(QNetworkReply *reply)
#else
void myHttp::httpRequestFinished(int requestId, bool error)
#endif
{

	QByteArray arr;
	string license;
	std::string newLicense;
	QString qstr;
#if QT_VERSION < 0x050000)
	if (requestId != m_httpGetId)
		return;
	if (error || m_httpRequestAborted)
		goto errore;

	arr = readAll();
#else
	if (reply->error() != QNetworkReply::NoError)
		goto errore;
	arr = reply->readAll();
#endif
	qstr = QString(arr);
	int startIndex = qstr.indexOf("Startcode");
	int endIndex = qstr.indexOf("Endcode");

	if ((endIndex - startIndex - 9) <= 0)
		goto errore;
	license = qstr.toStdString();
	license = license.substr(startIndex + 9, endIndex - startIndex - 9);

	if (checkLicense(license) == INVALID_LICENSE) {
		MsgBox(WARNING, tr("Activation error: ") + QString::fromStdString(license));
		// QMessageBox::information(0, QString("Invalid License"), QString::fromStdString(license));
		goto errore;
	}

	bool ok = setLicense(license);
	if (!ok) {
		MsgBox(CRITICAL, tr("Cannot write the license code to disk."));
		return;
	}

	// Verifico che tutto sia andato bene
	newLicense = getInstalledLicense();
	if (license != newLicense) {
		MsgBox(CRITICAL, tr("Can't write license on disk!"));
		// QMessageBox::information(0, QString("Error"), "Non e' possibile scrivere la licenza su
		// disco!");
		m_licenseWizard->setPage(1);
		return;
	} else {
		m_licenseWizard->accept();
		// MsgBox(INFORMATION,tr("Activation successfully!"));
		return;
	}

errore:
	QString errore = errorString();
	// MsgBox(WARNING,errorString());
}

void myHttp::slotAuthenticationRequired(const QString &hostName, quint16,
										QAuthenticator *authenticator)
{
	assert(false);
}

//-----------------------------------------------------------------------------------
extern const char *applicationVersion;
extern const char *applicationName;
ActivateStatus License::activate(string activationCode, LicenseWizard *licenseWizard)
{
	// licenseWizard->hide();//lose();//accept();
	string MacAddress = getFirstValidMacAddress();
	string tabversion = string(applicationVersion);
	string appname = string(applicationName);
	QString requestToServer = "http://www.the-tab.com/cgi-shl/tab30/activate/run.asp";
	requestToServer =
		requestToServer + QString("?Activation_Code=") + QString::fromStdString(activationCode);
	requestToServer =
		requestToServer + QString("&MAC_Address=") + QString::fromStdString(MacAddress);
	requestToServer = requestToServer + QString("&AppName=") + QString::fromStdString(appname);
	requestToServer = requestToServer + QString("&Version=") + QString::fromStdString(tabversion);
	requestToServer.remove(" ");

	myHttp *http = new myHttp(requestToServer, licenseWizard);
	return OK;
}

//-----------------------------------------------------------------------------------

TFilePath License::getMacAddressFilePath()
{
	return TFilePath(TEnv::getStuffDir() + "config" + "computercode.txt");
}

TFilePath License::getLicenseFilePath()
{
	return TFilePath(TEnv::getStuffDir() + "config" + "license.dat");
}
//-----------------------------------------------------------------------------

bool License::setLicense(std::string license)
{
	if (checkLicense(license) == INVALID_LICENSE)
		return false;
	TFilePath licenseFilePath = getLicenseFilePath();
	Tofstream os(licenseFilePath);
	os << license;

	return true;
	/*
	 QSettings settings(
	 "license.dat");
	 settings.setPath(QSettings::IniFormat,
	 QSettings::SystemScope,
	 QString::fromStdWString(configDir.getWideString()));
	 QSettings settings(
	 QSettings::SystemScope,
	 QCoreApplication::organizationName(),
	 QCoreApplication::applicationName());
	 settings.setValue(licenseKeyName, QString::fromStdString(license));
	 settings.sync(); */
	// non voglio rischiare che un crash mi faccia perdere la licenza
	// con sync mi assicuro che venga scritta subito (su disco o nei registry)
	// return true;
}
// Se c'è installata una licenza allora la decripta e legge il machine code.
// Se questo è uguale al mac Address di una delle interfacce di rete presenti
// sulla macchina corrente allora scrive questo MAC su file.
// In caso contrario scrive su file il mac address della prima interfaccia di rete valida.
bool License::writeMacAddress()
{
	std::string macAddress = "";
	std::string decryptedLicense = "";
	string encryptedLicense = getInstalledLicense();
	if (encryptedLicense != "") {
		// Elimino i trattini dalla licenza
		QString qlicense =
			QString::fromStdString(encryptedLicense).remove(QChar('-'), Qt::CaseInsensitive);
		string code = qlicense.toStdString();
		// decripto la licenza installata
		decryptedLicense = decryptLicense(code);
	}
	if (decryptedLicense != "") {
		string macAddressFromDecryptedLecense = "";
		bool ret =
			getMachineCodeFromDecryptedCode(decryptedLicense, macAddressFromDecryptedLecense);
		std::vector<string> allMacAddresses;
		// Recupero tutti i mac address validi presenti sulla macchina corrente.
		getAllValidMacAddresses(allMacAddresses);
		int t = 0;
		for (t = 0; t < (int)allMacAddresses.size(); t++) {
			std::string machineCode = getCodeFromMacAddress(allMacAddresses[t]);
			if (macAddressFromDecryptedLecense == machineCode) {
				macAddress = allMacAddresses[t];
				break;
			}
		}
	}
	if (macAddress == "")
		macAddress = getFirstValidMacAddress();
	TFilePath macAddressFilePath = getMacAddressFilePath();
	Tofstream os(macAddressFilePath);
	os << macAddress;
	return true;
}
//-----------------------------------------------------------------------------

// Restituisce la stringa decriptata.
// Se il code non è un codice valido restituisce "".
std::string decrypt(std::string code)
{
	CryptoPP::Integer p("5860230647");
	CryptoPP::Integer a("-3");
	CryptoPP::Integer b("-2814718968");
	CryptoPP::Integer n("158380489"); // R from ECB
	CryptoPP::Integer h("37");		  // S from ECB

	CryptoPP::Integer x("0");
	CryptoPP::Integer y("0");

	CryptoPP::AutoSeededRandomPool rng;
	CryptoPP::ECP ec(p, a, b);

	ECIESNullT<CryptoPP::ECP>::PrivateKey PrivateKey;

	// Inizializzazione Chiave
	PrivateKey.Initialize(ec, CryptoPP::ECP::Point(x, y), n, h);

	CryptoPP::Base32Decoder Decoder;

	Decoder.Put(reinterpret_cast<const byte *>(code.c_str()), code.length());
	Decoder.MessageEnd();

	// Scratch forBase 32 Encoded Ciphertext
	unsigned int DecodedTextLength = Decoder.MaxRetrievable();
	byte *DecodedText = new byte[DecodedTextLength + 1];

	if (NULL == DecodedText)
		return false; // DecodedText Allocation Failure }
	DecodedText[DecodedTextLength] = '\0';
	Decoder.Get(DecodedText, DecodedTextLength);

	// Decryption
	ECIESNullT<CryptoPP::ECP>::Decryptor Decryptor(PrivateKey);

	int plainTextLength = Decryptor.MaxPlaintextLength(DecodedTextLength);

	// Scratch for Decryption
	unsigned int RecoveredTextLength = Decryptor.MaxPlaintextLength(DecodedTextLength);
	if (0 == RecoveredTextLength) {
		return "";
	} // throw std::string("codeLength non valida (too long or too short)"); }

	byte *RecoveredText = new byte[4 * plainTextLength];
	if (NULL == RecoveredText) {
		return "";
	} // throw std::string( "RecoveredText CipherText Allocation Failure" ); }

	// Decryption
	CryptoPP::DecodingResult Result =
		Decryptor.Decrypt(rng, DecodedText, DecodedTextLength, RecoveredText);

	if (false == Result.isValidCoding)
		return ""; // throw std::string("CryptoPP::DecodingResult: Invalid Coding"); }
	std::string license = reinterpret_cast<char *>(RecoveredText);

	string decripetedLicense = license.substr(0, plainTextLength);
	return decripetedLicense;
}
//-----------------------------------------------------------------------------

std::string License::decryptLicense(std::string code)
{
	if (code == "")
		return "";
	// Sostituisco l'ultima lettera della licenza, che serve ad identificare il tipo di
	// formattazione della licenza
	// Rimetto il carattere "A" per permettere la corretta decrittazione.
	// Per maggiorni info vedere il generatore di licenze
	int codesize = code.size();
	code = code.substr(0, codesize - 1) + "A";
	code = addDefaultChars(code);

	std::string decripetedLicense = decrypt(code);

	if (LICENSE_FORMAT_TYPE == 1) {
		if (decripetedLicense.size() < 5)
			return "";
		// La data nella licenza è in formato esadecimale, la converto in decimale.
		QString datastring = "0x" + QString::fromStdString(
										decripetedLicense.substr(decripetedLicense.size() - 5, 5));
		bool ok = false;
		int date = datastring.toInt(&ok, 16);
		// sostituisco la data mettendola in formato esadecimale
		decripetedLicense = decripetedLicense.substr(0, decripetedLicense.size() - 5) +
							QString::number(date).toStdString();
	}
	return decripetedLicense;
}
//-----------------------------------------------------------------------------

bool License::isStoryPlannerPro()
{
	string license = getInstalledLicense();
	string decryptedLicense = decryptLicense(license);
	LicenseType licenseType = INVALID_LICENSE;
	bool ret = checkSeparatorFromDecryptedCode(decryptedLicense, licenseType);
	if (!ret)
		return false;
	if (licenseType == STORYPLANNERPRO_LICENSE)
		return true;
	else
		return false;
}

QString License::getLastError()
{
	return m_lastErrorMessage;
}
