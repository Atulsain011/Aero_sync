import React, { useState } from 'react';
import {
  Laptop,
  Smartphone,
  Send,
  Plus,
  ShieldCheck
} from 'lucide-react';
import { PeerInfo } from '../types/aerosync';

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
  const [directIp, setDirectIp] = useState<string>('');

  const handleDirectConnect = (e: React.FormEvent) => {
    e.preventDefault();
    if (!directIp.trim()) return;
    const directPeer: PeerInfo = {
      deviceId: `manual-${directIp.trim()}`,
      deviceName: `Device (${directIp.trim()})`,
      platform: 'unknown',
      deviceType: 0,
      ipAddress: directIp.trim(),
      port: 48124,
      lastSeenMs: Date.now()
    };
    onSendFilesToPeer(directPeer);
    setDirectIp('');
  };

  return (
    <div className="page-screen-card">
      <div className="screen-header-row">
        <div>
          <h2 className="screen-main-title">Connected & Nearby Devices</h2>
          <p className="screen-subtitle">Auto-discovered peers and direct IP connections on local network.</p>
        </div>
        <button
          className="btn-peer-send"
          onClick={onOpenDirectIpModal}
          style={{ background: 'var(--primary-gradient)' }}
        >
          <Plus size={14} />
          <span>Direct IP</span>
        </button>
      </div>

      {/* Direct IP Quick Connect */}
      <form onSubmit={handleDirectConnect} className="direct-ip-section" style={{ marginBottom: '8px' }}>
        <input
          type="text"
          placeholder="Enter IP address to connect directly (e.g. 192.168.1.100)..."
          className="direct-ip-input"
          value={directIp}
          onChange={(e) => setDirectIp(e.target.value)}
        />
        <button type="submit" className="btn-direct-connect">
          Connect & Send
        </button>
      </form>

      {/* Discovered Peers List */}
      {peers.length === 0 ? (
        <div className="empty-state-box" style={{ padding: '40px 16px' }}>
          <div className="accordion-icon-badge green" style={{ width: '56px', height: '56px', marginBottom: '12px' }}>
            <Laptop size={28} />
          </div>
          <h3 className="empty-state-title">No devices currently detected</h3>
          <p className="empty-state-desc">
            Make sure AeroSync is open on your other phone or computer and connected to the same Wi-Fi network or Mobile Hotspot.
          </p>
        </div>
      ) : (
        <div className="peers-list-group">
          {peers.map(peer => {
            const isAndroid = (peer.platform || '').toLowerCase().includes('android') || peer.deviceType === 1;
            const isSelected = selectedPeer?.deviceId === peer.deviceId;

            return (
              <div
                key={peer.deviceId || peer.ipAddress}
                className="peer-item-row"
                style={{
                  borderColor: isSelected ? 'var(--primary-blue)' : 'var(--border-subtle)',
                  padding: '14px 18px'
                }}
                onClick={() => onSelectPeer(peer)}
              >
                <div className="peer-info-left">
                  <div className={`accordion-icon-badge ${isAndroid ? 'green' : 'blue'}`} style={{ width: '40px', height: '40px' }}>
                    {isAndroid ? <Smartphone size={20} /> : <Laptop size={20} />}
                  </div>
                  <div>
                    <h4 className="peer-name-text" style={{ fontSize: '14px' }}>{peer.deviceName || 'AeroSync Device'}</h4>
                    <span className="peer-ip-text">
                      {isAndroid ? 'Android' : 'Windows PC'} • {peer.ipAddress}:{peer.port || 48124}
                    </span>
                  </div>
                </div>

                <button
                  className="btn-peer-send"
                  onClick={(e) => {
                    e.stopPropagation();
                    onSendFilesToPeer(peer);
                  }}
                >
                  <Send size={13} />
                  <span>Send Files</span>
                </button>
              </div>
            );
          })}
        </div>
      )}

      {/* Security note */}
      <div className="security-trust-card" style={{ marginTop: '12px' }}>
        <div className="security-trust-left">
          <div className="security-shield-badge">
            <ShieldCheck size={20} />
          </div>
          <div>
            <div className="security-title-row">
              <span>Direct Peer-to-Peer Transmission</span>
            </div>
            <p className="security-subtitle">Data travels directly between local network interfaces with zero cloud relay.</p>
          </div>
        </div>
      </div>
    </div>
  );
};
