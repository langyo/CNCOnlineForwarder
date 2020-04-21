#pragma once
#include "precompiled.hpp"
#include <IOManager.hpp>

namespace CNCOnlineForwarder::Utility {
    namespace Details
    {
        template<typename T>
        class WithStrandBase
        {
        public:
            template<typename... Args>
            WithStrandBase(IOManager::StrandType& strand, Args&&... args) :
                m_strand{ strand },
                m_object{ m_strand.get_inner_executor(), std::forward<Args>(args)... }
            {}

            T* operator->() noexcept { return &m_object; }

            T const* operator->() const noexcept { return &m_object; }

        protected:
            IOManager::StrandType& m_strand;
            T m_object;
        };
    }

    template<typename T>
    class WithStrand : public Details::WithStrandBase<T>
    {
    public:
        using Details::WithStrandBase<T>::WithStrandBase;
    };

    template<>
    class WithStrand<boost::asio::ip::udp::socket> :
        public Details::WithStrandBase<boost::asio::ip::udp::socket>
    {
    public:
        using Details::WithStrandBase<boost::asio::ip::udp::socket>::WithStrandBase;

        template<typename MutableBufferSequence, typename EndPoint, typename ReadHandler>
        auto asyncReceiveFrom
        (
            MutableBufferSequence const& buffers,
            EndPoint& from,
            ReadHandler&& handler
        )
        {
            return m_object.async_receive_from
            (
                buffers,
                from,
                boost::asio::bind_executor(m_strand, std::forward<ReadHandler>(handler))
            );
        }

        template<typename ConstBufferSequence, typename EndPoint, typename WriteHandler>
        auto asyncSendTo
        (
            ConstBufferSequence const& buffers,
            EndPoint const& to,
            WriteHandler&& handler
        )
        {
            return m_object.async_send_to
            (
                buffers,
                to,
                boost::asio::bind_executor(m_strand, std::forward<WriteHandler>(handler))
            );
        }
    };

    template<>
    class WithStrand<boost::asio::steady_timer> :
        public Details::WithStrandBase<boost::asio::steady_timer>
    {
    public:
        using Details::WithStrandBase<boost::asio::steady_timer>::WithStrandBase;

        template<typename WaitHandler>
        auto asyncWait(std::chrono::minutes const timeout, WaitHandler&& waitHandler)
        {
            m_object.expires_from_now(timeout);
            m_object.async_wait(std::forward<WaitHandler>(waitHandler));
        }
    };

    template<typename Protocol>
    class WithStrand<boost::asio::ip::basic_resolver<Protocol>> :
        public Details::WithStrandBase<boost::asio::ip::basic_resolver<Protocol>>
    {
    public:
        using Details::WithStrandBase<boost::asio::ip::basic_resolver<Protocol>>::WithStrandBase;

        template<typename ResolveHandler>
        auto asyncResolve
        (
            std::string_view const host,
            std::string_view const service,
            ResolveHandler&& resolveHandler
        )
        {
            m_object.async_resolve
            (
                host,
                service,
                std::forward<ResolveHandler>(resolveHandler)
            );
        }
    };
}