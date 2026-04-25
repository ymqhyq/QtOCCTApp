#ifndef DataModel_HeaderFile
#define DataModel_HeaderFile

#include <ActData_BaseModel.h>
#include <ActData_BaseNode.h>
#include <TDF_Label.h>

#include "DataNode_GeometricDefinition.h"

#include "DataNode_Property.h"

#include "DataNode_PropertySet.h"

#include "DataNode_ActiveObject.h"

/**
 * @brief 自动生成的业务模型管理类
 */
class DataModel : public ActData_BaseModel {
public:
  DEFINE_STANDARD_RTTI_EXT(DataModel, ActData_BaseModel)

public:
  // --- 数据分区 ID (基于 YAML 定义) ---
  enum PartitionID {

    PID_GeometryDefinitions = 1, // 几何与属性库

    PID_Topology = 2, // 装配与结构树

    PID_Resources = 3, // 材质与资源

    PID_Auxiliary = 4, // 辅助参考

    PID_Results = 5, // 分析结果

  };

  DataModel() : ActData_BaseModel() {}

  /**
   * @brief 初始化物理分区
   * 在根 Label 下创建各个分区的骨架
   */
  void InitPartitions() {
    TDF_Label rootLab = this->GetRootNode()->GetLabel();

    rootLab.FindChild(PID_GeometryDefinitions, Standard_True)
        .SetTag(PID_GeometryDefinitions);

    rootLab.FindChild(PID_Topology, Standard_True).SetTag(PID_Topology);

    rootLab.FindChild(PID_Resources, Standard_True).SetTag(PID_Resources);

    rootLab.FindChild(PID_Auxiliary, Standard_True).SetTag(PID_Auxiliary);

    rootLab.FindChild(PID_Results, Standard_True).SetTag(PID_Results);
  }

  /**
   * @brief 获取指定分区的根 Label
   */
  TDF_Label GetPartitionLabel(PartitionID pid) const {
    return this->GetRootNode()->GetLabel().FindChild(pid, Standard_False);
  }

  // --- 工厂辅助方法 (MDA Generated) ---

  Handle(DataNode_GeometricDefinition) AddGeometricDefinition() {
    return Handle(DataNode_GeometricDefinition)::DownCast(
        this->NewNode(DataNode_GeometricDefinition::Instance()));
  }

  Handle(DataNode_Property) AddProperty() {
    return Handle(DataNode_Property)::DownCast(
        this->NewNode(DataNode_Property::Instance()));
  }

  Handle(DataNode_PropertySet) AddPropertySet() {
    return Handle(DataNode_PropertySet)::DownCast(
        this->NewNode(DataNode_PropertySet::Instance()));
  }

  Handle(DataNode_ActiveObject) AddActiveObject() {
    return Handle(DataNode_ActiveObject)::DownCast(
        this->NewNode(DataNode_ActiveObject::Instance()));
  }
};

#endif