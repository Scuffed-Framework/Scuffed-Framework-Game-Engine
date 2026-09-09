#pragma once
#include <UtilityClasses/UUID.hpp>
#include <string>

namespace SF::Engine
{
    using namespace std;
    class PlayerBase
    {
    public:
        virtual ~PlayerBase() = default;
        string Name;
        UUID PlayerID;

        // internet (lil scripty might lag the server :sob:)
        int32_t CurrentNetSpeed;
        int32_t ConfiguredInternetSpeed;
        int32_t ConfiguredLanSpeed;

        virtual void SwitchPlayerController();
    };

    template<typename Player>
    constexpr bool IsDerivedOfPlayerBase = is_base_of_v<PlayerBase, Player> && is_same_v<Player, PlayerBase>;
} // namespace SF::Engine
