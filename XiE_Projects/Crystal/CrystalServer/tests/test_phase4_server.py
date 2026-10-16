from __future__ import annotations

from fastapi.testclient import TestClient


def test_number_constraints_and_schema_edit_safety(app):
    with TestClient(app) as client:
        schema_response = client.post("/api/v1/projects/P_HighSchool/schemas", json={
            "name": "Phase4Metrics", "display_name": "Phase 4 Metrics", "properties": [
                {"name": "progress", "display_name": "Progress", "type": "Number",
                 "default_value": 0, "min_value": 0, "max_value": 100, "step_value": 5},
                {"name": "status", "display_name": "Status", "type": "Enum",
                 "default_value": "Open", "options": ["Open", "Closed"]},
            ],
        })
        assert schema_response.status_code == 201, schema_response.text
        schema = schema_response.json()
        progress_id = next(item["id"] for item in schema["properties"] if item["name"] == "progress")
        status_id = next(item["id"] for item in schema["properties"] if item["name"] == "status")
        entity_response = client.post("/api/v1/entities", json={
            "project_id": "P_HighSchool", "entity_type": "Phase4Metrics",
            "schema_id": schema["id"], "title": "Constraint Card",
            "properties": {"progress": 20, "status": "Open"},
        })
        assert entity_response.status_code == 201, entity_response.text
        entity = entity_response.json()
        assert client.patch(f"/api/v1/entities/{entity['id']}", json={
            "expected_revision": 1, "properties": {"progress": 25},
        }).status_code == 200
        invalid_step = client.patch(f"/api/v1/entities/{entity['id']}", json={
            "expected_revision": 2, "properties": {"progress": 23},
        })
        assert invalid_step.status_code == 422
        invalid_type = client.patch(f"/api/v1/property-definitions/{progress_id}", json={
            "property_type": "Text",
        })
        assert invalid_type.status_code == 409
        assert invalid_type.json()["detail"]["error"] == "property_type_in_use"
        invalid_option = client.patch(f"/api/v1/property-definitions/{status_id}", json={
            "options": ["Closed"],
        })
        assert invalid_option.status_code == 409
        assert invalid_option.json()["detail"]["error"] == "property_option_in_use"


def test_layer_lock_blocks_all_visual_writes(app):
    with TestClient(app) as client:
        layer = client.post("/api/v1/projects/P_HighSchool/layers", json={
            "name": "Phase4 Locked", "space_id": "P_HighSchool:Overview", "locked": True,
        })
        assert layer.status_code == 201, layer.text
        layer_id = layer.json()["id"]
        view = client.put("/api/v1/entity-views/P_HighSchool:task-npc-scene/P_HighSchool:Overview", json={
            "x": 10, "y": 20, "layer_id": layer_id,
        })
        assert view.status_code == 409 and view.json()["detail"] == "layer_locked"
        board = client.post("/api/v1/projects/P_HighSchool/boards", json={
            "space_id": "P_HighSchool:Overview", "title": "Locked Board", "layer_id": layer_id,
        })
        assert board.status_code == 409 and board.json()["detail"] == "layer_locked"
        widget = client.post("/api/v1/projects/P_HighSchool/dashboard-widgets", json={
            "space_id": "P_HighSchool:Overview", "widget_type": "Text", "layer_id": layer_id,
        })
        assert widget.status_code == 409 and widget.json()["detail"] == "layer_locked"
        stroke = client.post("/api/v1/ink-strokes", json={
            "project_id": "P_HighSchool", "space_id": "P_HighSchool:Overview",
            "layer_id": layer_id, "points": [{"x": 1, "y": 1}, {"x": 3, "y": 3}],
        })
        assert stroke.status_code == 409 and stroke.json()["detail"] == "layer_locked"


def test_board_collapsed_and_full_stroke_patch_round_trip(app):
    with TestClient(app) as client:
        board_response = client.post("/api/v1/projects/P_HighSchool/boards", json={
            "space_id": "P_HighSchool:Overview", "title": "Phase 4 Board", "collapsed": True,
        })
        assert board_response.status_code == 201, board_response.text
        board = board_response.json()
        assert board["collapsed"] is True
        expanded = client.patch(f"/api/v1/boards/{board['id']}", json={"collapsed": False})
        assert expanded.status_code == 200 and expanded.json()["collapsed"] is False
        stroke_response = client.post("/api/v1/ink-strokes", json={
            "project_id": "P_HighSchool", "space_id": "P_HighSchool:Overview",
            "points": [{"x": 1, "y": 1}, {"x": 3, "y": 3}], "anchor_board_id": board["id"],
        })
        assert stroke_response.status_code == 201, stroke_response.text
        stroke_id = stroke_response.json()["id"]
        updated = client.patch(f"/api/v1/ink-strokes/{stroke_id}", json={
            "points": [{"x": 10, "y": 12}, {"x": 30, "y": 32}],
            "width": 6, "tool": "highlighter", "shape": "rectangle", "color": "#ffcc44",
        })
        assert updated.status_code == 200, updated.text
        assert updated.json()["points"][0] == {"x": 10, "y": 12}
        assert updated.json()["shape"] == "rectangle"
        assert updated.json()["width"] == 6


def test_view_clipboard_lifecycle_never_deletes_entity(app):
    with TestClient(app) as client:
        entity_response = client.post("/api/v1/entities", json={
            "project_id": "P_HighSchool", "entity_type": "note", "title": "View Copy Source",
        })
        assert entity_response.status_code == 201, entity_response.text
        entity_id = entity_response.json()["id"]
        space_id = "P_HighSchool:Overview"
        view_response = client.put(f"/api/v1/entity-views/{entity_id}/{space_id}", json={
            "x": 80, "y": 120,
        })
        assert view_response.status_code in (200, 201), view_response.text
        deleted = client.delete(f"/api/v1/entity-views/{entity_id}/{space_id}")
        assert deleted.status_code == 204
        remaining_entity = client.get(f"/api/v1/entities/{entity_id}")
        assert remaining_entity.status_code == 200
        assert remaining_entity.json()["title"] == "View Copy Source"
