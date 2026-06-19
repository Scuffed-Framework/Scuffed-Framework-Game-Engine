#define Window(name)    \
    ImGui::Begin(name); \
    ImGui::End()

#define Header(label) ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen) // put {} and ur members inside
#define MemberChange(name, value) ImGui::Text("%s: %s", name, value)                 // for string members
#define MemberChangeable(name, value) if (ImGui::Text("%s: %s", name, value))        // for string members
#define TreeNode(label) if (ImGui::TreeNode(label))
#define TreeNodeFlags(label, flags) if (ImGui::TreeNodeEx(label, flags))
#define TreeNodePop() ImGui::TreePop()

#define Text(...) ImGui::Text(__VA_ARGS__)
#define SameLine(...) ImGui::SameLine(__VA_ARGS__)

#define TextUnformatted(...) ImGui::TextUnformatted(__VA_ARGS__)
#define Spacing() ImGui::Spacing()
#define Separator() ImGui::Separator()

#define TextColored(col, ...) ImGui::TextColored(col, __VA_ARGS__)
#define ColorEdit4(label, col, flags) ImGui::ColorEdit4(label, col, flags)
#define ColorEdit3(label, col, flags) ImGui::ColorEdit3(label, col, flags)
#define SliderFloat(label, ref, vmin, vmax, fmt) ImGui::SliderFloat(label, ref, vmin, vmax, fmt)
#define DragFloat(label, ref, speed, vmin, vmax, fmt) ImGui::DragFloat(label, ref, speed, vmin, vmax, fmt)
#define SliderInt(label, ref, vmin, vmax) ImGui::SliderInt(label, ref, vmin, vmax)
#define Checkbox(label, ref) ImGui::Checkbox(label, ref)