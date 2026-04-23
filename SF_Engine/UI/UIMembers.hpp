#pragma once
#include <tuple>
#include <string_view>
#include <type_traits>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <ImGui/ocornut/imgui.h>

namespace SF::Engine
{

    // ---------------------------------------------------------------------------
    // Type traits
    // ---------------------------------------------------------------------------

    template <typename T>
    struct is_vec2 : std::false_type
    {
    };
    template <>
    struct is_vec2<glm::vec2> : std::true_type
    {
    };

    template <typename T>
    struct is_vec3 : std::false_type
    {
    };
    template <>
    struct is_vec3<glm::vec3> : std::true_type
    {
    };

    template <typename T>
    struct is_vec4 : std::false_type
    {
    };
    template <>
    struct is_vec4<glm::vec4> : std::true_type
    {
    };

    template <typename T>
    struct is_quat : std::false_type
    {
    };
    template <>
    struct is_quat<glm::quat> : std::true_type
    {
    };

    template <typename T>
    struct is_mat4 : std::false_type
    {
    };
    template <>
    struct is_mat4<glm::mat4> : std::true_type
    {
    };

    // ---------------------------------------------------------------------------
    // MemberInfo  holds one member pointer + its display name
    // ---------------------------------------------------------------------------

    template <typename T, typename M>
    struct MemberInfo
    {
        using ClassType = T;
        using MemberType = M;

        const char *name;
        M T::*ptr;
    };

    // Helper to build a MemberInfo without spelling out the types
    template <typename T, typename M>
    constexpr MemberInfo<T, M> makeMember(const char *name, M T::*ptr)
    {
        return {name, ptr};
    }

    // ---------------------------------------------------------------------------
    // ReflectionData  specialise this for every reflected struct
    // ---------------------------------------------------------------------------

    template <typename T>
    struct ReflectionData;
    // Left intentionally undefined; REFLECT macro provides the specialisation.

    // ---------------------------------------------------------------------------
    // REFLECT macro
    // Place inside the struct body:
    //   REFLECT(MyStruct, x, y, enabled)
    // ---------------------------------------------------------------------------

#define REFLECT(Type, ...)                            \
    friend struct ::SF::Engine::ReflectionData<Type>; \
    static constexpr auto _reflect()                  \
    {                                                 \
        return std::make_tuple(                       \
            SF_REFLECT_MEMBERS(Type, __VA_ARGS__));   \
    }

// Internal helper  expands each field name into a makeMember call
#define SF_REFLECT_ONE(Type, Field) \
    ::SF::Engine::makeMember(#Field, &Type::Field)

// Supports up to 16 members; extend the _16/_15/… ladder as needed
#define SF_REFLECT_MEMBERS(T, ...) SF_RM_DISPATCH(__VA_ARGS__,                                                   \
                                                  SF_RM16, SF_RM15, SF_RM14, SF_RM13, SF_RM12, SF_RM11, SF_RM10, \
                                                  SF_RM9, SF_RM8, SF_RM7, SF_RM6, SF_RM5, SF_RM4, SF_RM3, SF_RM2, SF_RM1)(T, __VA_ARGS__)

#define SF_RM_DISPATCH(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, NAME, ...) NAME
#define SF_RM1(T, a) SF_REFLECT_ONE(T, a)
#define SF_RM2(T, a, b) SF_RM1(T, a), SF_REFLECT_ONE(T, b)
#define SF_RM3(T, a, b, c) SF_RM2(T, a, b), SF_REFLECT_ONE(T, c)
#define SF_RM4(T, a, b, c, d) SF_RM3(T, a, b, c), SF_REFLECT_ONE(T, d)
#define SF_RM5(T, a, b, c, d, e) SF_RM4(T, a, b, c, d), SF_REFLECT_ONE(T, e)
#define SF_RM6(T, a, b, c, d, e, f) SF_RM5(T, a, b, c, d, e), SF_REFLECT_ONE(T, f)
#define SF_RM7(T, a, b, c, d, e, f, g) SF_RM6(T, a, b, c, d, e, f), SF_REFLECT_ONE(T, g)
#define SF_RM8(T, a, b, c, d, e, f, g, h) SF_RM7(T, a, b, c, d, e, f, g), SF_REFLECT_ONE(T, h)
#define SF_RM9(T, a, b, c, d, e, f, g, h, i) SF_RM8(T, a, b, c, d, e, f, g, h), SF_REFLECT_ONE(T, i)
#define SF_RM10(T, a, b, c, d, e, f, g, h, i, j) SF_RM9(T, a, b, c, d, e, f, g, h, i), SF_REFLECT_ONE(T, j)
#define SF_RM11(T, a, b, c, d, e, f, g, h, i, j, k) SF_RM10(T, a, b, c, d, e, f, g, h, i, j), SF_REFLECT_ONE(T, k)
#define SF_RM12(T, a, b, c, d, e, f, g, h, i, j, k, l) SF_RM11(T, a, b, c, d, e, f, g, h, i, j, k), SF_REFLECT_ONE(T, l)
#define SF_RM13(T, a, b, c, d, e, f, g, h, i, j, k, l, m) SF_RM12(T, a, b, c, d, e, f, g, h, i, j, k, l), SF_REFLECT_ONE(T, m)
#define SF_RM14(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n) SF_RM13(T, a, b, c, d, e, f, g, h, i, j, k, l, m), SF_REFLECT_ONE(T, n)
#define SF_RM15(T, ...) /* extend if needed */
#define SF_RM16(T, ...) /* extend if needed */

    // ---------------------------------------------------------------------------
    // ReflectionData specialisation (generated via the friend declaration)
    // ---------------------------------------------------------------------------

    template <typename T>
    struct ReflectionData
    {
        static constexpr auto members() { return T::_reflect(); }
    };

    // ---------------------------------------------------------------------------
    // DrawMember  dispatches on member type via if constexpr
    // ---------------------------------------------------------------------------

    template <typename T, typename M>
    void DrawMember(T &obj, const MemberInfo<T, M> &info)
    {
        M &value = obj.*(info.ptr);

        if constexpr (std::is_same_v<M, float>)
        {
            ImGui::DragFloat(info.name, &value, 0.01f);
        }
        else if constexpr (std::is_same_v<M, int>)
        {
            ImGui::DragInt(info.name, &value);
        }
        else if constexpr (std::is_same_v<M, bool>)
        {
            ImGui::Checkbox(info.name, &value);
        }
        else if constexpr (std::is_same_v<M, std::string>)
        {
            char buf[256];
            std::snprintf(buf, sizeof(buf), "%s", value.c_str());
            if (ImGui::InputText(info.name, buf, sizeof(buf)))
                value = buf;
        }
        else if constexpr (is_vec2<M>::value)
        {
            ImGui::DragFloat2(info.name, glm::value_ptr(value), 0.01f);
        }
        else if constexpr (is_vec3<M>::value)
        {
            ImGui::DragFloat3(info.name, glm::value_ptr(value), 0.01f);
        }
        else if constexpr (is_vec4<M>::value)
        {
            ImGui::DragFloat4(info.name, glm::value_ptr(value), 0.01f);
        }
        else if constexpr (is_quat<M>::value)
        {
            // Display as Euler angles (degrees) for usability; store as quat
            glm::vec3 euler = glm::degrees(glm::eulerAngles(value));
            if (ImGui::DragFloat3(info.name, glm::value_ptr(euler), 0.1f))
                value = glm::quat(glm::radians(euler));
        }
        else if constexpr (is_mat4<M>::value)
        {
            // Decompose into TRS for human-readable editing
            ImGui::PushID(info.name);
            ImGui::Text("%s", info.name);
            ImGui::Indent();

            glm::vec3 translation = value[3];
            glm::vec3 scale(glm::length(value[0]), glm::length(value[1]), glm::length(value[2]));
            glm::mat3 rotMat(value[0] / scale.x, value[1] / scale.y, value[2] / scale.z);
            glm::vec3 euler = glm::degrees(glm::eulerAngles(glm::quat_cast(rotMat)));

            bool changed = false;
            changed |= ImGui::DragFloat3("Translation", glm::value_ptr(translation), 0.01f);
            changed |= ImGui::DragFloat3("Rotation", glm::value_ptr(euler), 0.1f);
            changed |= ImGui::DragFloat3("Scale", glm::value_ptr(scale), 0.01f);

            if (changed)
            {
                value = glm::translate(glm::mat4(1.f), translation) * glm::mat4_cast(glm::quat(glm::radians(euler))) * glm::scale(glm::mat4(1.f), scale);
            }

            ImGui::Unindent();
            ImGui::PopID();
        }
        else
        {
            // Unhandled type  show a greyed-out label so nothing silently disappears
            ImGui::BeginDisabled();
            ImGui::LabelText(info.name, "<unsupported type>");
            ImGui::EndDisabled();
        }
    }

    // ---------------------------------------------------------------------------
    // DrawReflected  the single public entry point
    // ---------------------------------------------------------------------------

    template <typename T>
    void DrawReflected(T &obj, const char *label = nullptr)
    {
        const bool hasHeader = label && label[0];

        if (hasHeader && !ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen))
            return;

        constexpr auto members = ReflectionData<T>::members();

        std::apply([&obj](const auto &...infos)
                   { (DrawMember(obj, infos), ...); }, members);
    }

} // namespace SF::Engine