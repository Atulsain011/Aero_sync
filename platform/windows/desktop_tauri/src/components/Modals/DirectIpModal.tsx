import React, { useState } from 'react';
import { Network, X, ArrowRight } from 'lucide-react';

interface DirectIpModalProps {
  isOpen: boolean;
  onClose: () => void;
  onConnect: (ip: string, port: number) => void;
}

export const DirectIpModal: React.FC<DirectIpModalProps> = ({
  isOpen,
  onClose,
  onConnect
}) => {
  const [ip, setIp] = useState('');
  const [port, setPort] = useState(48124);
  const [error, setError] = useState('');

  if (!isOpen) return null;

  const handleSubmit = (e: React.FormEvent) => {
    e.preventDefault();
    const cleanIp = ip.trim();
    if (!cleanIp) {
      setError('Please enter a valid IP address');
      return;
    }
    setError('');
    onConnect(cleanIp, port || 48124);
    onClose();
  };

  return (
    <div className="modal-backdrop">
      <div className="modal-card">
        <div className="modal-header">
          <div className="modal-icon-badge">
            <Network size={22} />
          </div>
          <div>
            <h3 className="modal-title">Connect via Direct IP</h3>
            <p className="modal-subtitle">Connect manually to a device by entering its LAN or Hotspot IP.</p>
          </div>
        </div>

        <form onSubmit={handleSubmit}>
          <div className="modal-body">
            <div className="form-group">
              <label className="form-label">Target IPv4 Address</label>
              <input
                type="text"
                className="form-input"
                placeholder="e.g. 192.168.43.1 or 192.168.1.105"
                value={ip}
                onChange={e => {
                  setIp(e.target.value);
                  setError('');
                }}
                autoFocus
              />
            </div>

            <div className="form-group">
              <label className="form-label">Control Port</label>
              <input
                type="number"
                className="form-input"
                value={port}
                onChange={e => setPort(parseInt(e.target.value, 10) || 48124)}
              />
            </div>

            {error && <p className="form-error">{error}</p>}
          </div>

          <div className="modal-footer">
            <button type="button" className="btn btn-secondary" onClick={onClose}>
              <X size={16} />
              <span>Cancel</span>
            </button>
            <button type="submit" className="btn btn-primary">
              <span>Connect</span>
              <ArrowRight size={16} />
            </button>
          </div>
        </form>
      </div>
    </div>
  );
};
