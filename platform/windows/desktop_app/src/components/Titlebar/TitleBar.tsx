import React from 'react';
import { Minus, Square, X, Radio } from 'lucide-react';
import { tauriBridge } from '../../services/tauriBridge';

interface TitleBarProps {
  isDaemonOnline: boolean;
}

export const TitleBar: React.FC<TitleBarProps> = ({ isDaemonOnline }) => {
  const handleDoubleClick = () => {
    tauriBridge.maximizeWindow();
  };

  return (
    <header className="titlebar" data-tauri-drag-region onDoubleClick={handleDoubleClick}>
      <div className="titlebar-left" data-tauri-drag-region>
        <img src="/assets/logo.png" alt="AeroSync" className="titlebar-logo" />
        <span className="titlebar-title" data-tauri-drag-region>AeroSync</span>
        <div className={`status-pill ${isDaemonOnline ? 'status-online' : 'status-offline'}`}>
          <Radio className="status-icon" size={12} />
          <span>{isDaemonOnline ? 'Ready' : 'Connecting'}</span>
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
