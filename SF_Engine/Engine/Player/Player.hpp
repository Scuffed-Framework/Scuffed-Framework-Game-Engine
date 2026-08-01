#pragma once
#include <string>
#include <ID/GUID.hpp>
#include <TemplateLibrary/TypeTraits.hpp>
#include <TemplateLibrary/Types.hpp>

using GUID = SF::Engine::GUID; // microslop might have seeped in

using namespace SFTL;
namespace SF::Engine
{

    class PlayerBase
    {
    public:
        std::string Name;
        GUID PlayerID;

        // internet (lil scripty might lag the server :sob:)
        int32 CurrentNetSpeed;
        int32 ConfiguredInternetSpeed;
        int32 ConfiguredLanSpeed;

        virtual void SwitchPlayerController();
    };

    template <typename Player>
    constexpr bool IsDerivedOfPlayerBase = is_base_of_v<PlayerBase, Player> && is_same_v<Player, PlayerBase>;
}