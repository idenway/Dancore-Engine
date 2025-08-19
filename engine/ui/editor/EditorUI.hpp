#pragma once

// Чистый интерфейс отрисовки редактора (только ImGui-вызовы).
// Вызывается после Begin/End кадра ImGui в твоём бэкенде (Vulkan/и т.д.)

namespace dancore::ui {

struct EditorState {
    bool show_console = true;
    bool show_file_explorer = true;
    bool show_inspector = true;
    bool play_mode = false;
    int  edit_mode = 0; // 0=Scene,1=UI,2=Animation
};

void DrawEditorUI(EditorState& state);

} // namespace dancore::ui