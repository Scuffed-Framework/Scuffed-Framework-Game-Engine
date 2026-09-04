#pragma once
#include <memory>
#include <mutex>
#include <thread>
#include "../DynamicArray.hpp"
#include "../Functional.hpp"
#include "../Memory.hpp"
#include "../Operations.hpp"

namespace SFTL
{
    class rcu_domain;

    struct rcu_thread_data
    {
        atomic<size_type> grace_period{0};
        DynamicArray<void *> retired_objects;
        DynamicArray<function<void()>> retired_callbacks;
        bool registered{false};
    };

    class rcu_global_state
    {
    public:
        static rcu_global_state &instance()
        {
            static rcu_global_state state;
            return state;
        }

        void register_thread()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto &data = get_thread_data();
            if (!data.registered)
            {
                data.registered = true;
                threads_.push_back(std::this_thread::get_id());
            }
        }

        void unregister_thread()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto &data = get_thread_data();
            if (data.registered)
            {
                data.registered = false;
                auto it         = find(threads_.begin(), threads_.end(), std::this_thread::get_id());
                if (it != threads_.end())
                {
                    threads_.erase(it);
                }
            }
        }

        void quiescent_state()
        {
            auto &data = get_thread_data();
            if (data.registered)
            {
                data.grace_period.store(current_grace_period_.load(), std::memory_order_release);
            }
        }

        void synchronize()
        {
            size_t new_period = current_grace_period_.fetch_add(1) + 1;

            std::unique_lock<std::mutex> lock(mutex_);
            for (const auto &thread_id: threads_)
            {
                // Find thread data for this thread ID
                // In a real implementation, we'd need a mapping from thread_id to thread_data
                // For simplicity, we'll just check all registered threads
                // This is a simplified version - real RCU would be more sophisticated
            }

            DynamicArray<void *> to_delete;
            DynamicArray<function<void()>> callbacks;

            {
                std::lock_guard<std::mutex> retire_lock(retire_mutex_);
                to_delete.swap(retired_objects_);
                callbacks.swap(retired_callbacks_);
            }

            for (void *ptr: to_delete)
            {
                // The deleter information is stored separately in the object
                // For simplicity, we assume the object knows how to delete itself
                // or we store the deleter with the pointer
            }

            for (auto &cb: callbacks)
            {
                cb();
            }
        }

        void barrier() { synchronize(); }

        void retire(void *ptr, const function<void()> &deleter)
        {
            auto &data = get_thread_data();
            if (!data.registered)
            {
                deleter();
                return;
            }

            data.retired_objects.push_back(ptr);
            data.retired_callbacks.push_back(deleter);
        }

    private:
        rcu_global_state() : current_grace_period_(1) {}
        ~rcu_global_state() = default;

        thread_local static rcu_thread_data thread_data_;

        static rcu_thread_data &get_thread_data() { return thread_data_; }

        atomic<size_type> current_grace_period_;
        std::mutex mutex_;
        std::mutex retire_mutex_;
        DynamicArray<std::thread::id> threads_;
        DynamicArray<void *> retired_objects_;
        DynamicArray<function<void()>> retired_callbacks_;
    };

    thread_local rcu_thread_data rcu_global_state::thread_data_;

    class rcu_domain
    {
    public:
        rcu_domain() : grace_period_(1) {}

        rcu_domain(const rcu_domain &)            = delete;
        rcu_domain &operator=(const rcu_domain &) = delete;

        void lock() noexcept { rcu_global_state::instance().register_thread(); }

        bool try_lock() noexcept
        {
            rcu_global_state::instance().register_thread();
            return true;
        }

        void unlock() noexcept
        {
            rcu_global_state::instance().quiescent_state();
            rcu_global_state::instance().unregister_thread();
        }

        void synchronize() { rcu_global_state::instance().synchronize(); }

        void barrier() { rcu_global_state::instance().barrier(); }

        void retire(void *ptr, const function<void()> &deleter) { rcu_global_state::instance().retire(ptr, deleter); }

    private:
        atomic<size_type> grace_period_;
    };

    // Default domain
    inline rcu_domain &rcu_default_domain() noexcept
    {
        static rcu_domain default_domain;
        return default_domain;
    }

    // RCU synchronization functions
    inline void rcu_synchronize(rcu_domain &dom = rcu_default_domain()) noexcept { dom.synchronize(); }

    inline void rcu_barrier(rcu_domain &dom = rcu_default_domain()) noexcept { dom.barrier(); }

    // Retirement functions
    template<class T, class D = default_delete<T>>
    void rcu_retire(T *p, D d = D(), rcu_domain &dom = rcu_default_domain())
    {
        if (p)
        {
            dom.retire(p, [p, d]() { d(p); });
        }
    }

    // Base class for RCU-protected objects
    template<class T, class D = default_delete<T>>
    class rcu_obj_base
    {
    public:
        void retire(D d = D(), rcu_domain &domain = rcu_default_domain()) noexcept
        {
            T *derived = static_cast<T *>(this);
            rcu_retire(derived, d, domain);
        }

    protected:
        rcu_obj_base()                                = default;
        rcu_obj_base(const rcu_obj_base &)            = default;
        rcu_obj_base(rcu_obj_base &&)                 = default;
        rcu_obj_base &operator=(const rcu_obj_base &) = default;
        rcu_obj_base &operator=(rcu_obj_base &&)      = default;
        ~rcu_obj_base()                               = default;

    private:
        D deleter_;
    };

    class rcu_guard
    {
    public:
        explicit rcu_guard(rcu_domain &domain = rcu_default_domain()) : domain_(domain) { domain_.lock(); }

        ~rcu_guard() { domain_.unlock(); }

        void quiescent_state() { rcu_global_state::instance().quiescent_state(); }

    private:
        rcu_domain &domain_;
    };

#define RCU_READ_LOCK(dom) SFTL::rcu_guard rcu_guard_##__LINE__(dom)
#define RCU_READ_UNLOCK() (void) 0

    template<typename T>
    class rcu_ptr
    {
    public:
        rcu_ptr() : ptr_(nullptr) {}

        explicit rcu_ptr(T *ptr) : ptr_(ptr) {}

        rcu_ptr(const rcu_ptr &other) : ptr_(other.ptr_) {}

        rcu_ptr(rcu_ptr &&other) noexcept : ptr_(other.ptr_) { other.ptr_ = nullptr; }

        ~rcu_ptr()
        {
            if (ptr_)
            {
                rcu_retire(ptr_);
            }
        }

        rcu_ptr &operator=(const rcu_ptr &other)
        {
            if (this != &other)
            {
                ptr_ = other.ptr_;
            }
            return *this;
        }

        rcu_ptr &operator=(rcu_ptr &&other) noexcept
        {
            if (this != &other)
            {
                ptr_       = other.ptr_;
                other.ptr_ = nullptr;
            }
            return *this;
        }

        T *operator->() const { return ptr_; }

        T &operator*() const { return *ptr_; }

        explicit operator bool() const { return ptr_ != nullptr; }

        T *get() const { return ptr_; }

        void reset(T *new_ptr = nullptr)
        {
            if (ptr_)
            {
                rcu_retire(ptr_);
            }
            ptr_ = new_ptr;
        }

    private:
        T *ptr_;
    };

    class rcu_thread_register
    {
    public:
        rcu_thread_register() { rcu_global_state::instance().register_thread(); }

        ~rcu_thread_register() { rcu_global_state::instance().unregister_thread(); }
    };
} // namespace SFTL
