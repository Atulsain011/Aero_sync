import React from 'react';
import {
  Monitor,
  Smartphone,
  Send,
  Plus,
  RefreshCw,
  CheckCircle2,
  Cpu
} from 'lucide-react';
import { PeerInfo } from '../types/aerosync';
import { formatTimestamp } from '../utils/formatters';

interface DevicesPageProps {
  peers: PeerInfo[];
  selectedPeer: PeerInfo | null;
  onSelectPeer: (peer: PeerInfo) => void;
  onSendFilesToPeer: (peer: PeerInfo) => void;
  onOpenDirectIpModal: () => void;
}

export const DevicesPage: React.FC<DevicesPageProps> = ({
  peers,
  selectedPeer,
  onSelectPeer,
  onSendFilesToPeer,
  onOpenDirectIpModal
}) => {
  return (
    <div className="page-container">
      <div className="page-header">
        <div>
          <h2 className="page-title">Discovered Devices</h2>
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
        <div className="empty-state-card">
          <div className="empty-icon-circle">
            <RefreshCw size={32} className="animate-spin-slow" />
          </div>
          <h3 className="empty-state-title">Searching for Devices...</h3>
          <p className="empty-state-text">
            Ensure the recipient has AeroSync open on Windows or Android and is connected to the same Wi-Fi or Mobile Hotspot.
          </p>
          <button className="btn btn-primary" onClick={onOpenDirectIpModal}>
            <Plus size={16} />
            <span>Enter Target IP Manually</span>
          </button>
        </div>
      ) : (
        <div className="devices-grid">
          {peers.map(peer => {
            const isAndroid = peer.platform.toLowerCase().includes('android');
            const isSelected = selectedPeer?.deviceId === peer.deviceId;

            return (
              <div
                key={peer.deviceId}
                className={`device-card ${isSelected ? 'device-card-selected' : ''}`}
                onClick={() => onSelectPeer(peer)}
              >
                <div className="device-card-header">
                  <div className={`device-avatar ${isAndroid ? 'avatar-android' : 'avatar-windows'}`}>
                    {isAndroid ? <Smartphone size={24} /> : <Monitor size={24} />}
                  </div>
                  <div className="device-meta">
                    <h4 className="device-name">{peer.deviceName}</h4>
                    <span className="device-platform-badge">
                      {isAndroid ? 'Android' : 'Windows PC'}
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
                    <span className="spec-value">{formatTimestamp(peer.lastSeenMs)}</span>
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
