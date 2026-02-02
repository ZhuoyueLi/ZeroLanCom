#pragma once

#include <future>
#include <thread>

#include "zerolancom/utils/thread_pool.hpp"

namespace zlc
{

/**
 * @brief Helper for running periodic tasks in a thread pool.
 *
 * Usage:
 *   PeriodicTask periodic([](){ std::cout << "Running\n"; }, 1000);  // 1000ms
 *   periodic.start();
 *   ...
 *   periodic.stop();
 *
 * This is useful for:
 * - Heartbeat threads (like multicast sender)
 * - Polling loops (like subscriber manager)
 * - Periodic health checks
 */
class PeriodicTask
{
public:
  using Callback = std::function<void()>;

  /**
   * @brief Create a periodic task.
   *
   * @param callback Function to call periodically
   * @param interval_ms Interval in milliseconds between executions
   * @param pool Thread pool to use for execution
   */
  PeriodicTask(Callback callback, int interval_ms, ThreadPool &pool)
      : callback_(std::move(callback)), interval_ms_(interval_ms), pool_(&pool),
        is_running_(false)
  {
  }

  /**
   * @brief Start the periodic task.
   */
  void start()
  {
    if (is_running_)
    {
      return;
    }

    is_running_ = true;

    // Use thread pool to enqueue the loop
    pool_->enqueue([this]() { runLoop(); });
  }

  /**
   * @brief Stop the periodic task and wait for it to finish.
   *
   * This method blocks until the task thread actually stops executing.
   * Uses std::future to guarantee synchronization.
   */
  void stop()
  {
    if (!is_running_)
    {
      return;
    }

    is_running_ = false;

    // Wait for the task to actually finish (with timeout of 5 seconds)
    if (task_done_.valid())
    {
      auto status = task_done_.wait_for(std::chrono::seconds(5));
      if (status == std::future_status::timeout)
      {
        zlc::warn("[PeriodicTask] Task did not finish within timeout period");
      }
    }
  }

  /**
   * @brief Destructor: automatically stops the task.
   */
  ~PeriodicTask()
  {
    stop();
  }

  bool is_running() const
  {
    return is_running_;
  }

private:
  void runLoop()
  {
    while (is_running_)
    {
      try
      {
        callback_();
      }
      catch (const std::exception &e)
      {
        zlc::error("[PeriodicTask] Exception: {}", e.what());
      }
      catch (...)
      {
        zlc::error("[PeriodicTask] Unknown exception");
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms_));
    }

    // Signal that the task has finished
    promise_done_.set_value();
  }

private:
  Callback callback_;
  int interval_ms_;
  ThreadPool *pool_;
  std::atomic<bool> is_running_;

  // Use std::promise/future for clean one-time synchronization
  std::promise<void> promise_done_;
  std::shared_future<void> task_done_{promise_done_.get_future()};
};

} // namespace zlc