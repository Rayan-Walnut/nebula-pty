const Terminal = require('../build/Release/terminal.node');

console.log('Creating terminal...');
const terminal = new Terminal.WebTerminal();

// Configurer le callback avant de démarrer le processus
terminal.onData((data) => {
    console.log('Terminal output:', data);
});

console.log('Starting process...');
terminal.startProcess({ cols: 80, rows: 24 });

// Attendre un peu avant d'envoyer la première commande
setTimeout(() => {
    console.log('Sending first command...');
    terminal.write('echo "Hello World"\r\n');
}, 1000);

// Envoyer une deuxième commande plus tard
setTimeout(() => {
    console.log('Sending second command...');
    terminal.write('dir\r\n');
}, 2000);

// Empêcher le programme de se terminer
process.stdin.resume();

// Gérer la fermeture proprement
process.on('SIGINT', () => {
    console.log('Closing terminal...');
    process.exit();
});