import React, { useState, useEffect } from 'react';
import { useAeroSyncStore } from './stores/useAeroSyncStore';
import { HomePage } from './pages/HomePage';
import { DevicesPage } from './pages/DevicesPage';
import { TransfersPage } from './pages/TransfersPage';
import { DirectIpModal } from './components/Modals/DirectIpModal';
import { IncomingRequestModal } from './components/Modals/IncomingRequestModal';
import { tauriBridge } from './services/tauriBridge';
import { PeerInfo } from './types/aerosync';
import logoImg from './assets/logo.png';
import {
  Files,
  Laptop2,
  Activity,
  Sun,
  Moon,
  Minus,
  Square,
  X,
  CheckCircle2,
  AlertCircle
} from 'lucide-react';

export const App: React.FC = () => {
  const {
    currentTab,
    setCurrentTab,
    peers,
    selectedPeer,
    setSelectedPeer,
    queue,
    history,
    settings,
    diskSpace,
    isDaemonOnline,
    statusMessage,
    isTransferring,
    currentProgress,
    incomingRequest,
    setIncomingRequest,
    isDirectIpModalOpen,
    setIsDirectIpModalOpen,
    enqueueFiles,
    cancelTransfer,
    clearCompletedQueue,
    clearHistory,
    updateSettings
  } = useAeroSyncStore();

  const [isDarkMode, setIsDarkMode] = useState<boolean>(false);
  const [toastMessage, setToastMessage] = useState<{ text: string; type: 'success' | 'info' | 'error' } | null>(null);

  // Sync dark mode HTML class
  useEffect(() => {
    if (isDarkMode) {
      document.documentElement.classList.add('dark');
    } else {
      document.documentElement.classList.remove('dark');
    }
  }, [isDarkMode]);

  // Temporary toast notification helper
  const showToast = (text: string, type: 'success' | 'info' | 'error' = 'info') => {
    setToastMessage({ text, type });
    setTimeout(() => {
      setToastMessage(prev => prev?.text === text ? null : prev);
    }, 4000);
  };

  // Real native file picker
  const handlePickAndSendFiles = async (targetPeer?: PeerInfo) => {
    const files = await tauriBridge.selectFiles();
    if (files && files.length > 0) {
      enqueueFiles(files, targetPeer);
      showToast(`Added ${files.length} file(s) to transfer queue`, 'info');
    }
  };

  // Real native folder picker
  const handlePickAndSendFolder = async (targetPeer?: PeerInfo) => {
    const folder = await tauriBridge.selectFolder();
    if (folder) {
      enqueueFiles([folder], targetPeer);
      showToast('Folder added to transfer queue', 'info');
    }
  };

  const handleSendFilesToPeer = async (peer: PeerInfo) => {
    setSelectedPeer(peer);
    await handlePickAndSendFiles(peer);
  };

  const handleConnectDirectIp = (ip: string, port: number) => {
    const directPeer: PeerInfo = {
      deviceId: `manual-${ip}`,
      deviceName: `Device (${ip})`,
      platform: 'unknown',
      deviceType: 0,
      ipAddress: ip,
      port: port,
      lastSeenMs: Date.now()
    };
    setSelectedPeer(directPeer);
    setIsDirectIpModalOpen(false);
    handlePickAndSendFiles(directPeer);
  };

  const handleChangeDownloadDir = async () => {
    const folder = await tauriBridge.selectFolder();
    if (folder) {
      updateSettings({ downloadDirectory: folder });
      showToast(`Download location updated to ${folder}`, 'success');
    }
  };

  return (
    <div className="app-layout">
      {/* Top Header Bar */}
      <header className="header-topbar" data-tauri-drag-region>
        {/* Brand Left */}
        <div className="brand-section" data-tauri-drag-region>
          <img src={logoImg} alt="AeroSync" className="brand-logo-img" />
          <span className="brand-name" data-tauri-drag-region>AeroSync</span>
        </div>

        {/* Center Pill Navigation */}
        <nav className="nav-pill-container" data-tauri-drag-region>
          <button
            className={`nav-pill-item ${currentTab === 'home' ? 'active' : ''}`}
            onClick={() => setCurrentTab('home')}
            id="tab-files-btn"
          >
            <Files size={15} />
            <span>Files</span>
          </button>

          <button
            className={`nav-pill-item ${currentTab === 'devices' ? 'active' : ''}`}
            onClick={() => setCurrentTab('devices')}
            id="tab-devices-btn"
          >
            <Laptop2 size={15} />
            <span>Devices</span>
            {peers.length > 0 && (
              <span className="nav-pill-badge">{peers.length}</span>
            )}
          </button>

          <button
            className={`nav-pill-item ${currentTab === 'transfers' ? 'active' : ''}`}
            onClick={() => setCurrentTab('transfers')}
            id="tab-activity-btn"
          >
            <Activity size={15} />
            <span>Activity</span>
            {isTransferring && (
              <span className="nav-pill-badge">1</span>
            )}
          </button>
        </nav>

        {/* Header Right Actions */}
        <div className="header-actions">
          {/* Theme Toggle Button */}
          <button
            className="icon-btn-circle"
            onClick={() => setIsDarkMode(!isDarkMode)}
            title={isDarkMode ? 'Switch to Bright Mode' : 'Switch to Dark Mode'}
            aria-label="Toggle Theme"
          >
            {isDarkMode ? <Sun size={17} /> : <Moon size={17} />}
          </button>

          {/* Window Controls */}
          <div className="window-controls-group">
            <button
              className="win-control-btn"
              onClick={() => tauriBridge.minimizeWindow()}
              title="Minimize"
              aria-label="Minimize"
            >
              <Minus size={14} />
            </button>
            <button
              className="win-control-btn"
              onClick={() => tauriBridge.maximizeWindow()}
              title="Maximize"
              aria-label="Maximize"
            >
              <Square size={12} />
            </button>
            <button
              className="win-control-btn close"
              onClick={() => tauriBridge.closeWindow()}
              title="Close"
              aria-label="Close"
            >
              <X size={15} />
            </button>
          </div>
        </div>
      </header>

      {/* Main Workspace (Scrollable & Centered) */}
      <main className="app-workspace-scrollable">
        <div className="app-content-container">
          {currentTab === 'home' && (
            <HomePage
              peers={peers}
              queue={queue}
              currentProgress={currentProgress}
              isTransferring={isTransferring}
              diskSpace={diskSpace}
              recentHistory={history}
              downloadDirectory={settings.downloadDirectory}
              isDaemonOnline={isDaemonOnline}
              statusMessage={statusMessage}
              onSendFiles={() => handlePickAndSendFiles()}
              onSendFolder={() => handlePickAndSendFolder()}
              onFilesDropped={(files) => enqueueFiles(files)}
              onSendFilesToPeer={handleSendFilesToPeer}
              onChangeDownloadDir={handleChangeDownloadDir}
              onCancelTransfer={cancelTransfer}
              onSelectTab={setCurrentTab}
              onOpenDirectIpModal={() => setIsDirectIpModalOpen(true)}
              onClearHistory={clearHistory}
            />
          )}

          {currentTab === 'devices' && (
            <DevicesPage
              peers={peers}
              selectedPeer={selectedPeer}
              onSelectPeer={setSelectedPeer}
              onSendFilesToPeer={handleSendFilesToPeer}
              onOpenDirectIpModal={() => setIsDirectIpModalOpen(true)}
            />
          )}

          {currentTab === 'transfers' && (
            <TransfersPage
              queue={queue}
              history={history}
              currentProgress={currentProgress}
              isTransferring={isTransferring}
              downloadDirectory={settings.downloadDirectory}
              onAddFiles={() => handlePickAndSendFiles()}
              onCancelTransfer={cancelTransfer}
              onClearCompleted={clearCompletedQueue}
              onClearHistory={clearHistory}
            />
          )}
        </div>
      </main>

      {/* Toast Notification Container */}
      {toastMessage && (
        <div className="toast-container">
          <div className="toast-item">
            {toastMessage.type === 'success' ? (
              <CheckCircle2 size={18} color="#10B981" />
            ) : (
              <AlertCircle size={18} color="#3B82F6" />
            )}
            <span>{toastMessage.text}</span>
          </div>
        </div>
      )}

      {/* Direct IP Modal */}
      <DirectIpModal
        isOpen={isDirectIpModalOpen}
        onConnect={handleConnectDirectIp}
        onClose={() => setIsDirectIpModalOpen(false)}
      />

      {/* Incoming Request Modal */}
      <IncomingRequestModal
        request={incomingRequest}
        onAccept={() => {
          setIncomingRequest(null);
          showToast('Transfer accepted', 'success');
        }}
        onDecline={() => {
          setIncomingRequest(null);
          showToast('Transfer declined', 'info');
        }}
      />
    </div>
  );
};
