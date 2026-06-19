/******************************************************************************/
/* ThreadPool.hpp                                                             */
/******************************************************************************/
/*                            This file is part of                            */
/*                                SF Game Engine                              */
/******************************************************************************/
/* MIT License                                                                */
/*                                                                            */
/* Copyright (c) 2025-present Martin (the name I was assigned in french class).                                         */
/*                                                                            */
/* May all those that this source may reach be blessed by the LORD and find   */
/* peace and joy in life.                                                     */
/* Everyone who drinks of this water will be thirsty again; but whoever       */
/* drinks of the water that I will give him shall never thirst; John 4:13-14  */
/*                                                                            */
/* Permission is hereby granted, free of charge, to any person obtaining a    */
/* copy of this software and associated documentation files (the "Software"), */
/* to deal in the Software without restriction, including without limitation  */
/* the rights to use, copy, modify, merge, publish, distribute, sublicense,   */
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
/* OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                              */
/******************************************************************************/
#pragma once

#include <thread>
#include <vector>
#include <functional>
#include <mutex>
#include <queue>
#include <future>

namespace SF::Engine
{
    /**
     * @brief A fixed size pool of threads.
     */
    class ThreadPool
    {
    public:
        explicit ThreadPool(uint32_t threadCount = std::thread::hardware_concurrency());
        ~ThreadPool();

        template <typename F, typename... Args>
        auto Enqueue(F &&f, Args &&...args);

        void Wait();

        const std::vector<std::thread> &GetWorkers() const { return workers; }

    private:
        std::vector<std::thread> workers;
        std::queue<std::function<void()>> tasks;

        std::mutex queueMutex;
        std::condition_variable condition;
        bool stop = false;
    };

    template <typename F, typename... Args>
    auto ThreadPool::Enqueue(F &&f, Args &&...args)
    {
        using return_type = typename std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<return_type()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        auto result = task->get_future();

        {
            std::unique_lock<std::mutex> lock(queueMutex);

            if (stop)
                throw std::runtime_error("Enqueue called on a stopped ThreadPool");

            tasks.emplace([task]()
                          { (*task)(); });
        }

        condition.notify_one();
        return result;
    }
    template <typename F, typename... Args>
    auto RetardNextThreadPoolEnqueue(F &&f, Args &&...args)
    {
        static ThreadPool pool;
        return pool.Enqueue(std::forward<F>(f), std::forward<Args>(args)...);
    }
}