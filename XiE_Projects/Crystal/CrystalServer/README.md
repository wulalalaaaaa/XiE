# Crystal Server

Crystal Server uses FastAPI + SQLite. Keep dependencies inside the project
virtual environment; `requirements.txt` pins the versions used by the tests.
From this directory:

```powershell
$env:PYTHONUTF8 = "1"
.\.venv\Scripts\python.exe -m pip install -r requirements.txt
$env:PYTHONPATH = (Get-Location).Path
.\.venv\Scripts\python.exe run_server.py
```

Run the complete server test set with:

```powershell
.\.venv\Scripts\python.exe -m pytest -q
```

The development server listens on `http://127.0.0.1:8787`. Set
`CRYSTAL_HOST`, `CRYSTAL_PORT`, or `CRYSTAL_DB_PATH` when integrating a
deployment. The production target can place the same ASGI app behind HTTPS
and WSS at `crystal.559494.xyz`.
