"""
Ozayn Core Bindings — ctypes interface to libozayn_core.so
Exposes C/C++ ML, vision, system, and crypto functions to Python
"""

import ctypes
import os
import json
import sys

_CORE_DIR = os.path.join(os.path.dirname(__file__), "..", "core", "build")
_CORE_PATH = os.path.join(_CORE_DIR, "libozayn_core.so")

_lib = None


def _load():
    global _lib
    if _lib is not None:
        return _lib
    if not os.path.exists(_CORE_PATH):
        raise OSError(f"Core library not found: {_CORE_PATH}\nRun 'make' in core/ first.")
    _lib = ctypes.CDLL(_CORE_PATH)
    _setup_signatures()
    return _lib


def _setup_signatures():
    lib = _lib
    lib.ozayn_version.restype = ctypes.c_char_p
    lib.ozayn_version.argtypes = []

    lib.ozayn_ml_load_model.restype = ctypes.c_void_p
    lib.ozayn_ml_load_model.argtypes = [ctypes.c_char_p]

    lib.ozayn_ml_free_model.restype = None
    lib.ozayn_ml_free_model.argtypes = [ctypes.c_void_p]

    lib.ozayn_ml_list_models.restype = ctypes.c_int
    lib.ozayn_ml_list_models.argtypes = [ctypes.c_char_p, ctypes.c_int]

    lib.ozayn_vision_init.restype = ctypes.c_int
    lib.ozayn_vision_init.argtypes = []

    lib.ozayn_vision_shutdown.restype = None
    lib.ozayn_vision_shutdown.argtypes = []

    lib.ozayn_vision_capture_screen.restype = ctypes.c_int
    lib.ozayn_vision_capture_screen.argtypes = [ctypes.c_char_p]

    lib.ozayn_vision_detect_faces.restype = ctypes.c_int
    lib.ozayn_vision_detect_faces.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_int)]

    lib.ozayn_vision_analyze_image.restype = ctypes.c_int
    lib.ozayn_vision_analyze_image.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]

    lib.ozayn_vision_detect_gesture.restype = ctypes.c_int
    lib.ozayn_vision_detect_gesture.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_int]

    lib.ozayn_sys_get_cpu_usage.restype = ctypes.c_int
    lib.ozayn_sys_get_cpu_usage.argtypes = []

    lib.ozayn_sys_get_memory_usage.restype = ctypes.c_int
    lib.ozayn_sys_get_memory_usage.argtypes = [
        ctypes.POINTER(ctypes.c_long), ctypes.POINTER(ctypes.c_long)
    ]

    lib.ozayn_sys_get_disk_usage.restype = ctypes.c_int
    lib.ozayn_sys_get_disk_usage.argtypes = [
        ctypes.c_char_p, ctypes.POINTER(ctypes.c_long), ctypes.POINTER(ctypes.c_long)
    ]

    lib.ozayn_sys_list_processes.restype = ctypes.c_int
    lib.ozayn_sys_list_processes.argtypes = [
        ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_int
    ]

    lib.ozayn_sys_run_command.restype = ctypes.c_int
    lib.ozayn_sys_run_command.argtypes = [
        ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_int
    ]

    lib.ozayn_sys_get_uptime.restype = ctypes.c_long
    lib.ozayn_sys_get_uptime.argtypes = []

    lib.ozayn_sys_get_hostname.restype = ctypes.c_int
    lib.ozayn_sys_get_hostname.argtypes = [ctypes.c_char_p, ctypes.c_int]

    lib.ozayn_sys_list_files.restype = ctypes.c_int
    lib.ozayn_sys_list_files.argtypes = [
        ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_int
    ]

    lib.ozayn_crypto_hash.restype = ctypes.c_int
    lib.ozayn_crypto_hash.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]

    lib.ozayn_crypto_random.restype = ctypes.c_int
    lib.ozayn_crypto_random.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_int]

    lib.ozayn_util_timestamp.restype = ctypes.c_int
    lib.ozayn_util_timestamp.argtypes = [ctypes.c_char_p, ctypes.c_int]

    lib.ozayn_util_system_info.restype = ctypes.c_int
    lib.ozayn_util_system_info.argtypes = [ctypes.c_char_p, ctypes.c_int]


# ==================== Python API ====================


def version():
    return _load().ozayn_version().decode()


def get_cpu_usage():
    return _load().ozayn_sys_get_cpu_usage()


def get_memory_usage():
    used = ctypes.c_long()
    total = ctypes.c_long()
    _load().ozayn_sys_get_memory_usage(ctypes.byref(used), ctypes.byref(total))
    return {"used": used.value, "total": total.value}


def get_disk_usage(path="/"):
    used = ctypes.c_long()
    total = ctypes.c_long()
    _load().ozayn_sys_get_disk_usage(path.encode(), ctypes.byref(used), ctypes.byref(total))
    return {"used": used.value, "total": total.value}


def list_processes(sort_by="cpu", limit=20):
    buf = ctypes.create_string_buffer(65536)
    _load().ozayn_sys_list_processes(sort_by.encode(), limit, buf, 65536)
    return json.loads(buf.value.decode())


def run_command(cmd, timeout=30):
    buf = ctypes.create_string_buffer(65536)
    exit_code = _load().ozayn_sys_run_command(cmd.encode(), timeout, buf, 65536)
    return {"exit_code": exit_code, "output": buf.value.decode()}


def get_uptime():
    return _load().ozayn_sys_get_uptime()


def get_hostname():
    buf = ctypes.create_string_buffer(256)
    _load().ozayn_sys_get_hostname(buf, 256)
    return buf.value.decode()


def list_files(path=".", show_hidden=False):
    buf = ctypes.create_string_buffer(131072)
    _load().ozayn_sys_list_files(path.encode(), 1 if show_hidden else 0, buf, 131072)
    return json.loads(buf.value.decode())


def system_info():
    buf = ctypes.create_string_buffer(4096)
    _load().ozayn_util_system_info(buf, 4096)
    return json.loads(buf.value.decode())


def timestamp():
    buf = ctypes.create_string_buffer(64)
    _load().ozayn_util_timestamp(buf, 64)
    return buf.value.decode()


def crypto_hash(data):
    buf = ctypes.create_string_buffer(128)
    _load().ozayn_crypto_hash(data.encode(), buf, 128)
    return buf.value.decode()


def crypto_random(length=32):
    buf = ctypes.create_string_buffer(length * 2 + 1)
    _load().ozayn_crypto_random(length, buf, length * 2 + 1)
    return buf.value.decode()


def capture_screen(output_path):
    return _load().ozayn_vision_capture_screen(output_path.encode()) == 0


def detect_faces(image_path):
    count = ctypes.c_int(0)
    _load().ozayn_vision_detect_faces(image_path.encode(), ctypes.byref(count))
    return count.value


def analyze_image(image_path):
    buf = ctypes.create_string_buffer(8192)
    _load().ozayn_vision_analyze_image(image_path.encode(), buf, 8192)
    return json.loads(buf.value.decode())


def detect_gesture(camera_id=0):
    buf = ctypes.create_string_buffer(1024)
    _load().ozayn_vision_detect_gesture(camera_id, buf, 1024)
    return json.loads(buf.value.decode())


def vision_init():
    return _load().ozayn_vision_init() == 0


def vision_shutdown():
    _load().ozayn_vision_shutdown()


def ml_load_model(path):
    handle = _load().ozayn_ml_load_model(path.encode())
    return handle


def ml_free_model(handle):
    _load().ozayn_ml_free_model(handle)


def ml_list_models():
    buf = ctypes.create_string_buffer(8192)
    _load().ozayn_ml_list_models(buf, 8192)
    return json.loads(buf.value.decode())
