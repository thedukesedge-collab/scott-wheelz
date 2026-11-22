# SPDX-License-Identifier: GPL-3.0-or-later
# Part of Refloat Glow, 2026. See FORK_NOTES.md.
"""
LCM Flasher - pick a .hex file and flash it to the LCM over ST-Link.

Runs the same steps as flash.bat, with the chip pack passed directly:
    pyocd erase -c -t hk32f030mf4p6 --pack <HKMicroChip...pack>
    pyocd load <file> -t hk32f030mf4p6 --pack <HKMicroChip...pack>

The .pack is looked for next to this program first, then in the folder of
the chosen .hex. Easiest: keep this file in the folder with flash.bat.
"""
import os
import re
import sys
import traceback
import json
import queue
import shutil
import threading
import subprocess
import tkinter as tk
from tkinter import filedialog, messagebox, scrolledtext

# Built as LCM_Flasher.exe (PyInstaller): pyocd and the chip pack are inside
# the exe and pyocd runs in this process; no Python install needed.
FROZEN = getattr(sys, "frozen", False)
if FROZEN:
    APP_DIR = os.path.dirname(os.path.abspath(sys.executable))
    BUNDLE_DIR = getattr(sys, "_MEIPASS", APP_DIR)
else:
    APP_DIR = os.path.dirname(os.path.abspath(__file__))
    BUNDLE_DIR = APP_DIR
ANSI = re.compile(r"\x1b\[[0-9;]*m")
TARGET = "hk32f030mf4p6"
SETTINGS = os.path.join(APP_DIR, "lcm_flasher_settings.json")
NO_WINDOW = 0x08000000 if os.name == "nt" else 0  # hide console popups


def find_pyocd():
    """Return the command prefix used to run pyocd."""
    if FROZEN:
        return ["pyocd"]  # placeholder: runs in-process, see App.run
    exe = shutil.which("pyocd")
    if exe:
        return [exe]
    py = sys.executable
    folder, name = os.path.split(py)
    if name.lower() == "pythonw.exe":
        alt = os.path.join(folder, "python.exe")
        if os.path.exists(alt):
            py = alt
    scripts = os.path.join(os.path.dirname(py), "Scripts", "pyocd.exe")
    if os.path.exists(scripts):
        return [scripts]
    return [py, "-m", "pyocd"]


class QueueWriter(object):
    """stdout/stderr replacement that sends text to the log window."""

    def __init__(self):
        self.q = None
        self.file = None

    def write(self, text):
        text = ANSI.sub("", text)
        if self.file is not None:
            self.file.write(text)
        elif self.q is not None and text:
            self.q.put(("log", text))

    def flush(self):
        if self.file is not None:
            self.file.flush()

    def isatty(self):
        return False


OUT = QueueWriter()
if FROZEN or sys.stdout is None:
    sys.stdout = sys.stderr = OUT


def run_pyocd_here(args, cwd):
    """Run pyocd inside this process; returns its exit code."""
    old = os.getcwd()
    try:
        os.chdir(cwd)
        from pyocd.__main__ import PyOCDTool
        rc = PyOCDTool().run(args)
    except SystemExit as e:
        rc = e.code if isinstance(e.code, int) else 1
    except Exception:
        OUT.write(traceback.format_exc())
        rc = 1
    finally:
        os.chdir(old)
    return rc or 0


def find_pack(folders):
    """Return (folder, pack path) for the first folder holding an HK32 .pack."""
    for d in folders:
        if not d or not os.path.isdir(d):
            continue
        for f in sorted(os.listdir(d)):
            if f.lower().endswith(".pack") and "hk32" in f.lower():
                return d, os.path.join(d, f)
    return None, None


def load_settings():
    try:
        with open(SETTINGS, "r") as f:
            return json.load(f)
    except Exception:
        return {}


def save_settings(data):
    try:
        with open(SETTINGS, "w") as f:
            json.dump(data, f)
    except Exception:
        pass


class App:
    def __init__(self, root):
        self.root = root
        self.settings = load_settings()
        self.hex_path = None
        self.busy = False
        self.work_dir = APP_DIR
        self.q = queue.Queue()
        OUT.q = self.q

        root.title("LCM Flasher")
        root.geometry("640x430")
        root.minsize(480, 320)

        top = tk.Frame(root, padx=10, pady=10)
        top.pack(fill="x")

        self.choose_btn = tk.Button(top, text="Choose File...", width=14,
                                    command=self.choose_file)
        self.choose_btn.pack(side="left")

        self.file_var = tk.StringVar(value="No file selected")
        tk.Label(top, textvariable=self.file_var, anchor="w",
                 fg="#444").pack(side="left", fill="x", expand=True, padx=10)

        mid = tk.Frame(root, padx=10)
        mid.pack(fill="x")
        self.flash_btn = tk.Button(mid, text="FLASH LCM", height=2,
                                   font=("Segoe UI", 12, "bold"),
                                   bg="#2d7d32", fg="white",
                                   activebackground="#256628",
                                   state="disabled", command=self.start_flash)
        self.flash_btn.pack(fill="x")

        self.status_var = tk.StringVar(value="Choose a .hex file to begin.")
        self.status = tk.Label(root, textvariable=self.status_var,
                               font=("Segoe UI", 10, "bold"), pady=6)
        self.status.pack(fill="x")

        self.log = scrolledtext.ScrolledText(root, height=14,
                                             font=("Consolas", 9),
                                             state="disabled")
        self.log.pack(fill="both", expand=True, padx=10, pady=(0, 10))

        # Allow a file to be passed on the command line (drag onto the .pyw)
        if len(sys.argv) > 1 and os.path.isfile(sys.argv[1]):
            self.set_file(sys.argv[1])

        self.check_setup()
        self.root.after(100, self.poll)

    # ---------- UI helpers ----------
    def write(self, text):
        self.log.configure(state="normal")
        self.log.insert("end", text)
        self.log.see("end")
        self.log.configure(state="disabled")

    def set_status(self, text, color="black"):
        self.status_var.set(text)
        self.status.configure(fg=color)

    def set_file(self, path):
        self.hex_path = os.path.abspath(path)
        self.file_var.set(self.hex_path)
        self.flash_btn.configure(state="normal")
        self.set_status("Ready to flash: " + os.path.basename(path))
        self.settings["last_dir"] = os.path.dirname(self.hex_path)
        save_settings(self.settings)

    def check_setup(self):
        self.write("Program folder: " + APP_DIR + "\n")
        if find_pack([BUNDLE_DIR, APP_DIR])[1] is None:
            self.write("No HK32 .pack next to this program. It will also look "
                       "in the folder of the .hex you choose.\n")

    # ---------- actions ----------
    def choose_file(self):
        start = self.settings.get("last_dir") or os.path.join(
            os.path.expanduser("~"), "Downloads")
        path = filedialog.askopenfilename(
            title="Choose LCM firmware",
            initialdir=start if os.path.isdir(start) else APP_DIR,
            filetypes=[("Intel HEX firmware", "*.hex *.HEX"),
                       ("All files", "*.*")])
        if path:
            self.set_file(path)

    def start_flash(self):
        if self.busy or not self.hex_path:
            return
        if not os.path.isfile(self.hex_path):
            messagebox.showerror("LCM Flasher", "File not found:\n" + self.hex_path)
            return
        self.busy = True
        self.choose_btn.configure(state="disabled")
        self.flash_btn.configure(state="disabled")
        self.set_status("Flashing... keep the ST-Link connected.", "#b26a00")
        self.write("\n=== Flashing " + os.path.basename(self.hex_path) + " ===\n")
        threading.Thread(target=self.flash_worker, daemon=True).start()

    def run(self, args):
        """Run a command, stream its output to the log, return exit code."""
        self.q.put(("log", "> " + " ".join(args) + "\n"))
        if FROZEN:
            return run_pyocd_here(args[1:], self.work_dir)
        try:
            proc = subprocess.Popen(args, cwd=self.work_dir,
                                    stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT,
                                    stdin=subprocess.DEVNULL,
                                    universal_newlines=True,
                                    creationflags=NO_WINDOW)
        except OSError as e:
            self.q.put(("log", "Could not start pyocd: %s\n" % e))
            return -1
        for line in proc.stdout:
            self.q.put(("log", line))
        return proc.wait()

    def flash_worker(self):
        pyocd = find_pyocd()
        folder, pack = find_pack([BUNDLE_DIR, APP_DIR, os.path.dirname(self.hex_path)])
        if pack is None:
            self.q.put(("log", "Can't find the HKMicroChip .pack file. Put "
                        "LCM_Flasher.pyw in the folder with the .pack "
                        "(where flash.bat is).\n"))
            self.q.put(("done", -1))
            return
        self.work_dir = folder
        self.q.put(("log", "Chip pack: " + pack + "\n"))
        # Pass the pack directly (all pyocd.yaml does is name it), so this
        # works whatever folder pyocd starts in. Not combined with
        # --config, which would load the same pack a second time.
        common = ["-t", TARGET, "--pack", pack]
        rc = self.run(pyocd + ["erase", "-c"] + common)
        if rc == 0:
            rc = self.run(pyocd + ["load", self.hex_path, "--format", "hex"]
                          + common)
        self.q.put(("done", rc))

    def poll(self):
        try:
            while True:
                kind, val = self.q.get_nowait()
                if kind == "log":
                    self.write(val)
                elif kind == "done":
                    self.busy = False
                    self.choose_btn.configure(state="normal")
                    self.flash_btn.configure(state="normal")
                    if val == 0:
                        self.set_status("DONE - LCM flashed successfully.",
                                        "#2d7d32")
                        self.write("=== SUCCESS ===\n")
                    else:
                        self.set_status("FLASH FAILED - check ST-Link "
                                        "connection/driver and try again.",
                                        "#c62828")
                        self.write("=== FAILED (code %s) ===\n" % val)
        except queue.Empty:
            pass
        self.root.after(100, self.poll)


def selftest(out_path):
    """CI check for the exe: is the chip target known with the built-in pack?"""
    with open(out_path, "w") as f:
        OUT.file = f
        folder, pack = find_pack([BUNDLE_DIR, APP_DIR])
        print("pack: %s" % pack)
        rc = run_pyocd_here(["list", "--targets", "--pack", pack], folder or APP_DIR)
        print("exit: %s" % rc)
    return rc


if __name__ == "__main__":
    if len(sys.argv) > 2 and sys.argv[1] == "--selftest":
        sys.exit(selftest(sys.argv[2]))
    root = tk.Tk()
    App(root)
    root.mainloop()
