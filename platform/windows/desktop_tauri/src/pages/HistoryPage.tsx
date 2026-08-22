import React from 'react';
import {
  Clock,
  FolderOpen,
  ArrowUpRight,
  ArrowDownLeft,
  FileCheck,
  ExternalLink
} from 'lucide-react';
import { TransferHistoryRecord } from '../types/aerosync';
import { formatBytes, formatTimestamp } from '../utils/formatters';
import { tauriBridge } from '../services/tauriBridge';

interface HistoryPageProps {
  history: TransferHistoryRecord[];
  downloadDirectory: string;
  onClearHistory?: () => void;
}

export const HistoryPage: React.FC<HistoryPageProps> = ({
  history,
  downloadDirectory
}) => {
  return (
    <div className="page-container">
      <div className="page-header">
        <div>
          <h2 className="page-title">Transfer History</h2>
          <p className="page-subtitle">Log of completed and verified P2P file transmissions.</p>
        </div>
        <div className="page-header-actions">
          <button
            className="btn btn-secondary"
            onClick={() => tauriBridge.openFolder(downloadDirectory)}
          >
            <FolderOpen size={15} />
            <span>Open Downloads</span>
          </button>
        </div>
      </div>

      {history.length === 0 ? (
        <div className="empty-state-card">
          <div className="empty-icon-circle">
            <Clock size={32} />
          </div>
          <h3 className="empty-state-title">No Transfer History</h3>
          <p className="empty-state-text">
            Files sent and received with AeroSync will be recorded here with integrity verification logs.
          </p>
        </div>
      ) : (
        <div className="history-table-card">
          <div className="history-table-header">
            <span className="col-file">File Details</span>
            <span className="col-peer">Peer Device</span>
            <span className="col-size">Size</span>
            <span className="col-date">Completed</span>
            <span className="col-actions">Action</span>
          </div>

          <div className="history-table-body">
            {history.map(record => {
              const isSent = record.direction === 'sent';

              return (
                <div key={record.id} className="history-row">
                  <div className="col-file">
                    <div className="history-direction-icon">
                      {isSent ? (
                        <ArrowUpRight size={16} className="text-cyan" />
                      ) : (
                        <ArrowDownLeft size={16} className="text-green" />
                      )}
                    </div>
                    <div className="history-file-meta">
                      <span className="history-file-title" title={record.fileName}>
                        {record.fileName}
                      </span>
                      <span className="history-integrity-tag">
                        <FileCheck size={11} /> CRC32C Verified
                      </span>
                    </div>
                  </div>

                  <div className="col-peer">
                    <span className="history-peer-name">{record.peerName}</span>
                    <span className="history-peer-ip">{record.peerIp}</span>
                  </div>

                  <div className="col-size">
                    <span>{formatBytes(record.fileSize)}</span>
                  </div>

                  <div className="col-date">
                    <span>{formatTimestamp(record.timestampMs)}</span>
                  </div>

                  <div className="col-actions">
                    <button
                      className="btn btn-sm btn-ghost"
                      onClick={() => tauriBridge.showInFolder(record.filePath)}
                      title="Open in Windows Explorer"
                    >
                      <ExternalLink size={13} />
                      <span>Explorer</span>
                    </button>
                  </div>
                </div>
              );
            })}
          </div>
        </div>
      )}
    </div>
  );
};
