"""
Ozayn Native API — Pure Python, no HTTP, no PHP backend
Replaces api_client.py entirely. All operations go through SQLite directly.
"""

import os
import json
import platform
import subprocess
from datetime import datetime

import database as db


class OzaynAPI:
    """Native desktop API — zero network dependency."""

    def __init__(self):
        db.init_db()
        db.seed_demo()
        self.session_id = None
        self.user = None

    def _uid(self):
        return self.user["id"] if self.user else None

    def _audit(self, action, details=""):
        db.audit_log(self._uid(), action, details)

    # ─── Auth ───────────────────────────────────────────────────────────

    def login(self, username, password):
        user = db.authenticate_user(username, password)
        if not user:
            return {"success": False, "error": "Invalid username or password"}
        token = db.create_session(user["id"])
        self.session_id = token
        self.user = user
        self._audit("login", f"User {username} logged in")
        return {"success": True, "session_id": token, "user": user}

    def register(self, username, password, email=None, fullname=None):
        user = db.create_user(username, password, email, fullname)
        if not user:
            return {"success": False, "error": "Username already exists"}
        self._audit("register", f"New user: {username}")
        return {"success": True, "user": user}

    def logout(self):
        if self.session_id:
            db.delete_session(self.session_id)
            self._audit("logout", f"User {self.user.get('username', '?')} logged out")
        self.session_id = None
        self.user = None
        return {"success": True}

    # ─── Chat ───────────────────────────────────────────────────────────

    def send_chat(self, message, conversation_id=None, project_id=None):
        uid = self._uid()
        if not conversation_id:
            conv = db.create_conversation(uid, title=message[:50], project_id=project_id)
            conversation_id = conv["id"]

        db.add_message(conversation_id, "user", message)

        # Simple AI response (local, no external API needed)
        response = self._generate_response(message)

        db.add_message(conversation_id, "assistant", response)
        self._audit("chat", f"Message in conversation {conversation_id}")

        return {
            "success": True,
            "response": response,
            "conversation_id": conversation_id,
        }

    def _generate_response(self, message):
        """Generate a local AI response. Replace with LLM API when available."""
        msg = message.lower()

        if any(w in msg for w in ["hello", "hi", "hey"]):
            return "Hello! I'm Ozayn, your AI Digital Twin. How can I help you today?"
        elif any(w in msg for w in ["who are you", "what are you"]):
            return "I'm Ozayn — your personal AI Digital Twin, part of the ARWE intelligence network. I help you manage decisions, projects, knowledge, and system intelligence."
        elif any(w in msg for w in ["help", "what can you do"]):
            return ("I can help you with:\n"
                    "• Chat & conversation\n"
                    "• Project & task management\n"
                    "• Knowledge base management\n"
                    "• Decision analysis\n"
                    "• ARWE system monitoring\n"
                    "• System information\n\n"
                    "Just ask me anything!")
        elif "time" in msg:
            return f"The current time is {datetime.now().strftime('%H:%M:%S')}."
        elif "date" in msg:
            return f"Today is {datetime.now().strftime('%d %B %Y')}."
        elif "system" in msg or "status" in msg:
            return self._system_summary()
        else:
            return (f"I received your message: \"{message}\"\n\n"
                    "I'm running locally on your desktop. For full AI capabilities, "
                    "connect me to an LLM API in Settings.")

    def _system_summary(self):
        try:
            from ozayn import core_bindings as core
            cpu = core.get_cpu_usage()
            mem = core.get_memory_usage()
            return (f"System Status:\n"
                    f"• CPU: {cpu:.1f}%\n"
                    f"• Memory: {mem:.1f}%\n"
                    f"• Hostname: {core.get_hostname()}\n"
                    f"• Uptime: {core.get_uptime()}")
        except Exception:
            return "System info unavailable (C core not loaded)"

    def list_conversations(self):
        return {"conversations": db.list_conversations(self._uid()) or []}

    def get_chat_history(self, conversation_id):
        messages = db.get_messages(conversation_id)
        return {"messages": messages}

    # ─── Projects ───────────────────────────────────────────────────────

    def list_projects(self):
        return {"projects": db.list_projects(self._uid()) or []}

    def create_project(self, name, description=""):
        proj = db.create_project(self._uid(), name, description)
        self._audit("create_project", f"Created: {name}")
        return {"success": True, "project": proj}

    # ─── Tasks ──────────────────────────────────────────────────────────

    def list_tasks(self):
        return {"tasks": db.list_tasks(self._uid()) or []}

    def create_task(self, title, description="", priority="medium", project_id=None):
        task = db.create_task(self._uid(), title, description, priority, project_id)
        self._audit("create_task", f"Created: {title}")
        return {"success": True, "task": task}

    def update_task(self, task_id, status):
        task = db.update_task(task_id, status)
        self._audit("update_task", f"Task {task_id} → {status}")
        return {"success": True, "task": task}

    # ─── Knowledge ──────────────────────────────────────────────────────

    def list_knowledge(self):
        return {"knowledge": db.list_knowledge(self._uid()) or []}

    def add_knowledge(self, title, content, tags=None, project_id=None):
        k = db.add_knowledge(self._uid(), title, content, tags or "", project_id)
        self._audit("add_knowledge", f"Added: {title}")
        return {"success": True, "knowledge": k}

    # ─── Memory ─────────────────────────────────────────────────────────

    def memory_search(self, query):
        return {"results": db.memory_search(self._uid(), query)}

    def memory_recent(self):
        return {"results": db.memory_recent(self._uid())}

    def memory_store(self, key, value, memory_type="short_term", importance=0.5):
        m = db.memory_store(self._uid(), key, value, memory_type, importance)
        return {"success": True, "memory": m}

    # ─── Decisions ──────────────────────────────────────────────────────

    def list_decisions(self):
        return {"decisions": db.list_decisions(self._uid()) or []}

    def create_decision(self, context, options=None, project_id=None):
        d = db.create_decision(self._uid(), context, options, project_id)
        self._audit("create_decision", f"Decision: {context[:50]}")
        return {"success": True, "decision": d}

    def get_decision(self, decision_id):
        d = db.get_decision(decision_id)
        return {"decision": d} if d else {"error": "Not found"}

    def make_decision(self, decision_id, chosen_option, reasoning=""):
        d = db.make_decision(decision_id, chosen_option, reasoning)
        self._audit("make_decision", f"Decided: {chosen_option}")
        return {"success": True, "decision": d}

    # ─── Audit ──────────────────────────────────────────────────────────

    def audit_log(self):
        return {"entries": db.get_audit_log(self._uid())}

    def audit_recent(self):
        return {"entries": db.get_audit_log(self._uid(), limit=20)}

    def audit_search(self, query):
        return {"entries": db.audit_search(query)}

    # ─── ARWE ───────────────────────────────────────────────────────────

    def arwe_status(self):
        systems = {
            "govyx": {
                "name": "Govyx",
                "description": "Government Services",
                "status": "online",
                "details": {"version": "2.4.1", "uptime": "99.9%", "requests": "1.2M/day"}
            },
            "edunex": {
                "name": "Edunex",
                "description": "Education Platform",
                "status": "online",
                "details": {"version": "3.1.0", "uptime": "99.8%", "students": "450K"}
            },
            "locify": {
                "name": "Locify",
                "description": "Identity & Location",
                "status": "online",
                "details": {"version": "1.8.3", "uptime": "99.7%", "identities": "2.1M"}
            },
            "terrachain": {
                "name": "TerraChain",
                "description": "Land & Procurement",
                "status": "online",
                "details": {"version": "2.0.1", "uptime": "99.6%", "records": "890K"}
            },
            "bilen": {
                "name": "Bilen",
                "description": "Security Intelligence",
                "status": "online",
                "details": {"version": "4.2.0", "uptime": "99.99%", "threats_blocked": "12K"}
            },
            "kidane": {
                "name": "Kidane",
                "description": "Drone Intelligence",
                "status": "online",
                "details": {"version": "1.5.2", "uptime": "99.5%", "flights": "340/day"}
            },
        }
        return {"systems": systems}

    def arwe_briefing(self):
        return {
            "briefing": (
                "ARWE Daily Intelligence Briefing\n"
                "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n"
                "• Govyx: 4 pending government workflows require attention\n"
                "• Edunex: Grade 12 performance up 12% this quarter\n"
                "• Locify: 2,340 new identity verifications processed\n"
                "• TerraChain: Procurement anomaly flagged in Addis region\n"
                "• Bilen: 3 medium-risk network patterns detected\n"
                "• Kidane: All drone fleets operational, 340 flights today"
            )
        }

    def arwe_system_status(self, system):
        status = self.arwe_status()
        systems = status.get("systems", {})
        sys_info = systems.get(system.lower(), {"status": "unknown", "details": {}})
        return {"system": system, **sys_info}

    # ─── System (via C core) ────────────────────────────────────────────

    def system_overview(self):
        try:
            from ozayn import core_bindings as core
            return {
                "hostname": core.get_hostname(),
                "uptime": core.get_uptime(),
                "cpu": core.get_cpu_usage(),
                "memory": core.get_memory_usage(),
                "disk": core.get_disk_usage("/"),
                "version": core.version(),
            }
        except Exception as e:
            return {"error": str(e)}

    def system_cpu(self):
        try:
            from ozayn import core_bindings as core
            return {"cpu": core.get_cpu_usage()}
        except Exception as e:
            return {"error": str(e)}

    def system_memory(self):
        try:
            from ozayn import core_bindings as core
            return {"memory": core.get_memory_usage()}
        except Exception as e:
            return {"error": str(e)}

    def system_disk(self):
        try:
            from ozayn import core_bindings as core
            return {"disk": core.get_disk_usage("/")}
        except Exception as e:
            return {"error": str(e)}

    def system_processes(self, sort="cpu", limit=20):
        try:
            from ozayn import core_bindings as core
            return {"processes": core.list_processes(sort, limit)}
        except Exception as e:
            return {"error": str(e)}

    def system_network(self):
        return {"status": "ok", "platform": platform.system()}

    # ─── Agents ─────────────────────────────────────────────────────────

    def list_agents(self):
        return {"agents": [
            {"name": "Chat Agent", "status": "active", "type": "conversation"},
            {"name": "Decision Agent", "status": "active", "type": "analysis"},
            {"name": "System Agent", "status": "active", "type": "monitoring"},
            {"name": "ARWE Agent", "status": "active", "type": "intelligence"},
        ]}

    # ─── Apps ───────────────────────────────────────────────────────────

    def list_apps(self):
        return {"apps": [
            {"name": "Terminal", "command": "terminal"},
            {"name": "File Manager", "command": "files"},
            {"name": "Settings", "command": "settings"},
        ]}

    def launch_app(self, app, args=None):
        return {"success": True, "message": f"Launching {app}"}

    # ─── Code ───────────────────────────────────────────────────────────

    def analyze_code(self, code, language=None):
        lines = len(code.split("\n"))
        chars = len(code)
        return {
            "analysis": {
                "lines": lines,
                "characters": chars,
                "language": language or "unknown",
                "assessment": "Code analyzed successfully"
            }
        }

    def generate_code(self, type_="function", name="MyFunction", language="python"):
        templates = {
            "python": f"def {name}():\n    pass",
            "javascript": f"function {name}() {{}}",
            "php": f"function {name}() {{}}",
        }
        code = templates.get(language, templates["python"])
        return {"code": code, "language": language}

    # ─── Collab ─────────────────────────────────────────────────────────

    def collab_sessions(self):
        return {"sessions": []}

    def collab_create(self, name):
        return {"success": True, "session": {"name": name, "id": "local"}}

    # ─── Tools ──────────────────────────────────────────────────────────

    def tools_run(self, command, timeout=30):
        try:
            result = subprocess.run(
                command, shell=True, capture_output=True, text=True, timeout=timeout
            )
            return {"output": result.stdout, "error": result.stderr, "code": result.returncode}
        except subprocess.TimeoutExpired:
            return {"error": "Command timed out"}
        except Exception as e:
            return {"error": str(e)}

    def tools_list_files(self, path="."):
        try:
            entries = []
            for entry in os.scandir(path):
                entries.append({
                    "name": entry.name,
                    "type": "directory" if entry.is_dir() else "file",
                    "size": entry.stat().st_size if entry.is_file() else 0,
                })
            return {"files": entries}
        except Exception as e:
            return {"error": str(e)}

    def tools_read_file(self, path):
        try:
            with open(path, "r") as f:
                content = f.read()
            return {"content": content, "path": path}
        except Exception as e:
            return {"error": str(e)}
