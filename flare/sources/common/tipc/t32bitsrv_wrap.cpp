

#include <QDebug>

#include "t32bitsrv_wrap.h"

//================================================================================

int t32bitsrv::BufferExchanger::read(const char *srcBuf, int len) {
  m_data = (UCHAR *)memcpy(m_data, srcBuf, len);
  return len;
}

//--------------------------------------------------------------------------------

int t32bitsrv::BufferExchanger::write(char *dstBuf, int len) {
  memcpy(dstBuf, m_data, len);
  m_data += len;

  return len;
}

//================================================================================

template <typename PIXEL>
int t32bitsrv::RasterExchanger<PIXEL>::read(const char *srcBuf, int len) {
  if (m_ras->getWrap() == m_ras->getLx()) {
    memcpy(m_pix, srcBuf, len);
    m_pix = (PIXEL *)((UCHAR *)m_pix + len);
  } else {
    int xStart         = (m_pix - m_ras->pixels(0)) % m_ras->getWrap();
    int remainingData  = len;
    int lineData       = m_ras->getLx() * sizeof(PIXEL);
    int lineDataToRead = std::min(
        (int)((m_ras->getLx() - xStart) * sizeof(PIXEL)), remainingData);

    for (; remainingData > 0;
         m_pix += (m_ras->getWrap() - xStart), remainingData -= lineDataToRead,
         lineDataToRead = std::min(lineData, remainingData), xStart = 0)
      memcpy(m_pix, srcBuf, lineDataToRead);
  }

  return len;
}

//--------------------------------------------------------------------------------

template <typename PIXEL>
int t32bitsrv::RasterExchanger<PIXEL>::write(char *dstBuf, int len) {
  // We pass entire pixels, not just bytes
  len = len - (len % sizeof(PIXEL));

  if (m_ras->getWrap() == m_ras->getLx()) {
    memcpy(dstBuf, m_pix, len);
    m_pix = (PIXEL *)((UCHAR *)m_pix + len);
  } else {
    int xStart          = (m_pix - m_ras->pixels(0)) % m_ras->getWrap();
    int remainingData   = len;
    int lineData        = m_ras->getLx() * sizeof(PIXEL);
    int lineDataToWrite = std::min(
        (int)((m_ras->getLx() - xStart) * sizeof(PIXEL)), remainingData);

    for (; remainingData > 0;
         m_pix += (m_ras->getWrap() - xStart), remainingData -= lineDataToWrite,
         lineDataToWrite = std::min(lineData, remainingData), xStart = 0)
      memcpy(dstBuf, m_pix, lineDataToWrite);
  }

  return len;
}

//--------------------------------------------------------------------------------

//  Explicit specialization of raster exchangers
template class DVAPI t32bitsrv::RasterExchanger<TPixel32>;

//================================================================================
