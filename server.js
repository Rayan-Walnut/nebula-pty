const express = require('express');
const http = require('http');
const WebSocket = require('ws');
const path = require('path');
const os = require('os');
const iconv = require('iconv-lite');
const Terminal = require('./build/Release/terminal').Terminal;

// Logging function
function log(type, message, data = null) {
    const timestamp = new Date().toISOString();
    const logMessage = `[${timestamp}] [${type}] ${message}`;
    console.log(logMessage, data || '');
}

const app = express();
const server = http.createServer(app);
const wss = new WebSocket.Server({
    server,
    perMessageDeflate: false
});

// Middleware
app.use(express.static(path.join(__dirname, 'public')));
app.use('/xterm', express.static(path.join(__dirname, 'node_modules/xterm')));

// Global state
const terminals = new Map();
const activeTerminals = new Map();

function isInteractiveCommand(command) {
    const interactiveCommands = [
        'python',
        'python3',
        'node',
        'npm',
        'yarn',
        'ssh',
        'powershell',
        'cmd'
    ];

    command = command.trim().toLowerCase();
    return interactiveCommands.some(cmd =>
        command === cmd || command.startsWith(`${cmd} `)
    );
}

function convertWindowsOutput(buffer) {
    if (typeof buffer === 'string') {
        buffer = Buffer.from(buffer);
    }

    // BOM detection
    if (buffer.length >= 3 &&
        buffer[0] === 0xEF &&
        buffer[1] === 0xBB &&
        buffer[2] === 0xBF) {
        return buffer.toString('utf8', 3);
    }

    const encodings = ['utf8', 'win1252', 'cp850', 'latin1'];

    for (const encoding of encodings) {
        try {
            const decoded = iconv.decode(Buffer.from(buffer), encoding);
            if (decoded.replace(/[\x00-\x1F\x7F-\x9F]/g, '').length > 0) {
                return decoded
                    .replace(/\r\n/g, '\n')
                    .replace(/[\x00-\x08\x0B\x0C\x0E-\x1F]/g, '');
            }
        } catch (e) {
            continue;
        }
    }

    return buffer.toString('utf8');
}

function startReading(terminalId) {
    const terminal = terminals.get(terminalId);
    if (!terminal) return;

    const readLoop = () => {
        try {
            const data = terminal.term.read();
            if (data && data.length > 0) {
                terminal.ws.send(JSON.stringify({
                    type: 'output',
                    data: convertWindowsOutput(data)
                }));
            }

            if (activeTerminals.get(terminalId)) {
                setTimeout(readLoop, 50);
            }
        } catch (error) {
            log('ERROR', 'Erreur lecture terminal', error);
        }
    };

    readLoop();
}

// WebSocket connection handling
wss.on('connection', async (ws, req) => {
    const terminalId = Date.now().toString();
    let terminalInstance = null;

    try {
        const userHomeDir = os.homedir();
        const defaultPath = path.join(userHomeDir, 'Desktop');

        const term = new Terminal({ cwd: defaultPath });
        terminalInstance = { term, ws, interactive: false };
        terminals.set(terminalId, terminalInstance);
        activeTerminals.set(terminalId, true);

        // Send terminal ID
        ws.send(JSON.stringify({
            type: 'terminal-id',
            id: terminalId
        }));

        // Send initial prompt
        ws.send(JSON.stringify({
            type: 'output',
            data: `Terminal prêt dans : ${defaultPath}\n> `
        }));


        // Handle incoming messages
        ws.on('message', async (message) => {
            try {
                const msg = JSON.parse(message);
                if (msg.type === 'input' && terminals.has(msg.terminalId)) {
                    const terminal = terminals.get(msg.terminalId);
                    const command = msg.data.trim();

                    // Exit from interactive mode
                    if (command === 'exit' && terminal.interactive) {
                        terminal.interactive = false;
                        terminal.term.write('\x03'); // Send Ctrl+C
                        ws.send(JSON.stringify({
                            type: 'output',
                            data: '\n> '
                        }));
                        return;
                    }

                    if (!terminal.interactive && isInteractiveCommand(command)) {
                        terminal.interactive = true;
                        terminal.term.executeInteractive(command);
                        startReading(msg.terminalId);
                    } else if (terminal.interactive) {
                        terminal.term.write(command + '\n');
                    } else {
                        const output = terminal.term.executeCommand(command);
                        ws.send(JSON.stringify({
                            type: 'output',
                            data: convertWindowsOutput(output) + '\n> '
                        }));
                    }
                } else if (msg.type === 'resize') {
                    const terminal = terminals.get(msg.terminalId);
                    if (terminal) {
                        terminal.term.resize(msg.cols, msg.rows);
                    }
                }
            } catch (error) {
                log('ERROR', 'Erreur traitement message', error);
            }
        });

        // Handle connection close
        ws.on('close', () => {
            try {
                if (terminals.has(terminalId)) {
                    const terminal = terminals.get(terminalId);
                    activeTerminals.set(terminalId, false);
                    if (terminal && terminal.term) {
                        terminal.term.destroy();
                    }
                    terminals.delete(terminalId);
                    activeTerminals.delete(terminalId);
                    log('INFO', `Terminal ${terminalId} fermé`);
                }
            } catch (error) {
                log('ERROR', `Erreur fermeture terminal ${terminalId}`, error);
            }
        });

        // Handle errors
        ws.on('error', () => {
            try {
                if (terminalInstance && terminalInstance.term) {
                    terminalInstance.term.destroy();
                }
                terminals.delete(terminalId);
                activeTerminals.delete(terminalId);
            } catch (error) {
                log('ERROR', `Erreur nettoyage terminal ${terminalId}`, error);
            }
        });

    } catch (error) {
        log('ERROR', 'Erreur création terminal', error);
        if (terminalInstance && terminalInstance.term) {
            try {
                terminalInstance.term.destroy();
            } catch (e) {
                log('ERROR', 'Erreur nettoyage après échec création', e);
            }
        }
        ws.close();
    }
});

// Cleanup on server shutdown
function cleanupTerminals() {
    for (const [id, terminal] of terminals) {
        try {
            if (terminal && terminal.term) {
                terminal.term.destroy();
            }
        } catch (error) {
            log('ERROR', `Erreur nettoyage terminal ${id}`, error);
        }
    }
    terminals.clear();
    activeTerminals.clear();
}

process.on('SIGTERM', cleanupTerminals);
process.on('SIGINT', cleanupTerminals);

// Start server
const port = process.env.PORT || 3000;
server.listen(port, () => {
    log('SERVER', `Serveur démarré sur http://localhost:${port}`);
});