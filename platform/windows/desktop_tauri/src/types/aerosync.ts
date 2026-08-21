export type DevicePlatform = 'windows' | 'android' | 'linux' | 'macos' | 'ios' | 'unknown';

export interface PeerInfo {
  deviceId: string;
  deviceName: string;
  platform: string;
  deviceType: number;
  ipAddress: string;
  port: number;
  lastSeenMs: number;
}

export type TransferUiState =
  | 'IDLE'
  | 'FILE_SELECTED'
  | 'PREPARING'
  | 'WAITING_FOR_DEVICE'
  | 'WAITING_FOR_ACCEPT'
  | 'TRANSFERRING'
  | 'COMPLETED'
  | 'FAILED'
  | 'CANCELLED';

export type QueueItemStatus = 'waiting' | 'connecting' | 'transferring' | 'paused' | 'completed' | 'failed' | 'cancelled';

export interface QueueItem {
  id: string;
  name: string;
  path: string;
  size: number;
  targetDeviceName: string;
  targetIp: string;
  status: QueueItemStatus;
  progressPercent: number;
  transferredBytes: number;
  speedBytesPerSec: number;
  etaSeconds: number;
  errorMessage?: string;
  startTime?: number;
}

export interface TransferHistoryRecord {
  id: string;
  fileName: string;
  filePath: string;
  fileSize: number;
  direction: 'sent' | 'received';
  peerName: string;
  peerIp: string;
  status: 'completed' | 'failed' | 'cancelled';
  speedAvgMbSec: number;
  timestampMs: number;
}

export interface DaemonStatusResponse {
  deviceName: string;
  deviceId: string;
  downloadDir: string;
  isTransferring: boolean;
  statusMessage: string;
  peers: Array<{
    deviceId: string;
    deviceName: string;
    platform: string;
    deviceType: number;
    ipAddress: string;
    port: number;
    lastSeenMs: number;
  }>;
  currentProgress: {
    state: number;
    currentFileName: string;
    fileSize: number;
    fileBytesTransferred: number;
    totalBytesTransferred: number;
    speedBytesPerSec: number;
    progressPercent: number;
    etaSeconds: number;
    errorCode: number;
  };
  completedHistory: string[];
}

export interface SettingsState {
  theme: 'dark' | 'light';
  downloadDirectory: string;
  deviceName: string;
  startWithWindows: boolean;
  notificationsEnabled: boolean;
}

export interface DiskSpace {
  freeBytes: number;
  totalBytes: number;
}

export interface IncomingRequest {
  senderId: string;
  senderName: string;
  senderIp: string;
  platform: string;
  pairingPin: string;
  totalFiles?: number;
  totalBytes?: number;
}
