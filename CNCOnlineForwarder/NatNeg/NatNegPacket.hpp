#pragma once
#include <precompiled.hpp>

namespace CNCOnlineForwarder::NatNeg
{
    enum class NatNegStep : char
    {
        init = 0,
        initAck = 1,
        connect = 5,
        connectAck = 6,
        connectPing = 7,
        report = 13,
        reportAck = 14,
        preInit = 15,
        preInitAck = 16,
    };

    inline std::ostream& operator<<(std::ostream& out, NatNegStep const step)
    {
        return out << static_cast<int>(step);
    }

    using NatNegID = std::uint32_t;

    struct NatNegPlayerID
    {
        struct Hash;

        NatNegID natNegID;
        std::int8_t playerID;
        
    };

    inline bool operator==(NatNegPlayerID const a, NatNegPlayerID const& b)
    {
        return a.natNegID == b.natNegID && a.playerID == b.playerID;
    }
    
    struct NatNegPlayerID::Hash
    {
        std::size_t operator()(NatNegPlayerID const& id) const
        {
            auto hash = std::size_t{ 0 };
            boost::hash_combine(hash, id.natNegID);
            boost::hash_combine(hash, id.playerID);
            return hash;
        }
    };

    inline std::ostream& operator<<(std::ostream& out, NatNegPlayerID const& id)
    {
        return out << '[' << id.natNegID << ':' << static_cast<int>(id.playerID) << ']';
    }

    class NatNegPacketView
    {
    private:
        std::string_view m_natNegPacket;
    
    public:
        NatNegPacketView(std::string_view const data) :
            m_natNegPacket{ data }
        {
        }

        std::string_view getView() const noexcept 
        {
            return m_natNegPacket;
        }

        std::string copyBuffer() const
        {
            return std::string{ m_natNegPacket };
        }

        bool isNatNeg() const noexcept
        {
            using namespace std::string_view_literals;
            static constexpr auto natNegMagic = "\xFD\xFC\x1E\x66\x6A\xB2"sv;
            static constexpr auto versionSize = 1;
            static constexpr auto stepSize = 1;

            if (m_natNegPacket.size() < natNegMagic.size() + versionSize + stepSize)
            {
                return false;
            }

            if (m_natNegPacket.rfind(natNegMagic, 0) == m_natNegPacket.npos)
            {
                return false;
            }

            return true;
        }

        NatNegStep getStep() const
        {
            constexpr auto stepPosition = 7;

            if (isNatNeg() == false)
            {
                throw new std::invalid_argument{ "Invalid NatNeg packet: incorrect magic." };
            }

            return static_cast<NatNegStep>(m_natNegPacket.at(stepPosition));
        }

        // Returns: NatNegID of the packet,
        // if this packet actually contains a NatNegID
        std::optional<NatNegID> getNatNegID() const
        {
            switch (getStep())
            {
            case NatNegStep::init:
            case NatNegStep::initAck:
            case NatNegStep::connect:
            case NatNegStep::connectAck:
            case NatNegStep::connectPing:
            case NatNegStep::report:
            case NatNegStep::reportAck:
            {
                constexpr auto natNegIDPosition = 8;
                if (m_natNegPacket.size() < (natNegIDPosition + sizeof(NatNegID)))
                {
                    throw std::invalid_argument{ "Invalid NatNeg packet: packet too small to contain NatNegID." };
                }
                auto id = NatNegID{};
                std::copy_n(m_natNegPacket.begin() + natNegIDPosition, sizeof(id), reinterpret_cast<char*>(&id));
                return id;
            }
            default:
                return std::nullopt;
            }
        }

        // Returns: NatNegID and PlayerID of the packet,
        // if this packet actually contains both NatNegID and PlayerID
        std::optional<NatNegPlayerID> getNatNegPlayerID() const
        {
            auto const natNegID = getNatNegID();
            if (natNegID == std::nullopt)
            {
                return std::nullopt;
            }

            auto playerIDPosition = m_natNegPacket.npos;
            switch (getStep())
            {
            case NatNegStep::init:
            case NatNegStep::initAck:
            case NatNegStep::connectAck:
            case NatNegStep::report:
            case NatNegStep::reportAck:
                playerIDPosition = 13;
                break;
            case NatNegStep::preInit:
            case NatNegStep::preInitAck:
                playerIDPosition = 12;
                break;
            default:
                return std::nullopt;
            }

            if (m_natNegPacket.size() < (playerIDPosition + sizeof(char)))
            {
                throw std::invalid_argument{ "Invalid NatNeg packet: packet too small to contain playerID." };
            }

            return NatNegPlayerID{ natNegID.value(), m_natNegPacket.at(playerIDPosition) };
        }

        // Returns: Position of IP relative to the beginning of the packet,
        // if this packet actually contains an IP address
        static constexpr std::optional<std::size_t> getAddressOffset(NatNegStep const step)
        {
            switch (step)
            {
            case NatNegStep::connect:
            case NatNegStep::connectPing:
                return 12;
            default:
                return std::nullopt;
            }
        }
    };

    inline std::pair<std::array<std::uint8_t, 4>, std::uint16_t> parseAddress
    (
        std::string_view const source,
        std::size_t const position
    )
    {
        auto ip = std::array<std::uint8_t, 4>{};
        auto port = std::uint16_t{};
        if (source.size() < (position + ip.size() + sizeof(port)))
        {
            throw std::out_of_range{ "NatNeg Packet too short!" };
        }

        auto const ipHolder = source.substr(position, ip.size());
        auto const portHolder = source.substr(position + ip.size(), sizeof(port));
        std::copy_n(ipHolder.begin(), ip.size(), ip.begin());
        std::copy_n(portHolder.begin(), sizeof(port), reinterpret_cast<char*>(&port));
        return std::pair{ ip, port };
    }

    inline void rewriteAddress
    (
        std::string& destination,
        std::size_t const position,
        std::array<std::uint8_t, 4> const& ip,
        std::uint16_t const port
    )
    {
        if (destination.size() < (position + ip.size() + sizeof(port)))
        {
            throw std::out_of_range{ "NatNeg Packet too short!" };
        }

        auto const then = std::copy_n(ip.begin(), ip.size(), destination.begin() + position);
        std::copy_n(reinterpret_cast<char const*>(&port), sizeof(port), then);
    }
}