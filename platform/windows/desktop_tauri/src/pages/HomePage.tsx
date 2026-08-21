import React, { useState } from 'react';
import {
  Send,
  FolderUp,
  MonitorSmartphone,
  ArrowUpDown,
  Wifi,
  Sparkles,
  ChevronRight,
  HardDrive,
  UploadCloud,
  FileText,
  RefreshCw,
  Radio,
  Hourglass,
  CheckCircle2,
  AlertCircle,
  XCircle,
  RotateCcw,
  X
} from 'lucide-react';
import { PeerInfo, TransferHistoryRecord, DiskSpace, TransferUiState, DaemonStatusResponse } from '../types/aerosync';
import { formatBytes, formatTimestamp } from '../utils/formatters';
import { tauriBridge } from '../services/tauriBridge';

interface HomePageProps {
  peers: PeerInfo[];
  selectedPeer: PeerInfo | null;
  activeTransferCount: number;
  diskSpace: DiskSpace;
  recentHistory: TransferHistoryRecord[];
  isDaemonOnline: boolean;
  statusMessage: string;
  transferUiState: TransferUiState;
  selectedFiles: { name: string; size: number; path: string }[];
  lastCompletedFile: { name: string; size: number } | null;
  transferErrorMessage: string;
  currentProgress: DaemonStatusResponse['currentProgress'];
  onSendFiles: () => void;
  onSendFolder: () => void;
  onSelectPeer: (peer: PeerInfo) => void;
  onSelectTab: (tab: 'home' | 'devices' | 'transfers' | 'history' | 'settings') => void;
  onFilesDropped: (files: string[]) => void;
  onStartTransfer: () => void;
  onCancelTransfer: () => void;
  onResetTransfer: () => void;
  onRetryTransfer: () => void;
  onClearFiles: () => void;
}

export const HomePage: React.FC<HomePageProps> = ({
  peers,
  selectedPeer,
  activeTransferCount,
  diskSpace,
  recentHistory,
  isDaemonOnline,
  statusMessage,
  transferUiState,
  selectedFiles,
  lastCompletedFile,
  transferErrorMessage,
  currentProgress,
  onSendFiles,
  onSendFolder,
  onSelectPeer,
  onSelectTab,
  onFilesDropped,
  onStartTransfer,
  onCancelTransfer,
  onResetTransfer,
  onRetryTransfer,
  onClearFiles
}) => {
  const [isDragging, setIsDragging] = useState(false);

  const handleDragOver = (e: React.DragEvent) => {
    e.preventDefault();
    setIsDragging(true);
  };

  const handleDragLeave = () => {
    setIsDragging(false);
  };

  const handleDrop = (e: React.DragEvent) => {
    e.preventDefault();
    setIsDragging(false);
    if (e.dataTransfer.files && e.dataTransfer.files.length > 0) {
      const paths = Array.from(e.dataTransfer.files).map(f => (f as any).path || f.name);
      onFilesDropped(paths);
    }
  };

  return (
    <div className="page-container">
      {/* Top Banner / Status Hero */}
      <section className="hero-section">
        <div className="hero-content">
          <div className="hero-badge">
            <Sparkles size={14} className="hero-sparkle" />
            <span>High-Throughput P2P Engine</span>
          </div>
          <h1 className="hero-title">Ultra-Fast LAN File Transfer</h1>
          <p className="hero-subtitle">{statusMessage || 'AeroSync is ready to transmit across PC and Android devices.'}</p>
        </div>

        <div className="hero-actions">
          <button className="btn btn-primary btn-lg" onClick={onSendFiles}>
            <Send size={18} />
            <span>Send Files</span>
          </button>
          <button className="btn btn-secondary btn-lg" onClick={onSendFolder}>
            <FolderUp size={18} />
            <span>Send Folder</span>
          </button>
        </div>
      </section>

      {/* Metrics Row */}
      <section className="metrics-grid">
        <div className="metric-card" onClick={() => onSelectTab('devices')}>
          <div className="metric-icon-box metric-icon-cyan">
            <MonitorSmartphone size={20} />
          </div>
          <div className="metric-info">
            <span className="metric-label">Nearby Devices</span>
            <strong className="metric-value">{peers.length}</strong>
            <span className="metric-subtext">{peers.length === 1 ? '1 peer discovered' : `${peers.length} peers on network`}</span>
          </div>
        </div>

        <div className="metric-card" onClick={() => onSelectTab('transfers')}>
          <div className="metric-icon-box metric-icon-blue">
            <ArrowUpDown size={20} />
          </div>
          <div className="metric-info">
            <span className="metric-label">Active Transfers</span>
            <strong className="metric-value">{activeTransferCount}</strong>
            <span className="metric-subtext">{activeTransferCount > 0 ? 'Pipelined streaming' : 'Idle'}</span>
          </div>
        </div>

        <div className="metric-card">
          <div className="metric-icon-box metric-icon-green">
            <Wifi size={20} />
          </div>
          <div className="metric-info">
            <span className="metric-label">Network Status</span>
            <strong className="metric-value">{isDaemonOnline ? 'Connected' : 'Offline'}</strong>
            <span className="metric-subtext">Direct P2P LAN</span>
          </div>
        </div>

        <div className="metric-card">
          <div className="metric-icon-box metric-icon-purple">
            <HardDrive size={20} />
          </div>
          <div className="metric-info">
            <span className="metric-label">Free Storage</span>
            <strong className="metric-value">{formatBytes(diskSpace.freeBytes)}</strong>
            <span className="metric-subtext">of {formatBytes(diskSpace.totalBytes)}</span>
          </div>
        </div>
      </section>

      {/* Unified Big Circle Transfer Interface */}
      <section className="flex flex-col items-center justify-center py-6 px-4 bg-slate-800/40 rounded-3xl border border-slate-700/60 shadow-xl my-4">
        <div
          className={`relative w-64 h-64 rounded-full flex items-center justify-center transition-transform duration-200 ${
            isDragging ? 'scale-105 ring-4 ring-blue-500' : ''
          }`}
          onDragOver={handleDragOver}
          onDragLeave={handleDragLeave}
          onDrop={handleDrop}
          onClick={() => {
            if (transferUiState === 'IDLE' || transferUiState === 'FILE_SELECTED') {
              onSendFiles();
            }
          }}
          style={{ cursor: transferUiState === 'IDLE' || transferUiState === 'FILE_SELECTED' ? 'pointer' : 'default' }}
        >
          {/* SVG Progress / State Ring */}
          <svg className="absolute inset-0 w-full h-full transform -rotate-90" viewBox="0 0 256 256">
            <circle
              cx="128"
              cy="128"
              r="116"
              stroke="currentColor"
              strokeWidth="6"
              className={`fill-none ${
                transferUiState === 'IDLE' ? 'text-blue-500/20' :
                transferUiState === 'COMPLETED' ? 'text-emerald-500/30' :
                transferUiState === 'FAILED' ? 'text-red-500/30' :
                transferUiState === 'CANCELLED' ? 'text-amber-500/30' :
                'text-indigo-500/20'
              }`}
              strokeDasharray={transferUiState === 'IDLE' ? '8 6' : 'none'}
            />
            {transferUiState === 'TRANSFERRING' && (
              <circle
                cx="128"
                cy="128"
                r="116"
                stroke="url(#bluePurpleGradient)"
                strokeWidth="8"
                strokeLinecap="round"
                className="fill-none transition-all duration-300"
                strokeDasharray={2 * Math.PI * 116}
                strokeDashoffset={(2 * Math.PI * 116) * (1 - (currentProgress.progressPercent || 0) / 100)}
              />
            )}
            {(transferUiState === 'PREPARING' || transferUiState === 'WAITING_FOR_DEVICE' || transferUiState === 'WAITING_FOR_ACCEPT') && (
              <circle
                cx="128"
                cy="128"
                r="116"
                stroke="url(#bluePurpleGradient)"
                strokeWidth="8"
                strokeLinecap="round"
                className="fill-none animate-spin origin-center"
                strokeDasharray={2 * Math.PI * 116 * 0.75}
              />
            )}
            {transferUiState === 'FILE_SELECTED' && (
              <circle
                cx="128"
                cy="128"
                r="116"
                stroke="url(#bluePurpleGradient)"
                strokeWidth="8"
                className="fill-none"
              />
            )}
            {transferUiState === 'COMPLETED' && (
              <circle
                cx="128"
                cy="128"
                r="116"
                stroke="#10B981"
                strokeWidth="8"
                className="fill-none"
              />
            )}
            {transferUiState === 'FAILED' && (
              <circle
                cx="128"
                cy="128"
                r="116"
                stroke="#EF4444"
                strokeWidth="8"
                className="fill-none"
              />
            )}
            {transferUiState === 'CANCELLED' && (
              <circle
                cx="128"
                cy="128"
                r="116"
                stroke="#F59E0B"
                strokeWidth="8"
                className="fill-none"
              />
            )}
            <defs>
              <linearGradient id="bluePurpleGradient" x1="0%" y1="0%" x2="100%" y2="100%">
                <stop offset="0%" stopColor="#3B82F6" />
                <stop offset="100%" stopColor="#8B5CF6" />
              </linearGradient>
            </defs>
          </svg>

          {/* Inner Content */}
          <div className="w-48 h-48 rounded-full bg-slate-900/90 border border-slate-700/60 shadow-2xl flex flex-col items-center justify-center p-4 text-center z-10 backdrop-blur-md">
            {transferUiState === 'IDLE' && (
              <>
                <div className="p-3 rounded-2xl bg-blue-500/10 text-blue-400 mb-2">
                  <UploadCloud size={32} />
                </div>
                <span className="text-base font-bold text-white">Drop files here</span>
                <span className="text-xs text-slate-400 mt-1">or click to pick files</span>
              </>
            )}

            {transferUiState === 'FILE_SELECTED' && (
              <>
                <div className="p-3 rounded-2xl bg-blue-500/10 text-blue-400 mb-1">
                  <FileText size={28} />
                </div>
                <span className="text-sm font-bold text-white truncate max-w-[160px]">
                  {selectedFiles.length === 1 ? selectedFiles[0].name : `${selectedFiles.length} Files Selected`}
                </span>
                <span className="text-xs text-slate-400">
                  {selectedFiles.length === 1 ? formatBytes(selectedFiles[0].size) : formatBytes(selectedFiles.reduce((acc, f) => acc + f.size, 0))}
                </span>
                <span className="text-xs font-semibold text-emerald-400 mt-1">Ready to send</span>
              </>
            )}

            {transferUiState === 'PREPARING' && (
              <>
                <RefreshCw size={36} className="text-blue-400 animate-spin mb-2" />
                <span className="text-sm font-bold text-white">Preparing file...</span>
                <span className="text-xs text-slate-400 mt-1">Getting your file ready</span>
              </>
            )}

            {transferUiState === 'WAITING_FOR_DEVICE' && (
              <>
                <Radio size={36} className="text-cyan-400 animate-pulse mb-2" />
                <span className="text-sm font-bold text-white">Waiting for device...</span>
                <span className="text-xs text-slate-400 mt-1 truncate max-w-[150px]">
                  {selectedPeer ? selectedPeer.deviceName : 'Target Device'}
                </span>
              </>
            )}

            {transferUiState === 'WAITING_FOR_ACCEPT' && (
              <>
                <Hourglass size={34} className="text-purple-400 animate-pulse mb-2" />
                <span className="text-xs font-bold text-white">Waiting for acceptance...</span>
                <span className="text-xs text-slate-400 mt-1 truncate max-w-[150px]">
                  {selectedPeer ? selectedPeer.deviceName : 'Target Device'}
                </span>
              </>
            )}

            {transferUiState === 'TRANSFERRING' && (
              <>
                <span className="text-2xl font-extrabold text-white">
                  {currentProgress.progressPercent}%
                </span>
                <span className="text-xs font-medium text-slate-300 truncate max-w-[150px] mt-1">
                  {currentProgress.currentFileName || 'File'}
                </span>
                <span className="text-xs font-bold text-purple-400">
                  {(currentProgress.speedBytesPerSec / (1024 * 1024)).toFixed(1)} MB/s
                </span>
                <span className="text-[10px] text-slate-400">
                  {formatBytes(currentProgress.fileBytesTransferred)} / {formatBytes(currentProgress.fileSize)}
                </span>
              </>
            )}

            {transferUiState === 'COMPLETED' && (
              <>
                <CheckCircle2 size={38} className="text-emerald-400 mb-1" />
                <span className="text-sm font-bold text-emerald-400">Transfer complete</span>
                <span className="text-xs font-semibold text-white truncate max-w-[150px]">
                  {lastCompletedFile ? lastCompletedFile.name : 'File received'}
                </span>
                <span className="text-[11px] text-slate-400">
                  {lastCompletedFile ? formatBytes(lastCompletedFile.size) : ''}
                </span>
              </>
            )}

            {transferUiState === 'FAILED' && (
              <>
                <AlertCircle size={38} className="text-red-400 mb-1" />
                <span className="text-sm font-bold text-red-400">Transfer failed</span>
                <span className="text-[11px] text-slate-400 truncate max-w-[150px] mt-1">
                  {transferErrorMessage || 'Connection lost'}
                </span>
              </>
            )}

            {transferUiState === 'CANCELLED' && (
              <>
                <XCircle size={38} className="text-amber-400 mb-1" />
                <span className="text-sm font-bold text-amber-400">Transfer cancelled</span>
                <span className="text-xs text-slate-400 mt-1">Operation stopped</span>
              </>
            )}
          </div>
        </div>

        {/* Controls directly below Big Circle */}
        <div className="w-full max-w-sm mt-4 flex flex-col gap-2">
          {transferUiState === 'IDLE' && (
            <button className="btn btn-primary btn-md w-full" onClick={onSendFiles}>
              <FolderUp size={16} />
              <span>Browse Files</span>
            </button>
          )}

          {transferUiState === 'FILE_SELECTED' && (
            <>
              <div className="p-2 rounded-xl bg-slate-800/80 border border-slate-700/80 flex items-center justify-between cursor-pointer" onClick={() => onSelectTab('devices')}>
                <div className="flex items-center gap-2">
                  <MonitorSmartphone size={16} className={selectedPeer ? 'text-emerald-400' : 'text-amber-400'} />
                  <span className="text-xs font-semibold text-slate-200 truncate">
                    {selectedPeer ? `Send to: ${selectedPeer.deviceName}` : 'Select target device'}
                  </span>
                </div>
                <span className="text-xs font-semibold text-blue-400">Change →</span>
              </div>
              <div className="flex gap-2">
                <button className="btn btn-secondary flex-1" onClick={onClearFiles}>
                  <span>Clear</span>
                </button>
                <button className="btn btn-primary flex-[2]" onClick={onStartTransfer}>
                  <Send size={16} />
                  <span>Send File</span>
                </button>
              </div>
            </>
          )}

          {(transferUiState === 'PREPARING' || transferUiState === 'WAITING_FOR_DEVICE' || transferUiState === 'WAITING_FOR_ACCEPT' || transferUiState === 'TRANSFERRING') && (
            <button className="btn btn-danger btn-md w-full" onClick={onCancelTransfer}>
              <X size={16} />
              <span>Cancel Transfer</span>
            </button>
          )}

          {transferUiState === 'COMPLETED' && (
            <button className="btn btn-success btn-md w-full" onClick={onResetTransfer}>
              <CheckCircle2 size={16} />
              <span>Done</span>
            </button>
          )}

          {transferUiState === 'FAILED' && (
            <div className="flex gap-2 w-full">
              <button className="btn btn-secondary flex-1" onClick={onResetTransfer}>
                <span>Close</span>
              </button>
              <button className="btn btn-primary flex-1" onClick={onRetryTransfer}>
                <RotateCcw size={16} />
                <span>Retry</span>
              </button>
            </div>
          )}

          {transferUiState === 'CANCELLED' && (
            <button className="btn btn-secondary btn-md w-full" onClick={onResetTransfer}>
              <span>Clear / Close</span>
            </button>
          )}
        </div>
      </section>

      {/* Nearby Devices Quick Strip */}
      <section className="section-block">
        <div className="section-header">
          <h3 className="section-title">Nearby Devices</h3>
          <button className="section-link-btn" onClick={() => onSelectTab('devices')}>
            <span>View all</span>
            <ChevronRight size={14} />
          </button>
        </div>

        {peers.length === 0 ? (
          <div className="empty-strip">
            <p>Scanning for nearby Windows and Android devices running AeroSync...</p>
          </div>
        ) : (
          <div className="quick-peers-row">
            {peers.slice(0, 4).map(peer => (
              <div
                key={peer.deviceId}
                className="quick-peer-chip"
                onClick={() => onSelectPeer(peer)}
              >
                <div className="quick-peer-avatar">
                  <MonitorSmartphone size={16} />
                </div>
                <div className="quick-peer-meta">
                  <span className="quick-peer-name">{peer.deviceName}</span>
                  <span className="quick-peer-ip">{peer.ipAddress}</span>
                </div>
              </div>
            ))}
          </div>
        )}
      </section>

      {/* Recent Activity */}
      {recentHistory.length > 0 && (
        <section className="section-block">
          <div className="section-header">
            <h3 className="section-title">Recent Activity</h3>
            <button className="section-link-btn" onClick={() => onSelectTab('history')}>
              <span>Full history</span>
              <ChevronRight size={14} />
            </button>
          </div>

          <div className="recent-list">
            {recentHistory.slice(0, 3).map(item => (
              <div key={item.id} className="recent-item">
                <div className="recent-item-info">
                  <span className="recent-item-name">{item.fileName}</span>
                  <span className="recent-item-meta">{formatBytes(item.fileSize)} • {formatTimestamp(item.timestampMs)}</span>
                </div>
                <button
                  className="btn btn-sm btn-ghost"
                  onClick={() => tauriBridge.showInFolder(item.filePath)}
                >
                  Show in Explorer
                </button>
              </div>
            ))}
          </div>
        </section>
      )}
    </div>
  );
};
