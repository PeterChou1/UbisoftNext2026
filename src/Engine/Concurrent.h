//---------------------------------------------------------------------------------
// Concurrent.h
//---------------------------------------------------------------------------------
//
// A parallel for each over the engine's worker threads (C++17 without
// std::execution::par)
//

#pragma once

#include <algorithm>
#include <condition_variable>
#include <functional>
#include <future>
#include <iterator>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

class ThreadPool
{
  public:
    explicit ThreadPool(size_t threads)
    {
        for (size_t i = 0; i < threads; ++i)
            m_Workers.emplace_back([this] { WorkerLoop(); });
    }

    ~ThreadPool()
    {
        {
            std::unique_lock<std::mutex> lock(m_QueueMutex);
            m_Stop = true;
        }
        m_Condition.notify_all();
        for (std::thread& worker : m_Workers)
            worker.join();
    }

    size_t Size() const { return m_Workers.size(); }

    template <class F>
    std::future<void> Enqueue(F&& f)
    {
        auto task = std::make_shared<std::packaged_task<void()>>(std::forward<F>(f));
        std::future<void> result = task->get_future();
        {
            std::unique_lock<std::mutex> lock(m_QueueMutex);
            if (m_Stop)
                throw std::runtime_error("enqueue on stopped ThreadPool");
            m_Tasks.emplace([task]() { (*task)(); });
        }
        m_Condition.notify_one();
        return result;
    }

  private:
    void WorkerLoop()
    {
        for (;;)
        {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(m_QueueMutex);
                m_Condition.wait(lock, [this] { return m_Stop || !m_Tasks.empty(); });
                if (m_Stop && m_Tasks.empty())
                    return;
                task = std::move(m_Tasks.front());
                m_Tasks.pop();
            }
            task();
        }
    }

    std::vector<std::thread> m_Workers;
    std::queue<std::function<void()>> m_Tasks;
    std::mutex m_QueueMutex;
    std::condition_variable m_Condition;
    bool m_Stop = false;
};

class Concurrent final
{
  public:
    /**
     * \brief std::for_each split in one contiguous chunk per worker thread,
     *        returns when every chunk is done
     */
    template <class It, class Fn>
    static void ForEach(It first, It last, Fn func)
    {
        ThreadPool& pool = GetThreadPool();

        auto totalElements = std::distance(first, last);
        auto chunkSize = totalElements / pool.Size();
        auto remainder = totalElements % pool.Size();

        std::vector<std::future<void>> futures;
        auto begin = first;
        for (size_t i = 0; i < pool.Size(); ++i)
        {
            auto end = std::next(begin, chunkSize + (remainder > 0 ? 1 : 0));
            if (remainder > 0)
                --remainder;
            futures.push_back(pool.Enqueue([=, &func]() { std::for_each(begin, end, func); }));
            begin = end;
        }

        for (auto& future : futures)
            future.get();
    }

  private:
    static ThreadPool& GetThreadPool()
    {
        static ThreadPool pool(std::thread::hardware_concurrency());
        return pool;
    }
};
