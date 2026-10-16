from __future__ import annotations

import json
import sqlite3
from typing import Any

from app.database import (
    MILESTONE_STATUSES,
    PROPERTY_TYPES,
    RELATIONSHIP_TYPES,
    SYSTEM_STATUSES,
    TASK_PRIORITIES,
    TASK_STATUSES,
    utc_now,
    WORKER_STATUSES,
    WORKER_TYPES,
    parse_json,
)


ENTITY_TYPES = ("text", "note", "scene", "task", "system", "worker", "milestone")


class DomainError(Exception):
    def __init__(self, code: str, status_code: int = 400, extra: dict[str, Any] | None = None):
        super().__init__(code)
        self.code = code
        self.status_code = status_code
        self.extra = extra or {}


def schema_row(
    connection: sqlite3.Connection,
    project_id: str,
    entity_type: str,
    schema_id: str | None = None,
) -> sqlite3.Row | None:
    if entity_type in ENTITY_TYPES:
        return None
    row = connection.execute(
        """SELECT * FROM entity_schemas
           WHERE project_id = ? AND (id = ? OR name = ?)""",
        (project_id, schema_id or entity_type, entity_type),
    ).fetchone()
    if row is None:
        raise DomainError(
            "unsupported_entity_type", 422,
            {"allowed": list(ENTITY_TYPES), "schema": entity_type},
        )
    if schema_id is not None and row["id"] != schema_id:
        raise DomainError("schema_project_mismatch", 409)
    return row


def validate_entity_type(
    connection: sqlite3.Connection,
    project_id: str,
    entity_type: str,
    schema_id: str | None = None,
) -> sqlite3.Row | None:
    if entity_type not in ENTITY_TYPES:
        return schema_row(connection, project_id, entity_type, schema_id)
    if schema_id:
        raise DomainError("built_in_entity_cannot_use_schema", 422)
    return None


def _as_dict(row: sqlite3.Row | dict[str, Any]) -> dict[str, Any]:
    return dict(row)


def _json_list(value: str | None) -> list[str]:
    decoded = parse_json(value, [])
    return [str(item) for item in decoded] if isinstance(decoded, list) else []


def _entity_row(connection: sqlite3.Connection, entity_id: str) -> sqlite3.Row | None:
    return connection.execute("SELECT * FROM entities WHERE id = ?", (entity_id,)).fetchone()


def assert_reference(
    connection: sqlite3.Connection,
    project_id: str,
    entity_id: str | None,
    expected_type: str | None,
    field_name: str,
) -> None:
    if not entity_id:
        return
    row = _entity_row(connection, entity_id)
    if row is None or row["project_id"] != project_id:
        raise DomainError("reference_not_found", 404, {"field": field_name, "entity_id": entity_id})
    if expected_type is not None and row["entity_type"] != expected_type:
        raise DomainError(
            "reference_type_mismatch",
            422,
            {"field": field_name, "expected_type": expected_type, "entity_id": entity_id},
        )


def _number(value: Any, default: float = 0.0) -> float:
    try:
        return float(value)
    except (TypeError, ValueError):
        return default


def _schema_definitions(connection: sqlite3.Connection, schema_id: str) -> list[sqlite3.Row]:
    return connection.execute(
        "SELECT * FROM property_definitions WHERE schema_id = ? ORDER BY sort_order, id",
        (schema_id,),
    ).fetchall()


def property_definition_to_dict(row: sqlite3.Row | dict[str, Any]) -> dict[str, Any]:
    value = _as_dict(row)
    value["default_value"] = parse_json(value.pop("default_value_json", "null"), None)
    value["options"] = parse_json(value.pop("options_json", "[]"), [])
    value["required"] = bool(value.get("required", 0))
    return value


def schema_to_dict(connection: sqlite3.Connection, row: sqlite3.Row | dict[str, Any]) -> dict[str, Any]:
    value = _as_dict(row)
    value["properties"] = [property_definition_to_dict(item) for item in _schema_definitions(connection, value["id"])]
    return value


def _validate_property_value(
    connection: sqlite3.Connection,
    project_id: str,
    definition: sqlite3.Row,
    value: Any,
) -> Any:
    if value is None:
        return None
    property_type = definition["property_type"]
    if property_type not in PROPERTY_TYPES:
        raise DomainError("unsupported_property_type", 422, {"property_type": property_type})
    options = parse_json(definition["options_json"], [])
    if property_type in ("Text", "MultilineText", "Date", "URL"):
        if not isinstance(value, str):
            raise DomainError("invalid_property_value", 422, {"property": definition["name"]})
        return value
    if property_type == "Number":
        if isinstance(value, bool) or not isinstance(value, (int, float)):
            raise DomainError("invalid_property_value", 422, {"property": definition["name"]})
        number = float(value)
        minimum = definition["min_value"]
        maximum = definition["max_value"]
        step = definition["step_value"]
        if minimum is not None and number < float(minimum):
            raise DomainError("invalid_property_value", 422, {
                "property": definition["name"], "reason": "minimum", "min": float(minimum),
            })
        if maximum is not None and number > float(maximum):
            raise DomainError("invalid_property_value", 422, {
                "property": definition["name"], "reason": "maximum", "max": float(maximum),
            })
        if step is not None and float(step) > 0 and minimum is not None:
            offset = (number - float(minimum)) / float(step)
            if abs(offset - round(offset)) > 1e-6:
                raise DomainError("invalid_property_value", 422, {
                    "property": definition["name"], "reason": "step", "step": float(step),
                })
        return number
    if property_type == "Bool":
        if not isinstance(value, bool):
            raise DomainError("invalid_property_value", 422, {"property": definition["name"]})
        return value
    if property_type == "Enum":
        if not isinstance(value, str) or value not in options:
            raise DomainError(
                "invalid_property_value", 422,
                {"property": definition["name"], "options": options},
            )
        return value
    if property_type == "EntityReference":
        if not isinstance(value, str):
            raise DomainError("invalid_property_value", 422, {"property": definition["name"]})
        assert_reference(connection, project_id, value, None, definition["name"])
        return value
    if property_type == "EntityReferenceList":
        if not isinstance(value, list) or not all(isinstance(item, str) for item in value):
            raise DomainError("invalid_property_value", 422, {"property": definition["name"]})
        for item in value:
            assert_reference(connection, project_id, item, None, definition["name"])
        return value
    if property_type == "Tags":
        if not isinstance(value, list) or not all(isinstance(item, str) for item in value):
            raise DomainError("invalid_property_value", 422, {"property": definition["name"]})
        return value
    if property_type == "Progress":
        if isinstance(value, bool) or not isinstance(value, (int, float)) or not 0 <= value <= 100:
            raise DomainError("invalid_property_value", 422, {"property": definition["name"]})
        return float(value)
    return value


def normalize_custom_properties(
    connection: sqlite3.Connection,
    project_id: str,
    schema_id: str,
    properties: dict[str, Any] | None,
    *,
    existing: dict[str, Any] | None = None,
) -> dict[str, Any]:
    definitions = _schema_definitions(connection, schema_id)
    if not definitions:
        raise DomainError("schema_has_no_properties", 422, {"schema_id": schema_id})
    source = dict(existing or {})
    source.update(properties or {})
    known = {definition["name"] for definition in definitions}
    unknown = sorted(set(source) - known)
    if unknown:
        raise DomainError("unknown_property", 422, {"properties": unknown})
    result: dict[str, Any] = {}
    for definition in definitions:
        name = definition["name"]
        if name in source:
            value = source[name]
        else:
            value = parse_json(definition["default_value_json"], None)
        if value is None and definition["required"]:
            raise DomainError("required_property_missing", 422, {"property": name})
        if value is not None:
            result[name] = _validate_property_value(connection, project_id, definition, value)
    return result


def normalize_details(
    connection: sqlite3.Connection,
    project_id: str,
    entity_type: str,
    details: dict[str, Any] | None,
) -> dict[str, Any]:
    validate_entity_type(connection, project_id, entity_type)
    source = dict(details or {})
    if entity_type in ("text", "note"):
        return source
    if entity_type == "task":
        status = str(source.get("status", "Backlog"))
        priority = str(source.get("priority", "Normal"))
        if status not in TASK_STATUSES:
            raise DomainError("invalid_task_status", 422, {"allowed": list(TASK_STATUSES)})
        if priority not in TASK_PRIORITIES:
            raise DomainError("invalid_task_priority", 422, {"allowed": list(TASK_PRIORITIES)})
        assert_reference(connection, project_id, source.get("assigned_worker_id"), "worker", "assigned_worker_id")
        assert_reference(connection, project_id, source.get("milestone_id"), "milestone", "milestone_id")
        assert_reference(connection, project_id, source.get("system_id"), "system", "system_id")
        return {
            "status": status,
            "priority": priority,
            "assigned_worker_id": source.get("assigned_worker_id"),
            "milestone_id": source.get("milestone_id"),
            "system_id": source.get("system_id"),
            "branch": str(source.get("branch", "")),
            "allowed_paths": [str(item) for item in source.get("allowed_paths", [])],
            "forbidden_paths": [str(item) for item in source.get("forbidden_paths", [])],
            "acceptance_criteria": str(source.get("acceptance_criteria", "")),
            "started_at": source.get("started_at"),
            "completed_at": source.get("completed_at"),
        }
    if entity_type == "system":
        status = str(source.get("status", "Planned"))
        if status not in SYSTEM_STATUSES:
            raise DomainError("invalid_system_status", 422, {"allowed": list(SYSTEM_STATUSES)})
        assert_reference(connection, project_id, source.get("owner_worker_id"), "worker", "owner_worker_id")
        progress = _number(source.get("progress", 0))
        if not 0 <= progress <= 1:
            raise DomainError("invalid_system_progress", 422)
        return {
            "status": status,
            "owner_worker_id": source.get("owner_worker_id"),
            "progress": progress,
        }
    if entity_type == "worker":
        worker_type = str(source.get("worker_type", "Automation"))
        status = str(source.get("status", "Offline"))
        if worker_type not in WORKER_TYPES:
            raise DomainError("invalid_worker_type", 422, {"allowed": list(WORKER_TYPES)})
        if status not in WORKER_STATUSES:
            raise DomainError("invalid_worker_status", 422, {"allowed": list(WORKER_STATUSES)})
        worker_id = str(source.get("worker_id", ""))
        if not worker_id:
            raise DomainError("worker_id_required", 422)
        return {
            "worker_id": worker_id,
            "worker_type": worker_type,
            "status": status,
            "branch": str(source.get("branch", "")),
            "last_activity_at": source.get("last_activity_at"),
        }
    if entity_type == "milestone":
        status = str(source.get("status", "Planned"))
        if status not in MILESTONE_STATUSES:
            raise DomainError("invalid_milestone_status", 422, {"allowed": list(MILESTONE_STATUSES)})
        progress = _number(source.get("progress", 0))
        if not 0 <= progress <= 1:
            raise DomainError("invalid_milestone_progress", 422)
        return {
            "status": status,
            "target_date": source.get("target_date"),
            "progress": progress,
            "acceptance_criteria": str(source.get("acceptance_criteria", "")),
        }
    if entity_type == "scene":
        return {"asset_path": str(source.get("asset_path", ""))}
    return source


def stored_details(connection: sqlite3.Connection, row: sqlite3.Row | dict[str, Any]) -> dict[str, Any]:
    entity = _as_dict(row)
    entity_type = entity["entity_type"]
    entity_id = entity["id"]
    if entity_type == "task":
        detail = connection.execute("SELECT * FROM task_details WHERE entity_id = ?", (entity_id,)).fetchone()
        if detail is None:
            return normalize_details(connection, entity["project_id"], entity_type, {})
        value = dict(detail)
        value["allowed_paths"] = _json_list(value.pop("allowed_paths_json", "[]"))
        value["forbidden_paths"] = _json_list(value.pop("forbidden_paths_json", "[]"))
        value.pop("entity_id", None)
        return value
    if entity_type == "system":
        detail = connection.execute("SELECT * FROM system_details WHERE entity_id = ?", (entity_id,)).fetchone()
        value = dict(detail) if detail is not None else {}
        value.pop("entity_id", None)
        return value
    if entity_type == "worker":
        detail = connection.execute("SELECT * FROM worker_details WHERE entity_id = ?", (entity_id,)).fetchone()
        value = dict(detail) if detail is not None else {}
        value.pop("entity_id", None)
        return value
    if entity_type == "milestone":
        detail = connection.execute("SELECT * FROM milestone_details WHERE entity_id = ?", (entity_id,)).fetchone()
        value = dict(detail) if detail is not None else {}
        value.pop("entity_id", None)
        return value
    if entity_type == "scene":
        detail = connection.execute("SELECT * FROM scene_details WHERE entity_id = ?", (entity_id,)).fetchone()
        value = dict(detail) if detail is not None else {}
        value.pop("entity_id", None)
        return value
    schema_id = entity.get("schema_id")
    if schema_id:
        values = connection.execute(
            """SELECT pd.name, epv.value_json FROM entity_property_values epv
               JOIN property_definitions pd ON pd.id = epv.property_definition_id
               WHERE epv.entity_id = ? ORDER BY pd.sort_order, pd.id""",
            (entity_id,),
        ).fetchall()
        return {item["name"]: parse_json(item["value_json"], None) for item in values}
    return {}


def _computed_milestone_progress(connection: sqlite3.Connection, entity_id: str, fallback: float) -> tuple[float, int, int]:
    rows = connection.execute(
        """SELECT e.id, td.status FROM relationships r
           JOIN entities e ON e.id = r.source_entity_id
           JOIN task_details td ON td.entity_id = e.id
           WHERE r.target_entity_id = ? AND r.relationship_type = 'belongs_to_milestone'""",
        (entity_id,),
    ).fetchall()
    total = len(rows)
    done = sum(1 for row in rows if row["status"] == "Done")
    return (done / total if total else fallback, done, total)


def entity_to_dict(connection: sqlite3.Connection, row: sqlite3.Row | dict[str, Any]) -> dict[str, Any]:
    entity = _as_dict(row)
    entity["data"] = parse_json(entity.pop("data_json", "{}"), {})
    entity["description"] = entity.get("description", "")
    entity["revision"] = int(entity.get("revision", 1))
    details = stored_details(connection, row)
    if entity["entity_type"] == "milestone":
        stored_progress = _number(details.get("progress", 0))
        progress, done, total = _computed_milestone_progress(connection, entity["id"], stored_progress)
        details["progress"] = progress
        details["done_task_count"] = done
        details["total_related_task_count"] = total
    if entity["entity_type"] == "worker":
        task = connection.execute(
            """SELECT e.id, e.title, td.status FROM task_details td
               JOIN entities e ON e.id = td.entity_id
               WHERE td.assigned_worker_id = ? AND td.status NOT IN ('Done', 'Blocked')
               ORDER BY e.updated_at DESC LIMIT 1""",
            (entity["id"],),
        ).fetchone()
        details["current_task_id"] = task["id"] if task else None
        details["current_task"] = task["title"] if task else None
        details["current_task_status"] = task["status"] if task else None
        details["current_locks"] = [
            dict(lock)
            for lock in connection.execute(
                "SELECT * FROM scene_locks WHERE worker_id = ? AND status = 'LOCKED' ORDER BY scene_id",
                (details.get("worker_id", entity["id"]),),
            ).fetchall()
        ]
    entity["details"] = details
    if entity.get("schema_id"):
        schema = connection.execute(
            "SELECT * FROM entity_schemas WHERE id = ?", (entity["schema_id"],)
        ).fetchone()
        entity["schema"] = schema_to_dict(connection, schema) if schema is not None else None
        entity["properties"] = details
    else:
        entity["properties"] = {}
    return entity


def write_details(
    connection: sqlite3.Connection,
    entity_id: str,
    entity_type: str,
    details: dict[str, Any],
    *,
    schema_id: str | None = None,
    properties: dict[str, Any] | None = None,
) -> None:
    if entity_type == "task":
        connection.execute(
            """INSERT INTO task_details
               (entity_id, status, priority, assigned_worker_id, milestone_id, system_id,
                branch, allowed_paths_json, forbidden_paths_json, acceptance_criteria,
                started_at, completed_at)
               VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
               ON CONFLICT(entity_id) DO UPDATE SET
                status=excluded.status, priority=excluded.priority,
                assigned_worker_id=excluded.assigned_worker_id, milestone_id=excluded.milestone_id,
                system_id=excluded.system_id, branch=excluded.branch,
                allowed_paths_json=excluded.allowed_paths_json,
                forbidden_paths_json=excluded.forbidden_paths_json,
                acceptance_criteria=excluded.acceptance_criteria,
                started_at=excluded.started_at, completed_at=excluded.completed_at""",
            (
                entity_id,
                details["status"],
                details["priority"],
                details.get("assigned_worker_id"),
                details.get("milestone_id"),
                details.get("system_id"),
                details.get("branch", ""),
                json.dumps(details.get("allowed_paths", []), ensure_ascii=False),
                json.dumps(details.get("forbidden_paths", []), ensure_ascii=False),
                details.get("acceptance_criteria", ""),
                details.get("started_at"),
                details.get("completed_at"),
            ),
        )
    elif entity_type == "system":
        connection.execute(
            """INSERT INTO system_details(entity_id, status, owner_worker_id, progress)
               VALUES (?, ?, ?, ?)
               ON CONFLICT(entity_id) DO UPDATE SET
                 status=excluded.status, owner_worker_id=excluded.owner_worker_id,
                 progress=excluded.progress""",
            (entity_id, details["status"], details.get("owner_worker_id"), details.get("progress", 0)),
        )
    elif entity_type == "worker":
        try:
            connection.execute(
                """INSERT INTO worker_details
                   (entity_id, worker_id, worker_type, status, branch, last_activity_at)
                   VALUES (?, ?, ?, ?, ?, ?)
                   ON CONFLICT(entity_id) DO UPDATE SET
                    worker_id=excluded.worker_id, worker_type=excluded.worker_type,
                    status=excluded.status, branch=excluded.branch,
                    last_activity_at=excluded.last_activity_at""",
                (
                    entity_id,
                    details["worker_id"],
                    details["worker_type"],
                    details["status"],
                    details.get("branch", ""),
                    details.get("last_activity_at"),
                ),
            )
        except sqlite3.IntegrityError as error:
            raise DomainError("worker_id_already_exists", 409) from error
    elif entity_type == "milestone":
        connection.execute(
            """INSERT INTO milestone_details
               (entity_id, status, target_date, progress, acceptance_criteria)
               VALUES (?, ?, ?, ?, ?)
               ON CONFLICT(entity_id) DO UPDATE SET
                 status=excluded.status, target_date=excluded.target_date,
                 progress=excluded.progress, acceptance_criteria=excluded.acceptance_criteria""",
            (
                entity_id,
                details["status"],
                details.get("target_date"),
                details.get("progress", 0),
                details.get("acceptance_criteria", ""),
            ),
        )
    elif entity_type == "scene":
        connection.execute(
            """INSERT INTO scene_details(entity_id, asset_path) VALUES (?, ?)
               ON CONFLICT(entity_id) DO UPDATE SET asset_path=excluded.asset_path""",
            (entity_id, details.get("asset_path", "")),
        )
    elif schema_id is not None:
        now = utc_now()
        connection.execute("DELETE FROM entity_property_values WHERE entity_id = ?", (entity_id,))
        definitions = connection.execute(
            "SELECT id, name FROM property_definitions WHERE schema_id = ?", (schema_id,)
        ).fetchall()
        for definition in definitions:
            if definition["name"] not in (properties or {}):
                continue
            connection.execute(
                """INSERT INTO entity_property_values(entity_id, property_definition_id, value_json, updated_at)
                   VALUES (?, ?, ?, ?)""",
                (
                    entity_id,
                    definition["id"],
                    json.dumps(properties[definition["name"]], ensure_ascii=False),
                    now,
                ),
            )


def relationship_to_dict(row: sqlite3.Row | dict[str, Any]) -> dict[str, Any]:
    value = dict(row)
    if "visible" in value:
        value["visible"] = bool(value["visible"])
    return value


def validate_relationship_type(relationship_type: str) -> None:
    if relationship_type not in RELATIONSHIP_TYPES:
        raise DomainError("unsupported_relationship_type", 422, {"allowed": list(RELATIONSHIP_TYPES)})
