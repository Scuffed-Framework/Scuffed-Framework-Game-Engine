#pragma once
#include "../Algorithm.hpp"
#include "../Allocator.hpp"
#include "../Compare.hpp"
#include "../NumericProperties.hpp"
#include "../PointerTraits.hpp"

namespace SFTL
{
#ifndef DequeBufferSize
    #define DequeBufferSize 512
#endif

    constexpr size_type deque_buf_size(size_type sz)
    {
        return (sz < DequeBufferSize ? size_type(DequeBufferSize / sz) : size_type(1));
    }

    template<typename Type, typename Reference, typename Pointer>
    struct deque_iterator
    {
    private:
        template<typename I>
        using Iterator = deque_iterator<Type, I &, ptr_rebind<Pointer, I>>;

    public:
        typedef Iterator<Type> iterator;
        typedef Iterator<const Type> const_iterator;
        typedef ptr_rebind<Pointer, Type> elt_pointer;
        typedef ptr_rebind<Pointer, elt_pointer> map_pointer;

        static size_type buffer_size() noexcept { return deque_buf_size(sizeof(Type)); }

        typedef random_access_iterator_tag iterator_category;
        typedef Type value_type;
        typedef Pointer pointer;
        typedef Reference reference;
        typedef size_type szt;
        typedef ptrdiff_t difference_type;
        typedef deque_iterator Self;

        elt_pointer current;
        elt_pointer first;
        elt_pointer last;
        map_pointer node;

        deque_iterator(elt_pointer x, map_pointer y) noexcept : current(x), first(*y), last(*y + buffer_size()), node(y)
        {
        }

        deque_iterator() noexcept : current(), first(), last(), node() {}

        template<typename Iter, typename = Require<is_same<Self, const_iterator>, is_same<Iter, iterator>>>
        deque_iterator(const Iter &x) noexcept : current(x.current), first(x.first), last(x.last), node(x.node)
        {
        }

        deque_iterator(const deque_iterator &x) noexcept :
            current(x.current), first(x.first), last(x.last), node(x.node)
        {
        }

        deque_iterator &operator=(const deque_iterator &) = default;

        iterator const_cast_self() const noexcept { return iterator(current, node); }

        [[nodiscard]] reference operator*() const noexcept { return *current; }

        [[nodiscard]] pointer operator->() const noexcept { return current; }

        Self &operator++() noexcept
        {
            ++current;
            if (current == last)
            {
                set_node(node + 1);
                current = first;
            }
            return *this;
        }

        Self operator++(int) noexcept
        {
            Self tmp = *this;
            ++*this;
            return tmp;
        }

        Self &operator--() noexcept
        {
            if (current == first)
            {
                set_node(node - 1);
                current = last;
            }
            --current;
            return *this;
        }

        Self operator--(int) noexcept
        {
            Self tmp = *this;
            --*this;
            return tmp;
        }

        Self &operator+=(difference_type n) noexcept
        {
            const difference_type offset = n + (current - first);
            if (offset >= 0 && offset < difference_type(buffer_size()))
                current += n;
            else
            {
                const difference_type node_offset = offset > 0 ? offset / difference_type(buffer_size())
                                                               : -difference_type((-offset - 1) / buffer_size()) - 1;
                set_node(node + node_offset);
                current = first + (offset - node_offset * difference_type(buffer_size()));
            }
            return *this;
        }

        Self &operator-=(difference_type n) noexcept { return *this += -n; }

        [[nodiscard]] reference operator[](difference_type n) const noexcept { return *(*this + n); }

        void set_node(map_pointer new_node) noexcept
        {
            node  = new_node;
            first = *new_node;
            last  = first + difference_type(buffer_size());
        }

        [[nodiscard]] friend bool operator==(const Self &x, const Self &y) noexcept { return x.current == y.current; }

        template<typename ReferenceR, typename PointerR>
        [[nodiscard]] friend bool operator==(const Self &x,
                                             const deque_iterator<Type, ReferenceR, PointerR> &y) noexcept
        {
            return x.current == y.current;
        }

        [[nodiscard]] friend strong_ordering operator<=>(const Self &x, const Self &y) noexcept
        {
            if (const auto cmp = x.node <=> y.node; cmp != 0)
                return cmp;
            return x.current <=> y.current;
        }

        [[nodiscard]] friend difference_type operator-(const Self &x, const Self &y) noexcept
        {
            return difference_type(buffer_size()) * (x.node - y.node - bool(x.node)) + (x.current - x.first) +
                   (y.last - y.current);
        }

        template<typename ReferenceR, typename PointerR>
        [[nodiscard]] friend difference_type operator-(const Self &x,
                                                       const deque_iterator<Type, ReferenceR, PointerR> &y) noexcept
        {
            return difference_type(buffer_size()) * (x.node - y.node - bool(x.node)) + (x.current - x.first) +
                   (y.last - y.current);
        }

        [[nodiscard]] friend Self operator+(const Self &x, difference_type n) noexcept
        {
            Self tmp = x;
            tmp += n;
            return tmp;
        }

        [[nodiscard]] friend Self operator-(const Self &x, difference_type n) noexcept
        {
            Self tmp = x;
            tmp -= n;
            return tmp;
        }

        [[nodiscard]] friend Self operator+(difference_type n, const Self &x) noexcept { return x + n; }
    };

    namespace Detail
    {
        template<typename Type, typename Alloc>
        class DequeBase
        {
        protected:
            typedef Alloc::template rebind<Type>::other TypeAllocator;
            typedef allocator_traits<TypeAllocator> AllocTraits;

            typedef AllocTraits::pointer Pointer;
            typedef AllocTraits::const_pointer Pointer_const;

            typedef AllocTraits::template rebind<Pointer>::other MapAlloc_type;
            typedef allocator_traits<MapAlloc_type> MapAllocTraits;

            typedef Alloc allocator_type;

            allocator_type getAllocator() const noexcept { return allocator_type(getTypeAllocator()); }

            typedef deque_iterator<Type, Type &, Pointer> iterator;
            typedef deque_iterator<Type, const Type &, Pointer_const> const_iterator;

            DequeBase() : impl() { initialize_map(0); }

            DequeBase(size_type num_elements) : impl() { initialize_map(num_elements); }

            DequeBase(const allocator_type &al, size_type num_elements) : impl(al) { initialize_map(num_elements); }

            DequeBase(const allocator_type &al) : impl(al) {}

            DequeBase(DequeBase &&x) noexcept : impl(move(x.getTypeAllocator()))
            {
                initialize_map(0);
                if (x.impl.map)
                    this->impl.swap_data(x.impl);
            }

            DequeBase(DequeBase &&x, const allocator_type &al) : impl(move(x.impl), TypeAllocator(al))
            {
                x.initialize_map(0);
            }

            DequeBase(DequeBase &&x, const allocator_type &al, size_type n) : impl(al)
            {
                if (x.getAllocator() == al)
                {
                    if (x.impl.map)
                    {
                        initialize_map(0);
                        this->impl.swap_data(x.impl);
                    }
                } else
                {
                    initialize_map(n);
                }
            }

            ~DequeBase() noexcept;

            typedef iterator::map_pointer map_pointer;

            struct deque_impl_data
            {
                map_pointer map;
                size_type map_size;
                iterator start;
                iterator finish;

                deque_impl_data() noexcept : map(), map_size(), start(), finish() {}

                deque_impl_data(const deque_impl_data &)            = default;
                deque_impl_data &operator=(const deque_impl_data &) = default;

                deque_impl_data(deque_impl_data &&x) noexcept : deque_impl_data(x) { x = deque_impl_data(); }

                void swap_data(deque_impl_data &x) noexcept { swap(*this, x); }
            };

            struct DequeImpl : public TypeAllocator, public deque_impl_data
            {
                DequeImpl() noexcept(is_nothrow_default_constructible<TypeAllocator>::value) : TypeAllocator() {}

                DequeImpl(const TypeAllocator &al) noexcept : TypeAllocator(al) {}

                DequeImpl(DequeImpl &&) = default;

                DequeImpl(TypeAllocator &&al) noexcept : TypeAllocator(move(al)) {}

                DequeImpl(DequeImpl &&d, TypeAllocator &&al) : TypeAllocator(move(al)), deque_impl_data(move(d)) {}
            };

            TypeAllocator &getTypeAllocator() noexcept { return this->impl; }

            const TypeAllocator &getTypeAllocator() const noexcept { return this->impl; }

            MapAlloc_type get_mapAllocator() const noexcept { return MapAlloc_type(getTypeAllocator()); }

            Pointer allocate_node()
            {
                typedef allocator_traits<TypeAllocator> Traits;
                return Traits::allocate(impl, deque_buf_size(sizeof(Type)));
            }

            void deallocate_node(Pointer p) noexcept
            {
                typedef allocator_traits<TypeAllocator> Traits;
                Traits::deallocate(impl, p, deque_buf_size(sizeof(Type)));
            }

            map_pointer allocate_map(size_type n)
            {
                MapAlloc_type mapAlloc = get_mapAllocator();
                return MapAllocTraits::allocate(mapAlloc, n);
            }

            void deallocate_map(map_pointer p, size_type n) noexcept
            {
                MapAlloc_type mapAlloc = get_mapAllocator();
                MapAllocTraits::deallocate(mapAlloc, p, n);
            }

            void initialize_map(size_type);
            void create_nodes(map_pointer nstart, map_pointer nfinish);
            void destroy_nodes(map_pointer nstart, map_pointer nfinish) noexcept;
            enum
            {
                initial_map_size = 8
            };

            DequeImpl impl;
        };

        template<typename Type, typename Alloc>
        DequeBase<Type, Alloc>::~DequeBase() noexcept
        {
            if (this->impl.map)
            {
                destroy_nodes(this->impl.start.node, this->impl.finish.node + 1);
                deallocate_map(this->impl.map, this->impl.map_size);
            }
        }

        template<typename Type, typename Alloc>
        void DequeBase<Type, Alloc>::initialize_map(size_type num_elements)
        {
            const size_type num_nodes = (num_elements / deque_buf_size(sizeof(Type)) + 1);

            this->impl.map_size = max((size_type) initial_map_size, size_type(num_nodes + 2));
            this->impl.map      = allocate_map(this->impl.map_size);

            map_pointer nstart  = (this->impl.map + (this->impl.map_size - num_nodes) / 2);
            map_pointer nfinish = nstart + num_nodes;

            try
            {
                create_nodes(nstart, nfinish);
            } catch (...)
            {
                deallocate_map(this->impl.map, this->impl.map_size);
                this->impl.map      = map_pointer();
                this->impl.map_size = 0;
                throw;
            }

            this->impl.start.set_node(nstart);
            this->impl.finish.set_node(nfinish - 1);
            this->impl.start.current  = impl.start.first;
            this->impl.finish.current = (this->impl.finish.first + num_elements % deque_buf_size(sizeof(Type)));
        }

        template<typename Type, typename Alloc>
        void DequeBase<Type, Alloc>::create_nodes(map_pointer nstart, map_pointer nfinish)
        {
            map_pointer cur;
            try
            {
                for (cur = nstart; cur < nfinish; ++cur)
                    *cur = this->allocate_node();
            } catch (...)
            {
                destroy_nodes(nstart, cur);
                throw;
            }
        }

        template<typename Type, typename Alloc>
        void DequeBase<Type, Alloc>::destroy_nodes(map_pointer nstart, map_pointer nfinish) noexcept
        {
            for (map_pointer n = nstart; n < nfinish; ++n)
                deallocate_node(*n);
        }
    } // namespace Detail

    template<typename Type, typename Alloc = allocator<Type>>
    class deque : protected Detail::DequeBase<Type, Alloc>
    {
        static_assert(is_same<typename remove_cv<Type>::type, Type>::value,
                      "deque must have a non-const, non-volatile value_type");
        static_assert(is_same<typename Alloc::value_type, Type>::value,
                      "deque must have the same value_type as its allocator");

        typedef Detail::DequeBase<Type, Alloc> Base;
        typedef Base::TypeAllocator TypeAllocator;
        typedef Base::AllocTraits AllocTraits;
        typedef Base::map_pointer map_pointer;

    public:
        typedef Type value_type;

        typedef AllocTraits::pointer pointer;
        typedef AllocTraits::const_pointer const_pointer;
        typedef value_type &reference;
        typedef const value_type &const_reference;

        typedef Base::iterator iterator;
        typedef Base::const_iterator const_iterator;
        typedef reverse_iterator<const_iterator> const_reverse_iterator;
        typedef reverse_iterator<iterator> reverse_iterator;
        typedef size_type szt;
        typedef ptrdiff_t difference_type;
        typedef Alloc allocator_type;

    private:
        static size_type buffer_size() noexcept { return deque_buf_size(sizeof(Type)); }

        using Base::allocate_map;
        using Base::allocate_node;
        using Base::create_nodes;
        using Base::deallocate_map;
        using Base::deallocate_node;
        using Base::destroy_nodes;
        using Base::getTypeAllocator;
        using Base::impl;
        using Base::initialize_map;

    public:
        deque() = default;
        explicit deque(const allocator_type &al) : Base(al, 0) {}

        explicit deque(szt n, const allocator_type &al = allocator_type()) : Base(al, check_init_len(n, al))
        {
            default_initialize();
        }

        deque(szt n, const value_type &value, const allocator_type &al = allocator_type()) :
            Base(al, check_init_len(n, al))
        {
            fill_initialize(value);
        }

        deque(const deque &x) : Base(AllocTraits::sselect_on_copy(x.getTypeAllocator()), x.size())
        {
            uninitialized_copy(x.begin(), x.end(), this->impl.start, getTypeAllocator());
        }

        deque(deque &&) = default;

        deque(const deque &x, const type_identity_t<allocator_type> &al) : Base(al, x.size())
        {
            uninitialized_copy(x.begin(), x.end(), this->impl.start, getTypeAllocator());
        }

        deque(deque &&x, type_identity_t<allocator_type> &al) :
            deque(move(x), al, typename AllocTraits::is_always_equal{})
        {
        }

    private:
        deque(deque &&x, const allocator_type &al, true_type) : Base(move(x), al) {}

        deque(deque &&x, const allocator_type &al, false_type) : Base(move(x), al, x.size())
        {
            if (x.getAllocator() != al && !x.empty())
            {
                uninitialized_move(x.begin(), x.end(), this->impl.start, getTypeAllocator());
                x.clear();
            }
        }

    public:
        deque(initializer_list<value_type> l, const allocator_type &al = allocator_type()) : Base(al)
        {
            range_initialize(l.begin(), l.end(), random_access_iterator_tag());
        }

        template<typename InputIterator, typename = ::SFTL::RequireInputIter<InputIterator>>
        deque(InputIterator first, InputIterator last, const allocator_type &al = allocator_type()) : Base(al)
        {
            range_initialize(first, last, typename iterator_traits<InputIterator>::iterator_category());
        }

        ~deque() { destroy_data(begin(), end(), getTypeAllocator()); }

        constexpr deque &operator=(const deque &x);
        deque &operator=(deque &&x) noexcept(AllocTraits::salways_equal())
        {
            using always_equal = AllocTraits::is_always_equal;
            move_assign1(move(x), always_equal{});
            return *this;
        }

        deque &operator=(initializer_list<value_type> l)
        {
            assign_aux(l.begin(), l.end(), random_access_iterator_tag());
            return *this;
        }

        void assign(szt n, const value_type &val) { fill_assign(n, val); }

        template<typename InputIterator, typename = RequireInputIter<InputIterator>>
        void assign(InputIterator first, InputIterator last)
        {
            assign_aux(first, last, typename iterator_traits<InputIterator>::iterator_category());
        }
        void assign(initializer_list<value_type> l) { assign_aux(l.begin(), l.end(), random_access_iterator_tag()); }

        [[nodiscard]] allocator_type getAllocator() const noexcept { return Base::getAllocator(); }

        [[nodiscard]] iterator begin() noexcept { return this->impl.start; }
        [[nodiscard]] const_iterator begin() const noexcept { return this->impl.start; }
        [[nodiscard]] iterator end() noexcept { return this->impl.finish; }
        [[nodiscard]] const_iterator end() const noexcept { return this->impl.finish; }

        [[nodiscard]] reverse_iterator rbegin() noexcept { return reverse_iterator(this->impl.finish); }
        [[nodiscard]] const_reverse_iterator rbegin() const noexcept
        {
            return const_reverse_iterator(this->impl.finish);
        }
        [[nodiscard]] reverse_iterator rend() noexcept { return reverse_iterator(this->impl.start); }
        [[nodiscard]] const_reverse_iterator rend() const noexcept { return const_reverse_iterator(this->impl.start); }

        [[nodiscard]] const_iterator cbegin() const noexcept { return this->impl.start; }
        [[nodiscard]] const_iterator cend() const noexcept { return this->impl.finish; }
        [[nodiscard]] const_reverse_iterator crbegin() const noexcept
        {
            return const_reverse_iterator(this->impl.finish);
        }
        [[nodiscard]] const_reverse_iterator crend() const noexcept { return const_reverse_iterator(this->impl.start); }

        [[nodiscard]] szt size() const noexcept
        {
            szt sz = this->impl.finish - this->impl.start;
            if (sz > max_size())
                __builtin_unreachable();
            return sz;
        }

        [[nodiscard]] szt max_size() const noexcept { return max_size_internal(getTypeAllocator()); }

        void resize(szt new_size)
        {
            const szt len = size();
            if (new_size > len)
                default_append(new_size - len);
            else if (new_size < len)
                erase_at_end(this->impl.start + difference_type(new_size));
        }
        void shrink_to_fit() noexcept { shrink_to_fit_internal(); }

        [[nodiscard]] bool empty() const noexcept { return this->impl.finish == this->impl.start; }

        [[nodiscard]] reference operator[](szt n) noexcept { return this->impl.start[difference_type(n)]; }
        [[nodiscard]] const_reference operator[](szt n) const noexcept { return this->impl.start[difference_type(n)]; }

    protected:
        void range_check(szt n) const;

    public:
        reference at(szt n)
        {
            range_check(n);
            return (*this)[n];
        }

        const_reference at(szt n) const
        {
            range_check(n);
            return (*this)[n];
        }

        [[nodiscard]] reference front() noexcept { return *begin(); }
        [[nodiscard]] const_reference front() const noexcept { return *begin(); }

        [[nodiscard]] reference back() noexcept
        {
            iterator tmp = end();
            --tmp;
            return *tmp;
        }

        [[nodiscard]] const_reference back() const noexcept
        {
            const_iterator tmp = end();
            --tmp;
            return *tmp;
        }

        void push_front(const value_type &x)
        {
            if (this->impl.start.current != this->impl.start.first)
            {
                AllocTraits::construct(this->impl, this->impl.start.current - 1, x);
                --this->impl.start.current;
            } else
                push_front_aux(x);
        }

        void push_front(value_type &&x) { emplace_front(move(x)); }

        template<typename... Args>
        constexpr reference emplace_front(Args &&...args);

        void push_back(const value_type &x)
        {
            if (this->impl.finish.current != this->impl.finish.last - 1)
            {
                AllocTraits::construct(this->impl, this->impl.finish.current, x);
                ++this->impl.finish.current;
            } else
                push_back_aux(x);
        }

        void push_back(value_type &&x) { emplace_back(move(x)); }

        template<typename... Args>
        constexpr reference emplace_back(Args &&...args);

        void pop_front() noexcept
        {
            if (this->impl.start.current != this->impl.start.last - 1)
            {
                AllocTraits::destroy(getTypeAllocator(), this->impl.start.current);
                ++this->impl.start.current;
            } else
                pop_front_aux();
        }

        void pop_back() noexcept
        {
            if (this->impl.finish.current != this->impl.finish.first)
            {
                --this->impl.finish.current;
                AllocTraits::destroy(getTypeAllocator(), this->impl.finish.current);
            } else
                pop_back_aux();
        }

        template<typename... Args>
        constexpr iterator emplace(const_iterator position, Args &&...args);

        constexpr iterator insert(const_iterator position, const value_type &x);

        constexpr iterator insert(const_iterator position, value_type &&x) { return emplace(position, move(x)); }

        constexpr iterator insert(const_iterator p, initializer_list<value_type> l)
        {
            auto offset = p - cbegin();
            range_insert_aux(p.const_cast_self(), l.begin(), l.end(), random_access_iterator_tag());
            return begin() + offset;
        }

        iterator insert(const_iterator position, szt n, const value_type &x)
        {
            difference_type offset = position - cbegin();
            fill_insert(position.const_cast_self(), n, x);
            return begin() + offset;
        }

        template<typename InputIterator, typename = RequireInputIter<InputIterator>>
        iterator insert(const_iterator position, InputIterator first, InputIterator last)
        {
            difference_type offset = position - cbegin();
            range_insert_aux(position.const_cast_self(), first, last,
                             typename iterator_traits<InputIterator>::iterator_category());
            return begin() + offset;
        }

        iterator erase(const_iterator position) { return erase_aux(position.const_cast_self()); }

        constexpr iterator erase(const_iterator first, const_iterator last)
        {
            return erase_aux(first.const_cast_self(), last.const_cast_self());
        }

        constexpr void swap(deque &x) noexcept
        {
            static_assert(AllocTraits::propagate_on_container_swap::value ||
                          getTypeAllocator() == x.getTypeAllocator());

            this->impl.swap_data(x.impl);
            AllocTraits::son_swap(getTypeAllocator(), x.getTypeAllocator());
        }

        constexpr void clear() noexcept { erase_at_end(begin()); }

    protected:
        static size_type check_init_len(size_type n, const allocator_type &al);

        static szt max_size_internal(const TypeAllocator &al) noexcept
        {
            const size_type diffmax  = Detail::__numeric_traits<ptrdiff_t>::__max;
            const size_type allocmax = AllocTraits::max_size(al);
            return (min) (diffmax, allocmax);
        }

        template<typename InputIterator>
        constexpr void range_initialize(InputIterator first, InputIterator last, input_iterator_tag);

        template<typename ForwardIterator>
        constexpr void range_initialize(ForwardIterator first, ForwardIterator last, forward_iterator_tag);

        constexpr void fill_initialize(const value_type &value);

        constexpr void default_initialize();

        template<typename InputIterator>
        constexpr void assign_aux(InputIterator first, InputIterator last, input_iterator_tag);

        template<typename ForwardIterator>
        constexpr void assign_aux(ForwardIterator first, ForwardIterator last, forward_iterator_tag)
        {
            const szt len = distance(first, last);
            if (len > size())
            {
                ForwardIterator mid = first;
                advance(mid, size());
                copy(first, mid, begin());
                range_insert_aux(end(), mid, last, typename iterator_traits<ForwardIterator>::iterator_category());
            } else
                erase_at_end(copy(first, last, begin()));
        }

        void fill_assign(szt n, const value_type &val)
        {
            if (n > size())
            {
                fill(begin(), end(), val);
                fill_insert(end(), n - size(), val);
            } else
            {
                erase_at_end(begin() + difference_type(n));
                fill(begin(), end(), val);
            }
        }

        template<typename... Args>
        constexpr void push_back_aux(Args &&...args);

        template<typename... Args>
        constexpr void push_front_aux(Args &&...args);

        constexpr void pop_back_aux();

        constexpr void pop_front_aux();

        template<typename InputIterator, typename Sentinel>
        constexpr void range_prepend(InputIterator first, Sentinel last, szt n);

        template<typename InputIterator, typename Sentinel>
        constexpr void range_append(InputIterator first, Sentinel last, szt n);

        template<typename InputIterator>
        constexpr void range_insert_aux(iterator pos, InputIterator first, InputIterator last, input_iterator_tag);

        template<typename ForwardIterator>
        constexpr void range_insert_aux(iterator pos, ForwardIterator first, ForwardIterator last,
                                        forward_iterator_tag);

        constexpr void fill_insert(iterator pos, szt n, const value_type &x);

        struct TemporaryValue
        {
            template<typename... Args>
            constexpr explicit TemporaryValue(deque *d, Args &&...args) : this_deque(d)
            {
                AllocTraits::construct(this_deque->impl, get_pointer(), forward<Args>(args)...);
            }

            constexpr ~TemporaryValue() { AllocTraits::destroy(this_deque->impl, get_pointer()); }

            constexpr value_type &val() noexcept { return tmp_val; }

        private:
            constexpr Type *get_pointer() noexcept { return addressof(tmp_val); }

            union
            {
                Type tmp_val;
            };

            deque *this_deque;
        };

        iterator insert_aux_elem(iterator pos, const value_type &x) { return emplace_aux(pos, x); }

        template<typename... Args>
        constexpr iterator emplace_aux(iterator pos, Args &&...args);

        constexpr void insert_aux_fill(iterator pos, szt n, const value_type &x);

        template<typename ForwardIterator>
        constexpr void insert_aux_range(iterator pos, ForwardIterator first, ForwardIterator last, szt n);

        constexpr void destroy_data_aux(iterator first, iterator last);

        template<typename Alloc1>
        void destroy_data(iterator first, iterator last, const Alloc1 &)
        {
            destroy_data_aux(first, last);
        }

        void destroy_data(iterator first, iterator last, const allocator<Type> &)
        {
            if constexpr (!is_trivially_destructible_v<value_type>)
                destroy_data_aux(first, last);
        }

        void erase_at_begin(iterator pos)
        {
            destroy_data(begin(), pos, getTypeAllocator());
            destroy_nodes(this->impl.start.node, pos.node);
            this->impl.start = pos;
        }

        void erase_at_end(iterator pos)
        {
            destroy_data(pos, end(), getTypeAllocator());
            destroy_nodes(pos.node + 1, this->impl.finish.node + 1);
            this->impl.finish = pos;
        }

        constexpr iterator erase_aux(iterator pos);

        constexpr iterator erase_aux(iterator first, iterator last);

        constexpr void default_append(szt n);

        constexpr bool shrink_to_fit_internal();

        iterator reserve_elements_at_front(szt n)
        {
            const szt vacancies = this->impl.start.current - this->impl.start.first;
            if (n > vacancies)
                new_elements_at_front(n - vacancies);
            return this->impl.start - difference_type(n);
        }

        iterator reserve_elements_at_back(szt n)
        {
            const szt vacancies = (this->impl.finish.last - this->impl.finish.current) - 1;
            if (n > vacancies)
                new_elements_at_back(n - vacancies);
            return this->impl.finish + difference_type(n);
        }

        constexpr void new_elements_at_front(szt new_elems);

        constexpr void new_elements_at_back(szt new_elems);

        void reserve_map_at_back(szt nodes_to_add = 1)
        {
            if (nodes_to_add + 1 > this->impl.map_size - (this->impl.finish.node - this->impl.map))
                reallocate_map(nodes_to_add, false);
        }

        void reserve_map_at_front(szt nodes_to_add = 1)
        {
            if (nodes_to_add > szt(this->impl.start.node - this->impl.map))
                reallocate_map(nodes_to_add, true);
        }

        constexpr void reallocate_map(szt nodes_to_add, bool add_at_front);

        void move_assign1(deque &&x, true_type) noexcept
        {
            this->impl.swap_data(x.impl);
            x.clear();
            _Alloc_on_move(getTypeAllocator(), x.getTypeAllocator());
        }

        void move_assign1(deque &&x, false_type)
        {
            if (getTypeAllocator() == x.getTypeAllocator())
                return move_assign1(move(x), true_type());

            constexpr bool move_storage = AllocTraits::spropagate_on_move_assign();
            move_assign2(move(x), bool_constant<move_storage>());
        }

        template<typename... Args>
        void replace_map(Args &&...args)
        {
            deque newobj(forward<Args>(args)...);
            clear();
            deallocate_node(*begin().node);
            deallocate_map(this->impl.map, this->impl.map_size);
            this->impl.map      = nullptr;
            this->impl.map_size = 0;
            this->impl.swap_data(newobj.impl);
        }

        void move_assign2(deque &&x, true_type)
        {
            auto alloc = x.getTypeAllocator();
            replace_map(move(x));
            getTypeAllocator() = move(alloc);
        }

        void move_assign2(deque &&x, false_type)
        {
            if (x.getTypeAllocator() == this->getTypeAllocator())
            {
                replace_map(move(x), x.getAllocator());
            } else
            {
                assign_aux(make_move_iterator(x.begin()), make_move_iterator(x.end()), random_access_iterator_tag());
                x.clear();
            }
        }
    };

    template<typename InputIterator, typename ValT = iterator_traits<InputIterator>::value_type,
             typename Allocator = allocator<ValT>, typename = RequireInputIter<InputIterator>,
             typename = require_allocator<Allocator>>
    deque(InputIterator, InputIterator, Allocator = Allocator()) -> deque<ValT, Allocator>;

    template<typename Type, typename Alloc>
    [[nodiscard]] bool operator==(const deque<Type, Alloc> &x, const deque<Type, Alloc> &y)
    {
        return x.size() == y.size() && equal(x.begin(), x.end(), y.begin());
    }

    template<typename Type, typename Alloc>
    [[nodiscard]] synthesize3way_type<Type> operator<=>(const deque<Type, Alloc> &x, const deque<Type, Alloc> &y)
    {
        return lexicographical_compare_three_way(x.begin(), x.end(), y.begin(), y.end(), Detail::synth3way);
    }

    template<typename Type, typename Alloc>
    void swap(deque<Type, Alloc> &x, deque<Type, Alloc> &y) noexcept
    {
        x.swap(y);
    }
    template<typename Type, typename Alloc>
    void deque<Type, Alloc>::range_check(szt n) const
    {
        if (n >= this->size())
            throw;
    }

    template<typename Type, typename Alloc>
    size_type deque<Type, Alloc>::check_init_len(size_type n, const allocator_type &al)
    {
        if (n > max_size_internal(al))
            throw;
        return n;
    }

    template<typename Type, typename Alloc>
    constexpr void deque<Type, Alloc>::default_initialize()
    {
        map_pointer cur;
        try
        {
            for (cur = this->impl.start.node; cur < this->impl.finish.node; ++cur)
                uninitialized_default(*cur, *cur + buffer_size(), getTypeAllocator());
            uninitialized_default(this->impl.finish.first, this->impl.finish.current, getTypeAllocator());
        } catch (...)
        {
            destroy_data(this->impl.start, iterator(*cur, cur), getTypeAllocator());
            throw;
        }
    }

    template<typename Type, typename Alloc>
    constexpr deque<Type, Alloc> &deque<Type, Alloc>::operator=(const deque &x)
    {
        if (addressof(x) != this)
        {
            if constexpr (AllocTraits::spropagate_on_copy_assign())
            {
                if constexpr (!AllocTraits::salways_equal())
                {
                    if (getTypeAllocator() != x.getTypeAllocator())
                    {
                        replace_map(x, x.getAllocator());
                        _Alloc_on_copy(getTypeAllocator(), x.getTypeAllocator());
                        return *this;
                    }
                }
                _Alloc_on_copy(getTypeAllocator(), x.getTypeAllocator());
            }
            const szt len = size();
            if (len >= x.size())
                erase_at_end(copy(x.begin(), x.end(), this->impl.start));
            else
            {
                const_iterator mid = x.begin() + difference_type(len);
                copy(x.begin(), mid, this->impl.start);
                range_insert_aux(this->impl.finish, mid, x.end(), random_access_iterator_tag());
            }
        }
        return *this;
    }

    template<typename Type, typename Alloc>
    template<typename... Args>
    constexpr deque<Type, Alloc>::reference deque<Type, Alloc>::emplace_front(Args &&...args)
    {
        if (this->impl.start.current != this->impl.start.first)
        {
            AllocTraits::construct(this->impl, this->impl.start.current - 1, forward<Args>(args)...);
            --this->impl.start.current;
        } else
            push_front_aux(forward<Args>(args)...);
        return front();
    }

    template<typename Type, typename Alloc>
    template<typename... Args>
    constexpr deque<Type, Alloc>::reference deque<Type, Alloc>::emplace_back(Args &&...args)
    {
        if (this->impl.finish.current != this->impl.finish.last - 1)
        {
            AllocTraits::construct(this->impl, this->impl.finish.current, forward<Args>(args)...);
            ++this->impl.finish.current;
        } else
            push_back_aux(forward<Args>(args)...);
        return back();
    }

    template<typename Type, typename Alloc>
    template<typename... Args>
    constexpr deque<Type, Alloc>::iterator deque<Type, Alloc>::emplace(const_iterator position, Args &&...args)
    {
        if (position.current == this->impl.start.current)
        {
            emplace_front(forward<Args>(args)...);
            return this->impl.start;
        }
        if (position.current == this->impl.finish.current)
        {
            emplace_back(forward<Args>(args)...);
            iterator tmp = this->impl.finish;
            --tmp;
            return tmp;
        }
        return emplace_aux(position.const_cast_self(), forward<Args>(args)...);
    }

    template<typename Type, typename Alloc>
    constexpr deque<Type, Alloc>::iterator deque<Type, Alloc>::insert(const_iterator position, const value_type &x)
    {
        if (position.current == this->impl.start.current)
        {
            push_front(x);
            return this->impl.start;
        } else if (position.current == this->impl.finish.current)
        {
            push_back(x);
            iterator tmp = this->impl.finish;
            --tmp;
            return tmp;
        } else
            return insert_aux_elem(position.const_cast_self(), x);
    }

    template<typename Type, typename Alloc>
    constexpr deque<Type, Alloc>::iterator deque<Type, Alloc>::erase_aux(iterator position)
    {
        iterator next = position;
        ++next;
        const difference_type index = position - begin();
        if (static_cast<szt>(index) < (size() >> 1))
        {
            if (position != begin())
                copy_backward(begin(), position, next);
            pop_front();
        } else
        {
            if (next != end())
                copy(next, end(), position);
            pop_back();
        }
        return begin() + index;
    }

    template<typename Type, typename Alloc>
    constexpr deque<Type, Alloc>::iterator deque<Type, Alloc>::erase_aux(iterator first, iterator last)
    {
        if (first == last)
            return first;
        else if (first == begin() && last == end())
        {
            clear();
            return end();
        } else
        {
            const difference_type n            = last - first;
            const difference_type elems_before = first - begin();
            if (static_cast<szt>(elems_before) <= (size() - szt(n)) / 2)
            {
                if (first != begin())
                    copy_backward(begin(), first, last);
                erase_at_begin(begin() + n);
            } else
            {
                if (last != end())
                    copy(last, end(), first);
                erase_at_end(end() - n);
            }
            return begin() + elems_before;
        }
    }

    template<typename Type, typename Alloc>
    template<typename InputIterator>
    constexpr void deque<Type, Alloc>::assign_aux(InputIterator first, InputIterator last, input_iterator_tag)
    {
        iterator cur = begin();
        for (; first != last && cur != end(); ++cur, (void) ++first)
            *cur = *first;
        if (first == last)
            erase_at_end(cur);
        else
            range_insert_aux(end(), first, last, typename iterator_traits<InputIterator>::iterator_category());
    }

    template<typename Type, typename Alloc>
    constexpr void deque<Type, Alloc>::fill_insert(iterator pos, szt n, const value_type &x)
    {
        if (pos.current == this->impl.start.current)
        {
            iterator new_start = reserve_elements_at_front(n);
            try
            {
                uninitialized_fill(new_start, this->impl.start, x, getTypeAllocator());
                this->impl.start = new_start;
            } catch (...)
            {
                destroy_nodes(new_start.node, this->impl.start.node);
                throw;
            }
        } else if (pos.current == this->impl.finish.current)
        {
            iterator new_finish = reserve_elements_at_back(n);
            try
            {
                uninitialized_fill(this->impl.finish, new_finish, x, getTypeAllocator());
                this->impl.finish = new_finish;
            } catch (...)
            {
                destroy_nodes(this->impl.finish.node + 1, new_finish.node + 1);
                throw;
            }
        } else
            insert_aux_fill(pos, n, x);
    }

    template<typename Type, typename Alloc>
    constexpr void deque<Type, Alloc>::default_append(szt n)
    {
        if (n)
        {
            iterator new_finish = reserve_elements_at_back(n);
            try
            {
                uninitialized_default(this->impl.finish, new_finish, getTypeAllocator());
                this->impl.finish = new_finish;
            } catch (...)
            {
                destroy_nodes(this->impl.finish.node + 1, new_finish.node + 1);
                throw;
            }
        }
    }

    template<typename Type, typename Alloc>
    constexpr bool deque<Type, Alloc>::shrink_to_fit_internal()
    {
        const difference_type front_capacity = (this->impl.start.current - this->impl.start.first);
        if (front_capacity == 0)
            return false;

        const difference_type back_capacity = (this->impl.finish.last - this->impl.finish.current);
        if (szt(front_capacity + back_capacity) < buffer_size())
            return false;

        return shrink_to_fit_auxiliary<deque>::sdo_it(*this);
    }

    template<typename Type, typename Alloc>
    constexpr void deque<Type, Alloc>::fill_initialize(const value_type &value)
    {
        map_pointer cur;
        try
        {
            for (cur = this->impl.start.node; cur < this->impl.finish.node; ++cur)
                uninitialized_fill(*cur, *cur + buffer_size(), value, getTypeAllocator());
            uninitialized_fill(this->impl.finish.first, this->impl.finish.current, value, getTypeAllocator());
        } catch (...)
        {
            destroy_data(this->impl.start, iterator(*cur, cur), getTypeAllocator());
            throw;
        }
    }

    template<typename Type, typename Alloc>
    template<typename InputIterator>
    constexpr void deque<Type, Alloc>::range_initialize(InputIterator first, InputIterator last, input_iterator_tag)
    {
        initialize_map(0);
        try
        {
            for (; first != last; ++first)
                emplace_back(*first);
        } catch (...)
        {
            clear();
            throw;
        }
    }

    template<typename Type, typename Alloc>
    template<typename ForwardIterator>
    constexpr void deque<Type, Alloc>::range_initialize(ForwardIterator first, ForwardIterator last,
                                                        forward_iterator_tag)
    {
        const szt n = distance(first, last);
        initialize_map(check_init_len(n, getTypeAllocator()));

        map_pointer cur_node;
        try
        {
            for (cur_node = this->impl.start.node; cur_node < this->impl.finish.node; ++cur_node)
            {
                if (n < buffer_size())
                    __builtin_unreachable();

                ForwardIterator mid = first;
                advance(mid, buffer_size());
                uninitialized_copy(first, mid, *cur_node, getTypeAllocator());
                first = mid;
            }
            uninitialized_copy(first, last, this->impl.finish.first, getTypeAllocator());
        } catch (...)
        {
            destroy_data(this->impl.start, iterator(*cur_node, cur_node), getTypeAllocator());
            throw;
        }
    }

    template<typename Type, typename Alloc>
    template<typename... Args>
    constexpr void deque<Type, Alloc>::push_back_aux(Args &&...args)
    {
        if (size() == max_size())
            throw;

        reserve_map_at_back();
        *(this->impl.finish.node + 1) = allocate_node();
        try
        {
            AllocTraits::construct(this->impl, this->impl.finish.current, forward<Args>(args)...);
            this->impl.finish.set_node(this->impl.finish.node + 1);
            this->impl.finish.current = this->impl.finish.first;
        } catch (...)
        {
            deallocate_node(*(this->impl.finish.node + 1));
            throw;
        }
    }

    template<typename Type, typename Alloc>
    template<typename... Args>
    constexpr void deque<Type, Alloc>::push_front_aux(Args &&...args)
    {
        if (size() == max_size())
            throw;

        reserve_map_at_front();
        *(this->impl.start.node - 1) = allocate_node();
        try
        {
            this->impl.start.set_node(this->impl.start.node - 1);
            this->impl.start.current = this->impl.start.last - 1;
            AllocTraits::construct(this->impl, this->impl.start.current, forward<Args>(args)...);
        } catch (...)
        {
            ++this->impl.start;
            deallocate_node(*(this->impl.start.node - 1));
            throw;
        }
    }

    template<typename Type, typename Alloc>
    constexpr void deque<Type, Alloc>::pop_back_aux()
    {
        deallocate_node(this->impl.finish.first);
        this->impl.finish.set_node(this->impl.finish.node - 1);
        this->impl.finish.current = this->impl.finish.last - 1;
        AllocTraits::destroy(getTypeAllocator(), this->impl.finish.current);
    }

    template<typename Type, typename Alloc>
    constexpr void deque<Type, Alloc>::pop_front_aux()
    {
        AllocTraits::destroy(getTypeAllocator(), this->impl.start.current);
        deallocate_node(this->impl.start.first);
        this->impl.start.set_node(this->impl.start.node + 1);
        this->impl.start.current = this->impl.start.first;
    }

    template<typename Type, typename Alloc>
    template<typename InputIterator, typename Sentinel>
    constexpr void deque<Type, Alloc>::range_prepend(InputIterator first, Sentinel last, szt n)
    {
        iterator new_start = reserve_elements_at_front(n);
        try
        {
            uninitialized_copy(move(first), last, new_start, getTypeAllocator());
            this->impl.start = new_start;
        } catch (...)
        {
            destroy_nodes(new_start.node, this->impl.start.node);
            throw;
        }
    }

    template<typename Type, typename Alloc>
    template<typename InputIterator, typename Sentinel>
    constexpr void deque<Type, Alloc>::range_append(InputIterator first, Sentinel last, szt n)
    {
        iterator new_finish = reserve_elements_at_back(n);
        try
        {
            uninitialized_copy(move(first), last, this->impl.finish, getTypeAllocator());
            this->impl.finish = new_finish;
        } catch (...)
        {
            destroy_nodes(this->impl.finish.node + 1, new_finish.node + 1);
            throw;
        }
    }

    template<typename Type, typename Alloc>
    template<typename InputIterator>
    constexpr void deque<Type, Alloc>::range_insert_aux(iterator pos, InputIterator first, InputIterator last,
                                                        input_iterator_tag)
    {
        copy(first, last, inserter(*this, pos));
    }

    template<typename Type, typename Alloc>
    template<typename ForwardIterator>
    constexpr void deque<Type, Alloc>::range_insert_aux(iterator pos, ForwardIterator first, ForwardIterator last,
                                                        forward_iterator_tag)
    {
        const szt n = distance(first, last);
        if (n == 0)
            return;

        if (pos.current == this->impl.start.current)
            range_prepend(first, last, n);
        else if (pos.current == this->impl.finish.current)
            range_append(first, last, n);
        else
            insert_aux_range(pos, first, last, n);
    }

    template<typename Type, typename Alloc>
    template<typename... Args>
    constexpr deque<Type, Alloc>::iterator deque<Type, Alloc>::emplace_aux(iterator pos, Args &&...args)
    {
        TemporaryValue tmp(this, forward<Args>(args)...);
        difference_type index = pos - this->impl.start;
        if (static_cast<szt>(index) < size() / 2)
        {
            push_front(move(front()));
            iterator front1 = this->impl.start;
            ++front1;
            iterator front2 = front1;
            ++front2;
            pos           = this->impl.start + index;
            iterator pos1 = pos;
            ++pos1;
            copy(front2, pos1, front1);
        } else
        {
            push_back(move(back()));
            iterator back1 = this->impl.finish;
            --back1;
            iterator back2 = back1;
            --back2;
            pos = this->impl.start + index;
            copy_backward(pos, back2, back1);
        }
        *pos = move(tmp.val());
        return pos;
    }

    template<typename Type, typename Alloc>
    constexpr void deque<Type, Alloc>::insert_aux_fill(iterator pos, szt n, const value_type &x)
    {
        const difference_type elems_before = pos - this->impl.start;
        const szt length                   = size();
        value_type x_copy                  = x;
        if (static_cast<szt>(elems_before) < length / 2)
        {
            iterator new_start = reserve_elements_at_front(n);
            iterator old_start = this->impl.start;
            pos                = this->impl.start + elems_before;
            try
            {
                if (elems_before >= difference_type(n))
                {
                    iterator start_n = (this->impl.start + difference_type(n));
                    uninitialized_move(this->impl.start, start_n, new_start, getTypeAllocator());
                    this->impl.start = new_start;
                    copy(start_n, pos, old_start);
                    fill(pos - difference_type(n), pos, x_copy);
                } else
                {
                    this->impl.start = new_start;
                    fill(old_start, pos, x_copy);
                }
            } catch (...)
            {
                destroy_nodes(new_start.node, this->impl.start.node);
                throw;
            }
        } else
        {
            iterator new_finish               = reserve_elements_at_back(n);
            iterator old_finish               = this->impl.finish;
            const difference_type elems_after = difference_type(length) - elems_before;
            pos                               = this->impl.finish - elems_after;
            try
            {
                if (elems_after > difference_type(n))
                {
                    iterator finish_n = (this->impl.finish - difference_type(n));
                    uninitialized_move(finish_n, this->impl.finish, this->impl.finish, getTypeAllocator());
                    this->impl.finish = new_finish;
                    copy_backward(pos, finish_n, old_finish);
                    fill(pos, pos + difference_type(n), x_copy);
                } else
                {
                    this->impl.finish = new_finish;
                    fill(pos, old_finish, x_copy);
                }
            } catch (...)
            {
                destroy_nodes(this->impl.finish.node + 1, new_finish.node + 1);
                throw;
            }
        }
    }

    template<typename Type, typename Alloc>
    template<typename ForwardIterator>
    constexpr void deque<Type, Alloc>::insert_aux_range(iterator pos, ForwardIterator first, ForwardIterator last,
                                                        szt n)
    {
        const difference_type elemsbefore = pos - this->impl.start;
        const szt length                  = size();
        if (static_cast<szt>(elemsbefore) < length / 2)
        {
            iterator new_start = reserve_elements_at_front(n);
            iterator old_start = this->impl.start;
            pos                = this->impl.start + elemsbefore;
            try
            {
                if (elemsbefore >= difference_type(n))
                {
                    iterator start_n = (this->impl.start + difference_type(n));
                    uninitialized_move(this->impl.start, start_n, new_start, getTypeAllocator());
                    this->impl.start = new_start;
                    copy(start_n, pos, old_start);
                    copy(first, last, pos - difference_type(n));
                } else
                {
                    ForwardIterator mid = first;
                    advance(mid, difference_type(n) - elemsbefore);
                    this->impl.start = new_start;
                    copy(mid, last, old_start);
                }
            } catch (...)
            {
                destroy_nodes(new_start.node, this->impl.start.node);
                throw;
            }
        } else
        {
            iterator new_finish              = reserve_elements_at_back(n);
            iterator old_finish              = this->impl.finish;
            const difference_type elemsafter = difference_type(length) - elemsbefore;
            pos                              = this->impl.finish - elemsafter;
            try
            {
                if (elemsafter > difference_type(n))
                {
                    iterator finish_n = (this->impl.finish - difference_type(n));
                    uninitialized_move(finish_n, this->impl.finish, this->impl.finish, getTypeAllocator());
                    this->impl.finish = new_finish;
                    copy_backward(pos, finish_n, old_finish);
                    copy(first, last, pos);
                } else
                {
                    ForwardIterator mid = first;
                    advance(mid, elemsafter);
                    this->impl.finish = new_finish;
                    copy(first, mid, pos);
                }
            } catch (...)
            {
                destroy_nodes(this->impl.finish.node + 1, new_finish.node + 1);
                throw;
            }
        }
    }
    template<typename Type, typename Alloc>
    constexpr void deque<Type, Alloc>::destroy_data_aux(iterator first, iterator last)
    {
        // Destroy full nodes in the middle
        for (map_pointer node = first.node + 1; node < last.node; ++node)
        {
            for (auto it = *node; it != *node + buffer_size(); ++it)
            {
                AllocTraits::destroy(getTypeAllocator(), it);
            }
        }

        if (first.node != last.node)
        {
            // Destroy the active elements in the first node
            for (auto it = first.current; it != first.last; ++it)
            {
                AllocTraits::destroy(getTypeAllocator(), it);
            }
            // Destroy the active elements in the last node
            for (auto it = last.first; it != last.current; ++it)
            {
                AllocTraits::destroy(getTypeAllocator(), it);
            }
        } else
        {
            // First and last iterators point into the exact same node block
            for (auto it = first.current; it != last.current; ++it)
            {
                AllocTraits::destroy(getTypeAllocator(), it);
            }
        }
    }

    template<typename Type, typename Alloc>
    constexpr void deque<Type, Alloc>::new_elements_at_front(szt new_elems)
    {
        if (max_size() - size() < new_elems)
            throw;

        const szt new_nodes = ((new_elems + buffer_size() - 1) / buffer_size());
        reserve_map_at_front(new_nodes);
        szt i;
        try
        {
            for (i = 1; i <= new_nodes; ++i)
                *(this->impl.start.node - i) = allocate_node();
        } catch (...)
        {
            for (szt j = 1; j < i; ++j)
                deallocate_node(*(this->impl.start.node - j));
            throw;
        }
    }

    template<typename Type, typename Alloc>
    constexpr void deque<Type, Alloc>::new_elements_at_back(szt new_elems)
    {
        if (max_size() - size() < new_elems)
            throw;

        const szt new_nodes = ((new_elems + buffer_size() - 1) / buffer_size());
        reserve_map_at_back(new_nodes);
        szt i;
        try
        {
            for (i = 1; i <= new_nodes; ++i)
                *(this->impl.finish.node + i) = allocate_node();
        } catch (...)
        {
            for (szt j = 1; j < i; ++j)
                deallocate_node(*(this->impl.finish.node + j));
            throw;
        }
    }

    template<typename Type, typename Alloc>
    constexpr void deque<Type, Alloc>::reallocate_map(szt nodes_to_add, bool add_at_front)
    {
        const szt old_num_nodes = this->impl.finish.node - this->impl.start.node + 1;
        const szt new_num_nodes = old_num_nodes + nodes_to_add;

        map_pointer new_nstart;
        if (this->impl.map_size > 2 * new_num_nodes)
        {
            new_nstart = this->impl.map + (this->impl.map_size - new_num_nodes) / 2 + (add_at_front ? nodes_to_add : 0);
            if (new_nstart < this->impl.start.node)
                copy(this->impl.start.node, this->impl.finish.node + 1, new_nstart);
            else
                copy_backward(this->impl.start.node, this->impl.finish.node + 1, new_nstart + old_num_nodes);
        } else
        {
            szt new_map_size = this->impl.map_size + max(this->impl.map_size, nodes_to_add) + 2;

            const szt bufsz = buffer_size();
            if (new_map_size > ((max_size() + bufsz - 1) / bufsz) * 2)
                __builtin_unreachable();

            map_pointer new_map = allocate_map(new_map_size);
            new_nstart          = new_map + (new_map_size - new_num_nodes) / 2 + (add_at_front ? nodes_to_add : 0);
            copy(this->impl.start.node, this->impl.finish.node + 1, new_nstart);
            deallocate_map(this->impl.map, this->impl.map_size);

            this->impl.map      = new_map;
            this->impl.map_size = new_map_size;
        }

        this->impl.start.set_node(new_nstart);
        this->impl.finish.set_node(new_nstart + old_num_nodes - 1);
    }
} // namespace SFTL
