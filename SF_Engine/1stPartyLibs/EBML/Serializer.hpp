#pragma once

#include "CRC32.hpp"
#include "Element.hpp"
#include "Identifier.hpp"
#include "VINT.hpp"
#include <algorithm>
#include <stdexcept>
#include <vector>
#include <fstream>
#include <memory>
#include <unordered_map>

namespace SF::EBML
{
    // Forward declaration
    class BackPatchableWriter;

    // Serialization context for tracking positions
    struct SerializationContext
    {
        std::unordered_map<uint64_t, size_t> elementPositions;
        std::unordered_map<uint64_t, std::vector<size_t>> seekPositions;
        size_t currentPosition = 0;
        bool isTwoPass = false;
        bool isBackPatching = false;
    };

    inline std::vector<byte> serialize(const Element& element, SerializationContext* ctx = nullptr)
    {
        std::vector<byte> body;
        
        if (element.is_master())
        {
            for (const auto& child : element.children())
            {
                auto childBytes = serialize(child, ctx);
                body.insert(body.end(), childBytes.begin(), childBytes.end());
            }
        }
        else
        {
            body = element.raw();
        }

        std::vector<byte> out = element.id().encode();
        auto sizeBytes = encode_size(body.size());
        out.insert(out.end(), sizeBytes.begin(), sizeBytes.end());
        out.insert(out.end(), body.begin(), body.end());

        if (ctx)
        {
            ctx->currentPosition += out.size();
            ctx->elementPositions[element.id().value()] = ctx->currentPosition;
        }

        return out;
    }

    // Back-patchable writer with support for updating SeekHead
    class BackPatchableWriter
    {
    private:
        std::ostream& out_;
        mutable std::mutex mutex_;
        size_t position_ = 0;
        bool isTwoPass_ = false;
        
        // Track positions that need back-patching
        struct PatchPoint
        {
            size_t position;
            uint64_t value;
            bool isWritten = false;
        };
        std::unordered_map<uint64_t, std::vector<PatchPoint>> patches_;
        std::unordered_map<uint64_t, size_t> elementPositions_;

    public:
        BackPatchableWriter(std::ostream& out, bool twoPass = false)
            : out_(out), isTwoPass_(twoPass) {}

        void write(const Element& element)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            auto bytes = serialize(element);
            out_.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
            
            // Track position for this element
            elementPositions_[element.id().value()] = position_;
            position_ += bytes.size();
        }

        void write_with_backpatch(uint64_t id, const Element& element, size_t patchPosition)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            // Write placeholder
            std::vector<byte> placeholder(8, 0); // 8 bytes for uint64
            size_t writePos = position_;
            out_.write(reinterpret_cast<const char*>(placeholder.data()), placeholder.size());
            position_ += placeholder.size();
            
            // Record patch point
            patches_[id].push_back({writePos, 0, false});
        }

        void patch_position(uint64_t id, uint64_t value)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            auto it = patches_.find(id);
            if (it == patches_.end())
                throw std::runtime_error("No patch point found for ID");
            
            for (auto& patch : it->second)
            {
                if (!patch.isWritten)
                {
                    // Seek to position and write value
                    auto currentPos = out_.tellp();
                    out_.seekp(patch.position);
                    
                    std::vector<byte> bytes(8);
                    for (int i = 7; i >= 0; --i)
                    {
                        bytes[i] = static_cast<byte>(value & 0xFF);
                        value >>= 8;
                    }
                    out_.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
                    
                    out_.seekp(currentPos);
                    patch.isWritten = true;
                }
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

        // Get position of an element
        size_t get_element_position(uint64_t id) const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = elementPositions_.find(id);
            return it != elementPositions_.end() ? it->second : 0;
        }

        // Check if a seek entry needs updating
        bool needs_seek_update(uint64_t id) const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return patches_.find(id) != patches_.end();
        }

        // Apply all pending patches
        void apply_all_patches()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& [id, patches] : patches_)
            {
                for (auto& patch : patches)
                {
                    if (!patch.isWritten)
                    {
                        auto currentPos = out_.tellp();
                        out_.seekp(patch.position);
                        std::vector<byte> bytes(8);
                        uint64_t value = patch.value;
                        for (int i = 7; i >= 0; --i)
                        {
                            bytes[i] = static_cast<byte>(value & 0xFF);
                            value >>= 8;
                        }
                        out_.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
                        out_.seekp(currentPos);
                        patch.isWritten = true;
                    }
                }
            }
        }
    };

    inline std::vector<byte> serialize(const ElementList& elements, SerializationContext* ctx = nullptr)
    {
        std::vector<byte> out;
        for (const auto& e : elements)
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
        void add_element(Element element)
        {
            elements_.push_back(std::move(element));
        }

        void compute_positions()
        {
            size_t pos = 0;
            SerializationContext ctx;
            for (const auto& e : elements_)
            {
                auto bytes = serialize(e, &ctx);
                elementPositions_[e.id().value()] = pos;
                pos += bytes.size();
            }
        }

        void write(std::ostream& out, bool backPatch = true)
        {
            BackPatchableWriter writer(out, true);
            
            // First pass: write with placeholders
            for (auto& element : elements_)
            {
                writer.write(element);
            }
            
            // Second pass: apply back-patches if needed
            if (backPatch)
            {
                writer.apply_all_patches();
            }
            
            writer.flush();
        }

        size_t get_element_position(uint64_t id) const
        {
            auto it = elementPositions_.find(id);
            return it != elementPositions_.end() ? it->second : 0;
        }
    };
}