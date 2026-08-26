"""
Ozayn Native Database — SQLite persistence layer
Replaces the PHP backend entirely. All data stored locally.
"""

import sqlite3
import os
import hashlib
import secrets
import time
import json
from datetime import datetime, timedelta
from pathlib import Path


DB_DIR = Path.home() / ".ozayn"
DB_PATH = DB_DIR / "ozayn.db"


def _ensure_db():
    DB_DIR.mkdir(parents=True, exist_ok=True)


def get_conn():
    _ensure_db()
    conn = sqlite3.connect(str(DB_PATH))
    conn.row_factory = sqlite3.Row
    conn.execute("PRAGMA journal_mode=WAL")
    conn.execute("PRAGMA foreign_keys=ON")
    return conn


def init_db():
    conn = get_conn()
    c = conn.cursor()

    c.executescript("""
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL,
            password_hash TEXT NOT NULL,
            email TEXT,
            full_name TEXT,
            role TEXT DEFAULT 'user',
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );

        CREATE TABLE IF NOT EXISTS sessions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL,
            token TEXT UNIQUE NOT NULL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            expires_at TIMESTAMP,
            FOREIGN KEY (user_id) REFERENCES users(id)
        );

        CREATE TABLE IF NOT EXISTS conversations (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL,
            title TEXT DEFAULT 'New Conversation',
            project_id INTEGER,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (user_id) REFERENCES users(id)
        );

        CREATE TABLE IF NOT EXISTS messages (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            conversation_id INTEGER NOT NULL,
            role TEXT NOT NULL,
            content TEXT NOT NULL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (conversation_id) REFERENCES conversations(id) ON DELETE CASCADE
        );

        CREATE TABLE IF NOT EXISTS projects (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL,
            name TEXT NOT NULL,
            description TEXT DEFAULT '',
            status TEXT DEFAULT 'active',
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (user_id) REFERENCES users(id)
        );

        CREATE TABLE IF NOT EXISTS tasks (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL,
            project_id INTEGER,
            title TEXT NOT NULL,
            description TEXT DEFAULT '',
            status TEXT DEFAULT 'pending',
            priority TEXT DEFAULT 'medium',
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (user_id) REFERENCES users(id)
        );

        CREATE TABLE IF NOT EXISTS knowledge (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL,
            title TEXT NOT NULL,
            content TEXT NOT NULL,
            tags TEXT DEFAULT '',
            project_id INTEGER,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (user_id) REFERENCES users(id)
        );

        CREATE TABLE IF NOT EXISTS memory (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL,
            key TEXT NOT NULL,
            value TEXT NOT NULL,
            memory_type TEXT DEFAULT 'short_term',
            importance REAL DEFAULT 0.5,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (user_id) REFERENCES users(id)
        );

        CREATE TABLE IF NOT EXISTS decisions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL,
            context TEXT NOT NULL,
            options TEXT,
            chosen_option TEXT,
            reasoning TEXT,
            status TEXT DEFAULT 'pending',
            project_id INTEGER,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            decided_at TIMESTAMP,
            FOREIGN KEY (user_id) REFERENCES users(id)
        );

        CREATE TABLE IF NOT EXISTS audit_log (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER,
            action TEXT NOT NULL,
            details TEXT DEFAULT '',
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );
    """)

    conn.commit()
    conn.close()


# ─── Auth ───────────────────────────────────────────────────────────────────

def hash_password(password):
    salt = secrets.token_hex(16)
    h = hashlib.sha256(f"{salt}:{password}".encode()).hexdigest()
    return f"{salt}:{h}"


def verify_password(stored, password):
    salt, h = stored.split(":", 1)
    return hashlib.sha256(f"{salt}:{password}".encode()).hexdigest() == h


def create_user(username, password, email=None, full_name=None):
    conn = get_conn()
    try:
        conn.execute(
            "INSERT INTO users (username, password_hash, email, full_name) VALUES (?, ?, ?, ?)",
            (username, hash_password(password), email, full_name)
        )
        conn.commit()
        user = conn.execute("SELECT * FROM users WHERE username = ?", (username,)).fetchone()
        return dict(user) if user else None
    except sqlite3.IntegrityError:
        return None
    finally:
        conn.close()


def authenticate_user(username, password):
    conn = get_conn()
    user = conn.execute("SELECT * FROM users WHERE username = ?", (username,)).fetchone()
    conn.close()
    if user and verify_password(user["password_hash"], password):
        return dict(user)
    return None


def create_session(user_id):
    conn = get_conn()
    token = secrets.token_hex(32)
    expires = datetime.now() + timedelta(days=30)
    conn.execute(
        "INSERT INTO sessions (user_id, token, expires_at) VALUES (?, ?, ?)",
        (user_id, token, expires.isoformat())
    )
    conn.commit()
    conn.close()
    return token


def get_session(token):
    conn = get_conn()
    session = conn.execute(
        "SELECT s.*, u.id as uid, u.username, u.email, u.full_name, u.role "
        "FROM sessions s JOIN users u ON s.user_id = u.id "
        "WHERE s.token = ? AND s.expires_at > ?",
        (token, datetime.now().isoformat())
    ).fetchone()
    conn.close()
    if session:
        return {
            "session_id": session["token"],
            "user": {
                "id": session["uid"],
                "username": session["username"],
                "email": session["email"],
                "full_name": session["full_name"],
                "role": session["role"],
            }
        }
    return None


def delete_session(token):
    conn = get_conn()
    conn.execute("DELETE FROM sessions WHERE token = ?", (token,))
    conn.commit()
    conn.close()


# ─── Conversations ──────────────────────────────────────────────────────────

def list_conversations(user_id):
    conn = get_conn()
    rows = conn.execute(
        "SELECT * FROM conversations WHERE user_id = ? ORDER BY updated_at DESC", (user_id,)
    ).fetchall()
    conn.close()
    return [dict(r) for r in rows]


def create_conversation(user_id, title="New Conversation", project_id=None):
    conn = get_conn()
    cur = conn.execute(
        "INSERT INTO conversations (user_id, title, project_id) VALUES (?, ?, ?)",
        (user_id, title, project_id)
    )
    conn.commit()
    conv = conn.execute("SELECT * FROM conversations WHERE id = ?", (cur.lastrowid,)).fetchone()
    conn.close()
    return dict(conv) if conv else None


def get_messages(conversation_id):
    conn = get_conn()
    rows = conn.execute(
        "SELECT * FROM messages WHERE conversation_id = ? ORDER BY created_at", (conversation_id,)
    ).fetchall()
    conn.close()
    return [dict(r) for r in rows]


def add_message(conversation_id, role, content):
    conn = get_conn()
    conn.execute(
        "INSERT INTO messages (conversation_id, role, content) VALUES (?, ?, ?)",
        (conversation_id, role, content)
    )
    conn.execute(
        "UPDATE conversations SET updated_at = CURRENT_TIMESTAMP WHERE id = ?",
        (conversation_id,)
    )
    conn.commit()
    conn.close()


# ─── Projects ───────────────────────────────────────────────────────────────

def list_projects(user_id):
    conn = get_conn()
    rows = conn.execute(
        "SELECT * FROM projects WHERE user_id = ? ORDER BY created_at DESC", (user_id,)
    ).fetchall()
    conn.close()
    return [dict(r) for r in rows]


def create_project(user_id, name, description=""):
    conn = get_conn()
    cur = conn.execute(
        "INSERT INTO projects (user_id, name, description) VALUES (?, ?, ?)",
        (user_id, name, description)
    )
    conn.commit()
    proj = conn.execute("SELECT * FROM projects WHERE id = ?", (cur.lastrowid,)).fetchone()
    conn.close()
    return dict(proj) if proj else None


# ─── Tasks ──────────────────────────────────────────────────────────────────

def list_tasks(user_id):
    conn = get_conn()
    rows = conn.execute(
        "SELECT * FROM tasks WHERE user_id = ? ORDER BY created_at DESC", (user_id,)
    ).fetchall()
    conn.close()
    return [dict(r) for r in rows]


def create_task(user_id, title, description="", priority="medium", project_id=None):
    conn = get_conn()
    cur = conn.execute(
        "INSERT INTO tasks (user_id, title, description, priority, project_id) VALUES (?, ?, ?, ?, ?)",
        (user_id, title, description, priority, project_id)
    )
    conn.commit()
    task = conn.execute("SELECT * FROM tasks WHERE id = ?", (cur.lastrowid,)).fetchone()
    conn.close()
    return dict(task) if task else None


def update_task(task_id, status):
    conn = get_conn()
    conn.execute("UPDATE tasks SET status = ? WHERE id = ?", (status, task_id))
    conn.commit()
    task = conn.execute("SELECT * FROM tasks WHERE id = ?", (task_id,)).fetchone()
    conn.close()
    return dict(task) if task else None


# ─── Knowledge ──────────────────────────────────────────────────────────────

def list_knowledge(user_id):
    conn = get_conn()
    rows = conn.execute(
        "SELECT * FROM knowledge WHERE user_id = ? ORDER BY created_at DESC", (user_id,)
    ).fetchall()
    conn.close()
    return [dict(r) for r in rows]


def add_knowledge(user_id, title, content, tags="", project_id=None):
    conn = get_conn()
    cur = conn.execute(
        "INSERT INTO knowledge (user_id, title, content, tags, project_id) VALUES (?, ?, ?, ?, ?)",
        (user_id, title, content, tags, project_id)
    )
    conn.commit()
    k = conn.execute("SELECT * FROM knowledge WHERE id = ?", (cur.lastrowid,)).fetchone()
    conn.close()
    return dict(k) if k else None


# ─── Memory ─────────────────────────────────────────────────────────────────

def memory_search(user_id, query):
    conn = get_conn()
    rows = conn.execute(
        "SELECT * FROM memory WHERE user_id = ? AND (key LIKE ? OR value LIKE ?) "
        "ORDER BY importance DESC, created_at DESC LIMIT 20",
        (user_id, f"%{query}%", f"%{query}%")
    ).fetchall()
    conn.close()
    return [dict(r) for r in rows]


def memory_recent(user_id, limit=20):
    conn = get_conn()
    rows = conn.execute(
        "SELECT * FROM memory WHERE user_id = ? ORDER BY created_at DESC LIMIT ?",
        (user_id, limit)
    ).fetchall()
    conn.close()
    return [dict(r) for r in rows]


def memory_store(user_id, key, value, memory_type="short_term", importance=0.5):
    conn = get_conn()
    cur = conn.execute(
        "INSERT INTO memory (user_id, key, value, memory_type, importance) VALUES (?, ?, ?, ?, ?)",
        (user_id, key, value, memory_type, importance)
    )
    conn.commit()
    m = conn.execute("SELECT * FROM memory WHERE id = ?", (cur.lastrowid,)).fetchone()
    conn.close()
    return dict(m) if m else None


# ─── Decisions ──────────────────────────────────────────────────────────────

def list_decisions(user_id):
    conn = get_conn()
    rows = conn.execute(
        "SELECT * FROM decisions WHERE user_id = ? ORDER BY created_at DESC", (user_id,)
    ).fetchall()
    conn.close()
    return [dict(r) for r in rows]


def create_decision(user_id, context, options=None, project_id=None):
    conn = get_conn()
    opts = json.dumps(options) if options else None
    cur = conn.execute(
        "INSERT INTO decisions (user_id, context, options, project_id) VALUES (?, ?, ?, ?)",
        (user_id, context, opts, project_id)
    )
    conn.commit()
    d = conn.execute("SELECT * FROM decisions WHERE id = ?", (cur.lastrowid,)).fetchone()
    conn.close()
    return dict(d) if d else None


def get_decision(decision_id):
    conn = get_conn()
    d = conn.execute("SELECT * FROM decisions WHERE id = ?", (decision_id,)).fetchone()
    conn.close()
    return dict(d) if d else None


def make_decision(decision_id, chosen_option, reasoning=""):
    conn = get_conn()
    conn.execute(
        "UPDATE decisions SET chosen_option = ?, reasoning = ?, status = 'decided', "
        "decided_at = CURRENT_TIMESTAMP WHERE id = ?",
        (chosen_option, reasoning, decision_id)
    )
    conn.commit()
    d = conn.execute("SELECT * FROM decisions WHERE id = ?", (decision_id,)).fetchone()
    conn.close()
    return dict(d) if d else None


# ─── Audit Log ──────────────────────────────────────────────────────────────

def audit_log(user_id=None, action="", details=""):
    conn = get_conn()
    conn.execute(
        "INSERT INTO audit_log (user_id, action, details) VALUES (?, ?, ?)",
        (user_id, action, details)
    )
    conn.commit()
    conn.close()


def get_audit_log(user_id=None, limit=100):
    conn = get_conn()
    if user_id:
        rows = conn.execute(
            "SELECT * FROM audit_log WHERE user_id = ? ORDER BY created_at DESC LIMIT ?",
            (user_id, limit)
        ).fetchall()
    else:
        rows = conn.execute(
            "SELECT * FROM audit_log ORDER BY created_at DESC LIMIT ?", (limit,)
        ).fetchall()
    conn.close()
    return [dict(r) for r in rows]


def audit_search(query, limit=50):
    conn = get_conn()
    rows = conn.execute(
        "SELECT * FROM audit_log WHERE action LIKE ? OR details LIKE ? "
        "ORDER BY created_at DESC LIMIT ?",
        (f"%{query}%", f"%{query}%", limit)
    ).fetchall()
    conn.close()
    return [dict(r) for r in rows]


# ─── Seed Demo User ─────────────────────────────────────────────────────────

def seed_demo():
    if not create_user("demo", "demo123", email="demo@ozayn.ai", full_name="Demo User"):
        pass  # already exists
