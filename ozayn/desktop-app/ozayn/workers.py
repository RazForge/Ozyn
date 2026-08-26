"""
Ozayn Workers — QThread pool for async API calls
"""

from PyQt6.QtCore import QThread, pyqtSignal, QTimer


class WorkerMixin:
    """Mixin that provides async worker tracking for any QMainWindow."""

    def _init_workers(self):
        self._workers = []

    def _run(self, method, data=None, callback=None):
        w = APIWorker(self.api, method, data)
        if callback:
            # Marshal callback to main thread via QTimer
            w.finished.connect(lambda r: QTimer.singleShot(0, lambda: callback(r)))
        w.finished.connect(lambda _: self._workers.remove(w) if w in self._workers else None)
        self._workers.append(w)
        w.start()
        return w


class APIWorker(QThread):
    finished = pyqtSignal(dict)

    def __init__(self, api, method, data=None):
        super().__init__()
        self.api = api
        self.method = method
        self.data = data or {}

    def run(self):
        try:
            result = self._do_work()
        except Exception as e:
            result = {"success": False, "error": str(e)}
        self.finished.emit(result)

    def _do_work(self):
        m, d = self.method, self.data
        if m == "login":
            return self.api.login(d["username"], d["password"])
        elif m == "register":
            return self.api.register(d["username"], d["password"], d.get("email"), d.get("full_name"))
        elif m == "chat":
            return self.api.send_chat(d["message"], d.get("conversation_id"), d.get("project_id"))
        elif m == "conversations":
            return self.api.list_conversations()
        elif m == "history":
            return self.api.get_chat_history(d["conversation_id"])
        elif m == "projects":
            return self.api.list_projects()
        elif m == "create_project":
            return self.api.create_project(d["name"], d.get("description", ""))
        elif m == "tasks":
            return self.api.list_tasks()
        elif m == "create_task":
            return self.api.create_task(d["title"], d.get("description", ""), d.get("priority", "medium"), d.get("project_id"))
        elif m == "update_task":
            return self.api.update_task(d["task_id"], d["status"])
        elif m == "knowledge":
            return self.api.list_knowledge()
        elif m == "add_knowledge":
            return self.api.add_knowledge(d["title"], d["content"], d.get("tags"), d.get("project_id"))
        elif m == "arwe_status":
            return self.api.arwe_status()
        elif m == "arwe_briefing":
            return self.api.arwe_briefing()
        elif m == "arwe_system":
            return self.api.arwe_system_status(d["system"])
        elif m == "decisions":
            return self.api.list_decisions()
        elif m == "create_decision":
            return self.api.create_decision(d["context"], d.get("options"), d.get("project_id"))
        elif m == "get_decision":
            return self.api.get_decision(d["decision_id"])
        elif m == "make_decision":
            return self.api.make_decision(d["decision_id"], d["chosen_option"], d.get("reasoning"))
        elif m == "audit":
            return self.api.audit_log()
        elif m == "audit_recent":
            return self.api.audit_recent()
        elif m == "audit_search":
            return self.api.audit_search(d["query"])
        elif m == "system_overview":
            return self.api.system_overview()
        elif m == "system_cpu":
            return self.api.system_cpu()
        elif m == "system_memory":
            return self.api.system_memory()
        elif m == "system_disk":
            return self.api.system_disk()
        elif m == "system_processes":
            return self.api.system_processes(d.get("sort", "cpu"), d.get("limit", 20))
        elif m == "system_network":
            return self.api.system_network()
        elif m == "agents":
            return self.api.list_agents()
        elif m == "apps":
            return self.api.list_apps()
        elif m == "launch_app":
            return self.api.launch_app(d["app"], d.get("args"))
        elif m == "analyze_code":
            return self.api.analyze_code(d["code"], d.get("language"))
        elif m == "generate_code":
            return self.api.generate_code(d.get("type", "function"), d.get("name", "MyFunction"), d.get("language", "php"))
        elif m == "collab_sessions":
            return self.api.collab_sessions()
        elif m == "collab_create":
            return self.api.collab_create(d["name"])
        elif m == "memory_search":
            return self.api.memory_search(d["query"])
        elif m == "memory_recent":
            return self.api.memory_recent()
        elif m == "memory_store":
            return self.api.memory_store(d["key"], d["value"], d.get("type", "short_term"), d.get("importance", 0.5))
        elif m == "tools_run":
            return self.api.tools_run(d["command"], d.get("timeout", 30))
        elif m == "tools_files":
            return self.api.tools_list_files(d.get("path", "."))
        elif m == "tools_read":
            return self.api.tools_read_file(d["path"])
        elif m == "logout":
            self.api.logout()
            return {"success": True}
        return {"success": False, "error": "Unknown method"}
