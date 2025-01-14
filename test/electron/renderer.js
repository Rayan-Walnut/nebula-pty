const os = require('os');
const { Terminal } = require('xterm');
const { FitAddon } = require('xterm-addon-fit');
const { WebLinksAddon } = require('xterm-addon-web-links');
const { WebTerminal } = require('nebula-pty');

const term = new Terminal({
    cursorBlink: true,
    cursorStyle: 'block',
    fontFamily: 'Consolas, monospace',
    theme: {
        background: '#1e1e1e',
        foreground: '#d4d4d4',
        cursor: '#ffffff'
    }
});

const fitAddon = new FitAddon();
term.loadAddon(fitAddon);
term.loadAddon(new WebLinksAddon());

term.open(document.getElementById('terminal-container'));
fitAddon.fit();

const ptyProcess = new WebTerminal();
ptyProcess.startProcess({
    cols: term.cols,
    rows: term.rows,
    env: process.env
});

term.onData(data => ptyProcess.write(data));
ptyProcess.onData(data => term.write(data));

window.addEventListener('resize', () => {
    fitAddon.fit();
    ptyProcess.resize(term.cols, term.rows);
});

term.focus();