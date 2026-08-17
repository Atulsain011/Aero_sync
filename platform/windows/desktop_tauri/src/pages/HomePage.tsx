import React, { useState } from 'react';
import {
  Send,
  FolderUp,
  MonitorSmartphone,
  ArrowUpDown,
  Wifi,
  Sparkles,
  ChevronRight,
  HardDrive
} from 'lucide-react';
import { PeerInfo, TransferHistoryRecord, DiskSpace } from '../types/aerosync';
import { formatBytes, formatTimestamp } from '../utils/formatters';
import { tauriBridge } from '../services/tauriBridge';

interface HomePageProps {
  peers: PeerInfo[];
  activeTransferCount: number;
  diskSpace: DiskSpace;
  recentHistory: TransferHistoryRecord[];
  isDaemonOnline: boolean;
  statusMessage: string;
  onSendFiles: () => void;
  onSendFolder: () => void;
  onSelectPeer: (peer: PeerInfo) => void;
  onSelectTab: (tab: 'home' | 'devices' | 'transfers' | 'history' | 'settings') => void;
  onFilesDropped: (files: string[]) => void;
}

export const HomePage: React.FC<HomePageProps> = ({
  peers,
  activeTransferCount,
  diskSpace,
  recentHistory,
  isDaemonOnline,
  statusMessage,
  onSendFiles,
  onSendFolder,
  onSelectPeer,
  onSelectTab,
  onFilesDropped
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

      {/* Drag and Drop Zone */}
      <section
        className={`dropzone-card ${isDragging ? 'dropzone-active' : ''}`}
        onDragOver={handleDragOver}
        onDragLeave={handleDragLeave}
        onDrop={handleDrop}
        onClick={onSendFiles}
      >
        <div className="dropzone-inner">
          <div className="dropzone-icon-circle">
            <Send size={24} />
          </div>
          <h3 className="dropzone-heading">Drag & drop files or folders here</h3>
          <p className="dropzone-desc">Or click anywhere in this box to browse local storage</p>
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
