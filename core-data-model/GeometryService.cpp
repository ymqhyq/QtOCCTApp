#include "GeometryService.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

// OCCT includes
#include <ActData_BasePartition.h>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <TCollection_AsciiString.hxx>
#include <TDF_ChildIterator.hxx>
#include <TDF_Label.hxx>
#include <TDF_Tool.hxx>
#include <gp_Ax1.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

// Windows HTTP (WinHTTP) - 轻量级 HTTP 客户端
// 以后迁移为微服务时，此部分可替换为 gRPC/Dapr SDK
#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

// MD5 实现 (轻量级，无外部依赖)
#include <functional>

// ============================================================
// 辅助工具函数
// ============================================================

static std::string ToStdString(const TCollection_ExtendedString &es) {
  TCollection_AsciiString as(es);
  return std::string(as.ToCString());
}

static TCollection_ExtendedString ToExtString(const std::string &s) {
  return TCollection_ExtendedString(s.c_str(), Standard_True); // isMultiByte
}

// 简单 MD5 实现 (RFC 1321)
namespace {
// --- 精简 MD5 ---
struct MD5Context {
  uint32_t state[4];
  uint64_t count;
  uint8_t buffer[64];
};

static const uint32_t S[64] = {
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
    5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
    6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};

static const uint32_t K[64] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a,
    0xa8304613, 0xfd469501, 0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
    0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821, 0xf61e2562, 0xc040b340,
    0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8,
    0x676f02d9, 0x8d2a4c8a, 0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
    0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70, 0x289b7ec6, 0xeaa127fa,
    0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92,
    0xffeff47d, 0x85845dd1, 0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
    0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};

static uint32_t leftRotate(uint32_t x, uint32_t c) {
  return (x << c) | (x >> (32 - c));
}

static void md5Transform(uint32_t state[4], const uint8_t block[64]) {
  uint32_t M[16];
  for (int i = 0; i < 16; ++i)
    M[i] = (uint32_t)block[i * 4] | ((uint32_t)block[i * 4 + 1] << 8) |
           ((uint32_t)block[i * 4 + 2] << 16) |
           ((uint32_t)block[i * 4 + 3] << 24);
  uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
  for (int i = 0; i < 64; ++i) {
    uint32_t f, g;
    if (i < 16) {
      f = (b & c) | (~b & d);
      g = i;
    } else if (i < 32) {
      f = (d & b) | (~d & c);
      g = (5 * i + 1) % 16;
    } else if (i < 48) {
      f = b ^ c ^ d;
      g = (3 * i + 5) % 16;
    } else {
      f = c ^ (b | ~d);
      g = (7 * i) % 16;
    }
    f += a + K[i] + M[g];
    a = d;
    d = c;
    c = b;
    b += leftRotate(f, S[i]);
  }
  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
}

static std::string md5(const std::string &input) {
  MD5Context ctx;
  ctx.state[0] = 0x67452301;
  ctx.state[1] = 0xefcdab89;
  ctx.state[2] = 0x98badcfe;
  ctx.state[3] = 0x10325476;
  ctx.count = 0;

  const uint8_t *data = reinterpret_cast<const uint8_t *>(input.data());
  size_t len = input.size();
  size_t offset = 0;

  // Process full blocks
  while (offset + 64 <= len) {
    md5Transform(ctx.state, data + offset);
    offset += 64;
  }

  // Final block with padding
  uint8_t finalBlock[128] = {0};
  size_t remaining = len - offset;
  memcpy(finalBlock, data + offset, remaining);
  finalBlock[remaining] = 0x80;

  size_t padLen = (remaining < 56) ? 64 : 128;
  uint64_t bitLen = (uint64_t)len * 8;
  memcpy(finalBlock + padLen - 8, &bitLen, 8);

  md5Transform(ctx.state, finalBlock);
  if (padLen == 128)
    md5Transform(ctx.state, finalBlock + 64);

  // Format as hex
  std::ostringstream oss;
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      oss << std::hex << std::setfill('0') << std::setw(2)
          << ((ctx.state[i] >> (j * 8)) & 0xff);
    }
  }
  return oss.str();
}
} // anonymous namespace

// ============================================================
// HTTP 客户端 (WinHTTP)
// ============================================================

#ifdef _WIN32
static std::string
HttpPost(const std::string &host, int port, const std::string &path,
         const std::string &body,
         const std::string &contentType = "application/json") {
  std::string result;

  HINTERNET hSession =
      WinHttpOpen(L"GeometryService/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                  WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!hSession)
    return "";

  std::wstring wHost(host.begin(), host.end());
  HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), port, 0);
  if (!hConnect) {
    WinHttpCloseHandle(hSession);
    return "";
  }

  std::wstring wPath(path.begin(), path.end());
  HINTERNET hRequest =
      WinHttpOpenRequest(hConnect, L"POST", wPath.c_str(), NULL,
                         WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
  if (!hRequest) {
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return "";
  }

  // 设置较短的超时时间 (2秒)，避免在服务不可用时加载过慢
  WinHttpSetTimeouts(hRequest, 2000, 2000, 2000, 2000);

  std::wstring wContentType(contentType.begin(), contentType.end());
  std::wstring headers = L"Content-Type: " + wContentType;

  BOOL bResults = WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)-1,
                                     (LPVOID)body.c_str(), (DWORD)body.size(),
                                     (DWORD)body.size(), 0);
  if (bResults)
    bResults = WinHttpReceiveResponse(hRequest, NULL);

  if (bResults) {
    DWORD dwSize = 0;
    do {
      dwSize = 0;
      WinHttpQueryDataAvailable(hRequest, &dwSize);
      if (dwSize > 0) {
        std::vector<char> buf(dwSize);
        DWORD dwDownloaded = 0;
        WinHttpReadData(hRequest, buf.data(), dwSize, &dwDownloaded);
        result.append(buf.data(), dwDownloaded);
      }
    } while (dwSize > 0);
  }

  WinHttpCloseHandle(hRequest);
  WinHttpCloseHandle(hConnect);
  WinHttpCloseHandle(hSession);
  return result;
}
#endif

// ============================================================
// GeometryService 实现
// ============================================================

GeometryService::GeometryService(const Handle(DataModel) & model,
                                 const std::string &serviceUrl)
    : m_model(model), m_serviceUrl(serviceUrl) {}

// ------ 提取几何参数 ------
GeometryService::ExtractedParams
GeometryService::ExtractGeoParams(const Handle(BrNode_adObject) & adObj) {
  ExtractedParams result;
  result.modelType = ToStdString(adObj->GetObjectType());

  // 遍历属性集，找到名称含 "Geometry" 的 Pset
  NCollection_Sequence<Handle(BrNode_adPropertySet)> psets =
      adObj->GetPropertySetsList();
  for (int i = 1; i <= psets.Length(); ++i) {
    Handle(BrNode_adPropertySet) ps = psets.Value(i);
    if (ps.IsNull())
      continue;

    std::string psName = ToStdString(ps->GetName());
    if (psName.find("Geometry") == std::string::npos)
      continue;

    std::cout << "[GeometryService] Found Geometry PSet: " << psName << " for "
              << result.modelType << std::endl;
    result.geoPset = ps;

    // 提取所有 Property 为 JSON
    NCollection_Sequence<Handle(BrNode_adProperty)> props =
        ps->GetPropertiesList();
    for (int j = 1; j <= props.Length(); ++j) {
      Handle(BrNode_adProperty) p = props.Value(j);
      if (p.IsNull())
        continue;

      std::string key = ToStdString(p->GetPropertyName());
      std::string val = ToStdString(p->GetPropertyValue());
      std::string vtype = ToStdString(p->GetValueType());

      if (key == "ModelNumber") {
        result.modelNumber = val;
      }

      // 根据值类型转换
      if (vtype == "Real") {
        try {
          result.params[key] = std::stod(val);
        } catch (...) {
          result.params[key] = val;
        }
      } else if (vtype == "Int") {
        try {
          result.params[key] = std::stoi(val);
        } catch (...) {
          result.params[key] = val;
        }
      } else {
        result.params[key] = val;
      }
    }
    break; // 只取第一个 Geometry Pset
  }

  return result;
}

// ------ 计算参数指纹 ------
std::string GeometryService::ComputeParamGeoID(const std::string &modelType,
                                               const json &params) {
  // 排序后序列化，确保相同参数产生相同 MD5
  json sorted = params;
  std::string input =
      modelType + "|" +
      sorted.dump(-1, ' ', false, json::error_handler_t::replace);
  return md5(input);
}

// ------ 查找缓存的几何定义 ------
Handle(BrNode_adGeometricDef)
    GeometryService::FindCachedGeoDef(const std::string &paramGeoId) {
  // 获取几何定义分区
  Handle(ActAPI_IPartition) partition =
      m_model->Partition(DataModel::PID_GeometryDefinitions);
  if (partition.IsNull())
    return nullptr;

  // 使用分区的迭代器遍历节点
  Handle(ActData_BasePartition) basePart =
      Handle(ActData_BasePartition)::DownCast(partition);
  if (!basePart.IsNull()) {
    for (ActData_BasePartition::Iterator it(basePart); it.More(); it.Next()) {
      Handle(BrNode_adGeometricDef) gd =
          Handle(BrNode_adGeometricDef)::DownCast(it.Value());
      if (gd.IsNull())
        continue;

      std::string existingId = ToStdString(gd->GetParamGeoID());
      if (existingId == paramGeoId) {
        std::cout << "[GeometryService] Cache HIT: " << paramGeoId << std::endl;
        return gd;
      }
    }
  }
  return nullptr;
}

// ------ 调用建模服务 ------
GeometryService::ServiceResult
GeometryService::CallModelingService(const std::string &modelType,
                                     const json &params) {
  ServiceResult result;
  result.success = false;

  // 构造请求体
  json requestBody;
  requestBody["model_type"] = modelType;
  requestBody["args"] = params;
  requestBody["code"] = "";       // 由服务端根据 model_type 查找脚本
  requestBody["format"] = "brep"; // 直接获取 BREP

  std::string bodyStr = requestBody.dump();

  std::cout << "[GeometryService] Calling service: " << m_serviceUrl
            << " modelType=" << modelType << std::endl;

#ifdef _WIN32
  std::string host = "127.0.0.1";
  int port = 8000;
  std::string path = "/api/v1/model/generate";

  // 从 m_serviceUrl 中提取 host, port 和 path
  {
    std::string url = m_serviceUrl;
    if (url.find("http://") == 0)
      url = url.substr(7);

    size_t slashPos = url.find('/');
    if (slashPos != std::string::npos) {
      std::string basePath = url.substr(slashPos);
      if (!basePath.empty() && basePath.back() == '/') {
        basePath.pop_back();
      }
      path = basePath + path;
      url = url.substr(0, slashPos);
    }

    size_t colonPos = url.find(':');
    if (colonPos != std::string::npos) {
      host = url.substr(0, colonPos);
      try {
        port = std::stoi(url.substr(colonPos + 1));
      } catch (...) {
      }
    } else {
      host = url;
      port = 80;
    }
  }

  std::string response = HttpPost(host, port, path, bodyStr);
  if (response.empty()) {
    result.error = "HTTP request failed or empty response";
    return result;
  }

  // 解析 JHB 响应: [4字节JSON长度][JSON元数据][BREP数据]
  if (response.size() < 4) {
    result.error = "Response too short";
    return result;
  }

  uint32_t jsonLen = 0;
  memcpy(&jsonLen, response.data(), 4);

  if (4 + jsonLen > response.size()) {
    result.error = "Invalid JHB header";
    return result;
  }

  std::string jsonStr(response.data() + 4, jsonLen);
  result.brepData =
      std::string(response.data() + 4 + jsonLen, response.size() - 4 - jsonLen);

  try {
    json metadata = json::parse(jsonStr);
    result.allParams = metadata.value("args", json::object());
    result.success = true;
  } catch (const json::exception &e) {
    result.error = std::string("JSON parse error: ") + e.what();
  }
#else
  result.error = "HTTP client not implemented for this platform";
#endif

  return result;
}

// ------ 解析 BREP ------
TopoDS_Shape GeometryService::ParseBREP(const std::string &brepData) {
  TopoDS_Shape shape;
  BRep_Builder builder;

  std::istringstream iss(brepData);
  BRepTools::Read(shape, iss, builder);

  return shape;
}

// ------ 创建几何定义节点 ------
Handle(BrNode_adGeometricDef) GeometryService::CreateGeoDef(
    const std::string &paramGeoId, const json &allParams,
    const TopoDS_Shape &shape) {
  Handle(BrNode_adGeometricDef) geoDef = m_model->AddadGeometricDef();
  if (geoDef.IsNull())
    return nullptr;

  geoDef->SetName(ToExtString(paramGeoId.substr(0, 8))); // 短名
  geoDef->SetParamGeoID(ToExtString(paramGeoId));
  geoDef->SetGeoParameter(ToExtString(allParams.dump()));
  geoDef->SetShape(shape);

  std::cout << "[GeometryService] Created GeoDef: " << paramGeoId << std::endl;
  return geoDef;
}

// ------ 回写参数 ------
void GeometryService::WriteBackParams(const Handle(BrNode_adPropertySet) &
                                          geoPset,
                                      const json &returnedParams) {
  if (geoPset.IsNull())
    return;

  NCollection_Sequence<Handle(BrNode_adProperty)> props =
      geoPset->GetPropertiesList();
  for (int j = 1; j <= props.Length(); ++j) {
    Handle(BrNode_adProperty) p = props.Value(j);
    if (p.IsNull())
      continue;

    std::string key = ToStdString(p->GetPropertyName());

    // 如果 scripts-service 返回了该参数的新值，回写
    if (returnedParams.contains(key)) {
      const auto &val = returnedParams[key];
      std::string strVal;
      if (val.is_number_float())
        strVal = std::to_string(val.get<double>());
      else if (val.is_number_integer())
        strVal = std::to_string(val.get<int>());
      else
        strVal = val.get<std::string>();

      p->SetPropertyValue(ToExtString(strVal));
    }
  }
}

// ------ 核心接口: 从 adObject 构建几何 ------
Handle(BrNode_adGeometricDef)
    GeometryService::BuildGeometry(const Handle(BrNode_adObject) & adObj) {
  if (adObj.IsNull())
    return nullptr;

  // 0. 离线加载优化：优先检查对象是否已经关联了有效的几何定义
  Handle(BrNode_adGeometry) existingGeom =
      Handle(BrNode_adGeometry)::DownCast(adObj->GetGeometry());
  if (!existingGeom.IsNull()) {
    Handle(BrNode_adGeometricDef) linkedDef =
        Handle(BrNode_adGeometricDef)::DownCast(existingGeom->GetGeometryRef());
    if (!linkedDef.IsNull() && !linkedDef->GetShape().IsNull()) {
      return linkedDef;
    }
  }

  // 1. 提取参数
  ExtractedParams ep = ExtractGeoParams(adObj);
  if (ep.geoPset.IsNull()) {
    std::cout << "[GeometryService] Skip building: No Geometry Pset found for ["
              << ep.modelType << "] " << ToStdString(adObj->GetName())
              << std::endl;
    return nullptr;
  }

  std::cout << "[GeometryService] BuildGeometry for [" << ep.modelType << "] "
            << ToStdString(adObj->GetName()) << std::endl;

  // 2. 计算指纹
  std::string paramGeoId = ComputeParamGeoID(ep.modelType, ep.params);

  // 3. 查找缓存
  Handle(BrNode_adGeometricDef) cached = FindCachedGeoDef(paramGeoId);
  if (!cached.IsNull()) {
    // 建立引用关系
    return cached;
  }

  // 4. 调用建模服务
  std::cout << "[GeometryService] Cache MISS, calling service..." << std::endl;
  ServiceResult sr = CallModelingService(ep.modelType, ep.params);
  if (!sr.success) {
    std::cerr << "[GeometryService] Fallback to dummy geometry for "
              << ep.modelType << std::endl;

    // 根据类型创建不同的占位几何
    TopoDS_Shape fallbackShape;
    try {
      if (ep.modelType == "SinglePile" || ep.modelType == "Pile") {
        fallbackShape = BRepPrimAPI_MakeCylinder(500, 5000).Shape();
      } else if (ep.modelType == "Bearing") {
        fallbackShape = BRepPrimAPI_MakeBox(200, 200, 100).Shape();
      } else {
        fallbackShape = BRepPrimAPI_MakeBox(1000, 1000, 1000).Shape();
      }
    } catch (...) {
      fallbackShape = BRepPrimAPI_MakeBox(500, 500, 500).Shape();
    }

    return CreateGeoDef(paramGeoId, ep.params, fallbackShape);
  }

  // 5. 解析 BREP
  TopoDS_Shape shape = ParseBREP(sr.brepData);
  if (shape.IsNull()) {
    std::cerr << "[GeometryService] Failed to parse BREP" << std::endl;
    return nullptr;
  }

  // 6. 创建 adGeometricDef 保存到几何分区
  Handle(BrNode_adGeometricDef) geoDef =
      CreateGeoDef(paramGeoId, sr.allParams, shape);

  // 7. 回写 inout/out 参数
  WriteBackParams(ep.geoPset, sr.allParams);

  return geoDef;
}

// ------ 核心接口: 直接参数建模 ------
GeometryService::BuildResult
GeometryService::BuildGeometryFromParams(const std::string &modelType,
                                         const json &inputParams) {
  BuildResult result;
  result.fromCache = false;

  // 1. 计算指纹
  result.paramGeoId = ComputeParamGeoID(modelType, inputParams);

  // 2. 查找缓存
  Handle(BrNode_adGeometricDef) cached = FindCachedGeoDef(result.paramGeoId);
  if (!cached.IsNull()) {
    result.shape = cached->GetShape();
    try {
      result.allParams = json::parse(ToStdString(cached->GetGeoParameter()));
    } catch (...) {
      result.allParams = inputParams;
    }
    result.fromCache = true;
    return result;
  }

  // 3. 调用建模服务
  ServiceResult sr = CallModelingService(modelType, inputParams);
  if (!sr.success) {
    std::cerr << "[GeometryService] BuildFromParams failed: " << sr.error
              << std::endl;
    return result;
  }

  // 4. 解析几何
  result.shape = ParseBREP(sr.brepData);
  result.allParams = sr.allParams;

  CreateGeoDef(result.paramGeoId, sr.allParams, result.shape);

  return result;
}

// ------ 遍历并构建 ------
void GeometryService::TraverseAndBuild(const Handle(BrNode_adObject) & rootObj,
                                       std::vector<VisualShape> &outShapes,
                                       const gp_Trsf &parentTrsf) {
  if (rootObj.IsNull())
    return;

  // 1. 获取局部变换
  gp_Trsf localTrsf;
  Handle(ActAPI_IUserParameter) p =
      rootObj->Parameter(BrNode_adObject::PID_ObjectPlacement);
  Handle(ActData_RealArrayParameter) typedP =
      ActData_ParameterFactory::AsRealArray(p);

  if (!typedP.IsNull() && typedP->NbElements() >= 3) {
    localTrsf.SetTranslation(gp_Vec(
        typedP->GetElement(0), typedP->GetElement(1), typedP->GetElement(2)));
    if (typedP->NbElements() >= 6) {
      double rx = typedP->GetElement(3);
      double ry = typedP->GetElement(4);
      double rz = typedP->GetElement(5);
      gp_Trsf rot;
      if (std::abs(rz) > 1e-6) {
        gp_Trsf r;
        r.SetRotation(gp_Ax1(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)),
                      rz * 3.14159265358979323846 / 180.0);
        rot.Multiply(r);
      }
      if (std::abs(ry) > 1e-6) {
        gp_Trsf r;
        r.SetRotation(gp_Ax1(gp_Pnt(0, 0, 0), gp_Dir(0, 1, 0)),
                      ry * 3.14159265358979323846 / 180.0);
        rot.Multiply(r);
      }
      if (std::abs(rx) > 1e-6) {
        gp_Trsf r;
        r.SetRotation(gp_Ax1(gp_Pnt(0, 0, 0), gp_Dir(1, 0, 0)),
                      rx * 3.14159265358979323846 / 180.0);
        rot.Multiply(r);
      }
      localTrsf.Multiply(rot);
    }
  }

  gp_Trsf currentTrsf = parentTrsf * localTrsf;

  // 2. 构建当前对象的几何
  Handle(BrNode_adGeometricDef) geoDef = this->BuildGeometry(rootObj);
  if (!geoDef.IsNull()) {
    // --- 建立数据模型中的链接，确保导出服务能找到几何 ---
    if (rootObj->GetGeometry().IsNull()) {
      Handle(BrNode_adGeometry) geoNode = m_model->AddadGeometry();
      if (!geoNode.IsNull()) {
        geoNode->SetGeometryRef(geoDef);
        rootObj->SetGeometry(geoNode);
      }
    }

    TopoDS_Shape shape = geoDef->GetShape();
    if (!shape.IsNull()) {
      VisualShape vs;
      vs.shape = shape;
      vs.name = ToStdString(rootObj->GetName());
      vs.transform = currentTrsf;
      // Extract all PropertySets as metadata
      NCollection_Sequence<Handle(BrNode_adPropertySet)> psets =
          rootObj->GetPropertySetsList();
      for (int i = 1; i <= psets.Length(); ++i) {
        Handle(BrNode_adPropertySet) ps = psets.Value(i);
        if (ps.IsNull())
          continue;
        std::string psName = ToStdString(ps->GetName());
        json psJson;
        NCollection_Sequence<Handle(BrNode_adProperty)> props =
            ps->GetPropertiesList();
        for (int j = 1; j <= props.Length(); ++j) {
          Handle(BrNode_adProperty) p = props.Value(j);
          if (p.IsNull())
            continue;

          std::string key = ToStdString(p->GetPropertyName());
          std::string val = ToStdString(p->GetPropertyValue());

          // Special handling for BaseColor (comma separated string to list)
          if (key == "BaseColor" && psName == "Pset_MaterialPBR") {
            std::stringstream ss(val);
            std::string segment;
            json colorList = json::array();
            while (std::getline(ss, segment, ',')) {
              try {
                colorList.push_back(std::stod(segment));
              } catch (...) {
              }
            }
            psJson[key] = colorList;
          } else {
            // Try to parse as number if possible
            try {
              if (val.find('.') != std::string::npos)
                psJson[key] = std::stod(val);
              else
                psJson[key] = std::stol(val);
            } catch (...) {
              psJson[key] = val;
            }
          }
        }
        vs.metadata[psName] = psJson;
      }
      vs.metadata["_adNodeId"] = rootObj->GetId().ToCString();
      outShapes.push_back(vs);
    }
  }

  // 3. 递归子对象
  NCollection_Sequence<Handle(BrNode_adObject)> children =
      rootObj->GetSubObjectsList();
  for (int i = 1; i <= children.Length(); ++i) {
    TraverseAndBuild(children.Value(i), outShapes, currentTrsf);
  }
}

// ------ 初始化对象 ------
void GeometryService::InitializeObject(const Handle(BrNode_adObject) & adObj,
                                       const std::string &type) {
  if (adObj.IsNull())
    return;
  adObj->SetObjectType(ToExtString(type));

  // 确保有一个 Geometry 属性集
  bool hasGeoPset = false;
  NCollection_Sequence<Handle(BrNode_adPropertySet)> psets =
      adObj->GetPropertySetsList();
  for (int i = 1; i <= psets.Length(); ++i) {
    if (ToStdString(psets.Value(i)->GetName()).find("Geometry") !=
        std::string::npos) {
      hasGeoPset = true;
      break;
    }
  }

  if (!hasGeoPset) {
    // 这里应由工厂或模型创建，暂由模型 AddPropertySet 实现
    // 注意: 实际应根据 Schema 动态创建
  }
}
