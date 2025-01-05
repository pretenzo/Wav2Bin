//
//  main.cpp
//  Wav2Bin
//
//  Created by Lorenzo Bachman on 2024-12-31.
//

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cmath>
#include <cstdint>
#include <filesystem>

// Constants
constexpr int CD_SECTOR_SIZE = 2352;
constexpr int CD_FRAMES_PER_SECOND = 75;
constexpr int CD_BYTES_PER_FRAME = CD_SECTOR_SIZE;

// WAV File Header
struct WAVHeader {
    char chunkID[4];       // "RIFF"
    uint32_t chunkSize;    // File size - 8 bytes
    char format[4];        // "WAVE"
    char subchunk1ID[4];   // "fmt "
    uint32_t subchunk1Size;// Size of fmt chunk (16 for PCM)
    uint16_t audioFormat;  // Audio format (1 = PCM)
    uint16_t numChannels;  // Number of channels
    uint32_t sampleRate;   // Sample rate (44100 for CD audio)
    uint32_t byteRate;     // (Sample Rate * BitsPerSample * Channels) / 8
    uint16_t blockAlign;   // (BitsPerSample * Channels) / 8
    uint16_t bitsPerSample;// Bits per sample (16 for CD audio)
    char subchunk2ID[4];   // "data"
    uint32_t subchunk2Size;// Size of data chunk
};

struct TrackInfo {
    std::string title;
    uint32_t offsetFrames; // Offset in frames for the track
};

void writeCUEFile(const std::string& cueFilePath, const std::string& binFileName, const std::vector<TrackInfo>& tracks) {
    std::ofstream cueFile(cueFilePath);
    if (!cueFile) {
        throw std::runtime_error("Failed to open CUE file for writing.");
    }

    cueFile << "FILE \"" << binFileName << "\" BINARY\n";
    for (size_t i = 0; i < tracks.size(); ++i) {
        const auto& track = tracks[i];
        cueFile << "  TRACK " << (i + 1) << " AUDIO\n";
        cueFile << "    TITLE \"" << track.title << "\"\n";
        cueFile << "    INDEX 01 "
                << std::setw(2) << std::setfill('0') << (track.offsetFrames / (CD_FRAMES_PER_SECOND * 60)) << ":"
                << std::setw(2) << std::setfill('0') << ((track.offsetFrames / CD_FRAMES_PER_SECOND) % 60) << ":"
                << std::setw(2) << std::setfill('0') << (track.offsetFrames % CD_FRAMES_PER_SECOND) << "\n";
    }
    cueFile.close();
}

void convertWAVToBIN(const std::vector<std::string>& wavFilePaths, const std::string& binFilePath, std::vector<TrackInfo>& tracks) {
    std::ofstream binFile(binFilePath, std::ios::binary);
    if (!binFile) {
        throw std::runtime_error("Failed to open BIN file for writing.");
    }

    uint32_t currentOffsetFrames = 0;
    for (const auto& wavFilePath : wavFilePaths) {
        // Open WAV file
        std::ifstream wavFile(wavFilePath, std::ios::binary);
        if (!wavFile) {
            throw std::runtime_error("Failed to open WAV file: " + wavFilePath);
        }

        // Read WAV header
        WAVHeader header;
        wavFile.read(reinterpret_cast<char*>(&header), sizeof(WAVHeader));

        // Validate WAV file format
        if (std::string(header.chunkID, 4) != "RIFF" || std::string(header.format, 4) != "WAVE" || header.audioFormat != 1) {
            throw std::runtime_error("Invalid or unsupported WAV file format: " + wavFilePath);
        }

        // Calculate total data size and align to sector size
        uint32_t rawAudioSize = header.subchunk2Size;
        uint32_t alignedSize = std::ceil(static_cast<double>(rawAudioSize) / CD_SECTOR_SIZE) * CD_SECTOR_SIZE;

        // Read audio data into buffer
        std::vector<char> audioData(alignedSize, 0); // Zero-padded buffer
        wavFile.read(audioData.data(), rawAudioSize);
        wavFile.close();

        // Write audio data to BIN file
        binFile.write(audioData.data(), alignedSize);

        // Add track information
        TrackInfo track;
        track.title = std::filesystem::path(wavFilePath).stem().string();
        track.offsetFrames = currentOffsetFrames;
        tracks.push_back(track);

        // Update current offset in frames
        currentOffsetFrames += alignedSize / CD_BYTES_PER_FRAME;
    }

    binFile.close();
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cout << "Wav2Bin (c) 2024 Lorenzo Bachman\n";
        std::cerr << "Usage: " << argv[0] << " <output BIN file> <output CUE file> <input WAV file(s)>\n";
        return 1;
    }

    try {
        const std::string binFilePath = argv[1];
        const std::string cueFilePath = argv[2];
        std::vector<std::string> wavFilePaths;

        for (int i = 3; i < argc; ++i) {
            wavFilePaths.push_back(argv[i]);
        }

        std::vector<TrackInfo> tracks;
        convertWAVToBIN(wavFilePaths, binFilePath, tracks);
        writeCUEFile(cueFilePath, binFilePath, tracks);

        std::cout << "Conversion completed successfully.\n";
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
