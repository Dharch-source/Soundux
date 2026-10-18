#pragma once
#include <json.hpp>
#include <optional>
#include <regex>
#include <string>

// tiny-process-library keys its string type off UNICODE, which this project
// defines globally. The library is built with the narrow char interface, so
// this TU has to match that. misc.cpp already guards its own include the
// same way; only headers leak the wide type into other translation units.
#pragma push_macro("UNICODE")
#undef UNICODE
#include <process.hpp>
#pragma pop_macro("UNICODE")

namespace Soundux
{
    namespace Objects
    {
        class YoutubeDl
        {
            bool isAvailable = false;
            static const std::regex urlRegex;
            std::optional<TinyProcessLib::Process> currentDownload;

          public:
            void setup();
            void killDownload();
            bool available() const;
            bool download(const std::string &);
            std::optional<nlohmann::json> getInfo(const std::string &) const;
        };
    } // namespace Objects
} // namespace Soundux