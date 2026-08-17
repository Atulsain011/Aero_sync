import React from 'react';
import { ShieldCheck, Monitor, Smartphone, Check, X } from 'lucide-react';
import { IncomingRequest } from '../../types/aerosync';
import { formatBytes } from '../../utils/formatters';

interface IncomingRequestModalProps {
  request: IncomingRequest | null;
  onAccept: () => void;
  onDecline: () => void;
}

export const IncomingRequestModal: React.FC<IncomingRequestModalProps> = ({
  request,
  onAccept,
  onDecline
}) => {
  if (!request) return null;

  const isAndroid = request.platform.toLowerCase().includes('android');

  return (
    <div className="modal-backdrop">
      <div className="modal-card">
        <div className="modal-header">
          <div className="modal-icon-badge">
            <ShieldCheck size={24} className="modal-shield-icon" />
          </div>
          <div>
            <h3 className="modal-title">Incoming Connection Request</h3>
            <p className="modal-subtitle">A nearby device wants to connect and share files with you.</p>
          </div>
        </div>

        <div className="modal-body">
          <div className="peer-request-card">
            <div className="peer-avatar">
              {isAndroid ? <Smartphone size={24} /> : <Monitor size={24} />}
            </div>
            <div className="peer-details">
              <h4 className="peer-name">{request.senderName}</h4>
              <p className="peer-ip">{request.senderIp} • {isAndroid ? 'Android Phone' : 'Windows PC'}</p>
            </div>
          </div>

          {request.pairingPin && (
            <div className="pin-verification-box">
              <span className="pin-label">Security PIN Code</span>
              <span className="pin-value">{request.pairingPin}</span>
              <p className="pin-hint">Ensure this 6-digit PIN matches the code shown on the sender device.</p>
            </div>
          )}

          {request.totalFiles !== undefined && request.totalFiles > 0 && (
            <div className="transfer-summary-box">
              <span>Transfer Payload:</span>
              <strong>{request.totalFiles} file(s) ({formatBytes(request.totalBytes || 0)})</strong>
            </div>
          )}
        </div>

        <div className="modal-footer">
          <button className="btn btn-secondary" onClick={onDecline}>
            <X size={16} />
            <span>Decline</span>
          </button>
          <button className="btn btn-primary" onClick={onAccept}>
            <Check size={16} />
            <span>Accept & Connect</span>
          </button>
        </div>
      </div>
    </div>
  );
};
