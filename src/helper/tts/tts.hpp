#pragma once
#if defined(_WIN32)
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <httplib.h>
#include <optional>
#include <string>
#include <vector>

namespace Soundux::Objects
{
    struct TtsResult
    {
        bool ok = false;
        std::string path;   // path to the downloaded mp3 (absolute)
        std::string error;
        std::size_t bytes = 0;
    };

    /**
     * Text-to-speech: fetches an MP3 from the unofficial Google translate_tts
     * endpoint and writes it to the Soundux tts cache. The caller then plays the
     * file through the normal Audio::play() pipeline, so it uses the same
     * routing as a regular sound (speakers + virtual mic passthrough).
     */
    class Tts
    {
        static constexpr std::size_t kMaxCharsPerChunk = 200;   // hard limit of the endpoint
        static constexpr int kTimeoutSec = 20;

      public:
        /** Downloads text->mp3 into cacheDir. Returns ok=false with a short error
         *  message on any failure (network, bad payload, empty text). */
        static TtsResult speak(const std::string &text,
                               const std::string &lang,
                               double rate,
                               const std::filesystem::path &cacheDir);
    };
}
#endif
