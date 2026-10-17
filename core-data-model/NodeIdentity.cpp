#include "NodeIdentity.h"

#include "generated/BrNode_adRoot.h"
#include "generated/BrNode_adSlopeIndication.h"

#include <ActAPI_IPartition.h>
#include <ActData_BasePartition.h>
#include <Standard_Failure.hxx>
#include <TDataStd_AsciiString.hxx>
#include <TDF_ChildIterator.hxx>
#include <TDocStd_Document.hxx>
#include <TCollection_AsciiString.hxx>

#include <map>
#include <string>

#include <chrono>
#include <cstdio>
#include <iostream>
#include <random>

namespace {

std::mt19937_64 &guidEngine() {
  static std::mt19937_64 engine([] {
    std::random_device device;
    const auto now = static_cast<unsigned long long>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    std::seed_seq seed{device(), device(), static_cast<unsigned>(now),
                       static_cast<unsigned>(now >> 32)};
    return std::mt19937_64(seed);
  }());
  return engine;
}

bool looksLikeStandardGuid(const TCollection_ExtendedString &value) {
  if (value.Length() != 32) {
    return false;
  }
  for (Standard_Integer i = 1; i <= value.Length(); ++i) {
    const auto c = value.Value(i);
    const bool isDigit = (c >= u'0' && c <= u'9');
    const bool isUpper = (c >= u'A' && c <= u'F');
    const bool isLower = (c >= u'a' && c <= u'f');
    if (!isDigit && !isUpper && !isLower) {
      return false;
    }
  }
  return true;
}

/// 把模型内所有节点挂到回调上（节点 + 所属模型下标）
template <typename Fn>
void forEachNode(const std::vector<Handle(DataModel)> &models, Fn &&fn) {
  for (std::size_t modelIndex = 0; modelIndex < models.size(); ++modelIndex) {
    const Handle(DataModel) &model = models[modelIndex];
    if (model.IsNull()) {
      continue;
    }
    for (int partIndex = 1; partIndex <= 8; ++partIndex) {
      Handle(ActAPI_IPartition) partition;
      try {
        partition = model->Partition(partIndex);
      } catch (...) {
        continue;
      }
      if (partition.IsNull()) {
        continue;
      }
      for (ActData_BasePartition::Iterator it(partition); it.More(); it.Next()) {
        Handle(BrNode_adRoot) node = Handle(BrNode_adRoot)::DownCast(it.Value());
        if (!node.IsNull()) {
          fn(model, node, modelIndex);
        }
      }
    }
  }
}

} // namespace

TCollection_ExtendedString NodeIdentity::NewGuid() {
  const unsigned long long hi = guidEngine()();
  const unsigned long long lo = guidEngine()();
  char buffer[33] = {0};
  std::snprintf(buffer, sizeof(buffer), "%016llX%016llX", hi, lo);
  return TCollection_ExtendedString(buffer, Standard_True);
}

int NodeIdentity::EnsureGlobalIds(const Handle(DataModel) &model) {
  if (model.IsNull()) {
    return 0;
  }

  // 用事务包住补号，保证撤销/合并语义一致；若外部已开启事务则复用
  bool commandOpened = false;
  try {
    model->OpenCommand();
    commandOpened = true;
  } catch (...) {
    commandOpened = false;
  }

  int assigned = 0;
  try {
    for (int partIndex = 1; partIndex <= 8; ++partIndex) {
      Handle(ActAPI_IPartition) partition;
      try {
        partition = model->Partition(partIndex);
      } catch (...) {
        continue; // 未注册的分区编号会抛异常
      }
      if (partition.IsNull()) {
        continue;
      }

      for (ActData_BasePartition::Iterator it(partition); it.More(); it.Next()) {
        Handle(BrNode_adRoot) node = Handle(BrNode_adRoot)::DownCast(it.Value());
        if (node.IsNull()) {
          continue;
        }
        if (!node->GetGlobalID().IsEmpty()) {
          continue;
        }
        node->SetGlobalID(NewGuid());
        ++assigned;
      }
    }
  } catch (const Standard_Failure &failure) {
    std::cerr << "[NodeIdentity] 补号过程发生 OCCT 异常: "
              << failure.GetMessageString() << std::endl;
  } catch (...) {
    std::cerr << "[NodeIdentity] 补号过程发生未知异常" << std::endl;
  }

  if (commandOpened) {
    model->CommitCommand();
  }
  return assigned;
}

int NodeIdentity::NormalizeGlobalIds(const std::vector<Handle(DataModel)> &models) {
  // 1. 先建立“旧 GUID -> 新 GUID”映射（只针对格式不规范的已有 GUID）
  std::map<std::string, std::string> guidMap;
  forEachNode(models, [&](const Handle(DataModel) &, const Handle(BrNode_adRoot) &node,
                          std::size_t) {
    const TCollection_ExtendedString current = node->GetGlobalID();
    if (current.IsEmpty() || looksLikeStandardGuid(current)) {
      return;
    }
    const std::string oldKey = TCollection_AsciiString(current, Standard_True).ToCString();
    if (guidMap.find(oldKey) == guidMap.end()) {
      guidMap[oldKey] = TCollection_AsciiString(NewGuid(), Standard_True).ToCString();
    }
  });

  if (guidMap.empty()) {
    // 没有需要改格式的 GUID，但仍补齐空 GUID
    int filled = 0;
    for (const Handle(DataModel) &model : models) {
      filled += EnsureGlobalIds(model);
    }
    return filled;
  }

  // 2. 逐模型：在同一个事务内完成“改名 + 引用更新”。
  //    OCAF 要求属性修改必须处于事务中，否则会抛
  //    "Attribute ... is changed outside transaction"（此前正是这样崩溃的）。
  int changed = 0;
  for (const Handle(DataModel) &model : models) {
    if (model.IsNull()) {
      continue;
    }
    bool commandOpened = false;
    try {
      model->OpenCommand();
      commandOpened = true;
    } catch (...) {
      commandOpened = false;
    }

    try {
      // 2.1 GUID 改名（同时补齐空 GUID）
      forEachNode({model}, [&](const Handle(DataModel) &, const Handle(BrNode_adRoot) &node,
                               std::size_t) {
        const TCollection_ExtendedString current = node->GetGlobalID();
        if (current.IsEmpty()) {
          node->SetGlobalID(NewGuid());
          ++changed;
          return;
        }
        if (looksLikeStandardGuid(current)) {
          return;
        }
        const std::string key =
            TCollection_AsciiString(current, Standard_True).ToCString();
        const auto found = guidMap.find(key);
        if (found != guidMap.end()) {
          node->SetGlobalID(
              TCollection_ExtendedString(found->second.c_str(), Standard_True));
          ++changed;
        }
      });

      // 2.2 业务节点上的显式引用（示坡线 -> 目标 3D 对象）
      forEachNode({model}, [&](const Handle(DataModel) &, const Handle(BrNode_adRoot) &node,
                               std::size_t) {
        Handle(BrNode_adSlopeIndication) indication =
            Handle(BrNode_adSlopeIndication)::DownCast(node);
        if (indication.IsNull()) {
          return;
        }
        const std::string target =
            TCollection_AsciiString(indication->GetTargetObjectID(), Standard_True)
                .ToCString();
        const auto found = guidMap.find(target);
        if (found != guidMap.end()) {
          indication->SetTargetObjectID(
              TCollection_ExtendedString(found->second.c_str(), Standard_True));
        }
      });

      // 2.3 图纸 XCAF 标签上的 AsciiString（2D 图元 -> 3D 节点）
      Handle(ActAPI_INode) rootNode = model->GetRootNode();
      if (!rootNode.IsNull()) {
        Handle(TDocStd_Document) document =
            TDocStd_Document::Get(rootNode->RootLabel());
        if (!document.IsNull()) {
          for (TDF_ChildIterator it(document->Main(), Standard_True); it.More();
               it.Next()) {
            Handle(TDataStd_AsciiString) attr;
            if (!it.Value().FindAttribute(TDataStd_AsciiString::GetID(), attr)) {
              continue;
            }
            const auto found = guidMap.find(attr->Get().ToCString());
            if (found != guidMap.end()) {
              attr->Set(TCollection_AsciiString(found->second.c_str()));
            }
          }
        }
      }
    } catch (...) {
      if (commandOpened) {
        model->AbortCommand();
      }
      throw; // 由调用方转换成界面上的错误提示
    }

    if (commandOpened) {
      model->CommitCommand();
    }
  }

  return changed;
}
