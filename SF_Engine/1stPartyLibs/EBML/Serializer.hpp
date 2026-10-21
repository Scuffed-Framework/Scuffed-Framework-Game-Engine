#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <memory>
#include <mutex>
#include <ostream>
#include <ranges>
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include "CRC32.hpp"
#include "Element.hpp"
#include "Identifier.hpp"
#include "VINT.hpp"

namespace SF::EBML
{
    // Forward declaration
    class BackPatchableWriter;

    // Serialization context for tracking positions
    struct SerializationContext
    {
        // Start offset (id byte of the element header) of the FIRST occurrence of each element id.
        std::unordered_map<uint64_t, size_t> elementPositions;
        std::unordered_map<uint64_t, std::vector<size_t>> seekPositions;
        // Running absolute offset. Set it to where the bytes will land in the output
        // before calling serialize(); it is advanced as elements are serialized.
        size_t currentPosition = 0;
        bool isTwoPass         = false;
        bool isBackPatching    = false;
    };

    inline size_t serialized_size(const Element &element);

    // Size of just the payload (children or raw bytes), without the id/size header.
    inline size_t serialized_body_size(const Element &element)
    {
        if (!element.is_master())
            return element.raw().size();

        size_t total = 0;
        for (const auto &child: element.children())
            total += serialized_size(child);
        return total;
    }

    // Full serialized size: id + size field + payload.
    inline size_t serialized_size(const Element &element)
    {
        const size_t body = serialized_body_size(element);
        return element.id().encode().size() + encode_size(body).size() + body;
    }

    // Byte offset, inside the element's serialized bytes, where its payload begins
    // (i.e. the id + size-field length). Handy as `patchPosition` for write_with_backpatch.
    inline size_t payload_offset(const Element &element)
    {
        return element.id().encode().size() + encode_size(serialized_body_size(element)).size();
    }

    inline std::vector<byte> serialize(const Element &element, SerializationContext *ctx = nullptr)
    {
        // The header size depends on the payload size, so get that first. That lets every
        // nested element know its absolute start offset before it is serialized.
        const size_t start   = ctx ? ctx->currentPosition : 0;
        const size_t bodyLen = serialized_body_size(element);

        std::vector<byte> out = element.id().encode();
        const auto sizeBytes  = encode_size(bodyLen);
        out.insert(out.end(), sizeBytes.begin(), sizeBytes.end());
        out.reserve(out.size() + bodyLen);

        if (ctx)
        {
            ctx->elementPositions.try_emplace(element.id().value(), start);
            ctx->currentPosition = start + out.size(); // children start right after the header
        }

        if (element.is_master())
        {
            for (const auto &child: element.children())
            {
                auto childBytes = serialize(child, ctx);
                out.insert(out.end(), childBytes.begin(), childBytes.end());
            }
        } else
        {
            const auto &raw = element.raw();
            out.insert(out.end(), raw.begin(), raw.end());
        }

        if (ctx)
            ctx->currentPosition = start + out.size();

        return out;
    }

    // Back-patchable writer with support for updating SeekHead
    class BackPatchableWriter
    {
    private:
        std::ostream &out_;
        mutable std::mutex mutex_;
        size_t position_ = 0;
        bool isTwoPass_  = false;
        bool seekable_   = false;

        // A fixed 8-byte big-endian uint64 field, already written as a placeholder,
        // that gets overwritten later.
        struct PatchPoint
        {
            size_t position; // absolute offset in the output stream
            uint64_t value = 0;
            bool hasValue  = false; // value assigned but not yet written (deferred patch)
            bool isWritten = false;
        };
        std::unordered_map<uint64_t, std::vector<PatchPoint>> patches_;
        std::unordered_map<uint64_t, size_t> elementPositions_;

        // Appends the element at the current position and records the start offset of it
        // and everything nested in it. Returns the element's start offset.
        size_t append_locked(const Element &element)
        {
            SerializationContext ctx;
            ctx.currentPosition = position_;
            ctx.isTwoPass       = isTwoPass_;

            const auto bytes = serialize(element, &ctx);
            out_.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            if (!out_)
                throw std::runtime_error("BackPatchableWriter: write failed");

            for (const auto &[id, pos]: ctx.elementPositions)
                elementPositions_.try_emplace(id, pos);

            const size_t start = position_;
            position_ += bytes.size();
            return start;
        }

        void write_patch_locked(PatchPoint &patch)
        {
            if (!seekable_)
                throw std::runtime_error("BackPatchableWriter: cannot back-patch a non-seekable stream");

            std::array<byte, 8> bytes{};
            uint64_t v = patch.value;
            for (int i = 7; i >= 0; --i)
            {
                bytes[static_cast<size_t>(i)] = static_cast<byte>(v & 0xFF);
                v >>= 8;
            }

            const std::streampos resume = out_.tellp();
            out_.seekp(static_cast<std::streamoff>(patch.position));
            out_.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            out_.seekp(resume);

            if (!out_)
                throw std::runtime_error("BackPatchableWriter: failed to back-patch");
            patch.isWritten = true;
        }

    public:
        BackPatchableWriter(std::ostream &out, bool twoPass = false) : out_(out), isTwoPass_(twoPass)
        {
            // Non-seekable streams (pipes, sockets) report -1; they can write but not back-patch.
            const std::streampos p = out_.tellp();
            seekable_              = (p != std::streampos(-1));
            // Start from wherever the stream already is, so recorded offsets are absolute.
            position_ = seekable_ ? static_cast<size_t>(static_cast<std::streamoff>(p)) : 0;
        }

        void write(const Element &element)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            append_locked(element);
        }

        /**
         * @brief Writes `element` and registers an 8-byte big-endian uint64 field inside it
         *        to be overwritten later via patch_position(id, value).
         *        `patchPosition` is the byte offset of that field within the element's
         *        serialized bytes (see payload_offset() for the start of the payload).
         *        The element should already contain 8 placeholder bytes there.
         */
        void write_with_backpatch(uint64_t id, const Element &element, size_t patchPosition)
        {
            std::lock_guard<std::mutex> lock(mutex_);

            if (patchPosition + sizeof(uint64_t) > serialized_size(element))
                throw std::out_of_range("BackPatchableWriter: patch field does not fit inside the element");

            const size_t start = append_locked(element);
            patches_[id].push_back(PatchPoint{start + patchPosition});
        }

        /**
         * @brief Assigns `value` to every unwritten patch point registered under `id`.
         *        Written immediately, unless `defer` is set, in which case it waits for
         *        apply_all_patches().
         */
        void patch_position(uint64_t id, uint64_t value, bool defer = false)
        {
            std::lock_guard<std::mutex> lock(mutex_);

            auto it = patches_.find(id);
            if (it == patches_.end())
                throw std::runtime_error("No patch point found for ID");

            for (auto &patch: it->second)
            {
                if (patch.isWritten)
                    continue;

                patch.value    = value; // each patch gets the full value
                patch.hasValue = true;
                if (!defer)
                    write_patch_locked(patch);
            }
        }

        size_t tell() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return position_;
        }

        void flush()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            out_.flush();
        }

        bool is_two_pass() const { return isTwoPass_; }
        bool is_seekable() const { return seekable_; }

        // Absolute start offset of the first element written with this id (nested ones included).
        size_t get_element_position(uint64_t id) const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = elementPositions_.find(id);
            return it != elementPositions_.end() ? it->second : 0;
        }

        // True while any patch point for this id is still unwritten.
        bool needs_seek_update(uint64_t id) const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = patches_.find(id);
            if (it == patches_.end())
                return false;
            return std::ranges::any_of(it->second, [](const PatchPoint &p) { return !p.isWritten; });
        }

        // Writes every deferred patch (assigned via patch_position(..., defer = true)).
        // Patch points that were never given a value are left as their placeholder.
        void apply_all_patches()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto &patches: patches_ | std::views::values)
            {
                for (auto &patch: patches)
                {
                    if (patch.hasValue && !patch.isWritten)
                        write_patch_locked(patch);
                }
            }
        }
    };

    inline std::vector<byte> serialize(const ElementList &elements, SerializationContext *ctx = nullptr)
    {
        std::vector<byte> out;
        for (const auto &e: elements)
        {
            auto bytes = serialize(e, ctx);
            out.insert(out.end(), bytes.begin(), bytes.end());
        }
        return out;
    }

    // Two-pass muxing support
    class TwoPassMuxer
    {
    private:
        std::vector<Element> elements_;
        std::unordered_map<uint64_t, size_t> elementPositions_;

    public:
        void add_element(Element element) { elements_.push_back(std::move(element)); }

        // Pass 1: works out where every element (nested ones included) will land,
        // relative to the start of the first element added.
        void compute_positions()
        {
            SerializationContext ctx;
            ctx.isTwoPass = true;
            for (const auto &e: elements_)
                serialize(e, &ctx); // bytes discarded, only the positions matter
            elementPositions_ = std::move(ctx.elementPositions);
        }

        void write(std::ostream &out, bool backPatch = true)
        {
            BackPatchableWriter writer(out, true);

            for (auto &element: elements_)
                writer.write(element);

            if (backPatch)
                writer.apply_all_patches();

            writer.flush();
        }

        size_t get_element_position(uint64_t id) const
        {
            auto it = elementPositions_.find(id);
            return it != elementPositions_.end() ? it->second : 0;
        }
    };
} // namespace SF::EBML
