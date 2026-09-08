#pragma once
#include "Containers/String.hpp"

namespace SFTL
{
    struct locale_struct
    {
        const unsigned short int *ctype_b;
        const int *ctype_tolower;
        const int *ctype_toupper;
        const char *names[13];
    };

    using clocale = locale_struct *;

    class locale
    {
    public:
        using category = int;

        class facet;
        class id;
        class IMPL;

        friend class facet;
        friend class IMPL;

        template<typename Facet>
        friend bool has_facet(const locale &) noexcept;

        template<typename Facet>
        friend const Facet &use_facet(const locale &);

        template<typename Facet>
        friend const Facet *try_use_facet(const locale &);

        static constexpr category none     = 0;
        static constexpr category ctype    = 1 << 0;
        static constexpr category numeric  = 1 << 1;
        static constexpr category collate  = 1 << 2;
        static constexpr category time     = 1 << 3;
        static constexpr category monetary = 1 << 4;
        static constexpr category messages = 1 << 5;
        static constexpr category all      = (ctype | numeric | collate | time | monetary | messages);

        locale() noexcept;
        locale(const locale &other) noexcept;
        locale(const locale &base, const char *s, category cat);

        locale(const locale &base, const string &s, category cat) : locale(base, s.c_str(), cat) {}

        locale(const locale &base, const locale &add, category cat);

        template<typename Facet>
        locale(const locale &other, Facet *f);

        ~locale() noexcept;

        const locale &operator=(const locale &other) noexcept;

        template<typename Facet>
        [[nodiscard]] locale combine(const locale &other) const;

        [[nodiscard]] string name() const;
        [[nodiscard]] bool operator==(const locale &other) const noexcept;
        [[nodiscard]] bool operator!=(const locale &other) const noexcept { return !(*this == other); }

        template<typename Character, typename Traits>
        [[nodiscard]] bool operator()(const AdvancedString<Character, Traits> &s1,
                                      const AdvancedString<Character, Traits> &s2) const;

        [[nodiscard]] static const locale &classic() noexcept;

    private:
        IMPL *impl;

        explicit locale(IMPL *) noexcept;
        static void initialize();
    };

    class locale::facet
    {
    private:
        friend class locale;
        friend class locale::IMPL;

        mutable int refcount;
        static clocale c_locale;
        static const char c_name[2];

    protected:
        explicit facet(size_type refs = 0) noexcept : refcount(refs ? 1 : 0) {}
        virtual ~facet();

        static void create_c_locale(clocale &cl, const char *s, clocale old = nullptr);
        static clocale clone_c_locale(clocale &cl) noexcept;
        static void destroy_c_locale(clocale &cl);
        static clocale get_c_locale();

        facet(const facet &)            = delete;
        facet &operator=(const facet &) = delete;

    private:
        void add_reference() const noexcept { ++refcount; }
        void remove_reference() const noexcept
        {
            if (--refcount == 0)
            {
                delete this;
            }
        }
    };

    class locale::id
    {
    private:
        friend class locale;
        friend class locale::IMPL;

        mutable size_type index = 0;
        static size_type refcount;

        id(const id &)             = delete;
        void operator=(const id &) = delete;

    public:
        id() {}
        size_type ident() const noexcept;
    };

    class locale::IMPL
    {
    public:
        friend class locale;
        friend class locale::facet;

    protected:
        const facet **facets;

    private:
        int refcount;
        size_type facets_size;
        char **names;

        void add_reference() noexcept { ++refcount; }
        void remove_reference() noexcept
        {
            if (--refcount == 0)
            {
                delete this;
            }
        }

        IMPL(size_type refs) noexcept : refcount(1), facets(nullptr), facets_size(0), names(nullptr) {}
        ~IMPL() noexcept;
    };

    template<typename CharacterType>
    class collate : public locale::facet
    {
    public:
        using char_type   = CharacterType;
        using string_type = AdvancedString<CharacterType>;

    protected:
        clocale c_locale_collate;

    public:
        static locale::id id;

        explicit collate(size_type refs = 0) : facet(refs), c_locale_collate(get_c_locale()) {}
        explicit collate(clocale cl, size_type refs = 0) : facet(refs), c_locale_collate(clone_c_locale(cl)) {}

        int compare(const CharacterType *low1, const CharacterType *high1, const CharacterType *low2,
                    const CharacterType *high2) const
        {
            return do_compare(low1, high1, low2, high2);
        }

        string_type transform(const CharacterType *lo, const CharacterType *hi) const { return do_transform(lo, hi); }

        long hash(const CharacterType *lo, const CharacterType *hi) const { return do_hash(lo, hi); }

    protected:
        ~collate() override { destroy_c_locale(c_locale_collate); }

        virtual int do_compare(const CharacterType *low1, const CharacterType *high1, const CharacterType *low2,
                               const CharacterType *high2) const
        {
            while (low1 < high1 && low2 < high2)
            {
                if (*low1 < *low2)
                    return -1;
                if (*low2 < *low1)
                    return 1;
                ++low1;
                ++low2;
            }
            if (low1 < high1)
                return 1;
            if (low2 < high2)
                return -1;
            return 0;
        }

        virtual string_type do_transform(const CharacterType *lo, const CharacterType *hi) const
        {
            return string_type(lo, static_cast<size_type>(hi - lo));
        }

        virtual long do_hash(const CharacterType *lo, const CharacterType *hi) const
        {
            unsigned long val = 0;
            for (; lo < hi; ++lo)
                val = val * 33 + static_cast<unsigned long>(*lo);
            return static_cast<long>(val);
        }
    };

    template<typename CharacterType>
    locale::id collate<CharacterType>::id;

    template<typename CharacterType>
    class collate_byname : public collate<CharacterType>
    {
    public:
        using char_type   = CharacterType;
        using string_type = AdvancedString<CharacterType>;

        explicit collate_byname(const char *s, size_type refs = 0) : collate<CharacterType>(refs)
        {
            if (s && strcmp(s, "C") != 0 && strcmp(s, "POSIX") != 0)
            {
                this->destroy_c_locale(this->c_locale_collate);
                this->create_c_locale(this->c_locale_collate, s);
            }
        }

        explicit collate_byname(const string &s, size_type refs = 0) : collate_byname(s.c_str(), refs) {}

    protected:
        ~collate_byname() override {}
    };
} // namespace SFTL
