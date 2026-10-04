#include "ThreadPool.h"
#include <iostream>
#include <pthread.h>
#include <ctime>

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

    // ★ 不用无条件的 join()：
    //   如果某个 worker 卡住了（比如卡在第三方库的阻塞调用里 —— 我们已经
    //   在 RedisClient 上踩过一次），join() 会永远等不到，导致【进程永远
    //   退不出】，连 SIGTERM 都杀不掉，只能 kill -9。
    //   所以用带超时的 pthread_timedjoin_np：超时后放弃等待并 detach。
    for (auto& worker : workers_)
    {
        if (!worker.joinable())
        {
            continue;
        }

        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 3;                         // 最多等 3 秒

        int rc = pthread_timedjoin_np(worker.native_handle(), nullptr, &ts);

        if (rc != 0)
        {
            std::cerr << "[WARN] worker 线程超时未退出，放弃等待" << std::endl;
        }

        // ★ 无论 pthread_timedjoin_np 成功与否，都必须 detach：
        //   pthread 层面 join 成功了，但 std::thread 对象并不知道，
        //   它析构时会发现自己 joinable() 仍为 true → 调用 std::terminate()。
        worker.detach();
    }
}
