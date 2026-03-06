export interface SchemaInfo {
    name: string;
    [key: string]: any;
}

export interface ModelMetadata {
    args: Record<string, any>;
    modelType: string;
    name: string;
    schema: {
        name: string;
        fields: Array<{
            key: string;
            name?: string;
            value?: any;
            type?: 'int' | 'float' | 'string';
            [key: string]: any;
        }>;
    };
}

export interface GenerateResult {
    metadata: ModelMetadata;
    modelData: Uint8Array;
}

export class CadService {
    public static async fetchSchemas(): Promise<Record<string, SchemaInfo>> {
        const response = await fetch('/api/v1/schemas');
        if (!response.ok) {
            throw new Error(`Failed to fetch schemas: ${response.statusText}`);
        }
        return response.json();
    }

    public static async generateModel(modelType: string, args: Record<string, any>): Promise<GenerateResult> {
        const response = await fetch('/api/v1/model/generate', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                code: '',
                model_type: modelType,
                args: args
            })
        });

        if (!response.ok) {
            let errorMsg = await response.text();
            try {
                const parsed = JSON.parse(errorMsg);
                errorMsg = parsed.detail || errorMsg;
            } catch { }
            throw new Error(errorMsg);
        }

        const arrayBuffer = await response.arrayBuffer();
        const view = new DataView(arrayBuffer);
        const jsonLength = view.getUint32(0, true);
        const metadataBytes = new Uint8Array(arrayBuffer, 4, jsonLength);
        const metadata: ModelMetadata = JSON.parse(new TextDecoder().decode(metadataBytes));
        const modelData = new Uint8Array(arrayBuffer, 4 + jsonLength);

        return { metadata, modelData };
    }
}
