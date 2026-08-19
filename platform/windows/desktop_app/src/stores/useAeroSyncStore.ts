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
    } else {
      root.classList.remove('light');
      root.classList.add('dark');
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

  const pollTriggerRef = useRef<(() => void) | null>(null);

  // Enqueue multiple files for transfer with 0ms instantaneous UI feedback
  const enqueueFiles = useCallback((filePaths: string[], targetPeer?: PeerInfo) => {
    if (!filePaths || filePaths.length === 0) return;

    let peer = targetPeer || selectedPeer;
    if (!peer && peers.length > 0) {
      peer = peers[0];
    }

    // 1. INSTANT OPTIMISTIC UI: Immediately add items to queue without waiting for IPC or network
    const now = Date.now();
    const newItems: QueueItem[] = filePaths.map((filePath, index) => {
      const fileName = getFileName(filePath);
      return {
        id: `q-${now}-${index}-${Math.random().toString(36).substr(2, 5)}`,
        name: fileName,
        path: filePath,
        size: 0,
        targetDeviceName: peer ? peer.deviceName : 'Select Device',
        targetIp: peer ? peer.ipAddress : '',
        status: peer ? 'transferring' : 'waiting',
        progressPercent: 0,
        transferredBytes: 0,
        speedBytesPerSec: 0,
        etaSeconds: 0
      };
    });

    // 2. Switch tab to transfers IMMEDIATELY on the same frame (<1ms)
    setQueue(prev => [...prev, ...newItems]);
    setCurrentTab('transfers');
    setStatusMessage(`${filePaths.length} file(s) selected.`);

    // 3. Retrieve exact file metadata in background
    tauriBridge.getFilesMetadata(filePaths).then(metaList => {
      setQueue(prev => prev.map(item => {
        const meta = metaList.find(m => m.path === item.path);
        return meta && meta.size > 0 ? { ...item, size: meta.size, name: meta.name } : item;
      }));
    }).catch(() => {});

    // 4. If peer available, trigger daemon transfer API & immediate poll
    if (peer) {
      setSelectedPeer(peer);
      setIsTransferring(true);
      latestTransferringRef.current = true;
      setStatusMessage(`Transferring ${filePaths.length} file(s) to ${peer.deviceName}...`);
      const targetPeerObj = peer;
      daemonService.sendTransfer(targetPeerObj.ipAddress, targetPeerObj.port || 48124, filePaths)
        .then(res => {
          if (res.success) {
            setStatusMessage(`Streaming ${filePaths.length} file(s) to ${targetPeerObj.deviceName}`);
          }
          if (pollTriggerRef.current) pollTriggerRef.current();
        })
        .catch(err => {
          console.error('Failed to trigger transfer via daemon:', err);
          setStatusMessage(`Transfer error: ${err.message}`);
        });
    }
  }, [selectedPeer, peers]);

  // Cancel active transfer
  const cancelTransfer = useCallback(async () => {
    try {
      await daemonService.cancelTransfer();
    } catch (err) {
      console.warn('Error cancelling transfer:', err);
    }
    // Remove cancelled items immediately from UI queue
    setQueue(prev => prev.filter(item => item.status !== 'transferring'));
    setIsTransferring(false);
    latestTransferringRef.current = false;
    setCurrentProgress({
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
    setSelectedPeer(null);
    setStatusMessage('Transfer cancelled.');
    if (pollTriggerRef.current) pollTriggerRef.current();
  }, []);

  // Clear all queue items
  const clearQueue = useCallback(() => {
    setQueue([]);
  }, []);

  // Clear non-active queue items
  const clearCompletedQueue = useCallback(() => {
    setQueue(prev => prev.filter(item => item.status === 'transferring'));
  }, []);

  // Clear all history
  const clearHistory = useCallback(() => {
    setHistory([]);
    try {
      localStorage.removeItem(STORAGE_KEY_HISTORY);
    } catch {}
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

  // High-frequency adaptive daemon polling loop (Immediate reaction)
  useEffect(() => {
    let timer: number | null = null;
    let isCancelled = false;

    const poll = async () => {
      try {
        const data = await daemonService.getStatus();
        if (isCancelled) return;

        setIsDaemonOnline(true);
        const rawPeers = data.peers || [];
        const filteredPeers = rawPeers
          .filter(p => p.deviceId && p.deviceId !== data.deviceId && p.ipAddress !== '127.0.0.1')
          .filter((p, idx, arr) => arr.findIndex(x => x.deviceId === p.deviceId) === idx);
        setPeers(filteredPeers);
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

        // Handle cancellation from either device
        if (data.currentProgress && data.currentProgress.state === 7 /* CANCELLED */) {
          if (latestTransferringRef.current) {
            setIsTransferring(false);
            latestTransferringRef.current = false;
            setSelectedPeer(null);
            setQueue(prev => prev.map(item => item.status === 'transferring' ? { ...item, status: 'cancelled' } : item));
            setStatusMessage('Transfer cancelled by peer. Device disconnected.');
          }
        } else if (!data.isTransferring && latestTransferringRef.current) {
          setIsTransferring(false);
          latestTransferringRef.current = false;
          setSelectedPeer(null);
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
        if (now - lastHistorySyncRef.current > 4000 && data.completedHistory && data.completedHistory.length > 0) {
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
        // High responsiveness: 80ms when transferring, 250ms when idle, 80ms when connecting
        const interval = !isDaemonOnline ? 80 : (latestTransferringRef.current ? 80 : 250);
        timer = window.setTimeout(poll, interval);
      }
    };

    pollTriggerRef.current = () => {
      if (timer) clearTimeout(timer);
      poll();
    };

    poll();
    refreshStorage();

    return () => {
      isCancelled = true;
      if (timer) clearTimeout(timer);
      pollTriggerRef.current = null;
    };
  }, [isDaemonOnline, selectedPeer, settings.downloadDirectory, refreshStorage]);

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
    clearQueue,
    clearCompletedQueue,
    clearHistory,
    updateSettings,
    refreshStorage
  };
}

export type AeroSyncStore = ReturnType<typeof useAeroSyncStore>;
