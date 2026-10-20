import { sha256Hex } from '../shared/deterministic';
import { SPEC_PIER_GENERATOR_SOURCE } from './spec-generator';

export interface GeneratorMeta {
  familyId: string;
  name: string;
  inputs: string[];
}

function entryName(familyId: string): string {
  if (familyId === 'PierBody') return 'build_pier';
  return `build_${familyId.replace(/([a-z0-9])([A-Z])/g, '$1_$2').toLowerCase()}`;
}

/** 与 generator.py 中实际函数名严格一致，供 recipe.entry 使用 */
export function generatorEntry(familyId: string): string {
  return entryName(familyId);
}

/**
 * 生成随 .cbfc 包一同交付的建模脚本。
 *
 * 关键约束：`recipe.digest` 必须严格等于本函数输出的 UTF-8 字节 SHA-256，
 * 否则包内 component.json 与 generators/generator.py 会自相矛盾，真实 GATE-03
 * 校验会直接阻断。因此该函数必须是纯函数，且 seed / artifacts 共用同一实现。
 *
 * PierBody 返回规范 §6.2 的 1486 字节标准脚本；其余构件返回结构确定、
 * 带同一函数签名与安全隔离块的占位脚本（生产环境替换为真实建模脚本后，
 * digest 与 cacheKey 会随源码自动更新）。
 */
function safePyText(value: string): string {
  return String(value)
    .replace(/\\/g, '\\\\')
    .replace(/"""/g, '\\"\\"\\"')
    .replace(/\r?\n/g, ' ')
    .replace(/"/g, '\\"');
}

export function buildGeneratorSource(meta: GeneratorMeta): string {
  if (meta.familyId === 'PierBody') {
    return SPEC_PIER_GENERATOR_SOURCE;
  }
  const entry = entryName(meta.familyId);
  const signature = meta.inputs.map((input) => `${input}=0.0`).join(', ');
  const args = meta.inputs.map((input) => `"${input}": ${input}`).join(', ');
  const safeName = safePyText(meta.name);
  return `"""CBF-C 构件参数化建模脚本（${safeName}）"""

from build123d import *


def ${entry}(${signature}):
    """${safeName}

    演示环境占位实现：真实几何由构件库 Mock 内核预先烘焙为 JHB。
    生产环境请替换为 CAD 工程师研制的 build123d / cadquery 脚本，
    并重新生成 recipe.digest 与 defaultCacheKey。
    """
    with BuildPart() as part:
        Box(10, 10, 10)
    solid = part.part
    return {"outputs": {}, "args": {${args}}}


if __name__ == "__main__":
    # 本地调试可视化，线上 Worker 通过 importlib 调用，绝不执行此块
    print("CBF-C 本地建模入口：${safePyText(meta.familyId)}")
`;
}

/** 供测试与自检使用：生成脚本的字节级摘要 */
export async function generatorDigest(meta: GeneratorMeta): Promise<string> {
  return sha256Hex(buildGeneratorSource(meta));
}
