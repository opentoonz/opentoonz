

#ifdef __LP64__

//Toonz includes
#include "tvectorimage.h"
#include "tstroke.h"
#include "trop.h"

//Qt includes
#include <QSharedMemory>

/*
なぜか tipc.h の

 template<typename T>
 QDataStream& operator>>(QDataStream& ds, std::vector<T>& vec)

が T=TThickPoint でインスタンス化されようとして曖昧ですエラーになる
*/
//tipc includes
#include "tipc.h"
#include "t32bitsrv_wrap.h"

#include "tfont.h"

//**************************************************************************
//    Local namespace stuff
//**************************************************************************

namespace
{

QDataStream &operator>>(QDataStream &ds, TThickPoint &p)
{
	return ds >> p.x >> p.y >> p.thick;
}

void readVImage(TVectorImageP &vi, tipc::Message &msg)
{
	std::vector<TThickPoint> strokeCPs;

	//Read in all strokes
	while (!msg.ds().atEnd()) {
		strokeCPs.clear();
		msg >> strokeCPs;

		vi->addStroke(new TStroke(strokeCPs));
	}

	vi->group(0, vi->getStrokeCount());
}

} //namespace

//**************************************************************************
//    TFontManager Private stuff
//**************************************************************************

/*
  The proxied Impl private just caches some data of the actual
  background counterpart
*/
struct TFontManager::Impl {
	int m_ascender;
	int m_descender;

	Impl() {}
	~Impl() {}
};

//**************************************************************************
//    TFontManager Proxied implementation
//**************************************************************************

TFontManager::TFontManager()
{
	m_pimpl = new TFontManager::Impl();
}

//--------------------------------------------------------------

TFontManager::~TFontManager()
{
	delete m_pimpl;
}

//--------------------------------------------------------------

TFontManager *TFontManager::instance()
{
	static TFontManager theInstance;
	return &theInstance;
}

//--------------------------------------------------------------

//!This function is retained from old 32-bit legacy code.
//!Its use is now forbidden - use the TFontManager directly instead.
TFont *TFontManager::getCurrentFont()
{
	assert(false);
	return 0;
}

//--------------------------------------------------------------

//!\note Throws TFontLibraryLoadingError if fonts could not be loaded
void TFontManager::loadFontNames()
{
	//Connect to the 32-bit background server process
	QLocalSocket socket;
	tipc::startSlaveConnection(&socket, t32bitsrv::srvName(), -1, t32bitsrv::srvCmdline());

	tipc::Stream stream(&socket);
	tipc::Message msg;

	stream << (msg << QString("$FNTloadFontNames"));
	if (tipc::readMessage(stream, msg) != "ok")
		throw TFontLibraryLoadingError();
}

//--------------------------------------------------------------

//!\note Throws TFontCreationError if the font could not be created, and
//!leaves the old font as current.
void TFontManager::setFamily(const std::wstring family)
{
	QLocalSocket socket;
	tipc::startSlaveConnection(&socket, t32bitsrv::srvName(), -1, t32bitsrv::srvCmdline());

	tipc::Stream stream(&socket);
	tipc::Message msg;

	stream << (msg << QString("$FNTsetFamily") << family);
	if (tipc::readMessage(stream, msg) != "ok")
		throw TFontCreationError();
}

//--------------------------------------------------------------

//!\note Throws TFontCreationError if the font could not be created, and
//!leaves the old font as current.
void TFontManager::setTypeface(const std::wstring typeface)
{
	QLocalSocket socket;
	tipc::startSlaveConnection(&socket, t32bitsrv::srvName(), -1, t32bitsrv::srvCmdline());

	tipc::Stream stream(&socket);
	tipc::Message msg;

	stream << (msg << QString("$FNTsetTypeface") << typeface);
	if (tipc::readMessage(stream, msg) != "ok")
		throw TFontCreationError();

	//Also, store the font's ascender and descender
	msg >> m_pimpl->m_ascender >> m_pimpl->m_descender;
}

//--------------------------------------------------------------

std::wstring TFontManager::getCurrentFamily() const
{
	QLocalSocket socket;
	tipc::startSlaveConnection(&socket, t32bitsrv::srvName(), -1, t32bitsrv::srvCmdline());

	tipc::Stream stream(&socket);
	tipc::Message msg;

	stream << (msg << QString("$FNTgetCurrentFamily"));
	if (tipc::readMessage(stream, msg) != "ok")
		throw TException("Could not get font family");

	std::wstring family;
	msg >> family;
	return family;
}

//--------------------------------------------------------------

std::wstring TFontManager::getCurrentTypeface() const
{
	QLocalSocket socket;
	tipc::startSlaveConnection(&socket, t32bitsrv::srvName(), -1, t32bitsrv::srvCmdline());

	tipc::Stream stream(&socket);
	tipc::Message msg;

	stream << (msg << QString("$FNTgetCurrentTypeface"));
	if (tipc::readMessage(stream, msg) != "ok")
		throw TException("Could not get font typeface");

	std::wstring typeface;
	msg >> typeface;
	return typeface;
}

//--------------------------------------------------------------

void TFontManager::getAllFamilies(std::vector<std::wstring> &families) const
{
	QLocalSocket socket;
	tipc::startSlaveConnection(&socket, t32bitsrv::srvName(), -1, t32bitsrv::srvCmdline());

	tipc::Stream stream(&socket);
	tipc::Message msg;

	stream << (msg << QString("$FNTgetAllFamilies"));
	if (tipc::readMessage(stream, msg) != "ok")
		throw TException("Could not get font families");

	msg >> families;
}

//--------------------------------------------------------------

void TFontManager::getAllTypefaces(std::vector<std::wstring> &typefaces) const
{
	QLocalSocket socket;
	tipc::startSlaveConnection(&socket, t32bitsrv::srvName(), -1, t32bitsrv::srvCmdline());

	tipc::Stream stream(&socket);
	tipc::Message msg;

	stream << (msg << QString("$FNTgetAllTypefaces"));
	if (tipc::readMessage(stream, msg) != "ok")
		throw TException("Could not get font typefaces");

	msg >> typefaces;
}

//--------------------------------------------------------------

void TFontManager::setVertical(bool vertical)
{
}

//--------------------------------------------------------------

void TFontManager::setSize(int size)
{
	QLocalSocket socket;
	tipc::startSlaveConnection(&socket, t32bitsrv::srvName(), -1, t32bitsrv::srvCmdline());

	tipc::Stream stream(&socket);
	tipc::Message msg;

	stream << (msg << QString("$FNTsetSize") << size);
	if (tipc::readMessage(stream, msg) != "ok")
		throw TException("Unexpected error");

	//Also, update ascender and descender
	msg >> m_pimpl->m_ascender >> m_pimpl->m_descender;
}

//--------------------------------------------------------------

TPoint TFontManager::getDistance(wchar_t firstChar, wchar_t secondChar)
{
	QLocalSocket socket;
	tipc::startSlaveConnection(&socket, t32bitsrv::srvName(), -1, t32bitsrv::srvCmdline());

	tipc::Stream stream(&socket);
	tipc::Message msg;

	stream << (msg << QString("$FNTgetDistance") << firstChar << secondChar);
	if (tipc::readMessage(stream, msg) != "ok")
		throw TException("Unexpected error");

	TPoint d;
	msg >> d.x >> d.y;
	return d;
}

//--------------------------------------------------------------

int TFontManager::getMaxHeight()
{
	return m_pimpl->m_ascender - m_pimpl->m_descender;
}

//--------------------------------------------------------------

int TFontManager::getMaxWidth()
{
	assert(!"not implemented yet");
	return 100;
}

//--------------------------------------------------------------

bool TFontManager::hasVertical()
{
	return false;
}

//--------------------------------------------------------------

bool TFontManager::hasKerning()
{
	return true;
}

//--------------------------------------------------------------

int TFontManager::getLineAscender()
{
	return m_pimpl->m_ascender;
}

//--------------------------------------------------------------

int TFontManager::getLineDescender()
{
	return m_pimpl->m_descender;
}

//--------------------------------------------------------------

TPoint TFontManager::drawChar(TVectorImageP &outImage, wchar_t charcode, wchar_t nextCode)
{
	QLocalSocket socket;
	tipc::startSlaveConnection(&socket, t32bitsrv::srvName(), -1, t32bitsrv::srvCmdline());

	tipc::Stream stream(&socket);
	tipc::Message msg;

	stream << (msg << QString("$FNTdrawCharVI") << charcode << nextCode);
	if (tipc::readMessage(stream, msg) != "ok")
		throw TException("Unexpected error");

	TPoint ret;
	msg >> ret.x >> ret.y;
	::readVImage(outImage, msg);

	return ret;
}

//--------------------------------------------------------------

TPoint TFontManager::drawChar(TRasterGR8P &outImage, TPoint &glyphOrigin, wchar_t charcode, wchar_t nextCode)
{
	QLocalSocket socket;
	tipc::startSlaveConnection(&socket, t32bitsrv::srvName(), -1, t32bitsrv::srvCmdline());

	tipc::Stream stream(&socket);
	tipc::Message msg;

	QString shMemId(tipc::uniqueId()), res;
	{
		//Invoke the appropriate command
		stream << (msg << QString("$FNTdrawCharGR")
					   << shMemId << charcode << nextCode);
		if (tipc::readMessage(stream, msg) != "ok")
			throw TException("Unexpected error");
	}

	TDimension dim(0, 0);
	msg >> dim.lx >> dim.ly;
	TPoint ret;
	msg >> ret.x >> ret.y;

	//Create outImage
	outImage = TRasterGR8P(dim.lx, dim.ly);

	QSharedMemory shmem(shMemId);
	shmem.attach();
	shmem.lock();

	//Copy the returned image to outImage
	TRasterGR8P ras(dim.lx, dim.ly, dim.lx, (TPixelGR8 *)shmem.data());
	TRop::copy(outImage, ras);

	shmem.unlock();
	shmem.detach();

	//Release the shared segment
	stream << (msg << tipc::clr << QString("$shmem_release") << shMemId);
	if (tipc::readMessage(stream, msg) != "ok")
		throw TException("Unexpected error");

	return ret;
}

//--------------------------------------------------------------

TPoint TFontManager::drawChar(TRasterCM32P &outImage, TPoint &glyphOrigin,
							  int inkId, wchar_t charcode, wchar_t nextCode)
{
	QLocalSocket socket;
	tipc::startSlaveConnection(&socket, t32bitsrv::srvName(), -1, t32bitsrv::srvCmdline());

	tipc::Stream stream(&socket);
	tipc::Message msg;

	QString shMemId(tipc::uniqueId()), res;
	{
		//Invoke the appropriate command
		stream << (msg << QString("$FNTdrawCharCM")
					   << inkId << shMemId << charcode << nextCode);
		if (tipc::readMessage(stream, msg) != "ok")
			throw TException("Unexpected error");
	}

	TDimension dim(0, 0);
	msg >> dim.lx >> dim.ly;
	TPoint ret;
	msg >> ret.x >> ret.y;

	//Create outImage
	outImage = TRasterCM32P(dim.lx, dim.ly);

	QSharedMemory shmem(shMemId);
	shmem.attach();
	shmem.lock();

	//Copy the returned image to outImage
	TRasterCM32P ras(dim.lx, dim.ly, dim.lx, (TPixelCM32 *)shmem.data());
	TRop::copy(outImage, ras);

	shmem.unlock();
	shmem.detach();

	//Release the shared segment
	stream << (msg << tipc::clr << QString("$shmem_release") << shMemId);
	if (tipc::readMessage(stream, msg) != "ok")
		throw TException("Unexpected error");

	return ret;
}

#endif
