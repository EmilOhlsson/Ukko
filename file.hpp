#pragma once

#include <fmt/core.h>

#include <fcntl.h>
#include <poll.h>
#include <span>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/**
 * Representing a file with an accesible file descriptor
 */
struct File {
    File(std::string_view filename, int flags) {
        fmt::print("Opening {}\n", filename);
        // TODO: It might be that we want to use some flags here
        // TODO: Adding a delay here seem to solve problems...
        fd = ::open(filename.data(), flags);
        if (fd < 0) {
            throw std::runtime_error(
                fmt::format("Error opening {}: {}", filename, strerror(errno)));
        }
        this->filename = filename;
    }

    ~File() {
        fmt::print("Closing {}\n", filename);
        close(fd);
    }

    template <typename T> void write(std::span<T> data) {
        fmt::print("GPIO: Writing {} to {}\n", data, filename);
        ssize_t result = ::write(fd, data.data(), sizeof(T) * data.size());
        if (result < 0) {
            // TODO it might be that we need some gracious handling here
            throw std::runtime_error(fmt::format("Error writing: {}", strerror(errno)));
        }
    }

    void write(std::string_view data) { write(std::span(data)); }

    operator int() const { return fd; }

  private:
    int fd;
    std::string filename;
};
