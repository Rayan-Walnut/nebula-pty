const os = require('os');
const pty = require('nebula-pty');
const { Terminal } = require('@xterm/xterm');
require('@xterm/xterm/css/xterm.css');

console.log('Initializing terminal...');

// Créer un terminal xterm.js
const terminal = new Terminal({
    cursorBlink: true,
    fontSize: 14,
    fontFamily: 'Consolas,Liberation Mono,Menlo,Courier,monospace'
});

// Attacher le terminal au DOM
terminal.open(document.getElementById('xterm'));

// Initialiser le PTY
const shell = process.env[os.platform() === 'win32' ? 'COMSPEC' : 'SHELL'];
console.log('Starting shell:', shell);

const ptyProcess = pty.spawn(shell, [], {
    name: 'xterm-color',
    cols: 80,
    rows: 30,
    cwd: process.cwd(),
    env: process.env
});

// Configuration des événements
terminal.onData((data) => {
    console.log('Terminal -> PTY:', data);
    ptyProcess.write(data);
});

ptyProcess.on('data', (data) => {
    console.log('PTY -> Terminal:', data);
    terminal.write(data);
});

// Gestion du redimensionnement
const fitTerminal = () => {
    const dims = document.getElementById('xterm').getBoundingClientRect();
    const cols = Math.max(2, Math.floor(dims.width / 9));
    const rows = Math.max(1, Math.floor(dims.height / 17));
    
    terminal.resize(cols, rows);
    ptyProcess.resize(cols, rows);
};

window.addEventListener('resize', fitTerminal);
fitTerminal();