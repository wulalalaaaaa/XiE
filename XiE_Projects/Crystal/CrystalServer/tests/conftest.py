from __future__ import annotations

import uuid
from pathlib import Path

import pytest

from app.main import create_app


@pytest.fixture()
def app():
    path = Path(__file__).parent / f".phase4-server-{uuid.uuid4().hex}.db"
    try:
        yield create_app(path)
    finally:
        for suffix in ("", "-wal", "-shm"):
            Path(f"{path}{suffix}").unlink(missing_ok=True)
