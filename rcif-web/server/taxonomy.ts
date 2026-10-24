import type { TaxonomyScheme } from '../shared/types';

export interface TaxonomyNodeDef {
  key: string;
  code: string;
  label: string;
  children?: TaxonomyNodeDef[];
}

export const TAXONOMY_SCHEMES: { id: string; code: string; name: string; version: string; description: string }[] = [
  {
    id: 'crbim',
    code: 'CRBIM-2023',
    name: 'CRBIM 铁路工程信息模型 53 表',
    version: 'v2.3',
    description: '中国铁路 BIM 联盟桥梁/隧道/路基专业分类标准（53 表）',
  },
  {
    id: 'gb51269',
    code: 'GB/T-51269',
    name: 'GB/T 51269 建筑信息模型分类和编码标准',
    version: '2017',
    description: '国家标准建筑信息模型分类编码，用于跨行业交付与归档',
  },
  {
    id: 'jtg',
    code: 'JTG-2020',
    name: 'JTG 公路工程标准体系',
    version: '2020',
    description: '交通运输部公路工程分类体系，用于公路项目复用',
  },
];

export const TAXONOMY_TREES: Record<string, TaxonomyNodeDef[]> = {
  crbim: [
    {
      key: '53',
      code: '53',
      label: '桥涵工程',
      children: [
        {
          key: '53-01',
          code: '53-01',
          label: '桥梁基础',
          children: [
            { key: '53-01-10', code: '53-01-10', label: '明挖基础' },
            { key: '53-01-20', code: '53-01-20', label: '桩基础' },
            { key: '53-01-30', code: '53-01-30', label: '承台' },
          ],
        },
        {
          key: '53-02',
          code: '53-02',
          label: '桥梁下部结构',
          children: [
            { key: '53-02-10', code: '53-02-10', label: '桥墩墩身' },
            { key: '53-02-20', code: '53-02-20', label: '桥墩托盘与顶帽' },
            { key: '53-02-30', code: '53-02-30', label: '桥台' },
            { key: '53-02-40', code: '53-02-40', label: '支座与垫石' },
          ],
        },
        {
          key: '53-03',
          code: '53-03',
          label: '桥梁上部结构',
          children: [
            { key: '53-03-10', code: '53-03-10', label: '预应力混凝土梁' },
            { key: '53-03-20', code: '53-03-20', label: '桥面板与横隔板' },
          ],
        },
        {
          key: '53-04',
          code: '53-04',
          label: '桥面系及附属',
          children: [
            { key: '53-04-10', code: '53-04-10', label: '护栏与伸缩缝' },
            { key: '53-04-20', code: '53-04-20', label: '排水与电缆槽' },
            { key: '53-04-30', code: '53-04-30', label: '附属钢结构' },
          ],
        },
        {
          key: '53-05',
          code: '53-05',
          label: '涵洞与通道',
          children: [{ key: '53-05-10', code: '53-05-10', label: '箱涵' }],
        },
        {
          key: '53-06',
          code: '53-06',
          label: '隧道工程',
          children: [{ key: '53-06-10', code: '53-06-10', label: '隧道二次衬砌' }],
        },
        {
          key: '53-07',
          code: '53-07',
          label: '路基与防护',
          children: [
            { key: '53-07-10', code: '53-07-10', label: '挡土墙' },
            { key: '53-07-20', code: '53-07-20', label: '边坡防护' },
          ],
        },
      ],
    },
  ],
  gb51269: [
    {
      key: '04',
      code: '04',
      label: '桥梁工程',
      children: [
        {
          key: '04.02',
          code: '04.02',
          label: '下部结构',
          children: [
            { key: '04.02.10', code: '04.02.10', label: '墩台身' },
            { key: '04.02.20', code: '04.02.20', label: '基础与承台' },
            { key: '04.02.30', code: '04.02.30', label: '支座系统' },
          ],
        },
        {
          key: '04.03',
          code: '04.03',
          label: '上部结构',
          children: [
            { key: '04.03.10', code: '04.03.10', label: '梁体' },
            { key: '04.03.20', code: '04.03.20', label: '桥面板' },
          ],
        },
        {
          key: '04.04',
          code: '04.04',
          label: '桥面系',
          children: [
            { key: '04.04.10', code: '04.04.10', label: '护栏与伸缩装置' },
            { key: '04.04.20', code: '04.04.20', label: '排水与管线' },
          ],
        },
      ],
    },
    {
      key: '03',
      code: '03',
      label: '隧道工程',
      children: [{ key: '03.01', code: '03.01', label: '衬砌' }],
    },
    {
      key: '02',
      code: '02',
      label: '路基工程',
      children: [
        { key: '02.01', code: '02.01', label: '支挡结构' },
        { key: '02.02', code: '02.02', label: '边坡防护' },
      ],
    },
  ],
  jtg: [
    {
      key: 'JTG-3',
      code: 'JTG-3',
      label: '桥涵工程',
      children: [
        {
          key: 'JTG-3.1',
          code: 'JTG-3.1',
          label: '基础',
          children: [{ key: 'JTG-3.1.1', code: 'JTG-3.1.1', label: '桩基与承台' }],
        },
        {
          key: 'JTG-3.2',
          code: 'JTG-3.2',
          label: '墩台',
          children: [
            { key: 'JTG-3.2.1', code: 'JTG-3.2.1', label: '墩身与桥台' },
            { key: 'JTG-3.2.2', code: 'JTG-3.2.2', label: '支座与垫石' },
          ],
        },
        {
          key: 'JTG-3.3',
          code: 'JTG-3.3',
          label: '上部结构',
          children: [
            { key: 'JTG-3.3.1', code: 'JTG-3.3.1', label: '梁体' },
            { key: 'JTG-3.3.2', code: 'JTG-3.3.2', label: '桥面板' },
          ],
        },
        {
          key: 'JTG-3.4',
          code: 'JTG-3.4',
          label: '附属工程',
          children: [
            { key: 'JTG-3.4.1', code: 'JTG-3.4.1', label: '护栏与伸缩缝' },
            { key: 'JTG-3.4.2', code: 'JTG-3.4.2', label: '排水与管线' },
          ],
        },
      ],
    },
    {
      key: 'JTG-4',
      code: 'JTG-4',
      label: '隧道工程',
      children: [{ key: 'JTG-4.1', code: 'JTG-4.1', label: '衬砌' }],
    },
    {
      key: 'JTG-2',
      code: 'JTG-2',
      label: '路基工程',
      children: [
        { key: 'JTG-2.1', code: 'JTG-2.1', label: '支挡结构' },
        { key: 'JTG-2.2', code: 'JTG-2.2', label: '边坡防护' },
      ],
    },
  ],
};

/** 依据 CRBIM 叶节点构造其余标准体系的映射（演示数据保持口径一致） */
export const CROSS_SCHEME_MAP: Record<string, { gb51269: string; jtg: string }> = {
  '53-01-10': { gb51269: '04.02.20', jtg: 'JTG-3.1.1' },
  '53-01-20': { gb51269: '04.02.20', jtg: 'JTG-3.1.1' },
  '53-01-30': { gb51269: '04.02.20', jtg: 'JTG-3.1.1' },
  '53-02-10': { gb51269: '04.02.10', jtg: 'JTG-3.2.1' },
  '53-02-20': { gb51269: '04.02.10', jtg: 'JTG-3.2.1' },
  '53-02-30': { gb51269: '04.02.10', jtg: 'JTG-3.2.1' },
  '53-02-40': { gb51269: '04.02.30', jtg: 'JTG-3.2.2' },
  '53-03-10': { gb51269: '04.03.10', jtg: 'JTG-3.3.1' },
  '53-03-20': { gb51269: '04.03.20', jtg: 'JTG-3.3.2' },
  '53-04-10': { gb51269: '04.04.10', jtg: 'JTG-3.4.1' },
  '53-04-20': { gb51269: '04.04.20', jtg: 'JTG-3.4.2' },
  '53-04-30': { gb51269: '04.04.10', jtg: 'JTG-3.4.1' },
  '53-05-10': { gb51269: '04.03.10', jtg: 'JTG-3.3.1' },
  '53-06-10': { gb51269: '03.01', jtg: 'JTG-4.1' },
  '53-07-10': { gb51269: '02.01', jtg: 'JTG-2.1' },
  '53-07-20': { gb51269: '02.02', jtg: 'JTG-2.2' },
};

export function buildTaxonomySchemes(
  components: { taxonomy: { crbim: string } }[],
): TaxonomyScheme[] {
  const countUnder = (code: string) =>
    components.filter((c) => c.taxonomy.crbim.startsWith(code)).length;
  return TAXONOMY_SCHEMES.map((scheme) => {
    const tree = TAXONOMY_TREES[scheme.id];
    const convert = (node: TaxonomyNodeDef): any => ({
      key: node.key,
      code: node.code,
      label: node.label,
      count: scheme.id === 'crbim' ? countUnder(node.code) : 0,
      children: node.children?.map(convert),
    });
    const nodes = tree.map(convert);
    if (scheme.id !== 'crbim') {
      // 非 CRBIM 方案按 GB/JTG 码前缀统计
      const countByCode = (code: string) => {
        const key = scheme.id === 'gb51269' ? 'gb51269' : 'jtg';
        return components.filter((c: any) => String(c.taxonomy[key]).startsWith(code)).length;
      };
      const walk = (list: any[]) => {
        for (const n of list) {
          n.count = countByCode(n.code);
          if (n.children) walk(n.children);
        }
      };
      walk(nodes);
    }
    return { ...scheme, nodes };
  });
}
