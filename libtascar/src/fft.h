/**
 * @file   fft.h
 * @author Giso Grimm
 * 
 * @brief  Wrapper class for FFTW
 */ 
/* License (GPL)
 *
 * Copyright (C) 2018  Giso Grimm
 *
 * This program is free software; you can redistribute it and/ or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */
#ifndef FFT_H
#define FFT_H

#include "spectrum.h"
#include "audiochunks.h"
#include <fftw3.h>

namespace TASCAR {

  /**
     \brief Wrapper class around real-to-complex and complex-to-real fftw
   */
  class fft_t {
  public:
    /**
       \brief Constructor
       \param fftlen FFT length
     */
    fft_t(uint32_t fftlen);
    /**
       \brief Copy constructor
    */
    fft_t(const fft_t& src);
    /**
       \brief Perform a waveform to spectrum (real-to-complex) transformation

       \param src Input waveform

       The result is stored in the public member fft_t::s.
     */
    void execute(const TASCAR::wave_t& src);
    /**
       \brief Perform a spectrum to waveform (complex-to-real) transformation

       \param src Input spectrum

       The result is stored in the public member fft_t::w.
     */
    void execute(const TASCAR::spec_t& src);
    /**
       \brief Perform a spectrum to waveform (complex-to-real) transformation

       The input is read from the public member fft_t::s.
       The result is stored in the public member fft::w.
     */
    void ifft();
    
    /**
       \brief Perform a waveform to spectrum (real-to-complex) transformation

       The input is read from the public member fft_t::w.
       The result is stored in the public member fft_t::s.
     */
    void fft();
    ~fft_t();
    TASCAR::wave_t w; ///< waveform container
    TASCAR::spec_t s; ///< spectrum container
  private:
    fftwf_plan fftwp_w2s;
    fftwf_plan fftwp_s2w;
  };

}

#endif

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
