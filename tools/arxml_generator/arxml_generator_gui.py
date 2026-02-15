#!/usr/bin/env python3
"""Simple Tkinter GUI for generate_arxml.py."""

from __future__ import annotations

from pathlib import Path
import tkinter as tk
from tkinter import filedialog, messagebox, ttk
from typing import List

from generate_arxml import ConfigError, GenerationContext, generate_arxml_from_files


class ArxmlGeneratorGui(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        self.title("ARXML Generator (YAML -> ARXML)")
        self.geometry("920x560")

        self.input_files: List[Path] = []
        self.output_var = tk.StringVar()
        self.indent_var = tk.IntVar(value=4)
        self.strict_var = tk.BooleanVar(value=False)
        self.allow_extensions_var = tk.BooleanVar(value=False)
        self.validate_only_var = tk.BooleanVar(value=False)

        self._build_widgets()

    def _build_widgets(self) -> None:
        root = ttk.Frame(self, padding=12)
        root.pack(fill=tk.BOTH, expand=True)

        inputs_frame = ttk.LabelFrame(root, text="Input YAML files", padding=8)
        inputs_frame.pack(fill=tk.BOTH, expand=True)

        self.listbox = tk.Listbox(inputs_frame, selectmode=tk.EXTENDED)
        self.listbox.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        list_button_frame = ttk.Frame(inputs_frame)
        list_button_frame.pack(side=tk.RIGHT, fill=tk.Y, padx=(8, 0))

        ttk.Button(list_button_frame, text="Add", command=self._add_files).pack(fill=tk.X, pady=2)
        ttk.Button(list_button_frame, text="Remove", command=self._remove_selected).pack(fill=tk.X, pady=2)
        ttk.Button(list_button_frame, text="Clear", command=self._clear_files).pack(fill=tk.X, pady=2)

        output_frame = ttk.LabelFrame(root, text="Output", padding=8)
        output_frame.pack(fill=tk.X, pady=(10, 0))

        ttk.Label(output_frame, text="ARXML path:").grid(row=0, column=0, sticky=tk.W)
        ttk.Entry(output_frame, textvariable=self.output_var).grid(row=0, column=1, sticky=tk.EW, padx=(8, 8))
        ttk.Button(output_frame, text="Browse", command=self._browse_output).grid(row=0, column=2)
        output_frame.columnconfigure(1, weight=1)

        options_frame = ttk.LabelFrame(root, text="Options", padding=8)
        options_frame.pack(fill=tk.X, pady=(10, 0))

        ttk.Label(options_frame, text="Indent:").grid(row=0, column=0, sticky=tk.W)
        ttk.Spinbox(options_frame, from_=0, to=12, textvariable=self.indent_var, width=6).grid(
            row=0, column=1, sticky=tk.W, padx=(8, 16)
        )

        ttk.Checkbutton(options_frame, text="Strict mode (fail on unknown keys)", variable=self.strict_var).grid(
            row=0, column=2, sticky=tk.W
        )
        ttk.Checkbutton(options_frame, text="Allow extensions (zerocopy/custom_elements)", variable=self.allow_extensions_var).grid(
            row=0, column=3, sticky=tk.W, padx=(16, 0)
        )
        ttk.Checkbutton(options_frame, text="Validate only", variable=self.validate_only_var).grid(
            row=0, column=4, sticky=tk.W, padx=(16, 0)
        )

        action_frame = ttk.Frame(root)
        action_frame.pack(fill=tk.X, pady=(14, 0))

        ttk.Button(action_frame, text="Generate", command=self._generate).pack(side=tk.RIGHT)

    def _add_files(self) -> None:
        selected = filedialog.askopenfilenames(
            title="Select YAML files",
            filetypes=[("YAML files", "*.yaml *.yml"), ("All files", "*.*")],
        )
        for item in selected:
            path = Path(item).resolve()
            if path not in self.input_files:
                self.input_files.append(path)
                self.listbox.insert(tk.END, str(path))

    def _remove_selected(self) -> None:
        selected_indices = sorted(self.listbox.curselection(), reverse=True)
        for index in selected_indices:
            self.listbox.delete(index)
            del self.input_files[index]

    def _clear_files(self) -> None:
        self.listbox.delete(0, tk.END)
        self.input_files.clear()

    def _browse_output(self) -> None:
        path = filedialog.asksaveasfilename(
            title="Select output ARXML",
            defaultextension=".arxml",
            filetypes=[("ARXML files", "*.arxml"), ("XML files", "*.xml"), ("All files", "*.*")],
        )
        if path:
            self.output_var.set(path)

    def _build_summary_text(self, gen_ctx: GenerationContext, output_target: str) -> str:
        s = gen_ctx.summary
        lines = [
            f"Output: {output_target}",
            f"Packages: {s.package_count}",
            f"Communication clusters: {s.communication_cluster_count}",
            f"Ethernet connectors: {s.connector_count}",
            f"Provided SOME/IP instances: {s.provided_someip_count}",
            f"Required SOME/IP instances: {s.required_someip_count}",
            f"DDS bindings: {s.dds_binding_count}",
            f"ZeroCopy bindings: {s.zerocopy_binding_count}",
            f"Custom elements: {s.custom_element_count}",
        ]
        if gen_ctx.warnings:
            lines.append(f"Warnings ({len(gen_ctx.warnings)}):")
            lines.extend([f"- {w}" for w in gen_ctx.warnings])
        else:
            lines.append("Warnings: 0")
        return "\n".join(lines)

    def _generate(self) -> None:
        if not self.input_files:
            messagebox.showerror("Input required", "Add at least one YAML input file.")
            return

        try:
            xml_text, gen_ctx = generate_arxml_from_files(
                input_paths=self.input_files,
                indent=int(self.indent_var.get()),
                strict=bool(self.strict_var.get()),
                allow_extensions=bool(self.allow_extensions_var.get()),
            )
        except (ValueError, ConfigError) as exc:
            messagebox.showerror("Generation failed", str(exc))
            return

        validate_only = bool(self.validate_only_var.get())
        output_path_text = self.output_var.get().strip()

        if validate_only:
            messagebox.showinfo("Validation success", self._build_summary_text(gen_ctx, "<validate-only>"))
            return

        if not output_path_text:
            messagebox.showerror("Output required", "Select output ARXML path.")
            return

        output_path = Path(output_path_text)
        try:
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_text(xml_text, encoding="utf-8")
        except OSError as exc:
            messagebox.showerror("Write failed", str(exc))
            return

        messagebox.showinfo("Generation success", self._build_summary_text(gen_ctx, str(output_path)))


def main() -> int:
    app = ArxmlGeneratorGui()
    app.mainloop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
