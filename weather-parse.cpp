#include "nlohmann/json.hpp"
#include <fmt/core.h>

using json = nlohmann::json;

void do_json_stuff() {
    char text[] = R"(
        "Image": {
            "Width":  800,
            "Height": 600,
            "Title":  "View from 15th Floor",
            "Thumbnail": {
                "Url":    "http://www.example.com/image/481989943",
                "Height": 125,
                "Width":  100
            },
            "Animated" : false,
            "IDs": [116, 943, 234, 38793]
        }
    }
    )";
    json j_complete = json::parse(text);
    fmt::print("Stuff: {}\n\n", j_complete);
}

