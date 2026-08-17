import { DaemonStatusResponse } from '../types/aerosync';

const DAEMON_BASE_URL = 'http://127.0.0.1:48126';

export const daemonService = {
  async getStatus(): Promise<DaemonStatusResponse> {
    const res = await fetch(`${DAEMON_BASE_URL}/api/status`, {
      method: 'GET',
      headers: { 'Accept': 'application/json' },
      cache: 'no-cache'
    });

    if (!res.ok) {
      throw new Error(`Daemon status returned HTTP ${res.status}`);
    }

    return await res.json();
  },

  async sendTransfer(targetIp: string, targetPort: number, filePaths: string[]): Promise<{ success: boolean; message: string }> {
    const res = await fetch(`${DAEMON_BASE_URL}/api/transfer/send`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        targetIp,
        targetPort,
        filePaths
      })
    });

    if (!res.ok) {
      throw new Error(`Send transfer request failed: HTTP ${res.status}`);
    }

    return await res.json();
  },

  async cancelTransfer(): Promise<boolean> {
    try {
      const res = await fetch(`${DAEMON_BASE_URL}/api/transfer/cancel`, {
        method: 'POST'
      });
      return res.ok;
    } catch {
      return false;
    }
  },

  async updateDownloadDirectory(dirPath: string): Promise<boolean> {
    try {
      const res = await fetch(`${DAEMON_BASE_URL}/api/settings/download_dir`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ downloadDir: dirPath })
      });
      return res.ok;
    } catch {
      return false;
    }
  }
};
