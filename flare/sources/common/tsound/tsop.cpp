

#include <cstring>

#include "tsop.h"
#include "tsound_t.h"

// TRop::ResampleFilterType Tau_resample_filter = Hamming3;

//---------------------------------------------------------

typedef enum {
  FLT_NONE,

  FLT_TRIANGLE, /* triangle filter */
  FLT_MITCHELL, /* Mitchell-Netravali filter */
  FLT_CUBIC_5,  /* cubic convolution, a = .5 */
  FLT_CUBIC_75, /* cubic convolution, a = .75 */
  FLT_CUBIC_1,  /* cubic convolution, a = 1 */
  FLT_HANN2,    /* Hann window, rad = 2 */
  FLT_HANN3,    /* Hann window, rad = 3 */
  FLT_HAMMING2, /* Hamming window, rad = 2 */
  FLT_HAMMING3, /* Hamming window, rad = 3 */
  FLT_LANCZOS2, /* Lanczos window, rad = 2 */
  FLT_LANCZOS3, /* Lanczos window, rad = 3 */
  FLT_GAUSS,    /* Gaussian convolution */

  FLT_HOW_MANY
} FLT_TYPE;

//---------------------------------------------------------

typedef enum {
  RESORDER_NONE,
  RESORDER_BITS_RATE_CHANS,
  RESORDER_CHANS_RATE_BITS,
  RESORDER_RATE_BITS_CHANS,
  RESORDER_CHANS_BITS_RATE,
  RESORDER_BITS_RATE,
  RESORDER_RATE_BITS,
  RESORDER_CHANS_RATE,
  RESORDER_RATE_CHANS,
  RESORDER_RATE,
  RESORDER_SIGN,
  RESORDER_HOW_MANY
} RESORDER_TYPE;

//---------------------------------------------------------

typedef struct {
  int src_offset; /* of weight[0] relative to start of period */
  int n_weights;
  double *weight; /* [n_weights],  -1.0 <= weight <= 1.0 */
} WEIGHTSET;

//---------------------------------------------------------

typedef struct {
  int src_period; /* after a period src and dst are in step again */
  int dst_period;
  WEIGHTSET *weightset; /* [dst_period] */
} FILTER;

//---------------------------------------------------------

#define M_PIF float(M_PI)
#define SINC0(x, a) (sin((M_PI / (a)) * (x)) / ((M_PI / (a)) * (x)))
#define SINC0F(x, a) (sinf((M_PIF / (a)) * (x)) / ((M_PIF / (a)) * (x)))
#define SINC(x, a) ((x) == 0.0 ? 1.0 : SINC0(x, a))
#define SINCF(x, a) ((x) == 0.0F ? 1.0F : SINC0F(x, a))

#define FULL_INT_MUL_DIV(X, Y, Z) ((int)(((double)(X) * (Y) + ((Z)-1)) / (Z)))

// prototipi
void convert(TSoundTrackP &dst, const TSoundTrackP &src);

namespace {

//---------------------------------------------------------

void simplifyRatio(int *p_a, int *p_b) {
  int a = *p_a, b = *p_b;

  while (a != b)
    if (a > b)
      a -= b;
    else
      b -= a;
  if (a != 1) {
    *p_a /= a;
    *p_b /= a;
  }
}

//---------------------------------------------------------

int getFilterRadius(FLT_TYPE flt_type) {
  int result = 0;
  switch (flt_type) {
  case FLT_TRIANGLE:
    result = 1;
    break;
  case FLT_MITCHELL:
    result = 2;
    break;
  case FLT_CUBIC_5:
    result = 2;
    break;
  case FLT_CUBIC_75:
    result = 2;
    break;
  case FLT_CUBIC_1:
    result = 2;
    break;
  case FLT_HANN2:
    result = 2;
    break;
  case FLT_HANN3:
    result = 3;
    break;
  case FLT_HAMMING2:
    result = 2;
    break;
  case FLT_HAMMING3:
    result = 3;
    break;
  case FLT_LANCZOS2:
    result = 2;
    break;
  case FLT_LANCZOS3:
    result = 3;
    break;
  case FLT_GAUSS:
    result = 2;
    break;
  default:
    assert(!"bad filter type");
    break;
  }
  return result;
}

//---------------------------------------------------------

double filterValue(FLT_TYPE flt_type, double x) {
  if (!x) return 1.0;
  double result;
  switch (flt_type) {
  case FLT_TRIANGLE:
    if (x < -1.0)
      result = 0.0;
    else if (x < 0.0)
      result = 1.0 + x;
    else if (x < 1.0)
      result = 1.0 - x;
    else
      result = 0.0;
    break;

  case FLT_MITCHELL: {
    static double p0, p2, p3, q0, q1, q2, q3;

    if (!p0) {
      const double b = 1.0 / 3.0;
      const double c = 1.0 / 3.0;

      p0 = (6.0 - 2.0 * b) / 6.0;
      p2 = (-18.0 + 12.0 * b + 6.0 * c) / 6.0;
      p3 = (12.0 - 9.0 * b - 6.0 * c) / 6.0;
      q0 = (8.0 * b + 24.0 * c) / 6.0;
      q1 = (-12.0 * b - 48.0 * c) / 6.0;
      q2 = (6.0 * b + 30.0 * c) / 6.0;
      q3 = (-b - 6.0 * c) / 6.0;
    }
    if (x < -2.0)
      result = 0.0;
    else if (x < -1.0)
      result = (q0 - x * (q1 - x * (q2 - x * q3)));
    else if (x < 0.0)
      result = (p0 + x * x * (p2 - x * p3));
    else if (x < 1.0)
      result = (p0 + x * x * (p2 + x * p3));
    else if (x < 2.0)
      result = (q0 + x * (q1 + x * (q2 + x * q3)));

    break;
  }

  case FLT_CUBIC_5:
    if (x < 0.0)
      x = -x;
    else if (x < 1.0)
      result = 2.5 * x * x * x - 3.5 * x * x + 1;
    else if (x < 2.0)
      result = 0.5 * x * x * x - 2.5 * x * x + 4 * x - 2;
    break;

  case FLT_CUBIC_75:
    if (x < 0.0)
      x = -x;
    else if (x < 1.0)
      result = 2.75 * x * x * x - 3.75 * x * x + 1;
    else if (x < 2.0)
      result = 0.75 * x * x * x - 3.75 * x * x + 6 * x - 3;
    break;

  case FLT_CUBIC_1:
    if (x < 0.0)
      x = -x;
    else if (x < 1.0)
      result = 3 * x * x * x - 4 * x * x + 1;
    else if (x < 2.0)
      result = x * x * x - 5 * x * x + 8 * x - 4;
    break;

  case FLT_HANN2:
    if (x <= -2.0)
      result = 0.0;
    else if (x < 2.0)
      result = SINC0(x, 1) * (0.5 + 0.5 * cos(M_PI_2 * x));
    break;

  case FLT_HANN3:
    if (x <= -3.0)
      result = 0.0;
    else if (x < 3.0)
      result = SINC0(x, 1) * (0.5 + 0.5 * cos((M_PI / 3) * x));
    break;

  case FLT_HAMMING2:
    if (x <= -2.0)
      result = 0.0;
    else if (x < 2.0)
      result = SINC0(x, 1) * (0.54 + 0.46 * cos(M_PI_2 * x));
    break;

  case FLT_HAMMING3:
    if (x <= -3.0)
      result = 0.0;
    else if (x < 3.0)
      result = SINC0(x, 1) * (0.54 + 0.46 * cos((M_PI / 3) * x));
    break;

  case FLT_LANCZOS2:
    if (x <= -2.0)
      result = 0.0;
    else if (x < 2.0)
      result = SINC0(x, 1) * SINC0(x, 2);
    break;

  case FLT_LANCZOS3:
    if (x <= -3.0)
      result = 0.0;
    else if (x < 3.0)
      result = SINC0(x, 1) * SINC0(x, 3);
    break;

  case FLT_GAUSS:
    if (x <= -2.0)
      result = 0.0;
    else if (x < 2.0)
      result = exp((-M_PI) * x * x);
    /* exp(-M_PI*2*2)~=3.5*10^-6 */
    break;

  default:
    assert(!"bad filter type");
  }
  return result;
}

//---------------------------------------------------------

}  // namespace

//==============================================================================

template <class T1, class T2>
void convertSamplesT(TSoundTrackT<T1> &dst, const TSoundTrackT<T2> &src) {
  const T2 *srcSample = src.samples();
  T1 *dstSample       = dst.samples();

  const T2 *srcEndSample =
      srcSample + std::min(src.getSampleCount(), dst.getSampleCount());
  while (srcSample < srcEndSample) {
    *dstSample = T1::from(*srcSample);
    ++dstSample;
    ++srcSample;
  }
}

//==============================================================================

template <class T>
T *resampleT(T &src, TINT32 sampleRate, FLT_TYPE flt_type) {
  typedef typename T::SampleType SampleType;
  typedef typename T::SampleType::ChannelValueType ChannelValueType;
  T *dst = new TSoundTrackT<SampleType>(
      sampleRate, src.getChannelCount(),
      (TINT32)(src.getSampleCount() *
               (sampleRate / (double)src.getSampleRate())));

  double src_rad, f, src_f0, src_to_f;
  double weight, weightsum;
  int iw, is, s0, ip, first, last;

  FILTER filter;
  filter.src_period = (int)src.getSampleRate();
  filter.dst_period = (int)dst->getSampleRate();
  simplifyRatio(&filter.src_period, &filter.dst_period);
  filter.weightset = new WEIGHTSET[filter.dst_period];
  if (!filter.weightset) return 0;

  int flt_rad       = getFilterRadius(flt_type);
  double dstRate    = (double)dst->getSampleRate();
  double srcRate    = (double)src.getSampleRate();
  double dst_to_src = srcRate / dstRate;
  if (srcRate > dstRate) {
    src_rad  = flt_rad * dst_to_src;
    src_to_f = dstRate / srcRate;
  } else {
    src_rad  = flt_rad;
    src_to_f = 1.0;
  }

  for (ip = 0; ip < filter.dst_period; ip++) {
    src_f0 = ip * dst_to_src;
    if (ip == 0 && srcRate < dstRate)
      first = last = 0;
    else {
      first = intGT(src_f0 - src_rad);
      last  = intLT(src_f0 + src_rad);
    }

    filter.weightset[ip].src_offset = first;
    filter.weightset[ip].n_weights  = last - first + 1;
    filter.weightset[ip].weight = new double[filter.weightset[ip].n_weights];

    if (!filter.weightset[ip].weight) return 0;

    weightsum = 0.0;
    for (is = first; is <= last; is++) {
      f                                       = (is - src_f0) * src_to_f;
      weight                                  = filterValue(flt_type, f);
      filter.weightset[ip].weight[is - first] = weight;
      weightsum += weight;
    }
    assert(weightsum);
    for (is = first; is <= last; is++)
      filter.weightset[ip].weight[is - first] /= weightsum;
  }

  ip = 0;
  s0 = 0;

  for (TINT32 id = 0; id < dst->getSampleCount(); id++) {
    SampleType dstSample;
    SampleType srcSample;

    is = s0 + filter.weightset[ip].src_offset;

    int iwFirst, iwLast;
    if (is > 0) {
      iwFirst = 0;
      iwLast  = std::min<int>(filter.weightset[ip].n_weights,
                             src.getSampleCount() - is);
    } else {
      iwFirst = -is;
      is      = 0;
      iwLast =
          std::min<int>(filter.weightset[ip].n_weights, src.getSampleCount());
    }

    double dstChannel[2];

    dstChannel[0] = 0;
    dstChannel[1] = 0;

    dstSample = SampleType();

    for (iw = iwFirst; iw < iwLast; iw++, is++) {
      weight = filter.weightset[ip].weight[iw];

      srcSample = *(src.samples() + is);

      /*
T::SampleType tmp = srcSample*weight;
dstSample += tmp;
*/

      for (int i = 0; i < src.getChannelCount(); i++)
        dstChannel[i] += (double)(srcSample.getValue(i) * weight);

      // assert(dstSample.getValue(0) == dstChannel[0]);
    }

    // Removed tround() since it was destroying 32-bits float data
    // for (int i = 0; i < src.getChannelCount(); i++)
    //  dstSample.setValue(i, (ChannelValueType)(tround(dstChannel[i])));

    for (int i = 0; i < src.getChannelCount(); i++)
      dstSample.setValue(i, (ChannelValueType)(dstChannel[i]));

    *(dst->samples() + id) = dstSample;

    ip++;
    if (ip == filter.dst_period) {
      ip = 0;
      s0 += filter.src_period;
    }
  }

  for (ip = 0; ip < filter.dst_period; ip++)
    delete[] filter.weightset[ip].weight;

  delete[] filter.weightset;
  return dst;
}

//=============================================================================

class TSoundTrackResample final : public TSoundTransform {
  TINT32 m_sampleRate;
  FLT_TYPE m_filterType;

public:
  TSoundTrackResample(TINT32 sampleRate, FLT_TYPE filterType)
      : TSoundTransform(), m_sampleRate(sampleRate), m_filterType(filterType) {}

  ~TSoundTrackResample(){};

  TSoundTrackP compute(const TSoundTrackMono8Signed &src) override {
    TSoundTrackMono8Signed *dst = resampleT(
        const_cast<TSoundTrackMono8Signed &>(src), m_sampleRate, m_filterType);

    return TSoundTrackP(dst);
  }

  TSoundTrackP compute(const TSoundTrackMono8Unsigned &src) override {
    TSoundTrackMono8Unsigned *dst =
        resampleT(const_cast<TSoundTrackMono8Unsigned &>(src), m_sampleRate,
                  m_filterType);

    return TSoundTrackP(dst);
  }

  TSoundTrackP compute(const TSoundTrackStereo8Signed &src) override {
    TSoundTrackStereo8Signed *dst =
        resampleT(const_cast<TSoundTrackStereo8Signed &>(src), m_sampleRate,
                  m_filterType);

    return TSoundTrackP(dst);
  }

  TSoundTrackP compute(const TSoundTrackStereo8Unsigned &src) override {
    TSoundTrackStereo8Unsigned *dst =
        resampleT(const_cast<TSoundTrackStereo8Unsigned &>(src), m_sampleRate,
                  m_filterType);

    return TSoundTrackP(dst);
  }

  TSoundTrackP compute(const TSoundTrackMono16 &src) override {
    TSoundTrackMono16 *dst = resampleT(const_cast<TSoundTrackMono16 &>(src),
                                       m_sampleRate, m_filterType);

    return TSoundTrackP(dst);
  }

  TSoundTrackP compute(const TSoundTrackStereo16 &src) override {
    TSoundTrackStereo16 *dst = resampleT(const_cast<TSoundTrackStereo16 &>(src),
                                         m_sampleRate, m_filterType);

    return TSoundTrackP(dst);
  }

  TSoundTrackP compute(const TSoundTrackMono24 &src) override {
    TSoundTrackMono24 *dst = resampleT(const_cast<TSoundTrackMono24 &>(src),
                                       m_sampleRate, m_filterType);

    return TSoundTrackP(dst);
  }

  TSoundTrackP compute(const TSoundTrackStereo24 &src) override {
    TSoundTrackStereo24 *dst = resampleT(const_cast<TSoundTrackStereo24 &>(src),
                                         m_sampleRate, m_filterType);

    return TSoundTrackP(dst);
  }

  TSoundTrackP compute(const TSoundTrackMono32Float &src) override {
    TSoundTrackMono32Float *dst = resampleT(
        const_cast<TSoundTrackMono32Float &>(src), m_sampleRate, m_filterType);

    return TSoundTrackP(dst);
  }

  TSoundTrackP compute(const TSoundTrackStereo32Float &src) override {
    TSoundTrackStereo32Float *dst =
        resampleT(const_cast<TSoundTrackStereo32Float &>(src), m_sampleRate,
                  m_filterType);

    return TSoundTrackP(dst);
  }
};

//==============================================================================

TSoundTrackP TSop::resample(TSoundTrackP src, TINT32 sampleRate) {
  TSoundTrackResample *resample =
      new TSoundTrackResample(sampleRate, FLT_HAMMING3);
  TSoundTrackP dst = src->apply(resample);
  delete resample;
  return dst;
}

//==============================================================================

template <class SRC>
TSoundTrackP doConvertWithoutResamplingT(SRC *src,
                                         const TSoundTrackFormat &dstFormat) {
  TSoundTrackP dst = TSoundTrack::create(dstFormat, src->getSampleCount());
  if (!dst) return 0;

  TSoundTrackMono8Unsigned *dstM8U =
      dynamic_cast<TSoundTrackMono8Unsigned *>(dst.getPointer());
  if (dstM8U) {
    convertSamplesT(*dstM8U, *src);
    return dstM8U;
  }

  TSoundTrackMono8Signed *dstM8S =
      dynamic_cast<TSoundTrackMono8Signed *>(dst.getPointer());
  if (dstM8S) {
    convertSamplesT(*dstM8S, *src);
    return dstM8S;
  }

  TSoundTrackStereo8Signed *dstS8S =
      dynamic_cast<TSoundTrackStereo8Signed *>(dst.getPointer());
  if (dstS8S) {
    convertSamplesT(*dstS8S, *src);
    return dstS8S;
  }

  TSoundTrackStereo8Unsigned *dstS8U =
      dynamic_cast<TSoundTrackStereo8Unsigned *>(dst.getPointer());
  if (dstS8U) {
    convertSamplesT(*dstS8U, *src);
    return dstS8U;
  }

  TSoundTrackMono16 *dstM16 =
      dynamic_cast<TSoundTrackMono16 *>(dst.getPointer());
  if (dstM16) {
    convertSamplesT(*dstM16, *src);
    return dstM16;
  }

  TSoundTrackStereo16 *dstS16 =
      dynamic_cast<TSoundTrackStereo16 *>(dst.getPointer());
  if (dstS16) {
    convertSamplesT(*dstS16, *src);
    return dstS16;
  }

  TSoundTrackMono24 *dstM24 =
      dynamic_cast<TSoundTrackMono24 *>(dst.getPointer());
  if (dstM24) {
    convertSamplesT(*dstM24, *src);
    return dstM24;
  }

  TSoundTrackStereo24 *dstS24 =
      dynamic_cast<TSoundTrackStereo24 *>(dst.getPointer());
  if (dstS24) {
    convertSamplesT(*dstS24, *src);
    return dstS24;
  }

  TSoundTrackMono32Float *dstM32F =
      dynamic_cast<TSoundTrackMono32Float *>(dst.getPointer());
  if (dstM32F) {
    convertSamplesT(*dstM32F, *src);
    return dstM32F;
  }

  TSoundTrackStereo32Float *dstS32F =
      dynamic_cast<TSoundTrackStereo32Float *>(dst.getPointer());
  if (dstS32F) {
    convertSamplesT(*dstS32F, *src);
    return dstS32F;
  }

  return 0;
}

//------------------------------------------------------------------------------

class TSoundTrackConverterWithoutResampling final : public TSoundTransform {
  TSoundTrackFormat m_format;

public:
  TSoundTrackConverterWithoutResampling(const TSoundTrackFormat &format)
      : m_format(format) {}

  ~TSoundTrackConverterWithoutResampling(){};

  TSoundTrackP compute(const TSoundTrackMono8Signed &src) override {
    return doConvertWithoutResamplingT(&src, m_format);
  }

  TSoundTrackP compute(const TSoundTrackMono8Unsigned &src) override {
    return doConvertWithoutResamplingT(&src, m_format);
  }

  TSoundTrackP compute(const TSoundTrackStereo8Signed &src) override {
    return doConvertWithoutResamplingT(&src, m_format);
  }

  TSoundTrackP compute(const TSoundTrackStereo8Unsigned &src) override {
    return doConvertWithoutResamplingT(&src, m_format);
  }

  TSoundTrackP compute(const TSoundTrackMono16 &src) override {
    return doConvertWithoutResamplingT(&src, m_format);
  }

  TSoundTrackP compute(const TSoundTrackStereo16 &src) override {
    return doConvertWithoutResamplingT(&src, m_format);
  }

  TSoundTrackP compute(const TSoundTrackMono24 &src) override {
    return doConvertWithoutResamplingT(&src, m_format);
  }

  TSoundTrackP compute(const TSoundTrackStereo24 &src) override {
    return doConvertWithoutResamplingT(&src, m_format);
  }

  TSoundTrackP compute(const TSoundTrackMono32Float &src) override {
    return doConvertWithoutResamplingT(&src, m_format);
  }

  TSoundTrackP compute(const TSoundTrackStereo32Float &src) override {
    return doConvertWithoutResamplingT(&src, m_format);
  }
};

//-----------------------------------------------------------------------------
namespace {

void convertWithoutResampling(TSoundTrackP &dst, const TSoundTrackP &src) {
  TSoundTrackConverterWithoutResampling *converter;
  converter = new TSoundTrackConverterWithoutResampling(dst->getFormat());
  dst       = src->apply(converter);
  delete converter;
}

}  // namespace

//==============================================================================

TSoundTrackP TSop::convert(const TSoundTrackP &src,
                           const TSoundTrackFormat &dstFormat) {
  TINT32 dstSampleCount =
      (TINT32)(src->getSampleCount() *
               (dstFormat.m_sampleRate / (double)src->getSampleRate()));

  TSoundTrackP dst = TSoundTrack::create(dstFormat, dstSampleCount);
  TSop::convert(dst, src);
  return dst;
}

//------------------------------------------------------------------------------

void TSop::convert(TSoundTrackP &dst, const TSoundTrackP &src) {
  int src_reslen, dst_reslen;
  int src_bits, dst_bits;
  int src_chans, dst_chans;
  TSoundTrackP tmp, tmq;
  RESORDER_TYPE order;

  assert(src->getSampleCount() >= 0 && dst->getSampleCount() >= 0);
  if (!dst->getSampleCount()) return;

  if (!src->getSampleCount()) {
    dst->blank(0L, (TINT32)(dst->getSampleCount() - 1));
    return;
  }

  if (src->getSampleRate() == dst->getSampleRate()) {
    src_reslen = dst->getSampleCount();
    notMoreThan((int)src->getSampleCount(), src_reslen);
    dst_reslen = src_reslen;
    convertWithoutResampling(dst, src);
  } else {
    src_reslen = FULL_INT_MUL_DIV(dst->getSampleCount(), src->getSampleRate(),
                                  dst->getSampleRate());

    if (src_reslen > src->getSampleCount()) {
      src_reslen = src->getSampleCount();
      dst_reslen = FULL_INT_MUL_DIV(src_reslen, dst->getSampleRate(),
                                    src->getSampleRate());
    } else
      dst_reslen = dst->getSampleCount();

    src_chans = src->getChannelCount();
    dst_chans = dst->getChannelCount();
    src_bits  = src->getBitPerSample();
    dst_bits  = dst->getBitPerSample();

    if (src_chans == dst_chans && src_bits == dst_bits)
      if (src->isSampleSigned() == dst->isSampleSigned())
        order = RESORDER_RATE;
      else
        order = RESORDER_SIGN;
    else if (src_chans < dst_chans) {
      if (src_bits < dst_bits)
        order = RESORDER_BITS_RATE_CHANS;
      else
        order = RESORDER_RATE_BITS_CHANS;
    } else if (src_chans > dst_chans) {
      if (src_bits > dst_bits)
        order = RESORDER_CHANS_RATE_BITS;
      else
        order = RESORDER_CHANS_BITS_RATE;
    } else {
      if (src_bits > dst_bits)
        order = RESORDER_RATE_BITS;
      else
        order = RESORDER_BITS_RATE;
    }

    switch (order) {
    case RESORDER_CHANS_RATE_BITS:
    case RESORDER_BITS_RATE_CHANS:
      int chans, bitPerSample;
      if (src->getChannelCount() <= dst->getChannelCount())
        chans = src->getChannelCount();
      else
        chans = dst->getChannelCount();

      if (src->getBitPerSample() >= dst->getBitPerSample())
        bitPerSample = src->getBitPerSample();
      else
        bitPerSample = dst->getBitPerSample();

      tmp = TSoundTrack::create((int)src->getSampleRate(), bitPerSample, chans,
                                src_reslen * src->getSampleSize(),
                                src->getSampleType());

      convertWithoutResampling(tmp, src);
      tmq = TSop::resample(tmp, (TINT32)dst->getSampleRate());
      convertWithoutResampling(dst, tmq);
      break;

    case RESORDER_RATE_BITS_CHANS:
    case RESORDER_RATE_BITS:
    case RESORDER_RATE_CHANS:
      tmp = TSop::resample(src, (TINT32)dst->getSampleRate());
      convertWithoutResampling(dst, tmp);
      break;

    case RESORDER_CHANS_BITS_RATE:
    case RESORDER_BITS_RATE:
    case RESORDER_CHANS_RATE:
    case RESORDER_SIGN:
      tmp = TSoundTrack::create((int)src->getSampleRate(),
                                dst->getBitPerSample(), dst->getChannelCount(),
                                src_reslen * dst->getSampleSize(),
                                dst->getSampleType());

      convertWithoutResampling(tmp, src);
      dst = TSop::resample(tmp, (TINT32)dst->getSampleRate());
      break;

    case RESORDER_RATE:
      dst = TSop::resample(src, (TINT32)dst->getSampleRate());
      break;

    default:
      assert(false);
      break;
    }
  }

  if (dst_reslen < dst->getSampleCount())
    dst->blank((TINT32)dst_reslen, (TINT32)(dst->getSampleCount() - 1));
}

//==============================================================================

template <class T>
TSoundTrackP doReverb(TSoundTrackT<T> *src, double delayTime,
                      double decayFactor, double extendTime) {
  TINT32 dstSampleCount =
      src->getSampleCount() + (TINT32)(src->getSampleRate() * extendTime);

  TSoundTrackT<T> *dst = new TSoundTrackT<T>(
      src->getSampleRate(), src->getChannelCount(), dstSampleCount);

  TINT32 sampleRate = (TINT32)src->getSampleRate();
  TINT32 k          = (TINT32)(sampleRate * delayTime);

  T *srcSample    = src->samples();
  T *dstSample    = dst->samples();
  T *endDstSample = dst->samples() + k;

  while (dstSample < endDstSample) *dstSample++ = *srcSample++;

  // la formula del reverb e'
  // out(i) = in(i) + decayFactor * out(i - k)

  //  int channelCount = src->getChannelCount();

  endDstSample =
      dst->samples() + std::min(dstSampleCount, (TINT32)src->getSampleCount());
  while (dstSample < endDstSample) {
    //*dstSample = *srcSample + *(dstSample - k)*decayFactor;
    *dstSample = T::mix(*srcSample, 1, *(dstSample - k), decayFactor);
    ++dstSample;
    ++srcSample;
  }

  endDstSample = dst->samples() + dstSampleCount;
  while (dstSample < endDstSample) {
    //*dstSample = *(dstSample - k)*decayFactor;
    *dstSample = T::mix(T(), 0, *(dstSample - k), decayFactor);
    ++dstSample;
  }

  return TSoundTrackP(dst);
}

//==============================================================================

class TSoundReverb final : public TSoundTransform {
  double m_delayTime;
  double m_decayFactor;
  double m_extendTime;

public:
  TSoundReverb(double delayTime, double decayFactor, double extendTime)
      : m_delayTime(delayTime)
      , m_decayFactor(decayFactor)
      , m_extendTime(extendTime) {}

  ~TSoundReverb() {}

  TSoundTrackP compute(const TSoundTrackMono8Signed &src) override {
    return doReverb(const_cast<TSoundTrackMono8Signed *>(&src), m_delayTime,
                    m_decayFactor, m_extendTime);
  }

  TSoundTrackP compute(const TSoundTrackMono8Unsigned &src) override {
    return doReverb(const_cast<TSoundTrackMono8Unsigned *>(&src), m_delayTime,
                    m_decayFactor, m_extendTime);
  }

  TSoundTrackP compute(const TSoundTrackStereo8Signed &src) override {
    return doReverb(const_cast<TSoundTrackStereo8Signed *>(&src), m_delayTime,
                    m_decayFactor, m_extendTime);
  }

  TSoundTrackP compute(const TSoundTrackStereo8Unsigned &src) override {
    return doReverb(const_cast<TSoundTrackStereo8Unsigned *>(&src), m_delayTime,
                    m_decayFactor, m_extendTime);
  }

  TSoundTrackP compute(const TSoundTrackMono16 &src) override {
    return doReverb(const_cast<TSoundTrackMono16 *>(&src), m_delayTime,
                    m_decayFactor, m_extendTime);
  }

  TSoundTrackP compute(const TSoundTrackStereo16 &src) override {
    return doReverb(const_cast<TSoundTrackStereo16 *>(&src), m_delayTime,
                    m_decayFactor, m_extendTime);
  }

  TSoundTrackP compute(const TSoundTrackMono24 &src) override {
    return doReverb(const_cast<TSoundTrackMono24 *>(&src), m_delayTime,
                    m_decayFactor, m_extendTime);
  }

  TSoundTrackP compute(const TSoundTrackStereo24 &src) override {
    return doReverb(const_cast<TSoundTrackStereo24 *>(&src), m_delayTime,
                    m_decayFactor, m_extendTime);
  }

  TSoundTrackP compute(const TSoundTrackMono32Float &src) override {
    return doReverb(const_cast<TSoundTrackMono32Float *>(&src), m_delayTime,
                    m_decayFactor, m_extendTime);
  }

  TSoundTrackP compute(const TSoundTrackStereo32Float &src) override {
    return doReverb(const_cast<TSoundTrackStereo32Float *>(&src), m_delayTime,
                    m_decayFactor, m_extendTime);
  }
};

//==============================================================================

TSoundTrackP TSop::reverb(TSoundTrackP src, double delayTime,
                          double decayFactor, double extendTime) {
  TSoundReverb *reverb = new TSoundReverb(delayTime, decayFactor, extendTime);
  assert(reverb);
  if (!reverb) return TSoundTrackP();
  TSoundTrackP dst = src->apply(reverb);
  delete reverb;
  return dst;
}

//==============================================================================

template <class T>
TSoundTrackP doGate(TSoundTrackT<T> *src, double threshold, double holdTime,
                    double /*releaseTime*/) {
  TSoundTrackT<T> *dst = new TSoundTrackT<T>(
      src->getSampleRate(), src->getChannelCount(), src->getSampleCount());

  double sampleExcursion_inv =
      1.0 / (double)(src->getMaxPressure(0, src->getSampleCount() - 1, 0) -
                     src->getMinPressure(0, src->getSampleCount() - 1, 0));

  TINT32 holdTimeSamples = src->secondsToSamples(holdTime);
  TINT32 time            = 0;

  const T *srcSample    = src->samples();
  const T *srcEndSample = srcSample + src->getSampleCount();
  T *dstSample          = dst->samples();

  while (srcSample < srcEndSample) {
    if (fabs(srcSample->getValue(0) * sampleExcursion_inv) < threshold) {
      if (time >= holdTimeSamples)
        *dstSample = T();
      else
        *dstSample = *srcSample;

      ++time;
    } else {
      time       = 0;
      *dstSample = *srcSample;
    }

    ++srcSample;
    ++dstSample;
  }

  return dst;
}

//==============================================================================

class TSoundGate final : public TSoundTransform {
  double m_threshold;
  double m_holdTime;
  double m_releaseTime;

public:
  TSoundGate(double threshold, double holdTime, double releaseTime)
      : m_threshold(threshold)
      , m_holdTime(holdTime)
      , m_releaseTime(releaseTime) {}

  ~TSoundGate() {}

  TSoundTrackP compute(const TSoundTrackMono8Signed &src) override {
    return doGate(const_cast<TSoundTrackMono8Signed *>(&src), m_threshold,
                  m_holdTime, m_releaseTime);
  }

  TSoundTrackP compute(const TSoundTrackMono8Unsigned &src) override {
    return doGate(const_cast<TSoundTrackMono8Unsigned *>(&src), m_threshold,
                  m_holdTime, m_releaseTime);
  }

  TSoundTrackP compute(const TSoundTrackStereo8Signed &src) override {
    return doGate(const_cast<TSoundTrackStereo8Signed *>(&src), m_threshold,
                  m_holdTime, m_releaseTime);
  }

  TSoundTrackP compute(const TSoundTrackStereo8Unsigned &src) override {
    return doGate(const_cast<TSoundTrackStereo8Unsigned *>(&src), m_threshold,
                  m_holdTime, m_releaseTime);
  }

  TSoundTrackP compute(const TSoundTrackMono16 &src) override {
    return doGate(const_cast<TSoundTrackMono16 *>(&src), m_threshold,
                  m_holdTime, m_releaseTime);
  }

  TSoundTrackP compute(const TSoundTrackStereo16 &src) override {
    return doGate(const_cast<TSoundTrackStereo16 *>(&src), m_threshold,
                  m_holdTime, m_releaseTime);
  }

  TSoundTrackP compute(const TSoundTrackMono24 &src) override {
    return doGate(const_cast<TSoundTrackMono24 *>(&src), m_threshold,
                  m_holdTime, m_releaseTime);
  }

  TSoundTrackP compute(const TSoundTrackStereo24 &src) override {
    return doGate(const_cast<TSoundTrackStereo24 *>(&src), m_threshold,
                  m_holdTime, m_releaseTime);
  }

  TSoundTrackP compute(const TSoundTrackMono32Float &src) override {
    return doGate(const_cast<TSoundTrackMono32Float *>(&src), m_threshold,
                  m_holdTime, m_releaseTime);
  }

  TSoundTrackP compute(const TSoundTrackStereo32Float &src) override {
    return doGate(const_cast<TSoundTrackStereo32Float *>(&src), m_threshold,
                  m_holdTime, m_releaseTime);
  }
};

//==============================================================================

TSoundTrackP TSop::gate(TSoundTrackP src, double threshold, double holdTime,
                        double releaseTime) {
  TSoundGate *gate = new TSoundGate(threshold, holdTime, releaseTime);
  assert(gate);
  if (!gate) return TSoundTrackP();
  TSoundTrackP dst = src->apply(gate);
  delete gate;
  return dst;
}

//==============================================================================

TSoundTrackP TSop::timeStretch(TSoundTrackP src, double ratio) {
  TINT32 sampleRate = (TINT32)(src->getSampleRate() * ratio);

  if (sampleRate > 100000) sampleRate = 100000;

  TSoundTrackP st;

  if (sampleRate > 0) {
    TSoundTrackResample *resample =
        new TSoundTrackResample(sampleRate, FLT_TRIANGLE);
    st = src->apply(resample);
    delete resample;
    st->setSampleRate(src->getSampleRate());
  }

  return st;
}

//========================================================================================

template <class T>
TSoundTrackP doEcho(TSoundTrackT<T> *src, double delayTime, double decayFactor,
                    double extendTime) {
  typedef typename T::ChannelValueType ChannelValueType;

  TINT32 dstSampleCount =
      src->getSampleCount() + (TINT32)(src->getSampleRate() * extendTime);

  TSoundTrackT<T> *dst = new TSoundTrackT<T>(
      src->getSampleRate(), src->getChannelCount(), dstSampleCount);

  TINT32 sampleRate = (TINT32)src->getSampleRate();
  TINT32 k          = (TINT32)(sampleRate * delayTime);

  T *srcSample    = src->samples();
  T *dstSample    = dst->samples();
  T *endDstSample = dst->samples() + k;

  while (dstSample < endDstSample) *dstSample++ = *srcSample++;

  // la formula dell'echo e'
  // out(i) = in(i) + decayFactor * in(i - k)

  bool chans = src->getChannelCount() == 2;
  endDstSample =
      dst->samples() + std::min(dstSampleCount, (TINT32)src->getSampleCount());
  while (dstSample < endDstSample) {
    //*dstSample = *srcSample + *(srcSample - k)*decayFactor;
    ChannelValueType val = (ChannelValueType)(
        (srcSample - k)->getValue(TSound::MONO) * decayFactor);
    dstSample->setValue(TSound::MONO, srcSample->getValue(TSound::MONO) + val);
    if (chans) {
      ChannelValueType val = (ChannelValueType)(
          (srcSample - k)->getValue(TSound::RIGHT) * decayFactor);
      dstSample->setValue(TSound::RIGHT,
                          srcSample->getValue(TSound::RIGHT) + val);
    }
    ++dstSample;
    ++srcSample;
  }

  endDstSample = dstSample + k;
  while (dstSample < endDstSample) {
    //*dstSample = *(srcSample - k)*decayFactor;
    ChannelValueType val = (ChannelValueType)(
        (srcSample - k)->getValue(TSound::MONO) * decayFactor);
    dstSample->setValue(TSound::MONO, val);
    if (chans) {
      ChannelValueType val = (ChannelValueType)(
          (srcSample - k)->getValue(TSound::RIGHT) * decayFactor);
      dstSample->setValue(TSound::RIGHT, val);
    }
    ++dstSample;
    ++srcSample;
  }

  srcSample    = src->samples() + src->getSampleCount() - 1;
  endDstSample = dst->samples() + dstSampleCount;
  while (dstSample < endDstSample) {
    //*dstSample = *(srcSample)*decayFactor;
    ChannelValueType val =
        (ChannelValueType)(srcSample->getValue(TSound::MONO) * decayFactor);
    dstSample->setValue(TSound::MONO, val);
    if (chans) {
      ChannelValueType val =
          (ChannelValueType)(srcSample->getValue(TSound::RIGHT) * decayFactor);
      dstSample->setValue(TSound::RIGHT, val);
    }
    ++dstSample;
  }

  return TSoundTrackP(dst);
}

//------------------------------------------------------------------------------

void TSop::echo(TSoundTrackP &dst, const TSoundTrackP &src, double delayTime,
                double decayFactor, double extendTime) {
  TSoundTrackMono8Signed *srcM8S;
  srcM8S = dynamic_cast<TSoundTrackMono8Signed *>(src.getPointer());
  if (srcM8S)
    dst = doEcho(srcM8S, delayTime, decayFactor, extendTime);
  else {
    TSoundTrackMono8Unsigned *srcM8U;
    srcM8U = dynamic_cast<TSoundTrackMono8Unsigned *>(src.getPointer());
    if (srcM8U)
      dst = doEcho(srcM8U, delayTime, decayFactor, extendTime);
    else {
      TSoundTrackStereo8Signed *srcS8S;
      srcS8S = dynamic_cast<TSoundTrackStereo8Signed *>(src.getPointer());
      if (srcS8S)
        dst = doEcho(srcS8S, delayTime, decayFactor, extendTime);
      else {
        TSoundTrackStereo8Unsigned *srcS8U;
        srcS8U = dynamic_cast<TSoundTrackStereo8Unsigned *>(src.getPointer());
        if (srcS8U)
          dst = doEcho(srcS8U, delayTime, decayFactor, extendTime);
        else {
          TSoundTrackMono16 *srcM16;
          srcM16 = dynamic_cast<TSoundTrackMono16 *>(src.getPointer());
          if (srcM16)
            dst = doEcho(srcM16, delayTime, decayFactor, extendTime);
          else {
            TSoundTrackStereo16 *srcS16;
            srcS16 = dynamic_cast<TSoundTrackStereo16 *>(src.getPointer());
            if (srcS16)
              dst = doEcho(srcS16, delayTime, decayFactor, extendTime);
            else {
              TSoundTrackMono24 *srcM24;
              srcM24 = dynamic_cast<TSoundTrackMono24 *>(src.getPointer());
              if (srcM24)
                dst = doEcho(srcM24, delayTime, decayFactor, extendTime);
              else {
                TSoundTrackStereo24 *srcS24;
                srcS24 = dynamic_cast<TSoundTrackStereo24 *>(src.getPointer());
                if (srcS24)
                  dst = doEcho(srcS24, delayTime, decayFactor, extendTime);
                else {
                  TSoundTrackMono32Float *srcM32F;
                  srcM32F =
                      dynamic_cast<TSoundTrackMono32Float *>(src.getPointer());
                  if (srcM32F)
                    dst = doEcho(srcM32F, delayTime, decayFactor, extendTime);
                  else {
                    TSoundTrackStereo32Float *srcS32F;
                    srcS32F = dynamic_cast<TSoundTrackStereo32Float *>(
                        src.getPointer());
                    if (srcS32F)
                      dst = doEcho(srcS32F, delayTime, decayFactor, extendTime);
                    // Yo Dawg, I herd you like nesting ifs, so I put an nest if
                    // in your nesting ifs so you can nest if while you nesting
                    // ifs
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}

//------------------------------------------------------------------------------

TSoundTrackP TSop::insertBlank(TSoundTrackP src, TINT32 s0, TINT32 len) {
  assert(len >= 0);
  if (len == 0) return src;

  TINT32 ss0 = tcrop<TINT32>(s0, 0, src->getSampleCount());

  TSoundTrackFormat format = src->getFormat();
  TSoundTrackP dst = TSoundTrack::create(format, (src->getSampleCount() + len));

  UCHAR *dstRawData = (UCHAR *)dst->getRawData();
  UCHAR *srcRawData = (UCHAR *)src->getRawData();

  int bytePerSample = dst->getSampleSize();
  memcpy(dstRawData, srcRawData, ss0 * bytePerSample);
  if (format.m_sampleType != TSound::UINT)
    memset(dstRawData + ss0 * bytePerSample, 0, len * bytePerSample);
  else
    memset(dstRawData + ss0 * bytePerSample, 127, len * bytePerSample);
  memcpy(dstRawData + (ss0 + len) * bytePerSample,
         srcRawData + ss0 * bytePerSample,
         (src->getSampleCount() - ss0) * bytePerSample);

  return dst;
}

//------------------------------------------------------------------------------

TSoundTrackP TSop::insertBlank(TSoundTrackP src, double t0, double len) {
  return insertBlank(src, src->secondsToSamples(t0),
                     src->secondsToSamples(len));
}

//------------------------------------------------------------------------------

TSoundTrackP TSop::remove(TSoundTrackP src, TINT32 s0, TINT32 s1,
                          TSoundTrackP &paste) {
  TINT32 ss0, ss1;

  ss0 = std::max<TINT32>((TINT32)0, s0);
  ss1 = std::min(s1, (TINT32)(src->getSampleCount() - 1));
  TSoundTrackP soundTrackSlice;
  if (ss0 <= ss1) soundTrackSlice = src->extract(ss0, ss1);
  if (!soundTrackSlice) {
    paste = TSoundTrackP();
    return src;
  }
  paste = soundTrackSlice->clone();

  TSoundTrackFormat format = src->getFormat();
  TSoundTrackP dst =
      TSoundTrack::create(format, (src->getSampleCount() - (ss1 - ss0 + 1)));

  TINT32 dstSampleSize = dst->getSampleSize();
  UCHAR *newRowData    = (UCHAR *)dst->getRawData();
  UCHAR *srcRowData    = (UCHAR *)src->getRawData();

  memcpy(newRowData, srcRowData, ss0 * dstSampleSize);
  memcpy(newRowData + (ss0 * dstSampleSize),
         srcRowData + (ss1 + 1) * dstSampleSize,
         (src->getSampleCount() - ss1 - 1) * dst->getSampleSize());

  return dst;
}

//------------------------------------------------------------------------------

TSoundTrackP TSop::remove(TSoundTrackP src, double t0, double t1,
                          TSoundTrackP &paste) {
  return remove(src, src->secondsToSamples(t0), src->secondsToSamples(t1),
                paste);
}

//------------------------------------------------------------------------------

template <class T>
TSoundTrackP mixT(TSoundTrackT<T> *st1, double a1, TSoundTrackT<T> *st2,
                  double a2) {
  TINT32 sampleCount = std::max(st1->getSampleCount(), st2->getSampleCount());

  TSoundTrackT<T> *dst = new TSoundTrackT<T>(
      st1->getSampleRate(), st1->getChannelCount(), sampleCount);

  T *dstSample = dst->samples();
  T *endDstSample =
      dstSample + std::min(st1->getSampleCount(), st2->getSampleCount());

  T *st1Sample = st1->samples();
  T *st2Sample = st2->samples();

  while (dstSample < endDstSample) {
    *dstSample++ = T::mix(*st1Sample, a1, *st2Sample, a2);
    ++st1Sample;
    ++st2Sample;
  }

  T *srcSample =
      st1->getSampleCount() > st2->getSampleCount() ? st1Sample : st2Sample;
  endDstSample                                  = dst->samples() + sampleCount;
  while (dstSample < endDstSample) *dstSample++ = *srcSample++;

  return TSoundTrackP(dst);
}

//=============================================================================

class TSoundTrackMixer final : public TSoundTransform {
  double m_alpha1, m_alpha2;
  TSoundTrackP m_sndtrack;

public:
  TSoundTrackMixer(double a1, double a2, const TSoundTrackP &st2)
      : TSoundTransform(), m_alpha1(a1), m_alpha2(a2), m_sndtrack(st2) {}

  ~TSoundTrackMixer(){};

  TSoundTrackP compute(const TSoundTrackMono8Signed &src) override {
    assert(src.getFormat() == m_sndtrack->getFormat());

    return (
        mixT(const_cast<TSoundTrackMono8Signed *>(&src), m_alpha1,
             dynamic_cast<TSoundTrackMono8Signed *>(m_sndtrack.getPointer()),
             m_alpha2));
  }

  TSoundTrackP compute(const TSoundTrackMono8Unsigned &src) override {
    assert(src.getFormat() == m_sndtrack->getFormat());

    return (
        mixT(const_cast<TSoundTrackMono8Unsigned *>(&src), m_alpha1,
             dynamic_cast<TSoundTrackMono8Unsigned *>(m_sndtrack.getPointer()),
             m_alpha2));
  }

  TSoundTrackP compute(const TSoundTrackStereo8Signed &src) override {
    assert(src.getFormat() == m_sndtrack->getFormat());

    return (
        mixT(const_cast<TSoundTrackStereo8Signed *>(&src), m_alpha1,
             dynamic_cast<TSoundTrackStereo8Signed *>(m_sndtrack.getPointer()),
             m_alpha2));
  }

  TSoundTrackP compute(const TSoundTrackStereo8Unsigned &src) override {
    assert(src.getFormat() == m_sndtrack->getFormat());

    return (mixT(
        const_cast<TSoundTrackStereo8Unsigned *>(&src), m_alpha1,
        dynamic_cast<TSoundTrackStereo8Unsigned *>(m_sndtrack.getPointer()),
        m_alpha2));
  }

  TSoundTrackP compute(const TSoundTrackMono16 &src) override {
    assert(src.getFormat() == m_sndtrack->getFormat());

    return (mixT(const_cast<TSoundTrackMono16 *>(&src), m_alpha1,
                 dynamic_cast<TSoundTrackMono16 *>(m_sndtrack.getPointer()),
                 m_alpha2));
  }

  TSoundTrackP compute(const TSoundTrackStereo16 &src) override {
    assert(src.getFormat() == m_sndtrack->getFormat());

    return (mixT(const_cast<TSoundTrackStereo16 *>(&src), m_alpha1,
                 dynamic_cast<TSoundTrackStereo16 *>(m_sndtrack.getPointer()),
                 m_alpha2));
  }

  TSoundTrackP compute(const TSoundTrackMono24 &src) override {
    assert(src.getFormat() == m_sndtrack->getFormat());

    return (mixT(const_cast<TSoundTrackMono24 *>(&src), m_alpha1,
                 dynamic_cast<TSoundTrackMono24 *>(m_sndtrack.getPointer()),
                 m_alpha2));
  }

  TSoundTrackP compute(const TSoundTrackStereo24 &src) override {
    assert(src.getFormat() == m_sndtrack->getFormat());

    return (mixT(const_cast<TSoundTrackStereo24 *>(&src), m_alpha1,
                 dynamic_cast<TSoundTrackStereo24 *>(m_sndtrack.getPointer()),
                 m_alpha2));
  }

  TSoundTrackP compute(const TSoundTrackMono32Float &src) override {
    assert(src.getFormat() == m_sndtrack->getFormat());

    return (
        mixT(const_cast<TSoundTrackMono32Float *>(&src), m_alpha1,
             dynamic_cast<TSoundTrackMono32Float *>(m_sndtrack.getPointer()),
             m_alpha2));
  }

  TSoundTrackP compute(const TSoundTrackStereo32Float &src) override {
    assert(src.getFormat() == m_sndtrack->getFormat());

    return (
        mixT(const_cast<TSoundTrackStereo32Float *>(&src), m_alpha1,
             dynamic_cast<TSoundTrackStereo32Float *>(m_sndtrack.getPointer()),
             m_alpha2));
  }
};

TSoundTrackP TSop::mix(const TSoundTrackP &st1, const TSoundTrackP &st2,
                       double a1, double a2) {
  TSoundTrackMixer *converter;
  a1               = tcrop<double>(a1, 0.0, 1.0);
  a2               = tcrop<double>(a2, 0.0, 1.0);
  converter        = new TSoundTrackMixer(a1, a2, st2);
  TSoundTrackP snd = st1->apply(converter);
  delete converter;
  return (snd);
}

//==============================================================================
//
// TSop::FadeIn
//
//==============================================================================

template <class T>
TSoundTrackP doFadeIn(const TSoundTrackT<T> &track, double riseFactor) {
  typedef typename T::ChannelValueType ChannelValueType;
  int sampleCount = (int)((double)track.getSampleCount() * riseFactor);
  if (!sampleCount) sampleCount = 1;
  assert(sampleCount);
  int channelCount = track.getChannelCount();

  TSoundTrackT<T> *out =
      new TSoundTrackT<T>(track.getSampleRate(), channelCount, sampleCount);

  double val[2], step[2];

  ChannelValueType chan[2];
  const T *firstSample = track.samples();
  for (int k = 0; k < channelCount; ++k) {
    chan[k] = firstSample->getValue(k);
    if (firstSample->isSampleSigned()) {
      val[k]  = 0;
      step[k] = (double)chan[k] / (double)sampleCount;
    } else {
      val[k]  = 127;
      step[k] = (double)(chan[k] - 128) / (double)sampleCount;
    }
  }

  T *psample = out->samples();
  T *end     = psample + out->getSampleCount();

  while (psample < end) {
    T sample;
    for (int k = 0; k < channelCount; ++k) {
      sample.setValue(k, (ChannelValueType)val[k]);
      val[k] += step[k];
    }
    *psample = sample;
    ++psample;
  }

  return out;
}

//------------------------------------------------------------------------------

class TSoundTrackFaderIn final : public TSoundTransform {
public:
  TSoundTrackFaderIn(double riseFactor)
      : TSoundTransform(), m_riseFactor(riseFactor) {}

  TSoundTrackP compute(const TSoundTrackMono8Signed &) override;
  TSoundTrackP compute(const TSoundTrackStereo8Signed &) override;
  TSoundTrackP compute(const TSoundTrackMono8Unsigned &) override;
  TSoundTrackP compute(const TSoundTrackStereo8Unsigned &) override;
  TSoundTrackP compute(const TSoundTrackMono16 &) override;
  TSoundTrackP compute(const TSoundTrackStereo16 &) override;
  TSoundTrackP compute(const TSoundTrackMono24 &) override;
  TSoundTrackP compute(const TSoundTrackStereo24 &) override;
  TSoundTrackP compute(const TSoundTrackMono32Float &) override;
  TSoundTrackP compute(const TSoundTrackStereo32Float &) override;

  double m_riseFactor;
};

//------------------------------------------------------------------------------

TSoundTrackP TSoundTrackFaderIn::compute(const TSoundTrackMono8Signed &track) {
  return doFadeIn(track, m_riseFactor);
}

//------------------------------------------------------------------------------

TSoundTrackP TSoundTrackFaderIn::compute(
    const TSoundTrackStereo8Signed &track) {
  return doFadeIn(track, m_riseFactor);
}

//------------------------------------------------------------------------------

TSoundTrackP TSoundTrackFaderIn::compute(
    const TSoundTrackMono8Unsigned &track) {
  return doFadeIn(track, m_riseFactor);
}

//------------------------------------------------------------------------------

TSoundTrackP TSoundTrackFaderIn::compute(
    const TSoundTrackStereo8Unsigned &track) {
  return doFadeIn(track, m_riseFactor);
}

//------------------------------------------------------------------------------

TSoundTrackP TSoundTrackFaderIn::compute(const TSoundTrackMono16 &track) {
  return doFadeIn(track, m_riseFactor);
}

//------------------------------------------------------------------------------

TSoundTrackP TSoundTrackFaderIn::compute(const TSoundTrackStereo16 &track) {
  return doFadeIn(track, m_riseFactor);
}
//------------------------------------------------------------------------------

TSoundTrackP TSoundTrackFaderIn::compute(const TSoundTrackMono24 &track) {
  return doFadeIn(track, m_riseFactor);
}
//------------------------------------------------------------------------------

TSoundTrackP TSoundTrackFaderIn::compute(const TSoundTrackStereo24 &track) {
  return doFadeIn(track, m_riseFactor);
}

//------------------------------------------------------------------------------

TSoundTrackP TSoundTrackFaderIn::compute(const TSoundTrackMono32Float &track) {
  return doFadeIn(track, m_riseFactor);
}

//------------------------------------------------------------------------------

TSoundTrackP TSoundTrackFaderIn::compute(
    const TSoundTrackStereo32Float &track) {
  return doFadeIn(track, m_riseFactor);
}

//------------------------------------------------------------------------------

TSoundTrackP TSop::fadeIn(const TSoundTrackP src, double riseFactor) {
  TSoundTrackFaderIn *fader = new TSoundTrackFaderIn(riseFactor);
  TSoundTrackP out          = src->apply(fader);
  delete fader;
  return out;
}

//==============================================================================
//
// TSop::FadeOut
//
//==============================================================================

template <class T>
TSoundTrackP doFadeOut(const TSoundTrackT<T> &track, double decayFactor) {
  typedef typename T::ChannelValueType ChannelValueType;
  int sampleCount = (int)((double)track.getSampleCount() * decayFactor);
  if (!sampleCount) sampleCount = 1;
  assert(sampleCount);
  int channelCount = track.getChannelCount();

  TSoundTrackT<T> *out =
      new TSoundTrackT<T>(track.getSampleRate(), channelCount, sampleCount);

  double val[2], step[2];
  ChannelValueType chan[2];
  const T *lastSample = (track.samples() + track.getSampleCount() - 1);
  for (int k = 0; k < channelCount; ++k) {
    chan[k] = lastSample->getValue(k);
    val[k]  = (double)chan[k];
    if (lastSample->isSampleSigned())
      step[k] = (double)chan[k] / (double)sampleCount;
    else
      step[k] = (double)(chan[k] - 128) / (double)sampleCount;
  }

  T *psample = out->samples();
  T *end     = psample + out->getSampleCount();

  while (psample < end) {
    T sample;
    for (int k = 0; k < channelCount; ++k) {
      sample.setValue(k, (ChannelValueType)val[k]);
      val[k] -= step[k];
    }
    *psample = sample;
    ++psample;
  }

  return out;
}

//------------------------------------------------------------------------------

class TSoundTrackFaderOut final : public TSoundTransform {
public:
  TSoundTrackFaderOut(double decayFactor)
      : TSoundTransform(), m_decayFactor(decayFactor) {}

  TSoundTrackP compute(const TSoundTrackMono8Signed &) override;
  TSoundTrackP compute(const TSoundTrackStereo8Signed &) override;
  TSoundTrackP compute(const TSoundTrackMono8Unsigned &) override;
  TSoundTrackP compute(const TSoundTrackStereo8Unsigned &) override;
  TSoundTrackP compute(const TSoundTrackMono16 &) override;
  TSoundTrackP compute(const TSoundTrackStereo16 &) override;
  TSoundTrackP compute(const TSoundTrackMono24 &) override;
  TSoundTrackP compute(const TSoundTrackStereo24 &) override;
  TSoundTrackP compute(const TSoundTrackMono32Float &) override;
  TSoundTrackP compute(const TSoundTrackStereo32Float &) override;

  double m_decayFactor;
};

//------------------------------------------------------------------------------

TSoundTrackP TSoundTrackFaderOut::compute(const TSoundTrackMono8Signed &track) {
  return doFadeOut(track, m_decayFactor);
}

//------------------------------------------------------------------------------

TSoundTrackP TSoundTrackFaderOut::compute(
    const TSoundTrackStereo8Signed &track) {
  return doFadeOut(track, m_decayFactor);
}

//------------------------------------------------------------------------------

TSoundTrackP TSoundTrackFaderOut::compute(
    const TSoundTrackMono8Unsigned &track) {
  return doFadeOut(track, m_decayFactor);
}

//------------------------------------------------------------------------------

TSoundTrackP TSoundTrackFaderOut::compute(
    const TSoundTrackStereo8Unsigned &track) {
  return doFadeOut(track, m_decayFactor);
}

//------------------------------------------------------------------------------

TSoundTrackP TSoundTrackFaderOut::compute(const TSoundTrackMono16 &track) {
  return doFadeOut(track, m_decayFactor);
}

//------------------------------------------------------------------------------

TSoundTrackP TSoundTrackFaderOut::compute(const TSoundTrackStereo16 &track) {
  return doFadeOut(track, m_decayFactor);
}
//------------------------------------------------------------------------------

TSoundTrackP TSoundTrackFaderOut::compute(const TSoundTrackMono24 &track) {
  return doFadeOut(track, m_decayFactor);
}

//------------------------------------------------------------------------------

TSoundTrackP TSoundTrackFaderOut::compute(const TSoundTrackStereo24 &track) {
  return doFadeOut(track, m_decayFactor);
}

//------------------------------------------------------------------------------

TSoundTrackP TSoundTrackFaderOut::compute(const TSoundTrackMono32Float &track) {
  return doFadeOut(track, m_decayFactor);
}

//------------------------------------------------------------------------------

TSoundTrackP TSoundTrackFaderOut::compute(
    const TSoundTrackStereo32Float &track) {
  return doFadeOut(track, m_decayFactor);
}

//------------------------------------------------------------------------------

TSoundTrackP TSop::fadeOut(const TSoundTrackP src, double decayFactor) {
  TSoundTrackFaderOut *fader = new TSoundTrackFaderOut(decayFactor);
  TSoundTrackP out           = src->apply(fader);
  delete fader;
  return out;
}

//==============================================================================
//
// TSop::CrossFade
//
//==============================================================================

template <class T>
TSoundTrackP doCrossFade(const TSoundTrackT<T> &track1, TSoundTrackT<T> *track2,
                         double crossFactor) {
  typedef typename T::ChannelValueType ChannelValueType;
  int channelCount = track2->getChannelCount();
  int sampleCount  = (int)((double)track2->getSampleCount() * crossFactor);
  if (!sampleCount) sampleCount = 1;
  assert(sampleCount);

  // ultimo campione di track1
  ChannelValueType chanTrack1[2];
  const T *lastSample = (track1.samples() + track1.getSampleCount() - 1);
  int k;
  for (k = 0; k < channelCount; ++k) chanTrack1[k] = lastSample->getValue(k);

  double val[2], step[2];

  // primo campione di track2
  ChannelValueType chanTrack2[2];
  const T *firstSample = track2->samples();
  for (k = 0; k < channelCount; ++k) {
    chanTrack2[k] = firstSample->getValue(k);
    val[k]        = chanTrack1[k] - chanTrack2[k];
    step[k]       = val[k] / (double)sampleCount;
  }

  TSoundTrackT<T> *out =
      new TSoundTrackT<T>(track2->getSampleRate(), channelCount, sampleCount);

  T *psample = out->samples();
  T *end     = psample + out->getSampleCount();

  while (psample < end) {
    T sample;
    for (int k = 0; k < channelCount; ++k) {
      double tot             = (double)firstSample->getValue(k) + val[k];
      ChannelValueType value = (ChannelValueType)tot;

      sample.setValue(k, value);
      val[k] -= step[k];
    }
    *psample = sample;
    ++psample;
    //++firstSample;
  }

  return out;
}

//------------------------------------------------------------------------------

class TSoundTrackCrossFader final : public TSoundTransform {
public:
  TSoundTrackCrossFader(TSoundTrackP src, double crossFactor)
      : TSoundTransform(), m_st(src), m_crossFactor(crossFactor) {}

  TSoundTrackP compute(const TSoundTrackMono8Signed &) override;
  TSoundTrackP compute(const TSoundTrackStereo8Signed &) override;
  TSoundTrackP compute(const TSoundTrackMono8Unsigned &) override;
  TSoundTrackP compute(const TSoundTrackStereo8Unsigned &) override;
  TSoundTrackP compute(const TSoundTrackMono16 &) override;
  TSoundTrackP compute(const TSoundTrackStereo16 &) override;
  TSoundTrackP compute(const TSoundTrackMono24 &) override;
  TSoundTrackP compute(const TSoundTrackStereo24 &) override;
  TSoundTrackP compute(const TSoundTrackMono32Float &) override;
  TSoundTrackP compute(const TSoundTrackStereo32Float &) override;

  TSoundTrackP m_st;
  double m_crossFactor;
};

//------------------------------------------------------------------------------

TSoundTrackP TSoundTrackCrossFader::compute(const TSoundTrackMono8Signed &src) {
  assert(src.getFormat() == m_st->getFormat());
  return doCrossFade(src,
                     dynamic_cast<TSoundTrackMono8Signed *>(m_st.getPointer()),
                     m_crossFactor);
}

//------------------------------------------------------------------------------

TSoundTrackP TSoundTrackCrossFader::compute(
    const TSoundTrackStereo8Signed &src) {
  assert(src.getFormat() == m_st->getFormat());
  return doCrossFade(
      src, dynamic_cast<TSoundTrackStereo8Signed *>(m_st.getPointer()),
      m_crossFactor);
}

//------------------------------------------------------------------------------

TSoundTrackP TSoundTrackCrossFader::compute(
    const TSoundTrackMono8Unsigned &src) {
  assert(src.getFormat() == m_st->getFormat());
  return doCrossFade(
      src, dynamic_cast<TSoundTrackMono8Unsigned *>(m_st.getPointer()),
      m_crossFactor);
}

//------------------------------------------------------------------------------

TSoundTrackP TSoundTrackCrossFader::compute(
    const TSoundTrackStereo8Unsigned &src) {
  assert(src.getFormat() == m_st->getFormat());
  return doCrossFade(
      src, dynamic_cast<TSoundTrackStereo8Unsigned *>(m_st.getPointer()),
      m_crossFactor);
}

//------------------------------------------------------------------------------

TSoundTrackP TSoundTrackCrossFader::compute(const TSoundTrackMono16 &src) {
  assert(src.getFormat() == m_st->getFormat());
  return doCrossFade(src, dynamic_cast<TSoundTrackMono16 *>(m_st.getPointer()),
                     m_crossFactor);
}

//------------------------------------------------------------------------------

TSoundTrackP TSoundTrackCrossFader::compute(const TSoundTrackStereo16 &src) {
  assert(src.getFormat() == m_st->getFormat());
  return doCrossFade(src,
                     dynamic_cast<TSoundTrackStereo16 *>(m_st.getPointer()),
                     m_crossFactor);
}

//------------------------------------------------------------------------------

TSoundTrackP TSoundTrackCrossFader::compute(const TSoundTrackMono24 &src) {
  assert(src.getFormat() == m_st->getFormat());
  return doCrossFade(src, dynamic_cast<TSoundTrackMono24 *>(m_st.getPointer()),
                     m_crossFactor);
}

//------------------------------------------------------------------------------

TSoundTrackP TSoundTrackCrossFader::compute(const TSoundTrackStereo24 &src) {
  assert(src.getFormat() == m_st->getFormat());
  return doCrossFade(src,
                     dynamic_cast<TSoundTrackStereo24 *>(m_st.getPointer()),
                     m_crossFactor);
}

//------------------------------------------------------------------------------

TSoundTrackP TSoundTrackCrossFader::compute(const TSoundTrackMono32Float &src) {
  assert(src.getFormat() == m_st->getFormat());
  return doCrossFade(src,
                     dynamic_cast<TSoundTrackMono32Float *>(m_st.getPointer()),
                     m_crossFactor);
}

//------------------------------------------------------------------------------

TSoundTrackP TSoundTrackCrossFader::compute(
    const TSoundTrackStereo32Float &src) {
  assert(src.getFormat() == m_st->getFormat());
  return doCrossFade(
      src, dynamic_cast<TSoundTrackStereo32Float *>(m_st.getPointer()),
      m_crossFactor);
}

//------------------------------------------------------------------------------

TSoundTrackP TSop::crossFade(const TSoundTrackP src1, const TSoundTrackP src2,
                             double crossFactor) {
  TSoundTrackCrossFader *fader = new TSoundTrackCrossFader(src2, crossFactor);
  TSoundTrackP out             = src1->apply(fader);
  delete fader;
  return out;
}

//
//
//
//
//==============================================================================
//
// TSop::CrossFadeOverWrite
//
//==============================================================================

template <class T>
TSoundTrackP doCrossFadeOverWrite(const TSoundTrackT<T> &track1,
                                  TSoundTrackT<T> *track2, double crossFactor) {
  typedef typename T::ChannelValueType ChannelValueType;
  int channelCount  = track2->getChannelCount();
  int sampleCount   = (int)((double)track2->getSampleCount() * crossFactor);
  int sampleCountT2 = track2->getSampleCount();

  if (sampleCount == 0 && sampleCountT2 == 1) return track2;
  if (sampleCount == 0) sampleCount = 1;
  assert(sampleCount);

  // ultimo campione di track1
  ChannelValueType chanTrack1[2];
  const T *lastSample = (track1.samples() + track1.getSampleCount() - 1);
  int k;
  for (k = 0; k < channelCount; ++k) chanTrack1[k] = lastSample->getValue(k);

  double val[2], step[2];

  // primo campione di track2
  ChannelValueType chanTrack2[2];
  const T *firstSample = track2->samples() + sampleCount;
  for (k = 0; k < channelCount; ++k) {
    chanTrack2[k] = firstSample->getValue(k);
    val[k]        = chanTrack1[k] - chanTrack2[k];
    step[k]       = val[k] / (double)sampleCount;
  }

  TSoundTrackT<T> *out =
      new TSoundTrackT<T>(track2->getSampleRate(), channelCount, sampleCountT2);

  T *psample = out->samples();
  T *end     = psample + sampleCount;

  while (psample < end) {
    T sample;
    for (int k = 0; k < channelCount; ++k) {
      double tot             = (double)firstSample->getValue(k) + val[k];
      ChannelValueType value = (ChannelValueType)tot;

      sample.setValue(k, value);
      val[k] -= step[k];
    }
    *psample = sample;
    ++psample;
  }

  out->copy(track2->extract(sampleCount, sampleCountT2 - 1), sampleCount);

  return out;
}

//------------------------------------------------------------------------------

class TSoundTrackCrossFaderOverWrite final : public TSoundTransform {
public:
  TSoundTrackCrossFaderOverWrite(TSoundTrackP src, double crossFactor)
      : TSoundTransform(), m_st(src), m_crossFactor(crossFactor) {}

  TSoundTrackP compute(const TSoundTrackMono8Signed &) override;
  TSoundTrackP compute(const TSoundTrackStereo8Signed &) override;
  TSoundTrackP compute(const TSoundTrackMono8Unsigned &) override;
  TSoundTrackP compute(const TSoundTrackStereo8Unsigned &) override;
  TSoundTrackP compute(const TSoundTrackMono16 &) override;
  TSoundTrackP compute(const TSoundTrackStereo16 &) override;
  TSoundTrackP compute(const TSoundTrackMono24 &) override;
  TSoundTrackP compute(const TSoundTrackStereo24 &) override;
  TSoundTrackP compute(const TSoundTrackMono32Float &) override;
  TSoundTrackP compute(const TSoundTrackStereo32Float &) override;

  TSoundTrackP m_st;
  double m_crossFactor;
};

//------------------------------------------------------------------------------

TSoundTrackP TSoundTrackCrossFaderOverWrite::compute(
    const TSoundTrackMono8Signed &src) {
  assert(src.getFormat() == m_st->getFormat());
  return doCrossFadeOverWrite(
      src, dynamic_cast<TSoundTrackMono8Signed *>(m_st.getPointer()),
      m_crossFactor);
}

//------------------------------------------------------------------------------

TSoundTrackP TSoundTrackCrossFaderOverWrite::compute(
    const TSoundTrackStereo8Signed &src) {
  assert(src.getFormat() == m_st->getFormat());
  return doCrossFadeOverWrite(
      src, dynamic_cast<TSoundTrackStereo8Signed *>(m_st.getPointer()),
      m_crossFactor);
}

//------------------------------------------------------------------------------

TSoundTrackP TSoundTrackCrossFaderOverWrite::compute(
    const TSoundTrackMono8Unsigned &src) {
  assert(src.getFormat() == m_st->getFormat());
  return doCrossFadeOverWrite(
      src, dynamic_cast<TSoundTrackMono8Unsigned *>(m_st.getPointer()),
      m_crossFactor);
}

//------------------------------------------------------------------------------

TSoundTrackP TSoundTrackCrossFaderOverWrite::compute(
    const TSoundTrackStereo8Unsigned &src) {
  assert(src.getFormat() == m_st->getFormat());
  return doCrossFadeOverWrite(
      src, dynamic_cast<TSoundTrackStereo8Unsigned *>(m_st.getPointer()),
      m_crossFactor);
}

//------------------------------------------------------------------------------

TSoundTrackP TSoundTrackCrossFaderOverWrite::compute(
    const TSoundTrackMono16 &src) {
  assert(src.getFormat() == m_st->getFormat());
  return doCrossFadeOverWrite(
      src, dynamic_cast<TSoundTrackMono16 *>(m_st.getPointer()), m_crossFactor);
}

//------------------------------------------------------------------------------

TSoundTrackP TSoundTrackCrossFaderOverWrite::compute(
    const TSoundTrackStereo16 &src) {
  assert(src.getFormat() == m_st->getFormat());
  return doCrossFadeOverWrite(
      src, dynamic_cast<TSoundTrackStereo16 *>(m_st.getPointer()),
      m_crossFactor);
}

//------------------------------------------------------------------------------

TSoundTrackP TSoundTrackCrossFaderOverWrite::compute(
    const TSoundTrackMono24 &src) {
  assert(src.getFormat() == m_st->getFormat());
  return doCrossFadeOverWrite(
      src, dynamic_cast<TSoundTrackMono24 *>(m_st.getPointer()), m_crossFactor);
}

//------------------------------------------------------------------------------

TSoundTrackP TSoundTrackCrossFaderOverWrite::compute(
    const TSoundTrackStereo24 &src) {
  assert(src.getFormat() == m_st->getFormat());
  return doCrossFadeOverWrite(
      src, dynamic_cast<TSoundTrackStereo24 *>(m_st.getPointer()),
      m_crossFactor);
}

//------------------------------------------------------------------------------

TSoundTrackP TSoundTrackCrossFaderOverWrite::compute(
    const TSoundTrackMono32Float &src) {
  assert(src.getFormat() == m_st->getFormat());
  return doCrossFadeOverWrite(
      src, dynamic_cast<TSoundTrackMono32Float *>(m_st.getPointer()),
      m_crossFactor);
}

//------------------------------------------------------------------------------

TSoundTrackP TSoundTrackCrossFaderOverWrite::compute(
    const TSoundTrackStereo32Float &src) {
  assert(src.getFormat() == m_st->getFormat());
  return doCrossFadeOverWrite(
      src, dynamic_cast<TSoundTrackStereo32Float *>(m_st.getPointer()),
      m_crossFactor);
}

//------------------------------------------------------------------------------

TSoundTrackP TSop::crossFade(double crossFactor, const TSoundTrackP src1,
                             const TSoundTrackP src2) {
  TSoundTrackCrossFaderOverWrite *fader =
      new TSoundTrackCrossFaderOverWrite(src2, crossFactor);
  TSoundTrackP out = src1->apply(fader);
  delete fader;
  return out;
}
