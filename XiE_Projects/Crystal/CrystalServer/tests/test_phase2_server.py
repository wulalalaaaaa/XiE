from __future__ import annotations

import pytest
import uuid
from pathlib import Path

pytest.importorskip("fastapi")
from fastapi.testclient import TestClient

from app.main import create_app


@pytest.fixture()
def app():
    path = Path(__file__).parent / f".phase2-server-{uuid.uuid4().hex}.db"
    try:
        yield create_app(path)
    finally:
        for suffix in ("", "-wal", "-shm"):
            Path(f"{path}{suffix}").unlink(missing_ok=True)


def test_entity_types_revision_conflict_and_multiview(app):
    with TestClient(app) as client:
        worker = client.post("/api/v1/entities", json={
            "project_id": "P_HighSchool", "entity_type": "worker", "title": "Phase2 Worker",
            "details": {"worker_id": "phase2-worker", "worker_type": "Codex", "status": "Idle"},
        }).json()
        task = client.post("/api/v1/entities", json={
            "project_id": "P_HighSchool", "entity_type": "task", "title": "Phase2 Task",
            "description": "Revision test", "details": {
                "status": "Ready", "priority": "High", "assigned_worker_id": worker["id"],
                "milestone_id": "P_HighSchool:milestone-alpha",
                "system_id": "P_HighSchool:system-npc",
            },
        }).json()
        task_id = task["id"]
        current = client.patch(f"/api/v1/entities/{task_id}", json={
            "expected_revision": 1, "details": {"status": "Testing"},
        })
        assert current.status_code == 200
        assert current.json()["revision"] == 2
        conflict = client.patch(f"/api/v1/entities/{task_id}", json={
            "expected_revision": 1, "details": {"status": "Done"},
        })
        assert conflict.status_code == 409
        assert conflict.json()["detail"]["error"] == "revision_conflict"
        assert conflict.json()["detail"]["expected_revision"] == 1
        assert conflict.json()["detail"]["current_entity"]["revision"] == 2
        assert conflict.json()["detail"]["current_entity"]["details"]["status"] == "Testing"

        sprint = client.post("/api/v1/projects/P_HighSchool/spaces", json={
            "name": "Phase 2 Sprint", "view_type": "TaskBoard",
        }).json()["id"]
        for space_id in ("P_HighSchool:Overview", sprint, "P_HighSchool:NPCArchitecture"):
            assert client.put(f"/api/v1/entity-views/{task_id}/{space_id}", json={
                "x": 100, "y": 120, "view_type": "CompactNode" if space_id == sprint else "Node",
            }).status_code == 200
        workspace = client.get(f"/api/v1/projects/P_HighSchool/workspace?space_id={sprint}").json()
        assert sum(view["entity_id"] == task_id for view in workspace["views"]) == 1
        assert client.get(f"/api/v1/entities/{task_id}").json()["id"] == task_id
        assert client.delete(f"/api/v1/entity-views/{task_id}/{sprint}").status_code == 204
        assert client.get(f"/api/v1/entities/{task_id}").status_code == 200


def test_relationship_duplicate_cross_project_and_delete_view_preserves_entity(app):
    with TestClient(app) as client:
        source = "P_HighSchool:task-npc-scene"
        target = "P_HighSchool:system-npc"
        created = client.post("/api/v1/relationships", json={
            "project_id": "P_HighSchool", "source_entity_id": source,
            "target_entity_id": target, "relationship_type": "related_to",
        })
        assert created.status_code == 201
        duplicate = client.post("/api/v1/relationships", json={
            "project_id": "P_HighSchool", "source_entity_id": source,
            "target_entity_id": target, "relationship_type": "related_to",
        })
        assert duplicate.status_code == 409
        cross = client.post("/api/v1/relationships", json={
            "project_id": "P_HighSchool", "source_entity_id": source,
            "target_entity_id": "scene-xie", "relationship_type": "related_to",
        })
        assert cross.status_code == 409
        assert client.delete(f"/api/v1/relationships/{created.json()['id']}").status_code == 204


def test_scene_lock_task_worker_association_and_scene_delete_guard(app):
    with TestClient(app) as client:
        acquire = client.post("/api/v1/scene-locks/scene-main-school/acquire", json={
            "worker_id": "P_HighSchool:worker-codex-npc",
            "task_id": "P_HighSchool:task-npc-scene",
            "task_note": "NPC demo", "branch": "codex/npc-system",
        })
        assert acquire.status_code == 200
        assert acquire.json()["task_id"] == "P_HighSchool:task-npc-scene"
        assert client.delete("/api/v1/entities/scene-main-school").status_code == 409
        assert client.post("/api/v1/scene-locks/scene-main-school/release", json={
            "worker_id": "P_HighSchool:worker-codex-npc",
        }).status_code == 200


def test_websocket_incremental_entity_update(app):
    with TestClient(app) as client:
        with client.websocket_connect("/ws/projects/P_HighSchool") as websocket:
            assert websocket.receive_json()["type"] == "realtime.ready"
            update = client.patch("/api/v1/entities/P_HighSchool:task-npc-scene", json={
                "expected_revision": 1, "details": {"status": "Testing"},
            })
            assert update.status_code == 200
            event = websocket.receive_json()
            assert event["type"] == "entity.updated"
            assert event["entity"]["details"]["status"] == "Testing"


def test_two_websocket_clients_receive_entity_relationship_lock_and_view_events(app):
    def receive_type(websocket, event_type):
        for _ in range(8):
            event = websocket.receive_json()
            if event["type"] == event_type:
                return event
        raise AssertionError(f"did not receive {event_type}")

    def receive_entity(websocket, entity_id):
        for _ in range(12):
            event = websocket.receive_json()
            if event["type"] == "entity.updated" and event["entity"]["id"] == entity_id:
                return event
        raise AssertionError(f"did not receive entity update for {entity_id}")

    with TestClient(app) as client:
        with client.websocket_connect("/ws/projects/P_HighSchool") as socket_a, \
                client.websocket_connect("/ws/projects/P_HighSchool") as socket_b:
            assert socket_a.receive_json()["type"] == "realtime.ready"
            assert socket_b.receive_json()["type"] == "realtime.ready"

            task_id = "P_HighSchool:task-npc-scene"
            updated = client.patch(f"/api/v1/entities/{task_id}", json={
                "expected_revision": 1, "details": {"status": "Testing"},
            })
            assert updated.status_code == 200
            assert receive_entity(socket_a, task_id)["entity"]["revision"] == 2
            assert receive_entity(socket_b, task_id)["entity"]["details"]["status"] == "Testing"

            priority = client.patch(f"/api/v1/entities/{task_id}", json={
                "expected_revision": 2, "details": {"priority": "High"},
            })
            assert priority.status_code == 200
            assert receive_entity(socket_a, task_id)["entity"]["details"]["priority"] == "High"
            assert receive_entity(socket_b, task_id)["entity"]["revision"] == 3

            relationship = client.post("/api/v1/relationships", json={
                "project_id": "P_HighSchool", "source_entity_id": task_id,
                "target_entity_id": "P_HighSchool:task-story-validation",
                "relationship_type": "depends_on",
            })
            assert relationship.status_code == 201
            relationship_id = relationship.json()["id"]
            assert receive_type(socket_a, "relationship.created")["relationship"]["id"] == relationship_id
            assert receive_type(socket_b, "relationship.created")["relationship"]["id"] == relationship_id
            assert client.delete(f"/api/v1/relationships/{relationship_id}").status_code == 204
            assert receive_type(socket_a, "relationship.deleted")["relationship"]["id"] == relationship_id
            assert receive_type(socket_b, "relationship.deleted")["relationship"]["id"] == relationship_id

            space = client.post("/api/v1/projects/P_HighSchool/spaces", json={
                "name": "Phase 2.5 Two Client", "view_type": "Graph",
            })
            assert space.status_code == 201
            space_id = space.json()["id"]
            assert receive_type(socket_a, "space.created")["space"]["id"] == space_id
            assert receive_type(socket_b, "space.created")["space"]["id"] == space_id
            view = client.put(f"/api/v1/entity-views/{task_id}/{space_id}", json={
                "x": 222, "y": 333, "view_type": "Node",
            })
            assert view.status_code == 200
            view_id = view.json()["id"]
            assert receive_type(socket_a, "view.created")["view"]["id"] == view_id
            assert receive_type(socket_b, "view.created")["view"]["id"] == view_id

            acquire = client.post("/api/v1/scene-locks/scene-main-school/acquire", json={
                "worker_id": "phase25-a", "worker_name": "Phase 2.5 A",
                "task_id": task_id, "task_note": "two-client", "branch": "phase25",
            })
            assert acquire.status_code == 200
            assert receive_type(socket_a, "scene_lock.acquired")["lock"]["status"] == "LOCKED"
            assert receive_type(socket_b, "scene_lock.acquired")["lock"]["worker_id"] == "phase25-a"
            heartbeat = client.post("/api/v1/scene-locks/scene-main-school/heartbeat", json={
                "worker_id": "phase25-a",
            })
            assert heartbeat.status_code == 200
            assert receive_type(socket_a, "scene_lock.heartbeat")["lock"]["status"] == "LOCKED"
            assert receive_type(socket_b, "scene_lock.heartbeat")["lock"]["heartbeat_at"]
            duplicate = client.post("/api/v1/scene-locks/scene-main-school/acquire", json={
                "worker_id": "phase25-b", "worker_name": "Phase 2.5 B",
            })
            assert duplicate.status_code == 409
            release = client.post("/api/v1/scene-locks/scene-main-school/release", json={
                "worker_id": "phase25-a",
            })
            assert release.status_code == 200
            assert receive_type(socket_a, "scene_lock.released")["lock"]["status"] == "FREE"
            assert receive_type(socket_b, "scene_lock.released")["lock"]["status"] == "FREE"
