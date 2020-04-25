#pragma once
// boost::asio::use_future does not work on MSVC because MSVC incorrectly 
// requires std::promise to use default constructible types
#include <boost/asio/async_result.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>

class CancellableFutureHelper
{
private:
    std::mutex m_mutex;
    boost::asio::steady_timer m_timer;

public:
    explicit CancellableFutureHelper(boost::asio::steady_timer timer) : m_timer{ std::move(timer) } {}

    template<typename Handler>
    void addHandler(Handler&& handler)
    {
        auto const lock = std::scoped_lock{ m_mutex };
        m_timer.async_wait(std::move(handler));
    }
};

template<typename T>
class Promise
{
private:
    std::shared_ptr<std::promise<std::optional<T>>> m_promise;

public:
    explicit Promise(std::shared_ptr<CancellableFutureHelper> const& helper) :
        m_promise{ std::make_shared<std::promise<std::optional<T>>>() }
    {
        if (helper == nullptr)
        {
            throw std::invalid_argument("timer is null");
        }
        helper->addHandler([p = std::weak_ptr{ m_promise }](boost::system::error_code const& code)
        {
            auto const promise = p.lock();
            if (promise == nullptr) { return; }
            try
            {
                promise->set_exception(std::make_exception_ptr(std::runtime_error{ "timer expired" }));
            }
            catch (...) {}
        });
    }

    Promise(Promise&&) = default;
    Promise& operator=(Promise&&) = default;

    template<typename U>
    void operator()(boost::system::error_code const& code, U&& result)
    {
        if (m_promise == nullptr)
        {
            return;
        }

        if (code.failed())
        {
            m_promise->set_exception(std::make_exception_ptr(boost::system::system_error{ code }));
            return;
        }

        m_promise->set_value(std::forward<U>(result));
    }

    std::future<std::optional<T>> get() const
    {
        if (m_promise == nullptr)
        {
            throw std::future_error{ std::future_errc::no_state };
        }

        return m_promise->get_future();
    }
};

template<typename T>
class boost::asio::async_result<std::shared_ptr<CancellableFutureHelper>, void(boost::system::error_code, T)>
{
public:
    using completion_handler_type = Promise<T>;
    using return_type = decltype(std::declval<completion_handler_type>().get());

private:
    return_type m_future;

public:
    async_result(completion_handler_type& handler) : m_future{ handler.get() } {}

    return_type get()
    {
        if (not m_future.valid())
        {
            throw std::future_error{ std::future_errc::no_state };
        }
        return std::move(m_future);
    }
};

template<typename T>
T get(std::future<std::optional<T>>&& future)
{
    if (not future.valid()) { throw std::future_error{ std::future_errc::no_state }; }
    return std::move(future.get().value());
}