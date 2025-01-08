const express = require('express');
const http = require('http');
const WebSocket = require('ws');
const path = require('path');
const os = require('os');
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

wss.on('connection', (ws, req) => {
    const clientIp = req.socket.remoteAddress;
    const terminalId = Date.now().toString();
    
    log('WEBSOCKET', 'Nouvelle connexion entrante', { clientIp, terminalId });

    try {
        const homedir = os.homedir();
        log('TERMINAL', 'Création du terminal', { terminalId, homedir });

        const term = new Terminal({
            cwd: homedir
        });

        terminals.set(terminalId, { term, ws });

        ws.send(JSON.stringify({ 
            type: 'terminal-id', 
            id: terminalId 
        }));

        // Maintenant le message de bienvenue montre le répertoire actuel
        const output = term.executeCommand('cd');
        ws.send(JSON.stringify({
            type: 'output',
            data: `Terminal prêt dans : ${output}\n> `
        }));

        ws.on('message', async (message) => {
            try {
                const msg = JSON.parse(message);
                log('INPUT', 'Message reçu', {
                    terminalId,
                    type: msg.type,
                    data: msg.data
                });

                if (msg.type === 'input' && terminals.has(msg.terminalId)) {
                    const terminal = terminals.get(msg.terminalId);
                    const output = terminal.term.executeCommand(msg.data);
                    
                    ws.send(JSON.stringify({
                        type: 'output',
                        data: output + '\n> '
                    }));
                }
            } catch (error) {
                log('ERROR', 'Erreur message', error);
            }
        });

        ws.on('close', () => {
            log('WEBSOCKET', 'Fermeture connexion', { terminalId });
            if (terminals.has(terminalId)) {
                const terminal = terminals.get(terminalId);
                terminal.term.destroy();
                terminals.delete(terminalId);
            }
        });

        ws.on('error', (error) => {
            log('ERROR', 'Erreur WebSocket', { terminalId, error: error.message });
        });

    } catch (error) {
        log('ERROR', 'Erreur lors de la création du terminal', error);
        ws.close();
    }
});

const port = process.env.PORT || 3000;
server.listen(port, () => {
    log('SERVER', `Serveur démarré sur http://localhost:${port}`);
});

process.on('uncaughtException', (error) => {
    log('ERROR', 'Erreur non capturée', error);
});

process.on('unhandledRejection', (reason) => {
    log('ERROR', 'Promesse rejetée', reason);
});