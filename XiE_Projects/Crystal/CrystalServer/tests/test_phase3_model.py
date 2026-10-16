from __future__ import annotations

import json
import sqlite3
import uuid
from contextlib import contextmanager
from pathlib import Path

import pytest

from app.database import Database
from app.entities import DomainError, entity_to_dict, normalize_custom_properties


@contextmanager
def database_path(prefix: str):
    path = Path(__file__).parent / f".{prefix}-{uuid.uuid4().hex}.db"
    try:
        yield path
    finally:
        for suffix in ("", "-wal", "-shm"):
            Path(f"{path}{suffix}").unlink(missing_ok=True)


def test_v3_seed_contains_custom_schema_board_query_and_template_fixtures():
    with database_path("phase3-seed") as path:
        database = Database(path)
        database.initialize()
        with database.connect() as connection:
            assert connection.execute("SELECT version FROM schema_version").fetchone()[0] == 4
            assert connection.execute(
                "SELECT COUNT(*) FROM entity_schemas WHERE project_id = 'P_HighSchool'"
            ).fetchone()[0] == 2
            bug = connection.execute(
                "SELECT * FROM entities WHERE id = 'P_HighSchool:bug-crosswalk'"
            ).fetchone()
            payload = entity_to_dict(connection, bug)
            assert payload["entity_type"] == "Bug"
            assert payload["schema_id"] == "P_HighSchool:schema:Bug"
            assert payload["properties"]["severity"] == "High"
            assert connection.execute(
                "SELECT COUNT(*) FROM boards WHERE project_id = 'P_HighSchool'"
                ).fetchone()[0] == 3
            assert connection.execute(
                "SELECT COUNT(*) FROM query_views WHERE project_id = 'P_HighSchool'"
            ).fetchone()[0] == 2
            assert connection.execute(
                "SELECT COUNT(*) FROM dashboard_widgets WHERE project_id = 'P_HighSchool'"
            ).fetchone()[0] == 5
            assert connection.execute(
                "SELECT COUNT(*) FROM project_templates"
            ).fetchone()[0] >= 3
            assert connection.execute(
                "SELECT visible FROM relationships LIMIT 1"
            ).fetchone()[0] == 1


def test_custom_property_validation_supports_defaults_and_rejects_invalid_values():
    with database_path("phase3-properties") as path:
        database = Database(path)
        database.initialize()
        with database.connect() as connection:
            schema_id = "P_HighSchool:schema:Bug"
            values = normalize_custom_properties(
                connection, "P_HighSchool", schema_id,
                {"severity": "Critical", "status": "Open"},
            )
            assert values["severity"] == "Critical"
            assert values["priority"] == "Normal"
            with pytest.raises(DomainError) as error:
                normalize_custom_properties(
                    connection, "P_HighSchool", schema_id, {"severity": "Impossible"}
                )
            assert error.value.code == "invalid_property_value"
            with pytest.raises(DomainError) as error:
                normalize_custom_properties(
                    connection, "P_HighSchool", schema_id, {"unknown": "value"}
                )
            assert error.value.code == "unknown_property"


def test_v2_to_v3_additive_migration_preserves_phase2_rows_and_adds_visibility():
    with database_path("phase3-migration") as path:
        connection = sqlite3.connect(path)
        connection.executescript(
            """
            CREATE TABLE schema_version(version INTEGER NOT NULL);
            INSERT INTO schema_version VALUES (2);
            CREATE TABLE projects(id TEXT PRIMARY KEY, name TEXT NOT NULL, created_at TEXT NOT NULL);
            CREATE TABLE spaces(id TEXT PRIMARY KEY, project_id TEXT NOT NULL, name TEXT NOT NULL,
                camera_x REAL NOT NULL DEFAULT 0, camera_y REAL NOT NULL DEFAULT 0,
                zoom REAL NOT NULL DEFAULT 1, view_type TEXT NOT NULL DEFAULT 'Graph',
                created_at TEXT NOT NULL);
            CREATE TABLE entities(id TEXT PRIMARY KEY, project_id TEXT NOT NULL, entity_type TEXT NOT NULL,
                title TEXT NOT NULL, description TEXT NOT NULL DEFAULT '', data_json TEXT NOT NULL DEFAULT '{}',
                revision INTEGER NOT NULL DEFAULT 1, created_at TEXT NOT NULL, updated_at TEXT NOT NULL);
            CREATE TABLE entity_views(id TEXT PRIMARY KEY, entity_id TEXT NOT NULL, space_id TEXT NOT NULL,
                x REAL NOT NULL, y REAL NOT NULL, width REAL NOT NULL, height REAL NOT NULL,
                view_type TEXT NOT NULL DEFAULT 'Node', layer_id TEXT NOT NULL DEFAULT 'Main',
                visible INTEGER NOT NULL DEFAULT 1, revision INTEGER NOT NULL DEFAULT 1);
            CREATE TABLE ink_strokes(id TEXT PRIMARY KEY, project_id TEXT NOT NULL, space_id TEXT NOT NULL,
                points_json TEXT NOT NULL, width REAL NOT NULL, tool TEXT NOT NULL,
                layer_id TEXT NOT NULL, created_at TEXT NOT NULL);
            CREATE TABLE scene_locks(scene_id TEXT PRIMARY KEY, project_id TEXT NOT NULL,
                status TEXT NOT NULL, owner_id TEXT, owner_name TEXT, task TEXT, branch TEXT,
                acquired_at TEXT, heartbeat_at TEXT);
            CREATE TABLE user_layers(id TEXT PRIMARY KEY, project_id TEXT NOT NULL,
                name TEXT NOT NULL, visible INTEGER NOT NULL);
            CREATE TABLE relationships(id TEXT PRIMARY KEY, project_id TEXT NOT NULL,
                source_entity_id TEXT NOT NULL, target_entity_id TEXT NOT NULL,
                relationship_type TEXT NOT NULL, created_at TEXT NOT NULL,
                UNIQUE(project_id, source_entity_id, target_entity_id, relationship_type));
            """
        )
        connection.execute(
            "INSERT INTO projects VALUES ('Legacy', 'Legacy', '2026-08-10T00:00:00Z')"
        )
        connection.execute(
            "INSERT INTO spaces VALUES ('Legacy:Overview', 'Legacy', 'Overview', 0, 0, 1, 'Graph', '2026-08-10T00:00:00Z')"
        )
        connection.execute(
            "INSERT INTO entities VALUES ('legacy-task', 'Legacy', 'task', 'Legacy Task', '', ?, 1, ?, ?)"
            , (json.dumps({"status": "Ready"}), "2026-08-10T00:00:00Z", "2026-08-10T00:00:00Z")
        )
        connection.execute(
            "INSERT INTO relationships VALUES ('legacy-rel', 'Legacy', 'legacy-task', 'legacy-task', 'related_to', '2026-08-10T00:00:00Z')"
        )
        connection.commit()
        connection.close()

        Database(path).initialize()
        with Database(path).connect() as migrated:
            assert migrated.execute("SELECT version FROM schema_version").fetchone()[0] == 4
            assert migrated.execute("SELECT title FROM entities WHERE id = 'legacy-task'").fetchone()[0] == "Legacy Task"
            assert migrated.execute("SELECT visible FROM relationships WHERE id = 'legacy-rel'").fetchone()[0] == 1
            assert migrated.execute("SELECT COUNT(*) FROM entity_schemas").fetchone()[0] >= 4
