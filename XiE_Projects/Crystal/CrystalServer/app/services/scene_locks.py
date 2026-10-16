from __future__ import annotations

import sqlite3
from datetime import datetime, timedelta, timezone
from typing import Any

from app.database import Database, utc_now


STALE_AFTER = timedelta(seconds=60)


class SceneLockError(Exception):
    def __init__(self, message: str, lock: dict[str, Any] | None = None):
        super().__init__(message)
        self.message = message
        self.lock = lock or {}


class SceneNotFound(SceneLockError):
    pass


class AlreadyLocked(SceneLockError):
    pass


class WrongOwner(SceneLockError):
    pass


def _parse_time(value: str | None) -> datetime | None:
    if not value:
        return None
    try:
        return datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError:
        return None


def public_lock(row: sqlite3.Row | dict[str, Any] | None) -> dict[str, Any] | None:
    if row is None:
        return None
    value = dict(row)
    status = value.get("status", "FREE")
    heartbeat = _parse_time(value.get("heartbeat_at"))
    if status == "LOCKED" and heartbeat is not None:
        if datetime.now(timezone.utc) - heartbeat > STALE_AFTER:
            status = "STALE"
    value["status"] = status
    return value


class SceneLockService:
    def __init__(self, database: Database):
        self.database = database

    def list(self, project_id: str) -> list[dict[str, Any]]:
        with self.database.connect() as connection:
            rows = connection.execute(
                "SELECT * FROM scene_locks WHERE project_id = ? ORDER BY scene_id", (project_id,)
            ).fetchall()
        result: list[dict[str, Any]] = []
        for row in rows:
            lock = public_lock(row)
            if lock is not None:
                result.append(lock)
        return result

    def get(self, scene_id: str) -> dict[str, Any]:
        with self.database.connect() as connection:
            row = connection.execute(
                "SELECT * FROM scene_locks WHERE scene_id = ?", (scene_id,)
            ).fetchone()
        result = public_lock(row)
        if result is None:
            raise SceneNotFound("scene_lock_not_found")
        return result

    def acquire(
        self,
        scene_id: str,
        *,
        worker_id: str,
        worker_name: str,
        task: str = "",
        branch: str = "",
        task_id: str | None = None,
        task_note: str | None = None,
    ) -> dict[str, Any]:
        """Atomically transition FREE -> LOCKED.

        BEGIN IMMEDIATE serializes writers at the SQLite database level. The
        lock decision is made inside that transaction; no client-side
        GET/UPDATE check can create a second success.
        """

        connection = self.database.connect()
        try:
            connection.execute("BEGIN IMMEDIATE")
            row = connection.execute(
                "SELECT * FROM scene_locks WHERE scene_id = ?", (scene_id,)
            ).fetchone()
            if row is None:
                connection.rollback()
                raise SceneNotFound("scene_lock_not_found")
            if row["status"] != "FREE":
                connection.rollback()
                raise AlreadyLocked("already_locked", public_lock(row))
            acquired_at = utc_now()
            connection.execute(
                """UPDATE scene_locks
                   SET status = 'LOCKED', owner_id = ?, owner_name = ?, worker_id = ?,
                       task_id = ?, task = ?, task_note = ?, branch = ?,
                       acquired_at = ?, heartbeat_at = ?
                   WHERE scene_id = ? AND status = 'FREE'""",
                (worker_id, worker_name, worker_id, task_id, task, task_note or task,
                 branch, acquired_at, acquired_at, scene_id),
            )
            if connection.total_changes != 1:
                connection.rollback()
                row = connection.execute(
                    "SELECT * FROM scene_locks WHERE scene_id = ?", (scene_id,)
                ).fetchone()
                raise AlreadyLocked("already_locked", public_lock(row))
            updated = connection.execute(
                "SELECT * FROM scene_locks WHERE scene_id = ?", (scene_id,)
            ).fetchone()
            connection.commit()
            result = public_lock(updated)
            if result is None:
                raise SceneNotFound("scene_lock_not_found")
            return result
        except sqlite3.OperationalError:
            connection.rollback()
            raise
        finally:
            connection.close()

    def heartbeat(self, scene_id: str, *, worker_id: str) -> dict[str, Any]:
        connection = self.database.connect()
        try:
            connection.execute("BEGIN IMMEDIATE")
            row = connection.execute(
                "SELECT * FROM scene_locks WHERE scene_id = ?", (scene_id,)
            ).fetchone()
            if row is None:
                connection.rollback()
                raise SceneNotFound("scene_lock_not_found")
            if row["owner_id"] != worker_id:
                connection.rollback()
                raise WrongOwner("wrong_owner", public_lock(row))
            connection.execute(
                "UPDATE scene_locks SET heartbeat_at = ? WHERE scene_id = ? AND status = 'LOCKED'",
                (utc_now(), scene_id),
            )
            updated = connection.execute(
                "SELECT * FROM scene_locks WHERE scene_id = ?", (scene_id,)
            ).fetchone()
            connection.commit()
            return public_lock(updated) or {}
        finally:
            connection.close()

    def release(self, scene_id: str, *, worker_id: str) -> dict[str, Any]:
        connection = self.database.connect()
        try:
            connection.execute("BEGIN IMMEDIATE")
            row = connection.execute(
                "SELECT * FROM scene_locks WHERE scene_id = ?", (scene_id,)
            ).fetchone()
            if row is None:
                connection.rollback()
                raise SceneNotFound("scene_lock_not_found")
            if row["status"] != "FREE" and row["owner_id"] != worker_id:
                connection.rollback()
                raise WrongOwner("wrong_owner", public_lock(row))
            connection.execute(
                """UPDATE scene_locks SET status = 'FREE', owner_id = NULL,
                   owner_name = NULL, worker_id = NULL, task_id = NULL,
                   task = NULL, task_note = NULL, branch = NULL,
                   acquired_at = NULL, heartbeat_at = NULL
                   WHERE scene_id = ?""",
                (scene_id,),
            )
            updated = connection.execute(
                "SELECT * FROM scene_locks WHERE scene_id = ?", (scene_id,)
            ).fetchone()
            connection.commit()
            return public_lock(updated) or {}
        finally:
            connection.close()
