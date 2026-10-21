/******************************************************************************/
/* ThreadPool.hpp                                                             */
/******************************************************************************/
/*                            This file is part of                            */
/*                                SF Game Engine                              */
/******************************************************************************/
/* MIT License                                                                */
/*                                                                            */
/* Copyright (c) 2025-present Noah Lee                                        */
/*                                                                            */
/* May all those that this source may reach be blessed by the LORD and find   */
/* peace and joy in life.                                                     */
/* Everyone who drinks of this water will be thirsty again; but whoever       */
/* drinks of the water that I will give him shall never thirst; John 4:13-14  */
/*                                                                            */
/* Permission is hereby granted, free of charge, to any person obtaining a    */
/* copy of this software and associated documentation files (the "Software"), */
/* to deal in the Software without restriction, including without limitation */
/* the rights to use, copy, modify, merge, publish, distribute, sublicense,  */
/* and/or sell copies of the Software, and to permit persons to whom the      */
/* Software is furnished to do so, subject to the following conditions:       */
/*                                                                            */
/* The above copyright notice and this permission notice shall be included in */
/* all copies or substantial portions of the Software.                        */
/*                                                                            */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS    */
/* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF                 */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.     */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY       */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT  */
/* OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE      */
/*                                                                            */
/******************************************************************************/
#pragma once

#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

namespace SF::Engine
{
    using namespace std;
    inline namespace Threads
    {
        [[nodiscard]] inline uint32_t
        CalculateOptimalNumWorkerThreads(float ratio = 1.0f, uint32_t minThreads = 1,
                                         uint32_t maxThreads      = thread::hardware_concurrency(),
                                         uint32_t reservedThreads = 1)
        {
            if (maxThreads == 0)
                maxThreads = 1;

            const uint32_t availableThreads = maxThreads - min(maxThreads, reservedThreads);

            return max(minThreads, static_cast<uint32_t>(lround(clamp(ratio, 0.0f, 1.0f) * availableThreads)));
        }
    } // namespace Threads

    /**
     * @brief A fixed-size pool of worker threads.
     *
     * Enqueue() submits a task and returns a future containing its result.
     *
     * Dispatch() submits a fire-and-forget task.
     *
     * Wait() blocks until all currently queued and executing tasks have
     * completed.
     */
    class ThreadPool
    {
    public:
        explicit ThreadPool(uint32_t threadCount = CalculateOptimalNumWorkerThreads());

        ~ThreadPool();

        ThreadPool(const ThreadPool &)            = delete;
        ThreadPool &operator=(const ThreadPool &) = delete;

        ThreadPool(ThreadPool &&)            = delete;
        ThreadPool &operator=(ThreadPool &&) = delete;

        /**
         * @brief Enqueue a task and return a future for its result.
         */
        template<typename F, typename... Args>
        [[nodiscard]]
        auto Enqueue(F &&f, Args &&...args);

        /**
         * @brief Enqueue a task without requiring a result.
         */
        template<typename F, typename... Args>
        void Dispatch(F &&f, Args &&...args);

        /**
         * @brief Wait until all queued and currently executing tasks finish.
         */
        void Wait();

        /**
         * @brief Get the worker threads.
         */
        [[nodiscard]]
        const vector<thread> &GetWorkers() const noexcept
        {
            return workers;
        }

        /**
         * @brief Get the number of worker threads.
         */
        [[nodiscard]]
        uint32_t GetThreadCount() const noexcept
        {
            return static_cast<uint32_t>(workers.size());
        }

    private:
        vector<thread> workers;
        queue<function<void()>> tasks;

        mutex queueMutex;

        condition_variable condition;
        condition_variable completionCondition;

        uint32_t activeTasks = 0;
        bool stop            = false;
    };

    template<typename F, typename... Args>
    auto ThreadPool::Enqueue(F &&f, Args &&...args)
    {
        using ReturnType = invoke_result_t<F, Args...>;

        auto task = make_shared<packaged_task<ReturnType()>>(bind(forward<F>(f), forward<Args>(args)...));

        auto result = task->get_future();

        {
            unique_lock<mutex> lock(queueMutex);

            if (stop)
                throw runtime_error("Enqueue called on a stopped ThreadPool");

            tasks.emplace([task]() { (*task)(); });
        }

        condition.notify_one();

        return result;
    }

    template<typename F, typename... Args>
    void ThreadPool::Dispatch(F &&f, Args &&...args)
    {
        {
            unique_lock<mutex> lock(queueMutex);

            if (stop)
                throw runtime_error("Dispatch called on a stopped ThreadPool");

            tasks.emplace(bind(forward<F>(f), forward<Args>(args)...));
        }

        condition.notify_one();
    }
} // namespace SF::Engine
