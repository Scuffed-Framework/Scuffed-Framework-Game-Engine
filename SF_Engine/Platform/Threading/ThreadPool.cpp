#include "ThreadPool.hpp"
namespace SF::Engine
{

    ThreadPool::ThreadPool(uint32_t threadCount)
    {
        // A pool with zero workers would accept jobs that can never execute.
        if (threadCount == 0)
            threadCount = 1;

        workers.reserve(threadCount);

        for (uint32_t i = 0; i < threadCount; ++i)
        {
            workers.emplace_back(
                    [this]
                    {
                        while (true)
                        {
                            function<void()> task;

                            {
                                unique_lock<mutex> lock(queueMutex);

                                condition.wait(lock, [this] { return stop || !tasks.empty(); });

                                // Finish processing queued work before exiting.
                                if (stop && tasks.empty())
                                    return;

                                task = std::move(tasks.front());
                                tasks.pop();

                                ++activeTasks;
                            }

                            // Do not hold queueMutex while executing user code.
                            task();

                            {
                                unique_lock<mutex> lock(queueMutex);

                                --activeTasks;

                                if (tasks.empty() && activeTasks == 0)
                                    completionCondition.notify_all();
                            }
                        }
                    });
        }
    }

    ThreadPool::~ThreadPool()
    {
        {
            unique_lock<mutex> lock(queueMutex);
            stop = true;
        }

        // Wake every worker so they can finish their remaining tasks.
        condition.notify_all();

        for (auto &worker: workers)
        {
            if (worker.joinable())
                worker.join();
        }
    }

    void ThreadPool::Wait()
    {
        unique_lock<mutex> lock(queueMutex);

        completionCondition.wait(lock, [this] { return tasks.empty() && activeTasks == 0; });
    }
} // namespace SF::Engine
