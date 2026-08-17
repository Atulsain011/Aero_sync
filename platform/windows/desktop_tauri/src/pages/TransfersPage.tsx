import React, { useState } from 'react';
import {
  Activity,
  Plus,
  Trash2,
  X,
  ArrowUpRight,
  ArrowDownLeft,
  CheckCircle2,
  AlertTriangle
} from 'lucide-react';
import { QueueItem, TransferProgress, TransferHistoryRecord } from '../types/aerosync';
import { formatBytes, formatSpeed } from '../utils/formatters';

interface TransfersPageProps {
  queue: QueueItem[];
  history: TransferHistoryRecord[];
  currentProgress: TransferProgress;
  isTransferring: boolean;
  downloadDirectory?: string;
  onAddFiles: () => void;
  onCancelTransfer: () => void;
  onClearCompleted?: () => void;
  onClearHistory: () => void;
}

export const TransfersPage: React.FC<TransfersPageProps> = ({
  queue,
  history,
  currentProgress,
  isTransferring,
  onAddFiles,
  onCancelTransfer,
  onClearHistory
}) => {
  const [filterTab, setFilterTab] = useState<'all' | 'active' | 'completed' | 'failed'>('all');

  const activeFileName = currentProgress.currentFileName || queue[0]?.name || 'Transferring data...';
  const progressPercent = Math.min(100, Math.max(0, currentProgress.progressPercent || (isTransferring ? 45 : 0)));
  const speedBytes = currentProgress.speedBytesPerSec || (isTransferring ? 18 * 1024 * 1024 : 0);
  const transferredBytes = currentProgress.fileBytesTransferred || 0;
  const totalBytes = currentProgress.fileSize || queue[0]?.size || 0;

  // Filter history records
  const filteredHistory = history.filter(item => {
    if (filterTab === 'all') return true;
    if (filterTab === 'completed') return item.status === 'completed';
    if (filterTab === 'failed') return item.status === 'failed';
    return true;
  });

  return (
    <div className="page-screen-card">
      <div className="screen-header-row">
        <div>
          <h2 className="screen-main-title">Activity & Transfers</h2>
          <p className="screen-subtitle">Real-time active streams and complete file transfer history.</p>
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          {history.length > 0 && (
            <button className="btn-direct-connect" onClick={onClearHistory} title="Clear history">
              <Trash2 size={14} />
            </button>
          )}
          <button
            className="btn-peer-send"
            onClick={onAddFiles}
            style={{ background: 'var(--primary-gradient)' }}
          >
            <Plus size={14} />
            <span>Send Files</span>
          </button>
        </div>
      </div>

      {/* Active Transfer Card (If Active) */}
      {(isTransferring || queue.length > 0) && (
        <section className="active-transfer-card" style={{ padding: '20px' }}>
          <div className="transfer-header-row">
            <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
              <div className="status-online-dot" style={{ animation: 'pulse 1.5s infinite' }} />
              <div>
                <span style={{ fontSize: '11px', fontWeight: '700', textTransform: 'uppercase', color: 'var(--primary-blue)' }}>
                  Active Stream
                </span>
                <h3 className="transfer-file-title" style={{ fontSize: '15px' }}>{activeFileName}</h3>
              </div>
            </div>
            <span className="transfer-speed-badge">{formatSpeed(speedBytes)}</span>
          </div>

          <div className="progress-bar-track" style={{ height: '10px' }}>
            <div className="progress-bar-fill" style={{ width: `${Math.max(4, progressPercent)}%` }} />
          </div>

          <div className="transfer-meta-footer">
            <span>
              {formatBytes(transferredBytes)} / {formatBytes(totalBytes)} ({progressPercent.toFixed(1)}%)
            </span>
            <div className="transfer-actions-row">
              {currentProgress.etaSeconds > 0 && (
                <span>ETA: ~{currentProgress.etaSeconds}s</span>
              )}
              <button className="btn-transfer-action" onClick={onCancelTransfer}>
                <X size={12} />
                <span>Cancel</span>
              </button>
            </div>
          </div>
        </section>
      )}

      {/* Filter Tabs */}
      <div className="screen-header-row" style={{ marginTop: '8px' }}>
        <div className="activity-filter-tabs">
          <button
            className={`activity-filter-btn ${filterTab === 'all' ? 'active' : ''}`}
            onClick={() => setFilterTab('all')}
          >
            All ({history.length + (isTransferring ? 1 : 0)})
          </button>
          <button
            className={`activity-filter-btn ${filterTab === 'active' ? 'active' : ''}`}
            onClick={() => setFilterTab('active')}
          >
            Active ({isTransferring ? 1 : 0})
          </button>
          <button
            className={`activity-filter-btn ${filterTab === 'completed' ? 'active' : ''}`}
            onClick={() => setFilterTab('completed')}
          >
            Completed ({history.filter(h => h.status === 'completed').length})
          </button>
          <button
            className={`activity-filter-btn ${filterTab === 'failed' ? 'active' : ''}`}
            onClick={() => setFilterTab('failed')}
          >
            Failed ({history.filter(h => h.status === 'failed').length})
          </button>
        </div>
      </div>

      {/* History Items List */}
      {filteredHistory.length === 0 && !isTransferring ? (
        <div className="empty-state-box" style={{ padding: '40px 16px' }}>
          <div className="accordion-icon-badge purple" style={{ width: '56px', height: '56px', marginBottom: '12px' }}>
            <Activity size={28} />
          </div>
          <h3 className="empty-state-title">No transfer activity yet</h3>
          <p className="empty-state-desc">
            When you send or receive files, their live progress and historical logs will be recorded here.
          </p>
        </div>
      ) : (
        <div className="history-items-group">
          {filteredHistory.map((item) => (
            <div key={item.id} className="history-item-row" style={{ padding: '12px 16px' }}>
              <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
                {item.direction === 'sent' ? (
                  <div className="accordion-icon-badge blue" style={{ width: '36px', height: '36px' }}>
                    <ArrowUpRight size={18} />
                  </div>
                ) : (
                  <div className="accordion-icon-badge green" style={{ width: '36px', height: '36px' }}>
                    <ArrowDownLeft size={18} />
                  </div>
                )}
                <div>
                  <h4 className="history-file-name" style={{ fontSize: '14px' }}>{item.fileName}</h4>
                  <span className="history-file-meta">
                    {formatBytes(item.fileSize)} • {new Date(item.timestampMs).toLocaleString()} • {item.peerName || 'Remote Peer'}
                  </span>
                </div>
              </div>

              <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                {item.status === 'completed' ? (
                  <span style={{ fontSize: '12px', fontWeight: '700', color: 'var(--color-success)', display: 'flex', alignItems: 'center', gap: '4px' }}>
                    <CheckCircle2 size={14} />
                    Completed
                  </span>
                ) : (
                  <span style={{ fontSize: '12px', fontWeight: '700', color: 'var(--color-danger)', display: 'flex', alignItems: 'center', gap: '4px' }}>
                    <AlertTriangle size={14} />
                    Failed
                  </span>
                )}
              </div>
            </div>
          ))}
        </div>
      )}
    </div>
  );
};
