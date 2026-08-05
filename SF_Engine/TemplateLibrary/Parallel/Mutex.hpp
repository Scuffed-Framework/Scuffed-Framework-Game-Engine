/*
#include <yvals_core.h>
#include <__msvc_chrono.hpp>
#include <cstdlib>
#include <system_error>
#include <thread>
#include <utility>
#include <xcall_once.h>

using namespace std;
class _Mutex_base
{
public:
    constexpr _Mutex_base(int _Flags = 0) noexcept
    {
        _Mtx_storage._Critical_section = {};
        _Mtx_storage._Thread_id = -1;
        _Mtx_storage.Typepe = _Flags | _Mtx_try;
        _Mtx_storage._Count = 0;
    }

    _Mutex_base(const _Mutex_base &) = delete;
    _Mutex_base &operator=(const _Mutex_base &) = delete;

    void lock()
    {
        if (_Mtx_lock(_Mymtx()) != _Thrd_result::_Success)
        {
            printf("RESOURCE DEADLOCK WOULD OCCUR");
        }

        if (!_Verify_ownership_levels())
        {
            printf("RESOURCE UNAVAILABLE_TRY AGAIN");
        }
    }

    bool try_lock() noexcept
    {
        return _Mtx_trylock(_Mymtx()) == _Thrd_result::_Success;
    }

    void unlock() noexcept
    {
        _Mtx_unlock(_Mymtx());
    }

protected:
    bool _Verify_ownership_levels() noexcept
    {
        if (_Mtx_storage._Count == INT_MAX)
        {
            --_Mtx_storage._Count;
            return false;
        }

        return true;
    }

private:
    friend condition_variable;
    friend condition_variable_any;

    _Mtx_internal_imp_t _Mtx_storage{};

    _Mtx_t _Mymtx() noexcept
    {
        return &_Mtx_storage;
    }
};

class mutex : public _Mutex_base
{
public:
    mutex() noexcept = default;

    mutex(const mutex &) = delete;
    mutex &operator=(const mutex &) = delete;
};

class recursive_mutex : public _Mutex_base
{
public:
    recursive_mutex() noexcept : _Mutex_base(_Mtx_recursive)
    {
    }

    bool try_lock() noexcept
    {
        return _Mutex_base::try_lock() && _Verify_ownership_levels();
    }

    recursive_mutex(const recursive_mutex &) = delete;
    recursive_mutex &operator=(const recursive_mutex &) = delete;
};

struct adopt_lock_t
{
    explicit adopt_lock_t() = default;
};

struct defer_lock_t
{
    explicit defer_lock_t() = default;
};

struct try_to_lock_t
{
    explicit try_to_lock_t() = default;
};

inline constexpr adopt_lock_t adopt_lock{};
inline constexpr defer_lock_t defer_lock{};
inline constexpr try_to_lock_t try_to_lock{};

template <class _Mutex>
class unique_lock
{
public:
    using mutex_type = _Mutex;

    unique_lock() noexcept = default;

    explicit unique_lock(_Mutex &_Mtx)
        : _Pmtx(std::addressof(_Mtx)), _Owns(false)
    {
        _Pmtx->lock();
        _Owns = true;
    }

    unique_lock(_Mutex &_Mtx, adopt_lock_t) noexcept //
        : _Pmtx(std::addressof(_Mtx)), _Owns(true)
    {
    }
    unique_lock(_Mutex &_Mtx, defer_lock_t) noexcept
        : _Pmtx(std::addressof(_Mtx)), _Owns(false) {} // construct but don't lock

    unique_lock(_Mutex &_Mtx, try_to_lock_t)
        : _Pmtx(std::addressof(_Mtx)), _Owns(_Pmtx->try_lock()) {} // construct and try to lock

    template <class _Rep, class _Period>
    unique_lock(_Mutex &_Mtx, const chrono::duration<_Rep, _Period> &_Rel_time)
        : _Pmtx(std::addressof(_Mtx)), _Owns(_Pmtx->try_lock_for(_Rel_time)) {} // construct and lock with timeout

    template <class _Clock, class _Duration>
    unique_lock(_Mutex &_Mtx, const chrono::time_point<_Clock, _Duration> &_Abs_time)
        : _Pmtx(std::addressof(_Mtx)), _Owns(_Pmtx->try_lock_until(_Abs_time))
    {
        static_assert(chrono::_Is_clock_v<_Clock>, "Clock type required");
    }

    unique_lock(unique_lock &&_Other) noexcept : _Pmtx(_Other._Pmtx), _Owns(_Other._Owns)
    {
        _Other._Pmtx = nullptr;
        _Other._Owns = false;
    }

    unique_lock &operator=(unique_lock &&_Other) noexcept
    {
        unique_lock{std::move(_Other)}.swap(*this);
        return *this;
    }

    ~unique_lock() noexcept
    {
        if (_Owns)
        {
            _Pmtx->unlock();
        }
    }

    unique_lock(const unique_lock &) = delete;
    unique_lock &operator=(const unique_lock &) = delete;

    void lock()
    {
        _Validate();
        _Pmtx->lock();
        _Owns = true;
    }

    bool try_lock()
    {
        _Validate();
        _Owns = _Pmtx->try_lock();
        return _Owns;
    }

    template <class _Rep, class _Period>
    bool try_lock_for(const chrono::duration<_Rep, _Period> &_Rel_time)
    {
        _Validate();
        _Owns = _Pmtx->try_lock_for(_Rel_time);
        return _Owns;
    }

    template <class _Clock, class _Duration>
    bool try_lock_until(const chrono::time_point<_Clock, _Duration> &_Abs_time)
    {
        static_assert(chrono::_Is_clock_v<_Clock>, "Clock type required");
        _Validate();
        _Owns = _Pmtx->try_lock_until(_Abs_time);
        return _Owns;
    }

    void unlock()
    {
        if (!_Pmtx || !_Owns)
        {
            _Throw_system_error(errc::operation_not_permitted);
        }

        _Pmtx->unlock();
        _Owns = false;
    }

    void swap(unique_lock &_Other) noexcept
    {
        std::swap(_Pmtx, _Other._Pmtx);
        std::swap(_Owns, _Other._Owns);
    }

    _Mutex *release() noexcept
    {
        _Mutex *_Res = _Pmtx;
        _Pmtx = nullptr;
        _Owns = false;
        return _Res;
    }

    [[nodiscard]] bool owns_lock() const noexcept
    {
        return _Owns;
    }

    explicit operator bool() const noexcept
    {
        return _Owns;
    }

    [[nodiscard]] _Mutex *mutex() const noexcept
    {
        return _Pmtx;
    }

private:
    _Mutex *_Pmtx = nullptr;
    bool _Owns = false;

    void _Validate() const
    {
        if (!_Pmtx)
        {
            _Throw_system_error(errc::operation_not_permitted);
        }

        if (_Owns)
        {
            _Throw_system_error(errc::resource_deadlock_would_occur);
        }
    }
};

template <class _Mutex>
void swap(unique_lock<_Mutex> &_Left, unique_lock<_Mutex> &_Right) noexcept
{
    _Left.swap(_Right);
}

template <size_t... _Indices, class... _LockN>
void _Lock_from_locks(const int _Target, index_sequence<_Indices...>, _LockN &..._LkN)
{ // lock _LkN[_Target]
    int _Ignored[] = {((static_cast<int>(_Indices) == _Target ? (void)_LkN.lock() : void()), 0)...};
    (void)_Ignored;
}

template <size_t... _Indices, class... _LockN>
bool _Try_lock_from_locks(
    const int _Target, index_sequence<_Indices...>, _LockN &..._LkN)
{ // try to lock _LkN[_Target]
    bool _Result{};
    int _Ignored[] = {((static_cast<int>(_Indices) == _Target ? (void)(_Result = _LkN.try_lock()) : void()), 0)...};
    (void)_Ignored;
    return _Result;
}

template <size_t... _Indices, class... _LockN>
void _Unlock_locks(const int _First, const int _Last, index_sequence<_Indices...>, _LockN &..._LkN) noexcept
{
    // unlock locks in _LkN[_First, _Last)
    int _Ignored[] = {
        ((_First <= static_cast<int>(_Indices) && static_cast<int>(_Indices) < _Last ? (void)_LkN.unlock() : void()),
         0)...};
    (void)_Ignored;
}

template <class _Fn>
struct [[nodiscard]] _Unlock_call_guard
{
    static_assert(
        is_trivially_copyable_v<_Fn>, "This scope guard is only used for trivially copyable function objects.");

    explicit _Unlock_call_guard(const _Fn &_Fx) noexcept : _Func(_Fx) {}

    ~_Unlock_call_guard() noexcept
    {
        if (_Valid)
        {
            _Func();
        }
    }

    _Unlock_call_guard(const _Unlock_call_guard &) = delete;
    _Unlock_call_guard &operator=(const _Unlock_call_guard &) = delete;

    _Fn _Func;
    bool _Valid = true;
};

template <class _Lock>
struct [[nodiscard]] _Unlock_one_guard
{
    explicit _Unlock_one_guard(_Lock &_Lk) noexcept : _Lk_ptr(std::addressof(_Lk)) {}

    ~_Unlock_one_guard() noexcept
    {
        if (_Lk_ptr)
        {
            _Lk_ptr->unlock();
        }
    }

    _Unlock_one_guard(const _Unlock_one_guard &) = delete;
    _Unlock_one_guard &operator=(const _Unlock_one_guard &) = delete;

    _Lock *_Lk_ptr;
};

template <class... _LockN>
int _Try_lock_range(const int _First, const int _Last, _LockN &..._LkN)
{
    using _Indices = index_sequence_for<_LockN...>;
    int _Next = _First;

    auto _Unlocker = [_First, &_Next, &_LkN...]() noexcept
    { std::_Unlock_locks(_First, _Next, _Indices{}, _LkN...); };
    _Unlock_call_guard<decltype(_Unlocker)> _Guard{_Unlocker};

    for (; _Next != _Last; ++_Next)
    {
        if (!std::_Try_lock_from_locks(_Next, _Indices{}, _LkN...))
        { // try_lock failed, backout
            return _Next;
        }
    }

    _Guard._Valid = false;
    return -1;
}

template <class _Lock0, class _Lock1, class... _LockN>
int try_lock(_Lock0 &_Lk0, _Lock1 &_Lk1, _LockN &..._LkN)
{
    if constexpr (sizeof...(_LockN) == 0)
    {
        if (!_Lk0.try_lock())
        {
            return 0;
        }

        _Unlock_one_guard<_Lock0> _Guard{_Lk0};
        if (!_Lk1.try_lock())
        {
            return 1;
        }

        _Guard._Lk_ptr = nullptr;
        return -1;
    }
    else
    {
        return _Try_lock_range(0, sizeof...(_LockN) + 2, _Lk0, _Lk1, _LkN...);
    }
}

template <class... _LockN>
int _Lock_attempt(const int _Hard_lock, _LockN &..._LkN)
{
    using _Indices = index_sequence_for<_LockN...>;
    _Lock_from_locks(_Hard_lock, _Indices{}, _LkN...);
    int _Failed = -1;
    int _Backout_start = _Hard_lock;

    {
        auto _Unlocker = [&_Backout_start, _Hard_lock, &_LkN...]() noexcept
        {
            std::_Unlock_locks(_Backout_start, _Hard_lock + 1, _Indices{}, _LkN...);
        };
        _Unlock_call_guard<decltype(_Unlocker)> _Guard{_Unlocker};

        _Failed = std::_Try_lock_range(0, _Hard_lock, _LkN...);
        if (_Failed == -1)
        {
            _Backout_start = 0;
            _Failed = std::_Try_lock_range(_Hard_lock + 1, sizeof...(_LockN), _LkN...);
            if (_Failed == -1)
            {
                _Guard._Valid = false;
                return -1;
            }
        }
    }

    std::this_thread::yield();
    return _Failed;
}

template <class _Lock0, class _Lock1>
bool _Lock_attempt_small(_Lock0 &_Lk0, _Lock1 &_Lk1)
{
    _Lk0.lock();
    {
        _Unlock_one_guard<_Lock0> _Guard{_Lk0};
        if (_Lk1.try_lock())
        {
            _Guard._Lk_ptr = nullptr;
            return false;
        }
    }

    std::this_thread::yield();
    return true;
}

template <class _Lock0, class _Lock1, class... _LockN>
void lock(_Lock0 &_Lk0, _Lock1 &_Lk1, _LockN &..._LkN)
{
    if constexpr (sizeof...(_LockN) == 0)
    {
        while (_Lock_attempt_small(_Lk0, _Lk1) && _Lock_attempt_small(_Lk1, _Lk0))
        { // keep trying
        }
    }
    else
    {
        int _Hard_lock = 0;
        while (_Hard_lock != -1)
        {
            _Hard_lock = _Lock_attempt(_Hard_lock, _Lk0, _Lk1, _LkN...);
        }
    }
}

template <class _Mutex>
class lock_guard
{
public:
    using mutex_type = _Mutex;

    explicit lock_guard(_Mutex &_Mtx) : _MyMutex(_Mtx)
    {
        _MyMutex.lock();
    }

    lock_guard(_Mutex &_Mtx, adopt_lock_t) noexcept
        : _MyMutex(_Mtx)
    {
    }
    ~lock_guard() noexcept
    {
        _MyMutex.unlock();
    }

    lock_guard(const lock_guard &) = delete;
    lock_guard &operator=(const lock_guard &) = delete;

private:
    _Mutex &_MyMutex;
};

template <class... _Mutexes>
class scoped_lock
{
public:
    explicit scoped_lock(_Mutexes &..._Mtxes) : _MyMutexes(_Mtxes...)
    {
        std::lock(_Mtxes...);
    }

    explicit scoped_lock(adopt_lock_t, _Mutexes &..._Mtxes) noexcept //
        : _MyMutexes(_Mtxes...)
    {
    }
    ~scoped_lock() noexcept
    {
        std::apply([](_Mutexes &..._Mtxes) static
                   { (..., (void)_Mtxes.unlock()); }, _MyMutexes);
    }

    scoped_lock(const scoped_lock &) = delete;
    scoped_lock &operator=(const scoped_lock &) = delete;

private:
    tuple<_Mutexes &...> _MyMutexes;
};

template <class _Mutex>
class scoped_lock<_Mutex>
{
public:
    using mutex_type = _Mutex;

    explicit scoped_lock(_Mutex &_Mtx) : _MyMutex(_Mtx)
    {
        _MyMutex.lock();
    }

    explicit scoped_lock(adopt_lock_t, _Mutex &_Mtx) noexcept
        : _MyMutex(_Mtx)
    {
    }

    ~scoped_lock() noexcept
    {
        _MyMutex.unlock();
    }

    scoped_lock(const scoped_lock &) = delete;
    scoped_lock &operator=(const scoped_lock &) = delete;

private:
    _Mutex &_MyMutex;
};

template <>
class scoped_lock<>
{
public:
    explicit scoped_lock() = default;
    explicit scoped_lock(adopt_lock_t) noexcept {}

    scoped_lock(const scoped_lock &) = delete;
    scoped_lock &operator=(const scoped_lock &) = delete;
};

enum class cv_status
{
    no_timeout,
    timeout
};

class condition_variable
{
public:
    condition_variable() noexcept = default;

    ~condition_variable() noexcept = default;

    condition_variable(const condition_variable &) = delete;
    condition_variable &operator=(const condition_variable &) = delete;

    void notify_one() noexcept
    {
        _Cnd_signal(_Mycnd());
    }

    void notify_all() noexcept
    {
        _Cnd_broadcast(_Mycnd());
    }

    void wait(unique_lock<mutex> &_Lck) noexcept
    { // wait for signal
        _Cnd_wait(_Mycnd(), _Lck.mutex()->_Mymtx());
    }

    template <class _Predicate>
    void wait(unique_lock<mutex> &_Lck, _Predicate _Pred)
    {
        while (!_Pred())
        {
            wait(_Lck);
        }
    }

    template <class _Rep, class _Period>
    cv_status wait_for(unique_lock<mutex> &_Lck, const chrono::duration<_Rep, _Period> _Rel_time)
    {
        if (_Rel_time <= chrono::duration<_Rep, _Period>::zero())
        {
            return cv_status::timeout;
        }
        return wait_until(_Lck, _To_absolute_time(_Rel_time));
    }

    template <class _Rep, class _Period, class _Predicate>
    bool wait_for(unique_lock<mutex> &_Lck, const chrono::duration<_Rep, _Period> _Rel_time, _Predicate _Pred)
    {
        // wait for signal with timeout and check predicate
        return wait_until(_Lck, _To_absolute_time(_Rel_time), std::_Pass_fn(_Pred));
    }

    template <class _Clock, class _Duration>
    cv_status wait_until(unique_lock<mutex> &_Lck, const chrono::time_point<_Clock, _Duration> _Abs_time)
    {
        // wait until time point
        static_assert(chrono::_Is_clock_v<_Clock>, "Clock type required");
        for (;;)
        {
            const auto _Now = _Clock::now();
            if (_Abs_time <= _Now)
            {
                // we don't unlock-and-relock _Lck for this case because it's not observable
                return cv_status::timeout;
            }

            const unsigned long _Rel_ms_count = _Clamped_rel_time_ms_count(_Abs_time - _Now)._Count;

            const _Thrd_result _Res = _Cnd_timedwait_for_unchecked(_Mycnd(), _Lck.mutex()->_Mymtx(), _Rel_ms_count);
            if (_Res == _Thrd_result::_Success)
            {
                return cv_status::no_timeout;
            }
        }
    }

    template <class _Clock, class _Duration, class _Predicate>
    bool wait_until(unique_lock<mutex> &_Lck, const chrono::time_point<_Clock, _Duration> _Abs_time, _Predicate _Pred)
    {
        // wait for signal with timeout and check predicate
        static_assert(chrono::_Is_clock_v<_Clock>, "Clock type required");
        while (!_Pred())
        {
            if (wait_until(_Lck, _Abs_time) == cv_status::timeout)
            {
                return _Pred();
            }
        }

        return true;
    }

    // native_handle_type and native_handle() have intentionally been removed. See GH-3820.

    void _Register(unique_lock<mutex> &_Lck, int *_Ready) noexcept
    {
        _Cnd_register_at_thread_exit(_Mycnd(), _Lck.release()->_Mymtx(), _Ready);
    }

    void _Unregister(mutex &_Mtx) noexcept
    {
        _Cnd_unregister_at_thread_exit(_Mtx._Mymtx());
    }

private:
    _Cnd_internal_imp_t _Cnd_storage{};

    _Cnd_t _Mycnd() noexcept
    {
        return &_Cnd_storage;
    }
};

struct _UInt_is_zero
{
    const unsigned int &_UInt;

    [[nodiscard]] bool operator()() const noexcept
    {
        return _UInt == 0;
    }
};

class timed_mutex
{ // class for timed mutual exclusion
public:
    timed_mutex() = default;

    timed_mutex(const timed_mutex &) = delete;
    timed_mutex &operator=(const timed_mutex &) = delete;

    void lock()
    { // lock the mutex
        unique_lock<mutex> _Lock(_My_mutex);
        while (_My_locked != 0)
        {
            _My_cond.wait(_Lock);
        }

        _My_locked = UINT_MAX;
    }

    bool try_lock() noexcept
    { // try to lock the mutex
        lock_guard<mutex> _Lock(_My_mutex);
        if (_My_locked != 0)
        {
            return false;
        }
        else
        {
            _My_locked = UINT_MAX;
            return true;
        }
    }

    void unlock()
    { // unlock the mutex
        {
            // The lock here is necessary
            lock_guard<mutex> _Lock(_My_mutex);
            _My_locked = 0;
        }
        _My_cond.notify_one();
    }

    template <class _Rep, class _Period>
    bool try_lock_for(
        const chrono::duration<_Rep, _Period> &_Rel_time)
    { // try to lock for duration
        return try_lock_until(_To_absolute_time(_Rel_time));
    }

    template <class _Clock, class _Duration>
    bool try_lock_until(const chrono::time_point<_Clock, _Duration> &_Abs_time)
    {
        // try to lock the mutex with timeout
        static_assert(chrono::_Is_clock_v<_Clock>, "Clock type required");
        unique_lock<mutex> _Lock(_My_mutex);
        if (!_My_cond.wait_until(_Lock, _Abs_time, _UInt_is_zero{_My_locked}))
        {
            return false;
        }

        _My_locked = UINT_MAX;
        return true;
    }

private:
    mutex _My_mutex;
    condition_variable _My_cond;
    unsigned int _My_locked = 0;
};

class recursive_timed_mutex
{ // class for recursive timed mutual exclusion
public:
    recursive_timed_mutex() = default;

    recursive_timed_mutex(const recursive_timed_mutex &) = delete;
    recursive_timed_mutex &operator=(const recursive_timed_mutex &) = delete;

    void lock()
    { // lock the mutex
        const thread::id _Tid = this_thread::get_id();

        unique_lock<mutex> _Lock(_My_mutex);

        if (_Tid == _My_owner)
        {
            if (_My_locked < UINT_MAX)
            {
                ++_My_locked;
            }
            else
            {
                std::_Throw_system_error(errc::resource_unavailable_try_again);
            }
        }
        else
        {
            while (_My_locked != 0)
            {
                _My_cond.wait(_Lock);
            }

            _My_locked = 1;
            _My_owner = _Tid;
        }
    }

    bool try_lock() noexcept
    { // try to lock the mutex
        const thread::id _Tid = this_thread::get_id();

        lock_guard<mutex> _Lock(_My_mutex);

        if (_Tid == _My_owner)
        {
            if (_My_locked < UINT_MAX)
            {
                ++_My_locked;
            }
            else
            {
                return false;
            }
        }
        else
        {
            if (_My_locked != 0)
            {
                return false;
            }
            else
            {
                _My_locked = 1;
                _My_owner = _Tid;
            }
        }
        return true;
    }

    void unlock()
    { // unlock the mutex
        bool _Do_notify = false;

        {
            lock_guard<mutex> _Lock(_My_mutex);
            --_My_locked;
            if (_My_locked == 0)
            {
                _Do_notify = true;
                _My_owner = thread::id();
            }
        }

        if (_Do_notify)
        {
            _My_cond.notify_one();
        }
    }

    template <class _Rep, class _Period>
    bool try_lock_for(
        const chrono::duration<_Rep, _Period> &_Rel_time)
    { // try to lock for duration
        return try_lock_until(_To_absolute_time(_Rel_time));
    }

    template <class _Clock, class _Duration>
    bool try_lock_until(const chrono::time_point<_Clock, _Duration> &_Abs_time)
    {
        // try to lock the mutex with timeout
        static_assert(chrono::_Is_clock_v<_Clock>, "Clock type required");
        const thread::id _Tid = this_thread::get_id();

        unique_lock<mutex> _Lock(_My_mutex);

        if (_Tid == _My_owner)
        {
            if (_My_locked < UINT_MAX)
            {
                ++_My_locked;
            }
            else
            {
                return false;
            }
        }
        else
        {
            if (!_My_cond.wait_until(_Lock, _Abs_time, _UInt_is_zero{_My_locked}))
            {
                return false;
            }

            _My_locked = 1;
            _My_owner = _Tid;
        }
        return true;
    }

private:
    mutex _My_mutex;
    condition_variable _My_cond;
    unsigned int _My_locked = 0;
    thread::id _My_owner;
};*/