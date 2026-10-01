// https://github.com/CedricGuillemet/ImGuizmo
// v1.92.5 WIP
//
// The MIT License(MIT)
//
// Copyright(c) 2016-2026 Cedric Guillemet and contributors
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
#include "ImGradient.h"
#include "../ocornut/imgui.h"
#include "../ocornut/imgui_internal.h"

namespace ImGradient
{
#ifndef IMGUI_DEFINE_MATH_OPERATORS
    static inline ::SF::Engine::Vec2 operator*(const ::SF::Engine::Vec2 &lhs, const float rhs)
    {
        return ::SF::Engine::Vec2(lhs.x * rhs, lhs.y * rhs);
    }
    static inline ::SF::Engine::Vec2 operator/(const ::SF::Engine::Vec2 &lhs, const float rhs)
    {
        return ::SF::Engine::Vec2(lhs.x / rhs, lhs.y / rhs);
    }
    static inline ::SF::Engine::Vec2 operator+(const ::SF::Engine::Vec2 &lhs, const ::SF::Engine::Vec2 &rhs)
    {
        return ::SF::Engine::Vec2(lhs.x + rhs.x, lhs.y + rhs.y);
    }
    static inline ::SF::Engine::Vec2 operator-(const ::SF::Engine::Vec2 &lhs, const ::SF::Engine::Vec2 &rhs)
    {
        return ::SF::Engine::Vec2(lhs.x - rhs.x, lhs.y - rhs.y);
    }
    static inline ::SF::Engine::Vec2 operator*(const ::SF::Engine::Vec2 &lhs, const ::SF::Engine::Vec2 &rhs)
    {
        return ::SF::Engine::Vec2(lhs.x * rhs.x, lhs.y * rhs.y);
    }
    static inline ::SF::Engine::Vec2 operator/(const ::SF::Engine::Vec2 &lhs, const ::SF::Engine::Vec2 &rhs)
    {
        return ::SF::Engine::Vec2(lhs.x / rhs.x, lhs.y / rhs.y);
    }
#endif

    static int DrawPoint(ImDrawList *draw_list, ::SF::Engine::Vec4 color, const ::SF::Engine::Vec2 size, bool editing,
                         ::SF::Engine::Vec2 pos)
    {
        ImGuiIO &io = ImGui::GetIO();

        ::SF::Engine::Vec2 p1 =
                ImLerp(pos, ::SF::Engine::Vec2(pos + ::SF::Engine::Vec2(size.x - size.y, 0.f)), color.w) +
                ::SF::Engine::Vec2(3, 3);
        ::SF::Engine::Vec2 p2 =
                ImLerp(pos + ::SF::Engine::Vec2(size.y, size.y), ::SF::Engine::Vec2(pos + size), color.w) -
                ::SF::Engine::Vec2(3, 3);
        ImRect rc(p1, p2);

        color.w = 1.f;
        draw_list->AddRectFilled(p1, p2, ::SF::Engine::Color(color).ToInt());

        if (editing)
            draw_list->AddRect(p1, p2, 0xFFFFFFFF, 2.f, 2.5f);
        else
            draw_list->AddRect(p1, p2, 0x80FFFFFF, 2.f, 1.25f);
        if (rc.Contains(io.MousePos))
        {
            if (io.MouseClicked[0])
                return 2;
            return 1;
        }
        return 0;
    }

    bool Edit(Delegate &delegate, const ::SF::Engine::Vec2 &size, int &selection)
    {
        bool ret    = false;
        ImGuiIO &io = ImGui::GetIO();
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ::SF::Engine::Vec2(0, 0));
        ImGui::BeginChild(137, size, ImGuiChildFlags_FrameStyle);

        ImDrawList *draw_list           = ImGui::GetWindowDrawList();
        const ::SF::Engine::Vec2 offset = ImGui::GetCursorScreenPos();

        const ::SF::Engine::Vec4 *pts = delegate.GetPoints();
        static int currentSelection   = -1;
        static int movingPt           = -1;
        if (currentSelection >= int(delegate.GetPointCount()))
            currentSelection = -1;
        if (movingPt != -1)
        {
            ::SF::Engine::Vec4 current = pts[movingPt];
            current.w += io.MouseDelta.x / size.x;
            current.w = ImClamp(current.w, 0.f, 1.f);
            delegate.EditPoint(movingPt, current);
            ret = true;
            if (!io.MouseDown[0])
                movingPt = -1;
        }
        for (size_t i = 0; i < delegate.GetPointCount(); i++)
        {
            int ptSel = DrawPoint(draw_list, pts[i], size, i == currentSelection, offset);
            if (ptSel == 2)
            {
                currentSelection = int(i);
                ret              = true;
            }
            if (ptSel == 1 && io.MouseDown[0] && movingPt == -1)
            {
                movingPt = int(i);
            }
        }
        ImRect rc(offset, offset + size);
        if (rc.Contains(io.MousePos) && io.MouseDoubleClicked[0])
        {
            float t = (io.MousePos.x - offset.x) / size.x;
            delegate.AddPoint(delegate.GetPoint(t));
            ret = true;
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();

        selection = currentSelection;
        return ret;
    }
} // namespace ImGradient
