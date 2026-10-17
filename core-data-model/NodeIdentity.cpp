#include "NodeIdentity.h"

#include "generated/BrNode_adRoot.h"

#include <ActAPI_IPartition.h>
#include <ActData_BasePartition.h>
#include <Standard_Failure.hxx>

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
