#pragma once

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

// A map kept as a sorted std::vector of pairs, for the containers inside the net Module's ECS components. A component
// must be nothrow move constructible, move assignable and destructible (Kernel spec W-8, Registrar::Component), and
// std::map, std::deque and std::unordered_map are not nothrow movable on every standard library (MSVC's are not);
// std::vector is on all three of the L1 platforms. The few entries a Peer holds (its client's entities, its unconfirmed
// value parts, its input slots) keep the linear inserts cheap.

namespace GYO::Engine::Net {

template <class Key, class Value>
class FlatMap final {
public:
    using value_type = std::pair<Key, Value>;
    using iterator = typename std::vector<value_type>::iterator;
    using const_iterator = typename std::vector<value_type>::const_iterator;
    using reverse_iterator = typename std::vector<value_type>::reverse_iterator;
    using const_reverse_iterator = typename std::vector<value_type>::const_reverse_iterator;

    [[nodiscard]] iterator begin() noexcept { return items_.begin(); }
    [[nodiscard]] iterator end() noexcept { return items_.end(); }
    [[nodiscard]] const_iterator begin() const noexcept { return items_.begin(); }
    [[nodiscard]] const_iterator end() const noexcept { return items_.end(); }
    [[nodiscard]] reverse_iterator rbegin() noexcept { return items_.rbegin(); }
    [[nodiscard]] const_reverse_iterator rbegin() const noexcept { return items_.rbegin(); }
    [[nodiscard]] bool empty() const noexcept { return items_.empty(); }
    [[nodiscard]] std::size_t size() const noexcept { return items_.size(); }
    void clear() noexcept { items_.clear(); }

    [[nodiscard]] iterator lower_bound(const Key& key) {
        return std::lower_bound(items_.begin(), items_.end(), key, [](const value_type& item, const Key& k) { return item.first < k; });
    }
    [[nodiscard]] const_iterator lower_bound(const Key& key) const {
        return std::lower_bound(items_.begin(), items_.end(), key, [](const value_type& item, const Key& k) { return item.first < k; });
    }
    [[nodiscard]] iterator find(const Key& key) {
        auto at = lower_bound(key);
        return at != items_.end() && at->first == key ? at : items_.end();
    }
    [[nodiscard]] const_iterator find(const Key& key) const {
        auto at = lower_bound(key);
        return at != items_.end() && at->first == key ? at : items_.end();
    }
    [[nodiscard]] std::size_t count(const Key& key) const { return find(key) != items_.end() ? 1 : 0; }

    // Inserts when the key is not there; the bool says whether it inserted (as std::map::emplace).
    template <class... Args>
    std::pair<iterator, bool> emplace(const Key& key, Args&&... args) {
        auto at = lower_bound(key);
        if (at != items_.end() && at->first == key) return {at, false};
        return {items_.insert(at, value_type(key, Value(std::forward<Args>(args)...))), true};
    }
    Value& operator[](const Key& key) { return emplace(key).first->second; }

    iterator erase(const_iterator at) { return items_.erase(at); }
    std::size_t erase(const Key& key) {
        auto at = find(key);
        if (at == items_.end()) return 0;
        items_.erase(at);
        return 1;
    }

private:
    std::vector<value_type> items_;
};

} // namespace GYO::Engine::Net
