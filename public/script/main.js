const maxReconnectAttempts = 5;
let reconnectAttempts = 0;
let ws = null;
let currentLine = '';
let terminalId = null;
let isConnecting = false;
let commandHistory = [];
let historyIndex = -1;

// Configuration du terminal
const term = new Terminal({
    fontFamily: '"Fira Code", monospace',
    fontSize: 14,
    theme: {
        background: '#2E3440',
        foreground: '#D8DEE9',
        cursor: '#81A1C1',
        selection: '#4C566A66',
        black: '#3B4252',
        red: '#BF616A',
        green: '#A3BE8C',
        yellow: '#EBCB8B',
        blue: '#81A1C1',
        magenta: '#B48EAD',
        cyan: '#88C0D0',
        white: '#E5E9F0',
        brightBlack: '#4C566A',
        brightRed: '#BF616A',
        brightGreen: '#A3BE8C',
        brightYellow: '#EBCB8B',
        brightBlue: '#81A1C1',
        brightMagenta: '#B48EAD',
        brightCyan: '#8FBCBB',
        brightWhite: '#ECEFF4'
    },
    allowTransparency: true,
    cursorBlink: true,
    convertEol: true,
    screenReaderMode: true,
    cols: 80,
    rows: 24,
    windowsMode: true,
    scrollback: 1000,
    windowOptions: {
        setWinSizeChars: true
    },
    defaultEncoding: 'utf8',
    useFlowControl: true
});

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
                // Nettoyage et conversion de l'encodage
                const cleanOutput = message.data
                    .replace(/\^[\{\}]/g, '') // Supprime les caractères parasites
                    .replace(/\u0000/g, '');  // Supprime les caractères nuls

                term.write(cleanOutput);
            }
        } catch (error) {
            console.error('Erreur message:', error);
        }
    };

}

function handleSpecialCommands(command) {
    switch (command.trim().toLowerCase()) {
        case 'clear':
        case 'cls':
            term.write('\x1b[2J\x1b[H');
            term.write('> ');
            return true;
        default:
            return false;
    }
}

// Gestionnaire des entrées du terminal
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
                term.write('\r\x1b[K> ');
                currentLine = commandHistory[historyIndex];
                term.write(currentLine);
            }
            break;

        case '\u001b[B': // Flèche bas
            if (historyIndex < commandHistory.length) {
                historyIndex++;
                term.write('\r\x1b[K> ');
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

// Initialisation
term.open(document.getElementById('terminal'));
connect();