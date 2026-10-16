from __future__ import annotations

import concurrent.futures
import sqlite3
import uuid
from pathlib import Path

from app.database import Database
from app.services.scene_locks import AlreadyLocked, SceneLockService, WrongOwner


def database_for_test(name: str) -> Database:
    return Database(Path(__file__).parent / f".{name}-{uuid.uuid4().hex}.db")


def test_scene_lock_service_concurrent_acquire_100_competitors():
    database = database_for_test("scene-lock-race")
    database.initialize()
    service = SceneLockService(database)

    def acquire(index: int):
        try:
            lock = service.acquire(
                "scene-main-school",
                worker_id=f"worker-{index}",
                worker_name=f"Worker {index}",
                task="race",
                branch="race",
            )
            return True, lock
        except AlreadyLocked as error:
            return False, error.lock

    with concurrent.futures.ThreadPoolExecutor(max_workers=32) as pool:
        results = list(pool.map(acquire, range(100)))
    successes = [result for result in results if result[0]]
    assert len(successes) == 1
    assert sum(1 for result in results if not result[0]) == 99
    assert service.get("scene-main-school")["owner_id"] == successes[0][1]["owner_id"]


def test_scene_lock_service_wrong_owner_release_is_rejected():
    database = database_for_test("scene-lock-owner")
    database.initialize()
    service = SceneLockService(database)
    service.acquire(
        "scene-main-school", worker_id="owner-a", worker_name="A",
        task="task", branch="branch",
    )
    try:
        service.release("scene-main-school", worker_id="owner-b")
    except WrongOwner:
        pass
    else:
        raise AssertionError("wrong owner release unexpectedly succeeded")
    assert service.get("scene-main-school")["status"] == "LOCKED"


def test_stale_lock_is_visible_but_remains_effectively_locked():
    database = database_for_test("scene-lock-stale")
    database.initialize()
    service = SceneLockService(database)
    service.acquire(
        "scene-main-school", worker_id="owner-a", worker_name="A",
        task="task", branch="branch",
    )
    with sqlite3.connect(database.path) as connection:
        connection.execute(
            "UPDATE scene_locks SET heartbeat_at = '2000-01-01T00:00:00Z' WHERE scene_id = ?",
            ("scene-main-school",),
        )
    assert service.get("scene-main-school")["status"] == "STALE"
    try:
        service.acquire(
            "scene-main-school", worker_id="owner-b", worker_name="B",
            task="other", branch="other",
        )
    except AlreadyLocked as error:
        assert error.lock["status"] == "STALE"
    else:
        raise AssertionError("STALE lock was incorrectly treated as FREE")
