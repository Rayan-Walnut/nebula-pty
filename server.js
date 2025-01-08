const express = require('express');
const http = require('http');
const WebSocket = require('ws');
const path = require('path');
const os = require('os');
const iconv = require('iconv-lite');
const Terminal = require('./build/Release/terminal').Terminal;

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

app.use(express.static(path.join(__dirname, 'public')));
app.use('/xterm', express.static(path.join(__dirname, 'node_modules/xterm')));

const terminals = new Map();
function convertWindowsOutput(buffer) {
    // Détecter l'encodage BOM
    if (buffer.length >= 3 &&
        buffer[0] === 0xEF &&
        buffer[1] === 0xBB &&
        buffer[2] === 0xBF) {
        // UTF-8 avec BOM
        return buffer.toString('utf8', 3);
    }

    // Essayer différents encodages dans l'ordre
    const encodings = ['utf8', 'win1252', 'cp850', 'latin1'];

    for (const encoding of encodings) {
        try {
            const decoded = iconv.decode(Buffer.from(buffer), encoding);
            // Vérifier si le texte décodé est valide
            if (decoded.replace(/[\x00-\x1F\x7F-\x9F]/g, '').length > 0) {
                return decoded
                    .replace(/\r\n/g, '\n')  // Normaliser les sauts de ligne
                    .replace(/[\x00-\x08\x0B\x0C\x0E-\x1F]/g, ''); // Nettoyer les caractères de contrôle
            }
        } catch (e) {
            continue;
        }
    }

    // Fallback sur UTF-8 simple
    return buffer.toString('utf8');
}

wss.on('connection', (ws, req) => {
    const terminalId = Date.now().toString();

    try {
        const term = new Terminal({ cwd: 'C:\\Users\\prora\\Desktop' });

        terminals.set(terminalId, { term, ws });

        // Envoyer l'ID du terminal
        ws.send(JSON.stringify({
            type: 'terminal-id',
            id: terminalId
        }));

        // Initialiser avec chcp 65001 pour forcer l'UTF-8
        const initOutput = term.executeCommand('chcp 65001');

        // Obtenir le répertoire courant
        const output = term.executeCommand('cd');
        const convertedOutput = convertWindowsOutput(output);

        ws.send(JSON.stringify({
            type: 'output',
            data: `Terminal prêt dans : ${convertedOutput}\n> `
        }));

        ws.on('message', async (message) => {
            try {
                const msg = JSON.parse(message);
                if (msg.type === 'input' && terminals.has(msg.terminalId)) {
                    const terminal = terminals.get(msg.terminalId);
                    const output = terminal.term.executeCommand(msg.data);
                    const convertedOutput = convertWindowsOutput(output);

                    ws.send(JSON.stringify({
                        type: 'output',
                        data: convertedOutput + '\n> '
                    }));
                }
            } catch (error) {
                log('ERROR', 'Erreur message', error);
            }
        });

        // ... (reste du code de gestion des erreurs et fermeture)

    } catch (error) {
        log('ERROR', 'Erreur lors de la création du terminal', error);
        ws.close();
    }
});

const port = process.env.PORT || 3000;
server.listen(port, () => {
    log('SERVER', `Serveur démarré sur http://localhost:${port}`);
});