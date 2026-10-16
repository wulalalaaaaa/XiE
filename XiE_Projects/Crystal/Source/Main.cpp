#include "Platform/GLFW/GlfwApplicationHost.h"
#include "Platform/GLFW/GlfwOpenGLSurface.h"
#include "Platform/GLFW/GlfwWindowSystem.h"
#include "Application/FrameScheduler.h"
#include "Input/InputEventQueue.h"
#include "UI/CrystalRenderer.h"
#include "UI/CrystalRenderGeometry.h"
#include "UI/CrystalProfiler.h"
#include "UI/PropertyEditor.h"
#include "UI/WorkspaceEditors.h"
#include "Workspace/CrystalWorkspace.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/gl.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <chrono>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <variant>

namespace {

Crystal::CrystalProfiler* g_Profiler = nullptr;

std::string InspectorDraftText(const Engine::Json::Value& value);
const Crystal::CrystalPropertyDefinition* CustomDefinitionFor(
    const Crystal::CrystalWorkspace& workspace, const Crystal::CrystalEntity& entity,
    const std::string& propertyName);

struct Options {
    std::string server = "http://127.0.0.1:8787";
    std::string project = "P_HighSchool";
    bool performanceFixture = false;
};

enum class InspectorField {
    Title,
    Description,
    Status,
    Priority,
    Owner,
    Branch,
    AcceptanceCriteria,
    TargetDate,
    AssetPath,
};

Options ParseOptions(int argc, char** argv) {
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--server" && index + 1 < argc) options.server = argv[++index];
        else if (argument == "--project" && index + 1 < argc) options.project = argv[++index];
        else if (argument == "--fixture") options.performanceFixture = true;
    }
    return options;
}

bool IsLayerVisible(const Crystal::ProjectWorkspaceModel& model,
    const std::string& layerId, const std::string& spaceId) {
    for (const auto& layer : model.userLayers) {
        if ((layer.id == layerId || layer.name == layerId) &&
            (layer.spaceId.empty() || layer.spaceId == spaceId))
            return layer.visible;
    }
    return true;
}

Crystal::EntityView* HitView(Crystal::CrystalWorkspace& workspace, Engine::Vec2F screen) {
    Crystal::CrystalProfiler::Scope profile(g_Profiler, "hittest");
    auto& controller = workspace.Projects().CurrentSpace();
    const Engine::Vec2F world = controller.Canvas().ScreenToWorld(screen);
    auto& model = workspace.Projects().Workspace();
    for (auto view = model.views.rbegin(); view != model.views.rend(); ++view) {
        if (view->spaceId != controller.Current().id || !view->visible ||
            !IsLayerVisible(model, view->layerId, controller.Current().id)) continue;
        if (!view->boardId.empty()) {
            const auto board = std::find_if(model.boards.begin(), model.boards.end(),
                [&](const Crystal::BoardModel& item) { return item.id == view->boardId; });
            if (board != model.boards.end() && board->collapsed) continue;
        }
        if (Crystal::WorldPointInRect(world, view->position, view->size)) return &*view;
    }
    return nullptr;
}

Crystal::CrystalEntity* EntityForView(Crystal::CrystalWorkspace& workspace, const std::string& id) {
    auto& entities = workspace.Projects().Workspace().entities;
    const auto found = std::find_if(entities.begin(), entities.end(),
        [&](const Crystal::CrystalEntity& entity) { return entity.id == id; });
    return found == entities.end() ? nullptr : &*found;
}

std::optional<std::string> HitStroke(Crystal::CrystalWorkspace& workspace, Engine::Vec2F screen) {
    Crystal::CrystalProfiler::Scope profile(g_Profiler, "hittest");
    const auto& canvas = workspace.Projects().CurrentSpace().Canvas();
    const Engine::Vec2F point = canvas.ScreenToWorld(screen);
    for (auto stroke = workspace.Ink().Strokes().rbegin();
         stroke != workspace.Ink().Strokes().rend(); ++stroke) {
        for (const Engine::Vec2F sample : stroke->points) {
            const float dx = sample.x - point.x;
            const float dy = sample.y - point.y;
            if (dx * dx + dy * dy < 400.0f) return stroke->id;
        }
    }
    return std::nullopt;
}

Crystal::BoardModel* HitBoard(Crystal::CrystalWorkspace& workspace, Engine::Vec2F screen) {
    Crystal::CrystalProfiler::Scope profile(g_Profiler, "hittest");
    const auto& canvas = workspace.Projects().CurrentSpace().Canvas();
    const Engine::Vec2F point = canvas.ScreenToWorld(screen);
    const std::string spaceId = workspace.Projects().CurrentSpace().Current().id;
    const auto& model = workspace.Projects().Workspace();
    auto& boards = workspace.Projects().Workspace().boards;
    for (auto board = boards.rbegin(); board != boards.rend(); ++board) {
        if (board->spaceId != spaceId || !IsLayerVisible(model, board->layerId, spaceId)) continue;
        if (Crystal::WorldPointInRect(point, board->position,
                {board->size.x, board->collapsed ? 42.0f : board->size.y})) return &*board;
    }
    return nullptr;
}

Crystal::DashboardWidgetModel* HitWidget(
    Crystal::CrystalWorkspace& workspace, Engine::Vec2F screen) {
    Crystal::CrystalProfiler::Scope profile(g_Profiler, "hittest");
    const auto& canvas = workspace.Projects().CurrentSpace().Canvas();
    const Engine::Vec2F point = canvas.ScreenToWorld(screen);
    const std::string spaceId = workspace.Projects().CurrentSpace().Current().id;
    const auto& model = workspace.Projects().Workspace();
    auto& widgets = workspace.Projects().Workspace().dashboardWidgets;
    for (auto widget = widgets.rbegin(); widget != widgets.rend(); ++widget) {
        if (widget->spaceId != spaceId || !widget->visible ||
            !IsLayerVisible(model, widget->layerId, spaceId)) continue;
        if (Crystal::WorldPointInRect(point, widget->position, widget->size))
            return &*widget;
    }
    return nullptr;
}

bool HasModifier(Engine::InputModifiers modifiers, Engine::InputModifiers value) {
    return (static_cast<std::uint8_t>(modifiers) & static_cast<std::uint8_t>(value)) != 0;
}

void ApplyBoxSelection(
    Crystal::CrystalWorkspace& workspace, Engine::Vec2F screenStart, Engine::Vec2F screenEnd,
    bool additive) {
    Crystal::CrystalProfiler::Scope profile(g_Profiler, "box_select");
    const auto& canvas = workspace.Projects().CurrentSpace().Canvas();
    const Engine::Vec2F a = canvas.ScreenToWorld(screenStart);
    const Engine::Vec2F b = canvas.ScreenToWorld(screenEnd);
    const float left = std::min(a.x, b.x);
    const float right = std::max(a.x, b.x);
    const float top = std::min(a.y, b.y);
    const float bottom = std::max(a.y, b.y);
    if (!additive) workspace.Selection().Clear();
    const std::string spaceId = workspace.Projects().CurrentSpace().Current().id;
    auto overlaps = [&](Engine::Vec2F position, Engine::Vec2F size) {
        return position.x <= right && position.x + size.x >= left &&
            position.y <= bottom && position.y + size.y >= top;
    };
    for (const auto& board : workspace.Projects().Workspace().boards)
        if (board.spaceId == spaceId && overlaps(board.position,
            {board.size.x, board.collapsed ? 42.0f : board.size.y}))
            workspace.Selection().SelectBoard(board.id, true);
    for (const auto& view : workspace.Projects().Workspace().views) {
        if (view.spaceId != spaceId || !view.visible || !overlaps(view.position, view.size)) continue;
        workspace.Selection().Select(view.entityId, true);
    }
    for (const auto& widget : workspace.Projects().Workspace().dashboardWidgets)
        if (widget.spaceId == spaceId && widget.visible && overlaps(widget.position, widget.size))
            workspace.Selection().SelectWidget(widget.id, true);
    std::unordered_set<std::string> groups;
    for (const auto& stroke : workspace.Ink().Strokes()) {
        const bool hit = std::any_of(stroke.points.begin(), stroke.points.end(),
            [&](Engine::Vec2F point) {
                return point.x >= left && point.x <= right && point.y >= top && point.y <= bottom;
            });
        if (!hit) continue;
        if (!stroke.groupId.empty()) groups.insert(stroke.groupId);
        else workspace.Selection().SelectInk(stroke.id, true);
    }
    for (const std::string& group : groups) workspace.Selection().SelectInkGroup(group, true);
}

void SelectContextTarget(Crystal::CrystalWorkspace& workspace,
    const Crystal::ContextMenuState& menu) {
    workspace.SelectRelationship({});
    if (menu.targetType == "View") workspace.Selection().Select(menu.targetId);
    else if (menu.targetType == "Board") workspace.Selection().SelectBoard(menu.targetId);
    else if (menu.targetType == "Widget") workspace.Selection().SelectWidget(menu.targetId);
    else if (menu.targetType == "Layer") workspace.SetActiveLayerId(menu.targetId);
    else if (menu.targetType == "Ink") {
        const auto found = std::find_if(workspace.Ink().Strokes().begin(),
            workspace.Ink().Strokes().end(), [&](const Engine::UI2D::InkStroke& stroke) {
                return stroke.id == menu.targetId;
            });
        if (found != workspace.Ink().Strokes().end() && !found->groupId.empty())
            workspace.Selection().SelectInkGroup(found->groupId);
        else workspace.Selection().SelectInk(menu.targetId);
    }
}

bool HandleContextMenuClick(Crystal::CrystalWorkspace& workspace,
    Engine::Vec2F screen, float width, float height) {
    if (!workspace.ContextMenu().open) return false;
    const Crystal::ContextMenuState menu = workspace.ContextMenu();
    const std::size_t optionCount = menu.targetType == "Board" ? 6
        : menu.targetType == "Layer" ? 4
        : menu.targetType == "Ink" ? 5
        : menu.targetType == "Widget" ? 5
        : menu.targetType == "View" ? 7 : 3;
    constexpr float menuWidth = 178.0f;
    const float menuHeight = 18.0f + static_cast<float>(optionCount) * 25.0f;
    const float menuX = std::min(menu.screenPosition.x, width - menuWidth - 8.0f);
    const float menuY = std::min(menu.screenPosition.y, height - menuHeight - 8.0f);
    const bool inside = screen.x >= menuX && screen.x <= menuX + menuWidth &&
        screen.y >= menuY && screen.y <= menuY + menuHeight;
    if (inside) {
        const int index = static_cast<int>((screen.y - menuY - 3.0f) / 25.0f);
        if (index >= 0 && index < static_cast<int>(optionCount)) {
            SelectContextTarget(workspace, menu);
            if (menu.targetType == "Board") {
                if (index == 0) {
                    for (const auto& board : workspace.Boards())
                        if (board.id == menu.targetId)
                            workspace.BeginInlineEdit("Board", board.id, board.title);
                } else if (index == 1) {
                    for (const auto& board : workspace.Boards()) {
                        if (board.id != menu.targetId) continue;
                        auto updated = board;
                        updated.layerId = workspace.ActiveLayerId();
                        (void)workspace.UpdateBoard(std::move(updated));
                        break;
                    }
                } else if (index == 2) {
                    for (const auto& board : workspace.Boards()) {
                        if (board.id != menu.targetId) continue;
                        auto updated = board;
                        updated.locked = !updated.locked;
                        (void)workspace.UpdateBoard(std::move(updated));
                        break;
                    }
                } else if (index == 3) (void)workspace.ToggleSelectedBoardCollapsed();
                else if (index == 4) (void)workspace.DuplicateVisualSelection();
                else if (index == 5) (void)workspace.DeleteVisualSelection();
            } else if (menu.targetType == "Layer") {
                std::size_t layerIndex = workspace.Layers().size();
                for (std::size_t candidate = 0; candidate < workspace.Layers().size(); ++candidate)
                    if (workspace.Layers()[candidate].id == menu.targetId) { layerIndex = candidate; break; }
                if (index == 0 && layerIndex < workspace.Layers().size()) {
                    const auto& layer = workspace.Layers()[layerIndex];
                    workspace.BeginInlineEdit("Layer", layer.id, layer.name);
                } else if ((index == 1 || index == 2) && layerIndex < workspace.Layers().size()) {
                    const std::ptrdiff_t offset = index == 1 ? -1 : 1;
                    const std::ptrdiff_t adjacent = static_cast<std::ptrdiff_t>(layerIndex) + offset;
                    if (adjacent >= 0 && adjacent < static_cast<std::ptrdiff_t>(workspace.Layers().size())) {
                        auto first = workspace.Layers()[layerIndex];
                        auto second = workspace.Layers()[static_cast<std::size_t>(adjacent)];
                        std::swap(first.sortOrder, second.sortOrder);
                        (void)workspace.UpdateLayer(std::move(second));
                        (void)workspace.UpdateLayer(std::move(first));
                    }
                } else if (index == 3 && layerIndex < workspace.Layers().size()) {
                    if (workspace.Layers()[layerIndex].name == "Main")
                        workspace.SetLocalNotification("MAIN LAYER CANNOT BE DELETED");
                    else {
                        (void)workspace.DeleteLayer(menu.targetId, false);
                        if (workspace.ActiveLayerId() == menu.targetId) workspace.SetActiveLayerId("Main");
                    }
                }
            } else if (menu.targetType == "View") {
                if (index == 1) {
                    workspace.CopyVisualSelection();
                    if (!workspace.PasteVisualSelectionLocal())
                        workspace.SetLocalNotification("VIEW COPY: same-space view kept; entity not duplicated");
                } else if (index == 2) {
                    const auto view = std::find_if(workspace.Projects().Workspace().views.begin(),
                        workspace.Projects().Workspace().views.end(),
                        [&](const Crystal::EntityView& candidate) {
                            return candidate.entityId == menu.targetId || candidate.id == menu.targetId;
                        });
                    Crystal::BoardModel* board = HitBoard(workspace, menu.screenPosition);
                    if (view != workspace.Projects().Workspace().views.end() && board != nullptr) {
                        const Engine::Vec2F local{view->position.x - board->position.x,
                            view->position.y - board->position.y};
                        const auto result = workspace.AttachViewToBoard(board->id, view->id, local);
                        if (!result.ok && result.error.find("board_locked") != std::string::npos)
                            workspace.SetLocalNotification("BOARD LOCKED");
                    } else workspace.SetLocalNotification("ATTACH: place the view over a board");
                } else if (index == 3) {
                    for (const auto& view : workspace.Projects().Workspace().views)
                        if (view.entityId == menu.targetId || view.id == menu.targetId) {
                            if (view.boardId.empty()) workspace.SetLocalNotification("VIEW IS NOT ATTACHED");
                            else (void)workspace.DetachViewFromBoard(view.boardId, view.id);
                            break;
                        }
                } else if (index == 4) {
                    for (const auto& view : workspace.Projects().Workspace().views)
                        if (view.entityId == menu.targetId || view.id == menu.targetId) {
                            auto updated = view;
                            updated.layerId = workspace.ActiveLayerId();
                            (void)workspace.SaveView(updated);
                            break;
                        }
                } else if (index == 5) {
                    bool exists = false;
                    for (const auto& view : workspace.Projects().Workspace().views)
                        if (view.entityId == menu.targetId &&
                            view.spaceId == workspace.Projects().CurrentSpace().Current().id) exists = true;
                    if (!exists) {
                        Crystal::EntityView view;
                        view.entityId = menu.targetId;
                        view.spaceId = workspace.Projects().CurrentSpace().Current().id;
                        view.id = "view:" + view.entityId + ":" + view.spaceId;
                        view.position = workspace.Projects().CurrentSpace().Canvas().ScreenToWorld(menu.screenPosition);
                        view.revision = 0;
                        (void)workspace.SaveView(view);
                    }
                } else if (index == 6) (void)workspace.DeleteVisualSelection();
            } else if (menu.targetType == "Ink") {
                std::vector<std::string> selectedStrokes;
                for (const auto& stroke : workspace.Ink().Strokes()) {
                    if (workspace.Selection().IsSelected(Engine::UI2D::SelectionKind::InkStroke, stroke.id) ||
                        (!stroke.groupId.empty() && workspace.Selection().IsSelected(
                            Engine::UI2D::SelectionKind::InkGroup, stroke.groupId)))
                        selectedStrokes.push_back(stroke.id);
                }
                if (index == 0 && selectedStrokes.size() >= 1)
                    (void)workspace.GroupInkStrokes(selectedStrokes, "Ink Group");
                else if (index == 1) {
                    for (const auto& stroke : workspace.Ink().Strokes()) {
                        if (std::find(selectedStrokes.begin(), selectedStrokes.end(), stroke.id) == selectedStrokes.end() ||
                            stroke.groupId.empty()) continue;
                        auto updated = stroke;
                        updated.groupId.clear();
                        if (workspace.Api().UpdateStroke(updated, nullptr).ok)
                            workspace.ApplyRealtimeEvent(Crystal::CrystalRealtimeEvent{
                                Crystal::CrystalRealtimeEventType::InkUpdated, "ink.updated",
                                Engine::Json::Value::Object{{"stroke", Crystal::ToJson(updated)}}});
                    }
                } else if (index == 2) (void)workspace.DuplicateVisualSelection();
                else if (index == 3) {
                    for (const auto& stroke : workspace.Ink().Strokes()) {
                        if (std::find(selectedStrokes.begin(), selectedStrokes.end(), stroke.id) == selectedStrokes.end()) continue;
                        auto updated = stroke;
                        updated.layer = workspace.ActiveLayerId();
                        if (workspace.Api().UpdateStroke(updated, nullptr).ok)
                            workspace.ApplyRealtimeEvent(Crystal::CrystalRealtimeEvent{
                                Crystal::CrystalRealtimeEventType::InkUpdated, "ink.updated",
                                Engine::Json::Value::Object{{"stroke", Crystal::ToJson(updated)}}});
                    }
                } else if (index == 4) (void)workspace.DeleteVisualSelection();
            } else if (menu.targetType == "Widget") {
                if (index == 0) workspace.SetLocalNotification("WIDGET INSPECTOR: E RENAME, drag corner RESIZE");
                else if (index == 1) {
                    for (const auto& widget : workspace.DashboardWidgets())
                        if (widget.id == menu.targetId)
                            workspace.BeginInlineEdit("Widget", widget.id,
                                widget.title.empty() ? widget.widgetType : widget.title);
                } else if (index == 2) {
                    for (const auto& widget : workspace.DashboardWidgets())
                        if (widget.id == menu.targetId) {
                            auto updated = widget;
                            updated.layerId = workspace.ActiveLayerId();
                            (void)workspace.UpdateDashboardWidget(std::move(updated));
                            break;
                        }
                } else if (index == 3) (void)workspace.DuplicateVisualSelection();
                else if (index == 4) (void)workspace.DeleteVisualSelection();
            } else {
                const Engine::Vec2F world = workspace.Projects().CurrentSpace().Canvas().ScreenToWorld(menu.screenPosition);
                if (index == 0) (void)workspace.CreateNote("New Note", world);
                else if (index == 1) {
                    Crystal::BoardModel board;
                    board.projectId = workspace.Projects().CurrentProject().id;
                    board.spaceId = workspace.Projects().CurrentSpace().Current().id;
                    board.title = "New Board";
                    board.position = world;
                    board.layerId = workspace.ActiveLayerId();
                    (void)workspace.CreateBoard(std::move(board));
                } else if (index == 2 && workspace.PasteVisualSelectionLocal())
                    (void)workspace.PersistVisualSelection();
            }
        }
    }
    workspace.CloseContextMenu();
    return true;
}

bool HandlePropertyPickerClick(Crystal::CrystalWorkspace& workspace,
    Engine::Vec2F screen, float width, float height) {
    if (!workspace.PropertyPicker().open || width < 1050.0f) return false;
    constexpr float popupWidth = 274.0f;
    constexpr float popupHeight = 240.0f;
    const float popupX = width - popupWidth - 14.0f;
    const float popupY = std::min(190.0f, height - popupHeight - 12.0f);
    const bool inside = screen.x >= popupX && screen.x <= popupX + popupWidth &&
        screen.y >= popupY && screen.y <= popupY + popupHeight;
    if (!inside) {
        workspace.ClosePropertyPicker();
        return true;
    }
    const int row = static_cast<int>((screen.y - popupY - 42.0f) / 28.0f);
    if (row < 0) return true;
    auto lower = [](std::string value) {
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
        return value;
    };
    const std::string filter = lower(workspace.PropertyPicker().filter);
    std::vector<std::string> matchingIds;
    for (const auto& entity : workspace.Projects().Workspace().entities) {
        if (std::find(workspace.PropertyPicker().entityIds.begin(),
                workspace.PropertyPicker().entityIds.end(), entity.id) ==
            workspace.PropertyPicker().entityIds.end()) continue;
        const std::string haystack = lower(entity.title + " " +
            std::string(Crystal::EntityTypeName(entity.type)) + " " + entity.typeName);
        if (filter.empty() || haystack.find(filter) != std::string::npos)
            matchingIds.push_back(entity.id);
    }
    if (row >= static_cast<int>(matchingIds.size())) return true;
    const std::string selectedEntityId = workspace.Selection().SelectedId();
    const Crystal::CrystalEntity* selected = EntityForView(workspace, selectedEntityId);
    const Crystal::CrystalPropertyDefinition* definition = selected == nullptr ? nullptr
        : CustomDefinitionFor(workspace, *selected, workspace.PropertyPicker().property);
    if (selected == nullptr || definition == nullptr) {
        workspace.ClosePropertyPicker();
        return true;
    }
    std::string draft;
    const Engine::Json::Value* current = selected->properties.Find(workspace.PropertyPicker().property);
    if (definition->type == Crystal::PropertyType::EntityReferenceList) {
        std::vector<std::string> references;
        if (current != nullptr && current->IsArray()) {
            for (const auto& item : current->AsArray())
                if (item.IsString()) references.push_back(item.AsString());
        }
        references = Crystal::PropertyEditor::AddReference(std::move(references),
            matchingIds[static_cast<std::size_t>(row)]);
        for (std::size_t index = 0; index < references.size(); ++index) {
            if (index != 0) draft += ",";
            draft += references[index];
        }
    } else draft = matchingIds[static_cast<std::size_t>(row)];
    const Crystal::PropertyEditResult parsed = Crystal::PropertyEditor::Parse(
        *definition, draft, workspace.PropertyPicker().entityIds);
    if (parsed.valid) {
        const Crystal::ApiResult result = workspace.UpdateCustomProperty(
            selected->id, workspace.PropertyPicker().property, parsed.value);
        if (!result.ok) workspace.SetLocalNotification("PROPERTY: " + result.error);
    } else workspace.SetLocalNotification("PROPERTY: " + parsed.error);
    workspace.ClosePropertyPicker();
    return true;
}

std::size_t TaskColumn(Crystal::TaskStatus status) {
    switch (status) {
    case Crystal::TaskStatus::Ready: return 1;
    case Crystal::TaskStatus::InProgress: return 2;
    case Crystal::TaskStatus::Review: return 3;
    case Crystal::TaskStatus::Testing: return 4;
    case Crystal::TaskStatus::Done: return 5;
    case Crystal::TaskStatus::Blocked: return 6;
    default: return 0;
    }
}

Crystal::TaskStatus TaskStatusForColumn(std::size_t column) {
    switch (column) {
    case 1: return Crystal::TaskStatus::Ready;
    case 2: return Crystal::TaskStatus::InProgress;
    case 3: return Crystal::TaskStatus::Review;
    case 4: return Crystal::TaskStatus::Testing;
    case 5: return Crystal::TaskStatus::Done;
    case 6: return Crystal::TaskStatus::Blocked;
    default: return Crystal::TaskStatus::Backlog;
    }
}

std::optional<std::string> HitTaskBoard(
    Crystal::CrystalWorkspace& workspace, Engine::Vec2F screen, float width) {
    if (workspace.Projects().CurrentSpace().Current().viewType != Crystal::SpaceViewType::TaskBoard)
        return std::nullopt;
    const float worldWidth = width >= 1050.0f ? width - 300.0f : width;
    if (screen.y < 106.0f || screen.x < 0.0f || screen.x >= worldWidth) return std::nullopt;
    const std::size_t column = static_cast<std::size_t>(screen.x / (worldWidth / 7.0f));
    const int row = static_cast<int>((screen.y - 106.0f) / 92.0f);
    if (row < 0) return std::nullopt;
    std::array<int, 7> currentRows{};
    for (const auto& entity : workspace.Projects().Workspace().entities) {
        if (entity.type != Crystal::EntityType::Task) continue;
        const auto* details = std::get_if<Crystal::TaskDetails>(&entity.details);
        const std::size_t entityColumn = details == nullptr ? 0 : TaskColumn(details->status);
        if (entityColumn == column && currentRows[entityColumn]++ == row) return entity.id;
    }
    return std::nullopt;
}

std::optional<std::string> HitRelationship(
    Crystal::CrystalWorkspace& workspace, Engine::Vec2F screen) {
    if (workspace.Projects().CurrentSpace().Current().viewType == Crystal::SpaceViewType::TaskBoard)
        return std::nullopt;
    const auto& camera = workspace.Projects().CurrentSpace().Canvas();
    const auto& model = workspace.Projects().Workspace();
    const auto findView = [&](const std::string& entityId) -> const Crystal::EntityView* {
        for (const auto& view : model.views)
            if (view.entityId == entityId && view.spaceId == workspace.Projects().CurrentSpace().Current().id && view.visible)
                return &view;
        return nullptr;
    };
    for (const auto& relationship : model.relationships) {
        if (!relationship.visible) continue;
        const auto* source = findView(relationship.sourceEntityId);
        const auto* target = findView(relationship.targetEntityId);
        if (source == nullptr || target == nullptr) continue;
        const Engine::Vec2F a = camera.WorldToScreen({
            source->position.x + source->size.x * 0.5f, source->position.y + source->size.y * 0.5f});
        const Engine::Vec2F b = camera.WorldToScreen({
            target->position.x + target->size.x * 0.5f, target->position.y + target->size.y * 0.5f});
        const float dx = b.x - a.x;
        const float dy = b.y - a.y;
        const float lengthSquared = dx * dx + dy * dy;
        const float projection = lengthSquared <= 0.01f
            ? 0.0f : std::clamp(((screen.x - a.x) * dx + (screen.y - a.y) * dy) / lengthSquared, 0.0f, 1.0f);
        const float px = a.x + projection * dx - screen.x;
        const float py = a.y + projection * dy - screen.y;
        if (px * px + py * py <= 100.0f) return relationship.id;
    }
    return std::nullopt;
}

void SetToolbarTool(Crystal::CrystalWorkspace& workspace, float x) {
    constexpr float left = 335.0f;
    constexpr float step = 90.0f;
    if (x < left || x >= left + step * 5.0f) return;
    const int index = static_cast<int>((x - left) / step);
    workspace.Tools().SetTool(index == 0 ? Engine::UI2D::CanvasTool::Select
        : index == 1 ? Engine::UI2D::CanvasTool::Node
        : index == 2 ? Engine::UI2D::CanvasTool::Connect
        : index == 3 ? Engine::UI2D::CanvasTool::Pen
                     : Engine::UI2D::CanvasTool::Eraser);
}

void SwitchProject(Crystal::CrystalWorkspace& workspace) {
    const std::string next = workspace.Projects().CurrentProject().id == "P_HighSchool"
        ? "XiE" : "P_HighSchool";
    const Crystal::ApiResult result = workspace.Connect(next);
    if (!result.ok) std::cerr << "Project switch failed: " << result.error << '\n';
}

void SwitchSpace(Crystal::CrystalWorkspace& workspace) {
    const auto& spaces = workspace.Projects().Workspace().spaces;
    if (spaces.size() < 2) return;
    const std::string current = workspace.Projects().CurrentSpace().Current().id;
    const std::string next = spaces[0].id == current ? spaces[1].id : spaces[0].id;
    const Crystal::ApiResult result = workspace.SwitchSpace(next);
    if (!result.ok) std::cerr << "Space switch failed: " << result.error << '\n';
}

std::string AppendCodepoint(std::string value, char32_t codepoint) {
    if (codepoint <= 0x7F) value.push_back(static_cast<char>(codepoint));
    else if (codepoint <= 0x7FF) {
        value.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
        value.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0xFFFF) {
        value.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
        value.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        value.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0x10FFFF) {
        value.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
        value.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        value.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        value.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
    return value;
}

std::string InspectorDraftText(const Engine::Json::Value& value) {
    if (value.IsString()) return value.AsString();
    if (value.IsBool()) return value.AsBool() ? "true" : "false";
    if (value.IsNumber()) return std::to_string(value.AsNumber());
    if (value.IsArray()) {
        std::string result;
        for (std::size_t index = 0; index < value.AsArray().size(); ++index) {
            if (index != 0) result += ", ";
            result += InspectorDraftText(value.AsArray()[index]);
        }
        return result;
    }
    return value.Dump();
}

std::optional<InspectorField> InspectorFieldAt(const Crystal::CrystalEntity& entity, float y) {
    if (y >= 96.0f && y < 132.0f) return InspectorField::Title;
    if (y >= 170.0f && y < 220.0f) return InspectorField::Description;
    if (y < 220.0f) return std::nullopt;
    if (entity.type == Crystal::EntityType::Task) {
        if (y < 252.0f) return InspectorField::Status;
        if (y < 276.0f) return InspectorField::Priority;
        if (y < 300.0f) return InspectorField::Owner;
        if (y < 324.0f) return InspectorField::Branch;
        if (y < 396.0f) return InspectorField::AcceptanceCriteria;
    } else if (entity.type == Crystal::EntityType::System) {
        if (y < 252.0f) return InspectorField::Status;
    } else if (entity.type == Crystal::EntityType::Milestone) {
        if (y < 252.0f) return InspectorField::Status;
        if (y < 288.0f) return InspectorField::TargetDate;
    } else if (entity.type == Crystal::EntityType::Scene) {
        if (y < 276.0f) return InspectorField::AssetPath;
    }
    return std::nullopt;
}

const Crystal::CrystalPropertyDefinition* CustomDefinitionFor(
    const Crystal::CrystalWorkspace& workspace, const Crystal::CrystalEntity& entity,
    const std::string& propertyName) {
    for (const auto& schema : workspace.Schemas()) {
        if (schema.id != entity.schemaId) continue;
        for (const auto& definition : schema.properties)
            if (definition.name == propertyName) return &definition;
    }
    return nullptr;
}

std::string PopCodepoint(std::string value);

std::optional<std::string> CustomPropertyAt(
    const Crystal::CrystalWorkspace& workspace, const Crystal::CrystalEntity& entity, float y) {
    if (entity.type != Crystal::EntityType::Unknown || entity.schemaId.empty()) return std::nullopt;
    float propertyY = 58.0f + 170.0f + 28.0f;
    for (const auto& schema : workspace.Schemas()) {
        if (schema.id != entity.schemaId) continue;
        for (const auto& definition : schema.properties) {
            if (y >= propertyY - 14.0f && y < propertyY + 10.0f) return definition.name;
            propertyY += 22.0f;
        }
        break;
    }
    return std::nullopt;
}

Crystal::ApiResult UpdateCustomInspectorText(
    Crystal::CrystalWorkspace& workspace, const std::string& propertyName,
    bool removeLast, char32_t codepoint = U'\0') {
    const std::string entityId = workspace.Selection().SelectedId();
    Crystal::CrystalEntity* selected = EntityForView(workspace, entityId);
    if (selected == nullptr || selected->type != Crystal::EntityType::Unknown)
        return {false, 404, {}, "custom_entity_not_found"};
    const auto* definition = CustomDefinitionFor(workspace, *selected, propertyName);
    if (definition == nullptr || (definition->type != Crystal::PropertyType::Text &&
        definition->type != Crystal::PropertyType::MultilineText &&
        definition->type != Crystal::PropertyType::Date && definition->type != Crystal::PropertyType::URL))
        return {true, 200, {}, {}};
    std::string current;
    if (const auto* value = selected->properties.Find(propertyName); value != nullptr && value->IsString())
        current = value->AsString();
    current = removeLast ? PopCodepoint(std::move(current)) : AppendCodepoint(std::move(current), codepoint);
    return workspace.UpdateCustomProperty(entityId, propertyName, Engine::Json::Value(std::move(current)));
}

Crystal::ApiResult CycleCustomInspectorProperty(
    Crystal::CrystalWorkspace& workspace, const std::string& propertyName) {
    const std::string entityId = workspace.Selection().SelectedId();
    Crystal::CrystalEntity* selected = EntityForView(workspace, entityId);
    if (selected == nullptr) return {false, 404, {}, "entity_not_found"};
    const auto* definition = CustomDefinitionFor(workspace, *selected, propertyName);
    if (definition == nullptr) return {true, 200, {}, {}};
    const auto* currentValue = selected->properties.Find(propertyName);
    if (definition->type == Crystal::PropertyType::Bool) {
        const bool next = currentValue == nullptr || !currentValue->AsBool();
        return workspace.UpdateCustomProperty(entityId, propertyName, Engine::Json::Value(next));
    }
    if (definition->type != Crystal::PropertyType::Enum || definition->options.empty())
        return {true, 200, {}, {}};
    const std::string current = currentValue != nullptr && currentValue->IsString()
        ? currentValue->AsString() : std::string{};
    auto found = std::find(definition->options.begin(), definition->options.end(), current);
    const std::size_t index = found == definition->options.end() ? 0
        : (static_cast<std::size_t>(std::distance(definition->options.begin(), found)) + 1) % definition->options.size();
    return workspace.UpdateCustomProperty(entityId, propertyName, Engine::Json::Value(definition->options[index]));
}

std::string PopCodepoint(std::string value) {
    if (value.empty()) return value;
    value.pop_back();
    while (!value.empty() && (static_cast<unsigned char>(value.back()) & 0xC0) == 0x80)
        value.pop_back();
    return value;
}

Crystal::ApiResult UpdateInspectorText(
    Crystal::CrystalWorkspace& workspace, InspectorField field, bool removeLast,
    char32_t codepoint = U'\0') {
    const std::string entityId = workspace.Selection().SelectedId();
    Crystal::CrystalEntity* selected = EntityForView(workspace, entityId);
    if (selected == nullptr) return {false, 404, {}, "entity_not_found"};
    Crystal::CrystalEntity copy = *selected;
    auto edit = [&](std::string& value) {
        value = removeLast ? PopCodepoint(std::move(value)) : AppendCodepoint(std::move(value), codepoint);
    };
    if (field == InspectorField::Title) {
        edit(copy.title);
        return workspace.RenameEntity(entityId, std::move(copy.title));
    }
    if (field == InspectorField::Description) {
        edit(copy.description);
    } else if (field == InspectorField::Branch || field == InspectorField::AcceptanceCriteria ||
               field == InspectorField::Owner) {
        Crystal::TaskDetails details = std::holds_alternative<Crystal::TaskDetails>(copy.details)
            ? std::get<Crystal::TaskDetails>(copy.details) : Crystal::TaskDetails{};
        if (field == InspectorField::Branch) edit(details.branch);
        else if (field == InspectorField::AcceptanceCriteria) edit(details.acceptanceCriteria);
        else edit(details.assignedWorkerId);
        copy.details = std::move(details);
    } else if (field == InspectorField::TargetDate) {
        Crystal::MilestoneDetails details = std::holds_alternative<Crystal::MilestoneDetails>(copy.details)
            ? std::get<Crystal::MilestoneDetails>(copy.details) : Crystal::MilestoneDetails{};
        edit(details.targetDate);
        copy.details = std::move(details);
    } else if (field == InspectorField::AssetPath) {
        Crystal::SceneDetails details = std::holds_alternative<Crystal::SceneDetails>(copy.details)
            ? std::get<Crystal::SceneDetails>(copy.details) : Crystal::SceneDetails{};
        edit(details.assetPath);
        copy.details = std::move(details);
    } else {
        return {true, 200, {}, {}};
    }
    return workspace.UpdateEntity(std::move(copy));
}

Crystal::ApiResult CycleInspectorField(
    Crystal::CrystalWorkspace& workspace, InspectorField field) {
    const std::string entityId = workspace.Selection().SelectedId();
    Crystal::CrystalEntity* selected = EntityForView(workspace, entityId);
    if (selected == nullptr) return {false, 404, {}, "entity_not_found"};
    Crystal::CrystalEntity copy = *selected;
    if (field == InspectorField::Status && copy.type == Crystal::EntityType::Task) {
        auto details = std::holds_alternative<Crystal::TaskDetails>(copy.details)
            ? std::get<Crystal::TaskDetails>(copy.details) : Crystal::TaskDetails{};
        switch (details.status) {
        case Crystal::TaskStatus::Backlog: details.status = Crystal::TaskStatus::Ready; break;
        case Crystal::TaskStatus::Ready: details.status = Crystal::TaskStatus::InProgress; break;
        case Crystal::TaskStatus::InProgress: details.status = Crystal::TaskStatus::Review; break;
        case Crystal::TaskStatus::Review: details.status = Crystal::TaskStatus::Testing; break;
        case Crystal::TaskStatus::Testing: details.status = Crystal::TaskStatus::Done; break;
        case Crystal::TaskStatus::Done: details.status = Crystal::TaskStatus::Blocked; break;
        case Crystal::TaskStatus::Blocked: details.status = Crystal::TaskStatus::Backlog; break;
        }
        copy.details = std::move(details);
    } else if (field == InspectorField::Priority && copy.type == Crystal::EntityType::Task) {
        auto details = std::holds_alternative<Crystal::TaskDetails>(copy.details)
            ? std::get<Crystal::TaskDetails>(copy.details) : Crystal::TaskDetails{};
        switch (details.priority) {
        case Crystal::TaskPriority::Low: details.priority = Crystal::TaskPriority::Normal; break;
        case Crystal::TaskPriority::Normal: details.priority = Crystal::TaskPriority::High; break;
        case Crystal::TaskPriority::High: details.priority = Crystal::TaskPriority::Critical; break;
        case Crystal::TaskPriority::Critical: details.priority = Crystal::TaskPriority::Low; break;
        }
        copy.details = std::move(details);
    } else if (field == InspectorField::Owner && copy.type == Crystal::EntityType::Task) {
        auto details = std::holds_alternative<Crystal::TaskDetails>(copy.details)
            ? std::get<Crystal::TaskDetails>(copy.details) : Crystal::TaskDetails{};
        std::string nextWorker;
        bool foundCurrent = details.assignedWorkerId.empty();
        for (const auto& entity : workspace.Projects().Workspace().entities) {
            if (entity.type != Crystal::EntityType::Worker) continue;
            if (foundCurrent) {
                nextWorker = entity.id;
                break;
            }
            if (entity.id == details.assignedWorkerId) foundCurrent = true;
        }
        if (nextWorker.empty()) {
            for (const auto& entity : workspace.Projects().Workspace().entities)
                if (entity.type == Crystal::EntityType::Worker) { nextWorker = entity.id; break; }
        }
        details.assignedWorkerId = std::move(nextWorker);
        copy.details = std::move(details);
    } else if (field == InspectorField::Status && copy.type == Crystal::EntityType::System) {
        auto details = std::holds_alternative<Crystal::SystemDetails>(copy.details)
            ? std::get<Crystal::SystemDetails>(copy.details) : Crystal::SystemDetails{};
        switch (details.status) {
        case Crystal::SystemStatus::Planned: details.status = Crystal::SystemStatus::Developing; break;
        case Crystal::SystemStatus::Developing: details.status = Crystal::SystemStatus::Testing; break;
        case Crystal::SystemStatus::Testing: details.status = Crystal::SystemStatus::Stable; break;
        case Crystal::SystemStatus::Stable: details.status = Crystal::SystemStatus::Deprecated; break;
        case Crystal::SystemStatus::Deprecated: details.status = Crystal::SystemStatus::Planned; break;
        }
        copy.details = std::move(details);
    } else if (field == InspectorField::Status && copy.type == Crystal::EntityType::Milestone) {
        auto details = std::holds_alternative<Crystal::MilestoneDetails>(copy.details)
            ? std::get<Crystal::MilestoneDetails>(copy.details) : Crystal::MilestoneDetails{};
        switch (details.status) {
        case Crystal::MilestoneStatus::Planned: details.status = Crystal::MilestoneStatus::Active; break;
        case Crystal::MilestoneStatus::Active: details.status = Crystal::MilestoneStatus::Completed; break;
        case Crystal::MilestoneStatus::Completed: details.status = Crystal::MilestoneStatus::Blocked; break;
        case Crystal::MilestoneStatus::Blocked: details.status = Crystal::MilestoneStatus::Planned; break;
        }
        copy.details = std::move(details);
    } else {
        return {true, 200, {}, {}};
    }
    return workspace.UpdateEntity(std::move(copy));
}

} // namespace

int main(int argc, char** argv) {
    const Options options = ParseOptions(argc, argv);
    Engine::GlfwApplicationHost host;
    if (!host.IsInitialized()) {
        std::cerr << "GLFW initialization failed\n";
        return 1;
    }
    Engine::GlfwWindowSystem windows;
    const Engine::WindowHandle window = windows.CreateWindow({
        "Crystal", 1280, 760, true, true, true, false, false, false});
    Engine::InputEventQueue input(window);
    windows.SetInputEventQueue(window, &input);
    GLFWwindow* native = windows.GetGlfwWindow(window);
    if (native == nullptr || gladLoadGL(glfwGetProcAddress) == 0) {
        std::cerr << "OpenGL initialization failed\n";
        return 1;
    }
    // Some multi-monitor/overlaid desktop setups make GLFW restore a stale
    // off-screen position.  Crystal is a desktop tool, so make the first
    // frame reachable and focused for real acceptance runs.
    glfwRestoreWindow(native);
    glfwShowWindow(native);
    glfwSetWindowSize(native, 1280, 760);
    glfwSetWindowPos(native, 64, 64);
    glfwFocusWindow(native);

    Crystal::CrystalWorkspace workspace(options.server);
    workspace.SetRealtimeWakeCallback([&host]() { host.WakeEventLoop(); });
    const Crystal::ApiResult connected = workspace.Connect(options.project);
    if (!connected.ok) std::cerr << "Crystal server unavailable: " << connected.error << '\n';
    Engine::GlfwOpenGLSurface surface(windows, window);
    Crystal::CrystalRenderer renderer(surface);
    if (!renderer.Initialize()) {
        std::cerr << "Crystal renderer initialization failed\n";
        return 1;
    }
    Crystal::CrystalProfiler profiler;
    g_Profiler = &profiler;
    renderer.SetProfiler(&profiler);

    bool draggingEntity = false;
    std::string draggingEntityId;
    Engine::Vec2F dragOffset{};
    Engine::Vec2F previousPointer{};
    bool havePreviousPointer = false;
    std::string connectSourceId;
    Crystal::RelationshipType connectType = Crystal::RelationshipType::RelatedTo;
    bool searchMode = false;
    InspectorField inspectorField = InspectorField::Title;
    std::string inspectorCustomProperty;
    std::string inspectorDraft;
    bool inspectorDraftActive = false;
    bool boxSelecting = false;
    Engine::Vec2F boxStart{};
    bool draggingBoard = false;
    bool resizingBoard = false;
    std::string draggingBoardId;
    bool draggingWidget = false;
    bool resizingWidget = false;
    std::string draggingWidgetId;
    Engine::Vec2F visualStartSize{};
    bool performanceFixtureLoaded = false;
    Engine::FrameScheduler frameScheduler;
    Engine::WindowContext frameWindow;
    frameWindow.window = window;
    frameWindow.visible = true;
    frameWindow.focused = true;
    frameWindow.framePolicy = Engine::FramePolicy::Static;
    frameWindow.dirty = true;
    bool wasMinimized = false;
    bool wasVisible = true;
    float previousDpiScale = 0.0f;
    int previousLogicalWidth = 0;
    int previousLogicalHeight = 0;
    Engine::RenderSurfaceSize previousFramebufferSize{};
    auto requestFrame = [&](Engine::FrameReason reason) {
        frameScheduler.RequestFrame(window, reason);
        profiler.RecordFrameRequest(Engine::FrameReasonName(reason));
    };
    requestFrame(Engine::FrameReason::Explicit);
    while (!windows.IsCloseRequested(window)) {
        profiler.BeginFrame();
        bool waitForEvents = false;
        double frameWaitSeconds = 0.0;
        {
            Crystal::CrystalProfiler::Scope profile(g_Profiler, "FrameTotal");
            {
                Crystal::CrystalProfiler::Scope eventProfile(g_Profiler, "EventPump");
                host.PollEvents();
            }
            frameWindow.visible = windows.IsVisible(window);
            frameWindow.minimized = windows.IsMinimized(window);
            frameWindow.focused = windows.IsFocused(window);
            frameWindow.closeRequested = windows.IsCloseRequested(window);
            frameWindow.dpiScale = windows.GetDpiScale(window);
            frameWindow.framebufferSize = windows.GetFramebufferSize(window);
            if (!wasVisible && frameWindow.visible)
                requestFrame(Engine::FrameReason::WindowExpose);
            wasVisible = frameWindow.visible;
            if (wasMinimized && !frameWindow.minimized)
                requestFrame(Engine::FrameReason::WindowRestore);
            wasMinimized = frameWindow.minimized;
            if (previousDpiScale > 0.0f &&
                std::abs(previousDpiScale - frameWindow.dpiScale) > 0.001f) {
                requestFrame(Engine::FrameReason::Resize);
                renderer.InvalidateGeometryCache();
            }
            previousDpiScale = frameWindow.dpiScale;
            if (previousFramebufferSize.width != frameWindow.framebufferSize.width ||
                previousFramebufferSize.height != frameWindow.framebufferSize.height) {
                requestFrame(Engine::FrameReason::Resize);
                renderer.InvalidateGeometryCache();
                previousFramebufferSize = frameWindow.framebufferSize;
            }
        {
            Crystal::CrystalProfiler::Scope profile(g_Profiler, "RealtimePump");
            if (workspace.PumpRealtimeEvents()) {
                renderer.InvalidateGeometryCache();
                requestFrame(Engine::FrameReason::Realtime);
            }
        }
        if (options.performanceFixture && !performanceFixtureLoaded &&
            workspace.ConnectionStatus() == "Connected") {
            workspace.LoadSyntheticPerformanceFixtureForDebug();
            performanceFixtureLoaded = true;
            renderer.InvalidateGeometryCache();
            requestFrame(Engine::FrameReason::WorkspaceChange);
        }
        int logicalWidth = 1;
        int logicalHeight = 1;
        glfwGetWindowSize(native, &logicalWidth, &logicalHeight);
        if (logicalWidth != previousLogicalWidth || logicalHeight != previousLogicalHeight) {
            workspace.SetViewport({static_cast<float>(logicalWidth), static_cast<float>(logicalHeight)});
            previousLogicalWidth = logicalWidth;
            previousLogicalHeight = logicalHeight;
            requestFrame(Engine::FrameReason::Resize);
            renderer.InvalidateGeometryCache();
        }

        const std::vector<Engine::InputEvent> events = input.Drain();
        bool pointerMoveOnly = !events.empty();
        bool cameraOnly = !events.empty();
        for (const Engine::InputEvent& event : events)
            if (!std::holds_alternative<Engine::PointerMoveEvent>(event)) pointerMoveOnly = false;
        for (const Engine::InputEvent& event : events)
            if (!std::holds_alternative<Engine::PointerWheelEvent>(event)) cameraOnly = false;
        const bool inputWasInteractive = draggingEntity || draggingBoard || draggingWidget ||
            boxSelecting || workspace.Ink().Drawing();
        bool cameraPan = false;
        {
            Crystal::CrystalProfiler::Scope updateProfile(g_Profiler, "Update");
            for (const Engine::InputEvent& event : events) {
            if (const auto* button = std::get_if<Engine::PointerButtonEvent>(&event)) {
                const Engine::Vec2F position = button->logicalPosition;
                if (position.y < 58.0f && button->pressed &&
                    button->button == Engine::PointerButton::Primary) {
                    if (position.x >= 335.0f && position.x < 785.0f) SetToolbarTool(workspace, position.x);
                    else if (position.x >= 245.0f && position.x < 325.0f) searchMode = true;
                    else if (position.x >= 140.0f && position.x < 240.0f) SwitchProject(workspace);
                    continue;
                }
                if (workspace.ContextMenu().open && button->pressed &&
                    button->button == Engine::PointerButton::Primary) {
                    (void)HandleContextMenuClick(workspace, position,
                        static_cast<float>(logicalWidth), static_cast<float>(logicalHeight));
                    continue;
                }
                if (workspace.PropertyPicker().open && button->pressed &&
                    button->button == Engine::PointerButton::Primary) {
                    (void)HandlePropertyPickerClick(workspace, position,
                        static_cast<float>(logicalWidth), static_cast<float>(logicalHeight));
                    continue;
                }
                if (button->pressed && button->button == Engine::PointerButton::Secondary &&
                    position.y >= 58.0f) {
                    if (position.x < 190.0f && position.y >= 90.0f) {
                        const int layerIndex = static_cast<int>((position.y - 90.0f) / 28.0f);
                        if (layerIndex >= 0 && layerIndex < static_cast<int>(workspace.Layers().size()))
                            workspace.OpenContextMenu(position,
                                workspace.Layers()[static_cast<std::size_t>(layerIndex)].id, "Layer");
                        else workspace.OpenContextMenu(position, {}, "Blank");
                    } else if (const auto* widget = HitWidget(workspace, position))
                        workspace.OpenContextMenu(position, widget->id, "Widget");
                    else if (const auto* view = HitView(workspace, position))
                        workspace.OpenContextMenu(position, view->entityId, "View");
                    else if (const auto* board = HitBoard(workspace, position))
                        workspace.OpenContextMenu(position, board->id, "Board");
                    else if (const auto stroke = HitStroke(workspace, position))
                        workspace.OpenContextMenu(position, *stroke, "Ink");
                    else workspace.OpenContextMenu(position, {}, "Blank");
                    continue;
                }
                if (position.y < 58.0f) continue;
                if (searchMode && button->pressed && button->button == Engine::PointerButton::Primary) {
                    const auto& results = workspace.SearchResults();
                    const int index = static_cast<int>((position.y - 62.0f) / 28.0f);
                    if (index >= 0 && index < static_cast<int>(results.size())) {
                        workspace.SelectRelationship({});
                        workspace.Selection().Select(results[static_cast<std::size_t>(index)].id);
                        inspectorField = InspectorField::Title;
                        inspectorCustomProperty.clear();
                        searchMode = false;
                    }
                    continue;
                }
                if (button->pressed && button->button == Engine::PointerButton::Primary &&
                    position.x < 190.0f && position.y >= 58.0f) {
                    Crystal::CrystalProfiler::Scope profile(g_Profiler, "layer");
                    const int layerIndex = position.y < 90.0f ? -1
                        : static_cast<int>((position.y - 90.0f) / 28.0f);
                    if (layerIndex >= 0 && layerIndex < static_cast<int>(workspace.Layers().size())) {
                        auto layer = workspace.Layers()[static_cast<std::size_t>(layerIndex)];
                        if (position.x < 32.0f) layer.visible = !layer.visible;
                        else if (position.x > 150.0f) layer.locked = !layer.locked;
                        else workspace.SetActiveLayerId(layer.id);
                        if (position.x < 32.0f || position.x > 150.0f)
                            (void)workspace.UpdateLayer(std::move(layer));
                    } else if (layerIndex == static_cast<int>(workspace.Layers().size()) &&
                               position.y >= 90.0f + static_cast<float>(workspace.Layers().size()) * 28.0f &&
                               position.y < 118.0f + static_cast<float>(workspace.Layers().size()) * 28.0f) {
                        Crystal::UserLayerModel layer;
                        layer.projectId = workspace.Projects().CurrentProject().id;
                        layer.spaceId = workspace.Projects().CurrentSpace().Current().id;
                        layer.name = "Layer " + std::to_string(workspace.Layers().size());
                        layer.sortOrder = static_cast<int>(workspace.Layers().size());
                        Crystal::UserLayerModel created;
                        const auto result = workspace.CreateLayer(std::move(layer), &created);
                        if (result.ok) workspace.SetActiveLayerId(created.id);
                    }
                    continue;
                }
                if (button->pressed && button->button == Engine::PointerButton::Primary &&
                    workspace.GetDisplayMode() == Crystal::DisplayMode::Draw &&
                    position.x >= 204.0f && position.x < 554.0f && position.y >= 66.0f && position.y < 96.0f) {
                    const int toolIndex = static_cast<int>((position.x - 204.0f) / 50.0f);
                    if (toolIndex == 6) workspace.Tools().SetTool(Engine::UI2D::CanvasTool::Eraser);
                    else {
                        workspace.Tools().SetTool(Engine::UI2D::CanvasTool::Pen);
                        workspace.Ink().SetTool(toolIndex == 1 ? Engine::UI2D::StrokeTool::Highlighter
                            : Engine::UI2D::StrokeTool::Pen);
                    }
                    workspace.Ink().SetShape(toolIndex == 2 ? Engine::UI2D::StrokeShape::StraightLine
                        : toolIndex == 3 ? Engine::UI2D::StrokeShape::Arrow
                        : toolIndex == 4 ? Engine::UI2D::StrokeShape::Rectangle
                        : toolIndex == 5 ? Engine::UI2D::StrokeShape::Ellipse
                        : Engine::UI2D::StrokeShape::Freehand);
                    continue;
                }
                if (button->pressed && button->button == Engine::PointerButton::Primary &&
                    workspace.GetDisplayMode() == Crystal::DisplayMode::Draw &&
                    position.x >= 560.0f && position.x < 742.0f && position.y >= 66.0f && position.y < 96.0f) {
                    constexpr std::array<const char*, 7> colors = {
                        "#9ed5ff", "#f05d68", "#f5a24d", "#f4d35e", "#61d095", "#5b8def", "#a477d6"};
                    const std::size_t index = std::min<std::size_t>(6,
                        static_cast<std::size_t>((position.x - 560.0f) / 26.0f));
                    workspace.Ink().SetColor(colors[index]);
                    continue;
                }
                const float inspectorWidth = logicalWidth >= 1050 ? 300.0f : 0.0f;
                const float worldWidth = static_cast<float>(logicalWidth) - inspectorWidth;
                if (button->pressed && button->button == Engine::PointerButton::Primary &&
                    inspectorWidth > 0.0f && position.x >= worldWidth) {
                    Crystal::CrystalProfiler::Scope profile(g_Profiler, "inspector");
                    const Crystal::DashboardWidgetModel* selectedWidget = nullptr;
                    for (const auto& item : workspace.Selection().Items()) {
                        if (item.kind != Engine::UI2D::SelectionKind::DashboardWidget) continue;
                        for (const auto& widget : workspace.DashboardWidgets())
                            if (widget.id == item.id) { selectedWidget = &widget; break; }
                        break;
                    }
                    if (selectedWidget != nullptr) {
                        if (position.y < 140.0f) {
                            workspace.BeginInlineEdit("Widget", selectedWidget->id,
                                selectedWidget->title.empty() ? selectedWidget->widgetType : selectedWidget->title);
                        } else if (position.y >= 150.0f && position.y < 185.0f) {
                            (void)workspace.ResizeSelectedWidget(
                                {selectedWidget->size.x + 40.0f, selectedWidget->size.y});
                        } else if (position.y >= 185.0f && position.y < 260.0f) {
                            auto updated = *selectedWidget;
                            const std::string current = updated.config.GetString("text", "READY");
                            updated.config["text"] = current == "READY" ? "CONFIG EDITED" : "READY";
                            (void)workspace.UpdateDashboardWidget(std::move(updated));
                        }
                    } else if (const auto* selected = EntityForView(workspace, workspace.Selection().SelectedId())) {
                        if (const auto property = CustomPropertyAt(workspace, *selected, position.y); property.has_value()) {
                            inspectorCustomProperty = *property;
                            const auto* definition = CustomDefinitionFor(
                                workspace, *selected, inspectorCustomProperty);
                            if (definition != nullptr && (definition->type == Crystal::PropertyType::EntityReference ||
                                    definition->type == Crystal::PropertyType::EntityReferenceList)) {
                                std::vector<std::string> entityIds;
                                for (const auto& entity : workspace.Projects().Workspace().entities)
                                    entityIds.push_back(entity.id);
                                workspace.OpenPropertyPicker(inspectorCustomProperty, std::move(entityIds));
                                inspectorDraftActive = false;
                                inspectorDraft.clear();
                            } else inspectorDraftActive = definition != nullptr &&
                                definition->type != Crystal::PropertyType::Bool &&
                                definition->type != Crystal::PropertyType::Enum;
                            const auto* value = selected->properties.Find(inspectorCustomProperty);
                            if (definition == nullptr || (definition->type != Crystal::PropertyType::EntityReference &&
                                    definition->type != Crystal::PropertyType::EntityReferenceList))
                                inspectorDraft = value == nullptr ? std::string{} : InspectorDraftText(*value);
                            if (!inspectorDraftActive)
                                if (definition == nullptr || (definition->type != Crystal::PropertyType::EntityReference &&
                                        definition->type != Crystal::PropertyType::EntityReferenceList))
                                    (void)CycleCustomInspectorProperty(workspace, inspectorCustomProperty);
                            continue;
                        }
                        if (const auto field = InspectorFieldAt(*selected, position.y); field.has_value()) {
                            inspectorField = *field;
                            inspectorCustomProperty.clear();
                            (void)CycleInspectorField(workspace, inspectorField);
                        }
                    }
                    continue;
                }
                const Engine::Vec2F world = workspace.Projects().CurrentSpace().Canvas().ScreenToWorld(position);
                if (button->pressed) {
                    const bool additive = HasModifier(button->modifiers, Engine::InputModifiers::Shift);
                    const bool toggle = HasModifier(button->modifiers, Engine::InputModifiers::Control);
                    const auto* hitViewBeforeVisual = HitView(workspace, position);
                    Crystal::BoardModel* hitBoard = HitBoard(workspace, position);
                    Crystal::DashboardWidgetModel* hitWidget = HitWidget(workspace, position);
                    if (workspace.Tools().Tool() == Engine::UI2D::CanvasTool::Select &&
                        button->button == Engine::PointerButton::Primary) {
                        const bool boardHeader = hitBoard != nullptr &&
                            (world.y <= hitBoard->position.y + 42.0f || hitViewBeforeVisual == nullptr);
                        if (hitWidget != nullptr && hitViewBeforeVisual == nullptr) {
                            workspace.SelectRelationship({});
                            if (toggle) workspace.Selection().ToggleWidget(hitWidget->id);
                            else workspace.Selection().SelectWidget(hitWidget->id, additive);
                            draggingWidget = true;
                            draggingWidgetId = hitWidget->id;
                            resizingWidget = world.x >= hitWidget->position.x + hitWidget->size.x - 24.0f &&
                                world.y >= hitWidget->position.y + hitWidget->size.y - 24.0f;
                            visualStartSize = hitWidget->size;
                            workspace.Tools().Cancel();
                            previousPointer = position;
                            havePreviousPointer = true;
                            continue;
                        }
                        if (hitBoard != nullptr && boardHeader) {
                            workspace.SelectRelationship({});
                            if (toggle) workspace.Selection().ToggleBoard(hitBoard->id);
                            else workspace.Selection().SelectBoard(hitBoard->id, additive);
                            if (hitBoard->locked || workspace.IsLayerLocked(hitBoard->layerId))
                                workspace.SetLocalNotification("BOARD LOCKED");
                            draggingBoard = true;
                            draggingBoardId = hitBoard->id;
                            resizingBoard = world.x >= hitBoard->position.x + hitBoard->size.x - 24.0f &&
                                world.y >= hitBoard->position.y + hitBoard->size.y - 24.0f;
                            visualStartSize = hitBoard->size;
                            workspace.Tools().Cancel();
                            previousPointer = position;
                            havePreviousPointer = true;
                            continue;
                        }
                        if (hitViewBeforeVisual == nullptr && !HitStroke(workspace, position).has_value() &&
                            !HitRelationship(workspace, position).has_value()) {
                            boxSelecting = true;
                            boxStart = position;
                            workspace.BeginSelectionRectangle(position);
                            if (!additive && !toggle) workspace.Selection().Clear();
                            workspace.Tools().Cancel();
                            previousPointer = position;
                            havePreviousPointer = true;
                            continue;
                        }
                    }
                    const auto boardEntity = HitTaskBoard(workspace, position, static_cast<float>(logicalWidth));
                    const Crystal::EntityView* hitView = hitViewBeforeVisual;
                    const auto relationshipId = HitRelationship(workspace, position);
                    const std::string entityId = boardEntity.has_value() ? *boardEntity
                        : hitView != nullptr ? hitView->entityId : std::string{};
                    const auto strokeId = HitStroke(workspace, position);
                    const Engine::UI2D::ToolAction action = workspace.Tools().PointerDown(
                        position, button->button, entityId, strokeId.value_or(std::string{}));
                    previousPointer = position;
                    havePreviousPointer = true;
                    if (action.type == Engine::UI2D::ToolActionType::SelectEntity) {
                        workspace.SelectRelationship({});
                        if (toggle) workspace.Selection().ToggleEntity(action.targetId);
                        else workspace.Selection().Select(action.targetId, additive);
                        inspectorField = InspectorField::Title;
                        inspectorCustomProperty.clear();
                        inspectorDraftActive = false;
                        if (workspace.Tools().Tool() == Engine::UI2D::CanvasTool::Connect) {
                            if (connectSourceId.empty()) {
                                connectSourceId = action.targetId;
                            } else if (connectSourceId != action.targetId) {
                                const Crystal::ApiResult result = workspace.CreateRelationship(
                                    connectSourceId, action.targetId, connectType);
                                if (!result.ok) std::cerr << "Relationship create failed: " << result.error << '\n';
                                connectSourceId.clear();
                            }
                        } else if (auto* view = HitView(workspace, position)) {
                            draggingEntity = true;
                            draggingEntityId = view->entityId;
                            dragOffset = {world.x - view->position.x, world.y - view->position.y};
                        } else if (boardEntity.has_value()) {
                            draggingEntity = true;
                            draggingEntityId = *boardEntity;
                        }
                    } else if (action.type == Engine::UI2D::ToolActionType::SelectStroke) {
                        workspace.SelectRelationship({});
                        if (toggle || additive) workspace.Selection().ToggleInk(action.targetId);
                        else workspace.Selection().SelectInk(action.targetId);
                        inspectorCustomProperty.clear();
                        inspectorDraftActive = false;
                    } else if (action.type == Engine::UI2D::ToolActionType::None &&
                               workspace.Tools().Tool() == Engine::UI2D::CanvasTool::Select &&
                               relationshipId.has_value()) {
                        workspace.SelectRelationship(*relationshipId);
                    } else if (action.type == Engine::UI2D::ToolActionType::BeginNode) {
                        const Crystal::ApiResult result = workspace.CreateNote("Current Alpha Target", world);
                        if (!result.ok) std::cerr << "Create note failed: " << result.error << '\n';
                    } else if (action.type == Engine::UI2D::ToolActionType::BeginStroke) {
                        workspace.Ink().BeginStroke(world, workspace.Ink().Width(), workspace.ActiveLayerId());
                } else if (action.type == Engine::UI2D::ToolActionType::EraseStroke) {
                        (void)workspace.DeleteStroke(action.targetId);
                    }
                } else {
                    if (boxSelecting) {
                        ApplyBoxSelection(workspace, boxStart, position,
                            HasModifier(button->modifiers, Engine::InputModifiers::Shift) ||
                            HasModifier(button->modifiers, Engine::InputModifiers::Control));
                        workspace.EndSelectionRectangle();
                        boxSelecting = false;
                        havePreviousPointer = false;
                        continue;
                    }
                    if (draggingBoard || draggingWidget) {
                        (void)workspace.PersistVisualSelection();
                        draggingBoard = false;
                        resizingBoard = false;
                        draggingBoardId.clear();
                        draggingWidget = false;
                        resizingWidget = false;
                        draggingWidgetId.clear();
                        workspace.Tools().PointerUp(position);
                        havePreviousPointer = false;
                        continue;
                    }
                    if (draggingEntity) {
                        if (workspace.Projects().CurrentSpace().Current().viewType == Crystal::SpaceViewType::TaskBoard) {
                            const float boardWidth = logicalWidth >= 1050 ? static_cast<float>(logicalWidth - 300) : static_cast<float>(logicalWidth);
                            const std::size_t column = std::min<std::size_t>(6, static_cast<std::size_t>(position.x / (boardWidth / 7.0f)));
                            const Crystal::ApiResult result = workspace.UpdateTaskStatus(
                                draggingEntityId, TaskStatusForColumn(column));
                            if (!result.ok) std::cerr << "Task status update failed: " << result.error << '\n';
                        } else if (EntityForView(workspace, draggingEntityId) != nullptr) {
                            (void)workspace.PersistVisualSelection();
                            const Crystal::BoardModel* droppedBoard = HitBoard(workspace, position);
                            for (const auto& view : workspace.Projects().Workspace().views) {
                                if (view.entityId != draggingEntityId || view.spaceId !=
                                        workspace.Projects().CurrentSpace().Current().id)
                                    continue;
                                if (droppedBoard != nullptr && view.boardId != droppedBoard->id) {
                                    const Engine::Vec2F worldPosition = workspace.Projects().CurrentSpace().Canvas().ScreenToWorld(position);
                                    const Crystal::ApiResult result = workspace.AttachViewToBoard(droppedBoard->id, view.id,
                                        {worldPosition.x - droppedBoard->position.x,
                                         worldPosition.y - droppedBoard->position.y});
                                    if (!result.ok) workspace.SetLocalNotification(
                                        result.error == "board_locked" ? "BOARD LOCKED" : "ATTACH FAILED");
                                } else if (droppedBoard == nullptr && !view.boardId.empty()) {
                                    const Crystal::ApiResult result = workspace.DetachViewFromBoard(view.boardId, view.id);
                                    if (!result.ok) workspace.SetLocalNotification("DETACH FAILED");
                                }
                                break;
                            }
                        }
                    }
                    Engine::UI2D::InkStroke completed;
                    if (workspace.Ink().EndStroke(&completed)) {
                        (void)workspace.CreateStroke(completed);
                    }
                    workspace.Tools().PointerUp(position);
                    draggingEntity = false;
                    draggingEntityId.clear();
                    workspace.ClearBoardHover();
                    havePreviousPointer = false;
                }
            } else if (const auto* move = std::get_if<Engine::PointerMoveEvent>(&event)) {
                const Engine::Vec2F position = move->logicalPosition;
                if (boxSelecting) {
                    workspace.UpdateSelectionRectangle(position);
                    previousPointer = position;
                    havePreviousPointer = true;
                    continue;
                }
                if ((draggingBoard || draggingWidget) && havePreviousPointer) {
                    const Engine::Vec2F previousWorld = workspace.Projects().CurrentSpace().Canvas().ScreenToWorld(previousPointer);
                    const Engine::Vec2F currentWorld = workspace.Projects().CurrentSpace().Canvas().ScreenToWorld(position);
                    const Engine::Vec2F delta{currentWorld.x - previousWorld.x, currentWorld.y - previousWorld.y};
                    if (draggingBoard && resizingBoard) {
                        for (const auto& board : workspace.Boards()) {
                            if (board.id == draggingBoardId) {
                                (void)workspace.ResizeSelectedBoard({board.size.x + delta.x, board.size.y + delta.y});
                                break;
                            }
                        }
                    } else if (draggingWidget && resizingWidget) {
                        for (const auto& widget : workspace.DashboardWidgets()) {
                            if (widget.id == draggingWidgetId) {
                                (void)workspace.ResizeSelectedWidget({widget.size.x + delta.x, widget.size.y + delta.y});
                                break;
                            }
                        }
                    } else {
                        (void)workspace.MoveSelectionLocal(delta);
                    }
                    previousPointer = position;
                    havePreviousPointer = true;
                    continue;
                }
                const Engine::UI2D::ToolAction action = workspace.Tools().PointerMove(position);
                if (draggingEntity && workspace.Tools().Tool() == Engine::UI2D::CanvasTool::Select) {
                    if (const auto* hoverBoard = HitBoard(workspace, position))
                        workspace.SetBoardHover(hoverBoard->id,
                            hoverBoard->locked || workspace.IsLayerLocked(hoverBoard->layerId));
                    else workspace.ClearBoardHover();
                }
                if (action.type == Engine::UI2D::ToolActionType::PanCanvas && havePreviousPointer) {
                    Crystal::CrystalProfiler::Scope profile(g_Profiler, "pan");
                    cameraPan = true;
                    workspace.Projects().CurrentSpace().Canvas().Pan({
                        position.x - previousPointer.x, position.y - previousPointer.y});
                } else if (action.type == Engine::UI2D::ToolActionType::AppendStroke) {
                    workspace.Ink().AppendPoint(workspace.Projects().CurrentSpace().Canvas().ScreenToWorld(position));
                } else if (draggingEntity && action.type == Engine::UI2D::ToolActionType::SelectEntity) {
                    if (workspace.Projects().CurrentSpace().Current().viewType != Crystal::SpaceViewType::TaskBoard) {
                        const Engine::Vec2F previousWorld = workspace.Projects().CurrentSpace().Canvas().ScreenToWorld(previousPointer);
                        const Engine::Vec2F currentWorld = workspace.Projects().CurrentSpace().Canvas().ScreenToWorld(position);
                        (void)workspace.MoveSelectionLocal({currentWorld.x - previousWorld.x,
                            currentWorld.y - previousWorld.y});
                    }
                } else if (action.type == Engine::UI2D::ToolActionType::EraseStroke) {
                    if (const auto strokeId = HitStroke(workspace, position)) {
                        (void)workspace.DeleteStroke(*strokeId);
                    }
                }
                previousPointer = position;
                havePreviousPointer = true;
            } else if (const auto* wheel = std::get_if<Engine::PointerWheelEvent>(&event)) {
                if (wheel->logicalPosition.y >= 58.0f) {
                    Crystal::CrystalProfiler::Scope profile(g_Profiler, "zoom");
                    workspace.Projects().CurrentSpace().Canvas().ZoomAt(
                        wheel->logicalPosition, std::pow(1.1, wheel->logicalDelta.y));
                }
            } else if (const auto* text = std::get_if<Engine::TextInputEvent>(&event)) {
                if (text->codepoint >= U' ') {
                    if (workspace.InlineEdit().active) {
                        workspace.SetInlineEditDraft(AppendCodepoint(
                            workspace.InlineEdit().draft, text->codepoint));
                    } else if (searchMode) {
                        (void)workspace.SetSearchQuery(AppendCodepoint(workspace.SearchQuery(), text->codepoint));
                    } else if (workspace.PropertyPicker().open) {
                        workspace.SetPropertyPickerFilter(AppendCodepoint(
                            workspace.PropertyPicker().filter, text->codepoint));
                    } else if (workspace.Panel() == Crystal::WorkspacePanel::QueryBuilder &&
                               !workspace.QueryDraft().filters.empty()) {
                        const auto& filter = workspace.QueryDraft().filters.back();
                        workspace.SetLastQueryFilterValue(Engine::Json::Value(AppendCodepoint(
                            InspectorDraftText(filter.value), text->codepoint)));
                    } else {
                        if (!inspectorCustomProperty.empty() && inspectorDraftActive)
                            inspectorDraft = AppendCodepoint(std::move(inspectorDraft), text->codepoint);
                        else if (!inspectorCustomProperty.empty())
                            (void)UpdateCustomInspectorText(workspace, inspectorCustomProperty, false, text->codepoint);
                        else
                            (void)UpdateInspectorText(workspace, inspectorField, false, text->codepoint);
                    }
                }
            } else if (const auto* key = std::get_if<Engine::KeyEvent>(&event); key != nullptr && key->pressed) {
                if (key->key == Engine::InputKeyCode::Escape) {
                    if (workspace.ContextMenu().open) {
                        workspace.CloseContextMenu();
                    } else if (workspace.PropertyPicker().open) {
                        workspace.ClosePropertyPicker();
                    } else if (workspace.InlineEdit().active) {
                        workspace.CancelInlineEdit();
                    } else if (workspace.Panel() != Crystal::WorkspacePanel::None) {
                        if (workspace.Panel() == Crystal::WorkspacePanel::QueryBuilder)
                            workspace.ToggleQueryBuilder();
                        else workspace.ToggleSchemaEditor();
                    } else if (searchMode) {
                        searchMode = false;
                        (void)workspace.SetSearchQuery({});
                    } else if (inspectorDraftActive) {
                        inspectorDraftActive = false;
                        inspectorDraft.clear();
                    } else {
                        connectSourceId.clear();
                        glfwSetWindowShouldClose(native, GLFW_TRUE);
                    }
                }
                else if (key->key == Engine::InputKeyCode::Space && !inspectorDraftActive)
                    workspace.CycleDisplayMode();
                else if (key->key == Engine::InputKeyCode::Tab) SwitchSpace(workspace);
                else if (workspace.Panel() == Crystal::WorkspacePanel::SchemaEditor &&
                         !HasModifier(key->modifiers, Engine::InputModifiers::Control) &&
                         key->key == Engine::InputKeyCode::N) {
                    workspace.BeginNewSchemaDraft();
                    workspace.SetLocalNotification("SCHEMA DRAFT: ENTER CREATE, A ADD PROPERTY, X DELETE");
                }
                else if (key->key == Engine::InputKeyCode::A &&
                         workspace.Panel() == Crystal::WorkspacePanel::SchemaEditor) {
                    workspace.AddSchemaDraftProperty();
                    workspace.SetLocalNotification("SCHEMA: property added to draft; ENTER SAVE");
                }
                else if (key->key == Engine::InputKeyCode::X &&
                         workspace.Panel() == Crystal::WorkspacePanel::SchemaEditor) {
                    if (workspace.SchemaDraft().id.empty()) workspace.SetLocalNotification("SCHEMA DRAFT CANCELLED");
                    else (void)workspace.DeleteSchema(workspace.SchemaDraft().id);
                }
                else if (key->key == Engine::InputKeyCode::E &&
                         workspace.Panel() == Crystal::WorkspacePanel::SchemaEditor &&
                         !workspace.SchemaDraft().id.empty())
                    workspace.BeginInlineEdit("Schema", workspace.SchemaDraft().id,
                        workspace.SchemaDraft().displayName);
                else if (key->key == Engine::InputKeyCode::A) SwitchProject(workspace);
                else if (!HasModifier(key->modifiers, Engine::InputModifiers::Control) &&
                         key->key == Engine::InputKeyCode::Q)
                    workspace.ToggleQueryBuilder();
                else if (!HasModifier(key->modifiers, Engine::InputModifiers::Control) &&
                         key->key == Engine::InputKeyCode::R)
                    workspace.ToggleSchemaEditor();
                else if (workspace.Panel() == Crystal::WorkspacePanel::QueryBuilder &&
                         key->key == Engine::InputKeyCode::I)
                    workspace.AddQueryFilter();
                else if (workspace.Panel() == Crystal::WorkspacePanel::QueryBuilder &&
                         key->key == Engine::InputKeyCode::O)
                    workspace.CycleLastQueryFilterOperator();
                else if (workspace.Panel() == Crystal::WorkspacePanel::QueryBuilder &&
                         key->key == Engine::InputKeyCode::U)
                    workspace.RemoveLastQueryFilter();
                else if (!HasModifier(key->modifiers, Engine::InputModifiers::Control) &&
                         key->key == Engine::InputKeyCode::S)
                    workspace.Tools().SetTool(Engine::UI2D::CanvasTool::Select);
                else if (!HasModifier(key->modifiers, Engine::InputModifiers::Control) &&
                         key->key == Engine::InputKeyCode::N)
                    workspace.Tools().SetTool(Engine::UI2D::CanvasTool::Node);
                else if (!HasModifier(key->modifiers, Engine::InputModifiers::Control) &&
                         key->key == Engine::InputKeyCode::P)
                    workspace.Tools().SetTool(Engine::UI2D::CanvasTool::Pen);
                else if (!HasModifier(key->modifiers, Engine::InputModifiers::Control) &&
                         key->key == Engine::InputKeyCode::E) {
                    std::string widgetId;
                    for (const auto& item : workspace.Selection().Items())
                        if (item.kind == Engine::UI2D::SelectionKind::DashboardWidget) {
                            widgetId = item.id;
                            break;
                        }
                    if (!widgetId.empty()) {
                        for (const auto& widget : workspace.DashboardWidgets())
                            if (widget.id == widgetId) {
                                workspace.BeginInlineEdit("Widget", widget.id,
                                    widget.title.empty() ? widget.widgetType : widget.title);
                                break;
                            }
                    } else workspace.Tools().SetTool(Engine::UI2D::CanvasTool::Eraser);
                }
                else if (key->key == Engine::InputKeyCode::C &&
                          HasModifier(key->modifiers, Engine::InputModifiers::Control))
                    workspace.CopyVisualSelection();
                else if (key->key == Engine::InputKeyCode::V &&
                         HasModifier(key->modifiers, Engine::InputModifiers::Control)) {
                    if (workspace.PasteVisualSelectionLocal()) (void)workspace.PersistVisualSelection();
                }
                else if (key->key == Engine::InputKeyCode::P &&
                         HasModifier(key->modifiers, Engine::InputModifiers::Control) &&
                         HasModifier(key->modifiers, Engine::InputModifiers::Shift))
                    workspace.LoadSyntheticPerformanceFixtureForDebug();
                else if (key->key == Engine::InputKeyCode::D &&
                         HasModifier(key->modifiers, Engine::InputModifiers::Control) &&
                         HasModifier(key->modifiers, Engine::InputModifiers::Shift)) {
#if defined(_DEBUG)
                    workspace.ForceRealtimeDisconnectForDebug();
#endif
                }
                else if (key->key == Engine::InputKeyCode::D &&
                          HasModifier(key->modifiers, Engine::InputModifiers::Control))
                    (void)workspace.DuplicateVisualSelection();
                else if (key->key == Engine::InputKeyCode::F)
                    (void)workspace.FocusSelection();
                else if (key->key == Engine::InputKeyCode::Home)
                    (void)workspace.FitContent();
                else if (key->key == Engine::InputKeyCode::L) {
                    for (const auto& candidate : workspace.Layers()) {
                        if (candidate.id != workspace.ActiveLayerId()) continue;
                        auto layer = candidate;
                        layer.locked = !layer.locked;
                        (void)workspace.UpdateLayer(std::move(layer));
                        break;
                    }
                }
                else if (key->key == Engine::InputKeyCode::C &&
                         workspace.Tools().Tool() == Engine::UI2D::CanvasTool::Connect &&
                         !connectSourceId.empty()) {
                    constexpr std::array<Crystal::RelationshipType, 7> types = {
                        Crystal::RelationshipType::RelatedTo,
                        Crystal::RelationshipType::DependsOn,
                        Crystal::RelationshipType::Blocks,
                        Crystal::RelationshipType::BelongsToSystem,
                        Crystal::RelationshipType::AssignedTo,
                        Crystal::RelationshipType::BelongsToMilestone,
                        Crystal::RelationshipType::IntegratesInto};
                    const auto current = std::find(types.begin(), types.end(), connectType);
                    connectType = current == types.end() || current + 1 == types.end() ? types.front() : *(current + 1);
                }
                else if (key->key == Engine::InputKeyCode::Enter && workspace.InlineEdit().active)
                    (void)workspace.CommitInlineEdit();
                else if (key->key == Engine::InputKeyCode::Enter && inspectorDraftActive) {
                    const std::string entityId = workspace.Selection().SelectedId();
                    if (const auto* selected = EntityForView(workspace, entityId)) {
                        if (const auto* definition = CustomDefinitionFor(
                                workspace, *selected, inspectorCustomProperty)) {
                            std::vector<std::string> entityIds;
                            for (const auto& entity : workspace.Projects().Workspace().entities)
                                entityIds.push_back(entity.id);
                            const Crystal::PropertyEditResult parsed = Crystal::PropertyEditor::Parse(
                                *definition, inspectorDraft, entityIds);
                            if (parsed.valid) {
                                (void)workspace.UpdateCustomProperty(entityId, inspectorCustomProperty, parsed.value);
                                inspectorDraftActive = false;
                            } else {
                                workspace.SetLocalNotification("PROPERTY: " + parsed.error);
                            }
                        }
                    }
                }
                else if (key->key == Engine::InputKeyCode::Enter &&
                         workspace.Panel() == Crystal::WorkspacePanel::QueryBuilder)
                    (void)workspace.SaveQueryDraft();
                else if (key->key == Engine::InputKeyCode::Enter &&
                         workspace.Panel() == Crystal::WorkspacePanel::SchemaEditor)
                    (void)workspace.SaveSchemaDraft();
                else if (key->key == Engine::InputKeyCode::Enter) {
                    const std::string selected = workspace.Selection().SelectedId();
                    if (auto* scene = EntityForView(workspace, selected); scene != nullptr && scene->type == Crystal::EntityType::Scene) {
                        const std::string currentStatus = workspace.CurrentSceneStatus(scene->id);
                        std::string workerId;
                        std::string workerName = "Crystal Worker";
                        std::string workerBranch = "crystal/phase2";
                        std::string taskId;
                        std::string taskTitle = "Scene work";
                        for (const auto& entity : workspace.Projects().Workspace().entities) {
                            if (workerId.empty() && entity.type == Crystal::EntityType::Worker) {
                                workerId = entity.id;
                                workerName = entity.title;
                                if (const auto* details = std::get_if<Crystal::WorkerDetails>(&entity.details)) {
                                    if (!details->workerId.empty()) workerId = details->workerId;
                                    workerBranch = details->branch;
                                }
                            }
                            if (taskId.empty() && entity.type == Crystal::EntityType::Task) {
                                taskId = entity.id;
                                taskTitle = entity.title;
                            }
                        }
                        if (currentStatus == "FREE" && !workerId.empty())
                            (void)workspace.Acquire(scene->id, workerId, workerName, taskTitle, workerBranch, taskId, taskTitle);
                        else if (currentStatus != "FREE")
                            (void)workspace.Release(scene->id, workerId);
                    }
                }
                else if (key->key == Engine::InputKeyCode::V) {
                    const std::string selected = workspace.Selection().SelectedId();
                    if (!selected.empty()) {
                        bool exists = false;
                        for (const auto& view : workspace.Projects().Workspace().views)
                            if (view.entityId == selected && view.spaceId == workspace.Projects().CurrentSpace().Current().id) exists = true;
                        if (!exists) {
                            Crystal::EntityView view;
                            view.entityId = selected;
                            view.spaceId = workspace.Projects().CurrentSpace().Current().id;
                            view.id = "view:" + selected + ":" + view.spaceId;
                            view.position = workspace.Projects().CurrentSpace().Canvas().ScreenToWorld({180, 180});
                            view.revision = 0;
                            (void)workspace.SaveView(view);
                        }
                    }
                }
                else if (key->key == Engine::InputKeyCode::X) {
                    const std::string selected = workspace.Selection().SelectedId();
                    if (!selected.empty() && !workspace.Ink().Strokes().empty()) {
                        const auto& stroke = workspace.Ink().Strokes().back();
                        (void)workspace.AttachInkToEntity(stroke.id, selected);
                    }
                }
                else if (key->key == Engine::InputKeyCode::Backspace) {
                    if (workspace.InlineEdit().active) {
                        std::string draft = workspace.InlineEdit().draft;
                        if (!draft.empty()) workspace.SetInlineEditDraft(PopCodepoint(std::move(draft)));
                    } else if (workspace.Panel() == Crystal::WorkspacePanel::SchemaEditor) {
                        workspace.RemoveSchemaDraftProperty();
                    } else if (searchMode) {
                        std::string query = workspace.SearchQuery();
                        if (!query.empty()) query.pop_back();
                        (void)workspace.SetSearchQuery(std::move(query));
                    } else if (workspace.PropertyPicker().open) {
                        std::string filter = workspace.PropertyPicker().filter;
                        if (!filter.empty()) filter.pop_back();
                        workspace.SetPropertyPickerFilter(std::move(filter));
                    } else if (workspace.Panel() == Crystal::WorkspacePanel::QueryBuilder &&
                               !workspace.QueryDraft().filters.empty()) {
                        const auto& filter = workspace.QueryDraft().filters.back();
                        std::string value = InspectorDraftText(filter.value);
                        if (!value.empty()) value = PopCodepoint(std::move(value));
                        workspace.SetLastQueryFilterValue(Engine::Json::Value(std::move(value)));
                    } else if (!inspectorCustomProperty.empty() && inspectorDraftActive)
                        inspectorDraft = PopCodepoint(std::move(inspectorDraft));
                    else if (!inspectorCustomProperty.empty())
                        (void)UpdateCustomInspectorText(workspace, inspectorCustomProperty, true);
                    else (void)UpdateInspectorText(workspace, inspectorField, true);
                } else if (key->key == Engine::InputKeyCode::Delete) {
                    if (!workspace.SelectedRelationshipId().empty())
                        (void)workspace.DeleteRelationship(workspace.SelectedRelationshipId());
                    else if (HasModifier(key->modifiers, Engine::InputModifiers::Shift)) {
                        if (const std::string selected = workspace.Selection().SelectedId(); !selected.empty())
                            (void)workspace.DeleteEntity(selected);
                    } else (void)workspace.DeleteVisualSelection();
                }
                else if (key->key == Engine::InputKeyCode::Z &&
                         HasModifier(key->modifiers, Engine::InputModifiers::Control)) {
                    if (workspace.UndoVisualChange()) (void)workspace.PersistVisualSelection();
                }
                else if (key->key == Engine::InputKeyCode::Y &&
                         HasModifier(key->modifiers, Engine::InputModifiers::Control)) {
                    if (workspace.RedoVisualChange()) (void)workspace.PersistVisualSelection();
                }
                else if (key->key == Engine::InputKeyCode::B &&
                         !HasModifier(key->modifiers, Engine::InputModifiers::Control))
                    (void)workspace.ToggleSelectedBoardCollapsed();
            }
        }
        }
        const bool cameraInput = cameraOnly || (cameraPan && pointerMoveOnly);
        const bool inputRequestsFrame = !events.empty() &&
            (!pointerMoveOnly || inputWasInteractive || cameraPan);
        if (inputRequestsFrame)
            requestFrame(cameraInput ? Engine::FrameReason::Camera : Engine::FrameReason::Input);
        if (inputRequestsFrame && !cameraInput && (!pointerMoveOnly || draggingEntity || draggingBoard ||
                draggingWidget || boxSelecting || workspace.Ink().Drawing()))
            renderer.InvalidateGeometryCache();
        frameWindow.framePolicy =
            (frameWindow.focused && (inputWasInteractive || cameraOnly || cameraPan ||
             draggingEntity || draggingBoard || draggingWidget || boxSelecting ||
             workspace.Ink().Drawing() || workspace.Tools().Captured()))
            ? Engine::FramePolicy::Active : Engine::FramePolicy::Static;
        const double nowSeconds = std::chrono::duration<double>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        const Engine::FrameDecision decision = frameScheduler.Evaluate(
            frameWindow, false, false, nowSeconds);
        if (decision.render && frameWindow.visible && !frameWindow.minimized) {
            {
                Crystal::CrystalProfiler::Scope profile(g_Profiler, "RenderBuild");
                renderer.Render(workspace);
            }
            profiler.RecordFrameRendered(Engine::FrameReasonName(decision.reason));
            profiler.SetWorkspaceCounts(
                workspace.Projects().Workspace().views.size(),
                workspace.Relationships().size(), workspace.Ink().Strokes().size(),
                workspace.Boards().size(), workspace.DashboardWidgets().size());
            {
                Crystal::CrystalProfiler::Scope profile(g_Profiler, "Present");
                surface.Present();
            }
            profiler.RecordPresent();
            profiler.SetRendererStats(
                renderer.DrawCalls(), renderer.Vertices(), renderer.UploadedBytes(),
                renderer.GeometryBuildCount(), renderer.ObjectCacheHits(),
                renderer.ObjectCacheMisses(), renderer.TextLayoutBuildCount(),
                renderer.LastGpuDrawMilliseconds(), renderer.GpuTimerSamples());
            frameWindow.dirty = false;
            frameScheduler.AcknowledgeFrame(window, nowSeconds);
        } else if (decision.requested) {
            profiler.RecordFrameSkipped(Engine::FrameReasonName(decision.reason));
        }
        waitForEvents = !decision.render;
        frameWaitSeconds = decision.waitSeconds;
        }
        if (waitForEvents) {
            Crystal::CrystalProfiler::Scope profile(g_Profiler, "IdleWait");
            if (frameWaitSeconds > 0.0) host.WaitForEventsTimeout(frameWaitSeconds);
            else host.WaitForEventsTimeout(1.0);
        }
    }
    profiler.WriteSummary();
    g_Profiler = nullptr;
    windows.SetInputEventQueue(window, nullptr);
    windows.DestroyWindow(window);
    return 0;
}
