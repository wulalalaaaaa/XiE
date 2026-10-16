from __future__ import annotations

import os
from pathlib import Path

import uvicorn


if __name__ == "__main__":
    db_path = os.environ.get("CRYSTAL_DB_PATH")
    if db_path:
        os.environ["CRYSTAL_DB_PATH"] = str(Path(db_path).resolve())
    uvicorn.run(
        "app.main:app",
        host=os.environ.get("CRYSTAL_HOST", "127.0.0.1"),
        port=int(os.environ.get("CRYSTAL_PORT", "8787")),
        reload=False,
    )
