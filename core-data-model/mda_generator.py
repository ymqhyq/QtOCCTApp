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
    "Real":      {"cpp": "double", "act": "Real",      "set": "SetValue", "get": "GetValue", "id": "Parameter_Real"},
    "Int":       {"cpp": "int",    "act": "Int",       "set": "SetValue", "get": "GetValue", "id": "Parameter_Int"},
    "String":    {"cpp": "TCollection_ExtendedString", "act": "Name", "set": "SetValue", "get": "GetValue", "id": "Parameter_Name"},
    "Bool":      {"cpp": "bool",   "act": "Bool",      "set": "SetValue", "get": "GetValue", "id": "Parameter_Bool"},
    "Shape":     {"cpp": "TopoDS_Shape", "act": "Shape", "set": "SetShape", "get": "GetShape", "id": "Parameter_Shape"},
    "RealArray": {"cpp": "Handle(TColStd_HArray1OfReal)", "act": "RealArray", "set": "SetArray", "get": "GetArray", "id": "Parameter_RealArray"},
    "enum":      {"cpp": "TCollection_ExtendedString", "act": "Name", "set": "SetValue", "get": "GetValue", "id": "Parameter_Name"},
    "Reference": {"cpp": "Handle(ActAPI_IDataCursor)", "act": "Reference", "set": "SetTarget", "get": "GetTarget", "id": "Parameter_Reference"},
    "BrEntityRef": {"cpp": "Handle(ActAPI_IDataCursor)", "act": "Reference", "set": "SetTarget", "get": "GetTarget", "id": "Parameter_Reference"},
    "GUID":      {"cpp": "TCollection_ExtendedString", "act": "Name", "set": "SetValue", "get": "GetValue", "id": "Parameter_Name"}
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
    meta_keys = ["DataTypes", "PropertySetDefinitions", "ObjectTypes", "Partitions", "ClassificationDefinitions"]
    entities_to_gen = {k: v for k, v in all_schemas.items() if k not in meta_keys}
    
    all_node_names = list(entities_to_gen.keys())
    
    for entity_name, entity_data in entities_to_gen.items():
        print(f"Generating code for: {entity_name}")
        
        # Determine parent class: support 'base' or 'parent', default to ActData_BaseNode
        parent_class = entity_data.get("base", entity_data.get("parent", "ActData_BaseNode"))
        if parent_class in all_node_names:
            parent_class = f"BrNode_{parent_class}"

        ctx = {
            "name": entity_data.get("name", entity_name),
            "class_name": f"BrNode_{entity_name}",
            "parent_class": parent_class,
            "attributes": {},
            "children": {},
            "all_entity_names": all_node_names
        }
        
        attrs = entity_data.get("attributes", {})
        if attrs:
            for attr_name, attr_info in attrs.items():
                attr_type = attr_info["type"]
                # If type is another node class, treat it as a Reference
                if attr_type in all_node_names:
                    t_info = TYPE_MAP["Reference"].copy()
                else:
                    t_info = TYPE_MAP.get(attr_type, TYPE_MAP["Real"]).copy()
                
                ctx["attributes"][attr_name] = {
                    "cpp_type": t_info["cpp"],
                    "act_type": t_info["act"],
                    "set_method": t_info["set"],
                    "get_method": t_info["get"],
                    "id": t_info["id"],
                    "label": attr_info.get("label", attr_name)
                }

        children = entity_data.get("children", {})
        if children:
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
