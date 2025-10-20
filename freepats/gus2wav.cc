// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0

// Extract samples from a GUS patch file and save them as WAV files.
// Based on: https://github.com/tim-janik/beast/blob/54fda788c98fda700de4defcbae1ff1dd55b64c7/bse/bseloader-guspatch.cc
// Permission to relicense: https://mail.gnome.org/archives/beast/2021-February/msg00001.html

#include <stdio.h>
#include <stdint.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>
#include <algorithm>

#pragma pack(push, 1)

// Structures for the GUS Patch file format, adapted from the provided code.

struct PatHeader {
  char id[12];                // ID='GF1PATCH110'
  char manufacturer_id[10];   // Manufacturer ID
  char description[60];       // Description
  uint8_t instruments;        // Number of instruments
  uint8_t voices;             // Number of voices for sample
  uint8_t channels;           // Number of output channels (1=mono, 2=stereo)
  uint16_t waveforms;         // Number of waveforms
  uint16_t mastervolume;      // Master volume
  uint32_t size;              // Size of the following data
  char reserved[36];          // reserved
};

struct PatInstrument {
  uint16_t number;
  char name[16];
  uint32_t size;              // Size of the whole instrument in bytes.
  uint8_t layers;
  char reserved1[40];
  uint16_t layerUnknown;
  uint32_t layerSize;
  uint8_t sampleCount;        // number of samples in this layer
  char layerReserved[40];
};

enum {
  PAT_FORMAT_16BIT = (1 << 0),
  PAT_FORMAT_UNSIGNED = (1 << 1),
  PAT_FORMAT_LOOPED = (1 << 2),
  PAT_FORMAT_LOOP_BIDI = (1 << 3),
  PAT_FORMAT_LOOP_BACKWARDS = (1 << 4),
  PAT_FORMAT_SUSTAIN = (1 << 5),
  PAT_FORMAT_ENVELOPE = (1 << 6),
  PAT_FORMAT_CLAMPED = (1 << 7)
};

struct PatPatch {
  char filename[7];
  uint8_t fractions;
  uint32_t wavesize;
  uint32_t loopStart;
  uint32_t loopEnd;
  uint16_t sampleRate;
  uint32_t minFreq;
  uint32_t maxFreq;
  uint32_t origFreq;
  int16_t fineTune;
  uint8_t balance;
  uint8_t filterRate[6];
  uint8_t filterOffset[6];
  uint8_t tremoloSweep;
  uint8_t tremoloRate;
  uint8_t tremoloDepth;
  uint8_t vibratoSweep;
  uint8_t vibratoRate;
  uint8_t vibratoDepth;
  uint8_t waveFormat;
  int16_t freqScale;
  uint16_t freqScaleFactor;
  char reserved[36];
};

// Structures for the WAV file format.

struct WavHeader {
  char riff[4];
  uint32_t chunkSize;
  char wave[4];
  char fmt[4];
  uint32_t subchunk1Size;
  uint16_t audioFormat;
  uint16_t numChannels;
  uint32_t sampleRate;
  uint32_t byteRate;
  uint16_t blockAlign;
  uint16_t bitsPerSample;
  char data[4];
  uint32_t subchunk2Size;
};

#pragma pack(pop)

// Helper function to safely create a string from a fixed-size, potentially non-null-terminated buffer.
static std::string
stringFromFixedBuffer (const char *buffer, size_t size)
{
  std::string str (buffer, size);
  size_t first_null = str.find ('\0');
  if (first_null != std::string::npos) {
    str.resize (first_null);
  }
  return str;
}

// Function to write a WAV file.
static void
writeWavFile (const std::string &filename, const PatPatch &patch, const std::vector<char> &sampleData)
{
  std::ofstream outFile (filename, std::ios::binary);
  if (!outFile) {
    std::cerr << "Error: Could not create WAV file " << filename << std::endl;
    return;
  }

  WavHeader wavHeader;
  strncpy (wavHeader.riff, "RIFF", 4);
  strncpy (wavHeader.wave, "WAVE", 4);
  strncpy (wavHeader.fmt, "fmt ", 4);
  strncpy (wavHeader.data, "data", 4);

  wavHeader.subchunk1Size = 16;
  wavHeader.audioFormat = 1; // PCM
  wavHeader.numChannels = 1; // GUS patches are mono
  wavHeader.sampleRate = patch.sampleRate;
  wavHeader.bitsPerSample = (patch.waveFormat & PAT_FORMAT_16BIT) ? 16 : 8;
  wavHeader.byteRate = wavHeader.sampleRate * wavHeader.numChannels * (wavHeader.bitsPerSample / 8);
  wavHeader.blockAlign = wavHeader.numChannels * (wavHeader.bitsPerSample / 8);
  wavHeader.subchunk2Size = sampleData.size();
  wavHeader.chunkSize = 36 + wavHeader.subchunk2Size;

  outFile.write (reinterpret_cast<const char *> (&wavHeader), sizeof (WavHeader));
  outFile.write (sampleData.data(), sampleData.size());

  std::cout << "Wrote: " << filename << std::endl;
}

static std::string
stripPath (const std::string &outputFilename)
{
  // Find the last occurrence of path separators
  size_t lastSlash = outputFilename.find_last_of ("/\\");
  if (lastSlash != std::string::npos) {
    return outputFilename.substr (lastSlash + 1);
  }
  return outputFilename; // No path separator found
}

int
main (int argc, char *argv[])
{
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <input_patch_file.pat>" << std::endl;
    return 1;
  }

  std::string fullPath (argv[1]);
  size_t lastSeparator = fullPath.find_last_of ("/\\");
  std::string filenameWithExt = (lastSeparator == std::string::npos) ? fullPath : fullPath.substr (lastSeparator + 1);
  size_t lastDot = filenameWithExt.rfind ('.');
  std::string patFileStem = (lastDot == std::string::npos) ? filenameWithExt : filenameWithExt.substr (0, lastDot);

  std::ifstream patFile (argv[1], std::ios::binary);
  if (!patFile) {
    std::cerr << "Error: Could not open file " << argv[1] << std::endl;
    return 1;
  }

  PatHeader header;
  patFile.read (reinterpret_cast<char *> (&header), sizeof (PatHeader));

  if (strncmp (header.id, "GF1PATCH", 8) != 0) {
    std::cerr << "Error: Not a valid GUS patch file." << std::endl;
    return 1;
  }

  std::cout << "GUS Patch File: " << stringFromFixedBuffer (header.description, 60) << std::endl;
  std::cout << "Number of instruments: " << static_cast<int> (header.instruments) << std::endl;

  for (int i = 0; i < header.instruments; ++i) {
    PatInstrument instrument;
    patFile.read (reinterpret_cast<char *> (&instrument), sizeof (PatInstrument));

    std::string instrumentName = stringFromFixedBuffer (instrument.name, 16);

    std::cout << "\nInstrument " << i << ": " << instrumentName << std::endl;
    std::cout << "  Number of samples: " << static_cast<int> (instrument.sampleCount) << std::endl;

    for (int j = 0; j < instrument.sampleCount; ++j) {
      PatPatch patch;
      patFile.read (reinterpret_cast<char *> (&patch), sizeof (PatPatch));

      if (patch.wavesize > 0) {
        std::vector<char> sampleData (patch.wavesize);
        patFile.read (sampleData.data(), patch.wavesize);

        // Handle 8-bit unsigned to signed conversion for WAV
        if (! (patch.waveFormat & PAT_FORMAT_16BIT) && (patch.waveFormat & PAT_FORMAT_UNSIGNED)) {
          for (size_t k = 0; k < sampleData.size(); ++k) {
            // convert unsigned 8-bit (0 to 255) to signed 8-bit (-128 to 127)
            sampleData[k] = static_cast<char> (static_cast<unsigned char> (sampleData[k]) - 128);
          }
        }

        std::string cleanInstrumentName = instrumentName;
        cleanInstrumentName.erase (cleanInstrumentName.find_last_not_of (" \t\n\r\f\v") + 1);

        // --- MODIFIED: Prepend the patch file's stem to the output filename ---
        std::string outputFilename = patFileStem + "_" + cleanInstrumentName + "_" + std::to_string (j) + ".wav";
        outputFilename = stripPath (outputFilename);
        writeWavFile (outputFilename, patch, sampleData);
      }
    }
  }

  return 0;
}
// g++ -std=gnu++23 -Wall -g -Og gus2wav.cc -o gus2wav && ./gus2wav.cc <file.pat>
