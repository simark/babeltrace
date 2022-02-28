/*
 * Copyright (c) 2016-2022 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "text-loc.hpp"

namespace bt2_common {

TextLoc::TextLoc(const std::size_t offset, const std::size_t lineNo,
                 const std::size_t colNo) noexcept :
    _mOffset {offset},
    _mLineNo {lineNo}, _mColNo {colNo}
{
}

} /* namespace bt2_common */
