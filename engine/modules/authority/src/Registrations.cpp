#include "engine/authority/Registrations.hpp"

#include "engine/authority/AuthorityStrategy.hpp"
#include "engine/base/Sha256.hpp"

#include <cstddef>
#include <string>
#include <utility>

namespace GYO::Engine::Authority {

void AuthorityRegistrations::Open() const {
    GYO_ASSERT(!buffer_->Frozen() && "an authority registration after Build: the ModuleBuffer is frozen");
}

DataRecord& AuthorityRegistrations::Append(DataRecord record) {
    Open();
    record.seq = next_++;
    data_.push_back(std::move(record));
    return data_.back();
}
MarkRecord& AuthorityRegistrations::Append(MarkRecord record) {
    Open();
    record.seq = next_++;
    marks_.push_back(std::move(record));
    return marks_.back();
}
void AuthorityRegistrations::Append(IngressRecord record) {
    Open();
    ingress_.push_back(std::move(record));
}
void AuthorityRegistrations::Append(RunsRecord record) {
    Open();
    runs_.push_back(std::move(record));
}
void AuthorityRegistrations::Append(OneEndRecord record) {
    Open();
    oneEnd_.push_back(std::move(record));
}

void HashValue(Kernel::Hasher& h, const AuthorityRegistry& registry) {
    HashValue(h, registry.data.size());
    for (const DataRecord& record : registry.data) {
        HashValue(h, record.name);
        HashValue(h, record.kind);
        HashValue(h, record.options.input);
        HashValue(h, record.options.digestExclude);
        HashValue(h, record.options.rollback);
        HashValue(h, record.capacity);
        HashValue(h, record.seq);
    }
    HashValue(h, registry.marks.size());
    for (const MarkRecord& record : registry.marks) {
        HashValue(h, record.name);
        HashValue(h, record.key.has_value());
        HashValue(h, record.seq);
    }
    HashValue(h, registry.ingress.size());
    for (const IngressRecord& record : registry.ingress) HashValue(h, record.system);
    HashValue(h, registry.runs.size());
    for (const RunsRecord& record : registry.runs) {
        HashValue(h, record.system);
        HashValue(h, record.schedules);
    }
    HashValue(h, registry.oneEnd.size());
    for (const OneEndRecord& record : registry.oneEnd) {
        HashValue(h, record.system);
        HashValue(h, record.reason);
    }
}

namespace Detail {

std::uint64_t CellDigest(std::uint64_t index, const Kernel::Hasher& value) {
    Kernel::Hasher h;
    h.U64(index);
    h.Bytes(value.Data());
    // The first 16 hex digits of the SHA-256 (GYO::Engine::Base::Sha256 is lowercase hex).
    const std::string hex = Base::Sha256(h.Data());
    std::uint64_t out = 0;
    for (std::size_t i = 0; i < 16; ++i) {
        const char c = hex[i];
        out = (out << 4U) | static_cast<std::uint64_t>(c <= '9' ? c - '0' : c - 'a' + 10);
    }
    return out;
}

} // namespace Detail

void RegisterContext::RegisterInKernel() {
    GYO_ASSERT(data_ != nullptr && "RegisterContext::RegisterInKernel: not a data registration");
    data_->ops->registerInKernel(setup_->Registrar(), data_->name, data_->capacity);
}

void RegisterContext::RegisterInRollback() {
    GYO_ASSERT(data_ != nullptr && "RegisterContext::RegisterInRollback: not a data registration");
    if (data_->options.input) {
        // A decision input: its recorded history, not rollback state (Z8). Self and NoRollback: the author's own.
        if (data_->ops->recordInput != nullptr && data_->options.rollback == RollbackChoice::Rollback) data_->ops->recordInput(rollback_);
        return;
    }
    if (data_->ops->registerInRollback == nullptr) return;
    data_->ops->registerInRollback(rollback_, data_->options.rollback);
}

void RegisterContext::AddIngress() {
    GYO_ASSERT(ingressOptions_ != nullptr && "RegisterContext::AddIngress: not an entry registration");
    GYO_ASSERT(!ingressAdded_ && "RegisterContext::AddIngress: the entry is added once");
    ingressAdded_ = true;
    addIngress_(setup_->Registrar(), *ingressOptions_, ingressSource_);
}

AuthorityEntry::AuthorityEntry(Orchestration::WorldSetup& setup) : setup_(&setup) {
    table_ = setup.Buffer().Find<AuthorityRegistrations>();
    GYO_ASSERT(table_ != nullptr && "authority registrations need the authority Module in this World");
    const AuthorityOptions* options = setup.Options<AuthorityOptions>();
    strategy_ = options != nullptr ? options->strategy.get() : nullptr;
}

void AuthorityEntry::Data(DataRecord record) {
    const DataRecord& kept = table_->Append(std::move(record));
    if (strategy_ == nullptr) {
        // No strategy (AU-E2 at Check): the type still goes into the kernel, so a System that uses it is not the
        // kernel's R-E4 hiding the real cause (authority-rollback declaration 4.2).
        kept.ops->registerInKernel(setup_->Registrar(), kept.name, kept.capacity);
        return;
    }
    RegisterContext context(*setup_);
    context.data_ = &kept;
    strategy_->OnRegister(context);
}

void AuthorityEntry::MarkType(MarkRecord record) {
    const MarkRecord& kept = table_->Append(std::move(record));
    if (strategy_ == nullptr) return;
    RegisterContext context(*setup_);
    context.mark_ = &kept;
    strategy_->OnRegister(context);
}

void AuthorityEntry::MarkResource(std::string name) {
    MarkType(MarkRecord{std::nullopt, std::move(name), nullptr, 0});
}

void AuthorityEntry::IngressOf(Kernel::SystemOptions options,
                               Kernel::RegistrationSeq (*add)(Kernel::Registrar&, Kernel::SystemOptions, std::source_location),
                               std::source_location source) {
    table_->Append(IngressRecord{options.name});
    const IngressRecord& kept = table_->Ingress().back();
    if (strategy_ == nullptr) {
        add(setup_->Registrar(), std::move(options), source);
        return;
    }
    RegisterContext context(*setup_);
    context.ingress_ = &kept;
    context.ingressOptions_ = &options;
    context.addIngress_ = add;
    context.ingressSource_ = source;
    strategy_->OnRegister(context);
}

void AuthorityEntry::Runs(std::string system, std::vector<std::string> schedules) {
    table_->Append(RunsRecord{std::move(system), std::move(schedules)});
    if (strategy_ == nullptr) return;
    RegisterContext context(*setup_);
    context.runs_ = &table_->Runs().back();
    strategy_->OnRegister(context);
}

void AuthorityEntry::OneEndOnly(std::string system, std::string reason) {
    table_->Append(OneEndRecord{std::move(system), std::move(reason)});
    if (strategy_ == nullptr) return;
    RegisterContext context(*setup_);
    context.oneEnd_ = &table_->OneEnd().back();
    strategy_->OnRegister(context);
}

} // namespace GYO::Engine::Authority
