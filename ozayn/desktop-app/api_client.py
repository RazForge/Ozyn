import requests
import json
import os
import subprocess
import time

API_BASE = "http://127.0.0.1:8000/ozayn/backend/api"
ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))


class OzaynAPI:
    def __init__(self, base_url=API_BASE):
        self.base_url = base_url
        self.session_id = None
        self.user = None
        self.server_process = None
        self._ensure_server()

    def _ensure_server(self):
        try:
            r = requests.get(f"{self.base_url}/../..", timeout=2)
            return True
        except Exception:
            pass

        php_bin = "php"
        router = os.path.join(ROOT_DIR, "router.php")
        try:
            self.server_process = subprocess.Popen(
                [php_bin, "-S", "127.0.0.1:8000", "router.php"],
                cwd=ROOT_DIR,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                preexec_fn=os.setsid
            )
            for _ in range(10):
                time.sleep(0.5)
                try:
                    requests.get(f"{self.base_url}/../..", timeout=1)
                    return True
                except Exception:
                    continue
        except Exception:
            pass
        return False

    def _request(self, method, endpoint, data=None):
        url = f"{self.base_url}{endpoint}"
        headers = {"Content-Type": "application/json"}
        if self.session_id:
            headers["X-Session-ID"] = self.session_id

        try:
            if method == "GET":
                r = requests.get(url, headers=headers, timeout=15)
            elif method == "POST":
                r = requests.post(url, headers=headers, json=data, timeout=15)
            elif method == "PUT":
                r = requests.put(url, headers=headers, json=data, timeout=15)
            elif method == "DELETE":
                r = requests.delete(url, headers=headers, timeout=15)
            else:
                return {"success": False, "error": "Invalid method"}

            return r.json()
        except requests.ConnectionError:
            return {"success": False, "error": "Cannot connect to server"}
        except Exception as e:
            return {"success": False, "error": str(e)}

    def login(self, username, password):
        result = self._request("POST", "/auth/login", {"username": username, "password": password})
        if result.get("success"):
            self.session_id = result["session_id"]
            self.user = result["user"]
        return result

    def register(self, username, password, email=None, full_name=None):
        data = {"username": username, "password": password}
        if email:
            data["email"] = email
        if full_name:
            data["full_name"] = full_name
        return self._request("POST", "/auth/register", data)

    def logout(self):
        self._request("POST", "/auth/logout")
        self.session_id = None
        self.user = None

    def send_chat(self, message, conversation_id=None, project_id=None):
        data = {"message": message}
        if conversation_id:
            data["conversation_id"] = conversation_id
        if project_id:
            data["project_id"] = project_id
        return self._request("POST", "/chat/send", data)

    def list_conversations(self):
        return self._request("GET", "/chat/list")

    def get_chat_history(self, conversation_id):
        return self._request("GET", f"/chat/history/{conversation_id}")

    def list_projects(self):
        return self._request("GET", "/projects/list")

    def create_project(self, name, description=""):
        return self._request("POST", "/projects/create", {"name": name, "description": description})

    def list_tasks(self):
        return self._request("GET", "/tasks/list")

    def create_task(self, title, description="", priority="medium", project_id=None):
        data = {"title": title, "description": description, "priority": priority}
        if project_id:
            data["project_id"] = project_id
        return self._request("POST", "/tasks/create", data)

    def update_task(self, task_id, status):
        return self._request("PUT", f"/tasks/{task_id}", {"status": status})

    def list_knowledge(self):
        return self._request("GET", "/knowledge/list")

    def add_knowledge(self, title, content, tags=None, project_id=None):
        data = {"title": title, "content": content, "tags": tags or []}
        if project_id:
            data["project_id"] = project_id
        return self._request("POST", "/knowledge/add", data)

    # --- ARWE ---
    def arwe_status(self):
        return self._request("GET", "/arwe/status")

    def arwe_briefing(self):
        return self._request("GET", "/arwe/briefing")

    def arwe_system_status(self, system):
        return self._request("GET", f"/arwe/{system}")

    def arwe_config_systems(self):
        return self._request("GET", "/arwe/config/systems")

    def arwe_config_list(self):
        return self._request("GET", "/arwe/config/list")

    def arwe_config_save(self, system, config):
        return self._request("POST", "/arwe/config/save", {"system": system, "config": config})

    def arwe_config_test(self, system):
        return self._request("POST", "/arwe/config/test", {"system": system})

    # --- Decisions ---
    def list_decisions(self, status=None):
        url = "/decisions/list" if status else "/decisions"
        if status:
            url += f"?status={status}"
        return self._request("GET", url)

    def create_decision(self, context, options=None, project_id=None):
        data = {"context": context, "options": options or []}
        if project_id:
            data["project_id"] = project_id
        return self._request("POST", "/decisions/create", data)

    def get_decision(self, decision_id):
        return self._request("GET", f"/decisions/{decision_id}")

    def make_decision(self, decision_id, chosen_option, reasoning=None):
        data = {"chosen_option": chosen_option}
        if reasoning:
            data["reasoning"] = reasoning
        return self._request("PUT", f"/decisions/{decision_id}/decide", data)

    # --- Audit ---
    def audit_log(self, limit=100):
        return self._request("GET", f"/audit/log?limit={limit}")

    def audit_recent(self, limit=50):
        return self._request("GET", f"/audit/recent?limit={limit}")

    def audit_search(self, query):
        return self._request("GET", f"/audit/search?q={query}")

    # --- System ---
    def system_overview(self):
        return self._request("GET", "/system/overview")

    def system_cpu(self):
        return self._request("GET", "/system/cpu")

    def system_memory(self):
        return self._request("GET", "/system/memory")

    def system_disk(self):
        return self._request("GET", "/system/disk")

    def system_processes(self, sort="cpu", limit=20):
        return self._request("GET", f"/system/processes?sort={sort}&limit={limit}")

    def system_network(self):
        return self._request("GET", "/system/network")

    # --- Agents ---
    def list_agents(self):
        return self._request("GET", "/agents/list")

    def route_agent_task(self, task, context=None):
        return self._request("POST", "/agents/route", {"task": task, "context": context or {}})

    # --- Apps ---
    def list_apps(self):
        return self._request("GET", "/apps/list")

    def launch_app(self, app, args=None):
        return self._request("POST", "/apps/launch", {"app": app, "args": args or []})

    # --- Code ---
    def analyze_code(self, code, language=None):
        data = {"code": code}
        if language:
            data["language"] = language
        return self._request("POST", "/code/analyze", data)

    def generate_code(self, type_="function", name="MyFunction", language="php"):
        return self._request("POST", "/code/generate", {"type": type_, "name": name, "language": language})

    # --- Collaboration ---
    def collab_sessions(self):
        return self._request("GET", "/collab/sessions")

    def collab_create(self, name):
        return self._request("POST", "/collab/create", {"name": name})

    def collab_join(self, session_id):
        return self._request("POST", "/collab/join", {"session_id": session_id})

    # --- Memory ---
    def memory_search(self, query):
        return self._request("GET", f"/memory/search?q={query}")

    def memory_recent(self):
        return self._request("GET", "/memory/recent")

    def memory_store(self, key, value, type_="short_term", importance=0.5):
        return self._request("POST", "/memory/store", {
            "key": key, "value": value, "type": type_, "importance": importance
        })

    # --- Tools ---
    def tools_run(self, command, timeout=30):
        return self._request("POST", "/tools/run", {"command": command, "timeout": timeout})

    def tools_list_files(self, path="."):
        return self._request("GET", f"/tools/files?path={path}")

    def tools_read_file(self, path):
        return self._request("GET", f"/tools/read?path={path}")

    def cleanup(self):
        if self.server_process:
            try:
                self.server_process.terminate()
                self.server_process.wait(timeout=3)
            except Exception:
                try:
                    self.server_process.kill()
                except Exception:
                    pass
            self.server_process = None
