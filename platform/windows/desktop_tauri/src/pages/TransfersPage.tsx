import React from 'react';
import {
  X,
  Plus,
  Trash2,
  FileText,
  FilePlus,
  Activity,
  Zap,
  Clock,
  ArrowDownCircle,
  CheckCircle2
} from 'lucide-react';
import { QueueItem, DaemonStatusResponse } from '../types/aerosync';
import {
  formatBytes,
  formatSpeedMBs,
  formatSpeedMbps,
  formatEta
} from '../utils/formatters';

interface TransfersPageProps {
  queue: QueueItem[];
  currentProgress: DaemonStatusResponse['currentProgress'];
  isTransferring: boolean;
  historyCount?: number;
  onAddFiles: () => void;
  onNewFile?: () => void;
  onCancelTransfer: () => void;
  onClearQueue: () => void;
  onClearHistory?: () => void;
}

export const TransfersPage: React.FC<TransfersPageProps> = ({
  queue,
  currentProgress,
  isTransferring,
  historyCount = 0,
  onAddFiles,
  onNewFile,
  onCancelTransfer,
  onClearQueue,
  onClearHistory
}) => {
  const activeFileName = currentProgress?.currentFileName || (queue[0]?.name ?? 'Preparing files...');
  const progressPercent = Math.min(100, Math.max(0, currentProgress?.progressPercent || 0));
  const speedBytes = currentProgress?.speedBytesPerSec || 0;
  const etaSec = currentProgress?.etaSeconds || 0;
  const transferredBytes = currentProgress?.fileBytesTransferred || 0;
  const totalBytes = currentProgress?.fileSize || 0;

  const handleNewFileClick = onNewFile || onAddFiles;

  return (
    <div className="page-container">
      <div className="page-header">
        <div>
          <h2 className="page-title">Transfer Queue</h2>
          <p className="page-subtitle">Real-time parallel multi-stream transmission progress.</p>
        </div>
        <div className="page-header-actions queue-action-bar">
          <button
            className="btn btn-action btn-secondary"
            onClick={onClearQueue}
            disabled={queue.length === 0}
            title={queue.length === 0 ? "Queue is already empty" : "Clear all queued files"}
          >
            <Trash2 size={15} />
            <span>Clear Queue</span>
          </button>

          {onClearHistory && (
            <button
              className="btn btn-action btn-secondary"
              onClick={onClearHistory}
              disabled={historyCount === 0}
              title={historyCount === 0 ? "History is already empty" : "Clear completed transfer history"}
            >
              <Trash2 size={15} />
              <span>Clear History</span>
            </button>
          )}

          <button
            className="btn btn-action btn-secondary"
            onClick={handleNewFileClick}
            title="Select a new single file to send"
          >
            <FilePlus size={15} />
            <span>New File</span>
          </button>

          <button
            className="btn btn-action btn-primary"
            onClick={onAddFiles}
            title="Open file picker to add files to queue"
          >
            <Plus size={16} />
            <span>Add Files</span>
          </button>
        </div>
      </div>

      {/* Active Transfer Hero Card */}
      {isTransferring ? (
        <div className="active-transfer-card">
          <div className="active-card-top">
            <div className="active-file-group">
              <div className="active-file-icon">
                <Activity size={22} className="animate-pulse" />
              </div>
              <div>
                <span className="active-badge">Active Streaming</span>
                <h3 className="active-file-name">{activeFileName}</h3>
              </div>
            </div>

            <div className="active-card-controls">
              <button
                className="btn btn-danger btn-sm"
                onClick={onCancelTransfer}
                title="Cancel Transfer"
              >
                <X size={15} />
                <span>Cancel</span>
              </button>
            </div>
          </div>

          {/* Progress Bar */}
          <div className="active-progress-container">
            <div className="active-progress-bar">
              <div
                className="active-progress-fill"
                style={{ width: `${progressPercent}%` }}
              />
            </div>
            <div className="active-progress-labels">
              <span>{progressPercent.toFixed(1)}% Completed</span>
              <span>{formatBytes(transferredBytes)} / {formatBytes(totalBytes)}</span>
            </div>
          </div>

          {/* Real-Time Live Telemetry Row */}
          <div className="telemetry-grid">
            <div className="telemetry-item">
              <div className="telemetry-label">
                <Zap size={14} />
                <span>Throughput</span>
              </div>
              <div className="telemetry-val-group">
                <span className="telemetry-primary-val">{formatSpeedMBs(speedBytes)}</span>
                <span className="telemetry-secondary-val">({formatSpeedMbps(speedBytes)})</span>
              </div>
            </div>

            <div className="telemetry-item">
              <div className="telemetry-label">
                <Clock size={14} />
                <span>Estimated Time</span>
              </div>
              <div className="telemetry-val-group">
                <span className="telemetry-primary-val">{formatEta(etaSec)}</span>
              </div>
            </div>

            <div className="telemetry-item">
              <div className="telemetry-label">
                <ArrowDownCircle size={14} />
                <span>Stream Channels</span>
              </div>
              <div className="telemetry-val-group">
                <span className="telemetry-primary-val">4 Parallel TCP</span>
              </div>
            </div>
          </div>
        </div>
      ) : (
        <div className="idle-transfer-card">
          <div className="idle-icon-box">
            <CheckCircle2 size={32} />
          </div>
          <div>
            <h3 className="idle-title">No Active Transfers</h3>
            <p className="idle-desc">Pick files or select a discovered peer to begin high-speed transmission.</p>
          </div>
          <div className="idle-actions">
            <button className="btn btn-action btn-primary" onClick={onAddFiles}>
              <Plus size={16} />
              <span>Add Files</span>
            </button>
          </div>
        </div>
      )}

      {/* Queue Items List */}
      <section className="section-block">
        <h3 className="section-title">Enqueued Files ({queue.length})</h3>

        {queue.length === 0 ? (
          <div className="empty-queue-box">
            <FileText size={28} />
            <p>Queue is empty. Select files to queue them for direct transmission.</p>
          </div>
        ) : (
          <div className="queue-list">
            {queue.map(item => {
              const isItemActive = isTransferring && item.name === activeFileName;

              return (
                <div key={item.id} className={`queue-item ${isItemActive ? 'queue-item-active' : ''}`}>
                  <div className="queue-item-left">
                    <div className="queue-file-icon">
                      <FileText size={18} />
                    </div>
                    <div className="queue-file-meta">
                      <span className="queue-file-name">{item.name}</span>
                      <span className="queue-file-details">
                        {item.targetDeviceName} ({item.targetIp || 'Local LAN'})
                      </span>
                    </div>
                  </div>

                  <div className="queue-item-right">
                    <span className={`queue-status-tag status-${item.status}`}>
                      {item.status.toUpperCase()}
                    </span>
                  </div>
                </div>
              );
            })}
          </div>
        )}
      </section>
    </div>
  );
};
