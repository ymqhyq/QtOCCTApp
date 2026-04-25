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
    "String":    {"cpp": "TCollection_AsciiString", "act": "AsciiString", "set": "SetValue", "get": "GetValue"},
    "Bool":      {"cpp": "bool",   "act": "Bool",      "set": "SetValue", "get": "GetValue"},
    "Shape":     {"cpp": "TopoDS_Shape", "act": "Shape", "set": "SetShape", "get": "GetShape"},
    "RealArray": {"cpp": "Handle(TColStd_HArray1OfReal)", "act": "RealArray", "set": "SetArray", "get": "GetArray"},
    "enum":      {"cpp": "TCollection_AsciiString", "act": "AsciiString", "set": "SetValue", "get": "GetValue"},
    "Reference": {"cpp": "Handle(ActAPI_IDataCursor)", "act": "Reference", "set": "SetTarget", "get": "GetTarget"},
    "BrEntityRef": {"cpp": "Handle(ActAPI_IDataCursor)", "act": "Reference", "set": "SetTarget", "get": "GetTarget"},
    "GUID":      {"cpp": "TCollection_AsciiString", "act": "AsciiString", "set": "SetValue", "get": "GetValue"}
}

def generate_code():
    # Setup Jinja2
    env = Environment(loader=FileSystemLoader(TEMPLATE_DIR), extensions=['jinja2.ext.do'])
    header_tmpl = env.get_template("node_h.jinja2")
    cpp_tmpl = env.get_template("node_cpp.jinja2")

    if not os.path.exists(OUTPUT_DIR):
        os.makedirs(OUTPUT_DIR)

    # 1. Collect all schemas
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

    # 2. Render Main Model Class
    model_tmpl = env.get_template("model_h.jinja2")
    model_cpp_tmpl = env.get_template("model_cpp.jinja2")
    
    with open(os.path.join(OUTPUT_DIR, "DataModel.h"), "w", encoding="utf-8-sig") as mf:
        mf.write(model_tmpl.render(schema=all_schemas))
    with open(os.path.join(OUTPUT_DIR, "DataModel.cpp"), "w", encoding="utf-8-sig") as mcf:
        mcf.write(model_cpp_tmpl.render(schema=all_schemas))

    # 3. Render Data Factory Class
    factory_h_tmpl = env.get_template("factory_h.jinja2")
    factory_cpp_tmpl = env.get_template("factory_cpp.jinja2")
    
    with open(os.path.join(OUTPUT_DIR, "DataFactory.h"), "w", encoding="utf-8-sig") as fh:
        fh.write(factory_h_tmpl.render(schema=all_schemas, pset_defs=pset_defs, object_types=object_types))
    with open(os.path.join(OUTPUT_DIR, "DataFactory.cpp"), "w", encoding="utf-8-sig") as fcpp:
        fcpp.write(factory_cpp_tmpl.render(schema=all_schemas, pset_defs=pset_defs, object_types=object_types))

    # 4. Render Node Classes
    for entity_name, entity_data in all_schemas.items():
        if entity_name in ["DataTypes", "PropertySetDefinitions", "ObjectTypes", "Partitions"]:
            continue
            
        print(f"Generating code for: {entity_name}")
        
        ctx = {
            "name": entity_data.get("label", entity_name),
            "class_name": f"BrNode_{entity_name}",
            "parent_class": entity_data.get("parent", "ActData_BaseNode"),
            "attributes": {},
            "children": {},
            "all_entity_names": [e for e in all_schemas.keys() if e not in ["DataTypes", "PropertySetDefinitions", "ObjectTypes", "Partitions"]]
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
                "class_name": f"BrNode_{child_info['type']}",
                "cardinality": child_info.get("cardinality", "1")
            }
        
        # Write files
        with open(os.path.join(OUTPUT_DIR, f"BrNode_{entity_name}.h"), "w", encoding="utf-8-sig") as hf:
            hf.write(header_tmpl.render(**ctx))
        with open(os.path.join(OUTPUT_DIR, f"BrNode_{entity_name}.cpp"), "w", encoding="utf-8-sig") as cf:
            cf.write(cpp_tmpl.render(**ctx))

if __name__ == "__main__":
    generate_code()
    print("\nMDA Generation Complete! Files are in 'generated' folder.")
