from __future__ import annotations

from fastapi.testclient import TestClient

def test_phase5_schema_and_property_crud_round_trip(app):
    with TestClient(app) as client:
        created = client.post("/api/v1/projects/P_HighSchool/schemas", json={
            "name": "Phase5Asset",
            "display_name": "Phase 5 Asset",
            "properties": [{
                "name": "status", "display_name": "Status", "property_type": "Enum",
                "options": ["Open", "Done"], "sort_order": 0,
            }],
        })
        assert created.status_code == 201, created.text
        schema = created.json()
        assert schema["properties"][0]["property_type"] == "Enum"

        renamed = client.patch(f"/api/v1/schemas/{schema['id']}", json={
            "display_name": "Phase 5 Asset Renamed",
        })
        assert renamed.status_code == 200
        extra = client.post(f"/api/v1/schemas/{schema['id']}/properties", json={
            "name": "asset_path", "display_name": "Asset Path", "property_type": "URL",
        })
        assert extra.status_code == 201, extra.text
        property_id = extra.json()["id"]
        assert client.patch(f"/api/v1/property-definitions/{property_id}", json={
            "display_name": "Asset File", "required": True,
        }).status_code == 200
        assert client.delete(f"/api/v1/property-definitions/{property_id}").status_code == 204
        assert client.delete(f"/api/v1/schemas/{schema['id']}").status_code == 204


def test_phase5_layer_rename_migrates_visual_references_and_delete_preserves_entities(app):
    with TestClient(app) as client:
        space_id = "P_HighSchool:Overview"
        layer_response = client.post("/api/v1/projects/P_HighSchool/layers", json={
            "name": "Phase5 Layer", "space_id": space_id,
        })
        assert layer_response.status_code == 201, layer_response.text
        layer_id = layer_response.json()["id"]
        entity_response = client.post("/api/v1/entities", json={
            "project_id": "P_HighSchool", "entity_type": "note", "title": "Phase5 Layer Card",
        })
        assert entity_response.status_code == 201
        entity_id = entity_response.json()["id"]
        view_response = client.put(f"/api/v1/entity-views/{entity_id}/{space_id}", json={
            "x": 20, "y": 30, "layer_id": layer_id,
        })
        assert view_response.status_code == 200, view_response.text
        board_response = client.post("/api/v1/projects/P_HighSchool/boards", json={
            "space_id": space_id, "title": "Phase5 Layer Board", "layer_id": layer_id,
        })
        assert board_response.status_code == 201, board_response.text
        widget_response = client.post("/api/v1/projects/P_HighSchool/dashboard-widgets", json={
            "space_id": space_id, "widget_type": "Text", "title": "Phase5 Layer Widget",
            "layer_id": layer_id,
        })
        assert widget_response.status_code == 201, widget_response.text

        renamed = client.patch(f"/api/v1/layers/{layer_id}", json={"name": "Phase5 Layer Renamed"})
        assert renamed.status_code == 200, renamed.text
        workspace = client.get(
            f"/api/v1/projects/P_HighSchool/workspace?space_id={space_id}"
        ).json()
        view = next(item for item in workspace["views"] if item["entity_id"] == entity_id)
        board = next(item for item in workspace["boards"] if item["id"] == board_response.json()["id"])
        widget = next(item for item in workspace["dashboard_widgets"]
                       if item["id"] == widget_response.json()["id"])
        assert view["layer_id"] == "Phase5 Layer Renamed"
        assert board["layer_id"] == "Phase5 Layer Renamed"
        assert widget["layer_id"] == "Phase5 Layer Renamed"

        assert client.request("DELETE", f"/api/v1/layers/{layer_id}",
                              json={"mode": "move_to_main"}).status_code == 204
        after_delete = client.get(
            f"/api/v1/projects/P_HighSchool/workspace?space_id={space_id}"
        ).json()
        assert any(item["id"] == entity_id for item in after_delete["entities"])
