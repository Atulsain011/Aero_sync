export function formatBytes(bytes: number, decimals: number = 1): string {
  if (bytes === 0) return '0 B';
  if (!Number.isFinite(bytes) || bytes < 0) return '0 B';

  const k = 1024;
  const dm = decimals < 0 ? 0 : decimals;
  const sizes = ['B', 'KB', 'MB', 'GB', 'TB', 'PB'];

  const i = Math.floor(Math.log(bytes) / Math.log(k));
  if (i < 0) return `${bytes} B`;
  const idx = Math.min(i, sizes.length - 1);

  return `${parseFloat((bytes / Math.pow(k, idx)).toFixed(dm))} ${sizes[idx]}`;
}

export function formatSpeedMBs(bytesPerSec: number): string {
  if (!Number.isFinite(bytesPerSec) || bytesPerSec <= 0) return '0.0 MB/s';
  const mbPerSec = bytesPerSec / (1024 * 1024);
  return `${mbPerSec.toFixed(1)} MB/s`;
}

export function formatSpeedMbps(bytesPerSec: number): string {
  if (!Number.isFinite(bytesPerSec) || bytesPerSec <= 0) return '0 Mbps';
  const mbps = (bytesPerSec * 8) / (1024 * 1024);
  return `${mbps.toFixed(0)} Mbps`;
}

export function formatEta(seconds: number): string {
  if (!Number.isFinite(seconds) || seconds <= 0) return 'Calculating...';
  if (seconds < 60) return `${Math.round(seconds)}s remaining`;
  
  const mins = Math.floor(seconds / 60);
  const secs = Math.round(seconds % 60);
  if (mins < 60) {
    return `${mins}m ${secs}s remaining`;
  }
  
  const hours = Math.floor(mins / 60);
  const remMins = mins % 60;
  return `${hours}h ${remMins}m remaining`;
}

export function formatTimestamp(ms: number): string {
  if (!ms || ms <= 0) return 'Just now';
  // If timestamp is smaller than year 2000 in ms (946684800000), treat as recent relative uptime
  if (ms < 946684800000) return 'Just now';

  const date = new Date(ms);
  const now = new Date();
  const diffSec = Math.floor((now.getTime() - date.getTime()) / 1000);

  if (diffSec < 10 || diffSec < 0) return 'Just now';
  if (diffSec < 60) return `${diffSec}s ago`;
  if (diffSec < 3600) return `${Math.floor(diffSec / 60)}m ago`;
  if (diffSec < 86400) return `${Math.floor(diffSec / 3600)}h ago`;

  return date.toLocaleDateString(undefined, {
    month: 'short',
    day: 'numeric',
    hour: '2-digit',
    minute: '2-digit'
  });
}

export function sanitizePath(filePath: string): string {
  return filePath.replace(/\\/g, '/');
}

export function getFileName(filePath: string): string {
  const clean = sanitizePath(filePath);
  const parts = clean.split('/');
  return parts[parts.length - 1] || filePath;
}
