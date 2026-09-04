#pragma once
#include <1stPartyLibs/TemplateLibrary/TypeTraits.hpp>
#include <1stPartyLibs/TemplateLibrary/Types.hpp>
#include <UtilityClasses/UUID.hpp>
#include <string>

using namespace SFTL;
namespace SF::Engine
{

    class PlayerBase
    {
    public:
        std::string Name;
        UUID PlayerID;

        // internet (lil scripty might lag the server :sob:)
        int32 CurrentNetSpeed;
        int32 ConfiguredInternetSpeed;
        int32 ConfiguredLanSpeed;

        virtual void SwitchPlayerController();
    };

    template<typename Player>
    constexpr bool IsDerivedOfPlayerBase = is_base_of_v<PlayerBase, Player> && is_same_v<Player, PlayerBase>;
}