from __future__ import annotations

import concurrent.futures
import json
import uuid
from pathlib import Path

import pytest

fastapi = pytest.importorskip("fastapi")
from fastapi.testclient import TestClient

from app.database import Database
from app.main import create_app
from app.services.scene_locks import AlreadyLocked, SceneLockService, WrongOwner


@pytest.fixture()
def app():
    path = Path(__file__).parent / f".phase1-server-{uuid.uuid4().hex}.db"
    try:
        yield create_app(path)
    finally:
        for suffix in ("", "-wal", "-shm"):
            Path(f"{path}{suffix}").unlink(missing_ok=True)


def test_project_space_entity_and_stroke_round_trip(app):
    with TestClient(app) as client:
        projects = client.get("/api/v1/projects")
        assert projects.status_code == 200
        assert {item["id"] for item in projects.json()["projects"]} == {"P_HighSchool", "XiE"}

        created_space = client.post("/api/v1/projects/XiE/spaces", json={"name": "Notes"})
        assert created_space.status_code == 201
        space_id = created_space.json()["id"]

        entity = client.post("/api/v1/entities", json={
            "project_id": "XiE", "entity_type": "note", "title": "Alpha target",
            "data": {"priority": "MVP"},
        })
        assert entity.status_code == 201
        entity_id = entity.json()["id"]
        view = client.put(f"/api/v1/entity-views/{entity_id}/{space_id}", json={
            "x": 44, "y": 55, "width": 220, "height": 120,
        })
        assert view.status_code == 200

        stroke = client.post("/api/v1/ink-strokes", json={
            "project_id": "XiE", "space_id": space_id,
            "points": [{"x": 1, "y": 2}, {"x": 10, "y": 12}],
            "width": 3, "tool": "pen", "layer_id": "Notes",
        })
        assert stroke.status_code == 201

        workspace = client.get(f"/api/v1/projects/XiE/workspace?space_id={space_id}").json()
        assert any(item["id"] == entity_id and item["title"] == "Alpha target"
                   for item in workspace["entities"])
        assert any(item["entity_id"] == entity_id and item["x"] == 44
                   for item in workspace["views"])
        assert workspace["strokes"][0]["points"] == [{"x": 1.0, "y": 2.0}, {"x": 10.0, "y": 12.0}]


def test_scene_lock_acquire_duplicate_release_and_wrong_owner(app):
    with TestClient(app) as client:
        acquire = client.post("/api/v1/scene-locks/scene-main-school/acquire", json={
            "worker_id": "worker-a", "worker_name": "Codex-NPC",
            "task": "NPC Integration", "branch": "codex/crystal",
        })
        assert acquire.status_code == 200
        assert acquire.json()["status"] == "LOCKED"
        assert acquire.json()["owner_name"] == "Codex-NPC"

        duplicate = client.post("/api/v1/scene-locks/scene-main-school/acquire", json={
            "worker_id": "worker-b", "worker_name": "Other", "task": "Other",
            "branch": "other",
        })
        assert duplicate.status_code == 409
        assert duplicate.json()["detail"]["error"] == "already_locked"
        assert duplicate.json()["detail"]["lock"]["owner_id"] == "worker-a"

        wrong_release = client.post("/api/v1/scene-locks/scene-main-school/release", json={
            "worker_id": "worker-b",
        })
        assert wrong_release.status_code == 409
        release = client.post("/api/v1/scene-locks/scene-main-school/release", json={
            "worker_id": "worker-a",
        })
        assert release.status_code == 200
        assert release.json()["status"] == "FREE"


def test_concurrent_acquire_has_exactly_one_success(tmp_path):
    database = Database(tmp_path / "concurrency.db")
    database.initialize()
    service = SceneLockService(database)

    def attempt(index: int):
        try:
            return (index, True, service.acquire(
                "scene-main-school", worker_id=f"worker-{index}",
                worker_name=f"Worker {index}", task="race", branch="race"))
        except AlreadyLocked as error:
            return (index, False, error.lock)

    with concurrent.futures.ThreadPoolExecutor(max_workers=32) as executor:
        results = list(executor.map(attempt, range(100)))
    successes = [result for result in results if result[1]]
    assert len(successes) == 1
    assert sum(1 for result in results if not result[1]) == 99
    assert service.get("scene-main-school")["owner_id"] == successes[0][2]["owner_id"]


def test_websocket_receives_lock_broadcast(app):
    with TestClient(app) as client:
        with client.websocket_connect("/ws/projects/P_HighSchool") as websocket:
            ready = websocket.receive_json()
            assert ready["type"] == "realtime.ready"
            result = client.post("/api/v1/scene-locks/scene-main-school/acquire", json={
                "worker_id": "ws-worker", "worker_name": "WebSocket Worker",
                "task": "broadcast", "branch": "ws",
            })
            assert result.status_code == 200
            event = websocket.receive_json()
            assert event["type"] == "scene_lock.acquired"
            assert event["lock"]["owner_id"] == "ws-worker"
