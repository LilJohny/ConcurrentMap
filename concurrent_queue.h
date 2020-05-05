#ifndef AKSLAB5_CONCURRENT_QUEUE_H
#define AKSLAB5_CONCURRENT_QUEUE_H

#include <condition_variable>
#include "iostream"
#include <deque>

template<typename T>
class concurrent_queue {
 public:
  explicit concurrent_queue(size_t max_size) : max_size(max_size) {}

  void push_back(T value) {
    std::unique_lock<std::mutex> lg{mutex};
    cv_not_full.wait(lg, [this]() { return queue.size() != max_size; });
    queue.push_back(value);
    cv_not_empty.notify_one();
  }
  void push_front(T value) {
    std::unique_lock<std::mutex> lg{mutex};
    cv_not_full.wait(lg, [this]() { return queue.size() != max_size; });
    queue.push_front(value);
    cv_not_empty.notify_one();
  }

  size_t size() {
    std::unique_lock<std::mutex> lock(mutex);
    return queue.size();
  }

  T pop() {
    std::unique_lock<std::mutex> lg{mutex};
    cv_not_empty.wait(lg, [this]() { return queue.size() != 0; });
    T value = queue.front();
    queue.pop_front();
    cv_not_full.notify_one();
    return value;
  }

  T front() {
    std::unique_lock<std::mutex> lg{mutex};
    cv_not_empty.wait(lg, [this]() { return queue.size() != 0; });
    T value = queue.front();
    return value;
  }

 private:
  std::deque<T> queue;
  mutable std::mutex mutex;
  size_t max_size;
  std::condition_variable cv_not_full, cv_not_empty;
};

#endif //AKSLAB5_CONCURRENT_QUEUE_H
