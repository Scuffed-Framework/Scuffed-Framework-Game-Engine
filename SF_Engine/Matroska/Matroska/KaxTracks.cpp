// Copyright © 2002-2010 Steve Lhomme.
// SPDX-License-Identifier: LGPL-2.1-or-later

/*!
  \file
  \author Steve Lhomme     <robux4 @ users.sf.net>
*/
#include "KaxTracks.h"

// sub elements
#include "KaxContexts.h"
#include "KaxDefines.h"

using namespace libebml;

namespace libmatroska
{

    void KaxTrackEntry::EnableLacing(bool bEnable)
    {
        auto& myLacing = GetChild<KaxTrackFlagLacing>(*this);
        myLacing.SetValue(bEnable ? 1 : 0);
    }

}  // namespace libmatroska
