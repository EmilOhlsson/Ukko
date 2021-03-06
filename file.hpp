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
        fd = ::open(filename.data(), flags);
        if (fd < 0) {
            throw std::runtime_error(
                fmt::format("Error opening {}: {}", filename, strerror(errno)));
        }
        this->filename = filename;
    }

    ~File() {
        close(fd);
    }

    template <typename T> void write(std::span<T> data) {
        ssize_t result = ::write(fd, data.data(), sizeof(T) * data.size());
        if (result < 0) {
            throw std::runtime_error(fmt::format("Error writing: {}", strerror(errno)));
        }
    }

    void write(std::string_view data) {
        write(std::span(data));
    }

    operator int() const {
        return fd;
    }

  private:
    int fd;
    std::string filename;
};
