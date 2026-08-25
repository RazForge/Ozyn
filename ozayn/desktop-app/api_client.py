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
