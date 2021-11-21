#include <curl/curl.h>
#include <exception>
#include <fmt/core.h>

void do_curl_stuff() {
    CURL *curl = curl_easy_init();
    if (!curl)
        throw std::runtime_error("Could not initialize curl");

    char errbuf[CURL_ERROR_SIZE];

    FILE *forecast_file = fopen("forecast.json", "w");
    curl_easy_setopt(
        curl, CURLOPT_URL,
        "https://opendata-download-metobs.smhi.se/api/version/latest/parameter/4.json");
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    // curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, forecast_file);
    int err = curl_easy_perform(curl);
    if (err)
        fmt::print("Got cURL error: {}", errbuf);

    curl_easy_cleanup(curl);
}
