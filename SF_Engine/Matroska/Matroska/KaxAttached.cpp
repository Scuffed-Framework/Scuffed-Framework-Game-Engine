// Copyright © 2002-2010 Steve Lhomme.
// SPDX-License-Identifier: LGPL-2.1-or-later

/*!
  \file
  \author Steve Lhomme     <robux4 @ users.sf.net>
*/
#include "KaxContexts.h"
#include "KaxDefines.h"
#include "KaxSemantic.h"

// sub elements

using namespace libebml;

namespace libmatroska
{

    KaxAttached::KaxAttached() : EbmlMaster(KaxAttached::ClassInfos)
    {
        SetSizeLength(2);  // mandatory min size support (for easier updating) (2^(7*2)-2 = 16Ko)
    }

}  // namespace libmatroska
