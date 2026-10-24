import { describe, expect, it } from 'vitest';
import { createState } from './state';
import { buildCbfcFiles, makeIfcFile, makeJhb } from './artifacts';
import { decodeJhb } from '../src/jhb/decode';
import { SPEC_PIER_GENERATOR_SOURCE } from './spec-generator';
import { buildGeneratorSource } from './generator';
import { sha256Hex } from '../shared/deterministic';

const text = (bytes: Uint8Array) => new TextDecoder().decode(bytes);

describe('.cbfc 制品包内部一致性', () => {
  it('规范 §6.2 脚本为 1486 字节且摘要为 0c034d0f…', async () => {
    const bytes = new TextEncoder().encode(SPEC_PIER_GENERATOR_SOURCE);
    expect(bytes.byteLength).toBe(1486);
    expect(await sha256Hex(bytes)).toBe(
      '0c034d0fe7643a9ffcfffb1d79ec9ce2363a0e79baa0ca599cb99cd259f1eb0d',
    );
    expect(
      buildGeneratorSource({ familyId: 'PierBody', name: '实体圆端形桥墩', inputs: [] }),
    ).toBe(SPEC_PIER_GENERATOR_SOURCE);
  });

  it('component.json 的 recipe.digest、generator.py 与 checksums.json 三方一致', async () => {
    const state = await createState();
    const parametric = state.components.filter((item) => item.recipe);
    expect(parametric.length).toBeGreaterThan(10);
    for (const component of parametric) {
      const familyId = component.familyId;
      const files = await buildCbfcFiles(component);
      const byName = new Map(files.map((file) => [file.name, file.data]));
      const componentJson = JSON.parse(text(byName.get('component.json')!));
      const scriptBytes = byName.get('generators/generator.py')!;
      const scriptDigest = await sha256Hex(scriptBytes);
      expect(componentJson.recipe.digest, `${familyId} recipe.digest`).toBe(scriptDigest);
      expect(componentJson.recipe.digest, `${familyId} seed recipe.digest`).toBe(
        component.recipe!.digest,
      );
      expect(text(scriptBytes), `${familyId} entry`).toContain(`def ${componentJson.recipe.entry}(`);
      if (familyId === 'PierBody') {
        expect(componentJson.recipe.entry).toBe('build_pier');
      }
      expect(componentJson.geometry.defaultCacheKey).toBe(component.geometry.defaultCacheKey);

      const checksums = JSON.parse(text(byName.get('checksums.json')!));
      for (const [name, expected] of Object.entries(checksums.files as Record<string, string>)) {
        expect(await sha256Hex(byName.get(name)!), `${familyId} ${name}`).toBe(
          expected.replace(/^sha256:/, ''),
        );
      }
    }
  });

  it('IFC4 交付物包含可被浏览器加载的 B-Rep 三角面', async () => {
    const state = await createState();
    const component = state.components.find((item) => item.familyId === 'Abutment')!;
    const ifc = text(makeIfcFile(component));
    expect(ifc).toContain("FILE_SCHEMA(('IFC4'))");
    expect(ifc).toContain('IFCFACETEDBREP');
    expect(ifc).toContain('IFCCLOSEDSHELL');
    expect(ifc).toContain('IFCCOLUMN');
    expect(ifc).toContain('IFCCARTESIANPOINT');
    expect(ifc).not.toContain('IFCBRIDGE');
  });

  it('族名写入生成脚本时被执行字符串转义', () => {
    const source = buildGeneratorSource({
      familyId: 'EscapedName',
      name: '"""\nprint("injected")',
      inputs: [],
    });
    expect(source).not.toContain('\nprint("injected")');
    expect(source).toContain('\\"');
  });

  it('JHB 的 solid/face/wire 输出按 shape 独立计算统计值', async () => {
    const state = await createState();
    const component = state.components.find((item) => item.familyId === 'PierBody')!;
    const defaultModel = String(component.parameters.schema[0].default);
    const preset = component.parameters.presets.find((item) => item.modelNumber === defaultModel)!;
    const jhb = makeJhb(component, preset.params, preset.cacheKey!);
    const decoded = decodeJhb(jhb.buffer);
    const outputs = decoded.header.outputs!;
    const wireKey = Object.keys(outputs).find((key) => outputs[key].geomType === 'wire')!;
    expect(outputs.PierBodySolid.volume).toBeGreaterThan(0);
    expect(outputs[wireKey].length).toBeGreaterThan(0);
  });
});
