import {
  Home,
  MonitorSmartphone,
  ArrowUpDown,
  Clock,
  Settings,
  HardDrive,
  FolderOpen,
  Cpu,
  RefreshCw
} from 'lucide-react';
import { formatBytes } from '../../utils/formatters';
import { DiskSpace } from '../../types/aerosync';
import { tauriBridge } from '../../services/tauriBridge';

interface SidebarProps {
  currentTab: 'home' | 'devices' | 'transfers' | 'history' | 'settings';
  onSelectTab: (tab: 'home' | 'devices' | 'transfers' | 'history' | 'settings') => void;
  peerCount: number;
  activeTransferCount: number;
  diskSpace: DiskSpace;
  downloadDirectory: string;
  isDaemonOnline?: boolean;
  daemonError?: string | null;
  onRestartDaemon?: () => void;
  isRestartingDaemon?: boolean;
}

export const Sidebar: React.FC<SidebarProps> = ({
  currentTab,
  onSelectTab,
  peerCount,
  activeTransferCount,
  diskSpace,
  downloadDirectory,
  isDaemonOnline = false,
  daemonError,
  onRestartDaemon,
  isRestartingDaemon = false
}) => {
  const usedBytes = Math.max(0, diskSpace.totalBytes - diskSpace.freeBytes);
  const usedPercent = diskSpace.totalBytes > 0
    ? Math.min(100, Math.round((usedBytes / diskSpace.totalBytes) * 100))
    : 0;

  return (
    <aside className="sidebar">
      <nav className="sidebar-nav">
        <button
          className={`nav-item ${currentTab === 'home' ? 'nav-item-active' : ''}`}
          onClick={() => onSelectTab('home')}
        >
          <Home size={18} className="nav-icon" />
          <span className="nav-label">Home</span>
        </button>

        <button
          className={`nav-item ${currentTab === 'devices' ? 'nav-item-active' : ''}`}
          onClick={() => onSelectTab('devices')}
        >
          <MonitorSmartphone size={18} className="nav-icon" />
          <span className="nav-label">Devices</span>
          {peerCount > 0 && (
            <span className="nav-badge">{peerCount}</span>
          )}
        </button>

        <button
          className={`nav-item ${currentTab === 'transfers' ? 'nav-item-active' : ''}`}
          onClick={() => onSelectTab('transfers')}
        >
          <ArrowUpDown size={18} className="nav-icon" />
          <span className="nav-label">Transfers</span>
          {activeTransferCount > 0 && (
            <span className="nav-badge nav-badge-active">{activeTransferCount}</span>
          )}
        </button>

        <button
          className={`nav-item ${currentTab === 'history' ? 'nav-item-active' : ''}`}
          onClick={() => onSelectTab('history')}
        >
          <Clock size={18} className="nav-icon" />
          <span className="nav-label">History</span>
        </button>

        <button
          className={`nav-item ${currentTab === 'settings' ? 'nav-item-active' : ''}`}
          onClick={() => onSelectTab('settings')}
        >
          <Settings size={18} className="nav-icon" />
          <span className="nav-label">Settings</span>
        </button>
      </nav>

      <div className="sidebar-footer">
        <div className={`sidebar-core-box ${isDaemonOnline ? 'sidebar-core-running' : 'sidebar-core-error'}`}>
          <div className="sidebar-core-header">
            <div className="sidebar-core-label">
              <Cpu size={14} />
              <span>AeroSync Core</span>
            </div>
            <div className={`sidebar-core-pill ${isDaemonOnline ? 'pill-running' : 'pill-not-running'}`}>
              <span className="sidebar-core-symbol">{isDaemonOnline ? '●' : '✕'}</span>
              <span>{isDaemonOnline ? 'Running' : 'Not running'}</span>
            </div>
          </div>
          {!isDaemonOnline && (
            <div className="sidebar-core-error-content">
              <p className="sidebar-core-error-text">
                {daemonError ? `${daemonError.slice(0, 75)}${daemonError.length > 75 ? '...' : ''}` : 'Daemon backend offline (127.0.0.1:48126)'}
              </p>
              {onRestartDaemon && (
                <button
                  className="sidebar-core-restart-btn"
                  onClick={onRestartDaemon}
                  disabled={isRestartingDaemon}
                >
                  <RefreshCw size={11} className={isRestartingDaemon ? 'animate-spin' : ''} />
                  <span>{isRestartingDaemon ? 'Starting...' : 'Restart Core'}</span>
                </button>
              )}
            </div>
          )}
        </div>

        <div className="storage-card">
          <div className="storage-header">
            <div className="storage-title-group">
              <HardDrive size={14} className="storage-icon" />
              <span>Storage Free</span>
            </div>
            <button
              className="storage-open-btn"
              onClick={() => tauriBridge.openFolder(downloadDirectory)}
              title="Open AeroSync Downloads Folder"
            >
              <FolderOpen size={13} />
            </button>
          </div>
          <div className="storage-progress-bg">
            <div
              className="storage-progress-fill"
              style={{ width: `${usedPercent}%` }}
            />
          </div>
          <div className="storage-stats">
            <span className="storage-free">{formatBytes(diskSpace.freeBytes)} available</span>
            <span className="storage-total">{formatBytes(diskSpace.totalBytes)}</span>
          </div>
        </div>
      </div>
    </aside>
  );
};
