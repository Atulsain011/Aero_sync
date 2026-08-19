import React, { useState } from 'react';
import {
  Settings,
  Folder,
  Sun,
  Moon,
  Laptop,
  Check,
  Bell,
  Power,
  Info,
  Shield,
  HardDrive,
  RefreshCw
} from 'lucide-react';
import { SettingsState, DiskSpace } from '../types/aerosync';
import { formatBytes } from '../utils/formatters';
import { tauriBridge } from '../services/tauriBridge';

interface SettingsPageProps {
  settings: SettingsState;
  diskSpace: DiskSpace;
  onUpdateSettings: (partial: Partial<SettingsState>) => void;
  onRefreshStorage: () => void;
}

export const SettingsPage: React.FC<SettingsPageProps> = ({
  settings,
  diskSpace,
  onUpdateSettings,
  onRefreshStorage
}) => {
  const [deviceNameInput, setDeviceNameInput] = useState(settings.deviceName);
  const [isSaved, setIsSaved] = useState(false);

  const handlePickFolder = async () => {
    const selected = await tauriBridge.selectFolder();
    if (selected) {
      onUpdateSettings({ downloadDirectory: selected });
      onRefreshStorage();
    }
  };

  const handleSaveDeviceName = (e: React.FormEvent) => {
    e.preventDefault();
    if (deviceNameInput.trim()) {
      onUpdateSettings({ deviceName: deviceNameInput.trim() });
      setIsSaved(true);
      setTimeout(() => setIsSaved(false), 2000);
    }
  };

  return (
    <div className="page-container">
      <div className="page-header">
        <div>
          <h2 className="page-title">Settings</h2>
          <p className="page-subtitle">Configure application behavior, appearance, and download destinations.</p>
        </div>
      </div>

      <div className="settings-grid">
        {/* Appearance & Theme */}
        <div className="settings-card">
          <div className="settings-card-header">
            <div className="settings-icon-box">
              <Sun size={20} />
            </div>
            <div>
              <h3 className="settings-card-title">Appearance Theme</h3>
              <p className="settings-card-desc">Choose between Light or Dark theme mode.</p>
            </div>
          </div>

          <div className="theme-toggle-row">
            <button
              className={`theme-pill ${settings.theme === 'light' ? 'theme-pill-active' : ''}`}
              onClick={() => onUpdateSettings({ theme: 'light' })}
            >
              <Sun size={16} />
              <span>Light</span>
            </button>
            <button
              className={`theme-pill ${settings.theme === 'dark' ? 'theme-pill-active' : ''}`}
              onClick={() => onUpdateSettings({ theme: 'dark' })}
            >
              <Moon size={16} />
              <span>Dark</span>
            </button>
          </div>
        </div>

        {/* Download Storage Location */}
        <div className="settings-card">
          <div className="settings-card-header">
            <div className="settings-icon-box">
              <Folder size={20} />
            </div>
            <div>
              <h3 className="settings-card-title">Download Directory</h3>
              <p className="settings-card-desc">Received files are automatically organized in this directory.</p>
            </div>
          </div>

          <div className="folder-picker-group">
            <input
              type="text"
              readOnly
              className="form-input form-input-readonly"
              value={settings.downloadDirectory}
            />
            <button className="btn btn-secondary" onClick={handlePickFolder}>
              <Folder size={15} />
              <span>Change Folder</span>
            </button>
          </div>

          <div className="storage-diagnostic-row">
            <div className="storage-diagnostic-item">
              <HardDrive size={14} />
              <span>{formatBytes(diskSpace.freeBytes)} available on target drive</span>
            </div>
            <button className="btn btn-sm btn-ghost" onClick={onRefreshStorage}>
              <RefreshCw size={13} />
              <span>Refresh</span>
            </button>
          </div>
        </div>

        {/* Device Name */}
        <div className="settings-card">
          <div className="settings-card-header">
            <div className="settings-icon-box">
              <Laptop size={20} />
            </div>
            <div>
              <h3 className="settings-card-title">Device Display Name</h3>
              <p className="settings-card-desc">The identifier shown to nearby Android and Windows peers during discovery.</p>
            </div>
          </div>

          <form onSubmit={handleSaveDeviceName} className="device-name-form">
            <input
              type="text"
              className="form-input"
              value={deviceNameInput}
              onChange={e => setDeviceNameInput(e.target.value)}
              placeholder="e.g. Workstation PC"
            />
            <button type="submit" className="btn btn-primary">
              {isSaved ? <Check size={16} /> : <span>Update</span>}
            </button>
          </form>
        </div>

        {/* System & Notification Preferences */}
        <div className="settings-card">
          <div className="settings-card-header">
            <div className="settings-icon-box">
              <Power size={20} />
            </div>
            <div>
              <h3 className="settings-card-title">System Integration</h3>
              <p className="settings-card-desc">Configure system startup and notification preferences.</p>
            </div>
          </div>

          <div className="toggle-list">
            <label className="toggle-item">
              <div className="toggle-meta">
                <span className="toggle-title">Start with Windows</span>
                <span className="toggle-desc">Automatically launch AeroSync in background when starting Windows.</span>
              </div>
              <input
                type="checkbox"
                className="toggle-checkbox"
                checked={settings.startWithWindows}
                onChange={e => onUpdateSettings({ startWithWindows: e.target.checked })}
              />
            </label>

            <label className="toggle-item">
              <div className="toggle-meta">
                <span className="toggle-title">Desktop Notifications</span>
                <span className="toggle-desc">Show notification toasts when file transfers complete.</span>
              </div>
              <input
                type="checkbox"
                className="toggle-checkbox"
                checked={settings.notificationsEnabled}
                onChange={e => onUpdateSettings({ notificationsEnabled: e.target.checked })}
              />
            </label>
          </div>
        </div>

        {/* About & Core Engine */}
        <div className="settings-card">
          <div className="settings-card-header">
            <div className="settings-icon-box">
              <Info size={20} />
            </div>
            <div>
              <h3 className="settings-card-title">About AeroSync</h3>
              <p className="settings-card-desc">High-Performance P2P LAN File Transfer Engine.</p>
            </div>
          </div>

          <div className="about-spec-grid">
            <div className="about-spec-row">
              <span className="spec-name">App Version</span>
              <span className="spec-val">v2.0.0 (Production)</span>
            </div>
            <div className="about-spec-row">
              <span className="spec-name">Desktop Runtime</span>
              <span className="spec-val">Tauri 2.x + React 18 + TypeScript</span>
            </div>
            <div className="about-spec-row">
              <span className="spec-name">Core Engine</span>
              <span className="spec-val">C++17 High-Throughput (4-Stream Pipelined)</span>
            </div>
            <div className="about-spec-row">
              <span className="spec-name">Data Integrity</span>
              <span className="spec-val">Hardware Castagnoli CRC32C</span>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};
