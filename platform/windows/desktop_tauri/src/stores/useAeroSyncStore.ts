import { useState, useEffect, useCallback, useRef } from 'react';
import {
  PeerInfo,
  QueueItem,
  TransferHistoryRecord,
  SettingsState,
  DiskSpace,
  IncomingRequest,
  DaemonStatusResponse
} from '../types/aerosync';
import { daemonService } from '../services/daemonService';
import { tauriBridge } from '../services/tauriBridge';
import { getFileName } from '../utils/formatters';

const STORAGE_KEY_SETTINGS = 'aerosync_settings_v2';
const STORAGE_KEY_HISTORY = 'aerosync_history_v2';

const DEFAULT_SETTINGS: SettingsState = {
  theme: 'dark',
  downloadDirectory: 'C:\\Users\\Atul\\Downloads\\AeroSync',
  deviceName: 'Windows PC (AeroSync)',
  startWithWindows: false,
  notificationsEnabled: true
};

export function useAeroSyncStore() {
  // Navigation
  const [currentTab, setCurrentTab] = useState<'home' | 'devices' | 'transfers' | 'history' | 'settings'>('home');

  // Peers & Selection
  const [peers, setPeers] = useState<PeerInfo[]>([]);
  const [selectedPeer, setSelectedPeer] = useState<PeerInfo | null>(null);

  // Queue & History
  const [queue, setQueue] = useState<QueueItem[]>([]);
  const [history, setHistory] = useState<TransferHistoryRecord[]>(() => {
    try {
      const saved = localStorage.getItem(STORAGE_KEY_HISTORY);
      return saved ? JSON.parse(saved) : [];
    } catch {
      return [];
    }
  });

  // Settings
  const [settings, setSettings] = useState<SettingsState>(() => {
    try {
      const saved = localStorage.getItem(STORAGE_KEY_SETTINGS);
      if (saved) {
        return { ...DEFAULT_SETTINGS, ...JSON.parse(saved) };
      }
    } catch {}
    return DEFAULT_SETTINGS;
  });

  // Telemetry & Daemon state
  const [isDaemonOnline, setIsDaemonOnline] = useState<boolean>(false);
  const [statusMessage, setStatusMessage] = useState<string>('Initializing AeroSync...');
  const [isTransferring, setIsTransferring] = useState<boolean>(false);
  const [currentProgress, setCurrentProgress] = useState<DaemonStatusResponse['currentProgress']>({
    state: 0,
    currentFileName: '',
    fileSize: 0,
    fileBytesTransferred: 0,
    totalBytesTransferred: 0,
    speedBytesPerSec: 0,
    progressPercent: 0,
    etaSeconds: 0,
    errorCode: 0
  });

  // Storage
  const [diskSpace, setDiskSpace] = useState<DiskSpace>({
    freeBytes: 150 * 1024 * 1024 * 1024,
    totalBytes: 512 * 1024 * 1024 * 1024
  });

  // Modals
  const [incomingRequest, setIncomingRequest] = useState<IncomingRequest | null>(null);
  const [isDirectIpModalOpen, setIsDirectIpModalOpen] = useState<boolean>(false);

  // Throttling and state refs
  const lastActiveFileRef = useRef<string>('');
  const lastHistorySyncRef = useRef<number>(0);
  const latestTransferringRef = useRef<boolean>(false);

  // Save history to localStorage
  useEffect(() => {
    try {
      localStorage.setItem(STORAGE_KEY_HISTORY, JSON.stringify(history));
    } catch (err) {
      console.warn('Failed to save transfer history:', err);
    }
  }, [history]);

  // Save settings to localStorage & apply theme
  useEffect(() => {
    try {
      localStorage.setItem(STORAGE_KEY_SETTINGS, JSON.stringify(settings));
    } catch (err) {
      console.warn('Failed to save settings:', err);
    }

    // Apply theme to document root
    const root = document.documentElement;
    if (settings.theme === 'light') {
      root.classList.remove('dark');
      root.classList.add('light');
    } else if (settings.theme === 'dark') {
      root.classList.remove('light');
      root.classList.add('dark');
    } else {
      // System theme detection
      const prefersDark = window.matchMedia('(prefers-color-scheme: dark)').matches;
      root.classList.toggle('dark', prefersDark);
      root.classList.toggle('light', !prefersDark);
    }
  }, [settings]);

  // Refresh storage metrics dynamically
  const refreshStorage = useCallback(async () => {
    try {
      const space = await tauriBridge.getDiskSpace(settings.downloadDirectory);
      setDiskSpace(space);
    } catch (err) {
      console.warn('Failed to refresh disk space:', err);
    }
  }, [settings.downloadDirectory]);

  // Enqueue multiple files for transfer
  const enqueueFiles = useCallback(async (filePaths: string[], targetPeer?: PeerInfo) => {
    let peer = targetPeer || selectedPeer;
    if (!peer && peers.length > 0) {
      peer = peers[0];
      setSelectedPeer(peer);
    }

    if (!peer) {
      setCurrentTab('devices');
      setStatusMessage('Files selected. Click Send Files on any discovered device below.');
      return;
    }

    // Instantly retrieve file metadata (sizes & names)
    const metaList = await tauriBridge.getFilesMetadata(filePaths);

    const newItems: QueueItem[] = filePaths.map((filePath, index) => {
      const meta = metaList.find(m => m.path === filePath);
      const fileName = meta?.name || getFileName(filePath);
      const fileSize = meta?.size || 0;
      return {
        id: `q-${Date.now()}-${index}-${Math.random().toString(36).substr(2, 5)}`,
        name: fileName,
        path: filePath,
        size: fileSize,
        targetDeviceName: peer.deviceName,
        targetIp: peer.ipAddress,
        status: 'waiting',
        progressPercent: 0,
        transferredBytes: 0,
        speedBytesPerSec: 0,
        etaSeconds: 0
      };
    });

    setQueue(prev => [...prev, ...newItems]);
    setCurrentTab('transfers');
    setStatusMessage(`Transferring ${filePaths.length} file(s) to ${peer.deviceName}...`);

    // Trigger daemon transfer API
    daemonService.sendTransfer(peer.ipAddress, peer.port || 48124, filePaths)
      .then(res => {
        if (res.success) {
          setStatusMessage(`Streaming ${filePaths.length} file(s) to ${peer.deviceName}`);
        }
      })
      .catch(err => {
        console.error('Failed to trigger transfer via daemon:', err);
        setStatusMessage(`Transfer error: ${err.message}`);
      });
  }, [selectedPeer, peers]);

  // Cancel active transfer
  const cancelTransfer = useCallback(async () => {
    await daemonService.cancelTransfer();
    setQueue(prev => prev.map(item => item.status === 'transferring' ? { ...item, status: 'cancelled' } : item));
    setIsTransferring(false);
    setStatusMessage('Transfer cancelled');
  }, []);

  // Clear completed/cancelled queue items
  const clearCompletedQueue = useCallback(() => {
    setQueue(prev => prev.filter(item => item.status === 'transferring' || item.status === 'waiting'));
  }, []);

  // Clear all history
  const clearHistory = useCallback(() => {
    setHistory([]);
  }, []);

  // Update Settings
  const updateSettings = useCallback((partial: Partial<SettingsState>) => {
    setSettings(prev => {
      const next = { ...prev, ...partial };
      if (partial.downloadDirectory) {
        daemonService.updateDownloadDirectory(partial.downloadDirectory);
      }
      return next;
    });
  }, []);

  // Adaptive daemon polling loop
  useEffect(() => {
    let timer: number | null = null;
    let isCancelled = false;

    const poll = async () => {
      try {
        const data = await daemonService.getStatus();
        if (isCancelled) return;

        setIsDaemonOnline(true);
        setPeers(data.peers || []);
        setIsTransferring(data.isTransferring);
        latestTransferringRef.current = data.isTransferring;
        setCurrentProgress(data.currentProgress || {
          state: 0,
          currentFileName: '',
          fileSize: 0,
          fileBytesTransferred: 0,
          totalBytesTransferred: 0,
          speedBytesPerSec: 0,
          progressPercent: 0,
          etaSeconds: 0,
          errorCode: 0
        });

        if (data.downloadDir && data.downloadDir !== settings.downloadDirectory) {
          // Sync download directory from daemon if configured
          setSettings(prev => ({ ...prev, downloadDirectory: data.downloadDir }));
        }

        if (data.statusMessage) {
          setStatusMessage(data.statusMessage);
        }

        // Handle completed transfer migration from queue to history
        if (data.currentProgress && data.currentProgress.state === 3 /* COMPLETED */) {
          const completedName = data.currentProgress.currentFileName;
          if (completedName && completedName !== lastActiveFileRef.current) {
            lastActiveFileRef.current = completedName;

            // Add to history
            const newRecord: TransferHistoryRecord = {
              id: `hist-${Date.now()}-${Math.random().toString(36).substr(2, 5)}`,
              fileName: completedName,
              filePath: `${settings.downloadDirectory}\\${completedName}`,
              fileSize: data.currentProgress.fileSize,
              direction: 'received',
              peerName: selectedPeer ? selectedPeer.deviceName : 'Peer Device',
              peerIp: selectedPeer ? selectedPeer.ipAddress : 'LAN',
              status: 'completed',
              speedAvgMbSec: data.currentProgress.speedBytesPerSec / (1024 * 1024),
              timestampMs: Date.now()
            };

            setHistory(prev => [newRecord, ...prev.filter(h => h.fileName !== completedName || Math.abs(h.timestampMs - newRecord.timestampMs) > 2000)]);
            setQueue(prev => prev.filter(q => q.name !== completedName));
            refreshStorage();
          }
        }

        // Periodic completed history check from daemon array
        const now = Date.now();
        if (now - lastHistorySyncRef.current > 5000 && data.completedHistory && data.completedHistory.length > 0) {
          lastHistorySyncRef.current = now;
          refreshStorage();
        }

      } catch {
        if (!isCancelled) {
          setIsDaemonOnline(false);
          setStatusMessage('Connecting to AeroSync native engine...');
        }
      }

      if (!isCancelled) {
        // Adaptive rate: 200ms when transferring, 1500ms when idle
        const interval = latestTransferringRef.current ? 200 : 1500;
        timer = window.setTimeout(poll, interval);
      }
    };

    poll();
    refreshStorage();

    return () => {
      isCancelled = true;
      if (timer) clearTimeout(timer);
    };
  }, [isTransferring, selectedPeer, settings.downloadDirectory, refreshStorage]);

  return {
    currentTab,
    setCurrentTab,
    peers,
    selectedPeer,
    setSelectedPeer,
    queue,
    history,
    settings,
    diskSpace,
    isDaemonOnline,
    statusMessage,
    isTransferring,
    currentProgress,
    incomingRequest,
    setIncomingRequest,
    isDirectIpModalOpen,
    setIsDirectIpModalOpen,
    enqueueFiles,
    cancelTransfer,
    clearCompletedQueue,
    clearHistory,
    updateSettings,
    refreshStorage
  };
}

export type AeroSyncStore = ReturnType<typeof useAeroSyncStore>;
