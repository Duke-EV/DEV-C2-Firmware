import tkinter as tk
from tkinter import ttk, font
import serial
import serial.tools.list_ports
import threading
import time
import re

# ── Field definitions ────────────────────────────────────────────────────────
FIELDS = [
    ("Windshield",          "m_peripherals_windshield",          "Peripherals"),
    ("Back Running Lights", "m_peripherals_backrunninglights",   "Peripherals"),
    ("Turn Signal",         "m_peripherals_turn",                "Peripherals"),
    ("Headlights",          "m_peripherals_headlights",          "Peripherals"),
    ("Brake Lights",        "m_peripherals_brakelights",         "Peripherals"),
    ("Hazard",              "m_peripherals_hazard",              "Peripherals"),
    ("PDB Current",         "m_pdb_current",                     "PDB"),
    ("PDB Voltage",         "m_pdb_voltage",                     "PDB"),
    ("Motor RPM",           "m_motor_rpm",                       "Motor"),
    ("Throttle Raw",        "m_throttle_raw",                    "Motor"),
    ("Throttle Average",    "m_throttle_average",                "Motor"),
    ("Joulemeter Current",  "m_joulemeter_current",              "Joulemeter"),
    ("Joulemeter Voltage",  "m_joulemeter_voltage",              "Joulemeter"),
    ("Joulemeter Energy",   "m_joulemeter_energy",               "Joulemeter"),
]

BAUD_RATE = 115200

# ── Colour palette (Duke) ─────────────────────────────────────────────────────
BG        = "#f0f4f8"   # soft off-white base
PANEL     = "#ffffff"   # pure white cards
BORDER    = "#c8d6e5"   # cool grey outlines
ACCENT    = "#A3A3A3"   # Duke royal blue (pops on white)
ACCENT2   = "#A3A3A3"   # same for hz label
TEXT      = "#0d1b2a"   # near-black text
MUTED     = "#6b7f99"   # medium grey labels
GREEN     = "#0077cc"   # blue instead of cyan (readable on light)
RED       = "#cc2200"   # darker red for contrast
HEADER_BG = "#002470"   # deep Duke navy header

CATEGORY_COLORS = {
    "Peripherals": "#003087",   # Duke royal blue
    "Power":       "#0055bb",
    "Motor":       "#0077cc",
    "Joulemeter":  "#2299dd",
}

# Matches:  ID: 0x1AB  Data: AA BB CC DD EE FF 00 11
CAN_RE = re.compile(r"ID:\s*(0x[0-9A-Fa-f]+)\s+Data:\s+([0-9A-Fa-f ]+)", re.IGNORECASE)


class VehicleMonitor(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Vehicle Monitor")
        self.configure(bg=BG)
        self.resizable(True, True)
        self.geometry("820x720")
        self.minsize(700, 520)

        self._serial: serial.Serial | None = None
        self._thread: threading.Thread | None = None
        self._running = False
        self._lock = threading.Lock()

        self._values: dict[str, tk.StringVar] = {
            key: tk.StringVar(value="—") for _, key, _ in FIELDS
        }
        self._status   = tk.StringVar(value="Disconnected")
        self._port_var = tk.StringVar()
        self._hz_label = tk.StringVar(value="— Hz")
        self._last_ts  = 0.0

        # CAN history: id_str -> {frame, data, count, ts StringVars}
        self._can_rows: dict[str, dict] = {}

        self._build_ui()
        self._refresh_ports()
        self.protocol("WM_DELETE_WINDOW", self._on_close)

    # ── UI Construction ───────────────────────────────────────────────────────

    def _build_ui(self):
        # Header
        header = tk.Frame(self, bg=HEADER_BG, height=56)
        header.pack(fill="x", side="top")
        header.pack_propagate(False)

        tk.Label(header, text="◈  VEHICLE MONITOR",
                 font=("Courier New", 15, "bold"),
                 fg=ACCENT, bg=HEADER_BG).pack(side="left", padx=20, pady=12)

        self._status_dot = tk.Label(header, text="●", fg=RED, bg=HEADER_BG,
                                    font=("Courier New", 14))
        self._status_dot.pack(side="right", padx=(0, 8), pady=12)
        tk.Label(header, textvariable=self._status, fg=MUTED, bg=HEADER_BG,
                 font=("Courier New", 10)).pack(side="right", pady=12)
        tk.Label(header, textvariable=self._hz_label, fg=ACCENT2, bg=HEADER_BG,
                 font=("Courier New", 10, "bold")).pack(side="right", padx=20, pady=12)

        # Toolbar
        toolbar = tk.Frame(self, bg=PANEL, pady=10)
        toolbar.pack(fill="x")

        tk.Label(toolbar, text="PORT", fg=MUTED, bg=PANEL,
                 font=("Courier New", 9, "bold")).pack(side="left", padx=(20, 6))

        style = ttk.Style(self)
        style.theme_use("clam")
        style.configure("Dark.TCombobox",
                        fieldbackground=BG, background=BG,
                        foreground=TEXT, selectbackground=BG,
                        selectforeground=ACCENT, bordercolor=BORDER,
                        arrowcolor=ACCENT)
        style.configure("Duke.TNotebook",
                        background=BG, bordercolor=BORDER, tabmargins=0)
        style.configure("Duke.TNotebook.Tab",
                        background=PANEL, foreground=MUTED,
                        font=("Courier New", 9, "bold"),
                        padding=(14, 6), bordercolor=BORDER)
        style.map("Duke.TNotebook.Tab",
                  background=[("selected", HEADER_BG)],
                  foreground=[("selected", ACCENT)])

        self._port_combo = ttk.Combobox(toolbar, textvariable=self._port_var,
                                        width=22, style="Dark.TCombobox",
                                        state="readonly")
        self._port_combo.pack(side="left", padx=4)

        btn_cfg = dict(bg=PANEL, fg=MUTED, activebackground=BORDER,
                       activeforeground=TEXT, relief="flat", bd=0,
                       font=("Courier New", 10), cursor="hand2", pady=4, padx=8)
        tk.Button(toolbar, text="↻", command=self._refresh_ports,
                  **btn_cfg).pack(side="left", padx=2)

        self._connect_btn = tk.Button(toolbar, text="CONNECT",
                                      command=self._toggle_connect,
                                      bg=ACCENT, fg=BG, activebackground="#2a6aaa",
                                      activeforeground=BG, relief="flat", bd=0,
                                      font=("Courier New", 10, "bold"),
                                      cursor="hand2", pady=4, padx=14)
        self._connect_btn.pack(side="left", padx=(8, 0))

        tk.Button(toolbar, text="CLEAR CAN", command=self._clear_can,
                  bg=PANEL, fg=MUTED, activebackground=BORDER, activeforeground=TEXT,
                  relief="flat", bd=0, font=("Courier New", 9, "bold"),
                  cursor="hand2", pady=4, padx=10).pack(side="right", padx=16)

        tk.Frame(self, bg=BORDER, height=1).pack(fill="x")

        # Tabs
        nb = ttk.Notebook(self, style="Duke.TNotebook")
        nb.pack(fill="both", expand=True)

        tab_tel = tk.Frame(nb, bg=BG)
        tab_can = tk.Frame(nb, bg=BG)
        nb.add(tab_tel, text="  TELEMETRY  ")
        nb.add(tab_can, text="  CAN BUS    ")

        self._build_telemetry_tab(tab_tel)
        self._build_can_tab(tab_can)

        # Footer
        tk.Frame(self, bg=BORDER, height=1).pack(fill="x")
        footer = tk.Frame(self, bg=HEADER_BG, height=28)
        footer.pack(fill="x", side="bottom")
        footer.pack_propagate(False)
        tk.Label(footer, text=f"BAUD {BAUD_RATE}",
                 fg=MUTED, bg=HEADER_BG,
                 font=("Courier New", 8)).pack(side="left", padx=16, pady=6)

    def _build_telemetry_tab(self, parent):
        container = tk.Frame(parent, bg=BG)
        container.pack(fill="both", expand=True, padx=20, pady=16)

        col_font   = ("Courier New", 9,  "bold")
        label_font = ("Courier New", 10)
        val_font   = ("Courier New", 11, "bold")

        hdr = tk.Frame(container, bg=BG)
        hdr.pack(fill="x", pady=(0, 6))
        tk.Label(hdr, text="CATEGORY", width=14, anchor="w", fg=MUTED, bg=BG, font=col_font).pack(side="left")
        tk.Label(hdr, text="FIELD",    width=26, anchor="w", fg=MUTED, bg=BG, font=col_font).pack(side="left")
        tk.Label(hdr, text="VALUE",              anchor="e", fg=MUTED, bg=BG, font=col_font).pack(side="right", padx=4)
        tk.Frame(container, bg=BORDER, height=1).pack(fill="x", pady=(0, 8))

        prev_cat = None
        for label, key, cat in FIELDS:
            cat_color = CATEGORY_COLORS.get(cat, MUTED)
            if cat != prev_cat:
                tk.Frame(container, bg=BG, height=6).pack(fill="x")
                prev_cat = cat

            row = tk.Frame(container, bg=PANEL, pady=7, padx=12,
                           highlightthickness=1, highlightbackground=BORDER)
            row.pack(fill="x", pady=2)

            pill = tk.Frame(row, bg=cat_color, width=4)
            pill.pack(side="left", fill="y", padx=(0, 10))
            pill.pack_propagate(False)

            tk.Label(row, text=cat.upper(), width=13, anchor="w",
                     fg=cat_color, bg=PANEL,
                     font=("Courier New", 8, "bold")).pack(side="left")
            tk.Label(row, text=label, width=24, anchor="w",
                     fg=TEXT, bg=PANEL, font=label_font).pack(side="left")
            tk.Label(row, textvariable=self._values[key],
                     anchor="e", fg=ACCENT, bg=PANEL,
                     font=val_font).pack(side="right", padx=4)

    def _build_can_tab(self, parent):
        container = tk.Frame(parent, bg=BG)
        container.pack(fill="both", expand=True, padx=20, pady=16)

        col_font = ("Courier New", 9, "bold")

        hdr = tk.Frame(container, bg=BG)
        hdr.pack(fill="x", pady=(0, 6))
        tk.Label(hdr, text="CAN ID",    width=10, anchor="w", fg=MUTED, bg=BG, font=col_font).pack(side="left")
        tk.Label(hdr, text="DATA (HEX)",          anchor="w", fg=MUTED, bg=BG, font=col_font).pack(side="left", padx=(8, 0))
        tk.Label(hdr, text="COUNT",     width=7,  anchor="e", fg=MUTED, bg=BG, font=col_font).pack(side="right", padx=(0, 4))
        tk.Label(hdr, text="LAST SEEN", width=12, anchor="e", fg=MUTED, bg=BG, font=col_font).pack(side="right", padx=(0, 12))
        tk.Frame(container, bg=BORDER, height=1).pack(fill="x", pady=(0, 8))

        # Scrollable rows
        wrap = tk.Frame(container, bg=BG)
        wrap.pack(fill="both", expand=True)

        self._can_canvas = tk.Canvas(wrap, bg=BG, bd=0, highlightthickness=0)
        sb = ttk.Scrollbar(wrap, orient="vertical", command=self._can_canvas.yview)
        self._can_inner = tk.Frame(self._can_canvas, bg=BG)

        self._can_inner.bind("<Configure>", lambda e: self._can_canvas.configure(
            scrollregion=self._can_canvas.bbox("all")))
        self._can_canvas.create_window((0, 0), window=self._can_inner, anchor="nw")
        self._can_canvas.configure(yscrollcommand=sb.set)

        self._can_canvas.pack(side="left", fill="both", expand=True)
        sb.pack(side="right", fill="y")

        self._can_canvas.bind_all("<MouseWheel>", lambda e: self._can_canvas.yview_scroll(
            int(-1 * (e.delta / 120)), "units"))

    # ── CAN row management ────────────────────────────────────────────────────

    def _upsert_can_row(self, id_str: str, data_str: str):
        ts = time.strftime("%H:%M:%S")

        if id_str in self._can_rows:
            r = self._can_rows[id_str]
            r["data"].set(data_str)
            r["count"].set(str(int(r["count"].get()) + 1))
            r["ts"].set(ts)
            # brief highlight flash
            r["frame"].config(highlightbackground=ACCENT)
            self.after(150, lambda f=r["frame"]: f.config(highlightbackground=BORDER))
        else:
            data_var  = tk.StringVar(value=data_str)
            count_var = tk.StringVar(value="1")
            ts_var    = tk.StringVar(value=ts)

            row = tk.Frame(self._can_inner, bg=PANEL, pady=6, padx=12,
                           highlightthickness=1, highlightbackground=ACCENT)
            row.pack(fill="x", pady=2, padx=2)

            pill = tk.Frame(row, bg=ACCENT, width=4)
            pill.pack(side="left", fill="y", padx=(0, 10))
            pill.pack_propagate(False)

            tk.Label(row, text=id_str, width=10, anchor="w",
                     fg=ACCENT, bg=PANEL,
                     font=("Courier New", 10, "bold")).pack(side="left")
            tk.Label(row, textvariable=data_var, anchor="w",
                     fg=TEXT, bg=PANEL,
                     font=("Courier New", 10)).pack(side="left", padx=(8, 0), fill="x", expand=True)
            tk.Label(row, textvariable=ts_var, width=10, anchor="e",
                     fg=MUTED, bg=PANEL,
                     font=("Courier New", 9)).pack(side="right", padx=(0, 8))
            tk.Label(row, textvariable=count_var, width=6, anchor="e",
                     fg=ACCENT2, bg=PANEL,
                     font=("Courier New", 9, "bold")).pack(side="right")

            self._can_rows[id_str] = {
                "frame": row, "data": data_var, "count": count_var, "ts": ts_var,
            }
            # fade pill after initial flash
            self.after(400, lambda: pill.config(bg=MUTED))

    def _clear_can(self):
        for r in self._can_rows.values():
            r["frame"].destroy()
        self._can_rows.clear()

    # ── Port helpers ──────────────────────────────────────────────────────────

    def _refresh_ports(self):
        ports = [p.device for p in serial.tools.list_ports.comports()]
        self._port_combo["values"] = ports
        if ports and not self._port_var.get():
            self._port_var.set(ports[0])

    def _toggle_connect(self):
        if self._running:
            self._disconnect()
        else:
            self._connect()

    def _connect(self):
        port = self._port_var.get()
        if not port:
            return
        try:
            self._serial = serial.Serial(port, BAUD_RATE, timeout=1)
            self._running = True
            self._thread = threading.Thread(target=self._read_loop, daemon=True)
            self._thread.start()
            self._status.set(f"Connected  {port}")
            self._status_dot.config(fg=GREEN)
            self._connect_btn.config(text="DISCONNECT", bg=RED,
                                     activebackground="#cc1133")
        except serial.SerialException as e:
            self._status.set(f"Error: {e}")

    def _disconnect(self):
        self._running = False
        if self._serial:
            self._serial.close()
            self._serial = None
        self._status.set("Disconnected")
        self._status_dot.config(fg=RED)
        self._connect_btn.config(text="CONNECT", bg=ACCENT,
                                 activebackground="#2a6aaa")
        self._hz_label.set("— Hz")
        for var in self._values.values():
            var.set("—")

    # ── Serial read thread ────────────────────────────────────────────────────

    def _read_loop(self):
        while self._running:
            try:
                line = self._serial.readline().decode("utf-8", errors="ignore").strip()
                if not line:
                    continue

                # CAN message?
                m = CAN_RE.search(line)
                if m:
                    id_str   = m.group(1).upper()
                    data_str = m.group(2).strip().upper()
                    self.after(0, self._upsert_can_row, id_str, data_str)
                    continue

                # Telemetry (comma-separated)?
                parts = line.split(",")
                if len(parts) == len(FIELDS):
                    now = time.time()
                    dt  = now - self._last_ts if self._last_ts else None
                    self._last_ts = now
                    with self._lock:
                        for i, (_, key, _) in enumerate(FIELDS):
                            self._values[key].set(parts[i])
                        if dt and dt > 0:
                            self._hz_label.set(f"{1/dt:.1f} Hz")

            except (serial.SerialException, OSError):
                self._running = False
                self.after(0, self._disconnect)
                break

    def _on_close(self):
        self._disconnect()
        self.destroy()


if __name__ == "__main__":
    app = VehicleMonitor()
    app.mainloop()