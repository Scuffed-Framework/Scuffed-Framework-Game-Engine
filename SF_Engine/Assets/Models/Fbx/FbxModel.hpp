#pragma once

#include <Assets/Models/Model.hpp>

namespace SF::Engine
{
    /**
     * @brief Resource that represents an FBX model.
     */
    class FbxModel : public Model::Registrar<FbxModel>
    {
        inline static const bool Registered = Register("fbx", ".fbx");

    public:
        /**
         * Creates a new FBX model, or finds one with the same values.
         * @param filename The file to load the FBX model from.
         * @return The FBX model with the requested values.
         */
        static std::shared_ptr<FbxModel> Create(const std::filesystem::path &filename);

        /**
         * Creates a new FBX model.
         * @param filename The file to load the FBX model from.
         * @param load If this resource will be loaded immediately, otherwise {@link FbxModel#Load} can be called later.
         */
        explicit FbxModel(std::filesystem::path filename, bool load = true);

    private:
        void Load();

        std::filesystem::path filename;
    };
}