const maxReconnectAttempts = 5;
let reconnectAttempts = 0;
let ws = null;
let currentLine = '';
let terminalId = null;
let isConnecting = false;
let commandHistory = [];
let historyIndex = -1;


// Dans ton script main.js par exemple
const term = new Terminal({
    
  });
  term.open(document.getElementById('terminal'));
  

function updateStatus(status, info = '') {
    document.getElementById('connection-status').textContent = status;
    document.getElementById('reconnect-info').textContent = info;
}

function connect() {
    if (isConnecting) return;
    isConnecting = true;

    console.log('Connexion WebSocket...');
    ws = new WebSocket(`ws://${window.location.host}`);

    ws.onopen = () => {
        console.log('WebSocket connecté');
        isConnecting = false;
        reconnectAttempts = 0;
        updateStatus('Connecté');
    };

    ws.onclose = () => {
        console.log('WebSocket fermé');
        isConnecting = false;
        updateStatus('Déconnecté');

        if (reconnectAttempts < maxReconnectAttempts) {
            reconnectAttempts++;
            updateStatus('Déconnecté', `Reconnexion ${reconnectAttempts}/${maxReconnectAttempts}`);
            setTimeout(connect, 2000);
        }
    };

    ws.onerror = (error) => {
        console.error('Erreur WebSocket:', error);
    };

    ws.onmessage = (event) => {
        try {
            const message = JSON.parse(event.data);

            if (message.type === 'terminal-id') {
                terminalId = message.id;
            } else if (message.type === 'output') {
                term.write(message.data);
            }
        } catch (error) {
            console.error('Erreur message:', error);
        }
    };
}
let outputHistory = [];

function handleSpecialCommands(command) {
    switch (command.trim().toLowerCase()) {
        case 'clear':
        case 'cls':
            // Séquence ANSI pour effacer l'écran et repositionner le curseur
            term.write('\x1b[2J\x1b[H');
            term.write('> ');
            return true;

        default:
            return false;
    }
}

term.onData(e => {
    if (!ws || ws.readyState !== WebSocket.OPEN || !terminalId) return;

    switch (e) {
        case '\r': // Enter
            term.write('\r\n');
            if (currentLine.trim()) {
                commandHistory.push(currentLine);
                historyIndex = commandHistory.length;

                if (!handleSpecialCommands(currentLine)) {
                    ws.send(JSON.stringify({
                        type: 'input',
                        terminalId: terminalId,
                        data: currentLine
                    }));
                }
            }
            currentLine = '';
            break;

        case '\u007F': // Backspace
            if (currentLine.length > 0) {
                currentLine = currentLine.slice(0, -1);
                term.write('\b \b');
            }
            break;

        case '\u001b[A': // Flèche haut
            if (historyIndex > 0) {
                historyIndex--;
                term.write('\r' + ' '.repeat(currentLine.length + 2) + '\r> ');
                currentLine = commandHistory[historyIndex];
                term.write(currentLine);
            } else if (outputHistory.length > 0) {
                // Afficher la sortie précédente
                term.write('\r\n' + outputHistory.join('\r\n') + '\r\n');
            }
            break;

        case '\u001b[B': // Flèche bas
            if (historyIndex < commandHistory.length) {
                historyIndex++;
                // Effacer la ligne actuelle
                term.write('\r' + ' '.repeat(currentLine.length + 2) + '\r> ');
                currentLine = historyIndex < commandHistory.length ?
                    commandHistory[historyIndex] : '';
                term.write(currentLine);
            }
            break;

        default:
            if (e >= String.fromCharCode(32) || e === '\t') {
                currentLine += e;
                term.write(e);
            }
    }
});

// Démarrer la connexion
connect();