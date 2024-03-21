#pragma once

#include "settings.hpp"
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>

template <typename T> struct MessageQueue {
    MessageQueue(const Settings &settings)
        : settings{settings}, debug{settings.get_logger(Logger::Facility::MessageQueue)} {
    }

    /** Push item to queue */
    auto push(T item) -> void {
        std::unique_lock<std::mutex> lock{mutex};
        queue.emplace_back(item);
        cond.notify_one();
    }

    /** Wait for a value until timeout */
    auto pop(std::chrono::time_point<std::chrono::system_clock> timepoint) -> std::optional<T> {
        std::unique_lock<std::mutex> lock{mutex};
        if (queue.empty()) {
            if (cond.wait_until(lock, timepoint) == std::cv_status::timeout) {
                return std::nullopt;
            }
        }
        T item = queue.front();
        queue.pop_front();
        return item;
    }

  private:
    Settings settings;
    Logger debug;
    std::deque<T> queue{};
    std::mutex mutex{};
    std::condition_variable cond{};
};
