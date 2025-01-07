const express = require('express');
const http = require('http');
const WebSocket = require('ws');
const path = require('path');

// Chargement du module Terminal
let Terminal;
try {
    const terminalPath = path.join(__dirname, 'build', 'Release', 'terminal.node');
    console.log('Chargement du module depuis:', terminalPath);
    Terminal = require(terminalPath).Terminal;
    console.log('Module Terminal chargé avec succès');
} catch (error) {
    console.error('Erreur lors du chargement du module Terminal:', error);
    process.exit(1);
}

const app = express();
const server = http.createServer(app);
const wss = new WebSocket.Server({ server });

// Configuration des routes statiques
app.use(express.static('public'));
app.use('/xterm', express.static(path.join(__dirname, 'node_modules/xterm')));
app.use('/xterm-addon-fit', express.static(path.join(__dirname, 'node_modules/xterm-addon-fit')));

// Gestion des connexions WebSocket
wss.on('connection', (ws) => {
    console.log('Nouvelle connexion client établie');
    let term;

    try {
        // Création d'une nouvelle instance de Terminal
        term = new Terminal({
            cols: 80,
            rows: 24
        });
        console.log('Instance de Terminal créée');

        // Configuration de la réception des données du terminal
        term.onData((data) => {
            console.log('Données reçues du terminal:', data);
            if (ws.readyState === WebSocket.OPEN) {
                ws.send(data);
            }
        });

        // Message de bienvenue
        term.write('Terminal connecté\r\n');

        // Gestion des messages du client WebSocket
        ws.on('message', (message) => {
            try {
                const data = message.toString();
                
                // Si c'est une commande de redimensionnement
                if (data.startsWith('{')) {
                    try {
                        const cmd = JSON.parse(data);
                        if (cmd.type === 'resize') {
                            console.log(`Redimensionnement: ${cmd.cols}x${cmd.rows}`);
                            term.resize(cmd.cols, cmd.rows);
                            return;
                        }
                    } catch (e) {
                        console.error('Erreur de parsing JSON:', e);
                    }
                }

                // Sinon, envoyer au terminal
                console.log('Envoi au terminal:', data);
                term.write(data);

            } catch (err) {
                console.error('Erreur de traitement du message:', err);
            }
        });

        // Gestion de la fermeture de la connexion
        ws.on('close', () => {
            console.log('Client déconnecté');
            if (term) {
                term.destroy();
                term = null;
            }
        });

        // Gestion des erreurs WebSocket
        ws.on('error', (error) => {
            console.error('Erreur WebSocket:', error);
            if (term) {
                term.destroy();
                term = null;
            }
        });

    } catch (error) {
        console.error('Erreur lors de l\'initialisation du terminal:', error);
        ws.close();
    }
});

// Gestion des erreurs du serveur HTTP
server.on('error', (error) => {
    console.error('Erreur serveur:', error);
});

// Démarrage du serveur
const port = process.env.PORT || 3000;
server.listen(port, () => {
    console.log(`Serveur démarré sur http://localhost:${port}`);
});

// Gestion de l'arrêt propre
process.on('SIGTERM', () => {
    console.log('Signal SIGTERM reçu, arrêt du serveur...');
    server.close(() => {
        console.log('Serveur arrêté');
        process.exit(0);
    });
});

process.on('SIGINT', () => {
    console.log('Signal SIGINT reçu, arrêt du serveur...');
    server.close(() => {
        console.log('Serveur arrêté');
        process.exit(0);
    });
});