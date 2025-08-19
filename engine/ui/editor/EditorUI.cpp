#include "EditorUI.hpp"
#include <imgui.h>

namespace dancore::ui {

// Вспомогательно: верхняя DockSpace-панель
static void BeginDockspace()
{
    static ImGuiDockNodeFlags dock_flags = ImGuiDockNodeFlags_PassthruCentralNode;
    ImGuiWindowFlags win_flags = ImGuiWindowFlags_MenuBar
      | ImGuiWindowFlags_NoDocking
      | ImGuiWindowFlags_NoTitleBar
      | ImGuiWindowFlags_NoCollapse
      | ImGuiWindowFlags_NoResize
      | ImGuiWindowFlags_NoMove
      | ImGuiWindowFlags_NoBringToFrontOnFocus
      | ImGuiWindowFlags_NoNavFocus;

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##DockSpaceHost", nullptr, win_flags);
    ImGui::PopStyleVar(2);

    ImGuiID dock_id = ImGui::GetID("DancoreDockspace");
    ImGui::DockSpace(dock_id, ImVec2(0,0), dock_flags);
}

static void EndDockspace()
{
    ImGui::End(); // ##DockSpaceHost
}

// Верхнее меню + тулбар (Undo/Redo, Play/Pause/Stop, EditMode, Поиск, Лого)
static void DrawMainMenuAndToolbar(EditorState& state)
{
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            ImGui::MenuItem("New Project...");
            ImGui::MenuItem("Open Project...");
            ImGui::Separator();
            ImGui::MenuItem("New Scene");
            ImGui::MenuItem("Save Scene", "Ctrl+S");
            ImGui::MenuItem("Save All", "Ctrl+Alt+S");
            ImGui::Separator();
            if (ImGui::BeginMenu("Import")) {
                ImGui::MenuItem("Model (.gltf/.glb)");
                ImGui::MenuItem("Texture (.png/.ktx2)");
                ImGui::MenuItem("Audio (.ogg/.opus)");
                ImGui::MenuItem("Shader (HLSL)");
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Export")) {
                ImGui::MenuItem("Package (.pak)...");
                ImGui::MenuItem("Scene as Template...");
                ImGui::EndMenu();
            }
            ImGui::Separator();
            ImGui::MenuItem("Show in Finder/Explorer");
            ImGui::Separator();
            ImGui::MenuItem("Quit", "Ctrl+Q");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit"))
        {
            ImGui::MenuItem("Undo", "Ctrl+Z");
            ImGui::MenuItem("Redo", "Ctrl+Y");
            ImGui::Separator();
            ImGui::MenuItem("Duplicate", "Ctrl+D");
            ImGui::MenuItem("Delete", "Del");
            ImGui::Separator();
            ImGui::MenuItem("Preferences...");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View"))
        {
            ImGui::MenuItem("Reset Layout to Default");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Project"))
        {
            ImGui::MenuItem("Project Settings...");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Build"))
        {
            ImGui::MenuItem("Build Project");
            ImGui::MenuItem("Rebuild Project");
            ImGui::MenuItem("Clean Project");
            ImGui::Separator();
            ImGui::MenuItem("Build Settings...");
            ImGui::MenuItem("Build & Run");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Window"))
        {
            ImGui::MenuItem("Console", nullptr, &state.show_console);
            ImGui::MenuItem("File Explorer", nullptr, &state.show_file_explorer);
            ImGui::MenuItem("Inspector", nullptr, &state.show_inspector);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help"))
        {
            ImGui::MenuItem("Docs");
            ImGui::MenuItem("About Dancore");
            ImGui::EndMenu();
        }

        // --- Тулбар справа: Undo/Redo, Play/Pause/Stop, Edit Mode, Search, Лого ---
        ImGui::Separator();
        if (ImGui::SmallButton("↶")) { /* Undo */ }
        ImGui::SameLine();
        if (ImGui::SmallButton("↷")) { /* Redo */ }
        ImGui::SameLine();
        ImGui::Separator();

        ImGui::SameLine();
        if (!state.play_mode) {
            if (ImGui::SmallButton("▶ Play")) state.play_mode = true;
        } else {
            if (ImGui::SmallButton("⏹ Stop")) state.play_mode = false;
            ImGui::SameLine();
            if (ImGui::SmallButton("⏸ Pause")) { /* pause */ }
        }

        ImGui::SameLine();
        const char* modes[] = {"Scene","UI","Animation"};
        ImGui::SetNextItemWidth(120);
        ImGui::Combo("##EditMode", &state.edit_mode, modes, IM_ARRAYSIZE(modes));

        ImGui::SameLine();
        static char search[128] = {};
        ImGui::SetNextItemWidth(200);
        ImGui::InputTextWithHint("##Search", "Search assets/objects...", search, IM_ARRAYSIZE(search));

        ImGui::SameLine();
        if (ImGui::BeginMenu("Dancore ▾")) {
            if (ImGui::MenuItem("Settings...")) {}
            if (ImGui::MenuItem("Reset Layout to Default")) {}
            ImGui::Separator();
            if (ImGui::MenuItem("About Dancore")) {}
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }
}

// Левая колонка: инструменты (Select/Move/Rotate/Scale/…)
static void DrawToolbox()
{
    ImGui::Begin("Toolbox", nullptr, ImGuiWindowFlags_NoCollapse);
    ImGui::TextUnformatted("Tools");
    if (ImGui::Button("Select")) {}
    if (ImGui::Button("Move  ⭢")) {}     // стрелки во все стороны — иконки добавим позже
    if (ImGui::Button("Rotate ⟳")) {}
    if (ImGui::Button("Scale  ⤢")) {}
    ImGui::Separator();
    if (ImGui::Button("Camera")) {}
    if (ImGui::Button("Voxel")) {}
    if (ImGui::Button("Paint")) {}
    ImGui::End();
}

// Центр: Viewport (пока просто окно-плейсхолдер)
static void DrawViewport()
{
    ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoCollapse);
    ImGui::TextDisabled("Viewport: здесь будет отрисовка Vulkan.");
    ImGui::Dummy(ImVec2(0, 400)); // заглушка высоты
    ImGui::End();
}

// Нижний проводник (File Explorer)
static void DrawFileExplorer(bool& open)
{
    if (!open) return;
    ImGui::Begin("File Explorer", &open, ImGuiWindowFlags_NoCollapse);
    ImGui::TextUnformatted("Content/");
    ImGui::BulletText("Scenes/");
    ImGui::BulletText("Scripts/  (Lua, Python)");
    ImGui::BulletText("UI/      (HTML/CSS/JS)");
    ImGui::BulletText("Assets/  (Models, Textures, Audio)");
    ImGui::BulletText("Shaders/");
    if (ImGui::Button("New Scene")) {}
    ImGui::SameLine();
    if (ImGui::Button("New Script (Lua)")) {}
    ImGui::SameLine();
    if (ImGui::Button("Import...")) {}
    ImGui::End();
}

// Правая нижняя: Inspector
static void DrawInspector(bool& open)
{
    if (!open) return;
    ImGui::Begin("Inspector", &open, ImGuiWindowFlags_NoCollapse);
    ImGui::TextUnformatted("Transform");
    ImGui::Separator();
    static float pos[3] = {0,0,0}, rot[3] = {0,0,0}, scl[3] = {1,1,1};
    ImGui::DragFloat3("Position", pos, 0.1f);
    ImGui::DragFloat3("Rotation", rot, 0.5f);
    ImGui::DragFloat3("Scale", scl, 0.01f, 0.01f, 100.0f);

    ImGui::Separator();
    ImGui::TextUnformatted("Physics");
    static int mode = 0; // 0 Rigid, 1 Voxel
    ImGui::RadioButton("Rigid", &mode, 0); ImGui::SameLine();
    ImGui::RadioButton("Voxel", &mode, 1);

    ImGui::Separator();
    ImGui::TextUnformatted("Scripts");
    if (ImGui::Button("Add Component...")) {}
    ImGui::End();
}

// Консоль
static void DrawConsole(bool& open)
{
    if (!open) return;
    ImGui::Begin("Console", &open, ImGuiWindowFlags_NoCollapse);
    ImGui::TextDisabled("[Info] Editor started.");
    ImGui::TextDisabled("[Warn] Example warning.");
    ImGui::TextColored(ImVec4(1,0.3f,0.3f,1), "[Error] Example error.");
    if (ImGui::Button("Clear")) {}
    ImGui::End();
} 

void DrawEditorUI(EditorState& state)
{
    BeginDockspace();
    DrawMainMenuAndToolbar(state);

    // Окна (их расположение и докинг пользователь сохранит/сбросит)
    DrawToolbox();               // слева
    DrawViewport();              // центр
    DrawFileExplorer(state.show_file_explorer); // низ
    DrawInspector(state.show_inspector);        // право-низ
    DrawConsole(state.show_console);            // низ

    EndDockspace();
}

} // namespace dancore::ui