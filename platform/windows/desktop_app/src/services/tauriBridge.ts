import { invoke } from '@tauri-apps/api/core';
import { open } from '@tauri-apps/plugin-dialog';

export const isTauriAvailable = (): boolean => {
  return typeof window !== 'undefined' && '__TAURI_INTERNALS__' in window;
};

export const tauriBridge = {
  minimizeWindow: async (): Promise<void> => {
    if (isTauriAvailable()) {
      await invoke('minimize_window');
    }
  },

  maximizeWindow: async (): Promise<void> => {
    if (isTauriAvailable()) {
      await invoke('maximize_window');
    }
  },

  closeWindow: async (): Promise<void> => {
    if (isTauriAvailable()) {
      await invoke('close_window');
    } else {
      window.close();
    }
  },

  selectFiles: async (): Promise<string[]> => {
    if (isTauriAvailable()) {
      const selected = await open({
        multiple: true,
        directory: false,
        title: 'Select Files to Send via AeroSync'
      });

      if (!selected) return [];
      if (Array.isArray(selected)) return selected;
      return [selected];
    }

    return new Promise((resolve) => {
      const input = document.createElement('input');
      input.type = 'file';
      input.multiple = true;
      input.onchange = (e) => {
        const files = (e.target as HTMLInputElement).files;
        if (files) {
          const names = Array.from(files).map(f => f.name);
          resolve(names);
        } else {
          resolve([]);
        }
      };
      input.click();
    });
  },

  selectFolder: async (): Promise<string | null> => {
    if (isTauriAvailable()) {
      const selected = await open({
        multiple: false,
        directory: true,
        title: 'Select Folder'
      });

      if (!selected) return null;
      if (Array.isArray(selected)) return selected[0] || null;
      return selected;
    }
    return null;
  },

  showInFolder: async (filePath: string): Promise<void> => {
    if (isTauriAvailable()) {
      try {
        await invoke('show_in_folder', { path: filePath });
      } catch (err) {
        console.error('Failed to show file in explorer:', err);
      }
    }
  },

  openFolder: async (dirPath: string): Promise<void> => {
    if (isTauriAvailable()) {
      try {
        await invoke('open_folder', { path: dirPath });
      } catch (err) {
        console.error('Failed to open folder in explorer:', err);
      }
    }
  },

  getDiskSpace: async (path?: string): Promise<{ freeBytes: number; totalBytes: number }> => {
    if (isTauriAvailable()) {
      try {
        return await invoke<{ free_bytes: number; total_bytes: number }>('get_disk_space', { path })
          .then(res => ({ freeBytes: res.free_bytes, totalBytes: res.total_bytes }));
      } catch (err) {
        console.warn('Failed to query disk space via Tauri:', err);
      }
    }
    return { freeBytes: 120 * 1024 * 1024 * 1024, totalBytes: 512 * 1024 * 1024 * 1024 };
  }
};
