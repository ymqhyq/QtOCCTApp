
import sys

print(f"Python Executable: {sys.executable}")
print("Attempting to import cadquery...")

try:
    import cadquery as cq
    print(f"CadQuery Version: {cq.__version__}")
    
    # Simple model test
    result = cq.Workplane('XY').box(10, 10, 10)
    print("CadQuery Model Created Successfully!")
    
except ImportError as e:
    print(f"ImportError: {e}")
    sys.exit(1)
except Exception as e:
    print(f"An error occurred: {e}")
    sys.exit(1)
