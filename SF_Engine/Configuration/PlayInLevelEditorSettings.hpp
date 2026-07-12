#pragma once

namespace SF::Engine
{
    // for later :)
    enum PlayInEditorType : int
    {
        IN_EDITOR,
        NEW_FLOATING,
        NEW_PROC,
        SIMULATE
    };

    enum PlayNetMode : int
    {
        Standalone,
        Client,
        ListenServer
    };
}