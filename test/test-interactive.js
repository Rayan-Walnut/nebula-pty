const { WebTerminal } = require('../build/Release/terminal.node');
const readline = require('readline');

const term = new WebTerminal();

// Configuration de readline pour l'entrée utilisateur
const rl = readline.createInterface({
    input: process.stdin,
    output: process.stdout
});

// Callback pour recevoir les données du terminal
term.onData((data) => {
    process.stdout.write(data);
});

// Démarrer le processus
term.startProcess({ cols: 80, rows: 24 });

// Gérer l'entrée utilisateur
rl.on('line', (input) => {
    term.write(input + '\r\n');
});

// Nettoyage à la sortie
process.on('SIGINT', () => {
    rl.close();
    process.exit(0);
});