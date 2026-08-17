import React from 'react';
import { useAeroSyncStore } from './stores/useAeroSyncStore';
import { TitleBar } from './components/Titlebar/TitleBar';
import { Sidebar } from './components/Sidebar/Sidebar';
import { IncomingRequestModal } from './components/Modals/IncomingRequestModal';
import { DirectIpModal } from './components/Modals/DirectIpModal';
import { HomePage } from './pages/HomePage';
import { DevicesPage } from './pages/DevicesPage';
import { TransfersPage } from './pages/TransfersPage';
import { HistoryPage } from './pages/HistoryPage';
import { SettingsPage } from './pages/SettingsPage';
import { tauriBridge } from './services/tauriBridge';
import { PeerInfo } from './types/aerosync';

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
    updateSettings,
    refreshStorage
  } = useAeroSyncStore();

  const handlePickAndSendFiles = async () => {
    const files = await tauriBridge.selectFiles();
    if (files && files.length > 0) {
      enqueueFiles(files);
    }
  };

  const handlePickAndSendFolder = async () => {
    const folder = await tauriBridge.selectFolder();
    if (folder) {
      enqueueFiles([folder]);
    }
  };

  const handleSendFilesToPeer = async (peer: PeerInfo) => {
    setSelectedPeer(peer);
    const files = await tauriBridge.selectFiles();
    if (files && files.length > 0) {
      enqueueFiles(files, peer);
    }
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
    handlePickAndSendFiles();
  };

  return (
    <div className="app-layout">
      <TitleBar isDaemonOnline={isDaemonOnline} />

      <div className="app-workspace">
        <Sidebar
          currentTab={currentTab}
          onSelectTab={setCurrentTab}
          peerCount={peers.length}
          activeTransferCount={isTransferring ? 1 : queue.filter(q => q.status === 'waiting' || q.status === 'transferring').length}
          diskSpace={diskSpace}
          downloadDirectory={settings.downloadDirectory}
        />

        <main className="app-main-content">
          {currentTab === 'home' && (
            <HomePage
              peers={peers}
              activeTransferCount={isTransferring ? 1 : 0}
              diskSpace={diskSpace}
              recentHistory={history}
              isDaemonOnline={isDaemonOnline}
              statusMessage={statusMessage}
              onSendFiles={handlePickAndSendFiles}
              onSendFolder={handlePickAndSendFolder}
              onSelectPeer={(peer) => {
                setSelectedPeer(peer);
                setCurrentTab('devices');
              }}
              onSelectTab={setCurrentTab}
              onFilesDropped={(files) => enqueueFiles(files)}
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
              currentProgress={currentProgress}
              isTransferring={isTransferring}
              onAddFiles={handlePickAndSendFiles}
              onCancelTransfer={cancelTransfer}
              onClearCompleted={clearCompletedQueue}
            />
          )}

          {currentTab === 'history' && (
            <HistoryPage
              history={history}
              downloadDirectory={settings.downloadDirectory}
              onClearHistory={clearHistory}
            />
          )}

          {currentTab === 'settings' && (
            <SettingsPage
              settings={settings}
              diskSpace={diskSpace}
              onUpdateSettings={updateSettings}
              onRefreshStorage={refreshStorage}
            />
          )}
        </main>
      </div>

      {/* Global Incoming Connection / Pairing Modal */}
      <IncomingRequestModal
        request={incomingRequest}
        onAccept={() => setIncomingRequest(null)}
        onDecline={() => setIncomingRequest(null)}
      />

      {/* Direct IP Connect Modal */}
      <DirectIpModal
        isOpen={isDirectIpModalOpen}
        onClose={() => setIsDirectIpModalOpen(false)}
        onConnect={handleConnectDirectIp}
      />
    </div>
  );
};
