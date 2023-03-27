#pragma once
#include <pthread.h>
#include <sys/logger.h>
#include "thread_pool.h"

// @brief Thread-safe, no-manual destroy Singleton template
template <typename T>
class Singleton {
 public:
  // @brief Get the singleton instance
  static T* get() {
    pthread_once(&p_once_, &Singleton::new_);
    return instance_;
  }

 private:
  Singleton();
  ~Singleton();

  // @brief Construct the singleton instance
  static void new_() { instance_ = new T(); }

  // @brief Destruct the singleton instance
  // @note Only work with gcc/clang
  __attribute__((destructor)) static void delete_() {
    typedef char T_must_be_complete[sizeof(T) == 0 ? -1 : 1];
    (void)sizeof(T_must_be_complete);
    delete instance_;
  }

  static pthread_once_t p_once_;  // Initialization once control
  static T* instance_;            // The singleton instance
};

template <typename T>
pthread_once_t Singleton<T>::p_once_ = PTHREAD_ONCE_INIT;

template <typename T>
T* Singleton<T>::instance_ = NULL;

// put this in the private
#define MAKE_SINGLETON(Type) \
 private:                    \
  Type() {}                  \
  ~Type() {}                 \
  friend class Singleton<Type>

class PublicThreadPool {
 public:
  template <typename InputIter, typename F>
  void ForEach(InputIter begin, InputIter end, F f) {
    if (!pool_) {
      CRINFO << "[PublicThreadPool] Init PublicThreadPool";
      pool_.reset(new ThreadPool(10 /*max_planning_thread_pool_size*/));
    }
    std::vector<std::future<void>> futures;
    results_vec_.clear();
    for (auto iter = begin; iter != end; ++iter) {
      auto& elem = *iter;
      futures.emplace_back(pool_->enqueue(std::bind(f, std::ref(elem))));
    }
    for (auto& future : futures) {
      try {
        if (future.valid()) {
          future.get();
          results_vec_.emplace_back(true);
        } else {
          results_vec_.emplace_back(false);
          CRERROR << "Future is invalid.";
        }
      } catch (const std::future_error& ex) {
        results_vec_.emplace_back(false);
        CRERROR << "Caught a future_error with code \"" << ex.code() << "\"\nMessage: \"" << ex.what() << "\"\n";
        throw ex;
      }
    }
  }

  int Capacity() const { return capacity_; }

  const std::vector<bool>& GetResults() const { return results_vec_; }

 private:
  PublicThreadPool() = default;
  ~PublicThreadPool() = default;
  std::shared_ptr<ThreadPool> pool_ = nullptr;
  static const int capacity_;
  friend class Singleton<PublicThreadPool>;
  std::vector<bool> results_vec_;
};
