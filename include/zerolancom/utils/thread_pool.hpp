#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "zerolancom/utils/logger.hpp"
#include "zerolancom/utils/singleton.hpp"
namespace zlc
{

/**
 * @brief A simple, thread-safe thread pool for executing tasks concurrently.
 *
 * Usage:
 *   ThreadPool pool(4);  // Create 4 worker threads
 *   pool.start();
 *   pool.enqueue([]() { std::cout << "Task 1\n"; });
 *   pool.enqueue([]() { std::cout << "Task 2\n"; });
 *   pool.wait();  // Wait for all tasks to complete
 *   pool.stop();
 *
 * Features:
 * - Header-only, no compilation overhead
 * - RAII: automatic cleanup on destruction
 * - Thread-safe task enqueueing
 * - Optional task result handling via callbacks
 */
class ThreadPool : public Singleton<ThreadPool>
{
public:
  using Task = std::function<void()>;

  /**
   * @brief Create a thread pool with specified number of worker threads.
   *
   * @param num_threads Number of worker threads. If 0, uses
   * std::thread::hardware_concurrency()
   */
  explicit ThreadPool(size_t num_threads = 0)
      : num_threads_(num_threads > 0 ? num_threads
                                     : std::thread::hardware_concurrency()),
        is_running_(false)
  {
    if (num_threads_ == 0)
    {
      num_threads_ = 1; // Fallback if hardware_concurrency() returns 0
    }
  }

  /**
   * @brief Destructor: stops the pool and waits for all threads.
   */
  ~ThreadPool()
  {
    stop();
  }

  /**
   * @brief Start the worker threads.
   */
  void start()
  {
    if (is_running_)
    {
      return; // Already running
    }

    is_running_ = true;
    workers_.clear();
    workers_.reserve(num_threads_);

    for (size_t i = 0; i < num_threads_; ++i)
    {
      workers_.emplace_back([this]() { workerLoop(); });
    }
  }

  /**
   * @brief Stop the pool and wait for all threads to exit.
   *
   * Clears remaining tasks and joins all worker threads.
   */
  void stop()
  {
    if (!is_running_)
    {
      return;
    }

    {
      std::unique_lock<std::mutex> lock(mutex_);
      is_running_ = false;
    }

    cv_.notify_all();

    for (auto &worker : workers_)
    {
      if (worker.joinable())
      {
        worker.join();
      }
    }

    workers_.clear();
  }

  /**
   * @brief Enqueue a task to be executed by a worker thread.
   *
   * @param task A callable (lambda, function pointer, std::function)
   */
  void enqueue(Task task)
  {
    if (!is_running_)
    {
      zlc::warn("[ThreadPool] Attempted to enqueue task on stopped pool");
      return;
    }

    {
      std::unique_lock<std::mutex> lock(mutex_);
      tasks_.push(std::move(task));
    }

    cv_.notify_one();
  }

  /**
   * @brief Wait for all currently queued tasks to complete.
   *
   * Note: This only waits for tasks that were already queued.
   * Tasks enqueued after this call are not waited for.
   */
  void wait()
  {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_done_.wait(lock, [this]() { return tasks_.empty() && active_tasks_ == 0; });
  }

  /**
   * @brief Get the number of worker threads.
   */
  size_t size() const
  {
    return num_threads_;
  }

  /**
   * @brief Check if the pool is running.
   */
  bool is_running() const
  {
    return is_running_;
  }

  /**
   * @brief Get the number of pending tasks.
   */
  size_t pending_tasks() const
  {
    std::unique_lock<std::mutex> lock(mutex_);
    return tasks_.size();
  }

  // Non-copyable
  ThreadPool(const ThreadPool &) = delete;
  ThreadPool &operator=(const ThreadPool &) = delete;

  // Movable
  ThreadPool(ThreadPool &&) = default;
  ThreadPool &operator=(ThreadPool &&) = default;

private:
  /**
   * @brief Main loop for worker threads.
   */
  void workerLoop()
  {
    while (true)
    {
      Task task;

      {
        std::unique_lock<std::mutex> lock(mutex_);

        // Wait until there's a task or the pool is stopped
        cv_.wait(lock, [this]() { return !tasks_.empty() || !is_running_; });

        // If pool is stopped and no more tasks, exit
        if (!is_running_ && tasks_.empty())
        {
          break;
        }

        // If there are no tasks (spurious wakeup), continue waiting
        if (tasks_.empty())
        {
          continue;
        }

        task = std::move(tasks_.front());
        tasks_.pop();
        active_tasks_++;
      }

      // Execute task outside the lock
      try
      {
        if (task)
        {
          task();
        }
      }
      catch (const std::exception &e)
      {
        zlc::error("[ThreadPool] Exception in task: {}", e.what());
      }
      catch (...)
      {
        zlc::error("[ThreadPool] Unknown exception in task");
      }

      {
        std::unique_lock<std::mutex> lock(mutex_);
        active_tasks_--;
      }

      cv_done_.notify_one();
    }
  }

private:
  size_t num_threads_;
  bool is_running_;

  std::vector<std::thread> workers_;
  std::queue<Task> tasks_;
  size_t active_tasks_{0};

  mutable std::mutex mutex_;
  std::condition_variable cv_;      // Notify workers of new tasks
  std::condition_variable cv_done_; // Notify wait() of completion
};

} // namespace zlc
