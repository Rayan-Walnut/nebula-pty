const express = require('express');
const http = require('http');
const WebSocket = require('ws');
const path = require('path');
const TerminalModule = require('./build/Release/terminal');
const Terminal = TerminalModule.Terminal;
console.log('Module terminal chargé:', TerminalModule);


const app = express();
const server = http.createServer(app);
const wss = new WebSocket.Server({ server });

// Servir les fichiers statiques
app.use(express.static('public'));
app.use('/xterm', express.static(path.join(__dirname, 'node_modules/xterm')));
app.use('/xterm-addon-fit', express.static(path.join(__dirname, 'node_modules/xterm-addon-fit')));

// Route principale
app.get('/', (req, res) => {
    res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

// Map pour stocker les instances de terminal
const terminals = new Map();

// Gestion des connexions WebSocket
wss.on('connection', (ws) => {
    console.log('Nouvelle connexion client établie');
    
    // Créer une nouvelle instance de terminal
    const term = new Terminal({
        cols: 80,
        rows: 24,
        cwd: 'C:\\Users\\prora\\Desktop' // Remplacez par votre chemin
    });


    const terminalId = Date.now().toString();
    terminals.set(terminalId, { term, ws });

    // Configurer la callback de données du terminal
    term.onData((data) => {
        if (ws.readyState === WebSocket.OPEN) {
            try {
                ws.send(JSON.stringify({
                    type: 'output',
                    data: data
                }));
            } catch (err) {
                console.error('Erreur lors de l\'envoi des données:', err);
            }
        }
    });

    // Envoyer l'ID du terminal au client
    ws.send(JSON.stringify({
        type: 'terminal-id',
        id: terminalId
    }));

    // Gestion des messages du client
    ws.on('message', (message) => {
        try {
            const msg = JSON.parse(message.toString());
            
            const terminalData = terminals.get(msg.terminalId);
            if (!terminalData) {
                console.error('Terminal non trouvé:', msg.terminalId);
                return;
            }

            switch (msg.type) {
                case 'input':
                    if (msg.data.trim()) {
                        terminalData.term.write(msg.data);
                    }
                    break;
                
                case 'resize':
                    terminalData.term.resize(msg.cols, msg.rows);
                    break;

                default:
                    console.warn('Type de message inconnu:', msg.type);
            }
        } catch (err) {
            console.error('Erreur de traitement du message:', err);
        }
    });

    // Gestion des erreurs WebSocket
    ws.on('error', (error) => {
        console.error('Erreur WebSocket:', error);
    });

    // Nettoyage à la déconnexion
    ws.on('close', () => {
        const terminalData = terminals.get(terminalId);
        if (terminalData) {
            console.log('Fermeture du terminal:', terminalId);
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