#include "ThreadPool.h"

ThreadPool::ThreadPool(std::size_t n)
{
    for (std::size_t i = 0; i < n; i++)
    {
        workers_.emplace_back([this, i]()
        {
            while (true)
            {
                std::function<void()> task;

                {
                    std::unique_lock<std::mutex> lock(mtx_);

                    cv_.wait(lock, [this] {
                        return stop_ || !tasks_.empty();
                    });

                    if (stop_ && tasks_.empty())
                    {
                        return;
                    }

                    task = tasks_.front();
                    tasks_.pop();
                }

                task();
            }
        });
    }
}

void ThreadPool::submit(std::function<void()> task)
{
    std::unique_lock<std::mutex> lock(mtx_);

    if (stop_)
    {
        return;
    }

    tasks_.push(task);
    cv_.notify_one();
}

ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(mtx_);
        stop_ = true;
    }

    cv_.notify_all();

    for (auto& worker : workers_)
    {
        worker.join();
    }
}
