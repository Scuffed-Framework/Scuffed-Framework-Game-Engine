#pragma once

#include "Layer.hpp"
#include "UtilityClasses/NoCopy.hpp"
#include <Communication/Events/EventDispatcher.hpp>
#include <vector>
#include <algorithm>

namespace SF::Engine {
    class LayerStack : NoCopy {
    public:
        LayerStack() = default;
        
        ~LayerStack() {
            Clear();
        }

        void PushLayer(std::shared_ptr<Layer> layer) {
            if (!layer) return;
            
            _layers.emplace_back(std::move(layer));
            _layers.back()->OnAttach();
        }
        
        void PushOverlay(std::shared_ptr<Layer> overlay) {
            if (!overlay) return;
            
            _overlays.emplace_back(std::move(overlay));
            _overlays.back()->OnAttach();
        }

        void PopLayer(const std::shared_ptr<Layer>& layer) {
            auto it = std::find(_layers.begin(), _layers.end(), layer);
            if (it != _layers.end()) {
                (*it)->OnDetach();
                _layers.erase(it);
            }
        }
        
        void PopOverlay(const std::shared_ptr<Layer>& overlay) {
            auto it = std::find(_overlays.begin(), _overlays.end(), overlay);
            if (it != _overlays.end()) {
                (*it)->OnDetach();
                _overlays.erase(it);
            }
        }

        void UpdateLayers() {
            // Update layers first, then overlays (overlays are always on top)
            for (auto& layer : _layers) {
                layer->OnUpdate();
            }
            for (auto& overlay : _overlays) {
                overlay->OnUpdate();
            }
        }

        void RenderImGui() {
            // Render layers first, then overlays on top
            for (auto& layer : _layers) {
                layer->OnImGuiRender();
            }
            for (auto& overlay : _overlays) {
                overlay->OnImGuiRender();
            }
        }
        
        // Dispatch events in reverse order (top layer gets priority)
        template<typename EventType>
        void DispatchEvent(EventType& event) {
            // Overlays get events first (they're on top)
            for (auto it = _overlays.rbegin(); it != _overlays.rend(); ++it) {
                (*it)->OnEvent(event);
                if (event.handled) return;
            }
            
            // Then layers
            for (auto it = _layers.rbegin(); it != _layers.rend(); ++it) {
                (*it)->OnEvent(event);
                if (event.handled) return;
            }
        }
        
        void Clear() {
            for (auto& overlay : _overlays) {
                overlay->OnDetach();
            }
            for (auto& layer : _layers) {
                layer->OnDetach();
            }
            _overlays.clear();
            _layers.clear();
        }
        
        size_t GetLayerCount() const { return _layers.size(); }
        size_t GetOverlayCount() const { return _overlays.size(); }
        
    private:
        std::vector<std::shared_ptr<Layer>> _layers;
        std::vector<std::shared_ptr<Layer>> _overlays;  // Always rendered on top
    };
}