/*
 * Copyright (c) 2016-2022 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_TEXT_LOC_HPP
#define BABELTRACE_CPP_COMMON_TEXT_LOC_HPP

#include <cstdlib>

namespace bt2_common {

/*
 * Location in a multiline text.
 */
class TextLoc final
{
public:
    /*
     * Builds a text location which targets offset `offset` bytes,
     * zero-based line number `lineNo`, and zero-based column number
     * `colNo`.
     */
    explicit TextLoc(std::size_t offset = 0, std::size_t lineNo = 0,
                     std::size_t colNo = 0) noexcept;

    /* Default move/copy constructor/assignment operator */
    TextLoc(const TextLoc&) = default;
    TextLoc(TextLoc&&) = default;
    TextLoc& operator=(const TextLoc&) = default;
    TextLoc& operator=(TextLoc&&) = default;

    /*
     * Offset (bytes).
     */
    std::size_t offset() const noexcept
    {
        return _mOffset;
    }

    /*
     * Line number (zero-based).
     */
    std::size_t lineNo() const noexcept
    {
        return _mLineNo;
    }

    /*
     * Column number (zero-based).
     */
    std::size_t colNo() const noexcept
    {
        return _mColNo;
    }

    /*
     * Line number (one-based).
     */
    std::size_t naturalLineNo() const noexcept
    {
        return _mLineNo + 1;
    }

    /*
     * Column number (one-based).
     */
    std::size_t naturalColNo() const noexcept
    {
        return _mColNo + 1;
    }

private:
    std::size_t _mOffset = 0;
    std::size_t _mLineNo = 0;
    std::size_t _mColNo = 0;
};

} /* namespace bt2_common */

#endif /* BABELTRACE_CPP_COMMON_TEXT_LOC_HPP */
