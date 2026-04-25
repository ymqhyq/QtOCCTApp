import os
import yaml
from jinja2 import Environment, FileSystemLoader

# Configuration
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SCHEMA_DIR = os.path.join(SCRIPT_DIR, "schema")
TEMPLATE_DIR = os.path.join(SCRIPT_DIR, "templates")
OUTPUT_DIR = os.path.join(SCRIPT_DIR, "generated")

# Type Mapping
TYPE_MAP = {
    "Real":      {"cpp": "double", "act": "Real",      "set": "SetValue", "get": "GetValue"},
    "Int":       {"cpp": "int",    "act": "Int",       "set": "SetValue", "get": "GetValue"},
    "String":    {"cpp": "TCollection_AsciiString", "act": "String", "set": "SetValue", "get": "GetValue"},
    "Bool":      {"cpp": "bool",   "act": "Bool",      "set": "SetValue", "get": "GetValue"},
    "Shape":     {"cpp": "TopoDS_Shape", "act": "Shape", "set": "SetShape", "get": "GetShape"},
    "RealArray": {"cpp": "Handle(TColStd_HArray1OfReal)", "act": "RealArray", "set": "SetArray", "get": "GetArray"},
    "enum":      {"cpp": "TCollection_AsciiString", "act": "String", "set": "SetValue", "get": "GetValue"},
    "Reference": {"cpp": "Handle(ActData_BaseNode)", "act": "Reference", "set": "SetTargetNode", "get": "GetTargetNode"},
    "GUID":      {"cpp": "TCollection_AsciiString", "act": "String", "set": "SetValue", "get": "GetValue"}
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
            # Skip metadata nodes
            if entity_name in ["DataTypes", "PropertySetDefinitions", "ObjectTypes"]:
                continue
                
            print(f"Generating code for: {entity_name}")
            
            # Prepare context
            parent_name = entity_data.get("parent")
            parent_class = f"DataNode_{parent_name}" if parent_name else "ActData_BaseNode"
            
            ctx = {
                "class_name": f"DataNode_{entity_name}",
                "parent_class": parent_class,
                "all_entity_names": [n for n in all_entity_names if n not in ["DataTypes", "PropertySetDefinitions", "ObjectTypes"]],
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

    # 2. Render Main Model Class
    model_tmpl = env.get_template("model_h.jinja2")
    all_schemas = {}
    pset_defs = {}
    object_types = {}
    
    for filename in os.listdir(SCHEMA_DIR):
        if filename.endswith(".yaml"):
            with open(os.path.join(SCHEMA_DIR, filename), "r", encoding="utf-8") as f:
                data = yaml.safe_load(f)
                all_schemas.update(data)
                if "PropertySetDefinitions" in data:
                    pset_defs.update(data["PropertySetDefinitions"])
                if "ObjectTypes" in data:
                    object_types.update(data["ObjectTypes"])
    
    model_content = model_tmpl.render(schema=all_schemas)
    with open(os.path.join(OUTPUT_DIR, "DataModel.h"), "w", encoding="utf-8") as mf:
        mf.write(model_content)

    # 3. Render Data Factory Class
    factory_h_tmpl = env.get_template("factory_h.jinja2")
    factory_cpp_tmpl = env.get_template("factory_cpp.jinja2")

    f_ctx = {
        "pset_defs": pset_defs,
        "object_types": object_types
    }

    with open(os.path.join(OUTPUT_DIR, "DataFactory.h"), "w", encoding="utf-8") as fh:
        fh.write(factory_h_tmpl.render(f_ctx))
    with open(os.path.join(OUTPUT_DIR, "DataFactory.cpp"), "w", encoding="utf-8") as fc:
        fc.write(factory_cpp_tmpl.render(f_ctx))

if __name__ == "__main__":
    generate_code()
    print("\nMDA Generation Complete! Files are in 'generated' folder.")
