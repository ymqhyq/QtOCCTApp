import fs from 'node:fs';
import crypto from 'node:crypto';

const spec = fs.readFileSync('../docs/cbf_c_component_format_spec_v1.4.2.md', 'utf8');
const match = spec.match(/```python\r?\n([\s\S]*?)\r?\n```/);
if (!match) throw new Error('未找到规范 §6.2 Python 代码块');
const code = `${match[1].replace(/\r\n/g, '\n')}\n`;
const digest = crypto.createHash('sha256').update(Buffer.from(code, 'utf8')).digest('hex');
const bytes = Buffer.byteLength(code, 'utf8');
if (bytes !== 1486 || digest !== '0c034d0fe7643a9ffcfffb1d79ec9ce2363a0e79baa0ca599cb99cd259f1eb0d') {
  throw new Error(`规范脚本校验失败: ${bytes} bytes / ${digest}`);
}
const output = `/**
 * 规范 §6.2 的标准 build123d 建模脚本（1486 字节 UTF-8 LF）。
 * 由 scripts/gen-generator-source.mjs 从规范文档逐字节提取，请勿手工修改。
 */
export const SPEC_PIER_GENERATOR_SOURCE = ${JSON.stringify(code)};
`;
fs.writeFileSync('server/spec-generator.ts', output, 'utf8');
console.log(`generated: ${bytes} bytes, sha256=${digest}`);
