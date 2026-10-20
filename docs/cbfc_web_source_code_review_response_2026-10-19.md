# 对《CBFC Web 构件库管理系统源代码审核报告》的评估与修复记录

- 审核报告：[cbfc_web_source_code_review_2026-10-19.md](cbfc_web_source_code_review_2026-10-19.md)
- 处理日期：2026-10-19
- 当前版本：`cbfc-web` v2.5.0（41 个构件 / 63 个型号）

## 1. 总体评估

报告的主要判断成立：系统演示完成度较高，但认证、对象级授权、门禁真实性、制品一致性仍不具备直接生产能力。本轮不把演示环境改造成完整生产后端，而是：

1. 修复可以直接闭环的授权、制品和输出一致性问题；
2. 对无法在纯 Mock 环境真实完成的门禁，明确标注为演示，避免继续伪装成生产校验；
3. 保留报告指出的剩余风险，作为后续生产迭代前置条件。

## 2. 本轮已修复

### 2.1 P0 认证与会话

- Token 从可伪造的 `demo.<userId>.<nonce>` 改为：
  `demo.<userId>.<expiresAt>.<HMAC-SHA256>`；
- 增加 15 分钟过期校验和签名常量时间比较；
- 登录接口返回真实 `expiresIn`；
- 伪造 token 现在返回 401。

**仍保留的演示特性**：登录仍按 `role` 直接签发身份，URL `?role=` 仍可切换演示身份；生产必须替换为企业 IdP/SSO。

### 2.2 P0 对象级授权

统一收口 `accessible()`：

- public：所有可读；
- tenant / confidential：仅同租户内部角色可读；
- 合作方仅 public。

同时补齐：

- 私有构件 `preview.svg` 必须携带有效 token；公开构件仍允许匿名；
- `/reviews`、`/reviews/:id/geometry` 仅 reviewer/asset-admin；
- `/jobs/:id`、`/jobs/:id/result`、取消任务校验 owner；
- 下载、导出、语义、格式清单、收藏校验 `accessible`；
- 上传仅 designer/asset-admin；
- 提交审核校验 upload owner（asset-admin 例外）；
- `/analytics/overview` 仅资产/标准管理员；
- `/projects`、`/semantic-profiles` 禁止合作方访问。

新增 `scripts/security-smoke.mjs`，覆盖 11 项伪造 token、匿名预览、合作方越权等断言。

### 2.3 P1 cacheKey 一致性

- `geometry.defaultCacheKey` 现在直接取默认 preset 的 cacheKey；
- seed 测试从“仅 parametric”扩展为**全部构件**，断言默认 preset key 与 defaultCacheKey 完全一致。

### 2.4 P1 .cbfc / JHB 制品

- `buildCbfcFiles()` 与 `makeCbfZip()` 现在使用默认 preset 参数生成 JHB，不再传空参数；
- ZIP 时间戳固定为 1980-01-01，保证相同输入字节级可复现；
- 按 shape 独立计算 `volume / area / length / bbox`，wire 长度不再恒为 0；
- 生成脚本对族名做 Python 字符串转义，避免破坏生成脚本语法；
- IFC 文本对名称、分类、familyId 做 STEP 单引号转义。

### 2.5 P1 状态机与输入边界

- 族 PATCH 不再允许直接把状态改成 `published/reviewing`，必须走上传门禁和审核流程；
- 仅允许 `draft` / `archived`；
- 上传门禁响应显式带 `demo: true`，页面增加 Mock 门禁警示。

### 2.6 审核页与角色导航

- `/reviews` 标题统一为「审核质检」；
- 删除顶部当值/快捷键提示和权威缓存键展示；
- 设计师无法访问审核页；
- 审核几何与审核队列增加角色 + 构件可访问性检查。

## 3. 仍未闭环的风险

| 优先级 | 剩余问题 | 处理建议 |
| :--- | :--- | :--- |
| P0 | 入库门禁仍只接收元数据，不解析真实 ZIP | 接入服务端文件校验/沙箱解压，当前页面已明确标注演示 |
| P0 | 登录仍按 role 直接签发，`?role=` 可切身份 | 生产替换 SSO/IdP，关闭演示身份切换 |
| P1 | 配置器“保存并下载”仍导出默认几何 | 导出请求携带当前参数/cacheKey，服务端按本次求值结果打包 |
| P1 | 族改名后 recipe digest/cacheKey 未全部重算 | 统一触发 digest/cacheKey/预烘焙重建 |
| P1 | 审核通过仍未把新参数几何落到 component.mesh | 审核通过时应用 review.newParams 并重新编码 JHB |
| P1 | preset 非参数 cacheKey 仍以体积/型号摘要为主 | 改为 mesh/JHB 内容摘要 |
| P2 | 无 rate limit、无持久化会话、无审计存储 | 生产网关与数据库 |

## 4. 验证

```text
vue-tsc --noEmit     通过
vitest run           30/30 通过（7 个测试文件）
vite build           通过
security-smoke       11/11 通过
```

新增/更新测试：

- 全部 41 个构件的 defaultCacheKey 与默认 preset cacheKey 一致；
- JHB solid/face/wire 输出按 shape 独立计算；
- 生成脚本转义；
- `.cbfc` 包内 recipe.digest / generator.py / checksums.json 三方一致。

## 5. 结论

审核报告的核心风险仍然成立，但本轮已经消除了最直接的伪造 token、跨租户读取、匿名私有预览、合作方越权、JHB 全 shape 同统计值等可验证问题。系统可以继续作为演示/走查版本使用；上线前必须优先完成真实门禁、SSO、配置器导出和审核几何落地。
