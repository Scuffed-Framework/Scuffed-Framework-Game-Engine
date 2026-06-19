#pragma once
#include <cstdint>
#include <Audio/Microphone.hpp>
#include <string>
#include <ID/GUID.hpp>

namespace SF::Engine
{
    struct GameSession
    {
        uint32_t maxSpectators;
        uint32_t maxPlayers;
        uint32_t maxPartySize;
        uint32_t maxSplitscreenPlayers = 4;

        MicrophoneSpeechMode micMode = MicrophoneSpeechMode::PushToTalk;
        std::wstring sessionName;

        GUID GetNextPlayerID();

        virtual bool ProcessAutoLogin();
        virtual void OnLoginComplete(const GUID &playerID, bool success, const std::string &error);
        virtual std::string ApproveLogin(const std::string options);

        virtual void RegisterPlayer(const GUID &playerID, bool fromInvitation);
    };
}