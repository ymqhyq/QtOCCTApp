
import struct

def analyze_shx(path):
    with open(path, 'rb') as f:
        data = f.read()
    
    # scan for 0x1A
    header_end = -1
    for i in range(min(128, len(data))):
        if data[i] == 0x1A:
            header_end = i
            break
            
    if header_end == -1:
        print("No 0x1A found")
        return

    pos = header_end + 1
    while pos < len(data) and data[pos] == 0:
        pos += 1
        
    print(f"Shapes Start: {pos}")
    
    if pos + 4 > len(data):
        print("EOF before headers")
        return
        
    h1 = struct.unpack('<H', data[pos:pos+2])[0]
    h2 = struct.unpack('<H', data[pos+2:pos+4])[0]
    print(f"Header: {h1}, {h2}")
    
    pos += 4
    
    # Try parsing shapes
    for i in range(100):
        if pos >= len(data):
            break
            
        if pos + 4 > len(data):
            print(f"EOF at {pos}")
            break
            
        shape_num = struct.unpack('<H', data[pos:pos+2])[0]
        
        if shape_num == 0:
            def_bytes = struct.unpack('<H', data[pos+2:pos+4])[0]
            print(f"Shape 0 at {pos}, Len {def_bytes}")
            pos += 4 + def_bytes
            continue
            
        def_bytes = struct.unpack('<H', data[pos+2:pos+4])[0]
        print(f"Shape {shape_num} at {pos}, Len {def_bytes}")
        
        # Check if content looks sane (ends with 0?)
        # if def_bytes > 0 and pos + 4 + def_bytes <= len(data):
             # last_byte = data[pos + 4 + def_bytes - 1]
             # print(f"  ends with {last_byte:02X}")
             
        pos += 4 + def_bytes

analyze_shx('d:/QtOCCTApp/TTT.shx')
