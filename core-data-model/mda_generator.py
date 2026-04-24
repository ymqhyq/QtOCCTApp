import os
import yaml
from jinja2 import Environment, FileSystemLoader

# Configuration
SCHEMA_DIR = "./schema"
TEMPLATE_DIR = "./templates"
OUTPUT_DIR = "./generated"

# Type Mapping
TYPE_MAP = {
    "Real":      {"cpp": "double", "act": "Real",      "set": "SetValue", "get": "GetValue"},
    "Int":       {"cpp": "int",    "act": "Int",       "set": "SetValue", "get": "GetValue"},
    "String":    {"cpp": "TCollection_AsciiString", "act": "String", "set": "SetValue", "get": "GetValue"},
    "Bool":      {"cpp": "bool",   "act": "Bool",      "set": "SetValue", "get": "GetValue"},
    "Shape":     {"cpp": "TopoDS_Shape", "act": "Shape", "set": "SetShape", "get": "GetShape"},
    "RealArray": {"cpp": "Handle(TColStd_HArray1OfReal)", "act": "RealArray", "set": "SetArray", "get": "GetArray"}
}

def generate_code():
    # Setup Jinja2
    env = Environment(loader=FileSystemLoader(TEMPLATE_DIR))
    header_tmpl = env.get_template("node_h.jinja2")
    cpp_tmpl = env.get_template("node_cpp.jinja2")

    if not os.path.exists(OUTPUT_DIR):
        os.makedirs(OUTPUT_DIR)

    # All entity names for forward declarations
    all_entity_names = []
    for filename in os.listdir(SCHEMA_DIR):
        if filename.endswith(".yaml"):
            with open(os.path.join(SCHEMA_DIR, filename), "r", encoding="utf-8") as f:
                s = yaml.safe_load(f)
                all_entity_names.extend(s.keys())

    # Process each schema file
    for filename in os.listdir(SCHEMA_DIR):
        if not filename.endswith(".yaml"):
            continue
            
        with open(os.path.join(SCHEMA_DIR, filename), "r", encoding="utf-8") as f:
            schema = yaml.safe_load(f)
            
        for entity_name, entity_data in schema.items():
            print(f"Generating code for: {entity_name}")
            
            # Prepare context
            parent_name = entity_data.get("parent")
            parent_class = f"DataNode_{parent_name}" if parent_name else "ActData_BaseNode"
            
            ctx = {
                "class_name": f"DataNode_{entity_name}",
                "parent_class": parent_class,
                "all_entity_names": all_entity_names,
                "name": entity_data.get("name", entity_name),
                "attributes": {},
                "children": {}
            }
            
            attrs = entity_data.get("attributes", {})
            for attr_name, attr_info in attrs.items():
                t_info = TYPE_MAP.get(attr_info["type"], TYPE_MAP["Real"])
                ctx["attributes"][attr_name] = {
                    "cpp_type": t_info["cpp"],
                    "act_type": t_info["act"],
                    "set_method": t_info["set"],
                    "get_method": t_info["get"],
                    "label": attr_info.get("label", attr_name)
                }
            
            children = entity_data.get("children", {})
            for child_name, child_info in children.items():
                ctx["children"][child_name] = {
                    "type_name": child_info["type"],
                    "class_name": f"DataNode_{child_info['type']}",
                    "cardinality": child_info.get("cardinality", "1")
                }
            
            # Render Header
            header_content = header_tmpl.render(ctx)
            with open(os.path.join(OUTPUT_DIR, f"{ctx['class_name']}.h"), "w", encoding="utf-8") as hf:
                hf.write(header_content)
                
            # Render CPP
            cpp_content = cpp_tmpl.render(ctx)
            with open(os.path.join(OUTPUT_DIR, f"{ctx['class_name']}.cpp"), "w", encoding="utf-8") as cf:
                cf.write(cpp_content)

    # Render Main Model Class
    model_tmpl = env.get_template("model_h.jinja2")
    # We need a combined schema view here
    all_schemas = {}
    for filename in os.listdir(SCHEMA_DIR):
        if filename.endswith(".yaml"):
            with open(os.path.join(SCHEMA_DIR, filename), "r", encoding="utf-8") as f:
                all_schemas.update(yaml.safe_load(f))
    
    model_content = model_tmpl.render(schema=all_schemas)
    with open(os.path.join(OUTPUT_DIR, "DataModel.h"), "w", encoding="utf-8") as mf:
        mf.write(model_content)

if __name__ == "__main__":
    try:
        generate_code()
        print("\nMDA Generation Complete! Files are in 'generated' folder.")
    except Exception as e:
        print(f"Error during generation: {e}")
        print("Please ensure 'pyyaml' and 'jinja2' are installed: pip install PyYAML Jinja2")
