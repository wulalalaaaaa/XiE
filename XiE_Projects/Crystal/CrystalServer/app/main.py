from __future__ import annotations

import json
import os
import sqlite3
import uuid
from pathlib import Path
from typing import Any

from fastapi import FastAPI, HTTPException, Query, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel, Field

from app.database import Database, PROPERTY_TYPES, rows_to_dicts, utc_now
from app.entities import (
    DomainError,
    entity_to_dict,
    normalize_details,
    normalize_custom_properties,
    property_definition_to_dict,
    relationship_to_dict,
    schema_row,
    schema_to_dict,
    stored_details,
    validate_entity_type,
    validate_relationship_type,
    write_details,
)
from app.services.scene_locks import (
    AlreadyLocked,
    SceneLockError,
    SceneLockService,
    SceneNotFound,
    WrongOwner,
    public_lock,
)
from app.websocket.manager import ProjectConnectionManager


SERVER_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_DB_PATH = SERVER_ROOT / "data" / "crystal.db"


def _dump(model: BaseModel) -> dict[str, Any]:
    return model.model_dump() if hasattr(model, "model_dump") else model.dict()


class SpaceCreate(BaseModel):
    name: str = Field(min_length=1, max_length=100)
    view_type: str = Field(default="Graph", pattern="^(Graph|TaskBoard)$")
    sort_order: int = 0


class SpacePatch(BaseModel):
    name: str | None = Field(default=None, min_length=1, max_length=100)
    view_type: str | None = Field(default=None, pattern="^(Graph|TaskBoard)$")
    sort_order: int | None = None


class SpaceDuplicate(BaseModel):
    name: str = Field(min_length=1, max_length=100)


class EntityCreate(BaseModel):
    project_id: str
    entity_type: str = Field(min_length=1, max_length=32)
    schema_id: str | None = None
    title: str = Field(min_length=1, max_length=500)
    description: str = Field(default="", max_length=10000)
    # `data` remains for Phase 1 Text/Note compatibility. Typed Phase 2
    # fields are persisted in the normalized detail tables via `details`.
    data: dict[str, Any] = Field(default_factory=dict)
    details: dict[str, Any] = Field(default_factory=dict)
    properties: dict[str, Any] = Field(default_factory=dict)


class EntityPatch(BaseModel):
    expected_revision: int | None = Field(default=None, ge=1)
    title: str | None = Field(default=None, min_length=1, max_length=500)
    description: str | None = Field(default=None, max_length=10000)
    data: dict[str, Any] | None = None
    details: dict[str, Any] | None = None
    schema_id: str | None = None
    properties: dict[str, Any] | None = None


class EntityViewUpsert(BaseModel):
    expected_revision: int | None = Field(default=None, ge=1)
    x: float
    y: float
    width: float = Field(default=220, gt=1, le=10000)
    height: float = Field(default=120, gt=1, le=10000)
    view_type: str = Field(default="Node", pattern="^(Node|CompactNode|Card|StatusCard|Badge)$")
    layer_id: str = "Main"
    visible: bool = True
    visible_properties: list[str] = Field(default_factory=list)
    board_id: str | None = None
    board_local_x: float = 0
    board_local_y: float = 0


class Point(BaseModel):
    x: float
    y: float


class StrokeCreate(BaseModel):
    project_id: str
    space_id: str
    points: list[Point] = Field(min_length=2)
    width: float = Field(default=2, gt=0, le=100)
    tool: str = "pen"
    shape: str = "freehand"
    color: str = "#9ed5ff"
    layer_id: str = "Main"
    id: str | None = None
    group_id: str | None = None
    anchor_entity_id: str | None = None
    anchor_board_id: str | None = None
    local_offset_x: float = 0
    local_offset_y: float = 0


class StrokePatch(BaseModel):
    points: list[Point] | None = Field(default=None, min_length=2)
    width: float | None = Field(default=None, gt=0, le=100)
    tool: str | None = None
    layer_id: str | None = None
    anchor_entity_id: str | None = None
    anchor_board_id: str | None = None
    local_offset_x: float = 0
    local_offset_y: float = 0
    shape: str | None = None
    color: str | None = None
    group_id: str | None = None


class CameraUpdate(BaseModel):
    camera_x: float
    camera_y: float
    zoom: float = Field(gt=0)


class RelationshipCreate(BaseModel):
    project_id: str
    source_entity_id: str
    target_entity_id: str
    relationship_type: str


class RelationshipPatch(BaseModel):
    visible: bool | None = None


class InkGroupCreate(BaseModel):
    project_id: str
    space_id: str
    stroke_ids: list[str] = Field(min_length=1)
    group_id: str | None = None
    name: str = Field(default="", max_length=120)
    layer_id: str = "Main"


class TemplateApply(BaseModel):
    template_id: str


class TemplateSave(BaseModel):
    name: str = Field(min_length=1, max_length=120)
    template_type: str = Field(default="Custom", min_length=1, max_length=50)


class SchemaCreate(BaseModel):
    name: str = Field(min_length=1, max_length=80)
    display_name: str = Field(min_length=1, max_length=120)
    icon: str = Field(default="", max_length=32)
    description: str = Field(default="", max_length=1000)
    properties: list[dict[str, Any]] = Field(default_factory=list)


class SchemaPatch(BaseModel):
    display_name: str | None = Field(default=None, min_length=1, max_length=120)
    icon: str | None = Field(default=None, max_length=32)
    description: str | None = Field(default=None, max_length=1000)


class PropertyDefinitionCreate(BaseModel):
    name: str = Field(min_length=1, max_length=80)
    display_name: str = Field(min_length=1, max_length=120)
    property_type: str | None = None
    # The short `type` spelling is accepted for template/import payloads.
    type: str | None = None
    default_value: Any = None
    required: bool = False
    options: list[str] = Field(default_factory=list)
    min_value: float | None = None
    max_value: float | None = None
    step_value: float | None = None
    sort_order: int = 0


class PropertyDefinitionPatch(BaseModel):
    display_name: str | None = Field(default=None, min_length=1, max_length=120)
    property_type: str | None = None
    type: str | None = None
    default_value: Any = None
    required: bool | None = None
    options: list[str] | None = None
    min_value: float | None = None
    max_value: float | None = None
    step_value: float | None = None
    sort_order: int | None = None


class BoardCreate(BaseModel):
    space_id: str
    title: str = Field(min_length=1, max_length=200)
    x: float = 0
    y: float = 0
    width: float = Field(default=800, gt=10, le=10000)
    height: float = Field(default=500, gt=10, le=10000)
    background: str = "#172235"
    border: str = "#4d7198"
    locked: bool = False
    collapsed: bool = False
    layer_id: str = "Main"
    z_order: int = 0


class BoardPatch(BaseModel):
    title: str | None = Field(default=None, min_length=1, max_length=200)
    x: float | None = None
    y: float | None = None
    width: float | None = Field(default=None, gt=10, le=10000)
    height: float | None = Field(default=None, gt=10, le=10000)
    background: str | None = None
    border: str | None = None
    locked: bool | None = None
    collapsed: bool | None = None
    layer_id: str | None = None
    z_order: int | None = None


class BoardAttachView(BaseModel):
    local_x: float | None = None
    local_y: float | None = None


class LayerCreate(BaseModel):
    name: str = Field(min_length=1, max_length=100)
    space_id: str | None = None
    visible: bool = True
    locked: bool = False
    sort_order: int = 0


class LayerPatch(BaseModel):
    name: str | None = Field(default=None, min_length=1, max_length=100)
    visible: bool | None = None
    locked: bool | None = None
    sort_order: int | None = None


class LayerDelete(BaseModel):
    mode: str = Field(default="move_to_main", pattern="^(move_to_main|delete_visuals)$")


class QueryViewCreate(BaseModel):
    name: str = Field(min_length=1, max_length=120)
    space_id: str | None = None
    entity_schema: str = ""
    filters: list[dict[str, Any]] = Field(default_factory=list)
    sort: dict[str, Any] = Field(default_factory=dict)
    result_limit: int = Field(default=50, ge=1, le=200)


class QueryViewPatch(BaseModel):
    name: str | None = Field(default=None, min_length=1, max_length=120)
    space_id: str | None = None
    entity_schema: str | None = None
    filters: list[dict[str, Any]] | None = None
    sort: dict[str, Any] | None = None
    result_limit: int | None = Field(default=None, ge=1, le=200)


class WidgetCreate(BaseModel):
    space_id: str
    widget_type: str = Field(min_length=1, max_length=50)
    title: str = Field(default="", max_length=120)
    config: dict[str, Any] = Field(default_factory=dict)
    x: float = 0
    y: float = 0
    width: float = Field(default=320, gt=10, le=10000)
    height: float = Field(default=180, gt=10, le=10000)
    layer_id: str = "Main"
    visible: bool = True


class WidgetPatch(BaseModel):
    title: str | None = Field(default=None, max_length=120)
    config: dict[str, Any] | None = None
    x: float | None = None
    y: float | None = None
    width: float | None = Field(default=None, gt=10, le=10000)
    height: float | None = Field(default=None, gt=10, le=10000)
    layer_id: str | None = None
    visible: bool | None = None


class AcquireRequest(BaseModel):
    worker_id: str = Field(min_length=1, max_length=200)
    # Phase 1 clients provided a display name/free-text task. Keep those
    # fields while making task_id the authoritative Phase 2 association.
    worker_name: str = Field(default="", max_length=200)
    task_id: str | None = None
    task_note: str | None = Field(default=None, max_length=1000)
    task: str = Field(default="", max_length=500)
    branch: str = Field(default="", max_length=500)


class HeartbeatRequest(BaseModel):
    worker_id: str = Field(min_length=1, max_length=200)


def _project_or_404(database: Database, project_id: str) -> dict[str, Any]:
    with database.connect() as connection:
        row = connection.execute("SELECT * FROM projects WHERE id = ?", (project_id,)).fetchone()
    if row is None:
        raise HTTPException(status_code=404, detail="project_not_found")
    return dict(row)


def _space_or_404(database: Database, project_id: str, space_id: str) -> dict[str, Any]:
    with database.connect() as connection:
        row = connection.execute(
            "SELECT * FROM spaces WHERE id = ? AND project_id = ?", (space_id, project_id)
        ).fetchone()
    if row is None:
        raise HTTPException(status_code=404, detail="space_not_found")
    return dict(row)


def _assert_layer_editable(
    connection: sqlite3.Connection, project_id: str, layer_id: str | None,
) -> None:
    if not layer_id:
        return
    row = connection.execute(
        """SELECT locked FROM user_layers
           WHERE project_id = ? AND (id = ? OR name = ?) LIMIT 1""",
        (project_id, layer_id, layer_id),
    ).fetchone()
    if row is not None and bool(row["locked"]):
        raise HTTPException(status_code=409, detail="layer_locked")


def _canonical_layer_name(
    connection: sqlite3.Connection, project_id: str, layer_id: str | None,
) -> str | None:
    """Store visual references by stable user-facing layer name.

    The layer resource itself is addressed by id, while visual payloads have
    historically used the layer name. Accept both forms at every write edge,
    then normalize to the name so a later rename can migrate all visuals.
    """
    if not layer_id:
        return layer_id
    row = connection.execute(
        """SELECT name FROM user_layers
           WHERE project_id = ? AND (id = ? OR name = ?) LIMIT 1""",
        (project_id, layer_id, layer_id),
    ).fetchone()
    return row["name"] if row is not None else layer_id


def _view_dict(row: sqlite3.Row | dict[str, Any]) -> dict[str, Any]:
    value = dict(row)
    value["visible"] = bool(value.get("visible", 1))
    value["revision"] = int(value.get("revision", 1))
    value["visible_properties"] = _decode_json(value.pop("visible_properties_json", "[]"), [])
    return value


def _stroke_dict(row: sqlite3.Row | dict[str, Any]) -> dict[str, Any]:
    value = dict(row)
    value["points"] = _decode_json(value.pop("points_json", "[]"), [])
    return value


def _board_dict(row: sqlite3.Row | dict[str, Any]) -> dict[str, Any]:
    value = dict(row)
    value["locked"] = bool(value.get("locked", 0))
    value["collapsed"] = bool(value.get("collapsed", 0))
    return value


def _layer_dict(row: sqlite3.Row | dict[str, Any]) -> dict[str, Any]:
    value = dict(row)
    value["visible"] = bool(value.get("visible", 1))
    value["locked"] = bool(value.get("locked", 0))
    return value


def _widget_dict(row: sqlite3.Row | dict[str, Any]) -> dict[str, Any]:
    value = dict(row)
    value["config"] = _decode_json(value.pop("config_json", "{}"), {})
    value["visible"] = bool(value.get("visible", 1))
    value["revision"] = int(value.get("revision", 1))
    return value


def _query_dict(
    connection: sqlite3.Connection,
    row: sqlite3.Row | dict[str, Any],
    *,
    include_results: bool = True,
) -> dict[str, Any]:
    value = dict(row)
    value["filters"] = _decode_json(value.pop("filters_json", "[]"), [])
    value["sort"] = _decode_json(value.pop("sort_json", "{}"), {})
    value["result_limit"] = int(value.get("result_limit", 50))
    if include_results:
        value["results"] = _run_query(connection, value)
    return value


def _template_dict(row: sqlite3.Row | dict[str, Any]) -> dict[str, Any]:
    value = dict(row)
    value["config"] = _decode_json(value.pop("config_json", "{}"), {})
    return value


def _property_value(entity: dict[str, Any], name: str) -> Any:
    if name == "entity_type":
        return entity.get("entity_type")
    if name == "title":
        return entity.get("title")
    if name == "description":
        return entity.get("description")
    if name in entity.get("properties", {}):
        return entity["properties"].get(name)
    return entity.get("details", {}).get(name)


def _matches_filters(entity: dict[str, Any], filters: list[dict[str, Any]]) -> bool:
    for item in filters:
        name = str(item.get("property", item.get("field", "")))
        operator = str(item.get("operator", "Equals"))
        expected = item.get("value")
        actual = _property_value(entity, name)
        if operator == "Equals" and actual != expected:
            return False
        if operator == "NotEquals" and actual == expected:
            return False
        if operator == "Contains":
            if isinstance(actual, list):
                if expected not in actual:
                    return False
            elif str(expected).lower() not in str(actual or "").lower():
                return False
    return True


def _run_query(connection: sqlite3.Connection, query: dict[str, Any]) -> list[dict[str, Any]]:
    rows = connection.execute(
        "SELECT * FROM entities WHERE project_id = ? ORDER BY updated_at DESC, id",
        (query["project_id"],),
    ).fetchall()
    entities = [entity_to_dict(connection, row) for row in rows]
    schema_name = str(query.get("entity_schema", ""))
    if schema_name:
        entities = [
            entity for entity in entities
            if entity.get("entity_type") == schema_name or entity.get("schema_id") == schema_name
        ]
    entities = [entity for entity in entities if _matches_filters(entity, query.get("filters", []))]
    sort = query.get("sort") or {}
    sort_property = sort.get("property") or sort.get("field")
    if sort_property:
        entities.sort(
            key=lambda entity: str(_property_value(entity, str(sort_property)) or ""),
            reverse=str(sort.get("direction", "asc")).lower() == "desc",
        )
    return entities[: int(query.get("result_limit", 50))]


def _workspace(database: Database, project_id: str, space_id: str | None = None) -> dict[str, Any]:
    project = _project_or_404(database, project_id)
    with database.connect() as connection:
        spaces = rows_to_dicts(connection.execute(
            "SELECT * FROM spaces WHERE project_id = ? ORDER BY sort_order, rowid", (project_id,)
        ).fetchall())
        entity_rows = connection.execute(
            "SELECT * FROM entities WHERE project_id = ? ORDER BY created_at, id", (project_id,)
        ).fetchall()
        view_sql = """SELECT v.* FROM entity_views v
                      JOIN spaces s ON s.id = v.space_id
                      WHERE s.project_id = ?"""
        view_params: tuple[Any, ...] = (project_id,)
        if space_id is not None:
            view_sql += " AND v.space_id = ?"
            view_params += (space_id,)
        views = [_view_dict(row) for row in connection.execute(view_sql, view_params).fetchall()]
        stroke_sql = "SELECT * FROM ink_strokes WHERE project_id = ?"
        stroke_params: tuple[Any, ...] = (project_id,)
        if space_id is not None:
            stroke_sql += " AND space_id = ?"
            stroke_params += (space_id,)
        stroke_sql += " ORDER BY created_at, id"
        strokes = [_stroke_dict(row) for row in connection.execute(stroke_sql, stroke_params).fetchall()]
        locks = rows_to_dicts(connection.execute(
            "SELECT * FROM scene_locks WHERE project_id = ? ORDER BY scene_id", (project_id,)
        ).fetchall())
        relationships = [relationship_to_dict(row) for row in connection.execute(
            "SELECT * FROM relationships WHERE project_id = ? ORDER BY created_at, id", (project_id,)
        ).fetchall()]
        entities = [entity_to_dict(connection, row) for row in entity_rows]
        layers = rows_to_dicts(connection.execute(
            "SELECT * FROM user_layers WHERE project_id = ? ORDER BY sort_order, rowid", (project_id,)
        ).fetchall())
        schemas = [schema_to_dict(connection, row) for row in connection.execute(
            "SELECT * FROM entity_schemas WHERE project_id = ? ORDER BY name", (project_id,)
        ).fetchall()]
        activities = [_activity_dict(row) for row in connection.execute(
            "SELECT * FROM activities WHERE project_id = ? ORDER BY created_at DESC, id LIMIT 100",
            (project_id,),
        ).fetchall()]
        query_rows = connection.execute(
            "SELECT * FROM query_views WHERE project_id = ? ORDER BY name", (project_id,)
        ).fetchall()
        queries = [_query_dict(connection, row) for row in query_rows]
        for lock in locks:
            lock.update(public_lock(lock) or {})
        for space in spaces:
            space["view_type"] = space.get("view_type", "Graph")
    selected_space = None
    if space_id is not None:
        selected_space = next((space for space in spaces if space["id"] == space_id), None)
        if selected_space is None:
            raise HTTPException(status_code=404, detail="space_not_found")
    elif spaces:
        selected_space = spaces[0]
    with database.connect() as connection:
        selected_id = selected_space["id"] if selected_space is not None else None
        boards = []
        widgets = []
        if selected_id is not None:
            boards = [_board_dict(row) for row in connection.execute(
                "SELECT * FROM boards WHERE project_id = ? AND space_id = ? ORDER BY z_order, id",
                (project_id, selected_id),
            ).fetchall()]
            widgets = [_widget_dict(row) for row in connection.execute(
                "SELECT * FROM dashboard_widgets WHERE project_id = ? AND space_id = ? ORDER BY id",
                (project_id, selected_id),
            ).fetchall()]
    return {
        "project": project,
        "spaces": spaces,
        "current_space": selected_space,
        "entities": entities,
        "views": views,
        "relationships": relationships,
        "strokes": strokes,
        "scene_locks": locks,
        "user_layers": layers,
        "schemas": schemas,
        "boards": boards,
        "query_views": queries,
        "dashboard_widgets": widgets,
        "activities": activities,
    }


def _decode_json(value: str | None, fallback: Any) -> Any:
    try:
        return json.loads(value or "")
    except (TypeError, ValueError):
        return fallback


def _domain_error(error: DomainError) -> HTTPException:
    detail = {"error": error.code}
    detail.update(error.extra)
    return HTTPException(status_code=error.status_code, detail=detail)


def _lock_error(error: Exception) -> HTTPException:
    if isinstance(error, SceneNotFound):
        return HTTPException(status_code=404, detail=error.message)
    if isinstance(error, AlreadyLocked):
        return HTTPException(status_code=409, detail={"error": error.message, "lock": error.lock})
    if isinstance(error, WrongOwner):
        return HTTPException(status_code=409, detail={"error": error.message, "lock": error.lock})
    return HTTPException(status_code=500, detail=str(error))


def _conflict_payload(connection: sqlite3.Connection, entity_id: str, expected_revision: int) -> DomainError:
    row = connection.execute("SELECT * FROM entities WHERE id = ?", (entity_id,)).fetchone()
    current = entity_to_dict(connection, row) if row is not None else None
    return DomainError(
        "revision_conflict",
        409,
        {"entity_id": entity_id, "expected_revision": expected_revision, "current_entity": current},
    )


def _validate_space_view_type(view_type: str) -> None:
    if view_type not in ("Graph", "TaskBoard"):
        raise DomainError("unsupported_space_view_type", 422)


def _sync_relationship_field(
    connection: sqlite3.Connection,
    source: sqlite3.Row,
    target: sqlite3.Row,
    relationship_type: str,
    *,
    clear: bool = False,
) -> None:
    if source["entity_type"] != "task":
        return
    expected_type: str | None = None
    column: str | None = None
    if relationship_type == "assigned_to":
        expected_type, column = "worker", "assigned_worker_id"
    elif relationship_type == "belongs_to_system":
        expected_type, column = "system", "system_id"
    elif relationship_type == "belongs_to_milestone":
        expected_type, column = "milestone", "milestone_id"
    if column is None:
        return
    if target["entity_type"] != expected_type:
        raise DomainError("relationship_type_mismatch", 422, {"relationship_type": relationship_type})
    current = connection.execute(
        f"SELECT {column} FROM task_details WHERE entity_id = ?", (source["id"],)
    ).fetchone()
    if current is None:
        return
    if clear:
        if current[0] == target["id"]:
            connection.execute(f"UPDATE task_details SET {column} = NULL WHERE entity_id = ?", (source["id"],))
    else:
        connection.execute(f"UPDATE task_details SET {column} = ? WHERE entity_id = ?", (target["id"], source["id"]))


async def _broadcast_dependent_entities(
    database: Database,
    manager: ProjectConnectionManager,
    project_id: str,
    entity_ids: set[str],
) -> None:
    if not entity_ids:
        return
    with database.connect() as connection:
        for entity_id in entity_ids:
            row = connection.execute(
                "SELECT * FROM entities WHERE id = ? AND project_id = ?", (entity_id, project_id)
            ).fetchone()
            if row is None:
                continue
            payload = entity_to_dict(connection, row)
            await manager.broadcast(project_id, {
                "type": "entity.updated", "project_id": project_id, "entity": payload,
            })
            if row["entity_type"] == "worker":
                await manager.broadcast(project_id, {
                    "type": "worker.updated", "project_id": project_id, "entity": payload,
                })


async def _record_activity(
    database: Database,
    manager: ProjectConnectionManager,
    project_id: str,
    event_type: str,
    summary: str,
    *,
    entity_id: str | None = None,
    actor_id: str = "",
    actor_name: str = "",
    payload: dict[str, Any] | None = None,
) -> dict[str, Any]:
    activity = {
        "id": f"activity-{uuid.uuid4().hex}",
        "project_id": project_id,
        "actor_id": actor_id,
        "actor_name": actor_name,
        "event_type": event_type,
        "entity_id": entity_id,
        "summary": summary,
        "payload": payload or {},
        "created_at": utc_now(),
    }
    with database.connect() as connection:
        connection.execute(
            """INSERT INTO activities
               (id, project_id, actor_id, actor_name, event_type, entity_id, summary, payload_json, created_at)
               VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)""",
            (
                activity["id"], project_id, actor_id, actor_name, event_type, entity_id,
                summary, json.dumps(activity["payload"], ensure_ascii=False), activity["created_at"],
            ),
        )
    await manager.broadcast(project_id, {
        "type": "activity.created", "project_id": project_id, "activity": activity,
    })
    return activity


def _activity_dict(row: sqlite3.Row | dict[str, Any]) -> dict[str, Any]:
    value = dict(row)
    value["payload"] = _decode_json(value.pop("payload_json", "{}"), {})
    return value


def _property_type(request: PropertyDefinitionCreate | PropertyDefinitionPatch) -> str | None:
    return getattr(request, "property_type", None) or getattr(request, "type", None)


def _validate_property_definition(request: PropertyDefinitionCreate | PropertyDefinitionPatch) -> None:
    property_type = _property_type(request)
    if isinstance(request, PropertyDefinitionCreate) and not property_type:
        raise HTTPException(status_code=422, detail="property_type_required")
    if property_type is not None and property_type not in PROPERTY_TYPES:
        raise HTTPException(status_code=422, detail={
            "error": "unsupported_property_type", "allowed": list(PROPERTY_TYPES),
        })
    options = getattr(request, "options", None)
    if property_type == "Enum" and options is not None and not options:
        raise HTTPException(status_code=422, detail="enum_options_required")
    min_value = getattr(request, "min_value", None)
    max_value = getattr(request, "max_value", None)
    step_value = getattr(request, "step_value", None)
    if property_type is not None and property_type != "Number" and any(
        value is not None for value in (min_value, max_value, step_value)
    ):
        raise HTTPException(status_code=422, detail="number_constraints_require_number_property")
    if min_value is not None and max_value is not None and min_value > max_value:
        raise HTTPException(status_code=422, detail="number_min_above_max")
    if step_value is not None and step_value <= 0:
        raise HTTPException(status_code=422, detail="number_step_must_be_positive")


def _validate_property_definition_change(
    connection: sqlite3.Connection,
    row: sqlite3.Row,
    changes: dict[str, Any],
) -> None:
    current_type = str(row["property_type"])
    next_type = str(changes.get("property_type", current_type))
    if next_type != current_type:
        values = connection.execute(
            "SELECT value_json FROM entity_property_values WHERE property_definition_id = ?",
            (row["id"],),
        ).fetchall()
        used = [value[0] for value in values if _decode_json(value[0], None) is not None]
        if used:
            raise HTTPException(status_code=409, detail={
                "error": "property_type_in_use",
                "property_id": row["id"],
                "property": row["name"],
            })
    if current_type == "Enum" or next_type == "Enum":
        if "options_json" not in changes:
            return
        previous = set(_decode_json(row["options_json"], []))
        next_options = set(_decode_json(changes["options_json"], []))
        removed = previous - next_options
        if not removed:
            return
        used_values = []
        for value in connection.execute(
            "SELECT value_json FROM entity_property_values WHERE property_definition_id = ?",
            (row["id"],),
        ).fetchall():
            decoded = _decode_json(value[0], None)
            if decoded in removed:
                used_values.append(decoded)
        if used_values:
            raise HTTPException(status_code=409, detail={
                "error": "property_option_in_use",
                "property_id": row["id"],
                "property": row["name"],
                "options": sorted(set(used_values)),
            })


def create_app(db_path: str | Path | None = None) -> FastAPI:
    database = Database(db_path or os.environ.get("CRYSTAL_DB_PATH") or DEFAULT_DB_PATH)
    database.initialize()
    lock_service = SceneLockService(database)
    manager = ProjectConnectionManager()
    app = FastAPI(title="Crystal Server", version="0.2.0")
    app.state.database = database
    app.state.lock_service = lock_service
    app.state.connection_manager = manager
    app.add_middleware(
        CORSMiddleware,
        allow_origins=["*"],
        allow_credentials=False,
        allow_methods=["*"],
        allow_headers=["*"],
    )

    @app.get("/api/v1/health")
    def health() -> dict[str, str]:
        return {"status": "ok", "service": "crystal-server", "schema_version": "4"}

    @app.get("/api/v1/projects")
    def list_projects() -> dict[str, Any]:
        with database.connect() as connection:
            projects = rows_to_dicts(connection.execute("SELECT * FROM projects ORDER BY id").fetchall())
        return {"projects": projects}

    @app.get("/api/v1/project-templates")
    def list_project_templates() -> dict[str, Any]:
        with database.connect() as connection:
            rows = connection.execute(
                "SELECT * FROM project_templates ORDER BY name, id"
            ).fetchall()
        return {"templates": [_template_dict(row) for row in rows]}

    @app.post("/api/v1/projects/{project_id}/project-templates", status_code=201)
    async def save_project_template(project_id: str, request: TemplateSave) -> dict[str, Any]:
        _project_or_404(database, project_id)
        template_id = f"template-{uuid.uuid4().hex}"
        now = utc_now()
        with database.connect() as connection:
            spaces = connection.execute(
                "SELECT id, name, view_type, sort_order FROM spaces WHERE project_id = ? ORDER BY sort_order, rowid",
                (project_id,),
            ).fetchall()
            space_names = {row["id"]: row["name"] for row in spaces}
            boards: list[dict[str, Any]] = []
            for row in connection.execute(
                "SELECT * FROM boards WHERE project_id = ? ORDER BY space_id, z_order, id", (project_id,)
            ).fetchall():
                board = dict(row)
                board["space"] = space_names.get(board.pop("space_id"), "")
                board.pop("project_id", None)
                board.pop("id", None)
                boards.append(board)
            config: dict[str, Any] = {
                "spaces": [dict(row) for row in spaces],
                "schemas": [schema_to_dict(connection, row) for row in connection.execute(
                    "SELECT * FROM entity_schemas WHERE project_id = ? ORDER BY name", (project_id,)
                ).fetchall()],
                "boards": boards,
                "widgets": [],
            }
            for row in connection.execute(
                "SELECT * FROM dashboard_widgets WHERE project_id = ? ORDER BY space_id, id", (project_id,)
            ).fetchall():
                widget = _widget_dict(row)
                widget["space"] = space_names.get(widget.pop("space_id"), "")
                widget.pop("project_id", None)
                widget.pop("id", None)
                widget.pop("revision", None)
                config["widgets"].append(widget)
            try:
                connection.execute(
                    """INSERT INTO project_templates
                       (id, name, template_type, config_json, created_at, updated_at)
                       VALUES (?, ?, ?, ?, ?, ?)""",
                    (template_id, request.name, request.template_type,
                     json.dumps(config, ensure_ascii=False), now, now),
                )
            except sqlite3.IntegrityError as error:
                raise HTTPException(status_code=409, detail="project_template_name_already_exists") from error
            payload = _template_dict(connection.execute(
                "SELECT * FROM project_templates WHERE id = ?", (template_id,)
            ).fetchone())
        await manager.broadcast(project_id, {
            "type": "project_template.created", "project_id": project_id, "template": payload,
        })
        await _record_activity(
            database, manager, project_id, "Project Template Saved", request.name,
            payload={"template_id": template_id},
        )
        return payload

    @app.post("/api/v1/projects/{project_id}/templates/apply")
    async def apply_project_template(project_id: str, request: TemplateApply) -> dict[str, Any]:
        _project_or_404(database, project_id)
        with database.connect() as connection:
            template_row = connection.execute(
                "SELECT * FROM project_templates WHERE id = ?", (request.template_id,)
            ).fetchone()
            if template_row is None:
                raise HTTPException(status_code=404, detail="project_template_not_found")
            config = _decode_json(template_row["config_json"], {})
            created_spaces: list[dict[str, Any]] = []
            created_schemas: list[dict[str, Any]] = []
            created_widgets: list[dict[str, Any]] = []
            space_ids: dict[str, str] = {}
            for item in config.get("spaces", []):
                name = str(item.get("name", "")).strip()
                if not name:
                    continue
                current = connection.execute(
                    "SELECT * FROM spaces WHERE project_id = ? AND name = ?", (project_id, name)
                ).fetchone()
                if current is None:
                    space_id = f"{project_id}:{uuid.uuid4().hex[:12]}"
                    view_type = str(item.get("view_type", "Graph"))
                    _validate_space_view_type(view_type)
                    connection.execute(
                        """INSERT INTO spaces(id, project_id, name, view_type, sort_order, created_at)
                           VALUES (?, ?, ?, ?, ?, ?)""",
                        (space_id, project_id, name, view_type, int(item.get("sort_order", 0)), utc_now()),
                    )
                    current = connection.execute(
                        "SELECT * FROM spaces WHERE id = ?", (space_id,)
                    ).fetchone()
                    created_spaces.append(dict(current))
                space_ids[name] = current["id"]
            for item in config.get("schemas", []):
                name = str(item.get("name", "")).strip()
                if not name:
                    continue
                current = connection.execute(
                    "SELECT * FROM entity_schemas WHERE project_id = ? AND name = ?", (project_id, name)
                ).fetchone()
                if current is not None:
                    continue
                schema_id = f"schema-{uuid.uuid4().hex}"
                now = utc_now()
                connection.execute(
                    """INSERT INTO entity_schemas
                       (id, project_id, name, display_name, icon, description, created_at, updated_at)
                       VALUES (?, ?, ?, ?, ?, ?, ?, ?)""",
                    (schema_id, project_id, name, str(item.get("display_name", name)),
                     str(item.get("icon", "")), str(item.get("description", "")), now, now),
                )
                for index, definition in enumerate(item.get("properties", [])):
                    property_name = str(definition.get("name", "")).strip()
                    if not property_name:
                        continue
                    property_type = definition.get("property_type") or definition.get("type") or "Text"
                    if property_type not in PROPERTY_TYPES:
                        raise HTTPException(status_code=422, detail="unsupported_property_type")
                    connection.execute(
                        """INSERT INTO property_definitions
                         (id, schema_id, name, display_name, property_type, default_value_json,
                          required, options_json, min_value, max_value, step_value, sort_order,
                          created_at, updated_at)
                         VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)""",
                        (f"{schema_id}:property:{property_name}", schema_id, property_name,
                         str(definition.get("display_name", property_name)), property_type,
                         json.dumps(definition.get("default_value"), ensure_ascii=False),
                         int(bool(definition.get("required", False))),
                         json.dumps(definition.get("options", []), ensure_ascii=False),
                         int(definition.get("sort_order", index)), now, now),
                    )
                current = connection.execute(
                    "SELECT * FROM entity_schemas WHERE id = ?", (schema_id,)
                ).fetchone()
                created_schemas.append(schema_to_dict(connection, current))
            for item in config.get("boards", []):
                space_id = space_ids.get(str(item.get("space", "")))
                if space_id is None:
                    continue
                title = str(item.get("title", "Board"))
                existing = connection.execute(
                    "SELECT id FROM boards WHERE project_id = ? AND space_id = ? AND title = ?",
                    (project_id, space_id, title),
                ).fetchone()
                if existing is not None:
                    continue
                board_id = f"board-{uuid.uuid4().hex}"
                now = utc_now()
                connection.execute(
                    """INSERT INTO boards
                       (id, project_id, space_id, title, x, y, width, height, background, border,
                        locked, collapsed, layer_id, z_order, created_at, updated_at)
                       VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)""",
                    (board_id, project_id, space_id, title, float(item.get("x", 0)), float(item.get("y", 0)),
                     float(item.get("width", 800)), float(item.get("height", 500)),
                     str(item.get("background", "#172235")), str(item.get("border", "#4d7198")),
                      int(bool(item.get("locked", False))), int(bool(item.get("collapsed", False))),
                      str(item.get("layer_id", "Main")),
                     int(item.get("z_order", 0)), now, now),
                )
            for item in config.get("widgets", []):
                space_id = space_ids.get(str(item.get("space", "")))
                if space_id is None:
                    continue
                title = str(item.get("title", ""))
                existing = connection.execute(
                    """SELECT id FROM dashboard_widgets
                       WHERE project_id = ? AND space_id = ? AND title = ?""",
                    (project_id, space_id, title),
                ).fetchone()
                if existing is not None:
                    continue
                widget_id = f"widget-{uuid.uuid4().hex}"
                widget_config = dict(item.get("config", {}))
                query_config = widget_config.pop("query", None)
                if isinstance(query_config, dict):
                    query_id = f"query-{uuid.uuid4().hex}"
                    query_name = f"{title} Query" if title else f"{item.get('widget_type', 'Widget')} Query"
                    connection.execute(
                        """INSERT OR IGNORE INTO query_views
                           (id, project_id, space_id, name, entity_schema, filters_json, sort_json,
                            result_limit, created_at, updated_at)
                           VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)""",
                        (query_id, project_id, space_id, query_name,
                         str(query_config.get("entity_schema", "")),
                         json.dumps(query_config.get("filters", []), ensure_ascii=False),
                         json.dumps(query_config.get("sort", {}), ensure_ascii=False),
                         int(query_config.get("result_limit", 50)), utc_now(), utc_now()),
                    )
                    widget_config["query_view_id"] = query_id
                now = utc_now()
                connection.execute(
                    """INSERT INTO dashboard_widgets
                       (id, project_id, space_id, widget_type, title, config_json, x, y, width, height,
                        layer_id, visible, revision, created_at, updated_at)
                       VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 1, ?, ?)""",
                    (widget_id, project_id, space_id, str(item.get("widget_type", "Text")), title,
                     json.dumps(widget_config, ensure_ascii=False), float(item.get("x", 24)),
                     float(item.get("y", 24 + len(created_widgets) * 200)), float(item.get("width", 320)),
                     float(item.get("height", 180)), str(item.get("layer_id", "Main")),
                     int(bool(item.get("visible", True))), now, now),
                )
                created_widgets.append(_widget_dict(connection.execute(
                    "SELECT * FROM dashboard_widgets WHERE id = ?", (widget_id,)
                ).fetchone()))
        await manager.broadcast(project_id, {
            "type": "workspace.updated", "project_id": project_id,
            "reason": "project_template_applied", "template_id": request.template_id,
        })
        await _record_activity(
            database, manager, project_id, "Project Template Applied",
            str(template_row["name"]), payload={"template_id": request.template_id},
        )
        return {
            "template": _template_dict(template_row),
            "created": {"spaces": created_spaces, "schemas": created_schemas, "widgets": created_widgets},
            "workspace": _workspace(database, project_id),
        }

    @app.get("/api/v1/projects/{project_id}")
    def get_project(project_id: str) -> dict[str, Any]:
        return _workspace(database, project_id)

    @app.get("/api/v1/projects/{project_id}/spaces")
    def list_spaces(project_id: str) -> dict[str, Any]:
        _project_or_404(database, project_id)
        with database.connect() as connection:
            spaces = rows_to_dicts(connection.execute(
                "SELECT * FROM spaces WHERE project_id = ? ORDER BY sort_order, rowid", (project_id,)
            ).fetchall())
        return {"spaces": spaces}

    @app.post("/api/v1/projects/{project_id}/spaces", status_code=201)
    async def create_space(project_id: str, request: SpaceCreate) -> dict[str, Any]:
        _project_or_404(database, project_id)
        try:
            _validate_space_view_type(request.view_type)
            space_id = f"{project_id}:{uuid.uuid4().hex[:12]}"
            with database.connect() as connection:
                connection.execute(
                    """INSERT INTO spaces(id, project_id, name, created_at, view_type)
                       VALUES (?, ?, ?, ?, ?)""",
                    (space_id, project_id, request.name, utc_now(), request.view_type),
                )
                connection.execute(
                    "UPDATE spaces SET sort_order = ? WHERE id = ?",
                    (request.sort_order, space_id),
                )
                row = connection.execute("SELECT * FROM spaces WHERE id = ?", (space_id,)).fetchone()
            payload = dict(row)
            await manager.broadcast(project_id, {"type": "space.created", "project_id": project_id, "space": payload})
            return payload
        except DomainError as error:
            raise _domain_error(error) from error
        except sqlite3.IntegrityError as error:
            raise HTTPException(status_code=409, detail="space_name_already_exists") from error

    @app.patch("/api/v1/projects/{project_id}/spaces/{space_id}")
    async def patch_space(project_id: str, space_id: str, request: SpacePatch) -> dict[str, Any]:
        _space_or_404(database, project_id, space_id)
        changes = _dump(request)
        changes = {key: value for key, value in changes.items() if value is not None}
        if "view_type" in changes:
            _validate_space_view_type(changes["view_type"])
        if not changes:
            return _space_or_404(database, project_id, space_id)
        assignments = ", ".join(f"{key} = ?" for key in changes)
        try:
            with database.connect() as connection:
                connection.execute(
                    f"UPDATE spaces SET {assignments} WHERE id = ? AND project_id = ?",
                    (*changes.values(), space_id, project_id),
                )
                row = connection.execute("SELECT * FROM spaces WHERE id = ?", (space_id,)).fetchone()
            payload = dict(row)
            await manager.broadcast(project_id, {
                "type": "space.updated", "project_id": project_id, "space": payload,
            })
            return payload
        except sqlite3.IntegrityError as error:
            raise HTTPException(status_code=409, detail="space_name_already_exists") from error

    @app.post("/api/v1/projects/{project_id}/spaces/{space_id}/duplicate", status_code=201)
    async def duplicate_space(project_id: str, space_id: str, request: SpaceDuplicate) -> dict[str, Any]:
        source = _space_or_404(database, project_id, space_id)
        new_id = f"{project_id}:{uuid.uuid4().hex[:12]}"
        now = utc_now()
        try:
            with database.connect() as connection:
                connection.execute(
                    """INSERT INTO spaces
                       (id, project_id, name, camera_x, camera_y, zoom, view_type, sort_order, created_at)
                       VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)""",
                    (new_id, project_id, request.name, source["camera_x"], source["camera_y"],
                     source["zoom"], source.get("view_type", "Graph"), source.get("sort_order", 0), now),
                )
                board_ids: dict[str, str] = {}
                for board in connection.execute(
                    "SELECT * FROM boards WHERE project_id = ? AND space_id = ?", (project_id, space_id)
                ).fetchall():
                    copied_id = f"{project_id}:board:{uuid.uuid4().hex[:12]}"
                    board_ids[board["id"]] = copied_id
                    connection.execute(
                        """INSERT INTO boards
                           (id, project_id, space_id, title, x, y, width, height, background, border,
                            locked, collapsed, layer_id, z_order, created_at, updated_at)
                           VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)""",
                        (copied_id, project_id, new_id, board["title"], board["x"], board["y"],
                         board["width"], board["height"], board["background"], board["border"],
                          board["locked"], board["collapsed"], board["layer_id"], board["z_order"],
                          board["created_at"], now),
                    )
                for view in connection.execute(
                    """SELECT * FROM entity_views v JOIN entities e ON e.id = v.entity_id
                       WHERE e.project_id = ? AND v.space_id = ?""", (project_id, space_id)
                ).fetchall():
                    copied_view_id = f"view:{view['entity_id']}:{new_id}"
                    connection.execute(
                        """INSERT INTO entity_views
                           (id, entity_id, space_id, x, y, width, height, view_type, layer_id, visible,
                            visible_properties_json, board_id, board_local_x, board_local_y, revision)
                           VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)""",
                        (copied_view_id, view["entity_id"], new_id, view["x"], view["y"], view["width"],
                         view["height"], view["view_type"], view["layer_id"], view["visible"],
                         view["visible_properties_json"], board_ids.get(view["board_id"]),
                         view["board_local_x"], view["board_local_y"], 1),
                    )
                for stroke in connection.execute(
                    "SELECT * FROM ink_strokes WHERE project_id = ? AND space_id = ?", (project_id, space_id)
                ).fetchall():
                    connection.execute(
                        """INSERT INTO ink_strokes
                           (id, project_id, space_id, points_json, width, tool, shape, color, layer_id,
                            group_id, anchor_entity_id, anchor_board_id, local_offset_x, local_offset_y, created_at)
                           VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)""",
                        (f"stroke-{uuid.uuid4().hex}", project_id, new_id, stroke["points_json"], stroke["width"],
                         stroke["tool"], stroke["shape"], stroke["color"], stroke["layer_id"], stroke["group_id"],
                         stroke["anchor_entity_id"], board_ids.get(stroke["anchor_board_id"]),
                         stroke["local_offset_x"], stroke["local_offset_y"], now),
                    )
                for widget in connection.execute(
                    "SELECT * FROM dashboard_widgets WHERE project_id = ? AND space_id = ?", (project_id, space_id)
                ).fetchall():
                    connection.execute(
                        """INSERT INTO dashboard_widgets
                           (id, project_id, space_id, widget_type, title, config_json, x, y, width, height,
                            layer_id, visible, revision, created_at, updated_at)
                           VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 1, ?, ?)""",
                        (f"widget-{uuid.uuid4().hex}", project_id, new_id, widget["widget_type"], widget["title"],
                         widget["config_json"], widget["x"], widget["y"], widget["width"], widget["height"],
                         widget["layer_id"], widget["visible"], now, now),
                    )
                row = connection.execute("SELECT * FROM spaces WHERE id = ?", (new_id,)).fetchone()
            payload = dict(row)
            await manager.broadcast(project_id, {
                "type": "space.created", "project_id": project_id, "space": payload,
            })
            return payload
        except sqlite3.IntegrityError as error:
            raise HTTPException(status_code=409, detail="space_name_already_exists") from error

    @app.delete("/api/v1/projects/{project_id}/spaces/{space_id}", status_code=204, response_model=None)
    async def delete_space(project_id: str, space_id: str) -> None:
        _space_or_404(database, project_id, space_id)
        with database.connect() as connection:
            count = connection.execute(
                "SELECT COUNT(*) FROM spaces WHERE project_id = ?", (project_id,)
            ).fetchone()[0]
            if count <= 1:
                raise HTTPException(status_code=409, detail="last_space_cannot_be_deleted")
            connection.execute("DELETE FROM spaces WHERE id = ? AND project_id = ?", (space_id, project_id))
        await manager.broadcast(project_id, {
            "type": "space.deleted", "project_id": project_id, "space_id": space_id,
        })

    @app.get("/api/v1/projects/{project_id}/workspace")
    def get_workspace(project_id: str, space_id: str | None = Query(default=None)) -> dict[str, Any]:
        return _workspace(database, project_id, space_id)

    @app.get("/api/v1/projects/{project_id}/schemas")
    def list_schemas(project_id: str) -> dict[str, Any]:
        _project_or_404(database, project_id)
        with database.connect() as connection:
            schemas = [schema_to_dict(connection, row) for row in connection.execute(
                "SELECT * FROM entity_schemas WHERE project_id = ? ORDER BY name", (project_id,)
            ).fetchall()]
        return {"schemas": schemas}

    @app.post("/api/v1/projects/{project_id}/schemas", status_code=201)
    async def create_schema(project_id: str, request: SchemaCreate) -> dict[str, Any]:
        _project_or_404(database, project_id)
        if len({str(item.get("name", "")) for item in request.properties}) != len(request.properties):
            raise HTTPException(status_code=409, detail="duplicate_property_name")
        schema_id = f"schema-{uuid.uuid4().hex}"
        now = utc_now()
        try:
            with database.connect() as connection:
                connection.execute(
                    """INSERT INTO entity_schemas
                       (id, project_id, name, display_name, icon, description, created_at, updated_at)
                       VALUES (?, ?, ?, ?, ?, ?, ?, ?)""",
                    (schema_id, project_id, request.name, request.display_name, request.icon,
                     request.description, now, now),
                )
                for index, item in enumerate(request.properties):
                    property_request = PropertyDefinitionCreate(**item)
                    _validate_property_definition(property_request)
                    property_type = _property_type(property_request)
                    if property_request.required and property_request.default_value is None:
                        raise HTTPException(status_code=422, detail="required_property_default_missing")
                    connection.execute(
                        """INSERT INTO property_definitions
                        (id, schema_id, name, display_name, property_type, default_value_json,
                         required, options_json, min_value, max_value, step_value, sort_order,
                         created_at, updated_at)
                        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)""",
                        (
                            f"{schema_id}:property:{property_request.name}", schema_id, property_request.name,
                             property_request.display_name, property_type,
                            json.dumps(property_request.default_value, ensure_ascii=False),
                             int(property_request.required), json.dumps(property_request.options, ensure_ascii=False),
                             property_request.min_value, property_request.max_value, property_request.step_value,
                             property_request.sort_order if property_request.sort_order else index, now, now,
                        ),
                    )
                row = connection.execute("SELECT * FROM entity_schemas WHERE id = ?", (schema_id,)).fetchone()
                payload = schema_to_dict(connection, row)
            await manager.broadcast(project_id, {
                "type": "schema.created", "project_id": project_id, "schema": payload,
            })
            await _record_activity(database, manager, project_id, "Schema Created", request.display_name)
            return payload
        except sqlite3.IntegrityError as error:
            raise HTTPException(status_code=409, detail="schema_name_already_exists") from error

    @app.get("/api/v1/schemas/{schema_id}")
    def get_schema(schema_id: str) -> dict[str, Any]:
        with database.connect() as connection:
            row = connection.execute("SELECT * FROM entity_schemas WHERE id = ?", (schema_id,)).fetchone()
            if row is None:
                raise HTTPException(status_code=404, detail="schema_not_found")
            return schema_to_dict(connection, row)

    @app.patch("/api/v1/schemas/{schema_id}")
    async def patch_schema(schema_id: str, request: SchemaPatch) -> dict[str, Any]:
        changes = {key: value for key, value in _dump(request).items() if value is not None}
        if not changes:
            return get_schema(schema_id)
        with database.connect() as connection:
            row = connection.execute("SELECT * FROM entity_schemas WHERE id = ?", (schema_id,)).fetchone()
            if row is None:
                raise HTTPException(status_code=404, detail="schema_not_found")
            assignments = ", ".join(f"{key} = ?" for key in changes)
            connection.execute(
                f"UPDATE entity_schemas SET {assignments}, updated_at = ? WHERE id = ?",
                (*changes.values(), utc_now(), schema_id),
            )
            updated = connection.execute("SELECT * FROM entity_schemas WHERE id = ?", (schema_id,)).fetchone()
            payload = schema_to_dict(connection, updated)
            project_id = row["project_id"]
        await manager.broadcast(project_id, {
            "type": "schema.updated", "project_id": project_id, "schema": payload,
        })
        return payload

    @app.delete("/api/v1/schemas/{schema_id}", status_code=204, response_model=None)
    async def delete_schema(schema_id: str) -> None:
        with database.connect() as connection:
            row = connection.execute("SELECT * FROM entity_schemas WHERE id = ?", (schema_id,)).fetchone()
            if row is None:
                raise HTTPException(status_code=404, detail="schema_not_found")
            if connection.execute(
                "SELECT COUNT(*) FROM entities WHERE schema_id = ?", (schema_id,)
            ).fetchone()[0] > 0:
                raise HTTPException(status_code=409, detail="schema_in_use")
            project_id = row["project_id"]
            connection.execute("DELETE FROM entity_schemas WHERE id = ?", (schema_id,))
        await manager.broadcast(project_id, {
            "type": "schema.deleted", "project_id": project_id, "schema_id": schema_id,
        })

    @app.post("/api/v1/schemas/{schema_id}/properties", status_code=201)
    async def create_property(schema_id: str, request: PropertyDefinitionCreate) -> dict[str, Any]:
        _validate_property_definition(request)
        property_type = _property_type(request)
        with database.connect() as connection:
            schema = connection.execute("SELECT * FROM entity_schemas WHERE id = ?", (schema_id,)).fetchone()
            if schema is None:
                raise HTTPException(status_code=404, detail="schema_not_found")
            property_id = f"{schema_id}:property:{request.name}"
            try:
                connection.execute(
                    """INSERT INTO property_definitions
                       (id, schema_id, name, display_name, property_type, default_value_json,
                        required, options_json, min_value, max_value, step_value, sort_order,
                        created_at, updated_at)
                       VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)""",
                    (property_id, schema_id, request.name, request.display_name, property_type,
                     json.dumps(request.default_value, ensure_ascii=False), int(request.required),
                     json.dumps(request.options, ensure_ascii=False), request.min_value, request.max_value,
                     request.step_value, request.sort_order, utc_now(), utc_now()),
                )
            except sqlite3.IntegrityError as error:
                raise HTTPException(status_code=409, detail="property_name_already_exists") from error
            property_row = connection.execute(
                "SELECT * FROM property_definitions WHERE id = ?", (property_id,)
            ).fetchone()
            payload = property_definition_to_dict(property_row)
            project_id = schema["project_id"]
        await manager.broadcast(project_id, {
            "type": "schema.updated", "project_id": project_id,
            "schema_id": schema_id, "property": payload,
        })
        return payload

    @app.patch("/api/v1/property-definitions/{property_id}")
    async def patch_property(property_id: str, request: PropertyDefinitionPatch) -> dict[str, Any]:
        _validate_property_definition(request)
        changes = {key: value for key, value in _dump(request).items() if value is not None}
        with database.connect() as connection:
            row = connection.execute(
                """SELECT pd.*, es.project_id FROM property_definitions pd
                   JOIN entity_schemas es ON es.id = pd.schema_id WHERE pd.id = ?""", (property_id,)
            ).fetchone()
            if row is None:
                raise HTTPException(status_code=404, detail="property_not_found")
            encoded = {
                "default_value": "default_value_json",
                "options": "options_json",
            }
            values: dict[str, Any] = {}
            for key, value in changes.items():
                if key == "type":
                    key = "property_type"
                column = encoded.get(key, key)
                values[column] = json.dumps(value, ensure_ascii=False) if key in encoded else value
            _validate_property_definition_change(connection, row, values)
            assignments = ", ".join(f"{key} = ?" for key in values)
            if assignments:
                connection.execute(
                    f"UPDATE property_definitions SET {assignments}, updated_at = ? WHERE id = ?",
                    (*values.values(), utc_now(), property_id),
                )
            updated = connection.execute(
                "SELECT * FROM property_definitions WHERE id = ?", (property_id,)
            ).fetchone()
            payload = property_definition_to_dict(updated)
            project_id = row["project_id"]
        await manager.broadcast(project_id, {
            "type": "schema.updated", "project_id": project_id,
            "schema_id": row["schema_id"], "property": payload,
        })
        return payload

    @app.delete("/api/v1/property-definitions/{property_id}", status_code=204, response_model=None)
    async def delete_property(property_id: str) -> None:
        with database.connect() as connection:
            row = connection.execute(
                """SELECT pd.*, es.project_id FROM property_definitions pd
                   JOIN entity_schemas es ON es.id = pd.schema_id WHERE pd.id = ?""", (property_id,)
            ).fetchone()
            if row is None:
                raise HTTPException(status_code=404, detail="property_not_found")
            connection.execute("DELETE FROM property_definitions WHERE id = ?", (property_id,))
            project_id = row["project_id"]
        await manager.broadcast(project_id, {
            "type": "schema.updated", "project_id": project_id,
            "schema_id": row["schema_id"], "property_id": property_id,
        })

    @app.patch("/api/v1/projects/{project_id}/spaces/{space_id}/camera")
    def update_camera(project_id: str, space_id: str, request: CameraUpdate) -> dict[str, Any]:
        _space_or_404(database, project_id, space_id)
        with database.connect() as connection:
            connection.execute(
                "UPDATE spaces SET camera_x = ?, camera_y = ?, zoom = ? WHERE id = ?",
                (request.camera_x, request.camera_y, request.zoom, space_id),
            )
            row = connection.execute("SELECT * FROM spaces WHERE id = ?", (space_id,)).fetchone()
        return dict(row)

    @app.get("/api/v1/projects/{project_id}/search")
    def search_entities(project_id: str, q: str = Query(min_length=1, max_length=200)) -> dict[str, Any]:
        _project_or_404(database, project_id)
        with database.connect() as connection:
            rows = connection.execute(
                """SELECT * FROM entities WHERE project_id = ?
                   AND (title LIKE ? OR description LIKE ?)
                   ORDER BY updated_at DESC, id LIMIT 50""",
                (project_id, f"%{q}%", f"%{q}%"),
            ).fetchall()
            entities = [entity_to_dict(connection, row) for row in rows]
        return {"query": q, "entities": entities}

    @app.post("/api/v1/entities", status_code=201)
    async def create_entity(request: EntityCreate) -> dict[str, Any]:
        _project_or_404(database, request.project_id)
        now = utc_now()
        entity_id = f"entity-{uuid.uuid4().hex}"
        raw_details = dict(request.data)
        raw_details.update(request.details)
        try:
            with database.connect() as connection:
                schema = validate_entity_type(
                    connection, request.project_id, request.entity_type, request.schema_id
                )
                resolved_schema_id = schema["id"] if schema is not None else None
                if resolved_schema_id is not None:
                    custom_properties = normalize_custom_properties(
                        connection, request.project_id, resolved_schema_id,
                        {**raw_details, **request.properties},
                    )
                    details = custom_properties
                else:
                    custom_properties = {}
                    details = normalize_details(connection, request.project_id, request.entity_type, raw_details)
                connection.execute(
                    """INSERT INTO entities
                       (id, project_id, entity_type, schema_id, title, description, data_json, revision, created_at, updated_at)
                       VALUES (?, ?, ?, ?, ?, ?, ?, 1, ?, ?)""",
                    (
                        entity_id, request.project_id, request.entity_type, resolved_schema_id,
                        request.title, request.description, json.dumps(request.data, ensure_ascii=False), now, now,
                    ),
                )
                write_details(
                    connection, entity_id, request.entity_type, details,
                    schema_id=resolved_schema_id, properties=custom_properties,
                )
                row = connection.execute("SELECT * FROM entities WHERE id = ?", (entity_id,)).fetchone()
                payload = entity_to_dict(connection, row)
            await manager.broadcast(request.project_id, {
                "type": "entity.created", "project_id": request.project_id, "entity": payload,
            })
            if request.entity_type == "worker":
                await manager.broadcast(request.project_id, {
                    "type": "worker.updated", "project_id": request.project_id, "entity": payload,
                })
            await _record_activity(
                database, manager, request.project_id, "Entity Created",
                f"{payload['title']} created", entity_id=entity_id,
                payload={"entity": payload},
            )
            return payload
        except DomainError as error:
            raise _domain_error(error) from error
        except sqlite3.IntegrityError as error:
            raise HTTPException(status_code=409, detail="entity_detail_conflict") from error

    @app.get("/api/v1/entities/{entity_id}")
    def get_entity(entity_id: str) -> dict[str, Any]:
        with database.connect() as connection:
            row = connection.execute("SELECT * FROM entities WHERE id = ?", (entity_id,)).fetchone()
            if row is None:
                raise HTTPException(status_code=404, detail="entity_not_found")
            return entity_to_dict(connection, row)

    @app.patch("/api/v1/entities/{entity_id}")
    async def patch_entity(entity_id: str, request: EntityPatch) -> dict[str, Any]:
        dependent_ids: set[str] = set()
        worker_ids: set[str] = set()
        try:
            with database.connect() as connection:
                connection.execute("BEGIN IMMEDIATE")
                row = connection.execute("SELECT * FROM entities WHERE id = ?", (entity_id,)).fetchone()
                if row is None:
                    connection.rollback()
                    raise DomainError("entity_not_found", 404)
                project_id = row["project_id"]
                old_details = stored_details(connection, row)
                if request.schema_id is not None and request.schema_id != row["schema_id"]:
                    connection.rollback()
                    raise DomainError("entity_schema_cannot_change", 409)
                if request.expected_revision is not None and row["revision"] != request.expected_revision:
                    error = _conflict_payload(connection, entity_id, request.expected_revision)
                    connection.rollback()
                    raise error
                title = request.title if request.title is not None else row["title"]
                description = request.description if request.description is not None else row["description"]
                data = request.data if request.data is not None else _decode_json(row["data_json"], {})
                details = dict(old_details)
                if request.data is not None:
                    details.update(request.data)
                if request.details is not None:
                    details.update(request.details)
                custom_properties: dict[str, Any] = {}
                if row["schema_id"]:
                    custom_properties = normalize_custom_properties(
                        connection, project_id, row["schema_id"],
                        request.properties if request.properties is not None else request.details,
                        existing=old_details,
                    )
                    details = custom_properties
                else:
                    details = normalize_details(connection, project_id, row["entity_type"], details)
                for candidate in (old_details.get("assigned_worker_id"), details.get("assigned_worker_id")):
                    if candidate:
                        worker_ids.add(candidate)
                for candidate in (old_details.get("milestone_id"), details.get("milestone_id")):
                    if candidate:
                        dependent_ids.add(candidate)
                if row["entity_type"] == "task":
                    dependent_ids.update(
                        relationship["target_entity_id"]
                        for relationship in connection.execute(
                            """SELECT target_entity_id FROM relationships
                               WHERE source_entity_id = ? AND relationship_type = 'belongs_to_milestone'""",
                            (entity_id,),
                        ).fetchall()
                    )
                connection.execute(
                    """UPDATE entities SET title = ?, description = ?, data_json = ?,
                       revision = revision + 1, updated_at = ? WHERE id = ?""",
                    (title, description, json.dumps(data, ensure_ascii=False), utc_now(), entity_id),
                )
                write_details(
                    connection, entity_id, row["entity_type"], details,
                    schema_id=row["schema_id"], properties=custom_properties,
                )
                updated = connection.execute("SELECT * FROM entities WHERE id = ?", (entity_id,)).fetchone()
                payload = entity_to_dict(connection, updated)
                connection.commit()
            await manager.broadcast(project_id, {
                "type": "entity.updated", "project_id": project_id, "entity": payload,
            })
            if payload["entity_type"] == "worker":
                await manager.broadcast(project_id, {
                    "type": "worker.updated", "project_id": project_id, "entity": payload,
                })
            await _broadcast_dependent_entities(database, manager, project_id, dependent_ids | worker_ids)
            activity_type = "Entity Status Changed" if old_details.get("status") != payload["details"].get("status") else "Entity Updated"
            await _record_activity(
                database, manager, project_id, activity_type,
                f"{payload['title']} updated", entity_id=entity_id,
                payload={"entity": payload},
            )
            return payload
        except DomainError as error:
            raise _domain_error(error) from error
        except sqlite3.IntegrityError as error:
            raise HTTPException(status_code=409, detail="entity_detail_conflict") from error

    @app.delete("/api/v1/entities/{entity_id}", status_code=204, response_model=None)
    async def delete_entity(entity_id: str) -> None:
        try:
            with database.connect() as connection:
                connection.execute("BEGIN IMMEDIATE")
                entity = connection.execute("SELECT * FROM entities WHERE id = ?", (entity_id,)).fetchone()
                if entity is None:
                    connection.rollback()
                    raise DomainError("entity_not_found", 404)
                project_id = entity["project_id"]
                if entity["entity_type"] == "scene":
                    lock = connection.execute(
                        "SELECT * FROM scene_locks WHERE scene_id = ? AND status = 'LOCKED'", (entity_id,)
                    ).fetchone()
                    if lock is not None:
                        connection.rollback()
                        raise DomainError("scene_locked", 409, {"lock": public_lock(lock)})
                views = rows_to_dicts(connection.execute(
                    "SELECT * FROM entity_views WHERE entity_id = ?", (entity_id,)
                ).fetchall())
                relationships = rows_to_dicts(connection.execute(
                    "SELECT * FROM relationships WHERE source_entity_id = ? OR target_entity_id = ?",
                    (entity_id, entity_id),
                ).fetchall())
                connection.execute("DELETE FROM entities WHERE id = ?", (entity_id,))
                connection.commit()
            for view in views:
                await manager.broadcast(project_id, {
                    "type": "view.deleted", "project_id": project_id, "view_id": view["id"],
                    "entity_id": entity_id, "space_id": view["space_id"],
                })
            for relationship in relationships:
                await manager.broadcast(project_id, {
                    "type": "relationship.deleted", "project_id": project_id,
                    "relationship": relationship,
                })
            await manager.broadcast(project_id, {
                "type": "entity.deleted", "project_id": project_id, "entity_id": entity_id,
            })
            await _record_activity(
                database, manager, project_id, "Entity Deleted",
                f"{entity['title']} deleted", entity_id=entity_id,
            )
        except DomainError as error:
            raise _domain_error(error) from error

    @app.put("/api/v1/entity-views/{entity_id}/{space_id}")
    async def upsert_entity_view(entity_id: str, space_id: str, request: EntityViewUpsert) -> dict[str, Any]:
        with database.connect() as connection:
            entity = connection.execute("SELECT * FROM entities WHERE id = ?", (entity_id,)).fetchone()
            space = connection.execute("SELECT * FROM spaces WHERE id = ?", (space_id,)).fetchone()
            if entity is None or space is None:
                raise HTTPException(status_code=404, detail="entity_or_space_not_found")
            if entity["project_id"] != space["project_id"]:
                raise HTTPException(status_code=409, detail="cross_project_view_rejected")
            layer_name = _canonical_layer_name(connection, entity["project_id"], request.layer_id)
            _assert_layer_editable(connection, entity["project_id"], layer_name)
            current = connection.execute(
                "SELECT * FROM entity_views WHERE entity_id = ? AND space_id = ?", (entity_id, space_id)
            ).fetchone()
            if current is not None and request.expected_revision is not None and current["revision"] != request.expected_revision:
                raise _domain_error(DomainError(
                    "revision_conflict", 409,
                    {"view_id": current["id"], "expected_revision": request.expected_revision, "current_view": _view_dict(current)},
                ))
            if current is None and request.expected_revision not in (None, 0):
                raise _domain_error(DomainError(
                    "revision_conflict", 409,
                    {"view_id": f"view:{entity_id}:{space_id}", "current_view": None},
                ))
            requested_board_id = request.board_id or None
            board_id = requested_board_id if requested_board_id is not None else (
                current["board_id"] if current is not None else None
            )
            if board_id is not None:
                board = connection.execute(
                    "SELECT * FROM boards WHERE id = ? AND project_id = ? AND space_id = ?",
                    (board_id, entity["project_id"], space_id),
                ).fetchone()
                if board is None:
                    raise HTTPException(status_code=404, detail="board_not_found")
            view_id = current["id"] if current is not None else f"view:{entity_id}:{space_id}"
            revision = int(current["revision"]) + 1 if current is not None else 1
            connection.execute(
                """INSERT INTO entity_views
                   (id, entity_id, space_id, x, y, width, height, view_type, layer_id, visible,
                    visible_properties_json, board_id, board_local_x, board_local_y, revision)
                       VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                   ON CONFLICT(entity_id, space_id) DO UPDATE SET
                    x=excluded.x, y=excluded.y, width=excluded.width, height=excluded.height,
                    view_type=excluded.view_type, layer_id=excluded.layer_id,
                    visible=excluded.visible, visible_properties_json=excluded.visible_properties_json,
                    board_id=excluded.board_id, board_local_x=excluded.board_local_x,
                    board_local_y=excluded.board_local_y, revision=excluded.revision""",
                (view_id, entity_id, space_id, request.x, request.y, request.width,
                 request.height, request.view_type, layer_name, int(request.visible),
                 json.dumps(request.visible_properties, ensure_ascii=False), board_id,
                 request.board_local_x, request.board_local_y, revision),
            )
            row = connection.execute("SELECT * FROM entity_views WHERE id = ?", (view_id,)).fetchone()
            payload = _view_dict(row)
        event_type = "view.updated" if current is not None else "view.created"
        await manager.broadcast(entity["project_id"], {
            "type": event_type, "project_id": entity["project_id"], "view": payload,
        })
        return payload

    @app.delete("/api/v1/entity-views/{entity_id}/{space_id}", status_code=204, response_model=None)
    async def delete_entity_view(entity_id: str, space_id: str) -> None:
        with database.connect() as connection:
            row = connection.execute(
                """SELECT v.*, e.project_id FROM entity_views v
                   JOIN entities e ON e.id = v.entity_id
                   WHERE v.entity_id = ? AND v.space_id = ?""",
                (entity_id, space_id),
            ).fetchone()
            if row is None:
                raise HTTPException(status_code=404, detail="view_not_found")
            _assert_layer_editable(connection, row["project_id"], row["layer_id"])
            project_id = row["project_id"]
            connection.execute("DELETE FROM entity_views WHERE id = ?", (row["id"],))
        await manager.broadcast(project_id, {
            "type": "view.deleted", "project_id": project_id, "view_id": row["id"],
            "entity_id": entity_id, "space_id": space_id,
        })

    @app.get("/api/v1/projects/{project_id}/boards")
    def list_boards(project_id: str, space_id: str | None = Query(default=None)) -> dict[str, Any]:
        _project_or_404(database, project_id)
        with database.connect() as connection:
            sql = "SELECT * FROM boards WHERE project_id = ?"
            params: list[Any] = [project_id]
            if space_id is not None:
                sql += " AND space_id = ?"
                params.append(space_id)
            sql += " ORDER BY z_order, id"
            boards = [_board_dict(row) for row in connection.execute(sql, params).fetchall()]
        return {"boards": boards}

    @app.post("/api/v1/projects/{project_id}/boards", status_code=201)
    async def create_board(project_id: str, request: BoardCreate) -> dict[str, Any]:
        _space_or_404(database, project_id, request.space_id)
        board_id = f"board-{uuid.uuid4().hex}"
        now = utc_now()
        try:
            with database.connect() as connection:
                layer_name = _canonical_layer_name(connection, project_id, request.layer_id)
                _assert_layer_editable(connection, project_id, layer_name)
                connection.execute(
                    """INSERT INTO boards
                       (id, project_id, space_id, title, x, y, width, height, background, border,
                        locked, collapsed, layer_id, z_order, created_at, updated_at)
                       VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)""",
                    (board_id, project_id, request.space_id, request.title, request.x, request.y,
                     request.width, request.height, request.background, request.border, int(request.locked),
                     int(request.collapsed),
                     layer_name, request.z_order, now, now),
                )
                row = connection.execute("SELECT * FROM boards WHERE id = ?", (board_id,)).fetchone()
                payload = _board_dict(row)
            await manager.broadcast(project_id, {
                "type": "board.created", "project_id": project_id, "board": payload,
            })
            await _record_activity(database, manager, project_id, "Board Created", request.title)
            return payload
        except sqlite3.IntegrityError as error:
            raise HTTPException(status_code=409, detail="board_create_conflict") from error

    @app.patch("/api/v1/boards/{board_id}")
    async def patch_board(board_id: str, request: BoardPatch) -> dict[str, Any]:
        changes = {key: value for key, value in _dump(request).items() if value is not None}
        with database.connect() as connection:
            current = connection.execute("SELECT * FROM boards WHERE id = ?", (board_id,)).fetchone()
            if current is None:
                raise HTTPException(status_code=404, detail="board_not_found")
            if current["locked"] and any(key in changes for key in ("x", "y", "width", "height")):
                raise HTTPException(status_code=409, detail="board_locked")
            _assert_layer_editable(connection, current["project_id"], current["layer_id"])
            if "layer_id" in changes:
                changes["layer_id"] = _canonical_layer_name(
                    connection, current["project_id"], changes["layer_id"])
            _assert_layer_editable(connection, current["project_id"], changes.get("layer_id", current["layer_id"]))
            old_x, old_y = current["x"], current["y"]
            if changes:
                assignments = ", ".join(f"{key} = ?" for key in changes)
                connection.execute(
                    f"UPDATE boards SET {assignments}, updated_at = ? WHERE id = ?",
                    (*changes.values(), utc_now(), board_id),
                )
            updated = connection.execute("SELECT * FROM boards WHERE id = ?", (board_id,)).fetchone()
            new_x, new_y = updated["x"], updated["y"]
            if new_x != old_x or new_y != old_y:
                connection.execute(
                    """UPDATE entity_views SET x = ? + board_local_x, y = ? + board_local_y,
                       revision = revision + 1 WHERE board_id = ?""",
                    (new_x, new_y, board_id),
                )
            payload = _board_dict(updated)
            project_id = updated["project_id"]
        await manager.broadcast(project_id, {
            "type": "board.updated", "project_id": project_id, "board": payload,
        })
        return payload

    @app.delete("/api/v1/boards/{board_id}", status_code=204, response_model=None)
    async def delete_board(board_id: str) -> None:
        with database.connect() as connection:
            row = connection.execute("SELECT * FROM boards WHERE id = ?", (board_id,)).fetchone()
            if row is None:
                raise HTTPException(status_code=404, detail="board_not_found")
            if row["locked"]:
                raise HTTPException(status_code=409, detail="board_locked")
            _assert_layer_editable(connection, row["project_id"], row["layer_id"])
            project_id = row["project_id"]
            connection.execute("UPDATE entity_views SET board_id = NULL WHERE board_id = ?", (board_id,))
            connection.execute("UPDATE ink_strokes SET anchor_board_id = NULL WHERE anchor_board_id = ?", (board_id,))
            connection.execute("DELETE FROM boards WHERE id = ?", (board_id,))
        await manager.broadcast(project_id, {
            "type": "board.deleted", "project_id": project_id, "board_id": board_id,
        })

    @app.put("/api/v1/boards/{board_id}/views/{view_id}")
    async def attach_view_to_board(board_id: str, view_id: str, request: BoardAttachView) -> dict[str, Any]:
        with database.connect() as connection:
            board = connection.execute("SELECT * FROM boards WHERE id = ?", (board_id,)).fetchone()
            view = connection.execute("SELECT * FROM entity_views WHERE id = ?", (view_id,)).fetchone()
            if board is None or view is None:
                raise HTTPException(status_code=404, detail="board_or_view_not_found")
            if board["locked"]:
                raise HTTPException(status_code=409, detail="board_locked")
            _assert_layer_editable(connection, board["project_id"], board["layer_id"])
            _assert_layer_editable(connection, board["project_id"], view["layer_id"])
            if board["space_id"] != view["space_id"] or board["project_id"] != connection.execute(
                "SELECT project_id FROM spaces WHERE id = ?", (view["space_id"],)
            ).fetchone()[0]:
                raise HTTPException(status_code=409, detail="cross_space_board_attach_rejected")
            local_x = view["x"] - board["x"] if request.local_x is None else request.local_x
            local_y = view["y"] - board["y"] if request.local_y is None else request.local_y
            connection.execute(
                """UPDATE entity_views SET board_id = ?, board_local_x = ?, board_local_y = ?,
                   x = ?, y = ?, revision = revision + 1 WHERE id = ?""",
                (board_id, local_x, local_y, board["x"] + local_x, board["y"] + local_y, view_id),
            )
            updated = connection.execute("SELECT * FROM entity_views WHERE id = ?", (view_id,)).fetchone()
            payload = _view_dict(updated)
            project_id = board["project_id"]
        await manager.broadcast(project_id, {
            "type": "view.updated", "project_id": project_id, "view": payload,
        })
        return payload

    @app.delete("/api/v1/boards/{board_id}/views/{view_id}", status_code=204, response_model=None)
    async def detach_view_from_board(board_id: str, view_id: str) -> None:
        with database.connect() as connection:
            board = connection.execute("SELECT * FROM boards WHERE id = ?", (board_id,)).fetchone()
            view = connection.execute("SELECT * FROM entity_views WHERE id = ?", (view_id,)).fetchone()
            if board is None or view is None or view["board_id"] != board_id:
                raise HTTPException(status_code=404, detail="board_view_attachment_not_found")
            if board["locked"]:
                raise HTTPException(status_code=409, detail="board_locked")
            _assert_layer_editable(connection, board["project_id"], board["layer_id"])
            _assert_layer_editable(connection, board["project_id"], view["layer_id"])
            connection.execute(
                "UPDATE entity_views SET board_id = NULL, revision = revision + 1 WHERE id = ?", (view_id,)
            )
            project_id = board["project_id"]
        await manager.broadcast(project_id, {
            "type": "view.updated", "project_id": project_id,
            "view": {"id": view_id, "board_id": None},
        })

    @app.get("/api/v1/projects/{project_id}/layers")
    def list_layers(project_id: str, space_id: str | None = Query(default=None)) -> dict[str, Any]:
        _project_or_404(database, project_id)
        with database.connect() as connection:
            sql = "SELECT * FROM user_layers WHERE project_id = ? AND (space_id IS NULL OR space_id = ?)"
            layers = [_layer_dict(row) for row in connection.execute(
                sql + " ORDER BY sort_order, rowid", (project_id, space_id)
            ).fetchall()]
        return {"layers": layers}

    @app.post("/api/v1/projects/{project_id}/layers", status_code=201)
    async def create_layer(project_id: str, request: LayerCreate) -> dict[str, Any]:
        _project_or_404(database, project_id)
        if request.space_id:
            _space_or_404(database, project_id, request.space_id)
        # Visual objects store the user-facing layer name for compact payloads;
        # keep the database row id stable and project-scoped so identical layer
        # names can exist in different projects.
        layer_id = f"{project_id}:layer:{uuid.uuid4().hex}"
        try:
            with database.connect() as connection:
                connection.execute(
                    """INSERT INTO user_layers(id, project_id, name, visible, space_id, locked, sort_order)
                       VALUES (?, ?, ?, ?, ?, ?, ?)""",
                    (layer_id, project_id, request.name, int(request.visible), request.space_id,
                     int(request.locked), request.sort_order),
                )
                payload = _layer_dict(connection.execute(
                    "SELECT * FROM user_layers WHERE id = ?", (layer_id,)
                ).fetchone())
            await manager.broadcast(project_id, {
                "type": "layer.created", "project_id": project_id, "layer": payload,
            })
            return payload
        except sqlite3.IntegrityError as error:
            raise HTTPException(status_code=409, detail="layer_name_already_exists") from error

    @app.patch("/api/v1/layers/{layer_id}")
    async def patch_layer(layer_id: str, request: LayerPatch) -> dict[str, Any]:
        changes = {key: value for key, value in _dump(request).items() if value is not None}
        with database.connect() as connection:
            row = connection.execute("SELECT * FROM user_layers WHERE id = ?", (layer_id,)).fetchone()
            if row is None:
                raise HTTPException(status_code=404, detail="layer_not_found")
            if row["name"] == "Main" and ("name" in changes and changes["name"] != "Main"):
                raise HTTPException(status_code=409, detail="main_layer_cannot_be_renamed")
            old_name = row["name"]
            new_name = changes.get("name", old_name)
            if changes:
                assignments = ", ".join(f"{key} = ?" for key in changes)
                connection.execute(
                    f"UPDATE user_layers SET {assignments} WHERE id = ?",
                    (*changes.values(), layer_id),
                )
            if new_name != old_name:
                connection.execute(
                    """UPDATE entity_views SET layer_id = ? WHERE layer_id = ?
                       AND space_id IN (SELECT id FROM spaces WHERE project_id = ?)""",
                    (new_name, old_name, row["project_id"]),
                )
                for table in ("ink_strokes", "dashboard_widgets", "boards"):
                    connection.execute(
                        f"UPDATE {table} SET layer_id = ? WHERE layer_id = ? AND project_id = ?",
                        (new_name, old_name, row["project_id"]),
                    )
            payload = _layer_dict(connection.execute(
                "SELECT * FROM user_layers WHERE id = ?", (layer_id,)
            ).fetchone())
            project_id = row["project_id"]
        await manager.broadcast(project_id, {
            "type": "layer.updated", "project_id": project_id, "layer": payload,
        })
        return payload

    @app.delete("/api/v1/layers/{layer_id}", status_code=204, response_model=None)
    async def delete_layer(layer_id: str, request: LayerDelete | None = None) -> None:
        mode = request.mode if request is not None else "move_to_main"
        with database.connect() as connection:
            row = connection.execute("SELECT * FROM user_layers WHERE id = ?", (layer_id,)).fetchone()
            if row is None:
                raise HTTPException(status_code=404, detail="layer_not_found")
            if row["name"] == "Main":
                raise HTTPException(status_code=409, detail="main_layer_cannot_be_deleted")
            project_id = row["project_id"]
            if mode == "delete_visuals":
                connection.execute("DELETE FROM entity_views WHERE layer_id = ?", (row["name"],))
                connection.execute("DELETE FROM ink_strokes WHERE layer_id = ?", (row["name"],))
                connection.execute("DELETE FROM dashboard_widgets WHERE layer_id = ?", (row["name"],))
                board_ids = [item[0] for item in connection.execute(
                    "SELECT id FROM boards WHERE project_id = ? AND layer_id = ?", (project_id, row["name"])
                ).fetchall()]
                connection.execute("DELETE FROM boards WHERE project_id = ? AND layer_id = ?", (project_id, row["name"]))
                for board_id in board_ids:
                    connection.execute("UPDATE entity_views SET board_id = NULL WHERE board_id = ?", (board_id,))
                    connection.execute("UPDATE ink_strokes SET anchor_board_id = NULL WHERE anchor_board_id = ?", (board_id,))
            else:
                connection.execute("UPDATE entity_views SET layer_id = 'Main' WHERE layer_id = ?", (row["name"],))
                connection.execute("UPDATE ink_strokes SET layer_id = 'Main' WHERE layer_id = ?", (row["name"],))
                connection.execute("UPDATE dashboard_widgets SET layer_id = 'Main' WHERE layer_id = ?", (row["name"],))
                connection.execute("UPDATE boards SET layer_id = 'Main' WHERE project_id = ? AND layer_id = ?", (project_id, row["name"]))
            connection.execute("DELETE FROM user_layers WHERE id = ?", (layer_id,))
        await manager.broadcast(project_id, {
            "type": "layer.deleted", "project_id": project_id, "layer_id": layer_id,
        })

    @app.get("/api/v1/projects/{project_id}/query-views")
    def list_query_views(project_id: str) -> dict[str, Any]:
        _project_or_404(database, project_id)
        with database.connect() as connection:
            queries = [_query_dict(connection, row) for row in connection.execute(
                "SELECT * FROM query_views WHERE project_id = ? ORDER BY name", (project_id,)
            ).fetchall()]
        return {"query_views": queries}

    @app.post("/api/v1/projects/{project_id}/query-views", status_code=201)
    async def create_query_view(project_id: str, request: QueryViewCreate) -> dict[str, Any]:
        _project_or_404(database, project_id)
        if request.space_id:
            _space_or_404(database, project_id, request.space_id)
        query_id = f"query-{uuid.uuid4().hex}"
        now = utc_now()
        try:
            with database.connect() as connection:
                connection.execute(
                    """INSERT INTO query_views
                       (id, project_id, space_id, name, entity_schema, filters_json, sort_json,
                        result_limit, created_at, updated_at)
                       VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)""",
                    (query_id, project_id, request.space_id, request.name, request.entity_schema,
                     json.dumps(request.filters, ensure_ascii=False), json.dumps(request.sort, ensure_ascii=False),
                     request.result_limit, now, now),
                )
                payload = _query_dict(connection, connection.execute(
                    "SELECT * FROM query_views WHERE id = ?", (query_id,)
                ).fetchone())
            await manager.broadcast(project_id, {
                "type": "query_view.created", "project_id": project_id, "query_view": payload,
            })
            return payload
        except sqlite3.IntegrityError as error:
            raise HTTPException(status_code=409, detail="query_view_name_already_exists") from error

    @app.patch("/api/v1/query-views/{query_id}")
    async def patch_query_view(query_id: str, request: QueryViewPatch) -> dict[str, Any]:
        changes = {key: value for key, value in _dump(request).items() if value is not None}
        encoded = {"filters": "filters_json", "sort": "sort_json"}
        with database.connect() as connection:
            current = connection.execute("SELECT * FROM query_views WHERE id = ?", (query_id,)).fetchone()
            if current is None:
                raise HTTPException(status_code=404, detail="query_view_not_found")
            values: dict[str, Any] = {}
            for key, value in changes.items():
                values[encoded.get(key, key)] = json.dumps(value, ensure_ascii=False) if key in encoded else value
            if values:
                assignments = ", ".join(f"{key} = ?" for key in values)
                connection.execute(
                    f"UPDATE query_views SET {assignments}, updated_at = ? WHERE id = ?",
                    (*values.values(), utc_now(), query_id),
                )
            payload = _query_dict(connection, connection.execute(
                "SELECT * FROM query_views WHERE id = ?", (query_id,)
            ).fetchone())
            project_id = current["project_id"]
        await manager.broadcast(project_id, {
            "type": "query_view.updated", "project_id": project_id, "query_view": payload,
        })
        return payload

    @app.delete("/api/v1/query-views/{query_id}", status_code=204, response_model=None)
    async def delete_query_view(query_id: str) -> None:
        with database.connect() as connection:
            row = connection.execute("SELECT * FROM query_views WHERE id = ?", (query_id,)).fetchone()
            if row is None:
                raise HTTPException(status_code=404, detail="query_view_not_found")
            project_id = row["project_id"]
            connection.execute("DELETE FROM query_views WHERE id = ?", (query_id,))
        await manager.broadcast(project_id, {
            "type": "query_view.deleted", "project_id": project_id, "query_view_id": query_id,
        })

    @app.get("/api/v1/projects/{project_id}/dashboard-widgets")
    def list_dashboard_widgets(project_id: str, space_id: str | None = Query(default=None)) -> dict[str, Any]:
        _project_or_404(database, project_id)
        with database.connect() as connection:
            sql = "SELECT * FROM dashboard_widgets WHERE project_id = ?"
            params: list[Any] = [project_id]
            if space_id:
                sql += " AND space_id = ?"
                params.append(space_id)
            widgets = [_widget_dict(row) for row in connection.execute(sql + " ORDER BY id", params).fetchall()]
        return {"dashboard_widgets": widgets}

    @app.post("/api/v1/projects/{project_id}/dashboard-widgets", status_code=201)
    async def create_dashboard_widget(project_id: str, request: WidgetCreate) -> dict[str, Any]:
        _space_or_404(database, project_id, request.space_id)
        widget_id = f"widget-{uuid.uuid4().hex}"
        now = utc_now()
        with database.connect() as connection:
            layer_name = _canonical_layer_name(connection, project_id, request.layer_id)
            _assert_layer_editable(connection, project_id, layer_name)
            connection.execute(
                """INSERT INTO dashboard_widgets
                   (id, project_id, space_id, widget_type, title, config_json, x, y, width, height,
                    layer_id, visible, revision, created_at, updated_at)
                   VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 1, ?, ?)""",
                (widget_id, project_id, request.space_id, request.widget_type, request.title,
                 json.dumps(request.config, ensure_ascii=False), request.x, request.y, request.width,
                 request.height, layer_name, int(request.visible), now, now),
            )
            payload = _widget_dict(connection.execute(
                "SELECT * FROM dashboard_widgets WHERE id = ?", (widget_id,)
            ).fetchone())
        await manager.broadcast(project_id, {
            "type": "dashboard_widget.created", "project_id": project_id, "widget": payload,
        })
        return payload

    @app.patch("/api/v1/dashboard-widgets/{widget_id}")
    async def patch_dashboard_widget(widget_id: str, request: WidgetPatch) -> dict[str, Any]:
        changes = {key: value for key, value in _dump(request).items() if value is not None}
        if "config" in changes:
            changes["config_json"] = json.dumps(changes.pop("config"), ensure_ascii=False)
        with database.connect() as connection:
            row = connection.execute("SELECT * FROM dashboard_widgets WHERE id = ?", (widget_id,)).fetchone()
            if row is None:
                raise HTTPException(status_code=404, detail="dashboard_widget_not_found")
            _assert_layer_editable(connection, row["project_id"], row["layer_id"])
            if "layer_id" in changes:
                changes["layer_id"] = _canonical_layer_name(
                    connection, row["project_id"], changes["layer_id"])
            _assert_layer_editable(connection, row["project_id"], changes.get("layer_id", row["layer_id"]))
            if changes:
                assignments = ", ".join(f"{key} = ?" for key in changes)
                connection.execute(
                    f"UPDATE dashboard_widgets SET {assignments}, revision = revision + 1, updated_at = ? WHERE id = ?",
                    (*changes.values(), utc_now(), widget_id),
                )
            payload = _widget_dict(connection.execute(
                "SELECT * FROM dashboard_widgets WHERE id = ?", (widget_id,)
            ).fetchone())
            project_id = row["project_id"]
        await manager.broadcast(project_id, {
            "type": "dashboard_widget.updated", "project_id": project_id, "widget": payload,
        })
        return payload

    @app.delete("/api/v1/dashboard-widgets/{widget_id}", status_code=204, response_model=None)
    async def delete_dashboard_widget(widget_id: str) -> None:
        with database.connect() as connection:
            row = connection.execute("SELECT * FROM dashboard_widgets WHERE id = ?", (widget_id,)).fetchone()
            if row is None:
                raise HTTPException(status_code=404, detail="dashboard_widget_not_found")
            _assert_layer_editable(connection, row["project_id"], row["layer_id"])
            project_id = row["project_id"]
            connection.execute("DELETE FROM dashboard_widgets WHERE id = ?", (widget_id,))
        await manager.broadcast(project_id, {
            "type": "dashboard_widget.deleted", "project_id": project_id, "widget_id": widget_id,
        })

    @app.get("/api/v1/projects/{project_id}/activities")
    def list_activities(project_id: str, limit: int = Query(default=50, ge=1, le=200)) -> dict[str, Any]:
        _project_or_404(database, project_id)
        with database.connect() as connection:
            rows = connection.execute(
                "SELECT * FROM activities WHERE project_id = ? ORDER BY created_at DESC, id LIMIT ?",
                (project_id, limit),
            ).fetchall()
        return {"activities": [_activity_dict(row) for row in rows]}

    @app.post("/api/v1/relationships", status_code=201)
    async def create_relationship(request: RelationshipCreate) -> dict[str, Any]:
        try:
            validate_relationship_type(request.relationship_type)
            if request.source_entity_id == request.target_entity_id:
                raise DomainError("self_relationship_rejected", 422)
            _project_or_404(database, request.project_id)
            with database.connect() as connection:
                source = connection.execute("SELECT * FROM entities WHERE id = ?", (request.source_entity_id,)).fetchone()
                target = connection.execute("SELECT * FROM entities WHERE id = ?", (request.target_entity_id,)).fetchone()
                if source is None or target is None:
                    raise DomainError("entity_not_found", 404)
                if source["project_id"] != request.project_id or target["project_id"] != request.project_id:
                    raise DomainError("cross_project_relationship_rejected", 409)
                _sync_relationship_field(connection, source, target, request.relationship_type)
                relationship_id = f"relationship-{uuid.uuid4().hex}"
                now = utc_now()
                connection.execute(
                    """INSERT INTO relationships
                       (id, project_id, source_entity_id, target_entity_id, relationship_type, created_at)
                       VALUES (?, ?, ?, ?, ?, ?)""",
                    (relationship_id, request.project_id, request.source_entity_id,
                     request.target_entity_id, request.relationship_type, now),
                )
                row = connection.execute("SELECT * FROM relationships WHERE id = ?", (relationship_id,)).fetchone()
                payload = relationship_to_dict(row)
                source_payload = entity_to_dict(
                    connection,
                    connection.execute("SELECT * FROM entities WHERE id = ?", (source["id"],)).fetchone(),
                )
            await manager.broadcast(request.project_id, {
                "type": "relationship.created", "project_id": request.project_id,
                "relationship": payload,
            })
            await manager.broadcast(request.project_id, {
                "type": "entity.updated", "project_id": request.project_id, "entity": source_payload,
            })
            await _record_activity(
                database, manager, request.project_id, "Relationship Created",
                f"{request.relationship_type} relationship created",
                entity_id=request.source_entity_id, payload={"relationship": payload},
            )
            return payload
        except DomainError as error:
            raise _domain_error(error) from error
        except sqlite3.IntegrityError as error:
            raise _domain_error(DomainError("relationship_already_exists", 409)) from error

    @app.get("/api/v1/projects/{project_id}/relationships")
    def list_relationships(project_id: str) -> dict[str, Any]:
        _project_or_404(database, project_id)
        with database.connect() as connection:
            rows = connection.execute(
                "SELECT * FROM relationships WHERE project_id = ? ORDER BY created_at, id", (project_id,)
            ).fetchall()
        return {"relationships": [relationship_to_dict(row) for row in rows]}

    @app.patch("/api/v1/relationships/{relationship_id}")
    async def patch_relationship(relationship_id: str, request: RelationshipPatch) -> dict[str, Any]:
        changes = {key: value for key, value in _dump(request).items() if value is not None}
        with database.connect() as connection:
            row = connection.execute(
                "SELECT * FROM relationships WHERE id = ?", (relationship_id,)
            ).fetchone()
            if row is None:
                raise HTTPException(status_code=404, detail="relationship_not_found")
            if changes:
                assignments = ", ".join(f"{key} = ?" for key in changes)
                connection.execute(
                    f"UPDATE relationships SET {assignments} WHERE id = ?",
                    (*[int(value) if isinstance(value, bool) else value for value in changes.values()], relationship_id),
                )
            updated = connection.execute(
                "SELECT * FROM relationships WHERE id = ?", (relationship_id,)
            ).fetchone()
            payload = relationship_to_dict(updated)
            project_id = row["project_id"]
        await manager.broadcast(project_id, {
            "type": "relationship.updated", "project_id": project_id,
            "relationship": payload,
        })
        await _record_activity(
            database, manager, project_id, "Relationship Updated",
            f"{payload['relationship_type']} relationship visibility changed",
            entity_id=payload["source_entity_id"], payload={"relationship": payload},
        )
        return payload

    @app.delete("/api/v1/relationships/{relationship_id}", status_code=204, response_model=None)
    async def delete_relationship(relationship_id: str) -> None:
        with database.connect() as connection:
            row = connection.execute("SELECT * FROM relationships WHERE id = ?", (relationship_id,)).fetchone()
            if row is None:
                raise HTTPException(status_code=404, detail="relationship_not_found")
            payload = relationship_to_dict(row)
            source = connection.execute("SELECT * FROM entities WHERE id = ?", (row["source_entity_id"],)).fetchone()
            target = connection.execute("SELECT * FROM entities WHERE id = ?", (row["target_entity_id"],)).fetchone()
            if source is not None and target is not None:
                _sync_relationship_field(connection, source, target, row["relationship_type"], clear=True)
            source_payload = entity_to_dict(connection, source) if source is not None else None
            connection.execute("DELETE FROM relationships WHERE id = ?", (relationship_id,))
        await manager.broadcast(payload["project_id"], {
            "type": "relationship.deleted", "project_id": payload["project_id"],
            "relationship": payload,
        })
        if source_payload is not None:
            await manager.broadcast(payload["project_id"], {
                "type": "entity.updated", "project_id": payload["project_id"], "entity": source_payload,
            })
        await _record_activity(
            database, manager, payload["project_id"], "Relationship Deleted",
            f"{payload['relationship_type']} relationship deleted",
            entity_id=payload["source_entity_id"], payload={"relationship": payload},
        )

    @app.get("/api/v1/projects/{project_id}/ink-groups")
    def list_ink_groups(project_id: str, space_id: str | None = Query(default=None)) -> dict[str, Any]:
        _project_or_404(database, project_id)
        with database.connect() as connection:
            sql = "SELECT id, space_id, group_id FROM ink_strokes WHERE project_id = ? AND group_id IS NOT NULL"
            params: list[Any] = [project_id]
            if space_id:
                sql += " AND space_id = ?"
                params.append(space_id)
            groups: dict[str, dict[str, Any]] = {}
            for row in connection.execute(sql, params).fetchall():
                group = groups.setdefault(row["group_id"], {
                    "id": row["group_id"], "project_id": project_id,
                    "space_id": row["space_id"], "stroke_ids": [],
                })
                group["stroke_ids"].append(row["id"])
        return {"ink_groups": list(groups.values())}

    @app.post("/api/v1/projects/{project_id}/ink-groups", status_code=201)
    async def create_ink_group(project_id: str, request: InkGroupCreate) -> dict[str, Any]:
        if request.project_id != project_id:
            raise HTTPException(status_code=409, detail="project_id_mismatch")
        _space_or_404(database, project_id, request.space_id)
        group_id = request.group_id or f"ink-group-{uuid.uuid4().hex}"
        with database.connect() as connection:
            layer_name = _canonical_layer_name(connection, project_id, request.layer_id)
            _assert_layer_editable(connection, project_id, layer_name)
            placeholders = ", ".join("?" for _ in request.stroke_ids)
            rows = connection.execute(
                f"SELECT * FROM ink_strokes WHERE project_id = ? AND space_id = ? AND id IN ({placeholders})",
                (project_id, request.space_id, *request.stroke_ids),
            ).fetchall()
            if len(rows) != len(set(request.stroke_ids)):
                raise HTTPException(status_code=409, detail="ink_group_stroke_not_found")
            connection.execute(
                f"UPDATE ink_strokes SET group_id = ?, layer_id = ? WHERE id IN ({placeholders})",
                (group_id, layer_name, *request.stroke_ids),
            )
            updated_rows = connection.execute(
                f"SELECT * FROM ink_strokes WHERE id IN ({placeholders}) ORDER BY id",
                request.stroke_ids,
            ).fetchall()
            payload_strokes = [_stroke_dict(row) for row in updated_rows]
        for stroke in payload_strokes:
            await manager.broadcast(project_id, {
                "type": "ink.updated", "project_id": project_id, "stroke": stroke,
            })
        payload = {
            "id": group_id, "project_id": project_id, "space_id": request.space_id,
            "name": request.name, "layer_id": layer_name,
            "stroke_ids": [stroke["id"] for stroke in payload_strokes],
            "strokes": payload_strokes,
        }
        await _record_activity(
            database, manager, project_id, "Ink Group Created",
            request.name or group_id, payload={"ink_group": payload},
        )
        return payload

    @app.post("/api/v1/ink-strokes", status_code=201)
    async def create_stroke(request: StrokeCreate) -> dict[str, Any]:
        _project_or_404(database, request.project_id)
        _space_or_404(database, request.project_id, request.space_id)
        if request.anchor_entity_id and request.anchor_board_id:
            raise HTTPException(status_code=409, detail="ink_multiple_anchors_rejected")
        if request.anchor_entity_id:
            with database.connect() as connection:
                anchor = connection.execute(
                    "SELECT project_id FROM entities WHERE id = ?", (request.anchor_entity_id,)
                ).fetchone()
            if anchor is None or anchor["project_id"] != request.project_id:
                raise HTTPException(status_code=409, detail="cross_project_ink_anchor_rejected")
        if request.anchor_board_id:
            with database.connect() as connection:
                board = connection.execute(
                    "SELECT project_id, space_id FROM boards WHERE id = ?", (request.anchor_board_id,)
                ).fetchone()
            if board is None or board["project_id"] != request.project_id or board["space_id"] != request.space_id:
                raise HTTPException(status_code=409, detail="cross_project_ink_board_anchor_rejected")
        stroke_id = request.id or f"stroke-{uuid.uuid4().hex}"
        created_at = utc_now()
        try:
            with database.connect() as connection:
                layer_name = _canonical_layer_name(connection, request.project_id, request.layer_id)
                _assert_layer_editable(connection, request.project_id, layer_name)
                connection.execute(
                    """INSERT INTO ink_strokes
                       (id, project_id, space_id, points_json, width, tool, shape, color, layer_id, group_id,
                        anchor_entity_id, anchor_board_id, local_offset_x, local_offset_y, created_at)
                       VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)""",
                    (
                        stroke_id, request.project_id, request.space_id,
                        json.dumps([_dump(point) for point in request.points]), request.width,
                        request.tool, request.shape, request.color, layer_name, request.group_id,
                        request.anchor_entity_id, request.anchor_board_id,
                        request.local_offset_x, request.local_offset_y, created_at,
                    ),
                )
                row = connection.execute("SELECT * FROM ink_strokes WHERE id = ?", (stroke_id,)).fetchone()
                payload = _stroke_dict(row)
            await manager.broadcast(request.project_id, {
                "type": "ink.created", "project_id": request.project_id, "stroke": payload,
            })
            return payload
        except sqlite3.IntegrityError as error:
            raise HTTPException(status_code=409, detail="stroke_already_exists") from error

    @app.delete("/api/v1/ink-strokes/{stroke_id}", status_code=204, response_model=None)
    async def delete_stroke(stroke_id: str) -> None:
        with database.connect() as connection:
            row = connection.execute("SELECT * FROM ink_strokes WHERE id = ?", (stroke_id,)).fetchone()
            if row is None:
                raise HTTPException(status_code=404, detail="stroke_not_found")
            _assert_layer_editable(connection, row["project_id"], row["layer_id"])
            project_id = row["project_id"]
            connection.execute("DELETE FROM ink_strokes WHERE id = ?", (stroke_id,))
        await manager.broadcast(project_id, {
            "type": "ink.deleted", "project_id": project_id, "stroke_id": stroke_id,
        })

    @app.patch("/api/v1/ink-strokes/{stroke_id}")
    async def patch_stroke(stroke_id: str, request: StrokePatch) -> dict[str, Any]:
        with database.connect() as connection:
            row = connection.execute("SELECT * FROM ink_strokes WHERE id = ?", (stroke_id,)).fetchone()
            if row is None:
                raise HTTPException(status_code=404, detail="stroke_not_found")
            _assert_layer_editable(connection, row["project_id"], row["layer_id"])
            fields_set = set(getattr(request, "model_fields_set", None) or getattr(request, "__fields_set__", set()))
            anchor_entity_id = request.anchor_entity_id if "anchor_entity_id" in fields_set else row["anchor_entity_id"]
            anchor_board_id = request.anchor_board_id if "anchor_board_id" in fields_set else row["anchor_board_id"]
            if anchor_entity_id and anchor_board_id:
                raise HTTPException(status_code=409, detail="ink_multiple_anchors_rejected")
            if anchor_entity_id:
                anchor = connection.execute(
                    "SELECT project_id FROM entities WHERE id = ?", (anchor_entity_id,)
                ).fetchone()
                if anchor is None or anchor["project_id"] != row["project_id"]:
                    raise HTTPException(status_code=409, detail="cross_project_ink_anchor_rejected")
            if anchor_board_id:
                board = connection.execute(
                    "SELECT project_id, space_id FROM boards WHERE id = ?", (anchor_board_id,)
                ).fetchone()
                if board is None or board["project_id"] != row["project_id"] or board["space_id"] != row["space_id"]:
                    raise HTTPException(status_code=409, detail="cross_project_ink_board_anchor_rejected")
            if "layer_id" in fields_set:
                request_layer = _canonical_layer_name(connection, row["project_id"], request.layer_id)
            else:
                request_layer = row["layer_id"]
            _assert_layer_editable(connection, row["project_id"], request_layer)
            points = (
                json.dumps([_dump(point) for point in request.points], ensure_ascii=False)
                if "points" in fields_set else row["points_json"]
            )
            width = request.width if "width" in fields_set else row["width"]
            tool = request.tool if "tool" in fields_set else row["tool"]
            layer_id = request_layer
            shape = request.shape if "shape" in fields_set else row["shape"]
            color = request.color if "color" in fields_set else row["color"]
            group_id = request.group_id if "group_id" in fields_set else row["group_id"]
            local_offset_x = request.local_offset_x if "local_offset_x" in fields_set else row["local_offset_x"]
            local_offset_y = request.local_offset_y if "local_offset_y" in fields_set else row["local_offset_y"]
            connection.execute(
                """UPDATE ink_strokes SET points_json = ?, width = ?, tool = ?, layer_id = ?,
                   anchor_entity_id = ?, anchor_board_id = ?, shape = ?, color = ?, group_id = ?,
                   local_offset_x = ?, local_offset_y = ?
                   WHERE id = ?""",
                (points, width, tool, layer_id, anchor_entity_id, anchor_board_id, shape, color, group_id,
                  local_offset_x, local_offset_y, stroke_id),
            )
            updated = connection.execute("SELECT * FROM ink_strokes WHERE id = ?", (stroke_id,)).fetchone()
            payload = _stroke_dict(updated)
            project_id = row["project_id"]
        await manager.broadcast(project_id, {
            "type": "ink.updated", "project_id": project_id, "stroke": payload,
        })
        return payload

    @app.get("/api/v1/projects/{project_id}/scene-locks")
    async def list_locks(project_id: str) -> dict[str, Any]:
        _project_or_404(database, project_id)
        locks = lock_service.list(project_id)
        for lock in locks:
            if lock["status"] == "STALE":
                await manager.broadcast(project_id, {
                    "type": "scene_lock.stale", "project_id": project_id,
                    "scene_id": lock["scene_id"], "lock": lock,
                })
        return {"scene_locks": locks}

    @app.get("/api/v1/scene-locks/{scene_id}")
    def get_lock(scene_id: str) -> dict[str, Any]:
        try:
            return lock_service.get(scene_id)
        except SceneLockError as error:
            raise _lock_error(error) from error

    @app.post("/api/v1/scene-locks/{scene_id}/acquire")
    async def acquire_lock(scene_id: str, request: AcquireRequest) -> dict[str, Any]:
        try:
            with database.connect() as connection:
                scene = connection.execute("SELECT * FROM entities WHERE id = ?", (scene_id,)).fetchone()
                if scene is None or scene["entity_type"] != "scene":
                    raise SceneNotFound("scene_lock_not_found")
                if request.task_id:
                    task = connection.execute(
                        "SELECT project_id, entity_type FROM entities WHERE id = ?", (request.task_id,)
                    ).fetchone()
                    if task is None or task["project_id"] != scene["project_id"] or task["entity_type"] != "task":
                        raise HTTPException(status_code=409, detail="invalid_scene_lock_task")
                worker = connection.execute(
                    """SELECT e.project_id, e.entity_type, e.title FROM entities e
                       JOIN worker_details wd ON wd.entity_id = e.id
                       WHERE wd.worker_id = ?""",
                    (request.worker_id,),
                ).fetchone()
                if worker is not None and (worker["project_id"] != scene["project_id"] or worker["entity_type"] != "worker"):
                    raise HTTPException(status_code=409, detail="invalid_scene_lock_worker")
                worker_name = request.worker_name or (worker["title"] if worker is not None else request.worker_id)
            lock = lock_service.acquire(
                scene_id,
                worker_id=request.worker_id,
                worker_name=worker_name,
                task=request.task or request.task_note or "",
                branch=request.branch,
                task_id=request.task_id,
                task_note=request.task_note or request.task,
            )
        except SceneLockError as error:
            raise _lock_error(error) from error
        await manager.broadcast(lock["project_id"], {
            "type": "scene_lock.acquired", "project_id": lock["project_id"],
            "scene_id": scene_id, "lock": lock,
        })
        await _record_activity(
            database, manager, lock["project_id"], "Scene Lock Acquired",
            f"{lock.get('worker_id') or lock.get('owner_id', '')} acquired {scene_id}",
            entity_id=scene_id, payload={"lock": lock},
        )
        return lock

    @app.post("/api/v1/scene-locks/{scene_id}/heartbeat")
    async def heartbeat_lock(scene_id: str, request: HeartbeatRequest) -> dict[str, Any]:
        try:
            lock = lock_service.heartbeat(scene_id, worker_id=request.worker_id)
        except SceneLockError as error:
            raise _lock_error(error) from error
        await manager.broadcast(lock["project_id"], {
            "type": "scene_lock.heartbeat", "project_id": lock["project_id"],
            "scene_id": scene_id, "lock": lock,
        })
        return lock

    @app.post("/api/v1/scene-locks/{scene_id}/release")
    async def release_lock(scene_id: str, request: HeartbeatRequest) -> dict[str, Any]:
        try:
            lock = lock_service.release(scene_id, worker_id=request.worker_id)
        except SceneLockError as error:
            raise _lock_error(error) from error
        await manager.broadcast(lock["project_id"], {
            "type": "scene_lock.released", "project_id": lock["project_id"],
            "scene_id": scene_id, "lock": lock,
        })
        await _record_activity(
            database, manager, lock["project_id"], "Scene Lock Released",
            f"{scene_id} lock released", entity_id=scene_id, payload={"lock": lock},
        )
        return lock

    @app.websocket("/ws/projects/{project_id}")
    async def project_websocket(websocket: WebSocket, project_id: str) -> None:
        try:
            _project_or_404(database, project_id)
        except HTTPException:
            await websocket.close(code=1008)
            return
        await manager.connect(project_id, websocket)
        try:
            # Phase 2 deliberately sends no full project snapshot over WS.
            # The client performs an authoritative REST GET before subscribing
            # and repeats that GET after a reconnect.
            await websocket.send_json({"type": "realtime.ready", "project_id": project_id})
            while True:
                message = await websocket.receive_text()
                try:
                    payload = json.loads(message)
                except ValueError:
                    continue
                if payload.get("type") == "scene_lock.heartbeat":
                    worker_id = str(payload.get("worker_id", ""))
                    try:
                        lock = lock_service.heartbeat(str(payload.get("scene_id", "")), worker_id=worker_id)
                    except SceneLockError:
                        continue
                    await manager.broadcast(project_id, {
                        "type": "scene_lock.heartbeat", "project_id": project_id,
                        "scene_id": lock["scene_id"], "lock": lock,
                    })
        except WebSocketDisconnect:
            pass
        finally:
            await manager.disconnect(project_id, websocket)

    return app


app = create_app()
