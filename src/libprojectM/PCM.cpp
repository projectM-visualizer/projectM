/**
 * projectM -- Milkdrop-esque visualisation SDK
 * Copyright (C)2003-2004 projectM Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * See 'LICENSE.txt' included within this release
 *
 */
/**
 * $Id: PCM.c,v 1.3 2006/03/13 20:35:26 psperl Exp $
 *
 * Takes sound data from wherever and hands it back out.
 * Returns PCM Data or spectrum data, or the derivative of the PCM data
 */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "Common.hpp"
#include "wipemalloc.h"
#include "fftsg.h"
#include "PCM.hpp"
#include <cassert>

//const size_t PCM::maxsamples = 2048;

//initPCM(int samples)
//
//Initializes the PCM buffer to
// number of samples specified.
#include <iostream>
PCM::PCM() : level(0.1), start(0), newsamples(0)
{
    _initPCM();

    #ifdef DEBUG
    std::cerr << "[PCM] MAX SAMPLES:" << maxsamples << std::endl;
    #endif
  }

void PCM::_initPCM()
{
    //Allocate FFT workspace
    // per rdft() documentation
    //    length of ip >= 2+sqrt(n) and length of w == n/2
#if FFT_LENGTH > 1024
#error update this code
#endif
    w  = (double *)wipemalloc(FFT_LENGTH*sizeof(double));
    ip = (int *)wipemalloc(34 * sizeof(int));
    ip[0]=0;

    memset(pcmL, 0, sizeof(pcmL));
    memset(pcmR, 0, sizeof(pcmR));
    memset(freqL, 0, sizeof(freqL));
    memset(freqR, 0, sizeof(freqR));
    memset(spectrumL, 0, sizeof(spectrumL));
    memset(spectrumR, 0, sizeof(spectrumR));
}


PCM::~PCM()
{
    free(w);
    free(ip);
}

#include <iostream>


void PCM::addPCMfloat(const float *PCMdata, size_t samples)
{
    float a,b,sum=0;
    for (size_t i=0; i<samples; i++)
    {
        size_t j=(i+start)%maxsamples;
        a=pcmL[j] = PCMdata[i];
        b=pcmR[j] = PCMdata[i];
        sum += abs(a) + abs(b);
    }
    start = (start+samples)%maxsamples;
    level = level*0.98 + ((double)sum/(samples*2))*0.02;
    newsamples += samples;
}


/* NOTE: this method expects total samples, not samples per channel */
void PCM::addPCMfloat_2ch(const float *PCMdata, size_t count)
{
    size_t samples = count/2;
    float a,b,sum = 0;
    for (size_t i=0; i<samples; i++)
    {
        size_t j=(start+i)%maxsamples;
        a = pcmL[j] = PCMdata[i*2];
        b = pcmR[j] = PCMdata[i*2+1];
        sum += abs(a) + abs(b);
    }
    start = (start + samples) % maxsamples;
    level = level*0.98 + ((double)sum/(samples*2))*0.02;
    newsamples += samples/2;
}


void PCM::addPCM16Data(const short* pcm_data, size_t samples)
{
    float a,b,sum = 0;
    for (size_t i = 0; i < samples; ++i)
    {
       size_t j = (i+start) % maxsamples;
       a=pcmL[j]=(pcm_data[i * 2 + 0]/16384.0);
       b=pcmR[j]=(pcm_data[i * 2 + 1]/16384.0);
       sum += abs(a) + abs(b);
    }
    start = (start + samples) % maxsamples;
    level = level*0.98 + ((double)sum/(samples*2))*0.02;
    newsamples += samples;
}


void PCM::addPCM16(const short PCMdata[2][512])
{
    const int samples=512;
    float a,b,sum = 0;
    for (size_t i=0;i<samples;i++)
    {
		size_t j=(i+start) % maxsamples;
        a=pcmL[j]=(PCMdata[0][i]/16384.0);
        b=pcmR[j]=(PCMdata[1][i]/16384.0);
        sum += abs(a) + abs(b);
    }
	start = (start+samples) % maxsamples;
    level = level*0.98 + ((double)sum/(samples*2))*0.02;
    newsamples += samples;
}


void PCM::addPCM8(const unsigned char PCMdata[2][1024])
{
    const int samples=1024;
    float a,b,sum = 0;
    for (size_t i=0; i<samples; i++)
    {
        size_t j= (i+start) % maxsamples;
        a=pcmL[j]=(((float)PCMdata[0][i] - 128.0) / 64 );
        b=pcmR[j]=(((float)PCMdata[1][i] - 128.0) / 64 );
        sum += abs(a) + abs(b);
    }
    start = (start + samples) % maxsamples;
    level = level*0.98 + (sum/(samples*2))*0.02;
    newsamples += samples;
}


void PCM::addPCM8_512(const unsigned char PCMdata[2][512])
{
    const size_t samples=512;
    float a,b,sum = 0;
    for (size_t i=0; i<samples; i++)
    {
        size_t j = (i+start) % maxsamples;
        a=pcmL[j]=(((float)PCMdata[0][i] - 128.0 ) / 64 );
        b=pcmR[j]=(((float)PCMdata[1][i] - 128.0 ) / 64 );
        sum += abs(a) + abs(b);
    }
    start = (start + samples) % maxsamples;
    level = level*0.98 + (sum/(samples*2))*0.02;
    newsamples += samples;
}


unsigned int getTicks( struct timeval *start );
unsigned int pcmstart = 0;
uint32_t pcmcount = 0;
double pcmps = 0.0;


// puts sound data requested at provided pointer
//
// samples is number of PCM samples to return
// smoothing is the smoothing coefficient
// returned values are normalized from -1 to 1

void PCM::getPCM(float *data, int channel, size_t samples, float smoothing)
{
    assert(channel == 0 || channel == 1);

    if (0==smoothing)
    {
        _copyPCM(data, channel, samples, true);
        return;
    }

    // since we've already got the freq data laying around, let's use that for smoothing
    _updateFFT();

    // copy
    double freq[FFT_LENGTH*2];
    double *from = channel==0 ? freqL : freqR;
    for (int i=0 ; i<FFT_LENGTH*2 ; i++)
        freq[i] = from[i];

    // apply gaussian filter
    // precompute constant:
    //   as smoothing->1 k->-inf, so freq->0 (more smoothing)
    //   also precompute constant part of (i/FFT_LENGTH)^2
    // The visible effects ramp up as you smoothing gets close to 1.0 (consistent with milkdrop2)
    double k=-1.0/((1-smoothing)*(1-smoothing)*FFT_LENGTH*FFT_LENGTH);
    for (int i=1 ; i<FFT_LENGTH ; i++)
    {
        float g = pow(2,i*i*k-1);
        freq[i*2]   *= g;
        freq[i*2+1] *= g;
    }
    freq[1] *= pow(2,k);

    // inverse fft
    rdft(FFT_LENGTH*2, -1, freq, ip, w);
    for (size_t j = 0; j < FFT_LENGTH*2; j++)
        freq[j] *= 1.0 / FFT_LENGTH;

    // copy out with zero-padding if necessary
    size_t count = samples<FFT_LENGTH ? samples : FFT_LENGTH;
    for (size_t i=0 ; i<count ; i++)
        data[i] = freq[i%(FFT_LENGTH*2)];
    for (size_t i=count ; i<samples ; i++)
        data[i] = 0;
}

void PCM::getSpectrum(float *data, int channel, size_t samples)
{
    assert(channel == 0 || channel == 1);
    _updateFFT();

    // compute magnitudes from R and I value
    float *spectrum = channel==0 ? spectrumL : spectrumR;
    // lets just ignore the first pair
    size_t count = samples <= FFT_LENGTH ? samples : FFT_LENGTH;
    for (size_t i=0 ; i<count ; i++)
        data[i] = spectrum[i];
    for (size_t i=count ; i<samples ; i++)
        data[0] = 0;
}

void PCM::_updateFFT()
{
    if (newsamples > 0)
    {
        _updateFFT(0);
        _updateFFT(1);
        newsamples = 0;
    }
}

void PCM::_updateFFT(size_t channel)
{
    assert(channel == 0 || channel == 1);

    double *freq = channel==0 ? freqL : freqR;
    _copyPCM(freq, channel, FFT_LENGTH*2, true);
    rdft(FFT_LENGTH*2, 1, freq, ip, w);

    float *spectrum = channel==0 ? spectrumL : spectrumR;
    // compute magnitude data (magnitude^2 actually)
    for (int i=1 ; i<FFT_LENGTH ; i++)
        spectrum[i-1] = (freq[i*2]*freq[i*2]+freq[i*2+1]*freq[i*2+1]);
    spectrum[FFT_LENGTH-1] = (freq[1]*freq[1]);
}

inline double constrain(double a, double mn, double mx)
{
    return a>mx ? mx : a<mn ? mn : a;
}

// pull data from circular buffer
void PCM::_copyPCM(float *to, int channel, size_t count, bool adjust_level)
{
    assert(channel == 0 || channel == 1);
    assert(count < maxsamples);
    const float *from = channel==0 ? pcmL : pcmR;
    const double volume = adjust_level ?  5 / (level<0.01?0.01:level) : 1.0;
    for (size_t i=0, pos=start ; i<count ; i++)
    {
        if (pos==0)
            pos = maxsamples;
        to[i] = from[--pos] * volume;
    }
}

void PCM::_copyPCM(double *to, int channel, size_t count, bool adjust_level)
{
    assert(channel == 0 || channel == 1);
    const float *from = channel==0 ? pcmL : pcmR;
    const double volume = adjust_level ? 5 / (level<0.01?0.01:level) : 1.0;
    for (size_t i=0, pos=start ; i<count ; i++)
    {
        if (pos==0)
            pos = maxsamples;
        to[i] = from[--pos] * volume;
    }
}

//Free stuff
void PCM::freePCM()
{
  free(ip);
  free(w);
  ip = NULL;
  w = NULL;
}



// TESTS


#include <TestRunner.hpp>

#ifndef NDEBUG

#include <PresetLoader.hpp>

#define TEST(cond) if (!verify(#cond,cond)) return false
#define TEST2(str,cond) if (!verify(str,cond)) return false

struct PCMTest : public Test
{
	PCMTest() : Test("PCMTest")
	{}

	bool eq(float a, float b)
	{
		return fabs(a-b) < (fabs(a)+fabs(b) + 1)/1000.0f;
	}

public:

    /* smoke test for each addPCM method */
	bool test_addpcm()
	{
	    PCM pcm;

        // mono float
        {
            const size_t samples = 301;
            float *data = new float[samples];
            for (size_t i = 0; i < samples; i++)
                data[i] = ((float) i) / (samples - 1);
            for (size_t i = 0; i < 10; i++)
                pcm.addPCMfloat(data, samples);
            float *copy = new float[samples];
            pcm._copyPCM(copy, 0, samples, false);
            for (size_t i = 0; i < samples; i++)
                TEST(eq(copy[i],((float)samples - 1 - i) / (samples - 1)));
            pcm._copyPCM(copy, 1, samples, false);
            for (size_t i = 0; i < samples; i++)
                TEST(eq(copy[i], ((float)samples - 1 - i) / (samples - 1)));
            free(data);
            free(copy);
        }

        // float_2ch
        {
            const size_t samples = 301;
            float *data = new float[samples*2];
            for (size_t i = 0; i < samples; i++)
            {
                data[i*2] = ((float) i) / (samples - 1);
                data[i*2+1] = 1.0-data[i*2] ;
            }
            for (size_t i = 0; i < 10; i++)
                pcm.addPCMfloat_2ch(data, samples*2);
            float *copy0 = new float[samples];
            float *copy1 = new float[samples];
            pcm._copyPCM(copy0, 0, samples, false);
            pcm._copyPCM(copy1, 1, samples, false);
            for (size_t i = 0; i < samples; i++)
                TEST(eq(1.0,copy0[i]+copy1[i]));
            free(data);
            free(copy0);
            free(copy1);
        }

//        void PCM::addPCM16Data(const short* pcm_data, size_t samples)
//        void PCM::addPCM16(const short PCMdata[2][512])
//        void PCM::addPCM8(const unsigned char PCMdata[2][1024])
//        void PCM::addPCM8_512(const unsigned char PCMdata[2][512])

        return true;
	}

	bool test_fft()
    {
        PCM pcm;

        // low frequency
        {
            const size_t samples = 1024;
            float *data = new float[samples * 2];
            for (size_t i = 0; i < samples; i++)
            {
                float f = 2 * M_PI * ((double) i) / (samples - 1);
                data[i * 2] = sin(f);
                data[i * 2 + 1] = sin(f + 1.0); // out of phase
            }
            pcm.addPCMfloat_2ch(data, samples * 2);
            pcm.addPCMfloat_2ch(data, samples * 2);
            float *freq0 = new float[FFT_LENGTH];
            float *freq1 = new float[FFT_LENGTH];
            pcm.level = 1.0;
            pcm.getSpectrum(freq0, 0, FFT_LENGTH);
            pcm.getSpectrum(freq1, 0, FFT_LENGTH);
            // freq0 and freq1 should be equal
            for (size_t i = 0; i < FFT_LENGTH; i++)
                TEST(eq(freq0[i], freq1[i]));
            TEST(freq0[0] > 100000);
            TEST(freq0[1] < 20);
            TEST(freq0[2] < 4);
            TEST(freq0[3] < 2);
            TEST(freq0[4] < 2);
            for (size_t i = 5; i < 20; i++)
                freq0[i] < 1;
            for (size_t i = 20; i < FFT_LENGTH; i++)
                freq0[i] < 0.1;
            free(data);
            free(freq0);
            free(freq1);
        }

        // high frequency
        {
            const size_t samples = 2;
            float data[4] = {1.0,0.0,0.0,1.0};
            for (size_t i = 0; i < 1024; i++)
                pcm.addPCMfloat_2ch(data, samples * 2);
            float *freq0 = new float[FFT_LENGTH];
            float *freq1 = new float[FFT_LENGTH];
            pcm.level = 1.0;
            pcm.getSpectrum(freq0, 0, FFT_LENGTH);
            pcm.getSpectrum(freq1, 0, FFT_LENGTH);
            // freq0 and freq1 should be equal
            for (size_t i = 0; i < FFT_LENGTH; i++)
                TEST(eq(freq0[i], freq1[i]));
            for (size_t i=0 ; i<FFT_LENGTH-1 ; i++)
                TEST(0==freq0[i]);
            TEST(freq0[FFT_LENGTH-1] > 6000000);
            free(freq0);
            free(freq1);
        }


        return true;
    }

	bool test() override
	{
		TEST(test_addpcm());
		TEST(test_fft());
		return true;
	}
};

Test* PCM::test()
{
	return new PCMTest();
}

#else

Test* PCM::test()
{
    return nullptr;
}

#endif
