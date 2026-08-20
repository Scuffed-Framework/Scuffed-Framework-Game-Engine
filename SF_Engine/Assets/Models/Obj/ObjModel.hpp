#pragma once

#include <Assets/Models/Model.hpp>

namespace SF::Engine
{
    /**
     * @brief Resource that represents a OBJ model.
     */
    class ObjModel : public Model::Registrar<ObjModel>
    {
        inline static const bool Registered = Register("obj", ".obj");
    
    public:
        /**
         * Creates a new OBJ model, or finds one with the same values.
         * @param filename The file to load the OBJ model from.
         * @return The OBJ model with the requested values.
         */
        static std::shared_ptr<ObjModel> Create(const std::filesystem::path &filename);

        /**
         * Creates a new OBJ model.
         * @param filename The file to load the OBJ model from.
         * @param load If this resource will be loaded immediately, otherwise {@link ObjModel#Load} can be called later.
         */
        explicit ObjModel(std::filesystem::path filename, bool load = true);

    private:
        void Load();

        std::filesystem::path filename;
    };
}
