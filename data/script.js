// DOM Elements
const doorStatus = document.getElementById('doorStatus');
const wifiSignal = document.getElementById('wifiSignal');
const lastAccess = document.getElementById('lastAccess');
const uptime = document.getElementById('uptime');
const lockBtn = document.getElementById('lockBtn');
const unlockBtn = document.getElementById('unlockBtn');
const rebootBtn = document.getElementById('rebootBtn');
const wifiForm = document.getElementById('wifiForm');
const refreshLogs = document.getElementById('refreshLogs');
const logsTable = document.getElementById('logsTable');

// Update status every second
setInterval(updateStatus, 1000);

// Event Listeners
lockBtn.addEventListener('click', () => sendCommand('lock'));
unlockBtn.addEventListener('click', () => sendCommand('unlock'));
rebootBtn.addEventListener('click', () => sendCommand('reboot'));
wifiForm.addEventListener('submit', handleWifiSubmit);
refreshLogs.addEventListener('click', updateLogs);

// Functions
async function updateStatus() {
    try {
        const response = await fetch('/api/status');
        const data = await response.json();
        
        // Update status elements
        doorStatus.textContent = data.doorStatus;
        doorStatus.className = `badge bg-${data.doorStatus === 'Locked' ? 'danger' : 'success'}`;
        
        wifiSignal.textContent = `${data.wifiSignal}%`;
        wifiSignal.className = `badge bg-${getSignalColor(data.wifiSignal)}`;
        
        lastAccess.textContent = data.lastAccess || 'Never';
        uptime.textContent = formatUptime(data.uptime);
    } catch (error) {
        console.error('Error updating status:', error);
    }
}

async function updateLogs() {
    try {
        const response = await fetch('/api/logs');
        const logs = await response.json();
        
        // Clear existing logs
        logsTable.innerHTML = '';
        
        // Add new logs
        logs.forEach(log => {
            const row = document.createElement('tr');
            row.innerHTML = `
                <td>${log.time}</td>
                <td>${log.method}</td>
                <td><span class="badge bg-${log.status === 'success' ? 'success' : 'danger'}">${log.status}</span></td>
            `;
            logsTable.appendChild(row);
        });
    } catch (error) {
        console.error('Error updating logs:', error);
    }
}

async function sendCommand(command) {
    try {
        const response = await fetch(`/api/command/${command}`, { method: 'POST' });
        const data = await response.json();
        
        if (data.success) {
            showToast('Command sent successfully');
            updateStatus();
        } else {
            showToast('Error sending command', 'error');
        }
    } catch (error) {
        console.error('Error sending command:', error);
        showToast('Error sending command', 'error');
    }
}

async function handleWifiSubmit(event) {
    event.preventDefault();
    
    const ssid = document.getElementById('ssid').value;
    const password = document.getElementById('password').value;
    
    try {
        const response = await fetch('/api/config', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ ssid, password })
        });
        
        const data = await response.json();
        
        if (data.success) {
            showToast('WiFi settings saved. Device will restart...');
            setTimeout(() => window.location.reload(), 5000);
        } else {
            showToast('Error saving WiFi settings', 'error');
        }
    } catch (error) {
        console.error('Error saving WiFi settings:', error);
        showToast('Error saving WiFi settings', 'error');
    }
}

// Helper functions
function getSignalColor(signal) {
    if (signal >= 80) return 'success';
    if (signal >= 50) return 'warning';
    return 'danger';
}

function formatUptime(seconds) {
    const hours = Math.floor(seconds / 3600);
    const minutes = Math.floor((seconds % 3600) / 60);
    const secs = seconds % 60;
    return `${hours}:${minutes.toString().padStart(2, '0')}:${secs.toString().padStart(2, '0')}`;
}

function showToast(message, type = 'success') {
    const toast = document.createElement('div');
    toast.className = `toast align-items-center text-white bg-${type} border-0 position-fixed bottom-0 end-0 m-3`;
    toast.setAttribute('role', 'alert');
    toast.setAttribute('aria-live', 'assertive');
    toast.setAttribute('aria-atomic', 'true');
    
    toast.innerHTML = `
        <div class="d-flex">
            <div class="toast-body">
                ${message}
            </div>
            <button type="button" class="btn-close btn-close-white me-2 m-auto" data-bs-dismiss="toast" aria-label="Close"></button>
        </div>
    `;
    
    document.body.appendChild(toast);
    const bsToast = new bootstrap.Toast(toast);
    bsToast.show();
    
    toast.addEventListener('hidden.bs.toast', () => {
        document.body.removeChild(toast);
    });
}

// Initial updates
updateStatus();
updateLogs(); 