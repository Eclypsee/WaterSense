#!/usr/bin/env python3
"""
WaterSense Configuration Tool
A GUI application to configure WaterSense operating modes and generate setup.h files.
"""

import tkinter as tk
from tkinter import ttk, filedialog, messagebox
import os
import re
from datetime import datetime

class WaterSenseConfig:
    def __init__(self, root):
        self.root = root
        self.root.title("WaterSense Configuration Tool")
        self.root.geometry("650x500")
        self.root.resizable(True, True)
        
        # Configuration variables
        self.modes = {
            'CONTINUOUS': tk.BooleanVar(),
            'STANDALONE': tk.BooleanVar(),
            'NO_SURVEY': tk.BooleanVar(),
            'LEGACY': tk.BooleanVar(),
            'RADAR': tk.BooleanVar(),
            'VARIABLE_DUTY': tk.BooleanVar()
        }
        
        # Timing variables
        self.timing_vars = {
            'HI_READ': tk.StringVar(value="300"),  # 5 minutes
            'MID_READ': tk.StringVar(value="120"),  # 2 minutes
            'LOW_READ': tk.StringVar(value="60"),   # 1 minute
            'HI_ALLIGN': tk.StringVar(value="10"),
            'MID_ALLIGN': tk.StringVar(value="30"),
            'LOW_ALLIGN': tk.StringVar(value="60"),
            'GNSS_READ_TIME': tk.StringVar(value="7200"),  # 2 hours
            'FIX_DELAY': tk.StringVar(value="120"),  # 2 minutes
        }
        
        # Task period variables
        self.task_periods = {
            'MEASUREMENT_PERIOD': tk.StringVar(value="100"),
            'SD_PERIOD': tk.StringVar(value="10"),
            'CLOCK_PERIOD': tk.StringVar(value="100"),
            'SLEEP_PERIOD': tk.StringVar(value="100"),
            'VOLTAGE_PERIOD': tk.StringVar(value="1000"),
            'WATCHDOG_PERIOD': tk.StringVar(value="100"),
            'RADAR_TASK_PERIOD': tk.StringVar(value="100")
        }
        
        self.setup_ui()
        self.bind_mode_conflicts()
        self.load_current_config()
    
    def setup_ui(self):
        # Create main scrollable frame
        main_frame = tk.Frame(self.root)
        main_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)
        
        # Title
        title_label = tk.Label(main_frame, text="WaterSense Configuration", 
                              font=("Arial", 14, "bold"))
        title_label.pack(pady=5)
        
        # Create compact sections
        self.create_compact_modes(main_frame)
        self.create_compact_timing(main_frame)
        self.create_compact_buttons(main_frame)
    
    def create_compact_modes(self, parent):
        # Operating Modes section
        modes_frame = tk.LabelFrame(parent, text="Operating Modes", font=("Arial", 11, "bold"))
        modes_frame.pack(fill=tk.X, pady=5)
        
        # Create a grid layout for modes
        modes_grid = tk.Frame(modes_frame)
        modes_grid.pack(padx=10, pady=5)
        
        # Compact mode descriptions
        mode_info = {
            'CONTINUOUS': "Always-on (no sleep)",
            'STANDALONE': "GNSS only",
            'NO_SURVEY': "GNSS timing + sensors",
            'LEGACY': "v3 Adafruit GPS",
            'RADAR': "Radar sensor",
            'VARIABLE_DUTY': "Adaptive power"
        }
        
        # Create checkboxes in a 2x3 grid
        self.mode_checkboxes = {}
        row = 0
        col = 0
        for mode, description in mode_info.items():
            frame = tk.Frame(modes_grid)
            frame.grid(row=row, column=col, sticky="w", padx=5, pady=2)
            
            checkbox = tk.Checkbutton(frame, text=f"{mode}", variable=self.modes[mode], 
                                    font=("Arial", 10, "bold"))
            checkbox.pack(anchor=tk.W)
            
            desc_label = tk.Label(frame, text=description, font=("Arial", 8), fg="gray")
            desc_label.pack(anchor=tk.W)
            
            self.mode_checkboxes[mode] = checkbox
            
            col += 1
            if col > 1:  # 2 columns
                col = 0
                row += 1
        
        # Simple conflict warning
        warning_label = tk.Label(modes_frame, text="⚠️ Auto-resolves conflicts: STANDALONE ❌ NO_SURVEY, LEGACY ❌ NO_SURVEY/STANDALONE", 
                               font=("Arial", 8), fg="#856404", wraplength=600)
        warning_label.pack(pady=5)
    
    def create_compact_timing(self, parent):
        # Key timing settings only
        timing_frame = tk.LabelFrame(parent, text="Key Timing Settings", font=("Arial", 11, "bold"))
        timing_frame.pack(fill=tk.X, pady=5)
        
        # Create grid for timing settings
        timing_grid = tk.Frame(timing_frame)
        timing_grid.pack(padx=10, pady=5)
        
        # Only show the most important timing settings
        key_timings = [
            ('HI_READ', 'Read Interval (sec):', 0, 0),
            ('HI_ALLIGN', 'Time Align (sec):', 0, 1),
            ('GNSS_READ_TIME', 'GNSS Update (sec):', 1, 0),
            ('FIX_DELAY', 'GPS Fix Delay (sec):', 1, 1)
        ]
        
        for var_name, label, row, col in key_timings:
            frame = tk.Frame(timing_grid)
            frame.grid(row=row, column=col, sticky="w", padx=10, pady=3)
            
            tk.Label(frame, text=label, font=("Arial", 9)).pack(anchor=tk.W)
            entry = tk.Entry(frame, textvariable=self.timing_vars[var_name], width=8, font=("Arial", 9))
            entry.pack(anchor=tk.W)
    
    def create_compact_buttons(self, parent):
        # Simple button layout
        button_frame = tk.Frame(parent)
        button_frame.pack(pady=10)
        
        # Main action button - Apply directly to project
        apply_btn = tk.Button(button_frame, text="✅ Apply to Project", 
                             command=self.quick_replace_setup, bg="#28a745", fg="white",
                             font=("Arial", 12, "bold"), padx=20, pady=8)
        apply_btn.pack(pady=5)
        
        # Secondary buttons in a row
        secondary_frame = tk.Frame(button_frame)
        secondary_frame.pack(pady=5)
        
        load_btn = tk.Button(secondary_frame, text="📁 Load", 
                           command=self.load_config_file, bg="#6c757d", fg="white",
                           font=("Arial", 9), padx=8, pady=4)
        load_btn.pack(side=tk.LEFT, padx=2)
        
        save_btn = tk.Button(secondary_frame, text="💾 Save As", 
                           command=self.generate_setup_h, bg="#6c757d", fg="white",
                           font=("Arial", 9), padx=8, pady=4)
        save_btn.pack(side=tk.LEFT, padx=2)
        
        reset_btn = tk.Button(secondary_frame, text="🔄 Reset", 
                            command=self.reset_defaults, bg="#6c757d", fg="white",
                            font=("Arial", 9), padx=8, pady=4)
        reset_btn.pack(side=tk.LEFT, padx=2)
        
        # Status label
        self.status_label = tk.Label(parent, text="Ready • Configure above and click 'Apply to Project'", 
                                   font=("Arial", 9), fg="#6c757d")
        self.status_label.pack(pady=5)
    
    def bind_mode_conflicts(self):
        """Bind mode conflict resolution"""
        def on_standalone_change():
            if self.modes['STANDALONE'].get():
                self.modes['NO_SURVEY'].set(False)
                self.mode_checkboxes['NO_SURVEY'].config(state=tk.DISABLED)
            else:
                self.mode_checkboxes['NO_SURVEY'].config(state=tk.NORMAL)
        
        def on_no_survey_change():
            if self.modes['NO_SURVEY'].get():
                self.modes['STANDALONE'].set(False)
                self.modes['LEGACY'].set(False)
                self.mode_checkboxes['STANDALONE'].config(state=tk.DISABLED)
                self.mode_checkboxes['LEGACY'].config(state=tk.DISABLED)
            else:
                self.mode_checkboxes['STANDALONE'].config(state=tk.NORMAL)
                self.mode_checkboxes['LEGACY'].config(state=tk.NORMAL)
        
        def on_legacy_change():
            if self.modes['LEGACY'].get():
                self.modes['NO_SURVEY'].set(False)
                self.modes['STANDALONE'].set(False)
                self.mode_checkboxes['NO_SURVEY'].config(state=tk.DISABLED)
                self.mode_checkboxes['STANDALONE'].config(state=tk.DISABLED)
            else:
                self.mode_checkboxes['NO_SURVEY'].config(state=tk.NORMAL)
                self.mode_checkboxes['STANDALONE'].config(state=tk.NORMAL)
        
        self.modes['STANDALONE'].trace_add('write', lambda *args: on_standalone_change())
        self.modes['NO_SURVEY'].trace_add('write', lambda *args: on_no_survey_change())
        self.modes['LEGACY'].trace_add('write', lambda *args: on_legacy_change())
    
    def load_current_config(self):
        """Load configuration from existing setup.h if available"""
        setup_path = "src/setup.h"
        if os.path.exists(setup_path):
            try:
                with open(setup_path, 'r') as f:
                    content = f.read()
                
                # Parse mode definitions
                for mode in self.modes:
                    pattern = rf'^#define\s+{mode}\s*$'
                    if re.search(pattern, content, re.MULTILINE):
                        self.modes[mode].set(True)
                
                # Parse timing values
                for var_name in self.timing_vars:
                    pattern = rf'#define\s+{var_name}\s+(.+?)(?://|$)'
                    match = re.search(pattern, content, re.MULTILINE)
                    if match:
                        value = match.group(1).strip()
                        # Handle expressions like "60*5"
                        try:
                            evaluated = eval(value)
                            self.timing_vars[var_name].set(str(evaluated))
                        except:
                            self.timing_vars[var_name].set(value)
                
                # Parse task periods
                for var_name in self.task_periods:
                    pattern = rf'#define\s+{var_name}\s+(\d+)'
                    match = re.search(pattern, content, re.MULTILINE)
                    if match:
                        self.task_periods[var_name].set(match.group(1))
                        
            except Exception as e:
                print(f"Error loading current config: {e}")
    
    def generate_setup_h(self):
        """Save configuration as setup.h file"""
        try:
            # Get output path
            output_path = filedialog.asksaveasfilename(
                defaultextension=".h",
                filetypes=[("Header files", "*.h"), ("All files", "*.*")],
                initialname="setup.h"
            )
            
            if not output_path:
                return
            
            self.status_label.config(text="Saving...", fg="#ffc107")
            self.root.update()
            
            # Generate header content
            header_content = self.generate_header_content()
            
            # Write to file
            with open(output_path, 'w') as f:
                f.write(header_content)
            
            filename = os.path.basename(output_path)
            self.status_label.config(text=f"✅ Saved as {filename}", fg="#28a745")
            
        except Exception as e:
            self.status_label.config(text="❌ Save failed", fg="#dc3545")
            messagebox.showerror("Error", f"Failed to save: {str(e)}")
    
    def quick_replace_setup(self):
        """Apply configuration directly to project"""
        try:
            setup_path = "src/setup.h"
            
            # Check if src directory exists
            if not os.path.exists("src"):
                messagebox.showerror("Error", "Run this tool from the WaterSense project root directory.\n\n'src' folder not found.")
                return
            
            self.status_label.config(text="Applying configuration...", fg="#ffc107")
            self.root.update()
            
            # Generate and save directly
            header_content = self.generate_header_content()
            
            # Backup existing file
            if os.path.exists(setup_path):
                backup_path = f"{setup_path}.backup"
                import shutil
                shutil.copy2(setup_path, backup_path)
            
            # Write new file
            with open(setup_path, 'w') as f:
                f.write(header_content)
            
            self.status_label.config(text="✅ Applied! Ready to build: pio run --target upload", fg="#28a745")
            messagebox.showinfo("Configuration Applied", "✅ Configuration saved to src/setup.h\n\n🚀 Ready to build and upload:\n   pio run --target upload")
            
        except Exception as e:
            self.status_label.config(text="❌ Failed to apply configuration", fg="#dc3545")
            messagebox.showerror("Error", f"Failed to apply configuration:\n{str(e)}")
    
    def generate_header_content(self):
        """Generate the actual header file content"""
        content = f"""/**
 * @file setup.h
 * @author WaterSense Configuration Tool
 * @brief Auto-generated configuration file
 * @version 0.1
 * @date {datetime.now().strftime('%Y-%m-%d')}
 * 
 * @copyright Copyright (c) 2023
 * 
 */

//-----------------------------------------------------------------------------------------------------||
//---------- Define Constants -------------------------------------------------------------------------||

"""
        
        # Add mode definitions
        mode_descriptions = {
            'CONTINUOUS': "Define this constant to enable continuous measurements\n * @details Writes data to the SD card at the specified read intervals but does not sleep",
            'STANDALONE': "Define this constant to enable standalone GNSS measurements (no sonar or temp)\n * @details Writes data to the SD card with minimal sleep time",
            'NO_SURVEY': "Define this constant to disable GNSS measurements (only used as clock)\n * @details Writes data to the SD card with minimal sleep time",
            'LEGACY': "Define this constant to enable v3 Adafruit Ultimate Breakout GPS (no sonar or temp)\n * @details enables taskClock2",
            'RADAR': "Define this constant to enable Radar instead of Ultrasonic\n * @details enables taskRadar",
            'VARIABLE_DUTY': "Define this constant to enable variable duty cycle\n * @details If undefined, HI_READ and HI_ALLIGN are used"
        }
        
        for mode, description in mode_descriptions.items():
            content += f"""/**
 * @brief {description}
 * 
 */
"""
            if self.modes[mode].get():
                content += f"#define {mode}\n\n"
            else:
                content += f"//#define {mode}\n\n"
        
        # Add timing definitions
        content += """//----------------------||
"""
        for var_name in ['HI_READ', 'MID_READ', 'LOW_READ']:
            content += f"#define {var_name} {self.timing_vars[var_name].get()} //||\n"
        
        content += "//                    //||\n"
        
        for var_name in ['HI_ALLIGN', 'MID_ALLIGN', 'LOW_ALLIGN']:
            content += f"#define {var_name} {self.timing_vars[var_name].get()} //||\n"
        
        content += """//----------------------||

"""
        
        # Add other timing constants
        content += f"#define GNSS_READ_TIME {self.timing_vars['GNSS_READ_TIME'].get()}\n\n"
        content += f"#define GNSS_STANDALONE_SLEEP (uint64_t) 60 * 1000000///<us of sleep time\n\n"
        content += f"#define WAKE_CYCLES 15 ///< Number of wake cycles between reset checks\n"
        content += f"#define FIX_DELAY {self.timing_vars['FIX_DELAY'].get()} ///< Seconds to wait for first GPS fix\n\n"
        content += f"#define WATCH_TIMER 15*1000 ///< ms of hang time before triggering a reset\n\n"
        
        # Add task periods
        for var_name, var in self.task_periods.items():
            content += f"#define {var_name} {var.get()}\n"
        
        # Add hardware constants (simplified version)
        content += """
#define R1b 9.25 ///< Larger resistor for battery voltage divider
#define R2b 3.3 ///< Smaller resistor for battery voltage divider

#define R1s 10.0 ///< Resistor for solar panel voltage divider
#define R2s 10.0 ///< Resistor for solar panel voltage divider

#define sdWriteSize 8192 ///<Write data to the SD card in blocks of 8192 bytes

//-----------------------------------------------------------------------------------------------------||
//---------- Define Pins ------------------------------------------------------------------------------||

#define LED GPIO_NUM_2
#define SD_CS GPIO_NUM_5 ///< SD card chip select pin
#define SONAR_RX GPIO_NUM_14 ///< Sonar sensor receive pin
#define SONAR_TX GPIO_NUM_32 ///< Sonar sensor transmit pin
#define SONAR_EN GPIO_NUM_33 ///< Sonar sensor enable pin
#define GPS_RX GPIO_NUM_16 ///< GPS receive pin
#define GPS_TX GPIO_NUM_17 ///< GPS transmit pin
#define GPS_EN GPIO_NUM_27 ///< GPS enable pin
#define TEMP_SENSOR_ADDRESS 0x44 ///< Temperature and humidity sensor hex address
#define TEMP_EN GPIO_NUM_15 ///< Temperature/humidity sensor enable pin
#define ADC_PIN GPIO_NUM_26

//-----------------------------------------------------------------------------------------------------||
//-----------------------------------------------------------------------------------------------------||
"""
        
        return content
    
    def load_config_file(self):
        """Load configuration from a setup.h file"""
        filepath = filedialog.askopenfilename(
            title="Load Configuration",
            filetypes=[("Header files", "*.h"), ("All files", "*.*")]
        )
        
        if filepath:
            try:
                self.status_label.config(text="Loading...", fg="#ffc107")
                self.root.update()
                
                with open(filepath, 'r') as f:
                    content = f.read()
                
                # Reset all modes first
                for mode in self.modes:
                    self.modes[mode].set(False)
                
                # Parse and set modes
                modes_found = []
                for mode in self.modes:
                    pattern = rf'^#define\s+{mode}\s*$'
                    if re.search(pattern, content, re.MULTILINE):
                        self.modes[mode].set(True)
                        modes_found.append(mode)
                
                # Parse timing values
                for var_name in self.timing_vars:
                    pattern = rf'#define\s+{var_name}\s+(.+?)(?://|$)'
                    match = re.search(pattern, content, re.MULTILINE)
                    if match:
                        value = match.group(1).strip()
                        try:
                            evaluated = eval(value)
                            self.timing_vars[var_name].set(str(evaluated))
                        except:
                            self.timing_vars[var_name].set(value)
                
                # Parse task periods
                for var_name in self.task_periods:
                    pattern = rf'#define\s+{var_name}\s+(\d+)'
                    match = re.search(pattern, content, re.MULTILINE)
                    if match:
                        self.task_periods[var_name].set(match.group(1))
                
                filename = os.path.basename(filepath)
                modes_text = ', '.join(modes_found) if modes_found else 'None'
                self.status_label.config(text=f"✅ Loaded {filename} • Modes: {modes_text}", fg="#28a745")
                
            except Exception as e:
                self.status_label.config(text="❌ Load failed", fg="#dc3545")
                messagebox.showerror("Error", f"Failed to load: {str(e)}")
    

    
    def reset_defaults(self):
        """Reset all settings to defaults"""
        self.status_label.config(text="Resetting...", fg="#ffc107")
        self.root.update()
        
        # Reset all modes
        for mode in self.modes:
            self.modes[mode].set(False)
        
        # Reset timing to defaults
        defaults = {
            'HI_READ': "300", 'MID_READ': "120", 'LOW_READ': "60",
            'HI_ALLIGN': "10", 'MID_ALLIGN': "30", 'LOW_ALLIGN': "60",
            'GNSS_READ_TIME': "7200", 'FIX_DELAY': "120"
        }
        for var, default in defaults.items():
            self.timing_vars[var].set(default)
        
        # Reset task periods to defaults
        task_defaults = {
            'MEASUREMENT_PERIOD': "100", 'SD_PERIOD': "10", 'CLOCK_PERIOD': "100",
            'SLEEP_PERIOD': "100", 'VOLTAGE_PERIOD': "1000", 'WATCHDOG_PERIOD': "100",
            'RADAR_TASK_PERIOD': "100"
        }
        for var, default in task_defaults.items():
            self.task_periods[var].set(default)
        
        # Set reasonable defaults for WaterSense
        self.modes['NO_SURVEY'].set(True)
        self.modes['RADAR'].set(True)
        
        self.status_label.config(text="✅ Reset complete • NO_SURVEY + RADAR enabled", fg="#28a745")

def main():
    root = tk.Tk()
    app = WaterSenseConfig(root)
    root.mainloop()

if __name__ == "__main__":
    main() 