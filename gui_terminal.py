#!/usr/bin/env python3
"""
GUI Terminal Wrapper
A graphical interface for the C Linux Terminal App.
Spawns the terminal app and displays output in a modern tkinter window.
"""

import tkinter as tk
from tkinter import scrolledtext, messagebox
import subprocess
import threading
import os
import signal

class GUITerminal:
    def __init__(self, root):
        self.root = root
        self.root.title("LINUX Terminal")
        self.root.geometry("900x600")
        self.root.configure(bg="#1e1e1e")
        
        # Path to the compiled terminal app
        script_dir = os.path.dirname(os.path.abspath(__file__))
        self.terminal_app = os.path.join(script_dir, "bin", "terminal_app")
        
        # Verify binary exists
        if not os.path.exists(self.terminal_app):
            messagebox.showerror("Error", f"Terminal app not found at {self.terminal_app}\nPlease run 'make' first.")
            self.root.destroy()
            return
        
        self.process = None
        self.reader_thread = None
        self.setup_ui()
        self.start_terminal()
    
    def setup_ui(self):
        """Create the GUI layout."""
        # Title bar
        title_frame = tk.Frame(self.root, bg="#121e25", height=40)
        title_frame.pack(side=tk.TOP, fill=tk.X)
        title_label = tk.Label(
            title_frame,
            text="C Linux Terminal (BY EL3ARABY)",
            font=("Consolas", 14, "bold"),
            bg="#121e25",
            fg="white"
        )
        title_label.pack(pady=10)
        
        # Output display (scrolled text widget)
        output_frame = tk.Frame(self.root, bg="#1e1e1e")
        output_frame.pack(expand=True, fill=tk.BOTH, padx=10, pady=(10, 0))
        
        output_label = tk.Label(
            output_frame,
            text="Output:",
            font=("Consolas", 10, "bold"),
            bg="#1e1e1e",
            fg="#d4d4d4"
        )
        output_label.pack(anchor="w")
        
        self.output_text = scrolledtext.ScrolledText(
            output_frame,
            width=100,
            height=20,
            font=("Consolas", 10),
            bg="#252526",
            fg="#d4d4d4",
            insertbackground="#d4d4d4",
            state=tk.DISABLED,
            wrap=tk.WORD
        )
        self.output_text.pack(expand=True, fill=tk.BOTH, pady=(5, 0))

        # Configure tags for styling
        self.output_text.tag_config("welcome", foreground="#4ec9b0")
        self.output_text.tag_config("prompt", foreground="#ce9178")
        self.output_text.tag_config("user", foreground="#66ff66")
        self.output_text.tag_config("cwd", foreground="#66a3ff")
        self.output_text.tag_config("error", foreground="#f48771")
        
        # Configure ANSI color tags
        self.output_text.tag_config('ansi_red', foreground='#ff5555')
        self.output_text.tag_config('ansi_green', foreground='#55ff55')
        self.output_text.tag_config('ansi_yellow', foreground='#ffff55')
        self.output_text.tag_config('ansi_blue', foreground='#5555ff')
        self.output_text.tag_config('ansi_magenta', foreground='#ff55ff')
        self.output_text.tag_config('ansi_cyan', foreground='#55ffff')
        self.output_text.tag_config('ansi_white', foreground='#ffffff')

        # Input frame
        input_frame = tk.Frame(self.root, bg="#1e1e1e")
        input_frame.pack(fill=tk.X, padx=10, pady=10)
        
        # Prompt pieces: user@host (green), cwd (blue), then $
        prompt_frame = tk.Frame(input_frame, bg="#1e1e1e")
        prompt_frame.pack(side=tk.LEFT)

        self.user_host_label = tk.Label(
            prompt_frame,
            text="",
            font=("Consolas", 10, "bold"),
            bg="#1e1e1e",
            fg="#66ff66"
        )
        self.user_host_label.pack(side=tk.LEFT)

        self.cwd_label = tk.Label(
            prompt_frame,
            text="",
            font=("Consolas", 10),
            bg="#1e1e1e",
            fg="#66a3ff"
        )
        self.cwd_label.pack(side=tk.LEFT)

        self.dollar_label = tk.Label(
            prompt_frame,
            text="$ ",
            font=("Consolas", 10, "bold"),
            bg="#1e1e1e",
            fg="#ce9178"
        )
        self.dollar_label.pack(side=tk.LEFT)
        
        self.input_entry = tk.Entry(
            input_frame,
            font=("Consolas", 10),
            bg="#252526",
            fg="#d4d4d4",
            insertbackground="#d4d4d4"
        )
        self.input_entry.pack(side=tk.LEFT, expand=True, fill=tk.X, padx=(0, 10))
        self.input_entry.bind("<Return>", self.on_input_enter)
        self.input_entry.focus()
        # Start updating prompt (username@host:cwd)
        self.update_prompt()
        
        # Send button
        send_button = tk.Button(
            input_frame,
            text="EXCUTE",
            font=("Consolas", 9),
            bg="#af2a2a",
            fg="white",
            command=self.on_input_enter,
            padx=10
        )
        send_button.pack(side=tk.LEFT)
        
        # Status bar
        status_frame = tk.Frame(self.root, bg="#333333", height=25)
        status_frame.pack(side=tk.BOTTOM, fill=tk.X)
        self.status_label = tk.Label(
            status_frame,
            text="Status: Initializing...",
            font=("Consolas", 9),
            bg="#333333",
            fg="#d4d4d4"
        )
        self.status_label.pack(anchor="w", padx=10, pady=5)
    
    def start_terminal(self):
        """Start the terminal app subprocess."""
        try:
            self.process = subprocess.Popen(
                [self.terminal_app],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                bufsize=1
            )
            self.update_status("Status: Running")
            
            # Start reader thread to capture output
            self.reader_thread = threading.Thread(target=self.read_output, daemon=True)
            self.reader_thread.start()
        except Exception as e:
            messagebox.showerror("Error", f"Failed to start terminal: {e}")
            self.root.destroy()
    
    def read_output(self):
        """Read output from the terminal app in a separate thread."""
        try:
            while self.process and self.process.poll() is None:
                try:
                    line = self.process.stdout.readline()
                    if line:
                        # Handle ANSI clear sequences by clearing the widget
                        if "\x1b[2J" in line or "\x1b[H" in line:
                            self.root.after(0, self._clear_output)
                            clean = line.replace("\x1b[2J", "").replace("\x1b[H", "")
                            if clean.strip():
                                self.display_output(clean, is_error=False)
                        else:
                            self.display_output(line, is_error=False)
                except:
                    break
            
            # Read any remaining output after process exits
            if self.process:
                remaining = self.process.stdout.read()
                if remaining:
                    self.display_output(remaining, is_error=False)
            
            self.update_status("Status: Terminated")
        except Exception as e:
            self.display_output(f"[Error reading output: {e}]\n", is_error=True)
    
    def display_output(self, text, is_error=False):
        """Thread-safe method to display output in the text widget."""
        self.root.after(0, self._display_output_main, text, is_error)
    
    def _display_output_main(self, text, is_error):
        """Main thread method to update output text with ANSI color support."""
        self.output_text.config(state=tk.NORMAL)
        if is_error:
            self.output_text.insert(tk.END, text, "error")
        else:
            self._insert_with_ansi_colors(text)
        self.output_text.see(tk.END)
        self.output_text.config(state=tk.DISABLED)
    
    def _insert_with_ansi_colors(self, text):
        """Insert text with ANSI color code support."""
        i = 0
        current_tags = []
        buf = ""
        
        while i < len(text):
            if text[i:i+2] == '\x1b[':
                # Found ANSI escape sequence, flush buffer first
                if buf:
                    self.output_text.insert(tk.END, buf, tuple(current_tags) if current_tags else ())
                    buf = ""
                
                # Parse the escape code
                i += 2
                code_str = ""
                while i < len(text) and text[i].isdigit():
                    code_str += text[i]
                    i += 1
                
                # Handle 'm' (SGR - Select Graphic Rendition)
                if i < len(text) and text[i] == 'm':
                    code = int(code_str) if code_str else 0
                    if code == 0:
                        current_tags = []  # reset
                    elif code == 31:
                        current_tags = ['ansi_red']
                    elif code == 32:
                        current_tags = ['ansi_green']
                    elif code == 33:
                        current_tags = ['ansi_yellow']
                    elif code == 34:
                        current_tags = ['ansi_blue']
                    elif code == 35:
                        current_tags = ['ansi_magenta']
                    elif code == 36:
                        current_tags = ['ansi_cyan']
                    elif code == 37:
                        current_tags = ['ansi_white']
                    i += 1
            else:
                # Regular character
                buf += text[i]
                i += 1
        
        # Flush remaining buffer
        if buf:
            self.output_text.insert(tk.END, buf, tuple(current_tags) if current_tags else ())
    
    def on_input_enter(self, event=None):
        """Handle input when user presses Enter or clicks Send."""
        command = self.input_entry.get().strip()
        self.input_entry.delete(0, tk.END)
        
        if not command:
            return
        
        # Display the command with prompt colors
        self.root.after(0, lambda: self._display_command_with_prompt(command))
        
        # Send command to terminal app
        try:
            if self.process and self.process.poll() is None:
                self.process.stdin.write(command + "\n")
                self.process.stdin.flush()
        except BrokenPipeError:
            self.display_output("[Process terminated]\n", is_error=True)
            self.update_status("Status: Process terminated")
        except Exception as e:
            self.display_output(f"[Error sending command: {e}]\n", is_error=True)

    def _clear_output(self):
        try:
            self.output_text.config(state=tk.NORMAL)
            self.output_text.delete("1.0", tk.END)
            self.output_text.config(state=tk.DISABLED)
        except:
            pass

    def _display_command_with_prompt(self, command):
        # Insert colored prompt pieces then the command
        try:
            self.output_text.config(state=tk.NORMAL)
            user = os.getenv('USER') or 'user'
            host = os.uname().nodename
            cwd = os.getcwd()
            prompt1 = f"{user}@{host}:"
            prompt2 = f"{cwd}$ "
            self.output_text.insert(tk.END, prompt1, 'user')
            self.output_text.insert(tk.END, prompt2, 'cwd')
            self.output_text.insert(tk.END, command + "\n")
            self.output_text.see(tk.END)
            self.output_text.config(state=tk.DISABLED)
        except Exception as e:
            self.output_text.config(state=tk.NORMAL)
            self.output_text.insert(tk.END, command + "\n")
            self.output_text.config(state=tk.DISABLED)
    
    def update_status(self, message):
        """Update the status bar."""
        self.root.after(0, lambda: self.status_label.config(text=message))

    def update_prompt(self):
        try:
            user = os.getenv('USER') or 'user'
            host = os.uname().nodename
            cwd = os.getcwd()
            self.user_host_label.config(text=f"{user}@{host}:")
            self.cwd_label.config(text=cwd)
        except Exception:
            self.user_host_label.config(text="user@host:")
            self.cwd_label.config(text="~")
        # Refresh every second
        self.root.after(1000, self.update_prompt)
    
    def on_closing(self):
        """Handle window close event."""
        if self.process and self.process.poll() is None:
            try:
                self.process.terminate()
                self.process.wait(timeout=2)
            except:
                self.process.kill()
        self.root.destroy()

def main():
    root = tk.Tk()
    app = GUITerminal(root)
    root.protocol("WM_DELETE_WINDOW", app.on_closing)
    root.mainloop()

if __name__ == "__main__":
    main()
