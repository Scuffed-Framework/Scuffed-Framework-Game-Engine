// Copyright © 2002-2010 Steve Lhomme.
// SPDX-License-Identifier: LGPL-2.1-or-later

/*!
  \file
  \author Steve Lhomme     <robux4 @ users.sf.net>
*/
#include "KaxSegment.h"
#include <Matroska/EBML/EbmlHead.h>

// sub elements
#include "KaxCluster.h"
#include "KaxContexts.h"
#include "KaxCues.h"
#include "KaxDefines.h"
#include "KaxSeekHead.h"
#include "KaxTracks.h"

using namespace libebml;

namespace libmatroska
{

    KaxSegment::KaxSegment() : EbmlMaster(KaxSegment::ClassInfos)
    {
        SetSizeLength(5);   // mandatory min size support (for easier updating) (2^(7*5)-2 = 32Go)
        SetSizeInfinite();  // by default a segment is big and the size is unknown in advance
    }

    KaxSegment::KaxSegment(const KaxSegment& ElementToClone) : EbmlMaster(ElementToClone)
    {
        // update the parent of each children
        for (const auto& child : *this)
            if (EbmlId(*child) == EBML_ID(KaxCluster))
                static_cast<KaxCluster*>(child)->SetParent(*this);
    }

    std::uint64_t KaxSegment::GetRelativePosition(std::uint64_t aGlobalPosition) const
    {
        return aGlobalPosition - GetDataStart();
    }

    std::uint64_t KaxSegment::GetRelativePosition(const EbmlElement& Elt) const
    {
        return GetRelativePosition(Elt.GetElementPosition());
    }

    std::uint64_t KaxSegment::GetGlobalPosition(std::uint64_t aRelativePosition) const
    {
        return aRelativePosition + GetDataStart();
    }

}  // namespace libmatroska
