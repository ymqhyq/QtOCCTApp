import { reactive } from 'vue';
import { SchemaInfo } from './cad-service';

export const store = reactive({
    schemas: {} as Record<string, SchemaInfo>,
    selectedModelKey: null as string | null,
    currentArgs: {} as Record<string, any>,
    isLoading: false,
    statusInfo: '准备就绪',

    setSchemas(schemas: Record<string, SchemaInfo>) {
        this.schemas = schemas;
    },

    selectModel(key: string) {
        this.selectedModelKey = key;
        const schema = this.schemas[key];
        const args: Record<string, any> = {};

        // Initialize args from schema
        for (const [propKey, propMeta] of Object.entries(schema)) {
            if (propKey === 'name' || typeof propMeta !== 'object') continue;
            args[propKey] = propMeta.value !== undefined ? propMeta.value : '';
        }
        this.currentArgs = args;
    },

    updateArg(key: string, value: any) {
        this.currentArgs[key] = value;
    }
});
