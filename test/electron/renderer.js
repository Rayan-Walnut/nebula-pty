const os = require('os');
const path = require('path');
const { Terminal } = require('xterm');
const { FitAddon } = require('xterm-addon-fit');
const { WebLinksAddon } = require('xterm-addon-web-links');
const { WebTerminal } = require('nebula-pty');

// Configuration du terminal
const term = new Terminal({
    cursorBlink: true,
    cursorStyle: 'block',
    fontFamily: 'Consolas, monospace',
    fontSize: 14,
    theme: {
        background: '#1e1e1e',
        foreground: '#d4d4d4',
        cursor: '#ffffff',
        selection: '#264f78',
        black: '#000000',
        red: '#cd3131',
        green: '#0dbc79',
        yellow: '#e5e510',
        blue: '#2472c8',
        magenta: '#bc3fbc',
        cyan: '#11a8cd',
        white: '#e5e5e5',
        brightBlack: '#666666',
        brightRed: '#f14c4c',
        brightGreen: '#23d18b',
        brightYellow: '#f5f543',
        brightBlue: '#3b8eea',
        brightMagenta: '#d670d6',
        brightCyan: '#29b8db',
        brightWhite: '#e5e5e5'
    }
});

// Addons
const fitAddon = new FitAddon();
term.loadAddon(fitAddon);
term.loadAddon(new WebLinksAddon());

// Configuration du conteneur et démarrage
const terminalContainer = document.getElementById('terminal-container');
term.open(terminalContainer);
fitAddon.fit();

// Configuration du processus PTY
const startPTY = () => {
    const ptyProcess = new WebTerminal();

    // Configuration de l'environnement
    const env = {
        ...process.env,
        TERM: 'xterm-256color',
        COLORTERM: 'truecolor',
        TERM_PROGRAM: 'nebula-pty',
        HOME: os.homedir(),
        LANG: 'en_US.UTF-8',
        LC_ALL: 'en_US.UTF-8',
        LC_CTYPE: 'en_US.UTF-8'
    };

    // Options de démarrage
    const ptyOptions = {
        cols: term.cols,
        rows: term.rows,
        cwd: process.env.HOME || process.env.USERPROFILE,
        env: env
    };

    try {
        ptyProcess.startProcess(ptyOptions);

        // Gestion des données
        term.onData(data => {
            try {
                ptyProcess.write(data);
            } catch (error) {
                console.error('Erreur lors de l\'écriture vers PTY:', error);
            }
        });

        ptyProcess.onData(data => {
            try {
                term.write(data);
            } catch (error) {
                console.error('Erreur lors de l\'écriture vers terminal:', error);
            }
        });

        // Gestion du redimensionnement
        const handleResize = () => {
            try {
                fitAddon.fit();
                ptyProcess.resize(term.cols, term.rows);
            } catch (error) {
                console.error('Erreur lors du redimensionnement:', error);
            }
        };

        window.addEventListener('resize', handleResize);

        // Ajout d'un gestionnaire pour le focus
        terminalContainer.addEventListener('click', () => {
            term.focus();
        });

        return ptyProcess;
    } catch (error) {
        console.error('Erreur lors du démarrage du PTY:', error);
        throw error;
    }
};

// Démarrage initial
let currentPTY;
try {
    currentPTY = startPTY();
} catch (error) {
    console.error('Erreur lors de l\'initialisation:', error);
}

// Focus initial
term.focus();

// Export pour debug si nécessaire
window.term = term;
window.ptyProcess = currentPTY;