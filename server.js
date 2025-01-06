const express = require('express');
const http = require('http');
const WebSocket = require('ws');
const path = require('path');
const Terminal = require('./build/Release/test_addon.node');

const app = express();
const server = http.createServer(app);
const wss = new WebSocket.Server({ server });

// Servir les fichiers statiques
app.use(express.static('public'));
app.use('/xterm', express.static(path.join(__dirname, 'node_modules/xterm')));
app.use('/xterm-addon-fit', express.static(path.join(__dirname, 'node_modules/xterm-addon-fit')));

wss.on('connection', (ws) => {
    console.log('Client connecté');
    
    // Créer un nouveau terminal pour chaque connexion
    const term = new Terminal({
        cols: 80,
        rows: 24
    });

    // Envoyer les données du terminal au client
    term.onData((data) => {
        if (ws.readyState === WebSocket.OPEN) {
            ws.send(data);
        }
    });

    // Recevoir les données du client
    ws.on('message', (data) => {
        const message = data.toString();
        
        // Vérifier si c'est une commande de redimensionnement
        if (message.startsWith('{')) {
            try {
                const cmd = JSON.parse(message);
                if (cmd.type === 'resize') {
                    term.resize(cmd.cols, cmd.rows);
                    return;
                }
            } catch (e) {}
        }

        // Sinon, envoyer au terminal
        term.write(message);
    });

    // Nettoyer à la déconnexion
    ws.on('close', () => {
        console.log('Client déconnecté');
        term.destroy();
    });
});

const port = 3000;
server.listen(port, () => {
    console.log(`Serveur démarré sur http://localhost:${port}`);
});