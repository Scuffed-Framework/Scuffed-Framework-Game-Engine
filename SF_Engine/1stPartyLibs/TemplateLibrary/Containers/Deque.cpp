#include "Deque.hpp"
#include "../Streams/InOutStream.hpp"
namespace SFTL
{
    template<typename Type, typename Alloc>
    void deque<Type, Alloc>::range_check(szt n) const
    {
        if (n >= this->size())
            cout << "deque::range_check out of range";
    }

    template<typename Type, typename Alloc>
    size_type deque<Type, Alloc>::check_init_len(size_type n, const allocator_type &al)
    {
        if (n > max_size_internal(al))
            cout << "cannot create deque larger than max_size()";
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
    constexpr typename deque<Type, Alloc>::iterator deque<Type, Alloc>::emplace(const_iterator position, Args &&...args)
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
    constexpr typename deque<Type, Alloc>::iterator deque<Type, Alloc>::insert(const_iterator position,
                                                                               const value_type &x)
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
    constexpr typename deque<Type, Alloc>::iterator deque<Type, Alloc>::erase_aux(iterator position)
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
    constexpr typename deque<Type, Alloc>::iterator deque<Type, Alloc>::erase_aux(iterator first, iterator last)
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
                __uninitialized_fill_a(new_start, this->impl.start, x, getTypeAllocator());
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
                __uninitialized_fill_a(this->impl.finish, new_finish, x, getTypeAllocator());
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
                __uninitialized_default_a(this->impl.finish, new_finish, getTypeAllocator());
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
                __uninitialized_fill_a(*cur, *cur + buffer_size(), value, getTypeAllocator());
            __uninitialized_fill_a(this->impl.finish.first, this->impl.finish.current, value, getTypeAllocator());
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
                __uninitialized_copy_a(first, mid, *cur_node, getTypeAllocator());
                first = mid;
            }
            __uninitialized_copy_a(first, last, this->impl.finish.first, getTypeAllocator());
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
            cout << "cannot create deque larger than max_size()";

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
            cout << "cannot create deque larger than max_size()";

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
            __uninitialized_copy_a(move(first), last, new_start, getTypeAllocator());
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
            __uninitialized_copy_a(move(first), last, this->impl.finish, getTypeAllocator());
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
    constexpr typename deque<Type, Alloc>::iterator deque<Type, Alloc>::emplace_aux(iterator pos, Args &&...args)
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
                    __uninitialized_move_a(this->impl.start, start_n, new_start, getTypeAllocator());
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
                    __uninitialized_move_a(finish_n, this->impl.finish, this->impl.finish, getTypeAllocator());
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
                    __uninitialized_move_a(this->impl.start, start_n, new_start, getTypeAllocator());
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
                    __uninitialized_move_a(finish_n, this->impl.finish, this->impl.finish, getTypeAllocator());
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
        for (map_pointer node = first.node + 1; node < last.node; ++node)
            destroy(*node, *node + buffer_size(), getTypeAllocator());

        if (first.node != last.node)
        {
            destroy(first.current, first.last, getTypeAllocator());
            destroy(last.first, last.current, getTypeAllocator());
        } else
            destroy(first.current, last.current, getTypeAllocator());
    }

    template<typename Type, typename Alloc>
    constexpr void deque<Type, Alloc>::new_elements_at_front(szt new_elems)
    {
        if (max_size() - size() < new_elems)
            cout << "deque::new_elements_at_front overflow";

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
            cout << "deque::new_elements_at_back overflow";

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
