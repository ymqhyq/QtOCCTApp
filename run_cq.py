import sys
import os
import traceback
import signal

# Force unbuffered output/utf-8
try:
    sys.stdout.reconfigure(encoding='utf-8')
    sys.stderr.reconfigure(encoding='utf-8')
    sys.stdin.reconfigure(encoding='utf-8')
except AttributeError:
    pass # Python < 3.7

def setup_environment():
    """Setup Conda paths if needed"""
    if sys.platform == 'win32':
        exe_dir = os.path.dirname(sys.executable)
        conda_lib_bin = os.path.join(exe_dir, "Library", "bin")
        if os.path.exists(conda_lib_bin):
            try:
                os.add_dll_directory(conda_lib_bin)
            except AttributeError:
                pass
            os.environ["PATH"] = conda_lib_bin + os.pathsep + os.environ["PATH"]

_script_global_vars = {}
_script_local_vars = {}

def process_request(line):
    """
    Process a single request line.
    Format: <script_path>|<output_brep_path>
    """
    try:
        parts = line.strip().split('|')
        if len(parts) != 2:
            return f"ERROR: Invalid request format. Expected 'script|output', got '{line.strip()}'"
        
        script_file, output_file = parts
        
        if not os.path.exists(script_file):
            return f"ERROR: Script file not found: {script_file}"

        # capture stdout/stderr from user script
        from io import StringIO
        import contextlib
        
        # Read script content
        with open(script_file, 'r', encoding='utf-8') as f:
            script_content = f.read()

        # Prepare execution environment (Persistent)
        global _script_global_vars, _script_local_vars
        
        # Clear specific variables to avoid carry-over
        _script_local_vars.pop('result', None)
        _script_global_vars.pop('result', None)
        _script_local_vars.pop('material', None)
        _script_global_vars.pop('material', None)
        
        # Helper for showing objects (CQ-editor compatibility)
        def show_object(obj, name=None, options=None):
            _script_local_vars['result'] = obj

        _script_global_vars['show_object'] = show_object
        
        # Execute script
        exec(script_content, _script_global_vars, _script_local_vars)
        
        # Extract result
        if 'result' not in _script_local_vars:
            # Check if user defined 'result' in global scope by mistake (or intentional)
            if 'result' in _script_global_vars:
                _script_local_vars['result'] = _script_global_vars['result']
            else:
                 return "ERROR: variable 'result' not found in script."

        result_obj = _script_local_vars['result']
        
        # Handle CadQuery objects
        import cadquery as cq
        from OCP.TopoDS import TopoDS_Shape
        from OCP.BRepTools import BRepTools
        
        shape_to_export = None
        
        if isinstance(result_obj, cq.Workplane):
            shape_to_export = result_obj.val().wrapped
        elif isinstance(result_obj, cq.Shape):
            shape_to_export = result_obj.wrapped
        elif isinstance(result_obj, TopoDS_Shape):
            shape_to_export = result_obj
        else:
             return f"ERROR: Unsupported result type: {type(result_obj)}"

        # Export to BREP
        if shape_to_export:
             BRepTools.Write_s(shape_to_export, output_file)
             # Check for material override in script (check both local and global)
             material = _script_local_vars.get('material', _script_global_vars.get('material', None))
             
             # DEBUG: Force print to stderr
             sys.stderr.write(f"DEBUG: Found material: {material}\n")
             
             if material:
                 return f"SUCCESS|{str(material).upper()}"
             return "SUCCESS"
        else:
             return "ERROR: Result shape is null"

    except Exception:
        return f"EXCEPTION: {traceback.format_exc()}"

def main():
    setup_environment()
    print("READY", flush=True)

    while True:
        try:
            line = sys.stdin.readline()
            if not line:
                break
            
            line = line.strip()
            if line == "EXIT":
                break
            
            if not line:
                continue
                
            result = process_request(line)
            # Use a specific delimiter for end of response to keep protocol simple
            # We print single line response for status
            print(result.replace('\n', ' || '), flush=True)
            
        except KeyboardInterrupt:
            break
        except Exception as e:
            print(f"FATAL: {e}", flush=True)

if __name__ == "__main__":
    main()
