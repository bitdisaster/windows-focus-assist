
declare module 'windows-focus-assist' {
    export interface focusAssistStatus {
        value: int;
        name: string;
    }

    export function getFocusAssist(): focusAssistStatus;
}
