/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2022 Francis Deslauriers <francis.deslauriers@efficios.com>
 */
#define BT_COMP_LOG_SELF_COMP       (_mLogCfg.selfComp)
#define BT_COMP_LOG_SELF_COMP_CLASS (_mLogCfg.selfCompClass)
#define BT_LOG_OUTPUT_LEVEL         (_mLogCfg.logLevel)
#define BT_LOG_TAG                  "PLUGIN/CTF/META/NEW-CTF-IR-GEN"

#include "logging/comp-logging.h"

#include "ctf-ir-generator.hpp"

#include "compat/memstream.h"

#include "cpp-common/comp-logging.hpp"
#include "cpp-common/exc.hpp"
#include <cpp-common/optional.hpp>

#include "../../logging/log-cfg.hpp"
#include "finalize-trace-cls.hpp"

namespace ctf {
namespace src {

static inline ir::DispBase translateDispBase(bt_field_class_integer_preferred_display_base dispBase)
{
    return static_cast<ir::DispBase>(dispBase);
}

static inline ir::ByteOrder translateByteOrder(ctf_byte_order oldBO)
{
    switch (oldBO) {
    case CTF_BYTE_ORDER_LITTLE:
        return ir::ByteOrder::LITTLE;
        break;
    case CTF_BYTE_ORDER_BIG:
        return ir::ByteOrder::BIG;
        break;
    default:
        bt_common_abort();
    }
}

static inline nonstd::optional<ir::UIntFieldRole> _translateMeaning(ctf_field_class_meaning meaning)
{
    switch (meaning) {
    case CTF_FIELD_CLASS_MEANING_PACKET_BEGINNING_TIME:
        return {ir::UIntFieldRole::DEF_CLK_TS};
    case CTF_FIELD_CLASS_MEANING_PACKET_END_TIME:
        return {ir::UIntFieldRole::PKT_END_DEF_CLK_TS};
    case CTF_FIELD_CLASS_MEANING_EVENT_CLASS_ID:
        return {ir::UIntFieldRole::EVENT_RECORD_CLS_ID};
    case CTF_FIELD_CLASS_MEANING_STREAM_CLASS_ID:
        return {ir::UIntFieldRole::DATA_STREAM_CLS_ID};
    case CTF_FIELD_CLASS_MEANING_DATA_STREAM_ID:
        return {ir::UIntFieldRole::DATA_STREAM_ID};
    case CTF_FIELD_CLASS_MEANING_MAGIC:
        return {ir::UIntFieldRole::PKT_MAGIC_NUMBER};
    case CTF_FIELD_CLASS_MEANING_PACKET_COUNTER_SNAPSHOT:
        return {ir::UIntFieldRole::PKT_SEQ_NUM};
    case CTF_FIELD_CLASS_MEANING_DISC_EV_REC_COUNTER_SNAPSHOT:
        return {ir::UIntFieldRole::DISC_EVENT_RECORD_COUNTER_SNAP};
    case CTF_FIELD_CLASS_MEANING_EXP_PACKET_TOTAL_SIZE:
        return {ir::UIntFieldRole::PKT_TOTAL_LEN};
    case CTF_FIELD_CLASS_MEANING_EXP_PACKET_CONTENT_SIZE:
        return {ir::UIntFieldRole::PKT_CONTENT_LEN};
    case CTF_FIELD_CLASS_MEANING_UUID:
    case CTF_FIELD_CLASS_MEANING_NONE:
        return nonstd::nullopt;
    default:
        bt_common_abort();
    }
}
static inline ir::UIntFieldRoles translateUIntMeaning(const ctf_field_class_int *oldIntFc)
{
    ir::UIntFieldRoles allRoles;

    auto role = _translateMeaning(oldIntFc->meaning);

    if (role) {
        allRoles.insert(*role);
    }

    if (oldIntFc->mapped_clock_class) {
        allRoles.insert(ir::UIntFieldRole::DEF_CLK_TS);
    }

    return allRoles;
}

static Fc::UP translateFc(Ctx& ctx, ctf_field_class *oldFc);

static std::unique_ptr<Fc> translateIntFc(Ctx& ctx, const ctf_field_class_int *oldFc)
{
    if (oldFc->is_signed) {
        return createFixedLenSIntFc(
            oldFc->base.base.alignment, bt2_common::DataLen::fromBits(oldFc->base.size),
            translateByteOrder(oldFc->base.byte_order), translateDispBase(oldFc->disp_base));
    } else {
        auto roles = translateUIntMeaning(oldFc);

        return createFixedLenUIntFc(
            oldFc->base.base.alignment, bt2_common::DataLen::fromBits(oldFc->base.size),
            translateByteOrder(oldFc->base.byte_order), translateDispBase(oldFc->disp_base), roles);
    }
}

template <typename EnumT>
static typename EnumT::Mappings translateEnumMappings(Ctx& ctx, const ctf_field_class_enum *oldFc)
{
    using Mappings = typename EnumT::Mappings;

    Mappings mappings;

    for (auto mappingIdx = 0; mappingIdx < oldFc->mappings->len; mappingIdx++) {
        const auto mapping = ctf_field_class_enum_borrow_mapping_by_index_const(oldFc, mappingIdx);

        std::set<typename Mappings::mapped_type::Range> ranges;

        for (auto rangeIdx = 0; rangeIdx < mapping->ranges->len; rangeIdx++) {
            const auto range =
                ctf_field_class_enum_mapping_borrow_range_by_index_const(mapping, rangeIdx);

            ranges.emplace(static_cast<typename EnumT::Val>(range->lower.u),
                           static_cast<typename EnumT::Val>(range->upper.u));
        }

        typename Mappings::mapped_type rangeSet {ranges};

        mappings.insert({mapping->label->str, rangeSet});
    }

    return mappings;
}

static Fc::UP translateEnumFc(Ctx& ctx, const ctf_field_class_enum *oldFc)
{
    auto byteOrder = translateByteOrder(oldFc->base.base.byte_order);
    auto dispBase = translateDispBase(oldFc->base.disp_base);

    if (oldFc->base.is_signed) {
        auto mappings = translateEnumMappings<FixedLenSEnumFc>(ctx, oldFc);

        return createFixedLenSEnumFc(oldFc->base.base.base.alignment,
                                     bt2_common::DataLen::fromBits(oldFc->base.base.size),
                                     byteOrder, mappings, dispBase);
    } else {
        auto roles = translateUIntMeaning(&oldFc->base);
        auto mappings = translateEnumMappings<FixedLenUEnumFc>(ctx, oldFc);

        return createFixedLenUEnumFc(oldFc->base.base.base.alignment,
                                     bt2_common::DataLen::fromBits(oldFc->base.base.size),
                                     byteOrder, mappings, dispBase, roles);
    }
}
static Fc::UP translateFloatFc(Ctx& ctx, const ctf_field_class_float *oldFc)
{
    return createFixedLenFloatFc(oldFc->base.base.alignment,
                                 bt2_common::DataLen::fromBits(oldFc->base.size),
                                 translateByteOrder(oldFc->base.byte_order));
}

static Fc::UP translateStringFc(Ctx& ctx, const ctf_field_class_string *oldFc)
{
    return createNullTerminatedStrFc();
}

static Fc::UP translateStructFc(Ctx& ctx, const ctf_field_class_struct *oldFc)
{
    StructFc::MemberClasses members;

    for (auto i = 0; i < oldFc->members->len; i++) {
        const auto namedFc = ctf_field_class_struct_borrow_member_by_index_const(oldFc, i);
        const auto name = namedFc->name->str;

        auto member = createStructFieldMemberCls(name, translateFc(ctx, namedFc->fc));
        members.emplace_back(std::move(member));
    }

    return createStructFc(std::move(members), oldFc->base.alignment);
}
static Fc::UP translateArrayFc(Ctx& ctx, const ctf_field_class_array *oldFc)
{
    if (oldFc->base.is_text) {
        return createStaticLenStrFc(oldFc->length);
    }

    const auto isUuid = oldFc->meaning == CTF_FIELD_CLASS_MEANING_UUID;

    return createStaticLenArrayFc(oldFc->length, translateFc(ctx, oldFc->base.elem_fc),
                                  oldFc->base.base.alignment, nonstd::nullopt, isUuid);
}

static FieldLoc translateFieldPath(Ctx& ctx, const ctf_field_path *fieldPath)
{
    struct ctf_field_class *fc;
    FieldLoc::Items items;
    ir::FieldLocScope scope;

    switch (fieldPath->root) {
    case CTF_SCOPE_PACKET_HEADER:
        fc = ctx.oldTC->packet_header_fc;
        scope = ir::FieldLocScope::PKT_HEADER;
        break;
    case CTF_SCOPE_PACKET_CONTEXT:
        fc = ctx.oldDSC->packet_context_fc;
        scope = ir::FieldLocScope::PKT_CTX;
        break;
    case CTF_SCOPE_EVENT_HEADER:
        fc = ctx.oldDSC->event_header_fc;
        scope = ir::FieldLocScope::EVENT_RECORD_HEADER;
        break;
    case CTF_SCOPE_EVENT_COMMON_CONTEXT:
        fc = ctx.oldDSC->event_common_context_fc;
        scope = ir::FieldLocScope::EVENT_RECORD_COMMON_CTX;
        break;
    case CTF_SCOPE_EVENT_SPECIFIC_CONTEXT:
        fc = ctx.oldEC->spec_context_fc;
        scope = ir::FieldLocScope::EVENT_RECORD_SPEC_CTX;
        break;
    case CTF_SCOPE_EVENT_PAYLOAD:
        fc = ctx.oldEC->payload_fc;
        scope = ir::FieldLocScope::EVENT_RECORD_PAYLOAD;
        break;
    default:
        bt_common_abort();
    }

    for (auto i = 0; i < fieldPath->path->len; i++) {
        switch (fc->type) {
        case CTF_FIELD_CLASS_TYPE_SEQUENCE:
        case CTF_FIELD_CLASS_TYPE_ARRAY:
        {
            auto childIdx = ctf_field_path_borrow_index_by_index(fieldPath, i);

            BT_ASSERT(childIdx == -1);
            fc = ctf_field_class_as_array_base(fc)->elem_fc;
            break;
        }
        case CTF_FIELD_CLASS_TYPE_STRUCT:
        {
            auto childIdx = ctf_field_path_borrow_index_by_index(fieldPath, i);

            auto child_fc =
                ctf_field_class_compound_borrow_named_field_class_by_index(fc, childIdx);

            BT_ASSERT_DBG(child_fc);

            items.emplace_back(child_fc->name->str);
            fc = child_fc->fc;
            break;
        }
        case CTF_FIELD_CLASS_TYPE_VARIANT:
        {
            auto childIdx = ctf_field_path_borrow_index_by_index(fieldPath, i);
            auto child_fc =
                ctf_field_class_compound_borrow_named_field_class_by_index(fc, childIdx);

            BT_ASSERT_DBG(child_fc);

            /*
             * For variant Fc, we omit adding the option's name to the Field
             * location. It's assumed that it's the one currently selected.
             * We still need to continue traversing the variant's FC.
             */
            fc = child_fc->fc;
            break;
        }
        default:
            break;
        }
    }

    return createFieldLoc(scope, std::move(items));
}

static Fc::UP translateSequenceFc(Ctx& ctx, const ctf_field_class_sequence *oldFc)
{
    auto fieldLoc = translateFieldPath(ctx, &oldFc->length_path);

    if (oldFc->base.is_text) {
        return createDynLenStrFc(fieldLoc);
    }

    auto elemFc = translateFc(ctx, oldFc->base.elem_fc);

    return createDynLenArrayFc(fieldLoc, std::move(elemFc), 1);
}

template <typename VariantT>
typename VariantT::Opt createVariantOpt(Fc::UP fc, typename VariantT::SelFieldRanges selFieldRanges,
                                        nonstd::optional<std::string> name);

template <>
inline VariantWithUIntSelFc::Opt
createVariantOpt<VariantWithUIntSelFc>(Fc::UP fc, UIntRangeSet selFieldRanges,
                                       nonstd::optional<std::string> name)
{
    return createVariantFcOpt(std::move(fc), std::move(selFieldRanges), std::move(name));
}

template <>
inline VariantWithSIntSelFc::Opt
createVariantOpt<VariantWithSIntSelFc>(Fc::UP fc, SIntRangeSet selFieldRanges,
                                       nonstd::optional<std::string> name)
{
    return createVariantFcOpt(std::move(fc), std::move(selFieldRanges), std::move(name));
}

template <typename VariantT>
static typename VariantT::Opts translateVarOpts(Ctx& ctx, const ctf_field_class_variant *oldFc)
{
    typename VariantT::Opts opts;

    for (auto i = 0; i < oldFc->options->len; i++) {
        const auto namedFc = ctf_field_class_variant_borrow_option_by_index_const(oldFc, i);
        const auto oldRange = ctf_field_class_variant_borrow_range_by_index_const(oldFc, i);

        typename VariantT::SelFieldRanges::Set ranges;

        ranges.emplace(static_cast<typename VariantT::SelVal>(oldRange->range.lower.u),
                       static_cast<typename VariantT::SelVal>(oldRange->range.upper.u));

        opts.push_back(createVariantOpt<VariantT>(translateFc(ctx, namedFc->fc),
                                                  typename VariantT::SelFieldRanges {ranges},
                                                  namedFc->name->str));
    }

    return opts;
}

static Fc::UP translateVariantFc(Ctx& ctx, const ctf_field_class_variant *oldFc)
{
    auto fieldLoc = translateFieldPath(ctx, &oldFc->tag_path);

    if (oldFc->tag_fc->base.is_signed) {
        return createVariantFc(translateVarOpts<VariantWithSIntSelFc>(ctx, oldFc), fieldLoc);
    } else {
        return createVariantFc(translateVarOpts<VariantWithUIntSelFc>(ctx, oldFc), fieldLoc);
    }
}

static Fc::UP translateFc(Ctx& ctx, ctf_field_class *oldFc)
{
    switch (oldFc->type) {
    case CTF_FIELD_CLASS_TYPE_INT:
        return translateIntFc(ctx, ctf_field_class_as_int(oldFc));
    case CTF_FIELD_CLASS_TYPE_ENUM:
        return translateEnumFc(ctx, ctf_field_class_as_enum(oldFc));
    case CTF_FIELD_CLASS_TYPE_FLOAT:
        return translateFloatFc(ctx, ctf_field_class_as_float(oldFc));
    case CTF_FIELD_CLASS_TYPE_STRING:
        return translateStringFc(ctx, ctf_field_class_as_string(oldFc));
    case CTF_FIELD_CLASS_TYPE_STRUCT:
        return translateStructFc(ctx, ctf_field_class_as_struct(oldFc));
    case CTF_FIELD_CLASS_TYPE_ARRAY:
        return translateArrayFc(ctx, ctf_field_class_as_array(oldFc));
    case CTF_FIELD_CLASS_TYPE_SEQUENCE:
        return translateSequenceFc(ctx, ctf_field_class_as_sequence(oldFc));
    case CTF_FIELD_CLASS_TYPE_VARIANT:
        return translateVariantFc(ctx, ctf_field_class_as_variant(oldFc));
    default:
        bt_common_abort();
    }
}

bt2::MapValue::Shared CtfIrGenerator::_translateEnv(ctf_trace_class& oldTc)
{
    const auto& logCfg = _mLogCfg;
    bt2::MapValue::Shared env = bt2::MapValue::create();

    BT_COMP_OR_COMP_CLASS_LOGD(logCfg.selfComp, logCfg.selfCompClass,
                               "Translating trace Env entries");

    for (auto i = 0; i < oldTc.env_entries->len; i++) {
        const auto envEntry = ctf_trace_class_borrow_env_entry_by_index(&oldTc, i);

        switch (envEntry->type) {
        case CTF_TRACE_CLASS_ENV_ENTRY_TYPE_INT:
            env->insert(envEntry->name->str, envEntry->value.i);
            break;
        case CTF_TRACE_CLASS_ENV_ENTRY_TYPE_STR:
            env->insert(envEntry->name->str, envEntry->value.str->str);
            break;
        default:
            bt_common_abort();
        }
    }

    return env;
}

CtfIrGenerator::_FileUP CtfIrGenerator::_createFileHandleOnStringContent(const std::string& text)
{
    auto fp = bt_fmemopen((void *) text.data(), text.length(), "rb");

    if (!fp) {
        BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE_AND_THROW(
            bt2_common::Error, _mLogCfg.selfComp, _mLogCfg.selfCompClass,
            "Error openning std::FILE to a string buffer");
    }

    return _FileUP {fp, &std::fclose};
}

void CtfIrGenerator::appendContent(const uint8_t *data, bt2_common::DataLen len)
{
    const auto plaintextMetadata = _mTsdlMetadataStreamDecoder.decode(data, len);

    auto newPlaintextMetadataFp = this->_createFileHandleOnStringContent(plaintextMetadata);

    /* Append the metadata text content */
    auto ret = ctf_scanner_append_ast(_mScanner.get(), newPlaintextMetadataFp.get());

    if (ret) {
        BT_COMP_LOGE_APPEND_CAUSE_AND_THROW(
            bt2_common::Error, _mLogCfg.selfComp,
            "Cannot create the metadata AST out of the plaintext metadata");
    }

    ret = ctf_visitor_semantic_check(0, &_mScanner.get()->ast->root, _mLogCfg);
    if (ret) {
        BT_COMP_LOGE_APPEND_CAUSE_AND_THROW(bt2_common::Error, _mLogCfg.selfComp,
                                            "Validation of the metadata semantics failed");
    }

    ret = ctf_visitor_generate_ir_visit_node(_mOldCtfGenerator.get(), &_mScanner.get()->ast->root);
    switch (ret) {
    case 0:
        /* Success */
        break;
    case -EINCOMPLETE:
        BT_COMP_LOGD("While visiting metadata AST: incomplete data");
        throw bt2_common::Error {};

    default:
        BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE_AND_THROW(
            bt2_common::Error, _mLogCfg.selfComp, _mLogCfg.selfCompClass,
            "Failed to visit AST node to create CTF IR objects: ret=%d", ret);
    }

    this->_translateToNewCtfIr(*_mOldCtfGenerator->ctf_tc);
    ctf::src::finalizeTraceCls(*_mNewTraceCls, _mLogCfg.selfComp);

    return;
}

void CtfIrGenerator::_translateToNewCtfIr(ctf_trace_class& oldCtfTraceCls)
{
    Ctx ctx = {0};

    ctx.oldTC = &oldCtfTraceCls;

    if (!_mNewTraceCls) {
        _mNewTraceCls = this->_translateTraceCls(ctx);
    }

    ctx.newTC = _mNewTraceCls.get();

    for (auto i = 0; i < oldCtfTraceCls.stream_classes->len; i++) {
        ctx.oldDSC = (ctf_stream_class *) ctx.oldTC->stream_classes->pdata[i];
        this->_translateDataStreamCls(ctx);

        for (auto j = 0; j < ctx.oldDSC->event_classes->len; j++) {
            ctx.oldEC = (ctf_event_class *) ctx.oldDSC->event_classes->pdata[j];
            this->_translateEventCls(ctx);
        }
    }
}

std::unique_ptr<TraceCls> CtfIrGenerator::_translateTraceCls(Ctx& ctx)
{
    Fc::UP pktHdrFc;

    if (ctx.oldTC->packet_header_fc) {
        pktHdrFc = translateFc(ctx, ctx.oldTC->packet_header_fc);
    }

    nonstd::optional<bt2_common::Uuid> uuid;
    if (ctx.oldTC->is_uuid_set) {
        uuid = ctx.oldTC->uuid;
    }

    auto newTraceCls = createTraceCls(uuid, this->_translateEnv(*ctx.oldTC), std::move(pktHdrFc));

    ctx.oldTC->is_translated = true;

    return newTraceCls;
}

std::shared_ptr<ClkCls> CtfIrGenerator::_translateClkCls(const ctf_clock_class *oldClkCls)
{
    if (!oldClkCls) {
        return nullptr;
    }

    auto clkClsIter = _mNewClkClasses.find(oldClkCls);

    if (clkClsIter != _mNewClkClasses.end()) {
        return clkClsIter->second;
    } else {
        nonstd::optional<std::string> description;
        if (oldClkCls->description->len > 0) {
            description = oldClkCls->description->str;
        }

        auto newClkCls = createClkCls(
            oldClkCls->name->str, oldClkCls->frequency,
            ir::ClkOffset {oldClkCls->offset_seconds, oldClkCls->offset_cycles},
            oldClkCls->is_absolute, std::move(description), oldClkCls->precision,
            oldClkCls->has_uuid ?
                nonstd::optional<bt2_common::Uuid> {bt2_common::Uuid {oldClkCls->uuid}} :
                nonstd::nullopt);

        _mNewClkClasses.emplace(oldClkCls, newClkCls);

        return newClkCls;
    }
}

void CtfIrGenerator::_translateDataStreamCls(Ctx& ctx)
{
    if (ctx.oldDSC->is_translated) {
        return;
    }

    ctx.oldDSC->is_translated = true;

    Fc::UP newPktCtxFc;

    if (ctx.oldDSC->packet_context_fc) {
        newPktCtxFc = translateFc(ctx, ctx.oldDSC->packet_context_fc);
    }

    Fc::UP newEvtHdrFc;

    if (ctx.oldDSC->event_header_fc) {
        newEvtHdrFc = translateFc(ctx, ctx.oldDSC->event_header_fc);
    }

    Fc::UP newEvtCommonCtxFc;

    if (ctx.oldDSC->event_common_context_fc) {
        newEvtCommonCtxFc = translateFc(ctx, ctx.oldDSC->event_common_context_fc);
    }

    auto newDataStreamCls = createDataStreamCls(
        ctx.oldDSC->id, nonstd::nullopt, nonstd::nullopt, std::move(newPktCtxFc),
        std::move(newEvtHdrFc), std::move(newEvtCommonCtxFc),
        this->_translateClkCls(ctx.oldDSC->default_clock_class));

    ctx.newDSC = newDataStreamCls.get();
    _mNewTraceCls->addDataStreamCls(std::move(newDataStreamCls));
}

static inline const char *translateLogLevel(const bt_event_class_log_level level)
{
    switch (level) {
    case BT_EVENT_CLASS_LOG_LEVEL_EMERGENCY:
        return "emergency";
    case BT_EVENT_CLASS_LOG_LEVEL_ALERT:
        return "alert";
    case BT_EVENT_CLASS_LOG_LEVEL_CRITICAL:
        return "critical";
    case BT_EVENT_CLASS_LOG_LEVEL_ERROR:
        return "error";
    case BT_EVENT_CLASS_LOG_LEVEL_WARNING:
        return "warning";
    case BT_EVENT_CLASS_LOG_LEVEL_NOTICE:
        return "notice";
    case BT_EVENT_CLASS_LOG_LEVEL_INFO:
        return "info";
    case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_SYSTEM:
        return "debug:system";
    case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_PROGRAM:
        return "debug:program";
    case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_PROCESS:
        return "debug:process";
    case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_MODULE:
        return "debug:module";
    case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_UNIT:
        return "debug:unit";
    case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_FUNCTION:
        return "debug:function";
    case BT_EVENT_CLASS_LOG_LEVEL_DEBUG_LINE:
        return "debug:line";
    case BT_EVENT_CLASS_LOG_LEVEL_DEBUG:
        return "debug";
    }

    bt_common_abort();
}

static ir::OptUserAttrs translateLogLevelAndEmfUri(const ctf_event_class *oldEC)
{
    if (!oldEC->emf_uri->len && !oldEC->is_log_level_set) {
        return nonstd::nullopt;
    }

    auto userAttr = bt2::MapValue::create();
    auto btNsEntry = userAttr->insertEmptyMap("babeltrace.org,2020");

    if (oldEC->emf_uri->len) {
        btNsEntry.insert("emf-uri", oldEC->emf_uri->str);
    }

    if (oldEC->is_log_level_set) {
        btNsEntry.insert("log-level", translateLogLevel(oldEC->log_level));
    }

    return {userAttr};
}

void CtfIrGenerator::_translateEventCls(Ctx& ctx)
{
    if (ctx.oldEC->is_translated) {
        return;
    }

    ctx.oldEC->is_translated = true;

    BT_ASSERT(ctx.newDSC);

    Fc::UP newSpecCtxFc;

    if (ctx.oldEC->spec_context_fc) {
        newSpecCtxFc = translateFc(ctx, ctx.oldEC->spec_context_fc);
    }

    Fc::UP newPayloadFc;

    if (ctx.oldEC->payload_fc) {
        newPayloadFc = translateFc(ctx, ctx.oldEC->payload_fc);
    }

    auto newEvtCls = createEventRecordCls(ctx.oldEC->id, nonstd::nullopt, ctx.oldEC->name->str,
                                          std::move(newSpecCtxFc), std::move(newPayloadFc),
                                          translateLogLevelAndEmfUri(ctx.oldEC));

    ctx.newDSC->addEventRecordCls(std::move(newEvtCls));
}

CtfIrGenerator::CtfIrGenerator(const LogCfg logCfg, ClkClsCfg clkClsCfg) :

    _mLogCfg {logCfg}, _mScanner {ctf_scanner_alloc(), this->_destroyCtfScanner},
    _mTsdlMetadataStreamDecoder {logCfg}
{
    ctf_metadata_decoder_config metadataCfg {logCfg};
    metadataCfg.clkClsCfg = clkClsCfg;
    _mOldCtfGenerator = ctf_visitor_generate_ir_create(&metadataCfg);
}

} /* namespace src */
} /* namespace ctf */
