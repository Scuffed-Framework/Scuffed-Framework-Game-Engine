#include "BarPanels.hpp"
#include <Assets/Audio/AudioClip.hpp>
#include <Assets/Audio/Waves.hpp>
#include <Controllers/CameraController.hpp>
#include <Engine/Engine.hpp>
#include <Engine/Project/Project.hpp>
#include <Gui/ImGui/CommonGuiFunctions.hpp>
#include <LowLevel/FileSystem/File.hpp>
#include <Scene/SceneManager.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include "AssetsWindow.hpp"
#include "Panels.hpp"

namespace SF::Engine
{
    namespace
    {
        void FormatAltitude(char *out, size_t n, float altM)
        {
            // Y=0 in game space = sea level. Metres below 10 km, km above.
            if (std::abs(altM) < 10000.0f)
                std::snprintf(out, n, "Alt: %.0f m", altM);
            else
                std::snprintf(out, n, "Alt: %.1f km", altM / 1000.0f);
        }

        constexpr const char *kKeyOpenScene = "OpenSceneDlg";
        constexpr const char *kKeySaveScene = "SaveSceneDlg";

        std::filesystem::path gScenePath;
        Scene *gScenePathOwner = nullptr;
        std::filesystem::path gLastSceneDir;

        std::filesystem::path &CurrentScenePath()
        {
            Scene *s = SceneManager::Get()->GetScene();
            if (s != gScenePathOwner)
            {
                gScenePathOwner = s;
                gScenePath.clear();
            }
            return gScenePath;
        }

        std::filesystem::path StartDir()
        {
            if (!gLastSceneDir.empty())
                return gLastSceneDir;
            if (ProjectManager::Get()->IsAProjectLoaded())
                return ProjectManager::Get()->GetProjectAssetPath();
            return ".";
        }

        std::string EnsureSceneExt(std::string p)
        {
            if (p.size() < SceneFileExtension.size() ||
                p.compare(p.size() - SceneFileExtension.size(), SceneFileExtension.size(), SceneFileExtension) != 0)
                p += SceneFileExtension;
            return p;
        }

        void OpenSceneDialog()
        {
            IGFD::FileDialogConfig cfg;
            cfg.path = StartDir().string();
            ImGuiFileDialog::Instance()->OpenDialog(kKeyOpenScene, "Open Scene",
                                                    std::string(SceneFileExtension).c_str(), cfg);
        }

        void EDITOR_SaveSceneAsDialog()
        {
            Scene *scene = SceneManager::Get()->GetScene();
            if (!scene)
                return;

            IGFD::FileDialogConfig cfg;
            const auto &cur = CurrentScenePath();
            cfg.path        = (cur.empty() ? StartDir() : cur.parent_path()).string();
            cfg.fileName    = cur.empty() ? SanitizeFileName(scene->GetName()) + std::string(SceneFileExtension)
                                          : cur.filename().string();
            cfg.flags       = ImGuiFileDialogFlags_ConfirmOverwrite;
            ImGuiFileDialog::Instance()->OpenDialog(kKeySaveScene, "Save Scene As",
                                                    std::string(SceneFileExtension).c_str(), cfg);
        }

        void EDITOR_SaveScene()
        {
            if (!SceneManager::Get()->GetScene())
                return;

            if (!CurrentScenePathNotEmpty())
            {
                EDITOR_SaveSceneAsDialog(); // never saved: ask where
                return;
            }
            if (!SaveScene())
                Log::Error("Failed to save scene: {}", CurrentScenePath().string());
        }

        void DrawFileDialogs()
        {
            auto *dlg = ImGuiFileDialog::Instance();
            constexpr Vec2 minSize(600, 400);

            if (dlg->Display(kKeyOpenScene, ImGuiWindowFlags_NoCollapse, minSize))
            {
                if (dlg->IsOk())
                {
                    const std::filesystem::path p = dlg->GetFilePathName();
                    if (OpenScene(p))
                        gLastSceneDir = p.parent_path();
                    else
                        Log::Error("Failed to open scene: {}", p.string());
                }
                dlg->Close();
            }

            // Save Scene As
            if (dlg->Display(kKeySaveScene, ImGuiWindowFlags_NoCollapse, minSize))
            {
                if (dlg->IsOk())
                {
                    const std::filesystem::path p = EnsureSceneExt(dlg->GetFilePathName());
                    if (SaveSceneAs(p))
                        gLastSceneDir = p.parent_path();
                    else
                        Log::Error("Failed to save scene: {}", p.string());
                }
                dlg->Close();
            }
        }
    } // namespace

    void BarPanels::Draw()
    {
        audioTests->UpdateMetronome(); // runs even when the window is closed so it keeps ticking
        DrawMenuBar();
        DrawFileDialogs();
        DrawExecutingPasses();
        DrawAudioTools();
        DrawEngineStatusBar();
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S))
            EDITOR_SaveScene();
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S))
            EDITOR_SaveSceneAsDialog();
    }

    void BarPanels::TestAudio(uint32_t dat, float freq, float time)
    {
        buffer = SoundBuffer::CreateWave(dat, freq, time);
        clip   = std::make_shared<AudioClip>(buffer);
        clip->SetEnabled(true);
        clip->Play();
    }

    void BarPanels::DrawMenuBar()
    {
        if (!ImGui::BeginMainMenuBar())
            return;

        const bool projectLoaded = ProjectManager::Get()->IsAProjectLoaded();
        const bool hasScene      = SceneManager::Get()->GetScene() != nullptr;

        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New Scene", nullptr, false, projectLoaded))
            {
                if (auto *browser = AssetBrowser::Get())
                    browser->BeginNewScene();
            }
            if (ImGui::MenuItem("New Project"))
            {
                ProjectManager::Get()->OpenProjectManagerWindow();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Open Scene", nullptr, false, hasScene))
                OpenSceneDialog();
            if (ImGui::MenuItem("Open Project"))
            {
                ProjectManager::Get()->OpenProjectManagerWindow();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Save Scene", "Ctrl+S", false, hasScene))
                EDITOR_SaveScene();
            if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S", false, hasScene))
                EDITOR_SaveSceneAsDialog();
            if (ImGui::MenuItem("Save Project", nullptr, false, projectLoaded))
            {
                ProjectResult res = ProjectManager::Get()->SaveProject();
                if (res == NotFound || res == UnknownError)
                    Log::Error("Failed to save project!");
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit"))
                Engine::Get()->RequestClose();

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Windows"))
        {
            ImGui::MenuItem("Hierarchy", nullptr, &ShowHierarchy);
            ImGui::MenuItem("Inspector", nullptr, &ShowInspector);
            ImGui::MenuItem("Pipeline Controller", nullptr, &ShowExecutingPasses);
            ImGui::MenuItem("Audio Tests", nullptr, &audioTests->gShowAudioTools);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Camera"))
        {
            auto *activeCam = CameraController::Get().GetActive();
            auto *ec        = dynamic_cast<EditorCamera *>(activeCam);

            if (ec)
            {
                ImGui::SeparatorText("Controls");
                ImGui::DragFloat("Speed##cs", &ec->moveSpeed, 1.0f, 0.1f, 1.0e6f, "%.1f", ImGuiSliderFlags_Logarithmic);
                ImGui::SliderFloat("Sensitivity##cs", &ec->lookSensitivity, 0.01f, 1.0f);
                ImGui::SliderFloat("FOV##cs", &ec->fovDeg, 20.0f, 120.0f, "%.0f deg");

                ImGui::SeparatorText("Teleport");

                char alt[32];
                FormatAltitude(alt, sizeof(alt), activeCam->GetPosition().y);
                ImGui::TextDisabled("%s", alt);

                struct Preset
                {
                    const char *label;
                    float y;
                };
                static const Preset presets[] = {
                        {"Surface", 10.0f},      {"10 km", 10000.0f},           {"50 km", 50000.0f},
                        {"100 km", 100000.0f},   {"200 km (orbit)", 200000.0f}, {"500 km", 500000.0f},
                        {"1000 km", 1000000.0f},
                };

                constexpr int columns = 3;
                int i                 = 0;
                for (const Preset &p: presets)
                {
                    if (i % columns != 0)
                        ImGui::SameLine();
                    if (ImGui::Button(p.label, Vec2(100, 0)))
                        activeCam->SetPosition({0.0f, p.y, 0.0f});
                    ++i;
                }
            } else
            {
                ImGui::TextDisabled("No active camera");
            }
            ImGui::EndMenu();
        }

        // ---- Test ----
        if (ImGui::BeginMenu("Test"))
        {
            ImGui::MenuItem("Audio Tools", nullptr, &audioTests->gShowAudioTools);
            ImGui::EndMenu();
        }

        // ---- FPS, right-aligned ----
        char fpsBuf[48];
        const float fps = ImGui::GetIO().Framerate;
        std::snprintf(fpsBuf, sizeof(fpsBuf), "%.0f FPS  (%.2f ms)", fps, fps > 0.0f ? 1000.0f / fps : 0.0f);
        const float fpsW = ImGui::CalcTextSize(fpsBuf).x + 16.0f;
        ImGui::SetCursorPosX(ImGui::GetWindowSize().x - fpsW);
        ImGui::TextDisabled("%s", fpsBuf);

        ImGui::EndMainMenuBar();
    }

    void BarPanels::DrawAudioTools()
    {
        if (!audioTests->gShowAudioTools)
            return;

        ImGui::SetNextWindowSize({360, 0}, ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Audio Tools", &audioTests->gShowAudioTools))
        {
            ImGui::End();
            return;
        }

        if (ImGui::BeginTabBar("##audiotabs"))
        {
            if (ImGui::BeginTabItem("Tone Generator"))
            {
                ImGui::SliderFloat("Frequency##tone", &audioTests->gTone.freq, 20.0f, 4000.0f, "%.0f Hz",
                                   ImGuiSliderFlags_Logarithmic);
                ImGui::SliderFloat("Duration##tone", &audioTests->gTone.duration, 0.1f, 5.0f, "%.1f s");

                ImGui::Spacing();

                struct Wave
                {
                    const char *name;
                    uint32_t id;
                };
                const Wave waves[] = {
                        {.name = "Sine", .id = ExtraAudioWaves::Wave_Sine},
                        {.name = "Square", .id = ExtraAudioWaves::Wave_Square},
                        {.name = "Sawtooth", .id = ExtraAudioWaves::Wave_Sawtooth},
                        {.name = "Triangle", .id = ExtraAudioWaves::Wave_Triangle},
                        {.name = "White Noise", .id = ExtraAudioWaves::Wave_WhiteNoise},
                };

                const float btnW = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
                int i            = 0;
                for (const Wave &w: waves)
                {
                    if (i % 2 != 0)
                        ImGui::SameLine();
                    if (ImGui::Button(w.name, Vec2(btnW, 0)))
                        TestAudio(w.id, audioTests->gTone.freq, audioTests->gTone.duration);
                    ++i;
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Metronome"))
            {
                auto &m = audioTests->gMetro;

                // Start / stop
                const float fullW = ImGui::GetContentRegionAvail().x;
                if (m.running)
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, Vec4(0.70f, 0.20f, 0.20f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Vec4(0.80f, 0.28f, 0.28f, 1.0f));
                    if (ImGui::Button("Stop", Vec2(fullW, 32)))
                        audioTests->StopMetronome();
                    ImGui::PopStyleColor(2);
                } else
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, Vec4(0.20f, 0.60f, 0.30f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Vec4(0.26f, 0.70f, 0.38f, 1.0f));
                    if (ImGui::Button("Start", Vec2(fullW, 32)))
                        audioTests->StartMetronome();
                    ImGui::PopStyleColor(2);
                }

                ImGui::Spacing();

                // Beat indicators
                {
                    const int n        = std::max(1, m.beatsPerBar);
                    const float availW = ImGui::GetContentRegionAvail().x;
                    const float step   = std::min(34.0f, availW / static_cast<float>(n));
                    const float radius = std::min(12.0f, step * 0.38f);
                    const float height = radius * 2.0f + 8.0f;

                    const Vec2 origin = ImGui::GetCursorScreenPos();
                    ImDrawList *dl    = ImGui::GetWindowDrawList();

                    const float since = std::chrono::duration<float>(chrono::steady_clock::now() - m.lastBeat).count();
                    const float flash = std::max(0.0f, 1.0f - since / 0.25f);

                    // Centre the row
                    const float rowW = step * static_cast<float>(n);
                    const float x0   = origin.x + (availW - rowW) * 0.5f + step * 0.5f;
                    const float cy   = origin.y + height * 0.5f;

                    for (int i = 0; i < n; ++i)
                    {
                        const bool accent = m.accentFirst && i == 0;
                        const bool active = m.running && i == m.currentBeat;
                        const Vec2 c(x0 + step * static_cast<float>(i), cy);

                        const Vec4 baseCol = accent ? Vec4(1.0f, 0.65f, 0.15f, 1.0f) : Vec4(0.35f, 0.70f, 1.0f, 1.0f);

                        if (active)
                        {
                            Vec4 fill = baseCol;
                            fill.w    = 0.35f + 0.65f * flash;
                            dl->AddCircleFilled(c, radius + 2.0f * flash, ImGui::GetColorU32(fill));
                        }
                        Vec4 outline = baseCol;
                        outline.w    = active ? 1.0f : 0.5f;
                        dl->AddCircle(c, radius, ImGui::GetColorU32(outline), 0, 2.0f);
                    }
                    ImGui::Dummy(Vec2(availW, height));
                }

                ImGui::Spacing();

                // Tempo
                ImGui::SetNextItemWidth(-90.0f);
                ImGui::SliderFloat("##bpm", &m.bpm, 20.0f, 300.0f, "%.0f BPM");
                ImGui::SameLine();
                if (ImGui::Button("Tap", Vec2(-FLT_MIN, 0)))
                    audioTests->TapTempo();
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Tap repeatedly to set tempo");

                ImGui::SliderInt("Beats / bar", &m.beatsPerBar, 1, 12);
                ImGui::Checkbox("Accent first beat", &m.accentFirst);
                ImGui::SliderFloat("Click pitch", &m.pitch, 400.0f, 3000.0f, "%.0f Hz");

                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        ImGui::End();
    }

    void BarPanels::DrawEngineStatusBar()
    {
        auto &io   = ImGui::GetIO();
        float barH = ImGui::GetFrameHeightWithSpacing() + 4.0f;

        ImGui::SetNextWindowPos({0, io.DisplaySize.y - barH}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({io.DisplaySize.x, barH}, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.85f);

        // Style must be pushed *before* Begin to affect this window.
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, Vec2(10, 3));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, Vec4(0.0f, 0.0f, 0.0f, 1.0f));

        ImGui::Begin("##statusbar", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoBringToFrontOnFocus);

        // Left: project
        if (ProjectManager::Get()->IsAProjectLoaded())
            ImGui::Text("Project: %s", ProjectManager::Get()->GetCurrentProject()->name.c_str());
        else
            ImGui::TextDisabled("No project loaded");

        // Right: metronome (if running) + altitude
        char right[96] = {};
        if (auto *cam = CameraController::Get().GetActive())
        {
            char alt[32];
            FormatAltitude(alt, sizeof(alt), cam->GetPosition().y);
            if (audioTests->gMetro.running)
                std::snprintf(right, sizeof(right), "%.0f BPM  |  %s", audioTests->gMetro.bpm, alt);
            else
                std::snprintf(right, sizeof(right), "%s", alt);
        } else if (audioTests->gMetro.running)
        {
            std::snprintf(right, sizeof(right), "%.0f BPM", audioTests->gMetro.bpm);
        }
        if (right[0])
            RightAlignedText(right);

        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
    }

    void BarPanels::DrawExecutingPasses()
    {
        if (!ShowExecutingPasses)
            return;

        ImGui::Begin("Pipeline Controller", &ShowExecutingPasses);

        // todo: fix
        // for (SFERenderpass *pass : SceneManager::Get()->GetScene()->GetSFERenderpass())
        // {
        //     ImGui::PushID(pass);
        //     bool enabled = pass->IsEnabled();
        //     if (ImGui::Checkbox("Enabled", &enabled))
        //         pass->SetEnabled(enabled);
        //     ImGui::Separator();
        //     ImGui::PopID();
        // }
        ImGui::TextDisabled("Pass list not wired up yet.");

        ImGui::End();
    }
} // namespace SF::Engine
