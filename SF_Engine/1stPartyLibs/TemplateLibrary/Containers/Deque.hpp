#pragma once
#include "../Algorithm.hpp"
#include "../Allocator.hpp"
#include "../Compare.hpp"
#include "../Iterators.hpp"
#include "../NumericProperties.hpp"
#include "../PointerTraits.hpp"
#include "../Streams/BasicOut.hpp"
#include "../TypeTraits.hpp"

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
            typedef typename Alloc::template rebind<Type>::other TypeAllocator;
            typedef allocator_traits<TypeAllocator> AllocTraits;

            typedef typename AllocTraits::pointer Pointer;
            typedef typename AllocTraits::const_pointer Pointer_const;

            typedef typename AllocTraits::template rebind<Pointer>::other MapAlloc_type;
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

            typedef typename iterator::map_pointer map_pointer;

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
        typedef typename Base::TypeAllocator TypeAllocator;
        typedef typename Base::AllocTraits AllocTraits;
        typedef typename Base::map_pointer map_pointer;

    public:
        typedef Type value_type;

        typedef typename AllocTraits::pointer pointer;
        typedef typename AllocTraits::const_pointer const_pointer;
        typedef value_type &reference;
        typedef const value_type &const_reference;

        typedef typename Base::iterator iterator;
        typedef typename Base::const_iterator const_iterator;
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

        deque(const deque &x) : Base(AllocTraits::_S_select_on_copy(x.getTypeAllocator()), x.size())
        {
            __uninitialized_copy_a(x.begin(), x.end(), this->impl.start, getTypeAllocator());
        }

        deque(deque &&) = default;

        deque(const deque &x, const type_identity_t<allocator_type> &al) : Base(al, x.size())
        {
            __uninitialized_copy_a(x.begin(), x.end(), this->impl.start, getTypeAllocator());
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
                __uninitialized_move_a(x.begin(), x.end(), this->impl.start, getTypeAllocator());
                x.clear();
            }
        }

    public:
        deque(initializer_list<value_type> l, const allocator_type &al = allocator_type()) : Base(al)
        {
            range_initialize(l.begin(), l.end(), random_access_iterator_tag());
        }

        template<typename InputIterator, typename = RequireInputIter<InputIterator>>
        deque(InputIterator first, InputIterator last, const allocator_type &al = allocator_type()) : Base(al)
        {
            range_initialize(first, last, typename iterator_traits<InputIterator>::iterator_category());
        }

        ~deque() { destroy_data(begin(), end(), getTypeAllocator()); }

        deque &operator=(const deque &x);
        deque &operator=(deque &&x) noexcept(AllocTraits::_S_always_equal())
        {
            using always_equal = typename AllocTraits::is_always_equal;
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
        void resize(szt new_size, const value_type &x);
        void shrink_to_fit() noexcept { shrink_to_fit_internal(); }

        [[nodiscard]] bool empty() const noexcept { return this->impl.finish == this->impl.start; }

        [[nodiscard]] reference operator[](szt n) noexcept { return this->impl.start[difference_type(n)]; }
        [[nodiscard]] const_reference operator[](szt n) const noexcept { return this->impl.start[difference_type(n)]; }

    protected:
        void range_check(szt n) const
        {
            if (n >= this->size())
                cout << "deque::range_check out of range";
        }

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
        reference emplace_front(Args &&...args);

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
        reference emplace_back(Args &&...args);

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
        iterator emplace(const_iterator position, Args &&...args);

        iterator insert(const_iterator position, const value_type &x);

        iterator insert(const_iterator position, value_type &&x) { return emplace(position, move(x)); }

        iterator insert(const_iterator p, initializer_list<value_type> l)
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

        iterator erase(const_iterator first, const_iterator last)
        {
            return erase_aux(first.const_cast_self(), last.const_cast_self());
        }

        void swap(deque &x) noexcept
        {
            static_assert(AllocTraits::propagate_on_container_swap::value ||
                          getTypeAllocator() == x.getTypeAllocator());

            this->impl.swap_data(x.impl);
            AllocTraits::_S_on_swap(getTypeAllocator(), x.getTypeAllocator());
        }

        void clear() noexcept { erase_at_end(begin()); }

    protected:
        static size_type check_init_len(size_type n, const allocator_type &al)
        {
            if (n > max_size_internal(al))
                cout << "cannot create deque larger than max_size()";
            return n;
        }

        static szt max_size_internal(const TypeAllocator &al) noexcept
        {
            const size_type diffmax  = Detail::__numeric_traits<ptrdiff_t>::__max;
            const size_type allocmax = AllocTraits::max_size(al);
            return (min) (diffmax, allocmax);
        }

        template<typename InputIterator>
        void range_initialize(InputIterator first, InputIterator last, input_iterator_tag);

        template<typename ForwardIterator>
        void range_initialize(ForwardIterator first, ForwardIterator last, forward_iterator_tag);

        void fill_initialize(const value_type &value);

        void default_initialize();

        template<typename InputIterator>
        void assign_aux(InputIterator first, InputIterator last, input_iterator_tag);

        template<typename ForwardIterator>
        void assign_aux(ForwardIterator first, ForwardIterator last, forward_iterator_tag)
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
        void push_back_aux(Args &&...args);

        template<typename... Args>
        void push_front_aux(Args &&...args);

        void pop_back_aux();

        void pop_front_aux();

        template<typename InputIterator, typename Sentinel>
        void range_prepend(InputIterator first, Sentinel last, szt n);

        template<typename InputIterator, typename Sentinel>
        void range_append(InputIterator first, Sentinel last, szt n);

        template<typename InputIterator>
        void range_insert_aux(iterator pos, InputIterator first, InputIterator last, input_iterator_tag);

        template<typename ForwardIterator>
        void range_insert_aux(iterator pos, ForwardIterator first, ForwardIterator last, forward_iterator_tag);

        void fill_insert(iterator pos, szt n, const value_type &x);

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
            constexpr Type *get_pointer() noexcept { return alddressof(tmp_val); }

            union
            {
                Type tmp_val;
            };

            deque *this_deque;
        };

        iterator insert_aux_elem(iterator pos, const value_type &x) { return emplace_aux(pos, x); }

        template<typename... Args>
        iterator emplace_aux(iterator pos, Args &&...args);

        void insert_aux_fill(iterator pos, szt n, const value_type &x);

        template<typename ForwardIterator>
        void insert_aux_range(iterator pos, ForwardIterator first, ForwardIterator last, szt n);

        void destroy_data_aux(iterator first, iterator last);

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

        iterator erase_aux(iterator pos);

        iterator erase_aux(iterator first, iterator last);

        void default_append(szt n);

        bool shrink_to_fit_internal();

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

        void new_elements_at_front(szt new_elements);

        void new_elements_at_back(szt new_elements);

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

        void reallocate_map(szt nodes_to_add, bool add_at_front);

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

            constexpr bool move_storage = AllocTraits::_S_propagate_on_move_assign();
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

    template<typename InputIterator, typename ValT = typename iterator_traits<InputIterator>::value_type,
             typename Allocator = allocator<ValT>, typename = RequireInputIter<InputIterator>,
             typename = require_allocator<Allocator>>
    deque(InputIterator, InputIterator, Allocator = Allocator()) -> deque<ValT, Allocator>;

    template<typename Type, typename Alloc>
    [[nodiscard]] inline bool operator==(const deque<Type, Alloc> &x, const deque<Type, Alloc> &y)
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
} // namespace SFTL
