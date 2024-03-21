#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <fmt/ostream.h>
#include <map>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <tuple>

#include "common.hpp"
#include "settings.hpp"

extern "C" {
#include <curl/curl.h>
}

/* TODO: We might want to move some of the curl code to a cpp file. That might be beneficial for
 * build times, but for the moment the biggest culprit is json code anyway, so doesn't make that
 * much of a difference */

inline std::ostream &operator<<(std::ostream &ostream, curl_infotype info) {
    switch (info) {
        case curl_infotype::CURLINFO_DATA_IN:
            return ostream << "DATA_IN";
        case curl_infotype::CURLINFO_DATA_OUT:
            return ostream << "DATA_OUT";
        case curl_infotype::CURLINFO_SSL_DATA_IN:
            return ostream << "SSL_DATA_IN";
        case curl_infotype::CURLINFO_SSL_DATA_OUT:
            return ostream << "SSL_DATA_OUT";
        case curl_infotype::CURLINFO_END:
            return ostream << "END";
        case curl_infotype::CURLINFO_HEADER_IN:
            return ostream << "HEADER_IN";
        case curl_infotype::CURLINFO_HEADER_OUT:
            return ostream << "HEADER_OUT";
        case curl_infotype::CURLINFO_TEXT:
            return ostream << "TEXT";
    }
    return ostream << "(unknown curl_infotype)";
}
template <> struct fmt::formatter<curl_infotype> : ostream_formatter {};

/** Type safe, printable, helper container for POST parameters */
struct PostParams final : public std::map<std::string, std::string> {
    using map::map;

  private:
    friend std::ostream &operator<<(std::ostream &ostream, const PostParams &self) {
        std::for_each(self.begin(), self.end(), [&](const auto &kv_pair) {
            ostream << kv_pair.first << "=" << kv_pair.second << "\n";
        });
        return ostream;
    }
};
template <> struct fmt::formatter<PostParams> : ostream_formatter {};

/** Type safe, printable, helper container for Query parameters */
struct QueryParams final : public std::map<std::string, std::string> {
    using map::map;

  private:
    friend std::ostream &operator<<(std::ostream &ostream, const QueryParams &self) {
        std::for_each(self.begin(), self.end(), [&](const auto &kv_pair) {
            ostream << kv_pair.first << "=" << kv_pair.second << "\n";
        });
        return ostream;
    }
};
template <> struct fmt::formatter<QueryParams> : ostream_formatter {};

/**
 * Container for header parameters
 *
 * As the string parameters in the linked list `slist` need to stay in a predictable place we need
 * to start creating all the strings beforehand, and make sure the don't move once we created them.
 * This is why all logic is in the constructor
 */
struct HeaderParams final {
    HeaderParams() = default;
    HeaderParams(const HeaderParams &) = delete;
    HeaderParams(HeaderParams &&) = delete;
    HeaderParams &operator=(const HeaderParams &) = delete;
    HeaderParams &operator=(HeaderParams &&) = delete;
    HeaderParams(std::initializer_list<std::pair<std::string_view, std::string_view>> params) {
        /* Create header line strings, and then store strings in linked list usable by curl */
        std::ranges::transform(params, std::back_inserter(lines),
                               [](const auto &kv_pair) -> std::string {
                                   return fmt::format("{}: {}", kv_pair.first, kv_pair.second);
                               });
        std::ranges::for_each(
            lines, [&](const auto &line) { list = curl_slist_append(list, line.c_str()); });
    }

    explicit operator const curl_slist *() const {
        return list;
    }

    ~HeaderParams() {
        curl_slist_free_all(list);
    }

  private:
    std::vector<std::string> lines{};
    curl_slist *list{};
    friend std::ostream &operator<<(std::ostream &ostream, const HeaderParams &self) {
        std::ranges::for_each(self.lines, [&](const auto &line) { ostream << line << "\n"; });
        return ostream;
    }
};
template <> struct fmt::formatter<HeaderParams> : ostream_formatter {};

struct Url {
    Url() = default;
    Url(const std::string &addr) {
        /* TODO: Maybe it would be conveient to take parameters as well in constructor */
        curl_url_set(url, CURLUPART_URL, addr.c_str(), 0);
    }
    Url(CURLU *url) : url{url} {
    }
    Url(const Url &) = delete;
    Url(Url &&) = delete;
    Url &operator=(const Url &) = delete;
    Url &operator=(Url &&) = delete;

    ~Url() {
        curl_url_cleanup(url);
    }

    auto add_param(const std::string &param) -> void {
        curl_url_set(url, CURLUPART_QUERY, param.c_str(), CURLU_APPENDQUERY | CURLU_URLENCODE);
    }

    auto add_param(std::pair<std::string_view, std::string_view> key_value) -> void {
        std::string param{fmt::format("{}={}", key_value.first, key_value.second)};
        add_param(param);
    }

    auto add_params(const QueryParams &params) -> void {
        std::for_each(params.begin(), params.end(), [this](auto kvp) { add_param(kvp); });
    }

    explicit operator const CURLU *() const {
        return url;
    }

  private:
    CURLU *url{curl_url()};

    friend std::ostream &operator<<(std::ostream &ostream, const Url &url) {
        char *address{};
        CURLUcode ret{curl_url_get(url.url, CURLUPART_URL, &address, 0)};
        if (ret != 0) {
            throw std::runtime_error("Unable to print address");
        }
        return ostream << address;
    }
};
template <> struct fmt::formatter<Url> : ostream_formatter {};

struct Curl {
    Curl() = delete;
    Curl(const Curl &) = delete;
    Curl(Curl &&) = delete;
    Curl &operator=(const Curl &) = delete;
    Curl &operator=(Curl &&) = delete;
    Curl(const Settings &settings)
        : settings{settings}
        , debug{settings.get_debug_logger(Logger::Facility::Curl)}
        , curl{curl_easy_init()} {
        if (curl == nullptr) {
            throw std::runtime_error("Error initializing cURL");
        }
    }

    ~Curl() {
        curl_easy_cleanup(curl);
    }

    auto postfields(const Url &address, const PostParams &params)
        -> std::pair<bool, std::span<std::byte>> {
        debug("Posting fields to  {}", address);
        std::string postdata{url_encode(params)};
        debug("Postdata=\n{}", postdata);
        CurlStderr curl_stderr{};
        boilerplate(address, curl_stderr);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postdata.c_str());
        CURLcode result = curl_easy_perform(curl);
        if (result != CURLE_OK) {
            debug("Error posting data {} ... {}", errbuf.data(), curl_easy_strerror(result));
        }
        debug("Curl output:\n{}", curl_stderr);
        return {result == CURLE_OK, response_data};
    }

    auto mimepost(const Url &address, const PostParams &params)
        -> std::pair<bool, std::span<std::byte>> {
        debug("Posting MIME to {}", address);
        Mime mime{settings, curl};
        std::for_each(params.begin(), params.end(), [&mime](auto kvp) { mime.add(kvp); });

        CurlStderr curl_stderr{};
        boilerplate(address, curl_stderr);
        curl_easy_setopt(curl, CURLOPT_MIMEPOST, static_cast<const curl_mime *>(mime));
        CURLcode result = curl_easy_perform(curl);
        if (result != CURLE_OK) {
            debug("Error posting data {} ... {}", errbuf.data(), curl_easy_strerror(result));
        }
        debug("Curl output:\n{}", curl_stderr);
        return {result == CURLE_OK, response_data};
    }

    auto get(const Url &url, const HeaderParams &params = {})
        -> std::pair<bool, std::span<std::byte>> {
        CurlStderr curl_stderr{};
        debug("Setting up HTTP GET from {}, with parameters:\n{}", url, params);

        boilerplate(url, curl_stderr);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, static_cast<const curl_slist *>(params));

        CURLcode result = curl_easy_perform(curl);
        debug("Curl output:\n{}", curl_stderr);
        return {result == CURLE_OK, response_data};
    }

  private:
    /**
     * Helper class for creating a FILE* stream backed by memory for being used as stderr for Curl
     */
    struct CurlStderr {
        CurlStderr() = default;
        CurlStderr(const CurlStderr &) = delete;
        CurlStderr(CurlStderr &&) = delete;
        CurlStderr &operator=(const CurlStderr &) = delete;
        CurlStderr &operator=(CurlStderr &&) = delete;

        // NOLINTBEGIN: Nasty C stuff, I think know what I'm doing
        auto open() -> FILE * {
            return stream = open_memstream(&memory, &size);
        }

        explicit operator FILE *() const {
            return stream;
        }

        ~CurlStderr() {
            if (stream != nullptr) {
                fclose(stream);
            }
            if (memory != nullptr) {
                free(memory);
            }
        }
        // NOLINTEND

      private:
        char *memory{};
        size_t size{};
        FILE *stream{};
        friend std::ostream &operator<<(std::ostream &ostream, const CurlStderr &self) {
            fflush(self.stream);
            return ostream << self.memory;
        }
    };

    /**
     * Helper container for curl mimepost
     */
    struct Mime {
        Mime() = delete;
        Mime(const Mime &) = delete;
        Mime(Mime &&) = delete;
        Mime &operator=(const Mime &) = delete;
        Mime &operator=(Mime &&) = delete;
        Mime(const Settings &settings, CURL *curl)
            : mime{curl_mime_init(curl)}
            , debug{settings.get_debug_logger(Logger::Facility::CurlMime)} {
        }

        ~Mime() {
            curl_mime_free(mime);
        }

        auto add(std::pair<std::string, std::string_view> kv_pair) const -> void {
            auto [key, value] = kv_pair;
            debug("Adding {}: {}", key, value);
            curl_mimepart *part = curl_mime_addpart(mime);
            curl_mime_name(part, key.c_str());
            curl_mime_data(part, value.begin(), value.size());
        }

        explicit operator const curl_mime *() const {
            return mime;
        }

      private:
        curl_mime *mime{};
        Logger debug;
    };

    auto url_encode(const std::string &str) const -> std::string {
        // NOLINTBEGIN: Nasty C stuff
        char *url_encoded{curl_easy_escape(curl, str.c_str(), str.size())};
        std::string result{url_encoded};
        curl_free(url_encoded);
        return result;
        // NOLINTEND
    }

    auto url_encode(const std::map<std::string, std::string> &params) const -> std::string {
        std::stringstream stringstream{};
        for (auto iter = params.begin(); iter != params.end(); ++iter) {
            if (iter != params.begin()) {
                stringstream << "&";
            }
            stringstream << iter->first << "=" << url_encode(iter->second);
        }
        return stringstream.str();
    }

    /**
     * Shared curl setup code
     */
    inline auto boilerplate(const Url &url, CurlStderr &curl_stderr) -> void {
        response_data.clear();
        errbuf[0] = '\0';
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, not settings.debug_log);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, settings.debug_log);
        if (settings.debug_log) {
            FILE *stream{curl_stderr.open()};
            curl_easy_setopt(curl, CURLOPT_STDERR, stream);
        }
        if (settings.dump_traffic) {
            curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, trace_cb);
            curl_easy_setopt(curl, CURLOPT_DEBUGDATA, this);
        }
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);
        curl_easy_setopt(curl, CURLOPT_CURLU, static_cast<const CURLU *>(url));
    }

    // NOLINTBEGIN: C API
    /**
     * Simply convert C API to calling object interface
     */
    static auto write_cb(void *data, size_t size, size_t nmemb, void *userp) -> size_t {
        assert(size == 1);
        return static_cast<Curl *>(userp)->write({static_cast<std::byte *>(data), nmemb});
    }
    // NOLINTEND

    auto write(std::span<std::byte> data) -> size_t {
        debug("Read data:\n{}", utils::dump_bytes_as_string(data));
        std::copy(data.begin(), data.end(), std::back_inserter(response_data));
        return data.size();
    }

    // NOLINTBEGIN: C API
    /**
     * Simply convert C API to calling object interface
     */
    static auto trace_cb(CURL * /*handle*/, curl_infotype type, char *data, size_t size,
                         void *userp) -> int {
        return static_cast<Curl *>(userp)->trace(type, {data, size});
    }
    // NOLINTEND

    auto trace(curl_infotype type, std::span<char> data) -> int {
        switch (type) {
            case curl_infotype::CURLINFO_DATA_IN:
            case curl_infotype::CURLINFO_DATA_OUT:
            case curl_infotype::CURLINFO_HEADER_IN:
            case curl_infotype::CURLINFO_HEADER_OUT:
                debug("Trace: {}", type);
                utils::dump_bytes(data);
                [[fallthrough]];
            default:
                /* We are not really interested in SSL/TLS debug data */
                return 0;
        }
    }

    Settings settings;
    Logger debug;
    CURL *curl{};
    std::array<char, CURL_ERROR_SIZE> errbuf{};
    std::vector<std::byte> response_data{};
};

template <> struct fmt::formatter<Curl::CurlStderr> : ostream_formatter {};
