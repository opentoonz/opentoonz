/*
 *  Copyright (c) 2003-2010, Mark Borgerding. All rights reserved.
 *  This file is part of KISS FFT - https://github.com/mborgerding/kissfft
 *
 *  SPDX-License-Identifier: BSD-3-Clause
 *  See COPYING file for more information.
 */

#ifndef KISSFFT_CLASS_HH
#define KISSFFT_CLASS_HH
#include <complex>
#include <utility>
#include <vector>


template <typename scalar_t>
class kissfft
{
    public:

        typedef std::complex<scalar_t> cpx_t;

        kissfft( const std::size_t nfft,
                 const bool inverse )
            :_nfft(nfft)
            ,_inverse(inverse)
        {
            // fill twiddle factors
            _twiddles.resize(_nfft);
            const scalar_t phinc =  (_inverse?2:-2)* std::acos( (scalar_t) -1)  / _nfft;
            for (std::size_t i=0;i<_nfft;++i)
                _twiddles[i] = std::exp( cpx_t(0,i*phinc) );

            //factorize
            //start factoring out 4's, then 2's, then 3,5,7,9,...
            std::size_t n= _nfft;
            std::size_t p=4;
            do {
                while (n % p) {
                    switch (p) {
                        case 4: p = 2; break;
                        case 2: p = 3; break;
                        default: p += 2; break;
                    }
                    if (p*p>n)
                        p = n;// no more factors
                }
                n /= p;
                _stageRadix.push_back(p);
                _stageRemainder.push_back(n);
            }while(n>1);
        }


        /// Changes the FFT-length and/or the transform direction.
        ///
        /// @post The @c kissfft object will be in the same state as if it
        /// had been newly constructed with the passed arguments.
        /// However, the implementation may be faster than constructing a
        /// new fft object.
        void assign( const std::size_t nfft,
                     const bool inverse )
        {
            if ( nfft != _nfft )
            {
                kissfft tmp( nfft, inverse ); // O(n) time.
                std::swap( tmp, *this ); // this is O(1) in C++11, O(n) otherwise.
            }
            else if ( inverse != _inverse )
            {
                // conjugate the twiddle factors.
                for ( typename std::vector<cpx_t>::iterator it = _twiddles.begin();
                      it != _twiddles.end(); ++it )
                    it->imag( -it->imag() );
            }
        }

        /// Calculates the complex Discrete Fourier Transform.
        ///
        /// The size of the passed arrays must be passed in the constructor.
        /// The sum of the squares of the absolute values in the @c dst
        /// array will be @c N times the sum of the squares of the absolute
        /// values in the @c src array, where @c N is the size of the array.
        /// In other words, the l_2 norm of the resulting array will be
        /// @c sqrt(N) times as big as the l_2 norm of the input array.
        /// This is also the case when the inverse flag is set in the
        /// constructor. Hence when applying the same transform twice, but with
        /// the inverse flag changed the second time, then the result will
        /// be equal to the original input times @c N.
        void transform(const cpx_t * fft_in, cpx_t * fft_out, const std::size_t stage = 0, const std::size_t fstride = 1, const std::size_t in_stride = 1) const
        {
            const std::size_t p = _stageRadix[stage];
            const std::size_t m = _stageRemainder[stage];
            cpx_t * const Fout_beg = fft_out;
            cpx_t * const Fout_end = fft_out + p*m;

            if (m==1) {
                do{
                    *fft_out = *fft_in;
                    fft_in += fstride*in_stride;
                }while(++fft_out != Fout_end );
            }else{
                do{
                    // recursive call:
                    // DFT of size m*p performed by doing
                    // p instances of smaller DFTs of size m,
                    // each one takes a decimated version of the input
                    transform(fft_in, fft_out, stage+1, fstride*p,in_stride);
                    fft_in += fstride*in_stride;
                }while( (fft_out += m) != Fout_end );
            }

            fft_out=Fout_beg;

            // recombine the p smaller DFTs
            switch (p) {
                case 2: kf_bfly2(fft_out,fstride,m); break;
                case 3: kf_bfly3(fft_out,fstride,m); break;
                case 4: kf_bfly4(fft_out,fstride,m); break;
                case 5: kf_bfly5(fft_out,fstride,m); break;
                default: kf_bfly_generic(fft_out,fstride,m,p); break;
            }
        }

        /// Calculates the Discrete Fourier Transform (DFT) of a real input
        /// of size @c 2*N.
        ///
        /// The 0-th and N-th value of the DFT are real numbers. These are
        /// stored in @c dst[0].real() and @c dst[0].imag() respectively.
        /// The remaining DFT values up to the index N-1 are stored in
        /// @c dst[1] to @c dst[N-1].
        /// The other half of the DFT values can be calculated from the
        /// symmetry relation
        ///     @code
        ///         DFT(src)[2*N-k] == conj( DFT(src)[k] );
        ///     @endcode
        /// The same scaling factors as in @c transform() apply.
        ///
        /// @note For this to work, the types @c scalar_t and @c cpx_t
        /// must fulfill the following requirements:
        ///
        /// For any object @c z of type @c cpx_t,
        /// @c reinterpret_cast<scalar_t(&)[2]>(z)[0] is the real part of @c z and
        /// @c reinterpret_cast<scalar_t(&)[2]>(z)[1] is the imaginary part of @c z.
        /// For any pointer to an element of an array of @c cpx_t named @c p
        /// and any valid array index @c i, @c reinterpret_cast<T*>(p)[2*i]
        /// is the real part of the complex number @c p[i], and
        /// @c reinterpret_cast<T*>(p)[2*i+1] is the imaginary part of the
        /// complex number @c p[i].
        ///
        /// Since C++11, these requirements are guaranteed to be satisfied for
        /// @c scalar_ts being @c float, @c double or @c long @c double
        /// together with @c cpx_t being @c std::complex<scalar_t>.
        void transform_real( const scalar_t * const src,
                             cpx_t * const dst ) const
        {
            const std::size_t N = _nfft;
            if ( N == 0 )
                return;

            // perform complex FFT
            transform( reinterpret_cast<const cpx_t*>(src), dst );

            // post processing for k = 0 and k = N
            dst[0] = cpx_t( dst[0].real() + dst[0].imag(),
                               dst[0].real() - dst[0].imag() );

            // post processing for all the other k = 1, 2, ..., N-1
            const scalar_t pi = std::acos( (scalar_t) -1);
            const scalar_t half_phi_inc = ( _inverse ? pi : -pi ) / N;
            const cpx_t twiddle_mul = std::exp( cpx_t(0, half_phi_inc) );
            for ( std::size_t k = 1; 2*k < N; ++k )
            {
                const cpx_t w = (scalar_t)0.5 * cpx_t(
                     dst[k].real() + dst[N-k].real(),
                     dst[k].imag() - dst[N-k].imag() );
                const cpx_t z = (scalar_t)0.5 * cpx_t(
                     dst[k].imag() + dst[N-k].imag(),
                    -dst[k].real() + dst[N-k].real() );
                const cpx_t twiddle =
                    k % 2 == 0 ?
                    _twiddles[k/2] :
                    _twiddles[k/2] * twiddle_mul;
                dst[  k] =       w + twiddle * z;
                dst[N-k] = std::conj( w - twiddle * z );
            }
            if ( N % 2 == 0 )
                dst[N/2] = std::conj( dst[N/2] );
        }

    private:

        void kf_bfly2( cpx_t * Fout, const size_t fstride, const std::size_t m) const
        {
            for (std::size_t k=0;k<m;++k) {
                const cpx_t t = Fout[m+k] * _twiddles[k*fstride];
                Fout[m+k] = Fout[k] - t;
                Fout[k] += t;
            }
        }

        void kf_bfly3( cpx_t * Fout, const std::size_t fstride, const std::size_t m) const
        {
            std::size_t k=m;
            const std::size_t m2 = 2*m;
            const cpx_t *tw1,*tw2;
            cpx_t scratch[5];
            const cpx_t epi3 = _twiddles[fstride*m];

            tw1=tw2=&_twiddles[0];

            do{
                scratch[1] = Fout[m]  * *tw1;
                scratch[2] = Fout[m2] * *tw2;

                scratch[3] = scratch[1] + scratch[2];
                scratch[0] = scratch[1] - scratch[2];
                tw1 += fstride;
                tw2 += fstride*2;

                Fout[m] = Fout[0] - scratch[3]*scalar_t(0.5);
                scratch[0] *= epi3.imag();

                Fout[0] += scratch[3];

                Fout[m2] = cpx_t(  Fout[m].real() + scratch[0].imag() , Fout[m].imag() - scratch[0].real() );

                Fout[m] += cpx_t( -scratch[0].imag(),scratch[0].real() );
                ++Fout;
            }while(--k);
        }

        void kf_bfly4( cpx_t * const Fout, const std::size_t fstride, const std::size_t m) const
        {
            cpx_t scratch[7];
            const scalar_t negative_if_inverse = _inverse ? -1 : +1;
            for (std::size_t k=0;k<m;++k) {
                scratch[0] = Fout[k+  m] * _twiddles[k*fstride  ];
                scratch[1] = Fout[k+2*m] * _twiddles[k*fstride*2];
                scratch[2] = Fout[k+3*m] * _twiddles[k*fstride*3];
                scratch[5] = Fout[k] - scratch[1];

                Fout[k] += scratch[1];
                scratch[3] = scratch[0] + scratch[2];
                scratch[4] = scratch[0] - scratch[2];
                scratch[4] = cpx_t( scratch[4].imag()*negative_if_inverse ,
                                      -scratch[4].real()*negative_if_inverse );

                Fout[k+2*m]  = Fout[k] - scratch[3];
                Fout[k    ]+= scratch[3];
                Fout[k+  m] = scratch[5] + scratch[4];
                Fout[k+3*m] = scratch[5] - scratch[4];
            }
        }

        void kf_bfly5( cpx_t * const Fout, const std::size_t fstride, const std::size_t m) const
        {
            cpx_t *Fout0,*Fout1,*Fout2,*Fout3,*Fout4;
            cpx_t scratch[13];
            const cpx_t ya = _twiddles[fstride*m];
            const cpx_t yb = _twiddles[fstride*2*m];

            Fout0=Fout;
            Fout1=Fout0+m;
            Fout2=Fout0+2*m;
            Fout3=Fout0+3*m;
            Fout4=Fout0+4*m;

            for ( std::size_t u=0; u<m; ++u ) {
                scratch[0] = *Fout0;

                scratch[1] = *Fout1 * _twiddles[  u*fstride];
                scratch[2] = *Fout2 * _twiddles[2*u*fstride];
                scratch[3] = *Fout3 * _twiddles[3*u*fstride];
                scratch[4] = *Fout4 * _twiddles[4*u*fstride];

                scratch[7] = scratch[1] + scratch[4];
                scratch[10]= scratch[1] - scratch[4];
                scratch[8] = scratch[2] + scratch[3];
                scratch[9] = scratch[2] - scratch[3];

                *Fout0 += scratch[7];
                *Fout0 += scratch[8];

                scratch[5] = scratch[0] + cpx_t(
                        scratch[7].real()*ya.real() + scratch[8].real()*yb.real(),
                        scratch[7].imag()*ya.real() + scratch[8].imag()*yb.real()
                        );

                scratch[6] =  cpx_t(
                         scratch[10].imag()*ya.imag() + scratch[9].imag()*yb.imag(),
                        -scratch[10].real()*ya.imag() - scratch[9].real()*yb.imag()
                        );

                *Fout1 = scratch[5] - scratch[6];
                *Fout4 = scratch[5] + scratch[6];

                scratch[11] = scratch[0] +
                    cpx_t(
                            scratch[7].real()*yb.real() + scratch[8].real()*ya.real(),
                            scratch[7].imag()*yb.real() + scratch[8].imag()*ya.real()
                            );

                scratch[12] = cpx_t(
                        -scratch[10].imag()*yb.imag() + scratch[9].imag()*ya.imag(),
                         scratch[10].real()*yb.imag() - scratch[9].real()*ya.imag()
                        );

                *Fout2 = scratch[11] + scratch[12];
                *Fout3 = scratch[11] - scratch[12];

                ++Fout0;
                ++Fout1;
                ++Fout2;
                ++Fout3;
                ++Fout4;
            }
        }

        /* perform the butterfly for one stage of a mixed radix FFT */
        void kf_bfly_generic(
                cpx_t * const Fout,
                const size_t fstride,
                const std::size_t m,
                const std::size_t p
                ) const
        {
            const cpx_t * twiddles = &_twiddles[0];

            if(p > _scratchbuf.size()) _scratchbuf.resize(p);

            for ( std::size_t u=0; u<m; ++u ) {
                std::size_t k = u;
                for ( std::size_t q1=0 ; q1<p ; ++q1 ) {
                    _scratchbuf[q1] = Fout[ k  ];
                    k += m;
                }

                k=u;
                for ( std::size_t q1=0 ; q1<p ; ++q1 ) {
                    std::size_t twidx=0;
                    Fout[ k ] = _scratchbuf[0];
                    for ( std::size_t q=1;q<p;++q ) {
                        twidx += fstride * k;
                        if (twidx>=_nfft)
                          twidx-=_nfft;
                        Fout[ k ] += _scratchbuf[q] * twiddles[twidx];
                    }
                    k += m;
                }
            }
        }

        std::size_t _nfft;
        bool _inverse;
        std::vector<cpx_t> _twiddles;
        std::vector<std::size_t> _stageRadix;
        std::vector<std::size_t> _stageRemainder;
        mutable std::vector<cpx_t> _scratchbuf;
};
#endif
