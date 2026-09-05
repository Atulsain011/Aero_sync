import React from 'react';
import {
  Monitor,
  Smartphone,
  Send,
  Plus,
  RefreshCw,
  Cpu,
  AlertTriangle,
  Radio
} from 'lucide-react';
import { PeerInfo } from '../types/aerosync';
import { formatTimestamp } from '../utils/formatters';

interface DevicesPageProps {
  peers: PeerInfo[];
  selectedPeer: PeerInfo | null;
  isDaemonOnline: boolean;
  isRestartingDaemon: boolean;
  daemonError: string | null;
  onRestartDaemon: () => void;
  onSelectPeer: (peer: PeerInfo) => void;
  onSendFilesToPeer: (peer: PeerInfo) => void;
  onOpenDirectIpModal: () => void;
}

export const DevicesPage: React.FC<DevicesPageProps> = ({
  peers,
  selectedPeer,
  isDaemonOnline,
  isRestartingDaemon,
  daemonError,
  onRestartDaemon,
  onSelectPeer,
  onSendFilesToPeer,
  onOpenDirectIpModal
}) => {
  const getDevicePlatform = (peer: PeerInfo) => {
    const p = (peer.platform || '').toLowerCase();
    if (p.includes('android') || peer.deviceType === 1) {
      return { label: 'Android', avatarClass: 'avatar-android', icon: <Smartphone size={24} /> };
    }
    if (p.includes('linux')) {
      return { label: 'Linux PC', avatarClass: 'avatar-linux', icon: <Monitor size={24} /> };
    }
    if (p.includes('darwin') || p.includes('mac') || p.includes('ios')) {
      return { label: 'Apple Device', avatarClass: 'avatar-apple', icon: <Monitor size={24} /> };
    }
    return { label: 'Windows PC', avatarClass: 'avatar-windows', icon: <Monitor size={24} /> };
  };

  return (
    <div className="page-container">
      {/* AeroSync Core Engine Status Banner */}
      <div className={`devices-core-banner ${isDaemonOnline ? 'banner-running' : 'banner-stopped'}`}>
        <div className="devices-core-banner-main">
          <div className="devices-core-title-group">
            <span className="devices-core-name">AeroSync Core</span>
            <span className={`devices-core-status-pill ${isDaemonOnline ? 'pill-online' : 'pill-offline'}`}>
              <span className="core-dot">{isDaemonOnline ? '●' : '✕'}</span>
              <span>{isDaemonOnline ? 'Running' : 'Not running'}</span>
            </span>
          </div>
          <p className="devices-core-desc">
            {isDaemonOnline
              ? 'Discovery backend active on UDP port 48123 (multicast 239.255.48.123 & broadcast). Local IPC port 48126 responding.'
              : (daemonError ? `Error: ${daemonError}` : 'The native discovery & transfer daemon is not responding on 127.0.0.1:48126. UDP discovery and peer detection are stopped.')}
          </p>
        </div>
        <button
          className={`btn ${isDaemonOnline ? 'btn-secondary' : 'btn-danger'}`}
          onClick={onRestartDaemon}
          disabled={isRestartingDaemon}
        >
          <RefreshCw size={15} className={isRestartingDaemon ? 'animate-spin' : ''} />
          <span>{isRestartingDaemon ? 'Starting...' : (isDaemonOnline ? 'Restart Core' : 'Start Core Engine')}</span>
        </button>
      </div>

      <div className="page-header">
        <div>
          <div className="devices-header-title-row">
            <h2 className="page-title">Nearby Devices</h2>
            {isDaemonOnline && (
              <span className="discovery-searching-badge">
                <span className="searching-pulse-dot"></span>
                <span>Searching for AeroSync devices...</span>
              </span>
            )}
          </div>
          <p className="page-subtitle">Auto-discovered AeroSync instances active on the local subnet & hotspot.</p>
        </div>
        <div className="page-header-actions">
          <button className="btn btn-secondary" onClick={onOpenDirectIpModal}>
            <Plus size={16} />
            <span>Connect Direct IP</span>
          </button>
        </div>
      </div>

      {peers.length === 0 ? (
        !isDaemonOnline ? (
          <div className="empty-state-card empty-state-offline">
            <div className="empty-icon-circle empty-icon-offline">
              <AlertTriangle size={32} />
            </div>
            <div className="empty-core-badge-row">
              <span className="empty-core-title">AeroSync Core</span>
              <span className="empty-core-state">✕ Not running</span>
            </div>
            <h3 className="empty-state-title">Discovery Backend Unavailable</h3>
            <p className="empty-state-text">
              {daemonError || 'The native AeroSync core daemon is offline or stopped. Start the core engine to enable device discovery on this network.'}
            </p>
            <button
              className="btn btn-primary"
              onClick={onRestartDaemon}
              disabled={isRestartingDaemon}
            >
              <RefreshCw size={16} className={isRestartingDaemon ? 'animate-spin' : ''} />
              <span>{isRestartingDaemon ? 'Starting Engine...' : 'Start Core Engine'}</span>
            </button>
          </div>
        ) : (
          <div className="empty-state-card">
            <div className="empty-icon-circle">
              <Radio size={32} className="radar-pulse" />
            </div>
            <h3 className="empty-state-title">Searching for AeroSync devices...</h3>
            <p className="empty-state-notice">No AeroSync devices found on this network.</p>
            <p className="empty-state-text">
              Ensure recipient devices have AeroSync open on Linux, Windows, or Android and are connected to the same Wi-Fi, Ethernet, or Mobile Hotspot.
            </p>
            <button className="btn btn-primary" onClick={onOpenDirectIpModal}>
              <Plus size={16} />
              <span>Connect Target IP Manually</span>
            </button>
          </div>
        )
      ) : (
        <div className="devices-grid">
          {peers.map(peer => {
            const platformInfo = getDevicePlatform(peer);
            const isSelected = selectedPeer?.deviceId === peer.deviceId;

            return (
              <div
                key={peer.deviceId}
                className={`device-card ${isSelected ? 'device-card-selected' : ''}`}
                onClick={() => onSelectPeer(peer)}
              >
                <div className="device-card-header">
                  <div className={`device-avatar ${platformInfo.avatarClass}`}>
                    {platformInfo.icon}
                  </div>
                  <div className="device-meta">
                    <h4 className="device-name">{peer.deviceName || 'AeroSync Device'}</h4>
                    <span className="device-platform-badge">
                      {platformInfo.label}
                    </span>
                  </div>
                </div>

                <div className="device-card-body">
                  <div className="device-spec-row">
                    <span className="spec-label">IP Address</span>
                    <span className="spec-value">{peer.ipAddress}:{peer.port || 48124}</span>
                  </div>
                  <div className="device-spec-row">
                    <span className="spec-label">Engine Protocol</span>
                    <span className="spec-value">4-Stream Turbo TCP</span>
                  </div>
                  <div className="device-spec-row">
                    <span className="spec-label">Last Seen</span>
                    <span className="spec-value">{formatTimestamp(peer.lastSeenMs || Date.now())}</span>
                  </div>
                </div>

                <div className="device-card-footer">
                  <button
                    className={`btn ${isSelected ? 'btn-primary' : 'btn-secondary'} btn-full`}
                    onClick={(e) => {
                      e.stopPropagation();
                      onSendFilesToPeer(peer);
                    }}
                  >
                    <Send size={15} />
                    <span>Send Files</span>
                  </button>
                </div>
              </div>
            );
          })}
        </div>
      )}

      {/* Network protocol footnote */}
      <div className="info-callout">
        <div className="callout-icon">
          <Cpu size={18} />
        </div>
        <div className="callout-content">
          <h4>Peer-to-Peer Zero-Cloud Direct Transmission</h4>
          <p>Files travel directly across your local network hardware without touching any external servers. Transfer speeds reach the maximum physical bandwidth of your Wi-Fi router or Mobile Hotspot.</p>
        </div>
      </div>
    </div>
  );
};
