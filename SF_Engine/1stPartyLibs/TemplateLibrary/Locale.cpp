#include "Locale.hpp"

namespace SFTL
{
    size_t locale::id::refcount         = 0;
    const char locale::facet::c_name[2] = "C";
    template<typename Facet>
    locale::locale(const locale &other, Facet *fac)
    {
        impl = new IMPL(1);
        if (fac)
        {
            fac->add_reference();
        }
    }

    template<typename facet>
    locale locale::combine(const locale &other) const
    {
        const auto *fac = (other.impl && other.impl->facets) ? other.impl->facets[facet::id.ident()] : nullptr;
        return locale(*this, const_cast<facet *>(static_cast<const facet *>(fac)));
    }
    bool locale::operator==(const locale &other) const noexcept { return impl == other.impl; }

    template<typename Character, typename Traits>
    bool locale::operator()(const AdvancedString<Character, Traits> &s1,
                            const AdvancedString<Character, Traits> &s2) const
    {
        const auto &col = use_facet<SFTL::collate<Character>>(*this);
        return col.compare(s1.data(), s1.data() + s1.size(), s2.data(), s2.data() + s2.size()) < 0;
    }

    locale::IMPL::~IMPL() noexcept
    {
        delete[] facets;
        delete[] names;
    }

    locale::locale() noexcept
    {
        initialize();
        impl = classic().impl;
        if (impl)
            impl->add_reference();
    }

    locale::locale(const locale &other) noexcept : impl(other.impl)
    {
        if (impl)
            impl->add_reference();
    }

    locale::locale(const locale &base, const char *s, category cat)
    {
        (void) cat;
        impl = new IMPL(1);
        if (s)
        {
            impl = base.impl;
            if (impl)
                impl->add_reference();
        }
    }

    locale::locale(const locale &base, const locale &add, category cat)
    {
        (void) cat;
        impl = new IMPL(1);
        impl = base.impl;
        if (impl)
            impl->add_reference();
        (void) add;
    }

    locale::locale(IMPL *impl) noexcept : impl(impl)
    {
        if (impl)
            impl->add_reference();
    }

    locale::~locale() noexcept
    {
        if (impl)
        {
            impl->remove_reference();
        }
    }

    const locale &locale::operator=(const locale &other) noexcept
    {
        if (impl != other.impl)
        {
            if (impl)
                impl->remove_reference();
            impl = other.impl;
            if (impl)
                impl->add_reference();
        }
        return *this;
    }

    string locale::name() const
    {
        if (impl && impl->names && impl->names[0])
        {
            return string(impl->names[0]);
        }
        return string("C");
    }

    const locale &locale::classic() noexcept
    {
        static locale classic_loc([]() { return new IMPL(1); }());
        return classic_loc;
    }

    void locale::initialize() {}
} // namespace SFTL
