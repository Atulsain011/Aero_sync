import React from 'react';
import { Minus, Square, X } from 'lucide-react';
import { tauriBridge } from '../../services/tauriBridge';
import logoImg from '../../assets/logo.png';

interface TitleBarProps {
  isDaemonOnline: boolean;
  daemonError?: string | null;
  onRestartDaemon?: () => void;
  isRestartingDaemon?: boolean;
}

export const TitleBar: React.FC<TitleBarProps> = ({
  isDaemonOnline,
  daemonError,
  onRestartDaemon,
  isRestartingDaemon
}) => {
  const handleDoubleClick = () => {
    tauriBridge.maximizeWindow();
  };

  return (
    <header className="titlebar" data-tauri-drag-region onDoubleClick={handleDoubleClick}>
      <div className="titlebar-left" data-tauri-drag-region>
        <img src={logoImg} alt="AeroSync" className="titlebar-logo" />
        <span className="titlebar-title" data-tauri-drag-region>AeroSync</span>
        <div
          className={`core-titlebar-status ${isDaemonOnline ? 'core-status-running' : 'core-status-stopped'}`}
          title={isDaemonOnline ? 'AeroSync Core is running and responding on 127.0.0.1:48126' : (daemonError ? `Error: ${daemonError}` : 'AeroSync Core is not running. Click to restart.')}
          onClick={!isDaemonOnline && onRestartDaemon ? onRestartDaemon : undefined}
          style={{ cursor: !isDaemonOnline ? 'pointer' : 'default' }}
        >
          <span className="core-titlebar-label">AeroSync Core</span>
          <span className="core-titlebar-pill">
            <span className="status-dot">{isDaemonOnline ? '●' : '✕'}</span>
            <span>{isRestartingDaemon ? 'Starting...' : (isDaemonOnline ? 'Running' : 'Not running')}</span>
          </span>
        </div>
      </div>

      <div className="titlebar-center" data-tauri-drag-region>
        {/* Clean draggable space */}
      </div>

      <div className="titlebar-right">
        <button
          className="titlebar-btn"
          onClick={() => tauriBridge.minimizeWindow()}
          title="Minimize"
          aria-label="Minimize"
        >
          <Minus size={14} />
        </button>
        <button
          className="titlebar-btn"
          onClick={() => tauriBridge.maximizeWindow()}
          title="Maximize"
          aria-label="Maximize"
        >
          <Square size={12} />
        </button>
        <button
          className="titlebar-btn titlebar-btn-close"
          onClick={() => tauriBridge.closeWindow()}
          title="Close"
          aria-label="Close"
        >
          <X size={14} />
        </button>
      </div>
    </header>
  );
};
