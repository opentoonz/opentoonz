#!/usr/bin/env python
#  Copyright (c) 2003-2019, Mark Borgerding. All rights reserved.
#  This file is part of KISS FFT - https://github.com/mborgerding/kissfft
#
# SPDX-License-Identifier: BSD-3-Clause
#  See COPYING file for more information.
from __future__ import absolute_import, division, print_function
import math
import sys
import os
import random
import struct
import getopt
import numpy as np

po = math.pi
e = math.e
do_real = False
datatype = os.environ.get('KISSFFT_DATATYPE', 'float')
openmp = os.environ.get('KISSFFT_OPENMP', 'float')

util = './fastfilt-' + datatype

if openmp == '1' or openmp == 'ON':
    util = util + '-openmp'

minsnr = 90
if datatype == 'double':
    dtype = np.float64
elif datatype == 'float':
    dtype = np.float32
elif datatype == 'int16_t':
    dtype = np.int16
    minsnr = 10
elif datatype == 'int32_t':
    dtype = np.int32
elif datatype == 'simd':
    sys.stderr.write('testkiss.py does not yet test simd')
    sys.exit(0)
else:
    sys.stderr.write('unrecognized datatype {0}\n'.format(datatype))
    sys.exit(1)

def dopack(x):
    if np.iscomplexobj(x):
        x = x.astype(np.complex128).view(np.float64)
    else:
        x = x.astype(np.float64)
    return x.astype(dtype).tobytes()

def dounpack(x, cpx):
    x = np.frombuffer(x, dtype).astype(np.float64)
    if cpx:
        x = x[::2] + 1j * x[1::2]
    return x

def make_random(shape):
    'create random uniform (-1,1) data of the given shape'
    if do_real:
        return np.random.uniform(-1, 1, shape)
    else:
        return (np.random.uniform(-1, 1, shape) + 1j * np.random.uniform(-1, 1, shape))

def randmat(ndim):
    'create a random multidimensional array in range (-1,1)'
    dims = np.random.randint(2, 5, ndim)
    if do_real:
        dims[-1] = (dims[-1] // 2) * 2  # force even last dimension if real
    return make_random(dims)

def test_fft(ndim):
    x = randmat(ndim)

    if do_real:
        xver = np.fft.rfftn(x)
    else:
        xver = np.fft.fftn(x)

    x2 = dofft(x, do_real)
    err = xver - x2
    errf = err.ravel()
    xverf = xver.ravel()
    errpow = np.vdot(errf, errf) + 1e-10
    sigpow = np.vdot(xverf, xverf) + 1e-10
    snr = 10 * math.log10(abs(sigpow / errpow))
    print('SNR (compared to NumPy) : {0:.1f}dB'.format(float(snr)))

    if snr < minsnr:
        print('xver=', xver)
        print('x2=', x2)
        print('err', err)
        sys.exit(1)

def dofft(x, isreal):
    dims = list(np.shape(x))
    x = x.ravel()

    scale = 1
    if datatype == 'int16_t':
        x = 32767 * x
        scale = len(x) / 32767.0
    elif datatype == 'int32_t':
        x = 2147483647.0 * x
        scale = len(x) / 2147483647.0

    cmd = util + ' -n '
    cmd += ','.join([str(d) for d in dims])
    if do_real:
        cmd += ' -R '

    print(cmd)

    from subprocess import Popen, PIPE
    p = Popen(cmd, shell=True, stdin=PIPE, stdout=PIPE)

    p.stdin.write(dopack(x))
    p.stdin.close()

    res = dounpack(p.stdout.read(), 1)
    if do_real:
        dims[-1] = (dims[-1] // 2) + 1

    res = scale * res

    p.wait()
    return np.reshape(res, dims)

def main():
    opts, args = getopt.getopt(sys.argv[1:], 'r')
    opts = dict(opts)
    global do_real
    do_real = '-r' in opts
    if do_real:
        print('Testing multi-dimensional real FFTs')
    else:
        print('Testing multi-dimensional FFTs')

    for dim in range(1, 4):
        test_fft(dim)


if __name__ == "__main__":
    main()
