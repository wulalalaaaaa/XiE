from __future__ import annotations

import uuid
from pathlib import Path

import pytest

pytest.importorskip("fastapi")
from fastapi.testclient import TestClient

from app.main import create_app


@pytest.fixture()
def app():
    path = Path(__file__).parent / f".phase3-server-{uuid.uuid4().hex}.db"
    try:
        yield create_app(path)
    finally:
        for suffix in ("", "-wal", "-shm"):
            Path(f"{path}{suffix}").unlink(missing_ok=True)


def test_custom_entity_schema_multi_view_board_layer_query_dashboard_and_ink(app):
    with TestClient(app) as client:
        schema = client.post("/api/v1/projects/P_HighSchool/schemas", json={
            "name": "Phase3Card", "display_name": "Phase 3 Card", "properties": [
                {"name": "status", "display_name": "Status", "type": "Enum",
                 "default_value": "Open", "options": ["Open", "Closed"]},
                {"name": "tags", "display_name": "Tags", "type": "Tags"},
            ],
        })
        assert schema.status_code == 201, schema.text
        schema_payload = schema.json()
        assert {item["property_type"] for item in schema_payload["properties"]} == {"Enum", "Tags"}

        entity = client.post("/api/v1/entities", json={
            "project_id": "P_HighSchool", "entity_type": "Phase3Card",
            "schema_id": schema_payload["id"], "title": "Generic Card",
            "properties": {"status": "Open", "tags": ["phase3", "mvp"]},
        })
        assert entity.status_code == 201, entity.text
        entity_payload = entity.json()
        assert entity_payload["entity_type"] == "Phase3Card"
        assert entity_payload["properties"]["tags"] == ["phase3", "mvp"]

        updated = client.patch(f"/api/v1/entities/{entity_payload['id']}", json={
            "expected_revision": 1, "properties": {"status": "Closed"},
        })
        assert updated.status_code == 200, updated.text
        assert updated.json()["properties"]["status"] == "Closed"

        space = client.post("/api/v1/projects/P_HighSchool/spaces", json={
            "name": "Phase 3 Board Space", "view_type": "Graph",
        }).json()
        space_id = space["id"]
        view_a = client.put(f"/api/v1/entity-views/{entity_payload['id']}/{space_id}", json={
            "x": 25, "y": 35, "view_type": "StatusCard", "visible_properties": ["status", "tags"],
        })
        assert view_a.status_code == 200, view_a.text
        view_id = view_a.json()["id"]
        view_b = client.put(f"/api/v1/entity-views/{entity_payload['id']}/P_HighSchool:Overview", json={
            "x": 100, "y": 110, "view_type": "Badge", "visible_properties": ["status"],
        })
        assert view_b.status_code == 200, view_b.text

        board = client.post("/api/v1/projects/P_HighSchool/boards", json={
            "space_id": space_id, "title": "Phase 3 Board", "x": 100, "y": 200,
        })
        assert board.status_code == 201, board.text
        board_id = board.json()["id"]
        attached = client.put(f"/api/v1/boards/{board_id}/views/{view_id}", json={
            "local_x": 15, "local_y": 25,
        })
        assert attached.status_code == 200, attached.text
        moved_board = client.patch(f"/api/v1/boards/{board_id}", json={"x": 300, "y": 400})
        assert moved_board.status_code == 200, moved_board.text
        workspace = client.get(
            f"/api/v1/projects/P_HighSchool/workspace?space_id={space_id}"
        ).json()
        moved_view = next(view for view in workspace["views"] if view["id"] == view_id)
        assert moved_view["x"] == 315 and moved_view["y"] == 425
        assert next(item for item in workspace["entities"] if item["id"] == entity_payload["id"])["title"] == "Generic Card"

        layer = client.post("/api/v1/projects/P_HighSchool/layers", json={
            "name": "Phase3 Hidden", "space_id": space_id,
        })
        assert layer.status_code == 201, layer.text
        layer_id = layer.json()["id"]
        assert client.patch(f"/api/v1/layers/{layer_id}", json={"visible": False}).status_code == 200
        assert client.get(f"/api/v1/projects/P_HighSchool/workspace?space_id={space_id}").json()["entities"]
        assert client.patch(f"/api/v1/layers/{layer_id}", json={"name": "Phase3 Renamed"}).status_code == 200

        query = client.post("/api/v1/projects/P_HighSchool/query-views", json={
            "name": "Phase 3 Live Query", "entity_schema": "Phase3Card",
            "filters": [{"property": "status", "operator": "Equals", "value": "Closed"}],
        })
        assert query.status_code == 201, query.text
        query_payload = query.json()
        assert any(item["id"] == entity_payload["id"] for item in query_payload["results"])
        reopened = client.patch(f"/api/v1/entities/{entity_payload['id']}", json={
            "expected_revision": 2, "properties": {"status": "Open"},
        })
        assert reopened.status_code == 200, reopened.text
        live_query = client.get("/api/v1/projects/P_HighSchool/query-views").json()["query_views"]
        assert not any(item["id"] == entity_payload["id"] for item in next(
            item for item in live_query if item["id"] == query_payload["id"]
        )["results"])

        widget = client.post("/api/v1/projects/P_HighSchool/dashboard-widgets", json={
            "space_id": space_id, "widget_type": "EntityList", "title": "Phase 3 Results",
            "config": {"query_view_id": query_payload["id"]},
        })
        assert widget.status_code == 201, widget.text
        current_workspace = client.get(
            f"/api/v1/projects/P_HighSchool/workspace?space_id={space_id}"
        ).json()
        assert any(item["title"] == "Phase 3 Results" for item in current_workspace["dashboard_widgets"])

        stroke_payload = {
            "project_id": "P_HighSchool", "space_id": space_id,
            "points": [{"x": 10, "y": 10}, {"x": 30, "y": 30}], "shape": "arrow",
        }
        stroke_a = client.post("/api/v1/ink-strokes", json=stroke_payload)
        stroke_b = client.post("/api/v1/ink-strokes", json={**stroke_payload, "points": [{"x": 40, "y": 40}, {"x": 60, "y": 60}]})
        assert stroke_a.status_code == 201 and stroke_b.status_code == 201
        group = client.post("/api/v1/projects/P_HighSchool/ink-groups", json={
            "project_id": "P_HighSchool", "space_id": space_id,
            "stroke_ids": [stroke_a.json()["id"], stroke_b.json()["id"]], "name": "Arrow Group",
        })
        assert group.status_code == 201, group.text
        assert len(group.json()["stroke_ids"]) == 2
        assert client.patch(f"/api/v1/ink-strokes/{stroke_a.json()['id']}", json={
            "anchor_board_id": board_id, "local_offset_x": 5, "local_offset_y": 6,
        }).status_code == 200
        assert client.get(f"/api/v1/projects/P_HighSchool/ink-groups?space_id={space_id}").json()["ink_groups"]

        relationship = client.post("/api/v1/relationships", json={
            "project_id": "P_HighSchool", "source_entity_id": entity_payload["id"],
            "target_entity_id": "P_HighSchool:system-npc", "relationship_type": "related_to",
        })
        assert relationship.status_code == 201, relationship.text
        relationship_id = relationship.json()["id"]
        hidden_edge = client.patch(f"/api/v1/relationships/{relationship_id}", json={"visible": False})
        assert hidden_edge.status_code == 200 and hidden_edge.json()["visible"] is False

        duplicate = client.post(
            f"/api/v1/projects/P_HighSchool/spaces/{space_id}/duplicate",
            json={"name": "Phase 3 Board Copy"},
        )
        assert duplicate.status_code == 201, duplicate.text
        duplicate_id = duplicate.json()["id"]
        assert client.delete(f"/api/v1/projects/P_HighSchool/spaces/{duplicate_id}").status_code == 204

        templates = client.get("/api/v1/project-templates")
        assert templates.status_code == 200 and len(templates.json()["templates"]) >= 3
        applied = client.post("/api/v1/projects/XiE/templates/apply", json={
            "template_id": "template:game-development",
        })
        assert applied.status_code == 200, applied.text
        assert applied.json()["workspace"]["project"]["id"] == "XiE"
        saved_template = client.post("/api/v1/projects/XiE/project-templates", json={
            "name": "Phase 3 Saved Template", "template_type": "Demo",
        })
        assert saved_template.status_code == 201, saved_template.text
        assert saved_template.json()["config"]["spaces"]

        activities = client.get("/api/v1/projects/P_HighSchool/activities").json()["activities"]
        assert any(item["event_type"] == "Entity Created" for item in activities)
