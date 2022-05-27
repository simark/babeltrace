/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2022 Francis Deslauriers <francis.deslauriers@efficios.com>
 */

#ifndef CTF_COMMON_SRC_METADATA_CTF_IR_GENERATOR_HPP
#define CTF_COMMON_SRC_METADATA_CTF_IR_GENERATOR_HPP

#include "cpp-common/optional.hpp"

#include "../../logging/log-cfg.hpp"
#include "../../metadata/ctf-ir.hpp"
#include "ctf-ir.hpp"
#include "tsdl/ctf-meta.hpp"
#include "tsdl/metadata-stream-decoder.hpp"
#include "tsdl/scanner.hpp"

namespace ctf {
namespace src {

struct Ctx
{
    TraceCls *newTC;
    DataStreamCls *newDSC;

    struct ctf_trace_class *oldTC;
    struct ctf_stream_class *oldDSC;
    struct ctf_event_class *oldEC;
};

class CtfIrGenerator final
{
public:
    explicit CtfIrGenerator(const LogCfg logCfg, ClkClsCfg clkClsCfg);

    void appendContent(const uint8_t *data, bt2_common::DataLen len);

    TraceCls *ctfTraceCls() const noexcept
    {
        return _mNewTraceCls.get();
    }

    nonstd::optional<bt2::TraceClass> irTraceCls() const noexcept
    {
        return _mNewTraceCls->libCls();
    }

private:
    void _translateToNewCtfIr(ctf_trace_class& oldCtfTraceClass);
    bt2::MapValue::Shared _translateEnv(ctf_trace_class& oldTc);
    std::unique_ptr<TraceCls> _translateTraceCls(Ctx& ctx);
    std::shared_ptr<ClkCls> _translateClkCls(const ctf_clock_class *oldClkCls);
    void _translateDataStreamCls(Ctx& ctx);
    void _translateEventCls(Ctx& ctx);

    static void _destroyCtfScanner(ctf_scanner *scanner)
    {
        ctf_scanner_free(scanner);
    }

    LogCfg _mLogCfg;

    /* Created CTF trace class. */
    std::unique_ptr<TraceCls> _mNewTraceCls;

    /*
     * Map from the "old" IR ctf_clock_class objects to "new" IR ClkCls
     * objects.
     */
    std::unordered_map<const ctf_clock_class *, std::shared_ptr<ClkCls>> _mNewClkClasses;

    /* Old IR generator. */
    ctf_visitor_generate_ir::UP _mOldCtfGenerator;
    std::unique_ptr<ctf_scanner, decltype(&_destroyCtfScanner)> _mScanner;

    MetadataStreamDecoder _mTsdlMetadataStreamDecoder;

    using _FileUP = std::unique_ptr<std::FILE, decltype(&std::fclose)>;
    _FileUP _createFileHandleOnStringContent(const std::string& text);
};

} /* namespace src */
} /* namespace ctf */

#endif /* CTF_COMMON_SRC_METADATA_CTF_IR_GENERATOR_HPP */
