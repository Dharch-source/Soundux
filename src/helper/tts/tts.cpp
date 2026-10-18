#include "tts.hpp"
#if defined(_WIN32)

#include <cstddef>
#include <cstdlib>
#include <ctime>
#include <fancy.hpp>
#include <mutex>
#include <sstream>

namespace Soundux::Objects
{
    namespace
    {
        std::mutex gTtsMutex; // serialize downloads - the endpoint throttles bursts

        std::filesystem::path userCacheRoot()
        {
            if (const char *localAppData = std::getenv("LOCALAPPDATA");
                localAppData != nullptr && *localAppData != '\0')
            {
                return std::filesystem::path(localAppData) / "Soundux" / "tts";
            }
            if (const char *userProfile = std::getenv("USERPROFILE");
                userProfile != nullptr && *userProfile != '\0')
            {
                return std::filesystem::path(userProfile) / "AppData" / "Local" / "Soundux" / "tts";
            }
            return std::filesystem::temp_directory_path() / "soundux-tts";
        }

        std::vector<std::byte> fetchOne(const std::string &chunk, const std::string &lang, int rate)
        {
            httplib::Client client("https://translate.google.com");
            client.set_follow_location(true);
            client.set_connection_timeout(10, 0);
            client.set_read_timeout(20, 0);

            httplib::Params params{
                {"client", "tw-ob"},
                {"tl", lang},
                {"q", chunk}};
            if (rate != 100)
            {
                params.emplace("ttsspeed", std::to_string(rate / 100.0));
            }

            httplib::Headers headers{
                {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                               "(KHTML, like Gecko) Chrome/126.0.0.0 Safari/537.36"},
                {"Accept", "audio/mpeg, audio/*"},
                {"Referer", "https://translate.google.com/"},
            };

            auto res = client.Get("/translate_tts", params, headers);

            if (!res)
            {
                return {};
            }
            if (res->status != 200)
            {
                Fancy::fancy.logTime().warning() << "TTS http status " << res->status << std::endl;
                return {};
            }
            const auto &body = res->body;
            return std::vector<std::byte>(reinterpret_cast<const std::byte *>(body.data()),
                                         reinterpret_cast<const std::byte *>(body.data()) + body.size());
        }

        bool looksLikeAudio(const std::vector<std::byte> &data)
        {
            if (data.size() < 16)
            {
                return false;
            }
            // MP3 frames often start with ID3 or an 0xFF sync byte. Google sometimes
            // served an 11-byte RIFF mpeg.ts header - both decode fine.
            const bool id3 = data[0] == std::byte{0x49} && data[1] == std::byte{0x44} && data[2] == std::byte{0x33};
            const bool sync = data[0] == std::byte{0xFF} && (data[1] & std::byte{0xC0}) == std::byte{0xC0};
            const bool riff = (data.size() >= 3 && data[0] == std::byte{0x52} && data[1] == std::byte{0x49});
            return id3 || sync || riff;
        }
    } // namespace

    std::filesystem::path Tts::defaultCacheDir()
    {
        auto root = userCacheRoot();
        std::error_code ec;
        std::filesystem::create_directories(root, ec);
        return root;
    }

    TtsResult Tts::speak(const std::string &text,
                         const std::string &lang,
                         double rate,
                         const std::filesystem::path &cacheDir)
    {
        TtsResult result;
        std::error_code ec;

        std::filesystem::create_directories(cacheDir, ec);

        // split text into <kMaxCharsPerChunk> pieces (drive-by: chunks longer than the
        // endpoint's limit return an empty 200 which would play nothing)
        std::vector<std::string> chunks;
        {
            std::istringstream stream(text);
            std::vector<std::string> words;
            std::string word;
            while (stream >> word)
            {
                words.push_back(word);
            }
            std::string cur;
            for (const auto &w : words)
            {
                if (cur.size() + w.size() + 1 > kMaxCharsPerChunk)
                {
                    if (!cur.empty())
                    {
                        chunks.push_back(cur);
                    }
                    cur = w.size() > kMaxCharsPerChunk ? w.substr(0, kMaxCharsPerChunk) : w;
                }
                else
                {
                    cur += (cur.empty() ? "" : " ") + w;
                }
            }
            if (!cur.empty())
            {
                chunks.push_back(cur);
            }
        }
        if (chunks.empty())
        {
            result.error = "empty text";
            return result;
        }

        std::vector<std::byte> combined;
        const int rateI = static_cast<int>(rate);

        for (const auto &chunk : chunks)
        {
            auto data = fetchOne(chunk, lang, rateI);
            if (!looksLikeAudio(data))
            {
                Fancy::fancy.logTime().failure() << "TTS download failed for chunk (" << chunk.size()
                                                 << " chars) got " << data.size() << " bytes" << std::endl;
                result.error = "download failed";
                return result;
            }
            combined.insert(combined.end(), data.begin(), data.end());
        }

        // cache key: stable hash of text+lang+rate so repeats play instantly and
        // don't re-download
        std::size_t seed = 0;
        const auto probe = [&](const std::string &s) {
            for (unsigned char c : s)
            {
                seed ^= std::hash<unsigned char>{}(c) + 0x9E3779B9U + (seed << 6) + (seed >> 2);
            }
        };
        probe(text);
        probe(lang);
        probe(std::to_string(rateI));

        auto path = cacheDir / (std::to_string(seed) + ".mp3");
        std::ofstream file(path, std::ios::out | std::ios::binary | std::ios::trunc);
        file.write(reinterpret_cast<const char *>(combined.data()),
                   static_cast<std::streamsize>(combined.size()));
        file.close();

        if (!file || ec)
        {
            result.error = "write failed";
            return result;
        }

        result.ok = true;
        result.path = path.string();
        result.bytes = combined.size();
        return result;
    }
} // namespace Soundux::Objects
#endif // _WIN32
