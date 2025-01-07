const express = require('express');
const http = require('http');
const WebSocket = require('ws');
const path = require('path');
const TerminalModule = require('./build/Release/terminal');
const Terminal = TerminalModule.Terminal;
console.log('Module terminal chargé:', TerminalModule);

const os = require('os');
const app = express();
const server = http.createServer(app);
const wss = new WebSocket.Server({ server });

// Servir les fichiers statiques
app.use(express.static('public'));
app.use('/xterm', express.static(path.join(__dirname, 'node_modules/xterm')));
app.use('/xterm-addon-fit', express.static(path.join(__dirname, 'node_modules/xterm-addon-fit')));
app.use('/node_modules', express.static(path.join(__dirname, 'node_modules')));

// Route principale
app.get('/', (req, res) => {
    res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

// Map pour stocker les instances de terminal
const terminals = new Map();
wss.on('connection', (ws) => {
    console.log('Nouvelle connexion client');
    
    // Utiliser le répertoire personnel de l'utilisateur comme chemin initial
    const homedir = os.homedir();
    
    const term = new Terminal({
        cols: 80,
        rows: 24,
        cwd: homedir // Définir le répertoire initial
    });

    const terminalId = Date.now().toString();
    terminals.set(terminalId, { term, ws });

    term.onData((data) => {
        if (ws.readyState === WebSocket.OPEN) {
            ws.send(JSON.stringify({ type: 'output', data }));
        }
    });

    // Envoyer l'ID du terminal
    ws.send(JSON.stringify({ type: 'terminal-id', id: terminalId }));

    ws.on('message', (message) => {
        try {
            const msg = JSON.parse(message.toString());
            const terminalData = terminals.get(msg.terminalId);
            
            if (!terminalData) return;

            if (msg.type === 'input') {
                terminalData.term.write(msg.data);
            }
        } catch (error) {
            console.error('Erreur de traitement du message:', error);
        }
    });

    ws.on('close', () => {
        const terminalData = terminals.get(terminalId);
        if (terminalData) {
            terminalData.term.destroy();
            terminals.delete(terminalId);
        }
        console.log('Client déconnecté');
    });
});


// Gestion des erreurs serveur WebSocket
wss.on('error', (error) => {
    console.error('Erreur serveur WebSocket:', error);
});

// Démarrage du serveur
const port = process.env.PORT || 3000;
server.listen(port, () => {
    console.log(`Serveur démarré sur http://localhost:${port}`);
});

// Gestion de l'arrêt propre
process.on('SIGTERM', () => {
    console.log('Signal SIGTERM reçu, arrêt du serveur...');
    for (const [id, { term }] of terminals) {
        term.destroy();
    }
    terminals.clear();
    server.close(() => {
        console.log('Serveur arrêté');
        process.exit(0);
    });
});

process.on('SIGINT', () => {
    console.log('Signal SIGINT reçu, arrêt du serveur...');
    for (const [id, { term }] of terminals) {
        term.destroy();
    }
    terminals.clear();
    server.close(() => {
        console.log('Serveur arrêté');
        process.exit(0);
    });
});