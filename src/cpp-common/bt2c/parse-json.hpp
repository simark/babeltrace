/*
 * Copyright (c) 2022-2024 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2C_PARSE_JSON_HPP
#define BABELTRACE_CPP_COMMON_BT2C_PARSE_JSON_HPP

#include <cstring>
#include <string>
#include <string_view>
#include <unordered_set>

#include "common/assert.h"

#include "exc.hpp"
#include "str-scanner.hpp"

namespace bt2c {
namespace internal {

template <typename ListenerT>
class JsonParser final
{
public:
    /*
     * Builds a JSON text parser, wrapping the string `str`, and parses
     * it, calling the methods of the JSON event listener `listener`.
     *
     * Adds to the text location offset for all error messages.
     *
     * When the JSON parser logs or appends a cause to the error of the
     * current thread, it uses `baseOffset` to format the text location
     * part of the message.
     */
    explicit JsonParser(std::string_view str, ListenerT& listener, std::size_t baseOffset,
                        const Logger& parentLogger);

private:
    /*
     * Parses the whole JSON string.
     */
    void _parse();

    /*
     * Expects a JSON value, appending a cause to the error of the
     * current thread and throwing `Error` if not found.
     */
    void _expectVal();

    /*
     * Tries to parse `null`, calling the event listener on success.
     */
    bool _tryParseNull();

    /*
     * Tries to parse `true` or `false`, calling the event listener on
     * success.
     */
    bool _tryParseBool();

    /*
     * Tries to parse a JSON number, calling the event listener on
     * success.
     */
    bool _tryParseNumber();

    /*
     * Tries to parse a JSON object key, calling the event listener on
     * success.
     */
    bool _tryParseObjKey();

    /*
     * Tries to parse a JSON string, calling the event listener on
     * success.
     */
    bool _tryParseStr();

    /*
     * Tries to parse a JSON array, calling the event listener on
     * success.
     */
    bool _tryParseArray();

    /*
     * Tries to parse a JSON object, calling the event listener on
     * success.
     */
    bool _tryParseObj();

    /*
     * Expects the specific token `token`, appending a cause to the
     * error of the current thread and throwing `Error` if not found.
     */
    void _expectToken(const std::string_view token)
    {
        if (!_mSs.tryScanToken(token)) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_THROW(Error, _mSs.loc(), "Expecting `{}`.", token);
        }
    }

    /*
     * Calls StrScanner::tryScanLitStr() with the JSON-specific escape
     * sequence starting characters.
     */
    std::string_view _tryScanLitStr()
    {
        return _mSs.tryScanLitStr("/bfnrtu");
    }

    /*
     * Returns whether or not the current character of the underlying
     * string scanner looks like the beginning of the fractional or
     * exponent part of a constant real number.
     */
    bool _ssCurCharLikeConstRealFracOrExp() const noexcept
    {
        return *_mSs.at() == '.' || *_mSs.at() == 'E' || *_mSs.at() == 'e';
    }

private:
    /* Logging configuration */
    Logger _mLogger;

    /* Underlying string scanner */
    StrScanner _mSs;

    /* JSON event listener */
    ListenerT *_mListener;

    /* Object key sets, one for each JSON object level, to detect duplicates */
    std::vector<std::unordered_set<std::string>> _mKeys;
};

template <typename ListenerT>
JsonParser<ListenerT>::JsonParser(const std::string_view str, ListenerT& listener,
                                  const std::size_t baseOffset, const Logger& parentLogger)
    : _mLogger {parentLogger, "PARSE-JSON"},
      _mSs {str, baseOffset, parentLogger},
      _mListener {&listener}
{
    this->_parse();
}

template <typename ListenerT>
void JsonParser<ListenerT>::_expectVal()
{
    if (this->_tryParseNull()) {
        return;
    }

    if (this->_tryParseBool()) {
        return;
    }

    if (this->_tryParseStr()) {
        return;
    }

    if (this->_tryParseArray()) {
        return;
    }

    if (this->_tryParseObj()) {
        return;
    }

    if (this->_tryParseNumber()) {
        return;
    }

    BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_THROW(
        Error, _mSs.loc(),
        "Expecting a JSON value: `null`, `true`, `false`, a supported number "
        "(for an integer: -9,223,372,036,854,775,808 to 18,446,744,073,709,551,615), "
        "`\"` (a string), `[` (an array), or `{{` (an object).");
}

template <typename ListenerT>
void JsonParser<ListenerT>::_parse()
{
    /* Expect a single JSON value */
    this->_expectVal();

    /* Skip trailing whitespaces */
    _mSs.skipWhitespaces();

    /* Make sure all the text is consumed */
    if (!_mSs.isDone()) {
        BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_THROW(Error, _mSs.loc(),
                                                   "Extra data after parsed JSON value.");
    }
}

template <typename ListenerT>
bool JsonParser<ListenerT>::_tryParseNull()
{
    _mSs.skipWhitespaces();

    const auto loc = _mSs.loc();

    if (_mSs.tryScanToken("null")) {
        _mListener->onNull(loc);
        return true;
    }

    return false;
}

template <typename ListenerT>
bool JsonParser<ListenerT>::_tryParseBool()
{
    _mSs.skipWhitespaces();

    const auto loc = _mSs.loc();

    if (_mSs.tryScanToken("true")) {
        _mListener->onScalarVal(true, loc);
        return true;
    } else if (_mSs.tryScanToken("false")) {
        _mListener->onScalarVal(false, loc);
        return true;
    }

    return false;
}

template <typename ListenerT>
bool JsonParser<ListenerT>::_tryParseNumber()
{
    _mSs.skipWhitespaces();

    const auto loc = _mSs.loc();

    /*
     * The `_mSs.tryScanConstReal()` call below is somewhat expensive
     * currently because it involves executing a regex to confirm the
     * constant real number form.
     *
     * The strategy below is to:
     *
     * 1. Keep the current position P of the string scanner.
     *
     * 2. Call `_mSs.tryScanConstUInt()` and
     *    `_mSs.tryScanConstSInt()` first.
     *
     *    If either one succeeds, make sure the scanned JSON number
     *    can't be in fact a real number. If it can, then reset the
     *    position of the string scanner to P. It's safe to reset the
     *    string scanner position at this point because
     *    `_mSs.skipWhitespaces()` was called above and the constant
     *    number scanning methods won't scan a newline character.
     *
     * 3. Call `_mSs.tryScanConstReal()` last.
     */
    const auto at = _mSs.at();

    if (const auto uIntVal = _mSs.tryScanConstUInt()) {
        if (!this->_ssCurCharLikeConstRealFracOrExp()) {
            /* Confirmed unsigned integer form */
            _mListener->onScalarVal(*uIntVal, loc);
            return true;
        }

        /* Looks like a constant real number: backtrack */
        _mSs.at(at);
    } else if (const auto sIntVal = _mSs.tryScanConstSInt()) {
        if (!this->_ssCurCharLikeConstRealFracOrExp()) {
            /* Confirmed signed integer form */
            _mListener->onScalarVal(*sIntVal, loc);
            return true;
        }

        /* Looks like a constant real number: backtrack */
        _mSs.at(at);
    }

    if (const auto realVal = _mSs.tryScanConstReal()) {
        _mListener->onScalarVal(*realVal, loc);
        return true;
    }

    return false;
}

template <typename ListenerT>
bool JsonParser<ListenerT>::_tryParseStr()
{
    _mSs.skipWhitespaces();

    const auto loc = _mSs.loc();

    if (const auto str = this->_tryScanLitStr(); str.data()) {
        _mListener->onScalarVal(str, loc);
        return true;
    }

    return false;
}

template <typename ListenerT>
bool JsonParser<ListenerT>::_tryParseObjKey()
{
    _mSs.skipWhitespaces();

    const auto loc = _mSs.loc();

    if (const auto str = this->_tryScanLitStr(); !str.empty()) {
        /* _tryParseObj() pushes */
        BT_ASSERT(!_mKeys.empty());

        /* Insert, checking for duplicate key */
        if (auto [it, inserted] = _mKeys.back().emplace(str); !inserted) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_THROW(Error, _mSs.loc(),
                                                       "Duplicate JSON object key `{}`.", str);
        }

        _mListener->onObjKey(str, loc);
        return true;
    }

    return false;
}

template <typename ListenerT>
bool JsonParser<ListenerT>::_tryParseArray()
{
    _mSs.skipWhitespaces();

    const auto loc = _mSs.loc();

    if (!_mSs.tryScanToken("[")) {
        return false;
    }

    /* Beginning of array */
    _mListener->onArrayBegin(loc);

    if (_mSs.tryScanToken("]")) {
        /* Empty array */
        _mListener->onArrayEnd(loc);
        return true;
    }

    while (true) {
        /* Expect array element */
        this->_expectVal();

        if (!_mSs.tryScanToken(",")) {
            /* No more array elements */
            break;
        }
    }

    /* End of array */
    this->_expectToken("]");
    _mListener->onArrayEnd(loc);
    return true;
}

template <typename ListenerT>
bool JsonParser<ListenerT>::_tryParseObj()
{
    _mSs.skipWhitespaces();

    const auto loc = _mSs.loc();

    if (!_mSs.tryScanToken("{")) {
        return false;
    }

    /* Beginning of object */
    _mListener->onObjBegin(loc);

    if (_mSs.tryScanToken("}")) {
        /* Empty object */
        _mListener->onObjEnd(loc);
        return true;
    }

    /* New level of object keys */
    _mKeys.push_back({});

    while (true) {
        /* Expect object key */
        _mSs.skipWhitespaces();

        if (!this->_tryParseObjKey()) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_THROW(
                Error, _mSs.loc(), "Expecting a JSON object key (double-quoted string).");
        }

        /* Expect colon */
        this->_expectToken(":");

        /* Expect entry value */
        this->_expectVal();

        if (!_mSs.tryScanToken(",")) {
            /* No more entries */
            break;
        }
    }

    /* End of object */
    BT_ASSERT(!_mKeys.empty());
    _mKeys.pop_back();
    this->_expectToken("}");
    _mListener->onObjEnd(loc);
    return true;
}

} /* namespace internal */

/*!
@brief
    Parses the JSON text \bt_p{str} encoding a single JSON value,
    calling methods of \bt_p{listener} for each JSON "event".

@ingroup common-cpp-bt2c-json

This function is similar to the
<a href="https://lloyd.github.io/yajl/">yajl</a> approach, but:

- It supports <code>unsigned long long</code> and <code>long long</code>
  integers instead of the user having to make their own conversion
  from text.

- It provides the original text location for all JSON events.

  This can help make error reporting much more accurate for the
  end user.

The requirements of \bt_p{ListenerT} are the following public methods:

@code{.cpp}
void onNull(const bt2c::TextLoc&);
void onScalarVal(bool, const bt2c::TextLoc&);
void onScalarVal(unsigned long long, const bt2c::TextLoc&);
void onScalarVal(long long, const bt2c::TextLoc&);
void onScalarVal(double, const bt2c::TextLoc&);
void onScalarVal(std::string_view, const bt2c::TextLoc&);
void onArrayBegin(const bt2c::TextLoc&);
void onArrayEnd(const bt2c::TextLoc&);
void onObjBegin(const bt2c::TextLoc&);
void onObjKey(std::string_view, const bt2c::TextLoc&);
void onObjEnd(const bt2c::TextLoc&);
@endcode

The received text location always indicate the location of the
\em beginning of the text representing the corresponding JSON value.
This function adds \bt_p{baseOffset} to the offset of each
text location it creates.

This function honours the grammar of
<a href="https://www.json.org/"><code>json.org</code></a>, \em not
parsing special floating-point number tokens (\c nan, \c inf, and the
rest) or C-style comments.

@note
    You must use this function in libbabeltrace2 context because it
    appends causes to the error of the current thread and throws
    bt2c::Error on error.

@param[in] str
    JSON text to parse, encoding a single JSON value.
@param[in] listener
    JSON event listener.
@param[in] baseOffset
    Value to add to the offset of any created text location during
    parsing.
@param[in] parentLogger
    Parent logger to use on error.

@sa parseJson(std::string_view, std::size_t, const Logger&)
*/
template <typename ListenerT>
void parseJson(const std::string_view str, ListenerT& listener, const std::size_t baseOffset,
               const Logger& parentLogger)
{
    internal::JsonParser {str, listener, baseOffset, parentLogger};
}

/*!
@brief
    Overload of
    parseJson(std::string_view, ListenerT&, std::size_t, const Logger&)
    with \bt_p{baseOffset} set to&nbsp;0.

@ingroup common-cpp-bt2c-json

@param[in] str
    See parseJson(std::string_view, ListenerT&, std::size_t, const Logger&).
@param[in] listener
    See parseJson(std::string_view, ListenerT&, std::size_t, const Logger&).
@param[in] parentLogger
    See parseJson(std::string_view, ListenerT&, std::size_t, const Logger&).
*/
template <typename ListenerT>
void parseJson(const std::string_view str, ListenerT& listener, const Logger& parentLogger)
{
    parseJson(str, listener, 0, parentLogger);
}

} /* namespace bt2c */

#endif /* BABELTRACE_CPP_COMMON_BT2C_PARSE_JSON_HPP */
