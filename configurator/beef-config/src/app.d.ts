// See https://kit.svelte.dev/docs/types#app
// for information about these interfaces
declare global {
	interface Window {
		beefNative?: {
			getWindowVisibility?: () => Promise<boolean>;
			onWindowVisibility?: (callback: (visible: boolean) => void) => () => void;
			selectFirmware(): Promise<{ name: string; bytes: number; commitHash: string | null; version: string | null } | null>;
			flashFirmware(): Promise<{
				success: boolean;
				exitCode: number | null;
				restartSuccess: boolean;
				restartExitCode: number | null;
			}>;
			restartDfu(): Promise<{ success: boolean; exitCode: number | null }>;
			authorizeHidIdentity(identity: { vendorId: number; productId: number }): Promise<number>;
			getVersion(): Promise<string>;
			checkForUpdates(): Promise<{
				currentVersion: string;
				latestVersion: string;
				releaseUrl: string;
				updateAvailable: boolean;
			} | null>;
            checkForFirmwareUpdates(board: 'rp2354b' | 'at90', version: string): Promise<{
                board: 'rp2354b' | 'at90'; currentVersion: string; latestVersion: string;
                releaseUrl: string; updateAvailable: boolean;
            } | null>;
			openUpdate(url: string): Promise<void>;
			onLog(callback: (text: string) => void): () => void;
		};
	}
	namespace App {
		// interface Error {}
		// interface Locals {}
		// interface PageData {}
		// interface PageState {}
		// interface Platform {}
	}
}

export {};
