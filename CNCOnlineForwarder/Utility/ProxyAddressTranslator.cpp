#include "ProxyAddressTranslator.hpp"
#include <precompiled.hpp>
#include <Logging/Logging.hpp>
#include <Utility/SimpleHTTPClient.hpp>
#include <Utility/WeakRefHandler.hpp>

using ErrorCode = boost::system::error_code;
using LogLevel = CNCOnlineForwarder::Logging::Level;

namespace CNCOnlineForwarder::Utility
{
    template<typename... Arguments>
    void log(LogLevel const level, Arguments&&... arguments)
    {
        return Logging::logLine<ProxyAddressTranslator>(level, std::forward<Arguments>(arguments)...);
    }

    std::shared_ptr<ProxyAddressTranslator> ProxyAddressTranslator::create
    (
        IOManager::ObjectMaker const& objectMaker
    )
    {
        auto const self = std::make_shared<ProxyAddressTranslator>
        (
            PrivateConstructor{},
            objectMaker
        );
        periodicallySetPublicAddress(self);
        return self;
    }

    ProxyAddressTranslator::ProxyAddressTranslator
    (
        PrivateConstructor,
        IOManager::ObjectMaker const& objectMaker
    ) :
        m_objectMaker{ objectMaker },
        m_publicAddress{}
    {
    }

    ProxyAddressTranslator::AddressV4 ProxyAddressTranslator::getPublicAddress() const
    {
        auto const lock = std::scoped_lock{ m_mutex };
        return m_publicAddress;
    }

    void ProxyAddressTranslator::setPublicAddress(AddressV4 const& newPublicAddress)
    {
        {
            auto const lock = std::scoped_lock{ m_mutex };
            m_publicAddress = newPublicAddress;
        }
        log(LogLevel::info, "Public address updated to ", newPublicAddress);
    }

    ProxyAddressTranslator::UDPEndPoint ProxyAddressTranslator::localToPublic
    (
        UDPEndPoint const& endPoint
    ) const
    {
        auto publicEndPoint = endPoint;
        publicEndPoint.address(getPublicAddress());
        return publicEndPoint;
    }

    void ProxyAddressTranslator::periodicallySetPublicAddress
    (
        std::weak_ptr<ProxyAddressTranslator> const& ref
    )
    {
        auto const self = ref.lock();
        if (!self)
        {
            log(LogLevel::info, "ProxyAddressTranslator expired, not updating anymore");
            return;
        }

        log(LogLevel::info, "Will update public address now.");

        auto const action = [](ProxyAddressTranslator& self, std::string newIP)
        {
            auto ip = std::optional<AddressV4>{};
            try 
            {
                boost::algorithm::trim(newIP);
                log(LogLevel::info, "Retrieved public IP address: ", newIP);
                ip = AddressV4::from_string(newIP);
            }
            catch (std::exception const& exception) 
            {
                log(LogLevel::error, "Failed to parse IP address ", newIP, ": ", exception.what());
                return;
            }
            self.setPublicAddress(ip.value());
        };
        Utility::asyncHttpGet
        (
            self->m_objectMaker,
            "api.ipify.org",
            "/",
            Utility::makeWeakHandler(self, action)
        );

        using Timer = boost::asio::steady_timer;
        auto const timer = std::make_shared<Timer>(self->m_objectMaker.make<Timer>());
        timer->expires_after(std::chrono::minutes{ 1 });
        timer->async_wait([ref, timer](ErrorCode const& code)
        {
            if (code.failed())
            {
                throw std::system_error{ code, "ProxyAddressTranslator: async wait failed" };
            }
            periodicallySetPublicAddress(ref);
        });
    }
}