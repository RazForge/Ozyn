"""
Ozayn Voice Command Engine — Continuous voice recognition + command execution.
Uses SpeechRecognition with Google Web API for speech-to-text.
"""

import os
import sys
import threading

os.environ['PYGAME_HIDE_SUPPORT_PROMPT'] = '1'
os.environ['SDL_AUDIODRIVER'] = 'dummy'


class VoiceCommandEngine:
    """Continuous voice recognition that maps spoken words to commands.
    Supports English and Amharic (am-ET) with bilingual matching.
    """

    def __init__(self):
        self._active = False
        self._thread = None
        self._recognizer = None
        self._mic = None
        self._callback = None
        self._status_callback = None
        self._command_map = {
            # Navigation
            "login": "LOGIN", "sign in": "LOGIN", "enter": "LOGIN",
            "go home": "HOME", "home": "HOME",
            "back": "BACK", "go back": "BACK",
            "next": "NEXT", "forward": "NEXT",

            # Actions
            "submit": "SUBMIT", "confirm": "CONFIRM", "ok": "CONFIRM",
            "yes": "CONFIRM", "cancel": "CANCEL", "no": "CANCEL",
            "close": "CLOSE", "exit": "CLOSE",

            # UI
            "open keyboard": "KEYBOARD", "keyboard": "KEYBOARD",
            "hide keyboard": "HIDE_KEYBOARD",
            "open settings": "SETTINGS", "settings": "SETTINGS",

            # Voice
            "stop listening": "VOICE_STOP", "voice off": "VOICE_STOP",
            "start listening": "VOICE_START", "voice on": "VOICE_START",

            # Gesture mode
            "gesture mode": "GESTURE_MODE", "hand control": "GESTURE_MODE",
            "fine mode": "MODE_FINE", "precision mode": "MODE_FINE",
            "normal mode": "MODE_NORMAL",
            "fast mode": "MODE_FAST", "turbo mode": "MODE_TURBO",

            # System
            "lock": "LOCK", "unlock": "UNLOCK",
            "scroll up": "SCROLL_UP", "scroll down": "SCROLL_DOWN",
            "click": "CLICK", "right click": "RIGHT_CLICK",

            # Pages
            "open chat": "PAGE_CHAT", "chat": "PAGE_CHAT",
            "open projects": "PAGE_PROJECTS", "projects": "PAGE_PROJECTS",
            "open tasks": "PAGE_TASKS", "tasks": "PAGE_TASKS",
            "open knowledge": "PAGE_KNOWLEDGE", "knowledge": "PAGE_KNOWLEDGE",
            "open dashboard": "PAGE_DASHBOARD", "dashboard": "PAGE_DASHBOARD",
            "open system": "PAGE_SYSTEM", "system": "PAGE_SYSTEM",

            # ── Amharic commands (Ge'ez script) ──
            # Navigation
            "ክፈት": "LOGIN",           # kifet - open/login
            "መግባት": "LOGIN",          # megibat - login
            "ዋና ገጽ": "HOME",          # wana gets - home page
            "መነሻ": "HOME",            # menesha - home
            "ወደ ኋላ": "BACK",          # wede hula - back
            "ወደጀርባ": "BACK",          # wede jerba - back
            "ቀጥል": "NEXT",            # ketil - next
            "ቀጥollo": "NEXT",          # ketillo - continue

            # Actions
            "አዎ": "CONFIRM",           # aw - yes
            "አይደለም": "CANCEL",        # aydelem - no
            "ተወ": "CANCEL",            # tewew - cancel/close
            "ዝጋ": "CLOSE",            # ziga - close
            " ጠቅ": "CLICK",           # tek - click
            "አ簨ቅ": "CLICK",           # click variant

            # UI
            "መሳፍያ": "KEYBOARD",       # mesafiya - keyboard
            "ቅንብር": "SETTINGS",       # kinibir - settings
            "ማስቀመጥ": "SAVE",          # masquemet - save

            # Voice
            "ቃል አድምጥ": "VOICE_START",   # kal adimt - listen
            "ቃል አስተምጥ": "VOICE_STOP",    # kal astemt - stop listening

            # Gesture
            "የእጅ ምልክት": "GESTURE_MODE",  # yeij milit - hand gesture
            "ንክል": "MODE_FINE",        # nikl - fine/precise
            "መደበኛ": "MODE_NORMAL",    # medebegna - normal
            "ፈጣን": "MODE_FAST",        # fetan - fast

            # System
            ".lock": "LOCK",            # lock
            " ክፈት": "UNLOCK",          # unlock
            "ወደ ላይ": "SCROLL_UP",      # wede lay - scroll up
            "ወደ ታች": "SCROLL_DOWN",    # wede tach - scroll down

            # Pages
            "ውይይት": "PAGE_CHAT",       # wiyit - chat
            "projprojects": "PAGE_PROJECTS",
            "ስራ": "PAGE_TASKS",        # sira - tasks/works
            "መረጃ": "PAGE_KNOWLEDGE",  # mereja - knowledge/info
            "ዳሽቦርድ": "PAGE_DASHBOARD",  # dashboard transliteration
            "ስርዓት": "PAGE_SYSTEM",     # sirat - system
        }

    @property
    def is_active(self):
        return self._active

    def start(self, command_callback, status_callback=None):
        if self._active:
            return

        self._callback = command_callback
        self._status_callback = status_callback

        try:
            old_stderr = sys.stderr
            sys.stderr = open(os.devnull, 'w')
            try:
                import speech_recognition as sr
            finally:
                sys.stderr.close()
                sys.stderr = old_stderr

            self._recognizer = sr.Recognizer()
            self._recognizer.energy_threshold = 200
            self._recognizer.dynamic_energy_threshold = True
            self._recognizer.pause_threshold = 0.6

            try:
                self._mic = sr.Microphone(device_index=None)
            except (AttributeError, OSError):
                if self._status_callback:
                    self._status_callback("NO_MIC")
                return

            # Quick calibration — 0.3s only, cap at 1000
            try:
                with self._mic as source:
                    self._recognizer.adjust_for_ambient_noise(source, duration=0.3)
                    if self._recognizer.energy_threshold > 1000:
                        self._recognizer.energy_threshold = 500
            except Exception:
                self._recognizer.energy_threshold = 500

            self._active = True
            if self._status_callback:
                self._status_callback("ACTIVE")

            self._thread = threading.Thread(target=self._listen_loop, daemon=True)
            self._thread.start()

        except ImportError:
            if self._status_callback:
                self._status_callback("UNAVAILABLE")
        except Exception:
            if self._status_callback:
                self._status_callback("ERROR")

    def stop(self):
        self._active = False
        if self._status_callback:
            self._status_callback("STOPPED")

    def _listen_loop(self):
        import speech_recognition as sr

        while self._active:
            try:
                with self._mic as source:
                    audio = self._recognizer.listen(source, timeout=5, phrase_time_limit=5)

                if not self._active:
                    break

                # Try English first, then Amharic
                text = None
                for lang in ['en-US', 'am-ET']:
                    try:
                        text = self._recognizer.recognize_google(audio, language=lang).lower().strip()
                        if text:
                            break
                    except sr.RequestError:
                        continue
                    except sr.UnknownValueError:
                        continue

                if text:
                    matched = self._match_command(text)
                    if matched and self._callback:
                        self._callback(matched, text)

            except sr.WaitTimeoutError:
                continue
            except sr.UnknownValueError:
                continue
            except Exception:
                continue

    def _match_command(self, text):
        if text in self._command_map:
            return self._command_map[text]

        for phrase, command in self._command_map.items():
            if phrase in text:
                return command

        if "username" in text or "user" in text:
            words = text.split()
            for i, w in enumerate(words):
                if w in ["username", "user", "name"] and i + 1 < len(words):
                    return f"USERNAME:{words[i + 1]}"

        if "password" in text or "pass" in text:
            words = text.split()
            for i, w in enumerate(words):
                if w in ["password", "pass"] and i + 1 < len(words):
                    return f"PASSWORD:{words[i + 1]}"

        return None

    def add_command(self, phrase, command):
        self._command_map[phrase.lower()] = command

    def remove_command(self, phrase):
        self._command_map.pop(phrase.lower(), None)


def fix_mic_volume():
    """Fix mic volume if it's clipping (PulseAudio)."""
    try:
        import subprocess
        result = subprocess.run(
            ['pactl', 'get-source-volume', '@DEFAULT_SOURCE@'],
            capture_output=True, text=True, timeout=3
        )
        if result.returncode == 0:
            line = result.stdout.strip()
            # Extract percentage
            import re
            match = re.search(r'(\d+)%', line)
            if match:
                pct = int(match.group(1))
                if pct > 60:
                    subprocess.run(
                        ['pactl', 'set-source-volume', '@DEFAULT_SOURCE@', '30%'],
                        timeout=3
                    )
                    return True  # Was fixed
    except Exception:
        pass
    return False
