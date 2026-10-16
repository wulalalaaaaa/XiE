from __future__ import annotations

import json
import sqlite3
import uuid
from contextlib import contextmanager
from pathlib import Path

from app.database import Database
from app.entities import entity_to_dict


@contextmanager
def database_path(prefix: str):
    path = Path(__file__).parent / f".{prefix}-{uuid.uuid4().hex}.db"
    try:
        yield path
    finally:
        for suffix in ("", "-wal", "-shm"):
            Path(f"{path}{suffix}").unlink(missing_ok=True)


def test_phase2_seed_and_multi_view_fixture():
    with database_path("phase2") as path:
        database = Database(path)
        database.initialize()
        with database.connect() as connection:
            assert connection.execute("SELECT version FROM schema_version").fetchone()[0] == 4
            assert connection.execute(
                "SELECT view_type FROM spaces WHERE id = 'P_HighSchool:CurrentSprint'"
            ).fetchone()[0] == "TaskBoard"
            task_id = "P_HighSchool:task-npc-scene"
            assert connection.execute(
                "SELECT COUNT(*) FROM entities WHERE id = ?", (task_id,)
            ).fetchone()[0] == 1
            assert connection.execute(
                "SELECT COUNT(*) FROM entity_views WHERE entity_id = ?", (task_id,)
                ).fetchone()[0] == 3
            assert connection.execute(
                "SELECT COUNT(*) FROM task_details WHERE entity_id = ?", (task_id,)
            ).fetchone()[0] == 1
            task = entity_to_dict(connection, connection.execute(
                "SELECT * FROM entities WHERE id = ?", (task_id,)
            ).fetchone())
            assert task["details"]["status"] == "Ready"
            assert task["details"]["assigned_worker_id"] == "P_HighSchool:worker-codex-npc"


def test_phase1_database_migrates_without_data_loss():
    with database_path("phase1-migration") as path:
        now = "2026-08-10T00:00:00Z"
        connection = sqlite3.connect(path)
        connection.executescript(
            """
            CREATE TABLE projects(id TEXT PRIMARY KEY, name TEXT NOT NULL, created_at TEXT NOT NULL);
            CREATE TABLE spaces(id TEXT PRIMARY KEY, project_id TEXT NOT NULL, name TEXT NOT NULL,
                camera_x REAL NOT NULL DEFAULT 0, camera_y REAL NOT NULL DEFAULT 0,
                zoom REAL NOT NULL DEFAULT 1, created_at TEXT NOT NULL);
            CREATE TABLE entities(id TEXT PRIMARY KEY, project_id TEXT NOT NULL, entity_type TEXT NOT NULL,
                title TEXT NOT NULL, data_json TEXT NOT NULL, created_at TEXT NOT NULL, updated_at TEXT NOT NULL);
            CREATE TABLE entity_views(id TEXT PRIMARY KEY, entity_id TEXT NOT NULL, space_id TEXT NOT NULL,
                x REAL NOT NULL, y REAL NOT NULL, width REAL NOT NULL, height REAL NOT NULL,
                layer_id TEXT NOT NULL, visible INTEGER NOT NULL);
            CREATE TABLE ink_strokes(id TEXT PRIMARY KEY, project_id TEXT NOT NULL, space_id TEXT NOT NULL,
                points_json TEXT NOT NULL, width REAL NOT NULL, tool TEXT NOT NULL,
                layer_id TEXT NOT NULL, created_at TEXT NOT NULL);
            CREATE TABLE scene_locks(scene_id TEXT PRIMARY KEY, project_id TEXT NOT NULL,
                status TEXT NOT NULL, owner_id TEXT, owner_name TEXT, task TEXT, branch TEXT,
                acquired_at TEXT, heartbeat_at TEXT);
            CREATE TABLE user_layers(id TEXT PRIMARY KEY, project_id TEXT NOT NULL,
                name TEXT NOT NULL, visible INTEGER NOT NULL);
            """
        )
        connection.execute("INSERT INTO projects VALUES (?, ?, ?)", ("Legacy", "Legacy", now))
        connection.execute(
            "INSERT INTO spaces VALUES (?, ?, ?, 11, 22, 1.25, ?)",
            ("Legacy:Overview", "Legacy", "Overview", now),
        )
        connection.execute(
            "INSERT INTO entities VALUES (?, ?, ?, ?, ?, ?, ?)",
            (
                "legacy-scene", "Legacy", "scene", "Legacy Scene",
                json.dumps({"description": "kept", "asset_path": "legacy.scene"}), now, now,
            ),
        )
        connection.execute(
            "INSERT INTO entity_views VALUES (?, ?, ?, 4, 5, 200, 100, 'Main', 1)",
            ("legacy-view", "legacy-scene", "Legacy:Overview"),
        )
        connection.execute(
            "INSERT INTO scene_locks VALUES (?, ?, 'FREE', NULL, NULL, NULL, NULL, NULL, NULL)",
            ("legacy-scene", "Legacy"),
        )
        connection.commit()
        connection.close()
        Database(path).initialize()
        with Database(path).connect() as connection:
            assert connection.execute("SELECT version FROM schema_version").fetchone()[0] == 4
            scene = connection.execute("SELECT * FROM entities WHERE id = 'legacy-scene'").fetchone()
            assert scene["title"] == "Legacy Scene"
            assert scene["description"] == "kept"
            assert scene["revision"] == 1
            assert connection.execute(
                "SELECT COUNT(*) FROM scene_details WHERE entity_id = 'legacy-scene'"
            ).fetchone()[0] == 1
            assert connection.execute(
                "SELECT revision FROM entity_views WHERE id = 'legacy-view'"
            ).fetchone()[0] == 1
            assert connection.execute(
                "SELECT COUNT(*) FROM scene_locks WHERE scene_id = 'legacy-scene'"
            ).fetchone()[0] == 1


def test_relationship_unique_constraint_and_revision_columns():
    with database_path("relationships") as path:
        database = Database(path)
        database.initialize()
        with database.connect() as connection:
            connection.execute(
                """INSERT INTO relationships
                   (id, project_id, source_entity_id, target_entity_id, relationship_type, created_at)
                   VALUES (?, ?, ?, ?, ?, ?)""",
                (
                    "r1", "P_HighSchool", "P_HighSchool:task-npc-scene",
                    "P_HighSchool:system-npc", "related_to", "2026-08-10T00:00:00Z",
                ),
            )
            try:
                connection.execute(
                    """INSERT INTO relationships
                       (id, project_id, source_entity_id, target_entity_id, relationship_type, created_at)
                       VALUES (?, ?, ?, ?, ?, ?)""",
                    (
                        "r2", "P_HighSchool", "P_HighSchool:task-npc-scene",
                        "P_HighSchool:system-npc", "related_to", "2026-08-10T00:00:00Z",
                    ),
                )
            except sqlite3.IntegrityError:
                pass
            else:
                raise AssertionError("duplicate relationship unexpectedly succeeded")
            assert connection.execute(
                "SELECT revision FROM entities WHERE id = 'P_HighSchool:task-npc-scene'"
            ).fetchone()[0] == 1
