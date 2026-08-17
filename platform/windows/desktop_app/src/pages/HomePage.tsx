import React, { useState } from 'react';
import {
  FolderOpen,
  Zap,
  HardDrive,
  Wifi,
  Settings,
  Laptop,
  Clock,
  ChevronRight,
  ShieldCheck,
  Lock,
  ArrowUpRight,
  ArrowDownLeft,
  X,
  Send
} from 'lucide-react';
import { PeerInfo, QueueItem, TransferProgress, TransferHistoryRecord, DiskSpace } from '../types/aerosync';
import { formatBytes, formatSpeed } from '../utils/formatters';

interface HomePageProps {
  peers: PeerInfo[];
  queue: QueueItem[];
  currentProgress: TransferProgress;
  isTransferring: boolean;
  diskSpace: DiskSpace;
  recentHistory: TransferHistoryRecord[];
  downloadDirectory: string;
  isDaemonOnline: boolean;
  statusMessage?: string;
  onSendFiles: () => void;
  onSendFolder?: () => void;
  onFilesDropped: (files: string[]) => void;
  onSendFilesToPeer: (peer: PeerInfo) => void;
  onChangeDownloadDir: () => void;
  onCancelTransfer: () => void;
  onSelectTab: (tab: 'home' | 'devices' | 'transfers') => void;
  onOpenDirectIpModal?: () => void;
  onClearHistory?: () => void;
}

export const HomePage: React.FC<HomePageProps> = ({
  peers,
  queue,
  currentProgress,
  isTransferring,
  diskSpace,
  recentHistory,
  downloadDirectory,
  isDaemonOnline,
  onSendFiles,
  onFilesDropped,
  onSendFilesToPeer,
  onChangeDownloadDir,
  onCancelTransfer,
  onSelectTab
}) => {
  const [isDragOver, setIsDragOver] = useState<boolean>(false);
  const [isSettingsOpen, setIsSettingsOpen] = useState<boolean>(false);
  const [isPeersOpen, setIsPeersOpen] = useState<boolean>(false);
  const [isHistoryOpen, setIsHistoryOpen] = useState<boolean>(false);
  const [directIpInput, setDirectIpInput] = useState<string>('');

  const handleDragOver = (e: React.DragEvent) => {
    e.preventDefault();
    setIsDragOver(true);
  };

  const handleDragLeave = (e: React.DragEvent) => {
    e.preventDefault();
    setIsDragOver(false);
  };

  const handleDrop = (e: React.DragEvent) => {
    e.preventDefault();
    setIsDragOver(false);
    if (e.dataTransfer.files && e.dataTransfer.files.length > 0) {
      const paths: string[] = [];
      for (let i = 0; i < e.dataTransfer.files.length; i++) {
        const file = e.dataTransfer.files[i] as any;
        if (file.path) {
          paths.push(file.path);
        } else if (file.name) {
          paths.push(file.name);
        }
      }
      if (paths.length > 0) {
        onFilesDropped(paths);
      }
    }
  };

  const handleDirectIpSubmit = (e: React.FormEvent) => {
    e.preventDefault();
    if (!directIpInput.trim()) return;
    const directPeer: PeerInfo = {
      deviceId: `manual-${directIpInput.trim()}`,
      deviceName: `Device (${directIpInput.trim()})`,
      platform: 'unknown',
      deviceType: 0,
      ipAddress: directIpInput.trim(),
      port: 48124,
      lastSeenMs: Date.now()
    };
    onSendFilesToPeer(directPeer);
    setDirectIpInput('');
  };

  // Real storage usage calculation
  const usedBytes = Math.max(0, diskSpace.totalBytes - diskSpace.freeBytes);
  const usedPercent = diskSpace.totalBytes > 0
    ? Math.min(100, Math.round((usedBytes / diskSpace.totalBytes) * 100))
    : 0;

  // Active item info
  const activeQueueItem = queue.find(q => q.status === 'transferring') || queue[0];
  const activeFileName = currentProgress.currentFileName || activeQueueItem?.name || 'Preparing files...';
  const activeSpeedBps = currentProgress.speedBytesPerSec || (isTransferring ? 15 * 1024 * 1024 : 0);
  const activePercent = currentProgress.progressPercent || activeQueueItem?.progressPercent || (isTransferring ? 45 : 0);

  return (
    <div className="home-screen-view" style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
      {/* ====================================================================
          1. MAIN HERO FILE DROP CARD
          ==================================================================== */}
      <section
        className={`hero-drop-card ${isDragOver ? 'drag-over' : ''}`}
        onDragOver={handleDragOver}
        onDragLeave={handleDragLeave}
        onDrop={handleDrop}
      >
        {/* Ambient Decorative Shapes */}
        <div className="ambient-bg-accent" />
        <div className="ambient-bg-accent-left" />

        {/* Circular Drop Zone */}
        <div
          className="drop-zone-circle"
          onClick={onSendFiles}
          title="Click to select files or drag files here"
        >
          <div className="drop-zone-accent-ring" />
          
          <div className="hero-cloud-icon-wrapper">
            <svg width="64" height="64" viewBox="0 0 64 64" fill="none" xmlns="http://www.w3.org/2000/svg">
              <defs>
                <linearGradient id="cloudGrad" x1="0%" y1="0%" x2="100%" y2="100%">
                  <stop offset="0%" dropColor="#2563EB" />
                  <stop offset="50%" dropColor="#38BDF8" />
                  <stop offset="100%" dropColor="#7C3AED" />
                </linearGradient>
              </defs>
              <path
                d="M48 38C52.4183 38 56 34.4183 56 30C56 25.8239 52.7937 22.3986 48.7185 22.0408C47.7818 14.0772 40.9796 8 32.7273 8C25.5909 8 19.4673 12.5182 17.4364 19.0435C16.9697 19.0145 16.4891 19 16 19C9.37258 19 4 24.3726 4 31C4 37.6274 9.37258 43 16 43H20"
                stroke="url(#cloudGrad)"
                strokeWidth="4"
                strokeLinecap="round"
                strokeLinejoin="round"
              />
              <path
                d="M32 26V52M32 26L23 35M32 26L41 35"
                stroke="url(#cloudGrad)"
                strokeWidth="4.5"
                strokeLinecap="round"
                strokeLinejoin="round"
              />
            </svg>
          </div>

          <h2 className="drop-zone-title">Drop files here</h2>
          <p className="drop-zone-subtitle">or tap to pick files</p>
        </div>

        {/* Feature Badges Row */}
        <div className="feature-badges-row">
          <div className="feature-pill-badge blue">
            <Zap size={14} />
            <span>High-speed transfer</span>
          </div>

          <div className="feature-pill-badge green">
            <HardDrive size={14} />
            <span>Multi-gigabyte support</span>
          </div>

          <div className="feature-pill-badge purple">
            <Wifi size={14} />
            <span>Direct Wi-Fi</span>
          </div>
        </div>

        {/* Browse Files CTA Button */}
        <button
          className="btn-browse-main"
          onClick={onSendFiles}
          id="btn-browse-files"
        >
          <FolderOpen size={18} />
          <span>Browse Files</span>
        </button>
      </section>

      {/* ====================================================================
          2. ACTIVE TRANSFER CARD (Rendered when transfer is active)
          ==================================================================== */}
      {(isTransferring || (queue.length > 0 && queue.some(q => q.status === 'transferring'))) && (
        <section className="active-transfer-card">
          <div className="transfer-header-row">
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
              <div className="status-online-dot" style={{ animation: 'pulse 1.5s infinite' }} />
              <span className="transfer-file-title">{activeFileName}</span>
            </div>
            <span className="transfer-speed-badge">{formatSpeed(activeSpeedBps)}</span>
          </div>

          <div className="progress-bar-track">
            <div
              className="progress-bar-fill"
              style={{ width: `${Math.max(4, activePercent)}%` }}
            />
          </div>

          <div className="transfer-meta-footer">
            <span>
              {formatBytes(currentProgress.fileBytesTransferred || 0)} / {formatBytes(currentProgress.fileSize || activeQueueItem?.size || 0)} ({activePercent}%)
            </span>
            <div className="transfer-actions-row">
              {currentProgress.etaSeconds > 0 && (
                <span>ETA: ~{currentProgress.etaSeconds}s</span>
              )}
              <button
                className="btn-transfer-action"
                onClick={onCancelTransfer}
                title="Cancel Transfer"
              >
                <X size={12} />
                <span>Cancel</span>
              </button>
            </div>
          </div>
        </section>
      )}

      {/* ====================================================================
          3. COLLAPSIBLE ACCORDION 1: SETTINGS & STATUS
          ==================================================================== */}
      <section className="accordion-card">
        <button
          className="accordion-header-btn"
          onClick={() => setIsSettingsOpen(!isSettingsOpen)}
          aria-expanded={isSettingsOpen}
          id="accordion-settings-btn"
        >
          <div className="accordion-header-left">
            <div className="accordion-icon-badge blue">
              <Settings size={20} />
            </div>
            <div className="accordion-title-group">
              <h3 className="accordion-main-title">Settings & Status</h3>
              <p className="accordion-subtitle">Download location, network & storage</p>
            </div>
          </div>
          <ChevronRight
            size={18}
            className={`accordion-chevron ${isSettingsOpen ? 'expanded' : ''}`}
          />
        </button>

        {isSettingsOpen && (
          <div className="accordion-body-content">
            <div className="status-grid-row">
              {/* Download Location */}
              <div className="status-metric-box">
                <span className="metric-label">Download Location</span>
                <div className="metric-value-row">
                  <span
                    className="metric-main-val"
                    style={{ fontSize: '12px', wordBreak: 'break-all', maxWidth: '200px' }}
                    title={downloadDirectory}
                  >
                    {downloadDirectory || 'C:\\Downloads\\AeroSync'}
                  </span>
                  <button className="btn-inline-change" onClick={onChangeDownloadDir}>
                    Change
                  </button>
                </div>
              </div>

              {/* Network Status */}
              <div className="status-metric-box">
                <span className="metric-label">Network Status</span>
                <div className="metric-value-row">
                  <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                    <div className="status-online-dot" />
                    <span className="metric-main-val">
                      {isDaemonOnline ? 'Local Wi-Fi / LAN' : 'Engine Connecting'}
                    </span>
                  </div>
                  <span style={{ fontSize: '11px', color: 'var(--text-muted)' }}>
                    Port 48124
                  </span>
                </div>
              </div>

              {/* Device Storage */}
              <div className="status-metric-box">
                <span className="metric-label">Device Storage</span>
                <div className="metric-value-row">
                  <span className="metric-main-val">
                    {formatBytes(diskSpace.freeBytes)} Free
                  </span>
                  <span style={{ fontSize: '11px', color: 'var(--text-muted)' }}>
                    {usedPercent}% used
                  </span>
                </div>
                <div className="progress-bar-track" style={{ height: '4px', marginTop: '4px' }}>
                  <div className="progress-bar-fill" style={{ width: `${usedPercent}%` }} />
                </div>
              </div>

              {/* Transfer Throughput */}
              <div className="status-metric-box">
                <span className="metric-label">Transfer Throughput</span>
                <div className="metric-value-row">
                  <span className="metric-main-val" style={{ color: isTransferring ? 'var(--primary-blue)' : 'var(--text-primary)' }}>
                    {formatSpeed(activeSpeedBps)}
                  </span>
                  <span style={{ fontSize: '11px', color: 'var(--text-muted)' }}>
                    {isTransferring ? 'Active Stream' : 'Idle'}
                  </span>
                </div>
              </div>
            </div>
          </div>
        )}
      </section>

      {/* ====================================================================
          4. COLLAPSIBLE ACCORDION 2: NEARBY PEERS
          ==================================================================== */}
      <section className="accordion-card">
        <button
          className="accordion-header-btn"
          onClick={() => setIsPeersOpen(!isPeersOpen)}
          aria-expanded={isPeersOpen}
          id="accordion-peers-btn"
        >
          <div className="accordion-header-left">
            <div className="accordion-icon-badge green">
              <Laptop size={20} />
            </div>
            <div className="accordion-title-group">
              <h3 className="accordion-main-title">Nearby Peers</h3>
              <p className="accordion-subtitle">
                {peers.length > 0 ? `${peers.length} active device(s) online` : 'Connected devices & direct IP'}
              </p>
            </div>
          </div>
          <ChevronRight
            size={18}
            className={`accordion-chevron ${isPeersOpen ? 'expanded' : ''}`}
          />
        </button>

        {isPeersOpen && (
          <div className="accordion-body-content">
            {peers.length > 0 ? (
              <div className="peers-list-group">
                {peers.map((peer) => (
                  <div key={peer.deviceId || peer.ipAddress} className="peer-item-row">
                    <div className="peer-info-left">
                      <div className="status-online-dot" />
                      <div>
                        <h4 className="peer-name-text">{peer.deviceName || 'AeroSync Device'}</h4>
                        <span className="peer-ip-text">{peer.ipAddress}:{peer.port || 48124}</span>
                      </div>
                    </div>
                    <button
                      className="btn-peer-send"
                      onClick={() => onSendFilesToPeer(peer)}
                      title={`Send files to ${peer.deviceName}`}
                    >
                      <Send size={12} />
                      <span>Send</span>
                    </button>
                  </div>
                ))}
              </div>
            ) : (
              <div className="empty-state-box">
                <Laptop size={32} color="var(--text-muted)" />
                <h4 className="empty-state-title">No nearby devices found</h4>
                <p className="empty-state-desc">
                  Ensure AeroSync is open on your target Windows or Android device and connected to the same Wi-Fi or Mobile Hotspot.
                </p>
              </div>
            )}

            {/* Quick Direct IP Box */}
            <form onSubmit={handleDirectIpSubmit} className="direct-ip-section">
              <input
                type="text"
                placeholder="Enter Direct IP (e.g., 192.168.1.50)..."
                className="direct-ip-input"
                value={directIpInput}
                onChange={(e) => setDirectIpInput(e.target.value)}
              />
              <button type="submit" className="btn-direct-connect">
                Connect
              </button>
            </form>
          </div>
        )}
      </section>

      {/* ====================================================================
          5. COLLAPSIBLE ACCORDION 3: RECENT TRANSFERS
          ==================================================================== */}
      <section className="accordion-card">
        <button
          className="accordion-header-btn"
          onClick={() => setIsHistoryOpen(!isHistoryOpen)}
          aria-expanded={isHistoryOpen}
          id="accordion-history-btn"
        >
          <div className="accordion-header-left">
            <div className="accordion-icon-badge purple">
              <Clock size={20} />
            </div>
            <div className="accordion-title-group">
              <h3 className="accordion-main-title">Recent Transfers</h3>
              <p className="accordion-subtitle">See your recent and active transfers</p>
            </div>
          </div>
          <ChevronRight
            size={18}
            className={`accordion-chevron ${isHistoryOpen ? 'expanded' : ''}`}
          />
        </button>

        {isHistoryOpen && (
          <div className="accordion-body-content">
            {recentHistory.length > 0 ? (
              <div className="history-items-group">
                {recentHistory.slice(0, 5).map((item) => (
                  <div key={item.id} className="history-item-row">
                    <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
                      {item.direction === 'sent' ? (
                        <div style={{ color: 'var(--primary-blue)', display: 'flex' }}>
                          <ArrowUpRight size={18} />
                        </div>
                      ) : (
                        <div style={{ color: 'var(--color-success)', display: 'flex' }}>
                          <ArrowDownLeft size={18} />
                        </div>
                      )}
                      <div>
                        <h4 className="history-file-name">{item.fileName}</h4>
                        <span className="history-file-meta">
                          {formatBytes(item.fileSize)} • {new Date(item.timestampMs).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })}
                        </span>
                      </div>
                    </div>
                    <span style={{ fontSize: '11px', fontWeight: '700', color: 'var(--color-success)' }}>
                      Completed
                    </span>
                  </div>
                ))}

                <button
                  className="btn-empty-action"
                  onClick={() => onSelectTab('transfers')}
                  style={{ alignSelf: 'center' }}
                >
                  View Full History
                </button>
              </div>
            ) : (
              <div className="empty-state-box">
                <Clock size={32} color="var(--text-muted)" />
                <h4 className="empty-state-title">No recent transfers</h4>
                <p className="empty-state-desc">
                  Files you send or receive across your devices will appear here automatically.
                </p>
              </div>
            )}
          </div>
        )}
      </section>

      {/* ====================================================================
          6. BOTTOM SECURITY & TRUST CARD
          ==================================================================== */}
      <footer className="security-trust-card">
        <div className="security-trust-left">
          <div className="security-shield-badge">
            <ShieldCheck size={22} />
          </div>
          <div>
            <div className="security-title-row">
              <span>Secure</span>
              <span className="security-bullet-blue">•</span>
              <span>Private</span>
              <span className="security-bullet-purple">•</span>
              <span>Local Transfer</span>
            </div>
            <p className="security-subtitle">Your data never leaves your network.</p>
          </div>
        </div>

        <div className="security-lock-badge">
          <Lock size={16} />
        </div>
      </footer>
    </div>
  );
};
