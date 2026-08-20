#pragma once

#include <Assets/Models/Model.hpp>
#include <Rendering/Images/Image2d.hpp>

namespace SF::Engine
{
    /**
     * @brief Resource that represents a GLTF model.
     */
    class GltfModel : public Model::Registrar<GltfModel>
    {
        inline static const bool Registered = Register("gltf", ".gltf");

    public:
        /**
         * Creates a new GLTF model, or finds one with the same values.
         * @param filename The file to load the GLTF model from.
         * @return The GLTF model with the requested values.
         */
        static std::shared_ptr<GltfModel> Create(const std::filesystem::path &filename);

        /**
         * Creates a new GLTF model.
         * @param filename The file to load the GLTF model from.
         * @param load If this resource will be loaded immediately, otherwise {@link ModelGltf#Load} can be called later.
         */
        explicit GltfModel(std::filesystem::path filename, bool load = true);

    private:
        void Load();

        std::filesystem::path filename;
    };
}
