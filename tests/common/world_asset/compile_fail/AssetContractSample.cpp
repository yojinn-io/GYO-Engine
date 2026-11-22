// Compile-failure samples of asset's public contract (declaration
// docs/architecture/target/v2/asset-declaration.zh-Hant.md, AS-10 (a), AS-11; section 10, "負向編譯的判法").
// tests/common/world_asset/CMakeLists.txt compiles this file at configure time once per variant. Each sample
// makes the check depend on a template parameter (a concept), so a missing member is not a hard error, and
// static_asserts that the forbidden property holds: it must fail, and the static assertion's failure must name
// the sample's marker on the same line (a failure for any other reason is not counted). The control
// (GYO_ASSET_SAMPLE_ACCEPTED) checks a member that does exist with the same form, and must compile.
//   HANDLE_STATE      a Handle has State()                 -> GYO_ASSET_SAMPLE_HANDLE_HAS_NO_STATE
//   HANDLE_GET        a Handle has Get()                   -> GYO_ASSET_SAMPLE_HANDLE_HAS_NO_GET
//   REQUESTS_STATE    AssetRequests has State(handle)      -> GYO_ASSET_SAMPLE_REQUESTS_HAVE_NO_STATE
//   REQUESTS_STORE    AssetRequests gives the store        -> GYO_ASSET_SAMPLE_REQUESTS_GIVE_NO_STORE
//   VIEW_GET_WRITABLE AssetView::Get gives a writable T*   -> GYO_ASSET_SAMPLE_VIEW_GET_IS_READ_ONLY
//   VIEW_MUTATION     AssetView has a mutating member      -> GYO_ASSET_SAMPLE_VIEW_HAS_NO_MUTATION
//   VIEW_COPY_ASSIGN  AssetView is copy assignable         -> GYO_ASSET_SAMPLE_VIEW_IS_NOT_COPY_ASSIGNABLE
//   VIEW_CONSTRUCT    an author can construct an AssetView -> GYO_ASSET_SAMPLE_VIEW_IS_NOT_CONSTRUCTIBLE
#include "engine/asset/AssetRef.hpp"
#include "engine/asset/AssetSnapshot.hpp"
#include "engine/asset/AssetStore.hpp"
#include "engine/asset/Handle.hpp"
#include "engine/asset/Text.hpp"
#include "engine/world_asset/AssetRequests.hpp"
#include "engine/world_asset/AssetView.hpp"

#include <concepts>
#include <type_traits>
#include <utility>

namespace {

namespace Asset = GYO::Engine::Asset;
namespace WA = GYO::Engine::WorldAsset;
using Text = Asset::Text;
using TextHandle = Asset::Handle<Text>;

// ---- Handle (AS-11): no State, no Get ----
template <class H>
concept HasState = requires(const H& handle) { handle.State(); };
template <class H>
concept HasGet = requires(const H& handle) { handle.Get(); };
template <class H>
concept HasId = requires(const H& handle) { handle.Id(); };

// ---- AssetRequests (AS-11): no State(handle), no store ----
template <class R>
concept RequestsHaveState = requires(const R& requests, const TextHandle& handle) { requests.State(handle); };
template <class R>
concept RequestsLoad = requires(const R& requests, const TextHandle& handle) { requests.template Load<Text>(handle.Id()); };
template <class R>
concept GivesStore = std::is_convertible_v<R&, Asset::AssetStore&> || std::is_convertible_v<R&, Asset::AssetStore*> ||
                     requires(R& requests) { requests.Store(); } || requires(R& requests) { requests.store; } ||
                     requires(R& requests) { requests.store_; };

// ---- AssetView (AS-10 (a)): read-only values, no mutating member, no copy assignment, not constructible ----
template <class V>
concept GetIsWritable =
    requires(const V& view, const TextHandle& handle) { { view.template Get<Text>(handle) } -> std::convertible_to<Text*>; } ||
    requires(V& view, const TextHandle& handle) { { view.template Get<Text>(handle) } -> std::convertible_to<Text*>; } ||
    requires(const V& view, const Asset::AssetRef<Text>& reference) { { view.template Get<Text>(reference) } -> std::convertible_to<Text*>; };
template <class V>
concept GetIsReadOnly =
    requires(const V& view, const TextHandle& handle) { { view.template Get<Text>(handle) } -> std::convertible_to<const Text*>; };
template <class V>
concept HasMutation = requires(V& view, Asset::AssetSnapshot snapshot) { view.Replace(std::move(snapshot)); } ||
                      requires(V& view, Asset::AssetSnapshot snapshot) { view.Set(std::move(snapshot)); } ||
                      requires(V& view, Asset::AssetStore& store) { view.Pull(store); } ||
                      requires(V& view) { view.Clear(); };
template <class V>
concept HasRead = requires(V& view, const TextHandle& handle) { view.State(handle); };

}  // namespace

#if defined(GYO_ASSET_SAMPLE_HANDLE_STATE)
static_assert(HasState<TextHandle>, "GYO_ASSET_SAMPLE_HANDLE_HAS_NO_STATE: a Handle tells nothing of the store's state");
#elif defined(GYO_ASSET_SAMPLE_HANDLE_GET)
static_assert(HasGet<TextHandle>, "GYO_ASSET_SAMPLE_HANDLE_HAS_NO_GET: a value is read through AssetView only");
#elif defined(GYO_ASSET_SAMPLE_REQUESTS_STATE)
static_assert(RequestsHaveState<WA::AssetRequests>, "GYO_ASSET_SAMPLE_REQUESTS_HAVE_NO_STATE: states are read from AssetView");
#elif defined(GYO_ASSET_SAMPLE_REQUESTS_STORE)
static_assert(GivesStore<WA::AssetRequests>, "GYO_ASSET_SAMPLE_REQUESTS_GIVE_NO_STORE: a System never reaches the store");
#elif defined(GYO_ASSET_SAMPLE_VIEW_GET_WRITABLE)
static_assert(GetIsWritable<WA::AssetView>, "GYO_ASSET_SAMPLE_VIEW_GET_IS_READ_ONLY: a loaded value is never written");
#elif defined(GYO_ASSET_SAMPLE_VIEW_MUTATION)
static_assert(HasMutation<WA::AssetView>, "GYO_ASSET_SAMPLE_VIEW_HAS_NO_MUTATION: only world_asset changes a view");
#elif defined(GYO_ASSET_SAMPLE_VIEW_COPY_ASSIGN)
static_assert(std::is_copy_assignable_v<WA::AssetView>, "GYO_ASSET_SAMPLE_VIEW_IS_NOT_COPY_ASSIGNABLE: a view is not copied over another");
#elif defined(GYO_ASSET_SAMPLE_VIEW_CONSTRUCT)
static_assert(std::is_constructible_v<WA::AssetView, Asset::AssetSnapshot> || std::is_default_constructible_v<WA::AssetView>,
              "GYO_ASSET_SAMPLE_VIEW_IS_NOT_CONSTRUCTIBLE: only world_asset makes a view");
#elif defined(GYO_ASSET_SAMPLE_ACCEPTED)
// The same forms on members that exist.
static_assert(HasId<TextHandle>, "control: Handle::Id");
static_assert(RequestsLoad<WA::AssetRequests>, "control: AssetRequests::Load");
static_assert(GetIsReadOnly<WA::AssetView>, "control: AssetView::Get gives a const T*");
static_assert(HasRead<WA::AssetView>, "control: AssetView::State");
static_assert(std::is_move_assignable_v<WA::AssetView>, "control: AssetView is move assignable");
static_assert(std::is_move_constructible_v<WA::AssetView>, "control: AssetView is move constructible");
#else
#error "define one GYO_ASSET_SAMPLE_* variant"
#endif

int GyoAssetContractSample() { return 0; }
