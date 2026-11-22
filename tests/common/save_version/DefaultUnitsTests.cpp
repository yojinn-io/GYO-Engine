// V7 (restore-entry declaration section 19.5): the unit table gyo_product_units generates from the real register
// (config/modules.csv) does not carry save_version (default=no), so a product that does not list it has no Policy.
#include <doctest/doctest.h>

#include <string>
#include <utility>

#include "engine/kernel/Kernel.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/save_version/SaveVersionModule.hpp"

namespace O = GYO::Engine::Orchestration;
namespace V = GYO::Engine::SaveVersion;

TEST_CASE("default: the real register does not carry save_version; no Policy in the Product's buffer") {
    O::ProductDescription d;
    d.name = "defaults";
    d.options.Set(V::SaveVersionOptions{5, 4});
    O::WorldDescription w;
    w.name = "Main";
    d.worlds.push_back(std::move(w));
    auto built = O::Product::Build(std::move(d), O::MakeRegisteredUnits());
    REQUIRE_MESSAGE(static_cast<bool>(built), (built ? std::string() : built.error().detail));
    CHECK_MESSAGE(std::as_const((*built)->Buffer()).Find<V::Policy>() == nullptr, "[sv-default-off] save_version is carried by default");
}
