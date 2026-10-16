from __future__ import annotations

import json
import sqlite3
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Iterable


SCHEMA_VERSION = 4

TASK_STATUSES = ("Backlog", "Ready", "InProgress", "Review", "Testing", "Done", "Blocked")
TASK_PRIORITIES = ("Low", "Normal", "High", "Critical")
SYSTEM_STATUSES = ("Planned", "Developing", "Testing", "Stable", "Deprecated")
WORKER_TYPES = ("Human", "Codex", "Automation")
WORKER_STATUSES = ("Offline", "Idle", "Working")
MILESTONE_STATUSES = ("Planned", "Active", "Completed", "Blocked")
RELATIONSHIP_TYPES = (
    "depends_on",
    "blocks",
    "belongs_to_system",
    "assigned_to",
    "belongs_to_milestone",
    "integrates_into",
    "related_to",
)

PROPERTY_TYPES = (
    "Text",
    "MultilineText",
    "Number",
    "Bool",
    "Enum",
    "Date",
    "EntityReference",
    "EntityReferenceList",
    "Tags",
    "Progress",
    "URL",
)


SCHEMA = """
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS schema_version (
    version INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS projects (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    created_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS spaces (
    id TEXT PRIMARY KEY,
    project_id TEXT NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
    name TEXT NOT NULL,
    camera_x REAL NOT NULL DEFAULT 0,
    camera_y REAL NOT NULL DEFAULT 0,
    zoom REAL NOT NULL DEFAULT 1,
    view_type TEXT NOT NULL DEFAULT 'Graph',
    sort_order INTEGER NOT NULL DEFAULT 0,
    created_at TEXT NOT NULL,
    UNIQUE(project_id, name)
);

CREATE TABLE IF NOT EXISTS entity_schemas (
    id TEXT PRIMARY KEY,
    project_id TEXT NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
    name TEXT NOT NULL,
    display_name TEXT NOT NULL,
    icon TEXT NOT NULL DEFAULT '',
    description TEXT NOT NULL DEFAULT '',
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL,
    UNIQUE(project_id, name)
);

CREATE TABLE IF NOT EXISTS property_definitions (
    id TEXT PRIMARY KEY,
    schema_id TEXT NOT NULL REFERENCES entity_schemas(id) ON DELETE CASCADE,
    name TEXT NOT NULL,
    display_name TEXT NOT NULL,
    property_type TEXT NOT NULL,
    default_value_json TEXT NOT NULL DEFAULT 'null',
    required INTEGER NOT NULL DEFAULT 0,
    options_json TEXT NOT NULL DEFAULT '[]',
    min_value REAL,
    max_value REAL,
    step_value REAL,
    sort_order INTEGER NOT NULL DEFAULT 0,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL,
    UNIQUE(schema_id, name)
);

CREATE TABLE IF NOT EXISTS entity_property_values (
    entity_id TEXT NOT NULL REFERENCES entities(id) ON DELETE CASCADE,
    property_definition_id TEXT NOT NULL REFERENCES property_definitions(id) ON DELETE CASCADE,
    value_json TEXT NOT NULL DEFAULT 'null',
    updated_at TEXT NOT NULL,
    PRIMARY KEY(entity_id, property_definition_id)
);

CREATE TABLE IF NOT EXISTS entities (
    id TEXT PRIMARY KEY,
    project_id TEXT NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
    entity_type TEXT NOT NULL,
    schema_id TEXT REFERENCES entity_schemas(id) ON DELETE SET NULL,
    title TEXT NOT NULL,
    description TEXT NOT NULL DEFAULT '',
    data_json TEXT NOT NULL DEFAULT '{}',
    revision INTEGER NOT NULL DEFAULT 1,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS entity_views (
    id TEXT PRIMARY KEY,
    entity_id TEXT NOT NULL REFERENCES entities(id) ON DELETE CASCADE,
    space_id TEXT NOT NULL REFERENCES spaces(id) ON DELETE CASCADE,
    x REAL NOT NULL DEFAULT 0,
    y REAL NOT NULL DEFAULT 0,
    width REAL NOT NULL DEFAULT 220,
    height REAL NOT NULL DEFAULT 120,
    view_type TEXT NOT NULL DEFAULT 'Node',
    layer_id TEXT NOT NULL DEFAULT 'Main',
    visible INTEGER NOT NULL DEFAULT 1,
    visible_properties_json TEXT NOT NULL DEFAULT '[]',
    board_id TEXT REFERENCES boards(id) ON DELETE SET NULL,
    board_local_x REAL NOT NULL DEFAULT 0,
    board_local_y REAL NOT NULL DEFAULT 0,
    revision INTEGER NOT NULL DEFAULT 1,
    UNIQUE(entity_id, space_id)
);

CREATE TABLE IF NOT EXISTS boards (
    id TEXT PRIMARY KEY,
    project_id TEXT NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
    space_id TEXT NOT NULL REFERENCES spaces(id) ON DELETE CASCADE,
    title TEXT NOT NULL,
    x REAL NOT NULL DEFAULT 0,
    y REAL NOT NULL DEFAULT 0,
    width REAL NOT NULL DEFAULT 800,
    height REAL NOT NULL DEFAULT 500,
    background TEXT NOT NULL DEFAULT '#172235',
    border TEXT NOT NULL DEFAULT '#4d7198',
    locked INTEGER NOT NULL DEFAULT 0,
    collapsed INTEGER NOT NULL DEFAULT 0,
    layer_id TEXT NOT NULL DEFAULT 'Main',
    z_order INTEGER NOT NULL DEFAULT 0,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS ink_strokes (
    id TEXT PRIMARY KEY,
    project_id TEXT NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
    space_id TEXT NOT NULL REFERENCES spaces(id) ON DELETE CASCADE,
    points_json TEXT NOT NULL,
    width REAL NOT NULL DEFAULT 2,
    tool TEXT NOT NULL DEFAULT 'pen',
    shape TEXT NOT NULL DEFAULT 'freehand',
    color TEXT NOT NULL DEFAULT '#9ed5ff',
    layer_id TEXT NOT NULL DEFAULT 'Main',
    group_id TEXT,
    anchor_entity_id TEXT REFERENCES entities(id) ON DELETE SET NULL,
    anchor_board_id TEXT REFERENCES boards(id) ON DELETE SET NULL,
    local_offset_x REAL NOT NULL DEFAULT 0,
    local_offset_y REAL NOT NULL DEFAULT 0,
    created_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS scene_locks (
    scene_id TEXT PRIMARY KEY REFERENCES entities(id) ON DELETE CASCADE,
    project_id TEXT NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
    status TEXT NOT NULL DEFAULT 'FREE' CHECK(status IN ('FREE', 'LOCKED')),
    owner_id TEXT,
    owner_name TEXT,
    worker_id TEXT,
    task_id TEXT REFERENCES entities(id) ON DELETE SET NULL,
    task TEXT,
    task_note TEXT,
    branch TEXT,
    acquired_at TEXT,
    heartbeat_at TEXT
);

CREATE TABLE IF NOT EXISTS user_layers (
    id TEXT PRIMARY KEY,
    project_id TEXT NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
    name TEXT NOT NULL,
    visible INTEGER NOT NULL DEFAULT 1,
    space_id TEXT REFERENCES spaces(id) ON DELETE CASCADE,
    locked INTEGER NOT NULL DEFAULT 0,
    sort_order INTEGER NOT NULL DEFAULT 0,
    UNIQUE(project_id, name)
);

CREATE TABLE IF NOT EXISTS task_details (
    entity_id TEXT PRIMARY KEY REFERENCES entities(id) ON DELETE CASCADE,
    status TEXT NOT NULL DEFAULT 'Backlog' CHECK(status IN ('Backlog', 'Ready', 'InProgress', 'Review', 'Testing', 'Done', 'Blocked')),
    priority TEXT NOT NULL DEFAULT 'Normal' CHECK(priority IN ('Low', 'Normal', 'High', 'Critical')),
    assigned_worker_id TEXT REFERENCES entities(id) ON DELETE SET NULL,
    milestone_id TEXT REFERENCES entities(id) ON DELETE SET NULL,
    system_id TEXT REFERENCES entities(id) ON DELETE SET NULL,
    branch TEXT NOT NULL DEFAULT '',
    allowed_paths_json TEXT NOT NULL DEFAULT '[]',
    forbidden_paths_json TEXT NOT NULL DEFAULT '[]',
    acceptance_criteria TEXT NOT NULL DEFAULT '',
    started_at TEXT,
    completed_at TEXT
);

CREATE TABLE IF NOT EXISTS system_details (
    entity_id TEXT PRIMARY KEY REFERENCES entities(id) ON DELETE CASCADE,
    status TEXT NOT NULL DEFAULT 'Planned' CHECK(status IN ('Planned', 'Developing', 'Testing', 'Stable', 'Deprecated')),
    owner_worker_id TEXT REFERENCES entities(id) ON DELETE SET NULL,
    progress REAL NOT NULL DEFAULT 0 CHECK(progress >= 0 AND progress <= 1)
);

CREATE TABLE IF NOT EXISTS worker_details (
    entity_id TEXT PRIMARY KEY REFERENCES entities(id) ON DELETE CASCADE,
    worker_id TEXT NOT NULL UNIQUE,
    worker_type TEXT NOT NULL DEFAULT 'Automation' CHECK(worker_type IN ('Human', 'Codex', 'Automation')),
    status TEXT NOT NULL DEFAULT 'Offline' CHECK(status IN ('Offline', 'Idle', 'Working')),
    branch TEXT NOT NULL DEFAULT '',
    last_activity_at TEXT
);

CREATE TABLE IF NOT EXISTS milestone_details (
    entity_id TEXT PRIMARY KEY REFERENCES entities(id) ON DELETE CASCADE,
    status TEXT NOT NULL DEFAULT 'Planned' CHECK(status IN ('Planned', 'Active', 'Completed', 'Blocked')),
    target_date TEXT,
    progress REAL NOT NULL DEFAULT 0 CHECK(progress >= 0 AND progress <= 1),
    acceptance_criteria TEXT NOT NULL DEFAULT ''
);

CREATE TABLE IF NOT EXISTS scene_details (
    entity_id TEXT PRIMARY KEY REFERENCES entities(id) ON DELETE CASCADE,
    asset_path TEXT NOT NULL DEFAULT ''
);

CREATE TABLE IF NOT EXISTS relationships (
    id TEXT PRIMARY KEY,
    project_id TEXT NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
    source_entity_id TEXT NOT NULL REFERENCES entities(id) ON DELETE CASCADE,
    target_entity_id TEXT NOT NULL REFERENCES entities(id) ON DELETE CASCADE,
    relationship_type TEXT NOT NULL CHECK(relationship_type IN ('depends_on', 'blocks', 'belongs_to_system', 'assigned_to', 'belongs_to_milestone', 'integrates_into', 'related_to')),
    visible INTEGER NOT NULL DEFAULT 1,
    created_at TEXT NOT NULL,
    UNIQUE(project_id, source_entity_id, target_entity_id, relationship_type)
);

CREATE TABLE IF NOT EXISTS query_views (
    id TEXT PRIMARY KEY,
    project_id TEXT NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
    space_id TEXT REFERENCES spaces(id) ON DELETE CASCADE,
    name TEXT NOT NULL,
    entity_schema TEXT NOT NULL DEFAULT '',
    filters_json TEXT NOT NULL DEFAULT '[]',
    sort_json TEXT NOT NULL DEFAULT '{}',
    result_limit INTEGER NOT NULL DEFAULT 50,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL,
    UNIQUE(project_id, name)
);

CREATE TABLE IF NOT EXISTS dashboard_widgets (
    id TEXT PRIMARY KEY,
    project_id TEXT NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
    space_id TEXT NOT NULL REFERENCES spaces(id) ON DELETE CASCADE,
    widget_type TEXT NOT NULL,
    title TEXT NOT NULL DEFAULT '',
    config_json TEXT NOT NULL DEFAULT '{}',
    x REAL NOT NULL DEFAULT 0,
    y REAL NOT NULL DEFAULT 0,
    width REAL NOT NULL DEFAULT 320,
    height REAL NOT NULL DEFAULT 180,
    layer_id TEXT NOT NULL DEFAULT 'Main',
    visible INTEGER NOT NULL DEFAULT 1,
    revision INTEGER NOT NULL DEFAULT 1,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS activities (
    id TEXT PRIMARY KEY,
    project_id TEXT NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
    actor_id TEXT NOT NULL DEFAULT '',
    actor_name TEXT NOT NULL DEFAULT '',
    event_type TEXT NOT NULL,
    entity_id TEXT,
    summary TEXT NOT NULL,
    payload_json TEXT NOT NULL DEFAULT '{}',
    created_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS project_templates (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL UNIQUE,
    template_type TEXT NOT NULL,
    config_json TEXT NOT NULL DEFAULT '{}',
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_spaces_project ON spaces(project_id);
CREATE INDEX IF NOT EXISTS idx_entities_project ON entities(project_id);
CREATE INDEX IF NOT EXISTS idx_entities_type ON entities(project_id, entity_type);
CREATE INDEX IF NOT EXISTS idx_views_space ON entity_views(space_id);
CREATE INDEX IF NOT EXISTS idx_strokes_space ON ink_strokes(space_id);
CREATE INDEX IF NOT EXISTS idx_locks_project ON scene_locks(project_id);
CREATE INDEX IF NOT EXISTS idx_relationships_project ON relationships(project_id);
CREATE INDEX IF NOT EXISTS idx_relationships_source ON relationships(source_entity_id);
CREATE INDEX IF NOT EXISTS idx_relationships_target ON relationships(target_entity_id);
CREATE INDEX IF NOT EXISTS idx_properties_schema ON property_definitions(schema_id);
CREATE INDEX IF NOT EXISTS idx_property_values_entity ON entity_property_values(entity_id);
CREATE INDEX IF NOT EXISTS idx_activities_project ON activities(project_id, created_at);
"""


def utc_now() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")


def parse_json(value: str | None, fallback: Any) -> Any:
    try:
        return json.loads(value or "")
    except (TypeError, ValueError):
        return fallback


def _columns(connection: sqlite3.Connection, table: str) -> set[str]:
    return {row[1] for row in connection.execute(f"PRAGMA table_info({table})").fetchall()}


def _ensure_column(
    connection: sqlite3.Connection,
    table: str,
    name: str,
    definition: str,
) -> None:
    if name not in _columns(connection, table):
        connection.execute(f"ALTER TABLE {table} ADD COLUMN {name} {definition}")


def migrate(connection: sqlite3.Connection) -> None:
    """Upgrade a Phase 1 database in place without deleting user data."""

    version_row = connection.execute("SELECT version FROM schema_version LIMIT 1").fetchone()
    if version_row is None:
        # A Phase 1 database had no version table. Mark it as v1 before the
        # additive v2 migration so the transition is explicit and repeatable.
        connection.execute("INSERT INTO schema_version(version) VALUES (1)")

    # These ALTERs are intentionally additive. CREATE TABLE IF NOT EXISTS in
    # SCHEMA above already handles a new Phase 2 database.
    _ensure_column(connection, "spaces", "view_type", "TEXT NOT NULL DEFAULT 'Graph'")
    _ensure_column(connection, "spaces", "sort_order", "INTEGER NOT NULL DEFAULT 0")
    _ensure_column(connection, "entities", "description", "TEXT NOT NULL DEFAULT ''")
    _ensure_column(connection, "entities", "schema_id", "TEXT")
    _ensure_column(connection, "entities", "revision", "INTEGER NOT NULL DEFAULT 1")
    _ensure_column(connection, "entity_views", "view_type", "TEXT NOT NULL DEFAULT 'Node'")
    _ensure_column(connection, "entity_views", "visible_properties_json", "TEXT NOT NULL DEFAULT '[]'")
    _ensure_column(connection, "entity_views", "board_id", "TEXT")
    _ensure_column(connection, "entity_views", "board_local_x", "REAL NOT NULL DEFAULT 0")
    _ensure_column(connection, "entity_views", "board_local_y", "REAL NOT NULL DEFAULT 0")
    _ensure_column(connection, "entity_views", "revision", "INTEGER NOT NULL DEFAULT 1")
    _ensure_column(connection, "ink_strokes", "shape", "TEXT NOT NULL DEFAULT 'freehand'")
    _ensure_column(connection, "ink_strokes", "color", "TEXT NOT NULL DEFAULT '#9ed5ff'")
    _ensure_column(connection, "ink_strokes", "group_id", "TEXT")
    _ensure_column(connection, "ink_strokes", "anchor_entity_id", "TEXT REFERENCES entities(id) ON DELETE SET NULL")
    _ensure_column(connection, "ink_strokes", "anchor_board_id", "TEXT")
    _ensure_column(connection, "ink_strokes", "local_offset_x", "REAL NOT NULL DEFAULT 0")
    _ensure_column(connection, "ink_strokes", "local_offset_y", "REAL NOT NULL DEFAULT 0")
    _ensure_column(connection, "user_layers", "space_id", "TEXT")
    _ensure_column(connection, "user_layers", "locked", "INTEGER NOT NULL DEFAULT 0")
    _ensure_column(connection, "user_layers", "sort_order", "INTEGER NOT NULL DEFAULT 0")
    _ensure_column(connection, "scene_locks", "worker_id", "TEXT")
    _ensure_column(connection, "scene_locks", "task_id", "TEXT REFERENCES entities(id) ON DELETE SET NULL")
    _ensure_column(connection, "scene_locks", "task_note", "TEXT")
    _ensure_column(connection, "relationships", "visible", "INTEGER NOT NULL DEFAULT 1")
    _ensure_column(connection, "property_definitions", "min_value", "REAL")
    _ensure_column(connection, "property_definitions", "max_value", "REAL")
    _ensure_column(connection, "property_definitions", "step_value", "REAL")
    _ensure_column(connection, "boards", "collapsed", "INTEGER NOT NULL DEFAULT 0")
    connection.execute(
        "CREATE INDEX IF NOT EXISTS idx_strokes_anchor ON ink_strokes(anchor_entity_id)"
    )
    connection.execute(
        "CREATE INDEX IF NOT EXISTS idx_strokes_board ON ink_strokes(anchor_board_id)"
    )
    connection.execute(
        "CREATE INDEX IF NOT EXISTS idx_strokes_group ON ink_strokes(group_id)"
    )
    connection.execute(
        "CREATE INDEX IF NOT EXISTS idx_entities_schema ON entities(schema_id)"
    )
    connection.execute(
        "CREATE INDEX IF NOT EXISTS idx_views_board ON entity_views(board_id)"
    )

    # Recover the common description and the Phase 1 lock aliases from the
    # old data_json/owner fields. This does not overwrite non-empty v2 data.
    for row in connection.execute(
        "SELECT id, data_json FROM entities WHERE description = ''"
    ).fetchall():
        data = parse_json(row[1], {})
        if isinstance(data, dict) and data.get("description"):
            connection.execute(
                "UPDATE entities SET description = ? WHERE id = ?",
                (str(data["description"]), row[0]),
            )
    connection.execute(
        "UPDATE scene_locks SET worker_id = owner_id WHERE worker_id IS NULL"
    )
    connection.execute(
        "UPDATE scene_locks SET task_note = task WHERE task_note IS NULL"
    )
    connection.execute("UPDATE entities SET revision = 1 WHERE revision IS NULL OR revision < 1")
    connection.execute("UPDATE entity_views SET revision = 1 WHERE revision IS NULL OR revision < 1")

    connection.execute(
        "INSERT OR IGNORE INTO scene_details(entity_id, asset_path) "
        "SELECT id, '' FROM entities WHERE entity_type = 'scene'"
    )
    connection.execute("UPDATE schema_version SET version = ?", (SCHEMA_VERSION,))


class _ManagedConnection(sqlite3.Connection):
    """Connection that closes when used with ``with database.connect()``."""

    def __exit__(self, exc_type, exc_value, traceback):
        try:
            return super().__exit__(exc_type, exc_value, traceback)
        finally:
            self.close()


class Database:
    """Small per-operation SQLite connection wrapper.

    A new connection per operation keeps FastAPI workers and concurrent lock
    requests independent while SQLite's WAL/timeout settings make every
    transaction boundary explicit.
    """

    def __init__(self, path: str | Path):
        self.path = Path(path)
        self.path.parent.mkdir(parents=True, exist_ok=True)

    def connect(self) -> sqlite3.Connection:
        connection = sqlite3.connect(
            self.path,
            timeout=30.0,
            isolation_level=None,
            check_same_thread=False,
            factory=_ManagedConnection,
        )
        connection.row_factory = sqlite3.Row
        connection.execute("PRAGMA foreign_keys = ON")
        connection.execute("PRAGMA busy_timeout = 30000")
        connection.execute("PRAGMA journal_mode = WAL")
        return connection

    def initialize(self) -> None:
        with self.connect() as connection:
            connection.executescript(SCHEMA)
            migrate(connection)
            seed(connection)


def _ensure_space(
    connection: sqlite3.Connection,
    project_id: str,
    space_id: str,
    name: str,
    now: str,
    view_type: str = "Graph",
) -> None:
    connection.execute(
        """INSERT OR IGNORE INTO spaces
           (id, project_id, name, camera_x, camera_y, zoom, view_type, created_at)
           VALUES (?, ?, ?, 0, 0, 1, ?, ?)""",
        (space_id, project_id, name, view_type, now),
    )


def _ensure_schema(
    connection: sqlite3.Connection,
    *,
    schema_id: str,
    project_id: str,
    name: str,
    display_name: str,
    description: str,
    now: str,
    properties: Iterable[dict[str, Any]],
) -> None:
    connection.execute(
        """INSERT OR IGNORE INTO entity_schemas
           (id, project_id, name, display_name, icon, description, created_at, updated_at)
           VALUES (?, ?, ?, ?, '', ?, ?, ?)""",
        (schema_id, project_id, name, display_name, description, now, now),
    )
    for index, definition in enumerate(properties):
        property_id = str(definition.get("id") or f"{schema_id}:property:{definition['name']}")
        connection.execute(
            """INSERT OR IGNORE INTO property_definitions
            (id, schema_id, name, display_name, property_type, default_value_json,
             required, options_json, min_value, max_value, step_value, sort_order, created_at, updated_at)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)""",
            (
                property_id,
                schema_id,
                str(definition["name"]),
                str(definition.get("display_name", definition["name"])),
                str(definition.get("property_type", "Text")),
                json.dumps(definition.get("default_value"), ensure_ascii=False),
                 int(bool(definition.get("required", False))),
                 json.dumps(definition.get("options", []), ensure_ascii=False),
                 definition.get("min_value"), definition.get("max_value"), definition.get("step_value"),
                 int(definition.get("sort_order", index)),
                now,
                now,
            ),
        )


def _ensure_property_values(
    connection: sqlite3.Connection,
    entity_id: str,
    schema_id: str,
    values: dict[str, Any],
    now: str,
) -> None:
    definitions = connection.execute(
        "SELECT id, name FROM property_definitions WHERE schema_id = ?",
        (schema_id,),
    ).fetchall()
    for definition in definitions:
        if definition["name"] not in values:
            continue
        connection.execute(
            """INSERT INTO entity_property_values(entity_id, property_definition_id, value_json, updated_at)
               VALUES (?, ?, ?, ?)
               ON CONFLICT(entity_id, property_definition_id) DO UPDATE SET
                 value_json=excluded.value_json, updated_at=excluded.updated_at""",
            (
                entity_id,
                definition["id"],
                json.dumps(values[definition["name"]], ensure_ascii=False),
                now,
            ),
        )


def _ensure_board(
    connection: sqlite3.Connection,
    *,
    board_id: str,
    project_id: str,
    space_id: str,
    title: str,
    x: float,
    y: float,
    width: float,
    height: float,
    now: str,
) -> None:
    connection.execute(
        """INSERT OR IGNORE INTO boards
           (id, project_id, space_id, title, x, y, width, height, background, border,
            locked, layer_id, z_order, created_at, updated_at)
           VALUES (?, ?, ?, ?, ?, ?, ?, ?, '#172235', '#4d7198', 0, 'Main', 0, ?, ?)""",
        (board_id, project_id, space_id, title, x, y, width, height, now, now),
    )


def _ensure_query_view(
    connection: sqlite3.Connection,
    *,
    query_id: str,
    project_id: str,
    space_id: str,
    name: str,
    entity_schema: str,
    filters: list[dict[str, Any]],
    now: str,
) -> None:
    connection.execute(
        """INSERT OR IGNORE INTO query_views
           (id, project_id, space_id, name, entity_schema, filters_json, sort_json,
            result_limit, created_at, updated_at)
           VALUES (?, ?, ?, ?, ?, ?, '{}', 50, ?, ?)""",
        (query_id, project_id, space_id, name, entity_schema,
         json.dumps(filters, ensure_ascii=False), now, now),
    )


def _ensure_widget(
    connection: sqlite3.Connection,
    *,
    widget_id: str,
    project_id: str,
    space_id: str,
    widget_type: str,
    title: str,
    config: dict[str, Any],
    x: float,
    y: float,
    now: str,
) -> None:
    connection.execute(
        """INSERT OR IGNORE INTO dashboard_widgets
           (id, project_id, space_id, widget_type, title, config_json, x, y, width, height,
            layer_id, visible, revision, created_at, updated_at)
           VALUES (?, ?, ?, ?, ?, ?, ?, ?, 320, 180, 'Main', 1, 1, ?, ?)""",
        (widget_id, project_id, space_id, widget_type, title,
         json.dumps(config, ensure_ascii=False), x, y, now, now),
    )


def _ensure_entity(
    connection: sqlite3.Connection,
    *,
    entity_id: str,
    project_id: str,
    entity_type: str,
    title: str,
    description: str,
    now: str,
    data: dict[str, Any] | None = None,
    details: dict[str, Any] | None = None,
    schema_id: str | None = None,
    properties: dict[str, Any] | None = None,
) -> None:
    connection.execute(
        """INSERT OR IGNORE INTO entities
           (id, project_id, entity_type, schema_id, title, description, data_json, revision, created_at, updated_at)
           VALUES (?, ?, ?, ?, ?, ?, ?, 1, ?, ?)""",
        (
            entity_id,
            project_id,
            entity_type,
            schema_id,
            title,
            description,
            json.dumps(data or {}, ensure_ascii=False),
            now,
            now,
        ),
    )
    if schema_id is not None:
        _ensure_property_values(connection, entity_id, schema_id, properties or {}, now)
    values = details or {}
    if entity_type == "task":
        connection.execute(
            """INSERT OR IGNORE INTO task_details
               (entity_id, status, priority, assigned_worker_id, milestone_id, system_id,
                branch, allowed_paths_json, forbidden_paths_json, acceptance_criteria,
                started_at, completed_at)
               VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)""",
            (
                entity_id,
                values.get("status", "Backlog"),
                values.get("priority", "Normal"),
                values.get("assigned_worker_id"),
                values.get("milestone_id"),
                values.get("system_id"),
                values.get("branch", ""),
                json.dumps(values.get("allowed_paths", []), ensure_ascii=False),
                json.dumps(values.get("forbidden_paths", []), ensure_ascii=False),
                values.get("acceptance_criteria", ""),
                values.get("started_at"),
                values.get("completed_at"),
            ),
        )
    elif entity_type == "system":
        connection.execute(
            """INSERT OR IGNORE INTO system_details
               (entity_id, status, owner_worker_id, progress) VALUES (?, ?, ?, ?)""",
            (entity_id, values.get("status", "Planned"), values.get("owner_worker_id"), values.get("progress", 0)),
        )
    elif entity_type == "worker":
        connection.execute(
            """INSERT OR IGNORE INTO worker_details
               (entity_id, worker_id, worker_type, status, branch, last_activity_at)
               VALUES (?, ?, ?, ?, ?, ?)""",
            (
                entity_id,
                values.get("worker_id", entity_id),
                values.get("worker_type", "Automation"),
                values.get("status", "Offline"),
                values.get("branch", ""),
                values.get("last_activity_at"),
            ),
        )
    elif entity_type == "milestone":
        connection.execute(
            """INSERT OR IGNORE INTO milestone_details
               (entity_id, status, target_date, progress, acceptance_criteria)
               VALUES (?, ?, ?, ?, ?)""",
            (
                entity_id,
                values.get("status", "Planned"),
                values.get("target_date"),
                values.get("progress", 0),
                values.get("acceptance_criteria", ""),
            ),
        )
    elif entity_type == "scene":
        connection.execute(
            "INSERT OR IGNORE INTO scene_details(entity_id, asset_path) VALUES (?, ?)",
            (entity_id, values.get("asset_path", "")),
        )


def _ensure_view(
    connection: sqlite3.Connection,
    entity_id: str,
    space_id: str,
    x: float,
    y: float,
    now: str,
    *,
    view_type: str = "Node",
    width: float = 240,
    height: float = 140,
) -> None:
    connection.execute(
        """INSERT OR IGNORE INTO entity_views
           (id, entity_id, space_id, x, y, width, height, view_type, layer_id, visible, revision)
           VALUES (?, ?, ?, ?, ?, ?, ?, ?, 'Main', 1, 1)""",
        (f"view:{entity_id}:{space_id}", entity_id, space_id, x, y, width, height, view_type),
    )


def _ensure_relationship(
    connection: sqlite3.Connection,
    relationship_id: str,
    project_id: str,
    source_id: str,
    target_id: str,
    relationship_type: str,
    now: str,
) -> None:
    connection.execute(
        """INSERT OR IGNORE INTO relationships
           (id, project_id, source_entity_id, target_entity_id, relationship_type, created_at)
           VALUES (?, ?, ?, ?, ?, ?)""",
        (relationship_id, project_id, source_id, target_id, relationship_type, now),
    )


def _ensure_template(
    connection: sqlite3.Connection,
    *,
    template_id: str,
    name: str,
    template_type: str,
    config: dict[str, Any],
    now: str,
) -> None:
    connection.execute(
        """INSERT OR IGNORE INTO project_templates
           (id, name, template_type, config_json, created_at, updated_at)
           VALUES (?, ?, ?, ?, ?, ?)""",
        (template_id, name, template_type, json.dumps(config, ensure_ascii=False), now, now),
    )


def seed(connection: sqlite3.Connection) -> None:
    """Create deterministic demo projects and Phase 2 fixture entities.

    Seed data is server-owned and idempotent. The client contains no project
    business fixture data.
    """

    now = utc_now()
    _ensure_template(
        connection, template_id="template:blank", name="Blank", template_type="Blank", now=now,
        config={"spaces": [{"name": "Overview", "view_type": "Graph"}], "schemas": [], "widgets": []},
    )
    _ensure_template(
        connection, template_id="template:software", name="Software", template_type="Software", now=now,
        config={
            "spaces": [
                {"name": "Overview", "view_type": "Graph"},
                {"name": "Architecture", "view_type": "Graph"},
                {"name": "Sprint Board", "view_type": "TaskBoard"},
            ],
            "schemas": [
                {"name": "Bug", "display_name": "Bug", "properties": [
                    {"name": "status", "display_name": "Status", "type": "Enum",
                     "default_value": "Open", "options": ["Open", "InProgress", "Resolved", "Closed"]},
                    {"name": "severity", "display_name": "Severity", "type": "Enum",
                     "default_value": "Medium", "options": ["Low", "Medium", "High", "Critical"]},
                    {"name": "owner", "display_name": "Owner", "type": "EntityReference"},
                ]},
                {"name": "Decision", "display_name": "Decision", "properties": [
                    {"name": "status", "display_name": "Status", "type": "Enum",
                     "default_value": "Proposed", "options": ["Proposed", "Accepted", "Rejected"]},
                    {"name": "decision", "display_name": "Decision", "type": "MultilineText"},
                ]},
            ],
            "widgets": [
                {"space": "Overview", "widget_type": "EntityList", "title": "Active Tasks",
                 "config": {"query": {"entity_schema": "task", "filters": [
                     {"property": "status", "operator": "NotEquals", "value": "Done"}]}}},
                {"space": "Overview", "widget_type": "RecentActivity", "title": "Recent Activity", "config": {}},
            ],
        },
    )
    _ensure_template(
        connection, template_id="template:game-development", name="Game Development", template_type="GameDevelopment", now=now,
        config={
            "spaces": [
                {"name": "Overview", "view_type": "Graph"},
                {"name": "Architecture", "view_type": "Graph"},
                {"name": "Current Sprint", "view_type": "TaskBoard"},
            ],
            "schemas": [
                {"name": "Bug", "display_name": "Bug", "properties": [
                    {"name": "status", "display_name": "Status", "type": "Enum",
                     "default_value": "Open", "options": ["Open", "InProgress", "Resolved", "Closed"]},
                    {"name": "severity", "display_name": "Severity", "type": "Enum",
                     "default_value": "Medium", "options": ["Low", "Medium", "High", "Critical"]},
                    {"name": "related_system", "display_name": "Related System", "type": "EntityReference"},
                ]},
                {"name": "Decision", "display_name": "Decision", "properties": [
                    {"name": "status", "display_name": "Status", "type": "Enum",
                     "default_value": "Proposed", "options": ["Proposed", "Accepted", "Rejected"]},
                    {"name": "context", "display_name": "Context", "type": "MultilineText"},
                    {"name": "date", "display_name": "Date", "type": "Date"},
                ]},
            ],
            "widgets": [
                {"space": "Overview", "widget_type": "EntityList", "title": "Open Bugs",
                 "config": {"query": {"entity_schema": "Bug", "filters": [
                     {"property": "status", "operator": "NotEquals", "value": "Closed"}]}}},
                {"space": "Overview", "widget_type": "RecentActivity", "title": "Activity", "config": {}},
            ],
        },
    )
    projects = (("P_HighSchool", "P_HighSchool"), ("XiE", "XiE"))
    for project_id, name in projects:
        connection.execute(
            "INSERT OR IGNORE INTO projects(id, name, created_at) VALUES (?, ?, ?)",
            (project_id, name, now),
        )
        _ensure_space(connection, project_id, f"{project_id}:Overview", "Overview", now)
        _ensure_space(connection, project_id, f"{project_id}:Architecture", "Architecture", now)
        connection.execute(
            "INSERT OR IGNORE INTO user_layers(id, project_id, name, visible) VALUES (?, ?, ?, 1)",
            (f"{project_id}:Main", project_id, "Main"),
        )
        connection.execute(
            "INSERT OR IGNORE INTO user_layers(id, project_id, name, visible) VALUES (?, ?, ?, 1)",
            (f"{project_id}:Notes", project_id, "Notes"),
        )

        _ensure_schema(
            connection,
            schema_id=f"{project_id}:schema:Bug",
            project_id=project_id,
            name="Bug",
            display_name="Bug",
            description="A user-defined issue tracked with generic properties.",
            now=now,
            properties=(
                {"name": "status", "display_name": "Status", "property_type": "Enum",
                 "default_value": "Open", "options": ["Open", "InProgress", "Resolved", "Closed"]},
                {"name": "severity", "display_name": "Severity", "property_type": "Enum",
                 "default_value": "Medium", "options": ["Low", "Medium", "High", "Critical"]},
                {"name": "priority", "display_name": "Priority", "property_type": "Enum",
                 "default_value": "Normal", "options": ["Low", "Normal", "High", "Critical"]},
                {"name": "owner", "display_name": "Owner", "property_type": "EntityReference"},
                {"name": "related_system", "display_name": "Related System", "property_type": "EntityReference"},
                {"name": "description", "display_name": "Description", "property_type": "MultilineText"},
            ),
        )
        _ensure_schema(
            connection,
            schema_id=f"{project_id}:schema:Decision",
            project_id=project_id,
            name="Decision",
            display_name="Decision",
            description="An architecture or product decision record.",
            now=now,
            properties=(
                {"name": "status", "display_name": "Status", "property_type": "Enum",
                 "default_value": "Proposed", "options": ["Proposed", "Accepted", "Rejected", "Superseded"]},
                {"name": "context", "display_name": "Context", "property_type": "MultilineText"},
                {"name": "decision", "display_name": "Decision", "property_type": "MultilineText"},
                {"name": "reason", "display_name": "Reason", "property_type": "MultilineText"},
                {"name": "date", "display_name": "Date", "property_type": "Date"},
                {"name": "related_system", "display_name": "Related System", "property_type": "EntityReference"},
            ),
        )

    _ensure_space(connection, "P_HighSchool", "P_HighSchool:CurrentSprint", "Current Sprint", now, "TaskBoard")
    _ensure_space(connection, "P_HighSchool", "P_HighSchool:NPCArchitecture", "NPC Architecture", now, "Graph")
    _ensure_space(connection, "P_HighSchool", "P_HighSchool:ProjectOverview", "Project Overview", now, "Graph")
    connection.execute(
        "INSERT OR IGNORE INTO user_layers(id, project_id, name, visible, sort_order) VALUES (?, ?, ?, 1, 2)",
        ("P_HighSchool:Architecture", "P_HighSchool", "Architecture"),
    )

    scene_rows = (
        ("scene-main-school", "P_HighSchool", "L_MainSchool", "Main school integration scene", "Assets/Scenes/MainSchool.unity"),
        ("scene-xie", "XiE", "L_XiE", "XiE runtime scene", "Assets/Scenes/XiE.unity"),
    )
    for entity_id, project_id, title, description, asset_path in scene_rows:
        _ensure_entity(
            connection,
            entity_id=entity_id,
            project_id=project_id,
            entity_type="scene",
            title=title,
            description=description,
            now=now,
            details={"asset_path": asset_path},
        )
        overview_id = f"{project_id}:Overview"
        _ensure_view(connection, entity_id, overview_id, 120, 120, now, width=260, height=150)
        connection.execute(
            """INSERT OR IGNORE INTO scene_locks
               (scene_id, project_id, status) VALUES (?, ?, 'FREE')""",
            (entity_id, project_id),
        )

    # The richer P_HighSchool fixture is deliberately kept here rather than
    # being embedded in the C++ client.
    systems = (
        ("system-task", "Task System", "Planned", 0.0, 400, 80),
        ("system-story", "Story System", "Developing", 0.35, 700, 80),
        ("system-weather", "Weather System", "Planned", 0.0, 1000, 80),
        ("system-survival", "Survival Stats", "Testing", 0.6, 400, 300),
        ("system-npc", "NPC System", "Developing", 0.45, 700, 300),
    )
    for entity_id, title, status, progress, x, y in systems:
        full_id = f"P_HighSchool:{entity_id}"
        _ensure_entity(
            connection,
            entity_id=full_id,
            project_id="P_HighSchool",
            entity_type="system",
            title=title,
            description=f"{title} module for P_HighSchool",
            now=now,
            details={"status": status, "progress": progress},
        )
        _ensure_view(connection, full_id, "P_HighSchool:Overview", x, y, now, width=260, height=150)
        _ensure_view(connection, full_id, "P_HighSchool:NPCArchitecture", x, y, now, width=280, height=160)

    workers = (
        ("worker-xiling", "Xiling", "Human", "Idle", "", ""),
        ("worker-codex-task", "Codex-Task", "Codex", "Working", "codex/task-system", ""),
        ("worker-codex-story", "Codex-Story", "Codex", "Working", "codex/story-system", ""),
        ("worker-codex-npc", "Codex-NPC", "Codex", "Working", "codex/npc-system", ""),
    )
    for entity_id, title, worker_type, status, branch, last_activity in workers:
        full_id = f"P_HighSchool:{entity_id}"
        _ensure_entity(
            connection,
            entity_id=full_id,
            project_id="P_HighSchool",
            entity_type="worker",
            title=title,
            description=f"{title} worker",
            now=now,
            details={
                "worker_id": full_id,
                "worker_type": worker_type,
                "status": status,
                "branch": branch,
                "last_activity_at": last_activity or now,
            },
        )
        _ensure_view(connection, full_id, "P_HighSchool:Overview", 40, 600 + len(title) * 8, now, width=260, height=140)

    milestone_id = "P_HighSchool:milestone-alpha"
    _ensure_entity(
        connection,
        entity_id=milestone_id,
        project_id="P_HighSchool",
        entity_type="milestone",
        title="Alpha Gameplay Integration",
        description="Close the first playable school loop.",
        now=now,
        details={
            "status": "Active",
            "target_date": "2026-09-30",
            "acceptance_criteria": "Core school gameplay path is playable.",
        },
    )
    _ensure_view(connection, milestone_id, "P_HighSchool:Overview", 40, 80, now, width=300, height=150)

    task_rows = (
        (
            "task-npc-scene", "NPC正式场景接入", "Ready", "Critical", "worker-codex-npc",
            "system-npc", "codex/npc-system", "NPC scene is loaded and lock-protected.", 400, 520,
        ),
        (
            "task-story-validation", "Story多人剧情验证", "InProgress", "High", "worker-codex-story",
            "system-story", "codex/story-system", "Two story paths can be replayed.", 720, 520,
        ),
        (
            "task-blueprint-test", "Task蓝图手动测试", "Testing", "Normal", "worker-codex-task",
            "system-task", "codex/task-system", "Blueprint smoke test checklist is green.", 1040, 520,
        ),
        (
            "task-survival-validation", "Survival蓝图手动验证", "Review", "High", "worker-codex-task",
            "system-survival", "codex/survival-system", "Survival stat transitions are reviewed.", 400, 720,
        ),
    )
    for entity_id, title, status, priority, worker_suffix, system_suffix, branch, criteria, x, y in task_rows:
        full_id = f"P_HighSchool:{entity_id}"
        worker_id = f"P_HighSchool:{worker_suffix}"
        system_id = f"P_HighSchool:{system_suffix}"
        _ensure_entity(
            connection,
            entity_id=full_id,
            project_id="P_HighSchool",
            entity_type="task",
            title=title,
            description=f"Phase 2 task: {title}",
            now=now,
            details={
                "status": status,
                "priority": priority,
                "assigned_worker_id": worker_id,
                "milestone_id": milestone_id,
                "system_id": system_id,
                "branch": branch,
                "allowed_paths": ["XiE_Projects/Crystal/**"],
                "forbidden_paths": ["Engine/Core/**", "Engine/Renderer/**"],
                "acceptance_criteria": criteria,
            },
        )
        _ensure_view(connection, full_id, "P_HighSchool:Overview", x, y, now, width=300, height=170)
        _ensure_view(connection, full_id, "P_HighSchool:CurrentSprint", x, y, now, view_type="CompactNode", width=260, height=110)

    _ensure_view(connection, "scene-main-school", "P_HighSchool:NPCArchitecture", 80, 80, now, width=280, height=160)
    _ensure_view(connection, "P_HighSchool:system-npc", "P_HighSchool:NPCArchitecture", 500, 80, now, width=300, height=180)
    _ensure_view(connection, "P_HighSchool:task-npc-scene", "P_HighSchool:NPCArchitecture", 900, 80, now, width=300, height=170)

    relation_rows = (
        ("rel-task-npc-system", "task-npc-scene", "system-npc", "belongs_to_system"),
        ("rel-task-npc-milestone", "task-npc-scene", "milestone-alpha", "belongs_to_milestone"),
        ("rel-task-npc-worker", "task-npc-scene", "worker-codex-npc", "assigned_to"),
        ("rel-task-story-system", "task-story-validation", "system-story", "belongs_to_system"),
        ("rel-task-story-milestone", "task-story-validation", "milestone-alpha", "belongs_to_milestone"),
        ("rel-task-story-worker", "task-story-validation", "worker-codex-story", "assigned_to"),
        ("rel-task-blueprint-system", "task-blueprint-test", "system-task", "belongs_to_system"),
        ("rel-task-blueprint-milestone", "task-blueprint-test", "milestone-alpha", "belongs_to_milestone"),
        ("rel-task-blueprint-worker", "task-blueprint-test", "worker-codex-task", "assigned_to"),
        ("rel-task-survival-system", "task-survival-validation", "system-survival", "belongs_to_system"),
        ("rel-task-survival-milestone", "task-survival-validation", "milestone-alpha", "belongs_to_milestone"),
        ("rel-task-survival-worker", "task-survival-validation", "worker-codex-task", "assigned_to"),
        ("rel-task-npc-depends-weather", "task-npc-scene", "system-weather", "depends_on"),
    )
    for relationship_id, source_suffix, target_suffix, relationship_type in relation_rows:
        _ensure_relationship(
            connection,
            f"P_HighSchool:{relationship_id}",
            "P_HighSchool",
            f"P_HighSchool:{source_suffix}",
            f"P_HighSchool:{target_suffix}",
            relationship_type,
            now,
        )

    bug_schema_id = "P_HighSchool:schema:Bug"
    decision_schema_id = "P_HighSchool:schema:Decision"
    _ensure_entity(
        connection,
        entity_id="P_HighSchool:bug-crosswalk",
        project_id="P_HighSchool",
        entity_type="Bug",
        schema_id=bug_schema_id,
        title="NPC无法通过斑马线",
        description="", now=now,
        properties={
            "status": "Open", "severity": "High", "priority": "High",
            "owner": "P_HighSchool:worker-codex-npc",
            "related_system": "P_HighSchool:system-npc",
            "description": "NPC crossing rule is not synchronized with the story event.",
        },
    )
    _ensure_entity(
        connection,
        entity_id="P_HighSchool:decision-story-unit",
        project_id="P_HighSchool",
        entity_type="Decision",
        schema_id=decision_schema_id,
        title="Story Segment 作为单次执行单位",
        description="", now=now,
        properties={
            "status": "Accepted",
            "context": "Story runtime needs a stable scheduling boundary.",
            "decision": "Treat each Story Segment as one execution unit.",
            "reason": "This keeps replay and lock ownership explicit.",
            "date": "2026-08-11",
            "related_system": "P_HighSchool:system-story",
        },
    )
    _ensure_view(connection, "P_HighSchool:bug-crosswalk", "P_HighSchool:ProjectOverview", 80, 80,
                 now, view_type="StatusCard", width=300, height=170)
    _ensure_view(connection, "P_HighSchool:bug-crosswalk", "P_HighSchool:Architecture", 900, 300,
                 now, view_type="Node", width=300, height=170)
    _ensure_view(connection, "P_HighSchool:decision-story-unit", "P_HighSchool:ProjectOverview", 420, 80,
                 now, view_type="Card", width=330, height=170)
    _ensure_view(connection, "P_HighSchool:decision-story-unit", "P_HighSchool:Architecture", 1200, 300,
                  now, view_type="Node", width=330, height=170)
    _ensure_view(connection, "P_HighSchool:system-npc", "P_HighSchool:Architecture", 120, 80,
                 now, view_type="Card", width=300, height=180)
    architecture_system_views = (
        ("P_HighSchool:system-task", "Task System", 100, 100, "P_HighSchool:board-gameplay", 60, 60),
        ("P_HighSchool:system-story", "Story System", 900, 100, "P_HighSchool:board-narrative", 60, 60),
        ("P_HighSchool:system-weather", "Weather System", 100, 450, "P_HighSchool:board-world", 60, 60),
        ("P_HighSchool:system-survival", "Survival Stats", 400, 450, "P_HighSchool:board-world", 360, 60),
        ("P_HighSchool:system-npc", "NPC System", 300, 100, "P_HighSchool:board-gameplay", 260, 60),
    )
    for entity_id, _title, x, y, _board_id, _local_x, _local_y in architecture_system_views:
        _ensure_view(connection, entity_id, "P_HighSchool:Architecture", x, y, now,
                     view_type="Card", width=300, height=180)

    _ensure_query_view(
        connection, query_id="P_HighSchool:query:active-tasks", project_id="P_HighSchool",
        space_id="P_HighSchool:ProjectOverview", name="Active Tasks", entity_schema="task",
        filters=[{"property": "status", "operator": "NotEquals", "value": "Done"}], now=now,
    )
    _ensure_query_view(
        connection, query_id="P_HighSchool:query:open-bugs", project_id="P_HighSchool",
        space_id="P_HighSchool:ProjectOverview", name="Open Bugs", entity_schema="Bug",
        filters=[{"property": "status", "operator": "NotEquals", "value": "Closed"}], now=now,
    )
    _ensure_widget(
        connection, widget_id="P_HighSchool:widget:active-tasks", project_id="P_HighSchool",
        space_id="P_HighSchool:ProjectOverview", widget_type="EntityList", title="ACTIVE TASKS",
        config={"query_view_id": "P_HighSchool:query:active-tasks"}, x=40, y=420, now=now,
    )
    _ensure_widget(
        connection, widget_id="P_HighSchool:widget:open-bugs", project_id="P_HighSchool",
        space_id="P_HighSchool:ProjectOverview", widget_type="EntityList", title="OPEN BUGS",
        config={"query_view_id": "P_HighSchool:query:open-bugs"}, x=390, y=420, now=now,
    )
    _ensure_widget(
        connection, widget_id="P_HighSchool:widget:activity", project_id="P_HighSchool",
        space_id="P_HighSchool:ProjectOverview", widget_type="RecentActivity", title="RECENT ACTIVITY",
        config={}, x=740, y=420, now=now,
    )
    _ensure_widget(
        connection, widget_id="P_HighSchool:widget:workers", project_id="P_HighSchool",
        space_id="P_HighSchool:ProjectOverview", widget_type="WorkerList", title="WORKERS",
        config={}, x=40, y=620, now=now,
    )
    _ensure_widget(
        connection, widget_id="P_HighSchool:widget:scene-locks", project_id="P_HighSchool",
        space_id="P_HighSchool:ProjectOverview", widget_type="SceneLockList", title="SCENE LOCKS",
        config={}, x=390, y=620, now=now,
    )

    board_rows = (
        ("P_HighSchool:board-gameplay", "Gameplay", 40, 40, 760, 300),
        ("P_HighSchool:board-narrative", "Narrative", 840, 40, 760, 300),
        ("P_HighSchool:board-world", "World Systems", 40, 390, 760, 300),
    )
    for board_id, title, x, y, width, height in board_rows:
        _ensure_board(
            connection, board_id=board_id, project_id="P_HighSchool",
            space_id="P_HighSchool:Architecture", title=title,
            x=x, y=y, width=width, height=height, now=now,
        )
    connection.execute(
        """UPDATE entity_views SET board_id = ?, board_local_x = ?, board_local_y = ?
           WHERE id = ?""",
        ("P_HighSchool:board-gameplay", 40, 40, "view:P_HighSchool:system-npc:P_HighSchool:Architecture"),
    )
    connection.execute(
        """UPDATE entity_views SET board_id = ?, board_local_x = ?, board_local_y = ?
           WHERE id = ?""",
        ("P_HighSchool:board-narrative", 360, 260, "view:P_HighSchool:decision-story-unit:P_HighSchool:Architecture"),
    )
    for entity_id, _title, x, y, board_id, local_x, local_y in architecture_system_views:
        connection.execute(
            """UPDATE entity_views SET board_id = ?, board_local_x = ?, board_local_y = ?, x = ?, y = ?
               WHERE id = ?""",
            (board_id, local_x, local_y, x, y,
             f"view:{entity_id}:P_HighSchool:Architecture"),
        )


def rows_to_dicts(rows: Iterable[sqlite3.Row]) -> list[dict[str, Any]]:
    return [dict(row) for row in rows]
