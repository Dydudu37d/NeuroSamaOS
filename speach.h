#pragma once

#include "int.h"
#include "math.h"
#include "kmalloc.h"
#include "debug.h"
#include "port.h"

#define SAMPLE_RATE 44100
#define AMPLITUDE 32767
#define PI 3.14159265359

extern AllocPool KernelPool;

typedef struct {
    s16* data;
    u64 length;
    u64 sampleRate;
} AudioBuffer;

static u64 SampleCount = 0;
static s16* AudioOutput = 0;
static u64 MaxSamples = 0;

static s16 GenerateWhiteNoise(u64 seed) {
    seed = seed * 1103515245 + 12345;
    s16 noise = (s16)((seed >> 16) & 0x7FFF);
    return noise * 0.3;
}

static s16 GenerateSine(u64 sample, u64 freq, u64 sampleRate) {
    double t = (double)sample / (double)sampleRate;
    return (s16)(AMPLITUDE * 0.5 * DoubleSin(2.0 * PI * freq * t));
}

static s16 GenerateVowel(char vowel, u64 sample, u64 sampleRate) {
    s16 result = 0;
    double t = (double)sample / (double)sampleRate;
    
    switch(vowel) {
        case 'a':
            result = (s16)(AMPLITUDE * 0.5 * (DoubleSin(2.0 * PI * 150 * t) + DoubleSin(2.0 * PI * 250 * t) + DoubleSin(2.0 * PI * 350 * t)));
            break;
        case 'i':
            result = (s16)(AMPLITUDE * 0.5 * (DoubleSin(2.0 * PI * 220 * t) + DoubleSin(2.0 * PI * 330 * t) + DoubleSin(2.0 * PI * 440 * t)));
            break;
        case 'u':
            result = (s16)(AMPLITUDE * 0.5 * (DoubleSin(2.0 * PI * 180 * t) + DoubleSin(2.0 * PI * 270 * t) + DoubleSin(2.0 * PI * 380 * t)));
            break;
        case 'e':
            result = (s16)(AMPLITUDE * 0.5 * (DoubleSin(2.0 * PI * 200 * t) + DoubleSin(2.0 * PI * 300 * t) + DoubleSin(2.0 * PI * 400 * t)));
            break;
        case 'o':
            result = (s16)(AMPLITUDE * 0.5 * (DoubleSin(2.0 * PI * 140 * t) + DoubleSin(2.0 * PI * 240 * t) + DoubleSin(2.0 * PI * 340 * t)));
            break;
        default:
            result = (s16)(AMPLITUDE * 0.3 * DoubleSin(2.0 * PI * 200 * t));
            break;
    }
    return result;
}

static s16 GenerateConsonant(char consonant, u64 sample, u64 sampleRate, u64* noiseSeed) {
    u64 duration = sampleRate / 20;
    double decay = 1.0 - ((double)(sample % duration) / (double)duration);
    
    switch(consonant) {
        case 's':
            return (s16)(GenerateWhiteNoise(*noiseSeed + sample) * decay);
        case 'n':
            return (s16)(GenerateSine(sample, 180, sampleRate) * 0.3 * decay);
        case 'r':
            return (s16)(GenerateSine(sample, 220, sampleRate) * 0.2 * (DoubleSin(2.0 * PI * 30 * (double)sample / (double)sampleRate) * 0.5 + 0.5));
        case 'k':
            return (s16)(GenerateWhiteNoise(*noiseSeed + sample * 2) * decay * 0.8);
        case 't':
            return (s16)(GenerateWhiteNoise(*noiseSeed + sample * 3) * decay * 0.6);
        case 'p':
            return (s16)(GenerateWhiteNoise(*noiseSeed + sample * 4) * decay * 0.7);
        case 'm':
            return (s16)(GenerateSine(sample, 160, sampleRate) * 0.4 * decay);
        case 'g':
            return (s16)(GenerateWhiteNoise(*noiseSeed + sample * 5) * decay * 0.5);
        case 'd':
            return (s16)(GenerateWhiteNoise(*noiseSeed + sample * 6) * decay * 0.5);
        case 'b':
            return (s16)(GenerateWhiteNoise(*noiseSeed + sample * 7) * decay * 0.6);
        case 'z':
            return (s16)(GenerateWhiteNoise(*noiseSeed + sample * 8) * decay * 0.4);
        case 'h':
            return (s16)(GenerateWhiteNoise(*noiseSeed + sample * 9) * decay * 0.7);
        case 'y':
            return (s16)(GenerateSine(sample, 190, sampleRate) * 0.3 * decay);
        case 'w':
            return (s16)(GenerateSine(sample, 170, sampleRate) * 0.3 * decay);
        default:
            return 0;
    }
}

static u16* Speach(char Consonant, char Vowel, u64 Ms) {
    u64 sampleRate = SAMPLE_RATE;
    u64 totalSamples = (sampleRate * Ms) / 1000;
    u64 noiseSeed = 0x12345678;
    
    s16* buffer = Alloc(&KernelPool, totalSamples * sizeof(s16));
    if (!buffer) return 0;
    
    u64 consonantSamples = sampleRate / 10;
    if (consonantSamples > totalSamples / 2) {
        consonantSamples = totalSamples / 2;
    }
    
    u64 vowelSamples = totalSamples - consonantSamples;
    
    for (u64 i = 0; i < totalSamples; i++) {
        s16 sample = 0;
        
        if (i < consonantSamples) {
            sample = GenerateConsonant(Consonant, i, sampleRate, &noiseSeed);
        } else if (i < consonantSamples + vowelSamples) {
            u64 vowelIndex = i - consonantSamples;
            sample = GenerateVowel(Vowel, vowelIndex, sampleRate);
            
            double envelope = 1.0;
            if (vowelIndex < sampleRate / 50) {
                envelope = (double)vowelIndex / ((double)sampleRate / 50.0);
            }
            sample = (s16)(sample * envelope);
        } else {
            sample = 0;
        }
        
        buffer[i] = sample;
    }
    
    return buffer;
}

s16* SpeachWord(char* Word, u64 MsPerMora) {
    if (!Word) return 0;
    
    u64 totalSamples = 0;
    u64 len = StrLen(Word);
    
    u64* moraLengths = Alloc(&KernelPool, len * sizeof(u64));
    if (!moraLengths) return 0;
    
    for (u64 i = 0; i < len; i++) {
        moraLengths[i] = (MsPerMora * SAMPLE_RATE) / 1000;
        totalSamples += moraLengths[i];
        totalSamples += SAMPLE_RATE / 20;
    }
    
    s16* output = Alloc(&KernelPool, totalSamples * sizeof(s16));
    if (!output) {
        Free(&KernelPool, moraLengths);
        return 0;
    }
    
    u64 currentPos = 0;
    u64 noiseSeed = 0x12345678;
    
    for (u64 i = 0; i < len; i++) {
        char c = Word[i];
        char consonant = 0;
        char vowel = 0;
        u64 duration = moraLengths[i];
        
        if (c == 'n' || c == 'N') {
            consonant = 0;
            vowel = 'n';
        } else if (c == 's' || c == 'S') {
            consonant = 's';
            vowel = 'u';
        } else {
            consonant = c;
            vowel = 'a';
        }
        
        u64 consonantSamples = SAMPLE_RATE / 10;
        if (consonantSamples > duration / 2) {
            consonantSamples = duration / 2;
        }
        u64 vowelSamples = duration - consonantSamples;
        
        if (consonant) {
            for (u64 j = 0; j < consonantSamples; j++) {
                output[currentPos + j] = GenerateConsonant(consonant, j, SAMPLE_RATE, &noiseSeed);
            }
            currentPos += consonantSamples;
        }
        
        for (u64 j = 0; j < vowelSamples; j++) {
            s16 sample = GenerateVowel(vowel, j, SAMPLE_RATE);
            double envelope = 1.0;
            if (j < SAMPLE_RATE / 50) {
                envelope = (double)j / ((double)SAMPLE_RATE / 50.0);
            }
            if (j > vowelSamples - SAMPLE_RATE / 20) {
                envelope = (double)(vowelSamples - j) / ((double)SAMPLE_RATE / 20.0);
            }
            output[currentPos + j] = (s16)(sample * envelope);
        }
        currentPos += vowelSamples;
        
        u64 gapSamples = SAMPLE_RATE / 40;
        for (u64 j = 0; j < gapSamples && currentPos + j < totalSamples; j++) {
            output[currentPos + j] = 0;
        }
        currentPos += gapSamples;
    }
    
    Free(&KernelPool, moraLengths);
    return output;
}

void PlayAudio(s16* Audio, u64 Samples) {
    if (!Audio || Samples == 0) return;
    
    for (u64 i = 0; i < Samples; i++) {
        outb(0x220, (u8)(Audio[i] & 0xFF));
        outb(0x221, (u8)((Audio[i] >> 8) & 0xFF));
    }
}

void FreeAudio(s16* Audio) {
    if (Audio) {
        Free(&KernelPool, Audio);
    }
}