import json

with open("D:\\QtOCCTApp\\core-data-model\\bridge_data.json", "r", encoding="utf-8") as f:
    data = json.load(f)

pbr_concrete = { "BaseColor": [0.6, 0.6, 0.6], "Metallic": 0.1, "Roughness": 0.8 }
pbr_iron = { "BaseColor": [0.3, 0.3, 0.35], "Metallic": 0.9, "Roughness": 0.3 }

def assign_pbr(node):
    obj_type = node.get("ObjectType", "")
    
    if obj_type in ["Girder", "PierTray", "PierBody", "PileCap", "SinglePile"]:
        if "PropertySets" not in node:
            node["PropertySets"] = {}
        node["PropertySets"]["Pset_MaterialPBR"] = pbr_concrete
    elif obj_type == "Bearing":
        if "PropertySets" not in node:
            node["PropertySets"] = {}
        node["PropertySets"]["Pset_MaterialPBR"] = pbr_iron
        
    if "children" in node:
        for child in node["children"]:
            assign_pbr(child)

assign_pbr(data["bridge"])

with open("D:\\QtOCCTApp\\core-data-model\\bridge_data.json", "w", encoding="utf-8") as f:
    json.dump(data, f, indent=2, ensure_ascii=False)
