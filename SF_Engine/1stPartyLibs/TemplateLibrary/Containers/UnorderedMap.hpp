#pragma once
#pragma once
#include "../Allocator.hpp"
#include "../Compare.hpp"
#include "../Iterators.hpp"

namespace SFTL
{
    template<typename Key>
    struct hash
    {
        static_assert(::SFTL::is_integral_v<Key> || ::SFTL::is_pointer_v<Key>,
                      "SFTL::hash<Key> has no specialization for this type - provide one "
                      "(see the AdvancedString/AdvancedStringView specializations in String.hpp)");

        constexpr size_t operator()(const Key &k) const noexcept
        {
            if constexpr (::SFTL::is_pointer_v<Key>)
                return reinterpret_cast<size_t>(k);
            else
                return static_cast<size_t>(k);
        }
    };

    // Separate-chaining hash map.
    //
    // Node/bucket-array storage is plain new/delete for now - not routed through
    // SFTL::allocator_traits like AdvancedString is. TODO: rebind through Allocator.hpp
    // once it exposes rebind_alloc<Node>, so this takes an Allocator template param too.
    //
    // Supports heterogeneous find()/contains() for any K where `Key::operator==(const K&)`
    // and `SFTL::hash<K>` both exist AND agree with Key's hash for equal content (true for
    // AdvancedString<T> vs. AdvancedStringView<T> - both hash via Detail::HashSpan over the
    // same bytes). That means looking up a `string` map with a `string_view` key needs zero
    // temporary `string` construction.
    template<typename Key, typename Value, typename Hash = hash<Key>, typename KeyEqual = equal_to<Key>>
    class unordered_map
    {
    public:
        struct value_type
        {
            const Key first;
            Value second;
        };

    private:
        struct Node
        {
            value_type kv;
            Node *next = nullptr;

            template<typename K, typename V>
            Node(K &&k, V &&v) : kv{Key(static_cast<K &&>(k)), Value(static_cast<V &&>(v))}
            {
            }
        };

        static constexpr size_type kInitialBucketCount = 8;
        static constexpr float kMaxLoadFactor          = 1.0f;

        Node **buckets_        = nullptr;
        size_type bucketCount_ = 0;
        size_type size_        = 0;
        Hash hasher_{};
        KeyEqual keyEqual_{};

        [[nodiscard]] size_type IndexOf(const Key &k, size_type bucketCount) const
        {
            return bucketCount == 0 ? 0 : static_cast<size_type>(hasher_(k) % bucketCount);
        }

        void Rehash(size_type newBucketCount)
        {
            auto **newBuckets = new Node *[newBucketCount]();
            for (size_type i = 0; i < bucketCount_; ++i)
            {
                Node *node = buckets_[i];
                while (node)
                {
                    Node *next      = node->next;
                    size_type idx   = IndexOf(node->kv.first, newBucketCount);
                    node->next      = newBuckets[idx];
                    newBuckets[idx] = node;
                    node            = next;
                }
            }
            delete[] buckets_;
            buckets_     = newBuckets;
            bucketCount_ = newBucketCount;
        }

        void GrowIfNeeded()
        {
            if (bucketCount_ == 0)
            {
                Rehash(kInitialBucketCount);
                return;
            }
            if (static_cast<float>(size_ + 1) > kMaxLoadFactor * static_cast<float>(bucketCount_))
                Rehash(bucketCount_ * 2);
        }

        Node *FindNode(const Key &k) const
        {
            if (bucketCount_ == 0)
                return nullptr;
            for (Node *n = buckets_[IndexOf(k, bucketCount_)]; n; n = n->next)
                if (keyEqual_(n->kv.first, k))
                    return n;
            return nullptr;
        }

    public:
        class iterator
        {
            friend class unordered_map;

            Node **buckets_        = nullptr;
            size_type bucketCount_ = 0;
            size_type bucketIdx_   = 0;
            Node *node_            = nullptr;

            void SkipEmptyBuckets()
            {
                while (!node_ && bucketIdx_ < bucketCount_)
                {
                    node_ = buckets_[bucketIdx_];
                    if (!node_)
                        ++bucketIdx_;
                }
            }

            iterator(Node **buckets, size_type bucketCount, size_type bucketIdx, Node *node) :
                buckets_(buckets), bucketCount_(bucketCount), bucketIdx_(bucketIdx), node_(node)
            {
                SkipEmptyBuckets();
            }

        public:
            iterator() = default;

            value_type &operator*() const { return node_->kv; }
            value_type *operator->() const { return &node_->kv; }

            iterator &operator++()
            {
                node_ = node_->next;
                if (!node_)
                {
                    ++bucketIdx_;
                    SkipEmptyBuckets();
                }
                return *this;
            }
            iterator operator++(int)
            {
                iterator tmp = *this;
                ++(*this);
                return tmp;
            }

            bool operator==(const iterator &rhs) const { return node_ == rhs.node_; }
            bool operator!=(const iterator &rhs) const { return node_ != rhs.node_; }
        };

        // No distinct const-traversal path yet - value_type::first is already const,
        // so aliasing is safe; second is mutable through either.
        using const_iterator = iterator;

        template<typename Iter>
        struct InsertResult
        {
            Iter first;
            bool second;
        };

        unordered_map() = default;

        unordered_map(const unordered_map &other)
        {
            for (auto &kv: other)
                emplace(kv.first, kv.second);
        }

        unordered_map(unordered_map &&other) noexcept :
            buckets_(other.buckets_), bucketCount_(other.bucketCount_), size_(other.size_)
        {
            other.buckets_     = nullptr;
            other.bucketCount_ = 0;
            other.size_        = 0;
        }

        unordered_map &operator=(const unordered_map &other)
        {
            if (this != &other)
            {
                Clear();
                for (auto &kv: other)
                    emplace(kv.first, kv.second);
            }
            return *this;
        }

        unordered_map &operator=(unordered_map &&other) noexcept
        {
            if (this != &other)
            {
                Clear();
                delete[] buckets_;
                buckets_           = other.buckets_;
                bucketCount_       = other.bucketCount_;
                size_              = other.size_;
                other.buckets_     = nullptr;
                other.bucketCount_ = 0;
                other.size_        = 0;
            }
            return *this;
        }

        ~unordered_map()
        {
            Clear();
            delete[] buckets_;
        }

        [[nodiscard]] size_type size() const noexcept { return size_; }
        [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
        [[nodiscard]] size_type bucket_count() const noexcept { return bucketCount_; }
        [[nodiscard]] float load_factor() const noexcept
        {
            return bucketCount_ == 0 ? 0.f : static_cast<float>(size_) / static_cast<float>(bucketCount_);
        }

        void reserve(size_type n)
        {
            size_type needed = static_cast<size_type>(static_cast<float>(n) / kMaxLoadFactor) + 1;
            if (needed > bucketCount_)
                Rehash(needed);
        }

        template<typename K, typename V>
        InsertResult<iterator> emplace(K &&k, V &&v)
        {
            if (Node *existing = FindNode(k))
                return {iterator(buckets_, bucketCount_, IndexOf(existing->kv.first, bucketCount_), existing), false};

            GrowIfNeeded();
            auto *node    = new Node(static_cast<K &&>(k), static_cast<V &&>(v));
            size_type idx = IndexOf(node->kv.first, bucketCount_);
            node->next    = buckets_[idx];
            buckets_[idx] = node;
            ++size_;
            return {iterator(buckets_, bucketCount_, idx, node), true};
        }

        Value &operator[](const Key &k)
        {
            if (Node *n = FindNode(k))
                return n->kv.second;
            return emplace(k, Value{}).first->second;
        }

        iterator find(const Key &k)
        {
            Node *n = FindNode(k);
            return n ? iterator(buckets_, bucketCount_, IndexOf(k, bucketCount_), n) : end();
        }
        const_iterator find(const Key &k) const { return const_cast<unordered_map *>(this)->find(k); }

        // Heterogeneous overload - see the class comment re: the hash-agreement requirement.
        template<typename K, typename H = ::SFTL::hash<K>>
        iterator find(const K &k)
        {
            if (bucketCount_ == 0)
                return end();
            size_type idx = static_cast<size_type>(H{}(k) % bucketCount_);
            for (Node *n = buckets_[idx]; n; n = n->next)
                if (n->kv.first == k)
                    return iterator(buckets_, bucketCount_, idx, n);
            return end();
        }
        template<typename K, typename H = ::SFTL::hash<K>>
        const_iterator find(const K &k) const
        {
            return const_cast<unordered_map *>(this)->template find<K, H>(k);
        }

        [[nodiscard]] bool contains(const Key &k) const { return FindNode(k) != nullptr; }
        template<typename K>
        [[nodiscard]] bool contains(const K &k) const
        {
            return const_cast<unordered_map *>(this)->find(k) != const_cast<unordered_map *>(this)->end();
        }

        size_type erase(const Key &k)
        {
            if (bucketCount_ == 0)
                return 0;
            size_type idx = IndexOf(k, bucketCount_);
            Node **link   = &buckets_[idx];
            while (*link)
            {
                if (keyEqual_((*link)->kv.first, k))
                {
                    Node *dead = *link;
                    *link      = dead->next;
                    delete dead;
                    --size_;
                    return 1;
                }
                link = &(*link)->next;
            }
            return 0;
        }

        void clear() { Clear(); }
        void Clear()
        {
            for (size_type i = 0; i < bucketCount_; ++i)
            {
                Node *n = buckets_[i];
                while (n)
                {
                    Node *next = n->next;
                    delete n;
                    n = next;
                }
                buckets_[i] = nullptr;
            }
            size_ = 0;
        }

        iterator begin() { return iterator(buckets_, bucketCount_, 0, nullptr); }
        iterator end() { return iterator(buckets_, bucketCount_, bucketCount_, nullptr); }
        const_iterator begin() const { return const_cast<unordered_map *>(this)->begin(); }
        const_iterator end() const { return const_cast<unordered_map *>(this)->end(); }

        // PascalCase aliases, matching AdvancedString's dual-naming convention.
        [[nodiscard]] size_type Size() const noexcept { return size(); }
        [[nodiscard]] bool Empty() const noexcept { return empty(); }
        template<typename K>
        iterator Find(const K &k)
        {
            return find(k);
        }
        template<typename K>
        const_iterator Find(const K &k) const
        {
            return find(k);
        }
        template<typename K>
        bool Contains(const K &k) const
        {
            return contains(k);
        }
        size_type Erase(const Key &k) { return erase(k); }
    };
} // namespace SFTL
