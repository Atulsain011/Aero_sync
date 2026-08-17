# AeroSync Inbound Windows Firewall Rule Setup Script
# Adds Firewall rules for AeroSync P2P Ports:
# - UDP 48123: Peer Discovery & mDNS
# - TCP 48124: Control Channel Handshake & Pairing
# - TCP 48125: Dedicated Parallel Data Channels

Param(
    [string]$AppPath = "$PSScriptRoot\AeroSync.exe"
)

Write-Host "Configuring Windows Firewall Inbound Rules for AeroSync..."

# Remove old rules if present
Remove-NetFirewallRule -DisplayName "AeroSync*" -ErrorAction SilentlyContinue

# Add Port-Based UDP Discovery Rule (Any Program)
New-NetFirewallRule -DisplayName "AeroSync P2P Discovery (UDP 48123)" `
                    -Direction Inbound `
                    -Action Allow `
                    -Protocol UDP `
                    -LocalPort 48123,5353 `
                    -Enabled True `
                    -Profile Any

# Add Port-Based TCP Control & Data Rule (Any Program)
New-NetFirewallRule -DisplayName "AeroSync P2P Transfer (TCP 48124-48126)" `
                    -Direction Inbound `
                    -Action Allow `
                    -Protocol TCP `
                    -LocalPort 48124,48125,48126 `
                    -Enabled True `
                    -Profile Any

if (Test-Path $AppPath) {
    New-NetFirewallRule -DisplayName "AeroSync Application Binary" `
                        -Direction Inbound `
                        -Action Allow `
                        -Program $AppPath `
                        -Enabled True `
                        -Profile Any
}

Write-Host "[OK] Windows Firewall rules configured successfully!"
